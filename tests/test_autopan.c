#define FLOW_IMPLEMENTATION
#include "../flow.h"
#include "flowtest.h"

/* Auto-pan near viewport edge during drags (work package #6, spec §8).
   Event-driven model A: ONE autopan_speed step per motion event while an OBJECT drag
   (node-drag, connection-drag, reconnect-drag) has the cursor within autopan_margin
   cells of a buffer edge. Pane-pan and marquee drags are excluded. The pan is applied
   BEFORE the drag re-places its object, so the object stays under the cursor.
   An axis whose margin bands would overlap (2*margin >= extent) never auto-pans. */

static void feed(flow_t *f, const char *s) { flow_feed(f, s, (int)strlen(s)); }

/* synthetic SGR-1006 at a 0-based screen cell (SGR is 1-based) */
static void press_at(flow_t *f, int sx, int sy)       { char b[32]; snprintf(b, sizeof b, "\x1b[<0;%d;%dM",  sx + 1, sy + 1); feed(f, b); }
static void move_to(flow_t *f, int sx, int sy)        { char b[32]; snprintf(b, sizeof b, "\x1b[<32;%d;%dM", sx + 1, sy + 1); feed(f, b); }
static void release_at(flow_t *f, int sx, int sy)     { char b[32]; snprintf(b, sizeof b, "\x1b[<0;%d;%dm",  sx + 1, sy + 1); feed(f, b); }
static void press_shift_at(flow_t *f, int sx, int sy) { char b[32]; snprintf(b, sizeof b, "\x1b[<4;%d;%dM",  sx + 1, sy + 1); feed(f, b); }
static void move_shift_to(flow_t *f, int sx, int sy)  { char b[32]; snprintf(b, sizeof b, "\x1b[<36;%d;%dM", sx + 1, sy + 1); feed(f, b); }

static flow_pt org(flow_t *f) { return flow_to_screen(f, (flow_pt){0, 0}); }  /* world origin on screen = view offset probe */

int main(void) {
  /* ---- config defaults (additive fields, set in flow_new) ---- */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    ASSERT_INT(f->autopan_margin, 3, "autopan_margin defaults to 3");
    ASSERT_INT(f->autopan_speed,  2, "autopan_speed defaults to 2");
    flow_free(f);
  }

  /* ---- node-drag near the RIGHT edge pans; node stays under the cursor (pan-then-place) ---- */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    int a = flow_add_node(f, "default", (flow_pt){10, 5}, (void*)"A");  /* rect (10,5,5,3) */
    flow_pt o0 = org(f);
    press_at(f, 11, 6);                       /* grab offset (1,1) */
    move_to(f, 79, 12);                       /* cursor at cols-1: right band (>=77) */
    ASSERT_INT(org(f).x, o0.x - 2, "right-edge motion auto-pans -2 (reveals world to the right)");
    ASSERT_INT(org(f).y, o0.y,     "  y untouched (cursor y interior)");
    /* pan FIRST, then place: node abs == post-pan world(cursor) - grab == (81,12)-(1,1) */
    ASSERT_INT(flow_get_node(f, a)->pos.x, 80, "  node placed at POST-pan world (stays under cursor)");
    ASSERT_INT(flow_get_node(f, a)->pos.y, 11, "  node y likewise");
    /* interior motion: no auto-pan, node just follows */
    move_to(f, 40, 12);
    ASSERT_INT(org(f).x, o0.x - 2, "interior motion does NOT auto-pan (offset unchanged)");
    ASSERT_INT(flow_get_node(f, a)->pos.x, 41, "  node follows cursor: world(40,12)-(1,1)");
    release_at(f, 40, 12);
    flow_free(f);
  }

  /* ---- direction correctness: left / top / bottom edges and a corner ---- */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    flow_add_node(f, "default", (flow_pt){38, 10}, (void*)"A");  /* rect (38,10,5,3) */
    press_at(f, 39, 11);
    flow_pt o = org(f);
    move_to(f, 0, 12);                        /* LEFT band (x<3) */
    ASSERT_INT(org(f).x, o.x + 2, "left edge pans +2 (reveals world to the left)");
    ASSERT_INT(org(f).y, o.y,     "  left edge: y untouched");
    o = org(f);
    move_to(f, 40, 0);                        /* TOP band (y<3) */
    ASSERT_INT(org(f).y, o.y + 2, "top edge pans +2 (reveals world above)");
    ASSERT_INT(org(f).x, o.x,     "  top edge: x untouched");
    o = org(f);
    move_to(f, 40, 23);                       /* BOTTOM band (y>=21) */
    ASSERT_INT(org(f).y, o.y - 2, "bottom edge pans -2 (reveals world below)");
    o = org(f);
    move_to(f, 1, 1);                         /* top-left CORNER: both bands */
    ASSERT_INT(org(f).x, o.x + 2, "corner pans diagonally: x +2");
    ASSERT_INT(org(f).y, o.y + 2, "  corner: y +2");
    release_at(f, 1, 1);
    flow_free(f);
  }

  /* ---- connection-drag near the edge pans (load-bearing: the offset delta) ---- */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    int a = flow_add_node(f, "default", (flow_pt){10, 5}, (void*)"A");
    flow_set_hover(f, a);                     /* handles hit only on hovered nodes */
    flow_pt o0 = org(f);
    press_at(f, 14, 6);                       /* RIGHT handle of rect (10,5,5,3) */
    move_to(f, 79, 12);                       /* right band while conn_active */
    ASSERT_INT(org(f).x, o0.x - 2, "connection-drag near right edge auto-pans -2");
    ASSERT_INT(f->conn_active, 1, "  connection still in flight");
    ASSERT_INT(f->conn_end.x, 79, "  conn_end tracks the cursor (screen coords; sanity)");
    release_at(f, 79, 12);                    /* over empty -> cancel */
    flow_free(f);
  }

  /* ---- reconnect-drag near the edge pans ---- */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    int a = flow_add_node(f, "default", (flow_pt){10, 5}, (void*)"A");
    int b = flow_add_node(f, "default", (flow_pt){30, 12}, (void*)"B");
    int e = flow_add_edge(f, a, b, "out", "in");
    flow_pt tp; ASSERT_INT(flow_edge_endpoint_screen(f, flow_get_edge(f, e), 1, &tp), 1, "target endpoint resolves");
    flow_pt o0 = org(f);
    press_at(f, tp.x, tp.y);                  /* exact endpoint cell arms reconnect */
    ASSERT_INT(f->reconnect_edge, e, "reconnect drag armed");
    move_to(f, 79, 12);
    ASSERT_INT(org(f).x, o0.x - 2, "reconnect-drag near right edge auto-pans -2");
    release_at(f, 79, 12);                    /* over empty -> edge unchanged */
    ASSERT_INT(flow_get_edge(f, e)->target, b, "  drop on empty leaves target unchanged (sanity)");
    flow_free(f);
  }

  /* ---- pane-pan drag near the edge does NOT double-pan ---- */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    flow_pt o0 = org(f);
    press_at(f, 50, 10); move_to(f, 79, 10);  /* empty pane drag INTO the right band */
    ASSERT_INT(org(f).x, o0.x + 29, "pane-pan pans by the motion delta ONLY (+29, no autopan -2 on top)");
    release_at(f, 79, 10);
    flow_free(f);
  }

  /* ---- space-pan drag near the edge does NOT double-pan (composition with #5) ---- */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    flow_add_node(f, "default", (flow_pt){10, 5}, (void*)"A");
    feed(f, " ");                             /* space-pan mode ON */
    flow_pt o0 = org(f);
    press_at(f, 50, 10); move_to(f, 79, 10);
    ASSERT_INT(org(f).x, o0.x + 29, "space-drag pans by the motion delta ONLY (+29)");
    release_at(f, 79, 10);
    flow_free(f);
  }

  /* ---- marquee drag near the edge does NOT auto-pan ---- */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    flow_add_node(f, "default", (flow_pt){10, 5}, (void*)"A");
    flow_pt o0 = org(f);
    press_shift_at(f, 50, 10); move_shift_to(f, 79, 22);  /* into the bottom-right bands */
    ASSERT_INT(org(f).x, o0.x, "marquee near edge: x offset unchanged");
    ASSERT_INT(org(f).y, o0.y, "marquee near edge: y offset unchanged");
    release_at(f, 79, 22);
    flow_free(f);
  }

  /* ---- degenerate-axis guard: 2*margin >= extent disables that axis only ---- */
  {
    flow_t *f = flow_new(24, 6); flow_register_defaults(f);  /* rows 6: 2*3 >= 6 -> y-axis dead */
    flow_add_node(f, "default", (flow_pt){5, 2}, (void*)"A"); /* rect (5,2,5,3) */
    flow_pt o0 = org(f);
    press_at(f, 6, 3);
    move_to(f, 12, 1);                        /* y=1 would be in the top band; y-axis is dead, x interior */
    ASSERT_INT(org(f).y, o0.y, "y-axis dead on a 6-row buffer (bands would overlap)");
    ASSERT_INT(org(f).x, o0.x, "  x interior: no pan");
    move_to(f, 1, 1);                         /* x=1: LEFT band, x-axis alive (2*3 < 24) */
    ASSERT_INT(org(f).x, o0.x + 2, "x-axis still auto-pans on the same buffer");
    ASSERT_INT(org(f).y, o0.y,     "  y stays dead even at the corner");
    release_at(f, 1, 1);
    flow_free(f);
  }

  /* ---- the config fields are honored (speed) ---- */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    flow_add_node(f, "default", (flow_pt){10, 5}, (void*)"A");
    f->autopan_speed = 5;
    flow_pt o0 = org(f);
    press_at(f, 11, 6); move_to(f, 79, 12);
    ASSERT_INT(org(f).x, o0.x - 5, "autopan_speed=5 pans -5 per event");
    release_at(f, 79, 12);
    flow_free(f);
  }

  return flowtest_report("test_autopan");
}
