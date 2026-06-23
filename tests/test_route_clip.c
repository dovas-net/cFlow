#define FLOW_IMPLEMENTATION
#include "../flow.h"
#include "flowtest.h"
#include <string.h>

/* inc-9 #3 — edge-router coordinate-DoS clip. A node at a hostile/large world coordinate
   (reachable via flow_load or flow_add_node) projects to a screen point billions of cells away;
   the orthogonal/straight routers then walk cell-by-cell across that span — billions of
   iterations + a multi-GB route realloc per edge per frame (a CPU/OOM DoS), and (s.x+t.x)/2 /
   (t.x-s.x) overflow int on the way. The render layer now clips each edge's projected endpoints
   to an expanded viewport rect (Liang-Barsky, clipping ALONG the line so slope is preserved)
   BEFORE routing — bounding the work to O(screen) while leaving on/near-screen edges
   byte-identical (endpoints are rewritten only when actually clipped). Applied identically at
   the draw / hit-test / toolbar route sites so render and hit-test never drift. */

int main(void) {
  /* 1: an on-screen segment is returned UNCHANGED (byte-identical) — the clip is a no-op, so
     every existing route and snapshot is unaffected. */
  {
    flow_pt s = {3, 4}, t = {20, 9};
    int draw = flow__clip_route_ends(80, 24, &s, &t);
    ASSERT_INT(draw, 1, "on-screen segment is drawn");
    ASSERT_INT(s.x, 3, "on-screen s.x unchanged");  ASSERT_INT(s.y, 4, "on-screen s.y unchanged");
    ASSERT_INT(t.x, 20, "on-screen t.x unchanged"); ASSERT_INT(t.y, 9, "on-screen t.y unchanged");
  }

  /* 2: a hostile far endpoint is clipped to near-screen, and routing the clipped segment yields
     a BOUNDED cell count (no billion-cell walk). The near (on-screen) end is left untouched. */
  {
    int cols = 80, rows = 24;
    flow_pt s = {5, 5}, t = {2000000000, 5};            /* t ~2e9 cells to the right */
    int draw = flow__clip_route_ends(cols, rows, &s, &t);
    ASSERT_INT(draw, 1, "edge with one on-screen end is still drawn");
    ASSERT_INT(s.x, 5, "near end x unchanged");  ASSERT_INT(s.y, 5, "near end y unchanged");
    ASSERT(t.x < 1000000, "far endpoint clipped to near-screen (down from 2e9)");
    ASSERT_INT(t.y, 5, "horizontal segment keeps its row");
    flow_route rt = {0};
    flow_route_orthogonal(s, (flow_pos){0}, t, (flow_pos){0}, &rt);
    ASSERT(rt.count > 0, "still produces a route");
    ASSERT(rt.count < 100000, "route is bounded after the clip (no DoS walk)");
    FLOW_FREE(rt.cells);
  }

  /* 3: a segment entirely outside the expanded rect is culled (return 0) — it would draw
     nothing on-screen anyway, so the wasted route is skipped. */
  {
    flow_pt s = {-1000000000, -5}, t = {-2000000000, -9};
    int draw = flow__clip_route_ends(80, 24, &s, &t);
    ASSERT_INT(draw, 0, "fully off-screen segment is culled");
  }

  /* 4: a STRAIGHT (diagonal) far segment keeps its slope through the clip (Liang-Barsky clips
     ALONG the line, not per-axis) — the clipped endpoint lies on the original line. */
  {
    int cols = 80, rows = 24;
    flow_pt s = {0, 0}, t = {2000000000, 1000000000};   /* slope exactly 1/2 */
    int draw = flow__clip_route_ends(cols, rows, &s, &t);
    ASSERT_INT(draw, 1, "diagonal far segment is drawn");
    ASSERT(t.x < 1000000, "diagonal far end clipped to near-screen");
    int expect_y = t.x / 2;                             /* on the line y = x/2 */
    ASSERT(t.y >= expect_y - 1 && t.y <= expect_y + 1, "clip preserved the diagonal slope");
  }

  /* 5: INTEGRATION — render a graph whose far node sits ~2e9 world cells away. Pre-clip this
     spun the router billions of times (hang/OOM); now flow_render COMPLETES, the on-screen node
     stays drawn/hittable, and the visible stub of the far edge is present and hittable. Reaching
     past flow_render at all is the DoS-gone proof. */
  {
    int cols = 80, rows = 24;
    flow_cell *buf = (flow_cell*)calloc((size_t)cols * rows, sizeof(flow_cell));
    flow_t *f = flow_new(cols, rows); flow_register_defaults(f);
    int a = flow_add_node(f, "default", (flow_pt){4, 4}, (void*)"A");   /* w=5,h=3 -> screen (4,4)-(8,6) */
    int b = flow_add_node(f, "default", (flow_pt){2000000000, 4}, (void*)"B");
    int e = flow_add_edge(f, a, b, "out", "in");
    flow_render(f, buf, cols, rows);                    /* must COMPLETE (bounded), not hang */
    ASSERT_INT(flow_hit_node(f, (flow_pt){5, 5}), a, "near node A is drawn/hittable after render");
    int found = -1;                                     /* the far edge's on-screen stub, somewhere right of A on its row */
    for (int x = 9; x < cols && found < 0; x++) found = flow_hit_edge(f, (flow_pt){x, 5}, 1);
    ASSERT_INT(found, e, "a visible, hittable stub of the far edge is drawn on-screen");
    free(buf); flow_free(f); (void)b;
  }

  /* 6: the clip must bound to the RENDER TARGET (the cols x rows passed to flow_render), not the
     engine's f->cols/f->rows. A host may render into a buffer larger than the engine size; a
     far-but-in-buffer edge must reach its node, not get clipped to the stale engine viewport.
     flow_new(80,24) rendered into 2000 cols: the edge to a node at x=1800 must draw its wire past
     the old engine-clip boundary (~x1520 = 80 + (80+24)*4+1024). */
  {
    int ecols = 2000, erows = 24;
    flow_cell *buf = (flow_cell*)calloc((size_t)ecols * erows, sizeof(flow_cell));
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);          /* engine size 80x24 */
    int a = flow_add_node(f, "default", (flow_pt){10, 10},   (void*)"A");
    int b = flow_add_node(f, "default", (flow_pt){1800, 10}, (void*)"B");
    flow_add_edge(f, a, b, "out", "in");
    flow_render(f, buf, ecols, erows);                               /* render into the LARGE buffer */
    int wire = 0;                                                    /* edge wire BETWEEN the old engine-clip bound and B */
    for (int y = 10; y <= 12 && !wire; y++)
      for (int x = 1530; x < 1798; x++) { uint32_t ch = buf[y * ecols + x].ch; if (ch >= 0x2500 && ch <= 0x257F) { wire = 1; break; } }
    ASSERT(wire, "far edge clips to the render target (cols x rows), reaching the node at x=1800");
    free(buf); flow_free(f); (void)a; (void)b;
  }

  return flowtest_report("test_route_clip");
}
