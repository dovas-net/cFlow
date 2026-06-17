// cpp_link_main — a C++ translation unit that includes flow.h declaration-only and
// links against a C-compiled flow implementation (tests/cpp_link_impl.c via cc).
// This is the definitive extern "C" test: WITHOUT extern "C" the C++ compiler would
// name-mangle the flow_* references and the link would fail to resolve the C symbols.
#include "../flow.h"
#include <cstdlib>
#include <cstring>

int main() {
  flow_t *f = flow_new(30, 10);
  if (!f) return 1;
  flow_register_defaults(f);
  int a = flow_add_node(f, "default", flow_pt{ 2, 2 },  const_cast<char *>("A"));
  int b = flow_add_node(f, "default", flow_pt{ 15, 5 }, const_cast<char *>("B"));
  flow_add_edge(f, a, b, "", "");
  char *frame = flow_render_diff(f);
  int ok = (frame && std::strlen(frame) > 0 && flow_node_count(f) == 2 && flow_edge_count(f) == 1);
  std::free(frame);
  flow_free(f);
  return ok ? 0 : 1;
}
