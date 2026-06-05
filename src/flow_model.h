/* ===== model: engine, nodes, edges, vtable types, transform, bounds, hit-test ===== */
enum { FLOW_SELECTED = 1u, FLOW_DRAGGING = 2u, FLOW_HOVERED = 4u, FLOW_HIDDEN = 8u, FLOW_EXTENT_PARENT = 16u };
/* FLOW_EXTENT_PARENT gates flow_move_node's child-inside-parent clamp; set/clear
   directly on n->flags. FLOW_HIDDEN is a VIEW-level skip (render, hit, marquee,
   bounds, minimap, handles) set via flow_set_node_hidden / flow_set_edge_hidden;
   MODEL-level ops (traversal/intersect queries, layout, serialize) still include
   hidden elements. Neither flag is journaled or persisted. */

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
/* extent clamps (xyflow nodeExtent / translateExtent, spec §6.1/§6.2) — both optional,
   disabled by default (zero rect; w<=0 or h<=0 = disabled). Transient: not saved/journaled. */
void flow_set_node_extent(flow_t *f, flow_rect world);      /* clamp flow_move_node targets so the node rect stays inside `world` (flush to the exceeded edge); applies to FUTURE moves only */
void flow_set_translate_extent(flow_t *f, flow_rect world); /* clamp pan/zoom/fit so the VISIBLE world window stays inside `world`; re-clamps the current view immediately (precedent: flow_set_zoom_limits) */
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

/* isValidConnection predicate (inc-4 #9): return 1 to allow, 0 to reject. A GATE, not
   an observer — it decides whether the mutation happens, so it lives on struct flow,
   NOT in flow_callbacks (those fire after success). Called at the ENGINE level for
   EVERY add/reconnect attempt, programmatic or interactive (deliberate divergence
   from xyflow's interactive-only gating), AFTER the structural rejects (self-edge,
   missing nodes, duplicates) so expensive user logic only sees structurally valid
   proposals. Rejection is silent: graph unchanged, nothing journaled, no callbacks.
   handles are "" if none. NULL fn (the default) = allow all, zero overhead.
   TRANSIENT, like the extents — not persisted. flow_load SUSPENDS the validator
   across its edge rebuild (save/NULL/restore, inc-5 #2) so a saved graph loads
   faithfully regardless of validator state; the validator remains installed and
   gates add/reconnect calls again the moment flow_load returns. */
typedef int (*flow_connection_validator)(flow_t *f, int source, int target,
                                         const char *source_handle, const char *target_handle,
                                         void *user);
void flow_set_connection_validator(flow_t *f, flow_connection_validator fn, void *user);
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
/* hidden flags (inc-4 #11) — VIEW-level visibility, MODEL-level presence. A hidden
   node is skipped by render, hit-tests, marquee selection, bounds/fit and the
   minimap; traversal/intersect queries, layout and serialize still see it. A hidden
   edge — or any edge with a hidden endpoint (cascade) — is skipped by render and
   hit-test. Hiding a SELECTED node deselects it (sig-gated on_selection_change);
   un-hiding does NOT reselect. NOT journaled (UI-transient, like zoom) and NOT
   persisted in v1: flags reload as 0, so saved-hidden elements come back visible. */
void flow_set_node_hidden(flow_t *f, int id, int hidden);
void flow_set_edge_hidden(flow_t *f, int id, int hidden);
void flow_set_autopan(flow_t *f, int margin, int speed); /* tune the drag auto-pan band (defaults 3/2): margin = band width in cells, speed = step per motion event; negatives clamp to 0, margin 0 disables */

/* ---- undo/redo: capped inverse-op command journal (spec §11) ----
   Every recorded mutator (add/remove node+edge, move, reconnect, set-label, reparent)
   pushes an inverse-op record keyed on STABLE ids; flow_undo/flow_redo replay inverses.
   One journal command == one user-visible undo STEP: a transaction bracket
   (flow__undo_begin/end) coalesces a whole gesture (multi-drag, group/ungroup,
   add-node-center, connect, delete-selection) into a single command. Selection and
   viewport zoom/pan/fit are deliberately NOT journaled (spec §11 omits them).
   Definitions live in src/flow_undo.h (amalgamated after flow_model) because applying
   an inverse calls the mutators defined here. */
void flow_undo(flow_t *f);        /* pop the top command, apply its inverse; no-op if empty */
void flow_redo(flow_t *f);        /* re-apply the last undone command; no-op if redo stack empty */
int  flow_can_undo(flow_t *f);
int  flow_can_redo(flow_t *f);
void flow_set_undo_limit(flow_t *f, int max_commands); /* cap history depth (default 128); evicting the
                                     oldest frees its label copies (drops, never frees, node->data ptrs).
                                     0 = disable journaling entirely; negative clamps to 0. */

/* internal recording/txn primitives, DECLARED here, DEFINED in flow_model.h's impl block.
   Pure data push — they never call mutators, so flow_model.h code may invoke them even
   though flow_undo/flow_redo (which DO call mutators) are defined later, in flow_undo.h. */
void flow__undo_begin(flow_t *f); /* open a coalescing transaction (nestable via depth counter) */
void flow__undo_end(flow_t *f);   /* close the innermost transaction */

typedef enum {
  FLOW_CMD_ADD_NODE, FLOW_CMD_REMOVE_NODE,   /* REMOVE_NODE snapshots the whole subtree */
  FLOW_CMD_ADD_EDGE, FLOW_CMD_REMOVE_EDGE,
  FLOW_CMD_MOVE_NODE, FLOW_CMD_RECONNECT_EDGE, FLOW_CMD_SET_LABEL,
  FLOW_CMD_REPARENT                          /* groups: invert flow_set_parent/group/ungroup */
} flow_cmd_kind;

typedef struct { int id, index; flow_node node; } flow__node_snap;   /* node.data borrowed */
typedef struct { int id, index; flow_edge edge; char *label_copy; } flow__edge_snap; /* edge.label points at label_copy */

/* one leaf mutation record (tagged union). nid0/eid0 = f->nextid/f->nexteid for the
   undo direction, nid1/eid1 for the redo direction (only ADD ops actually differ). */
typedef struct flow__op {
  flow_cmd_kind kind;
  int nid0, eid0, nid1, eid1;
  union {
    struct { flow__node_snap snap; } node;                            /* ADD_NODE (snap refreshed at undo) */
    struct { int root; flow__node_snap *nodes; int nn;
             flow__edge_snap *edges; int ne; } subtree;               /* REMOVE_NODE */
    struct { flow__edge_snap snap; } edge;                            /* ADD_EDGE / REMOVE_EDGE */
    struct { int id; flow_pt from, to; } move;                        /* MOVE_NODE (ABSOLUTE coords) */
    struct { int id, which, from_node, to_node;
             char from_handle[16], to_handle[16]; } reconnect;        /* RECONNECT_EDGE */
    struct { int id; char *from, *to; } label;                        /* SET_LABEL (owned dups, may be NULL) */
    struct { int child, from_parent, to_parent;
             flow_pt from_pos, to_pos; } reparent;                    /* REPARENT (stored-rel positions) */
  } u;
} flow__op;
/* COMPOSITE representation: one command = the ordered op span of one undo step. Undo
   applies inverses in REVERSE op order; redo re-applies forward. A transaction appends
   all its ops into a single open command, so the whole gesture is one step. */
struct flow__cmd { flow__op *ops; int nops, opcap; };

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
  void (*on_viewport_change)(flow_t *f, flow_viewport vp, void *user);    /* fired after a pan/zoom/fit/load/extent-reclamp mutates the viewport; only on actual change (clamps applied BEFORE the compare); never journaled */
  void (*on_edge_click)(flow_t *f, int edge, void *user);                 /* left-click (no drag) on an edge's routed path (fires AFTER flow_select_edge) */
  void (*on_edge_context)(flow_t *f, int edge, flow_pt screen, void *user);/* right-click on an edge's routed path (a node hit takes precedence) */
  void (*on_edge_dblclick)(flow_t *f, int edge, void *user);              /* 2nd consecutive click on the same edge id (fires AFTER on_edge_click; pair then consumed) */
  void (*on_connect_start)(flow_t *f, int source_node, const char *handle, void *user); /* a source handle was grabbed: connection preview in flight */
  void (*on_connect_end)(flow_t *f, int edge_id, int source, int target, void *user);
    /* fired at EVERY connection-gesture exit: edge_id = the new edge (success) or -1
       (cancel / drop / reject); target = the attempted target node (-1 for cancels and
       empty/self drops). Fires AFTER on_connect and AFTER the undo txn settles.
       Reconnect drags fire NO lifecycle events in v1 (model-only mutation). */
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
  flow_rect node_extent, translate_extent;  /* optional world-space clamps (spec §6.1/§6.2); zero rect = disabled (calloc default) */
  int drag_node, dragging_pan; flow_pt drag_grab, last_mouse;  /* mouse interaction state */
  flow_pt drag_last_world;                                     /* multi-drag: last drag pos in world coords (per-motion delta) */
  int mouse_down, down_node, moved; flow_pt down_pos;          /* press/click tracking */
  int down_modsel;                                             /* press was a SHIFT/CTRL modifier-select on a node (suppress release replace) */
  int space_held;                                              /* space-pan: sticky toggle (terminal model A) — a press forces drag-to-pan over node OR pane */
  int autopan_margin, autopan_speed;                           /* auto-pan near edge during object drags (node/connect/reconnect): band width (cells) + step per motion event; defaults 3/2 in flow_new */
  int last_click_node;                                         /* dblclick: id of the previous node-body click (-1 = none/consumed); a 2nd click on the same id is a double-click */
  int last_click_edge;                                         /* edge dblclick pair state, mirroring last_click_node; broken by any OTHER click (node/pane/different edge) and on flow_load */
  int cb_suppress;                                             /* >0 suppresses nested observer fires (on_nodes_delete / on_selection_change) from recursive/aggregate mutators (remove_node cascade, delete_selection, select_in_rect's internal clear) */
  int marquee_active, marquee_on; flow_pt marquee_anchor, marquee_cur; /* marquee: armed intent / live; screen coords */
  flow_select_mode marquee_mode;                              /* default mode for shift-drag marquee */
  int conn_active, conn_node; char conn_handle[16]; flow_pt conn_end; /* in-flight connection: source node/handle + free end (screen) */
  flow_connection_validator validator_fn; void *validator_user;       /* isValidConnection gate (inc-4 #9); NULL = allow all (calloc default) */
  int reconnect_edge, reconnect_which;                        /* in-flight endpoint-reconnect drag: edge id (-1 idle) + which endpoint (0=source,1=target) */
  flow_callbacks cb;
  struct { int enabled, w, h; flow_corner corner; } minimap;
  struct { flow_bg_variant variant; int gap; } bg;
  struct { char seq[8]; flow_key_fn fn; void *user; } keys[32]; int nkeys;  /* key-binding registry */
  int statusbar;  /* built-in bottom help/status line */
  struct {
    struct flow__cmd *items; int n, cap;    /* undo stack (top = items[n-1]) */
    struct flow__cmd *redo;  int rn, rcap;  /* redo stack */
    int limit;                              /* max depth in STEPS (default 128); 0 = journaling disabled */
    int applying;                           /* re-entrancy guard: 1 while undo/redo replays */
    int suppress;                           /* >0 during flow_remove_node's internal cascade + flow_load's rebuild */
    int txn_depth;                          /* >0 inside a coalescing transaction */
    int txn_base;                           /* index of the open txn's command, or -1 (opens lazily on first record) */
  } journal;
};
static void *flow__grow(void *arr, int *cap, int need, size_t sz) {
  if (need <= *cap) return arr;
  int c = *cap ? *cap : 8; while (c < need) c *= 2;
  arr = realloc(arr, (size_t)c * sz); *cap = c; return arr;
}
/* ---- undo journal: recording primitives (PURE DATA — never call mutators, so they are
   safe to invoke from this module; the appliers live in flow_undo.h, after flow_model) ---- */
static char *flow__dup(const char *s) {            /* malloc+memcpy (codebase avoids strdup under -std=c11) */
  if (!s) return NULL;
  size_t n = strlen(s) + 1; char *d = (char*)malloc(n); if (d) memcpy(d, s, n); return d;
}
static void flow__op_free(flow__op *op) {          /* frees owned label copies; NEVER frees node->data */
  switch (op->kind) {
    case FLOW_CMD_REMOVE_NODE:
      for (int i = 0; i < op->u.subtree.ne; i++) free(op->u.subtree.edges[i].label_copy);
      free(op->u.subtree.nodes); free(op->u.subtree.edges); break;
    case FLOW_CMD_ADD_EDGE: case FLOW_CMD_REMOVE_EDGE: free(op->u.edge.snap.label_copy); break;
    case FLOW_CMD_SET_LABEL: free(op->u.label.from); free(op->u.label.to); break;
    default: break;
  }
}
static void flow__cmd_free(struct flow__cmd *c) {
  for (int i = 0; i < c->nops; i++) flow__op_free(&c->ops[i]);
  free(c->ops);
}
static void flow__redo_clear(flow_t *f) {
  for (int i = 0; i < f->journal.rn; i++) flow__cmd_free(&f->journal.redo[i]);
  f->journal.rn = 0;
}
/* full teardown: flow_free + flow_load's flow__graph_reset (serialize seam — undo must
   not invert against a replaced graph) */
static void flow__journal_clear(flow_t *f) {
  for (int i = 0; i < f->journal.n; i++) flow__cmd_free(&f->journal.items[i]);
  free(f->journal.items); f->journal.items = NULL; f->journal.n = f->journal.cap = 0;
  flow__redo_clear(f);
  free(f->journal.redo); f->journal.redo = NULL; f->journal.rcap = 0;
  f->journal.txn_depth = 0; f->journal.txn_base = -1;
}
static int flow__rec_gate(flow_t *f) {             /* every record call is gated on this */
  return !f->journal.applying && !f->journal.suppress && f->journal.limit != 0;
}
/* push one op: append to the open transaction's command, else push a new single-op
   command (evicting the oldest past the cap). Any recorded mutation clears redo. */
static void flow__rec_push(flow_t *f, flow__op op) {
  flow__redo_clear(f);
  struct flow__cmd *c;
  if (f->journal.txn_depth > 0 && f->journal.txn_base >= 0) {
    c = &f->journal.items[f->journal.txn_base];    /* txn already open: coalesce into its command */
  } else {
    while (f->journal.n >= f->journal.limit && f->journal.n > 0) {  /* evict oldest STEP(s): a lowered
                                                       limit + redo-restores can leave n past the cap */
      flow__cmd_free(&f->journal.items[0]);
      memmove(&f->journal.items[0], &f->journal.items[1],
              (size_t)(f->journal.n - 1) * sizeof *f->journal.items);
      f->journal.n--;
    }
    f->journal.items = (struct flow__cmd*)flow__grow(f->journal.items, &f->journal.cap,
                                                     f->journal.n + 1, sizeof *f->journal.items);
    c = &f->journal.items[f->journal.n++];
    memset(c, 0, sizeof *c);
    if (f->journal.txn_depth > 0) f->journal.txn_base = f->journal.n - 1;  /* first record opens the txn's command */
  }
  c->ops = (flow__op*)flow__grow(c->ops, &c->opcap, c->nops + 1, sizeof *c->ops);
  c->ops[c->nops++] = op;
}
void flow__undo_begin(flow_t *f) {
  if (f->journal.applying) return;                 /* replay never opens transactions */
  if (f->journal.txn_depth++ == 0) f->journal.txn_base = -1;  /* command opens lazily on first record */
}
void flow__undo_end(flow_t *f) {
  if (f->journal.applying) return;
  if (f->journal.txn_depth > 0 && --f->journal.txn_depth == 0) f->journal.txn_base = -1;
}
static flow__op flow__op_base(flow_t *f, flow_cmd_kind kind) {
  flow__op op; memset(&op, 0, sizeof op);
  op.kind = kind;
  op.nid0 = op.nid1 = f->nextid; op.eid0 = op.eid1 = f->nexteid;
  return op;
}
static void flow__rec_add_node(flow_t *f, int id) {  /* call AFTER the add (id minted, node appended) */
  flow__op op = flow__op_base(f, FLOW_CMD_ADD_NODE);
  op.nid0 = id;                                      /* undo rewinds the counter to the pre-add value */
  op.u.node.snap.id = id; op.u.node.snap.index = f->nnodes - 1;  /* struct snapped at undo time */
  flow__rec_push(f, op);
}
static void flow__rec_add_edge(flow_t *f, int id) {
  flow__op op = flow__op_base(f, FLOW_CMD_ADD_EDGE);
  op.eid0 = id;
  op.u.edge.snap.id = id; op.u.edge.snap.index = f->nedges - 1;
  flow__rec_push(f, op);
}
static void flow__rec_move(flow_t *f, int id, flow_pt from_abs, flow_pt to_abs) {
  if (f->journal.txn_depth > 0 && f->journal.txn_base >= 0) {  /* coalesce within the open txn */
    struct flow__cmd *c = &f->journal.items[f->journal.txn_base];
    for (int i = 0; i < c->nops; i++)
      if (c->ops[i].kind == FLOW_CMD_MOVE_NODE && c->ops[i].u.move.id == id) {
        c->ops[i].u.move.to = to_abs;                /* keep the first `from`, overwrite `to` */
        return;
      }
  }
  flow__op op = flow__op_base(f, FLOW_CMD_MOVE_NODE);
  op.u.move.id = id; op.u.move.from = from_abs; op.u.move.to = to_abs;
  flow__rec_push(f, op);
}
static void flow__rec_remove_edge(flow_t *f, const flow_edge *e, int index) {
  flow__op op = flow__op_base(f, FLOW_CMD_REMOVE_EDGE);
  op.u.edge.snap.id = e->id; op.u.edge.snap.index = index;
  op.u.edge.snap.edge = *e;
  op.u.edge.snap.label_copy = flow__dup(e->label);
  op.u.edge.snap.edge.label = op.u.edge.snap.label_copy;
  flow__rec_push(f, op);
}
/* subtree snapshot BEFORE flow_remove_node's cascade: every node in the parent-chain
   subtree of `id` plus every edge incident to any of them, in ascending array order
   (so undo's positional re-inserts rebuild the original insertion order). */
static void flow__rec_remove_node(flow_t *f, int id) {
  flow__op op = flow__op_base(f, FLOW_CMD_REMOVE_NODE);
  op.u.subtree.root = id;
  int nn = 0, ne = 0;
  for (int i = 0; i < f->nnodes; i++) if (flow_is_ancestor(f, id, f->nodes[i].id)) nn++;
  for (int i = 0; i < f->nedges; i++)
    if (flow_is_ancestor(f, id, f->edges[i].source) || flow_is_ancestor(f, id, f->edges[i].target)) ne++;
  op.u.subtree.nodes = nn ? (flow__node_snap*)malloc((size_t)nn * sizeof(flow__node_snap)) : NULL;
  op.u.subtree.edges = ne ? (flow__edge_snap*)malloc((size_t)ne * sizeof(flow__edge_snap)) : NULL;
  for (int i = 0; i < f->nnodes; i++) {
    if (!flow_is_ancestor(f, id, f->nodes[i].id)) continue;
    flow__node_snap *s = &op.u.subtree.nodes[op.u.subtree.nn++];
    s->id = f->nodes[i].id; s->index = i; s->node = f->nodes[i];   /* data ptr BORROWED */
  }
  for (int i = 0; i < f->nedges; i++) {
    if (!flow_is_ancestor(f, id, f->edges[i].source) && !flow_is_ancestor(f, id, f->edges[i].target)) continue;
    flow__edge_snap *s = &op.u.subtree.edges[op.u.subtree.ne++];
    s->id = f->edges[i].id; s->index = i; s->edge = f->edges[i];
    s->label_copy = flow__dup(f->edges[i].label);                  /* label OWNED: dup it */
    s->edge.label = s->label_copy;
  }
  flow__rec_push(f, op);
}
static void flow__rec_reconnect(flow_t *f, int edge, int which, int from_node, const char *from_handle,
                                int to_node, const char *to_handle) {
  flow__op op = flow__op_base(f, FLOW_CMD_RECONNECT_EDGE);
  op.u.reconnect.id = edge; op.u.reconnect.which = which;
  op.u.reconnect.from_node = from_node; op.u.reconnect.to_node = to_node;
  snprintf(op.u.reconnect.from_handle, sizeof op.u.reconnect.from_handle, "%s", from_handle ? from_handle : "");
  snprintf(op.u.reconnect.to_handle, sizeof op.u.reconnect.to_handle, "%s", to_handle ? to_handle : "");
  flow__rec_push(f, op);
}
static void flow__rec_set_label(flow_t *f, int edge, const char *from, const char *to) {
  flow__op op = flow__op_base(f, FLOW_CMD_SET_LABEL);
  op.u.label.id = edge;
  op.u.label.from = flow__dup(from);                 /* dup'd BEFORE the mutator frees the old label */
  op.u.label.to = flow__dup(to);
  flow__rec_push(f, op);
}
static void flow__rec_reparent(flow_t *f, int child, int from_parent, flow_pt from_pos,
                               int to_parent, flow_pt to_pos) {
  flow__op op = flow__op_base(f, FLOW_CMD_REPARENT);
  op.u.reparent.child = child;
  op.u.reparent.from_parent = from_parent; op.u.reparent.from_pos = from_pos;
  op.u.reparent.to_parent = to_parent; op.u.reparent.to_pos = to_pos;
  flow__rec_push(f, op);
}
flow_t *flow_new(int cols, int rows) {
  flow_t *f = (flow_t*)calloc(1, sizeof *f);
  f->view.zoom = 1; f->zmin = FLOW_ZOOM_MIN; f->zmax = FLOW_ZOOM_MAX;
  f->cols = cols; f->rows = rows; f->nextid = 1; f->nexteid = 1;
  f->drag_node = -1; f->marquee_mode = FLOW_SELECT_PARTIAL; f->conn_node = -1;
  f->reconnect_edge = -1; f->last_click_node = -1; f->last_click_edge = -1;
  f->autopan_margin = 3; f->autopan_speed = 2;
  f->journal.limit = 128; f->journal.txn_base = -1;
  f->front = (flow_cell*)calloc((size_t)cols * rows, sizeof(flow_cell));
  return f;
}
void flow_free(flow_t *f) {
  if (!f) return;
  flow__journal_clear(f);   /* frees command label copies + stacks; drops (never frees) node->data */
  for (int i = 0; i < f->nedges; i++) free(f->edges[i].label);
  free(f->nodes); free(f->edges); free(f->ntypes); free(f->etypes); free(f->front); free(f);
}
/* Tear the graph back to empty for flow_load: free edge labels + node/edge arrays,
   NULL the pointers, zero counts/caps, reset id counters. Leaves view/types/cb/widgets
   intact. NEVER frees node->data (app owns it; same contract as flow_free).
   Also clears the undo journal (serialize×undo seam): undo must not invert against a
   replaced graph; flow_load additionally suppresses recording across its rebuild. */
static void flow__graph_reset(flow_t *f) {
  flow__journal_clear(f);
  for (int i = 0; i < f->nedges; i++) free(f->edges[i].label);
  free(f->nodes); free(f->edges);
  f->nodes = NULL; f->edges = NULL;
  f->nnodes = f->capnodes = 0; f->nedges = f->capedges = 0;
  f->nextid = 1; f->nexteid = 1;
  f->last_click_node = -1;   /* drop the stale dblclick target: reused ids must not fake a double-click */
  f->last_click_edge = -1;   /* same for the edge dblclick pair */
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
  int nid = n->id;
  if (flow__rec_gate(f)) flow__rec_add_node(f, nid);
  return nid;
}
/* ---- extent clamps (xyflow nodeExtent / translateExtent, spec §6.1/§6.2) ---- */
void flow_set_node_extent(flow_t *f, flow_rect world) { f->node_extent = world; }
/* Clamp f->view.ox/oy IN PLACE so the visible world window stays inside translate_extent.
   From flow_project (screen = world·z + o): the window spans world [−o/z, (screen−o)/z],
   so the o clamp range is [screen_extent − (e.x+e.w)·z, −e.x·z] — LARGER world x needs
   SMALLER ox (flow_feed's right-arrow pans (-1,0) for the same reason). When the window
   is BIGGER than the extent on an axis the range inverts (lo > hi): pin to the midpoint,
   which centers the extent in the window (d3-zoom translateExtent convention) — pans
   become no-ops on that axis until zoom-in shrinks the window. Per-axis independent. */
static void flow__clamp_view_offset(flow_t *f) {
  flow_rect e = f->translate_extent;
  if (e.w <= 0 || e.h <= 0) return;                     /* disabled (zero-init default) */
  float z = f->view.zoom == 0.0f ? 1.0f : f->view.zoom;
  float lo = (float)f->cols - (float)(e.x + e.w) * z, hi = -(float)e.x * z;
  f->view.ox = lo > hi ? (lo + hi) / 2.0f : (f->view.ox < lo ? lo : (f->view.ox > hi ? hi : f->view.ox));
  lo = (float)f->rows - (float)(e.y + e.h) * z; hi = -(float)e.y * z;
  f->view.oy = lo > hi ? (lo + hi) / 2.0f : (f->view.oy < lo ? lo : (f->view.oy > hi ? hi : f->view.oy));
}
/* THE viewport seam: every mutation (pan, zoom, fit, load-restore, extent re-clamp)
   routes through here. Clamps FIRST (translate extent), then fires on_viewport_change
   iff the final viewport differs from the prior one — a write clamped back to the same
   value fires nothing. Deliberately NOT journaled (spec §11) and NOT depth-guarded:
   each re-entrant mutation compares against the by-then-current state, so a callback
   whose mutation CONVERGES (re-applies the same value, or clamps back) terminates,
   firing once per actual change. A callback that pans/zooms by a nonzero delta on
   EVERY fire recurses unboundedly — apps must not; a cb_suppress-style depth guard
   is the spec's deferred escape hatch if one ever proves needed. */
static void flow__view_set(flow_t *f, float ox, float oy, float zoom) {
  flow_viewport old = f->view;
  f->view.ox = ox; f->view.oy = oy; f->view.zoom = zoom;
  flow__clamp_view_offset(f);
  if (memcmp(&old, &f->view, sizeof old) != 0 && f->cb.on_viewport_change)
    f->cb.on_viewport_change(f, f->view, f->cb.user);
}
void flow_set_translate_extent(flow_t *f, flow_rect world) {
  f->translate_extent = world;
  /* re-clamp the live view immediately (precedent: flow_set_zoom_limits re-clamps
     zoom); fires on_viewport_change iff the clamp actually moves the view */
  flow__view_set(f, f->view.ox, f->view.oy, f->view.zoom);
}
void flow_move_node(flow_t *f, int id, flow_pt pos) {
  /* ABSOLUTE-in contract: `pos` is a world-absolute target. Store it parent-relative so
     a child stays under the cursor. For top-level nodes (parent==-1) parent_abs=={0,0},
     so n->pos == pos — byte-identical to the pre-groups behaviour for every existing caller. */
  flow_node *n = flow_get_node(f, id);
  if (!n) return;
  /* node-extent clamp (absolute world space) BEFORE journaling, so undo/redo replay the
     CLAMPED target (flow__rec_move records `pos`). Max edge first, then min edge: a node
     LARGER than the extent lands flush to the min edge (deterministic, no oscillation). */
  if (f->node_extent.w > 0 && f->node_extent.h > 0) {
    flow_rect e = f->node_extent;
    if (pos.x + n->w > e.x + e.w) pos.x = e.x + e.w - n->w;
    if (pos.x < e.x) pos.x = e.x;
    if (pos.y + n->h > e.y + e.h) pos.y = e.y + e.h - n->h;
    if (pos.y < e.y) pos.y = e.y;
  }
  /* parent-extent clamp (FLOW_EXTENT_PARENT, spec §9 extent:'parent'): keep the child
     rect inside the parent's absolute rect. Applied AFTER the node-extent clamp (pinned
     order — with disjoint ranges the parent wins) and, like it, BEFORE journaling.
     Layouts commit through this function, so they inherit the clamp transparently. */
  if ((n->flags & FLOW_EXTENT_PARENT) && n->parent != -1) {
    flow_node *pp = flow_get_node(f, n->parent);
    if (pp) {
      flow_rect pr = flow_node_rect_abs(f, pp);
      if (pos.x + n->w > pr.x + pr.w) pos.x = pr.x + pr.w - n->w;
      if (pos.x < pr.x) pos.x = pr.x;
      if (pos.y + n->h > pr.y + pr.h) pos.y = pr.y + pr.h - n->h;
      if (pos.y < pr.y) pos.y = pr.y;
    }
  }
  if (flow__rec_gate(f)) flow__rec_move(f, id, flow_node_abs(f, n), pos);  /* MOVE journals ABSOLUTE coords */
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
  if (flow__rec_gate(f) && c->parent != parent)              /* same-parent call mutates nothing: skip */
    flow__rec_reparent(f, child, c->parent, c->pos, parent,
                       (flow_pt){ cabs.x - pabs.x, cabs.y - pabs.y });
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
  flow__undo_begin(f);                                /* add + N reparents = ONE undo step */
  const int pad = 1;
  flow_pt gpos = { bb.x - pad, bb.y - pad };
  int gw = bb.w + 2 * pad, gh = bb.h + 2 * pad;
  int gid = flow_add_node(f, "group", gpos, NULL);   /* measure is a no-op write-back; w/h set next */
  flow_node *g = flow_get_node(f, gid);              /* re-fetch: array may have realloc'd */
  if (g) { g->w = gw; g->h = gh; }                   /* direct write: captured by the ADD snap at undo time */
  for (int i = 0; i < n; i++) {                       /* reparent members (abs preserved) */
    if (flow_get_node(f, ids[i])) flow_set_parent(f, ids[i], gid);
  }
  flow__undo_end(f);
  return gid;
}
void flow_ungroup(flow_t *f, int id) {
  flow_node *g = flow_get_node(f, id);
  if (!g || strcmp(g->type, "group") != 0) return;   /* no-op if missing or not a group */
  flow__undo_begin(f);                                /* N reparents-out + container remove = ONE undo step */
  int gparent = g->parent;
  for (;;) {                                          /* reparent each direct child OUT (abs preserved) */
    int childid = -1;
    for (int i = 0; i < f->nnodes; i++) if (f->nodes[i].parent == id) { childid = f->nodes[i].id; break; }
    if (childid == -1) break;
    flow_set_parent(f, childid, gparent);             /* re-find by id each step (array stable here, but safe) */
  }
  flow_remove_node(f, id);                             /* now childless: its subtree snap is the container alone */
  flow__undo_end(f);
}
int flow_add_edge(flow_t *f, int src, int dst, const char *sh, const char *th) {
  if (src == dst) return -1;
  if (!flow_get_node(f, src) || !flow_get_node(f, dst)) return -1;
  const char *shs = sh ? sh : "", *ths = th ? th : "";
  for (int i = 0; i < f->nedges; i++)                          /* dup = same (source,target,handles) */
    if (f->edges[i].source == src && f->edges[i].target == dst &&
        strcmp(f->edges[i].source_handle, shs) == 0 && strcmp(f->edges[i].target_handle, ths) == 0) return -1;
  /* engine validator gate (inc-4 #9): AFTER the fast structural rejects, BEFORE the
     append/record. Silent reject: -1, nothing journaled. */
  if (f->validator_fn && !f->validator_fn(f, src, dst, shs, ths, f->validator_user)) return -1;
  f->edges = (flow_edge*)flow__grow(f->edges, &f->capedges, f->nedges + 1, sizeof(flow_edge));
  flow_edge *e = &f->edges[f->nedges++]; memset(e, 0, sizeof *e);
  e->id = f->nexteid++; e->source = src; e->target = dst;
  snprintf(e->source_handle, sizeof e->source_handle, "%s", sh ? sh : "");
  snprintf(e->target_handle, sizeof e->target_handle, "%s", th ? th : "");
  int eid = e->id;
  if (flow__rec_gate(f)) flow__rec_add_edge(f, eid);  /* records on success only (rejects returned above) */
  return eid;
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
/* THE hidden choke points (inc-4 #11). All view-layer skip logic gates through these
   two so render, hit-tests, marquee, bounds and the minimap can never drift apart
   (the flow__node_footprint precedent). Edges cascade: an edge is invisible when ITS
   flag is set OR either endpoint node is hidden. */
static int flow__node_visible(flow_t *f, const flow_node *n) { (void)f; return !(n->flags & FLOW_HIDDEN); }
static int flow__edge_visible(flow_t *f, const flow_edge *e) {
  if (e->flags & FLOW_HIDDEN) return 0;
  flow_node *sn = flow_get_node(f, e->source), *tn = flow_get_node(f, e->target);
  if (sn && !flow__node_visible(f, sn)) return 0;
  if (tn && !flow__node_visible(f, tn)) return 0;
  return 1;
}
flow_rect flow_bounds(flow_t *f) {
  /* union of VISIBLE node rects (hidden are view-skipped); zero rect when the graph
     is empty OR fully hidden — flow_fit_view's existing w<=0 guard makes that a no-op */
  flow_rect b = {0,0,0,0}; int seeded = 0;
  for (int i = 0; i < f->nnodes; i++) {
    if (!flow__node_visible(f, &f->nodes[i])) continue;
    flow_rect r = flow_node_rect_abs(f, &f->nodes[i]);
    b = seeded ? flow_rect_union(b, r) : r; seeded = 1;
  }
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
    if (!flow__node_visible(f, n)) continue;          /* hidden: not hittable (same gate as render) */
    flow_rect sr = flow__node_footprint(f, n, lod);   /* exact footprint the renderer draws */
    if (flow_rect_contains(sr, screen)) { hit = n->id; break; }
  }
  free(order);
  return hit;
}
void flow_pan(flow_t *f, int dx, int dy) { flow__view_set(f, f->view.ox + dx, f->view.oy + dy, f->view.zoom); }
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
    if (!flow__node_visible(f, n)) continue;            /* marquee is VIEW-level: hidden nodes are not selectable */
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
  /* engine validator gate (inc-4 #9): sees the PROSPECTIVE endpoints, after the
     structural rejects, before the record. Silent reject: edge left unchanged. */
  if (f->validator_fn && !f->validator_fn(f, nsrc, ntgt, nsh, nth, f->validator_user)) return;
  if (flow__rec_gate(f))                /* validated: record old endpoint+handle BEFORE the commit */
    flow__rec_reconnect(f, edge, which,
                        which == 0 ? e->source : e->target,
                        which == 0 ? e->source_handle : e->target_handle,
                        endpoint_node, hs);
  e->source = nsrc; e->target = ntgt;   /* scalar self-assign on the unchanged side is harmless */
  /* write ONLY the changed endpoint's handle, from the caller arg `hs` (never aliases
     e's buffers). Rewriting the unchanged side from e->*_handle would be a self-overlapping
     snprintf (UB per C11 7.21.6.5), so leave it untouched. */
  if (which == 0) snprintf(e->source_handle, sizeof e->source_handle, "%s", hs);
  else            snprintf(e->target_handle, sizeof e->target_handle, "%s", hs);
}
void flow_set_edge_label(flow_t *f, int edge, const char *label) {
  flow_edge *e = flow_get_edge(f, edge); if (!e) return;
  if (flow__rec_gate(f)) flow__rec_set_label(f, edge, e->label, label);  /* dup the OLD label BEFORE freeing it */
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
    if (flow__rec_gate(f)) flow__rec_remove_edge(f, &f->edges[i], i);  /* snapshot (dup'd label) before freeing */
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
  /* journal the WHOLE subtree as ONE command BEFORE callback + cascade (state at API-call
     time); the recursive child removals below run with journal.suppress set so the
     inlined cascade never records piecemeal. */
  if (flow__rec_gate(f) && flow_get_node(f, id)) flow__rec_remove_node(f, id);
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
  f->cb_suppress++; f->journal.suppress++;
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
  f->cb_suppress--; f->journal.suppress--;
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
  flow__undo_begin(f);  /* every removed root's subtree command coalesces into ONE undo step */
  f->cb_suppress++;
  for (;;) { int id = flow_selected_node(f); if (id < 0) break; flow_remove_node(f, id); }
  for (;;) { int e  = flow_selected_edge(f); if (e  < 0) break; flow_remove_edge(f, e);  }
  f->cb_suppress--;
  flow__undo_end(f);
  flow__notify_selection(f, sig);
}
int flow_add_node_center(flow_t *f, const char *type, void *data) {
  flow__undo_begin(f);                                        /* add + centering move = ONE undo step */
  flow_pt c = flow_to_world(f, (flow_pt){ f->cols / 2, f->rows / 2 });
  int id = flow_add_node(f, type, c, data);
  flow_node *n = flow_get_node(f, id);                        /* re-fetch for measured w,h */
  if (n) flow_move_node(f, id, (flow_pt){ c.x - n->w / 2, c.y - n->h / 2 });
  flow__undo_end(f);
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
  flow__view_set(f, f->cols / 2.0f - (b.x + b.w / 2.0f) * z,  /* seam clamps (extent wins over centering) + fires */
                    f->rows / 2.0f - (b.y + b.h / 2.0f) * z, z);
}
void flow_set_connection_validator(flow_t *f, flow_connection_validator fn, void *user) {
  f->validator_fn = fn; f->validator_user = user;   /* NULL fn = allow all (default) */
}
void flow_set_node_hidden(flow_t *f, int id, int hidden) {
  flow_node *n = flow_get_node(f, id);
  if (!n) return;
  if (hidden) {
    unsigned long sig = flow__sel_sig(f);
    n->flags |= FLOW_HIDDEN;
    if (n->flags & FLOW_SELECTED) {                 /* hide deselects: no invisible selection
                                                       (Delete would silently remove it) */
      n->flags &= ~(unsigned)FLOW_SELECTED;
      flow__notify_selection(f, sig);               /* sig-gated: fires once, only on change */
    }
  } else {
    n->flags &= ~(unsigned)FLOW_HIDDEN;             /* showing does NOT reselect (hide discards state) */
  }
}
void flow_set_edge_hidden(flow_t *f, int id, int hidden) {
  flow_edge *e = flow_get_edge(f, id);
  if (!e) return;
  if (hidden) {
    e->flags |= FLOW_HIDDEN;
    e->flags &= ~(unsigned)FLOW_SELECTED;           /* same no-invisible-selection rule as nodes;
                                                       no event — flow__sel_sig hashes nodes only */
  } else {
    e->flags &= ~(unsigned)FLOW_HIDDEN;
  }
}
void flow_set_statusbar(flow_t *f, int enabled) { f->statusbar = enabled ? 1 : 0; }
void flow_set_autopan(flow_t *f, int margin, int speed) {
  f->autopan_margin = margin < 0 ? 0 : margin;   /* 0 = no band = disabled */
  f->autopan_speed  = speed  < 0 ? 0 : speed;
}
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
  if (seq[0] == 'u') { flow_undo(f); return 1; }                    /* declared above, defined in flow_undo.h (same TU) */
  if (seq[0] == '\x12') { flow_redo(f); return 1; }                 /* Ctrl-r: single byte, longest-match safe */
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
  if (!flow__node_visible(f, n)) return 0;   /* hidden nodes never show handle markers */
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
  if (f->cb.on_connect_start) f->cb.on_connect_start(f, node, f->conn_handle, f->cb.user);
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
  flow__undo_begin(f);                            /* connect (incl. any on_connect follow-ups) = ONE undo step */
  int eid = flow_add_edge(f, src, node, sh, th);
  if (eid != -1 && f->cb.on_connect) f->cb.on_connect(f, src, node, f->cb.user);
  flow__undo_end(f);
  /* lifecycle end: AFTER the txn settles so the journal is consistent when the app
     reacts. target = the attempted node even on reject (spec 3c); success passes eid. */
  if (f->cb.on_connect_end) f->cb.on_connect_end(f, eid, src, node, f->cb.user);
  return eid;
}
void flow_cancel_connection(flow_t *f) {
  if (!f->conn_active) return;                    /* idempotent: no gesture -> no event (ESC calls this unconditionally) */
  int src = f->conn_node;
  f->conn_active = 0; f->conn_node = -1; f->conn_handle[0] = 0;
  if (f->cb.on_connect_end) f->cb.on_connect_end(f, -1, src, -1, f->cb.user);
}
int flow_connecting(flow_t *f) { return f->conn_active ? 1 : 0; }
#endif
