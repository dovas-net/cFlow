/* hello_flow — increment-6 + increment-7 showcase. Three nodes, two edges, plus
   everything the increments added, on the poll-driven redraw clock (#4).

   increment-7 (the feel layer):
     · theme / colorMode        — 't' cycles DEFAULT -> DARK -> LIGHT; all engine chrome
                                   (canvas, grid, edges, handles, minimap, statusbar,
                                   controls, toolbars) recolors. Node bodies keep FLOW_FG/BG.
     · live connection feedback — drag a wire from a node's right handle: the candidate
                                   target handle turns green (valid) / red (duplicate/self).
     · Controls bar             — top-right [+][-][fit][lock], clickable with the mouse.
     · lock mode                — 'L' or the [lock] button: drag/connect/select are frozen,
                                   pan + zoom still work (a read-only / presentation view).
     · node toolbar             — select ONE node: a [del][dup] strip appears above it.
     · edge toolbar             — select an edge: a [✕] button rides its midpoint.

   increment-6:
     · #5 marching-ants edges   — ON at launch; 'e' toggles. The clock arms itself
                                   while any edge is animated (10 Hz), idle otherwise.
     · #7 devtools HUD          — bottom-right panel; 'i' toggles. counts / viewport /
                                   focused node / undo+redo depth / most-recent op.
     · #6 modal command palette — '/' opens a live node finder. While open it is MODAL:
                                   Delete, arrows, Tab etc. are CAPTURED (they no longer
                                   act on the graph behind it). Type to select a match;
                                   Enter/Esc close.
     · #8 tick-driven auto-pan  — drag a node (mouse) to a screen edge and HOLD: the view
                                   keeps scrolling under the stationary cursor.
   Also: arrows pan · x/Del delete · n add · f fit · ? help bar · q quit. */
#define FLOW_IMPLEMENTATION
#include "../flow.h"
#include <string.h>
#include <stdio.h>

static int hud_on  = 1;                 /* #7 HUD visible at launch */
static int anim_on = 1;                 /* #5 ants marching at launch */
static struct { int open; char q[32]; int n; } pal;   /* #6 modal palette state */

static const char *op_name(int op) {    /* #7: flow_top_op -> a readable tag */
  switch (op) {
    case FLOW_CMD_ADD_NODE:       return "add-node";
    case FLOW_CMD_REMOVE_NODE:    return "rm-node";
    case FLOW_CMD_ADD_EDGE:       return "add-edge";
    case FLOW_CMD_REMOVE_EDGE:    return "rm-edge";
    case FLOW_CMD_MOVE_NODE:      return "move";
    case FLOW_CMD_RECONNECT_EDGE: return "reconnect";
    case FLOW_CMD_SET_LABEL:      return "label";
    case FLOW_CMD_REPARENT:       return "reparent";
    default:                      return "-";          /* -1 empty sentinel */
  }
}

/* #6 palette hook: while open, the engine routes EVERY key here first and DROPS anything
   this returns 0 for (modal capture). Printables build the query and live-select the first
   matching node; Backspace edits; Enter/Esc close. Returning 0 for a CSI (arrows/Delete)
   lets the engine drop it — so it never reaches the graph. */
static int pal_hook(flow_t *f, const char *seq, int len, void *user) {
  (void)user;
  if (!pal.open) return 0;                              /* closed: normal dispatch */
  unsigned char c = (unsigned char)seq[0];
  if (c == 0x1b) {                                      /* ESC: CSI -> let modal drop it; lone ESC -> close */
    if (len >= 2 && seq[1] == '[') return 0;
    pal.open = 0; flow_set_key_hook_modal(f, 0); return 1;
  }
  if (c == '\r') { pal.open = 0; flow_set_key_hook_modal(f, 0); return 1; }   /* Enter: keep selection, close */
  if (c == 0x7f || c == 0x08) { if (pal.n > 0) pal.q[--pal.n] = 0; }          /* Backspace */
  else if (c >= 0x20 && c < 0x7f) {                                            /* printable: append */
    if (pal.n < (int)sizeof pal.q - 1) { pal.q[pal.n++] = (char)c; pal.q[pal.n] = 0; }
  } else return 1;                                      /* other control byte: capture (drop) */
  if (pal.n > 0) { int hit; if (flow_find_nodes(f, pal.q, &hit, 1) > 0) flow_select_node(f, hit, 0); }
  return 1;
}

static void overlay(flow_t *f, flow_surface *s, void *u) {
  (void)u;
  flow_text(s, 1, 0, "e:anim i:hud /:find t:theme L:lock  select->toolbar  corner->controls",
            FLOW_FG, FLOW_BG, FLOW_BOLD);
  if (pal.open) {                                       /* #6 the modal find box */
    char line[80]; int out[1]; int n = flow_find_nodes(f, pal.q, out, 1);
    snprintf(line, sizeof line, " find: %s_  (%d match%s) ", pal.q, n, n == 1 ? "" : "es");
    flow_text(s, 1, 2, line, FLOW_FG, FLOW_BG, FLOW_REVERSE);
  }
  if (hud_on) {                                         /* #7 devtools panes */
    flow_viewport v = flow_view_get(f);
    int w = 26, h = 8;
    int px = flow_surface_w(s) - w - 1, py = flow_surface_h(s) - h - 2;
    char line[64];
    flow_box(s, px, py, w, h, FLOW_FG, FLOW_BG, FLOW_BOLD);
    flow_text(s, px + 2, py + 1, "devtools", FLOW_FG, FLOW_BG, FLOW_BOLD);
    snprintf(line, sizeof line, "nodes %d edges %d sel %d", flow_node_count(f), flow_edge_count(f), flow_selected_count(f));
    flow_text(s, px + 2, py + 2, line, FLOW_FG, FLOW_BG, 0);
    snprintf(line, sizeof line, "view %.0f,%.0f  z%.2f", v.ox, v.oy, v.zoom);
    flow_text(s, px + 2, py + 3, line, FLOW_FG, FLOW_BG, 0);
    int fn = flow_focused_node(f); flow_node *fnd = fn != -1 ? flow_get_node(f, fn) : NULL;
    snprintf(line, sizeof line, "focus: %s", fnd && fnd->data ? (const char*)fnd->data : (fn != -1 ? "?" : "none"));
    flow_text(s, px + 2, py + 4, line, FLOW_FG, FLOW_BG, 0);
    snprintf(line, sizeof line, "undo %d  redo %d", flow_undo_depth(f), flow_redo_depth(f));
    flow_text(s, px + 2, py + 5, line, FLOW_FG, FLOW_BG, 0);
    snprintf(line, sizeof line, "last: %s", op_name(flow_top_op(f)));
    flow_text(s, px + 2, py + 6, line, FLOW_FG, FLOW_BG, 0);
  }
}

static void key_anim(flow_t *f, void *u) {              /* #5 toggle marching ants on every edge */
  (void)u; anim_on = !anim_on;
  for (int i = 0; i < flow_edge_count(f); i++) flow_set_edge_animated(f, flow_edges(f)[i].id, anim_on);
}
static void key_hud(flow_t *f, void *u) { (void)f; (void)u; hud_on = !hud_on; }   /* #7 */
static void key_pal(flow_t *f, void *u) {              /* #6 open the modal palette */
  (void)u; pal.open = 1; pal.n = 0; pal.q[0] = 0; flow_set_key_hook_modal(f, 1);
}

/* ---- increment-7 (the feel layer) ---- */
static void key_theme(flow_t *f, void *u) {            /* #1 cycle DEFAULT -> DARK -> LIGHT */
  (void)u; flow_color_mode m = flow_color_mode_get(f);
  flow_set_color_mode(f, m == FLOW_COLOR_DEFAULT ? FLOW_COLOR_DARK
                       : m == FLOW_COLOR_DARK    ? FLOW_COLOR_LIGHT : FLOW_COLOR_DEFAULT);
}
static void key_lock(flow_t *f, void *u) { (void)u; flow_set_locked(f, !flow_locked(f)); }  /* #3 lock toggle */
/* #4/#5 toolbar actions — borrowed, file-static lifetime (the engine never copies them). */
static void act_node_del(flow_t *f, int id, void *u) { (void)u; flow_remove_node(f, id); }
static void act_node_dup(flow_t *f, int id, void *u) {
  (void)u; flow_node *n = flow_get_node(f, id); if (!n) return;
  flow_add_node(f, n->type, (flow_pt){ n->pos.x + 3, n->pos.y + 3 }, n->data);   /* shallow dup: same type + label */
}
static void act_edge_del(flow_t *f, int id, void *u) { (void)u; flow_remove_edge(f, id); }
static const flow_toolbar_action node_actions[] = { { "del", act_node_del, NULL }, { "dup", act_node_dup, NULL } };
static const flow_toolbar_action edge_actions[] = { { "\xe2\x9c\x95", act_edge_del, NULL } };   /* ✕ delete edge */

int main(void) {
  flow_t *f = flow_new(80, 24);
  flow_register_defaults(f);
  int a = flow_add_node(f, "default", (flow_pt){ 4,  3}, (void*)"web-server");
  int b = flow_add_node(f, "default", (flow_pt){34, 12}, (void*)"database");
  int c = flow_add_node(f, "default", (flow_pt){58,  4}, (void*)"cache");
  flow_add_edge(f, a, b, "", "");
  flow_add_edge(f, a, c, "", "");
  for (int i = 0; i < flow_edge_count(f); i++) flow_set_edge_animated(f, flow_edges(f)[i].id, 1);  /* #5 ON */

  flow_callbacks cb = {0}; cb.on_overlay = overlay; flow_set_callbacks(f, cb);   /* #7 HUD host */
  flow_set_key_hook(f, pal_hook, NULL);                /* #6 palette interceptor */
  flow_bind_key(f, "e", key_anim, NULL);
  flow_bind_key(f, "i", key_hud,  NULL);
  flow_bind_key(f, "/", key_pal,  NULL);
  flow_bind_key(f, "t", key_theme, NULL);             /* inc-7 #1 theme cycle */
  flow_bind_key(f, "L", key_lock,  NULL);             /* inc-7 #3 lock toggle */
  flow_set_controls(f, 1, FLOW_CORNER_TR);            /* inc-7 #3 Controls panel — TR keeps it clear of the row-0 help + bottom statusbar */
  flow_set_node_toolbar(f, node_actions, 2);          /* inc-7 #4 — shows above a single selected node */
  flow_set_edge_toolbar(f, edge_actions, 1);          /* inc-7 #5 — shows on a selected edge */
  flow_set_statusbar(f, 1);                            /* built-in n:add x:del f:fit ?:help q:quit bar */

  flow_run(f);
  flow_free(f);
  return 0;
}
