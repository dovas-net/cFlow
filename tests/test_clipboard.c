#define FLOW_IMPLEMENTATION
#include "../flow.h"
#include "flowtest.h"
#include <string.h>

/* Selection clipboard (inc-5 #7): copy/cut deep-snapshot the selected nodes plus
   the intra-selection edges (BOTH endpoints selected); node->data is ALIASED
   (borrowed, the undo-snapshot contract), edge labels are dup'd. Paste re-mints
   through flow_add_node/flow_add_edge (fresh ids, cumulative offset) as ONE undo
   step and selects the result with ONE sig-gated event. The clipboard survives
   graph mutations and flow_load; freed only in flow_free. Pasted edges stay
   validator-GATED — the deliberate asymmetry vs flow_load's suspension (inc-5 #2):
   a paste violating a live validator SHOULD drop those edges. */

static int sel_fires = 0;
static void on_sel(flow_t *f, const int *ids, int n, void *u) {
  (void)f; (void)ids; (void)n; (void)u; sel_fires++;
}
static int cb_reject_ab(flow_t *f, int s, int t, const char *sh, const char *th, void *u) {
  (void)f; (void)s; (void)t; (void)sh; (void)th; (void)u; return 0;  /* reject all */
}

/* find the id of a node at absolute (x,y), or -1 */
static int node_at(flow_t *f, int x, int y) {
  for (int i = 0; i < flow_node_count(f); i++) {
    flow_node *n = &flow_nodes(f)[i];
    flow_pt a = flow_node_abs(f, n);
    if (a.x == x && a.y == y) return n->id;
  }
  return -1;
}

int main(void) {
  /* ---- 1+2+3: copy/paste round-trip, intra-edge, deep label, re-id, offsets ---- */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    int a = flow_add_node(f, "default", (flow_pt){5, 5},  (void*)"A");
    int b = flow_add_node(f, "default", (flow_pt){20, 5}, (void*)"B");
    int e = flow_add_edge(f, a, b, "out", "in");
    flow_set_edge_label(f, e, "hi");
    flow_select_node(f, a, 0); flow_select_node(f, b, 1);
    flow_copy_selection(f);
    ASSERT_INT(flow_paste(f), 2, "paste returns nodes pasted");
    ASSERT_INT(flow_node_count(f), 4, "two new nodes landed");
    ASSERT_INT(flow_edge_count(f), 2, "intra-selection edge pasted");

    int pa = node_at(f, 6, 6), pb = node_at(f, 21, 6);   /* +1,+1 offset */
    ASSERT(pa != -1 && pb != -1, "pasted nodes at +1,+1");
    ASSERT(pa != a && pa != b && pb != a && pb != b, "fresh ids minted");
    /* the pasted edge connects the NEW ids and deep-copied the label */
    flow_edge *pe = NULL;
    for (int i = 0; i < flow_edge_count(f); i++) {
      flow_edge *ce = &flow_edges(f)[i];
      if (ce->id != e) pe = ce;
    }
    ASSERT(pe != NULL, "pasted edge found");
    ASSERT(pe->source == pa && pe->target == pb, "pasted edge endpoints are the NEW ids");
    ASSERT(pe->label && strcmp(pe->label, "hi") == 0, "label round-tripped");
    ASSERT(pe->label != flow_get_edge(f, e)->label, "label is a DISTINCT deep copy");
    ASSERT_INT((int)strcmp(pe->source_handle, "out"), 0, "source handle kept");

    /* cumulative offset: second paste (no re-copy) lands at +2,+2 */
    ASSERT_INT(flow_paste(f), 2, "second paste pastes again");
    ASSERT(node_at(f, 7, 7) != -1, "second paste cascades to +2,+2");
    flow_free(f);
  }

  /* ---- 4: cut deletes (one event), paste restores ---- */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    int a = flow_add_node(f, "default", (flow_pt){5, 5},  (void*)"A");
    int b = flow_add_node(f, "default", (flow_pt){20, 5}, (void*)"B");
    flow_add_edge(f, a, b, "", "");
    flow_select_node(f, a, 0); flow_select_node(f, b, 1);
    flow_cut_selection(f);
    ASSERT_INT(flow_node_count(f), 0, "cut deletes the selection");
    ASSERT_INT(flow_edge_count(f), 0, "  incident edge cascaded");
    ASSERT_INT(flow_paste(f), 2, "paste after cut restores");
    ASSERT_INT(flow_node_count(f), 2, "  both nodes back (fresh ids)");
    ASSERT_INT(flow_edge_count(f), 1, "  edge back");
    flow_free(f);
  }

  /* ---- 5: duplicate is a one-shot; the clipboard is left untouched ---- */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    int z = flow_add_node(f, "default", (flow_pt){50, 10}, (void*)"Z");
    int a = flow_add_node(f, "default", (flow_pt){5, 5},   (void*)"A");
    int b = flow_add_node(f, "default", (flow_pt){20, 5},  (void*)"B");
    flow_select_node(f, z, 0);
    flow_copy_selection(f);                       /* clipboard holds Z */
    flow_select_node(f, a, 0); flow_select_node(f, b, 1);
    ASSERT_INT(flow_duplicate_selection(f), 2, "duplicate returns nodes added");
    ASSERT_INT(flow_node_count(f), 5, "a,b duplicated");
    ASSERT(node_at(f, 6, 6) != -1 && node_at(f, 21, 6) != -1, "dup at +1,+1");
    ASSERT_INT(flow_paste(f), 1, "clipboard still holds Z (duplicate untouched it)");
    ASSERT(node_at(f, 51, 11) != -1, "  pasted Z at +1,+1");
    flow_free(f);
  }

  /* ---- 6: clipboard survives source delete (deep copy) ---- */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    int a = flow_add_node(f, "default", (flow_pt){5, 5},  (void*)"A");
    int b = flow_add_node(f, "default", (flow_pt){20, 5}, (void*)"B");
    int e = flow_add_edge(f, a, b, "", "");
    flow_set_edge_label(f, e, "deep");
    flow_select_node(f, a, 0); flow_select_node(f, b, 1);
    flow_copy_selection(f);
    flow_delete_selection(f);
    ASSERT_INT(flow_node_count(f), 0, "sources gone");
    ASSERT_INT(flow_paste(f), 2, "paste after source delete works");
    ASSERT_INT(flow_edge_count(f), 1, "  edge re-minted");
    ASSERT(flow_edges(f)[0].label && strcmp(flow_edges(f)[0].label, "deep") == 0,
           "  label survived via the clipboard's own dup");
    flow_free(f);
  }

  /* ---- 7: paste is ONE undo step ---- */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    int a = flow_add_node(f, "default", (flow_pt){5, 5},  (void*)"A");
    int b = flow_add_node(f, "default", (flow_pt){20, 5}, (void*)"B");
    flow_add_edge(f, a, b, "", "");
    flow_select_node(f, a, 0); flow_select_node(f, b, 1);
    flow_copy_selection(f);
    int j0 = f->journal.n;
    flow_paste(f);
    ASSERT_INT(f->journal.n, j0 + 1, "paste journals exactly one step");
    flow_undo(f);
    ASSERT_INT(flow_node_count(f), 2, "undo removes the whole paste in one step");
    ASSERT_INT(flow_edge_count(f), 1, "  pasted edge gone too");
    flow_free(f);
  }

  /* ---- 8: validator gates pasted edges (deliberate asymmetry vs load) ---- */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    int a = flow_add_node(f, "default", (flow_pt){5, 5},  (void*)"A");
    int b = flow_add_node(f, "default", (flow_pt){20, 5}, (void*)"B");
    flow_add_edge(f, a, b, "", "");
    flow_select_node(f, a, 0); flow_select_node(f, b, 1);
    flow_copy_selection(f);
    flow_set_connection_validator(f, cb_reject_ab, NULL);
    ASSERT_INT(flow_paste(f), 2, "nodes land despite the validator");
    ASSERT_INT(flow_node_count(f), 4, "  both pasted");
    ASSERT_INT(flow_edge_count(f), 1, "pasted edge DROPPED by the live validator (partial paste)");
    flow_free(f);
  }

  /* ---- 9: dangling-parent paste lands as root at absolute coords ---- */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    int g = flow_add_node(f, "default", (flow_pt){10, 10}, (void*)"G");
    flow_node *gn = flow_get_node(f, g); gn->w = 30; gn->h = 12;
    int b = flow_add_node(f, "default", (flow_pt){15, 14}, (void*)"B");
    flow_set_parent(f, b, g);                     /* abs preserved: (15,14) */
    flow_select_node(f, b, 0);                    /* select ONLY the child */
    flow_copy_selection(f);
    ASSERT_INT(flow_paste(f), 1, "child pastes alone");
    int pb = node_at(f, 16, 15);                  /* abs +1,+1 */
    ASSERT(pb != -1, "pasted child at source ABS +1,+1");
    ASSERT_INT(flow_get_node(f, pb)->parent, -1, "original parent not pasted -> root");
    flow_free(f);
  }

  /* ---- 9b: parent+child pasted together keep the relationship ---- */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    int g = flow_add_node(f, "default", (flow_pt){10, 10}, (void*)"G");
    flow_node *gn = flow_get_node(f, g); gn->w = 30; gn->h = 12;
    int b = flow_add_node(f, "default", (flow_pt){15, 14}, (void*)"B");
    flow_set_parent(f, b, g);
    flow_select_node(f, g, 0); flow_select_node(f, b, 1);
    flow_copy_selection(f);
    ASSERT_INT(flow_paste(f), 2, "parent+child paste");
    int pg = node_at(f, 11, 11), pb = node_at(f, 16, 15);
    ASSERT(pg != -1 && pb != -1, "both pasted at +1,+1 abs");
    ASSERT_INT(flow_get_node(f, pb)->parent, pg, "pasted child reparented under the PASTED parent");
    flow_free(f);
  }

  /* ---- 10: pasted elements become the selection, one event ---- */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    int a = flow_add_node(f, "default", (flow_pt){5, 5},  (void*)"A");
    int b = flow_add_node(f, "default", (flow_pt){20, 5}, (void*)"B");
    flow_select_node(f, a, 0); flow_select_node(f, b, 1);
    flow_copy_selection(f);
    flow_callbacks cb = {0}; cb.on_selection_change = on_sel; flow_set_callbacks(f, cb);
    sel_fires = 0;
    flow_paste(f);
    ASSERT_INT(sel_fires, 1, "paste fires on_selection_change exactly once");
    ASSERT_INT(flow_selected_count(f), 2, "the pasted pair is the new selection");
    int ids[4]; int n = flow_selected_nodes(f, ids, 4);
    ASSERT_INT(n, 2, "  two selected");
    ASSERT(ids[0] != a && ids[0] != b && ids[1] != a && ids[1] != b,
           "  and they are the PASTED ids, not the sources");
    flow_free(f);
  }

  /* ---- 11: empty clipboard / empty selection ---- */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    flow_add_node(f, "default", (flow_pt){5, 5}, (void*)"A");
    int j0 = f->journal.n;
    flow_callbacks cb = {0}; cb.on_selection_change = on_sel; flow_set_callbacks(f, cb);
    sel_fires = 0;
    ASSERT_INT(flow_paste(f), 0, "paste with empty clipboard returns 0");
    ASSERT_INT(f->journal.n, j0, "  journals nothing");
    ASSERT_INT(sel_fires, 0, "  fires nothing");
    ASSERT_INT(flow_duplicate_selection(f), 0, "duplicate with empty selection returns 0");
    ASSERT_INT(f->journal.n, j0, "  journals nothing");
    flow_free(f);
  }

  /* ---- 12: ownership across flow_load + keys (ASan exercises the frees) ---- */
  {
    const char *P = "/tmp/flow_clip_io.json";
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    int a = flow_add_node(f, "default", (flow_pt){5, 5},  (void*)"A");
    int b = flow_add_node(f, "default", (flow_pt){20, 5}, (void*)"B");
    int e = flow_add_edge(f, a, b, "", "");
    flow_set_edge_label(f, e, "io");
    ASSERT_INT(flow_save(f, P), 0, "save ok");
    flow_select_node(f, a, 0); flow_select_node(f, b, 1);
    flow_feed(f, "y", 1);                         /* copy via built-in key */
    flow_feed(f, "p", 1);                         /* paste via built-in key */
    ASSERT_INT(flow_node_count(f), 4, "y/p built-ins copy and paste");
    ASSERT_INT(flow_load(f, P), 0, "load replaces the graph");
    ASSERT_INT(flow_node_count(f), 2, "  loaded graph");
    ASSERT_INT(flow_paste(f), 2, "clipboard SURVIVES flow_load (not graph state)");
    ASSERT_INT(flow_node_count(f), 4, "  pasted into the loaded graph");
    /* cut + duplicate via keys */
    flow_select_node(f, flow_nodes(f)[0].id, 0);
    flow_feed(f, "d", 1);                         /* duplicate */
    ASSERT_INT(flow_node_count(f), 5, "d duplicates the selection");
    flow_feed(f, "c", 1);                         /* cut the (now selected) duplicate */
    ASSERT_INT(flow_node_count(f), 4, "c cuts");
    flow_free(f);                                 /* clipboard freed exactly once */
    remove(P);
  }

  /* ---- 13: group container size survives paste (inc-6 #1 paste-group-remeasure) ---- */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    int g = flow_add_node(f, "group", (flow_pt){10, 10}, NULL);
    flow_node *gn = flow_get_node(f, g); gn->w = 20; gn->h = 8;   /* the flow_group post-add idiom */
    flow_select_node(f, g, 0);
    flow_copy_selection(f);
    ASSERT_INT(flow_paste(f), 1, "paste mints the group");
    int pg = node_at(f, 11, 11);
    ASSERT(pg != -1 && pg != g, "pasted group at +1 offset");
    ASSERT_INT(flow_get_node(f, pg)->w, 20, "pasted group keeps w");
    ASSERT_INT(flow_get_node(f, pg)->h, 8,  "pasted group keeps h");
    flow_free(f);
  }

  /* ---- 14: manually-resized default node survives paste ---- */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    int a = flow_add_node(f, "default", (flow_pt){5, 5}, (void*)"A");
    flow_node *an = flow_get_node(f, a); an->w = 30; an->h = 5;   /* caller resize, would re-measure 5x3 */
    flow_select_node(f, a, 0);
    flow_copy_selection(f);
    ASSERT_INT(flow_paste(f), 1, "paste mints the resized node");
    int pa = node_at(f, 6, 6);
    ASSERT(pa != -1, "clone at +1 offset");
    ASSERT_INT(flow_get_node(f, pa)->w, 30, "manual w survives paste");
    ASSERT_INT(flow_get_node(f, pa)->h, 5,  "manual h survives paste");
    flow_free(f);
  }

  /* ---- 15: content-measured node unaffected — the restore is inert ---- */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    int a = flow_add_node(f, "default", (flow_pt){5, 5}, (void*)"Hello");
    flow_select_node(f, a, 0);
    flow_copy_selection(f);
    ASSERT_INT(flow_paste(f), 1, "paste mints the measured node");
    int pa = node_at(f, 6, 6);
    ASSERT(pa != -1, "clone at +1 offset");
    ASSERT_INT(flow_get_node(f, pa)->w, flow_get_node(f, a)->w, "restore inert for measured node");
    ASSERT_INT(flow_get_node(f, pa)->w, 9, "  = strlen(\"Hello\")+4");
    flow_free(f);
  }

  /* ---- 16: duplicate (d) shares the fix — same flow__paste_snaps core ---- */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    int g = flow_add_node(f, "group", (flow_pt){10, 10}, NULL);
    flow_node *gn = flow_get_node(f, g); gn->w = 20; gn->h = 8;
    flow_select_node(f, g, 0);
    ASSERT_INT(flow_duplicate_selection(f), 1, "duplicate mints");
    int d1 = node_at(f, 11, 11);
    ASSERT(d1 != -1, "duplicate at +1 offset");
    ASSERT_INT(flow_get_node(f, d1)->w, 20, "duplicate keeps w");
    ASSERT_INT(flow_get_node(f, d1)->h, 8,  "duplicate keeps h");
    flow_select_node(f, d1, 0);                  /* d built-in routes through the same core */
    flow_feed(f, "d", 1);
    int d2 = node_at(f, 12, 12);
    ASSERT(d2 != -1, "d key duplicates at +1");
    ASSERT_INT(flow_get_node(f, d2)->w, 20, "d key clone keeps w");
    ASSERT_INT(flow_get_node(f, d2)->h, 8,  "d key clone keeps h");
    flow_free(f);
  }

  /* ---- 17: restored size round-trips undo/redo (ADD_NODE snaps at undo time) ---- */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    int g = flow_add_node(f, "group", (flow_pt){10, 10}, NULL);
    flow_node *gn = flow_get_node(f, g); gn->w = 20; gn->h = 8;
    flow_select_node(f, g, 0);
    flow_copy_selection(f);
    int j0 = f->journal.n;
    ASSERT_INT(flow_paste(f), 1, "paste");
    ASSERT_INT(f->journal.n, j0 + 1, "paste is one undo step");
    flow_undo(f);
    ASSERT_INT(flow_node_count(f), 1, "undo removes the pasted group");
    flow_redo(f);
    ASSERT_INT(flow_node_count(f), 2, "redo reinstates it");
    int pg = node_at(f, 11, 11);
    ASSERT(pg != -1, "re-pasted group relocated");
    ASSERT_INT(flow_get_node(f, pg)->w, 20, "w survives undo/redo");
    ASSERT_INT(flow_get_node(f, pg)->h, 8,  "h survives undo/redo");
    flow_free(f);
  }

  return flowtest_report("test_clipboard");
}
