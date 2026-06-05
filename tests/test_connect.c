#define FLOW_IMPLEMENTATION
#include "../flow.h"
#include "flowtest.h"

/* on_connect tracking */
static int conn_src = -2, conn_dst = -2, conn_fires = 0;
static int pane_clicks = 0;
static void on_pane(flow_t *f, flow_pt w, void *u) { (void)f;(void)w;(void)u; pane_clicks++; }

/* connect lifecycle tracking (inc-4 #7): sequence-stamped to assert ordering */
static int ev_seq = 0;
static int start_fires = 0, start_src = -2, start_seq = 0;
static char start_handle[16];
static int end_fires = 0, end_eid = -2, end_src = -2, end_tgt = -2, end_seq = 0, end_txn_depth = -1;
static int conn_seq = 0;
static int eclick_fires = 0, eclick_seq = 0;
static int nclick_fires = 0;
static int ectx_fires = 0, ectx_seq = 0;
static void on_nclick(flow_t *f, int node, void *u) { (void)f;(void)node;(void)u; nclick_fires++; }
static void on_ectx(flow_t *f, int edge, flow_pt s, void *u) { (void)f;(void)edge;(void)s;(void)u; ectx_fires++; ectx_seq = ++ev_seq; }
static void on_conn(flow_t *f, int s, int t, void *u) { (void)f;(void)u; conn_src = s; conn_dst = t; conn_fires++; conn_seq = ++ev_seq; }
static void on_cstart(flow_t *f, int src, const char *handle, void *u) {
  (void)f;(void)u; start_fires++; start_src = src; start_seq = ++ev_seq;
  snprintf(start_handle, sizeof start_handle, "%s", handle ? handle : "(null)");
}
static void on_cend(flow_t *f, int eid, int src, int tgt, void *u) {
  (void)u; end_fires++; end_eid = eid; end_src = src; end_tgt = tgt; end_seq = ++ev_seq;
  end_txn_depth = f->journal.txn_depth;          /* 0 = fired AFTER the undo txn settled */
}
static void on_eclick(flow_t *f, int edge, void *u) { (void)f;(void)edge;(void)u; eclick_fires++; eclick_seq = ++ev_seq; }
static void lc_reset(void) {
  ev_seq = 0; start_fires = 0; start_src = -2; start_seq = 0; start_handle[0] = 0;
  end_fires = 0; end_eid = -2; end_src = -2; end_tgt = -2; end_seq = 0; end_txn_depth = -1;
  conn_fires = 0; conn_src = conn_dst = -2; conn_seq = 0;
  eclick_fires = 0; eclick_seq = 0; nclick_fires = 0; ectx_fires = 0; ectx_seq = 0;
}

int main(void) {
  /* ---- flow_handle_anchor math ---- */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    /* a node sized exactly 5x3 at world (10,5): label "abc" => w=3+4=7, so use a 5x3 by raw set */
    int id = flow_add_node(f, "default", (flow_pt){10, 5}, (void*)"hi");
    flow_node *n = flow_get_node(f, id); n->w = 5; n->h = 3;   /* pin 5x3 for the anchor math */
    flow_handle L = { "in",  FLOW_HANDLE_TARGET, FLOW_LEFT,   0 };
    flow_handle R = { "out", FLOW_HANDLE_SOURCE, FLOW_RIGHT,  0 };
    flow_handle T = { "t",   FLOW_HANDLE_SOURCE, FLOW_TOP,    0 };
    flow_handle B = { "b",   FLOW_HANDLE_SOURCE, FLOW_BOTTOM, 0 };
    flow_pt l = flow_handle_anchor(f, n, &L), r = flow_handle_anchor(f, n, &R);
    flow_pt t = flow_handle_anchor(f, n, &T), b = flow_handle_anchor(f, n, &B);
    ASSERT_INT(l.x, 10, "LEFT anchor x");   ASSERT_INT(l.y, 6, "LEFT anchor y");
    ASSERT_INT(r.x, 14, "RIGHT anchor x");  ASSERT_INT(r.y, 6, "RIGHT anchor y");   /* x+w-1 = 14 */
    ASSERT_INT(t.x, 12, "TOP anchor x");    ASSERT_INT(t.y, 5, "TOP anchor y");     /* x+w/2 = 12 */
    ASSERT_INT(b.x, 12, "BOTTOM anchor x"); ASSERT_INT(b.y, 7, "BOTTOM anchor y");  /* y+h-1 = 7 */
    /* 'along' shifts the anchor along the side */
    flow_handle La = { "in", FLOW_HANDLE_TARGET, FLOW_LEFT, 1 };   /* shift down 1 */
    flow_pt la = flow_handle_anchor(f, n, &La);
    ASSERT_INT(la.y, 7, "LEFT 'along'=1 shifts y by 1");
    flow_handle Ta = { "t", FLOW_HANDLE_SOURCE, FLOW_TOP, 2 };     /* shift right 2 */
    flow_pt ta = flow_handle_anchor(f, n, &Ta);
    ASSERT_INT(ta.x, 14, "TOP 'along'=2 shifts x by 2");
    flow_free(f);
  }

  /* ---- flow_default_handles wired into the default node type ---- */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    int id = flow_add_node(f, "default", (flow_pt){0, 0}, (void*)"n");
    ASSERT_INT(flow_node_handle_count(f, id), 2, "default node has 2 handles");
    const flow_handle *h0 = flow_node_handle_at(f, id, 0);
    const flow_handle *h1 = flow_node_handle_at(f, id, 1);
    ASSERT(h0 && h1, "both handles fetchable");
    ASSERT_STR(h0->id, "in", "handle 0 id 'in'");
    ASSERT_INT(h0->kind, FLOW_HANDLE_TARGET, "handle 0 is a target");
    ASSERT_INT(h0->pos, FLOW_LEFT, "handle 0 on LEFT");
    ASSERT_STR(h1->id, "out", "handle 1 id 'out'");
    ASSERT_INT(h1->kind, FLOW_HANDLE_SOURCE, "handle 1 is a source");
    ASSERT_INT(h1->pos, FLOW_RIGHT, "handle 1 on RIGHT");
    ASSERT(flow_node_handle_at(f, id, 2) == NULL, "out-of-range handle is NULL");
    flow_free(f);
  }

  /* ---- flow_hit_handle / flow_set_hover / flow_hovered_node ---- */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    int a = flow_add_node(f, "default", (flow_pt){10, 5}, (void*)"A");  /* w=5,h=3 */
    /* RIGHT anchor of A in screen coords (screen==world, zoom 1, no pan) = (14,6) */
    flow_pt rscr = { 14, 6 };
    /* not hovered/selected: handle is not hittable */
    int nn = -2; ASSERT_INT(flow_hit_handle(f, rscr, &nn), -1, "no handle hit when not hovered");
    ASSERT_INT(nn, -1, "out_node -1 when not hovered");
    /* hover A: now the RIGHT handle is hittable */
    flow_set_hover(f, a);
    ASSERT_INT(flow_hovered_node(f), a, "A is the hovered node");
    int hn = -2; int hi = flow_hit_handle(f, rscr, &hn);
    ASSERT(hi >= 0, "handle hit when hovered");
    ASSERT_INT(hn, a, "hit returns node A");
    const flow_handle *hh = flow_node_handle_at(f, a, hi);
    ASSERT_STR(hh->id, "out", "hit RIGHT anchor returns the 'out' handle");
    /* empty cell: no hit */
    int en = -2; ASSERT_INT(flow_hit_handle(f, (flow_pt){50, 20}, &en), -1, "empty cell no handle hit");
    /* clearing hover removes hittability */
    flow_set_hover(f, -1);
    ASSERT_INT(flow_hovered_node(f), -1, "hover cleared");
    ASSERT_INT(flow_hit_handle(f, rscr, NULL), -1, "handle not hittable after hover clear");
    flow_free(f);
  }

  /* ---- drag-connect happy path (synthetic SGR) ---- */
  {
    conn_src = conn_dst = -2; conn_fires = 0;
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    int a = flow_add_node(f, "default", (flow_pt){10, 5}, (void*)"A");  /* w=5,h=3; RIGHT@(14,6) */
    int b = flow_add_node(f, "default", (flow_pt){30, 5}, (void*)"B");  /* w=5,h=3; LEFT@(30,6)  */
    flow_callbacks cb = {0}; cb.on_connect = on_conn; flow_set_callbacks(f, cb);
    flow_set_hover(f, a);                                  /* reveal A's handles */
    /* press A's RIGHT handle: screen (14,6) => SGR 1-based (15,7) */
    flow_feed(f, "\x1b[<0;15;7M", 10);
    ASSERT_INT(flow_connecting(f), 1, "connecting after press on source handle");
    /* motion toward B: screen (20,6) => SGR (21,7) */
    flow_feed(f, "\x1b[<32;21;7M", 11);
    ASSERT_INT(flow_connecting(f), 1, "still connecting during drag");
    /* motion onto B body reveals B's handles */
    flow_feed(f, "\x1b[<32;31;7M", 11);                   /* screen (30,6) = B LEFT anchor */
    ASSERT(flow_get_node(f, b)->flags & FLOW_HOVERED, "B becomes hovered during drag");
    /* release on B's LEFT handle: screen (30,6) => SGR (31,7) */
    flow_feed(f, "\x1b[<0;31;7m", 10);
    ASSERT_INT(flow_connecting(f), 0, "not connecting after release");
    ASSERT_INT(flow_edge_count(f), 1, "one edge created");
    flow_edge *e = &flow_edges(f)[0];
    ASSERT_INT(e->source, a, "edge source == A");
    ASSERT_INT(e->target, b, "edge target == B");
    ASSERT_STR(e->source_handle, "out", "edge source_handle 'out'");
    ASSERT_STR(e->target_handle, "in",  "edge target_handle 'in'");
    ASSERT_INT(conn_fires, 1, "on_connect fired once");
    ASSERT_INT(conn_src, a, "on_connect source A");
    ASSERT_INT(conn_dst, b, "on_connect target B");
    flow_free(f);
  }

  /* ---- connectOnClick path ---- */
  {
    pane_clicks = 0;
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    int a = flow_add_node(f, "default", (flow_pt){10, 5}, (void*)"A");
    int b = flow_add_node(f, "default", (flow_pt){30, 5}, (void*)"B");
    flow_callbacks cb = {0}; cb.on_pane_click = on_pane; flow_set_callbacks(f, cb);
    flow_select_node(f, b, 0);                             /* pre-select B; completion must NOT clear it */
    flow_set_hover(f, a);
    /* click A's RIGHT handle (press+release same cell): begins connection, stays armed */
    flow_feed(f, "\x1b[<0;15;7M", 10); flow_feed(f, "\x1b[<0;15;7m", 10);
    ASSERT_INT(flow_connecting(f), 1, "connectOnClick armed after first click");
    /* second click on B's LEFT handle completes (B must be hittable: hover it) */
    flow_set_hover(f, b);
    flow_feed(f, "\x1b[<0;31;7M", 10); flow_feed(f, "\x1b[<0;31;7m", 10);
    ASSERT_INT(flow_connecting(f), 0, "connectOnClick completed -> not connecting");
    ASSERT_INT(flow_edge_count(f), 1, "connectOnClick made one edge A->B");
    ASSERT_INT(flow_edges(f)[0].source, a, "click-edge source A");
    ASSERT_INT(flow_edges(f)[0].target, b, "click-edge target B");
    /* completing click must NOT fire on_pane_click or clear selection */
    ASSERT_INT(pane_clicks, 0, "connectOnClick completion fires no on_pane_click");
    ASSERT(flow_get_node(f, b)->flags & FLOW_SELECTED, "selection survives connectOnClick completion");
    flow_free(f);
  }

  /* ---- connectOnClick: click empty pane mid-connection cancels AND, as of
          inc-5 #11 (cancel fall-through), the same click reports the pane —
          on_connect_end fires inside the cancel, then on_pane_click on release.
          (Inverted from the inc-4 contract, which consumed the press.) ---- */
  {
    pane_clicks = 0;
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    int a = flow_add_node(f, "default", (flow_pt){10, 5}, (void*)"A");
    flow_callbacks cb = {0}; cb.on_pane_click = on_pane; flow_set_callbacks(f, cb);
    flow_set_hover(f, a);
    flow_feed(f, "\x1b[<0;15;7M", 10); flow_feed(f, "\x1b[<0;15;7m", 10);  /* arm from A RIGHT */
    ASSERT_INT(flow_connecting(f), 1, "armed before empty click");
    /* click empty pane far away */
    flow_feed(f, "\x1b[<0;60;20M", 11); flow_feed(f, "\x1b[<0;60;20m", 11);
    ASSERT_INT(flow_connecting(f), 0, "empty-pane click cancels connection");
    ASSERT_INT(flow_edge_count(f), 0, "no edge created on cancel");
    ASSERT_INT(pane_clicks, 1, "the cancelling click ALSO reports the pane (inc-5 #11 fall-through)");
    flow_free(f);
  }

  /* ---- Esc cancels in-flight connection ---- */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    int a = flow_add_node(f, "default", (flow_pt){10, 5}, (void*)"A");
    flow_set_hover(f, a);
    flow_feed(f, "\x1b[<0;15;7M", 10); flow_feed(f, "\x1b[<0;15;7m", 10);
    ASSERT_INT(flow_connecting(f), 1, "armed before Esc");
    flow_feed(f, "\x1b", 1);                              /* lone ESC */
    ASSERT_INT(flow_connecting(f), 0, "Esc cancels connection");
    ASSERT_INT(flow_edge_count(f), 0, "no edge after Esc cancel");
    flow_free(f);
  }

  /* ---- validation reuse: self + duplicate rejected ---- */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    int a = flow_add_node(f, "default", (flow_pt){10, 5}, (void*)"A");
    int b = flow_add_node(f, "default", (flow_pt){30, 5}, (void*)"B");
    /* self-edge via the connection API: begin from A, end on A */
    ASSERT_INT(flow_begin_connection(f, a, "out"), 0, "begin from A:out ok");
    ASSERT_INT(flow_end_connection(f, a, "in"), -1, "self-connection rejected");
    ASSERT_INT(flow_edge_count(f), 0, "no self edge");
    ASSERT_INT(flow_connecting(f), 0, "rejected self leaves not-connecting");
    /* make a valid A->B, then a duplicate A->B (same handles) is rejected */
    flow_begin_connection(f, a, "out");
    int e1 = flow_end_connection(f, b, "in");
    ASSERT(e1 != -1, "first A->B accepted");
    ASSERT_INT(flow_edge_count(f), 1, "one edge after first connect");
    flow_begin_connection(f, a, "out");
    ASSERT_INT(flow_end_connection(f, b, "in"), -1, "duplicate A->B(out,in) rejected");
    ASSERT_INT(flow_edge_count(f), 1, "edge_count unchanged after dup reject");
    /* begin_connection from a target-only handle is rejected */
    ASSERT_INT(flow_begin_connection(f, a, "in"), -1, "begin from a TARGET handle rejected");
    flow_free(f);
  }

  /* ---- regression: node BODY press still drags/selects; empty press still pans ---- */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    int a = flow_add_node(f, "default", (flow_pt){10, 5}, (void*)"A");  /* rect (10,5,5,3) */
    /* press the node BODY interior (11,6) => SGR (12,7); NOT a handle (handles at x=10,14). drag +5 */
    flow_feed(f, "\x1b[<0;12;7M", 10);
    ASSERT_INT(flow_connecting(f), 0, "body press does not start a connection");
    flow_feed(f, "\x1b[<32;17;7M", 11);                  /* drag to (16,6): dx=+5 */
    ASSERT_INT(flow_get_node(f, a)->pos.x, 15, "node body drag moves the node +5");
    ASSERT_INT(flow_selected_node(f), a, "drag selects A");
    flow_feed(f, "\x1b[<0;17;7m", 10);                   /* release */
    /* empty-pane press+drag pans, does not connect */
    flow_pt before = flow_to_screen(f, (flow_pt){0, 0});
    flow_feed(f, "\x1b[<0;51;15M", 11);                  /* press empty (50,14) */
    ASSERT_INT(flow_connecting(f), 0, "empty press does not connect");
    flow_feed(f, "\x1b[<32;54;15M", 12);                 /* drag +3 */
    flow_pt after = flow_to_screen(f, (flow_pt){0, 0});
    ASSERT_INT(after.x, before.x + 3, "empty drag still pans +3");
    flow_feed(f, "\x1b[<0;54;15m", 11);
    flow_free(f);
  }

  /* ================= connect lifecycle events (inc-4 #7) ================= */

  /* ---- SUCCESS: start (press) -> on_connect (in txn) -> end (after txn) ---- */
  {
    lc_reset();
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    int a = flow_add_node(f, "default", (flow_pt){10, 5}, (void*)"A");  /* RIGHT@(14,6) */
    int b = flow_add_node(f, "default", (flow_pt){30, 5}, (void*)"B");  /* LEFT@(30,6) */
    flow_callbacks cb = {0};
    cb.on_connect = on_conn; cb.on_connect_start = on_cstart; cb.on_connect_end = on_cend;
    flow_set_callbacks(f, cb);
    flow_set_hover(f, a);
    flow_feed(f, "\x1b[<0;15;7M", 10);                  /* press A:out */
    ASSERT_INT(start_fires, 1, "on_connect_start fires on source-handle press");
    ASSERT_INT(start_src, a, "  start reports source A");
    ASSERT_STR(start_handle, "out", "  start reports the grabbed handle id");
    ASSERT_INT(end_fires, 0, "  no end yet (gesture in flight)");
    flow_feed(f, "\x1b[<32;31;7M", 11);                 /* motion onto B */
    flow_feed(f, "\x1b[<0;31;7m", 10);                  /* release on B:in */
    ASSERT_INT(conn_fires, 1, "on_connect fired (success)");
    ASSERT_INT(end_fires, 1, "on_connect_end fired once");
    ASSERT_INT(end_eid, flow_edges(f)[0].id, "  end reports the created edge id");
    ASSERT_INT(end_src, a, "  end reports source A");
    ASSERT_INT(end_tgt, b, "  end reports target B");
    ASSERT(start_seq < conn_seq && conn_seq < end_seq, "ordering: start -> on_connect -> end");
    ASSERT_INT(end_txn_depth, 0, "end fired AFTER the undo txn settled");
    flow_free(f);
  }

  /* ---- DROP ON EMPTY: start fires; end(-1, A, -1); no on_connect, no edge ---- */
  {
    lc_reset();
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    int a = flow_add_node(f, "default", (flow_pt){10, 5}, (void*)"A");
    flow_callbacks cb = {0};
    cb.on_connect = on_conn; cb.on_connect_start = on_cstart; cb.on_connect_end = on_cend;
    flow_set_callbacks(f, cb);
    flow_set_hover(f, a);
    flow_feed(f, "\x1b[<0;15;7M", 10);                  /* press A:out */
    flow_feed(f, "\x1b[<32;61;19M", 12);                /* motion to empty (60,18) */
    flow_feed(f, "\x1b[<0;61;19m", 11);                 /* release on empty */
    ASSERT_INT(start_fires, 1, "drop-empty: start fired on press");
    ASSERT_INT(end_fires, 1, "drop-empty: end fired once");
    ASSERT_INT(end_eid, -1, "  eid -1 (no edge)");
    ASSERT_INT(end_src, a, "  src A");
    ASSERT_INT(end_tgt, -1, "  target -1 (empty)");
    ASSERT_INT(conn_fires, 0, "  no on_connect");
    ASSERT_INT(flow_edge_count(f), 0, "  no edge added");
    flow_free(f);
  }

  /* ---- DROP ON SOURCE SELF: end(-1, A, -1), no on_connect ---- */
  {
    lc_reset();
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    int a = flow_add_node(f, "default", (flow_pt){10, 5}, (void*)"A");
    flow_callbacks cb = {0};
    cb.on_connect = on_conn; cb.on_connect_start = on_cstart; cb.on_connect_end = on_cend;
    flow_set_callbacks(f, cb);
    flow_set_hover(f, a);
    flow_feed(f, "\x1b[<0;15;7M", 10);                  /* press A:out */
    flow_feed(f, "\x1b[<32;13;7M", 11);                 /* motion onto A's own body (12,6) */
    flow_feed(f, "\x1b[<0;13;7m", 10);                  /* release on the source itself */
    ASSERT_INT(start_fires, 1, "self-drop: start fired");
    ASSERT_INT(end_fires, 1, "self-drop: end fired once");
    ASSERT_INT(end_eid, -1, "  eid -1");
    ASSERT_INT(end_tgt, -1, "  target -1 (self-drop reports none, spec 3b)");
    ASSERT_INT(conn_fires, 0, "  no on_connect");
    flow_free(f);
  }

  /* ---- VALIDATION REJECT (duplicate): end(-1, A, B) — target is the attempt ---- */
  {
    lc_reset();
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    int a = flow_add_node(f, "default", (flow_pt){10, 5}, (void*)"A");
    int b = flow_add_node(f, "default", (flow_pt){30, 5}, (void*)"B");
    flow_add_edge(f, a, b, "out", "in");                /* pre-existing duplicate */
    flow_callbacks cb = {0};
    cb.on_connect = on_conn; cb.on_connect_start = on_cstart; cb.on_connect_end = on_cend;
    flow_set_callbacks(f, cb);
    flow_set_hover(f, a);
    flow_feed(f, "\x1b[<0;15;7M", 10);                  /* press A:out */
    flow_feed(f, "\x1b[<32;31;7M", 11);                 /* motion onto B */
    flow_feed(f, "\x1b[<0;31;7m", 10);                  /* release on B:in -> duplicate reject */
    ASSERT_INT(start_fires, 1, "dup-reject: start fired");
    ASSERT_INT(end_fires, 1, "dup-reject: end fired once");
    ASSERT_INT(end_eid, -1, "  eid -1 (rejected)");
    ASSERT_INT(end_tgt, b, "  target B (the attempted target, spec 3c)");
    ASSERT_INT(conn_fires, 0, "  no on_connect on reject");
    ASSERT_INT(flow_edge_count(f), 1, "  still one edge");
    flow_free(f);
  }

  /* ---- ESC CANCEL: end(-1, A, -1); idempotent ESC with no gesture stays silent ---- */
  {
    lc_reset();
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    int a = flow_add_node(f, "default", (flow_pt){10, 5}, (void*)"A");
    flow_callbacks cb = {0};
    cb.on_connect_start = on_cstart; cb.on_connect_end = on_cend;
    flow_set_callbacks(f, cb);
    flow_feed(f, "\x1b", 1);                            /* ESC, nothing in flight */
    ASSERT_INT(end_fires, 0, "ESC with no gesture fires NO on_connect_end (cancel guard)");
    flow_cancel_connection(f);                          /* direct no-op cancel */
    ASSERT_INT(end_fires, 0, "direct cancel with no gesture stays silent");
    flow_set_hover(f, a);
    flow_feed(f, "\x1b[<0;15;7M", 10);                  /* press A:out */
    ASSERT_INT(start_fires, 1, "start fired");
    flow_feed(f, "\x1b", 1);                            /* ESC aborts */
    ASSERT_INT(end_fires, 1, "ESC cancel fires end once");
    ASSERT_INT(end_eid, -1, "  eid -1"); ASSERT_INT(end_src, a, "  src A");
    ASSERT_INT(end_tgt, -1, "  target -1");
    ASSERT_INT(flow_connecting(f), 0, "  connection state cleared");
    flow_free(f);
  }

  /* ---- RECONNECT ISOLATION: endpoint-reconnect drags fire NO lifecycle events ---- */
  {
    lc_reset();
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    int a = flow_add_node(f, "default", (flow_pt){10, 5},  (void*)"A");
    int b = flow_add_node(f, "default", (flow_pt){30, 5},  (void*)"B");
    int c = flow_add_node(f, "default", (flow_pt){30, 15}, (void*)"C");
    int e = flow_add_edge(f, a, b, "out", "in");
    flow_callbacks cb = {0};
    cb.on_connect = on_conn; cb.on_connect_start = on_cstart; cb.on_connect_end = on_cend;
    flow_set_callbacks(f, cb);
    flow_pt tp; flow_edge_endpoint_screen(f, flow_get_edge(f, e), 1, &tp);
    char seq[32]; int n = snprintf(seq, sizeof seq, "\x1b[<0;%d;%dM", tp.x + 1, tp.y + 1);
    flow_feed(f, seq, n);                               /* press target endpoint: arm reconnect */
    flow_feed(f, "\x1b[<32;33;17M", 12);                /* drag onto C's body (32,16) */
    flow_feed(f, "\x1b[<0;33;17m", 11);                 /* release on C: repoint */
    ASSERT_INT(flow_get_edge(f, e)->target, c, "reconnect repointed the edge (sanity)");
    ASSERT_INT(start_fires, 0, "reconnect: NO on_connect_start");
    ASSERT_INT(conn_fires, 0, "reconnect: NO on_connect");
    ASSERT_INT(end_fires, 0, "reconnect: NO on_connect_end (model-only in v1)");
    flow_free(f);
  }

  /* ---- CROSS-EVENT FALL-THROUGH (inc-5 #11, the #6-deferral contract REVERSED):
          a mid-flight press that CANCELS the connection (edge cell / empty pane /
          source) falls through to normal press classification — on_connect_end
          fires FIRST, then the element under the cursor gets its own event. A
          press that COMPLETES on a target node stays consumed (xyflow swallows
          the pointer event on a successful connect). One rule: consume iff the
          resolution completed on a node distinct from the source. ---- */
  {
    lc_reset();
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    int a = flow_add_node(f, "default", (flow_pt){10, 5},  (void*)"A");
    int c = flow_add_node(f, "default", (flow_pt){10, 15}, (void*)"C");
    int d = flow_add_node(f, "default", (flow_pt){30, 15}, (void*)"D");
    int e2 = flow_add_edge(f, c, d, "out", "in");
    flow_callbacks cb = {0};
    cb.on_connect_start = on_cstart; cb.on_connect_end = on_cend;
    cb.on_edge_click = on_eclick; cb.on_node_click = on_nclick;
    cb.on_pane_click = on_pane; cb.on_edge_context = on_ectx;
    flow_set_callbacks(f, cb);
    flow_pt s2, t2;
    flow_edge_endpoint_screen(f, flow_get_edge(f, e2), 0, &s2);
    flow_edge_endpoint_screen(f, flow_get_edge(f, e2), 1, &t2);
    flow_pt mid = { (s2.x + t2.x) / 2, (s2.y + t2.y) / 2 };
    ASSERT_INT(flow_hit_edge(f, mid, 0), e2, "precondition: mid cell sits on the edge path");
    char seq[32]; int n;

    /* (1) edge-cancel falls through: end fires, THEN the edge click */
    flow_set_hover(f, a);
    flow_feed(f, "\x1b[<0;15;7M", 10);                  /* press A:out -> in flight */
    ASSERT_INT(start_fires, 1, "in-flight precondition: start fired");
    n = snprintf(seq, sizeof seq, "\x1b[<0;%d;%dM", mid.x + 1, mid.y + 1);
    flow_feed(f, seq, n);                               /* press the edge mid-cell */
    ASSERT_INT(end_fires, 1, "press on edge mid-flight: gesture resolves (end fires)");
    ASSERT_INT(end_eid, -1, "  resolved as cancel (no node under the cell)");
    n = snprintf(seq, sizeof seq, "\x1b[<0;%d;%dm", mid.x + 1, mid.y + 1);
    flow_feed(f, seq, n);                               /* release */
    ASSERT_INT(eclick_fires, 1, "edge click NOW fires (cancel falls through — inverted pin)");
    ASSERT(end_seq < eclick_seq, "  ordering: on_connect_end BEFORE on_edge_click");

    /* (2) COMPLETION on a node stays consumed: no node-click from the same press */
    lc_reset();
    int ec0 = flow_edge_count(f);
    flow_set_hover(f, a);
    flow_feed(f, "\x1b[<0;15;7M", 10);                  /* press A:out -> in flight */
    flow_feed(f, "\x1b[<0;33;17M", 11);                 /* press D's body @ (32,16): completes */
    ASSERT_INT(end_fires, 1, "completion press: end fires");
    ASSERT(end_eid != -1, "  with the new edge id (success)");
    ASSERT_INT(flow_edge_count(f), ec0 + 1, "  edge added");
    flow_feed(f, "\x1b[<0;33;17m", 11);                 /* release on D */
    ASSERT_INT(nclick_fires, 0, "  the completing press is CONSUMED: no node-click on D");

    /* (3) pane-cancel falls through to the FULL pane-click path — the event AND
       its side effects (the release branch clears the selection before firing) */
    lc_reset(); pane_clicks = 0;
    flow_select_node(f, c, 0);                          /* pre-selected node to observe the clear */
    flow_set_hover(f, a);
    flow_feed(f, "\x1b[<0;15;7M", 10);                  /* in flight again */
    flow_feed(f, "\x1b[<0;61;4M", 10);                  /* press empty pane @ (60,3): cancel */
    ASSERT_INT(end_fires, 1, "pane press mid-flight: cancel fires end");
    ASSERT_INT(end_eid, -1, "  as a cancel");
    flow_feed(f, "\x1b[<0;61;4m", 10);                  /* release */
    ASSERT_INT(pane_clicks, 1, "  and the pane click NOW fires (fall-through)");
    ASSERT_INT(flow_selected_count(f), 0, "  with the full click path's side effects: selection cleared");

    /* (4) right-click mid-flight: cancel fires end, THEN on_edge_context */
    lc_reset();
    flow_set_hover(f, a);
    flow_feed(f, "\x1b[<0;15;7M", 10);                  /* in flight */
    n = snprintf(seq, sizeof seq, "\x1b[<2;%d;%dM", mid.x + 1, mid.y + 1);
    flow_feed(f, seq, n);                               /* right-press the edge mid-cell */
    ASSERT_INT(end_fires, 1, "right-click mid-flight: cancel fires end");
    ASSERT_INT(ectx_fires, 1, "  and on_edge_context fires for the same press");
    ASSERT(end_seq < ectx_seq, "  ordering: end BEFORE context");
    (void)a; (void)c; (void)d;
    flow_free(f);
  }

  return flowtest_report("test_connect");
}
