/* test_embed — the headless / host-owned-loop contract (H3):
   flow_render_diff is a pure embed primitive (no stdout/termios), it returns an
   idempotent empty diff when nothing changed, flow_feed mutates the model with
   no terminal, and the wrapper is byte-identical to a manual render+diff. */
#define FLOW_IMPLEMENTATION
#include "../flow.h"
#include "flowtest.h"

static flow_t *scene(void) {
  flow_t *f = flow_new(40, 12); flow_register_defaults(f);
  int a = flow_add_node(f, "default", (flow_pt){ 2, 2}, (void*)"A");
  int b = flow_add_node(f, "default", (flow_pt){20, 6}, (void*)"B");
  flow_add_edge(f, a, b, "out", "in");
  return f;
}

int main(void) {
  /* (1) first frame is the whole scene; an unchanged second frame is empty. */
  {
    flow_t *f = scene();
    char *first = flow_render_diff(f);
    ASSERT(strlen(first) > 0, "first flow_render_diff emits a non-empty frame");
    free(first);
    char *again = flow_render_diff(f);                 /* model untouched -> no diff */
    ASSERT(again != NULL, "flow_render_diff returns non-NULL even when empty");
    ASSERT_INT((int)strlen(again), 0, "unchanged frame produces an empty diff (front advanced)");
    free(again);
    flow_free(f);
  }

  /* (2) flow_feed drives the model headlessly — no TTY, no flow_run. Input bytes
     are the same a terminal would send; state changes are observable + redraw. */
  {
    flow_t *f = scene();
    free(flow_render_diff(f));                         /* prime the front buffer */

    /* a mouse click is also just bytes: SGR press+release inside node A (world
       (2,2), default view) selects it — done first, before zoom/pan move it. */
    flow_feed(f, "\x1b[<0;4;4M", 9);
    flow_feed(f, "\x1b[<0;4;4m", 9);
    ASSERT(flow_selected_count(f) >= 1, "flow_feed(SGR mouse) selects a node headlessly");

    float z0 = flow_zoom(f);
    flow_feed(f, "+", 1);                              /* keyboard zoom-in */
    ASSERT(flow_zoom(f) > z0, "flow_feed('+') zooms the model with no terminal");

    flow_viewport v0 = flow_view_get(f);
    flow_feed(f, "\x1b[C", 3);                         /* right-arrow pan */
    flow_viewport v1 = flow_view_get(f);
    ASSERT(v0.ox != v1.ox || v0.oy != v1.oy, "flow_feed(arrow) pans the viewport headlessly");

    char *diff = flow_render_diff(f);                  /* the moves produce a real frame */
    ASSERT(strlen(diff) > 0, "post-input frame is a non-empty diff");
    free(diff);
    flow_free(f);
  }

  /* (3) flow_render_diff == a manual flow_render + flow_diff_emit on the same
     start state: the wrapper packages exactly the documented primitive, and the
     front-buffer advance is identical (flow_present rides on this). */
  {
    flow_t *a = scene();
    flow_t *b = scene();                               /* identical sibling */
    char *wrapped = flow_render_diff(a);

    flow_cell *back = (flow_cell*)calloc((size_t)b->cols * b->rows, sizeof(flow_cell));
    flow_render(b, back, b->cols, b->rows);
    char *manual = flow_diff_emit(b->front, back, b->cols, b->rows);

    ASSERT_STR(wrapped, manual, "flow_render_diff equals manual render+diff_emit");
    free(back); free(manual); free(wrapped);
    flow_free(a); flow_free(b);
  }

  return flowtest_report("test_embed");
}
