#define FLOW_IMPLEMENTATION
#include "../flow.h"
#include "flowtest.h"
#include <math.h>

/* Extent clamps (work package inc-4 #3, spec §6.1/§6.2):
   flow_set_node_extent      — flow_move_node targets clamp so the node rect stays inside
                               a world-space boundary (flush to the edge it exceeded).
   flow_set_translate_extent — pan/zoom/fit clamp so the VISIBLE world window stays inside
                               a world-space boundary; when the window is BIGGER than the
                               extent on an axis, that axis pins to the value centering the
                               extent in the window (d3-zoom translateExtent convention).
   Both default DISABLED (zero rect via calloc); w<=0 or h<=0 = disabled. */

#define ASSERT_F(got, want, msg) ASSERT(fabsf((float)(got) - (float)(want)) < 0.01f, msg)

int main(void) {
  /* ---- node extent disabled (default): moves are unclamped ---- */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    int a = flow_add_node(f, "default", (flow_pt){10, 10}, (void*)"A");
    flow_move_node(f, a, (flow_pt){-50, 300});
    ASSERT_INT(flow_get_node(f, a)->pos.x, -50, "disabled node extent: x unclamped");
    ASSERT_INT(flow_get_node(f, a)->pos.y, 300, "disabled node extent: y unclamped");
    flow_free(f);
  }

  /* ---- node extent enabled: rect clamps flush to the exceeded edge ---- */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    int a = flow_add_node(f, "default", (flow_pt){10, 10}, (void*)"A");
    flow_node *n = flow_get_node(f, a); n->w = 4; n->h = 3;       /* exact rect for the math */
    flow_set_node_extent(f, (flow_rect){0, 0, 20, 20});
    flow_move_node(f, a, (flow_pt){18, 18});                      /* rect would span to (22,21) */
    ASSERT_INT(flow_get_node(f, a)->pos.x, 16, "overflow right: x flush so 16+4==20");
    ASSERT_INT(flow_get_node(f, a)->pos.y, 17, "overflow bottom: y flush so 17+3==20");
    flow_move_node(f, a, (flow_pt){-5, -5});
    ASSERT_INT(flow_get_node(f, a)->pos.x, 0, "overflow left: x flush to extent.x");
    ASSERT_INT(flow_get_node(f, a)->pos.y, 0, "overflow top: y flush to extent.y");
    flow_move_node(f, a, (flow_pt){8, 9});                        /* interior: untouched */
    ASSERT_INT(flow_get_node(f, a)->pos.x, 8, "interior move: x unclamped");
    ASSERT_INT(flow_get_node(f, a)->pos.y, 9, "interior move: y unclamped");
    /* node WIDER than the extent: max-edge clamp first, min-edge wins -> flush left */
    flow_node *b = flow_get_node(f, flow_add_node(f, "default", (flow_pt){0, 0}, (void*)"B"));
    b->w = 30; b->h = 3;
    flow_move_node(f, b->id, (flow_pt){5, 5});
    ASSERT_INT(b->pos.x, 0, "oversized node: flush to min edge (deterministic)");
    /* zero rect re-disables */
    flow_set_node_extent(f, (flow_rect){0, 0, 0, 0});
    flow_move_node(f, a, (flow_pt){100, 100});
    ASSERT_INT(flow_get_node(f, a)->pos.x, 100, "zero rect re-disables the node extent");
    flow_free(f);
  }

  /* ---- node extent: undo/redo replays the CLAMPED target ---- */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    int a = flow_add_node(f, "default", (flow_pt){10, 10}, (void*)"A");
    flow_node *n = flow_get_node(f, a); n->w = 4; n->h = 3;
    flow_set_node_extent(f, (flow_rect){0, 0, 20, 20});
    flow_move_node(f, a, (flow_pt){18, 18});                      /* journals the clamped (16,17) */
    flow_undo(f);
    ASSERT_INT(flow_get_node(f, a)->pos.x, 10, "undo restores the pre-move position");
    flow_redo(f);
    ASSERT_INT(flow_get_node(f, a)->pos.x, 16, "redo replays the CLAMPED target");
    flow_free(f);
  }

  /* ---- translate extent disabled (default): pan/zoom unclamped (regression) ---- */
  {
    flow_t *f = flow_new(30, 10); flow_register_defaults(f);
    flow_pan(f, -500, 70);
    ASSERT_F(f->view.ox, -500.0f, "disabled translate extent: ox unclamped");
    ASSERT_F(f->view.oy, 70.0f,   "disabled translate extent: oy unclamped");
    flow_free(f);
  }

  /* ---- translate extent: pan clamps the visible window inside the extent ---- */
  {
    flow_t *f = flow_new(30, 10); flow_register_defaults(f);     /* visible 30x10 at zoom 1 */
    flow_set_translate_extent(f, (flow_rect){0, 0, 100, 100});
    ASSERT_F(f->view.ox, 0.0f, "setter: in-bounds view untouched");
    flow_pan(f, -100, 0);                                        /* pan view right by 100 */
    ASSERT_F(f->view.ox, -70.0f, "pan right past extent: ox clamps to cols-(ex+ew)z = -70");
    flow_pan(f, 500, 0);                                         /* pan view left, far */
    ASSERT_F(f->view.ox, 0.0f, "pan left past extent: ox clamps to -ex*z = 0");
    flow_pan(f, 0, -95);                                         /* down: oy in [rows-100, 0] = [-90, 0] */
    ASSERT_F(f->view.oy, -90.0f, "pan down past extent: oy clamps to rows-(ey+eh)z = -90");
    flow_free(f);
  }

  /* ---- translate extent SMALLER than the window: axis pins centered, per-axis independent ---- */
  {
    flow_t *f = flow_new(60, 10); flow_register_defaults(f);     /* 60 wide > extent.w 40 */
    flow_set_translate_extent(f, (flow_rect){0, 0, 40, 40});
    ASSERT_F(f->view.ox, 10.0f, "setter immediately pins x centered: cols/2 - (ex+ew/2)z = 10");
    flow_pan(f, 50, 0);
    ASSERT_F(f->view.ox, 10.0f, "x pan is a no-op on the degenerate axis (+50)");
    flow_pan(f, -50, 0);
    ASSERT_F(f->view.ox, 10.0f, "x pan is a no-op on the degenerate axis (-50)");
    flow_pan(f, 0, -50);                                         /* y normal: oy in [10-40, 0] = [-30, 0] */
    ASSERT_F(f->view.oy, -30.0f, "y still pans within its clamp range (per-axis independence)");
    flow_pan(f, 0, 50);
    ASSERT_F(f->view.oy, 0.0f, "y clamps at its other bound");
    flow_free(f);
  }

  /* ---- flow_set_zoom honors the translate extent after the zoom write ---- */
  {
    flow_t *f = flow_new(30, 10); flow_register_defaults(f);
    flow_set_translate_extent(f, (flow_rect){0, 0, 50, 15});     /* window @z=1: 30x10 fits */
    flow_set_zoom(f, 0.5f, (flow_pt){0, 0});                     /* window 60x20 > 50x15: both degenerate */
    ASSERT_F(f->view.zoom, 0.5f, "zoom write itself unaffected");
    ASSERT_F(f->view.ox, 2.5f,  "zoom-out past extent: x pins centered ((30-25)/2)");
    ASSERT_F(f->view.oy, 1.25f, "zoom-out past extent: y pins centered ((10-7.5)/2)");
    flow_set_zoom(f, 1.0f, (flow_pt){15, 5});                    /* window 30x10: x fits again */
    /* x clamp range at z=1: [30-50, 0] = [-20, 0]; whatever the zoom math wanted, ox is in range */
    ASSERT(f->view.ox >= -20.01f && f->view.ox <= 0.01f, "zoom-in re-enters the normal x clamp range");
    flow_free(f);
  }

  /* ---- flow_fit_view: computed fit is clamped against the translate extent ---- */
  {
    flow_t *f = flow_new(30, 10); flow_register_defaults(f);
    flow_set_translate_extent(f, (flow_rect){0, 0, 100, 100});
    flow_add_node(f, "default", (flow_pt){150, 80}, (void*)"A"); /* bounds well outside the extent */
    flow_add_node(f, "default", (flow_pt){170, 90}, (void*)"B");
    flow_fit_view(f, 2);
    float z = f->view.zoom;
    float wx0 = (0.0f - f->view.ox) / z, wx1 = ((float)f->cols - f->view.ox) / z;
    float wy0 = (0.0f - f->view.oy) / z, wy1 = ((float)f->rows - f->view.oy) / z;
    ASSERT(wx0 >= -0.01f && wx1 <= 100.01f, "fit_view: visible x window stays inside the extent");
    ASSERT(wy0 >= -0.01f && wy1 <= 100.01f, "fit_view: visible y window stays inside the extent");
    flow_free(f);
  }

  return flowtest_report("test_extents");
}
