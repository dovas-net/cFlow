#define FLOW_IMPLEMENTATION
#include "../flow.h"
#include "flowtest.h"

/* cells_to_string: same helper as test_render.c (statusbar snapshot) */
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

static int g_hits = 0;
static void inc_hits(flow_t *f, void *u) { (void)f; (void)u; g_hits++; }
static int g_xcustom = 0;
/* connect-lifecycle ESC tracking (inc-4 #7) */
static int key_cend_fires = 0, key_cend_eid = -2, key_cend_src = -2;
static void key_on_cend(flow_t *f, int eid, int src, int tgt, void *u) {
  (void)f;(void)tgt;(void)u; key_cend_fires++; key_cend_eid = eid; key_cend_src = src;
}
static void x_custom(flow_t *f, void *u) { (void)f; (void)u; g_xcustom++; }
static int g_ctrlup = 0;
static void ctrl_up(flow_t *f, void *u) { (void)f; (void)u; g_ctrlup++; }

static void mark_overlay(flow_t *f, flow_surface *s, void *u) {
  (void)f; (void)u; flow_put(s, 0, 0, '@', FLOW_FG, FLOW_BG, 0);
}

int main(void) {
  /* ---- delete selected node via 'x' cascades its edges ---- */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    int a = flow_add_node(f, "default", (flow_pt){10, 5}, (void*)"A");
    int b = flow_add_node(f, "default", (flow_pt){30, 5}, (void*)"B");
    flow_add_edge(f, a, b, "", "");
    flow_select_node(f, a, 0);
    flow_feed(f, "x", 1);
    ASSERT(flow_get_node(f, a) == NULL, "'x' removed selected node A");
    ASSERT_INT(flow_node_count(f), 1, "one node remains after 'x'");
    ASSERT_INT(flow_edge_count(f), 0, "incident edge cascaded away");
    /* delete the remaining node via Delete escape seq */
    flow_select_node(f, b, 0);
    flow_feed(f, "\x1b[3~", 4);
    ASSERT(flow_get_node(f, b) == NULL, "Delete seq removed node B");
    ASSERT_INT(flow_node_count(f), 0, "no nodes after Delete");
    flow_free(f);
  }

  /* ---- edge delete: select an edge, 'x' with no node selected removes only it ---- */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    int a = flow_add_node(f, "default", (flow_pt){10, 5}, (void*)"A");
    int b = flow_add_node(f, "default", (flow_pt){30, 5}, (void*)"B");
    int c = flow_add_node(f, "default", (flow_pt){50, 5}, (void*)"C");
    int e1 = flow_add_edge(f, a, b, "", "");
    flow_add_edge(f, b, c, "", "");
    flow_get_edge(f, e1)->flags |= FLOW_SELECTED;
    ASSERT_INT(flow_selected_edge(f), e1, "flow_selected_edge returns selected edge");
    int before = flow_edge_count(f);
    flow_feed(f, "x", 1);  /* no node selected */
    ASSERT_INT(flow_edge_count(f), before - 1, "'x' removed only the selected edge");
    ASSERT_INT(flow_node_count(f), 3, "nodes untouched by edge delete");
    ASSERT(flow_get_edge(f, e1) == NULL, "selected edge gone");
    flow_free(f);
  }

  /* ---- add-node center via 'n' ---- */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    flow_feed(f, "n", 1);
    ASSERT_INT(flow_node_count(f), 1, "'n' added a node");
    flow_node *n = &flow_nodes(f)[0];
    flow_rect r = flow_node_rect_abs(f, n);
    flow_pt center = flow_to_world(f, (flow_pt){80/2, 24/2});
    int cx = r.x + r.w/2, cy = r.y + r.h/2;
    ASSERT(cx - center.x <= 1 && center.x - cx <= 1, "new node centered in x (~within 1)");
    ASSERT(cy - center.y <= 1 && center.y - cy <= 1, "new node centered in y (~within 1)");
    flow_free(f);
  }

  /* ---- fit view via 'f': off-screen nodes recentred with equal margins ---- */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    /* off-screen, but bounds small enough to fit inside the 80x24 viewport */
    flow_add_node(f, "default", (flow_pt){200, 100}, (void*)"X");
    flow_add_node(f, "default", (flow_pt){240, 110}, (void*)"Y");
    flow_feed(f, "f", 1);
    flow_rect b = flow_bounds(f);
    flow_pt tl = flow_to_screen(f, (flow_pt){b.x, b.y});
    flow_pt br = flow_to_screen(f, (flow_pt){b.x + b.w - 1, b.y + b.h - 1});
    ASSERT(tl.x >= 0 && br.x < 80, "fit: bounds within screen x");
    ASSERT(tl.y >= 0 && br.y < 24, "fit: bounds within screen y");
    int leftm = tl.x, rightm = 80 - 1 - br.x;
    ASSERT(leftm - rightm <= 1 && rightm - leftm <= 1, "fit: centred horizontally (margins ~equal)");
    int topm = tl.y, botm = 24 - 1 - br.y;
    ASSERT(topm - botm <= 1 && botm - topm <= 1, "fit: centred vertically (margins ~equal)");
    flow_free(f);
  }
  /* empty flow: 'f' is a no-op, no crash */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    flow_feed(f, "f", 1);
    ASSERT_INT(flow_node_count(f), 0, "fit on empty graph no-op");
    flow_free(f);
  }

  /* ---- flow_bind_key: register, override built-in, multi-byte longest-prefix ---- */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    g_hits = 0;
    flow_bind_key(f, "g", inc_hits, NULL);
    flow_feed(f, "g", 1);
    ASSERT_INT(g_hits, 1, "bound 'g' fn ran from flow_feed");

    /* override 'x' built-in: selected node must survive */
    int a = flow_add_node(f, "default", (flow_pt){10, 5}, (void*)"A");
    flow_select_node(f, a, 0);
    g_xcustom = 0;
    flow_bind_key(f, "x", x_custom, NULL);
    flow_feed(f, "x", 1);
    ASSERT_INT(g_xcustom, 1, "custom 'x' binding ran");
    ASSERT(flow_get_node(f, a) != NULL, "registry overrides built-in delete (node survives)");

    /* multi-byte seq: Ctrl-Up consumes all 6 bytes */
    g_ctrlup = 0;
    flow_bind_key(f, "\x1b[1;5A", ctrl_up, NULL);
    flow_feed(f, "\x1b[1;5A", 6);
    ASSERT_INT(g_ctrlup, 1, "multi-byte binding longest-prefix matched");
    flow_free(f);
  }

  /* ---- dispatch isolation: arrows still pan, 'q' not consumed ---- */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    flow_viewport v0 = flow_view_get(f);
    flow_feed(f, "\x1b[A", 3);  /* up */
    flow_viewport v1 = flow_view_get(f);
    ASSERT(v1.oy != v0.oy, "bare arrow still pans (not swallowed by dispatch)");
    ASSERT_INT(flow_dispatch_key(f, "q", 1), 0, "'q' not consumed by dispatch");
    ASSERT_INT(flow_dispatch_key(f, "\x1b[A", 3), 0, "bare arrow not consumed by dispatch");
    flow_free(f);
  }

  /* ---- statusbar: render last, does not clobber row 0 node; toggle via '?' ---- */
  {
    int cols = 30, rows = 6;
    flow_cell *buf = (flow_cell*)malloc((size_t)cols * rows * sizeof(flow_cell));
    flow_t *f = flow_new(cols, rows); flow_register_defaults(f);
    flow_add_node(f, "default", (flow_pt){0, 0}, (void*)"hi");  /* box on row 0 */
    flow_feed(f, "?", 1);  /* toggle statusbar on */
    flow_render(f, buf, cols, rows);
    char *s = cells_to_string(buf, cols, rows);
    SNAPSHOT("render_statusbar", s);
    free(s);
    /* row 0 still has a box corner; status row at rows-1 has text */
    ASSERT_INT(buf[0*cols + 0].ch, 0x250C, "node box intact on row 0 (statusbar drawn below)");
    int statusrow_has_text = 0;
    for (int x = 0; x < cols; x++) if (buf[(rows-1)*cols + x].ch != ' ' && buf[(rows-1)*cols + x].ch != 0) statusrow_has_text = 1;
    ASSERT(statusrow_has_text, "status row has content");
    /* toggle off */
    flow_feed(f, "?", 1);
    flow_render(f, buf, cols, rows);
    int statusrow_empty = 1;
    for (int x = 0; x < cols; x++) { uint32_t ch = buf[(rows-1)*cols + x].ch; if (ch != ' ' && ch != 0) statusrow_empty = 0; }
    ASSERT(statusrow_empty, "status row cleared after toggle off");
    free(buf); flow_free(f);
  }

  /* ---- statusbar + app overlay coexist (different rows) ---- */
  {
    int cols = 30, rows = 6;
    flow_cell *buf = (flow_cell*)malloc((size_t)cols * rows * sizeof(flow_cell));
    flow_t *f = flow_new(cols, rows); flow_register_defaults(f);
    flow_callbacks cb = {0}; cb.on_overlay = mark_overlay; flow_set_callbacks(f, cb);
    flow_set_statusbar(f, 1);
    flow_render(f, buf, cols, rows);
    ASSERT_INT(buf[0*cols + 0].ch, '@', "app overlay marker present at (0,0)");
    int statusrow_has_text = 0;
    for (int x = 0; x < cols; x++) if (buf[(rows-1)*cols + x].ch != ' ' && buf[(rows-1)*cols + x].ch != 0) statusrow_has_text = 1;
    ASSERT(statusrow_has_text, "status row present alongside overlay");
    free(buf); flow_free(f);
  }

  /* ---- statusbar help mentions Space-pan and undo/redo (integration pass) ----
     The hints are APPENDED past column 30 so the render_statusbar golden (rendered
     at cols=30, locking only the " n:add  x:del  f:fit  ?:help" prefix) stays
     byte-identical; a real-width terminal shows the full line. */
  {
    int cols = 80, rows = 4;
    flow_cell *buf = (flow_cell*)malloc((size_t)cols * rows * sizeof(flow_cell));
    flow_t *f = flow_new(cols, rows); flow_register_defaults(f);
    flow_set_statusbar(f, 1);
    flow_render(f, buf, cols, rows);
    char row[81];
    for (int x = 0; x < cols; x++) { uint32_t ch = buf[(rows-1)*cols + x].ch; row[x] = (ch >= 32 && ch < 127) ? (char)ch : ' '; }
    row[cols] = 0;
    ASSERT(strstr(row, " n:add  x:del  f:fit  ?:help  q:quit") == row, "locked prefix unchanged");
    ASSERT(strstr(row, "SPC:pan") != NULL, "help mentions Space-pan");
    ASSERT(strstr(row, "u:undo") != NULL, "help mentions undo");
    ASSERT(strstr(row, "^r:redo") != NULL, "help mentions redo");
    free(buf); flow_free(f);
  }

  /* ---- free-after-remove: remove all nodes, then flow_free (ASan clean) ---- */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    int a = flow_add_node(f, "default", (flow_pt){0,0}, (void*)"A");
    int b = flow_add_node(f, "default", (flow_pt){20,0}, (void*)"B");
    int e = flow_add_edge(f, a, b, "", "");
    flow_get_edge(f, e)->label = strdup("lbl");  /* exercise label free on remove */
    flow_remove_node(f, a);
    ASSERT_INT(flow_edge_count(f), 0, "remove_node freed labelled edge");
    flow_remove_node(f, b);
    ASSERT_INT(flow_node_count(f), 0, "all nodes removed");
    flow_free(f);
  }

  /* ---- multi-edge delete: ALL selected edges go, not just the first ---- */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    int a = flow_add_node(f, "default", (flow_pt){10, 5}, (void*)"A");
    int b = flow_add_node(f, "default", (flow_pt){30, 5}, (void*)"B");
    int c = flow_add_node(f, "default", (flow_pt){50, 5}, (void*)"C");
    int e1 = flow_add_edge(f, a, b, "", "");
    int e2 = flow_add_edge(f, b, c, "", "");
    flow_get_edge(f, e1)->flags |= FLOW_SELECTED;
    flow_get_edge(f, e2)->flags |= FLOW_SELECTED;   /* both selected, no node selected */
    int before = flow_edge_count(f);
    flow_delete_selection(f);
    ASSERT_INT(flow_edge_count(f), before - 2, "delete_selection removed BOTH selected edges");
    ASSERT_INT(flow_node_count(f), 3, "multi-edge delete left nodes intact");
    flow_free(f);
  }

  /* ---- many-node delete: 70 selected nodes all go (no fixed-cap truncation) ---- */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    for (int i = 0; i < 70; i++) {
      int id = flow_add_node(f, "default", (flow_pt){i, 0}, (void*)"x");
      flow_select_node(f, id, 1);                   /* additive: select all 70 */
    }
    flow_add_node(f, "default", (flow_pt){0, 10}, (void*)"keep");  /* unselected survivor */
    ASSERT_INT(flow_node_count(f), 71, "71 nodes before delete");
    flow_delete_selection(f);
    ASSERT_INT(flow_node_count(f), 1, "delete_selection removed all 70 selected (no cap)");
    flow_free(f);
  }

  /* ---- lone ESC during a connection fires on_connect_end and clears state (inc-4 #7) ---- */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    int a = flow_add_node(f, "default", (flow_pt){10, 5}, (void*)"A");
    flow_callbacks cb = {0}; cb.on_connect_end = key_on_cend; flow_set_callbacks(f, cb);
    key_cend_fires = 0; key_cend_eid = -2; key_cend_src = -2;
    flow_set_hover(f, a);
    flow_feed(f, "\x1b[<0;15;7M", 10);                  /* press A:out -> connection in flight */
    ASSERT_INT(flow_connecting(f), 1, "connection in flight before ESC");
    flow_feed(f, "\x1b", 1);                            /* lone ESC aborts */
    ASSERT_INT(flow_connecting(f), 0, "ESC cleared the connection state");
    ASSERT_INT(key_cend_fires, 1, "ESC cancel fired on_connect_end once");
    ASSERT_INT(key_cend_eid, -1, "  eid -1 (no edge)");
    ASSERT_INT(key_cend_src, a, "  source A");
    flow_feed(f, "\x1b", 1);                            /* ESC again: nothing in flight */
    ASSERT_INT(key_cend_fires, 1, "idempotent ESC fires NO further on_connect_end");
    flow_free(f);
  }

  /* ---- Shift-arrow selection nudge (inc-5 #6): \x1b[1;2{A,B,C,D} moves the
     FLOW_SELECTED set 1 world cell (A up, B down, C right, D left); bare arrows
     keep panning. One undo step per press; extent clamps apply; empty selection
     is a consumed no-op (no pan fallback, no journal step). ---- */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    int a = flow_add_node(f, "default", (flow_pt){10, 10}, (void*)"A");
    flow_select_node(f, a, 0);
    int j0 = f->journal.n;
    flow_feed(f, "\x1b[1;2A", 6);
    ASSERT_INT(flow_get_node(f, a)->pos.y, 9,  "Shift-Up: world y -1");
    flow_feed(f, "\x1b[1;2B", 6);
    ASSERT_INT(flow_get_node(f, a)->pos.y, 10, "Shift-Down: world y +1 (back)");
    flow_feed(f, "\x1b[1;2C", 6);
    ASSERT_INT(flow_get_node(f, a)->pos.x, 11, "Shift-Right: world x +1");
    flow_feed(f, "\x1b[1;2D", 6);
    ASSERT_INT(flow_get_node(f, a)->pos.x, 10, "Shift-Left: world x -1 (back)");
    ASSERT_INT(f->journal.n, j0 + 4, "one undo step per press");
    flow_feed(f, "u", 1);                               /* undo the last nudge (D) */
    flow_node *au = flow_get_node(f, a);                /* guarded: in a broken state undo
                                                           could pop the ADD instead */
    ASSERT(au != NULL && au->pos.x == 11, "undo reverses the last nudge");

    /* MULTI-RIGID: two top-level nodes share one delta, one journal step */
    int b = flow_add_node(f, "default", (flow_pt){0, 0}, (void*)"B");
    int c = flow_add_node(f, "default", (flow_pt){5, 0}, (void*)"C");
    flow_select_node(f, b, 0); flow_select_node(f, c, 1);
    int j1 = f->journal.n;
    flow_feed(f, "\x1b[1;2C", 6);
    ASSERT_INT(flow_get_node(f, b)->pos.x, 1, "multi-rigid: B moved +1");
    ASSERT_INT(flow_get_node(f, c)->pos.x, 6, "multi-rigid: C moved +1 (same delta)");
    ASSERT_INT(f->journal.n, j1 + 1, "multi-nudge = ONE journal step");

    /* NO-DOUBLE-MOVE: selected child of a selected parent follows, not adds */
    int p  = flow_add_node(f, "default", (flow_pt){30, 10}, (void*)"P");
    flow_node *pn = flow_get_node(f, p); pn->w = 20; pn->h = 10;   /* parent rect (30,10,20,10) */
    int ch = flow_add_node(f, "default", (flow_pt){35, 12}, (void*)"k");
    flow_set_parent(f, ch, p);                                     /* abs preserved */
    flow_select_node(f, p, 0); flow_select_node(f, ch, 1);
    flow_pt ca0 = flow_node_abs(f, flow_get_node(f, ch));
    flow_feed(f, "\x1b[1;2B", 6);
    flow_pt ca1 = flow_node_abs(f, flow_get_node(f, ch));
    ASSERT_INT(ca1.y, ca0.y + 1, "selected child of selected parent moves exactly +1 (not +2)");
    ASSERT_INT(flow_get_node(f, p)->pos.y, 11, "parent (the root) moved +1");

    /* CHILD-EXTENT CLAMP: nudge into the parent wall is clamped (unchanged) */
    flow_get_node(f, ch)->flags |= FLOW_EXTENT_PARENT;
    flow_move_node(f, ch, (flow_pt){35, 18});            /* flush: 18+3 == 21 == parent bottom (11+10) */
    flow_select_node(f, ch, 0);                          /* child only */
    flow_pt cb0 = flow_node_abs(f, flow_get_node(f, ch));
    flow_feed(f, "\x1b[1;2B", 6);
    flow_pt cb1 = flow_node_abs(f, flow_get_node(f, ch));
    ASSERT_INT(cb1.y, cb0.y, "nudge into the parent wall: clamped, abs unchanged");

    /* PLAIN-ARROW PAN STILL WORKS (no regression) */
    float oy0 = f->view.oy;
    flow_feed(f, "\x1b[A", 3);
    ASSERT(f->view.oy != oy0, "bare arrow still pans");

    /* EMPTY-SELECTION NO-OP: consumed, no move, no journal step, no pan */
    flow_clear_selection(f);
    int j2 = f->journal.n; float oy1 = f->view.oy; int bx = flow_get_node(f, b)->pos.x;
    flow_feed(f, "\x1b[1;2A", 6);
    ASSERT_INT(f->journal.n, j2, "empty selection: no journal step");
    ASSERT(f->view.oy == oy1, "empty selection: did NOT fall through to pan");
    ASSERT_INT(flow_get_node(f, b)->pos.x, bx, "empty selection: nothing moved");
    flow_free(f);
  }

  return flowtest_report("test_keys");
}
