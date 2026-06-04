#define FLOW_IMPLEMENTATION
#include "../flow.h"
#include "flowtest.h"

static int ctx_node = -2, pane_clicks = 0, sel_changes = 0;
static void on_ctx(flow_t *f, int n, flow_pt s, void *u) { (void)f;(void)s;(void)u; ctx_node = n; }
static void on_pane(flow_t *f, flow_pt w, void *u) { (void)f;(void)w;(void)u; pane_clicks++; }
static void on_selchange(flow_t *f, const int *ids, int n, void *u) { (void)f;(void)ids;(void)n;(void)u; sel_changes++; }

int main(void) {
  flow_t *f = flow_new(80, 24); flow_register_defaults(f);
  int a = flow_add_node(f, "default", (flow_pt){10, 5}, (void*)"A");  /* rect (10,5,5,3) */
  int b = flow_add_node(f, "default", (flow_pt){30, 5}, (void*)"B");
  flow_callbacks cb = {0}; cb.on_node_context = on_ctx; cb.on_pane_click = on_pane;
  cb.on_selection_change = on_selchange;
  flow_set_callbacks(f, cb);

  /* click (press+release, same cell) inside A selects it */
  flow_feed(f, "\x1b[<0;12;7M", 10); flow_feed(f, "\x1b[<0;12;7m", 10);
  ASSERT_INT(flow_selected_node(f), a, "click selects A");
  ASSERT(flow_get_node(f, a)->flags & FLOW_SELECTED, "A has selected flag");

  /* click B selects B and deselects A */
  flow_feed(f, "\x1b[<0;32;7M", 10); flow_feed(f, "\x1b[<0;32;7m", 10);
  ASSERT_INT(flow_selected_node(f), b, "click selects B");
  ASSERT(!(flow_get_node(f, a)->flags & FLOW_SELECTED), "A deselected");

  /* click empty clears selection and fires on_pane_click */
  flow_feed(f, "\x1b[<0;60;20M", 11); flow_feed(f, "\x1b[<0;60;20m", 11);
  ASSERT_INT(flow_selected_node(f), -1, "empty click clears selection");
  ASSERT_INT(pane_clicks, 1, "on_pane_click fired once");

  /* drag A: press, move (>=1 cell), release -> moves AND selects (selectNodesOnDrag) */
  flow_feed(f, "\x1b[<0;12;7M", 10); flow_feed(f, "\x1b[<32;15;7M", 11); flow_feed(f, "\x1b[<0;15;7m", 10);
  ASSERT(flow_get_node(f, a)->pos.x != 10, "drag moved A");
  ASSERT_INT(flow_selected_node(f), a, "drag selected A");

  /* right-click B fires context callback, does not start a drag */
  ctx_node = -2;
  flow_feed(f, "\x1b[<2;32;7M", 10);
  ASSERT_INT(ctx_node, b, "right-click fired on_node_context for B");

  /* ---- multi-select: shift/ctrl-click builds a set ---- */
  /* the drag above moved A; reset both to their original rects so the SGR
     click coordinates below land inside the nodes again. */
  flow_move_node(f, a, (flow_pt){10, 5});
  flow_move_node(f, b, (flow_pt){30, 5});
  flow_clear_selection(f);
  /* SHIFT-click A (button|4=4), then SHIFT-click B -> both selected, count==2 */
  flow_feed(f, "\x1b[<4;12;7M", 10); flow_feed(f, "\x1b[<4;12;7m", 10);
  flow_feed(f, "\x1b[<4;32;7M", 10); flow_feed(f, "\x1b[<4;32;7m", 10);
  ASSERT_INT(flow_selected_count(f), 2, "shift-click A then B -> 2 selected");
  ASSERT(flow_get_node(f, a)->flags & FLOW_SELECTED, "A selected (multi)");
  ASSERT(flow_get_node(f, b)->flags & FLOW_SELECTED, "B selected (multi)");

  /* CTRL-click A again (button|16=16) toggles A OFF -> count==1, B remains */
  flow_feed(f, "\x1b[<16;12;7M", 11); flow_feed(f, "\x1b[<16;12;7m", 11);
  ASSERT_INT(flow_selected_count(f), 1, "ctrl-click A toggles off -> 1 selected");
  ASSERT(!(flow_get_node(f, a)->flags & FLOW_SELECTED), "A toggled off");
  ASSERT(flow_get_node(f, b)->flags & FLOW_SELECTED, "B still selected");
  int out[8]; int total = flow_selected_nodes(f, out, 8);
  ASSERT_INT(total, 1, "flow_selected_nodes returns 1");
  ASSERT_INT(out[0], b, "flow_selected_nodes out[0]==B");

  /* plain (no-mod) click B after a multi-selection -> replace semantics: only B */
  flow_select_node(f, a, 1);                       /* make it a 2-set first ({A,B}) */
  ASSERT_INT(flow_selected_count(f), 2, "set up 2-selection before plain click");
  flow_feed(f, "\x1b[<0;32;7M", 10); flow_feed(f, "\x1b[<0;32;7m", 10);
  ASSERT_INT(flow_selected_count(f), 1, "plain click B replaces -> 1 selected");
  ASSERT_INT(flow_selected_node(f), b, "plain click B -> only B selected");

  /* ---- lone ESC clears selection (sig-gated on_selection_change) ---- */
  flow_select_node(f, a, 0); flow_select_node(f, b, 1);   /* {A,B} */
  ASSERT_INT(flow_selected_count(f), 2, "ESC test precondition: 2 selected");
  sel_changes = 0;
  flow_feed(f, "\x1b", 1);                                /* lone ESC, not a CSI prefix */
  ASSERT_INT(flow_selected_count(f), 0, "lone ESC clears selection");
  ASSERT_INT(sel_changes, 1, "on_selection_change fired once on ESC clear");
  flow_feed(f, "\x1b", 1);                                /* ESC again: already empty */
  ASSERT_INT(flow_selected_count(f), 0, "second ESC: selection still empty");
  ASSERT_INT(sel_changes, 1, "second ESC fires NO event (sig-gated)");

  flow_free(f);
  return flowtest_report("test_select");
}
