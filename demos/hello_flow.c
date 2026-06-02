/* hello_flow — minimal flow demo: three nodes, two edges, arrow-key pan, q to quit. */
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
  flow_run(f);
  flow_free(f);
  return 0;
}
