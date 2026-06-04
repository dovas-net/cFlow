/* ===== graph queries: incomers / outgoers / connected edges (spec §8 traversal) =====
   MODEL-level read-only queries over f->edges — no view filtering (hidden nodes/edges
   are included, per the inc-4 cross-cutting layering rule), no engine state, no
   allocation. All three use the fill-buffer idiom of flow_selected_nodes: the TRUE
   total count is returned; only out[0..min(count,max)-1] is written (out may be NULL
   with max 0 for a count-only query). IDs are emitted in f->edges insertion order.
   Missing node id: 0, out untouched. */
int flow_incomers(flow_t *f, int node, int *out, int max);        /* distinct source nodes with an edge TO node (multi-edges collapse to one entry) */
int flow_outgoers(flow_t *f, int node, int *out, int max);        /* distinct target nodes reachable FROM node (same dedup) */
int flow_connected_edges(flow_t *f, int node, int *out, int max); /* EVERY edge id incident on node (either endpoint; no dedup) */

/* spatial intersection queries (inc-4 #10, xyflow getIntersectingNodes): closed
   convention via flow_rect_intersects ("touching edges count", flow_geom.h) over
   flow_node_rect_abs. Ancestor pairs are NOT filtered — a group and its nested child
   both report when their rects overlap; apps exclude via flow_is_ancestor if desired.
   MODEL-level like the traversal queries: ALL nodes are swept, with no view-state
   filtering — hidden nodes (#11's FLOW_HIDDEN) stay included here even though
   view-layer marquee selection (flow_select_in_rect, the other flow_rect_intersects
   caller) skips them. Deliberate layering, not a missed filter site. */
int flow_intersecting_nodes(flow_t *f, flow_rect world, int *out, int max); /* all nodes whose absolute rect intersects `world` */
int flow_node_intersections(flow_t *f, int node, int *out, int max);        /* nodes intersecting `node`'s absolute rect, EXCLUDING node itself; missing id -> 0 */

#ifdef FLOW_IMPLEMENTATION
/* shared walk: dir 0 = incomers (edges INTO node, emit sources), dir 1 = outgoers
   (edges FROM node, emit targets). Dedup without allocation: for each candidate,
   re-scan the PRIOR matching edges and re-derive their neighbors — emit only at the
   neighbor's first occurrence. (Not the out buffer: neighbors past `max` are counted
   but unwritten, so dedup cannot rely on what was stored.) O(E^2) worst case; E is
   small in v1 and the queries allocate nothing. */
static int flow__neighbors(flow_t *f, int node, int dir, int *out, int max) {
  if (!flow_get_node(f, node)) return 0;
  int count = 0;
  for (int i = 0; i < f->nedges; i++) {
    flow_edge *e = &f->edges[i];
    int other;
    if (dir == 0) { if (e->target != node) continue; other = e->source; }
    else          { if (e->source != node) continue; other = e->target; }
    int seen = 0;                                /* dedup against ALL previously-emitted ids */
    for (int j = 0; j < f->nedges && j < i; j++) {
      flow_edge *p = &f->edges[j];
      int prev;
      if (dir == 0) { if (p->target != node) continue; prev = p->source; }
      else          { if (p->source != node) continue; prev = p->target; }
      if (prev == other) { seen = 1; break; }
    }
    if (seen) continue;
    if (count < max && out) out[count] = other;
    count++;
  }
  return count;
}
int flow_incomers(flow_t *f, int node, int *out, int max) { return flow__neighbors(f, node, 0, out, max); }
int flow_outgoers(flow_t *f, int node, int *out, int max) { return flow__neighbors(f, node, 1, out, max); }
int flow_connected_edges(flow_t *f, int node, int *out, int max) {
  if (!flow_get_node(f, node)) return 0;
  int count = 0;
  for (int i = 0; i < f->nedges; i++) {
    flow_edge *e = &f->edges[i];
    if (e->source != node && e->target != node) continue;
    if (count < max && out) out[count] = e->id;
    count++;
  }
  return count;
}
/* shared rect sweep: nodes whose abs rect intersects `world`, skipping id `excl`
   (-1 = none). Same fill-buffer idiom as the traversal queries above. */
static int flow__rect_sweep(flow_t *f, flow_rect world, int excl, int *out, int max) {
  int count = 0;
  for (int i = 0; i < f->nnodes; i++) {
    flow_node *n = &f->nodes[i];
    if (n->id == excl) continue;
    if (!flow_rect_intersects(world, flow_node_rect_abs(f, n))) continue;
    if (count < max && out) out[count] = n->id;
    count++;
  }
  return count;
}
int flow_intersecting_nodes(flow_t *f, flow_rect world, int *out, int max) {
  return flow__rect_sweep(f, world, -1, out, max);
}
int flow_node_intersections(flow_t *f, int node, int *out, int max) {
  flow_node *n = flow_get_node(f, node);
  if (!n) return 0;
  return flow__rect_sweep(f, flow_node_rect_abs(f, n), node, out, max);
}
#endif
