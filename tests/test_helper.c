#define FLOW_IMPLEMENTATION
#include "../flow.h"
#include "flowtest.h"
#include <string.h>

/* Alignment helper lines + snap on drag (inc-5 #8). Opt-in via
   flow_set_helper_lines (default OFF = byte-for-byte the landed drag path).
   Single-node drag only: when a dragged edge (L/R/T/B) comes within 1 cell of
   a VISIBLE neighbor's matching-axis edge, the drag snaps onto it and a
   full-row/column dashed guide (vert 0x254E, horz 0x254C) is recorded/drawn.
   Guides honor flow__node_visible (view-level) and clear on release. */

static void feed(flow_t *f, const char *s) { flow_feed(f, s, (int)strlen(s)); }
static void press_at(flow_t *f, int sx, int sy)   { char b[32]; snprintf(b, sizeof b, "\x1b[<0;%d;%dM",  sx + 1, sy + 1); feed(f, b); }
static void move_to(flow_t *f, int sx, int sy)    { char b[32]; snprintf(b, sizeof b, "\x1b[<32;%d;%dM", sx + 1, sy + 1); feed(f, b); }
static void release_at(flow_t *f, int sx, int sy) { char b[32]; snprintf(b, sizeof b, "\x1b[<0;%d;%dm",  sx + 1, sy + 1); feed(f, b); }

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

/* fixture: A fixed at (10,5) 4x3 (x-edges {10,14}, y-edges {5,8}); B starts at
   (start_x, start_y) 6x5 — widths/heights differ from A's by >1 so only ONE
   edge can match at a time. zoom 1, no pan: screen == world. */
static flow_t *mk(int *A, int *B, int bx, int by) {
  flow_t *f = flow_new(80, 30); flow_register_defaults(f);
  *A = flow_add_node(f, "default", (flow_pt){10, 5}, (void*)"A");
  flow_node *an = flow_get_node(f, *A); an->w = 4; an->h = 3;
  *B = flow_add_node(f, "default", (flow_pt){bx, by}, (void*)"B");
  flow_node *bn = flow_get_node(f, *B); bn->w = 6; bn->h = 5;
  return f;
}

int main(void) {
  /* ---- 1: alignment detected at the exact offset ---- */
  {
    int A, B; flow_t *f = mk(&A, &B, 30, 20);
    flow_set_helper_lines(f, 1);
    press_at(f, 32, 22);                  /* grab offset (2,2) */
    move_to(f, 12, 22);                   /* prospective left = 10 == A.left */
    ASSERT_INT(flow_node_abs(f, flow_get_node(f, B)).x, 10, "exact alignment: B.left at 10");
    ASSERT_INT(f->helper.nvert, 1, "one vertical guide");
    ASSERT_INT(f->helper.vert[0], 10, "  at the shared edge x=10");
    ASSERT_INT(f->helper.nhorz, 0, "no horizontal guide (y-edges clear)");
    release_at(f, 12, 22);
    flow_free(f);
  }

  /* ---- 2: snap pulls a one-cell-off drag onto the guide ---- */
  {
    int A, B; flow_t *f = mk(&A, &B, 30, 20);
    flow_set_helper_lines(f, 1);
    press_at(f, 32, 22);
    move_to(f, 13, 22);                   /* prospective left = 11 -> snaps to 10 */
    ASSERT_INT(flow_node_abs(f, flow_get_node(f, B)).x, 10, "snap pulled left edge to 10");
    ASSERT_INT(f->helper.nvert, 1, "guide recorded at the snapped edge");
    ASSERT_INT(f->helper.vert[0], 10, "  x=10");
    release_at(f, 13, 22);
    flow_free(f);
  }

  /* ---- 3: no snap outside tolerance ---- */
  {
    int A, B; flow_t *f = mk(&A, &B, 30, 20);
    flow_set_helper_lines(f, 1);
    press_at(f, 32, 22);
    move_to(f, 19, 22);                   /* prospective left = 17: >1 from every A edge */
    ASSERT_INT(flow_node_abs(f, flow_get_node(f, B)).x, 17, "unsnapped outside tolerance");
    ASSERT_INT(f->helper.nvert, 0, "no guide registered");
    release_at(f, 19, 22);
    flow_free(f);
  }

  /* ---- 4: per-axis independence (y snaps, x untouched) ---- */
  {
    int A, B; flow_t *f = mk(&A, &B, 40, 20);
    flow_set_helper_lines(f, 1);
    press_at(f, 42, 22);                  /* grab (2,2) */
    move_to(f, 42, 8);                    /* prospective top = 6 -> snaps to A.top 5; x stays 40 */
    ASSERT_INT(flow_node_abs(f, flow_get_node(f, B)).y, 5, "top edge snapped to 5");
    ASSERT_INT(flow_node_abs(f, flow_get_node(f, B)).x, 40, "x axis untouched");
    ASSERT_INT(f->helper.nhorz, 1, "one horizontal guide");
    ASSERT_INT(f->helper.horz[0], 5, "  at y=5");
    ASSERT_INT(f->helper.nvert, 0, "no vertical guide");
    release_at(f, 42, 8);
    flow_free(f);
  }

  /* ---- 5: hidden candidate seeds nothing (view-level gate) ---- */
  {
    int A, B; flow_t *f = mk(&A, &B, 30, 20);
    flow_set_helper_lines(f, 1);
    flow_set_node_hidden(f, A, 1);
    press_at(f, 32, 22);
    move_to(f, 13, 22);                   /* would snap to hidden A's edge */
    ASSERT_INT(flow_node_abs(f, flow_get_node(f, B)).x, 11, "hidden neighbor: NOT snapped");
    ASSERT_INT(f->helper.nvert, 0, "hidden neighbor: no guide");
    release_at(f, 13, 22);
    flow_free(f);
  }

  /* ---- 6: guides cleared on release ---- */
  {
    int A, B; flow_t *f = mk(&A, &B, 30, 20);
    flow_set_helper_lines(f, 1);
    press_at(f, 32, 22);
    move_to(f, 12, 22);
    ASSERT_INT(f->helper.nvert, 1, "guide active mid-drag");
    release_at(f, 12, 22);
    ASSERT_INT(f->helper.nvert, 0, "release clears vertical guides");
    ASSERT_INT(f->helper.nhorz, 0, "release clears horizontal guides");
    ASSERT_INT(f->drag_node, -1, "drag disarmed");
    flow_free(f);
  }

  /* ---- 7: toggle OFF reproduces the landed drag exactly ---- */
  {
    int A, B; flow_t *f = mk(&A, &B, 30, 20);
    flow_set_helper_lines(f, 0);          /* explicit OFF (also the default) */
    press_at(f, 32, 22);
    move_to(f, 13, 22);                   /* one cell off: would snap if ON */
    ASSERT_INT(flow_node_abs(f, flow_get_node(f, B)).x, 11, "OFF: no snap (landed behavior)");
    ASSERT_INT(f->helper.nvert, 0, "OFF: no guides");
    /* and the mid-drag render carries no guide glyphs */
    flow_cell *buf = (flow_cell*)malloc((size_t)80 * 30 * sizeof(flow_cell));
    flow_render(f, buf, 80, 30);
    int glyphs = 0;
    for (int i = 0; i < 80 * 30; i++)
      if (buf[i].ch == 0x254E || buf[i].ch == 0x254C) glyphs++;
    ASSERT_INT(glyphs, 0, "OFF: no guide glyph anywhere in the frame");
    free(buf);
    release_at(f, 13, 22);
    /* default-OFF: a fresh engine never snaps without the setter */
    flow_t *g = flow_new(80, 30); flow_register_defaults(g);
    ASSERT_INT(g->helper_on, 0, "calloc default is OFF");
    flow_free(f); flow_free(g);
  }

  /* ---- 8: mid-drag render golden with an active vertical guide ---- */
  {
    int cols = 40, rows = 12;
    flow_t *f = flow_new(cols, rows); flow_register_defaults(f);
    int A = flow_add_node(f, "default", (flow_pt){10, 5}, (void*)"A");
    flow_node *an = flow_get_node(f, A); an->w = 4; an->h = 3;
    int B = flow_add_node(f, "default", (flow_pt){30, 10}, (void*)"B");
    flow_node *bn = flow_get_node(f, B); bn->w = 6; bn->h = 5;
    flow_set_helper_lines(f, 1);
    press_at(f, 32, 11);                  /* grab (2,1); B.top edge y10: clear of A's {5,8} */
    move_to(f, 12, 11);                   /* prospective left = 10 == A.left -> guide */
    ASSERT_INT(f->helper.nvert, 1, "golden scene: guide active");
    flow_cell *buf = (flow_cell*)malloc((size_t)cols * rows * sizeof(flow_cell));
    flow_render(f, buf, cols, rows);
    char *s = cells_to_string(buf, cols, rows);
    ASSERT(strstr(s, "\xe2\x95\x8e") != NULL, "vertical guide glyph 0x254E present");
    SNAPSHOT("render_helper_snap", s);
    free(s); free(buf);
    release_at(f, 12, 11);
    flow_free(f);
  }

  return flowtest_report("test_helper");
}
