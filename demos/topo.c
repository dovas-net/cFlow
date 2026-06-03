/* topo — flow's flagship demo: a tiny network-topology editor.
 *
 * Demonstrates a custom node type (device) with rich data, selection,
 * a right-click details panel (via the on_overlay hook), and the minimap.
 *
 * Controls:
 *   left-drag a node ........ move it (edges re-route live)
 *   left-click a node ....... select it (highlighted border, reveals ◉ ports)
 *   drag from a ◉ port ...... draw a connection to another node's port (Esc cancels)
 *   right-click a node ...... open its details panel
 *   left-click empty space .. clear selection / close panel
 *   left-drag empty space ... pan;  scroll/wheel ... pan;  arrows ... pan
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

static void on_context(flow_t *f, int node, flow_pt scr, void *u) { (void)f;(void)scr;(void)u; g_info_node = node; }
static void on_pane(flow_t *f, flow_pt w, void *u) { (void)f;(void)w;(void)u; g_info_node = -1; }
static void on_overlay(flow_t *f, flow_surface *s, void *u) {
  (void)u;
  if (g_info_node == -1) return;
  flow_node *n = flow_get_node(f, g_info_node);
  if (!n) return;
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
  }

  flow_callbacks cb = {0};
  cb.on_node_context = on_context;
  cb.on_pane_click   = on_pane;
  cb.on_overlay      = on_overlay;
  flow_set_callbacks(f, cb);
  flow_set_minimap(f, 1, FLOW_CORNER_TR, 22, 7);
  flow_set_background(f, FLOW_BG_DOTS, 4);

  flow_run(f);
  flow_save(f, path);   /* save on quit: topology survives across runs */
  flow_free(f);
  return 0;
}
