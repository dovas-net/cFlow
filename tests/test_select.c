#define FLOW_IMPLEMENTATION
#include "../flow.h"
#include "flowtest.h"

static int ctx_node = -2, pane_clicks = 0;
static void on_ctx(flow_t *f, int n, flow_pt s, void *u) { (void)f;(void)s;(void)u; ctx_node = n; }
static void on_pane(flow_t *f, flow_pt w, void *u) { (void)f;(void)w;(void)u; pane_clicks++; }

int main(void) {
  flow_t *f = flow_new(80, 24); flow_register_defaults(f);
  int a = flow_add_node(f, "default", (flow_pt){10, 5}, (void*)"A");  /* rect (10,5,5,3) */
  int b = flow_add_node(f, "default", (flow_pt){30, 5}, (void*)"B");
  flow_callbacks cb = {0}; cb.on_node_context = on_ctx; cb.on_pane_click = on_pane;
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

  flow_free(f);
  return flowtest_report("test_select");
}
