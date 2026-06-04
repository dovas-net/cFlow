#define FLOW_IMPLEMENTATION
#include "../flow.h"
#include "flowtest.h"

/* graph queries (work package inc-4 #8, new module src/flow_query.h):
   flow_incomers / flow_outgoers — distinct NEIGHBOR NODES (multi-edges between the
   same pair collapse to one entry); flow_connected_edges — EVERY incident edge id
   (no dedup). All three: insertion order (f->edges array order), true count returned
   even past max (fill-buffer idiom of flow_selected_nodes), missing node id -> 0,
   MODEL-level (no view filtering). */

int main(void) {
  /* ---- fork-join graph: 1->3, 2->3, 4->3 plus 3->5; queries on node 3 ---- */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    int n1 = flow_add_node(f, "default", (flow_pt){0, 0},  (void*)"1");
    int n2 = flow_add_node(f, "default", (flow_pt){0, 5},  (void*)"2");
    int n3 = flow_add_node(f, "default", (flow_pt){20, 2}, (void*)"3");
    int n4 = flow_add_node(f, "default", (flow_pt){0, 10}, (void*)"4");
    int n5 = flow_add_node(f, "default", (flow_pt){40, 2}, (void*)"5");
    int e13 = flow_add_edge(f, n1, n3, "out", "in");
    int e23 = flow_add_edge(f, n2, n3, "out", "in");
    int e43 = flow_add_edge(f, n4, n3, "out", "in");
    int e35 = flow_add_edge(f, n3, n5, "out", "in");

    int out[8];
    ASSERT_INT(flow_incomers(f, n3, out, 8), 3, "node 3 has 3 incomers");
    ASSERT_INT(out[0], n1, "  incomers[0]==1 (edge insertion order)");
    ASSERT_INT(out[1], n2, "  incomers[1]==2");
    ASSERT_INT(out[2], n4, "  incomers[2]==4");
    ASSERT_INT(flow_outgoers(f, n3, out, 8), 1, "node 3 has 1 outgoer");
    ASSERT_INT(out[0], n5, "  outgoers[0]==5");
    ASSERT_INT(flow_connected_edges(f, n3, out, 8), 4, "node 3 touches all 4 edges");
    ASSERT_INT(out[0], e13, "  edges[0]==e13 (insertion order)");
    ASSERT_INT(out[1], e23, "  edges[1]==e23");
    ASSERT_INT(out[2], e43, "  edges[2]==e43");
    ASSERT_INT(out[3], e35, "  edges[3]==e35 (incident on EITHER endpoint)");

    /* leaf node: no outgoers; one incomer */
    ASSERT_INT(flow_outgoers(f, n5, out, 8), 0, "leaf node 5: no outgoers");
    ASSERT_INT(flow_incomers(f, n5, out, 8), 1, "leaf node 5: one incomer");
    ASSERT_INT(out[0], n3, "  incomers[0]==3");
    /* source node: no incomers */
    ASSERT_INT(flow_incomers(f, n1, out, 8), 0, "root node 1: no incomers");

    /* count-exceeds-max: true total returned, only out[0..max-1] written */
    int small[2] = {-7, -7};
    ASSERT_INT(flow_incomers(f, n3, small, 1), 3, "max=1 still returns true count 3");
    ASSERT_INT(small[0], n1, "  small[0] filled");
    ASSERT_INT(small[1], -7, "  small[1] untouched (no write past max)");

    /* missing node id: 0, out untouched */
    small[0] = -7;
    ASSERT_INT(flow_incomers(f, 9999, small, 2), 0, "missing node: incomers 0");
    ASSERT_INT(flow_outgoers(f, 9999, small, 2), 0, "missing node: outgoers 0");
    ASSERT_INT(flow_connected_edges(f, 9999, small, 2), 0, "missing node: connected_edges 0");
    ASSERT_INT(small[0], -7, "  out untouched on missing id");
    flow_free(f);
  }

  /* ---- multi-edge dedup: two edges A->B with different handles ---- */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    int a = flow_add_node(f, "default", (flow_pt){0, 0},  (void*)"A");
    int b = flow_add_node(f, "default", (flow_pt){20, 0}, (void*)"B");
    /* default nodes have handles in/out; same pair, different source handle names are
       legal non-duplicates per flow_add_edge's rule (dup = same src+tgt+handles) */
    int eA = flow_add_edge(f, a, b, "out",  "in");
    int eB = flow_add_edge(f, a, b, "out2", "in");
    ASSERT(eA != -1 && eB != -1, "both multi-edges added (different handle pairs)");
    int out[8];
    ASSERT_INT(flow_incomers(f, b, out, 8), 1, "multi-edge: ONE incomer entry for A");
    ASSERT_INT(out[0], a, "  incomers[0]==A");
    ASSERT_INT(flow_outgoers(f, a, out, 8), 1, "multi-edge: ONE outgoer entry for B");
    ASSERT_INT(flow_connected_edges(f, a, out, 8), 2, "connected_edges lists BOTH edges (no dedup)");
    ASSERT_INT(out[0], eA, "  edges[0]==eA"); ASSERT_INT(out[1], eB, "  edges[1]==eB");
    flow_free(f);
  }

  /* ---- self-contained insertion-order pin: A->B, C->B, A->C ---- */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    int a = flow_add_node(f, "default", (flow_pt){0, 0},  (void*)"A");
    int b = flow_add_node(f, "default", (flow_pt){20, 0}, (void*)"B");
    int c = flow_add_node(f, "default", (flow_pt){0, 10}, (void*)"C");
    flow_add_edge(f, a, b, "out", "in");
    flow_add_edge(f, c, b, "out", "in");
    flow_add_edge(f, a, c, "out", "in");
    int out[8];
    ASSERT_INT(flow_incomers(f, b, out, 8), 2, "B: two incomers");
    ASSERT_INT(out[0], a, "  [0]==A (creation order, not id-sorted)");
    ASSERT_INT(out[1], c, "  [1]==C");
    /* hidden-flag forward note (#11): queries are MODEL-level — they will keep
       including hidden nodes/edges; pinned by the cross-cutting layering rule. */
    flow_free(f);
  }

  /* ---- empty graph / no edges ---- */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    int out[4];
    ASSERT_INT(flow_incomers(f, 1, out, 4), 0, "empty graph: incomers 0");
    int lone = flow_add_node(f, "default", (flow_pt){0, 0}, (void*)"L");
    ASSERT_INT(flow_connected_edges(f, lone, out, 4), 0, "edgeless node: connected_edges 0");
    flow_free(f);
  }

  /* ---- NULL out / max 0: count-only query is legal (fill-buffer idiom) ---- */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    int a = flow_add_node(f, "default", (flow_pt){0, 0},  (void*)"A");
    int b = flow_add_node(f, "default", (flow_pt){20, 0}, (void*)"B");
    flow_add_edge(f, a, b, "out", "in");
    ASSERT_INT(flow_incomers(f, b, NULL, 0), 1, "NULL/0 buffer: count-only query works");
    ASSERT_INT(flow_connected_edges(f, a, NULL, 0), 1, "  connected_edges too");
    flow_free(f);
  }

  /* ============ spatial intersection queries (inc-4 #10, same module) ============
     NOTE: the spec's example coordinates for this package are internally inconsistent
     (its query rect cannot reach B and does hit D); the rects below are hand-verified
     against flow_rect_intersects' closed convention instead. */

  /* ---- region query: overlapping rects in insertion order; non-overlap excluded ---- */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    int a = flow_add_node(f, "default", (flow_pt){10, 5},  (void*)"A");
    int b = flow_add_node(f, "default", (flow_pt){30, 5},  (void*)"B");
    int c = flow_add_node(f, "default", (flow_pt){20, 10}, (void*)"C");
    int d = flow_add_node(f, "default", (flow_pt){10, 10}, (void*)"D");
    flow_get_node(f, a)->w = 5; flow_get_node(f, a)->h = 3;   /* [10,15]x[5,8]   */
    flow_get_node(f, b)->w = 5; flow_get_node(f, b)->h = 3;   /* [30,35]x[5,8]   */
    flow_get_node(f, c)->w = 4; flow_get_node(f, c)->h = 4;   /* [20,24]x[10,14] */
    flow_get_node(f, d)->w = 6; flow_get_node(f, d)->h = 3;   /* [10,16]x[10,13] */
    int out[8];
    ASSERT_INT(flow_intersecting_nodes(f, (flow_rect){12, 6, 10, 6}, out, 8), 3,
               "region [12,22]x[6,12] hits A, C, D");
    ASSERT_INT(out[0], a, "  [0]==A (insertion order)");
    ASSERT_INT(out[1], c, "  [1]==C");
    ASSERT_INT(out[2], d, "  [2]==D");
    /* closed convention: a rect STARTING at A's right edge (x=15) touches; x=16 misses */
    ASSERT_INT(flow_intersecting_nodes(f, (flow_rect){15, 5, 2, 3}, out, 8), 1,
               "edge-touch at x==15 counts (closed convention, flow_rect_intersects)");
    ASSERT_INT(out[0], a, "  the touch is A");
    ASSERT_INT(flow_intersecting_nodes(f, (flow_rect){16, 5, 2, 3}, out, 8), 0,
               "one cell past the edge misses");
    /* buffer overflow: true count, no write past max */
    int small[2] = {-7, -7};
    ASSERT_INT(flow_intersecting_nodes(f, (flow_rect){12, 6, 10, 6}, small, 1), 3,
               "max=1 still returns true count 3");
    ASSERT_INT(small[0], a, "  small[0] filled");
    ASSERT_INT(small[1], -7, "  small[1] untouched");
    flow_free(f);
  }

  /* ---- node convenience: intersectors of a node's rect, excluding itself ---- */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    int a = flow_add_node(f, "default", (flow_pt){10, 5}, (void*)"A");
    int b = flow_add_node(f, "default", (flow_pt){30, 5}, (void*)"B");
    int e = flow_add_node(f, "default", (flow_pt){14, 7}, (void*)"E");
    flow_get_node(f, a)->w = 5; flow_get_node(f, a)->h = 3;   /* [10,15]x[5,8]  */
    flow_get_node(f, b)->w = 5; flow_get_node(f, b)->h = 3;   /* far away       */
    flow_get_node(f, e)->w = 4; flow_get_node(f, e)->h = 3;   /* [14,18]x[7,10] overlaps A */
    int out[8];
    ASSERT_INT(flow_node_intersections(f, a, out, 8), 1, "A intersects exactly E");
    ASSERT_INT(out[0], e, "  [0]==E");
    ASSERT(out[0] != a, "  A itself excluded");
    ASSERT_INT(flow_node_intersections(f, b, out, 8), 0, "isolated B intersects nothing");
    ASSERT_INT(flow_node_intersections(f, 9999, out, 8), 0, "missing id returns 0 gracefully");
    flow_free(f);
  }

  /* ---- parent/child rect overlap is returned, NOT filtered (apps use flow_is_ancestor) ---- */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    int p = flow_add_node(f, "group", (flow_pt){10, 5}, NULL);
    flow_get_node(f, p)->w = 20; flow_get_node(f, p)->h = 10;  /* [10,30]x[5,15] */
    int k = flow_add_node(f, "default", (flow_pt){12, 7}, (void*)"k");
    flow_get_node(f, k)->w = 4; flow_get_node(f, k)->h = 3;
    flow_set_parent(f, k, p);
    int out[8];
    ASSERT_INT(flow_intersecting_nodes(f, (flow_rect){0, 0, 60, 24}, out, 8), 2,
               "region query returns parent AND nested child (no ancestor filtering)");
    ASSERT_INT(flow_node_intersections(f, p, out, 8), 1, "parent's intersections include its child");
    ASSERT_INT(out[0], k, "  the child");
    ASSERT_INT(flow_is_ancestor(f, p, k), 1, "  (apps filter via flow_is_ancestor when desired)");
    flow_free(f);
  }

  /* ---- empty graph ---- */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    int out[4];
    ASSERT_INT(flow_intersecting_nodes(f, (flow_rect){0, 0, 100, 100}, out, 4), 0,
               "empty graph: region query 0");
    flow_free(f);
  }

  /* ---- MODEL-level layering (#11 landed): hidden elements STAY in the queries ---- */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    int a = flow_add_node(f, "default", (flow_pt){0, 0},  (void*)"A");
    int b = flow_add_node(f, "default", (flow_pt){20, 0}, (void*)"B");
    int e = flow_add_edge(f, a, b, "out", "in");
    flow_set_node_hidden(f, a, 1);
    flow_set_edge_hidden(f, e, 1);
    int out[4];
    ASSERT_INT(flow_incomers(f, b, out, 4), 1, "hidden source still counted as an incomer");
    ASSERT_INT(out[0], a, "  the hidden node id");
    ASSERT_INT(flow_connected_edges(f, b, out, 4), 1, "hidden edge still in connected_edges");
    ASSERT_INT(flow_intersecting_nodes(f, (flow_rect){0, 0, 60, 24}, out, 4), 2,
               "region query still sweeps the hidden node (view-skip does not apply)");
    flow_free(f);
  }

  return flowtest_report("test_query");
}
