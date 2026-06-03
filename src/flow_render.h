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
  flow_surface s = { cb, ox, oy, bw, bh };
  flow_box(&s, 0, 0, bw, bh, FLOW_FG, FLOW_BG, 0);
  /* opaque panel: blank the interior so the background grid / edges / nodes
     underneath don't bleed through (flow_box strokes the border only) */
  for (int yy = 1; yy < bh - 1; yy++)
    for (int xx = 1; xx < bw - 1; xx++)
      flow_put(&s, xx, yy, ' ', FLOW_FG, FLOW_BG, 0);
  int iw = bw - 2, ih = bh - 2;
  /* world window encompasses all nodes and the current screen rect */
  flow_pt w0 = flow_to_world(f, (flow_pt){0, 0});
  flow_rect vp = { w0.x, w0.y, cb->w, cb->h };          /* zoom==1 */
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
    flow_node *n = &f->nodes[i]; flow_pt a = flow_node_abs(f, n);
    int cx = a.x + n->w / 2, cy = a.y + n->h / 2;
    int mx = (cx - W.x) * iw / W.w, my = (cy - W.y) * ih / W.h;
    if (mx < 0) mx = 0; if (mx > iw - 1) mx = iw - 1;
    if (my < 0) my = 0; if (my > ih - 1) my = ih - 1;
    int sel = n->flags & FLOW_SELECTED;
    flow_put(&s, 1+mx, 1+my, sel ? 0x25C9 : 0x2022, FLOW_FG, FLOW_BG, sel ? FLOW_BOLD : 0);  /* ◉ / • */
  }
}
void flow_render(flow_t *f, flow_cell *out, int cols, int rows) {
  flow_cellbuf cb = { out, cols, rows };
  flow_cellbuf_clear(&cb, FLOW_FG, FLOW_BG);

  /* background grid first (under edges/nodes, so it scrolls with pan) */
  if (f->bg.variant != FLOW_BG_NONE) flow__background(f, &cb);

  /* edges first (drawn under nodes) */
  for (int i = 0; i < flow_edge_count(f); i++) {
    flow_edge *e = &flow_edges(f)[i];
    flow_node *sn = flow_get_node(f, e->source), *tn = flow_get_node(f, e->target);
    if (!sn || !tn) continue;
    flow_rect sr = flow_node_rect_abs(f, sn), tr = flow_node_rect_abs(f, tn);
    flow_pt sa = { sr.x + sr.w, sr.y + sr.h / 2 };   /* one cell right of source border */
    flow_pt ta = { tr.x - 1,    tr.y + tr.h / 2 };   /* one cell left of target border  */
    flow_pt ss = flow_to_screen(f, sa), ts = flow_to_screen(f, ta);
    const flow_edge_type *et = flow_edge_type_for(f, e->type[0] ? e->type : "default");
    if (!et) et = &flow_default_edge_type;
    flow_route rt = {0};
    et->route(ss, FLOW_RIGHT, ts, FLOW_LEFT, &rt);
    for (int c = 0; c < rt.count; c++)
      flow_cellbuf_put(&cb, rt.cells[c].x, rt.cells[c].y, rt.cells[c].ch, FLOW_FG, FLOW_BG, 0);
    free(rt.cells);
  }

  /* nodes on top, in insertion order (selected-last refinement comes in a later increment) */
  for (int i = 0; i < flow_node_count(f); i++) {
    flow_node *n = &flow_nodes(f)[i];
    flow_rect wr = flow_node_rect_abs(f, n);
    flow_pt s = flow_to_screen(f, (flow_pt){ wr.x, wr.y });
    const flow_node_type *nt = flow_node_type_for(f, n->type);
    if (!nt || !nt->render) continue;
    flow_surface surf = { &cb, s.x, s.y, n->w, n->h };
    flow_render_ctx ctx = { f->view.zoom, n->flags, 0 };
    nt->render(n, &surf, ctx);
  }

  if (f->minimap.enabled) flow__minimap(f, &cb);
  if (f->cb.on_overlay) { flow_surface ov = { &cb, 0, 0, cols, rows }; f->cb.on_overlay(f, &ov, f->cb.user); }
}
#endif
