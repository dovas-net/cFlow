#define FLOW_IMPLEMENTATION
#include "../flow.h"
#include "flowtest.h"

/* inc-6 #4 redraw-clock: deterministic integer tick clock behind a unit-testable seam.
   flow_run advances `tick` on each poll() timeout while frames are armed; tests call
   flow_tick directly. THE DETERMINISM RULE: render is a pure function of (model, view,
   tick) and wall-clock never enters the model/render layers. */

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

int main(void) {
  /* 1. Tick advances deterministically (the seam) — no IO. */
  {
    flow_t *f = flow_new(30, 8);
    ASSERT_INT(flow_ticks(f), 0, "tick starts at 0");        /* calloc-zero */
    flow_tick(f); flow_tick(f); flow_tick(f);
    ASSERT_INT(flow_ticks(f), 3, "three ticks advance to 3");
    f->tick = 41;                                            /* poke internal */
    ASSERT_INT(flow_ticks(f), 41, "getter is a thin read of f->tick");
    flow_free(f);
  }

  /* 2. flow_tick is a pure counter-advance: no render, no block, f->front untouched. */
  {
    flow_t *f = flow_new(30, 8);
    size_t fb = (size_t)f->cols * f->rows * sizeof(flow_cell);
    flow_cell *snap = (flow_cell*)malloc(fb);
    memcpy(snap, f->front, fb);                              /* copy front buffer */
    for (int i = 0; i < 1000; i++) flow_tick(f);
    ASSERT_INT(flow_ticks(f), 1000, "1000 ticks advance to 1000");
    ASSERT(memcmp(snap, f->front, fb) == 0, "flow_tick never touches f->front (no hidden present)");
    free(snap); flow_free(f);
  }

  /* 3. tick_ms default + setter clamp (never 0 → never busy-spin). */
  {
    flow_t *f = flow_new(30, 8);
    ASSERT_INT(f->tick_ms, 100, "default interval is 100ms");
    flow_set_tick_ms(f, 250); ASSERT_INT(f->tick_ms, 250, "setter stores ms");
    flow_set_tick_ms(f, 0);   ASSERT_INT(f->tick_ms, 1,   "0 clamps to 1 (never busy-spin)");
    flow_set_tick_ms(f, -5);  ASSERT_INT(f->tick_ms, 1,   "negative clamps to 1");
    flow_free(f);
  }

  /* 4. Arming predicate: OFF with a plain edge; ON once an edge is FLOW_ANIMATED
     (inc-6 #5 added the first || clause). #8 later ORs in the in-flight-drag clause. */
  {
    flow_t *f = flow_new(30, 8); flow_register_defaults(f);
    int a = flow_add_node(f, "default", (flow_pt){0, 0}, (void*)"A");
    int b = flow_add_node(f, "default", (flow_pt){15, 4}, (void*)"B");
    int e = flow_add_edge(f, a, b, "", "");
    ASSERT_INT(flow__frames_armed(f), 0, "plain edge arms nothing (zero idle wakeups)");
    flow_set_edge_animated(f, e, 1);
    ASSERT_INT(flow__frames_armed(f), 1, "an animated edge arms the redraw clock");
    flow_set_edge_animated(f, e, 0);
    ASSERT_INT(flow__frames_armed(f), 0, "clearing animation disarms");
    flow_free(f);
  }

  /* 5. Render is a pure function of (model, view, tick) — determinism golden. */
  {
    int cols = 30, rows = 8;
    flow_cell *buf = (flow_cell*)malloc((size_t)cols * rows * sizeof(flow_cell));
    flow_t *f = flow_new(cols, rows); flow_register_defaults(f);
    flow_add_node(f, "default", (flow_pt){0, 0}, (void*)"A");
    flow_render(f, buf, cols, rows);
    char *s0 = cells_to_string(buf, cols, rows);
    SNAPSHOT("clock_tick0", s0);                             /* auto-mints on first GREEN run */
    for (int i = 0; i < 7; i++) flow_tick(f);
    flow_render(f, buf, cols, rows);
    char *s7 = cells_to_string(buf, cols, rows);
    ASSERT_STR(s7, s0, "render unchanged by tick when nothing animates");
    free(s0); free(s7); free(buf); flow_free(f);
  }

  return flowtest_report("test_clock");
}
