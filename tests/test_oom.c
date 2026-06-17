/* test_oom — out-of-memory handling (H8). A fail-after-N allocator drives the
   documented OOM contract: flow_new -> NULL (no deref, no leak), flow_add_node ->
   -1, and — the part an embedder actually cares about — the failure is RECOVERABLE,
   not a one-way wedge: the graph is unchanged and the next op succeeds. */
#define FLOW_IMPLEMENTATION
#include <stdlib.h>
static int  g_fail_at = -1;          /* -1 = never fail; else fail once the counter reaches it */
static long g_alloc_n = 0;
static int oom(void) { if (g_fail_at >= 0 && g_alloc_n >= g_fail_at) return 1; g_alloc_n++; return 0; }
static void *o_malloc(size_t n)           { return oom() ? NULL : malloc(n); }
static void *o_calloc(size_t a, size_t b) { return oom() ? NULL : calloc(a, b); }
static void *o_realloc(void *p, size_t n) { return oom() ? NULL : realloc(p, n); }
#define FLOW_MALLOC(n)      o_malloc(n)
#define FLOW_CALLOC(a, b)   o_calloc(a, b)
#define FLOW_REALLOC(p, n)  o_realloc(p, n)
#define FLOW_FREE(p)        free(p)
#include "../flow.h"
#include "flowtest.h"

int main(void) {
  /* (1) flow_new -> NULL when the flow_t calloc itself fails (no NULL deref). */
  g_alloc_n = 0; g_fail_at = 0;
  ASSERT(flow_new(40, 12) == NULL, "flow_new -> NULL when the flow_t calloc fails");

  /* (2) flow_new -> NULL when the FRONT-buffer calloc fails (2nd alloc); f is freed
     (ASan/UBSan in the gate proves no leak from the partial construction). */
  g_alloc_n = 0; g_fail_at = 1;
  ASSERT(flow_new(40, 12) == NULL, "flow_new -> NULL + frees f when front-buffer calloc fails");

  /* (3) flow_add_node -> -1 on OOM, graph unchanged, and recoverable. */
  g_fail_at = -1;                                  /* disarm: build a real graph */
  flow_t *f = flow_new(40, 12);
  ASSERT(f != NULL, "flow_new succeeds with the injector disarmed");
  flow_register_defaults(f);
  for (int i = 0; i < 8; i++)                       /* fill the node array to cap 8 exactly */
    flow_add_node(f, "default", (flow_pt){ (i % 4) * 9, (i / 4) * 5 }, (void*)"n");
  int before = flow_node_count(f);
  ASSERT_INT(before, 8, "8 nodes seeded (array at cap)");

  g_alloc_n = 0; g_fail_at = 0;                     /* arm: the 9th add's node-array realloc is the next alloc */
  int r = flow_add_node(f, "default", (flow_pt){ 30, 8 }, (void*)"x");
  g_fail_at = -1;                                   /* disarm immediately */
  ASSERT_INT(r, -1, "flow_add_node -> -1 when the node-array grow fails");
  ASSERT_INT(flow_node_count(f), before, "failed add leaves node_count unchanged (no half-insert)");

  int r2 = flow_add_node(f, "default", (flow_pt){ 30, 8 }, (void*)"y");   /* recover */
  ASSERT(r2 >= 0, "add succeeds again once memory is available (not a one-way wedge)");
  ASSERT_INT(flow_node_count(f), before + 1, "recovered add grows the graph");
  char *frame = flow_render_diff(f);
  ASSERT(frame && strlen(frame) > 0, "flow_render_diff still produces a frame after OOM recovery");
  free(frame);
  flow_free(f);

  /* (4) flow_undo grows the redo stack; on OOM it must drop the history entry rather
     than deref NULL. The inverse op is still applied (graph consistent); ASan/UBSan in
     the gate proves the dropped command is freed (no leak). */
  {
    flow_t *g = flow_new(40, 12);
    ASSERT(g != NULL, "flow_new for undo-OOM case");
    flow_register_defaults(g);
    int id = flow_add_node(g, "default", (flow_pt){ 2, 2 }, (void*)"u");
    ASSERT(id >= 0 && flow_node_count(g) == 1, "seeded one undoable add");

    g_alloc_n = 0; g_fail_at = 0;                  /* fail the redo-stack grow (0->8) inside flow_undo */
    flow_undo(g);                                  /* applies the inverse, then the redo grow fails */
    g_fail_at = -1;
    ASSERT_INT(flow_node_count(g), 0, "undo applied (node removed) despite redo-grow OOM, no crash");
    flow_free(g);
  }

  /* (5) edge routing under OOM (v0.3 #2): flow_route_orthogonal/straight grow their
     cell + path-point arrays via FLOW_REALLOC mid-route. A failing realloc must DROP the
     cell/point and keep the old buffer (no realloc-into-self: the bug overwrote the live
     pointer with NULL, leaking it and NULL-deref'ing the next write), and the target-end
     arrowhead fix-ups must never touch cells[-1] when every push was OOM-dropped. Drive a
     long (~Manhattan-60) path and fail at EVERY allocation boundary; the structural
     invariant (count<=cap, cells!=NULL unless empty) is what realloc-into-self violated,
     and ASan/UBSan in the gate proves no OOB write / no leak on each truncated route. */
  {
    flow_pt s = { 0, 0 }, t = { 40, 20 };            /* long path: both the cell and point arrays grow several times */
    int ok = 1;
    for (int fail_at = 0; fail_at <= 12; fail_at++) { /* > the handful of reallocs either router makes, so every boundary + the all-succeed case is covered */
      flow_route ro = { 0 };
      g_alloc_n = 0; g_fail_at = fail_at;
      flow_route_orthogonal(s, FLOW_RIGHT, t, FLOW_LEFT, &ro);
      g_fail_at = -1;
      if (ro.count > ro.cap || (ro.cells == NULL && ro.count != 0)) ok = 0;
      free(ro.cells);

      flow_route rs = { 0 };
      g_alloc_n = 0; g_fail_at = fail_at;
      flow_route_straight(s, FLOW_RIGHT, t, FLOW_LEFT, &rs);
      g_fail_at = -1;
      if (rs.count > rs.cap || (rs.cells == NULL && rs.count != 0)) ok = 0;
      free(rs.cells);
    }
    ASSERT(ok, "route_orthogonal/straight survive OOM at every alloc step (count<=cap, cells consistent, no cells[-1])");
  }

  return flowtest_report("test_oom");
}
