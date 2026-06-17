/* ===== present (diff+flush), feed (arrow-key pan), run loop ===== */
char *flow_render_diff(flow_t *f);   /* embed primitive: render the current model, diff it against the
                                        previous frame, ADVANCE the front buffer, and return a malloc'd
                                        escape string (absolute-positioned CSI/SGR) the HOST writes to its
                                        own fd/terminal, then free()s. "" (empty, non-NULL) when nothing
                                        changed. No stdout/termios coupling — for host-owned loops.
                                        flow_present is exactly this + fputs(stdout)+fflush. */
void flow_present(flow_t *f);
void flow_feed(flow_t *f, const char *bytes, int n);
void flow_run(flow_t *f);

#ifdef FLOW_IMPLEMENTATION
#include <poll.h>
#include <errno.h>
void flow_tick(flow_t *f) { ++f->tick; }                       /* pure counter-advance — NO present, NO read, NO clock */
unsigned flow_ticks(flow_t *f) { return f->tick; }
void flow_set_tick_ms(flow_t *f, int ms) { f->tick_ms = ms < 1 ? 1 : ms; }  /* clamp: 0/negative → 1 (a 0 poll timeout would busy-spin) */
int flow__frames_armed(flow_t *f) {                            /* recomputed each poll: scans current model state, no stored arm flag. */
  if (flow__drag_in_flight(f)) return 1;                        /* inc-6 #8: an object drag/connection is in flight (ticked autopan) */
  for (int i = 0; i < flow_edge_count(f); i++)                  /* inc-6 #5: any FLOW_ANIMATED edge needs frames (early-out on first) */
    if (flow_edges(f)[i].flags & FLOW_ANIMATED) return 1;
  return 0;
}
char *flow_render_diff(flow_t *f) {
  flow_cell *back = (flow_cell*)calloc((size_t)f->cols * f->rows, sizeof(flow_cell));
  flow_render(f, back, f->cols, f->rows);
  char *esc = flow_diff_emit(f->front, back, f->cols, f->rows);   /* malloc'd; caller frees */
  memcpy(f->front, back, (size_t)f->cols * f->rows * sizeof(flow_cell));   /* advance prev-frame */
  free(back);
  return esc;
}
void flow_present(flow_t *f) {                                    /* the stdout-bound convenience over flow_render_diff */
  char *esc = flow_render_diff(f);
  fputs(esc, stdout); fflush(stdout); free(esc);
}
void flow_feed(flow_t *f, const char *b, int n) {
  int i = 0;
  while (i < n) {
    if (b[i] == '\x1b' && i + 2 < n && b[i+1] == '[' && b[i+2] == '<') {
      flow_mouse_event ev; int used = flow_parse_mouse(b + i, n - i, &ev);
      if (used > 0) { flow_handle_mouse(f, &ev); i += used; continue; }
    }
    int dk = flow_dispatch_key(f, b + i, n - i);   /* registry/built-in keys (NOT bare arrows) */
    if (dk > 0) { i += dk; continue; }
    if (b[i] == '\x1b' && i + 2 < n && b[i+1] == '[') {
      switch (b[i+2]) {                          /* arrow-key pan */
        case 'A': flow_pan(f, 0,  1); i += 3; continue;   /* up    */
        case 'B': flow_pan(f, 0, -1); i += 3; continue;   /* down  */
        case 'C': flow_pan(f, -1, 0); i += 3; continue;   /* right */
        case 'D': flow_pan(f,  1, 0); i += 3; continue;   /* left  */
        case 'Z': flow_focus_prev(f); i += 3; continue;   /* Shift-Tab: focus backward (inc-5 #5);
                                                             previously fell to default and the
                                                             trailing i++ mis-parsed "[Z" */
        default: break;
      }
      /* Shift-arrow selection nudge (inc-5 #6): 6-byte \x1b[1;2{A,B,C,D} moves the
         FLOW_SELECTED set 1 world cell. The nudge delta is the NEGATION of the bare
         arrow's pan arg on every key (pan moves the camera, nudge moves the node;
         world-y grows downward): A up (0,-1), B down (0,+1), C right (+1,0), D left
         (-1,0). b[i+2]=='1' here, so the 3-byte switch above never collides. Consumed
         on EVERY path — incl. the empty-selection no-op, which opens no undo bracket
         (no pan fallback: Shift-arrow behavior must not be selection-dependent). */
      if (i + 5 < n && b[i+2] == '1' && b[i+3] == ';' && b[i+4] == '2' &&
          (b[i+5] == 'A' || b[i+5] == 'B' || b[i+5] == 'C' || b[i+5] == 'D')) {
        if (flow_selected_count(f) > 0) {
          int dx = b[i+5] == 'C' ? 1 : (b[i+5] == 'D' ? -1 : 0);
          int dy = b[i+5] == 'B' ? 1 : (b[i+5] == 'A' ? -1 : 0);
          flow__undo_begin(f);                /* multi-node nudge = ONE undo step */
          flow__nudge_selection(f, dx, dy);
          flow__undo_end(f);
        }
        i += 6; continue;
      }
    }
    /* +/- zoom (keyboard has no cursor, so center on the screen centre). Placed AFTER
       flow_dispatch_key so a user flow_bind_key('+') override still wins via the registry. */
    if (b[i] == '+' || b[i] == '=') { flow_zoom_in (f, (flow_pt){ f->cols / 2, f->rows / 2 }); i++; continue; }
    if (b[i] == '-' || b[i] == '_') { flow_zoom_out(f, (flow_pt){ f->cols / 2, f->rows / 2 }); i++; continue; }
    /* lone ESC (not the start of a CSI) cancels an in-flight connection, exits
       space-pan mode (the Esc alias for the sticky Space toggle), clears the
       selection (sig-gated: on_selection_change fires only if the set was non-empty),
       and clears keyboard focus (inc-5 #5). All four actions are idempotent no-ops
       on already-blank state. Real mouse/arrow/Delete sequences all have
       b[i+1]=='[' and are consumed above.
       ACCEPTED trade-off: a CSI split by a read() boundary exactly after its ESC
       byte reads as a lone ESC here (terminals write sequences atomically, so this
       is theoretical); the alternative — requiring a next byte to prove loneness —
       would break the COMMON case, a tapped ESC arriving as a 1-byte read. A real
       fix is an ESC-timeout state machine; out of scope for v1. */
    if (b[i] == '\x1b' && (i + 1 >= n || b[i+1] != '[')) { flow_cancel_connection(f); f->space_held = 0; flow_clear_selection(f); f->focus_node = -1; i++; continue; }
    i++;
  }
}
void flow_run(flow_t *f) {
  int cols, rows;
  if (flow_term_size(&cols, &rows) == 0) flow_resize(f, cols, rows);
  flow_term_setup(); f->running = 1;
  flow_present(f);
  char buf[64];
  while (f->running) {
    int timeout = flow__frames_armed(f) ? f->tick_ms : -1;   /* -1 = block forever (idle ⇒ zero wakeups) */
    struct pollfd pfd = { .fd = STDIN_FILENO, .events = POLLIN };
    int pr = poll(&pfd, 1, timeout);
    if (pr < 0) { if (errno == EINTR) continue; break; }      /* benign signal (SIGWINCH/SIGCONT): retry, don't quit */
    if (pr == 0) { flow_tick(f); flow__autopan_tick(f); flow_present(f); continue; } /* TIMEOUT: advance clock, replay in-flight autopan (#8), redraw */
    /* READABLE */
    int n = (int)read(STDIN_FILENO, buf, sizeof buf);
    if (n <= 0) break;
    flow_feed(f, buf, n);                                     /* q-quit is a dispatch built-in (#3); NO raw scan here */
    flow_present(f);
  }
  flow_term_restore();
}
#endif
