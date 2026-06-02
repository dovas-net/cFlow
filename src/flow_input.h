/* ===== mouse input: SGR-1006 parser + drag/pan interaction ===== */
enum { FLOW_MOD_SHIFT = 1u, FLOW_MOD_META = 2u, FLOW_MOD_CTRL = 4u };
typedef enum { FLOW_MOUSE_PRESS, FLOW_MOUSE_RELEASE, FLOW_MOUSE_MOTION, FLOW_MOUSE_WHEEL } flow_mouse_type;
typedef struct {
  flow_mouse_type type;
  int button;        /* press/release/motion: 0=left,1=mid,2=right; wheel: 0=up,1=down,2=left,3=right */
  int x, y;          /* 0-based cell coordinates */
  unsigned mods;     /* FLOW_MOD_SHIFT | FLOW_MOD_META | FLOW_MOD_CTRL */
} flow_mouse_event;

/* Parse one SGR-1006 mouse sequence at s (length n). Returns bytes consumed, or 0 if
   s is not a complete mouse sequence (caller should treat the bytes otherwise). */
int  flow_parse_mouse(const char *s, int n, flow_mouse_event *ev);
void flow_handle_mouse(flow_t *f, const flow_mouse_event *ev);

#ifdef FLOW_IMPLEMENTATION
int flow_parse_mouse(const char *s, int n, flow_mouse_event *ev) {
  if (n < 4 || s[0] != '\x1b' || s[1] != '[' || s[2] != '<') return 0;
  int B = 0, X = 0, Y = 0, field = 0;
  for (int i = 3; i < n; i++) {
    char c = s[i];
    if (c >= '0' && c <= '9') { int *t = field == 0 ? &B : field == 1 ? &X : &Y; *t = *t * 10 + (c - '0'); }
    else if (c == ';') { if (++field > 2) return 0; }
    else if (c == 'M' || c == 'm') {
      ev->mods = ((B&4)?FLOW_MOD_SHIFT:0) | ((B&8)?FLOW_MOD_META:0) | ((B&16)?FLOW_MOD_CTRL:0);
      ev->x = X - 1; ev->y = Y - 1; ev->button = B & 3;
      if (B & 64)       ev->type = FLOW_MOUSE_WHEEL;
      else if (c == 'm') ev->type = FLOW_MOUSE_RELEASE;
      else if (B & 32)   ev->type = FLOW_MOUSE_MOTION;
      else               ev->type = FLOW_MOUSE_PRESS;
      return i + 1;
    }
    else return 0;   /* unexpected byte: not a mouse sequence */
  }
  return 0;          /* incomplete */
}
void flow_handle_mouse(flow_t *f, const flow_mouse_event *ev) {
  if (ev->type == FLOW_MOUSE_WHEEL) {
    switch (ev->button) {
      case 0: flow_pan(f, 0,  1); break;   /* wheel up    */
      case 1: flow_pan(f, 0, -1); break;   /* wheel down  */
      case 2: flow_pan(f, 1,  0); break;   /* wheel left  */
      case 3: flow_pan(f, -1, 0); break;   /* wheel right */
    }
    return;
  }
  flow_pt scr = { ev->x, ev->y };
  if (ev->type == FLOW_MOUSE_PRESS) {
    if (ev->button == 2) {                       /* right-click: context, no drag */
      int id = flow_hit_node(f, scr);
      if (id != -1 && f->cb.on_node_context) f->cb.on_node_context(f, id, scr, f->cb.user);
      return;
    }
    if (ev->button == 0) {                       /* arm a press; classify on move/release */
      f->mouse_down = 1; f->moved = 0; f->down_pos = scr;
      f->down_node = flow_hit_node(f, scr);
      f->drag_node = -1; f->dragging_pan = 0;
    }
  } else if (ev->type == FLOW_MOUSE_MOTION) {
    if (!f->mouse_down) return;
    if (!f->moved && (scr.x != f->down_pos.x || scr.y != f->down_pos.y)) {
      f->moved = 1;                              /* threshold crossed: begin drag or pan */
      if (f->down_node != -1) {
        flow_node *nd = flow_get_node(f, f->down_node);
        flow_pt w = flow_to_world(f, f->down_pos), a = flow_node_abs(f, nd);
        f->drag_node = f->down_node; f->drag_grab.x = w.x - a.x; f->drag_grab.y = w.y - a.y;
        flow_select_node(f, f->down_node, 0);    /* selectNodesOnDrag */
      } else {
        f->dragging_pan = 1; f->last_mouse = f->down_pos;
      }
    }
    if (f->drag_node != -1) {
      flow_pt w = flow_to_world(f, scr);
      flow_move_node(f, f->drag_node, (flow_pt){ w.x - f->drag_grab.x, w.y - f->drag_grab.y });
    } else if (f->dragging_pan) {
      flow_pan(f, scr.x - f->last_mouse.x, scr.y - f->last_mouse.y);
      f->last_mouse = scr;
    }
  } else if (ev->type == FLOW_MOUSE_RELEASE) {
    if (f->mouse_down && !f->moved) {            /* a click, not a drag */
      if (f->down_node != -1) {
        flow_select_node(f, f->down_node, 0);
        if (f->cb.on_node_click) f->cb.on_node_click(f, f->down_node, f->cb.user);
      } else {
        flow_clear_selection(f);
        if (f->cb.on_pane_click) f->cb.on_pane_click(f, flow_to_world(f, scr), f->cb.user);
      }
    }
    f->mouse_down = 0; f->moved = 0; f->drag_node = -1; f->dragging_pan = 0; f->down_node = -1;
  }
}
#endif
