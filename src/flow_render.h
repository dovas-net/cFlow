/* ===== compose: render edges + nodes into a cell buffer ===== */
void flow_render(flow_t *f, flow_cell *out, int cols, int rows);

#ifdef FLOW_IMPLEMENTATION
/* FLOW_BG_GRID_FG (the grid's dim fg) relocated to flow_cell.h so flow_new can
   seed theme.grid_fg from it; the grid put below now reads f->theme.grid_fg. */
/* inc-6 #5 marching-ants: an animated edge's path cell is LIT when (cell_index + tick) %
   FLOW_DASH_PERIOD == 0 (every-other cell — the cadence the connection preview proves reads
   at TUI granularity); off-phase cells are skipped. Named so a longer ant pattern is a one-
   constant change. */
#define FLOW_DASH_PERIOD 2u
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
    if (ch) flow_cellbuf_put(cb, sx, sy, ch, f->theme.grid_fg, f->theme.bg, 0);
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
  flow_box(&s, 0, 0, bw, bh, f->theme.fg, f->theme.bg, 0);
  /* opaque panel: blank the interior so the background grid / edges / nodes
     underneath don't bleed through (flow_box strokes the border only) */
  for (int yy = 1; yy < bh - 1; yy++)
    for (int xx = 1; xx < bw - 1; xx++)
      flow_put(&s, xx, yy, ' ', f->theme.fg, f->theme.bg, 0);
  int iw = bw - 2, ih = bh - 2;
  /* world window encompasses all nodes and the current screen rect. Project BOTH
     screen corners so the world viewport size is zoom-correct (at zoom==1 this
     yields w/h == cb->w/cb->h, so the existing minimap asserts still hold). */
  flow_pt w0 = flow_to_world(f, (flow_pt){0, 0});
  flow_pt w1 = flow_to_world(f, (flow_pt){cb->w, cb->h});
  flow_rect vp = { w0.x, w0.y, w1.x - w0.x, w1.y - w0.y };
  /* visible footprint, not node count: flow_bounds is visible-only and returns a
     zero rect when the graph is empty OR fully hidden — unioning that zero rect
     would treat it as a point at the world origin and mis-scale the viewport rect */
  flow_rect b = flow_bounds(f);
  flow_rect W = (b.w > 0 || b.h > 0) ? flow_rect_union(b, vp) : vp;
  if (W.w < 1) W.w = 1; if (W.h < 1) W.h = 1;
  /* viewport rectangle first, node dots on top */
  int vx  = (vp.x - W.x) * iw / W.w,             vy  = (vp.y - W.y) * ih / W.h;
  int vx2 = (vp.x + vp.w - 1 - W.x) * iw / W.w,  vy2 = (vp.y + vp.h - 1 - W.y) * ih / W.h;
  if (vx < 0) vx = 0; if (vy < 0) vy = 0;
  if (vx2 > iw - 1) vx2 = iw - 1; if (vy2 > ih - 1) vy2 = ih - 1;
  for (int x = vx; x <= vx2; x++) { flow_put(&s, 1+x, 1+vy, 0x2500, f->theme.fg, f->theme.bg, 0); flow_put(&s, 1+x, 1+vy2, 0x2500, f->theme.fg, f->theme.bg, 0); }
  for (int y = vy; y <= vy2; y++) { flow_put(&s, 1+vx, 1+y, 0x2502, f->theme.fg, f->theme.bg, 0); flow_put(&s, 1+vx2, 1+y, 0x2502, f->theme.fg, f->theme.bg, 0); }
  for (int i = 0; i < f->nnodes; i++) {
    flow_node *n = &f->nodes[i];
    if (!flow__node_visible(f, n)) continue;     /* hidden nodes get no minimap dot */
    flow_pt a = flow_node_abs(f, n);
    int cx = a.x + n->w / 2, cy = a.y + n->h / 2;
    int mx = (cx - W.x) * iw / W.w, my = (cy - W.y) * ih / W.h;
    if (mx < 0) mx = 0; if (mx > iw - 1) mx = iw - 1;
    if (my < 0) my = 0; if (my > ih - 1) my = ih - 1;
    int sel = n->flags & FLOW_SELECTED;
    flow_put(&s, 1+mx, 1+my, sel ? 0x25C9 : 0x2022, f->theme.fg, f->theme.bg, sel ? FLOW_BOLD : 0);  /* ◉ / • */
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
/* inc-7 #3: the Controls bar — a one-row [+][-][fit][lock] strip drawn in the widget tier
   (after the minimap, before the app overlay). Each button is a 3-cell [g] group, and the
   SAME loop records each as a widgets[] hit-rect so the drawn region is exactly the
   hittable region (the flow__handle_screen render/hit single-source discipline). Chrome
   color is theme.widget_fg/bg so it tracks color_mode. The lock glyph differs when locked. */
static void flow__controls(flow_t *f, flow_cellbuf *cb) {
  enum { NBTN = 4, BW = NBTN * 3 };
  int ox, oy;
  switch (f->controls.corner) {
    case FLOW_CORNER_TL: ox = 0;          oy = 0;          break;
    case FLOW_CORNER_TR: ox = cb->w - BW; oy = 0;          break;
    case FLOW_CORNER_BL: ox = 0;          oy = cb->h - 1;  break;
    default:             ox = cb->w - BW; oy = cb->h - 1;  break;  /* BR */
  }
  static const uint32_t icon[NBTN] = { '+', '-', 0x26F6, 0 };  /* ⛶ fit; [3] lock glyph chosen per state */
  static const int      act[NBTN]  = { FLOW_WIDGET_ZOOM_IN, FLOW_WIDGET_ZOOM_OUT, FLOW_WIDGET_FIT, FLOW_WIDGET_LOCK };
  for (int k = 0; k < NBTN; k++) {
    int bx = ox + k * 3;
    uint32_t g = (k == 3) ? (f->locked ? 0x25CF : 0x25CB) : icon[k];   /* ● locked / ○ unlocked */
    flow_cellbuf_put(cb, bx,     oy, '[', f->theme.widget_fg, f->theme.widget_bg, 0);
    flow_cellbuf_put(cb, bx + 1, oy, g,   f->theme.widget_fg, f->theme.widget_bg, 0);
    flow_cellbuf_put(cb, bx + 2, oy, ']', f->theme.widget_fg, f->theme.widget_bg, 0);
    if (f->nwidgets < (int)(sizeof f->widgets / sizeof f->widgets[0])) {
      f->widgets[f->nwidgets].x = bx; f->widgets[f->nwidgets].y = oy;
      f->widgets[f->nwidgets].w = 3;  f->widgets[f->nwidgets].h = 1;
      f->widgets[f->nwidgets].owner = FLOW_WIDGET_OWNER_CONTROLS;
      f->widgets[f->nwidgets].action = act[k];
      f->nwidgets++;
    }
  }
}
/* inc-7 #4: display-cell (codepoint) count of a label — flow_text draws one cell each. */
static int flow__label_cells(const char *s) {
  int n = 0; if (!s) return 0;
  while (*s) { uint32_t cp; s += flow_utf8_decode(s, &cp); n++; }
  return n;
}
/* inc-7 #4: the node toolbar — a one-row action strip anchored one row ABOVE the single
   selected node's footprint (flips below when there's no room), left-aligned to the node
   and clamped on-screen. Draws each action label and records its rect in the SHARED
   widgets[] cache (owner NODE_TOOLBAR, action = index) so the press hit-test routes to
   exactly what's drawn. Constant screen-cell width (only the anchor scales with zoom).
   Chrome color is theme.widget_fg/bg so it tracks color_mode. */
static void flow__node_toolbar(flow_t *f, flow_cellbuf *cb) {
  if (!f->node_toolbar.actions || f->node_toolbar.n <= 0) return;
  if (flow_selected_count(f) != 1) return;
  flow_node *n = flow_get_node(f, flow_selected_node(f));
  if (!n) return;
  flow_rect fp = flow__node_footprint(f, n, flow__lod_for(f, f->view.zoom));
  int total = 0;                                   /* width = sum(label cells) + (n-1) separators */
  for (int k = 0; k < f->node_toolbar.n; k++) { total += flow__label_cells(f->node_toolbar.actions[k].label); if (k) total += 1; }
  if (total <= 0) return;
  int sx = fp.x, sy = fp.y - 1;                    /* left-align to node, one row above */
  if (sy < 0) sy = fp.y + fp.h;                    /* no room above: flip below */
  if (sx + total > cb->w) sx = cb->w - total;      /* clamp on-screen (visible == hittable) */
  if (sx < 0) sx = 0;
  flow_surface s = { cb, 0, 0, cb->w, cb->h, 0, 0, cb->w, cb->h };
  int cx = sx;
  for (int k = 0; k < f->node_toolbar.n; k++) {
    const char *label = f->node_toolbar.actions[k].label ? f->node_toolbar.actions[k].label : "";
    int w = flow__label_cells(label);
    flow_text(&s, cx, sy, label, f->theme.widget_fg, f->theme.widget_bg, 0);
    if (w > 0 && f->nwidgets < (int)(sizeof f->widgets / sizeof f->widgets[0])) {
      f->widgets[f->nwidgets].x = cx; f->widgets[f->nwidgets].y = sy;
      f->widgets[f->nwidgets].w = w;  f->widgets[f->nwidgets].h = 1;
      f->widgets[f->nwidgets].owner = FLOW_WIDGET_OWNER_NODE_TOOLBAR;
      f->widgets[f->nwidgets].action = k;
      f->nwidgets++;
    }
    cx += w + 1;                                   /* label + 1-cell separator */
  }
}
/* inc-7 #5: the edge toolbar — a floating action bar on the single selected edge, anchored
   one row ABOVE the route midpoint. The route is NOT stored on the edge, so recompute it for
   the selected edge EXACTLY as the edge draw loop does (shared screen-ends + route vtable),
   read label_anchor (screen coords), and free immediately so no later return leaks. Records
   cells in the shared widgets[] cache (owner EDGE_TOOLBAR). Mutually exclusive with the node
   toolbar by the selection model (no arbitration). Chrome color tracks color_mode. */
static void flow__edge_toolbar(flow_t *f, flow_cellbuf *cb) {
  if (!f->edge_toolbar.actions || f->edge_toolbar.n <= 0) return;
  int eid = flow_selected_edge(f);
  if (eid == -1) return;
  flow_edge *e = flow_get_edge(f, eid);
  if (!e || !flow__edge_visible(f, e)) return;
  flow_pt ss, ts; flow_pos sp, tp;
  if (!flow__edge_screen_ends(f, e, &ss, &sp, &ts, &tp)) return;
  const flow_edge_type *et = flow_edge_type_for(f, e->type[0] ? e->type : "default");
  if (!et) et = &flow_default_edge_type;
  flow_route rt = {0};
  et->route(ss, sp, ts, tp, &rt);
  flow_pt anchor = rt.label_anchor;
  free(rt.cells);                                   /* anchor copied out; every later return is leak-free */
  int total = 0;
  for (int k = 0; k < f->edge_toolbar.n; k++) { total += flow__label_cells(f->edge_toolbar.actions[k].label); if (k) total += 1; }
  if (total <= 0) return;
  int sx = anchor.x, sy = anchor.y - 1;             /* one row ABOVE the midpoint (wire stays visible) */
  if (sy < 0) sy = 0;                                /* clamp to row 0 rather than off-buffer */
  if (sx + total > cb->w) sx = cb->w - total;
  if (sx < 0) sx = 0;
  flow_surface s = { cb, 0, 0, cb->w, cb->h, 0, 0, cb->w, cb->h };
  int cx = sx;
  for (int k = 0; k < f->edge_toolbar.n; k++) {
    const char *label = f->edge_toolbar.actions[k].label ? f->edge_toolbar.actions[k].label : "";
    int w = flow__label_cells(label);
    flow_text(&s, cx, sy, label, f->theme.widget_fg, f->theme.widget_bg, 0);
    if (w > 0 && f->nwidgets < (int)(sizeof f->widgets / sizeof f->widgets[0])) {
      f->widgets[f->nwidgets].x = cx; f->widgets[f->nwidgets].y = sy;
      f->widgets[f->nwidgets].w = w; f->widgets[f->nwidgets].h = 1;
      f->widgets[f->nwidgets].owner = FLOW_WIDGET_OWNER_EDGE_TOOLBAR;
      f->widgets[f->nwidgets].action = k;
      f->nwidgets++;
    }
    cx += w + 1;
  }
}
void flow_render(flow_t *f, flow_cell *out, int cols, int rows) {
  flow_cellbuf cb = { out, cols, rows };
  flow_cellbuf_clear(&cb, f->theme.fg, f->theme.bg);

  /* background grid first (under edges/nodes, so it scrolls with pan) */
  if (f->bg.variant != FLOW_BG_NONE) flow__background(f, &cb);

  /* edges first (drawn under nodes). Endpoints/facings via the shared helper so the
     hit-test (flow_hit_edge) and this draw can never drift apart. */
  int elod = flow__lod_for(f, f->view.zoom);   /* inc-6 #5: marching-ants are suppressed at LOD 1 (per-cell dash is illegible zoomed out) */
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
    int animated = (e->flags & FLOW_ANIMATED) && elod == 0;     /* inc-6 #5: tick-phased dash at LOD 0 only */
    for (int c = 0; c < rt.count; c++) {
      if (animated && c < rt.count - 1 && ((c + f->tick) % FLOW_DASH_PERIOD) != 0)
        continue;                                               /* off-phase path cell: skip (gap shows backdrop). Arrowhead c==count-1 is exempt → always solid. */
      flow_cellbuf_put(&cb, rt.cells[c].x, rt.cells[c].y, rt.cells[c].ch, f->theme.edge_fg, f->theme.bg, attr);
    }
    if (e->label) {                                            /* label on top of the path at the router anchor (screen coords), clipped */
      const char *u = e->label; int gx = rt.label_anchor.x;
      while (*u) { uint32_t cp; int n = flow_utf8_decode(u, &cp); u += n;
        flow_cellbuf_put(&cb, gx++, rt.label_anchor.y, cp, f->theme.edge_fg, f->theme.bg, attr); }
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
      /* viewport cull (inc-5 #9): skip a node whose SCREEN FOOTPRINT misses the
         buffer. Screen space on purpose — footprints are constant-size (only
         position scales with zoom, src/flow_model.h flow__node_footprint), so a
         world-rect test would under-cover the drawn area at zoom<1 and drop
         on-screen cells. flow_rect_intersects' closed convention over-includes by
         a cell per edge; the per-cell clamps downstream decide what draws, so the
         cull can never drop a visible cell. RENDER-LOOP-ONLY (pinned): never
         inside flow__node_visible, which hit/marquee/bounds/minimap share. */
      if (!flow_rect_intersects(flow__node_footprint(f, n, lod),
                                (flow_rect){ 0, 0, cols, rows })) continue;
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
      uint8_t hfg = f->theme.handle;
      /* inc-7 #2: recolor the candidate target handle green/red — the live connection cue.
         Matched the same way the source-bold check matches conn_handle. LOD-0 only: the
         collapsed LOD-1 marker is not a legible per-handle affordance (parity with the
         marching-ants LOD-0 gate). Validity itself (flow_connection_valid) is LOD-independent. */
      if (f->conn_active && elod == 0 && h && n->id == f->conn_target_node &&
          strncmp(h->id, f->conn_target_handle, sizeof h->id) == 0)
        hfg = f->conn_valid ? f->theme.handle_valid : f->theme.handle_invalid;
      flow_cellbuf_put(&cb, s.x, s.y, 0x25C9, hfg, f->theme.bg, bold);  /* ◉ */
    }
  }

  /* focus ring (inc-5 #5): reverse-video re-stamp of the focused node's drawn border.
     A POST-PASS because focus is an id on struct flow, not a node flag — the body
     renderer (ctx.flags = n->flags) is blind to it. AFTER the handle markers, which
     flow_cellbuf_put with a fresh attr: OR-ing REVERSE here keeps a focused+selected
     node's ◉ cells inside the ring (no gaps at handle anchors). Footprint-aware: at
     LOD collapse the ring reverses the single marker cell. Glyphs/colors untouched. */
  if (f->focus_node != -1) {
    flow_node *fn = flow_get_node(f, f->focus_node);
    if (fn && flow__node_visible(f, fn)) {
      flow_rect r = flow__node_footprint(f, fn, flow__lod_for(f, f->view.zoom));
      for (int y = r.y; y < r.y + r.h; y++) {
        if (y < 0 || y >= rows) continue;
        for (int x = r.x; x < r.x + r.w; x++) {
          if (x < 0 || x >= cols) continue;
          if (y != r.y && y != r.y + r.h - 1 && x != r.x && x != r.x + r.w - 1) continue; /* border only */
          cb.cells[y * cb.w + x].attr |= FLOW_REVERSE;
        }
      }
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
          flow_cellbuf_put(&cb, rt.cells[c].x, rt.cells[c].y, rt.cells[c].ch, f->theme.edge_fg, f->theme.bg, 0);
      free(rt.cells);
    }
  }

  /* marquee box (after nodes, before minimap/overlay so app panels still win).
     The anchor corner is the WORLD-pinned press point re-projected each frame
     (inc-5 #3) so the drawn border tracks the GROWN rect after auto-pan; cur is
     already current screen. At zoom!=1 the round-trip can land ±1 cell from the
     raw press cell — acceptable for a 1-cell-glyph overlay. */
  if (f->marquee_on) {
    flow_pt ma = flow_to_screen(f, f->marquee_anchor_world);
    int x0 = ma.x, x1 = f->marquee_cur.x;
    int y0 = ma.y, y1 = f->marquee_cur.y;
    if (x1 < x0) { int t = x0; x0 = x1; x1 = t; }
    if (y1 < y0) { int t = y0; y0 = y1; y1 = t; }
    for (int x = x0; x <= x1; x++) {                       /* horizontal edges */
      flow_cellbuf_put(&cb, x, y0, 0x2592, f->theme.fg, f->theme.bg, 0);  /* ▒ */
      flow_cellbuf_put(&cb, x, y1, 0x2592, f->theme.fg, f->theme.bg, 0);
    }
    for (int y = y0; y <= y1; y++) {                       /* vertical edges */
      flow_cellbuf_put(&cb, x0, y, 0x2592, f->theme.fg, f->theme.bg, 0);
      flow_cellbuf_put(&cb, x1, y, 0x2592, f->theme.fg, f->theme.bg, 0);
    }
  }

  /* alignment guide rules (inc-5 #8): full-row/column dashed lines for the active
     helper guides — after the marquee box (mutually exclusive states anyway),
     before minimap/overlay/statusbar so app panels still win. World lines are
     projected per frame; dashed glyphs (╎ 0x254E / ╌ 0x254C) read as transient
     and stay distinct from the marquee ▒, handles ◉, and the SOLID grid │/─. */
  for (int g = 0; g < f->helper.nvert; g++) {
    int sx = flow_to_screen(f, (flow_pt){ f->helper.vert[g], 0 }).x;
    if (sx < 0 || sx >= cols) continue;
    for (int y = 0; y < rows; y++) flow_cellbuf_put(&cb, sx, y, 0x254E, f->theme.fg, f->theme.bg, 0);
  }
  for (int g = 0; g < f->helper.nhorz; g++) {
    int sy = flow_to_screen(f, (flow_pt){ 0, f->helper.horz[g] }).y;
    if (sy < 0 || sy >= rows) continue;
    for (int x = 0; x < cols; x++) flow_cellbuf_put(&cb, x, sy, 0x254C, f->theme.fg, f->theme.bg, 0);
  }

  if (f->minimap.enabled) flow__minimap(f, &cb);
  f->nwidgets = 0;                                   /* inc-7 #3: refill the widget hit-rect cache each frame (controls + #4/#5 toolbars append below) */
  if (f->controls.enabled) flow__controls(f, &cb);
  flow__node_toolbar(f, &cb);                        /* inc-7 #4: gated internally on actions + single selection */
  flow__edge_toolbar(f, &cb);                        /* inc-7 #5: gated on actions + a selected edge (exclusive with node toolbar) */
  if (f->cb.on_overlay) { flow_surface ov = { &cb, 0, 0, cols, rows, 0, 0, cols, rows }; f->cb.on_overlay(f, &ov, f->cb.user); }

  /* built-in status/help bar: drawn LAST (after the app overlay) on the bottom
     row only, so it never fights the app's overlay on other rows. */
  if (f->statusbar && rows > 0) {
    flow_surface s = { &cb, 0, rows - 1, cols, 1, 0, 0, cols, rows };  /* full-buffer clip */
    for (int x = 0; x < cols; x++) flow_put(&s, x, 0, ' ', f->theme.fg, f->theme.bg, FLOW_REVERSE);
    /* While space-pan is armed the bar becomes a mode indicator. The normal help
       line APPENDS the newer hints past column 30: the render_statusbar golden is
       rendered at cols=30 and locks only the " n:add ... ?:help" prefix — editing
       that prefix means deliberately regenerating the golden. */
    flow_text(&s, 0, 0, f->space_held
              ? " PAN  drag:pan  Space/Esc:exit "
              : " n:add  x:del  f:fit  ?:help  q:quit  SPC:pan  u:undo  ^r:redo  Tab:focus ",
              f->theme.fg, f->theme.bg, FLOW_REVERSE);
  }
}
#endif
