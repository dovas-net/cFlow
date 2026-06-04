/* ===== present (diff+flush), feed (arrow-key pan), run loop ===== */
void flow_present(flow_t *f);
void flow_feed(flow_t *f, const char *bytes, int n);
void flow_run(flow_t *f);

#ifdef FLOW_IMPLEMENTATION
void flow_present(flow_t *f) {
  flow_cell *back = (flow_cell*)calloc((size_t)f->cols * f->rows, sizeof(flow_cell));
  flow_render(f, back, f->cols, f->rows);
  char *esc = flow_diff_emit(f->front, back, f->cols, f->rows);
  fputs(esc, stdout); fflush(stdout); free(esc);
  memcpy(f->front, back, (size_t)f->cols * f->rows * sizeof(flow_cell));
  free(back);
}
void flow_feed(flow_t *f, const char *b, int n) {
  int i = 0;
  while (i < n) {
    if (b[i] == '\x1b' && i + 2 < n && b[i+1] == '[' && b[i+2] == '<') {
      flow_mouse_event ev; int used = flow_parse_mouse(b + i, n - i, &ev);
      if (used > 0) { flow_handle_mouse(f, &ev); i += used; continue; }
    }
    int dk = flow_dispatch_key(f, b + i, n - i);   /* registry/built-in keys (NOT bare arrows, NOT 'q') */
    if (dk > 0) { i += dk; continue; }
    if (b[i] == '\x1b' && i + 2 < n && b[i+1] == '[') {
      switch (b[i+2]) {                          /* arrow-key pan */
        case 'A': flow_pan(f, 0,  1); i += 3; continue;   /* up    */
        case 'B': flow_pan(f, 0, -1); i += 3; continue;   /* down  */
        case 'C': flow_pan(f, -1, 0); i += 3; continue;   /* right */
        case 'D': flow_pan(f,  1, 0); i += 3; continue;   /* left  */
        default: break;
      }
    }
    /* +/- zoom (keyboard has no cursor, so center on the screen centre). Placed AFTER
       flow_dispatch_key so a user flow_bind_key('+') override still wins via the registry. */
    if (b[i] == '+' || b[i] == '=') { flow_zoom_in (f, (flow_pt){ f->cols / 2, f->rows / 2 }); i++; continue; }
    if (b[i] == '-' || b[i] == '_') { flow_zoom_out(f, (flow_pt){ f->cols / 2, f->rows / 2 }); i++; continue; }
    /* lone ESC (not the start of a CSI) cancels an in-flight connection, exits
       space-pan mode (the Esc alias for the sticky Space toggle), and clears the
       selection (sig-gated: on_selection_change fires only if the set was non-empty).
       All three actions are idempotent no-ops on already-blank state. Real
       mouse/arrow/Delete sequences all have b[i+1]=='[' and are consumed above.
       ACCEPTED trade-off: a CSI split by a read() boundary exactly after its ESC
       byte reads as a lone ESC here (terminals write sequences atomically, so this
       is theoretical); the alternative — requiring a next byte to prove loneness —
       would break the COMMON case, a tapped ESC arriving as a 1-byte read. A real
       fix is an ESC-timeout state machine; out of scope for v1. */
    if (b[i] == '\x1b' && (i + 1 >= n || b[i+1] != '[')) { flow_cancel_connection(f); f->space_held = 0; flow_clear_selection(f); i++; continue; }
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
    int n = (int)read(STDIN_FILENO, buf, sizeof buf);
    if (n <= 0) break;
    for (int i = 0; i < n; i++) if (buf[i] == 'q') f->running = 0;
    flow_feed(f, buf, n);
    flow_present(f);
  }
  flow_term_restore();
}
#endif
