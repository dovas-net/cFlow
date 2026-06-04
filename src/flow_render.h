/* ===== compose: render edges + nodes into a cell buffer ===== */
void flow_render(flow_t *f, flow_cell *out, int cols, int rows);

#ifdef FLOW_IMPLEMENTATION
/* Grid "light" comes from a dim 256-color fg, NOT FLOW_DIM: flow_diff_emit
   serializes only BOLD/REVERSE attrs but always the ;38;5;<fg> color path, so a
   DIM-attr grid would look full-intensity on a real terminal. 8 = bright black. */
#define FLOW_BG_GRID_FG 8
static void flow__background(flow_t *f, flow_cellbuf *cb) {
  int gap = f->bg.gap;  /* setter guarantees gap >= 1 when variant != NONE */
  for (int sy = 0; sy < cb->h; sy++) for (int sx = 0; sx < cb->w; sx++) {
    flow_pt w = flow_to_world(f, (flow_pt){ sx, sy });  /* world-align under pan */
    int onx = (w.x % gap) == 0, ony = (w.y % gap) == 0; /* ==0 is sign-safe in C */
    uint32_t ch = 0;
    switch (f->bg.variant) {
      case FLOW_BG_DOTS:  if (onx && ony) ch = 0x00B7; break;            /* · */
      case FLOW_BG_CROSS: if (onx && ony) ch = 0x002B; break;            /* + */
      case FLOW_BG_LINES:
        if (onx && ony) ch = 0x253C;       /* ┼ intersection wins */
        else if (onx)   ch = 0x2502;       /* │ vertical          */
        else if (ony)   ch = 0x2500;       /* ─ horizontal        */
        break;
      default: break;
    }
    if (ch) flow_cellbuf_put(cb, sx, sy, ch, FLOW_BG_GRID_FG, FLOW_BG, 0);
  }
}
static void flow__minimap(flow_t *f, flow_cellbuf *cb) {
  int bw = f->minimap.w, bh = f->minimap.h;
  if (bw < 4 || bh < 4) return;
  int ox, oy;
  switch (f->minimap.corner) {
    case FLOW_CORNER_TL: ox = 0;          oy = 0;          break;
    case FLOW_CORNER_TR: ox = cb->w - bw; oy = 0;          break;
    case FLOW_CORNER_BL: ox = 0;          oy = cb->h - bh; break;
    default:             ox = cb->w - bw; oy = cb->h - bh; break;  /* BR */
  }
  flow_surface s = { cb, ox, oy, bw, bh, 0, 0, cb->w, cb->h };  /* full-buffer clip (no extra clip) */
  flow_box(&s, 0, 0, bw, bh, FLOW_FG, FLOW_BG, 0);
  /* opaque panel: blank the interior so the background grid / edges / nodes
     underneath don't bleed through (flow_box strokes the border only) */
  for (int yy = 1; yy < bh - 1; yy++)
    for (int xx = 1; xx < bw - 1; xx++)
      flow_put(&s, xx, yy, ' ', FLOW_FG, FLOW_BG, 0);
  int iw = bw - 2, ih = bh - 2;
  /* world window encompasses all nodes and the current screen rect. Project BOTH
     screen corners so the world viewport size is zoom-correct (at zoom==1 this
     yields w/h == cb->w/cb->h, so the existing minimap asserts still hold). */
  flow_pt w0 = flow_to_world(f, (flow_pt){0, 0});
  flow_pt w1 = flow_to_world(f, (flow_pt){cb->w, cb->h});
  flow_rect vp = { w0.x, w0.y, w1.x - w0.x, w1.y - w0.y };
  flow_rect W = f->nnodes ? flow_rect_union(flow_bounds(f), vp) : vp;
  if (W.w < 1) W.w = 1; if (W.h < 1) W.h = 1;
  /* viewport rectangle first, node dots on top */
  int vx  = (vp.x - W.x) * iw / W.w,             vy  = (vp.y - W.y) * ih / W.h;
  int vx2 = (vp.x + vp.w - 1 - W.x) * iw / W.w,  vy2 = (vp.y + vp.h - 1 - W.y) * ih / W.h;
  if (vx < 0) vx = 0; if (vy < 0) vy = 0;
  if (vx2 > iw - 1) vx2 = iw - 1; if (vy2 > ih - 1) vy2 = ih - 1;
  for (int x = vx; x <= vx2; x++) { flow_put(&s, 1+x, 1+vy, 0x2500, FLOW_FG, FLOW_BG, 0); flow_put(&s, 1+x, 1+vy2, 0x2500, FLOW_FG, FLOW_BG, 0); }
  for (int y = vy; y <= vy2; y++) { flow_put(&s, 1+vx, 1+y, 0x2502, FLOW_FG, FLOW_BG, 0); flow_put(&s, 1+vx2, 1+y, 0x2502, FLOW_FG, FLOW_BG, 0); }
  for (int i = 0; i < f->nnodes; i++) {
    flow_node *n = &f->nodes[i];
    if (!flow__node_visible(f, n)) continue;     /* hidden nodes get no minimap dot */
    flow_pt a = flow_node_abs(f, n);
    int cx = a.x + n->w / 2, cy = a.y + n->h / 2;
    int mx = (cx - W.x) * iw / W.w, my = (cy - W.y) * ih / W.h;
    if (mx < 0) mx = 0; if (mx > iw - 1) mx = iw - 1;
    if (my < 0) my = 0; if (my > ih - 1) my = ih - 1;
    int sel = n->flags & FLOW_SELECTED;
    flow_put(&s, 1+mx, 1+my, sel ? 0x25C9 : 0x2022, FLOW_FG, FLOW_BG, sel ? FLOW_BOLD : 0);  /* ◉ / • */
  }
}
/* nudge a (world) anchor one cell OUTSIDE the node along the handle's facing */
static flow_pt flow__anchor_outward(flow_pt a, flow_pos pos) {
  switch (pos) {
    case FLOW_TOP:    a.y -= 1; break;
    case FLOW_BOTTOM: a.y += 1; break;
    case FLOW_LEFT:   a.x -= 1; break;
    default:          a.x += 1; break;  /* RIGHT */
  }
  return a;
}
/* resolve an edge endpoint's handle: named id -> nearest by source/target kind ->
   first handle (so flow_handle_anchor gives a side); NULL only if the type has none. */
static const flow_handle *flow__edge_handle(flow_t *f, flow_node *n, const char *id, int want_source) {
  int hc = flow_node_handle_count(f, n->id);
  if (hc <= 0) return NULL;
  if (id && id[0]) for (int j = 0; j < hc; j++) {            /* named match wins */
    const flow_handle *h = flow_node_handle_at(f, n->id, j);
    if (h && strncmp(h->id, id, sizeof h->id) == 0) return h;
  }
  flow_handle_kind kind = want_source ? FLOW_HANDLE_SOURCE : FLOW_HANDLE_TARGET;
  for (int j = 0; j < hc; j++) {                             /* first matching kind */
    const flow_handle *h = flow_node_handle_at(f, n->id, j);
    if (h && (h->kind == kind || h->kind == FLOW_HANDLE_BOTH)) return h;
  }
  return flow_node_handle_at(f, n->id, 0);                   /* fall back to first */
}
/* Screen endpoints + facings for an edge, matching EXACTLY what flow_render draws:
   declared-handle -> flow__handle_screen (anchors to the constant-size footprint) ->
   one-cell SCREEN-space outward nudge. The render edge loop, flow_hit_edge and
   flow_edge_endpoint_screen all go through this so hit-test and render can never
   drift, and endpoints stay attached to node bodies at any zoom (the nudge is in
   screen space so it's "one screen cell outside the constant body").
   Returns 0 if either node is missing. */
static int flow__edge_screen_ends(flow_t *f, flow_edge *e, flow_pt *ss, flow_pos *sp, flow_pt *ts, flow_pos *tp) {
  flow_node *sn = flow_get_node(f, e->source), *tn = flow_get_node(f, e->target);
  if (!sn || !tn) return 0;
  const flow_handle *sh = flow__edge_handle(f, sn, e->source_handle, 1);
  const flow_handle *th = flow__edge_handle(f, tn, e->target_handle, 0);
  flow_pos lsp = sh ? sh->pos : FLOW_RIGHT, ltp = th ? th->pos : FLOW_LEFT;
  if (ss) *ss = flow__anchor_outward(flow__handle_screen(f, sn, sh), lsp);
  if (ts) *ts = flow__anchor_outward(flow__handle_screen(f, tn, th), ltp);
  if (sp) *sp = lsp;
  if (tp) *tp = ltp;
  return 1;
}
int flow_edge_endpoint_screen(flow_t *f, const flow_edge *e, int which, flow_pt *out) {
  flow_pt ss, ts;
  if (!flow__edge_screen_ends(f, (flow_edge*)e, &ss, NULL, &ts, NULL)) return 0;  /* cast: helper takes non-const */
  if (out) *out = which == 0 ? ss : ts;
  return 1;
}
int flow_hit_edge(flow_t *f, flow_pt screen, int tol) {
  for (int i = flow_edge_count(f) - 1; i >= 0; i--) {         /* topmost first (reverse, like flow_hit_node) */
    flow_edge *e = &flow_edges(f)[i];
    if (!flow__edge_visible(f, e)) continue;                  /* hidden / cascaded: not hittable */
    flow_pt ss, ts; flow_pos sp, tp;
    if (!flow__edge_screen_ends(f, e, &ss, &sp, &ts, &tp)) continue;
    const flow_edge_type *et = flow_edge_type_for(f, e->type[0] ? e->type : "default");
    if (!et) et = &flow_default_edge_type;
    flow_route rt = {0};
    et->route(ss, sp, ts, tp, &rt);
    int hit = 0;
    for (int c = 0; c < rt.count; c++) {
      int dx = rt.cells[c].x - screen.x, dy = rt.cells[c].y - screen.y;
      if (dx < 0) dx = -dx; if (dy < 0) dy = -dy;
      if ((dx > dy ? dx : dy) <= tol) { hit = 1; break; }     /* Chebyshev distance */
    }
    free(rt.cells);
    if (hit) return e->id;
  }
  return -1;
}
/* Buffer-space clip for a node = intersection of the screen rect and EVERY ancestor's
   drawn footprint (so a child is cut to its parent's frame, composing through zoom because
   flow__node_footprint projects abs). Top-level nodes (no ancestors) get the full screen
   rect — byte-identical to the unclipped behaviour. Returned rect is in BUFFER coords;
   an empty result (w<=0/h<=0) suppresses all of the node's writes via flow_put. */
static flow_rect flow__node_clip(flow_t *f, const flow_node *n, int lod, int cols, int rows) {
  flow_rect clip = { 0, 0, cols, rows };
  int parent = n->parent, guard = 0;
  while (parent != -1 && guard++ < 1024) {
    flow_node *pn = flow_get_node(f, parent); if (!pn) break;
    flow_rect fp = flow__node_footprint(f, pn, lod);   /* ancestor's drawn screen rect */
    int x0 = clip.x > fp.x ? clip.x : fp.x;
    int y0 = clip.y > fp.y ? clip.y : fp.y;
    int x1 = (clip.x + clip.w) < (fp.x + fp.w) ? (clip.x + clip.w) : (fp.x + fp.w);
    int y1 = (clip.y + clip.h) < (fp.y + fp.h) ? (clip.y + clip.h) : (fp.y + fp.h);
    clip.x = x0; clip.y = y0; clip.w = x1 - x0; clip.h = y1 - y0;
    if (clip.w < 0) clip.w = 0; if (clip.h < 0) clip.h = 0;
    parent = pn->parent;
  }
  return clip;
}
void flow_render(flow_t *f, flow_cell *out, int cols, int rows) {
  flow_cellbuf cb = { out, cols, rows };
  flow_cellbuf_clear(&cb, FLOW_FG, FLOW_BG);

  /* background grid first (under edges/nodes, so it scrolls with pan) */
  if (f->bg.variant != FLOW_BG_NONE) flow__background(f, &cb);

  /* edges first (drawn under nodes). Endpoints/facings via the shared helper so the
     hit-test (flow_hit_edge) and this draw can never drift apart. */
  for (int i = 0; i < flow_edge_count(f); i++) {
    flow_edge *e = &flow_edges(f)[i];
    if (!flow__edge_visible(f, e)) continue;   /* hidden edge OR hidden endpoint (cascade) */
    flow_pt ss, ts; flow_pos sp, tp;
    if (!flow__edge_screen_ends(f, e, &ss, &sp, &ts, &tp)) continue;
    const flow_edge_type *et = flow_edge_type_for(f, e->type[0] ? e->type : "default");
    if (!et) et = &flow_default_edge_type;
    flow_route rt = {0};
    et->route(ss, sp, ts, tp, &rt);
    uint8_t attr = (e->flags & FLOW_SELECTED) ? FLOW_BOLD : 0;  /* selected edge: bold path */
    for (int c = 0; c < rt.count; c++)
      flow_cellbuf_put(&cb, rt.cells[c].x, rt.cells[c].y, rt.cells[c].ch, FLOW_FG, FLOW_BG, attr);
    if (e->label) {                                            /* label on top of the path at the router anchor (screen coords), clipped */
      const char *u = e->label; int gx = rt.label_anchor.x;
      while (*u) { uint32_t cp; int n = flow_utf8_decode(u, &cp); u += n;
        flow_cellbuf_put(&cb, gx++, rt.label_anchor.y, cp, FLOW_FG, FLOW_BG, attr); }
    }
    free(rt.cells);
  }

  /* nodes on top — visited in depth-aware order (parent-before-child DOMINATES;
     selected-last applies only among siblings) so a group frame draws UNDER its children
     while still drawing under a selected sibling. Each node's surface carries a clip rect =
     intersection of its ancestor frames, so children are cut to their parent's box.
     For all-top-level scenes this reduces to the prior unselected-then-selected order with
     a full-screen clip — byte-identical output. */
  {
    int lod = flow__lod_for(f, f->view.zoom);
    int *order = flow__node_order(f, 1);
    for (int k = 0; k < flow_node_count(f); k++) {
      flow_node *n = &flow_nodes(f)[order ? order[k] : k];
      if (!flow__node_visible(f, n)) continue;   /* same gate as flow_hit_node */
      flow_rect wr = flow_node_rect_abs(f, n);
      flow_pt s = flow_to_screen(f, (flow_pt){ wr.x, wr.y });
      const flow_node_type *nt = flow_node_type_for(f, n->type);
      if (!nt || !nt->render) continue;
      flow_rect clip = flow__node_clip(f, n, lod, cols, rows);
      flow_surface surf = { &cb, s.x, s.y, n->w, n->h, clip.x, clip.y, clip.w, clip.h };
      flow_render_ctx ctx = { f->view.zoom, n->flags, lod };
      nt->render(n, &surf, ctx);
    }
    free(order);
  }

  /* handle markers: only on hovered/selected nodes (xyflow reveals on hover), plus
     the active connecting source so its marker stays visible mid-drag. ◉ on the border. */
  for (int i = 0; i < flow_node_count(f); i++) {
    flow_node *n = &flow_nodes(f)[i];
    if (!flow__node_handles_visible(f, n)) continue;   /* same gate as flow_hit_handle */
    int hc = flow_node_handle_count(f, n->id);
    for (int j = 0; j < hc; j++) {
      const flow_handle *h = flow_node_handle_at(f, n->id, j);
      flow_pt s = flow__handle_screen(f, n, h);         /* same projection as flow_hit_handle */
      unsigned bold = (n->id == f->conn_node && h &&
                       strncmp(h->id, f->conn_handle, sizeof h->id) == 0) ? FLOW_BOLD : 0;
      flow_cellbuf_put(&cb, s.x, s.y, 0x25C9, FLOW_FG, FLOW_BG, bold);  /* ◉ */
    }
  }

  /* in-flight connection preview: dashed rubber-band from the source handle to the
     free cursor cell. No arrowhead — overwrite the router's last cell with a dash. */
  if (f->conn_active) {
    flow_node *sn = flow_get_node(f, f->conn_node);
    const flow_handle *sh = sn ? flow__handle_named(f, f->conn_node, f->conn_handle) : NULL;
    if (sn) {
      flow_pos sp = sh ? sh->pos : FLOW_RIGHT;
      flow_pt sa = flow__anchor_outward(flow_handle_anchor(f, sn, sh), sp);
      flow_pt ss = flow_to_screen(f, sa);
      flow_route rt = {0};
      flow_route_orthogonal(ss, sp, f->conn_end, FLOW_LEFT, &rt);
      /* skip the LAST cell: the router stamps an arrowhead there — a preview has none. */
      int last = rt.count - 1;
      for (int c = 0; c < last; c++)
        if (((rt.cells[c].x + rt.cells[c].y) & 1) == 0)        /* dashed: every other cell */
          flow_cellbuf_put(&cb, rt.cells[c].x, rt.cells[c].y, rt.cells[c].ch, FLOW_FG, FLOW_BG, 0);
      free(rt.cells);
    }
  }

  /* marquee box (after nodes, before minimap/overlay so app panels still win).
     anchor/cur are SCREEN coords; stroke a normalized border with a distinct glyph. */
  if (f->marquee_on) {
    int x0 = f->marquee_anchor.x, x1 = f->marquee_cur.x;
    int y0 = f->marquee_anchor.y, y1 = f->marquee_cur.y;
    if (x1 < x0) { int t = x0; x0 = x1; x1 = t; }
    if (y1 < y0) { int t = y0; y0 = y1; y1 = t; }
    for (int x = x0; x <= x1; x++) {                       /* horizontal edges */
      flow_cellbuf_put(&cb, x, y0, 0x2592, FLOW_FG, FLOW_BG, 0);  /* ▒ */
      flow_cellbuf_put(&cb, x, y1, 0x2592, FLOW_FG, FLOW_BG, 0);
    }
    for (int y = y0; y <= y1; y++) {                       /* vertical edges */
      flow_cellbuf_put(&cb, x0, y, 0x2592, FLOW_FG, FLOW_BG, 0);
      flow_cellbuf_put(&cb, x1, y, 0x2592, FLOW_FG, FLOW_BG, 0);
    }
  }

  if (f->minimap.enabled) flow__minimap(f, &cb);
  if (f->cb.on_overlay) { flow_surface ov = { &cb, 0, 0, cols, rows, 0, 0, cols, rows }; f->cb.on_overlay(f, &ov, f->cb.user); }

  /* built-in status/help bar: drawn LAST (after the app overlay) on the bottom
     row only, so it never fights the app's overlay on other rows. */
  if (f->statusbar && rows > 0) {
    flow_surface s = { &cb, 0, rows - 1, cols, 1, 0, 0, cols, rows };  /* full-buffer clip */
    for (int x = 0; x < cols; x++) flow_put(&s, x, 0, ' ', FLOW_FG, FLOW_BG, FLOW_REVERSE);
    /* While space-pan is armed the bar becomes a mode indicator. The normal help
       line APPENDS the newer hints past column 30: the render_statusbar golden is
       rendered at cols=30 and locks only the " n:add ... ?:help" prefix — editing
       that prefix means deliberately regenerating the golden. */
    flow_text(&s, 0, 0, f->space_held
              ? " PAN  drag:pan  Space/Esc:exit "
              : " n:add  x:del  f:fit  ?:help  q:quit  SPC:pan  u:undo  ^r:redo ",
              FLOW_FG, FLOW_BG, FLOW_REVERSE);
  }
}
#endif
