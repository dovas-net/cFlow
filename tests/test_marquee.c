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

/* synthetic shift SGR-1006 helpers at 0-based cells (duplicated from test_autopan.c —
   they are static there, not in flowtest.h) */
static void ws_feed(flow_t *f, const char *s)            { flow_feed(f, s, (int)strlen(s)); }
static void ws_press_shift(flow_t *f, int sx, int sy)    { char b[32]; snprintf(b, sizeof b, "\x1b[<4;%d;%dM",  sx + 1, sy + 1); ws_feed(f, b); }
static void ws_move_shift(flow_t *f, int sx, int sy)     { char b[32]; snprintf(b, sizeof b, "\x1b[<36;%d;%dM", sx + 1, sy + 1); ws_feed(f, b); }
static void ws_release_shift(flow_t *f, int sx, int sy)  { char b[32]; snprintf(b, sizeof b, "\x1b[<4;%d;%dm",  sx + 1, sy + 1); ws_feed(f, b); }

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

  /* ---- WORLD-STABLE ANCHOR (inc-5 deferral #1): under sustained edge-band
     auto-pan the marquee rect must GROW from the press point's world position,
     not translate with the screen anchor. ---- */
  {
    /* 80x24, zoom 1, margin 3 / speed 2: right band x>=77, bottom band y>=21;
       each in-band shift-motion pans the view (-2,-2). Press (50,10) on empty
       pane (A is BESIDE the anchor at world x52..56 y12..14, never under it).
       Far (x95..99 y30..32) is off-screen at press; enters the grown rect only
       after sustained pan. After 9 motions at (79,22): ox=oy=-18, world cursor
       = (97,40), rect (50,10)..(97,40) covers BOTH A and Far. Under the old
       screen-pinned anchor the rect would be (68,28)..(97,40) — off A. */
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    int A   = flow_add_node(f, "default", (flow_pt){52, 12}, (void*)"A");
    int Far = flow_add_node(f, "default", (flow_pt){95, 30}, (void*)"F");
    ws_press_shift(f, 50, 10);
    ws_move_shift(f, 79, 22);                /* arms + first in-band pan */
    ASSERT_INT(f->marquee_on, 1, "world-anchor: marquee armed");
    ASSERT(f->view.ox != 0.0f && f->view.oy != 0.0f, "world-anchor: auto-pan fired (precondition)");
    for (int i = 0; i < 8; i++) ws_move_shift(f, 79, 22);   /* 9 in-band motions total */
    ASSERT(flow_get_node(f, A)->flags & FLOW_SELECTED,
           "world-anchor: node beside the ORIGINAL anchor still selected (rect grew)");
    ASSERT(flow_get_node(f, Far)->flags & FLOW_SELECTED,
           "world-anchor: far node swept up by the grown rect");
    ASSERT(flow_selected_count(f) >= 2, "world-anchor: rect expanded across the pan");
    ws_release_shift(f, 79, 22);
    ASSERT_INT(f->marquee_on, 0, "world-anchor: release clears marquee");
    ASSERT(flow_selected_count(f) >= 2, "world-anchor: release keeps the selection");
    flow_free(f);

    /* zoom != 1: the world<->screen round-trip is exercised, not bypassed.
       zoom 2 about screen (40,12): ox=-40, oy=-12. Press (50,10) -> world
       (45,11) exactly. Node Z at world (47,12) (screen x54..62 y12..16, not
       under the press). Short sweep to (60,16): no band touched, no pan. */
    flow_t *g = flow_new(80, 24); flow_register_defaults(g);
    int Z = flow_add_node(g, "default", (flow_pt){47, 12}, (void*)"Z");
    flow_set_zoom(g, 2.0f, (flow_pt){40, 12});
    ws_press_shift(g, 50, 10);
    ws_move_shift(g, 60, 16);
    ASSERT_INT(g->marquee_on, 1, "zoom2: marquee armed");
    flow_pt rp = flow_to_screen(g, g->marquee_anchor_world);
    ASSERT(rp.x >= 49 && rp.x <= 51 && rp.y >= 9 && rp.y <= 11,
           "zoom2: world anchor re-projects within +/-1 of the press cell");
    ASSERT(flow_get_node(g, Z)->flags & FLOW_SELECTED, "zoom2: swept node selected through the round-trip");
    ws_release_shift(g, 60, 16);
    ASSERT_INT(g->marquee_on, 0, "zoom2: release clears marquee");
    flow_free(g);
  }

  return flowtest_report("test_marquee");
}
