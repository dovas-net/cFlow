#define FLOW_IMPLEMENTATION
#include "../flow.h"
#include "flowtest.h"
#include <string.h>

/* inc-9 #1 — lone-ESC (and any future gesture-teardown caller) must CANCEL an in-flight
   pointer gesture, not strand it. Before this fix the lone-ESC handler (flow_run.h) cleared
   the connection/selection/focus but left resize_node / drag_node / reconnect_edge armed AND
   the gesture's undo bracket OPEN — so flow_undo/flow_redo went permanently dead (txn_depth>0
   guard) on the keyboard-only path, and a later mouse release acted on the stranded gesture
   (e.g. a reconnect repointing the edge onto whatever sat under the cursor). The shared
   flow__cancel_gesture closes the bracket (committing what was recorded as ONE undo step —
   "commit-what's-done" semantics) and resets all transient gesture state so the trailing
   release is an inert no-op. These suites drive the three bracket-opening gestures
   (resize / node-drag / edge-reconnect) plus the idle-ESC idempotency guard. */

static void feed(flow_t *f, const char *s) { flow_feed(f, s, (int)strlen(s)); }
static void press_at(flow_t *f, int sx, int sy)   { char b[32]; snprintf(b, sizeof b, "\x1b[<0;%d;%dM",  sx + 1, sy + 1); feed(f, b); }
static void move_to(flow_t *f, int sx, int sy)    { char b[32]; snprintf(b, sizeof b, "\x1b[<32;%d;%dM", sx + 1, sy + 1); feed(f, b); }
static void release_at(flow_t *f, int sx, int sy) { char b[32]; snprintf(b, sizeof b, "\x1b[<0;%d;%dm",  sx + 1, sy + 1); feed(f, b); }

/* SE corner screen cell of node `id` at zoom 1 / no pan (screen == world). */
static flow_pt se_corner(flow_t *f, int id) {
  flow_node *n = flow_get_node(f, id);
  flow_pt a = flow_node_abs(f, n);
  flow_pt p = { a.x + n->w - 1, a.y + n->h - 1 };
  return p;
}

int main(void) {
  /* 1: lone ESC mid-RESIZE — disarms, closes the bracket (undo live again), commits the
     partial resize as ONE step, the trailing release is inert, and undo reverts it. */
  {
    int cols = 30, rows = 16;
    flow_cell *buf = (flow_cell*)malloc((size_t)cols*rows*sizeof(flow_cell));
    flow_t *f = flow_new(cols, rows); flow_register_defaults(f);
    int a = flow_add_node(f, "default", (flow_pt){5, 5}, (void*)"X");  /* w=5,h=3 */
    flow_set_resizer(f, 1); flow_select_node(f, a, 0);
    flow_render(f, buf, cols, rows);
    flow_pt se = se_corner(f, a);
    int d0 = flow_undo_depth(f);
    press_at(f, se.x, se.y);
    ASSERT_INT(f->resize_node, a, "press armed the resize");
    move_to(f, se.x + 6, se.y + 4);                    /* grow to 11x7, records into the open txn */
    ASSERT_INT(flow_get_node(f, a)->w, 11, "mid-gesture width grew");
    ASSERT(f->journal.txn_depth > 0, "resize gesture left an OPEN undo bracket");
    feed(f, "\x1b");                                   /* lone ESC mid-gesture */
    ASSERT_INT(f->resize_node, -1, "ESC disarms the resize gesture");
    ASSERT_INT(f->journal.txn_depth, 0, "ESC closes the bracket — undo no longer dead");
    ASSERT_INT(flow_undo_depth(f), d0 + 1, "ESC commits the partial resize as ONE undo step");
    ASSERT_INT(flow_get_node(f, a)->w, 11, "committed at the dragged size (commit-what's-done)");
    release_at(f, se.x + 6, se.y + 4);                 /* the release AFTER an ESC is inert */
    ASSERT_INT(f->resize_node, -1, "post-ESC release stays disarmed");
    ASSERT_INT(flow_undo_depth(f), d0 + 1, "post-ESC release records nothing");
    ASSERT_INT(flow_get_node(f, a)->w, 11, "post-ESC release does not re-resize");
    flow_undo(f);
    ASSERT_INT(flow_get_node(f, a)->w, 5, "undo reverts to pre-gesture width");
    ASSERT_INT(flow_get_node(f, a)->h, 3, "undo reverts to pre-gesture height");
    free(buf); flow_free(f);
  }

  /* 2: lone ESC mid-DRAG — same contract for a node drag (the gesture brackets at the
     threshold-cross, so drag_node armed <=> bracket open). */
  {
    int cols = 30, rows = 16;
    flow_cell *buf = (flow_cell*)malloc((size_t)cols*rows*sizeof(flow_cell));
    flow_t *f = flow_new(cols, rows); flow_register_defaults(f);   /* resizer OFF: body press arms a drag */
    int a = flow_add_node(f, "default", (flow_pt){5, 5}, (void*)"X");  /* w=5,h=3 */
    flow_render(f, buf, cols, rows);
    flow_pt p0 = flow_node_abs(f, flow_get_node(f, a));            /* (5,5) */
    int d0 = flow_undo_depth(f);
    press_at(f, p0.x + 1, p0.y + 1);                              /* press the node body (6,6) */
    ASSERT_INT(f->down_node, a, "press hit the node body");
    move_to(f, p0.x + 5, p0.y + 4);                               /* threshold-cross: drag arms + moves */
    ASSERT_INT(f->drag_node, a, "motion armed the node drag");
    ASSERT(f->journal.txn_depth > 0, "drag gesture left an OPEN undo bracket");
    flow_pt pmid = flow_node_abs(f, flow_get_node(f, a));
    ASSERT(pmid.x != p0.x || pmid.y != p0.y, "node moved during the drag");
    feed(f, "\x1b");                                              /* lone ESC mid-drag */
    ASSERT_INT(f->drag_node, -1, "ESC disarms the drag gesture");
    ASSERT_INT(f->journal.txn_depth, 0, "ESC closes the drag bracket");
    ASSERT_INT(flow_undo_depth(f), d0 + 1, "ESC commits the partial drag as ONE undo step");
    release_at(f, p0.x + 5, p0.y + 4);                            /* inert */
    ASSERT_INT(f->drag_node, -1, "post-ESC release stays disarmed");
    ASSERT_INT(flow_undo_depth(f), d0 + 1, "post-ESC release records nothing");
    flow_undo(f);
    flow_pt pu = flow_node_abs(f, flow_get_node(f, a));
    ASSERT_INT(pu.x, p0.x, "undo reverts the drag (x)");
    ASSERT_INT(pu.y, p0.y, "undo reverts the drag (y)");
    free(buf); flow_free(f);
  }

  /* 3: lone ESC mid-RECONNECT — the most dangerous strand: a stranded reconnect would repoint
     the edge onto whatever the release lands on. ESC must disarm + close the (empty) bracket so
     the release cannot repoint, and the edge endpoints stay put. */
  {
    int cols = 40, rows = 16;
    flow_cell *buf = (flow_cell*)malloc((size_t)cols*rows*sizeof(flow_cell));
    flow_t *f = flow_new(cols, rows); flow_register_defaults(f);
    int a = flow_add_node(f, "default", (flow_pt){5, 5},  (void*)"A");
    int b = flow_add_node(f, "default", (flow_pt){20, 5}, (void*)"B");
    int e = flow_add_edge(f, a, b, "out", "in");
    flow_render(f, buf, cols, rows);
    flow_pt tp; int okt = flow_edge_endpoint_screen(f, flow_get_edge(f, e), 1, &tp);  /* target endpoint */
    ASSERT_INT(okt, 1, "target endpoint resolved");
    int src0 = flow_get_edge(f, e)->source, tgt0 = flow_get_edge(f, e)->target;
    int d0 = flow_undo_depth(f);
    press_at(f, tp.x, tp.y);                                      /* arm reconnect on the endpoint */
    ASSERT_INT(f->reconnect_edge, e, "press on the endpoint armed reconnect");
    ASSERT(f->journal.txn_depth > 0, "reconnect gesture left an OPEN undo bracket");
    move_to(f, tp.x + 2, tp.y + 2);                              /* drag the free end (records nothing yet) */
    feed(f, "\x1b");                                              /* lone ESC mid-reconnect */
    ASSERT_INT(f->reconnect_edge, -1, "ESC disarms the reconnect gesture");
    ASSERT_INT(f->journal.txn_depth, 0, "ESC closes the reconnect bracket");
    ASSERT_INT(flow_undo_depth(f), d0, "no repoint happened -> empty bracket -> no undo step");
    release_at(f, tp.x + 2, tp.y + 2);                           /* inert: must NOT repoint */
    ASSERT_INT(f->reconnect_edge, -1, "post-ESC release stays disarmed");
    ASSERT_INT(flow_get_edge(f, e)->source, src0, "edge source unchanged by the cancelled reconnect");
    ASSERT_INT(flow_get_edge(f, e)->target, tgt0, "edge target unchanged by the cancelled reconnect");
    free(buf); flow_free(f);
  }

  /* 4: idle ESC (no gesture in flight) stays an idempotent no-op — it must NOT call an
     unbalanced flow__undo_end (txn-depth underflow) nor invent an undo step, while still
     clearing the selection as it always has. */
  {
    flow_t *f = flow_new(30, 16); flow_register_defaults(f);
    int a = flow_add_node(f, "default", (flow_pt){5, 5}, (void*)"X");
    flow_select_node(f, a, 0);
    int d0 = flow_undo_depth(f);
    feed(f, "\x1b");                                              /* ESC, nothing armed */
    ASSERT_INT(f->journal.txn_depth, 0, "idle ESC does not underflow the txn depth");
    ASSERT_INT(flow_undo_depth(f), d0, "idle ESC records no undo step");
    ASSERT_INT(f->resize_node, -1, "no gesture armed");
    ASSERT_INT(flow_selected_count(f), 0, "idle ESC still clears the selection (existing behavior)");
    flow_free(f);
  }

  return flowtest_report("test_cancel_gesture");
}
