/* ===== default node type (data = C-string label) + register-defaults ===== */
extern const flow_node_type flow_default_node_type;
extern const flow_node_type flow_group_node_type;  /* built-in "group" container; registered by flow_register_defaults */
extern const flow_handle flow_default_handles[2];  /* LEFT target 'in', RIGHT source 'out' */
void flow_register_defaults(flow_t *f);

#ifdef FLOW_IMPLEMENTATION
static void flow__default_measure(const flow_node *n, int *w, int *h) {
  const char *label = n->data ? (const char*)n->data : "";
  int len = (int)strlen(label); if (len < 1) len = 1;
  *w = len + 4; *h = 3;
}
static void flow__default_render(const flow_node *n, flow_surface *s, flow_render_ctx ctx) {
  const char *label = n->data ? (const char*)n->data : "";
  unsigned bold = (ctx.flags & FLOW_SELECTED) ? FLOW_BOLD : 0;
  if (ctx.lod) {  /* collapsed: ONE marker at the top-left cell (matches flow__node_footprint) */
    flow_put(s, 0, 0, 0x25A0, FLOW_FG, FLOW_BG, bold);   /* ■ */
    return;
  }
  flow_box(s, 0, 0, flow_surface_w(s), flow_surface_h(s), FLOW_FG, FLOW_BG, bold);
  flow_text(s, 2, 1, label, FLOW_FG, FLOW_BG, bold);
}
const flow_handle flow_default_handles[2] = {
  { "in",  FLOW_HANDLE_TARGET, FLOW_LEFT,  0 },   /* input on left border  */
  { "out", FLOW_HANDLE_SOURCE, FLOW_RIGHT, 0 },   /* output on right border */
};
/* label accessor (inc-5 #10): both built-in types already treat n->data as the
   C-string label (measure/render above) — expose the same read for flow_find_nodes. */
static const char *flow__cstr_label(const flow_node *n) {
  return n->data ? (const char*)n->data : NULL;
}
const flow_node_type flow_default_node_type = { "default", flow__default_measure, flow__default_render, flow_default_handles, 2, NULL, NULL, flow__cstr_label };
/* group container: measure is a WRITE-BACK no-op — flow_measure_node does
   `int w=0,h=0; measure(n,&w,&h); n->w=w; n->h=h;`, so to leave the caller-set bbox
   size intact we must echo the node's CURRENT w/h back out (an empty body would zero
   them). flow_group sets w/h from the member bbox after add. */
static void flow__group_measure(const flow_node *n, int *w, int *h) { *w = n->w; *h = n->h; }
static void flow__group_render(const flow_node *n, flow_surface *s, flow_render_ctx ctx) {
  const char *label = n->data ? (const char*)n->data : "";
  unsigned bold = (ctx.flags & FLOW_SELECTED) ? FLOW_BOLD : 0;
  if (ctx.lod) {  /* collapsed: ONE marker at the top-left cell (matches flow__node_footprint) */
    flow_put(s, 0, 0, 0x25A1, FLOW_FG, FLOW_BG, bold);   /* □ hollow square: container marker */
    return;
  }
  flow_box(s, 0, 0, flow_surface_w(s), flow_surface_h(s), FLOW_FG, FLOW_BG, bold);  /* frame only */
  if (label[0]) flow_text(s, 1, 0, label, FLOW_FG, FLOW_BG, bold);  /* title on the top border */
}
/* group has NO handles (containers aren't connectable in v1): handle_count==0 leaves
   flow_hit_handle/edge anchoring untouched. No save/load hooks (parent is durable via the
   node field; the app re-registers this type on load). */
const flow_node_type flow_group_node_type = { "group", flow__group_measure, flow__group_render, NULL, 0, NULL, NULL, flow__cstr_label };
void flow_register_defaults(flow_t *f) {
  flow_register_node_type(f, &flow_default_node_type);
  flow_register_node_type(f, &flow_group_node_type);
  flow_register_edge_type(f, &flow_default_edge_type);
  flow_register_edge_type(f, &flow_straight_edge_type);
}
#endif
