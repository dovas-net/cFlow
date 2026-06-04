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
 *   n/x/f/? .. add node / delete / fit / help bar  (built-ins)
 *   q ........ quit
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
static void fc_overlay(flow_t *f, flow_surface *s, void *u) {
  (void)f; (void)u;
  flow_text(s, 1, 0, "flowchart  l:layout  g:group selection  G:ungroup  q:quit",
            FLOW_FG, FLOW_BG, FLOW_BOLD);
}

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
  flow_bind_key(f, "l", key_layout,  NULL);
  flow_bind_key(f, "g", key_group,   NULL);
  flow_bind_key(f, "G", key_ungroup, NULL);
  flow_callbacks cb = {0};
  cb.on_overlay = fc_overlay;
  flow_set_callbacks(f, cb);
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
