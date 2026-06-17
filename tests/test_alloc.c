/* test_alloc — FLOW_MALLOC/CALLOC/REALLOC/FREE indirection (H7). Override the
   allocator set with counting wrappers BEFORE including flow.h and prove a normal
   workload routes every allocation kind through the macros (the embeddability
   contract: an arena/tracker can intercept all of flow's heap traffic). */
#define FLOW_IMPLEMENTATION
#include <stdlib.h>
static long g_malloc = 0, g_calloc = 0, g_realloc = 0, g_free = 0, g_live = 0;
static void *cnt_malloc(size_t n)            { g_malloc++;  g_live++; return malloc(n); }
static void *cnt_calloc(size_t a, size_t b)  { g_calloc++;  g_live++; return calloc(a, b); }
static void *cnt_realloc(void *p, size_t n)  { g_realloc++; if (!p) g_live++; return realloc(p, n); }
static void  cnt_free(void *p)               { if (p) { g_free++; g_live--; } free(p); }
#define FLOW_MALLOC(n)      cnt_malloc(n)
#define FLOW_CALLOC(a, b)   cnt_calloc(a, b)
#define FLOW_REALLOC(p, n)  cnt_realloc(p, n)
#define FLOW_FREE(p)        cnt_free(p)
#include "../flow.h"
#include "flowtest.h"

int main(void) {
  flow_t *f = flow_new(60, 20);                 /* calloc: flow_t + front buffer */
  ASSERT(f != NULL, "flow_new");
  flow_register_defaults(f);
  /* >8 nodes forces the node array (cap 8) + the undo journal to grow -> realloc */
  int ids[12];
  for (int i = 0; i < 12; i++)
    ids[i] = flow_add_node(f, "default", (flow_pt){ (i % 4) * 12, (i / 4) * 5 }, (void*)"n");
  for (int i = 1; i < 12; i++) flow_add_edge(f, ids[i - 1], ids[i], "out", "in");
  int e = flow_add_edge(f, ids[0], ids[5], "out", "in");
  flow_set_edge_label(f, e, "label");           /* malloc: label dup */

  char *frame = flow_render_diff(f);            /* malloc/calloc: render back buffer + diff string */
  ASSERT(frame != NULL, "render_diff");
  FLOW_FREE(frame);                             /* free the flow-allocated string with the same allocator */

  flow_free(f);                                 /* frees everything flow owns */

  ASSERT(g_calloc  > 0, "flow routes calloc through FLOW_CALLOC");
  ASSERT(g_malloc  > 0, "flow routes malloc through FLOW_MALLOC");
  ASSERT(g_realloc > 0, "flow routes realloc through FLOW_REALLOC (array/journal growth)");
  ASSERT(g_free    > 0, "flow routes free through FLOW_FREE");
  /* every counted allocation was released through the same hooks (no raw libc free
     bypassing FLOW_FREE, no raw libc malloc bypassing the counters) */
  ASSERT_INT((int)g_live, 0, "all flow allocations released via FLOW_FREE (balanced)");

  printf("  [alloc routed: %ld malloc, %ld calloc, %ld realloc, %ld free]\n",
         g_malloc, g_calloc, g_realloc, g_free);
  return flowtest_report("test_alloc");
}
