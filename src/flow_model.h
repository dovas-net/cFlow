/* ===== model: engine, nodes, edges, vtable types, transform, bounds, hit-test ===== */
enum { FLOW_SELECTED = 1u, FLOW_DRAGGING = 2u, FLOW_HOVERED = 4u };
typedef enum { FLOW_HANDLE_SOURCE, FLOW_HANDLE_TARGET, FLOW_HANDLE_BOTH } flow_handle_kind;
typedef struct { char id[16]; flow_handle_kind kind; flow_pos pos; int along; } flow_handle;

typedef struct { int id; char type[32]; flow_pt pos; int parent; int w, h; void *data; unsigned flags; } flow_node;
typedef struct { int id, source, target; char source_handle[16], target_handle[16];
                 char type[16]; char *label; void *data; unsigned flags; } flow_edge;

typedef struct { float zoom; unsigned flags; int lod; } flow_render_ctx;
typedef struct { const char *type;
  void (*measure)(const flow_node*, int*, int*);
  void (*render)(const flow_node*, flow_surface*, flow_render_ctx);
  const flow_handle *handles; int handle_count; } flow_node_type;
typedef struct { const char *type;
  void (*route)(flow_pt s, flow_pos sp, flow_pt t, flow_pos tp, flow_route *out); } flow_edge_type;

typedef struct flow flow_t;

flow_t *flow_new(int cols, int rows);
void    flow_free(flow_t *f);
void    flow_resize(flow_t *f, int cols, int rows);
void    flow_register_node_type(flow_t *f, const flow_node_type *t);
void    flow_register_edge_type(flow_t *f, const flow_edge_type *t);
const flow_node_type *flow_node_type_for(flow_t *f, const char *type);
const flow_edge_type *flow_edge_type_for(flow_t *f, const char *type);
void    flow_measure_node(flow_t *f, flow_node *n);
int     flow_add_node(flow_t *f, const char *type, flow_pt pos, void *data);
int     flow_add_edge(flow_t *f, int src, int dst, const char *sh, const char *th);
flow_node *flow_get_node(flow_t *f, int id);
flow_edge *flow_get_edge(flow_t *f, int id);
int     flow_node_count(flow_t *f);
int     flow_edge_count(flow_t *f);
flow_node *flow_nodes(flow_t *f);
flow_edge *flow_edges(flow_t *f);
flow_pt   flow_node_abs(flow_t *f, const flow_node *n);
flow_rect flow_node_rect_abs(flow_t *f, const flow_node *n);
flow_rect flow_bounds(flow_t *f);
int       flow_hit_node(flow_t *f, flow_pt screen);
void      flow_pan(flow_t *f, int dx, int dy);
flow_pt   flow_to_screen(flow_t *f, flow_pt world_abs);
flow_pt   flow_to_world(flow_t *f, flow_pt screen);
flow_viewport flow_view_get(flow_t *f);

#ifdef FLOW_IMPLEMENTATION
struct flow {
  flow_node *nodes; int nnodes, capnodes, nextid;
  flow_edge *edges; int nedges, capedges, nexteid;
  const flow_node_type **ntypes; int nntypes;
  const flow_edge_type **etypes; int netypes;
  flow_viewport view; int cols, rows; flow_cell *front; int running;
};
static void *flow__grow(void *arr, int *cap, int need, size_t sz) {
  if (need <= *cap) return arr;
  int c = *cap ? *cap : 8; while (c < need) c *= 2;
  arr = realloc(arr, (size_t)c * sz); *cap = c; return arr;
}
flow_t *flow_new(int cols, int rows) {
  flow_t *f = (flow_t*)calloc(1, sizeof *f);
  f->view.zoom = 1; f->cols = cols; f->rows = rows; f->nextid = 1; f->nexteid = 1;
  f->front = (flow_cell*)calloc((size_t)cols * rows, sizeof(flow_cell));
  return f;
}
void flow_free(flow_t *f) {
  if (!f) return;
  for (int i = 0; i < f->nedges; i++) free(f->edges[i].label);
  free(f->nodes); free(f->edges); free(f->ntypes); free(f->etypes); free(f->front); free(f);
}
void flow_resize(flow_t *f, int cols, int rows) {
  f->cols = cols; f->rows = rows; free(f->front);
  f->front = (flow_cell*)calloc((size_t)cols * rows, sizeof(flow_cell));
}
void flow_register_node_type(flow_t *f, const flow_node_type *t) {
  f->ntypes = (const flow_node_type**)realloc(f->ntypes, (f->nntypes + 1) * sizeof *f->ntypes);
  f->ntypes[f->nntypes++] = t;
}
void flow_register_edge_type(flow_t *f, const flow_edge_type *t) {
  f->etypes = (const flow_edge_type**)realloc(f->etypes, (f->netypes + 1) * sizeof *f->etypes);
  f->etypes[f->netypes++] = t;
}
const flow_node_type *flow_node_type_for(flow_t *f, const char *type) {
  for (int i = 0; i < f->nntypes; i++) if (strcmp(f->ntypes[i]->type, type) == 0) return f->ntypes[i];
  return NULL;
}
const flow_edge_type *flow_edge_type_for(flow_t *f, const char *type) {
  for (int i = 0; i < f->netypes; i++) if (strcmp(f->etypes[i]->type, type) == 0) return f->etypes[i];
  return NULL;
}
void flow_measure_node(flow_t *f, flow_node *n) {
  const flow_node_type *t = flow_node_type_for(f, n->type);
  if (t && t->measure) { int w = 0, h = 0; t->measure(n, &w, &h); n->w = w; n->h = h; }
  else { n->w = 4; n->h = 3; }
}
int flow_add_node(flow_t *f, const char *type, flow_pt pos, void *data) {
  f->nodes = (flow_node*)flow__grow(f->nodes, &f->capnodes, f->nnodes + 1, sizeof(flow_node));
  flow_node *n = &f->nodes[f->nnodes++]; memset(n, 0, sizeof *n);
  n->id = f->nextid++; snprintf(n->type, sizeof n->type, "%s", type ? type : "default");
  n->pos = pos; n->parent = -1; n->data = data; flow_measure_node(f, n);
  return n->id;
}
int flow_add_edge(flow_t *f, int src, int dst, const char *sh, const char *th) {
  if (src == dst) return -1;
  if (!flow_get_node(f, src) || !flow_get_node(f, dst)) return -1;
  for (int i = 0; i < f->nedges; i++) if (f->edges[i].source == src && f->edges[i].target == dst) return -1;
  f->edges = (flow_edge*)flow__grow(f->edges, &f->capedges, f->nedges + 1, sizeof(flow_edge));
  flow_edge *e = &f->edges[f->nedges++]; memset(e, 0, sizeof *e);
  e->id = f->nexteid++; e->source = src; e->target = dst;
  snprintf(e->source_handle, sizeof e->source_handle, "%s", sh ? sh : "");
  snprintf(e->target_handle, sizeof e->target_handle, "%s", th ? th : "");
  return e->id;
}
flow_node *flow_get_node(flow_t *f, int id) { for (int i = 0; i < f->nnodes; i++) if (f->nodes[i].id == id) return &f->nodes[i]; return NULL; }
flow_edge *flow_get_edge(flow_t *f, int id) { for (int i = 0; i < f->nedges; i++) if (f->edges[i].id == id) return &f->edges[i]; return NULL; }
int flow_node_count(flow_t *f) { return f->nnodes; }
int flow_edge_count(flow_t *f) { return f->nedges; }
flow_node *flow_nodes(flow_t *f) { return f->nodes; }
flow_edge *flow_edges(flow_t *f) { return f->edges; }
flow_pt flow_node_abs(flow_t *f, const flow_node *n) {
  flow_pt p = n->pos; int parent = n->parent, guard = 0;
  while (parent != -1 && guard++ < 1024) { flow_node *pn = flow_get_node(f, parent); if (!pn) break;
    p.x += pn->pos.x; p.y += pn->pos.y; parent = pn->parent; }
  return p;
}
flow_rect flow_node_rect_abs(flow_t *f, const flow_node *n) { flow_pt a = flow_node_abs(f, n); flow_rect r = { a.x, a.y, n->w, n->h }; return r; }
flow_rect flow_bounds(flow_t *f) {
  if (f->nnodes == 0) { flow_rect z = {0,0,0,0}; return z; }
  flow_rect b = flow_node_rect_abs(f, &f->nodes[0]);
  for (int i = 1; i < f->nnodes; i++) b = flow_rect_union(b, flow_node_rect_abs(f, &f->nodes[i]));
  return b;
}
int flow_hit_node(flow_t *f, flow_pt screen) {
  for (int i = f->nnodes - 1; i >= 0; i--) {        /* topmost first */
    flow_node *n = &f->nodes[i]; flow_rect wr = flow_node_rect_abs(f, n);
    flow_pt s = flow_to_screen(f, (flow_pt){ wr.x, wr.y });
    flow_rect sr = { s.x, s.y, wr.w, wr.h };          /* zoom==1 in increment 1: size unscaled */
    if (flow_rect_contains(sr, screen)) return n->id;
  }
  return -1;
}
void flow_pan(flow_t *f, int dx, int dy) { f->view.ox += dx; f->view.oy += dy; }
flow_pt flow_to_screen(flow_t *f, flow_pt world_abs) { return flow_project(f->view, world_abs); }
flow_pt flow_to_world(flow_t *f, flow_pt screen) { return flow_unproject(f->view, screen); }
flow_viewport flow_view_get(flow_t *f) { return f->view; }
#endif
