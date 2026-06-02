/* ===== compose: render edges + nodes into a cell buffer ===== */
void flow_render(flow_t *f, flow_cell *out, int cols, int rows);

#ifdef FLOW_IMPLEMENTATION
void flow_render(flow_t *f, flow_cell *out, int cols, int rows) {
  flow_cellbuf cb = { out, cols, rows };
  flow_cellbuf_clear(&cb, FLOW_FG, FLOW_BG);

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
}
#endif
