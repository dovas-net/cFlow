#define FLOW_IMPLEMENTATION
#include "../flow.h"
#include "flowtest.h"

/* undo-redo package: capped inverse-op command journal over the settled mutator
   surface (spec §11). Tests poke f->journal.n directly (per-file FLOW_IMPLEMENTATION)
   to count UNDO STEPS — one journal command per user-visible step. */

static int g_ubind_fired = 0;
static void ubind(flow_t *f, void *user) { (void)f; (void)user; g_ubind_fired++; }

/* data-pointer identity probes: the journal must reattach the EXACT borrowed pointer
   (string literals would make == unspecified, so use named buffers) */
static char DAT_A[] = "A", DAT_B[] = "B";

int main(void) {
  /* ---- empty-journal no-ops + viewport not journaled ---- */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    ASSERT_INT(flow_can_undo(f), 0, "fresh engine: nothing to undo");
    ASSERT_INT(flow_can_redo(f), 0, "fresh engine: nothing to redo");
    flow_undo(f);                                    /* floor no-op: must not crash/underflow */
    flow_redo(f);
    ASSERT_INT(flow_can_undo(f), 0, "undo on empty stack is a no-op");
    flow_pan(f, 3, 2);                               /* viewport is deliberately NOT journaled */
    ASSERT_INT(flow_can_undo(f), 0, "pan records nothing (viewport excluded per spec §11)");
    flow_free(f);
  }

  /* ---- add node: undo removes, redo restores SAME id + SAME array index ---- */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    int a = flow_add_node(f, "default", (flow_pt){5, 5}, (void*)"A");
    int b = flow_add_node(f, "default", (flow_pt){20, 5}, DAT_B);
    ASSERT_INT(flow_can_undo(f), 1, "add recorded");
    ASSERT_INT(f->journal.n, 2, "two adds = two undo steps");
    flow_undo(f);
    ASSERT(flow_get_node(f, b) == NULL, "undo removes the added node");
    ASSERT_INT(flow_node_count(f), 1, "count back to 1");
    ASSERT_INT(flow_can_redo(f), 1, "redo available after undo");
    flow_redo(f);
    flow_node *nb = flow_get_node(f, b);
    ASSERT(nb != NULL, "redo restores the node");
    ASSERT_INT(nb->id, b, "restored node keeps its ORIGINAL id");
    ASSERT_INT((int)(nb - flow_nodes(f)), 1, "restored node keeps its ORIGINAL array index");
    ASSERT(nb->pos.x == 20 && nb->pos.y == 5, "restored node keeps its position");
    ASSERT(nb->data == (void*)DAT_B, "restored node keeps the exact data pointer");
    ASSERT_INT(flow_get_node(f, a)->id, a, "earlier node untouched");
    flow_free(f);
  }

  /* ---- remove-with-subtree round-trip: ONE undo step restores everything ---- */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    int a = flow_add_node(f, "default", (flow_pt){5, 5},  DAT_A);
    int b = flow_add_node(f, "default", (flow_pt){30, 5}, DAT_B);
    int c = flow_add_node(f, "default", (flow_pt){50, 5}, (void*)"C");
    int d = flow_add_node(f, "default", (flow_pt){60, 12}, (void*)"D");
    flow_set_parent(f, b, a);                          /* B is A's child: cascade target */
    int e1 = flow_add_edge(f, c, a, "out", "in");
    int e2 = flow_add_edge(f, a, d, "out", "in");
    int e3 = flow_add_edge(f, c, b, "out", "in");
    flow_set_edge_label(f, e1, "lbl-e1");
    int nid_before = f->nextid, eid_before = f->nexteid;
    flow_pt b_abs = flow_node_abs(f, flow_get_node(f, b));
    int steps_before = f->journal.n;
    flow_remove_node(f, a);                            /* cascades B + all 3 incident edges */
    ASSERT_INT(f->journal.n, steps_before + 1, "remove-with-subtree is ONE undo step");
    ASSERT(flow_get_node(f, a) == NULL && flow_get_node(f, b) == NULL, "A and child B removed");
    ASSERT_INT(flow_edge_count(f), 0, "all 3 incident edges removed");
    ASSERT_INT(flow_node_count(f), 2, "C and D survive");
    flow_undo(f);
    ASSERT_INT(flow_node_count(f), 4, "undo restores both nodes");
    ASSERT_INT(flow_edge_count(f), 3, "undo restores all 3 edges");
    /* insertion ORDER preserved (render/hit iterate by array order) */
    ASSERT_INT(flow_nodes(f)[0].id, a, "node order restored: A at index 0");
    ASSERT_INT(flow_nodes(f)[1].id, b, "node order restored: B at index 1");
    ASSERT_INT(flow_nodes(f)[2].id, c, "node order restored: C at index 2");
    ASSERT_INT(flow_nodes(f)[3].id, d, "node order restored: D at index 3");
    ASSERT_INT(flow_edges(f)[0].id, e1, "edge order restored: e1 first");
    ASSERT_INT(flow_edges(f)[1].id, e2, "edge order restored: e2 second");
    ASSERT_INT(flow_edges(f)[2].id, e3, "edge order restored: e3 third");
    /* field equality */
    flow_node *bn = flow_get_node(f, b);
    ASSERT_INT(bn->parent, a, "B's parent restored to A");
    flow_pt b_abs2 = flow_node_abs(f, bn);
    ASSERT(b_abs2.x == b_abs.x && b_abs2.y == b_abs.y, "B abs position restored");
    ASSERT(flow_get_node(f, a)->data == (void*)DAT_A, "A data pointer identical (borrowed, reattached)");
    ASSERT(bn->data == (void*)DAT_B, "B data pointer identical");
    flow_edge *re1 = flow_get_edge(f, e1);
    ASSERT(re1->source == c && re1->target == a, "e1 endpoints restored");
    ASSERT_STR(re1->source_handle, "out", "e1 source handle restored");
    ASSERT_STR(re1->target_handle, "in", "e1 target handle restored");
    ASSERT(re1->label && strcmp(re1->label, "lbl-e1") == 0, "e1 label restored");
    ASSERT_INT(f->nextid, nid_before, "nextid restored");
    ASSERT_INT(f->nexteid, eid_before, "nexteid restored");
    /* redo removes the subtree again; a second undo restores it again (snap reuse) */
    flow_redo(f);
    ASSERT_INT(flow_node_count(f), 2, "redo re-removes the subtree");
    ASSERT_INT(flow_edge_count(f), 0, "redo re-removes the edges");
    flow_undo(f);
    ASSERT_INT(flow_node_count(f), 4, "second undo restores again");
    ASSERT(flow_get_edge(f, e1)->label && strcmp(flow_get_edge(f, e1)->label, "lbl-e1") == 0,
           "label survives a second restore");
    flow_free(f);
  }

  /* ---- move undo/redo: ABSOLUTE coords, incl a parented child ---- */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    int a = flow_add_node(f, "default", (flow_pt){5, 5}, (void*)"A");
    flow_move_node(f, a, (flow_pt){30, 10});
    flow_undo(f);
    flow_pt p = flow_node_abs(f, flow_get_node(f, a));
    ASSERT(p.x == 5 && p.y == 5, "move undo restores the old absolute position");
    flow_redo(f);
    p = flow_node_abs(f, flow_get_node(f, a));
    ASSERT(p.x == 30 && p.y == 10, "move redo restores the new absolute position");
    /* parented child: MOVE replays the SAME absolute coords flow_move_node accepts */
    int g = flow_add_node(f, "default", (flow_pt){10, 4}, (void*)"G");
    int n = flow_add_node(f, "default", (flow_pt){20, 14}, (void*)"N");
    flow_set_parent(f, n, g);
    flow_move_node(f, n, (flow_pt){40, 20});
    flow_undo(f);
    p = flow_node_abs(f, flow_get_node(f, n));
    ASSERT(p.x == 20 && p.y == 14, "child move undo restores absolute (20,14)");
    ASSERT(flow_get_node(f, n)->pos.x == 10 && flow_get_node(f, n)->pos.y == 10,
           "child stored rel pos restored (abs - parent_abs)");
    flow_redo(f);
    p = flow_node_abs(f, flow_get_node(f, n));
    ASSERT(p.x == 40 && p.y == 20, "child move redo restores absolute (40,20)");
    flow_free(f);
  }

  /* ---- REPARENT: set_parent undo/redo restores parent + stored pos ---- */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    int g = flow_add_node(f, "default", (flow_pt){10, 4}, (void*)"G");
    int n = flow_add_node(f, "default", (flow_pt){20, 14}, (void*)"N");
    int steps_before = f->journal.n;
    flow_set_parent(f, n, g);
    ASSERT_INT(f->journal.n, steps_before + 1, "set_parent is one undo step");
    flow_undo(f);
    ASSERT_INT(flow_get_node(f, n)->parent, -1, "reparent undo restores old parent (-1)");
    ASSERT(flow_get_node(f, n)->pos.x == 20 && flow_get_node(f, n)->pos.y == 14,
           "reparent undo restores old stored pos");
    flow_redo(f);
    ASSERT_INT(flow_get_node(f, n)->parent, g, "reparent redo re-applies new parent");
    ASSERT(flow_get_node(f, n)->pos.x == 10 && flow_get_node(f, n)->pos.y == 10,
           "reparent redo restores new stored pos (abs preserved)");
    flow_free(f);
  }

  /* ---- flow_group: ONE undo dissolves it; redo rebuilds the container EXACTLY ---- */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    int a = flow_add_node(f, "default", (flow_pt){10, 5}, (void*)"A");
    int b = flow_add_node(f, "default", (flow_pt){30, 5}, (void*)"B");
    flow_pt a_abs0 = flow_node_abs(f, flow_get_node(f, a));
    int count0 = flow_node_count(f);
    int steps_before = f->journal.n;
    int ids[2] = { a, b };
    int gid = flow_group(f, ids, 2);
    ASSERT(gid > 0, "group created");
    ASSERT_INT(f->journal.n, steps_before + 1, "flow_group is ONE undo step (add + N reparents)");
    flow_node *g = flow_get_node(f, gid);
    int gw = g->w, gh = g->h;
    flow_undo(f);
    ASSERT(flow_get_node(f, gid) == NULL, "group undo: container gone");
    ASSERT_INT(flow_node_count(f), count0, "group undo: node count back");
    ASSERT_INT(flow_get_node(f, a)->parent, -1, "group undo: A back at top level");
    ASSERT_INT(flow_get_node(f, b)->parent, -1, "group undo: B back at top level");
    flow_pt a_abs1 = flow_node_abs(f, flow_get_node(f, a));
    ASSERT(a_abs1.x == a_abs0.x && a_abs1.y == a_abs0.y, "group undo: A abs unchanged");
    flow_redo(f);
    g = flow_get_node(f, gid);
    ASSERT(g != NULL, "group redo: container back with SAME id");
    ASSERT(g->w == gw && g->h == gh, "group redo: container w/h restored (post-add resize captured)");
    ASSERT_INT(flow_get_node(f, a)->parent, gid, "group redo: A reparented under container");
    ASSERT_INT(flow_get_node(f, b)->parent, gid, "group redo: B reparented under container");
    flow_free(f);
  }

  /* ---- flow_ungroup: ONE undo re-groups (container back, children re-parented) ---- */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    int a = flow_add_node(f, "default", (flow_pt){10, 5}, (void*)"A");
    int b = flow_add_node(f, "default", (flow_pt){20, 5}, (void*)"B");
    int ids[2] = { a, b };
    int gid = flow_group(f, ids, 2);
    flow_pt a_abs0 = flow_node_abs(f, flow_get_node(f, a));
    int steps_before = f->journal.n;
    flow_ungroup(f, gid);
    ASSERT_INT(f->journal.n, steps_before + 1, "flow_ungroup is ONE undo step");
    ASSERT(flow_get_node(f, gid) == NULL, "ungroup removed the container");
    flow_undo(f);
    ASSERT(flow_get_node(f, gid) != NULL, "ungroup undo: container re-added with SAME id");
    ASSERT_INT(flow_get_node(f, a)->parent, gid, "ungroup undo: A re-parented under container");
    ASSERT_INT(flow_get_node(f, b)->parent, gid, "ungroup undo: B re-parented under container");
    flow_pt a_abs1 = flow_node_abs(f, flow_get_node(f, a));
    ASSERT(a_abs1.x == a_abs0.x && a_abs1.y == a_abs0.y, "ungroup undo: A abs unchanged");
    flow_redo(f);
    ASSERT(flow_get_node(f, gid) == NULL, "ungroup redo: dissolved again");
    ASSERT_INT(flow_get_node(f, a)->parent, -1, "ungroup redo: A back at top level");
    flow_free(f);
  }

  /* ---- group-drag = ONE undo (synthetic SGR): both roots restored, bystander untouched ---- */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    int a = flow_add_node(f, "default", (flow_pt){5, 5},  (void*)"A");   /* body x5..9 y5..7 */
    int b = flow_add_node(f, "default", (flow_pt){20, 5}, (void*)"B");
    int c = flow_add_node(f, "default", (flow_pt){40, 12}, (void*)"C");
    flow_select_node(f, a, 0);
    flow_select_node(f, b, 1);                         /* selection itself is NOT journaled */
    int steps_before = f->journal.n;
    /* press inside A's body (not a handle cell), two motions, release: total delta +6,+4 */
    flow_feed(f, "\x1b[<0;7;6M", 9);                   /* press at (6,5) */
    flow_feed(f, "\x1b[<32;10;8M", 11);                /* motion -> (9,7): delta +3,+2 */
    flow_feed(f, "\x1b[<32;13;10M", 12);               /* motion -> (12,9): delta +3,+2 */
    flow_feed(f, "\x1b[<0;13;10m", 11);                /* release */
    flow_pt pa = flow_node_abs(f, flow_get_node(f, a));
    flow_pt pb = flow_node_abs(f, flow_get_node(f, b));
    flow_pt pc = flow_node_abs(f, flow_get_node(f, c));
    ASSERT(pa.x == 11 && pa.y == 9, "drag moved A by total delta");
    ASSERT(pb.x == 26 && pb.y == 9, "drag moved B by total delta");
    ASSERT(pc.x == 40 && pc.y == 12, "unselected C never moved");
    ASSERT_INT(f->journal.n, steps_before + 1, "whole multi-node drag is ONE undo step");
    flow_undo(f);
    pa = flow_node_abs(f, flow_get_node(f, a));
    pb = flow_node_abs(f, flow_get_node(f, b));
    pc = flow_node_abs(f, flow_get_node(f, c));
    ASSERT(pa.x == 5 && pa.y == 5, "ONE undo restores A");
    ASSERT(pb.x == 20 && pb.y == 5, "ONE undo restores B");
    ASSERT(pc.x == 40 && pc.y == 12, "C still untouched after undo");
    flow_redo(f);
    pa = flow_node_abs(f, flow_get_node(f, a));
    pb = flow_node_abs(f, flow_get_node(f, b));
    ASSERT(pa.x == 11 && pa.y == 9 && pb.x == 26 && pb.y == 9, "ONE redo re-applies both moves");
    flow_free(f);
  }

  /* ---- connect: one undo step; redo restores same id/endpoints/handles ---- */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    int a = flow_add_node(f, "default", (flow_pt){5, 5},  (void*)"A");
    int b = flow_add_node(f, "default", (flow_pt){30, 5}, (void*)"B");
    int steps_before = f->journal.n;
    ASSERT_INT(flow_begin_connection(f, a, "out"), 0, "begin connection from A.out");
    int eid = flow_end_connection(f, b, "in");
    ASSERT(eid != -1, "connection created an edge");
    ASSERT_INT(f->journal.n, steps_before + 1, "connect is ONE undo step");
    flow_undo(f);
    ASSERT_INT(flow_edge_count(f), 0, "connect undo removes the edge");
    flow_redo(f);
    ASSERT_INT(flow_edge_count(f), 1, "connect redo restores the edge");
    flow_edge *e = flow_get_edge(f, eid);
    ASSERT(e != NULL, "edge back with SAME id");
    ASSERT(e->source == a && e->target == b, "endpoints restored");
    ASSERT_STR(e->source_handle, "out", "source handle restored");
    ASSERT_STR(e->target_handle, "in", "target handle restored");
    flow_free(f);
  }

  /* ---- reconnect: undo restores old endpoint+handle; rejected reconnect records nothing ---- */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    int a = flow_add_node(f, "default", (flow_pt){5, 5},  (void*)"A");
    int b = flow_add_node(f, "default", (flow_pt){30, 5}, (void*)"B");
    int c = flow_add_node(f, "default", (flow_pt){55, 5}, (void*)"C");
    int e = flow_add_edge(f, a, b, "out", "in");
    int steps_before = f->journal.n;
    flow_reconnect_edge(f, e, c, "", 1);               /* repoint target B -> C, handle "in" -> "" */
    ASSERT_INT(f->journal.n, steps_before + 1, "applied reconnect is one undo step");
    ASSERT_INT(flow_get_edge(f, e)->target, c, "reconnect applied");
    flow_undo(f);
    ASSERT_INT(flow_get_edge(f, e)->target, b, "reconnect undo restores original target");
    ASSERT_STR(flow_get_edge(f, e)->target_handle, "in", "reconnect undo restores original handle");
    flow_redo(f);
    ASSERT_INT(flow_get_edge(f, e)->target, c, "reconnect redo re-applies");
    ASSERT_STR(flow_get_edge(f, e)->target_handle, "", "reconnect redo restores new handle");
    flow_undo(f);                                      /* back to a->b for the rejection check */
    int e2 = flow_add_edge(f, c, b, "out", "in");
    ASSERT(e2 != -1, "second edge added");
    steps_before = f->journal.n;
    flow_reconnect_edge(f, e2, a, "out", 0);           /* would duplicate a->b ("out","in"): rejected */
    ASSERT_INT(flow_get_edge(f, e2)->source, c, "rejected reconnect left edge unchanged");
    ASSERT_INT(f->journal.n, steps_before, "rejected reconnect records NOTHING");
    flow_free(f);
  }

  /* ---- set-label undo chain incl NULL <-> str ---- */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    int a = flow_add_node(f, "default", (flow_pt){5, 5},  (void*)"A");
    int b = flow_add_node(f, "default", (flow_pt){30, 5}, (void*)"B");
    int e = flow_add_edge(f, a, b, "out", "in");
    flow_set_edge_label(f, e, "L1");
    flow_set_edge_label(f, e, "L2");
    ASSERT_STR(flow_get_edge(f, e)->label, "L2", "label is L2");
    flow_undo(f);
    ASSERT_STR(flow_get_edge(f, e)->label, "L1", "first undo: L2 -> L1");
    flow_undo(f);
    ASSERT(flow_get_edge(f, e)->label == NULL, "second undo: L1 -> NULL");
    flow_redo(f);
    ASSERT_STR(flow_get_edge(f, e)->label, "L1", "redo: NULL -> L1");
    flow_redo(f);
    ASSERT_STR(flow_get_edge(f, e)->label, "L2", "redo: L1 -> L2");
    flow_free(f);
  }

  /* ---- add-node-center = one undo; delete-selection = one undo (restores all) ---- */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    int steps_before = f->journal.n;
    int id = flow_add_node_center(f, "default", (void*)"X");
    ASSERT_INT(f->journal.n, steps_before + 1, "add-node-center (add + centering move) is ONE step");
    flow_undo(f);
    ASSERT(flow_get_node(f, id) == NULL, "add-node-center undo removes the node");
    flow_redo(f);
    ASSERT(flow_get_node(f, id) != NULL, "add-node-center redo restores it");

    int a = flow_add_node(f, "default", (flow_pt){5, 5},  (void*)"A");
    int b = flow_add_node(f, "default", (flow_pt){30, 5}, (void*)"B");
    int c = flow_add_node(f, "default", (flow_pt){55, 5}, (void*)"C");
    int e = flow_add_edge(f, a, b, "out", "in");
    flow_select_node(f, a, 0);
    flow_select_node(f, b, 1);
    int count0 = flow_node_count(f), ecount0 = flow_edge_count(f);
    steps_before = f->journal.n;
    flow_delete_selection(f);
    ASSERT_INT(f->journal.n, steps_before + 1, "delete-selection is ONE undo step");
    ASSERT(flow_get_node(f, a) == NULL && flow_get_node(f, b) == NULL, "selected nodes removed");
    ASSERT(flow_get_edge(f, e) == NULL, "incident edge removed");
    flow_undo(f);
    ASSERT_INT(flow_node_count(f), count0, "delete-selection undo restores all nodes");
    ASSERT_INT(flow_edge_count(f), ecount0, "delete-selection undo restores the edge");
    ASSERT(!(flow_get_node(f, a)->flags & FLOW_SELECTED), "restored node NOT selected (ephemeral cleared)");
    ASSERT(flow_get_node(f, c) != NULL, "unselected C was never touched");
    flow_free(f);
  }

  /* ---- standalone edge removal (FLOW_CMD_REMOVE_EDGE): the one path connect/cascade
          never hit — select-edge + delete-selection, and the direct API call ---- */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    int a = flow_add_node(f, "default", (flow_pt){5, 5},  (void*)"A");
    int b = flow_add_node(f, "default", (flow_pt){30, 5}, (void*)"B");
    int e = flow_add_edge(f, a, b, "out", "in");
    flow_set_edge_label(f, e, "L");
    flow_select_edge(f, e, 0);
    int steps_before = f->journal.n;
    flow_delete_selection(f);                          /* no node selected: pure flow_remove_edge path */
    ASSERT_INT(f->journal.n, steps_before + 1, "edge-only delete-selection is ONE undo step");
    ASSERT_INT(flow_edge_count(f), 0, "edge removed");
    ASSERT(flow_get_node(f, a) != NULL && flow_get_node(f, b) != NULL, "nodes untouched");
    flow_undo(f);
    flow_edge *re = flow_get_edge(f, e);
    ASSERT(re != NULL, "remove-edge undo restores the edge with SAME id");
    ASSERT_INT((int)(re - flow_edges(f)), 0, "restored edge keeps its ORIGINAL array index");
    ASSERT(re->source == a && re->target == b, "endpoints restored");
    ASSERT_STR(re->source_handle, "out", "source handle restored");
    ASSERT_STR(re->target_handle, "in", "target handle restored");
    ASSERT(re->label && strcmp(re->label, "L") == 0, "label restored");
    ASSERT(!(re->flags & FLOW_SELECTED), "restored edge NOT selected (ephemeral cleared)");
    flow_undo(f);                                      /* chain continues through the re-insert */
    ASSERT(flow_get_edge(f, e)->label == NULL, "label-set undo still chains after re-insert");
    flow_redo(f); flow_redo(f);
    ASSERT_INT(flow_edge_count(f), 0, "redo chain re-applies label then removal");
    /* direct API call records too */
    flow_undo(f);                                      /* edge back ("L") */
    steps_before = f->journal.n;
    flow_remove_edge(f, e);
    ASSERT_INT(f->journal.n, steps_before + 1, "direct flow_remove_edge is one undo step");
    flow_undo(f);
    ASSERT(flow_get_edge(f, e) != NULL, "direct remove-edge undo restores it");
    flow_free(f);
  }

  /* ---- redo clears on new mutation; cap eviction; limit floor; limit 0 disables ---- */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    int a = flow_add_node(f, "default", (flow_pt){5, 5}, (void*)"A");
    flow_undo(f);
    ASSERT_INT(flow_can_redo(f), 1, "redo pending");
    flow_add_node(f, "default", (flow_pt){30, 5}, (void*)"B");
    ASSERT_INT(flow_can_redo(f), 0, "new recorded mutation clears the redo stack");
    (void)a;
    flow_free(f);

    /* cap eviction: limit 2, three label-sets (dup'd labels) -> oldest evicted, frees its copies */
    flow_t *g = flow_new(80, 24); flow_register_defaults(g);
    int x = flow_add_node(g, "default", (flow_pt){5, 5},  (void*)"X");
    int y = flow_add_node(g, "default", (flow_pt){30, 5}, (void*)"Y");
    int e = flow_add_edge(g, x, y, "out", "in");
    flow_set_undo_limit(g, 2);
    ASSERT_INT(g->journal.n, 2, "lowering the limit evicts oldest down to the cap");
    flow_set_edge_label(g, e, "L1");
    flow_set_edge_label(g, e, "L2");
    flow_set_edge_label(g, e, "L3");
    ASSERT_INT(g->journal.n, 2, "history capped at limit");
    flow_undo(g); flow_undo(g);
    ASSERT_STR(flow_get_edge(g, e)->label, "L1", "after undo-all only the 2 newest steps reverted");
    ASSERT_INT(flow_can_undo(g), 0, "evicted history is unreachable");
    flow_free(g);

    /* limit 0 disables journaling entirely; restoring a limit re-enables */
    flow_t *h = flow_new(80, 24); flow_register_defaults(h);
    flow_set_undo_limit(h, 0);
    flow_add_node(h, "default", (flow_pt){5, 5}, (void*)"Z");
    ASSERT_INT(flow_can_undo(h), 0, "limit 0: nothing recorded");
    flow_set_undo_limit(h, 8);
    flow_add_node(h, "default", (flow_pt){30, 5}, (void*)"W");
    ASSERT_INT(flow_can_undo(h), 1, "restoring a limit re-enables recording");
    flow_set_undo_limit(h, -3);                        /* negative clamps to 0 (disable) */
    ASSERT_INT(flow_can_undo(h), 0, "negative limit clamps to disabled");
    flow_free(h);
  }

  /* ---- keys: 'u' undoes, Ctrl-r (0x12) redoes via flow_feed; flow_bind_key override wins ---- */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    int a = flow_add_node(f, "default", (flow_pt){5, 5}, (void*)"A");
    flow_feed(f, "u", 1);
    ASSERT(flow_get_node(f, a) == NULL, "'u' key undoes via the built-in");
    flow_feed(f, "\x12", 1);
    ASSERT(flow_get_node(f, a) != NULL, "Ctrl-r (0x12) redoes via the built-in");
    flow_bind_key(f, "u", ubind, NULL);
    g_ubind_fired = 0;
    flow_feed(f, "u", 1);
    ASSERT_INT(g_ubind_fired, 1, "flow_bind_key('u') override fired");
    ASSERT(flow_get_node(f, a) != NULL, "override suppressed the built-in undo");
    flow_free(f);
  }

  /* ---- re-entrancy: replay records nothing (undo stack shrinks by exactly 1) ---- */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    int a = flow_add_node(f, "default", (flow_pt){5, 5}, (void*)"A");
    flow_remove_node(f, a);
    int n_before = f->journal.n;
    flow_undo(f);                                      /* replay calls mutators internally */
    ASSERT_INT(f->journal.n, n_before - 1, "undo-of-remove did not itself grow the undo stack");
    ASSERT_INT(f->journal.rn, 1, "undone command moved to the redo stack");
    flow_free(f);
  }

  /* ---- id-counter integrity after undo-all / redo-all then add ---- */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    int a = flow_add_node(f, "default", (flow_pt){5, 5},  (void*)"A");
    int b = flow_add_node(f, "default", (flow_pt){30, 5}, (void*)"B");
    ASSERT_INT(b, a + 1, "ids mint sequentially");
    flow_undo(f); flow_undo(f);
    int c = flow_add_node(f, "default", (flow_pt){50, 5}, (void*)"C");
    ASSERT_INT(c, a, "undo-all rewinds nextid: fresh add re-mints the first id");
    flow_free(f);

    flow_t *g = flow_new(80, 24); flow_register_defaults(g);
    int x = flow_add_node(g, "default", (flow_pt){5, 5}, (void*)"X");
    flow_undo(g); flow_redo(g);
    int y = flow_add_node(g, "default", (flow_pt){30, 5}, (void*)"Y");
    ASSERT_INT(y, x + 1, "redo re-advances nextid: fresh add mints the NEXT id");
    flow_free(g);
  }

  /* ---- serialize seam: flow_load clears the journal and records nothing ---- */
  {
    const char *path = "/tmp/test_undo_load.json";
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    int a = flow_add_node(f, "default", (flow_pt){5, 5},  (void*)"A");
    int b = flow_add_node(f, "default", (flow_pt){30, 5}, (void*)"B");
    flow_add_edge(f, a, b, "out", "in");
    ASSERT_INT(flow_save(f, path), 0, "save ok");
    ASSERT_INT(flow_can_undo(f), 1, "history present before load");
    ASSERT_INT(flow_load(f, path), 0, "load ok");
    ASSERT_INT(flow_can_undo(f), 0, "load cleared the undo stack");
    ASSERT_INT(flow_can_redo(f), 0, "load cleared the redo stack");
    ASSERT_INT(f->journal.n, 0, "journal empty after load (rebuild recorded nothing)");
    int count_loaded = flow_node_count(f);
    int c = flow_add_node(f, "default", (flow_pt){50, 5}, (void*)"C");
    ASSERT_INT(flow_can_undo(f), 1, "recording re-enabled after load");
    flow_undo(f);
    ASSERT(flow_get_node(f, c) == NULL, "undo removes only the post-load add");
    ASSERT_INT(flow_node_count(f), count_loaded, "loaded graph untouched by undo");
    flow_free(f);
  }

  /* ---- inc-6 #7 devtools-hud: read-only journal introspection (depth + top-op) ---- */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    /* 1. fresh-engine empty state */
    ASSERT_INT(flow_undo_depth(f), 0, "fresh: undo depth 0");
    ASSERT_INT(flow_redo_depth(f), 0, "fresh: redo depth 0");
    ASSERT_INT(flow_top_op(f), -1, "fresh: top-op sentinel -1");

    /* 2. depth tracks the stack; top-op reports the last kind */
    int a = flow_add_node(f, "default", (flow_pt){5, 5},  DAT_A);
    int b = flow_add_node(f, "default", (flow_pt){30, 5}, DAT_B);
    ASSERT_INT(flow_undo_depth(f), 2, "two adds = depth 2 (== journal.n)");
    ASSERT_INT(flow_redo_depth(f), 0, "no redo yet");
    ASSERT_INT(flow_top_op(f), FLOW_CMD_ADD_NODE, "top-op is the last add");
    flow_undo(f);
    ASSERT_INT(flow_undo_depth(f), 1, "undo drops depth to 1");
    ASSERT_INT(flow_redo_depth(f), 1, "undo grows redo to 1");
    ASSERT_INT(flow_top_op(f), FLOW_CMD_ADD_NODE, "surviving top is the first add");
    flow_redo(f);
    ASSERT_INT(flow_undo_depth(f), 2, "redo restores depth 2");
    ASSERT_INT(flow_redo_depth(f), 0, "redo empties redo stack");

    /* 3. top-op reflects op KIND across mutation types */
    flow_move_node(f, a, (flow_pt){10, 10});
    ASSERT_INT(flow_top_op(f), FLOW_CMD_MOVE_NODE, "top-op tracks a move");
    flow_add_edge(f, a, b, "", "");
    ASSERT_INT(flow_top_op(f), FLOW_CMD_ADD_EDGE, "top-op tracks an add-edge");
    flow_free(f);
  }

  /* 4. coalesced command returns its LAST op. flow_group's span is [ADD_NODE, REPARENT...]
     (container added first, members reparented after — src/flow_model.h flow_group), so the
     trailing op is the final REPARENT. (The spec prose's "ADD" example had the order inverted;
     the contract is "the last op", which this asserts empirically.) */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    int a = flow_add_node(f, "default", (flow_pt){5, 5},  DAT_A);
    int b = flow_add_node(f, "default", (flow_pt){30, 5}, DAT_B);
    int d0 = flow_undo_depth(f);
    int gid = flow_group(f, (int[]){ a, b }, 2);
    ASSERT(gid != -1, "group succeeded");
    ASSERT_INT(flow_undo_depth(f), d0 + 1, "group is ONE undo step (coalesced)");
    ASSERT_INT(flow_top_op(f), FLOW_CMD_REPARENT, "top-op is the trailing op (the final member REPARENT)");
    flow_free(f);
  }

  /* 5. journaling-disabled and fully-undone both read the empty state */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    flow_add_node(f, "default", (flow_pt){5, 5}, DAT_A);
    flow_set_undo_limit(f, 0);                          /* disables journaling, clears the stack */
    ASSERT_INT(flow_undo_depth(f), 0, "limit 0: depth 0");
    ASSERT_INT(flow_redo_depth(f), 0, "limit 0: redo 0");
    ASSERT_INT(flow_top_op(f), -1, "limit 0: top-op -1");
    int n0 = flow_node_count(f);
    flow_add_node(f, "default", (flow_pt){20, 5}, DAT_B);  /* still mutates, records nothing */
    ASSERT_INT(flow_node_count(f), n0 + 1, "graph still mutates with journaling off");
    ASSERT_INT(flow_undo_depth(f), 0, "still depth 0 (journaling off)");
    flow_free(f);

    flow_t *g = flow_new(80, 24); flow_register_defaults(g);
    flow_add_node(g, "default", (flow_pt){5, 5},  DAT_A);
    flow_add_node(g, "default", (flow_pt){20, 5}, DAT_B);
    flow_undo(g); flow_undo(g);                          /* fully undone */
    ASSERT_INT(flow_undo_depth(g), 0, "fully undone: depth 0");
    ASSERT_INT(flow_top_op(g), -1, "fully undone: top-op -1");
    flow_free(g);
  }

  /* ---- teardown: pending undo + redo history with dup'd labels + borrowed data ---- */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    int a = flow_add_node(f, "default", (flow_pt){5, 5},  (void*)"A");
    int b = flow_add_node(f, "default", (flow_pt){30, 5}, (void*)"B");
    int e = flow_add_edge(f, a, b, "out", "in");
    flow_set_edge_label(f, e, "first");
    flow_set_edge_label(f, e, "second");
    flow_remove_node(f, a);                            /* subtree snapshot holds a dup'd label + data ptrs */
    flow_undo(f);                                      /* one command (with dups) parked on the redo stack */
    flow_undo(f);                                      /* another left undone mid-history */
    flow_free(f);                                      /* ASan/UBSan gate: journal fully torn down */
    flowtest_pass++;                                   /* reaching here without a crash is the assertion */
  }

  return flowtest_report("test_undo");
}
