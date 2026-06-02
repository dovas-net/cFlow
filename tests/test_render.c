#define FLOW_IMPLEMENTATION
#include "../flow.h"
#include "flowtest.h"

static char *cells_to_string(const flow_cell *c, int cols, int rows) {
  char *s = (char*)malloc((size_t)cols * rows * 4 + rows + 1); int len = 0; char u[5];
  for (int y = 0; y < rows; y++) {
    int last = -1;
    for (int x = 0; x < cols; x++) { uint32_t ch = c[y*cols+x].ch; if (ch && ch != ' ') last = x; }
    for (int x = 0; x <= last; x++) { uint32_t ch = c[y*cols+x].ch; if (!ch) ch = ' ';
      int n = flow_utf8(ch, u); memcpy(s+len, u, n); len += n; }
    s[len++] = '\n';
  }
  s[len] = 0; return s;
}

int main(void) {
  int cols = 30, rows = 8;
  flow_cell *buf = (flow_cell*)malloc((size_t)cols * rows * sizeof(flow_cell));

  /* single node */
  flow_t *f = flow_new(cols, rows); flow_register_defaults(f);
  flow_add_node(f, "default", (flow_pt){0, 0}, (void*)"hi");
  flow_render(f, buf, cols, rows);
  char *s1 = cells_to_string(buf, cols, rows);
  SNAPSHOT("render_single", s1);   /* expect a 6x3 box "hi" at top-left */
  free(s1); flow_free(f);

  /* two nodes + edge */
  flow_t *g = flow_new(cols, rows); flow_register_defaults(g);
  int a = flow_add_node(g, "default", (flow_pt){0, 1}, (void*)"A");
  int b = flow_add_node(g, "default", (flow_pt){15, 4}, (void*)"B");
  flow_add_edge(g, a, b, "", "");
  flow_render(g, buf, cols, rows);
  char *s2 = cells_to_string(buf, cols, rows);
  SNAPSHOT("render_two_edge", s2);  /* expect two boxes joined by a path ending in an arrow */
  int has_arrow = 0;
  for (int i = 0; i < cols * rows; i++) { uint32_t c = buf[i].ch;
    if (c==0x25B6||c==0x25C0||c==0x25BC||c==0x25B2) has_arrow = 1; }
  ASSERT(has_arrow, "edge renders an arrowhead");
  free(s2); flow_free(g);

  free(buf);
  return flowtest_report("test_render");
}
