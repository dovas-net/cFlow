#define FLOW_IMPLEMENTATION
#include "../flow.h"
#include "flowtest.h"

/* observer log: a single struct threaded through cb.user so each test block gets a
   fresh zeroed counter set (ev_log L = {0}). */
typedef struct {
  int  sel_fires; int sel_n;  int sel_ids[16];
  int  del_fires; int del_n;  int del_ids[16];
  int  click_fires; int click_last;
  int  dbl_fires;   int dbl_last;
  int  eclick_fires; int eclick_last;
  int  edbl_fires;   int edbl_last;
  int  ectx_fires;   int ectx_last; flow_pt ectx_screen;
  int  nctx_fires;   int nctx_last;
} ev_log;

static void on_sel(flow_t *f, const int *ids, int n, void *u) {
  (void)f; ev_log *L = (ev_log*)u; L->sel_fires++; L->sel_n = n;
  for (int i = 0; i < n && i < 16; i++) L->sel_ids[i] = ids[i];
}
static void on_del(flow_t *f, const int *ids, int n, void *u) {
  (void)f; ev_log *L = (ev_log*)u; L->del_fires++; L->del_n = n;
  for (int i = 0; i < n && i < 16; i++) L->del_ids[i] = ids[i];
}
static void on_click(flow_t *f, int node, void *u) { (void)f; ev_log *L = (ev_log*)u; L->click_fires++; L->click_last = node; }
static void on_dbl(flow_t *f, int node, void *u)   { (void)f; ev_log *L = (ev_log*)u; L->dbl_fires++;   L->dbl_last   = node; }
static void on_eclick(flow_t *f, int edge, void *u) { (void)f; ev_log *L = (ev_log*)u; L->eclick_fires++; L->eclick_last = edge; }
static void on_edbl(flow_t *f, int edge, void *u)   { (void)f; ev_log *L = (ev_log*)u; L->edbl_fires++;   L->edbl_last   = edge; }
static void on_ectx(flow_t *f, int edge, flow_pt screen, void *u) { (void)f; ev_log *L = (ev_log*)u; L->ectx_fires++; L->ectx_last = edge; L->ectx_screen = screen; }
static void on_nctx(flow_t *f, int node, flow_pt screen, void *u) { (void)f;(void)screen; ev_log *L = (ev_log*)u; L->nctx_fires++; L->nctx_last = node; }

/* feed a no-motion left click (press+release) at world cell (wx,wy); zoom 1 / no pan
   means screen==world. SGR is 1-based. */
static void click_at(flow_t *f, int wx, int wy) {
  char buf[32];
  snprintf(buf, sizeof buf, "\x1b[<0;%d;%dM", wx + 1, wy + 1); flow_feed(f, buf, (int)strlen(buf));
  snprintf(buf, sizeof buf, "\x1b[<0;%d;%dm", wx + 1, wy + 1); flow_feed(f, buf, (int)strlen(buf));
}
static void rclick_at(flow_t *f, int sx, int sy) {           /* right-click press (context fires on press) */
  char buf[32];
  snprintf(buf, sizeof buf, "\x1b[<2;%d;%dM", sx + 1, sy + 1); flow_feed(f, buf, (int)strlen(buf));
}

int main(void) {
  /* ---- on_selection_change: select / toggle / clear, with the changed-only gate ---- */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    int a = flow_add_node(f, "default", (flow_pt){10, 5}, (void*)"A");
    int b = flow_add_node(f, "default", (flow_pt){30, 5}, (void*)"B");
    ev_log L = {0};
    flow_callbacks cb = {0}; cb.on_selection_change = on_sel; cb.user = &L; flow_set_callbacks(f, cb);

    flow_select_node(f, a, 0);                       /* {} -> {a} */
    ASSERT_INT(L.sel_fires, 1, "select_node(a) fires on_selection_change once");
    ASSERT_INT(L.sel_n, 1, "  ids = {a}: count 1"); ASSERT_INT(L.sel_ids[0], a, "  ids[0]==a");

    flow_toggle_node(f, b);                          /* {a} -> {a,b} */
    ASSERT_INT(L.sel_fires, 2, "toggle(b) fires");
    ASSERT_INT(L.sel_n, 2, "  ids = {a,b}: count 2");
    ASSERT_INT(L.sel_ids[0], a, "  ids[0]==a (insertion order)"); ASSERT_INT(L.sel_ids[1], b, "  ids[1]==b");

    flow_toggle_node(f, a);                          /* {a,b} -> {b} */
    ASSERT_INT(L.sel_fires, 3, "toggle(a) off fires");
    ASSERT_INT(L.sel_n, 1, "  ids = {b}"); ASSERT_INT(L.sel_ids[0], b, "  ids[0]==b");

    flow_select_node(f, b, 0);                       /* {b} -> {b}: NO net change */
    ASSERT_INT(L.sel_fires, 3, "no-op select(b) when b already sole selection does NOT fire (changed-only gate)");

    flow_clear_selection(f);                         /* {b} -> {} */
    ASSERT_INT(L.sel_fires, 4, "clear_selection fires");
    ASSERT_INT(L.sel_n, 0, "  ids = {} (count 0)");

    flow_clear_selection(f);                         /* {} -> {}: no change */
    ASSERT_INT(L.sel_fires, 4, "clear on empty selection does NOT fire");
    flow_free(f);
  }

  /* ---- on_selection_change from a marquee drag (fires on change, not per motion) ---- */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    int a = flow_add_node(f, "default", (flow_pt){10, 5},  (void*)"A");   /* (10,5,5,3) */
    int b = flow_add_node(f, "default", (flow_pt){30, 5},  (void*)"B");   /* (30,5,5,3) */
    int c = flow_add_node(f, "default", (flow_pt){10, 15}, (void*)"C");   /* (10,15,5,3) */
    (void)b;
    ev_log L = {0};
    flow_callbacks cb = {0}; cb.on_selection_change = on_sel; cb.user = &L; flow_set_callbacks(f, cb);
    /* shift-drag a box (8,4)->(16,17) enclosing A and C (PARTIAL default), not B */
    flow_feed(f, "\x1b[<4;9;5M", 9);                  /* press shift @ (8,4) empty */
    flow_feed(f, "\x1b[<36;17;18M", 12);              /* motion to (16,17): set {} -> {a,c} */
    ASSERT_INT(flow_selected_count(f), 2, "marquee encloses A and C");
    ASSERT_INT(L.sel_fires, 1, "marquee fires on_selection_change once for the change");
    ASSERT_INT(L.sel_n, 2, "  ids count 2");
    ASSERT_INT(L.sel_ids[0], a, "  ids[0]==a (insertion order)");
    ASSERT_INT(L.sel_ids[1], c, "  ids[1]==c");
    int before = L.sel_fires;
    flow_feed(f, "\x1b[<36;17;18M", 12);              /* identical motion: same enclosed set */
    ASSERT_INT(L.sel_fires, before, "repeated same-box motion does NOT re-fire (no per-motion flood)");
    flow_feed(f, "\x1b[<4;17;18m", 11);               /* release: selection already applied */
    flow_free(f);
  }

  /* ---- on_nodes_delete: flow_delete_selection fires ONCE with the full set ---- */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    int a = flow_add_node(f, "default", (flow_pt){10, 5}, (void*)"A");
    int b = flow_add_node(f, "default", (flow_pt){30, 5}, (void*)"B");
    ev_log L = {0};
    flow_callbacks cb = {0}; cb.on_nodes_delete = on_del; cb.user = &L; flow_set_callbacks(f, cb);
    flow_select_node(f, a, 0); flow_select_node(f, b, 1);   /* select {a,b} */
    flow_delete_selection(f);
    ASSERT_INT(L.del_fires, 1, "delete_selection fires on_nodes_delete exactly once");
    ASSERT_INT(L.del_n, 2, "  reports both ids");
    ASSERT_INT(L.del_ids[0], a, "  ids[0]==a (insertion order)");
    ASSERT_INT(L.del_ids[1], b, "  ids[1]==b");
    ASSERT_INT(flow_node_count(f), 0, "  both nodes actually removed");
    flow_free(f);
  }

  /* ---- on_nodes_delete: flow_remove_node reports its cascade (parent + child) ---- */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    int p = flow_add_node(f, "default", (flow_pt){10, 5}, (void*)"P");
    int c = flow_add_node(f, "default", (flow_pt){0, 0},  (void*)"C");
    flow_set_parent(f, c, p);
    ev_log L = {0};
    flow_callbacks cb = {0}; cb.on_nodes_delete = on_del; cb.user = &L; flow_set_callbacks(f, cb);
    flow_remove_node(f, p);                            /* cascades the child */
    ASSERT_INT(L.del_fires, 1, "remove_node fires on_nodes_delete once for the cascade");
    ASSERT_INT(L.del_n, 2, "  reports parent + child");
    ASSERT_INT(L.del_ids[0], p, "  ids[0]==p (insertion order)");
    ASSERT_INT(L.del_ids[1], c, "  ids[1]==c (the cascade child)");
    ASSERT_INT(flow_node_count(f), 0, "  both removed");
    flow_free(f);
  }

  /* ---- on_node_dblclick: two clicks on the SAME node; click + dblclick both fire ---- */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    int n = flow_add_node(f, "default", (flow_pt){10, 5}, (void*)"N");   /* center (12,6) */
    ev_log L = {0};
    flow_callbacks cb = {0}; cb.on_node_click = on_click; cb.on_node_dblclick = on_dbl; cb.user = &L;
    flow_set_callbacks(f, cb);

    click_at(f, 12, 6);
    ASSERT_INT(L.click_fires, 1, "click 1: on_node_click fires");
    ASSERT_INT(L.dbl_fires, 0, "click 1: no dblclick yet");

    click_at(f, 12, 6);
    ASSERT_INT(L.click_fires, 2, "click 2: on_node_click fires again (click AND dblclick both fire)");
    ASSERT_INT(L.dbl_fires, 1, "click 2 on same node: on_node_dblclick fires once");
    ASSERT_INT(L.dbl_last, n, "  dblclick reports the node id");

    click_at(f, 12, 6);                                /* 3rd: pair was consumed, so no dbl */
    ASSERT_INT(L.dbl_fires, 1, "click 3: pair consumed -> no new dblclick");
    click_at(f, 12, 6);                                /* 4th pairs with 3rd */
    ASSERT_INT(L.dbl_fires, 2, "click 4: pairs with click 3 -> dblclick fires again");
    flow_free(f);
  }

  /* ---- on_node_dblclick: two clicks on DIFFERENT nodes do NOT fire dblclick ---- */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    flow_add_node(f, "default", (flow_pt){10, 5}, (void*)"N");   /* center (12,6) */
    flow_add_node(f, "default", (flow_pt){30, 5}, (void*)"M");   /* center (32,6) */
    ev_log L = {0};
    flow_callbacks cb = {0}; cb.on_node_click = on_click; cb.on_node_dblclick = on_dbl; cb.user = &L;
    flow_set_callbacks(f, cb);
    click_at(f, 12, 6);   /* node N */
    click_at(f, 32, 6);   /* node M */
    ASSERT_INT(L.click_fires, 2, "two clicks -> two on_node_click");
    ASSERT_INT(L.dbl_fires, 0, "clicks on different nodes do NOT fire dblclick");
    /* an intervening pane click also breaks a pair: N, empty, N is not a dblclick */
    L.dbl_fires = 0; L.click_fires = 0;
    click_at(f, 12, 6);   /* node N */
    click_at(f, 60, 18);  /* empty pane */
    click_at(f, 12, 6);   /* node N again */
    ASSERT_INT(L.dbl_fires, 0, "pane click between two same-node clicks breaks the dblclick pair");
    flow_free(f);
  }

  /* ---- dblclick state must NOT survive flow_load: a stale last_click_node whose id
          collides with a reloaded node must not fake a double-click on the first click ---- */
  {
    const char *path = "/tmp/flow_events_reset.json";
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    int n = flow_add_node(f, "default", (flow_pt){10, 5}, (void*)"N");   /* center (12,6) */
    ev_log L = {0};
    flow_callbacks cb = {0}; cb.on_node_dblclick = on_dbl; cb.user = &L; flow_set_callbacks(f, cb);
    click_at(f, 12, 6);                                /* one click: last_click_node = n */
    ASSERT_INT(L.dbl_fires, 0, "single click before load: no dblclick");
    ASSERT_INT(flow_save(f, path), 0, "save ok");
    ASSERT_INT(flow_load(f, path), 0, "load ok (graph reset; node id reused)");
    ASSERT(flow_get_node(f, n) != NULL, "reloaded node has the same id as the pre-load click target");
    click_at(f, 12, 6);                                /* FIRST click after load on that id */
    ASSERT_INT(L.dbl_fires, 0, "first click after flow_load does NOT fire a spurious dblclick (last_click_node reset)");
    flow_free(f);
  }

  /* ---- edge observers: on_edge_click / on_edge_dblclick pair semantics ---- */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    int a = flow_add_node(f, "default", (flow_pt){10, 5},  (void*)"A");
    int b = flow_add_node(f, "default", (flow_pt){30, 5},  (void*)"B");
    int c = flow_add_node(f, "default", (flow_pt){10, 15}, (void*)"C");
    int d = flow_add_node(f, "default", (flow_pt){30, 15}, (void*)"D");
    int e1 = flow_add_edge(f, a, b, "out", "in");
    int e2 = flow_add_edge(f, c, d, "out", "in");
    /* click MID-PATH cells: an edge's ENDPOINT cell is the reconnect-grab affordance
       (press arms a reconnect drag; its no-move release selects silently), so body
       clicks — and the events — only happen away from the endpoints. */
    flow_pt s1, t1, s2, t2;
    flow_edge_endpoint_screen(f, flow_get_edge(f, e1), 0, &s1);
    flow_edge_endpoint_screen(f, flow_get_edge(f, e1), 1, &t1);
    flow_edge_endpoint_screen(f, flow_get_edge(f, e2), 0, &s2);
    flow_edge_endpoint_screen(f, flow_get_edge(f, e2), 1, &t2);
    flow_pt p1 = { (s1.x + t1.x) / 2, (s1.y + t1.y) / 2 };
    flow_pt p2 = { (s2.x + t2.x) / 2, (s2.y + t2.y) / 2 };
    ASSERT_INT(flow_hit_edge(f, p1, 0), e1, "precondition: mid cell sits on e1's path");
    ASSERT_INT(flow_hit_edge(f, p2, 0), e2, "precondition: mid cell sits on e2's path");
    ev_log L = {0};
    flow_callbacks cb = {0};
    cb.on_edge_click = on_eclick; cb.on_edge_dblclick = on_edbl;
    cb.on_node_click = on_click;  cb.on_node_dblclick = on_dbl; cb.user = &L;
    flow_set_callbacks(f, cb);

    click_at(f, p1.x, p1.y);                          /* click 1 on e1 */
    ASSERT_INT(L.eclick_fires, 1, "edge click 1: on_edge_click fires");
    ASSERT_INT(L.eclick_last, e1, "  reports e1");
    ASSERT_INT(L.edbl_fires, 0, "edge click 1: no dblclick yet");
    ASSERT_INT(flow_selected_edge(f), e1, "  edge selected");
    ASSERT_INT(flow_selected_node(f), -1, "  no node selected (mutual exclusivity)");

    click_at(f, p1.x, p1.y);                          /* click 2: pair */
    ASSERT_INT(L.eclick_fires, 2, "edge click 2: on_edge_click fires again (both fire)");
    ASSERT_INT(L.edbl_fires, 1, "edge click 2: on_edge_dblclick fires once");
    ASSERT_INT(L.edbl_last, e1, "  dblclick reports e1");

    click_at(f, p1.x, p1.y);                          /* click 3: pair consumed */
    ASSERT_INT(L.edbl_fires, 1, "edge click 3: pair consumed -> no new dblclick");
    click_at(f, p1.x, p1.y);                          /* click 4 pairs with 3 */
    ASSERT_INT(L.edbl_fires, 2, "edge click 4: pairs with click 3");

    /* pair break: different edge */
    click_at(f, p1.x, p1.y); click_at(f, p2.x, p2.y);
    ASSERT_INT(L.edbl_fires, 2, "click e1 then e2: no dblclick (pair broken by different edge)");
    /* pair break: pane click between */
    click_at(f, p1.x, p1.y); click_at(f, 70, 22); click_at(f, p1.x, p1.y);
    ASSERT_INT(L.edbl_fires, 2, "pane click between e1 clicks breaks the pair");
    /* pair break: node click between; AND node pair broken by an edge click.
       (pane click first: the block above ends with the e1 pair armed) */
    click_at(f, 70, 22);
    click_at(f, p1.x, p1.y); click_at(f, 12, 6); click_at(f, p1.x, p1.y);
    ASSERT_INT(L.edbl_fires, 2, "node click between e1 clicks breaks the edge pair");
    L.dbl_fires = 0;
    click_at(f, 12, 6); click_at(f, p1.x, p1.y); click_at(f, 12, 6);
    ASSERT_INT(L.dbl_fires, 0, "edge click between node clicks breaks the NODE pair");
    flow_free(f);
  }

  /* ---- on_edge_context: right-click on the routed path; node takes precedence ---- */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    int a = flow_add_node(f, "default", (flow_pt){10, 5}, (void*)"A");
    int b = flow_add_node(f, "default", (flow_pt){30, 5}, (void*)"B");
    int e = flow_add_edge(f, a, b, "out", "in");
    flow_pt tp; flow_edge_endpoint_screen(f, flow_get_edge(f, e), 1, &tp);
    ev_log L = {0};
    flow_callbacks cb = {0}; cb.on_edge_context = on_ectx; cb.on_node_context = on_nctx; cb.user = &L;
    flow_set_callbacks(f, cb);
    rclick_at(f, tp.x, tp.y);                         /* on the path, off the bodies */
    ASSERT_INT(L.ectx_fires, 1, "right-click on the edge path fires on_edge_context");
    ASSERT_INT(L.ectx_last, e, "  reports the edge id");
    ASSERT_INT(L.ectx_screen.x, tp.x, "  screen x delivered");
    ASSERT_INT(L.ectx_screen.y, tp.y, "  screen y delivered");
    ASSERT_INT(L.nctx_fires, 0, "  no node context fired");
    /* tp.x+1 is ON B's left border: node hit AND within edge tol -> node wins */
    rclick_at(f, tp.x + 1, tp.y);
    ASSERT_INT(L.nctx_fires, 1, "right-click where node and edge overlap: node context wins");
    ASSERT_INT(L.nctx_last, b, "  reports node B");
    ASSERT_INT(L.ectx_fires, 1, "  edge context did NOT fire (precedence)");
    /* far from everything: neither fires */
    rclick_at(f, 70, 22);
    ASSERT_INT(L.ectx_fires + L.nctx_fires, 2, "right-click on empty pane fires nothing");
    flow_free(f);
  }

  /* ---- edge dblclick state must NOT survive flow_load (mirror of the node test) ---- */
  {
    const char *path = "/tmp/flow_events_edge_reset.json";
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    int a = flow_add_node(f, "default", (flow_pt){10, 5}, (void*)"A");
    int b = flow_add_node(f, "default", (flow_pt){30, 5}, (void*)"B");
    int e = flow_add_edge(f, a, b, "out", "in");
    flow_pt sp, tp;                                   /* mid-path cell (endpoints arm reconnect) */
    flow_edge_endpoint_screen(f, flow_get_edge(f, e), 0, &sp);
    flow_edge_endpoint_screen(f, flow_get_edge(f, e), 1, &tp);
    flow_pt mid = { (sp.x + tp.x) / 2, (sp.y + tp.y) / 2 };
    ev_log L = {0};
    flow_callbacks cb = {0}; cb.on_edge_dblclick = on_edbl; cb.on_edge_click = on_eclick; cb.user = &L;
    flow_set_callbacks(f, cb);
    click_at(f, mid.x, mid.y);                        /* one click: last_click_edge = e */
    ASSERT_INT(L.eclick_fires, 1, "single edge click before load fires on_edge_click");
    ASSERT_INT(L.edbl_fires, 0, "single edge click before load: no dblclick");
    ASSERT_INT(flow_save(f, path), 0, "save ok");
    ASSERT_INT(flow_load(f, path), 0, "load ok (graph reset; edge id reused)");
    ASSERT(flow_get_edge(f, e) != NULL, "reloaded edge has the same id");
    click_at(f, mid.x, mid.y);                        /* FIRST click after load on that id */
    ASSERT_INT(L.eclick_fires, 2, "first click after load still fires on_edge_click");
    ASSERT_INT(L.edbl_fires, 0, "first edge click after flow_load does NOT fire a spurious dblclick");
    flow_free(f);
  }

  /* ---- NULL-callback safety: every instrumented path runs with all callbacks NULL ---- */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    int a = flow_add_node(f, "default", (flow_pt){10, 5}, (void*)"A");
    int b = flow_add_node(f, "default", (flow_pt){30, 5}, (void*)"B");
    flow_callbacks cb = {0}; flow_set_callbacks(f, cb);   /* all NULL */
    int e = flow_add_edge(f, a, b, "out", "in");
    flow_pt tp; flow_edge_endpoint_screen(f, flow_get_edge(f, e), 1, &tp);
    flow_select_node(f, a, 0); flow_toggle_node(f, b); flow_clear_selection(f);
    flow_feed(f, "\x1b[<4;9;5M", 9); flow_feed(f, "\x1b[<36;17;18M", 12); flow_feed(f, "\x1b[<4;17;18m", 11);
    click_at(f, 12, 6); click_at(f, 12, 6);              /* would-be node dblclick */
    click_at(f, tp.x, tp.y); click_at(f, tp.x, tp.y);    /* would-be edge dblclick */
    rclick_at(f, tp.x, tp.y); rclick_at(f, 12, 6);       /* would-be edge/node context */
    flow_select_node(f, a, 0); flow_select_node(f, b, 1);
    flow_delete_selection(f);
    ASSERT(1, "NULL callbacks: no crash across select/toggle/clear/marquee/dblclick/edge-events/delete");
    flow_free(f);
  }

  return flowtest_report("test_events");
}
