/* hello_flow — minimal flow demo: three nodes, two edges. Keys: arrows pan,
   x/Del delete selected, n add node, f fit-view, ? toggle help bar, q quit. */
#define FLOW_IMPLEMENTATION
#include "../flow.h"
int main(void) {
  flow_t *f = flow_new(80, 24);
  flow_register_defaults(f);
  int a = flow_add_node(f, "default", (flow_pt){ 4,  3}, (void*)"web-server");
  int b = flow_add_node(f, "default", (flow_pt){34, 12}, (void*)"database");
  int c = flow_add_node(f, "default", (flow_pt){58,  4}, (void*)"cache");
  flow_add_edge(f, a, b, "", "");
  flow_add_edge(f, a, c, "", "");
  flow_set_statusbar(f, 1);   /* show the built-in n:add x:del f:fit ?:help q:quit bar */
  flow_run(f);
  flow_free(f);
  return 0;
}
