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
static void fc_overlay(flow_t *f, flow_surface *s, void *u) {
  (void)f; (void)u;
  flow_text(s, 1, 0, "flowchart  l:layout  g:group  G:ungroup  h:hide sel  H:show all  q:quit",
            FLOW_FG, FLOW_BG, FLOW_BOLD);
  if (fc_event[0]) flow_text(s, 1, 1, fc_event, FLOW_FG, FLOW_BG, FLOW_DIM);
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
  flow_bind_key(f, "l", key_layout,   NULL);
  flow_bind_key(f, "g", key_group,    NULL);
  flow_bind_key(f, "G", key_ungroup,  NULL);
  flow_bind_key(f, "h", key_hide,     NULL);
  flow_bind_key(f, "H", key_show_all, NULL);
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
