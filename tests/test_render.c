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

  /* minimap: bordered box in the BR corner with node dots inside */
  flow_t *m = flow_new(40, 12); flow_register_defaults(m);
  flow_add_node(m, "default", (flow_pt){0, 0}, (void*)"A");
  flow_add_node(m, "default", (flow_pt){25, 8}, (void*)"B");
  flow_set_minimap(m, 1, FLOW_CORNER_BR, 12, 6);
  flow_cell *mb = (flow_cell*)malloc((size_t)40 * 12 * sizeof(flow_cell));
  flow_render(m, mb, 40, 12);
  ASSERT_INT(mb[6*40 + 28].ch, 0x250C, "minimap top-left corner at (28,6)");      /* 40-12, 12-6 */
  ASSERT_INT(mb[6*40 + 39].ch, 0x2510, "minimap top-right corner");
  int dots = 0;
  for (int i = 0; i < 40 * 12; i++) { uint32_t c = mb[i].ch; if (c == 0x2022 || c == 0x25C9) dots++; }
  ASSERT(dots >= 2, "minimap shows >=2 node dots");
  free(mb); flow_free(m);

  /* background grid: off by default — no grid glyph on an empty graph */
  {
    flow_t *bg = flow_new(cols, rows); flow_register_defaults(bg);
    flow_render(bg, buf, cols, rows);
    int grid = 0;
    for (int i = 0; i < cols * rows; i++) { uint32_t c = buf[i].ch;
      if (c==0x00B7 || c==0x002B || c==0x2502 || c==0x2500 || c==0x253C) grid = 1; }
    ASSERT(!grid, "background grid off by default");
    flow_free(bg);
  }

  /* background DOTS: membership + light fg color, on a small empty buffer */
  {
    int bc = 12, br = 6;
    flow_cell *bb = (flow_cell*)malloc((size_t)bc * br * sizeof(flow_cell));
    flow_t *bg = flow_new(bc, br); flow_register_defaults(bg);
    flow_set_background(bg, FLOW_BG_DOTS, 3);
    flow_render(bg, bb, bc, br);
    ASSERT_INT(bb[0*bc + 0].ch, 0x00B7, "DOTS grid point at (0,0)");
    ASSERT_INT(bb[0*bc + 0].fg, FLOW_BG_GRID_FG, "DOTS grid uses light fg");
    ASSERT_INT(bb[0*bc + 1].ch, ' ', "DOTS off-grid cell (1,0) blank");
    /* scroll snapshots: grid pans with the canvas */
    char *bp0 = cells_to_string(bb, bc, br);
    SNAPSHOT("bg_dots_pan0", bp0); free(bp0);
    flow_pan(bg, 1, 0);
    flow_render(bg, bb, bc, br);
    char *bp1 = cells_to_string(bb, bc, br);
    SNAPSHOT("bg_dots_pan1", bp1); free(bp1);
    free(bb); flow_free(bg);
  }

  /* background LINES: vertical, horizontal, intersection glyphs */
  {
    int bc = 12, br = 6;
    flow_cell *bb = (flow_cell*)malloc((size_t)bc * br * sizeof(flow_cell));
    flow_t *bg = flow_new(bc, br); flow_register_defaults(bg);
    flow_set_background(bg, FLOW_BG_LINES, 3);
    flow_render(bg, bb, bc, br);
    ASSERT_INT(bb[3*bc + 3].ch, 0x253C, "LINES intersection at (3,3)");
    ASSERT_INT(bb[1*bc + 3].ch, 0x2502, "LINES vertical-only at (3,1)");
    ASSERT_INT(bb[3*bc + 1].ch, 0x2500, "LINES horizontal-only at (1,3)");
    free(bb); flow_free(bg);
  }

  /* background CROSS: '+' at grid points, blank off-grid */
  {
    int bc = 12, br = 6;
    flow_cell *bb = (flow_cell*)malloc((size_t)bc * br * sizeof(flow_cell));
    flow_t *bg = flow_new(bc, br); flow_register_defaults(bg);
    flow_set_background(bg, FLOW_BG_CROSS, 3);
    flow_render(bg, bb, bc, br);
    ASSERT_INT(bb[0*bc + 0].ch, 0x002B, "CROSS grid point at (0,0)");
    ASSERT_INT(bb[0*bc + 1].ch, ' ', "CROSS off-grid cell (1,0) blank");
    free(bb); flow_free(bg);
  }

  /* background gap clamp / UBSan safety: gap 0 clamps to 1, every cell is a grid point */
  {
    int bc = 12, br = 6;
    flow_cell *bb = (flow_cell*)malloc((size_t)bc * br * sizeof(flow_cell));
    flow_t *bg = flow_new(bc, br); flow_register_defaults(bg);
    flow_set_background(bg, FLOW_BG_DOTS, 0);
    flow_render(bg, bb, bc, br);  /* must not divide/modulo by zero */
    ASSERT_INT(bb[0*bc + 0].ch, 0x00B7, "gap-clamp: (0,0) is a grid point");
    ASSERT_INT(bb[0*bc + 1].ch, 0x00B7, "gap-clamp: (1,0) is a grid point");
    free(bb); flow_free(bg);
  }

  /* background layering: a node box overwrites the grid (background is under nodes) */
  {
    int bc = 12, br = 6;
    flow_cell *bb = (flow_cell*)malloc((size_t)bc * br * sizeof(flow_cell));
    flow_t *bg = flow_new(bc, br); flow_register_defaults(bg);
    flow_set_background(bg, FLOW_BG_DOTS, 3);
    flow_add_node(bg, "default", (flow_pt){0, 0}, (void*)"n");
    flow_render(bg, bb, bc, br);
    ASSERT_INT(bb[0*bc + 0].ch, 0x250C, "node box overwrites grid at (0,0)");
    free(bb); flow_free(bg);
  }

  /* minimap + background both on: the grid must NOT bleed through the opaque
     panel. Empty graph 40x12, BR minimap 12x6, DOTS gap 3. Cell (33,9) is a
     minimap interior cell that is not a border, viewport line, or node dot,
     and would be a grid dot (33%3==0 && 9%3==0) without the opaque fill. */
  {
    flow_cell *mb = (flow_cell*)malloc((size_t)40 * 12 * sizeof(flow_cell));
    flow_t *bg = flow_new(40, 12); flow_register_defaults(bg);
    flow_set_minimap(bg, 1, FLOW_CORNER_BR, 12, 6);
    flow_set_background(bg, FLOW_BG_DOTS, 3);
    flow_render(bg, mb, 40, 12);
    ASSERT_INT(mb[9*40 + 33].ch, ' ', "grid does not bleed through minimap interior");
    free(mb); flow_free(bg);
  }

  free(buf);
  return flowtest_report("test_render");
}
