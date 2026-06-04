/* topo — flow's flagship demo: a tiny network-topology editor.
 *
 * Demonstrates a custom node type (device) with rich data, selection,
 * a right-click details panel (via the on_overlay hook), the minimap,
 * groups, force-directed auto-layout, undo/redo, and the observer events
 * (the top-left ticker echoes connect/select/delete as they fire).
 *
 * Controls:
 *   left-drag a node ........ move it (edges re-route live; auto-pans near the
 *                             edge, drop ONTO a group to nest, drag out to unnest)
 *   left-click a node ....... select it (highlighted border, reveals ◉ ports)
 *   drag from a ◉ port ...... draw a connection to another node's port (Esc cancels)
 *   right-click a node ...... open its details panel
 *   left-click empty space .. clear selection / close panel
 *   left-drag empty space ... pan;  scroll/wheel ... pan;  arrows ... pan
 *   Space ................... toggle pan mode (drag pans even over nodes; Esc exits)
 *   l ....................... auto-arrange (force-directed) + fit
 *   g / G ................... group the selection / ungroup the selected group
 *   u / Ctrl-r .............. undo / redo
 *   q ....................... quit
 */
#define FLOW_IMPLEMENTATION
#include "../flow.h"

typedef struct {
  char label[24];
  char kind[12];
  char ip[20];
  char status[12];
  char os[24];
} device;

static void dev_measure(const flow_node *n, int *w, int *h) {
  const device *d = (const device*)n->data;   /* NULL during flow_load's initial add (data set by dev_load, then re-measured) */
  int len = d ? (int)strlen(d->label) : 0;
  if (d && (int)strlen(d->kind) > len) len = (int)strlen(d->kind);
  *w = len + 4; *h = 4;
}
static void dev_render(const flow_node *n, flow_surface *s, flow_render_ctx ctx) {
  const device *d = (const device*)n->data;
  unsigned bold = (ctx.flags & FLOW_SELECTED) ? FLOW_BOLD : 0;
  if (ctx.lod) {  /* zoomed out: one collapsed marker at the top-left cell (matches footprint) */
    flow_put(s, 0, 0, 0x25A0, FLOW_FG, FLOW_BG, bold);   /* ■ */
    return;
  }
  flow_box(s, 0, 0, flow_surface_w(s), flow_surface_h(s), FLOW_FG, FLOW_BG, bold);
  flow_text(s, 2, 1, d->label, FLOW_FG, FLOW_BG, bold);
  flow_text(s, 2, 2, d->kind,  FLOW_FG, FLOW_BG, 0);
}
/* JSON persistence hooks: write/read the device struct as one JSON value.
   dev_load mallocs a device per node — app-owned memory the library never frees
   (process exit reclaims it here). */
static void dev_save(const flow_node *n, FILE *out) {
  const device *d = (const device*)n->data;
  fputs("{\"label\":", out);  flow__json_str(out, d->label);
  fputs(",\"kind\":", out);   flow__json_str(out, d->kind);
  fputs(",\"ip\":", out);     flow__json_str(out, d->ip);
  fputs(",\"status\":", out); flow__json_str(out, d->status);
  fputs(",\"os\":", out);     flow__json_str(out, d->os);
  fputc('}', out);
}
static void dev_load(flow_node *n, const char *data_json) {
  device *d = (device*)calloc(1, sizeof *d);
  const char *end = data_json + strlen(data_json);
  flow_json_rd fld;
  if (flow__json_find(data_json, end, "label", &fld))  flow__json_strv(fld, d->label,  (int)sizeof d->label);
  if (flow__json_find(data_json, end, "kind", &fld))   flow__json_strv(fld, d->kind,   (int)sizeof d->kind);
  if (flow__json_find(data_json, end, "ip", &fld))     flow__json_strv(fld, d->ip,     (int)sizeof d->ip);
  if (flow__json_find(data_json, end, "status", &fld)) flow__json_strv(fld, d->status, (int)sizeof d->status);
  if (flow__json_find(data_json, end, "os", &fld))     flow__json_strv(fld, d->os,     (int)sizeof d->os);
  n->data = d;
}
/* reuse the default LEFT-'in' / RIGHT-'out' ports so devices are connectable */
static const flow_node_type DEVICE = { "device", dev_measure, dev_render, flow_default_handles, 2, dev_save, dev_load };

/* app state for the details panel */
static int g_info_node = -1;
/* events showcase: the last observer callback, echoed in the overlay ticker */
static char g_event[48] = "";

static void on_context(flow_t *f, int node, flow_pt scr, void *u) { (void)f;(void)scr;(void)u; g_info_node = node; }
static void on_pane(flow_t *f, flow_pt w, void *u) { (void)f;(void)w;(void)u; g_info_node = -1; }
static void on_connect_ev(flow_t *f, int s, int t, void *u) {
  (void)f;(void)u; snprintf(g_event, sizeof g_event, "event: connect %d->%d", s, t);
}
/* edge observers (inc-4 #6): the context "menu" is a ticker entry naming the link's
   endpoints; a plain click echoes too. Right-click a path cell to try it. */
static void on_edge_ctx(flow_t *f, int edge, flow_pt scr, void *u) {
  (void)scr;(void)u;
  flow_edge *e = flow_get_edge(f, edge);
  if (e) snprintf(g_event, sizeof g_event, "edge %d menu: %d->%d  [x deletes selected]", edge, e->source, e->target);
  g_info_node = -1;                            /* edge menu replaces any node panel */
}
static void on_edge_click_ev(flow_t *f, int edge, void *u) {
  (void)f;(void)u; snprintf(g_event, sizeof g_event, "event: edge click #%d", edge);
}
/* connect lifecycle (inc-4 #7): echo aborted/dropped gestures — successes are already
   echoed by on_connect, so only the eid==-1 exits write the ticker here. */
static void on_connect_end_ev(flow_t *f, int eid, int s, int t, void *u) {
  (void)f;(void)u;
  if (eid != -1) return;
  if (t != -1) snprintf(g_event, sizeof g_event, "event: connect %d->%d rejected", s, t);
  else         snprintf(g_event, sizeof g_event, "event: connect from %d aborted", s);
}
static void on_select_ev(flow_t *f, const int *ids, int n, void *u) {
  (void)f;(void)ids;(void)u;
  if (n > 0) snprintf(g_event, sizeof g_event, "event: select x%d", n);
  else       snprintf(g_event, sizeof g_event, "event: selection cleared");
}
static void on_delete_ev(flow_t *f, const int *ids, int n, void *u) {
  (void)f;(void)ids;(void)u; snprintf(g_event, sizeof g_event, "event: delete x%d", n);
}
/* showcase keys: force-directed auto-layout + group/ungroup the selection */
static void key_layout(flow_t *f, void *u) {
  (void)u;
  flow_layout_opts o = {0};                  /* FORCE defaults: organic spread for a network */
  o.iterations = 250; o.fit_after = 1; o.margin = 2;
  flow_layout(f, o);
}
static void key_group(flow_t *f, void *u) {
  (void)u;
  int n = flow_selected_count(f);
  if (n < 1) return;
  int *ids = (int*)malloc((size_t)n * sizeof(int));
  flow_selected_nodes(f, ids, n);
  int gid = flow_group(f, ids, n);
  if (gid != -1) { flow_node *g = flow_get_node(f, gid); if (g) g->data = (void*)"cluster"; }
  free(ids);
}
static void key_ungroup(flow_t *f, void *u) {
  (void)u;
  int id = flow_selected_node(f);
  if (id != -1) flow_ungroup(f, id);         /* no-op unless it IS a group */
}
static void on_overlay(flow_t *f, flow_surface *s, void *u) {
  (void)u;
  if (g_event[0]) flow_text(s, 1, 0, g_event, FLOW_FG, FLOW_BG, FLOW_DIM);  /* events ticker */
  if (g_info_node == -1) return;
  flow_node *n = flow_get_node(f, g_info_node);
  if (!n) return;
  if (strcmp(n->type, "device") != 0) return;  /* details are device-only: 'n'-added defaults
                                                  and 'g'-created clusters carry no device struct */
  const device *d = (const device*)n->data;
  int w = 42, h = 8;
  int ox = flow_surface_w(s) - w - 1, oy = flow_surface_h(s) - h - 1;
  if (ox < 0) ox = 0; if (oy < 0) oy = 0;
  flow_box(s, ox, oy, w, h, FLOW_FG, FLOW_BG, FLOW_BOLD);
  char line[64];
  flow_text(s, ox + 2, oy + 1, d->label, FLOW_FG, FLOW_BG, FLOW_BOLD);
  snprintf(line, sizeof line, "type:   %s", d->kind);   flow_text(s, ox + 2, oy + 2, line, FLOW_FG, FLOW_BG, 0);
  snprintf(line, sizeof line, "ip:     %s", d->ip);     flow_text(s, ox + 2, oy + 3, line, FLOW_FG, FLOW_BG, 0);
  snprintf(line, sizeof line, "status: %s", d->status); flow_text(s, ox + 2, oy + 4, line, FLOW_FG, FLOW_BG, 0);
  snprintf(line, sizeof line, "os:     %s", d->os);     flow_text(s, ox + 2, oy + 5, line, FLOW_FG, FLOW_BG, 0);
  flow_text(s, ox + 2, oy + h - 2, "click empty space to close", FLOW_FG, FLOW_BG, 0);
}

int main(void) {
  static device dws = { "web-server", "http",     "10.0.0.10", "up",       "Ubuntu 22.04" };
  static device ddb = { "database",   "postgres", "10.0.0.20", "up",       "Debian 12" };
  static device dca = { "cache",      "redis",    "10.0.0.30", "degraded", "Alpine 3.19" };

  flow_t *f = flow_new(80, 24);
  flow_register_defaults(f);
  flow_register_node_type(f, &DEVICE);

  /* Persistence: load the saved topology on start if present (device data fully
     restored via dev_load); otherwise seed the demo graph. Save on quit. */
  const char *path = "topo.json";
  if (flow_load(f, path) != 0) {
    int a = flow_add_node(f, "device", (flow_pt){ 6,  3}, &dws);
    int b = flow_add_node(f, "device", (flow_pt){40, 14}, &ddb);
    int c = flow_add_node(f, "device", (flow_pt){62,  5}, &dca);
    flow_add_edge(f, a, b, "", "");
    flow_add_edge(f, a, c, "", "");
  } else {
    /* 'g'-created clusters persist structure (id/x/y/parent) but neither size nor
       label — the group type has no save/load hooks (v1) and JSON carries no w/h,
       so a loaded container would measure 0x0. Re-derive each box from its children
       (+1 pad, matching flow_group) and re-label. Array order suffices for nesting:
       containers are appended after their members, so inner boxes resolve first. */
    for (int i = 0; i < flow_node_count(f); i++) {
      flow_node *g = &flow_nodes(f)[i];
      if (strcmp(g->type, "group") != 0) continue;
      flow_rect bb = {0,0,0,0}; int have = 0;
      for (int j = 0; j < flow_node_count(f); j++) {
        flow_node *c = &flow_nodes(f)[j];
        if (c->parent != g->id) continue;
        flow_rect r = flow_node_rect_abs(f, c);
        bb = have ? flow_rect_union(bb, r) : r; have = 1;
      }
      flow_pt gp = flow_node_abs(f, g);
      if (have) { g->w = bb.x + bb.w + 1 - gp.x; g->h = bb.y + bb.h + 1 - gp.y; }
      else      { g->w = 6; g->h = 3; }          /* childless container: keep a visible box */
      g->data = (void*)"cluster";
    }
  }

  flow_callbacks cb = {0};
  cb.on_node_context     = on_context;
  cb.on_pane_click       = on_pane;
  cb.on_overlay          = on_overlay;
  cb.on_connect          = on_connect_ev;    /* events showcase: echo into the ticker */
  cb.on_selection_change = on_select_ev;
  cb.on_nodes_delete     = on_delete_ev;
  cb.on_edge_context     = on_edge_ctx;      /* inc-4 #6: right-click an edge -> ticker menu */
  cb.on_edge_click       = on_edge_click_ev;
  cb.on_connect_end      = on_connect_end_ev;/* inc-4 #7: echo aborted/rejected gestures */
  flow_set_callbacks(f, cb);
  flow_set_minimap(f, 1, FLOW_CORNER_TR, 22, 7);
  flow_set_background(f, FLOW_BG_DOTS, 4);
  flow_set_statusbar(f, 1);                  /* bottom bar: built-ins + SPC:pan u:undo ^r:redo */
  flow_set_autopan(f, 4, 3);                 /* the public knob: slightly wider/faster band than the 3/2 default */
  flow_bind_key(f, "l", key_layout,  NULL);  /* force-directed arrange + fit */
  flow_bind_key(f, "g", key_group,   NULL);
  flow_bind_key(f, "G", key_ungroup, NULL);

  flow_run(f);
  flow_save(f, path);   /* save on quit: topology survives across runs */
  flow_free(f);
  return 0;
}
