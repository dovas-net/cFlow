#define FLOW_IMPLEMENTATION
#include "../flow.h"
#include "flowtest.h"

/* inc-7 #3 helpers: synthetic SGR mouse (cells are 0-based; SGR is 1-based). */
static void press(flow_t *f, int cx, int cy)   { char b[40]; int n = snprintf(b, sizeof b, "\x1b[<0;%d;%dM",  cx+1, cy+1); flow_feed(f, b, n); }
static void release(flow_t *f, int cx, int cy) { char b[40]; int n = snprintf(b, sizeof b, "\x1b[<0;%d;%dm",  cx+1, cy+1); flow_feed(f, b, n); }
static void motion(flow_t *f, int cx, int cy)  { char b[40]; int n = snprintf(b, sizeof b, "\x1b[<32;%d;%dM", cx+1, cy+1); flow_feed(f, b, n); }
static void spress(flow_t *f, int cx, int cy)  { char b[40]; int n = snprintf(b, sizeof b, "\x1b[<4;%d;%dM",  cx+1, cy+1); flow_feed(f, b, n); }  /* shift-press */
/* middle cell (x) / row (y) of the Controls button with the given action (render first to fill the cache) */
static int wx_for(flow_t *f, int action) { for (int i = 0; i < f->nwidgets; i++) if (f->widgets[i].owner == FLOW_WIDGET_OWNER_CONTROLS && f->widgets[i].action == action) return f->widgets[i].x + 1; return -1; }
static int wy_for(flow_t *f, int action) { for (int i = 0; i < f->nwidgets; i++) if (f->widgets[i].owner == FLOW_WIDGET_OWNER_CONTROLS && f->widgets[i].action == action) return f->widgets[i].y; return -1; }
/* inc-7 #4 node-toolbar fixtures */
static int g_nt_id = -2, g_nt_idx = -2, g_nt_fires = 0;
static void nt_cap(flow_t *f, int id, void *u) { (void)f; g_nt_id = id; g_nt_idx = (int)(intptr_t)u; g_nt_fires++; }
static void nt_del(flow_t *f, int id, void *u) { (void)u; flow_remove_node(f, id); }   /* realloc-forcing action */
static int g_nclick = 0;
static void nt_onclick(flow_t *f, int id, void *u) { (void)f;(void)id;(void)u; g_nclick++; }
/* x,y of toolbar cell `action` for the given owner (render first to fill the cache); -1 if absent */
static int tbx(flow_t *f, int owner, int action) { for (int i = 0; i < f->nwidgets; i++) if (f->widgets[i].owner == owner && f->widgets[i].action == action) return f->widgets[i].x; return -1; }
static int tby(flow_t *f, int owner, int action) { for (int i = 0; i < f->nwidgets; i++) if (f->widgets[i].owner == owner && f->widgets[i].action == action) return f->widgets[i].y; return -1; }

int main(void) {
  flow_t *f = flow_new(80, 24); flow_register_defaults(f);
  flow_pt base = flow_to_screen(f, (flow_pt){0, 0});
  flow_feed(f, "\x1b[C", 3);  /* right arrow: reveal content to the right -> content shifts left */
  flow_pt right = flow_to_screen(f, (flow_pt){0, 0});
  ASSERT_INT(right.x, base.x - 1, "right arrow pans content left by 1");
  flow_feed(f, "\x1b[D", 3);  /* left arrow undoes it */
  flow_pt back = flow_to_screen(f, (flow_pt){0, 0});
  ASSERT_INT(back.x, base.x, "left arrow pans content right by 1");
  flow_feed(f, "\x1b[B", 3);  /* down arrow */
  flow_pt down = flow_to_screen(f, (flow_pt){0, 0});
  ASSERT_INT(down.y, base.y - 1, "down arrow pans content up by 1");
  flow_free(f);

  /* ===== inc-7 #3: Controls bar + widget seam + lock ===== */
  /* (1) + zooms in, - zooms out (the seam routes to flow_zoom_in/out). */
  {
    int W=40,H=12; flow_cell *buf = (flow_cell*)malloc((size_t)W*H*sizeof(flow_cell));
    flow_t *g = flow_new(W, H); flow_register_defaults(g);
    flow_set_controls(g, 1, FLOW_CORNER_BL);
    flow_render(g, buf, W, H);                         /* fills widgets[] */
    ASSERT_INT(g->nwidgets, 4, "controls cached 4 widget rects");
    float z0 = flow_zoom(g);
    press(g, wx_for(g, FLOW_WIDGET_ZOOM_IN), wy_for(g, FLOW_WIDGET_ZOOM_IN));
    ASSERT(flow_zoom(g) > z0, "+ widget zoomed in");
    float z1 = flow_zoom(g);
    press(g, wx_for(g, FLOW_WIDGET_ZOOM_OUT), wy_for(g, FLOW_WIDGET_ZOOM_OUT));
    ASSERT(flow_zoom(g) < z1, "- widget zoomed out");
    free(buf); flow_free(g);
  }

  /* (2) fit runs fit-view (changes the view to frame the bounds). */
  {
    int W=40,H=12; flow_cell *buf = (flow_cell*)malloc((size_t)W*H*sizeof(flow_cell));
    flow_t *g = flow_new(W, H); flow_register_defaults(g);
    flow_add_node(g, "default", (flow_pt){0,0},    (void*)"A");
    flow_add_node(g, "default", (flow_pt){100,40}, (void*)"B");
    flow_set_controls(g, 1, FLOW_CORNER_BL);
    flow_set_zoom(g, 2.0f, (flow_pt){0,0});
    flow_render(g, buf, W, H);
    float zb = flow_zoom(g);
    press(g, wx_for(g, FLOW_WIDGET_FIT), wy_for(g, FLOW_WIDGET_FIT));
    ASSERT(flow_zoom(g) != zb, "fit widget changed the view (fit-view ran)");
    free(buf); flow_free(g);
  }

  /* (3) lock toggles flow_locked, and [lock] stays clickable WHILE locked (widget test
     runs before the lock handler). */
  {
    int W=40,H=12; flow_cell *buf = (flow_cell*)malloc((size_t)W*H*sizeof(flow_cell));
    flow_t *g = flow_new(W, H); flow_register_defaults(g);
    flow_set_controls(g, 1, FLOW_CORNER_BL);
    ASSERT_INT(flow_locked(g), 0, "starts unlocked");
    flow_render(g, buf, W, H);
    press(g, wx_for(g, FLOW_WIDGET_LOCK), wy_for(g, FLOW_WIDGET_LOCK));
    ASSERT_INT(flow_locked(g), 1, "lock widget set locked");
    flow_render(g, buf, W, H);
    press(g, wx_for(g, FLOW_WIDGET_LOCK), wy_for(g, FLOW_WIDGET_LOCK));
    ASSERT_INT(flow_locked(g), 0, "lock widget toggled back (clickable while locked)");
    free(buf); flow_free(g);
  }

  /* (4) a click just OUTSIDE the bar falls through to canvas classification. */
  {
    int W=40,H=12; flow_cell *buf = (flow_cell*)malloc((size_t)W*H*sizeof(flow_cell));
    flow_t *g = flow_new(W, H); flow_register_defaults(g);
    int a = flow_add_node(g, "default", (flow_pt){5,2}, (void*)"A");
    flow_set_controls(g, 1, FLOW_CORNER_BL);
    flow_render(g, buf, W, H);
    flow_pt s = flow_to_screen(g, (flow_pt){6,3});   /* node interior, away from the BL bar */
    press(g, s.x, s.y); release(g, s.x, s.y);
    ASSERT_INT(flow_selected_node(g), a, "click outside bar falls through to node select");
    free(buf); flow_free(g);
  }

  /* (5) locked drag does NOT move or select a node; unlocked it does. */
  {
    flow_t *g = flow_new(80, 24); flow_register_defaults(g);
    int a = flow_add_node(g, "default", (flow_pt){5,5}, (void*)"A");
    flow_pt a0 = flow_node_abs(g, flow_get_node(g, a));
    flow_set_locked(g, 1);
    flow_pt s = flow_to_screen(g, (flow_pt){6,6});
    press(g, s.x, s.y); motion(g, s.x+10, s.y); release(g, s.x+10, s.y);
    flow_pt a1 = flow_node_abs(g, flow_get_node(g, a));
    ASSERT_INT(a1.x, a0.x, "locked: node did not move (x)");
    ASSERT_INT(a1.y, a0.y, "locked: node did not move (y)");
    ASSERT_INT(flow_selected_node(g), -1, "locked: no click-select");
    flow_free(g);
    flow_t *u = flow_new(80, 24); flow_register_defaults(u);
    int b = flow_add_node(u, "default", (flow_pt){5,5}, (void*)"A");
    flow_pt u0 = flow_node_abs(u, flow_get_node(u, b));
    flow_pt su = flow_to_screen(u, (flow_pt){6,6});
    press(u, su.x, su.y); motion(u, su.x+10, su.y); release(u, su.x+10, su.y);
    ASSERT(flow_node_abs(u, flow_get_node(u, b)).x != u0.x, "unlocked: drag moves the node");
    flow_free(u);
  }

  /* (6) locked marquee does NOT select (the gate a 3-point scheme would miss). */
  {
    flow_t *g = flow_new(80, 24); flow_register_defaults(g);
    flow_add_node(g, "default", (flow_pt){5,5},  (void*)"A");
    flow_add_node(g, "default", (flow_pt){20,12}, (void*)"B");
    flow_set_locked(g, 1);
    spress(g, 40, 2); motion(g, 2, 20); release(g, 2, 20);   /* shift-drag enclosing both */
    ASSERT_INT(flow_selected_count(g), 0, "locked: marquee selects nothing");
    flow_free(g);
    flow_t *u = flow_new(80, 24); flow_register_defaults(u);
    flow_add_node(u, "default", (flow_pt){5,5},  (void*)"A");
    flow_add_node(u, "default", (flow_pt){20,12}, (void*)"B");
    spress(u, 40, 2); motion(u, 2, 20); release(u, 2, 20);
    ASSERT(flow_selected_count(u) >= 2, "unlocked: marquee selects enclosed nodes");
    flow_free(u);
  }

  /* (7) locked: pressing an edge endpoint does NOT arm a reconnect (the arm a 3-point
     gate would slip through; the single top-of-block return preempts it). */
  {
    flow_t *g = flow_new(80, 24); flow_register_defaults(g);
    int a = flow_add_node(g, "default", (flow_pt){5,5},  (void*)"A");
    int b = flow_add_node(g, "default", (flow_pt){20,5}, (void*)"B");
    int e = flow_add_edge(g, a, b, "out", "in");
    flow_pt ts; int ok = flow_edge_endpoint_screen(g, flow_get_edge(g, e), 1, &ts);
    ASSERT(ok, "target endpoint resolved");
    flow_set_locked(g, 1);
    press(g, ts.x, ts.y);
    ASSERT_INT(g->reconnect_edge, -1, "locked: endpoint press does not arm reconnect");
    release(g, ts.x, ts.y);
    flow_set_locked(g, 0);
    press(g, ts.x, ts.y);
    ASSERT_INT(g->reconnect_edge, e, "unlocked: endpoint press arms reconnect");
    release(g, ts.x, ts.y);
    flow_free(g);
  }

  /* (8) lock keeps pan/zoom working and suppresses connect-arm. */
  {
    flow_t *g = flow_new(80, 24); flow_register_defaults(g);
    int a = flow_add_node(g, "default", (flow_pt){5,5}, (void*)"A");
    flow_set_locked(g, 1);
    float z0 = flow_zoom(g);
    flow_feed(g, "+", 1);
    ASSERT(flow_zoom(g) > z0, "locked: + key still zooms");
    flow_pt before = flow_to_screen(g, (flow_pt){0,0});
    flow_pt e = flow_to_screen(g, (flow_pt){50,18});      /* empty pane */
    press(g, e.x, e.y); motion(g, e.x+5, e.y);
    ASSERT(flow_to_screen(g, (flow_pt){0,0}).x != before.x, "locked: empty-pane drag still pans");
    release(g, e.x+5, e.y);
    flow_set_hover(g, a);
    flow_pt h = flow__handle_screen(g, flow_get_node(g, a), flow__handle_named(g, a, "out"));
    press(g, h.x, h.y);
    ASSERT_INT(flow_connecting(g), 0, "locked: source-handle press does not arm a connection");
    release(g, h.x, h.y);
    flow_free(g);
  }

  /* (9) controls OFF by default => no widget cells, press classifies as canvas. */
  {
    int W=40,H=12; flow_cell *buf = (flow_cell*)malloc((size_t)W*H*sizeof(flow_cell));
    flow_t *g = flow_new(W, H); flow_register_defaults(g);
    int a = flow_add_node(g, "default", (flow_pt){5,2}, (void*)"A");
    flow_render(g, buf, W, H);
    ASSERT_INT(g->nwidgets, 0, "controls off => no widget rects");
    flow_pt s = flow_to_screen(g, (flow_pt){6,3});
    press(g, s.x, s.y); release(g, s.x, s.y);
    ASSERT_INT(flow_selected_node(g), a, "press classifies as canvas when controls off");
    free(buf); flow_free(g);
  }

  /* (10) ORDERING (seam contract for #4/#5): the widget hit-test wins ABOVE the conn
     resolve — a control press mid-connection zooms, it does NOT complete/cancel. */
  {
    int W=40,H=12; flow_cell *buf = (flow_cell*)malloc((size_t)W*H*sizeof(flow_cell));
    flow_t *g = flow_new(W, H); flow_register_defaults(g);
    int a = flow_add_node(g, "default", (flow_pt){5,2}, (void*)"A");
    flow_set_controls(g, 1, FLOW_CORNER_BL);
    flow_begin_connection(g, a, "out");               /* connectOnClick: conn_active, no button */
    ASSERT_INT(flow_connecting(g), 1, "armed connection");
    flow_render(g, buf, W, H);
    float z0 = flow_zoom(g);
    press(g, wx_for(g, FLOW_WIDGET_ZOOM_IN), wy_for(g, FLOW_WIDGET_ZOOM_IN));
    ASSERT(flow_zoom(g) > z0, "widget consumed the press (zoomed) ABOVE conn-resolve");
    ASSERT_INT(flow_connecting(g), 1, "connection neither completed nor cancelled");
    free(buf); flow_free(g);
  }

  /* (11) ORDERING: the widget hit-test wins ABOVE the space-pan arm — a control press
     while space is held zooms, it does NOT arm a pan. */
  {
    int W=40,H=12; flow_cell *buf = (flow_cell*)malloc((size_t)W*H*sizeof(flow_cell));
    flow_t *g = flow_new(W, H); flow_register_defaults(g);
    flow_set_controls(g, 1, FLOW_CORNER_BL);
    g->space_held = 1;
    flow_render(g, buf, W, H);
    float z0 = flow_zoom(g);
    press(g, wx_for(g, FLOW_WIDGET_ZOOM_IN), wy_for(g, FLOW_WIDGET_ZOOM_IN));
    ASSERT(flow_zoom(g) > z0, "widget consumed the press ABOVE space-pan arm");
    ASSERT_INT(g->mouse_down, 0, "no pan armed (widget consumed, mouse_down cleared)");
    free(buf); flow_free(g);
  }

  /* ===== inc-7 #4: node toolbar (selection-anchored action strip) ===== */
  static const flow_toolbar_action NT_ACTS[] = {
    { "del", nt_cap, (void*)(intptr_t)0 },
    { "dup", nt_cap, (void*)(intptr_t)1 },
  };

  /* (1)+(2) action cells fire with the selected node id; distinct cells -> distinct actions. */
  {
    int W=80,H=24; flow_cell *buf = (flow_cell*)malloc((size_t)W*H*sizeof(flow_cell));
    flow_t *g = flow_new(W, H); flow_register_defaults(g);
    int a = flow_add_node(g, "default", (flow_pt){10, 8}, (void*)"A");
    flow_select_node(g, a, 0);
    flow_set_node_toolbar(g, NT_ACTS, 2);
    flow_render(g, buf, W, H);
    int c0x = tbx(g, FLOW_WIDGET_OWNER_NODE_TOOLBAR, 0), c0y = tby(g, FLOW_WIDGET_OWNER_NODE_TOOLBAR, 0);
    int c1x = tbx(g, FLOW_WIDGET_OWNER_NODE_TOOLBAR, 1), c1y = tby(g, FLOW_WIDGET_OWNER_NODE_TOOLBAR, 1);
    ASSERT(c0x >= 0 && c1x >= 0, "node toolbar cached 2 action rects");
    g_nt_id = -2; g_nt_idx = -2;
    press(g, c0x, c0y);
    ASSERT_INT(g_nt_id, a, "cell 0 fires with the selected node id");
    ASSERT_INT(g_nt_idx, 0, "cell 0 -> action 0");
    g_nt_id = -2; g_nt_idx = -2;
    press(g, c1x, c1y);
    ASSERT_INT(g_nt_idx, 1, "cell 1 -> action 1 (per-cell x-span mapping)");
    free(buf); flow_free(g);
  }

  /* (3) clicks elsewhere FALL THROUGH (the seam consumed ONLY the action cell). */
  {
    int W=80,H=24; flow_cell *buf = (flow_cell*)malloc((size_t)W*H*sizeof(flow_cell));
    flow_t *g = flow_new(W, H); flow_register_defaults(g);
    g->cb.on_node_click = nt_onclick;
    int a = flow_add_node(g, "default", (flow_pt){10, 8}, (void*)"A");
    flow_select_node(g, a, 0);
    flow_set_node_toolbar(g, NT_ACTS, 2);
    flow_render(g, buf, W, H);
    g_nt_id = -2; g_nclick = 0;
    flow_pt body = flow_to_screen(g, (flow_pt){11, 9});   /* node interior, NOT the strip row */
    press(g, body.x, body.y); release(g, body.x, body.y); /* a full click */
    ASSERT_INT(g_nclick, 1, "body click fell through to on_node_click");
    ASSERT_INT(g_nt_id, -2, "body click did NOT fire a toolbar action");
    free(buf); flow_free(g);
  }

  /* (4) hidden when selection != 1 (the same cell that fired in (1) is inert). */
  {
    int W=80,H=24; flow_cell *buf = (flow_cell*)malloc((size_t)W*H*sizeof(flow_cell));
    flow_t *g = flow_new(W, H); flow_register_defaults(g);
    int a = flow_add_node(g, "default", (flow_pt){10, 8}, (void*)"A");
    int b = flow_add_node(g, "default", (flow_pt){30, 8}, (void*)"B");
    flow_select_node(g, a, 0);
    flow_set_node_toolbar(g, NT_ACTS, 2);
    flow_render(g, buf, W, H);
    int cx = tbx(g, FLOW_WIDGET_OWNER_NODE_TOOLBAR, 0), cy = tby(g, FLOW_WIDGET_OWNER_NODE_TOOLBAR, 0);
    ASSERT(cx >= 0, "strip shown for single selection");
    /* clear selection: strip gone */
    flow_clear_selection(g);
    flow_render(g, buf, W, H);
    g_nt_id = -2; press(g, cx, cy);
    ASSERT_INT(g_nt_id, -2, "no-selection: that cell is inert");
    /* multi-select: strip hidden */
    flow_select_node(g, a, 0); flow_select_node(g, b, 1);
    ASSERT_INT(flow_selected_count(g), 2, "two selected");
    flow_render(g, buf, W, H);
    g_nt_id = -2; press(g, cx, cy);
    ASSERT_INT(g_nt_id, -2, "multi-select: strip hidden, cell inert");
    free(buf); flow_free(g);
  }

  /* (5) fn(id) mutation is realloc-safe (delete the node from within the action). */
  {
    int W=80,H=24; flow_cell *buf = (flow_cell*)malloc((size_t)W*H*sizeof(flow_cell));
    flow_t *g = flow_new(W, H); flow_register_defaults(g);
    flow_add_node(g, "default", (flow_pt){2, 2}, (void*)"keep");
    int a = flow_add_node(g, "default", (flow_pt){10, 8}, (void*)"A");
    static const flow_toolbar_action del_acts[] = { { "x", nt_del, NULL } };
    flow_select_node(g, a, 0);
    flow_set_node_toolbar(g, del_acts, 1);
    flow_render(g, buf, W, H);
    int n0 = flow_node_count(g);
    int cx = tbx(g, FLOW_WIDGET_OWNER_NODE_TOOLBAR, 0), cy = tby(g, FLOW_WIDGET_OWNER_NODE_TOOLBAR, 0);
    press(g, cx, cy);
    ASSERT_INT(flow_node_count(g), n0 - 1, "action deleted the node (realloc-safe, no UAF)");
    ASSERT_INT(flow_get_node(g, a) == NULL, 1, "deleted node id is gone");
    free(buf); flow_free(g);
  }

  /* (6) overlap: a toolbar cell drawn ON TOP of a Controls button must win the hit-test
     (topmost-rendered == first-hit). Controls TL + a node at world (0,1) puts both on row 0. */
  {
    int W=40,H=12; flow_cell *buf = (flow_cell*)malloc((size_t)W*H*sizeof(flow_cell));
    flow_t *g = flow_new(W, H); flow_register_defaults(g);
    int a = flow_add_node(g, "default", (flow_pt){0, 1}, (void*)"A");   /* strip lands on row 0 */
    flow_set_controls(g, 1, FLOW_CORNER_TL);                            /* controls also on row 0 */
    flow_select_node(g, a, 0);
    flow_set_node_toolbar(g, NT_ACTS, 2);
    flow_render(g, buf, W, H);
    int t0x = tbx(g, FLOW_WIDGET_OWNER_NODE_TOOLBAR, 0), t0y = tby(g, FLOW_WIDGET_OWNER_NODE_TOOLBAR, 0);
    ASSERT_INT(t0y, 0, "toolbar strip on row 0 (overlaps TL controls)");
    float z0 = flow_zoom(g); g_nt_id = -2;
    press(g, t0x, t0y);
    ASSERT_INT(g_nt_id, a, "overlap: press routed to the TOOLBAR (topmost), not controls");
    ASSERT(flow_zoom(g) == z0, "overlap: controls did NOT fire (no zoom)");
    free(buf); flow_free(g);
  }

  return flowtest_report("test_input");
}
