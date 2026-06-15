#define FLOW_IMPLEMENTATION
#include "../flow.h"
#include "flowtest.h"

/* inc-6 #5 animated-edges: marching-ants on FLOW_ANIMATED edges. The dash phase is keyed
   on PATH-CELL INDEX ((c + f->tick) % FLOW_DASH_PERIOD) so it marches on both the orthogonal
   and straight routers; off-phase cells are SKIPPED (the gap shows the backdrop), the
   arrowhead (last cell) and the label are exempt, and LOD 1 draws solid. Tick is read
   directly from f->tick — render stays a pure function of (model, view, tick); no wall-clock. */

/* render idiom borrowed verbatim from tests/test_render.c:5-15 */
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

/* arrowhead count: the four directional glyphs both routers stamp on the target cell */
static int arrowheads(const flow_cell *c, int n) {
  int k = 0; for (int i = 0; i < n; i++) { uint32_t ch = c[i].ch;
    if (ch == 0x25B6 || ch == 0x25C0 || ch == 0x25BC || ch == 0x25B2) k++; }
  return k;
}

int main(void) {
  int cols = 40, rows = 8;  /* A and B on the SAME world row → a long horizontal router run */

  /* 1. Setter sets/clears the flag (compile-RED on the new symbol); unknown id is a no-op. */
  {
    flow_t *f = flow_new(cols, rows); flow_register_defaults(f);
    int a = flow_add_node(f, "default", (flow_pt){0, 3}, (void*)"A");
    int b = flow_add_node(f, "default", (flow_pt){25, 3}, (void*)"B");
    int e = flow_add_edge(f, a, b, "", "");
    flow_set_edge_animated(f, e, 1);
    ASSERT(flow_get_edge(f, e)->flags & FLOW_ANIMATED, "setter sets FLOW_ANIMATED");
    flow_set_edge_animated(f, e, 0);
    ASSERT(!(flow_get_edge(f, e)->flags & FLOW_ANIMATED), "setter clears FLOW_ANIMATED");
    flow_set_edge_animated(f, 99999, 1);    /* unknown id: silent no-op, no crash */
    ASSERT(!(flow_get_edge(f, e)->flags & FLOW_ANIMATED), "unknown id changes nothing");
    flow_free(f);
  }

  /* 2. Tick-0 golden — the static baseline (even-index path cells drawn). */
  {
    flow_cell *buf = (flow_cell*)malloc((size_t)cols * rows * sizeof(flow_cell));
    flow_t *f = flow_new(cols, rows); flow_register_defaults(f);
    int a = flow_add_node(f, "default", (flow_pt){0, 3}, (void*)"A");
    int b = flow_add_node(f, "default", (flow_pt){25, 3}, (void*)"B");
    int e = flow_add_edge(f, a, b, "", "");
    flow_set_edge_animated(f, e, 1);
    f->tick = 0;
    flow_render(f, buf, cols, rows);
    char *s = cells_to_string(buf, cols, rows);
    SNAPSHOT("animated_tick0", s);          /* auto-mints on first GREEN run — verify dashed + solid arrowhead */
    free(s); free(buf); flow_free(f);
  }

  /* 3. Tick-k discriminator: a PATH cell DIFFERS at tick 1 vs tick 0 (ants march). */
  {
    flow_cell *b0 = (flow_cell*)malloc((size_t)cols * rows * sizeof(flow_cell));
    flow_cell *b1 = (flow_cell*)malloc((size_t)cols * rows * sizeof(flow_cell));
    flow_t *f = flow_new(cols, rows); flow_register_defaults(f);
    int a = flow_add_node(f, "default", (flow_pt){0, 3}, (void*)"A");
    int b = flow_add_node(f, "default", (flow_pt){25, 3}, (void*)"B");
    int e = flow_add_edge(f, a, b, "", "");
    flow_set_edge_animated(f, e, 1);
    f->tick = 0; flow_render(f, b0, cols, rows);
    f->tick = 1; flow_render(f, b1, cols, rows);
    int diffs = 0, toggles = 0;
    for (int i = 0; i < cols * rows; i++) {
      uint32_t c0 = b0[i].ch ? b0[i].ch : ' ', c1 = b1[i].ch ? b1[i].ch : ' ';
      if (c0 != c1) diffs++;                                          /* structural: only animated path cells vary across ticks */
      if ((c0 == 0x2500 && c1 == ' ') || (c0 == ' ' && c1 == 0x2500)) toggles++;
    }
    ASSERT(diffs > 0, "an animated path cell differs across ticks (layout-independent proof)");
    ASSERT(toggles > 0, "a horizontal-run cell toggles glyph<->gap across ticks (ants march)");
    free(b0); free(b1); flow_free(f);
  }

  /* 4. Arrowhead and label survive every phase. */
  {
    flow_cell *buf = (flow_cell*)malloc((size_t)cols * rows * sizeof(flow_cell));
    flow_t *f = flow_new(cols, rows); flow_register_defaults(f);
    int a = flow_add_node(f, "default", (flow_pt){0, 3}, (void*)"A");
    int b = flow_add_node(f, "default", (flow_pt){25, 3}, (void*)"B");
    int e = flow_add_edge(f, a, b, "", "");
    flow_set_edge_animated(f, e, 1);
    flow_set_edge_label(f, e, "X");
    for (unsigned t = 0; t < 2; t++) {
      f->tick = t; flow_render(f, buf, cols, rows);
      char *s = cells_to_string(buf, cols, rows);
      ASSERT_INT(arrowheads(buf, cols * rows), 1, "exactly one arrowhead survives every phase");
      ASSERT(strchr(s, 'X') != NULL, "edge label survives every phase");
      free(s);
    }
    free(buf); flow_free(f);
  }

  /* 5. LOD-1 suppression: animation is tick-invariant when zoomed out (draws solid). */
  {
    flow_cell *buf = (flow_cell*)malloc((size_t)cols * rows * sizeof(flow_cell));
    flow_t *f = flow_new(cols, rows); flow_register_defaults(f);
    int a = flow_add_node(f, "default", (flow_pt){0, 3}, (void*)"A");
    int b = flow_add_node(f, "default", (flow_pt){25, 3}, (void*)"B");
    int e = flow_add_edge(f, a, b, "", "");
    flow_set_edge_animated(f, e, 1);
    flow_set_zoom(f, 0.5f, (flow_pt){ cols / 2, rows / 2 });          /* below FLOW_LOD_THRESHOLD 0.6 → lod 1 */
    ASSERT(flow_zoom(f) < 0.6f, "zoom is below the LOD threshold");
    f->tick = 0; flow_render(f, buf, cols, rows); char *s0 = cells_to_string(buf, cols, rows);
    f->tick = 1; flow_render(f, buf, cols, rows); char *s1 = cells_to_string(buf, cols, rows);
    ASSERT_STR(s1, s0, "LOD 1 draws solid: render is tick-invariant when zoomed out");
    free(s0); free(s1); free(buf); flow_free(f);
  }

  /* 6. Non-animated edge is byte-identical across ticks (the goldens-invariance proof). */
  {
    flow_cell *buf = (flow_cell*)malloc((size_t)cols * rows * sizeof(flow_cell));
    flow_t *f = flow_new(cols, rows); flow_register_defaults(f);
    int a = flow_add_node(f, "default", (flow_pt){0, 3}, (void*)"A");
    int b = flow_add_node(f, "default", (flow_pt){25, 3}, (void*)"B");
    flow_add_edge(f, a, b, "", "");                                  /* NOT animated */
    f->tick = 0; flow_render(f, buf, cols, rows); char *s0 = cells_to_string(buf, cols, rows);
    f->tick = 7; flow_render(f, buf, cols, rows); char *s7 = cells_to_string(buf, cols, rows);
    ASSERT_STR(s7, s0, "non-animated edge renders identically across ticks (no golden can move)");
    free(s0); free(s7); free(buf); flow_free(f);
  }

  /* 7. Animation does NOT persist through save/load (OQ6 resolved-false lock). */
  {
    flow_t *f = flow_new(cols, rows); flow_register_defaults(f);
    int a = flow_add_node(f, "default", (flow_pt){0, 3}, (void*)"A");
    int b = flow_add_node(f, "default", (flow_pt){25, 3}, (void*)"B");
    int e = flow_add_edge(f, a, b, "", "");
    flow_set_edge_animated(f, e, 1);
    const char *path = "/tmp/flow_animated_test.json";
    ASSERT_INT(flow_save(f, path), 0, "save ok");
    flow_t *g = flow_new(cols, rows); flow_register_defaults(g);
    ASSERT_INT(flow_load(g, path), 0, "load ok");
    ASSERT_INT(flow_edge_count(g), 1, "one edge reloaded");
    ASSERT(!(flow_edges(g)[0].flags & FLOW_ANIMATED), "FLOW_ANIMATED dropped on save/load (flags not persisted)");
    flow_free(f); flow_free(g);
  }

  return flowtest_report("test_animated");
}
