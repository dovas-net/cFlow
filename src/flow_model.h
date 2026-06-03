/* ===== model: engine, nodes, edges, vtable types, transform, bounds, hit-test ===== */
enum { FLOW_SELECTED = 1u, FLOW_DRAGGING = 2u, FLOW_HOVERED = 4u };

/* zoom constants (used by flow_model, flow_view, flow_render, flow_types — all at/after flow_model) */
#define FLOW_ZOOM_MIN      0.25f   /* default lower clamp */
#define FLOW_ZOOM_MAX      4.0f    /* default upper clamp */
#define FLOW_ZOOM_STEP     1.2f    /* multiply/divide per zoom_in/out detent */
#define FLOW_LOD_THRESHOLD 0.6f    /* below this, node renderers collapse to a marker */
typedef enum { FLOW_HANDLE_SOURCE, FLOW_HANDLE_TARGET, FLOW_HANDLE_BOTH } flow_handle_kind;
typedef struct { char id[16]; flow_handle_kind kind; flow_pos pos; int along; } flow_handle; /* id must be a NUL-terminated C-string (matched by strcmp/strncmp, like node type[]/edge handle[]) */

typedef struct { int id; char type[32]; flow_pt pos; int parent; int w, h; void *data; unsigned flags; } flow_node;
typedef struct { int id, source, target; char source_handle[16], target_handle[16];
                 char type[16]; char *label; void *data; unsigned flags; } flow_edge;

typedef struct { float zoom; unsigned flags; int lod; } flow_render_ctx;
typedef struct { const char *type;
  void (*measure)(const flow_node*, int*, int*);
  void (*render)(const flow_node*, flow_surface*, flow_render_ctx);
  const flow_handle *handles; int handle_count;
  void (*save)(const flow_node *n, FILE *out);        /* optional: write node->data as ONE JSON value (omitted if NULL) */
  void (*load)(flow_node *n, const char *data_json);  /* optional: parse node->data from a NUL-terminated copy of the "data" value span */
} flow_node_type;
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
flow_pt   flow_node_pos(const flow_node *n);  /* relative (stored) position; companion to flow_node_abs */

/* subflows / groups — parent-relative coordinates + group container management.
   flow_set_parent: reparent `child` under `parent` (-1 detaches to top level), converting
     child->pos so its ABSOLUTE world position is unchanged. Rejects cycles (parent==child
     or parent is a descendant of child) and missing ids. Array order/ids unchanged.
   flow_group: create a `group` container enclosing the given node ids (world bbox+padding),
     reparent each under it (abs preserved); returns the new group id (-1 if none valid).
   flow_ungroup: reparent every direct child OUT to the group's own parent (abs preserved),
     THEN remove the now-childless container — children SURVIVE (unlike flow_remove_node).
   flow_is_ancestor: 1 if `maybe_ancestor` is `node` or any ancestor of `node`. */
void flow_set_parent(flow_t *f, int child, int parent);
int  flow_group(flow_t *f, const int *ids, int n);
void flow_ungroup(flow_t *f, int id);
int  flow_is_ancestor(flow_t *f, int maybe_ancestor, int node);
flow_rect flow_bounds(flow_t *f);
int       flow_hit_node(flow_t *f, flow_pt screen);
void      flow_pan(flow_t *f, int dx, int dy);
flow_pt   flow_to_screen(flow_t *f, flow_pt world_abs);
flow_pt   flow_to_world(flow_t *f, flow_pt screen);
flow_viewport flow_view_get(flow_t *f);

/* JSON persistence (implemented in src/flow_json.h, amalgamated after flow_render) */
int flow_save(flow_t *f, const char *path);  /* writes JSON; 0 ok, -1 on I/O/encode error */
int flow_load(flow_t *f, const char *path);  /* resets f then rebuilds from JSON; 0 ok, -1 on open/parse error */

/* handles & connections (port-handle drag/click to create edges) */
flow_pt flow_handle_anchor(flow_t *f, const flow_node *n, const flow_handle *h); /* world-abs cell: TOP->(x+w/2,y) RIGHT->(x+w-1,y+h/2) BOTTOM->(x+w/2,y+h-1) LEFT->(x,y+h/2), +'along' offset along the side */
int flow_node_handle_count(flow_t *f, int node);                  /* declared handles for node's type */
const flow_handle *flow_node_handle_at(flow_t *f, int node, int idx); /* idx-th handle of node's type, or NULL */
int flow_hit_handle(flow_t *f, flow_pt screen, int *out_node);   /* handle index + node id at screen cell, or -1; only on hovered/selected/connecting nodes */
void flow_set_hover(flow_t *f, int node);                         /* set FLOW_HOVERED on node (clears others); -1 clears all */
int flow_hovered_node(flow_t *f);                                 /* first FLOW_HOVERED node, or -1 */
int flow_begin_connection(flow_t *f, int node, const char *handle); /* enter connecting from source handle; 0 ok, -1 if not a valid source */
int flow_update_connection(flow_t *f, flow_pt screen);           /* move free end to screen cell; returns hovered candidate target node id or -1 */
int flow_end_connection(flow_t *f, int node, const char *handle);/* complete: add edge src->node; returns new edge id or -1; clears connecting */
void flow_cancel_connection(flow_t *f);                          /* abort in-flight connection */
int flow_connecting(flow_t *f);                                  /* 1 while a connection/preview is in flight */

/* selection — a true set, expressed via the FLOW_SELECTED node flag */
typedef enum { FLOW_SELECT_PARTIAL, FLOW_SELECT_FULL } flow_select_mode;
void flow_select_node(flow_t *f, int id, int additive);  /* additive=0 clears others then sets; additive=1 sets without clearing */
void flow_toggle_node(flow_t *f, int id);   /* add to selection if unset, remove if set; never clears others */
void flow_clear_selection(flow_t *f);
int  flow_selected_node(flow_t *f);   /* first selected node id, or -1 */
int  flow_selected_edge(flow_t *f);   /* first selected edge id, or -1 */
void flow_select_edge(flow_t *f, int id, int additive);  /* set FLOW_SELECTED on the edge; !additive clears node + other edge selection first (mutual exclusivity) */
int  flow_selected_count(flow_t *f);  /* number of FLOW_SELECTED nodes */
int  flow_selected_nodes(flow_t *f, int *out, int max);  /* fill out[] with selected ids in insertion order; returns total count (may exceed max) */
int  flow_select_in_rect(flow_t *f, flow_rect world, flow_select_mode mode, int additive);  /* select nodes contained (FULL) or intersecting (PARTIAL) the world rect; !additive clears first; returns count selected */
void flow_set_marquee_mode(flow_t *f, flow_select_mode mode);  /* default mode for shift-drag marquee (defaults to FLOW_SELECT_PARTIAL) */

/* mutators (callers re-fetch node/edge pointers after these — array may move) */
void flow_remove_node(flow_t *f, int id);  /* cascades incident edges AND child nodes (recursive), frees edge labels */
void flow_remove_edge(flow_t *f, int id);  /* frees label; no-op if id absent */
void flow_reconnect_edge(flow_t *f, int edge, int endpoint_node, const char *handle, int which); /* which: 0=source,1=target; repoint that endpoint; revalidated like flow_add_edge (rejects self + duplicate (source,target,handles)); edge left unchanged on rejection */
void flow_set_edge_label(flow_t *f, int edge, const char *label); /* strdup into edge->label, freeing prior; NULL clears */

/* edge hit-test & endpoints — DEFINED in flow_render.h (need the render anchor helpers).
   flow_hit_edge: topmost edge whose routed path passes within Chebyshev tol of screen, else -1.
   flow_edge_endpoint_screen: screen cell of source(0)/target(1) endpoint, matching flow_render EXACTLY; 1 on success. */
int  flow_hit_edge(flow_t *f, flow_pt screen, int tol);
int  flow_edge_endpoint_screen(flow_t *f, const flow_edge *e, int which, flow_pt *out);

/* keyboard command dispatch: bindings (registry) + built-ins, driven from flow_feed */
void flow_bind_key(flow_t *f, const char *seq, flow_key_fn fn, void *user); /* register/override; matched before built-ins, longest seq first. Up to 32 bindings, seq <=7 bytes; over-limit binds are silently ignored. */
int  flow_dispatch_key(flow_t *f, const char *seq, int n); /* run a binding/built-in for one key seq; returns bytes consumed (>0) or 0 if unhandled */
void flow_delete_selection(flow_t *f);     /* built-in: remove selected node(s) then selected edge */
int  flow_add_node_center(flow_t *f, const char *type, void *data); /* add at world point under viewport center; returns id */
void flow_fit_view(flow_t *f, int margin); /* getViewportForBounds: pick zoom+pan so flow_bounds fits with `margin` cells of padding (zoom clamped to [zmin,zmax]); no-op when empty */
void flow_set_statusbar(flow_t *f, int enabled); /* toggle the built-in bottom help/status line */

/* callbacks — the library/app seam (panel content stays app-side, like xyflow <Panel>) */
typedef struct {
  void (*on_overlay)(flow_t *f, flow_surface *screen, void *user);        /* draw HUD/panels last */
  void (*on_node_context)(flow_t *f, int node, flow_pt screen, void *user);/* right-click on a node */
  void (*on_node_click)(flow_t *f, int node, void *user);                  /* left-click (no drag)  */
  void (*on_pane_click)(flow_t *f, flow_pt world, void *user);             /* left-click empty space */
  void (*on_connect)(flow_t *f, int source, int target, void *user);      /* a connection was created (after flow_add_edge) */
  void (*on_node_dblclick)(flow_t *f, int node, void *user);              /* left double-click on a node body (fires AFTER on_node_click) */
  void (*on_selection_change)(flow_t *f, const int *ids, int n, void *user); /* fired after the FLOW_SELECTED node set changes; ids in insertion order, only on actual change */
  void (*on_nodes_delete)(flow_t *f, const int *ids, int n, void *user);  /* fired once per delete op with the removed node id set (a cascade reports its children) */
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
  flow_viewport view; float zmin, zmax; int cols, rows; flow_cell *front; int running;
  int drag_node, dragging_pan; flow_pt drag_grab, last_mouse;  /* mouse interaction state */
  flow_pt drag_last_world;                                     /* multi-drag: last drag pos in world coords (per-motion delta) */
  int mouse_down, down_node, moved; flow_pt down_pos;          /* press/click tracking */
  int down_modsel;                                             /* press was a SHIFT/CTRL modifier-select on a node (suppress release replace) */
  int space_held;                                              /* space-pan: sticky toggle (terminal model A) — a press forces drag-to-pan over node OR pane */
  int last_click_node;                                         /* dblclick: id of the previous node-body click (-1 = none/consumed); a 2nd click on the same id is a double-click */
  int cb_suppress;                                             /* >0 suppresses nested observer fires (on_nodes_delete / on_selection_change) from recursive/aggregate mutators (remove_node cascade, delete_selection, select_in_rect's internal clear) */
  int marquee_active, marquee_on; flow_pt marquee_anchor, marquee_cur; /* marquee: armed intent / live; screen coords */
  flow_select_mode marquee_mode;                              /* default mode for shift-drag marquee */
  int conn_active, conn_node; char conn_handle[16]; flow_pt conn_end; /* in-flight connection: source node/handle + free end (screen) */
  int reconnect_edge, reconnect_which;                        /* in-flight endpoint-reconnect drag: edge id (-1 idle) + which endpoint (0=source,1=target) */
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
  f->view.zoom = 1; f->zmin = FLOW_ZOOM_MIN; f->zmax = FLOW_ZOOM_MAX;
  f->cols = cols; f->rows = rows; f->nextid = 1; f->nexteid = 1;
  f->drag_node = -1; f->marquee_mode = FLOW_SELECT_PARTIAL; f->conn_node = -1;
  f->reconnect_edge = -1; f->last_click_node = -1;
  f->front = (flow_cell*)calloc((size_t)cols * rows, sizeof(flow_cell));
  return f;
}
void flow_free(flow_t *f) {
  if (!f) return;
  for (int i = 0; i < f->nedges; i++) free(f->edges[i].label);
  free(f->nodes); free(f->edges); free(f->ntypes); free(f->etypes); free(f->front); free(f);
}
/* Tear the graph back to empty for flow_load: free edge labels + node/edge arrays,
   NULL the pointers, zero counts/caps, reset id counters. Leaves view/types/cb/widgets
   intact. NEVER frees node->data (app owns it; same contract as flow_free).
   NOTE (serialize×undo seam): when undo lands, this must also clear the undo journal,
   else undo inverts against a replaced graph. */
static void flow__graph_reset(flow_t *f) {
  for (int i = 0; i < f->nedges; i++) free(f->edges[i].label);
  free(f->nodes); free(f->edges);
  f->nodes = NULL; f->edges = NULL;
  f->nnodes = f->capnodes = 0; f->nedges = f->capedges = 0;
  f->nextid = 1; f->nexteid = 1;
  f->last_click_node = -1;   /* drop the stale dblclick target: reused ids must not fake a double-click */
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
  /* ABSOLUTE-in contract: `pos` is a world-absolute target. Store it parent-relative so
     a child stays under the cursor. For top-level nodes (parent==-1) parent_abs=={0,0},
     so n->pos == pos — byte-identical to the pre-groups behaviour for every existing caller. */
  flow_node *n = flow_get_node(f, id);
  if (!n) return;
  flow_pt pa = { 0, 0 };
  if (n->parent != -1) { flow_node *p = flow_get_node(f, n->parent); if (p) pa = flow_node_abs(f, p); }
  n->pos.x = pos.x - pa.x; n->pos.y = pos.y - pa.y;
}
flow_pt flow_node_pos(const flow_node *n) { return n->pos; }
/* depth = length of the parent chain (0 for top-level). O(depth) walk, cycle-guarded. */
static int flow__node_depth(flow_t *f, const flow_node *n) {
  int d = 0, parent = n->parent, guard = 0;
  while (parent != -1 && guard++ < 1024) { flow_node *pn = flow_get_node(f, parent); if (!pn) break; d++; parent = pn->parent; }
  return d;
}
int flow_is_ancestor(flow_t *f, int maybe_ancestor, int node) {
  int cur = node, guard = 0;                       /* maybe_ancestor==node counts (chain includes self) */
  while (cur != -1 && guard++ < 1024) {
    if (cur == maybe_ancestor) return 1;
    flow_node *n = flow_get_node(f, cur); if (!n) break; cur = n->parent;
  }
  return 0;
}
void flow_set_parent(flow_t *f, int child, int parent) {
  flow_node *c = flow_get_node(f, child);
  if (!c) return;
  if (parent == child) return;                     /* trivial self-parent */
  if (parent != -1) {
    if (!flow_get_node(f, parent)) return;         /* missing parent id */
    if (flow_is_ancestor(f, child, parent)) return;/* cycle: parent is a descendant of child */
  }
  /* read both abs positions while the OLD parent chain is still intact */
  flow_pt cabs = flow_node_abs(f, c);
  flow_pt pabs = { 0, 0 };
  if (parent != -1) { flow_node *p = flow_get_node(f, parent); pabs = flow_node_abs(f, p); }
  c->parent = parent;
  c->pos.x = cabs.x - pabs.x; c->pos.y = cabs.y - pabs.y;   /* abs preserved across the move */
}
int flow_group(flow_t *f, const int *ids, int n) {
  if (!ids || n <= 0) return -1;
  /* world bbox of the valid member ids (pointers valid only before flow_add_node) */
  flow_rect bb = {0,0,0,0}; int have = 0;
  for (int i = 0; i < n; i++) {
    flow_node *m = flow_get_node(f, ids[i]); if (!m) continue;
    flow_rect mr = flow_node_rect_abs(f, m);
    bb = have ? flow_rect_union(bb, mr) : mr; have = 1;
  }
  if (!have) return -1;
  const int pad = 1;
  flow_pt gpos = { bb.x - pad, bb.y - pad };
  int gw = bb.w + 2 * pad, gh = bb.h + 2 * pad;
  int gid = flow_add_node(f, "group", gpos, NULL);   /* measure is a no-op write-back; w/h set next */
  flow_node *g = flow_get_node(f, gid);              /* re-fetch: array may have realloc'd */
  if (g) { g->w = gw; g->h = gh; }
  for (int i = 0; i < n; i++) {                       /* reparent members (abs preserved) */
    if (flow_get_node(f, ids[i])) flow_set_parent(f, ids[i], gid);
  }
  return gid;
}
void flow_ungroup(flow_t *f, int id) {
  flow_node *g = flow_get_node(f, id);
  if (!g || strcmp(g->type, "group") != 0) return;   /* no-op if missing or not a group */
  int gparent = g->parent;
  for (;;) {                                          /* reparent each direct child OUT (abs preserved) */
    int childid = -1;
    for (int i = 0; i < f->nnodes; i++) if (f->nodes[i].parent == id) { childid = f->nodes[i].id; break; }
    if (childid == -1) break;
    flow_set_parent(f, childid, gparent);             /* re-find by id each step (array stable here, but safe) */
  }
  flow_remove_node(f, id);                             /* now childless: removes only the container */
}
int flow_add_edge(flow_t *f, int src, int dst, const char *sh, const char *th) {
  if (src == dst) return -1;
  if (!flow_get_node(f, src) || !flow_get_node(f, dst)) return -1;
  const char *shs = sh ? sh : "", *ths = th ? th : "";
  for (int i = 0; i < f->nedges; i++)                          /* dup = same (source,target,handles) */
    if (f->edges[i].source == src && f->edges[i].target == dst &&
        strcmp(f->edges[i].source_handle, shs) == 0 && strcmp(f->edges[i].target_handle, ths) == 0) return -1;
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
/* Shared level-of-detail + footprint helpers. BOTH flow_hit_node and the render node
   loop go through these so the hittable area and the drawn area can never drift.
   flow__lod_for: 1 (collapsed) when zoom is below FLOW_LOD_THRESHOLD, else 0 (full).
   flow__node_footprint: the screen-space rect a node occupies — projected top-left,
   full w/h at lod 0, a single collapsed-marker cell (1x1) at lod 1. (Glyph size is
   constant: only POSITION scales with zoom; low zoom is expressed by LOD, not size.) */
static int flow__lod_for(flow_t *f, float zoom) { (void)f; return zoom < FLOW_LOD_THRESHOLD ? 1 : 0; }
static flow_rect flow__node_footprint(flow_t *f, const flow_node *n, int lod) {
  flow_rect wr = flow_node_rect_abs(f, n);
  flow_pt s = flow_to_screen(f, (flow_pt){ wr.x, wr.y });
  if (lod) { flow_rect r = { s.x, s.y, 1, 1 }; return r; }  /* collapsed marker: 1x1 at top-left */
  flow_rect r = { s.x, s.y, wr.w, wr.h }; return r;          /* full box (constant glyph size) */
}
/* Depth-aware visit order. Array order is NO LONGER a valid z/hit order once groups
   exist (a group appended at the tail must draw UNDER its earlier children and never
   steal their hits). We build a TRANSIENT index list each pass (the nodes[] array,
   ids, serialize and flow_get_node stay untouched) sorted by a stable key:
     RENDER (want_render=1): depth ASC, then unselected-before-selected, then array idx ASC
       — parent-before-child DOMINATES; selected-last applies only AMONG SIBLINGS.
     HIT    (want_render=0): depth DESC, then array idx DESC (no selection term)
       — topmost descendant first; for all-top-level scenes this reduces to reverse-array,
         keeping every existing hit-order golden byte-identical.
   idx is a unique final tiebreaker so qsort is deterministic. Caller frees the list. */
typedef struct { int idx, depth, sel; } flow__order_ent;
static int flow__order_cmp_render(const void *pa, const void *pb) {
  const flow__order_ent *a = (const flow__order_ent*)pa, *b = (const flow__order_ent*)pb;
  if (a->depth != b->depth) return a->depth - b->depth;   /* depth asc */
  if (a->sel  != b->sel)   return a->sel  - b->sel;       /* unselected(0) before selected(1) */
  return a->idx - b->idx;                                 /* array idx asc */
}
static int flow__order_cmp_hit(const void *pa, const void *pb) {
  const flow__order_ent *a = (const flow__order_ent*)pa, *b = (const flow__order_ent*)pb;
  if (a->depth != b->depth) return b->depth - a->depth;   /* depth desc */
  return b->idx - a->idx;                                 /* array idx desc */
}
/* Allocate + fill an ordered index list (length f->nnodes). Returns NULL when empty. */
static int *flow__node_order(flow_t *f, int want_render) {
  if (f->nnodes <= 0) return NULL;
  flow__order_ent *ents = (flow__order_ent*)malloc((size_t)f->nnodes * sizeof *ents);
  if (!ents) return NULL;
  for (int i = 0; i < f->nnodes; i++) {
    ents[i].idx = i; ents[i].depth = flow__node_depth(f, &f->nodes[i]);
    ents[i].sel = (f->nodes[i].flags & FLOW_SELECTED) ? 1 : 0;
  }
  qsort(ents, (size_t)f->nnodes, sizeof *ents, want_render ? flow__order_cmp_render : flow__order_cmp_hit);
  int *order = (int*)malloc((size_t)f->nnodes * sizeof(int));
  if (order) for (int i = 0; i < f->nnodes; i++) order[i] = ents[i].idx;
  free(ents);
  return order;
}
int flow_hit_node(flow_t *f, flow_pt screen) {
  int lod = flow__lod_for(f, f->view.zoom);
  int *order = flow__node_order(f, 0);                /* topmost-descendant first */
  int hit = -1;
  for (int k = 0; k < f->nnodes; k++) {
    flow_node *n = &f->nodes[order ? order[k] : k];
    flow_rect sr = flow__node_footprint(f, n, lod);   /* exact footprint the renderer draws */
    if (flow_rect_contains(sr, screen)) { hit = n->id; break; }
  }
  free(order);
  return hit;
}
void flow_pan(flow_t *f, int dx, int dy) { f->view.ox += dx; f->view.oy += dy; }
flow_pt flow_to_screen(flow_t *f, flow_pt world_abs) { return flow_project(f->view, world_abs); }
flow_pt flow_to_world(flow_t *f, flow_pt screen) { return flow_unproject(f->view, screen); }
flow_viewport flow_view_get(flow_t *f) { return f->view; }
/* ---- observer plumbing (events package) ---- */
/* order-sensitive signature of the FLOW_SELECTED node set: differs iff the set or its
   insertion order changes. Lets on_selection_change fire ONLY on an actual change. */
static unsigned long flow__sel_sig(flow_t *f) {
  unsigned long sig = 1469598103934665603UL; int c = 0;          /* FNV-1a seed */
  for (int i = 0; i < f->nnodes; i++)
    if (f->nodes[i].flags & FLOW_SELECTED) { sig = (sig ^ (unsigned long)f->nodes[i].id) * 1099511628211UL; c++; }
  return sig ^ ((unsigned long)c << 1);
}
/* fire on_selection_change iff the selected set changed vs sig_before and we are not
   inside a suppressed (recursive/aggregate) mutator. ids = selected ids, insertion order. */
static void flow__notify_selection(flow_t *f, unsigned long sig_before) {
  if (!f->cb.on_selection_change || f->cb_suppress) return;
  if (flow__sel_sig(f) == sig_before) return;
  int n = flow_selected_count(f);
  int *ids = n > 0 ? (int*)malloc((size_t)n * sizeof(int)) : NULL;
  if (n > 0) flow_selected_nodes(f, ids, n);
  f->cb.on_selection_change(f, ids, n, f->cb.user);
  free(ids);
}
/* 1 if node `id` is selected or has a selected ancestor — i.e. it will be removed when the
   current selection is deleted (flow_remove_node cascades a selected node's descendants). */
static int flow__sel_or_ancestor(flow_t *f, int id) {
  int guard = 0;
  while (id != -1 && guard++ < 1024) {
    flow_node *n = flow_get_node(f, id); if (!n) break;
    if (n->flags & FLOW_SELECTED) return 1;
    id = n->parent;
  }
  return 0;
}
void flow_select_node(flow_t *f, int id, int additive) {
  unsigned long sig = flow__sel_sig(f);
  if (!additive) {
    for (int i = 0; i < f->nnodes; i++) f->nodes[i].flags &= ~FLOW_SELECTED;
    for (int i = 0; i < f->nedges; i++) f->edges[i].flags &= ~FLOW_SELECTED;  /* mutual exclusivity: deselect any edge */
  }
  flow_node *n = flow_get_node(f, id); if (n) n->flags |= FLOW_SELECTED;
  flow__notify_selection(f, sig);
}
void flow_toggle_node(flow_t *f, int id) {
  unsigned long sig = flow__sel_sig(f);
  flow_node *n = flow_get_node(f, id); if (n) n->flags ^= FLOW_SELECTED;  /* never clears others */
  flow__notify_selection(f, sig);
}
void flow_clear_selection(flow_t *f) {
  unsigned long sig = flow__sel_sig(f);
  for (int i = 0; i < f->nnodes; i++) f->nodes[i].flags &= ~FLOW_SELECTED;
  for (int i = 0; i < f->nedges; i++) f->edges[i].flags &= ~FLOW_SELECTED;  /* edges too */
  flow__notify_selection(f, sig);
}
int flow_selected_node(flow_t *f) { for (int i = 0; i < f->nnodes; i++) if (f->nodes[i].flags & FLOW_SELECTED) return f->nodes[i].id; return -1; }
int flow_selected_count(flow_t *f) { int c = 0; for (int i = 0; i < f->nnodes; i++) if (f->nodes[i].flags & FLOW_SELECTED) c++; return c; }
int flow_selected_nodes(flow_t *f, int *out, int max) {
  int c = 0;                                            /* insertion order; total may exceed max */
  for (int i = 0; i < f->nnodes; i++) if (f->nodes[i].flags & FLOW_SELECTED) { if (c < max && out) out[c] = f->nodes[i].id; c++; }
  return c;
}
int flow_select_in_rect(flow_t *f, flow_rect world, flow_select_mode mode, int additive) {
  unsigned long sig = flow__sel_sig(f);
  f->cb_suppress++;                                   /* the internal clear must not fire its own change event */
  if (!additive) flow_clear_selection(f);
  f->cb_suppress--;
  int c = 0;
  for (int i = 0; i < f->nnodes; i++) {
    flow_node *n = &f->nodes[i];
    flow_rect nr = flow_node_rect_abs(f, n);            /* world rect; compared against the world-space marquee (caller unprojects) */
    int hit;
    if (mode == FLOW_SELECT_FULL) {                     /* node fully inside marquee */
      hit = nr.x >= world.x && nr.y >= world.y &&
            nr.x + nr.w <= world.x + world.w && nr.y + nr.h <= world.y + world.h;
    } else {                                            /* PARTIAL: any overlap */
      hit = flow_rect_intersects(world, nr);
    }
    if (hit) { n->flags |= FLOW_SELECTED; c++; }
  }
  flow__notify_selection(f, sig);                     /* one fire per call, only if the set changed */
  return c;
}
void flow_set_marquee_mode(flow_t *f, flow_select_mode mode) { f->marquee_mode = mode; }
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
void flow_select_edge(flow_t *f, int id, int additive) {
  if (!additive) flow_clear_selection(f);  /* clears node AND other edge selection (mutual exclusivity) */
  flow_edge *e = flow_get_edge(f, id); if (e) e->flags |= FLOW_SELECTED;
}
void flow_reconnect_edge(flow_t *f, int edge, int endpoint_node, const char *handle, int which) {
  flow_edge *e = flow_get_edge(f, edge);
  if (!e || !flow_get_node(f, endpoint_node)) return;
  /* compute the prospective endpoints/handles, validate, THEN commit (no rollback) */
  int nsrc = e->source, ntgt = e->target;
  const char *nsh = e->source_handle, *nth = e->target_handle;
  const char *hs = handle ? handle : "";
  if (which == 0) { nsrc = endpoint_node; nsh = hs; } else { ntgt = endpoint_node; nth = hs; }
  if (nsrc == ntgt) return;                                    /* reject self-edge */
  for (int i = 0; i < f->nedges; i++) {                        /* reject duplicate (skip this edge) */
    if (f->edges[i].id == edge) continue;
    if (f->edges[i].source == nsrc && f->edges[i].target == ntgt &&
        strcmp(f->edges[i].source_handle, nsh) == 0 &&
        strcmp(f->edges[i].target_handle, nth) == 0) return;
  }
  e->source = nsrc; e->target = ntgt;   /* scalar self-assign on the unchanged side is harmless */
  /* write ONLY the changed endpoint's handle, from the caller arg `hs` (never aliases
     e's buffers). Rewriting the unchanged side from e->*_handle would be a self-overlapping
     snprintf (UB per C11 7.21.6.5), so leave it untouched. */
  if (which == 0) snprintf(e->source_handle, sizeof e->source_handle, "%s", hs);
  else            snprintf(e->target_handle, sizeof e->target_handle, "%s", hs);
}
void flow_set_edge_label(flow_t *f, int edge, const char *label) {
  flow_edge *e = flow_get_edge(f, edge); if (!e) return;
  free(e->label); e->label = NULL;
  if (label) {                                                 /* malloc+memcpy (avoid strdup decl issues under -std=c11) */
    size_t n = strlen(label) + 1;
    e->label = (char*)malloc(n);
    if (e->label) memcpy(e->label, label, n);
  }
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
  /* on a DIRECT call (not a recursion or a delete_selection-suppressed one), fire
     on_nodes_delete once for this node + its whole descendant cascade, in insertion order,
     BEFORE removal so the app can still read node->data. Recursive child removals below run
     suppressed so they never refire. */
  int top = (f->cb_suppress == 0);
  unsigned long sig = 0;
  if (top) {
    sig = flow__sel_sig(f);
    if (f->cb.on_nodes_delete) {
      int *ids = f->nnodes ? (int*)malloc((size_t)f->nnodes * sizeof(int)) : NULL; int n = 0;
      for (int i = 0; i < f->nnodes; i++)
        if (flow_is_ancestor(f, id, f->nodes[i].id)) ids[n++] = f->nodes[i].id; /* id and its descendants */
      if (n) f->cb.on_nodes_delete(f, ids, n, f->cb.user);
      free(ids);
    }
  }
  f->cb_suppress++;
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
    break;                                                    /* break (not return) so cleanup below runs */
  }
  f->cb_suppress--;
  if (top) flow__notify_selection(f, sig);                    /* selection shrank if a selected node vanished */
}
void flow_delete_selection(flow_t *f) {
  /* remove all selected nodes (each cascades children + incident edges); re-query
     after each removal so any count works and cascade-removed selections are skipped.
     on_nodes_delete fires ONCE here with the full set (selected ∪ their descendants);
     the inner flow_remove_node calls run suppressed so they don't each refire. */
  unsigned long sig = flow__sel_sig(f);
  if (f->cb.on_nodes_delete) {
    int *ids = f->nnodes ? (int*)malloc((size_t)f->nnodes * sizeof(int)) : NULL; int n = 0;
    for (int i = 0; i < f->nnodes; i++)
      if (flow__sel_or_ancestor(f, f->nodes[i].id)) ids[n++] = f->nodes[i].id;
    if (n) f->cb.on_nodes_delete(f, ids, n, f->cb.user);
    free(ids);
  }
  f->cb_suppress++;
  for (;;) { int id = flow_selected_node(f); if (id < 0) break; flow_remove_node(f, id); }
  for (;;) { int e  = flow_selected_edge(f); if (e  < 0) break; flow_remove_edge(f, e);  }
  f->cb_suppress--;
  flow__notify_selection(f, sig);
}
int flow_add_node_center(flow_t *f, const char *type, void *data) {
  flow_pt c = flow_to_world(f, (flow_pt){ f->cols / 2, f->rows / 2 });
  int id = flow_add_node(f, type, c, data);
  flow_node *n = flow_get_node(f, id);                        /* re-fetch for measured w,h */
  if (n) flow_move_node(f, id, (flow_pt){ c.x - n->w / 2, c.y - n->h / 2 });
  return id;
}
void flow_fit_view(flow_t *f, int margin) {
  /* getViewportForBounds: pick a zoom that makes flow_bounds fit inside the usable
     area (cols/rows minus `margin` cells of padding on each side), clamped to the
     [zmin,zmax] range, then pan so the bounds centre lands on the screen centre.
     One editable definition — the zoom package made this zoom-aware in place. */
  if (f->nnodes == 0) return;                                 /* empty: no-op */
  flow_rect b = flow_bounds(f);
  if (b.w <= 0 || b.h <= 0) return;
  float zx = (float)(f->cols - 2 * margin) / (float)b.w;
  float zy = (float)(f->rows - 2 * margin) / (float)b.h;
  float z = zx < zy ? zx : zy;
  if (z < f->zmin) z = f->zmin;
  if (z > f->zmax) z = f->zmax;
  f->view.zoom = z;
  f->view.ox = f->cols / 2.0f - (b.x + b.w / 2.0f) * z;
  f->view.oy = f->rows / 2.0f - (b.y + b.h / 2.0f) * z;
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
  if (seq[0] == ' ') { f->space_held = !f->space_held; return 1; }  /* space-pan: sticky toggle (no key-up in a TTY) */
  /* (3) unhandled: q, bare arrows, anything else */
  return 0;
}
/* ----- handles & connections ----- */
flow_pt flow_handle_anchor(flow_t *f, const flow_node *n, const flow_handle *h) {
  flow_rect r = flow_node_rect_abs(f, n);            /* world-abs cell anchor; consumers project via flow_to_screen */
  flow_pt p; int along = h ? h->along : 0;
  switch (h ? h->pos : FLOW_RIGHT) {
    case FLOW_TOP:    p.x = r.x + r.w / 2 + along; p.y = r.y;             break;
    case FLOW_BOTTOM: p.x = r.x + r.w / 2 + along; p.y = r.y + r.h - 1;   break;
    case FLOW_LEFT:   p.x = r.x;                   p.y = r.y + r.h / 2 + along; break;
    default:          p.x = r.x + r.w - 1;         p.y = r.y + r.h / 2 + along; break;  /* RIGHT */
  }
  return p;
}
int flow_node_handle_count(flow_t *f, int node) {
  flow_node *n = flow_get_node(f, node); if (!n) return 0;
  const flow_node_type *t = flow_node_type_for(f, n->type);
  return t ? t->handle_count : 0;
}
const flow_handle *flow_node_handle_at(flow_t *f, int node, int idx) {
  flow_node *n = flow_get_node(f, node); if (!n) return NULL;
  const flow_node_type *t = flow_node_type_for(f, n->type);
  if (!t || !t->handles || idx < 0 || idx >= t->handle_count) return NULL;
  return &t->handles[idx];
}
/* shared handle projection + visibility gate — flow_hit_handle and the render
   marker pass MUST agree (handles render exactly where they're hittable).
   Node BODIES use constant glyph size (don't scale with zoom), so the handle's
   in-body offset must be applied UNSCALED against the rendered footprint — else at
   zoom>1 the anchor (projected from world) floats offset*(zoom-1) cells off the body.
   At zoom==1 this is byte-identical to project(world_anchor); at LOD-collapse the
   footprint is the 1x1 marker so the offset clamps to (0,0) -> the marker cell. */
static flow_pt flow__handle_screen(flow_t *f, const flow_node *n, const flow_handle *h) {
  flow_rect wr = flow_node_rect_abs(f, n);                 /* world rect (top-left + w,h) */
  flow_pt   wa = flow_handle_anchor(f, n, h);              /* world anchor (handles NULL h + 'along') */
  int ox = wa.x - wr.x, oy = wa.y - wr.y;                  /* in-body offset in CONSTANT cells */
  flow_rect fp = flow__node_footprint(f, n, flow__lod_for(f, f->view.zoom)); /* screen rect: full OR collapsed */
  if (ox > fp.w - 1) ox = fp.w - 1; if (ox < 0) ox = 0;    /* clamp into footprint */
  if (oy > fp.h - 1) oy = fp.h - 1; if (oy < 0) oy = 0;
  flow_pt s = { fp.x + ox, fp.y + oy }; return s;
}
static int flow__node_handles_visible(flow_t *f, const flow_node *n) {
  return (n->flags & (FLOW_HOVERED | FLOW_SELECTED)) || n->id == f->conn_node;
}
int flow_hit_handle(flow_t *f, flow_pt screen, int *out_node) {
  /* topmost first; only handles on hovered/selected/connecting nodes are hittable */
  for (int i = f->nnodes - 1; i >= 0; i--) {
    flow_node *n = &f->nodes[i];
    if (!flow__node_handles_visible(f, n)) continue;
    int hc = flow_node_handle_count(f, n->id);
    for (int j = 0; j < hc; j++) {
      const flow_handle *h = flow_node_handle_at(f, n->id, j);
      flow_pt s = flow__handle_screen(f, n, h);
      if (s.x == screen.x && s.y == screen.y) { if (out_node) *out_node = n->id; return j; }
    }
  }
  if (out_node) *out_node = -1;
  return -1;
}
void flow_set_hover(flow_t *f, int node) {
  for (int i = 0; i < f->nnodes; i++) f->nodes[i].flags &= ~FLOW_HOVERED;
  flow_node *n = flow_get_node(f, node); if (n) n->flags |= FLOW_HOVERED;
}
int flow_hovered_node(flow_t *f) {
  for (int i = 0; i < f->nnodes; i++) if (f->nodes[i].flags & FLOW_HOVERED) return f->nodes[i].id;
  return -1;
}
/* find a node's handle by id; returns it or NULL */
static const flow_handle *flow__handle_named(flow_t *f, int node, const char *id) {
  if (!id) return NULL;
  int hc = flow_node_handle_count(f, node);
  for (int j = 0; j < hc; j++) { const flow_handle *h = flow_node_handle_at(f, node, j);
    if (h && strncmp(h->id, id, sizeof h->id) == 0) return h; }
  return NULL;
}
int flow_begin_connection(flow_t *f, int node, const char *handle) {
  const flow_handle *h = flow__handle_named(f, node, handle);
  if (!h || (h->kind != FLOW_HANDLE_SOURCE && h->kind != FLOW_HANDLE_BOTH)) return -1;  /* must be a source */
  f->conn_active = 1; f->conn_node = node;
  snprintf(f->conn_handle, sizeof f->conn_handle, "%s", handle ? handle : "");
  flow_node *n = flow_get_node(f, node);
  if (n) f->conn_end = flow_to_screen(f, flow_handle_anchor(f, n, h));   /* start the free end at the source */
  return 0;
}
int flow_update_connection(flow_t *f, flow_pt screen) {
  if (!f->conn_active) return -1;
  f->conn_end = screen;
  int tn = -1; (void)flow_hit_handle(f, screen, &tn);   /* prefer a handle under the cursor */
  if (tn == -1) tn = flow_hit_node(f, screen);          /* else any node body */
  if (tn != -1 && tn != f->conn_node) flow_set_hover(f, tn);  /* reveal candidate's handles */
  return (tn != f->conn_node) ? tn : -1;
}
int flow_end_connection(flow_t *f, int node, const char *handle) {
  if (!f->conn_active) return -1;
  int src = f->conn_node; char sh[16]; snprintf(sh, sizeof sh, "%s", f->conn_handle);
  /* clear connecting state up-front so a rejected add never leaves us stuck */
  f->conn_active = 0; f->conn_node = -1; f->conn_handle[0] = 0;
  /* target handle: explicit name, else first TARGET/BOTH handle on the node */
  const char *th = handle;
  if (!th && node != -1) {
    int hc = flow_node_handle_count(f, node);
    for (int j = 0; j < hc; j++) { const flow_handle *h = flow_node_handle_at(f, node, j);
      if (h && (h->kind == FLOW_HANDLE_TARGET || h->kind == FLOW_HANDLE_BOTH)) { th = h->id; break; } }
  }
  int eid = flow_add_edge(f, src, node, sh, th);
  if (eid != -1 && f->cb.on_connect) f->cb.on_connect(f, src, node, f->cb.user);
  return eid;
}
void flow_cancel_connection(flow_t *f) {
  f->conn_active = 0; f->conn_node = -1; f->conn_handle[0] = 0;
}
int flow_connecting(flow_t *f) { return f->conn_active ? 1 : 0; }
#endif
