#define FLOW_IMPLEMENTATION
#include "../flow.h"
#include "flowtest.h"

static int pane_clicks = 0;
static void on_pane(flow_t *f, flow_pt w, void *u) { (void)f;(void)w;(void)u; pane_clicks++; }

/* Build a fresh graph with three nodes at known world rects (zoom==1, no pan, so
   screen == world). Single-char labels -> each node is 5 wide, 3 tall.
   A: (10,5,5,3) -> x10..14 y5..7   B: (30,5,5,3) -> x30..34 y5..7   C: (10,15,5,3) */
static flow_t *mkgraph(int *a, int *b, int *c) {
  flow_t *f = flow_new(80, 24); flow_register_defaults(f);
  *a = flow_add_node(f, "default", (flow_pt){10, 5},  (void*)"A");
  *b = flow_add_node(f, "default", (flow_pt){30, 5},  (void*)"B");
  *c = flow_add_node(f, "default", (flow_pt){10, 15}, (void*)"C");
  return f;
}

int main(void) {
  int a, b, c;

  /* ---- PARTIAL marquee: a box clipping only A selects A, not B/C ---- */
  {
    flow_t *f = mkgraph(&a, &b, &c);   /* default marquee mode is PARTIAL */
    /* shift-drag screen (8,4)->(16,9): SGR 1-based (9,5)->(17,10).
       press shift on empty (button|4=4), motion (button|4|32=36), release (4 lowercase m) */
    flow_feed(f, "\x1b[<4;9;5M", 9);     /* press shift @ (8,4) empty */
    flow_feed(f, "\x1b[<36;17;10M", 12); /* motion to (16,9) -> grow box */
    /* world box (8,4)..(16,9) fully encloses A; disjoint from B and C */
    ASSERT(flow_get_node(f, a)->flags & FLOW_SELECTED, "PARTIAL: A selected during drag");
    ASSERT(!(flow_get_node(f, b)->flags & FLOW_SELECTED), "PARTIAL: B not selected");
    ASSERT(!(flow_get_node(f, c)->flags & FLOW_SELECTED), "PARTIAL: C not selected");
    flow_feed(f, "\x1b[<4;17;10m", 11);  /* release: finalize */
    ASSERT_INT(flow_selected_count(f), 1, "PARTIAL marquee selects 1 (A)");
    ASSERT_INT(flow_selected_node(f), a, "PARTIAL marquee -> A");
    flow_free(f);
  }

  /* ---- PARTIAL vs FULL on a partially-overlapping box ---- */
  {
    /* box screen (8,4)->(12,9): SGR (9,5)->(13,10). world rect (8,4,4,5)=x8..12 y4..9.
       A spans x10..14 -> overlaps x10..12 only (partial, NOT fully enclosed). */
    flow_t *f = mkgraph(&a, &b, &c);
    flow_feed(f, "\x1b[<4;9;5M", 9);
    flow_feed(f, "\x1b[<36;13;10M", 12);
    flow_feed(f, "\x1b[<4;13;10m", 11);
    ASSERT_INT(flow_selected_count(f), 1, "PARTIAL: partial-overlap box selects A");
    ASSERT_INT(flow_selected_node(f), a, "PARTIAL: partial-overlap -> A");
    flow_free(f);

    /* FULL mode: same partial box does NOT select A */
    flow_t *g = mkgraph(&a, &b, &c);
    flow_set_marquee_mode(g, FLOW_SELECT_FULL);
    flow_feed(g, "\x1b[<4;9;5M", 9);
    flow_feed(g, "\x1b[<36;13;10M", 12);
    flow_feed(g, "\x1b[<4;13;10m", 11);
    ASSERT_INT(flow_selected_count(g), 0, "FULL: partial-overlap box selects nothing");
    flow_free(g);

    /* FULL mode: a fully-enclosing box (8,4)->(16,9) DOES select A */
    flow_t *h = mkgraph(&a, &b, &c);
    flow_set_marquee_mode(h, FLOW_SELECT_FULL);
    flow_feed(h, "\x1b[<4;9;5M", 9);
    flow_feed(h, "\x1b[<36;17;10M", 12);
    flow_feed(h, "\x1b[<4;17;10m", 11);
    ASSERT_INT(flow_selected_count(h), 1, "FULL: enclosing box selects A");
    ASSERT_INT(flow_selected_node(h), a, "FULL: enclosing -> A");
    flow_free(h);
  }

  /* ---- marquee drag does NOT fire on_pane_click; a plain empty click does ---- */
  {
    flow_t *f = mkgraph(&a, &b, &c);
    flow_callbacks cb = {0}; cb.on_pane_click = on_pane; flow_set_callbacks(f, cb);
    pane_clicks = 0;
    /* shift-drag marquee on empty space */
    flow_feed(f, "\x1b[<4;9;5M", 9);
    flow_feed(f, "\x1b[<36;17;10M", 12);
    flow_feed(f, "\x1b[<4;17;10m", 11);
    ASSERT_INT(pane_clicks, 0, "marquee drag does not fire on_pane_click");
    /* plain (no-shift) empty click fires it once */
    flow_feed(f, "\x1b[<0;60;20M", 11);
    flow_feed(f, "\x1b[<0;60;20m", 11);
    ASSERT_INT(pane_clicks, 1, "plain empty click fires on_pane_click once");
    flow_free(f);
  }

  /* ---- multi-node drag: select A and B (shift-clicks), drag A -> both shift; C stays ---- */
  {
    flow_t *f = mkgraph(&a, &b, &c);
    int ax = flow_get_node(f, a)->pos.x, ay = flow_get_node(f, a)->pos.y;
    int bx = flow_get_node(f, b)->pos.x, by = flow_get_node(f, b)->pos.y;
    int cx = flow_get_node(f, c)->pos.x, cy = flow_get_node(f, c)->pos.y;
    /* shift-click A then B -> {A,B} */
    flow_feed(f, "\x1b[<4;12;7M", 10); flow_feed(f, "\x1b[<4;12;7m", 10);
    flow_feed(f, "\x1b[<4;32;7M", 10); flow_feed(f, "\x1b[<4;32;7m", 10);
    ASSERT_INT(flow_selected_count(f), 2, "multi-drag setup: A and B selected");
    /* drag A by +10/+3: press in A @ screen (11,6)=SGR(12,7), motion to (21,9)=SGR(22,10), release */
    flow_feed(f, "\x1b[<0;12;7M", 10);     /* press in A (no mod) — A already selected -> group drag */
    flow_feed(f, "\x1b[<32;22;10M", 12);   /* motion +10 x, +3 y */
    flow_feed(f, "\x1b[<0;22;10m", 11);    /* release */
    ASSERT_INT(flow_get_node(f, a)->pos.x, ax + 10, "A.x shifted +10");
    ASSERT_INT(flow_get_node(f, a)->pos.y, ay + 3,  "A.y shifted +3");
    ASSERT_INT(flow_get_node(f, b)->pos.x, bx + 10, "B.x shifted +10 (group)");
    ASSERT_INT(flow_get_node(f, b)->pos.y, by + 3,  "B.y shifted +3 (group)");
    ASSERT_INT(flow_get_node(f, c)->pos.x, cx, "C.x unchanged (unselected)");
    ASSERT_INT(flow_get_node(f, c)->pos.y, cy, "C.y unchanged (unselected)");
    flow_free(f);
  }

  /* ---- marquee skips hidden nodes (inc-4 #11, view-level filtering) ---- */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    int a = flow_add_node(f, "default", (flow_pt){10, 5},  (void*)"A");
    int b = flow_add_node(f, "default", (flow_pt){10, 12}, (void*)"B");
    flow_set_node_hidden(f, b, 1);
    int n = flow_select_in_rect(f, (flow_rect){5, 2, 30, 18}, FLOW_SELECT_PARTIAL, 0);
    ASSERT_INT(n, 1, "marquee over both selects only the visible node");
    ASSERT(flow_get_node(f, a)->flags & FLOW_SELECTED, "  A selected");
    ASSERT(!(flow_get_node(f, b)->flags & FLOW_SELECTED), "  hidden B not selected");
    flow_free(f);
  }

  return flowtest_report("test_marquee");
}
