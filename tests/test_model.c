#define FLOW_IMPLEMENTATION
#include "../flow.h"
#include "flowtest.h"

/* a trivial node type so measure() gives deterministic sizes for hit/bounds */
static void m_measure(const flow_node *n, int *w, int *h) { (void)n; *w = 4; *h = 3; }
static void m_render(const flow_node *n, flow_surface *s, flow_render_ctx c) { (void)n;(void)s;(void)c; }
/* designated init: omits the optional save/load/label fields (zero = unused) without
   tripping -Wmissing-field-initializers under CI's -Werror */
static const flow_node_type M = { .type = "box", .measure = m_measure, .render = m_render };

/* hidden x selection (inc-4 #11) */
static int mh_sel_fires = 0;
static void mh_on_sel(flow_t *f, const int *ids, int n, void *u) { (void)f;(void)ids;(void)n;(void)u; mh_sel_fires++; }

int main(void) {
  flow_t *f = flow_new(80, 24);
  flow_register_node_type(f, &M);

  int a = flow_add_node(f, "box", (flow_pt){10, 5}, NULL);
  int b = flow_add_node(f, "box", (flow_pt){20, 5}, NULL);
  ASSERT_INT(a, 1, "first id is 1"); ASSERT_INT(b, 2, "second id is 2");
  ASSERT_INT(flow_node_count(f), 2, "two nodes");
  ASSERT_INT(flow_get_node(f, a)->w, 4, "measured w");
  ASSERT_INT(flow_get_node(f, a)->h, 3, "measured h");

  /* edges + validation */
  ASSERT_INT(flow_add_edge(f, a, b, "", "") > 0, 1, "valid edge ok");
  ASSERT_INT(flow_add_edge(f, a, a, "", ""), -1, "reject self-edge");
  ASSERT_INT(flow_add_edge(f, a, b, "", ""), -1, "reject duplicate");
  ASSERT_INT(flow_add_edge(f, a, 999, "", ""), -1, "reject missing endpoint");
  ASSERT_INT(flow_edge_count(f), 1, "one edge");

  /* transform + pan */
  flow_pt s = flow_to_screen(f, (flow_pt){10, 5});
  ASSERT_INT(s.x, 10, "screen x at zero pan"); ASSERT_INT(s.y, 5, "screen y at zero pan");
  flow_pan(f, 3, -2);
  s = flow_to_screen(f, (flow_pt){10, 5});
  ASSERT_INT(s.x, 13, "screen x after pan"); ASSERT_INT(s.y, 3, "screen y after pan");
  flow_pan(f, -3, 2); /* reset */

  /* bounds: union of node rects (10,5,4,3) and (20,5,4,3) -> x10 y5 w14 h3 */
  flow_rect bb = flow_bounds(f);
  ASSERT_INT(bb.x, 10, "bounds x"); ASSERT_INT(bb.y, 5, "bounds y");
  ASSERT_INT(bb.w, 14, "bounds w"); ASSERT_INT(bb.h, 3, "bounds h");

  /* hit-test in screen space (zero pan): point inside node b */
  ASSERT_INT(flow_hit_node(f, (flow_pt){21, 6}), b, "hit node b");
  ASSERT_INT(flow_hit_node(f, (flow_pt){0, 0}), -1, "miss -> -1");

  flow_free(f);

  /* default node type: data is a C-string label; measure = strlen+4 x 3 */
  flow_t *g = flow_new(80, 24);
  flow_register_defaults(g);
  int d = flow_add_node(g, "default", (flow_pt){0,0}, (void*)"hi");
  ASSERT_INT(flow_get_node(g, d)->w, 6, "default measure w = len('hi')+4");
  ASSERT_INT(flow_get_node(g, d)->h, 3, "default measure h = 3");
  ASSERT(flow_node_type_for(g, "default") != NULL, "default node type registered");
  ASSERT(flow_edge_type_for(g, "default") != NULL, "default edge type registered");
  flow_free(g);

  /* flow_remove_node cascade: incident edges + child nodes, survivors intact */
  {
    flow_t *h = flow_new(80, 24); flow_register_defaults(h);
    int n1 = flow_add_node(h, "default", (flow_pt){0, 0}, (void*)"a");   /* victim */
    int n2 = flow_add_node(h, "default", (flow_pt){20, 0}, (void*)"b");  /* peer */
    int n3 = flow_add_node(h, "default", (flow_pt){40, 0}, (void*)"c");  /* peer */
    int child = flow_add_node(h, "default", (flow_pt){2, 2}, (void*)"k");/* child of n1 */
    flow_get_node(h, child)->parent = n1;
    int ea = flow_add_edge(h, n1, n2, "", "");   /* incident to victim */
    int eb = flow_add_edge(h, n3, n1, "", "");   /* incident to victim */
    int ec = flow_add_edge(h, n2, n3, "", "");   /* unrelated, survives */
    flow_get_edge(h, ea)->label = strdup("ea");  /* labels must be freed on removal */
    flow_get_edge(h, eb)->label = strdup("eb");

    flow_remove_node(h, n1);
    ASSERT(flow_get_node(h, n1) == NULL, "cascade: victim removed");
    ASSERT(flow_get_node(h, child) == NULL, "cascade: child node removed");
    ASSERT(flow_get_edge(h, ea) == NULL, "cascade: incident edge ea removed");
    ASSERT(flow_get_edge(h, eb) == NULL, "cascade: incident edge eb removed");
    ASSERT(flow_get_edge(h, ec) != NULL, "cascade: unrelated edge ec survives");
    ASSERT(flow_get_node(h, n2) != NULL, "cascade: peer n2 survives");
    ASSERT(flow_get_node(h, n3) != NULL, "cascade: peer n3 survives");
    ASSERT_INT(flow_node_count(h), 2, "cascade: two nodes remain");
    ASSERT_INT(flow_edge_count(h), 1, "cascade: one edge remains");
    /* surviving ids unchanged */
    ASSERT_INT(flow_get_node(h, n2)->id, n2, "cascade: n2 id unchanged");
    ASSERT_INT(flow_get_edge(h, ec)->id, ec, "cascade: ec id unchanged");

    /* flow_remove_edge: free + no-op on absent id */
    flow_get_edge(h, ec)->label = strdup("ec");
    flow_remove_edge(h, ec);
    ASSERT_INT(flow_edge_count(h), 0, "remove_edge dropped ec");
    flow_remove_edge(h, 9999);  /* no-op, no crash */
    ASSERT_INT(flow_edge_count(h), 0, "remove_edge absent id is no-op");
    flow_free(h);
  }

  /* ---- FLOW_EXTENT_PARENT: child clamps inside its parent's absolute rect ---- */
  {
    flow_t *g = flow_new(80, 24); flow_register_defaults(g);
    int p = flow_add_node(g, "default", (flow_pt){10, 10}, (void*)"P");
    flow_node *pn = flow_get_node(g, p); pn->w = 30; pn->h = 20;     /* parent rect (10,10,30,20) */
    int c = flow_add_node(g, "default", (flow_pt){15, 15}, (void*)"C");
    flow_node *cn = flow_get_node(g, c); cn->w = 4; cn->h = 3;
    flow_set_parent(g, c, p);                                        /* abs preserved: rel (5,5) */
    flow_get_node(g, c)->flags |= FLOW_EXTENT_PARENT;
    flow_move_node(g, c, (flow_pt){20, 28});                         /* y would span to 31 > 30 */
    flow_pt a2 = flow_node_abs(g, flow_get_node(g, c));
    ASSERT_INT(a2.x, 20, "parent-extent: x in bounds, untouched");
    ASSERT_INT(a2.y, 27, "parent-extent bottom: y flush so 27+3==30");
    flow_move_node(g, c, (flow_pt){38, 15});                         /* x would span to 42 > 40 */
    a2 = flow_node_abs(g, flow_get_node(g, c));
    ASSERT_INT(a2.x, 36, "parent-extent right: x flush so 36+4==40");
    flow_move_node(g, c, (flow_pt){2, 2});                           /* top-left overflow */
    a2 = flow_node_abs(g, flow_get_node(g, c));
    ASSERT_INT(a2.x, 10, "parent-extent left: flush to parent.x");
    ASSERT_INT(a2.y, 10, "parent-extent top: flush to parent.y");
    flow_move_node(g, c, (flow_pt){50, 50});                         /* bottom-right corner */
    a2 = flow_node_abs(g, flow_get_node(g, c));
    ASSERT_INT(a2.x, 36, "corner overflow: x flush right");
    ASSERT_INT(a2.y, 27, "corner overflow: y flush bottom");
    flow_move_node(g, c, (flow_pt){12, 12});                         /* interior: untouched */
    a2 = flow_node_abs(g, flow_get_node(g, c));
    ASSERT_INT(a2.x, 12, "interior: x unclamped");
    ASSERT_INT(a2.y, 12, "interior: y unclamped");
    /* flag OFF -> unclamped */
    flow_get_node(g, c)->flags &= ~(unsigned)FLOW_EXTENT_PARENT;
    flow_move_node(g, c, (flow_pt){50, 50});
    a2 = flow_node_abs(g, flow_get_node(g, c));
    ASSERT_INT(a2.x, 50, "flag off: x unclamped");
    /* flag on an UNPARENTED node -> no clamp (parent==-1 guard) */
    int u = flow_add_node(g, "default", (flow_pt){0, 0}, (void*)"U");
    flow_get_node(g, u)->flags |= FLOW_EXTENT_PARENT;
    flow_move_node(g, u, (flow_pt){500, 500});
    ASSERT_INT(flow_get_node(g, u)->pos.x, 500, "unparented: no clamp");
    /* ordering: node-extent FIRST, parent-extent SECOND. Disjoint ranges discriminate:
       node-extent caps x<=8 (12-4), parent demands x>=10 -> last-applied (parent) wins. */
    flow_get_node(g, c)->flags |= FLOW_EXTENT_PARENT;
    flow_set_node_extent(g, (flow_rect){0, 0, 12, 100});
    flow_move_node(g, c, (flow_pt){50, 15});
    a2 = flow_node_abs(g, flow_get_node(g, c));
    ASSERT_INT(a2.x, 10, "disjoint extents: parent clamp applied AFTER node extent (wins)");
    flow_set_node_extent(g, (flow_rect){0, 0, 0, 0});
    flow_free(g);
  }

  /* ---- FLOW_EXTENT_PARENT x flow_layout: layout commits respect the clamp ---- */
  {
    flow_t *g = flow_new(80, 24); flow_register_defaults(g);
    int p = flow_add_node(g, "group", (flow_pt){10, 10}, NULL);      /* group measure preserves w/h */
    flow_node *pn = flow_get_node(g, p); pn->w = 8; pn->h = 8;
    int c1 = flow_add_node(g, "default", (flow_pt){11, 11}, (void*)"a");
    int c2 = flow_add_node(g, "default", (flow_pt){12, 12}, (void*)"b");
    flow_set_parent(g, c1, p); flow_set_parent(g, c2, p);
    flow_get_node(g, c1)->flags |= FLOW_EXTENT_PARENT;
    flow_get_node(g, c2)->flags |= FLOW_EXTENT_PARENT;
    flow_layout_opts lo = {0};
    flow_layout(g, lo);                                              /* 5x3 children spread > 8x8 unclamped */
    flow_rect pr = flow_node_rect_abs(g, flow_get_node(g, p));
    flow_rect r1 = flow_node_rect_abs(g, flow_get_node(g, c1));
    flow_rect r2 = flow_node_rect_abs(g, flow_get_node(g, c2));
    ASSERT(r1.x >= pr.x && r1.y >= pr.y && r1.x + r1.w <= pr.x + pr.w && r1.y + r1.h <= pr.y + pr.h,
           "layout: child 1 clamped inside the container");
    ASSERT(r2.x >= pr.x && r2.y >= pr.y && r2.x + r2.w <= pr.x + pr.w && r2.y + r2.h <= pr.y + pr.h,
           "layout: child 2 clamped inside the container");
    flow_free(g);
  }

  /* ---- FLOW_HIDDEN: view-level skip for hit/bounds/handles; sig-gated deselect ---- */
  {
    flow_t *g = flow_new(80, 24); flow_register_defaults(g);
    int a = flow_add_node(g, "default", (flow_pt){10, 5}, (void*)"A");   /* rect (10,5,5,3) */
    int b = flow_add_node(g, "default", (flow_pt){30, 5}, (void*)"B");
    int steps = g->journal.n;

    /* hit-test skip */
    ASSERT_INT(flow_hit_node(g, (flow_pt){12, 6}), a, "precondition: A hittable");
    flow_set_node_hidden(g, a, 1);
    ASSERT_INT(flow_hit_node(g, (flow_pt){12, 6}), -1, "hidden A is not hittable");
    ASSERT_INT(g->journal.n, steps, "hide journals nothing (UI-transient, like zoom)");

    /* bounds exclude hidden; all-hidden -> zero rect; fit_view no-ops on it */
    flow_rect bb = flow_bounds(g);
    ASSERT_INT(bb.x, 30, "bounds exclude hidden A (left edge is B's)");
    flow_set_node_hidden(g, b, 1);
    bb = flow_bounds(g);
    ASSERT(bb.w == 0 && bb.h == 0, "all hidden: bounds is the zero rect");
    float ox0 = g->view.ox;
    flow_fit_view(g, 2);
    ASSERT(g->view.ox == ox0, "fit_view no-ops on zero bounds (existing guard)");
    flow_set_node_hidden(g, b, 0);

    /* un-hide restores hittability */
    flow_set_node_hidden(g, a, 0);
    ASSERT_INT(flow_hit_node(g, (flow_pt){12, 6}), a, "un-hidden A hittable again");

    /* handles gate: hovered but hidden -> no handle hit, even though hover reveals */
    flow_set_hover(g, a);
    int hn = -2;
    ASSERT(flow_hit_handle(g, (flow_pt){14, 6}, &hn) >= 0, "precondition: hovered A's handle hittable");
    flow_set_node_hidden(g, a, 1);
    ASSERT_INT(flow_hit_handle(g, (flow_pt){14, 6}, &hn), -1, "hidden node's handles are not hittable");
    flow_set_node_hidden(g, a, 0);

    /* missing id: graceful no-op */
    flow_set_node_hidden(g, 9999, 1);
    flow_set_edge_hidden(g, 9999, 1);
    ASSERT(1, "missing ids: setters are no-ops, no crash");
    flow_free(g);
  }

  /* ---- FLOW_HIDDEN x selection: hiding a selected node deselects, sig-gated ---- */
  {
    flow_t *g = flow_new(80, 24); flow_register_defaults(g);
    int a = flow_add_node(g, "default", (flow_pt){10, 5}, (void*)"A");
    int b = flow_add_node(g, "default", (flow_pt){30, 5}, (void*)"B");
    flow_callbacks cb = {0}; cb.on_selection_change = mh_on_sel; cb.user = NULL;
    flow_set_callbacks(g, cb);
    flow_select_node(g, a, 0); flow_select_node(g, b, 1);
    mh_sel_fires = 0;
    flow_set_node_hidden(g, a, 1);
    ASSERT_INT(flow_selected_count(g), 1, "hiding selected A deselects it (B remains)");
    ASSERT_INT(mh_sel_fires, 1, "on_selection_change fired once for the hide-deselect");
    flow_set_node_hidden(g, a, 1);                     /* idempotent re-hide */
    ASSERT_INT(mh_sel_fires, 1, "re-hiding fires nothing (selection unchanged)");
    flow_set_node_hidden(g, a, 0);
    ASSERT_INT(flow_selected_count(g), 1, "un-hiding does NOT reselect (hide discards state)");
    ASSERT_INT(mh_sel_fires, 1, "un-hide fires no selection event");
    /* hiding an UNselected node never fires */
    int c = flow_add_node(g, "default", (flow_pt){50, 5}, (void*)"C");
    flow_set_node_hidden(g, c, 1);
    ASSERT_INT(mh_sel_fires, 1, "hiding an unselected node fires nothing");
    flow_free(g);
  }

  return flowtest_report("test_model");
}
