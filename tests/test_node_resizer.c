#define FLOW_IMPLEMENTATION
#include "../flow.h"
#include "flowtest.h"
#include <string.h>

/* inc-8 #3 — node-resizer: an interactive SE-corner resize grip (◢ 0x25E2) on the lone
   selected node, ported from xyflow's NodeResizer. Off by default (flow_set_resizer); when
   enabled it is drawn only on the single selected node, at LOD 0, while unlocked. The grip is
   RENDER-ONLY — NOT a widgets[] hit-rect: that cache feeds the click-fire-and-consume widget
   loop, which runs above the lock gate and would consume the press before a drag could begin.
   The press-arm does its own geometric hit-test via the SAME flow__resize_marker the render
   uses, so drawn == hittable by construction. A drag writes through flow_set_node_size
   (SIZE-only, NW origin fixed), clamps w,h >= 1, and brackets flow__undo_begin/end so the
   whole gesture coalesces into ONE undo step (the rail package 2 already journals). */

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

#define GRIP 0x25E2u   /* ◢ BLACK LOWER RIGHT TRIANGLE — the SE resize grip glyph */

/* SE corner screen cell of node `id` at zoom 1 / no pan (screen == world). */
static flow_pt se_corner(flow_t *f, int id) {
  flow_node *n = flow_get_node(f, id);
  flow_pt a = flow_node_abs(f, n);
  flow_pt p = { a.x + n->w - 1, a.y + n->h - 1 };
  return p;
}
static int count_glyph(const flow_cell *c, int n, uint32_t g) {
  int k = 0; for (int i = 0; i < n; i++) if (c[i].ch == g) k++; return k;
}

int main(void) {
  const char *P = "/tmp/flow_node_resizer.json";

  /* 1: the grip is drawn iff the resizer is enabled AND exactly one node is selected */
  {
    int cols = 20, rows = 12;
    flow_cell *buf = (flow_cell*)malloc((size_t)cols*rows*sizeof(flow_cell));
    flow_t *f = flow_new(cols, rows); flow_register_defaults(f);
    int a = flow_add_node(f, "default", (flow_pt){5, 5}, (void*)"X");  /* w=5,h=3 */
    flow_pt se = se_corner(f, a);                                      /* (9,7) */

    flow_set_resizer(f, 1);
    flow_select_node(f, a, 0);
    flow_render(f, buf, cols, rows);
    ASSERT_INT(count_glyph(buf, cols*rows, GRIP), 1, "exactly one grip with 1 selected");
    ASSERT_INT(buf[se.y*cols + se.x].ch, GRIP, "grip sits on the SE corner cell");

    flow_clear_selection(f);
    flow_render(f, buf, cols, rows);
    ASSERT_INT(count_glyph(buf, cols*rows, GRIP), 0, "no grip when nothing selected");

    int b = flow_add_node(f, "default", (flow_pt){12, 5}, (void*)"Y");
    flow_select_node(f, a, 0); flow_select_node(f, b, 1);
    ASSERT_INT(flow_selected_count(f), 2, "two selected");
    flow_render(f, buf, cols, rows);
    ASSERT_INT(count_glyph(buf, cols*rows, GRIP), 0, "no grip with >1 selected (single-selection chrome)");
    free(buf); flow_free(f);
  }

  /* 2: OFF by default — a selected node shows NO grip, and a press on the SE corner
     does NOT arm a resize (it falls through to the normal node interaction). This is the
     toggle that keeps every existing selected-node golden byte-identical. */
  {
    int cols = 20, rows = 12;
    flow_cell *buf = (flow_cell*)malloc((size_t)cols*rows*sizeof(flow_cell));
    flow_t *f = flow_new(cols, rows); flow_register_defaults(f);
    int a = flow_add_node(f, "default", (flow_pt){5, 5}, (void*)"X");
    ASSERT_INT(f->resizer.enabled, 0, "calloc default is OFF");
    flow_select_node(f, a, 0);
    flow_render(f, buf, cols, rows);
    ASSERT_INT(count_glyph(buf, cols*rows, GRIP), 0, "default-OFF: no grip on a selected node");
    flow_pt se = se_corner(f, a);
    int w0 = flow_get_node(f, a)->w;
    press_at(f, se.x, se.y);
    ASSERT_INT(f->resize_node, -1, "default-OFF: corner press does not arm resize");
    move_to(f, se.x + 3, se.y + 3);
    release_at(f, se.x + 3, se.y + 3);
    ASSERT_INT(flow_get_node(f, a)->w, w0, "default-OFF: corner press+drag did not resize");
    free(buf); flow_free(f);
  }

  /* 3: drag the SE grip outward grows the node — size-only, NW origin fixed */
  {
    int cols = 30, rows = 16;
    flow_cell *buf = (flow_cell*)malloc((size_t)cols*rows*sizeof(flow_cell));
    flow_t *f = flow_new(cols, rows); flow_register_defaults(f);
    int a = flow_add_node(f, "default", (flow_pt){5, 5}, (void*)"X");  /* w=5,h=3 */
    flow_set_resizer(f, 1); flow_select_node(f, a, 0);
    flow_render(f, buf, cols, rows);
    flow_pt se = se_corner(f, a);                       /* (9,7) */
    flow_pt p0 = flow_node_abs(f, flow_get_node(f, a));
    press_at(f, se.x, se.y);
    ASSERT_INT(f->resize_node, a, "press on grip arms the resize");
    move_to(f, se.x + 4, se.y + 2);                     /* dw=+4, dh=+2 => 9x5 */
    release_at(f, se.x + 4, se.y + 2);
    ASSERT_INT(flow_get_node(f, a)->w, 9, "drag grew width to 9");
    ASSERT_INT(flow_get_node(f, a)->h, 5, "drag grew height to 5");
    ASSERT(flow_get_node(f, a)->flags & FLOW_EXPLICIT_SIZE, "resize sets FLOW_EXPLICIT_SIZE");
    flow_pt p1 = flow_node_abs(f, flow_get_node(f, a));
    ASSERT_INT(p1.x, p0.x, "origin x fixed (size-only)");
    ASSERT_INT(p1.y, p0.y, "origin y fixed (size-only)");
    free(buf); flow_free(f);
  }

  /* 4: dragging inward past the minimum clamps w,h to >= 1 (the flow_set_node_size floor) */
  {
    int cols = 30, rows = 16;
    flow_cell *buf = (flow_cell*)malloc((size_t)cols*rows*sizeof(flow_cell));
    flow_t *f = flow_new(cols, rows); flow_register_defaults(f);
    int a = flow_add_node(f, "default", (flow_pt){5, 5}, (void*)"X");  /* w=5,h=3 */
    flow_set_resizer(f, 1); flow_select_node(f, a, 0);
    flow_render(f, buf, cols, rows);
    flow_pt se = se_corner(f, a);                       /* (9,7) */
    press_at(f, se.x, se.y);
    move_to(f, se.x - 6, se.y - 6);                     /* (3,1): well inside the origin */
    release_at(f, se.x - 6, se.y - 6);
    ASSERT_INT(flow_get_node(f, a)->w, 1, "width clamps to >= 1");
    ASSERT_INT(flow_get_node(f, a)->h, 1, "height clamps to >= 1");
    free(buf); flow_free(f);
  }

  /* 5: the whole gesture coalesces to ONE undo step; undo restores the pre-gesture size,
     redo re-applies. Mirrors tests/test_explicit_size.c scenario 8, driven via the gesture. */
  {
    int cols = 30, rows = 16;
    flow_cell *buf = (flow_cell*)malloc((size_t)cols*rows*sizeof(flow_cell));
    flow_t *f = flow_new(cols, rows); flow_register_defaults(f);
    int a = flow_add_node(f, "default", (flow_pt){5, 5}, (void*)"X");  /* w=5,h=3 */
    flow_set_resizer(f, 1); flow_select_node(f, a, 0);
    flow_render(f, buf, cols, rows);
    flow_pt se = se_corner(f, a);
    int d0 = flow_undo_depth(f);
    press_at(f, se.x, se.y);
    move_to(f, se.x + 2, se.y + 1);                     /* first motion: 7x4 */
    move_to(f, se.x + 6, se.y + 4);                     /* second motion (fp recomputed): 11x7 */
    release_at(f, se.x + 6, se.y + 4);
    ASSERT_INT(flow_undo_depth(f), d0 + 1, "coalesced resize gesture = ONE undo step");
    ASSERT_INT(flow_get_node(f, a)->w, 11, "final width = 5 + 6");
    ASSERT_INT(flow_get_node(f, a)->h, 7,  "final height = 3 + 4");
    flow_undo(f);
    ASSERT_INT(flow_get_node(f, a)->w, 5, "one undo reverts to pre-gesture width");
    ASSERT_INT(flow_get_node(f, a)->h, 3, "one undo reverts to pre-gesture height");
    flow_redo(f);
    ASSERT_INT(flow_get_node(f, a)->w, 11, "redo re-applies the resize");
    free(buf); flow_free(f);
  }

  /* 6: a click on the grip with no drag is a no-op — empty undo bracket records nothing */
  {
    int cols = 30, rows = 16;
    flow_cell *buf = (flow_cell*)malloc((size_t)cols*rows*sizeof(flow_cell));
    flow_t *f = flow_new(cols, rows); flow_register_defaults(f);
    int a = flow_add_node(f, "default", (flow_pt){5, 5}, (void*)"X");
    flow_set_resizer(f, 1); flow_select_node(f, a, 0);
    flow_render(f, buf, cols, rows);
    flow_pt se = se_corner(f, a);
    int d0 = flow_undo_depth(f);
    press_at(f, se.x, se.y);
    release_at(f, se.x, se.y);                          /* same cell: no motion */
    ASSERT_INT(flow_get_node(f, a)->w, 5, "no-drag click leaves width");
    ASSERT_INT(flow_get_node(f, a)->h, 3, "no-drag click leaves height");
    ASSERT_INT(flow_undo_depth(f), d0, "no-drag click records no undo step (empty bracket)");
    ASSERT_INT(f->resize_node, -1, "release disarms after a no-drag click");
    free(buf); flow_free(f);
  }

  /* 7: locked — no grip drawn and a corner press+drag does not resize */
  {
    int cols = 30, rows = 16;
    flow_cell *buf = (flow_cell*)malloc((size_t)cols*rows*sizeof(flow_cell));
    flow_t *f = flow_new(cols, rows); flow_register_defaults(f);
    int a = flow_add_node(f, "default", (flow_pt){5, 5}, (void*)"X");
    flow_set_resizer(f, 1); flow_select_node(f, a, 0);
    flow_set_locked(f, 1);
    flow_render(f, buf, cols, rows);
    ASSERT_INT(count_glyph(buf, cols*rows, GRIP), 0, "locked: no grip drawn");
    flow_pt se = se_corner(f, a);
    press_at(f, se.x, se.y);
    ASSERT_INT(f->resize_node, -1, "locked: press does not arm resize");
    move_to(f, se.x + 4, se.y + 4);
    release_at(f, se.x + 4, se.y + 4);
    ASSERT_INT(flow_get_node(f, a)->w, 5, "locked: corner drag did not resize");
    free(buf); flow_free(f);
  }

  /* 8: after a resize drag, the gesture fully disarms (resize_node + mouse_down cleared) */
  {
    int cols = 30, rows = 16;
    flow_cell *buf = (flow_cell*)malloc((size_t)cols*rows*sizeof(flow_cell));
    flow_t *f = flow_new(cols, rows); flow_register_defaults(f);
    int a = flow_add_node(f, "default", (flow_pt){5, 5}, (void*)"X");
    flow_set_resizer(f, 1); flow_select_node(f, a, 0);
    flow_render(f, buf, cols, rows);
    flow_pt se = se_corner(f, a);
    press_at(f, se.x, se.y);
    move_to(f, se.x + 3, se.y + 3);
    release_at(f, se.x + 3, se.y + 3);
    ASSERT_INT(f->resize_node, -1, "resize_node cleared on release");
    ASSERT_INT(f->mouse_down, 0, "mouse_down cleared on release");
    free(buf); flow_free(f);
  }

  /* 9: a gesture-driven resize persists across save/load (end-to-end through the rail) */
  {
    int cols = 30, rows = 16;
    flow_cell *buf = (flow_cell*)malloc((size_t)cols*rows*sizeof(flow_cell));
    flow_t *f = flow_new(cols, rows); flow_register_defaults(f);
    int a = flow_add_node(f, "default", (flow_pt){5, 5}, (void*)"X");
    flow_set_resizer(f, 1); flow_select_node(f, a, 0);
    flow_render(f, buf, cols, rows);
    flow_pt se = se_corner(f, a);
    press_at(f, se.x, se.y);
    move_to(f, se.x + 8, se.y + 5);                     /* => 13x8 */
    release_at(f, se.x + 8, se.y + 5);
    ASSERT_INT(flow_save(f, P), 0, "save gesture-resized graph");
    flow_t *g = flow_new(cols, rows); flow_register_defaults(g);
    ASSERT_INT(flow_load(g, P), 0, "load gesture-resized graph");
    ASSERT_INT(g->nodes[0].w, 13, "gesture-resized width persisted");
    ASSERT_INT(g->nodes[0].h, 8,  "gesture-resized height persisted");
    ASSERT(g->nodes[0].flags & FLOW_EXPLICIT_SIZE, "EXPLICIT_SIZE persisted");
    free(buf); flow_free(f); flow_free(g);
  }

  /* 10: render golden — a single selected node with the resizer enabled (◢ at SE corner).
     OFF-by-default keeps the existing selected-node goldens identical; this is the one new
     golden. (SNAPSHOT line added after GREEN, per the package discipline.) */
  {
    int cols = 16, rows = 7;
    flow_cell *buf = (flow_cell*)malloc((size_t)cols*rows*sizeof(flow_cell));
    flow_t *f = flow_new(cols, rows); flow_register_defaults(f);
    int a = flow_add_node(f, "default", (flow_pt){2, 2}, (void*)"N");  /* w=5,h=3 */
    flow_set_resizer(f, 1); flow_select_node(f, a, 0);
    flow_render(f, buf, cols, rows);
    char *s = cells_to_string(buf, cols, rows);
    ASSERT(strstr(s, "\xe2\x97\xa2") != NULL, "golden: SE resize grip ◢ present");
    SNAPSHOT("render_node_resizer", s);
    free(s);
    free(buf); flow_free(f);
  }

  /* 11: at zoom 2 (still LOD 0) the grip tracks the cursor 1:1 in SCREEN cells — the
     footprint-delta form, NOT a flow_to_world delta. Node bodies are constant glyph size
     (only position scales with zoom), so a size delta is zoom-independent; a flow_to_world
     port would divide the drag by zoom and grow the node by ~half, janking. This is the one
     scenario that discriminates the two — all the others run at zoom 1 (screen == world). */
  {
    int cols = 40, rows = 20;
    flow_cell *buf = (flow_cell*)malloc((size_t)cols*rows*sizeof(flow_cell));
    flow_t *f = flow_new(cols, rows); flow_register_defaults(f);
    int a = flow_add_node(f, "default", (flow_pt){5, 5}, (void*)"X");  /* w=5,h=3 */
    f->view.zoom = 2.0f; f->view.ox = 0; f->view.oy = 0;              /* zoom 2, no pan -> LOD 0 */
    flow_set_resizer(f, 1); flow_select_node(f, a, 0);
    flow_render(f, buf, cols, rows);
    flow_node *n = flow_get_node(f, a);
    flow_pt tl = flow_to_screen(f, flow_node_abs(f, n));               /* (10,10) at zoom 2 */
    flow_pt se = { tl.x + n->w - 1, tl.y + n->h - 1 };                 /* (14,12) */
    ASSERT_INT(buf[se.y*cols + se.x].ch, GRIP, "zoom 2: grip on the projected SE corner");
    press_at(f, se.x, se.y);
    ASSERT_INT(f->resize_node, a, "zoom 2: press arms resize");
    move_to(f, se.x + 3, se.y + 2);                                   /* +3,+2 SCREEN cells */
    release_at(f, se.x + 3, se.y + 2);
    ASSERT_INT(flow_get_node(f, a)->w, 8, "zoom 2: width grew by the SCREEN delta (5+3), not /zoom");
    ASSERT_INT(flow_get_node(f, a)->h, 5, "zoom 2: height grew by the SCREEN delta (3+2), not /zoom");
    free(buf); flow_free(f);
  }

  /* 12: v1 scopes the resizer to LOD 0 on BOTH press AND motion. Zooming below the LOD
     threshold mid-drag FREEZES the resize — otherwise the footprint collapses to a 1x1 marker
     and the SE-corner delta jumps discontinuously. The gesture resumes cleanly back at LOD 0. */
  {
    flow_t *f = flow_new(30, 16); flow_register_defaults(f);
    int a = flow_add_node(f, "default", (flow_pt){5, 5}, (void*)"X");  /* w=5,h=3 */
    flow_set_resizer(f, 1); flow_select_node(f, a, 0);
    flow_pt se = se_corner(f, a);                       /* (9,7) at zoom 1 (LOD 0) */
    press_at(f, se.x, se.y);
    move_to(f, se.x + 3, se.y + 2);                     /* LOD 0: grow to 8x5 */
    ASSERT_INT(flow_get_node(f, a)->w, 8, "LOD0 grow before zoom (w)");
    ASSERT_INT(flow_get_node(f, a)->h, 5, "LOD0 grow before zoom (h)");
    f->view.zoom = 0.5f;                                /* < FLOW_LOD_THRESHOLD (0.6) -> LOD 1 */
    move_to(f, 0, 0);                                   /* a motion at LOD 1 */
    ASSERT_INT(flow_get_node(f, a)->w, 8, "LOD1 motion is frozen (no erratic delta)");
    ASSERT_INT(flow_get_node(f, a)->h, 5, "LOD1 motion is frozen (h)");
    f->view.zoom = 1.0f;                                /* back to LOD 0 */
    flow_pt se2 = se_corner(f, a);                      /* SE of the 8x5 node: (12,9) */
    move_to(f, se2.x + 1, se2.y + 1);                   /* resume: grow by (1,1) -> 9x6 */
    ASSERT_INT(flow_get_node(f, a)->w, 9, "resume at LOD0 tracks again (w)");
    ASSERT_INT(flow_get_node(f, a)->h, 6, "resume at LOD0 tracks again (h)");
    release_at(f, se2.x + 1, se2.y + 1);
    ASSERT_INT(f->resize_node, -1, "release disarms");
    flow_free(f);
  }

  return flowtest_report("test_node_resizer");
}
