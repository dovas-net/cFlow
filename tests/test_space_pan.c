#define FLOW_IMPLEMENTATION
#include "../flow.h"
#include "flowtest.h"

/* Space-drag pan (work package #5, spec §8 "Space-drag").
   Terminal model A: Space is a STICKY pan-mode toggle (a TTY has no key-up), routed
   through flow_dispatch_key so it is overridable via flow_bind_key. While space_held,
   a drag — over a node OR the empty pane — pans the viewport instead of moving/selecting. */

static void feed(flow_t *f, const char *s) { flow_feed(f, s, (int)strlen(s)); }

/* synthetic SGR-1006 at a 0-based screen cell (SGR is 1-based) */
static void press_at(flow_t *f, int sx, int sy)   { char b[32]; snprintf(b, sizeof b, "\x1b[<0;%d;%dM",  sx + 1, sy + 1); feed(f, b); }
static void move_to(flow_t *f, int sx, int sy)    { char b[32]; snprintf(b, sizeof b, "\x1b[<32;%d;%dM", sx + 1, sy + 1); feed(f, b); }
static void release_at(flow_t *f, int sx, int sy) { char b[32]; snprintf(b, sizeof b, "\x1b[<0;%d;%dm",  sx + 1, sy + 1); feed(f, b); }

static void on_pane(flow_t *f, flow_pt w, void *u)      { (void)f; (void)w; (*(int*)u)++; }
static void on_space_custom(flow_t *f, void *u)         { (void)f; (*(int*)u)++; }

int main(void) {
  /* ---- dispatch: Space toggles space_held through the flow_dispatch_key built-in ---- */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    ASSERT_INT(f->space_held, 0, "space_held starts off (calloc zero-init)");
    feed(f, " ");
    ASSERT_INT(f->space_held, 1, "feed Space toggles space_held ON (built-in)");
    feed(f, " ");
    ASSERT_INT(f->space_held, 0, "feed Space again toggles OFF (sticky toggle, model A)");
    flow_free(f);
  }

  /* ---- space-drag over a NODE pans (node unchanged); toggle off restores the move ---- */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    int a = flow_add_node(f, "default", (flow_pt){10, 5}, (void*)"A");  /* rect (10,5,5,3), screen==world */

    feed(f, " ");                                       /* space-pan ON */
    flow_pt org0 = flow_to_screen(f, (flow_pt){0, 0});
    press_at(f, 11, 6); move_to(f, 21, 6);              /* drag inside node, dx=+10 */
    flow_pt org1 = flow_to_screen(f, (flow_pt){0, 0});
    ASSERT_INT(org1.x, org0.x + 10, "space-drag over a node PANS the viewport +10");
    ASSERT_INT(flow_get_node(f, a)->pos.x, 10, "  node x unchanged (panned, not moved)");
    ASSERT_INT(flow_get_node(f, a)->pos.y, 5,  "  node y unchanged");
    ASSERT_INT(flow_selected_count(f), 0, "  space-drag over a node selects nothing");
    release_at(f, 21, 6);

    feed(f, " ");                                       /* space-pan OFF */
    /* after the +10 pan, node world (10,5) sits at screen (20,5); drag it +10 in world */
    flow_pt org2 = flow_to_screen(f, (flow_pt){0, 0});
    press_at(f, 21, 6); move_to(f, 31, 6);
    ASSERT_INT(flow_get_node(f, a)->pos.x, 20, "space OFF: same gesture MOVES the node +10 (normal behavior restored)");
    ASSERT_INT(flow_to_screen(f, (flow_pt){0, 0}).x, org2.x, "  viewport did NOT pan while moving the node");
    release_at(f, 31, 6);
    flow_free(f);
  }

  /* ---- space-drag over the empty pane still pans (no regression) ---- */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    feed(f, " ");
    flow_pt o0 = flow_to_screen(f, (flow_pt){0, 0});
    press_at(f, 50, 10); move_to(f, 53, 10);            /* empty pane, dx=+3 */
    ASSERT_INT(flow_to_screen(f, (flow_pt){0, 0}).x, o0.x + 3, "space-drag over empty pane still pans +3");
    release_at(f, 53, 10);
    flow_free(f);
  }

  /* ---- a space-drag does NOT marquee or disturb an existing selection ---- */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    int a = flow_add_node(f, "default", (flow_pt){10, 5}, (void*)"A");
    flow_select_node(f, a, 0);
    ASSERT_INT(flow_selected_count(f), 1, "precondition: node a selected");
    feed(f, " ");
    press_at(f, 11, 6); move_to(f, 21, 6); release_at(f, 21, 6);   /* space-drag across the node */
    ASSERT_INT(flow_selected_count(f), 1, "space-drag preserves selection (no marquee, no clear)");
    flow_free(f);
  }

  /* ---- no-drag space-click over a node body is a no-op (callback contract: NOT a pane click) ---- */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    int a = flow_add_node(f, "default", (flow_pt){10, 5}, (void*)"A");
    flow_select_node(f, a, 0);
    int pane_clicks = 0;
    flow_callbacks cb = {0}; cb.on_pane_click = on_pane; cb.user = &pane_clicks;
    flow_set_callbacks(f, cb);
    feed(f, " ");
    press_at(f, 11, 6); release_at(f, 11, 6);           /* click on the node, no motion */
    ASSERT_INT(flow_selected_count(f), 1, "no-drag space-click does NOT clear selection");
    ASSERT_INT(pane_clicks, 0, "no-drag space-click over a node does NOT fire on_pane_click");
    flow_free(f);
  }

  /* ---- flow_bind_key(\" \") overrides the built-in (registry-before-builtin) ---- */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    int custom = 0;
    flow_bind_key(f, " ", on_space_custom, &custom);
    feed(f, " ");
    ASSERT_INT(custom, 1, "bound \" \" fn runs instead of the space-pan toggle");
    ASSERT_INT(f->space_held, 0, "  built-in suppressed: space_held NOT set");
    int a = flow_add_node(f, "default", (flow_pt){10, 5}, (void*)"A");
    press_at(f, 11, 6); move_to(f, 21, 6);              /* no space_held -> ordinary node drag */
    ASSERT_INT(flow_get_node(f, a)->pos.x, 20, "  node drag still moves (space-pan not engaged)");
    release_at(f, 21, 6);
    flow_free(f);
  }

  /* ---- Esc exits pan mode (integration-pass alias for the sticky toggle) ---- */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    feed(f, " ");
    ASSERT_INT(f->space_held, 1, "Space arms pan mode");
    feed(f, "\x1b");                                    /* lone ESC (not a CSI prefix) */
    ASSERT_INT(f->space_held, 0, "lone Esc exits pan mode");
    feed(f, "\x1b");
    ASSERT_INT(f->space_held, 0, "Esc with pan off stays off (no toggle)");
    flow_free(f);
  }

  /* ---- statusbar shows a pan-mode hint while space_held ---- */
  {
    int cols = 40, rows = 6;
    flow_cell *buf = (flow_cell*)malloc((size_t)cols * rows * sizeof(flow_cell));
    flow_t *f = flow_new(cols, rows); flow_register_defaults(f);
    flow_set_statusbar(f, 1);
    char row[41];
    flow_render(f, buf, cols, rows);
    for (int x = 0; x < cols; x++) { uint32_t ch = buf[(rows-1)*cols + x].ch; row[x] = (ch >= 32 && ch < 127) ? (char)ch : ' '; }
    row[cols] = 0;
    ASSERT(strstr(row, "n:add") != NULL, "pan off: normal help line");
    ASSERT(strstr(row, "PAN") == NULL, "pan off: no PAN hint");
    feed(f, " ");                                       /* pan mode ON */
    flow_render(f, buf, cols, rows);
    for (int x = 0; x < cols; x++) { uint32_t ch = buf[(rows-1)*cols + x].ch; row[x] = (ch >= 32 && ch < 127) ? (char)ch : ' '; }
    row[cols] = 0;
    ASSERT(strstr(row, "PAN") != NULL, "pan on: statusbar shows the PAN hint");
    feed(f, " ");                                       /* back OFF: normal help returns */
    flow_render(f, buf, cols, rows);
    for (int x = 0; x < cols; x++) { uint32_t ch = buf[(rows-1)*cols + x].ch; row[x] = (ch >= 32 && ch < 127) ? (char)ch : ' '; }
    row[cols] = 0;
    ASSERT(strstr(row, "n:add") != NULL, "pan off again: normal help restored");
    free(buf); flow_free(f);
  }

  return flowtest_report("test_space_pan");
}
