/* flowchart — the groups + auto-layout showcase (spec §2/§15's third demo).
 *
 * A small flowchart (start → validate → decision → branches → done) is arranged by
 * flow_layout (layered, top-to-bottom) at startup, framed with fit, and the two
 * branch nodes are boxed into a labeled `group` container. Everything stays live:
 * drag nodes (drop ONTO the group to nest, drag out to unnest), draw connections
 * from the ◉ ports, undo/redo with u / Ctrl-r.
 *
 * Controls (beyond the engine built-ins):
 *   l ........ re-run the layered layout + fit
 *   g ........ group the current selection into a new container
 *   G ........ ungroup the selected group (children survive)
 *   h / H .... hide the selection / show everything (FLOW_HIDDEN showcase)
 *   n/x/f/? .. add node / delete / fit / help bar  (built-ins)
 *   q ........ quit
 *
 * A flowchart must stay acyclic: a prevent-cycles validator (inc-4 #9's engine gate
 * composed with #8's flow_outgoers reachability) silently rejects any connection
 * whose target already reaches its source; the overlay ticker echoes the verdict
 * via the #7 connect-lifecycle events.
 *
 * Compiled headless by tests/test_flowchart.c via FLOWCHART_TEST (main() skipped),
 * so the test exercises THIS file's graph + key wiring, not a copy.
 */
#ifndef FLOWCHART_TEST
#define FLOW_IMPLEMENTATION
#endif
#include "../flow.h"

static void key_layout(flow_t *f, void *u) {
  (void)u;
  flow_layout_opts o = {0};
  o.mode = FLOW_LAYOUT_LAYERED; o.dir = FLOW_TB;
  o.gap_x = 4; o.gap_y = 2;
  o.fit_after = 1; o.margin = 2;
  flow_layout(f, o);
}
static void key_group(flow_t *f, void *u) {
  (void)u;
  int n = flow_selected_count(f);
  if (n < 1) return;
  int *ids = (int*)malloc((size_t)n * sizeof(int));
  flow_selected_nodes(f, ids, n);
  int gid = flow_group(f, ids, n);
  if (gid != -1) { flow_node *g = flow_get_node(f, gid); if (g) g->data = (void*)"group"; }
  free(ids);
}
static void key_ungroup(flow_t *f, void *u) {
  (void)u;
  int id = flow_selected_node(f);
  if (id != -1) flow_ungroup(f, id);            /* no-op unless it IS a group */
}
/* hidden showcase (inc-4 #11): hide the selection ('h'), show everything ('H').
   Snapshot the ids FIRST: hiding deselects, so iterating the live selection while
   hiding would skip entries. */
static void key_hide(flow_t *f, void *u) {
  (void)u;
  int n = flow_selected_count(f);
  if (n < 1) return;
  int *ids = (int*)malloc((size_t)n * sizeof(int));
  flow_selected_nodes(f, ids, n);
  for (int i = 0; i < n; i++) flow_set_node_hidden(f, ids[i], 1);
  free(ids);
}
static void key_show_all(flow_t *f, void *u) {
  (void)u;
  for (int i = 0; i < flow_node_count(f); i++) flow_set_node_hidden(f, flow_nodes(f)[i].id, 0);
  for (int i = 0; i < flow_edge_count(f); i++) flow_set_edge_hidden(f, flow_edges(f)[i].id, 0);
}
/* connect-lifecycle ticker (inc-4 #7): one line echoing the last gesture verdict —
   the user-visible feedback channel for validator rejections. */
static char fc_event[64] = "";
static void fc_on_connect_end(flow_t *f, int eid, int src, int tgt, void *u) {
  (void)f; (void)u;
  if (eid != -1)      snprintf(fc_event, sizeof fc_event, "connected %d->%d", src, tgt);
  else if (tgt != -1) snprintf(fc_event, sizeof fc_event, "rejected %d->%d: cycle/duplicate", src, tgt);
  else                snprintf(fc_event, sizeof fc_event, "connection aborted");
}
/* prevent-cycles validator (inc-4 #9 gate x #8 traversal): reject a connection whose
   TARGET already reaches its SOURCE — the edge would close a directed cycle.
   Iterative DFS over flow_outgoers, visited-by-id; stack sized by edge count (every
   push follows a distinct deduped out-neighbor hop). Demo-scale graphs only. */
static int fc_reaches(flow_t *f, int from, int target) {
  int nn = flow_node_count(f), cap = flow_edge_count(f) + 1;
  if (nn <= 0) return 0;
  int *stack = (int*)malloc((size_t)cap * sizeof(int));
  int *seen  = (int*)malloc((size_t)nn  * sizeof(int));
  int sp = 0, ns = 0, found = 0;
  if (stack && seen) {
    stack[sp++] = from;
    while (sp > 0) {
      int cur = stack[--sp];
      if (cur == target) { found = 1; break; }
      int already = 0;
      for (int i = 0; i < ns; i++) if (seen[i] == cur) { already = 1; break; }
      if (already || ns >= nn) continue;
      seen[ns++] = cur;
      int outs[32];
      int c = flow_outgoers(f, cur, outs, 32); if (c > 32) c = 32;
      for (int i = 0; i < c && sp < cap; i++) stack[sp++] = outs[i];
    }
  }
  free(stack); free(seen);
  return found;
}
static int fc_no_cycles(flow_t *f, int src, int tgt, const char *sh, const char *th, void *u) {
  (void)sh; (void)th; (void)u;
  return !fc_reaches(f, tgt, src);
}
/* '/' command palette (inc-5 #10): a modal search built on the engine's key hook
   + flow_find_nodes + view-frame framing. While open, the hook consumes printable
   chars (incremental query), Backspace, Enter (select + frame the first match),
   and lone ESC (close). inc-6 #6: the palette goes MODAL while open
   (flow_set_key_hook_modal) — the inc-5 leak is closed, so Delete/Tab/Shift-arrows/
   bare-arrow pan no longer act on the graph behind the search; any CSI the hook
   returns 0 for is DROPPED engine-side, not fallen through.
   The palette skips HIDDEN nodes — a view-level UI choice over the model-level
   flow_find_nodes (which includes them by the layering rule). */
static struct { int open; char q[32]; int qn; } fc_pal;
static int fc_pal_matches(flow_t *f, int *out, int max) {
  int all[64];
  int n = flow_find_nodes(f, fc_pal.q, all, 64); if (n > 64) n = 64;
  int c = 0;
  for (int i = 0; i < n; i++) {
    flow_node *nd = flow_get_node(f, all[i]);
    if (!nd || (nd->flags & FLOW_HIDDEN)) continue;   /* view-level skip */
    if (c < max && out) out[c] = all[i];
    c++;
  }
  return c;
}
static int fc_pal_hook(flow_t *f, const char *seq, int len, void *user) {
  (void)user;
  if (!fc_pal.open) return 0;
  unsigned char c = (unsigned char)seq[0];
  if (c == 0x1b) {                                    /* mirror flow_feed's loneness test */
    if (len >= 2 && seq[1] == '[') return 0;          /* CSI: hook declines → modal DROPS it (inc-6 #6) */
    fc_pal.open = 0; flow_set_key_hook_modal(f, 0); return 1;  /* lone ESC closes; leave modal */
  }
  if (c == '\r') {                                    /* Enter: select + frame first match */
    int first;
    if (fc_pal_matches(f, &first, 1) > 0) {
      flow_select_node(f, first, 0);
      flow_rect r = flow_bounds_of(f, &first, 1);     /* view-frame (inc-5 #4) */
      flow_set_center(f, r.x + r.w / 2, r.y + r.h / 2, -1.0f);
    }
    fc_pal.open = 0; flow_set_key_hook_modal(f, 0); return 1;  /* Enter selects + closes; leave modal */
  }
  if (c == 0x7f || c == 0x08) {                       /* Backspace */
    if (fc_pal.qn > 0) fc_pal.q[--fc_pal.qn] = 0;
    return 1;
  }
  if (c >= 0x20 && c < 0x7f) {                        /* printable: append to the query */
    if (fc_pal.qn < (int)sizeof fc_pal.q - 1) { fc_pal.q[fc_pal.qn++] = (char)c; fc_pal.q[fc_pal.qn] = 0; }
    return 1;
  }
  return 0;                                           /* other control bytes: pass (v1) */
}
static void key_palette(flow_t *f, void *u) {
  (void)u;
  fc_pal.open = 1; fc_pal.qn = 0; fc_pal.q[0] = 0;
  flow_set_key_hook_modal(f, 1);                       /* inc-6 #6: capture ALL input while the palette is open */
}
/* helper-lines toggle (inc-5 #8 integration): 'a' flips alignment guides + snap */
static int fc_align_on = 0;
static void key_align(flow_t *f, void *u) {
  (void)u;
  fc_align_on = !fc_align_on;
  flow_set_helper_lines(f, fc_align_on);
}
/* inc-6 #7 devtools HUD: bottom-right panel composed over public read accessors +
   the new journal-introspection getters. Toggle flag defaults ON; the toggle KEY
   binding is wired in the final integration pass (the engine surface lands here). */
static int fc_hud = 1;
static const char *fc_op_name(int op) {
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
static void fc_overlay(flow_t *f, flow_surface *s, void *u) {
  (void)u;
  flow_text(s, 1, 0, "l:layout g:group G:ungroup h:hide H:show a:align e:anim i:hud /:find q:quit",
            FLOW_FG, FLOW_BG, FLOW_BOLD);
  if (fc_hud) {                                          /* DevTools panes: counts + ViewportLogger + NodeInspector + ChangeLogger */
    flow_viewport v = flow_view_get(f);
    int w = 24, h = 8;
    int px = flow_surface_w(s) - w - 1, py = flow_surface_h(s) - h - 2;   /* above the statusbar row */
    char line[64];
    flow_box(s, px, py, w, h, FLOW_FG, FLOW_BG, FLOW_BOLD);
    flow_text(s, px + 2, py + 1, "devtools", FLOW_FG, FLOW_BG, FLOW_BOLD);
    snprintf(line, sizeof line, "nodes %d  edges %d  sel %d", flow_node_count(f), flow_edge_count(f), flow_selected_count(f));
    flow_text(s, px + 2, py + 2, line, FLOW_FG, FLOW_BG, 0);                /* counts */
    snprintf(line, sizeof line, "view %.0f,%.0f  z%.2f", v.ox, v.oy, v.zoom);
    flow_text(s, px + 2, py + 3, line, FLOW_FG, FLOW_BG, 0);                /* ViewportLogger */
    int fn = flow_focused_node(f);                                         /* NodeInspector: focused node's label */
    flow_node *fnd = fn != -1 ? flow_get_node(f, fn) : NULL;
    snprintf(line, sizeof line, "focus: %s", fnd && fnd->data ? (const char*)fnd->data : (fn != -1 ? "?" : "none"));
    flow_text(s, px + 2, py + 4, line, FLOW_FG, FLOW_BG, 0);
    snprintf(line, sizeof line, "undo %d  redo %d", flow_undo_depth(f), flow_redo_depth(f));
    flow_text(s, px + 2, py + 5, line, FLOW_FG, FLOW_BG, 0);                /* ChangeLogger: depth */
    snprintf(line, sizeof line, "last: %s", fc_op_name(flow_top_op(f)));
    flow_text(s, px + 2, py + 6, line, FLOW_FG, FLOW_BG, 0);                /*   + most-recent op */
  }
  if (fc_event[0]) flow_text(s, 1, 1, fc_event, FLOW_FG, FLOW_BG, FLOW_DIM);
  if (fc_pal.open) {
    char line[96]; int m[1]; int n = fc_pal_matches(f, m, 1);
    const char *firstlab = "";
    if (n > 0) { flow_node *nd = flow_get_node(f, m[0]); if (nd && nd->data) firstlab = (const char*)nd->data; }
    snprintf(line, sizeof line, " find: %s_  (%d match%s%s%s) ",
             fc_pal.q, n, n == 1 ? "" : "es", n > 0 ? " -> " : "", firstlab);
    flow_text(s, 1, 2, line, FLOW_FG, FLOW_BG, FLOW_REVERSE);
  }
}

/* inc-6 #5 showcase: 'e' toggles marching-ants animation on EVERY edge. #4's redraw
   clock arms itself while any edge is animated, so the ants march at 10 Hz; clearing
   them disarms it (idle graph blocks again). */
static int fc_anim_on = 0;
static void key_anim(flow_t *f, void *u) {
  (void)u;
  fc_anim_on = !fc_anim_on;
  for (int i = 0; i < flow_edge_count(f); i++)
    flow_set_edge_animated(f, flow_edges(f)[i].id, fc_anim_on);
}
/* inc-6 #7: 'i' toggles the devtools HUD overlay (default ON). */
static void key_hud(flow_t *f, void *u) { (void)f; (void)u; fc_hud = !fc_hud; }

/* Build the chart, arrange it, box the branches. Returns the group's node id.
   Layout FIRST, group SECOND: v1 layout ignores cross-partition edges by design,
   so ranking the leaves before boxing them keeps the flowchart shape (a container
   has no intra-partition edges and would otherwise rank 0 as an isolated node). */
static int flowchart_setup(flow_t *f) {
  flow_register_defaults(f);
  int start    = flow_add_node(f, "default", (flow_pt){ 2,  2}, (void*)"start");
  int validate = flow_add_node(f, "default", (flow_pt){26,  2}, (void*)"validate");
  int decide   = flow_add_node(f, "default", (flow_pt){50,  2}, (void*)"valid?");
  int save     = flow_add_node(f, "default", (flow_pt){26, 12}, (void*)"save");
  int reject   = flow_add_node(f, "default", (flow_pt){50, 12}, (void*)"reject");
  int done     = flow_add_node(f, "default", (flow_pt){38, 20}, (void*)"done");
  flow_add_edge(f, start,    validate, "", "");
  flow_add_edge(f, validate, decide,   "", "");
  flow_add_edge(f, decide,   save,     "", "");
  flow_add_edge(f, decide,   reject,   "", "");
  flow_add_edge(f, save,     done,     "", "");
  flow_add_edge(f, reject,   done,     "", "");
  key_layout(f, NULL);                          /* open auto-arranged + framed */
  int gid = flow_group(f, (int[]){ save, reject }, 2);
  if (gid != -1) { flow_node *g = flow_get_node(f, gid); if (g) g->data = (void*)"branches"; }
  flow_bind_key(f, "l", key_layout,   NULL);
  flow_bind_key(f, "g", key_group,    NULL);
  flow_bind_key(f, "G", key_ungroup,  NULL);
  flow_bind_key(f, "h", key_hide,     NULL);
  flow_bind_key(f, "H", key_show_all, NULL);
  flow_bind_key(f, "/", key_palette,  NULL);    /* command palette (inc-5 #10): the hook
                                                   below consumes input only WHILE open */
  flow_bind_key(f, "a", key_align,    NULL);    /* helper-lines toggle (inc-5 #8) */
  flow_bind_key(f, "e", key_anim,     NULL);    /* inc-6 #5: marching-ants showcase */
  flow_bind_key(f, "i", key_hud,      NULL);    /* inc-6 #7: devtools HUD toggle */
  flow_set_key_hook(f, fc_pal_hook, NULL);
  flow_callbacks cb = {0};
  cb.on_overlay = fc_overlay;
  cb.on_connect_end = fc_on_connect_end;        /* ticker: success / reject / abort verdicts */
  flow_set_callbacks(f, cb);
  /* prevent-cycles gate — set AFTER the graph is built: the validator gates every
     flow_add_edge, and a validator left active across a load/build would re-gate
     (and could silently drop) edges (#9's documented transient contract). */
  flow_set_connection_validator(f, fc_no_cycles, NULL);
  flow_set_statusbar(f, 1);                     /* built-in hint bar under the overlay line */
  return gid;
}

#ifndef FLOWCHART_TEST
int main(void) {
  flow_t *f = flow_new(80, 24);
  flowchart_setup(f);
  flow_run(f);
  flow_free(f);
  return 0;
}
#endif
