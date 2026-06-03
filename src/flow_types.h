/* ===== default node type (data = C-string label) + register-defaults ===== */
extern const flow_node_type flow_default_node_type;
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
  flow_box(s, 0, 0, flow_surface_w(s), flow_surface_h(s), FLOW_FG, FLOW_BG, bold);
  flow_text(s, 2, 1, label, FLOW_FG, FLOW_BG, bold);
}
const flow_handle flow_default_handles[2] = {
  { "in",  FLOW_HANDLE_TARGET, FLOW_LEFT,  0 },   /* input on left border  */
  { "out", FLOW_HANDLE_SOURCE, FLOW_RIGHT, 0 },   /* output on right border */
};
const flow_node_type flow_default_node_type = { "default", flow__default_measure, flow__default_render, flow_default_handles, 2 };
void flow_register_defaults(flow_t *f) {
  flow_register_node_type(f, &flow_default_node_type);
  flow_register_edge_type(f, &flow_default_edge_type);
}
#endif
