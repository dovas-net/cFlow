// cpp_smoke — compile flow.h AS the implementation under C++ (the H6 enum-init fix),
// and exercise a CUSTOM node type whose measure/render callbacks are C++ functions
// (surfaces any extern "C" fn-pointer-linkage mismatch). Compile + run with c++.
#define FLOW_IMPLEMENTATION
#include "../flow.h"
#include <cstring>
#include <cstdlib>

static void cpp_measure(const flow_node *n, int *w, int *h) {
  const char *l = n->data ? static_cast<const char *>(n->data) : "";
  *w = static_cast<int>(std::strlen(l)) + 4; *h = 3;
}
static void cpp_render(const flow_node *n, flow_surface *s, flow_render_ctx ctx) {
  unsigned bold = (ctx.flags & FLOW_SELECTED) ? FLOW_BOLD : 0u;
  flow_box(s, 0, 0, flow_surface_w(s), flow_surface_h(s), FLOW_FG, FLOW_BG, bold);
  if (n->data) flow_text(s, 1, 1, static_cast<const char *>(n->data), FLOW_FG, FLOW_BG, bold);
}

int main() {
  static const flow_node_type custom =
    { "cpp", cpp_measure, cpp_render, nullptr, 0, nullptr, nullptr, nullptr };
  flow_t *f = flow_new(40, 12);
  if (!f) return 1;
  flow_register_node_type(f, &custom);
  int a = flow_add_node(f, "cpp", flow_pt{ 2, 2 },  const_cast<char *>("hello"));
  int b = flow_add_node(f, "cpp", flow_pt{ 20, 6 }, const_cast<char *>("world"));
  flow_add_edge(f, a, b, "", "");

  flow_cell *buf = static_cast<flow_cell *>(std::calloc(static_cast<size_t>(40 * 12), sizeof(flow_cell)));
  flow_render(f, buf, 40, 12);                         // render into a host buffer
  char *frame = flow_render_diff(f);                  // and via the embed primitive
  int ok = (buf && frame && std::strlen(frame) > 0 && flow_node_count(f) == 2);
  std::free(frame); std::free(buf); flow_free(f);
  return ok ? 0 : 1;
}
