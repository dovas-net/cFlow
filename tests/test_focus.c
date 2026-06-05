#define FLOW_IMPLEMENTATION
#include "../flow.h"
#include "flowtest.h"
#include <math.h>

/* Tab focus traversal + focus ring + frame-on-focus (inc-5 #5).
   Focus is an int id field on struct flow (sentinel -1) — the keyboard analog of
   hover: Tab/Shift-Tab cycle VISIBLE nodes in insertion order (wrapping), Enter
   selects the focused node (replace), lone-ESC clears focus, delete/hide of the
   focused node invalidates to -1, and focusing a fully-offscreen node re-centres
   the viewport via flow_set_center (a partially/fully visible node never jumps). */

static int sel_fires = 0;
static void on_sel(flow_t *f, const int *ids, int n, void *u) {
  (void)f; (void)ids; (void)n; (void)u; sel_fires++;
}

static void feed(flow_t *f, const char *s) { flow_feed(f, s, (int)strlen(s)); }

#define ASSERT_F(got, want, msg) ASSERT(fabsf((float)(got) - (float)(want)) < 0.01f, msg)

int main(void) {
  /* ---- traversal: Tab cycles insertion order, wraps; Shift-Tab reverses ---- */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    int a = flow_add_node(f, "default", (flow_pt){0, 0},  (void*)"A");
    int b = flow_add_node(f, "default", (flow_pt){20, 0}, (void*)"B");
    int c = flow_add_node(f, "default", (flow_pt){40, 0}, (void*)"C");
    ASSERT_INT(flow_focused_node(f), -1, "no focus initially");
    feed(f, "\t");
    ASSERT_INT(flow_focused_node(f), a, "Tab from none lands on first visible");
    feed(f, "\t");
    ASSERT_INT(flow_focused_node(f), b, "Tab advances in insertion order");
    feed(f, "\t");
    ASSERT_INT(flow_focused_node(f), c, "Tab reaches last");
    feed(f, "\t");
    ASSERT_INT(flow_focused_node(f), a, "Tab wraps to first");
    feed(f, "\x1b[Z");
    ASSERT_INT(flow_focused_node(f), c, "Shift-Tab wraps backward to last");

    /* hidden nodes are skipped + cannot be focused */
    flow_set_node_hidden(f, b, 1);
    flow_set_focus(f, a);
    ASSERT_INT(flow_focused_node(f), a, "set_focus lands on a visible node");
    feed(f, "\t");
    ASSERT_INT(flow_focused_node(f), c, "Tab skips the hidden node");
    flow_set_focus(f, b);
    ASSERT_INT(flow_focused_node(f), -1, "cannot focus a hidden node (clears)");
    flow_set_node_hidden(f, b, 0);

    /* invalidation: delete and hide of the FOCUSED node both clear to -1 */
    flow_set_focus(f, a);
    flow_remove_node(f, a);
    ASSERT_INT(flow_focused_node(f), -1, "delete invalidates focus");
    flow_set_focus(f, b);
    flow_set_node_hidden(f, b, 1);
    ASSERT_INT(flow_focused_node(f), -1, "hide invalidates focus");
    flow_set_node_hidden(f, b, 0);

    /* Enter selects the focused node (REPLACE, sig-gated event) */
    flow_callbacks cb = {0}; cb.on_selection_change = on_sel; flow_set_callbacks(f, cb);
    flow_select_node(f, b, 0);                    /* pre-select another node */
    flow_set_focus(f, c);
    sel_fires = 0;
    feed(f, "\r");
    ASSERT_INT(flow_selected_node(f), c, "Enter selects the focused node");
    ASSERT_INT(flow_selected_count(f), 1, "Enter REPLACES the selection");
    ASSERT_INT(sel_fires, 1, "Enter fires on_selection_change once");
    feed(f, "\r");
    ASSERT_INT(sel_fires, 1, "re-Enter on same selection fires nothing (sig-gated)");

    /* Enter with no focus is a consumed no-op */
    flow_set_focus(f, -1);
    sel_fires = 0;
    feed(f, "\r");
    ASSERT_INT(flow_selected_count(f), 1, "Enter with no focus mutates nothing");
    ASSERT_INT(sel_fires, 0, "  and fires nothing");

    /* lone-ESC clears focus (4th idempotent ESC action) AND selection */
    flow_set_focus(f, c);
    feed(f, "\x1b");
    ASSERT_INT(flow_focused_node(f), -1, "ESC clears focus");
    ASSERT_INT(flow_selected_count(f), 0, "ESC still clears selection");
    flow_free(f);
  }

  /* ---- degenerate traversal: empty graph, all hidden ---- */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    feed(f, "\t");
    ASSERT_INT(flow_focused_node(f), -1, "Tab on empty graph stays -1");
    int a = flow_add_node(f, "default", (flow_pt){0, 0}, (void*)"A");
    flow_set_node_hidden(f, a, 1);
    feed(f, "\t");
    ASSERT_INT(flow_focused_node(f), -1, "Tab with all nodes hidden stays -1");
    feed(f, "\x1b[Z");
    ASSERT_INT(flow_focused_node(f), -1, "Shift-Tab with all hidden stays -1");
    flow_free(f);
  }

  /* ---- frame-on-focus: only fully-offscreen nodes re-centre the viewport ---- */
  {
    flow_t *g = flow_new(80, 24); flow_register_defaults(g);
    int on  = flow_add_node(g, "default", (flow_pt){10, 5},   (void*)"O");
    int far = flow_add_node(g, "default", (flow_pt){300, 100}, (void*)"F");
    float ox0 = g->view.ox, oy0 = g->view.oy;
    flow_set_focus(g, on);
    ASSERT_F(g->view.ox, ox0, "focusing a visible node does not move the view");
    ASSERT_F(g->view.oy, oy0, "  y too");
    flow_set_focus(g, far);
    ASSERT(g->view.ox != ox0 || g->view.oy != oy0, "focusing an offscreen node re-centres");
    {  /* the focused node's screen rect now intersects the viewport */
      flow_node *fn = flow_get_node(g, far);
      flow_rect wr = flow_node_rect_abs(g, fn);
      flow_pt tl = flow_to_screen(g, (flow_pt){wr.x, wr.y});
      ASSERT(tl.x < 80 && tl.y < 24 && tl.x + wr.w > 0 && tl.y + wr.h > 0,
             "framed node is on-screen");
    }
    float ox1 = g->view.ox, oy1 = g->view.oy;
    flow_set_focus(g, far);                        /* already visible now */
    ASSERT_F(g->view.ox, ox1, "re-focusing the now-visible node does not jump");
    ASSERT_F(g->view.oy, oy1, "  y too");
    /* keyboard path frames too: Tab to the offscreen node */
    flow_t *h = flow_new(80, 24); flow_register_defaults(h);
    flow_add_node(h, "default", (flow_pt){10, 5},   (void*)"O");
    int hf = flow_add_node(h, "default", (flow_pt){300, 100}, (void*)"F");
    feed(h, "\t"); feed(h, "\t");                  /* focus the far node */
    ASSERT_INT(flow_focused_node(h), hf, "Tab reached the offscreen node");
    ASSERT(h->view.ox != 0.0f || h->view.oy != 0.0f, "Tab onto offscreen node framed it");
    flow_free(g); flow_free(h);
  }

  /* ---- focus ring: reverse-video border post-pass + golden ---- */
  {
    int cols = 30, rows = 8;
    flow_t *f = flow_new(cols, rows); flow_register_defaults(f);
    int a = flow_add_node(f, "default", (flow_pt){0, 0},  (void*)"A");
    flow_add_node(f, "default", (flow_pt){12, 4}, (void*)"B");
    flow_cell *buf = (flow_cell*)malloc((size_t)cols * rows * sizeof(flow_cell));

    flow_set_focus(f, a);
    flow_render(f, buf, cols, rows);
    /* mechanical guards so even the snapshot-creation run proves the ring:
       A's border top-left (screen 0,0) carries REVERSE; B's (12,4) does not;
       A's interior label cell (2,1) is NOT reversed (border-only ring). */
    ASSERT(buf[0*cols + 0].attr & FLOW_REVERSE, "focused node border is reverse-video");
    ASSERT(buf[0*cols + 4].attr & FLOW_REVERSE, "  whole top edge ringed (x=4)");
    ASSERT(buf[2*cols + 0].attr & FLOW_REVERSE, "  left edge ringed (y=2)");
    ASSERT(!(buf[4*cols + 12].attr & FLOW_REVERSE), "unfocused node has no ring");
    ASSERT(!(buf[1*cols + 2].attr & FLOW_REVERSE), "ring is border-only (interior clean)");
    {
      char *s = (char*)malloc((size_t)cols * rows + rows + 1); int len = 0;
      for (int y = 0; y < rows; y++) {            /* attr map: R = reversed cell */
        for (int x = 0; x < cols; x++) s[len++] = (buf[y*cols+x].attr & FLOW_REVERSE) ? 'R' : '.';
        s[len++] = '\n';
      }
      s[len] = 0;
      SNAPSHOT("render_focus", s);                /* ring attr-map golden */
      free(s);
    }

    /* focused + SELECTED: handle markers draw, and the ring must not gap at the
       ◉ anchor cells (the ring pass runs AFTER the marker pass and ORs REVERSE) */
    flow_select_node(f, a, 0);
    flow_set_focus(f, a);
    flow_render(f, buf, cols, rows);
    ASSERT_INT((int)buf[1*cols + 4].ch, 0x25C9, "selected+focused: handle marker drawn");
    ASSERT(buf[1*cols + 4].attr & FLOW_REVERSE, "  and the ring covers the handle cell (no gap)");
    flow_clear_selection(f);

    /* clearing focus removes the ring */
    flow_set_focus(f, -1);
    flow_render(f, buf, cols, rows);
    ASSERT(!(buf[0*cols + 0].attr & FLOW_REVERSE), "no focus, no ring");
    free(buf); flow_free(f);
  }

  return flowtest_report("test_focus");
}
