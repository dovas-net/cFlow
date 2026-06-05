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
/* resolve an in-flight connection at a screen cell: complete on a target handle/node
   (handle id when one is hit, else the node's default), else cancel. Shared by the
   connectOnClick press path and the drag-complete release path. */
static void flow__resolve_connection_at(flow_t *f, flow_pt scr) {
  int tnode = -1; int hidx = flow_hit_handle(f, scr, &tnode);
  if (tnode == -1) tnode = flow_hit_node(f, scr);
  if (tnode != -1 && tnode != f->conn_node) {
    const flow_handle *th = (hidx >= 0) ? flow_node_handle_at(f, tnode, hidx) : NULL;
    flow_end_connection(f, tnode, th ? th->id : NULL);
  } else {
    flow_cancel_connection(f);             /* dropped on empty pane or the source */
  }
}
/* event-driven auto-pan (spec §8, model A): ONE autopan_speed step per motion event
   while an OBJECT drag (node / connection / reconnect / marquee) has the cursor
   within autopan_margin cells of a buffer edge, panning toward that edge so
   off-screen targets scroll into reach. A terminal delivers no events for a
   stationary cursor, so this only advances while the mouse keeps moving (the
   run-loop-ticked model is a documented follow-up). Callers gate: pane-pan drags
   never auto-pan (marquee drags auto-pan as of increment 4).
   An axis whose margin bands would overlap (2*margin >= extent) is skipped — defense
   for tiny buffers, where "near the edge" loses meaning. */
static void flow__autopan(flow_t *f, flow_pt scr) {
  int m = f->autopan_margin, s = f->autopan_speed, dx = 0, dy = 0;
  if (2 * m < f->cols) { if (scr.x < m) dx = s; else if (scr.x >= f->cols - m) dx = -s; }
  if (2 * m < f->rows) { if (scr.y < m) dy = s; else if (scr.y >= f->rows - m) dy = -s; }
  if (dx || dy) flow_pan(f, dx, dy);
}
void flow_handle_mouse(flow_t *f, const flow_mouse_event *ev) {
  if (ev->type == FLOW_MOUSE_WHEEL) {
    if (ev->mods & FLOW_MOD_CTRL) {           /* Ctrl+wheel: pointer-centered zoom (button 0=in,1=out) */
      flow_pt cur = { ev->x, ev->y };
      if (ev->button == 0)      flow_zoom_in(f, cur);
      else if (ev->button == 1) flow_zoom_out(f, cur);
      return;
    }
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
    if (ev->button == 2) {                       /* right-click: cancel any in-flight connection, else context */
      if (f->conn_active) { flow_cancel_connection(f); return; }
      int id = flow_hit_node(f, scr);
      if (id != -1) {                            /* node occludes: an edge under the same cell never fires */
        if (f->cb.on_node_context) f->cb.on_node_context(f, id, scr, f->cb.user);
        return;
      }
      int ectx = flow_hit_edge(f, scr, 1);       /* same tolerance as the left-click edge path */
      if (ectx != -1 && f->cb.on_edge_context) f->cb.on_edge_context(f, ectx, scr, f->cb.user);
      return;
    }
    if (ev->button == 0) {                       /* arm a press; classify on move/release */
      /* connectOnClick resolve: a press while already connecting (armed by a prior
         click) completes on a target handle/node, else cancels. Never arms drag. */
      if (f->conn_active) { flow__resolve_connection_at(f, scr); return; }
      if (f->space_held) {                       /* space-pan: force drag-to-pan, over a node OR the pane */
        f->mouse_down = 1; f->moved = 0; f->down_pos = scr;
        f->down_node = -1; f->drag_node = -1; f->dragging_pan = 0;
        f->down_modsel = 0; f->marquee_active = 0;
        return;                                  /* first motion arms the existing dragging_pan path */
      }
      f->mouse_down = 1; f->moved = 0; f->down_pos = scr;
      /* HIT PRECEDENCE (trio invariant): handle -> node-body -> pane. A later edge
         package inserts an edge-endpoint test BETWEEN handle and node-body here.
         Handles only hit on hovered/selected/connecting nodes (flow_hit_handle). */
      int hnode = -1; int hidx = flow_hit_handle(f, scr, &hnode);
      if (hidx >= 0 && hnode != -1) {
        const flow_handle *h = flow_node_handle_at(f, hnode, hidx);
        if (h && flow_begin_connection(f, hnode, h->id) == 0) {
          /* grabbed a source handle: do NOT arm node-drag/select. conn_* is orthogonal.
             Clear mouse_down so the trailing release (after the connection resolves) is
             a no-op and never falls into clear_selection/on_pane_click. */
          f->mouse_down = 0; f->down_node = -1; f->drag_node = -1; f->dragging_pan = 0;
          f->down_modsel = 0; f->marquee_active = 0;
          return;
        }
      }
      /* edge-endpoint test (BETWEEN handle and node-body): if the press lands EXACTLY on
         an endpoint cell of the topmost edge under the cursor, arm a reconnect drag and
         skip node-drag/select. Endpoints sit one cell OUTSIDE the node so this never
         steals an ordinary node-body press (require exact cell, not just within tol). */
      int eedge = flow_hit_edge(f, scr, 1);
      if (eedge != -1) {
        flow_pt ss, ts;
        int oks = flow_edge_endpoint_screen(f, flow_get_edge(f, eedge), 0, &ss);
        int okt = flow_edge_endpoint_screen(f, flow_get_edge(f, eedge), 1, &ts);
        int which = -1;
        if (okt && ts.x == scr.x && ts.y == scr.y) which = 1;        /* target endpoint */
        else if (oks && ss.x == scr.x && ss.y == scr.y) which = 0;   /* source endpoint */
        if (which != -1) {
          f->reconnect_edge = eedge; f->reconnect_which = which;
          f->mouse_down = 1; f->down_node = -1; f->drag_node = -1; f->dragging_pan = 0;
          f->down_modsel = 0; f->marquee_active = 0;
          flow__undo_begin(f);                 /* reconnect gesture = one undo step (closed on release) */
          return;
        }
      }
      f->down_node = flow_hit_node(f, scr);
      f->drag_node = -1; f->dragging_pan = 0;
      f->down_modsel = 0; f->marquee_active = 0;
      unsigned mod = ev->mods & (FLOW_MOD_SHIFT | FLOW_MOD_CTRL);
      if (f->down_node != -1) {
        if (mod) {                               /* shift/ctrl-click a node: modify the set NOW */
          if (ev->mods & FLOW_MOD_CTRL) flow_toggle_node(f, f->down_node);  /* toggle */
          else                          flow_select_node(f, f->down_node, 1);/* shift: additive add */
          f->down_modsel = 1;                    /* suppress release replace + on_node_click; arm group drag */
        }
        /* no-mod node press: classify on move/release as before (no select on press) */
      } else if (ev->mods & FLOW_MOD_SHIFT) {    /* shift-drag empty pane: arm marquee (anchor = down_pos) */
        f->marquee_active = 1;
      }
    }
  } else if (ev->type == FLOW_MOUSE_MOTION) {
    if (f->conn_active) {                         /* drag-connect: track free end + reveal candidate */
      if (scr.x != f->down_pos.x || scr.y != f->down_pos.y) f->moved = 1;
      /* auto-pan BEFORE the update so the drop-candidate reveal hit-tests the post-pan
         view (conn_end is screen coords — the pan never moves it). Deliberately fires
         on connectOnClick HOVER too (conn_active with no button down): the connection
         is in flight either way, and the edge is what makes off-screen targets reachable. */
      flow__autopan(f, scr);
      flow_update_connection(f, scr);
      return;
    }
    if (f->reconnect_edge != -1) {                /* reconnect drag: just track movement, no pan/drag */
      if (scr.x != f->down_pos.x || scr.y != f->down_pos.y) f->moved = 1;
      flow__autopan(f, scr);                      /* nothing to re-place: release hit-tests at the cursor */
      return;
    }
    if (!f->mouse_down) return;
    if (!f->moved && (scr.x != f->down_pos.x || scr.y != f->down_pos.y)) {
      f->moved = 1;                              /* threshold crossed: begin drag, marquee, or pan */
      if (f->marquee_active) {
        f->marquee_on = 1;
        f->marquee_anchor_world = flow_to_world(f, f->down_pos);  /* world-pin ONCE,
          before any auto-pan — same shape as drag_grab/drag_last_world below */
      } else if (f->down_node != -1) {
        flow_node *nd = flow_get_node(f, f->down_node);
        flow_pt w = flow_to_world(f, f->down_pos), a = flow_node_abs(f, nd);
        f->drag_node = f->down_node; f->drag_grab.x = w.x - a.x; f->drag_grab.y = w.y - a.y;
        f->drag_last_world = flow_to_world(f, f->down_pos);   /* multi-drag delta anchor */
        flow__undo_begin(f);                     /* whole drag gesture (single OR multi) = one undo step;
                                                    closed on release iff drag_node is still armed */
        if (!(nd->flags & FLOW_SELECTED))        /* unselected node: plain drag REPLACES selection */
          flow_select_node(f, f->down_node, 0);  /* selectNodesOnDrag; selected node keeps the set (group drag) */
      } else {
        f->dragging_pan = 1; f->last_mouse = f->down_pos;
      }
    }
    if (f->marquee_on) {                          /* live marquee: replace-select within the box */
      /* pan FIRST, then re-select at post-pan world coords (same order as node-drag).
         The anchor corner is the WORLD point pinned at threshold-cross, so as auto-pan
         scrolls the view only the cursor corner moves in world — the rect GROWS from
         the press point (world-stable selection, inc-5 #3) instead of chasing the
         screen anchor. */
      flow__autopan(f, scr);
      f->marquee_cur = scr;
      flow_pt wa = f->marquee_anchor_world, wc = flow_to_world(f, scr);
      flow_rect wr = { wa.x < wc.x ? wa.x : wc.x, wa.y < wc.y ? wa.y : wc.y,
                       (wa.x < wc.x ? wc.x - wa.x : wa.x - wc.x),
                       (wa.y < wc.y ? wc.y - wa.y : wa.y - wc.y) };
      flow_select_in_rect(f, wr, f->marquee_mode, 0);
    } else if (f->drag_node != -1) {
      /* pan FIRST, then place at the post-pan world(cursor): the node stays under the
         cursor as the view scrolls (place-then-pan would make it visually drift). */
      flow__autopan(f, scr);
      if (flow_selected_count(f) > 1) {           /* MULTI-DRAG: shift the set by per-motion world delta */
        flow_pt w = flow_to_world(f, scr);
        int dx = w.x - f->drag_last_world.x, dy = w.y - f->drag_last_world.y;
        if (dx || dy) {
          /* Apply the delta ONLY to selection ROOTS — selected nodes with no SELECTED
             ancestor. A root's selected descendants follow for free via relative coords;
             applying the delta to them too would double-move. flow_move_node is absolute-in,
             so a root moves to node_abs+delta. (All-top-level sets: every node is a root =>
             byte-identical to the prior per-node delta.) */
          for (int i = 0; i < f->nnodes; i++) {
            if (!(f->nodes[i].flags & FLOW_SELECTED)) continue;
            int root = 1;                          /* root unless a STRICT ancestor is selected */
            int parent = f->nodes[i].parent, guard = 0;
            while (parent != -1 && guard++ < 1024) {
              flow_node *pn = flow_get_node(f, parent); if (!pn) break;
              if (pn->flags & FLOW_SELECTED) { root = 0; break; }
              parent = pn->parent;
            }
            if (!root) continue;
            flow_pt a = flow_node_abs(f, &f->nodes[i]);
            flow_move_node(f, f->nodes[i].id, (flow_pt){ a.x + dx, a.y + dy });
          }
          f->drag_last_world = w;
        }
      } else {                                    /* single node: grab-offset move (unchanged) */
        flow_pt w = flow_to_world(f, scr);
        flow_move_node(f, f->drag_node, (flow_pt){ w.x - f->drag_grab.x, w.y - f->drag_grab.y });
      }
    } else if (f->dragging_pan) {
      flow_pan(f, scr.x - f->last_mouse.x, scr.y - f->last_mouse.y);
      f->last_mouse = scr;
    }
  } else if (ev->type == FLOW_MOUSE_RELEASE) {
    if (f->conn_active) {
      if (f->moved) {                            /* drag-connect: complete on target, else cancel */
        flow__resolve_connection_at(f, scr);
        f->mouse_down = 0; f->moved = 0;
      }
      /* else: the click that BEGAN the connection — stay armed for connectOnClick */
      return;
    }
    if (f->reconnect_edge != -1) {               /* finish an endpoint-reconnect drag */
      if (f->moved) {                            /* dragged: repoint onto a valid node, else leave unchanged */
        int hit = flow_hit_node(f, scr);
        if (hit != -1) flow_reconnect_edge(f, f->reconnect_edge, hit, "", f->reconnect_which);
      } else {                                   /* click on the endpoint, no drag: select the edge */
        flow_select_edge(f, f->reconnect_edge, 0);
      }
      f->reconnect_edge = -1; f->mouse_down = 0; f->moved = 0; f->down_node = -1;
      f->drag_node = -1; f->dragging_pan = 0; f->down_modsel = 0;
      f->marquee_active = 0; f->marquee_on = 0;
      flow__undo_end(f);                       /* pairs with the press-time begin (no-drag click: empty txn) */
      return;
    }
    if (f->mouse_down && !f->moved) {            /* a click, not a drag */
      if (f->space_held) {
        /* space-pan click with no motion: a grab without a move. Do NOT clear the
           selection, fire on_pane_click, or select an edge — the cursor may be over a
           node body, so reporting a pane/edge click would violate the callback contract. */
      } else if (f->down_modsel) {
        /* shift/ctrl-click already applied on press: do NOT replace, do NOT fire on_node_click */
        f->last_click_node = -1;                 /* a modifier-click breaks any double-click pair */
        f->last_click_edge = -1;
      } else if (f->down_node != -1) {
        flow_select_node(f, f->down_node, 0);
        if (f->cb.on_node_click) f->cb.on_node_click(f, f->down_node, f->cb.user);
        if (f->down_node == f->last_click_node) { /* 2nd consecutive plain click on the SAME node: double-click */
          if (f->cb.on_node_dblclick) f->cb.on_node_dblclick(f, f->down_node, f->cb.user); /* fires AFTER on_node_click */
          f->last_click_node = -1;               /* consume the pair so a 3rd click starts fresh */
        } else {
          f->last_click_node = f->down_node;
        }
        f->last_click_edge = -1;                 /* a node click breaks the edge dblclick pair */
      } else {
        int eclick = flow_hit_edge(f, scr, 1);   /* edge-body click-select before clearing/pane-click */
        if (eclick != -1) {
          flow_select_edge(f, eclick, 0);
          /* edge observer trio, mirroring the node branch exactly: select -> click -> dblclick */
          if (f->cb.on_edge_click) f->cb.on_edge_click(f, eclick, f->cb.user);
          if (eclick == f->last_click_edge) {    /* 2nd consecutive click on the SAME edge: double-click */
            if (f->cb.on_edge_dblclick) f->cb.on_edge_dblclick(f, eclick, f->cb.user); /* fires AFTER on_edge_click */
            f->last_click_edge = -1;             /* consume the pair */
          } else {
            f->last_click_edge = eclick;
          }
        } else {
          flow_clear_selection(f);
          if (f->cb.on_pane_click) f->cb.on_pane_click(f, flow_to_world(f, scr), f->cb.user);
          f->last_click_edge = -1;               /* a pane click breaks the edge pair */
        }
        f->last_click_node = -1;                 /* edge/pane click breaks the node pair */
      }
    } else if (f->moved && f->drag_node != -1 && flow_selected_count(f) == 1) {
      /* DRAG-TO-REPARENT (single-node drag only for v1). On drop, hit-test the cursor for a
         `group` node — skipping the dragged node AND its own descendants (avoid self-parent).
         A group hit that differs from the current parent reparents (abs preserved => the node
         stays visually put). Dropping on the empty pane while parented detaches to top level.
         flow_hit_node returns the TOPMOST descendant, so we walk the array for a group whose
         footprint contains the drop cell, honouring the same skip rule. */
      int dragged = f->drag_node;
      flow_node *dn = flow_get_node(f, dragged);
      int cur_parent = dn ? dn->parent : -1;
      int target = -1;                            /* the group to drop into, or -1 */
      int lod = flow__lod_for(f, f->view.zoom);
      int *order = flow__node_order(f, 0);        /* topmost-descendant first */
      for (int k = 0; k < f->nnodes; k++) {
        flow_node *cand = &f->nodes[order ? order[k] : k];
        if (strcmp(cand->type, "group") != 0) continue;          /* only group containers */
        if (flow_is_ancestor(f, dragged, cand->id)) continue;    /* skip self + own descendants */
        flow_rect fp = flow__node_footprint(f, cand, lod);
        if (flow_rect_contains(fp, scr)) { target = cand->id; break; }
      }
      free(order);
      if (target != -1) {
        if (target != cur_parent) flow_set_parent(f, dragged, target);
      } else if (cur_parent != -1) {
        flow_set_parent(f, dragged, -1);          /* dropped on empty pane: detach to top level */
      }
    }
    /* marquee finalize: selection already applied during motion; just clear state.
       A marqueed drag sets moved==1, so on_pane_click was never fired. */
    if (f->drag_node != -1) flow__undo_end(f);   /* close the drag gesture txn (begin fired iff drag armed);
                                                    placed AFTER drop-reparent so it joins the same undo step */
    if (f->moved) f->last_click_node = -1;       /* any drag breaks a double-click pair */
    f->mouse_down = 0; f->moved = 0; f->drag_node = -1; f->dragging_pan = 0; f->down_node = -1;
    f->down_modsel = 0; f->marquee_active = 0; f->marquee_on = 0;
  }
}
#endif
