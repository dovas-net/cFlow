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
#endif
