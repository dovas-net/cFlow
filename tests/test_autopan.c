#define FLOW_IMPLEMENTATION
#include "../flow.h"
#include "flowtest.h"

/* Auto-pan near viewport edge during drags (work package #6, spec §8).
   Event-driven model A: ONE autopan_speed step per motion event while an OBJECT drag
   (node-drag, connection-drag, reconnect-drag, marquee as of increment 4) has the
   cursor within autopan_margin cells of a buffer edge. Pane-pan drags are excluded.
   The pan is applied BEFORE the drag re-places its object, so the object stays under
   the cursor (marquee: the rect is recomputed at post-pan world coords each motion).
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

  /* ---- marquee drag near the edge DOES auto-pan (increment-4 contract INVERSION:
          this block previously asserted the offsets stayed unchanged) ---- */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    flow_add_node(f, "default", (flow_pt){10, 5}, (void*)"A");
    flow_pt o0 = org(f);
    press_shift_at(f, 50, 10); move_shift_to(f, 79, 22);  /* into the bottom-right bands */
    ASSERT_INT(org(f).x, o0.x - 2, "marquee near right edge: x DOES auto-pan (-2)");
    ASSERT_INT(org(f).y, o0.y - 2, "marquee near bottom edge: y DOES auto-pan (-2)");
    release_at(f, 79, 22);
    flow_free(f);
  }

  /* ---- marquee pan stability: interior motion after an edge pan does not reverse it ---- */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    flow_add_node(f, "default", (flow_pt){10, 5}, (void*)"A");
    flow_pt o0 = org(f);
    press_shift_at(f, 50, 10); move_shift_to(f, 79, 22);  /* edge bands: pan fires */
    int dpx = o0.x - org(f).x, dpy = o0.y - org(f).y;     /* record the pan delta */
    ASSERT_INT(dpx, 2, "stability precondition: x panned by speed");
    ASSERT_INT(dpy, 2, "stability precondition: y panned by speed");
    move_shift_to(f, 60, 15);                             /* interior: no further pan */
    ASSERT_INT(org(f).x, o0.x - dpx, "interior marquee motion keeps the pan (x stable)");
    ASSERT_INT(org(f).y, o0.y - dpy, "interior marquee motion keeps the pan (y stable)");
    release_at(f, 60, 15);
    flow_free(f);
  }

  /* ---- marquee on a degenerate axis: dead axis stays dead, live axis pans ---- */
  {
    flow_t *f = flow_new(24, 6); flow_register_defaults(f);  /* rows 6: 2*3 >= 6 -> y dead */
    flow_add_node(f, "default", (flow_pt){5, 2}, (void*)"A"); /* rect (5,2,5,3) */
    flow_pt o0 = org(f);
    press_shift_at(f, 15, 3);                 /* empty cell: arm marquee */
    move_shift_to(f, 1, 1);                   /* x in LEFT band (alive); y in top band (dead) */
    ASSERT_INT(org(f).x, o0.x + 2, "marquee: x-axis auto-pans on the small buffer");
    ASSERT_INT(org(f).y, o0.y,     "marquee: y-axis stays dead (bands overlap)");
    release_at(f, 1, 1);
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

  /* ---- public setter: flow_set_autopan (integration pass — the app-side knob) ---- */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    flow_set_autopan(f, 5, 3);
    ASSERT_INT(f->autopan_margin, 5, "flow_set_autopan sets margin");
    ASSERT_INT(f->autopan_speed,  3, "flow_set_autopan sets speed");
    flow_add_node(f, "default", (flow_pt){10, 5}, (void*)"A");
    flow_pt o0 = org(f);
    press_at(f, 11, 6); move_to(f, 79, 12);   /* node drag into the right band */
    ASSERT_INT(org(f).x, o0.x - 3, "setter-tuned speed honored by the drag path");
    release_at(f, 79, 12);
    flow_set_autopan(f, -2, -5);              /* negatives clamp to 0 (0 = disabled) */
    ASSERT_INT(f->autopan_margin, 0, "negative margin clamps to 0");
    ASSERT_INT(f->autopan_speed,  0, "negative speed clamps to 0");
    flow_free(f);
  }
  /* margin 0 disables — FRESH instance (the node above moved away from (11,6);
     pressing there again would start a pane-pan, not a node drag) */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    flow_set_autopan(f, 0, 3);
    flow_add_node(f, "default", (flow_pt){10, 5}, (void*)"A");
    flow_pt o0 = org(f);
    press_at(f, 11, 6); move_to(f, 79, 12);   /* node drag into the right band */
    ASSERT_INT(org(f).x, o0.x, "margin 0 disables auto-pan");
    release_at(f, 79, 12);
    flow_free(f);
  }

  /* ==== inc-6 #8 tick-autopan: ticked auto-pan during object drags ==== */

  /* 1. Ticks keep panning AND keep the node glued to the stationary cursor (the discriminator). */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    int a = flow_add_node(f, "default", (flow_pt){10, 5}, (void*)"A");
    flow_pt o0 = org(f);
    press_at(f, 11, 6);                          /* grab offset (1,1) */
    move_to(f, 79, 12);                          /* right band */
    ASSERT_INT(org(f).x, o0.x - 2, "motion pans -2 (landed single-event result)");
    ASSERT_INT(flow_get_node(f, a)->pos.x, 79 - org(f).x - 1, "node at post-pan world (glued)");
    ASSERT(flow__frames_armed(f) != 0, "node drag in flight arms the clock");
    flow__autopan_tick(f);                        /* NO new mouse event: tick replays the in-band motion */
    ASSERT_INT(org(f).x, o0.x - 4, "tick pans another -2");
    ASSERT_INT(flow_get_node(f, a)->pos.x, 79 - org(f).x - 1, "node follows the cursor (still glued)");
    flow__autopan_tick(f);
    ASSERT_INT(org(f).x, o0.x - 6, "second tick pans another -2");
    ASSERT_INT(flow_get_node(f, a)->pos.x, 79 - org(f).x - 1, "node still glued after two ticks");
    release_at(f, 79, 12);
    flow_free(f);
  }

  /* 2. Disarm on release — no runaway pan. */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    flow_add_node(f, "default", (flow_pt){10, 5}, (void*)"A");
    press_at(f, 11, 6); move_to(f, 79, 12);
    ASSERT(flow__frames_armed(f) != 0, "drag in band: armed");
    release_at(f, 79, 12);
    ASSERT_INT(flow__frames_armed(f), 0, "release: predicate no longer fires");
    flow_pt before = org(f);
    flow__autopan_tick(f);
    ASSERT_INT(org(f).x, before.x, "tick after release does not pan");
    flow__autopan_tick(f);
    ASSERT_INT(org(f).x, before.x, "still stable after a second post-release tick");
    flow_free(f);
  }

  /* 3. No pan when the cursor returns to the interior, but the drag stays armed. */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    flow_add_node(f, "default", (flow_pt){10, 5}, (void*)"A");
    press_at(f, 11, 6); move_to(f, 79, 12);      /* into the band */
    move_to(f, 40, 12);                           /* back to interior */
    ASSERT(flow__frames_armed(f) != 0, "drag still live: clock stays armed");
    flow_pt before = org(f);
    flow__autopan_tick(f);
    ASSERT_INT(org(f).x, before.x, "interior cursor: tick does not pan (flow__autopan no-ops)");
    flow__autopan_tick(f);
    ASSERT_INT(org(f).x, before.x, "still no runaway at the interior");
    release_at(f, 40, 12);
    flow_free(f);
  }

  /* 4. Marquee ticks pan and the gesture stays live. */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    flow_add_node(f, "default", (flow_pt){10, 5}, (void*)"A");
    flow_pt o0 = org(f);
    press_shift_at(f, 50, 10); move_shift_to(f, 79, 22);   /* bottom-right bands */
    ASSERT_INT(org(f).x, o0.x - 2, "marquee motion pans x -2");
    ASSERT_INT(org(f).y, o0.y - 2, "marquee motion pans y -2");
    ASSERT(flow__frames_armed(f) != 0, "marquee in flight: armed");
    flow__autopan_tick(f);
    ASSERT_INT(org(f).x, o0.x - 4, "marquee tick pans x another -2");
    ASSERT_INT(org(f).y, o0.y - 4, "marquee tick pans y another -2");
    ASSERT_INT(f->marquee_on, 1, "marquee gesture still live");
    release_at(f, 79, 22);
    flow_free(f);
  }

  /* 5. Connection-drag ticks pan; conn_end stays pinned at the cursor. */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    int a = flow_add_node(f, "default", (flow_pt){10, 5}, (void*)"A");
    flow_set_hover(f, a);
    flow_pt o0 = org(f);
    press_at(f, 14, 6);                           /* right handle */
    move_to(f, 79, 12);
    ASSERT_INT(f->conn_active, 1, "connection in flight");
    ASSERT_INT(org(f).x, o0.x - 2, "connection motion pans -2");
    ASSERT(flow__frames_armed(f) != 0, "connection arms the clock");
    flow__autopan_tick(f);
    ASSERT_INT(org(f).x, o0.x - 4, "connection tick pans another -2");
    ASSERT_INT(f->conn_active, 1, "connection still in flight");
    ASSERT_INT(f->conn_end.x, 79, "conn_end stays pinned at the cursor (screen coords)");
    release_at(f, 79, 12);
    flow_free(f);
  }

  /* 6. Reconnect-drag ticks pan-only (no object follows, endpoint unchanged until release). */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    int a = flow_add_node(f, "default", (flow_pt){10, 5}, (void*)"A");
    int b = flow_add_node(f, "default", (flow_pt){30, 12}, (void*)"B");
    int e = flow_add_edge(f, a, b, "out", "in");
    flow_pt tp; flow_edge_endpoint_screen(f, flow_get_edge(f, e), 1, &tp);
    flow_pt o0 = org(f);
    press_at(f, tp.x, tp.y);                      /* arm reconnect at the target endpoint */
    move_to(f, 79, 12);
    ASSERT_INT(f->reconnect_edge, e, "reconnect drag armed");
    ASSERT_INT(org(f).x, o0.x - 2, "reconnect motion pans -2");
    flow__autopan_tick(f);
    ASSERT_INT(org(f).x, o0.x - 4, "reconnect tick pans another -2");
    ASSERT_INT(f->reconnect_edge, e, "reconnect still armed");
    ASSERT_INT(flow_get_edge(f, e)->target, b, "target unchanged until release (pan-only)");
    release_at(f, 79, 12);
    flow_free(f);
  }

  /* 7. Pane-pan and space-pan never arm — the tick is a no-op for them. */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    flow_add_node(f, "default", (flow_pt){10, 5}, (void*)"A");
    press_at(f, 50, 10); move_to(f, 79, 10);      /* pane-pan into the band */
    ASSERT_INT(flow__frames_armed(f), 0, "pane-pan never arms");
    flow_pt before = org(f);
    flow__autopan_tick(f);
    ASSERT_INT(org(f).x, before.x, "tick is a no-op during a pane-pan");
    release_at(f, 79, 10);

    feed(f, " ");                                 /* space-pan mode ON */
    press_at(f, 50, 10); move_to(f, 79, 10);
    ASSERT_INT(flow__frames_armed(f), 0, "space-pan never arms");
    before = org(f);
    flow__autopan_tick(f);
    ASSERT_INT(org(f).x, before.x, "tick is a no-op during a space-pan");
    release_at(f, 79, 10);
    flow_free(f);
  }

  /* 8. Degenerate axis stays dead under ticks (the tick reads flow__autopan's guard, not a copy). */
  {
    flow_t *f = flow_new(24, 6); flow_register_defaults(f);   /* rows 6: 2*3 >= 6 -> y-axis dead */
    flow_add_node(f, "default", (flow_pt){5, 2}, (void*)"A");
    flow_pt o0 = org(f);
    press_at(f, 6, 3); move_to(f, 1, 1);          /* x in LEFT band (alive), y in dead top band */
    ASSERT_INT(org(f).x, o0.x + 2, "x-axis pans on the small buffer");
    ASSERT_INT(org(f).y, o0.y,     "y-axis dead (bands overlap)");
    flow__autopan_tick(f);
    ASSERT_INT(org(f).x, o0.x + 4, "tick keeps panning the live x-axis");
    ASSERT_INT(org(f).y, o0.y,     "y-axis NEVER moves under ticks");
    release_at(f, 1, 1);
    flow_free(f);
  }

  return flowtest_report("test_autopan");
}
