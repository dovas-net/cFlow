#define FLOW_IMPLEMENTATION
#include "../flow.h"
#include "flowtest.h"

static int pane_clicks = 0;
static void on_pane(flow_t *f, flow_pt w, void *u) { (void)f;(void)w;(void)u; pane_clicks++; }

/* connection validator tracking (inc-4 #9) */
static int val_calls = 0, val_ret = 1, val_src = -2, val_tgt = -2;
static char val_sh[16], val_th[16];
static void *val_user_seen = NULL;
static int validator(flow_t *f, int src, int tgt, const char *sh, const char *th, void *u) {
  (void)f; val_calls++; val_src = src; val_tgt = tgt; val_user_seen = u;
  snprintf(val_sh, sizeof val_sh, "%s", sh); snprintf(val_th, sizeof val_th, "%s", th);
  return val_ret;
}
/* lifecycle counters for the validator x connect-lifecycle composition */
static int vc_conn_fires = 0, vc_end_fires = 0, vc_end_eid = -2, vc_end_tgt = -2;
static void vc_on_conn(flow_t *f, int s, int t, void *u) { (void)f;(void)s;(void)t;(void)u; vc_conn_fires++; }
static void vc_on_cend(flow_t *f, int eid, int src, int tgt, void *u) { (void)f;(void)src;(void)u; vc_end_fires++; vc_end_eid = eid; vc_end_tgt = tgt; }

int main(void) {
  /* ---- flow_hit_edge: on-path hit, off-path miss ---- */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    int a = flow_add_node(f, "default", (flow_pt){10, 5}, (void*)"A");  /* w=5,h=3 */
    int b = flow_add_node(f, "default", (flow_pt){30, 5}, (void*)"B");
    int e = flow_add_edge(f, a, b, "out", "in");
    ASSERT(e != -1, "edge A->B created");
    /* target endpoint cell (arrowhead/route-start) is always on the routed path */
    flow_pt tp; ASSERT_INT(flow_edge_endpoint_screen(f, flow_get_edge(f, e), 1, &tp), 1, "target endpoint resolved");
    ASSERT_INT(flow_hit_edge(f, tp, 1), e, "hit on target endpoint cell returns edge id");
    flow_pt sp; ASSERT_INT(flow_edge_endpoint_screen(f, flow_get_edge(f, e), 0, &sp), 1, "source endpoint resolved");
    ASSERT_INT(flow_hit_edge(f, sp, 1), e, "hit on source endpoint cell returns edge id");
    /* far point: no edge within tol */
    ASSERT_INT(flow_hit_edge(f, (flow_pt){70, 22}, 1), -1, "far point hits no edge");
    flow_free(f);
  }

  /* ---- flow_select_edge / mutual exclusivity / clear ---- */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    int a = flow_add_node(f, "default", (flow_pt){10, 5}, (void*)"A");
    int b = flow_add_node(f, "default", (flow_pt){30, 5}, (void*)"B");
    int e = flow_add_edge(f, a, b, "out", "in");
    flow_select_edge(f, e, 0);
    ASSERT_INT(flow_selected_edge(f), e, "edge selected");
    ASSERT(flow_get_edge(f, e)->flags & FLOW_SELECTED, "edge has selected flag");
    /* selecting a node (non-additive) clears the edge selection */
    flow_select_node(f, a, 0);
    ASSERT_INT(flow_selected_edge(f), -1, "selecting a node clears edge selection");
    ASSERT_INT(flow_selected_node(f), a, "node A selected");
    /* selecting an edge (non-additive) clears node selection */
    flow_select_edge(f, e, 0);
    ASSERT_INT(flow_selected_node(f), -1, "selecting an edge clears node selection");
    ASSERT_INT(flow_selected_edge(f), e, "edge selected again");
    /* clear_selection clears both */
    flow_select_node(f, a, 1);   /* additive: now both node and edge flagged */
    ASSERT_INT(flow_selected_node(f), a, "additive node-select keeps edge");
    ASSERT_INT(flow_selected_edge(f), e, "additive node-select keeps edge selected");
    flow_clear_selection(f);
    ASSERT_INT(flow_selected_node(f), -1, "clear_selection clears nodes");
    ASSERT_INT(flow_selected_edge(f), -1, "clear_selection clears edges");
    flow_free(f);
  }

  /* ---- flow_set_edge_label: set, replace, clear; remove decrements ---- */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    int a = flow_add_node(f, "default", (flow_pt){10, 5}, (void*)"A");
    int b = flow_add_node(f, "default", (flow_pt){30, 5}, (void*)"B");
    int e = flow_add_edge(f, a, b, "out", "in");
    ASSERT(flow_get_edge(f, e)->label == NULL, "new edge has no label");
    flow_set_edge_label(f, e, "L");
    ASSERT_STR(flow_get_edge(f, e)->label, "L", "label set to L");
    flow_set_edge_label(f, e, "LONGER");                 /* frees prior + replaces (ASan: no leak) */
    ASSERT_STR(flow_get_edge(f, e)->label, "LONGER", "label replaced");
    flow_set_edge_label(f, e, NULL);                     /* NULL clears */
    ASSERT(flow_get_edge(f, e)->label == NULL, "NULL label clears");
    flow_set_edge_label(f, e, "X");                      /* set again, then remove must free it */
    int before = flow_edge_count(f);
    flow_remove_edge(f, e);
    ASSERT(flow_get_edge(f, e) == NULL, "edge removed");
    ASSERT_INT(flow_edge_count(f), before - 1, "edge_count decremented");
    flow_free(f);
  }

  /* ---- flow_reconnect_edge: repoint, reject self + duplicate ---- */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    int a = flow_add_node(f, "default", (flow_pt){10, 5}, (void*)"A");
    int b = flow_add_node(f, "default", (flow_pt){30, 5}, (void*)"B");
    int c = flow_add_node(f, "default", (flow_pt){50, 5}, (void*)"C");
    int e = flow_add_edge(f, a, b, "out", "in");
    /* repoint target B -> C (keep target handle "in" so the later dup-check is genuine) */
    flow_reconnect_edge(f, e, c, "in", 1);
    ASSERT_INT(flow_get_edge(f, e)->target, c, "target repointed to C");
    ASSERT_INT(flow_get_edge(f, e)->source, a, "source unchanged after target reconnect");
    /* self-edge rejected: reconnect SOURCE to C while target is already C => C->C self, reject */
    flow_reconnect_edge(f, e, c, "out", 0);  /* source -> C, target already C => self, reject */
    ASSERT_INT(flow_get_edge(f, e)->source, a, "self-edge reconnect rejected (source unchanged)");
    ASSERT_INT(flow_get_edge(f, e)->target, c, "self-edge reconnect rejected (target unchanged)");
    /* duplicate rejected: e is A->C(out,in); reconnecting e2's target to C(in) duplicates it */
    int e2 = flow_add_edge(f, a, b, "out", "in");   /* A->B(out,in), distinct from A->C(out,in) */
    ASSERT(e2 != -1, "second edge A->B created");
    flow_reconnect_edge(f, e2, c, "in", 1);          /* would make A->C(out,in) == dup of e */
    ASSERT_INT(flow_get_edge(f, e2)->target, b, "duplicate reconnect rejected (target unchanged)");
    /* endpoint screen coords match the renderer: source endpoint = RIGHT-of-A nudged out */
    flow_edge *eg = flow_get_edge(f, e);
    flow_pt ss; ASSERT_INT(flow_edge_endpoint_screen(f, eg, 0, &ss), 1, "source endpoint resolved");
    /* A rect (10,5,5,3): RIGHT anchor (14,6), nudged out RIGHT => (15,6) */
    ASSERT_INT(ss.x, 15, "source endpoint x = RIGHT-of-A nudged out");
    ASSERT_INT(ss.y, 6,  "source endpoint y = A handle row");
    flow_free(f);
  }

  /* ---- click an edge body selects it (synthetic SGR) ---- */
  {
    pane_clicks = 0;
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    int a = flow_add_node(f, "default", (flow_pt){10, 5}, (void*)"A");
    int b = flow_add_node(f, "default", (flow_pt){30, 5}, (void*)"B");
    int e = flow_add_edge(f, a, b, "out", "in");
    flow_callbacks cb = {0}; cb.on_pane_click = on_pane; flow_set_callbacks(f, cb);
    /* find a routed mid cell via the target endpoint (on path, away from node bodies) */
    flow_pt tp; flow_edge_endpoint_screen(f, flow_get_edge(f, e), 1, &tp);
    /* tp is the route-start at target side (one cell outside B's LEFT). Click it. */
    char seq[32]; int n = snprintf(seq, sizeof seq, "\x1b[<0;%d;%dM", tp.x + 1, tp.y + 1);
    flow_feed(f, seq, n);
    n = snprintf(seq, sizeof seq, "\x1b[<0;%d;%dm", tp.x + 1, tp.y + 1);
    flow_feed(f, seq, n);
    ASSERT_INT(flow_selected_edge(f), e, "edge-body click selects the edge");
    ASSERT_INT(flow_selected_node(f), -1, "edge-body click leaves no node selected");
    ASSERT_INT(pane_clicks, 0, "edge-body click does not fire on_pane_click");
    flow_free(f);
  }

  /* ---- reconnect drag (synthetic SGR): press endpoint, drag to C, release ---- */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    int a = flow_add_node(f, "default", (flow_pt){10, 5}, (void*)"A");  /* RIGHT@(14,6) out@(15,6) */
    int b = flow_add_node(f, "default", (flow_pt){30, 5}, (void*)"B");  /* LEFT@(30,6) in@(29,6)  */
    int c = flow_add_node(f, "default", (flow_pt){30, 12}, (void*)"C"); /* body around (30..34,12..14) */
    int e = flow_add_edge(f, a, b, "out", "in");
    /* target endpoint screen cell */
    flow_pt tp; flow_edge_endpoint_screen(f, flow_get_edge(f, e), 1, &tp);
    char seq[32]; int n;
    n = snprintf(seq, sizeof seq, "\x1b[<0;%d;%dM", tp.x + 1, tp.y + 1); flow_feed(f, seq, n);  /* press endpoint */
    /* drag into C body interior (31,13) => SGR (32,14) */
    flow_feed(f, "\x1b[<32;32;14M", 12);
    /* release over C */
    flow_feed(f, "\x1b[<0;32;14m", 11);
    ASSERT_INT(flow_get_edge(f, e)->target, c, "reconnect drag repoints target to C");
    ASSERT_INT(flow_get_edge(f, e)->source, a, "reconnect drag leaves source A");
    flow_free(f);
  }

  /* ---- reconnect drag dropped on empty space leaves edge unchanged ---- */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    int a = flow_add_node(f, "default", (flow_pt){10, 5}, (void*)"A");
    int b = flow_add_node(f, "default", (flow_pt){30, 5}, (void*)"B");
    int e = flow_add_edge(f, a, b, "out", "in");
    flow_pt tp; flow_edge_endpoint_screen(f, flow_get_edge(f, e), 1, &tp);
    char seq[32]; int n;
    n = snprintf(seq, sizeof seq, "\x1b[<0;%d;%dM", tp.x + 1, tp.y + 1); flow_feed(f, seq, n);  /* press endpoint */
    flow_feed(f, "\x1b[<32;70;22M", 12);    /* drag to empty (69,21) */
    flow_feed(f, "\x1b[<0;70;22m", 11);     /* release over empty */
    ASSERT_INT(flow_get_edge(f, e)->target, b, "drop on empty leaves target unchanged");
    flow_free(f);
  }

  /* ---- keyboard integration via flow_feed (keys-commands, NOT new dispatch) ---- */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    int a = flow_add_node(f, "default", (flow_pt){10, 5}, (void*)"A");
    int b = flow_add_node(f, "default", (flow_pt){30, 5}, (void*)"B");
    int e = flow_add_edge(f, a, b, "out", "in");
    /* select the edge, then 'x' deletes it (via existing flow_delete_selection) */
    flow_select_edge(f, e, 0);
    flow_feed(f, "x", 1);
    ASSERT_INT(flow_edge_count(f), 0, "x deletes the selected edge");
    /* re-add an edge, select node A, Delete removes A + cascades the incident edge */
    int e2 = flow_add_edge(f, a, b, "out", "in");
    ASSERT(e2 != -1, "edge re-added");
    flow_select_node(f, a, 0);
    flow_feed(f, "\x1b[3~", 4);
    ASSERT(flow_get_node(f, a) == NULL, "Delete removes selected node A");
    ASSERT_INT(flow_edge_count(f), 0, "Delete cascades the incident edge");
    flow_free(f);
  }

  /* ============== engine-level connection validator (inc-4 #9) ============== */

  /* ---- validator sees the exact connection context; allow path unchanged ---- */
  {
    val_calls = 0; val_ret = 1; val_user_seen = NULL;
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    int a = flow_add_node(f, "default", (flow_pt){10, 5}, (void*)"A");
    int b = flow_add_node(f, "default", (flow_pt){30, 5}, (void*)"B");
    int token = 42;
    flow_set_connection_validator(f, validator, &token);
    int e = flow_add_edge(f, a, b, "out", "in");
    ASSERT(e != -1, "validator returning 1 allows the edge");
    ASSERT_INT(val_calls, 1, "validator called exactly once");
    ASSERT_INT(val_src, a, "  src delivered");
    ASSERT_INT(val_tgt, b, "  tgt delivered");
    ASSERT_STR(val_sh, "out", "  source handle delivered");
    ASSERT_STR(val_th, "in", "  target handle delivered");
    ASSERT(val_user_seen == &token, "  user context delivered");
    flow_free(f);
  }

  /* ---- reject blocks flow_add_edge: -1, graph unchanged, nothing journaled ---- */
  {
    val_calls = 0; val_ret = 0;
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    int a = flow_add_node(f, "default", (flow_pt){10, 5}, (void*)"A");
    int b = flow_add_node(f, "default", (flow_pt){30, 5}, (void*)"B");
    int steps = f->journal.n;
    flow_set_connection_validator(f, validator, NULL);
    ASSERT_INT(flow_add_edge(f, a, b, "out", "in"), -1, "validator reject -> add_edge returns -1");
    ASSERT_INT(val_calls, 1, "  validator was consulted");
    ASSERT_INT(flow_edge_count(f), 0, "  no edge appended");
    ASSERT_INT(f->journal.n, steps, "  nothing journaled on reject");
    flow_free(f);
  }

  /* ---- reject blocks flow_reconnect_edge: edge unchanged, nothing journaled ---- */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    int a = flow_add_node(f, "default", (flow_pt){10, 5},  (void*)"A");
    int b = flow_add_node(f, "default", (flow_pt){30, 5},  (void*)"B");
    int c = flow_add_node(f, "default", (flow_pt){30, 15}, (void*)"C");
    int e = flow_add_edge(f, a, b, "out", "in");          /* added BEFORE the validator */
    int steps = f->journal.n;
    val_calls = 0; val_ret = 0;
    flow_set_connection_validator(f, validator, NULL);
    flow_reconnect_edge(f, e, c, "in", 1);                /* repoint target b -> c: rejected */
    ASSERT_INT(val_calls, 1, "reconnect consulted the validator");
    ASSERT_INT(val_src, a, "  prospective src delivered");
    ASSERT_INT(val_tgt, c, "  PROSPECTIVE target delivered (c, not the current b)");
    ASSERT_INT(flow_get_edge(f, e)->target, b, "  reject: target unchanged");
    ASSERT_INT(f->journal.n, steps, "  nothing journaled on reconnect reject");
    /* allow path: same reconnect succeeds once the validator says yes */
    val_ret = 1;
    flow_reconnect_edge(f, e, c, "in", 1);
    ASSERT_INT(flow_get_edge(f, e)->target, c, "  allow: reconnect repoints");
    flow_free(f);
  }

  /* ---- structural rejects come FIRST: the validator never fires for them ---- */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    int a = flow_add_node(f, "default", (flow_pt){10, 5}, (void*)"A");
    int b = flow_add_node(f, "default", (flow_pt){30, 5}, (void*)"B");
    flow_add_edge(f, a, b, "out", "in");                  /* pre-validator edge for the dup test */
    val_calls = 0; val_ret = 1;
    flow_set_connection_validator(f, validator, NULL);
    ASSERT_INT(flow_add_edge(f, a, a, "out", "in"), -1, "self-edge structurally rejected");
    ASSERT_INT(val_calls, 0, "  validator NOT consulted for self-edge");
    ASSERT_INT(flow_add_edge(f, a, 9999, "out", "in"), -1, "missing node structurally rejected");
    ASSERT_INT(val_calls, 0, "  validator NOT consulted for missing node");
    ASSERT_INT(flow_add_edge(f, a, b, "out", "in"), -1, "duplicate structurally rejected");
    ASSERT_INT(val_calls, 0, "  validator NOT consulted for duplicate");
    flow_free(f);
  }

  /* ---- NULL validator (default and after reset) = allow all ---- */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    int a = flow_add_node(f, "default", (flow_pt){10, 5}, (void*)"A");
    int b = flow_add_node(f, "default", (flow_pt){30, 5}, (void*)"B");
    ASSERT(flow_add_edge(f, a, b, "out", "in") != -1, "default NULL validator allows (regression)");
    val_ret = 0;
    flow_set_connection_validator(f, validator, NULL);
    ASSERT_INT(flow_add_edge(f, a, b, "out2", "in"), -1, "rejecting validator active");
    flow_set_connection_validator(f, NULL, NULL);
    ASSERT(flow_add_edge(f, a, b, "out2", "in") != -1, "set(NULL, NULL) restores allow-all");
    flow_free(f);
  }

  /* ---- composition with connect-lifecycle (#7): a validator-rejected interactive
          connection fires on_connect_end(-1, ...) and never on_connect ---- */
  {
    val_calls = 0; val_ret = 0;
    vc_conn_fires = 0; vc_end_fires = 0; vc_end_eid = -2; vc_end_tgt = -2;
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    int a = flow_add_node(f, "default", (flow_pt){10, 5}, (void*)"A");
    int b = flow_add_node(f, "default", (flow_pt){30, 5}, (void*)"B");
    flow_callbacks cb = {0}; cb.on_connect = vc_on_conn; cb.on_connect_end = vc_on_cend;
    flow_set_callbacks(f, cb);
    flow_set_connection_validator(f, validator, NULL);
    int steps = f->journal.n;
    ASSERT_INT(flow_begin_connection(f, a, "out"), 0, "begin ok (validator gates the ADD, not the grab)");
    ASSERT_INT(flow_end_connection(f, b, "in"), -1, "end: validator rejects the add");
    ASSERT_INT(val_calls, 1, "  validator consulted once");
    ASSERT_INT(flow_edge_count(f), 0, "  no edge");
    ASSERT_INT(f->journal.n, steps, "  rejected interactive connect journals nothing (empty txn opens lazily)");
    ASSERT_INT(vc_conn_fires, 0, "  on_connect did NOT fire");
    ASSERT_INT(vc_end_fires, 1, "  on_connect_end fired once");
    ASSERT_INT(vc_end_eid, -1, "  end eid -1");
    ASSERT_INT(vc_end_tgt, b, "  end target = the attempted node");
    flow_free(f);
  }

  return flowtest_report("test_edge");
}
