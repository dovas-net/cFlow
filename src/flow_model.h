/* ===== model: engine, nodes, edges, vtable types, transform, bounds, hit-test ===== */
enum { FLOW_SELECTED = 1u, FLOW_DRAGGING = 2u, FLOW_HOVERED = 4u, FLOW_HIDDEN = 8u, FLOW_EXTENT_PARENT = 16u, FLOW_ANIMATED = 32u,
       FLOW_NODRAG = 64u, FLOW_NOSELECT = 128u, FLOW_NODELETE = 256u, FLOW_EXPLICIT_SIZE = 512u };
/* inc-8 #1 per-element interaction gates (xyflow draggable/selectable/deletable=false). NEGATIVE
   polarity: calloc-zero == permissive default, so flow_add_node needs no seed. DURABLE, unlike the
   transient bits above — persisted (named bools in flow_json, emit-when-set) and undo-durable (they
   sit OUTSIDE flow_undo's SELECTED|DRAGGING|HOVERED clear mask, so a restored node keeps its gate). */
/* inc-8 #2 FLOW_EXPLICIT_SIZE (512u): a user-set w/h (flow_set_node_size) that SKIPS auto-measure
   (guards the top of flow_measure_node, covering add/paste/load at once) and PERSISTS as ,"w":N,"h":N
   emitted only when set. Like the gates it is DURABLE: outside the transient-clear mask (survives
   undo) and on the on-disk rail. The resize itself IS journaled (FLOW_CMD_RESIZE_NODE) so a resize
   gesture is one undo step — unlike the config-toggle gate setters which record nothing. */
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
  const char *(*label)(const flow_node *n);           /* optional (inc-5 #10): the node's searchable label, a NUL-terminated
                                                         string owned by the node (lifetime = the node), or NULL. APPENDED LAST:
                                                         zero-init keeps every existing initializer valid — NULL = unsearchable. */
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

/* keyboard focus (inc-5 #5) — the keyboard analog of hover: one focused node at a
   time, stored as an id (sentinel -1), never a flag. Tab/Shift-Tab built-ins cycle
   VISIBLE nodes in insertion order (wrapping; hidden skipped — focus is VIEW-level
   like render/hit/marquee); Enter selects the focused node (replace); lone-ESC
   clears focus. Focus is invalidated to -1 when the focused node is deleted or
   hidden. Focusing a FULLY-offscreen node re-centres the viewport on it via
   flow_set_center (autoPanOnNodeFocus); partially/fully visible nodes never jump.
   Transient view/interaction state: not journaled, not persisted. */
int  flow_focused_node(flow_t *f);          /* current focused node id, or -1 */
void flow_set_focus(flow_t *f, int id);     /* focus a specific node (id<0 or hidden/absent => clears to -1); frames if offscreen */
void flow_focus_next(flow_t *f);            /* Tab: next VISIBLE node in insertion order, wrapping; frames if offscreen */
void flow_focus_prev(flow_t *f);            /* Shift-Tab: previous VISIBLE node, wrapping; frames if offscreen */
void flow_set_center(flow_t *f, int wx, int wy, float zoom); /* declared here for the focus framing call; defined in flow_view.h (same TU — the flow_undo precedent) */
int flow_begin_connection(flow_t *f, int node, const char *handle); /* enter connecting from source handle; 0 ok, -1 if not a valid source */
int flow_update_connection(flow_t *f, flow_pt screen);           /* move free end to screen cell; returns hovered candidate target node id or -1 */
int flow_connection_valid(flow_t *f);                            /* inc-7 #2: 1 if the current in-flight candidate drop WOULD accept (resolved handle under cursor), else 0 (no connection, self, duplicate, or validator veto). Recomputed each flow_update_connection. */
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

/* In-process selection clipboard (inc-5 #7). Copy/cut deep-snapshot the selected
   nodes plus the intra-selection edges (BOTH endpoints selected; edge SELECTION
   flags are irrelevant); node->data is ALIASED (borrowed — the undo-snapshot
   contract), edge labels are dup'd. Paste re-mints through flow_add_node/
   flow_add_edge with FRESH ids at a cumulative offset (+gen+1 per paste, gen reset
   on copy/cut) as ONE undo step, reparents pasted children whose original parent
   was also pasted (others land as roots at absolute coords), restores edge type +
   label, and selects the result with ONE sig-gated on_selection_change. Pasted
   nodes are re-MEASURED (manual w/h resizes do not survive — the flow_load
   convention); pasted edges stay validator-GATED (deliberate asymmetry vs
   flow_load's suspension: a paste violating a LIVE validator drops those edges —
   partial paste). The clipboard survives graph mutations and flow_load; freed
   only in flow_free. Keys: y copy, c cut, p paste, d duplicate. */
void flow_copy_selection(flow_t *f);       /* snapshot selection into the clipboard (no graph change, no callbacks) */
void flow_cut_selection(flow_t *f);        /* = copy_selection then flow_delete_selection */
int  flow_paste(flow_t *f);                /* mint clipboard contents at a growing offset; returns nodes pasted (0 if clipboard empty) */
int  flow_duplicate_selection(flow_t *f);  /* snapshot+paste in one shot at +1,+1; clipboard left UNTOUCHED; returns nodes added */
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

/* pre-dispatch key hook (inc-5 #10): a GATE on struct flow (this validator's
   precedent — not a flow_callbacks observer). Called at the very top of
   flow_dispatch_key, BEFORE flow_bind_key bindings and built-ins, with the raw
   byte window. Returns BYTES CONSUMED: 0 = pass-through (dispatch continues);
   a positive count is returned verbatim and flow_feed advances i by it — so a
   modal can swallow a multibyte CSI by returning its length. Sees every
   key/escape sequence but NOT mouse (flow_feed parses mouse CSI before
   dispatch). NULL = no hook (calloc default), zero overhead. TRANSIENT. */
typedef int (*flow_key_hook)(flow_t *f, const char *seq, int len, void *user);
void flow_set_key_hook(flow_t *f, flow_key_hook fn, void *user);
/* modal key capture (inc-6 #6): when ON, a key sequence the installed hook does NOT
   consume is DROPPED inside flow_dispatch_key instead of falling through to bindings /
   built-ins / flow_feed's CSI switch. Engine state, not a hook parameter — the
   flow_key_hook typedef and the 3-arg flow_set_key_hook are UNCHANGED. INERT unless a
   hook is installed (modal + NULL hook drops nothing). Default OFF (calloc 0): modal-off
   input is byte-identical to today. Mouse is exempt (parsed before dispatch). TRANSIENT. */
void flow_set_key_hook_modal(flow_t *f, int on);
void flow_set_edge_label(flow_t *f, int edge, const char *label); /* strdup into edge->label, freeing prior; NULL clears */

/* edge hit-test & endpoints — DEFINED in flow_render.h (need the render anchor helpers).
   flow_hit_edge: topmost edge whose routed path passes within Chebyshev tol of screen, else -1.
   flow_edge_endpoint_screen: screen cell of source(0)/target(1) endpoint, matching flow_render EXACTLY; 1 on success. */
int  flow_hit_edge(flow_t *f, flow_pt screen, int tol);
int  flow_edge_endpoint_screen(flow_t *f, const flow_edge *e, int which, flow_pt *out);

/* keyboard command dispatch: bindings (registry) + built-ins, driven from flow_feed */
void flow_bind_key(flow_t *f, const char *seq, flow_key_fn fn, void *user); /* register/override; matched before built-ins, longest seq first. Up to 32 bindings, seq <=7 bytes; over-limit binds are silently ignored. */
int  flow_dispatch_key(flow_t *f, const char *seq, int n); /* run a binding/built-in for one key seq; returns bytes consumed (>0) or 0 if unhandled (inc-6 #6: while key_hook_modal is set with a hook installed, an unconsumed seq returns its dropped length >=1 instead of 0) */
void flow_delete_selection(flow_t *f);     /* built-in: remove selected node(s) then selected edge */
int  flow_add_node_center(flow_t *f, const char *type, void *data); /* add at world point under viewport center; returns id */
void flow_fit_view(flow_t *f, int margin); /* getViewportForBounds: pick zoom+pan so flow_bounds fits with `margin` cells of padding (zoom clamped to [zmin,zmax]); no-op when empty */
flow_rect flow_bounds_of(flow_t *f, const int *ids, int n); /* union of the ABSOLUTE rects of the n nodes named in ids[] — INCLUDES hidden nodes (explicit-id MODEL-level query, unlike flow_bounds); missing ids skip; zero rect on n<=0/NULL/none-resolve */
void flow_fit_bounds(flow_t *f, flow_rect r, int margin);   /* frame world rect r with `margin` cells of padding (zoom clamped to [zmin,zmax]); shares flow_fit_view's math; no-op when r.w<=0||r.h<=0 */
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
void flow_set_edge_animated(flow_t *f, int id, int on); /* inc-6 #5: marching-ants opt-in — on!=0 sets FLOW_ANIMATED on the edge, on==0 clears; no-op on unknown id. Arms #4's redraw clock via the recomputed flow__frames_armed predicate. NOT journaled, NOT persisted (flags are ephemeral; re-arm after flow_load). */
/* inc-8 #1 — per-element interaction gates (xyflow node config). POSITIVE verb over a NEGATIVE flag:
   on==0 SETS the gate, on!=0 clears it; no-op on unknown id. Gate USER INTERACTION only —
   programmatic flow_move_node / flow_select_node / flow_remove_node stay unconditional. NOT journaled
   (config toggle, like flow_set_edge_animated); the BIT is undo-durable AND persisted. */
void flow_set_node_draggable(flow_t *f, int id, int on);   /* on==0 sets FLOW_NODRAG  — held fixed under user drag + shift-arrow nudge */
void flow_set_node_selectable(flow_t *f, int id, int on);  /* on==0 sets FLOW_NOSELECT — not selectable by pointer/keyboard (public select API stays open) */
void flow_set_node_deletable(flow_t *f, int id, int on);   /* on==0 sets FLOW_NODELETE — survives delete-selection as a root (direct remove + cascade unaffected) */
/* inc-8 #2 — set an explicit node size that survives auto-measure (sets FLOW_EXPLICIT_SIZE) and
   persists across save/load. w,h clamp to >= 1; no-op on unknown id. JOURNALED (FLOW_CMD_RESIZE_NODE,
   coalescing within an open txn like flow_move_node) so package 3's resize gesture, bracketed in
   flow__undo_begin/end, is ONE undo step — this is the rail the node-resizer writes through. */
void flow_set_node_size(flow_t *f, int id, int w, int h);
void flow_set_autopan(flow_t *f, int margin, int speed); /* tune the drag auto-pan band (defaults 3/2): margin = band width in cells, speed = step per motion event; negatives clamp to 0, margin 0 disables */

/* inc-6 #4 redraw-clock — deterministic animation clock (declarations; impls live in
   src/flow_run.h). The tick is the ONLY animation clock; wall-clock NEVER enters model/render. */
void     flow_tick(flow_t *f);                /* advance clock: ++f->tick. No IO, no render, no time(). The testable seam. */
unsigned flow_ticks(flow_t *f);               /* read current tick (consumers derive dash phase = tick % period). */
void     flow_set_tick_ms(flow_t *f, int ms); /* redraw interval (ms) when frames are armed; default 100; <=0 clamps to 1 (never 0 → never busy-spin). */
int      flow__frames_armed(flow_t *f);       /* present-decision predicate; v1 returns 0 (nothing armed). #5 ORs "any FLOW_ANIMATED edge"; #8 ORs "object drag in flight". */

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
/* inc-6 #7 devtools-hud — minimal read-only journal introspection (feeds a ChangeLogger
   HUD pane). Value-returning, never a pointer into the journal, so flow__cmd/flow__op stay
   opaque. flow_can_undo(f) == (flow_undo_depth(f) > 0) by construction; these are a strict
   superset, no deprecation. */
int  flow_undo_depth(flow_t *f);  /* count of UNDO steps available (== journal depth); 0 when empty/disabled */
int  flow_redo_depth(flow_t *f);  /* count of REDO steps available */
int  flow_top_op(flow_t *f);      /* flow_cmd_kind of the LAST op of the top undo command (the most recent recorded mutation); -1 if the undo stack is empty */
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
  FLOW_CMD_REPARENT,                         /* groups: invert flow_set_parent/group/ungroup */
  FLOW_CMD_RESIZE_NODE                       /* inc-8 #2: invert flow_set_node_size (w/h pair) */
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
    struct { int id, fw, fh, tw, th, from_explicit; } resize;         /* RESIZE_NODE (from/to w,h + prior FLOW_EXPLICIT_SIZE bit, so undo restores a previously-auto node's absence of the flag) */
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

/* engine-chrome color mode (inc-7 #1). flow_set_color_mode re-seeds f->theme from a
   fixed preset table: FLOW_COLOR_DEFAULT == flow_new's seed == the legacy literals
   (fg 7 / bg 0 / grid 8), so a round-trip to DEFAULT is byte-identical; LIGHT flips
   the background to a light index, DARK keeps a dark bg with a brighter fg. The
   flow_theme token struct + flow_color_mode enum live in flow_cell.h (module 3, ahead
   of every consumer). Transient view-state: never saved, never journaled. */
void            flow_set_color_mode(flow_t *f, flow_color_mode mode);
flow_color_mode flow_color_mode_get(flow_t *f);   /* the last mode set (calloc-zero = FLOW_COLOR_DEFAULT) */

/* Controls panel (inc-7 #3) — the first interactive built-in widget: a corner-anchored
   [+][-][fit][lock] row (reuses flow_corner). Off by default (calloc-zero), like the
   minimap. Transient chrome: never saved, never journaled. The widget hit-test seam it
   introduces (top of the left-press classifier) is reused by the node/edge toolbars. */
void flow_set_controls(flow_t *f, int enabled, flow_corner corner);
/* Lock mode (inc-7 #3, the [lock] button): a whole-canvas bool. When set, the engine
   suppresses node-drag/connect/reconnect/marquee/click-select arming; pan and zoom keep
   working (xyflow Controls-lock). Toggle via the widget or directly. TRANSIENT. */
void flow_set_locked(flow_t *f, int on);
int  flow_locked(flow_t *f);

/* Toolbar action (inc-7 #4): one entry in a node/edge toolbar's borrowed action array.
   SHARED by node-toolbar (#4) and edge-toolbar (#5) — the generic `id` is a node OR an
   edge id. The engine stores the array as a borrowed pointer (NO copy, NO heap) and never
   copies the label strings; the app must keep the array alive while it is installed. */
typedef struct {
  const char *label;                          /* borrowed cell text drawn for the button */
  void (*fn)(flow_t *f, int id, void *user);  /* invoked with the selected node/edge id on a cell press */
  void *user;                                 /* opaque, passed through to fn */
} flow_toolbar_action;

/* Node toolbar (inc-7 #4): a selection-anchored action strip drawn one row above the
   single selected node (flow_selected_count(f)==1), riding the controls-bar widget seam.
   BORROWED array; NULL/0 disarms. Transient chrome: never saved, never journaled. */
void flow_set_node_toolbar(flow_t *f, const flow_toolbar_action *actions, int n);

/* Edge toolbar (inc-7 #5): a floating action bar on the single selected edge, anchored one
   row above the route midpoint (recomputed each frame so it tracks the wire). Reuses the
   shared flow_toolbar_action (its `id` is the selected edge id) and the controls-bar seam.
   BORROWED array; NULL/0 disarms. Transient chrome: never saved, never journaled. */
void flow_set_edge_toolbar(flow_t *f, const flow_toolbar_action *actions, int n);

/* Node resizer (inc-8 #3) — the interactive analog of flow_set_node_size: a single SE-corner
   resize grip (◢) drawn on the LONE selected node (flow_selected_count(f)==1), at LOD 0, while
   the canvas is unlocked. Grabbing it drags the node's w/h live (size-only — the NW origin stays
   fixed), clamped to >= 1, as ONE undo step (the journaled flow_set_node_size rail). Off by
   default (calloc-zero), like xyflow's opt-in NodeResizer; enabling it is the only way to add the
   grip without churning the existing selected-node snapshots. Transient chrome: never saved/
   journaled. v1 = SE corner only; origin-moving handles + min/max + aspect-ratio are deferred. */
void flow_set_resizer(flow_t *f, int enabled);

/* alignment helper lines + snap-to-guide during a single-node drag (inc-5 #8,
   xyflow helperLines). Off by default: with on==0 the drag path is byte-for-byte
   the landed behavior (no snap, no guides). When ON, a dragged edge (L/R/T/B)
   within 1 cell of a VISIBLE neighbor's matching-axis edge snaps onto it and a
   full-row/column dashed guide draws for every exactly-coincident edge.
   Single-node drags only (multi-drag has no single anchor rect). Transient:
   not saved/journaled; guides clear on release. */
void flow_set_helper_lines(flow_t *f, int on);

#ifdef FLOW_IMPLEMENTATION
/* inc-7 #3: the interactive-widget hit-test seam. A render-filled cache of screen rects
   (no heap) the left-press classifier scans ABOVE canvas classification; each entry's
   `owner` selects the provider and `action` keys the handler. Controls is the first
   provider; node/edge toolbars (#4/#5) push their own rects into the SAME list. */
enum { FLOW_WIDGET_OWNER_CONTROLS, FLOW_WIDGET_OWNER_NODE_TOOLBAR, FLOW_WIDGET_OWNER_EDGE_TOOLBAR };
enum { FLOW_WIDGET_ZOOM_IN, FLOW_WIDGET_ZOOM_OUT, FLOW_WIDGET_FIT, FLOW_WIDGET_LOCK };  /* controls actions (owner == CONTROLS) */
struct flow {
  flow_node *nodes; int nnodes, capnodes, nextid;
  flow_edge *edges; int nedges, capedges, nexteid;
  const flow_node_type **ntypes; int nntypes;
  const flow_edge_type **etypes; int netypes;
  flow_viewport view; float zmin, zmax; int cols, rows; flow_cell *front; int running;
  flow_rect node_extent, translate_extent;  /* optional world-space clamps (spec §6.1/§6.2); zero rect = disabled (calloc default) */
  int drag_node, dragging_pan; flow_pt drag_grab, last_mouse;  /* mouse interaction state */
  int focus_node;                 /* keyboard focus id, -1 none (inc-5 #5): an id, NOT a flag;
                                     invalidated on delete/hide of the focused node */
  flow_pt drag_last_world;                                     /* multi-drag: last drag pos in world coords (per-motion delta) */
  int mouse_down, down_node, moved; flow_pt down_pos;          /* press/click tracking */
  flow_pt last_cursor;                                         /* inc-6 #8: last autopan-eligible motion cell (screen); the tick replays a synthetic motion here. Transient — calloc-zero, never saved/journaled. */
  int down_modsel;                                             /* press was a SHIFT/CTRL modifier-select on a node (suppress release replace) */
  int space_held;                                              /* space-pan: sticky toggle (terminal model A) — a press forces drag-to-pan over node OR pane */
  int autopan_margin, autopan_speed;                           /* auto-pan near edge during object drags (node/connect/reconnect): band width (cells) + step per motion event; defaults 3/2 in flow_new */
  unsigned tick; int tick_ms;                                  /* inc-6 #4 redraw-clock: deterministic animation clock + redraw interval (ms). Transient — never journaled, never saved. `tick` is calloc-zero (correct start frame); `tick_ms` defaulted to 100 in flow_new (0 would mean "armed but poll-forever"). */
  int last_click_node;                                         /* dblclick: id of the previous node-body click (-1 = none/consumed); a 2nd click on the same id is a double-click */
  int last_click_edge;                                         /* edge dblclick pair state, mirroring last_click_node; broken by any OTHER click (node/pane/different edge) and on flow_load */
  int cb_suppress;                                             /* >0 suppresses nested observer fires (on_nodes_delete / on_selection_change) from recursive/aggregate mutators (remove_node cascade, delete_selection, select_in_rect's internal clear) */
  int marquee_active, marquee_on; flow_pt marquee_cur;        /* marquee: armed intent / live cursor (screen) */
  flow_pt marquee_anchor_world;   /* press point WORLD-pinned at threshold-cross (inc-5 #3):
                                     the rect grows from here under auto-pan instead of
                                     chasing a screen anchor; re-projected per frame for the
                                     render box; only read while marquee_on */
  int helper_on;                  /* alignment helper lines + snap (inc-5 #8); calloc OFF */
  struct { int vert[8], nvert, horz[8], nhorz; } helper;  /* active guide WORLD lines, refilled
                                     per single-node drag motion, cleared on release;
                                     transient — never journaled/persisted */
  flow_select_mode marquee_mode;                              /* default mode for shift-drag marquee */
  int conn_active, conn_node; char conn_handle[16]; flow_pt conn_end; /* in-flight connection: source node/handle + free end (screen) */
  int conn_valid; int conn_target_node; char conn_target_handle[16]; /* inc-7 #2: live connection-validity cache — would the drop accept, and the candidate node + RESOLVED handle the render recolor matches. Transient: never saved/journaled. conn_target_node inits -1; the other two are calloc-correct. */
  flow_connection_validator validator_fn; void *validator_user;       /* isValidConnection gate (inc-4 #9); NULL = allow all (calloc default) */
  flow_key_hook key_hook_fn; void *key_hook_user;                     /* pre-dispatch key gate (inc-5 #10); NULL = none (calloc default) */
  int key_hook_modal;                                                 /* inc-6 #6: while set AND a hook is installed, an unconsumed seq is DROPPED in flow_dispatch_key (never falls through). Calloc 0 = off. Transient — not journaled, not saved. */
  int reconnect_edge, reconnect_which;                        /* in-flight endpoint-reconnect drag: edge id (-1 idle) + which endpoint (0=source,1=target) */
  int resize_node, resize_corner;                             /* inc-8 #3: in-flight node resize: node id (-1 idle) + corner (0=SE for v1) */
  flow_callbacks cb;
  struct { int enabled, w, h; flow_corner corner; } minimap;
  struct { flow_bg_variant variant; int gap; } bg;
  flow_theme theme; flow_color_mode color_mode;  /* inc-7 #1: engine-chrome tokens + active preset.
                                     Value member (no heap), seeded to the DEFAULT preset in flow_new
                                     (calloc-zero would be black-on-black). Transient — never saved/journaled. */
  struct { char seq[8]; flow_key_fn fn; void *user; } keys[32]; int nkeys;  /* key-binding registry */
  int statusbar;  /* built-in bottom help/status line */
  int locked;     /* inc-7 #3: whole-canvas lock (Controls [lock]) — suppress drag/connect/reconnect/marquee/click-select; pan+zoom still work. Transient: never saved/journaled. */
  struct { int enabled; flow_corner corner; } controls;  /* inc-7 #3: Controls bar config (off by default; the minimap value-struct precedent) */
  struct { int enabled; } resizer;  /* inc-8 #3: node-resizer toggle (off by default; xyflow's opt-in NodeResizer). SE-corner grip on the lone selected node. Transient chrome: never saved/journaled. */
  struct { const flow_toolbar_action *actions; int n; } node_toolbar;  /* inc-7 #4: borrowed action array ({NULL,0}=off) */
  struct { const flow_toolbar_action *actions; int n; } edge_toolbar;  /* inc-7 #5: borrowed action array ({NULL,0}=off) */
  struct { int x, y, w, h, owner, action; } widgets[16]; int nwidgets;  /* inc-7 #3: render-filled widget hit-rect cache (no heap) — drawn region == hittable region; refilled each frame */
  struct {                                  /* selection clipboard (inc-5 #7): deep snapshots.
                                               node snaps store ABS pos in .node.pos (resolved at
                                               copy time — the source graph may be gone at paste);
                                               edge snaps own their label_copy. Survives
                                               flow__graph_reset/flow_load BY OMISSION (not graph
                                               state); freed only in flow_free. calloc-empty. */
    flow__node_snap *nodes; int nn;
    flow__edge_snap *edges; int ne;
    int gen;                                /* cumulative paste-offset counter; reset on copy/cut */
  } clip;
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
  if (need <= *cap) return arr;                  /* need>=1 always, so a no-grow return is non-NULL */
  int c = *cap ? *cap : 8; while (c < need) c *= 2;
  void *p = FLOW_REALLOC(arr, (size_t)c * sz);
  if (!p) return NULL;                           /* OOM: caller KEEPS its old arr; *cap untouched (no leak) */
  *cap = c; return p;                            /* NULL return is unambiguously OOM (success is never NULL) */
}
/* ---- undo journal: recording primitives (PURE DATA — never call mutators, so they are
   safe to invoke from this module; the appliers live in flow_undo.h, after flow_model) ---- */
static char *flow__dup(const char *s) {            /* malloc+memcpy (codebase avoids strdup under -std=c11) */
  if (!s) return NULL;
  size_t n = strlen(s) + 1; char *d = (char*)FLOW_MALLOC(n); if (d) memcpy(d, s, n); return d;
}
static void flow__op_free(flow__op *op) {          /* frees owned label copies; NEVER frees node->data */
  switch (op->kind) {
    case FLOW_CMD_REMOVE_NODE:
      for (int i = 0; i < op->u.subtree.ne; i++) FLOW_FREE(op->u.subtree.edges[i].label_copy);
      FLOW_FREE(op->u.subtree.nodes); FLOW_FREE(op->u.subtree.edges); break;
    case FLOW_CMD_ADD_EDGE: case FLOW_CMD_REMOVE_EDGE: FLOW_FREE(op->u.edge.snap.label_copy); break;
    case FLOW_CMD_SET_LABEL: FLOW_FREE(op->u.label.from); FLOW_FREE(op->u.label.to); break;
    default: break;
  }
}
static void flow__cmd_free(struct flow__cmd *c) {
  for (int i = 0; i < c->nops; i++) flow__op_free(&c->ops[i]);
  FLOW_FREE(c->ops);
}
static void flow__redo_clear(flow_t *f) {
  for (int i = 0; i < f->journal.rn; i++) flow__cmd_free(&f->journal.redo[i]);
  f->journal.rn = 0;
}
/* full teardown: flow_free + flow_load's flow__graph_reset (serialize seam — undo must
   not invert against a replaced graph) */
static void flow__journal_clear(flow_t *f) {
  for (int i = 0; i < f->journal.n; i++) flow__cmd_free(&f->journal.items[i]);
  FLOW_FREE(f->journal.items); f->journal.items = NULL; f->journal.n = f->journal.cap = 0;
  flow__redo_clear(f);
  FLOW_FREE(f->journal.redo); f->journal.redo = NULL; f->journal.rcap = 0;
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
    struct flow__cmd *items = (struct flow__cmd*)flow__grow(f->journal.items, &f->journal.cap,
                                                            f->journal.n + 1, sizeof *f->journal.items);
    if (!items) return;                            /* OOM: drop the undo record; the mutation already applied */
    f->journal.items = items;
    c = &f->journal.items[f->journal.n++];
    memset(c, 0, sizeof *c);
    if (f->journal.txn_depth > 0) f->journal.txn_base = f->journal.n - 1;  /* first record opens the txn's command */
  }
  flow__op *ops = (flow__op*)flow__grow(c->ops, &c->opcap, c->nops + 1, sizeof *c->ops);
  if (!ops) return;                                /* OOM: drop this op (command kept; empty-cmd undo is a benign no-op) */
  c->ops = ops;
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
static void flow__rec_resize(flow_t *f, int id, int fw, int fh, int tw, int th, int from_explicit) {
  if (f->journal.txn_depth > 0 && f->journal.txn_base >= 0) {  /* coalesce within the open txn (like move) */
    struct flow__cmd *c = &f->journal.items[f->journal.txn_base];
    for (int i = 0; i < c->nops; i++)
      if (c->ops[i].kind == FLOW_CMD_RESIZE_NODE && c->ops[i].u.resize.id == id) {
        c->ops[i].u.resize.tw = tw; c->ops[i].u.resize.th = th;  /* keep first from-size + from_explicit, overwrite to-size */
        return;
      }
  }
  flow__op op = flow__op_base(f, FLOW_CMD_RESIZE_NODE);
  op.u.resize.id = id; op.u.resize.fw = fw; op.u.resize.fh = fh; op.u.resize.tw = tw; op.u.resize.th = th;
  op.u.resize.from_explicit = from_explicit;          /* prior flag bit: undo of a once-auto node must drop the flag again */
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
  op.u.subtree.nodes = nn ? (flow__node_snap*)FLOW_MALLOC((size_t)nn * sizeof(flow__node_snap)) : NULL;
  op.u.subtree.edges = ne ? (flow__edge_snap*)FLOW_MALLOC((size_t)ne * sizeof(flow__edge_snap)) : NULL;
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
  flow_t *f = (flow_t*)FLOW_CALLOC(1, sizeof *f);
  if (!f) return NULL;                                         /* OOM: documented NULL return */
  f->view.zoom = 1; f->zmin = FLOW_ZOOM_MIN; f->zmax = FLOW_ZOOM_MAX;
  f->cols = cols; f->rows = rows; f->nextid = 1; f->nexteid = 1;
  f->drag_node = -1; f->marquee_mode = FLOW_SELECT_PARTIAL; f->conn_node = -1; f->focus_node = -1;
  f->conn_target_node = -1;                                    /* inc-7 #2: no candidate (calloc 0 is a valid node id) */
  f->reconnect_edge = -1; f->last_click_node = -1; f->last_click_edge = -1;
  f->resize_node = -1; f->resize_corner = -1;                  /* inc-8 #3: no resize in flight */
  f->autopan_margin = 3; f->autopan_speed = 2;
  f->tick_ms = 100;                                            /* inc-6 #4: 10 Hz redraw when armed; tick stays calloc-zero */
  flow_set_color_mode(f, FLOW_COLOR_DEFAULT);                  /* inc-7 #1: seed the legacy 7/0/8 preset (calloc-zero would be black-on-black) */
  f->journal.limit = 128; f->journal.txn_base = -1;
  f->front = (flow_cell*)FLOW_CALLOC((size_t)cols * rows, sizeof(flow_cell));
  if (!f->front && (size_t)cols * rows != 0) { FLOW_FREE(f); return NULL; }   /* OOM: free partial construction */
  return f;
}
/* free the clipboard's owned memory: each edge snap's label_copy + the arrays.
   NEVER frees node data (borrowed, the undo-snapshot contract). Called from
   flow_free and from copy (replace-on-copy); deliberately NOT from
   flow__graph_reset — the clipboard is not graph state and survives flow_load. */
static void flow__clipboard_clear(flow_t *f) {
  for (int i = 0; i < f->clip.ne; i++) FLOW_FREE(f->clip.edges[i].label_copy);
  FLOW_FREE(f->clip.nodes); FLOW_FREE(f->clip.edges);
  f->clip.nodes = NULL; f->clip.edges = NULL;
  f->clip.nn = f->clip.ne = 0; f->clip.gen = 0;
}
void flow_free(flow_t *f) {
  if (!f) return;
  flow__journal_clear(f);   /* frees command label copies + stacks; drops (never frees) node->data */
  flow__clipboard_clear(f); /* clipboard label copies + arrays (inc-5 #7) */
  for (int i = 0; i < f->nedges; i++) FLOW_FREE(f->edges[i].label);
  FLOW_FREE(f->nodes); FLOW_FREE(f->edges); FLOW_FREE(f->ntypes); FLOW_FREE(f->etypes); FLOW_FREE(f->front); FLOW_FREE(f);
}
/* Tear the graph back to empty for flow_load: free edge labels + node/edge arrays,
   NULL the pointers, zero counts/caps, reset id counters. Leaves view/types/cb/widgets
   intact. NEVER frees node->data (app owns it; same contract as flow_free).
   Also clears the undo journal (serialize×undo seam): undo must not invert against a
   replaced graph; flow_load additionally suppresses recording across its rebuild. */
static void flow__graph_reset(flow_t *f) {
  flow__journal_clear(f);
  for (int i = 0; i < f->nedges; i++) FLOW_FREE(f->edges[i].label);
  FLOW_FREE(f->nodes); FLOW_FREE(f->edges);
  f->nodes = NULL; f->edges = NULL;
  f->nnodes = f->capnodes = 0; f->nedges = f->capedges = 0;
  f->nextid = 1; f->nexteid = 1;
  f->last_click_node = -1;   /* drop the stale dblclick target: reused ids must not fake a double-click */
  f->last_click_edge = -1;   /* same for the edge dblclick pair */
}
void flow_resize(flow_t *f, int cols, int rows) {
  f->cols = cols; f->rows = rows; FLOW_FREE(f->front);
  f->front = (flow_cell*)FLOW_CALLOC((size_t)cols * rows, sizeof(flow_cell));
}
void flow_register_node_type(flow_t *f, const flow_node_type *t) {
  const flow_node_type **p = (const flow_node_type**)FLOW_REALLOC(f->ntypes, (f->nntypes + 1) * sizeof *f->ntypes);
  if (!p) return;                                    /* OOM: drop the registration (old array intact, no leak) */
  f->ntypes = p; f->ntypes[f->nntypes++] = t;
}
void flow_register_edge_type(flow_t *f, const flow_edge_type *t) {
  const flow_edge_type **p = (const flow_edge_type**)FLOW_REALLOC(f->etypes, (f->netypes + 1) * sizeof *f->etypes);
  if (!p) return;                                    /* OOM: drop the registration (old array intact, no leak) */
  f->etypes = p; f->etypes[f->netypes++] = t;
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
  if (n->flags & FLOW_EXPLICIT_SIZE) return;           /* inc-8 #2: a user-sized node keeps its w/h
                                                          at every call site (add/paste/load) */
  const flow_node_type *t = flow_node_type_for(f, n->type);
  if (t && t->measure) { int w = 0, h = 0; t->measure(n, &w, &h); n->w = w; n->h = h; }
  else { n->w = 4; n->h = 3; }
}
int flow_add_node(flow_t *f, const char *type, flow_pt pos, void *data) {
  flow_node *grown = (flow_node*)flow__grow(f->nodes, &f->capnodes, f->nnodes + 1, sizeof(flow_node));
  if (!grown) return -1;                                       /* OOM: graph unchanged (nnodes not advanced) */
  f->nodes = grown;
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
/* Shift-arrow nudge mover (inc-5 #6): move every selection ROOT — a FLOW_SELECTED
   node with no STRICT selected ancestor — by (dx,dy) world cells via flow_move_node
   (absolute-in, so extent clamps apply per node). MIRRORS the multi-drag roots walk
   in flow_input.h deliberately (extracting a shared mover would widen the package's
   blast radius into the byte-locked drag goldens); a root's selected descendants
   follow for free via relative coords. Eventless: flow_move_node fires no observer
   and the FLOW_SELECTED set is untouched. Caller brackets undo. */
static void flow__nudge_selection(flow_t *f, int dx, int dy) {
  for (int i = 0; i < f->nnodes; i++) {
    if (!(f->nodes[i].flags & FLOW_SELECTED)) continue;
    if (f->nodes[i].flags & FLOW_NODRAG) continue;  /* inc-8 #1: draggable=false also pins keyboard nudge (kept byte-in-sync with the multi-drag filter, NOT factored out) */
    int root = 1;                            /* root unless a STRICT ancestor is selected */
    int parent = f->nodes[i].parent, guard = 0;
    while (parent != -1 && guard++ < 1024) {
      flow_node *pn = flow_get_node(f, parent); if (!pn) break;
      if (pn->flags & FLOW_SELECTED) { root = 0; break; }
      parent = pn->parent;
    }
    if (!root) continue;
    flow_pt a = flow_node_abs(f, &f->nodes[i]);
    flow_move_node(f, f->nodes[i].id, (flow_pt){ a.x + dx, a.y + dy });
  }
}
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
  if (gid < 0) { flow__undo_end(f); return -1; }     /* OOM: nothing added, no members reparented */
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
/* inc-7 #2: the SINGLE accept predicate, factored out of flow_add_edge so the live
   connection preview and the commit can never drift. Mirrors the rejects verbatim
   (self / missing endpoint / duplicate / validator), normalizing handles internally
   (NULL -> ""). exclude_edge = an edge id to skip in the duplicate scan (-1 for a fresh
   connect; the reconnected edge's id for flow_reconnect_edge), so the one predicate
   serves add, preview AND reconnect. Returns nonzero iff the edge WOULD be accepted. */
static int flow__connection_would_accept(flow_t *f, int src, int dst,
                                         const char *sh, const char *th, int exclude_edge) {
  if (src == dst) return 0;
  if (!flow_get_node(f, src) || !flow_get_node(f, dst)) return 0;
  const char *shs = sh ? sh : "", *ths = th ? th : "";
  for (int i = 0; i < f->nedges; i++) {                        /* dup = same (source,target,handles) */
    if (f->edges[i].id == exclude_edge) continue;             /* reconnect: skip the edge being moved */
    if (f->edges[i].source == src && f->edges[i].target == dst &&
        strcmp(f->edges[i].source_handle, shs) == 0 && strcmp(f->edges[i].target_handle, ths) == 0) return 0;
  }
  if (f->validator_fn && !f->validator_fn(f, src, dst, shs, ths, f->validator_user)) return 0;
  return 1;
}
/* inc-7 #2: the target-handle defaulting, factored from flow_end_connection: an explicit
   `handle` if non-NULL, else the first TARGET/BOTH handle on `node`, else NULL. Preview
   and commit both resolve-THEN-check on the IDENTICAL resolved handle (anti-drift). */
static const char *flow__resolve_target_handle(flow_t *f, int node, const char *handle) {
  if (handle) return handle;
  if (node == -1) return NULL;
  int hc = flow_node_handle_count(f, node);
  for (int j = 0; j < hc; j++) {
    const flow_handle *h = flow_node_handle_at(f, node, j);
    if (h && (h->kind == FLOW_HANDLE_TARGET || h->kind == FLOW_HANDLE_BOTH)) return h->id;
  }
  return NULL;
}
int flow_add_edge(flow_t *f, int src, int dst, const char *sh, const char *th) {
  if (!flow__connection_would_accept(f, src, dst, sh, th, -1)) return -1;  /* the one shared gate */
  flow_edge *grown = (flow_edge*)flow__grow(f->edges, &f->capedges, f->nedges + 1, sizeof(flow_edge));
  if (!grown) return -1;                                       /* OOM: graph unchanged (nedges not advanced) */
  f->edges = grown;
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
  flow__order_ent *ents = (flow__order_ent*)FLOW_MALLOC((size_t)f->nnodes * sizeof *ents);
  if (!ents) return NULL;
  for (int i = 0; i < f->nnodes; i++) {
    ents[i].idx = i; ents[i].depth = flow__node_depth(f, &f->nodes[i]);
    ents[i].sel = (f->nodes[i].flags & FLOW_SELECTED) ? 1 : 0;
  }
  qsort(ents, (size_t)f->nnodes, sizeof *ents, want_render ? flow__order_cmp_render : flow__order_cmp_hit);
  int *order = (int*)FLOW_MALLOC((size_t)f->nnodes * sizeof(int));
  if (order) for (int i = 0; i < f->nnodes; i++) order[i] = ents[i].idx;
  FLOW_FREE(ents);
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
  FLOW_FREE(order);
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
  int *ids = n > 0 ? (int*)FLOW_MALLOC((size_t)n * sizeof(int)) : NULL;
  if (n > 0) flow_selected_nodes(f, ids, n);
  f->cb.on_selection_change(f, ids, n, f->cb.user);
  FLOW_FREE(ids);
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
/* inc-8 #1: 1 iff `n` accepts pointer/keyboard selection (FLOW_NOSELECT clears it). Interaction
   sites consult this; the public flow_select_node/flow_toggle_node primitives stay UNGATED so paste
   and host apps can still select programmatically (xyflow parity). */
static int flow__node_selectable(const flow_node *n) { return n && !(n->flags & FLOW_NOSELECT); }
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
    if (n->flags & FLOW_NOSELECT) continue;             /* inc-8 #1: selectable=false excluded from marquee */
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
/* inc-7 #1: re-seed f->theme from a fixed preset table. Designated array AND struct
   initializers so neither an enum reorder nor a flow_theme field reorder can silently
   corrupt a preset. DEFAULT == flow_new's seed == the legacy literals (round-trip
   byte-identical); LIGHT flips bg to a light index with a dark fg; DARK keeps the dark
   canvas with a brighter fg. handle_valid/handle_invalid (pkg2 green/red) and
   widget_fg/widget_bg (pkg3-5 chrome == canvas) are constant across presets for now. */
void flow_set_color_mode(flow_t *f, flow_color_mode mode) {
  static const flow_theme presets[] = {
    [FLOW_COLOR_DEFAULT] = { .fg = 7,  .bg = 0,  .grid_fg = 8, .handle = 7,  .handle_valid = 2, .handle_invalid = 1, .accent = 7,  .edge_fg = 7,  .widget_fg = 7,  .widget_bg = 0  },
    [FLOW_COLOR_LIGHT]   = { .fg = 0,  .bg = 15, .grid_fg = 7, .handle = 0,  .handle_valid = 2, .handle_invalid = 1, .accent = 0,  .edge_fg = 0,  .widget_fg = 0,  .widget_bg = 15 },
    [FLOW_COLOR_DARK]    = { .fg = 15, .bg = 0,  .grid_fg = 8, .handle = 15, .handle_valid = 2, .handle_invalid = 1, .accent = 15, .edge_fg = 15, .widget_fg = 15, .widget_bg = 0  },
  };
  if (mode < FLOW_COLOR_DEFAULT || mode > FLOW_COLOR_DARK) mode = FLOW_COLOR_DEFAULT;
  f->theme = presets[mode];
  f->color_mode = mode;
}
flow_color_mode flow_color_mode_get(flow_t *f) { return f->color_mode; }
void flow_set_controls(flow_t *f, int enabled, flow_corner corner) { f->controls.enabled = enabled ? 1 : 0; f->controls.corner = corner; }
void flow_set_locked(flow_t *f, int on) { f->locked = on ? 1 : 0; }
int  flow_locked(flow_t *f) { return f->locked; }
void flow_set_resizer(flow_t *f, int enabled) { f->resizer.enabled = enabled ? 1 : 0; }
void flow_set_node_toolbar(flow_t *f, const flow_toolbar_action *actions, int n) { f->node_toolbar.actions = actions; f->node_toolbar.n = n; }
void flow_set_edge_toolbar(flow_t *f, const flow_toolbar_action *actions, int n) { f->edge_toolbar.actions = actions; f->edge_toolbar.n = n; }
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
  /* inc-7 #2: same shared accept predicate as flow_add_edge — self/duplicate/validator,
     skipping THIS edge in the duplicate scan (exclude_edge = edge). Silent reject leaves
     the edge unchanged. */
  if (!flow__connection_would_accept(f, nsrc, ntgt, nsh, nth, edge)) return;
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
  FLOW_FREE(e->label); e->label = NULL;
  if (label) {                                                 /* malloc+memcpy (avoid strdup decl issues under -std=c11) */
    size_t n = strlen(label) + 1;
    e->label = (char*)FLOW_MALLOC(n);
    if (e->label) memcpy(e->label, label, n);
  }
}
void flow_remove_edge(flow_t *f, int id) {
  for (int i = 0; i < f->nedges; i++) {
    if (f->edges[i].id != id) continue;
    if (flow__rec_gate(f)) flow__rec_remove_edge(f, &f->edges[i], i);  /* snapshot (dup'd label) before freeing */
    FLOW_FREE(f->edges[i].label);                                  /* free-then-shift: no leak, no dbl-free */
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
      int *ids = f->nnodes ? (int*)FLOW_MALLOC((size_t)f->nnodes * sizeof(int)) : NULL; int n = 0;
      for (int i = 0; i < f->nnodes; i++)
        if (flow_is_ancestor(f, id, f->nodes[i].id)) ids[n++] = f->nodes[i].id; /* id and its descendants */
      if (n) f->cb.on_nodes_delete(f, ids, n, f->cb.user);
      FLOW_FREE(ids);
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
      FLOW_FREE(f->edges[i].label);
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
  if (f->focus_node == id) f->focus_node = -1;                /* focus is an id, not a flag: explicit
                                                                 invalidation (descendants clear in their
                                                                 own recursive calls above) */
  f->cb_suppress--; f->journal.suppress--;
  if (top) flow__notify_selection(f, sig);                    /* selection shrank if a selected node vanished */
}
void flow_delete_selection(flow_t *f) {
  /* remove all selected nodes (each cascades children + incident edges); re-query
     after each removal so any count works and cascade-removed selections are skipped.
     on_nodes_delete fires ONCE here with the full set (selected ∪ their descendants);
     the inner flow_remove_node calls run suppressed so they don't each refire. */
  unsigned long sig = flow__sel_sig(f);
  /* inc-8 #1: deselect protected ROOTS up front. Ordering is load-bearing and does double duty:
     (a) the :1225-style re-query loop below now skips them, so it TERMINATES with no change to the
     loop (a bare skip would re-return the same FLOW_SELECTED node forever); (b) a surviving protected
     root is excluded from the on_nodes_delete list, so the callback doesn't lie. A protected node with
     a SELECTED ancestor stays cascade-removed (still satisfies flow__sel_or_ancestor via the ancestor). */
  for (int i = 0; i < f->nnodes; i++)
    if (f->nodes[i].flags & FLOW_NODELETE) f->nodes[i].flags &= ~(unsigned)FLOW_SELECTED;
  if (f->cb.on_nodes_delete) {
    int *ids = f->nnodes ? (int*)FLOW_MALLOC((size_t)f->nnodes * sizeof(int)) : NULL; int n = 0;
    for (int i = 0; i < f->nnodes; i++)
      if (flow__sel_or_ancestor(f, f->nodes[i].id)) ids[n++] = f->nodes[i].id;
    if (n) f->cb.on_nodes_delete(f, ids, n, f->cb.user);
    FLOW_FREE(ids);
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
/* ---- selection clipboard (inc-5 #7) ---- */
static int flow__clip_endpoint_selected(flow_t *f, int id) {
  flow_node *n = flow_get_node(f, id);
  return n && (n->flags & FLOW_SELECTED);
}
/* snapshot the CURRENT selection into caller-owned arrays. Node snaps store the
   ABSOLUTE position in .node.pos (resolved NOW — the source graph may be gone at
   paste time) and keep .node.parent for the paste-time reparent map. Edge snaps
   are taken iff BOTH endpoints are selected; label dup'd into label_copy with
   .edge.label aliased to it (the flow__edge_snap convention). */
static void flow__snap_selection(flow_t *f, flow__node_snap **on, int *onn,
                                 flow__edge_snap **oe, int *one) {
  int nn = 0, ne = 0;
  for (int i = 0; i < f->nnodes; i++) if (f->nodes[i].flags & FLOW_SELECTED) nn++;
  for (int i = 0; i < f->nedges; i++)
    if (flow__clip_endpoint_selected(f, f->edges[i].source) &&
        flow__clip_endpoint_selected(f, f->edges[i].target)) ne++;
  flow__node_snap *ns = nn ? (flow__node_snap*)FLOW_MALLOC((size_t)nn * sizeof *ns) : NULL;
  flow__edge_snap *es = ne ? (flow__edge_snap*)FLOW_MALLOC((size_t)ne * sizeof *es) : NULL;
  int k = 0;
  for (int i = 0; i < f->nnodes; i++) {
    if (!(f->nodes[i].flags & FLOW_SELECTED)) continue;
    ns[k].id = f->nodes[i].id; ns[k].index = i;
    ns[k].node = f->nodes[i];                                /* data BORROWED */
    ns[k].node.pos = flow_node_abs(f, &f->nodes[i]);         /* ABS at copy time */
    k++;
  }
  k = 0;
  for (int i = 0; i < f->nedges; i++) {
    flow_edge *e = &f->edges[i];
    if (!flow__clip_endpoint_selected(f, e->source) ||
        !flow__clip_endpoint_selected(f, e->target)) continue;
    es[k].id = e->id; es[k].index = i;
    es[k].edge = *e;
    es[k].label_copy = flow__dup(e->label);                  /* label OWNED: dup it */
    es[k].edge.label = es[k].label_copy;
    k++;
  }
  *on = ns; *onn = nn; *oe = es; *one = ne;
}
/* paste core: re-mint snaps through the PUBLIC add path — never the id-preserving
   flow__insert_node_at (ids must be fresh). Three passes: mint nodes at abs+off,
   reparent children whose original parent is in the paste map, mint edges (type +
   dup'd label restored; validator gates apply — partial paste on reject). Whole
   sequence = ONE undo step, ONE sig-gated selection event (the pasted set). */
static int flow__paste_snaps(flow_t *f, const flow__node_snap *ns, int nn,
                             const flow__edge_snap *es, int ne, int off) {
  if (nn == 0) return 0;
  unsigned long sig = flow__sel_sig(f);
  int *newid = (int*)FLOW_MALLOC((size_t)nn * sizeof(int));
  if (!newid) return 0;                                       /* OOM: paste nothing rather than deref NULL */
  flow__undo_begin(f);
  f->cb_suppress++;                                          /* one notify at the end */
  for (int i = 0; i < nn; i++) {
    flow_pt at = { ns[i].node.pos.x + off, ns[i].node.pos.y + off };
    newid[i] = flow_add_node(f, ns[i].node.type, at, ns[i].node.data);
    flow_node *pn = flow_get_node(f, newid[i]);              /* re-fetch: add may realloc */
    if (pn) {
      pn->w = ns[i].node.w; pn->h = ns[i].node.h;            /* restore the snap's size over the
                                                                re-measure (group containers and
                                                                manual resizes survive paste; the
                                                                flow_group post-add idiom). The
                                                                ADD_NODE snap captures it at undo
                                                                time, so undo/redo round-trips. */
      pn->flags |= (ns[i].node.flags & (FLOW_EXTENT_PARENT | FLOW_EXPLICIT_SIZE));  /* behavior flags carry;
                                                                transient flags do not. EXPLICIT_SIZE
                                                                must ride along with the copied w/h
                                                                above (inc-8 #2) — copied size without
                                                                the flag would be dropped by the next
                                                                re-measure/save. */
    }
  }
  for (int i = 0; i < nn; i++) {                             /* reparent inside the pasted set */
    int op = ns[i].node.parent;
    if (op == -1) continue;
    for (int j = 0; j < nn; j++)
      if (ns[j].id == op) { flow_set_parent(f, newid[i], newid[j]); break; }
    /* original parent not pasted: stays root at abs+off (dangling-parent rule) */
  }
  for (int i = 0; i < ne; i++) {
    int s = -1, t = -1;
    for (int j = 0; j < nn; j++) {
      if (ns[j].id == es[i].edge.source) s = newid[j];
      if (ns[j].id == es[i].edge.target) t = newid[j];
    }
    int eid = flow_add_edge(f, s, t, es[i].edge.source_handle, es[i].edge.target_handle);
    if (eid == -1) continue;                                 /* validator/dup reject: partial paste */
    flow_edge *pe = flow_get_edge(f, eid);
    if (pe) snprintf(pe->type, sizeof pe->type, "%s", es[i].edge.type);  /* router type, like flow_load */
    if (es[i].label_copy) flow_set_edge_label(f, eid, es[i].label_copy); /* SET_LABEL coalesces into the txn */
  }
  flow_clear_selection(f);                                   /* pasted set becomes the selection */
  for (int i = 0; i < nn; i++) flow_select_node(f, newid[i], 1);
  f->cb_suppress--;
  flow__undo_end(f);
  flow__notify_selection(f, sig);                            /* node-only sig changed: fires once */
  FLOW_FREE(newid);
  return nn;
}
void flow_copy_selection(flow_t *f) {
  flow__clipboard_clear(f);                                  /* replace-on-copy; resets gen */
  flow__snap_selection(f, &f->clip.nodes, &f->clip.nn, &f->clip.edges, &f->clip.ne);
}
void flow_cut_selection(flow_t *f) {
  flow_copy_selection(f);
  flow_delete_selection(f);                                  /* fires on_nodes_delete once; ONE undo step */
}
int flow_paste(flow_t *f) {
  if (f->clip.nn == 0) return 0;
  int n = flow__paste_snaps(f, f->clip.nodes, f->clip.nn, f->clip.edges, f->clip.ne,
                            f->clip.gen + 1);
  f->clip.gen++;                                             /* consecutive pastes cascade */
  return n;
}
int flow_duplicate_selection(flow_t *f) {
  flow__node_snap *ns; flow__edge_snap *es; int nn, ne;
  flow__snap_selection(f, &ns, &nn, &es, &ne);               /* LOCAL temp: f->clip untouched */
  int r = flow__paste_snaps(f, ns, nn, es, ne, 1);
  for (int i = 0; i < ne; i++) FLOW_FREE(es[i].label_copy);
  FLOW_FREE(ns); FLOW_FREE(es);
  return r;
}
/* getViewportForBounds: pick a zoom that makes rect b fit inside the usable area
   (cols/rows minus `margin` cells of padding on each side), clamped to the
   [zmin,zmax] range, then pan so the rect centre lands on the screen centre.
   One editable definition — shared by flow_fit_view and flow_fit_bounds (inc-5 #4);
   callers guard b.w/b.h > 0. */
static void flow__fit_rect(flow_t *f, flow_rect b, int margin) {
  float zx = (float)(f->cols - 2 * margin) / (float)b.w;
  float zy = (float)(f->rows - 2 * margin) / (float)b.h;
  float z = zx < zy ? zx : zy;
  if (z < f->zmin) z = f->zmin;
  if (z > f->zmax) z = f->zmax;
  flow__view_set(f, f->cols / 2.0f - (b.x + b.w / 2.0f) * z,  /* seam clamps (extent wins over centering) + fires */
                    f->rows / 2.0f - (b.y + b.h / 2.0f) * z, z);
}
void flow_fit_view(flow_t *f, int margin) {
  if (f->nnodes == 0) return;                                 /* empty: no-op */
  flow_rect b = flow_bounds(f);
  if (b.w <= 0 || b.h <= 0) return;                           /* fully hidden: no-op */
  flow__fit_rect(f, b, margin);
}
void flow_fit_bounds(flow_t *f, flow_rect r, int margin) {
  if (r.w <= 0 || r.h <= 0) return;                           /* degenerate rect: no-op */
  flow__fit_rect(f, r, margin);
}
flow_rect flow_bounds_of(flow_t *f, const int *ids, int n) {
  /* explicit-id sibling of flow_bounds: MODEL-level (a caller who NAMED these nodes
     gets what they named, hidden included — matching the flow_query.h family), so no
     flow__node_visible skip. Missing ids skip (query convention: "missing id -> 0"). */
  flow_rect b = {0, 0, 0, 0}; int seeded = 0;
  if (!ids || n <= 0) return b;
  for (int i = 0; i < n; i++) {
    flow_node *nd = flow_get_node(f, ids[i]);
    if (!nd) continue;
    flow_rect r = flow_node_rect_abs(f, nd);
    b = seeded ? flow_rect_union(b, r) : r; seeded = 1;
  }
  return b;
}
void flow_set_connection_validator(flow_t *f, flow_connection_validator fn, void *user) {
  f->validator_fn = fn; f->validator_user = user;   /* NULL fn = allow all (default) */
}
void flow_set_helper_lines(flow_t *f, int on) {
  f->helper_on = on;
  if (!on) { f->helper.nvert = 0; f->helper.nhorz = 0; }  /* OFF drops any live guides */
}
void flow_set_key_hook(flow_t *f, flow_key_hook fn, void *user) {
  f->key_hook_fn = fn; f->key_hook_user = user;   /* NULL fn = no hook (default) */
}
void flow_set_key_hook_modal(flow_t *f, int on) { f->key_hook_modal = on ? 1 : 0; }  /* inc-6 #6: does NOT touch key_hook_fn, so the two setters' call order is free */
void flow_set_node_hidden(flow_t *f, int id, int hidden) {
  flow_node *n = flow_get_node(f, id);
  if (!n) return;
  if (hidden) {
    unsigned long sig = flow__sel_sig(f);
    n->flags |= FLOW_HIDDEN;
    if (f->focus_node == id) f->focus_node = -1;    /* hide unfocuses: same no-invisible-cursor
                                                       rule as the deselect below (inc-5 #5) */
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
void flow_set_edge_animated(flow_t *f, int id, int on) {        /* inc-6 #5: mirrors flow_set_edge_hidden, minus the deselect side-effect (animation is orthogonal to selection/visibility). Stores NO armed state — #4's flow__frames_armed scans the flag each poll. */
  flow_edge *e = flow_get_edge(f, id);
  if (!e) return;
  if (on) e->flags |= FLOW_ANIMATED;
  else    e->flags &= ~(unsigned)FLOW_ANIMATED;
}
/* inc-8 #1: positive verb, negative flag — on==0 SETS the gate. Mirrors flow_set_edge_animated's
   shape; polarity inverted because the public API reads in xyflow-permissive terms. */
void flow_set_node_draggable(flow_t *f, int id, int on)  { flow_node *n = flow_get_node(f, id); if (!n) return; if (on) n->flags &= ~(unsigned)FLOW_NODRAG;   else n->flags |= FLOW_NODRAG; }
void flow_set_node_selectable(flow_t *f, int id, int on) { flow_node *n = flow_get_node(f, id); if (!n) return; if (on) n->flags &= ~(unsigned)FLOW_NOSELECT; else n->flags |= FLOW_NOSELECT; }
void flow_set_node_deletable(flow_t *f, int id, int on)  { flow_node *n = flow_get_node(f, id); if (!n) return; if (on) n->flags &= ~(unsigned)FLOW_NODELETE; else n->flags |= FLOW_NODELETE; }
/* inc-8 #2: set explicit w/h + the durable FLOW_EXPLICIT_SIZE flag. Unlike the gate setters this is
   JOURNALED (the resize-as-drag analog of flow_move_node) — it records a RESIZE_NODE op so the change
   is undoable on its own AND coalesces inside a package-3 gesture txn into one step. */
void flow_set_node_size(flow_t *f, int id, int w, int h) {
  flow_node *n = flow_get_node(f, id);
  if (!n) return;
  if (w < 1) w = 1;                                  /* clamp to a renderable minimum */
  if (h < 1) h = 1;
  if (flow__rec_gate(f)) flow__rec_resize(f, id, n->w, n->h, w, h, (n->flags & FLOW_EXPLICIT_SIZE) != 0);  /* record BEFORE the write (from = current size + flag) */
  n->w = w; n->h = h; n->flags |= FLOW_EXPLICIT_SIZE;
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
/* inc-6 #6: length of the leading key sequence to DROP when modal. For a CSI (ESC '[' …)
   return ESC through the first final byte in 0x40..0x7e (@..~, the CSI terminator class —
   covers arrows A/B/C/D=3, Shift-Tab Z=3, Delete 3~=4, Shift-arrow 1;2A=6), clamped to n;
   otherwise 1 (a lone control byte or printable). Atomic: dropping only the ESC would
   re-feed the CSI tail as separate bytes into the query — a worse bug. A CSI split past n
   clamps to n (the SAME accepted trade-off flow_feed's lone-ESC path documents,
   src/flow_run.h: terminals write sequences atomically; ESC-timeout is out of scope). */
static int flow__seq_len(const char *seq, int n) {
  if (n >= 2 && seq[0] == '\x1b' && seq[1] == '[') {
    for (int i = 2; i < n; i++)
      if ((unsigned char)seq[i] >= 0x40 && (unsigned char)seq[i] <= 0x7e) return i + 1;
    return n;                                   /* no terminator within n: drop what we have */
  }
  return 1;
}
int flow_dispatch_key(flow_t *f, const char *seq, int n) {
  if (n <= 0) return 0;
  /* (0) pre-dispatch key hook (inc-5 #10): a modal UI sees the bytes before ANY
     binding or built-in; a positive return = bytes consumed, passed verbatim to
     flow_feed's i-advance. 0 = pass-through to the registry below. While key_hook_modal
     is set and a hook is installed, an UNCONSUMED seq returns its dropped length (>=1)
     instead of 0 (inc-6 #6) — so it never reaches a binding/built-in or flow_feed's CSI
     switch. Gated on key_hook_fn: modal + NULL hook drops nothing (footgun guard). */
  if (f->key_hook_fn) {
    int c = f->key_hook_fn(f, seq, n, f->key_hook_user);
    if (c > 0) return c;
    if (f->key_hook_modal) return flow__seq_len(seq, n);  /* drop the unconsumed sequence atomically */
  }
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
  if (seq[0] == '\t') { flow_focus_next(f); return 1; }             /* Tab: focus traversal (inc-5 #5); Shift-Tab is CSI \x1b[Z, handled in flow_feed */
  if (seq[0] == 'y') { flow_copy_selection(f); return 1; }          /* clipboard (inc-5 #7): y copy */
  if (seq[0] == 'c') { flow_cut_selection(f); return 1; }           /*   c cut  */
  if (seq[0] == 'p') { flow_paste(f); return 1; }                   /*   p paste */
  if (seq[0] == 'd') { flow_duplicate_selection(f); return 1; }     /*   d duplicate */
  if (seq[0] == 'q') { f->running = 0; return 1; }                  /* quit (inc-6 #3): behind hook+registry — a modal
                                                                       can veto, an app can rebind; consumed even when
                                                                       running is already 0 (flow_run's liveness bit) */
  if (seq[0] == '\r') {                                             /* Enter: select the focused node (REPLACE — focus is a single cursor) */
    if (f->focus_node != -1 && flow__node_selectable(flow_get_node(f, f->focus_node)))
      flow_select_node(f, f->focus_node, 0);                        /* inc-8 #1: selectable=false focused node isn't selected by Enter */
    return 1;                                                       /* consumed even with no focus (no-op) */
  }
  /* (3) unhandled: bare arrows, anything else */
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
/* inc-8 #3: the SINGLE source for "is there a resize grip, and where". The render pass
   (flow__node_resizer) and the press-arm (flow_input.h) both call this, so the drawn grip and
   the hittable cell can never drift — the flow__handle_screen single-source discipline. Returns
   the eligible node id (writing its SE-corner screen cell to *out_cell), or -1. Gated: resizer
   enabled, NOT locked, exactly one selected node, that node visible, LOD 0 (at LOD 1 the
   footprint collapses to a 1x1 marker, src/flow_model.h flow__node_footprint, so a corner grip
   would alias the body). The press-arm sits AFTER the lock gate too, so lock-safety is belt-and-
   suspenders. SE corner == (fp.x+fp.w-1, fp.y+fp.h-1) of the rendered footprint. */
static int flow__resize_marker(flow_t *f, flow_pt *out_cell) {
  if (!f->resizer.enabled || f->locked) return -1;
  if (flow_selected_count(f) != 1) return -1;
  if (flow__lod_for(f, f->view.zoom) != 0) return -1;
  int id = flow_selected_node(f);
  flow_node *n = flow_get_node(f, id);
  if (!n || !flow__node_visible(f, n)) return -1;
  flow_rect fp = flow__node_footprint(f, n, 0);   /* LOD 0 guaranteed above */
  if (out_cell) { out_cell->x = fp.x + fp.w - 1; out_cell->y = fp.y + fp.h - 1; }
  return id;
}
void flow_set_hover(flow_t *f, int node) {
  for (int i = 0; i < f->nnodes; i++) f->nodes[i].flags &= ~FLOW_HOVERED;
  flow_node *n = flow_get_node(f, node); if (n) n->flags |= FLOW_HOVERED;
}
int flow_hovered_node(flow_t *f) {
  for (int i = 0; i < f->nnodes; i++) if (f->nodes[i].flags & FLOW_HOVERED) return f->nodes[i].id;
  return -1;
}
/* ---- keyboard focus (inc-5 #5) ---- */
int flow_focused_node(flow_t *f) { return f->focus_node; }
/* fully offscreen iff the node's drawn screen footprint (LOD-aware, same projection
   as render/hit) misses [0,cols) x [0,rows) entirely — "looks offscreen" and "is
   framed" can never disagree. Partially-visible nodes are NOT offscreen (no jump). */
static int flow__focus_offscreen(flow_t *f, const flow_node *n) {
  flow_rect r = flow__node_footprint(f, n, flow__lod_for(f, f->view.zoom));
  return r.x + r.w <= 0 || r.y + r.h <= 0 || r.x >= f->cols || r.y >= f->rows;
}
/* shared tail: commit the focus, then frame iff the node is fully offscreen
   (autoPanOnNodeFocus). flow_set_center keeps the current zoom (-1) and routes
   through flow__view_set (clamp-first, on_viewport_change iff changed). */
static void flow__focus_commit(flow_t *f, int id) {
  f->focus_node = id;
  flow_node *n = flow_get_node(f, id);
  if (n && flow__focus_offscreen(f, n)) {
    flow_rect wr = flow_node_rect_abs(f, n);
    flow_set_center(f, wr.x + wr.w / 2, wr.y + wr.h / 2, -1.0f);
  }
}
void flow_set_focus(flow_t *f, int id) {
  flow_node *n = id >= 0 ? flow_get_node(f, id) : NULL;
  if (!n || !flow__node_visible(f, n)) { f->focus_node = -1; return; }  /* hidden/absent: clear */
  flow__focus_commit(f, id);
}
static int flow__focus_index(flow_t *f) {
  if (f->focus_node < 0) return -1;
  for (int i = 0; i < f->nnodes; i++) if (f->nodes[i].id == f->focus_node) return i;
  return -1;                                   /* stale id: treated as no focus */
}
void flow_focus_next(flow_t *f) {
  int n = f->nnodes;
  if (n == 0) { f->focus_node = -1; return; }
  int start = flow__focus_index(f);            /* -1 => first probe is index 0 */
  for (int s = 1; s <= n; s++) {
    flow_node *nd = &f->nodes[(start + s + n) % n];
    if (flow__node_visible(f, nd)) { flow__focus_commit(f, nd->id); return; }
  }
  f->focus_node = -1;                          /* zero visible nodes */
}
void flow_focus_prev(flow_t *f) {
  int n = f->nnodes;
  if (n == 0) { f->focus_node = -1; return; }
  int start = flow__focus_index(f);
  if (start < 0) start = 0;                    /* from none: first probe is the LAST node */
  for (int s = 1; s <= n; s++) {
    flow_node *nd = &f->nodes[(start - s + 2 * n) % n];
    if (flow__node_visible(f, nd)) { flow__focus_commit(f, nd->id); return; }
  }
  f->focus_node = -1;
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
  f->conn_target_node = -1; f->conn_target_handle[0] = 0; f->conn_valid = 0;  /* inc-7 #2: a fresh gesture starts with no candidate (no stale recolor before the first motion) */
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
  int tn = -1; int hidx = flow_hit_handle(f, screen, &tn);  /* capture the index (was discarded): a specific-handle hover vs a body hover */
  if (tn == -1) tn = flow_hit_node(f, screen);          /* else any node body */
  /* inc-7 #2: recompute the live validity cache each motion. Default = no candidate. */
  f->conn_target_node = -1; f->conn_target_handle[0] = 0; f->conn_valid = 0;
  if (tn != -1 && tn != f->conn_node) {
    flow_set_hover(f, tn);                               /* reveal candidate's handles */
    /* the SPECIFIC handle under the cursor, else default — resolve-THEN-check on the SAME
       handle the commit path resolves (flow__resolve_connection_at, src/flow_input.h:48-52),
       so the green/red preview can never lie about what the drop will do. */
    const flow_handle *hh = (hidx >= 0) ? flow_node_handle_at(f, tn, hidx) : NULL;
    const char *rth = flow__resolve_target_handle(f, tn, hh ? hh->id : NULL);
    f->conn_target_node = tn;
    if (rth) snprintf(f->conn_target_handle, sizeof f->conn_target_handle, "%s", rth);
    f->conn_valid = flow__connection_would_accept(f, f->conn_node, tn, f->conn_handle, rth, -1);
  }
  return (tn != f->conn_node) ? tn : -1;
}
int flow_connection_valid(flow_t *f) { return (f->conn_active && f->conn_valid) ? 1 : 0; }  /* bound to the active gesture */
int flow_end_connection(flow_t *f, int node, const char *handle) {
  if (!f->conn_active) return -1;
  int src = f->conn_node; char sh[16]; snprintf(sh, sizeof sh, "%s", f->conn_handle);
  /* clear connecting state up-front so a rejected add never leaves us stuck */
  f->conn_active = 0; f->conn_node = -1; f->conn_handle[0] = 0;
  /* target handle defaulting — shared with the live preview via flow__resolve_target_handle
     so commit and preview pick the IDENTICAL handle (inc-7 #2 anti-drift). */
  const char *th = flow__resolve_target_handle(f, node, handle);
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
