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

/* keyboard command dispatch — a key fn runs from flow_feed when its seq matches */
typedef void (*flow_key_fn)(flow_t *f, void *user);

flow_t *flow_new(int cols, int rows);
void    flow_free(flow_t *f);
void    flow_resize(flow_t *f, int cols, int rows);
void    flow_register_node_type(flow_t *f, const flow_node_type *t);
void    flow_register_edge_type(flow_t *f, const flow_edge_type *t);
const flow_node_type *flow_node_type_for(flow_t *f, const char *type);
const flow_edge_type *flow_edge_type_for(flow_t *f, const char *type);
void    flow_measure_node(flow_t *f, flow_node *n);
int     flow_add_node(flow_t *f, const char *type, flow_pt pos, void *data);
void    flow_move_node(flow_t *f, int id, flow_pt pos);
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

/* selection (single-select for now; `additive` reserved for shift-multi-select) */
void flow_select_node(flow_t *f, int id, int additive);
void flow_clear_selection(flow_t *f);
int  flow_selected_node(flow_t *f);   /* first selected node id, or -1 */
int  flow_selected_edge(flow_t *f);   /* first selected edge id, or -1 */

/* mutators (callers re-fetch node/edge pointers after these — array may move) */
void flow_remove_node(flow_t *f, int id);  /* cascades incident edges AND child nodes (recursive), frees edge labels */
void flow_remove_edge(flow_t *f, int id);  /* frees label; no-op if id absent */

/* keyboard command dispatch: bindings (registry) + built-ins, driven from flow_feed */
void flow_bind_key(flow_t *f, const char *seq, flow_key_fn fn, void *user); /* register/override; matched before built-ins, longest seq first. Up to 32 bindings, seq <=7 bytes; over-limit binds are silently ignored. */
int  flow_dispatch_key(flow_t *f, const char *seq, int n); /* run a binding/built-in for one key seq; returns bytes consumed (>0) or 0 if unhandled */
void flow_delete_selection(flow_t *f);     /* built-in: remove selected node(s) then selected edge */
int  flow_add_node_center(flow_t *f, const char *type, void *data); /* add at world point under viewport center; returns id */
void flow_fit_view(flow_t *f, int margin); /* zoom==1: pan so flow_bounds is centred; no-op when empty */
void flow_set_statusbar(flow_t *f, int enabled); /* toggle the built-in bottom help/status line */

/* callbacks — the library/app seam (panel content stays app-side, like xyflow <Panel>) */
typedef struct {
  void (*on_overlay)(flow_t *f, flow_surface *screen, void *user);        /* draw HUD/panels last */
  void (*on_node_context)(flow_t *f, int node, flow_pt screen, void *user);/* right-click on a node */
  void (*on_node_click)(flow_t *f, int node, void *user);                  /* left-click (no drag)  */
  void (*on_pane_click)(flow_t *f, flow_pt world, void *user);             /* left-click empty space */
  void *user;
} flow_callbacks;
void flow_set_callbacks(flow_t *f, flow_callbacks cb);

/* minimap widget */
typedef enum { FLOW_CORNER_TL, FLOW_CORNER_TR, FLOW_CORNER_BL, FLOW_CORNER_BR } flow_corner;
void flow_set_minimap(flow_t *f, int enabled, flow_corner corner, int w, int h);

/* background grid widget (world-aligned; off by default — pass FLOW_BG_NONE to disable) */
typedef enum { FLOW_BG_NONE, FLOW_BG_DOTS, FLOW_BG_LINES, FLOW_BG_CROSS } flow_bg_variant;
void flow_set_background(flow_t *f, flow_bg_variant variant, int gap);

#ifdef FLOW_IMPLEMENTATION
struct flow {
  flow_node *nodes; int nnodes, capnodes, nextid;
  flow_edge *edges; int nedges, capedges, nexteid;
  const flow_node_type **ntypes; int nntypes;
  const flow_edge_type **etypes; int netypes;
  flow_viewport view; int cols, rows; flow_cell *front; int running;
  int drag_node, dragging_pan; flow_pt drag_grab, last_mouse;  /* mouse interaction state */
  int mouse_down, down_node, moved; flow_pt down_pos;          /* press/click tracking */
  flow_callbacks cb;
  struct { int enabled, w, h; flow_corner corner; } minimap;
  struct { flow_bg_variant variant; int gap; } bg;
  struct { char seq[8]; flow_key_fn fn; void *user; } keys[32]; int nkeys;  /* key-binding registry */
  int statusbar;  /* built-in bottom help/status line */
};
static void *flow__grow(void *arr, int *cap, int need, size_t sz) {
  if (need <= *cap) return arr;
  int c = *cap ? *cap : 8; while (c < need) c *= 2;
  arr = realloc(arr, (size_t)c * sz); *cap = c; return arr;
}
flow_t *flow_new(int cols, int rows) {
  flow_t *f = (flow_t*)calloc(1, sizeof *f);
  f->view.zoom = 1; f->cols = cols; f->rows = rows; f->nextid = 1; f->nexteid = 1;
  f->drag_node = -1;
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
void flow_move_node(flow_t *f, int id, flow_pt pos) {
  flow_node *n = flow_get_node(f, id);
  if (n) n->pos = pos;   /* increment 1/2a: top-level nodes only, so pos == absolute */
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
void flow_select_node(flow_t *f, int id, int additive) {
  if (!additive) for (int i = 0; i < f->nnodes; i++) f->nodes[i].flags &= ~FLOW_SELECTED;
  flow_node *n = flow_get_node(f, id); if (n) n->flags |= FLOW_SELECTED;
}
void flow_clear_selection(flow_t *f) { for (int i = 0; i < f->nnodes; i++) f->nodes[i].flags &= ~FLOW_SELECTED; }
int flow_selected_node(flow_t *f) { for (int i = 0; i < f->nnodes; i++) if (f->nodes[i].flags & FLOW_SELECTED) return f->nodes[i].id; return -1; }
void flow_set_callbacks(flow_t *f, flow_callbacks cb) { f->cb = cb; }
void flow_set_minimap(flow_t *f, int enabled, flow_corner corner, int w, int h) {
  f->minimap.enabled = enabled; f->minimap.corner = corner; f->minimap.w = w; f->minimap.h = h;
}
void flow_set_background(flow_t *f, flow_bg_variant variant, int gap) {
  f->bg.variant = variant; f->bg.gap = gap < 1 ? 1 : gap;  /* gap>=1 clamp: no modulo-by-zero */
}
int flow_selected_edge(flow_t *f) {
  for (int i = 0; i < f->nedges; i++) if (f->edges[i].flags & FLOW_SELECTED) return f->edges[i].id;
  return -1;
}
void flow_remove_edge(flow_t *f, int id) {
  for (int i = 0; i < f->nedges; i++) {
    if (f->edges[i].id != id) continue;
    free(f->edges[i].label);                                  /* free-then-shift: no leak, no dbl-free */
    memmove(&f->edges[i], &f->edges[i+1], (size_t)(f->nedges - i - 1) * sizeof(flow_edge));
    f->nedges--;                                              /* preserve insertion order */
    return;
  }
  /* absent id: no-op */
}
void flow_remove_node(flow_t *f, int id) {
  /* never hold a node/edge pointer across mutation: re-find by id each step (array moves). */
  for (;;) {                                                  /* recursively remove child nodes first */
    int childid = -1;
    for (int i = 0; i < f->nnodes; i++) if (f->nodes[i].parent == id) { childid = f->nodes[i].id; break; }
    if (childid == -1) break;
    flow_remove_node(f, childid);                             /* shrinks array; cycle-safe, terminates */
  }
  for (int i = 0; i < f->nedges; ) {                          /* remove incident edges, freeing labels */
    if (f->edges[i].source == id || f->edges[i].target == id) {
      free(f->edges[i].label);
      memmove(&f->edges[i], &f->edges[i+1], (size_t)(f->nedges - i - 1) * sizeof(flow_edge));
      f->nedges--;                                            /* do NOT i++ on the removal branch */
    } else i++;
  }
  for (int i = 0; i < f->nnodes; i++) {                       /* finally remove the node slot */
    if (f->nodes[i].id != id) continue;
    memmove(&f->nodes[i], &f->nodes[i+1], (size_t)(f->nnodes - i - 1) * sizeof(flow_node));
    f->nnodes--;
    return;
  }
}
void flow_delete_selection(flow_t *f) {
  /* remove all selected nodes (each cascades children + incident edges); re-query
     after each removal so any count works and cascade-removed selections are skipped */
  for (;;) { int id = flow_selected_node(f); if (id < 0) break; flow_remove_node(f, id); }
  for (;;) { int e  = flow_selected_edge(f); if (e  < 0) break; flow_remove_edge(f, e);  }
}
int flow_add_node_center(flow_t *f, const char *type, void *data) {
  flow_pt c = flow_to_world(f, (flow_pt){ f->cols / 2, f->rows / 2 });
  int id = flow_add_node(f, type, c, data);
  flow_node *n = flow_get_node(f, id);                        /* re-fetch for measured w,h */
  if (n) flow_move_node(f, id, (flow_pt){ c.x - n->w / 2, c.y - n->h / 2 });
  return id;
}
void flow_fit_view(flow_t *f, int margin) {
  /* zoom==1 ONLY (honest carry-over, like flow_hit_node): pure integer pan that
     centres flow_bounds. A future zoom package SUPERSEDES this exact function to
     also choose a zoom level — keep it one editable definition. `margin` is the
     unused-but-reserved usable-area inset; centring already gives equal margins. */
  (void)margin;
  if (f->nnodes == 0) return;                                 /* empty: no-op */
  flow_rect b = flow_bounds(f);
  f->view.zoom = 1;
  f->view.ox = (float)(f->cols / 2 - (b.x + b.w / 2));
  f->view.oy = (float)(f->rows / 2 - (b.y + b.h / 2));
}
void flow_set_statusbar(flow_t *f, int enabled) { f->statusbar = enabled ? 1 : 0; }
void flow_bind_key(flow_t *f, const char *seq, flow_key_fn fn, void *user) {
  if (!seq || !*seq) return;
  size_t len = strlen(seq); if (len >= sizeof f->keys[0].seq) return;  /* seq too long for slot */
  for (int i = 0; i < f->nkeys; i++)                          /* override existing binding for this seq */
    if (strcmp(f->keys[i].seq, seq) == 0) { f->keys[i].fn = fn; f->keys[i].user = user; return; }
  if (f->nkeys >= (int)(sizeof f->keys / sizeof f->keys[0])) return;   /* registry full */
  snprintf(f->keys[f->nkeys].seq, sizeof f->keys[0].seq, "%s", seq);
  f->keys[f->nkeys].fn = fn; f->keys[f->nkeys].user = user; f->nkeys++;
}
int flow_dispatch_key(flow_t *f, const char *seq, int n) {
  if (n <= 0) return 0;
  /* (1) registry: longest registered seq that fully fits in n and byte-matches. */
  int best = -1; size_t bestlen = 0;
  for (int i = 0; i < f->nkeys; i++) {
    size_t L = strlen(f->keys[i].seq);
    if (L == 0 || L > (size_t)n) continue;                    /* only claim if the ENTIRE seq fits */
    if (memcmp(f->keys[i].seq, seq, L) != 0) continue;
    if (L > bestlen) { bestlen = L; best = i; }
  }
  if (best >= 0) { if (f->keys[best].fn) f->keys[best].fn(f, f->keys[best].user); return (int)bestlen; }
  /* (2) built-ins, longest-first. Only claim if the entire seq fits in n. */
  if (n >= 4 && memcmp(seq, "\x1b[3~", 4) == 0) { flow_delete_selection(f); return 4; }  /* Delete */
  if (seq[0] == 'x') { flow_delete_selection(f); return 1; }
  if (seq[0] == 'n') { flow_add_node_center(f, "default", (void*)"node"); return 1; }    /* static label */
  if (seq[0] == 'f') { flow_fit_view(f, 2); return 1; }
  if (seq[0] == '?') { flow_set_statusbar(f, !f->statusbar); return 1; }
  /* (3) unhandled: q, bare arrows, anything else */
  return 0;
}
#endif
