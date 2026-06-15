/* flowchart demo smoke test (increment-3 package #9): builds the DEMO'S OWN graph
   headless by including demos/flowchart.c with its main() compiled out, then asserts
   the groups + auto-layout integration: layered TB ranks are monotonic, no node-cell
   overlaps, the labeled group encloses its children, fit framed the result, and the
   demo's l/g/G key bindings invoke layout/group/ungroup through flow_feed. */
#define FLOW_IMPLEMENTATION
#include "../flow.h"
#include "flowtest.h"

#define FLOWCHART_TEST                 /* compile the demo's setup, not its main()/run loop */
#include "../demos/flowchart.c"

/* STRICT cell-sharing (half-open; touching edges do NOT count), ancestor pairs skipped
   — children legitimately sit INSIDE their container's rect. Same as test_layout.c. */
static int rects_share_cell(flow_rect a, flow_rect b) {
  return a.x < b.x + b.w && b.x < a.x + a.w &&
         a.y < b.y + b.h && b.y < a.y + a.h;
}
static int overlapping_pairs(flow_t *f) {
  int bad = 0;
  for (int i = 0; i < flow_node_count(f); i++)
    for (int j = i + 1; j < flow_node_count(f); j++) {
      flow_node *a = &flow_nodes(f)[i], *b = &flow_nodes(f)[j];
      if (flow_is_ancestor(f, a->id, b->id) || flow_is_ancestor(f, b->id, a->id)) continue;
      if (rects_share_cell(flow_node_rect_abs(f, a), flow_node_rect_abs(f, b))) bad++;
    }
  return bad;
}
static int by_label(flow_t *f, const char *lbl) {     /* default nodes carry their label as data */
  for (int i = 0; i < flow_node_count(f); i++) {
    flow_node *n = &flow_nodes(f)[i];
    if (n->data && strcmp((const char*)n->data, lbl) == 0) return n->id;
  }
  return -1;
}
static int rect_contains_rect(flow_rect outer, flow_rect inner) {
  return inner.x >= outer.x && inner.y >= outer.y &&
         inner.x + inner.w <= outer.x + outer.w && inner.y + inner.h <= outer.y + outer.h;
}

int main(void) {
  flow_t *f = flow_new(80, 24);
  int gid = flowchart_setup(f);

  /* ---- structure: 6 flowchart nodes + 1 labeled group container, 6 edges ---- */
  ASSERT(gid != -1, "setup returns the branches group id");
  ASSERT_INT(flow_node_count(f), 7, "6 nodes + the group container");
  ASSERT_INT(flow_edge_count(f), 6, "flowchart edge count");
  flow_node *g = flow_get_node(f, gid);
  ASSERT(g && strcmp(g->type, "group") == 0, "container is the built-in group type");
  ASSERT(g && g->data && strcmp((const char*)g->data, "branches") == 0, "group is labeled");

  int start = by_label(f, "start"), validate = by_label(f, "validate"), decide = by_label(f, "valid?");
  int save = by_label(f, "save"), reject = by_label(f, "reject"), done = by_label(f, "done");
  ASSERT(start != -1 && validate != -1 && decide != -1 && save != -1 && reject != -1 && done != -1,
         "all six flowchart nodes present");

  /* ---- startup layered TB layout: ranks march down, branches share a rank ---- */
  flow_pt ps = flow_node_abs(f, flow_get_node(f, start));
  flow_pt pv = flow_node_abs(f, flow_get_node(f, validate));
  flow_pt pd = flow_node_abs(f, flow_get_node(f, decide));
  flow_pt pa = flow_node_abs(f, flow_get_node(f, save));
  flow_pt pr = flow_node_abs(f, flow_get_node(f, reject));
  flow_pt po = flow_node_abs(f, flow_get_node(f, done));
  ASSERT(ps.y < pv.y && pv.y < pd.y && pd.y < pa.y && pa.y < po.y, "TB ranks monotonic down the chart");
  ASSERT_INT(pa.y, pr.y, "branches save/reject share a rank");
  ASSERT(pa.x != pr.x, "branches separate horizontally");
  ASSERT_INT(overlapping_pairs(f), 0, "no two nodes share a cell");

  /* ---- the group boxes the branches: parented + contained ---- */
  flow_node *ns = flow_get_node(f, save), *nr = flow_get_node(f, reject);
  ASSERT(ns->parent == gid && nr->parent == gid, "save/reject are children of the group");
  flow_rect gr = flow_node_rect_abs(f, g);
  ASSERT(rect_contains_rect(gr, flow_node_rect_abs(f, ns)), "group rect encloses save");
  ASSERT(rect_contains_rect(gr, flow_node_rect_abs(f, nr)), "group rect encloses reject");

  /* ---- startup fit framed the chart (margin 2 at the 80x24 default) ---- */
  {
    int ok = 1;
    for (int i = 0; i < flow_node_count(f); i++) {
      flow_node *n = &flow_nodes(f)[i];
      flow_pt s = flow_to_screen(f, flow_node_abs(f, n));
      if (s.x < 0 || s.y < 0 || s.x + n->w > 80 || s.y + n->h > 24) ok = 0;
    }
    ASSERT(ok, "fit_after framed every node on screen");
  }

  /* ---- 'l' key re-runs layout through the registry (one undo step) ----
     DELIBERATELY does NOT assert chart shape here: with save/reject grouped, the
     edges decide->save/reject and save/reject->done all cross the partition
     boundary and v1 layout ignores them by design — so re-running 'l' re-ranks
     the container (and `done`) as isolated rank-0 nodes. A known v1 limitation
     the demo surfaces, not a bug; the invariants that DO hold are asserted. */
  {
    int steps = f->journal.n;
    flow_feed(f, "l", 1);
    ASSERT_INT(f->journal.n, steps + 1, "'l' invoked flow_layout (exactly one undo step)");
    ASSERT_INT(overlapping_pairs(f), 0, "re-layout: still no shared cells");
  }

  /* ---- 'g' groups the current selection ---- */
  {
    int before = flow_node_count(f);
    flow_select_node(f, start, 0);
    flow_select_node(f, validate, 1);                  /* additive */
    flow_feed(f, "g", 1);
    ASSERT_INT(flow_node_count(f), before + 1, "'g' added a group container");
    flow_node *s2 = flow_get_node(f, start), *v2 = flow_get_node(f, validate);
    ASSERT(s2->parent != -1 && s2->parent == v2->parent, "'g' reparented the selection into the new group");
    /* ---- 'G' ungroups the selected group: children survive, container removed ---- */
    int g2 = s2->parent;
    flow_select_node(f, g2, 0);
    flow_feed(f, "G", 1);
    ASSERT_INT(flow_node_count(f), before, "'G' removed the container");
    s2 = flow_get_node(f, start); v2 = flow_get_node(f, validate);
    ASSERT(s2 && v2, "'G' kept the children alive");
    ASSERT(s2->parent == -1 && v2->parent == -1, "'G' detached the children to top level");
  }

  /* ---- inc-4 integration: prevent-cycles validator (#8 reachability x #9 gate) ---- */
  {
    int start2 = by_label(f, "start"), done2 = by_label(f, "done");
    int ec = flow_edge_count(f);
    ASSERT_INT(flow_add_edge(f, done2, start2, "", ""), -1,
               "back-edge done->start rejected (would close a directed cycle)");
    ASSERT_INT(flow_edge_count(f), ec, "  edge count unchanged on reject");
    int fwd = flow_add_edge(f, start2, done2, "", "");
    ASSERT(fwd != -1, "DAG-respecting edge start->done allowed through the validator");
    flow_remove_edge(f, fwd);                          /* restore the chart */
  }

  /* ---- inc-4 integration: 'h' hides the selection, 'H' shows everything ---- */
  {
    int start2 = by_label(f, "start");
    flow_select_node(f, start2, 0);
    flow_feed(f, "h", 1);
    ASSERT(flow_get_node(f, start2)->flags & FLOW_HIDDEN, "'h' hid the selected node");
    ASSERT_INT(flow_selected_count(f), 0, "  hiding deselected it (engine rule)");
    flow_feed(f, "H", 1);
    ASSERT(!(flow_get_node(f, start2)->flags & FLOW_HIDDEN), "'H' shows all again");
  }

  /* ---- inc-5 #10: '/' command palette (key hook + flow_find_nodes + framing).
     Seven searchable labels: start/validate/valid?/save/reject/done + the group
     container 'branches' (the group type's new label() accessor). ---- */
  {
    int m[8];
    flow_feed(f, "/", 1);                              /* open via the registry binding */
    ASSERT_INT(fc_pal.open, 1, "'/' opens the palette");
    flow_feed(f, "s", 1);                              /* hook consumes the printable */
    ASSERT_INT(fc_pal.qn, 1, "query took the char (not the engine/demo 's' paths)");
    ASSERT_INT(fc_pal_matches(f, m, 8), 3, "'s' matches start+save+branches");
    flow_feed(f, "a", 1);
    ASSERT_INT(fc_pal_matches(f, m, 8), 1, "'sa' narrows to save (substring, not prefix)");
    ASSERT_INT(m[0], by_label(f, "save"), "  the match IS save");
    flow_feed(f, "\x7f", 1);                           /* Backspace */
    ASSERT_INT(fc_pal.qn, 1, "Backspace shrank the query to 's'");
    ASSERT_INT(fc_pal_matches(f, m, 8), 3, "  match set re-widened");
    flow_feed(f, "a", 1);                              /* back to 'sa' */
    float ox0 = f->view.ox, oy0 = f->view.oy;
    flow_feed(f, "\r", 1);                             /* Enter: select + frame */
    ASSERT_INT(fc_pal.open, 0, "Enter closes the palette");
    ASSERT_INT(flow_selected_node(f), by_label(f, "save"), "Enter selected the first match");
    ASSERT(f->view.ox != ox0 || f->view.oy != oy0, "Enter framed the match (view moved)");

    /* lone ESC closes without selecting; CSIs are DROPPED while modal (inc-6 #6) */
    int sel_before = flow_selected_node(f);
    flow_feed(f, "/", 1);
    ASSERT_INT(fc_pal.open, 1, "palette reopens");
    float oy1 = f->view.oy;
    flow_feed(f, "\x1b[A", 3);                         /* arrow CSI: hook returns 0, modal DROPS it */
    ASSERT(f->view.oy == oy1, "arrow does NOT pan while the palette is open (modal capture)");
    ASSERT_INT(fc_pal.open, 1, "  palette stayed open");
    flow_feed(f, "\x1b", 1);                           /* lone ESC */
    ASSERT_INT(fc_pal.open, 0, "lone ESC closes the palette");
    ASSERT_INT(flow_selected_node(f), sel_before, "  selection unchanged");

    /* hidden nodes are skipped by the PALETTE (view-level UI choice over the
       model-level query, which includes them) */
    flow_set_node_hidden(f, by_label(f, "save"), 1);
    flow_feed(f, "/", 1); flow_feed(f, "s", 1); flow_feed(f, "a", 1);
    ASSERT_INT(fc_pal_matches(f, m, 8), 0, "palette skips the hidden match");
    ASSERT_INT(flow_find_nodes(f, "sa", m, 8), 1, "  engine query still includes it (model-level)");
    flow_feed(f, "\x1b", 1);
    flow_set_node_hidden(f, by_label(f, "save"), 0);
  }

  /* ---- inc-5 #8 integration: 'a' toggles alignment helper lines ---- */
  {
    ASSERT_INT(f->helper_on, 0, "helper lines start OFF");
    flow_feed(f, "a", 1);
    ASSERT_INT(f->helper_on, 1, "'a' turns helper lines on");
    flow_feed(f, "a", 1);
    ASSERT_INT(f->helper_on, 0, "'a' again turns them off");
  }

  flow_free(f);
  return flowtest_report("test_flowchart");
}
