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

  /* ================= framing primitives (inc-5 #4) ================= */

  /* ---- flow_set_center: centers, fires once, idempotent re-call fires nothing ---- */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    flow_callbacks cb = {0}; cb.on_viewport_change = on_vp; flow_set_callbacks(f, cb);
    vp_fires = 0;
    flow_set_center(f, 100, 50, -1.0f);            /* keep zoom (=1) */
    ASSERT_INT(vp_fires, 1, "set_center fires once");
    ASSERT_F(f->view.ox, -60.0f, "ox = cols/2 - wx*z = 40-100");
    ASSERT_F(f->view.oy, -38.0f, "oy = rows/2 - wy*z = 12-50");
    ASSERT_F(f->view.zoom, 1.0f, "zoom<=0 keeps current zoom");
    flow_set_center(f, 100, 50, -1.0f);            /* identical -> no fire */
    ASSERT_INT(vp_fires, 1, "idempotent set_center fires nothing");
    flow_free(f);
  }

  /* ---- flow_set_center: explicit zoom clamps to [zmin,zmax] ---- */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    flow_set_zoom_limits(f, 0.5f, 2.0f);
    flow_set_center(f, 0, 0, 100.0f);
    ASSERT_F(f->view.zoom, 2.0f, "explicit zoom clamped to zmax");
    ASSERT_F(f->view.ox, 40.0f, "centering uses the clamped zoom (40 - 0*2)");
    flow_free(f);
  }

  /* ---- flow_set_center: zoom<=0 keeps a non-1 current zoom ---- */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    flow_zoom_in(f, (flow_pt){40, 12});
    float z = f->view.zoom;
    ASSERT(z > 1.0f, "precondition: zoomed in");
    flow_set_center(f, 10, 10, 0.0f);
    ASSERT_F(f->view.zoom, z, "zoom 0 keeps current");
    ASSERT_F(f->view.ox, 40.0f - 10.0f * z, "centering uses the kept zoom");
    flow_free(f);
  }

  /* ---- flow_set_center: clamp-first under a translate extent ---- */
  {
    flow_t *f = flow_new(60, 10); flow_register_defaults(f);
    flow_callbacks cb = {0}; cb.on_viewport_change = on_vp; flow_set_callbacks(f, cb);
    flow_set_translate_extent(f, (flow_rect){0, 0, 40, 40});  /* x degenerate: ox pinned 10 */
    vp_fires = 0;
    flow_set_center(f, 0, 8, -1.0f);   /* raw (30,-3): ox pins to 10, oy -3 in range */
    ASSERT_INT(vp_fires, 1, "extent-pinned set_center fires once (oy changed)");
    ASSERT_F(vp_last.ox, 10.0f, "delivered ox is the PINNED value, not the raw 30");
    ASSERT_F(vp_last.oy, -3.0f, "oy clamped within range");
    flow_free(f);
  }

  /* ---- flow_fit_bounds(flow_bounds(f)) == flow_fit_view (shared math) ---- */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    flow_add_node(f, "default", (flow_pt){10, 5},  (void*)"A");
    flow_add_node(f, "default", (flow_pt){60, 18}, (void*)"B");
    flow_add_node(f, "default", (flow_pt){30, 10}, (void*)"C");
    flow_fit_view(f, 2);
    float fx = f->view.ox, fy = f->view.oy, fz = f->view.zoom;
    flow_pan(f, 13, 7);                                /* move away */
    flow_fit_bounds(f, flow_bounds(f), 2);
    ASSERT_F(f->view.ox, fx, "fit_bounds(all-node rect) == fit_view: ox");
    ASSERT_F(f->view.oy, fy, "  oy");
    ASSERT_F(f->view.zoom, fz, "  zoom");

    /* zero-rect no-op */
    flow_callbacks cb = {0}; cb.on_viewport_change = on_vp; flow_set_callbacks(f, cb);
    vp_fires = 0;
    flow_fit_bounds(f, (flow_rect){5, 5, 0, 0}, 2);
    ASSERT_INT(vp_fires, 0, "zero-rect fit_bounds is a no-op (no fire)");
    ASSERT_F(f->view.ox, fx, "  view untouched");
    flow_free(f);
  }

  /* ---- flow_bounds_of: subset, hidden-inclusion divergence, child abs, degenerate ---- */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    int a = flow_add_node(f, "default", (flow_pt){10, 5},  (void*)"A");
    int b = flow_add_node(f, "default", (flow_pt){60, 18}, (void*)"B");
    int c = flow_add_node(f, "default", (flow_pt){30, 10}, (void*)"C");

    /* subset excludes non-listed */
    int one[1] = { a };
    flow_rect r1 = flow_bounds_of(f, one, 1);
    ASSERT_INT(r1.x, 10, "bounds_of({a}).x");
    ASSERT_INT(r1.y, 5,  "bounds_of({a}).y");
    ASSERT_INT(r1.w, 5,  "bounds_of({a}).w (single node, not the graph)");
    ASSERT_INT(r1.h, 3,  "bounds_of({a}).h");

    /* INCLUDES a hidden id — the model-level divergence from flow_bounds */
    flow_set_node_hidden(f, b, 1);
    int two[2] = { a, b };
    flow_rect r2 = flow_bounds_of(f, two, 2);
    ASSERT_INT(r2.x, 10, "bounds_of includes hidden b: x");
    ASSERT_INT(r2.w, 55, "  spans to hidden b (x10..65)");
    ASSERT_INT(r2.h, 16, "  spans to hidden b (y5..21)");
    flow_rect vb = flow_bounds(f);
    ASSERT(vb.w != r2.w, "flow_bounds (view-level) skips hidden b — they diverge");
    flow_set_node_hidden(f, b, 0);

    /* child contributes its ABSOLUTE rect */
    flow_set_parent(f, c, a);                          /* preserves abs position */
    int kid[1] = { c };
    flow_rect r3 = flow_bounds_of(f, kid, 1);
    ASSERT_INT(r3.x, 30, "child id: absolute x (parent offset applied)");
    ASSERT_INT(r3.y, 10, "  absolute y");

    /* degenerate inputs: zero rect, no crash */
    flow_rect z1 = flow_bounds_of(f, NULL, 0);
    ASSERT_INT(z1.w, 0, "bounds_of(NULL,0) = zero rect");
    flow_rect z2 = flow_bounds_of(f, one, 0);
    ASSERT_INT(z2.w, 0, "bounds_of(ids,0) = zero rect");
    int missing = 9999;
    flow_rect z3 = flow_bounds_of(f, &missing, 1);
    ASSERT_INT(z3.w, 0, "bounds_of(missing id) = zero rect");
    flow_free(f);
  }

  /* ---- odd-dimension centering uses FLOAT halves (no half-cell drift) ---- */
  {
    flow_t *o = flow_new(81, 25); flow_register_defaults(o);
    flow_set_center(o, 0, 0, -1.0f);
    ASSERT_F(o->view.ox, 40.5f, "odd-width center uses float halves (not 40)");
    ASSERT_F(o->view.oy, 12.5f, "odd-height center uses float halves");
    flow_free(o);
  }

  /* ---- framing is never journaled ---- */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    flow_add_node(f, "default", (flow_pt){10, 5}, (void*)"A");
    int jn = f->journal.n;                       /* the add itself journals one op */
    flow_set_center(f, 50, 50, 2.0f);
    flow_fit_bounds(f, (flow_rect){0, 0, 30, 20}, 1);
    ASSERT_INT(f->journal.n, jn, "set_center/fit_bounds journal nothing");
    flow_free(f);
  }

  return flowtest_report("test_viewport_events");
}
