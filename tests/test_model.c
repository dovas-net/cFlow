#define FLOW_IMPLEMENTATION
#include "../flow.h"
#include "flowtest.h"

/* a trivial node type so measure() gives deterministic sizes for hit/bounds */
static void m_measure(const flow_node *n, int *w, int *h) { (void)n; *w = 4; *h = 3; }
static void m_render(const flow_node *n, flow_surface *s, flow_render_ctx c) { (void)n;(void)s;(void)c; }
static const flow_node_type M = { "box", m_measure, m_render, NULL, 0 };

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

  return flowtest_report("test_model");
}
