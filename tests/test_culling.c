#define FLOW_IMPLEMENTATION
#include "../flow.h"
#include "flowtest.h"
#include <string.h>

/* Render-only viewport culling (inc-5 #9): the node render loop skips a node
   whose SCREEN FOOTPRINT (flow__node_footprint — the exact rect flow_hit_node
   tests) misses {0,0,cols,rows}. Screen space, not world space: footprints are
   constant-size (only position scales with zoom), so a world-rect test would
   under-cover the drawn area at zoom<1. NODES only — edges keep routing (a
   connection between two offscreen nodes still draws its on-screen segment).
   Never inside flow__node_visible/flow__edge_visible (hit/marquee/bounds/
   minimap share those). Intersect-not-contain: straddling nodes keep drawing
   their sliver via the per-cell clamps. */

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

/* counting node type: the RED discriminator — without the cull, an offscreen
   node's render() is dispatched every frame; with it, never. */
static int ct_renders = 0;
static void ct_measure(const flow_node *n, int *w, int *h) { (void)n; *w = 5; *h = 3; }
static void ct_render(const flow_node *n, flow_surface *s, flow_render_ctx ctx) {
  (void)n; (void)ctx; ct_renders++;
  flow_box(s, 0, 0, 5, 3, FLOW_FG, FLOW_BG, 0);
}
static const flow_node_type COUNTER = { "counter", ct_measure, ct_render, NULL, 0, NULL, NULL };

int main(void) {
  int cols = 30, rows = 8;
  flow_cell *buf = (flow_cell*)malloc((size_t)cols * rows * sizeof(flow_cell));

  /* ---- 0 (RED-bearing): offscreen render() is NOT dispatched ---- */
  {
    flow_t *f = flow_new(cols, rows); flow_register_defaults(f);
    flow_register_node_type(f, &COUNTER);
    flow_add_node(f, "counter", (flow_pt){2, 2},       (void*)"on");
    flow_add_node(f, "counter", (flow_pt){5000, 5000}, (void*)"off");
    ct_renders = 0;
    flow_render(f, buf, cols, rows);
    ASSERT_INT(ct_renders, 1, "offscreen node's render() not dispatched (cull); onscreen one is");
    flow_free(f);
  }

  /* ---- 1+2: offscreen node = zero cells, but fully model-queryable ---- */
  {
    flow_t *f = flow_new(cols, rows); flow_register_defaults(f);
    int a = flow_add_node(f, "default", (flow_pt){0, 0},       (void*)"A");
    int b = flow_add_node(f, "default", (flow_pt){5000, 5000}, (void*)"B");
    (void)a;
    flow_render(f, buf, cols, rows);
    char *s = cells_to_string(buf, cols, rows);
    ASSERT(strchr(s, 'A') != NULL, "onscreen node renders");
    ASSERT(strchr(s, 'B') == NULL, "offscreen node contributes zero cells");
    free(s);
    /* model layer still sees it: count, bounds, and (after framing) hit-test */
    ASSERT(flow_get_node(f, b) != NULL, "culled node still in the model");
    ASSERT_INT(flow_node_count(f), 2, "  count includes it");
    flow_rect wb = flow_bounds(f);
    ASSERT(wb.x + wb.w >= 5005, "flow_bounds still spans the culled node (model-level)");
    flow_set_center(f, 5002, 5001, -1.0f);          /* frame B (inc-5 #4) */
    flow_render(f, buf, cols, rows);                /* re-render at the new view */
    char *s2 = cells_to_string(buf, cols, rows);
    ASSERT(strchr(s2, 'B') != NULL, "framed: the node renders again (cull is per-frame)");
    free(s2);
    flow_pt bs = flow_to_screen(f, (flow_pt){5002, 5001});
    ASSERT_INT(flow_hit_node(f, bs), b, "hit-test sees it (choke points untouched)");
    flow_free(f);
  }

  /* ---- 3: straddling node draws its on-screen sliver ---- */
  {
    flow_t *f = flow_new(cols, rows); flow_register_defaults(f);
    flow_add_node(f, "default", (flow_pt){-2, 1}, (void*)"S");  /* 5x3: screen x -2..2 */
    flow_render(f, buf, cols, rows);
    ASSERT_INT((int)buf[2*cols + 0].ch, (int)'S', "label cell at screen (0,2) survives the cull");
    ASSERT_INT((int)buf[1*cols + 2].ch, 0x2510, "right border corner at (2,1) drawn");
    flow_free(f);
  }

  /* ---- 4: viewport-crossing edge between two OFFSCREEN nodes still renders ---- */
  {
    flow_t *f = flow_new(cols, rows); flow_register_defaults(f);
    int L = flow_add_node(f, "default", (flow_pt){-50, 2}, (void*)"L");
    int R = flow_add_node(f, "default", (flow_pt){60, 2},  (void*)"R");
    flow_add_edge(f, L, R, "out", "in");
    flow_render(f, buf, cols, rows);
    char *s = cells_to_string(buf, cols, rows);
    ASSERT(strchr(s, 'L') == NULL && strchr(s, 'R') == NULL, "both endpoints culled");
    ASSERT(strstr(s, "\xe2\x94\x80") != NULL, "the through-line still draws (edges not endpoint-culled)");
    SNAPSHOT("cull_crossing_edge", s);
    free(s);
    flow_free(f);
  }

  /* ---- 5: fractional-zoom regression (the world-rect trap) ---- */
  {
    flow_t *f = flow_new(cols, rows); flow_register_defaults(f);
    /* 6-char label -> w=10. At zoom .75 (>= LOD threshold .6, full render),
       anchor (0,0) keeps ox=0: world x=-12 projects to screen x=-9, so the
       footprint spans screen -9..0 — the right border lands ON col 0 while the
       node's WORLD rect [-12,-2] misses the world viewport [0,40] entirely.
       A world-rect cull would wrongly drop it; the screen-footprint cull keeps it. */
    flow_add_node(f, "default", (flow_pt){-12, 2}, (void*)"BBBBBB");
    flow_set_zoom(f, 0.75f, (flow_pt){0, 0});
    flow_render(f, buf, cols, rows);
    /* world y=2 -> screen y=2 (lroundf(1.5)=2); box rows 2..4; right border col 0 */
    ASSERT_INT((int)buf[3*cols + 0].ch, 0x2502, "z<1: footprint pokes on-screen, not culled");
    flow_free(f);
  }

  free(buf);
  return flowtest_report("test_culling");
}
