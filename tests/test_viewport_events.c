#define FLOW_IMPLEMENTATION
#include "../flow.h"
#include "flowtest.h"
#include <math.h>

/* on_viewport_change (work package inc-4 #5): every viewport mutation (pan, zoom,
   fit, load-restore, extent re-clamp) routes through the internal flow__view_set
   seam, which clamps FIRST (translate extent) and fires the callback ONLY when the
   final viewport differs from the prior one. No journaling (spec §11: viewport is
   never undo state). Re-entrancy: the seam has no depth guard; the test below covers
   the CONVERGING case (callback mutates once, then stops). A callback that mutates
   by a nonzero delta on every fire would recurse unboundedly — that is an app bug,
   not engine-covered behavior. */

static int vp_fires = 0;
static flow_viewport vp_last;
static flow_t *vp_recurse_target = NULL;   /* when set, the callback pans once more */
static int vp_recursed = 0;

static void on_vp(flow_t *f, flow_viewport vp, void *u) {
  (void)f; (void)u;
  vp_fires++; vp_last = vp;
  if (vp_recurse_target && !vp_recursed) { vp_recursed = 1; flow_pan(vp_recurse_target, 1, 1); }
}

#define ASSERT_F(got, want, msg) ASSERT(fabsf((float)(got) - (float)(want)) < 0.01f, msg)

int main(void) {
  /* ---- flow_pan fires once per actual change; no-op pan fires nothing ---- */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    flow_callbacks cb = {0}; cb.on_viewport_change = on_vp; flow_set_callbacks(f, cb);
    vp_fires = 0;
    flow_pan(f, 5, 0);
    ASSERT_INT(vp_fires, 1, "pan fires once");
    ASSERT_F(vp_last.ox, 5.0f, "callback receives the new ox");
    flow_pan(f, 0, 0);
    ASSERT_INT(vp_fires, 1, "no-op pan (0,0) fires nothing");
    flow_pan(f, 0, -3);
    ASSERT_INT(vp_fires, 2, "each changing pan fires independently (no coalescing)");
    ASSERT_F(vp_last.oy, -3.0f, "y delta delivered");
    flow_free(f);
  }

  /* ---- flow_set_zoom fires on change, not on a same-value re-set ---- */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    flow_callbacks cb = {0}; cb.on_viewport_change = on_vp; flow_set_callbacks(f, cb);
    vp_fires = 0;
    flow_zoom_in(f, (flow_pt){40, 12});
    ASSERT_INT(vp_fires, 1, "zoom_in fires once");
    ASSERT(vp_last.zoom > 1.0f, "callback sees the new zoom");
    flow_zoom_in(f, (flow_pt){40, 12});
    ASSERT_INT(vp_fires, 2, "second zoom_in fires again");
    flow_free(f);
  }
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    flow_callbacks cb = {0}; cb.on_viewport_change = on_vp; flow_set_callbacks(f, cb);
    vp_fires = 0;
    flow_set_zoom(f, 1.0f, (flow_pt){40, 12});   /* zoom already 1.0; anchor math exact at z=1 */
    ASSERT_INT(vp_fires, 0, "re-setting the same zoom fires nothing");
    flow_free(f);
  }

  /* ---- flow_fit_view fires once with the fitted viewport ---- */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    flow_add_node(f, "default", (flow_pt){10, 5},  (void*)"A");
    flow_add_node(f, "default", (flow_pt){60, 18}, (void*)"B");
    flow_add_node(f, "default", (flow_pt){30, 10}, (void*)"C");
    flow_callbacks cb = {0}; cb.on_viewport_change = on_vp; flow_set_callbacks(f, cb);
    vp_fires = 0;
    flow_fit_view(f, 2);
    ASSERT_INT(vp_fires, 1, "fit_view fires once");
    ASSERT_F(vp_last.zoom, f->view.zoom, "callback sees the fitted zoom");
    ASSERT_F(vp_last.ox, f->view.ox, "callback sees the fitted ox");
    flow_free(f);
  }

  /* ---- flow_load fires once with the restored viewport ---- */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    flow_add_node(f, "default", (flow_pt){10, 5}, (void*)"A");
    flow_pan(f, -10, -20);
    flow_set_zoom(f, 1.5f, (flow_pt){0, 0});     /* viewport: ox=-15, oy=-30, zoom=1.5 */
    ASSERT_INT(flow_save(f, "/tmp/flow_vp_events.json"), 0, "save ok");
    flow_t *g = flow_new(80, 24); flow_register_defaults(g);
    flow_callbacks cb = {0}; cb.on_viewport_change = on_vp; flow_set_callbacks(g, cb);
    vp_fires = 0;
    ASSERT_INT(flow_load(g, "/tmp/flow_vp_events.json"), 0, "load ok");
    ASSERT_INT(vp_fires, 1, "load fires once for the viewport restore");
    ASSERT_F(vp_last.ox, -15.0f, "loaded ox delivered");
    ASSERT_F(vp_last.oy, -30.0f, "loaded oy delivered");
    ASSERT_F(vp_last.zoom, 1.5f, "loaded zoom delivered");
    flow_free(f); flow_free(g);
  }

  /* ---- re-entrancy: a callback that pans fires its own event, no loop ---- */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    flow_callbacks cb = {0}; cb.on_viewport_change = on_vp; flow_set_callbacks(f, cb);
    vp_fires = 0; vp_recursed = 0; vp_recurse_target = f;
    flow_pan(f, 5, 5);
    ASSERT_INT(vp_fires, 2, "recursive pan fires its own event (terminates, no guard)");
    ASSERT_F(f->view.ox, 6.0f, "final viewport includes initial + recursive pan");
    ASSERT_F(f->view.oy, 6.0f, "  y too");
    vp_recurse_target = NULL;
    flow_free(f);
  }

  /* ---- clamp-before-compare: extent setter fires when it moves the view;
          a fully-clamped-back pan fires NOTHING ---- */
  {
    flow_t *f = flow_new(60, 10); flow_register_defaults(f);
    flow_callbacks cb = {0}; cb.on_viewport_change = on_vp; flow_set_callbacks(f, cb);
    vp_fires = 0;
    flow_set_translate_extent(f, (flow_rect){0, 0, 40, 40});  /* x degenerate: ox 0 -> pinned 10 */
    ASSERT_INT(vp_fires, 1, "extent setter fires when the re-clamp moves the view");
    ASSERT_F(vp_last.ox, 10.0f, "pinned ox delivered");
    flow_pan(f, 50, 0);                                       /* dead axis: clamps straight back */
    ASSERT_INT(vp_fires, 1, "clamped-to-same pan fires nothing (clamp BEFORE compare)");
    flow_free(f);
  }

  /* ---- viewport changes are never journaled (spec §11) ---- */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    flow_callbacks cb = {0}; cb.on_viewport_change = on_vp; flow_set_callbacks(f, cb);
    flow_pan(f, 7, 7); flow_zoom_in(f, (flow_pt){0, 0}); flow_fit_view(f, 2);
    ASSERT_INT(f->journal.n, 0, "pan/zoom/fit journal nothing");
    flow_free(f);
  }

  return flowtest_report("test_viewport_events");
}
