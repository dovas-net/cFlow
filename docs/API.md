# flow — API reference

`flow` is a single-header C library for interactive node-graph editors in the terminal —
a C / terminal analog of [xyflow](https://github.com/xyflow/xyflow) (React Flow). This
document is the per-function reference; for the design rationale see
[`docs/superpowers/specs/2026-06-02-c-xyflow-flow-design.md`](superpowers/specs/2026-06-02-c-xyflow-flow-design.md),
and for a copy-paste start see the README Quickstart.

This reference tracks the API as declared in `flow.h` (version is in `FLOW_VERSION_STRING`).
Private `flow__*` symbols and internal types are deliberately omitted — they live inside
`#ifdef FLOW_IMPLEMENTATION` and are not part of the supported surface.

## Using the library

```c
#define FLOW_IMPLEMENTATION   /* in EXACTLY ONE translation unit, before the include */
#include "flow.h"
/* …everywhere else, include "flow.h" WITHOUT the macro. */
```

Build with C99 or later and link the math library: `cc app.c -std=c11 -lm`. The interactive
run-loop (`flow_run` / `flow_feed` / `flow_present` / `flow_term_*`) needs a POSIX terminal
(Linux / macOS); the model, geometry, rendering, routing, layout, and JSON layers are pure C
and embeddable with the host's own I/O.

## Conventions

These hold library-wide; per-function entries only restate a point when it deviates.

- **Naming.** `flow_*` = public functions; `FLOW_*` = public constants, flags, and macros;
  `flow__*` = private (not documented here). Negative-polarity flags (`FLOW_NODRAG`,
  `FLOW_NOSELECT`, …) mean *disable* — a zero-initialized graph is fully permissive.
- **Coordinates.** Two spaces: **world** (model coordinates, unbounded) and **screen**
  (terminal cells). The viewport maps between them; `flow_project` / `flow_unproject` (and the
  `flow_to_screen` / `flow_to_world` aliases) convert. Sizes are in whole cells.
- **Handles into the model.** `flow_get_node` / `flow_get_edge` return pointers *into* the
  node/edge arrays — re-fetch after any `flow_add_*`, which may reallocate. Nodes and edges are
  addressed by stable integer **id**, not array index.
- **Ownership.** A node's `data` pointer is **borrowed** — flow never frees it (it is handed
  back verbatim on undo-of-delete). Edge labels are **engine-owned** (copied in, freed by flow).
  Buffers flow returns to *you* — e.g. the strings from `flow_render_diff` and `flow_save` — are
  yours to release with the configured allocator's `free`.
- **Return-on-error / sentinels.** `flow_new` returns `NULL` on allocation failure;
  `flow_add_node` / `flow_add_edge` return `-1` (graph left unmodified). Mutators keyed by id are
  a silent **no-op on an unknown id**; lookups return `NULL` / `0` / `-1` for "not found". These
  are the documented contract — callers can branch on them.
- **Out-of-memory.** As above for `flow_new` / `flow_add_*`. Other paths (render / layout scratch
  and the selection / clipboard / remove-subtree snapshot buffers) currently assume allocation
  succeeds; `#define` a `FLOW_MALLOC` that aborts on failure to make every path fail-fast.
- **Undo.** Recorded mutators journal an inverse op; one user gesture = one undo **step**
  (transactions coalesce). Transient bits (`FLOW_SELECTED` / `FLOW_DRAGGING` / `FLOW_HOVERED`)
  clear on undo; durable gates and sizes survive. Viewport pan/zoom/fit and selection are **not**
  journaled. Entries below note "journaled" only where it applies.
- **Allocator & assert hooks (stb convention).** To route flow's heap through your own
  arena/tracker, `#define` **all four** of `FLOW_MALLOC` / `FLOW_CALLOC` / `FLOW_REALLOC` /
  `FLOW_FREE` before the include (they must be overridden as a set). `FLOW_ASSERT` defaults to
  `assert()`; `#define` it (even to a no-op) to override.
- **Thread-safety.** Not thread-safe. Serialize all calls against a single `flow_t`; distinct
  instances are fully independent.

---

## Contents

- [Core model](#core-model)
  - [Lifecycle](#lifecycle)
  - [Nodes](#nodes)
  - [Edges](#edges)
  - [Selection & focus](#selection--focus)
  - [Dragging & auto-pan](#dragging--auto-pan)
  - [Connections & handles](#connections--handles)
  - [Groups / subflows](#groups--subflows)
  - [Undo / redo](#undo--redo)
  - [Viewport, zoom & coordinates](#viewport-zoom--coordinates)
  - [Theming & color modes](#theming--color-modes)
  - [Chrome (minimap / controls / toolbars / background / status bar / resizer)](#chrome-minimap--controls--toolbars--background--status-bar--resizer)
  - [Per-node interaction gates, hidden / animated flags, explicit size](#per-node-interaction-gates-hidden--animated-flags-explicit-size)
  - [Selection clipboard & marquee](#selection-clipboard--marquee)
  - [Callbacks & key bindings](#callbacks--key-bindings)
  - [Animation clock](#animation-clock)
  - [Persistence (save / load)](#persistence-save--load)
  - [Edge hit-testing & endpoints](#edge-hit-testing--endpoints)
- [Type registration — custom nodes & edges (vtables)](#type-registration--custom-nodes--edges-vtables)
  - [Registering the built-in types](#registering-the-built-in-types)
- [Queries & hit-testing](#queries--hit-testing)
  - [Graph traversal](#graph-traversal)
  - [Spatial hit-tests](#spatial-hit-tests)
  - [Label search](#label-search)
- [Geometry helpers](#geometry-helpers)
  - [Points and viewport projection](#points-and-viewport-projection)
  - [Rectangles](#rectangles)
- [Auto-layout](#auto-layout)
  - [Force-directed and layered layout](#force-directed-and-layered-layout)
- [Edge routing](#edge-routing)
  - [Route buffer](#route-buffer)
  - [Routers](#routers)
  - [Predefined edge types](#predefined-edge-types)
- [Rendering](#rendering)
  - [Composing the model to a cell buffer](#composing-the-model-to-a-cell-buffer)
- [Cell buffer primitives](#cell-buffer-primitives)
  - [UTF-8 codec](#utf-8-codec)
  - [Cell buffer put/clear](#cell-buffer-putclear)
  - [Surface drawing](#surface-drawing)
  - [Surface dimensions](#surface-dimensions)
  - [Damage diff](#damage-diff)
- [Input feed](#input-feed)
  - [Parsing terminal input](#parsing-terminal-input)
  - [Feeding events to the model](#feeding-events-to-the-model)
- [Terminal control (POSIX)](#terminal-control-posix)
  - [Raw mode and alt-screen](#raw-mode-and-alt-screen)
  - [Restore](#restore)
  - [Terminal size](#terminal-size)
- [Run loop & presentation](#run-loop--presentation)
  - [Present & diff-flush](#present--diff-flush)
  - [The embedder seam](#the-embedder-seam)
  - [Blocking run loop](#blocking-run-loop)

---

## Core model

### Lifecycle

Engine construction, teardown, terminal resize, and type-vtable registration.

**`flow_new(int cols, int rows)` → `flow_t *`**
Allocates and zero-inits a new engine sized to a `cols`×`rows` terminal. Returns the new handle, or `NULL` on OOM (partial construction is freed). Seeds zoom 1.0 with the default `[0.25, 4.0]` limits, the `FLOW_COLOR_DEFAULT` theme, undo limit 128, and auto-pan band 3/2. Caller owns the returned handle and must release it with `flow_free`.

**`flow_free(flow_t *f)` → `void`**
Tears down the engine: frees the undo journal (label copies + stacks), the clipboard, every edge label, and the node/edge/type/front-buffer arrays, then frees `f` itself. No-op if `f` is `NULL`. Never frees node `data` pointers (borrowed — app owns them).

**`flow_resize(flow_t *f, int cols, int rows)` → `void`**
Resizes the engine to a new `cols`×`rows` terminal, reallocating the front cell buffer.

**`flow_register_node_type(flow_t *f, const flow_node_type *t)` → `void`**
Registers a node-type vtable (measure/render/handles/save/load/label) under `t->type`. The pointer is borrowed — the app must keep `t` alive while it is installed.

**`flow_register_edge_type(flow_t *f, const flow_edge_type *t)` → `void`**
Registers an edge-type vtable (route function) under `t->type`. The pointer is borrowed and must outlive its registration.

**`flow_node_type_for(flow_t *f, const char *type)` → `const flow_node_type *`**
Looks up the registered node-type vtable matching `type`. Returns the vtable pointer, or `NULL` if none is registered.

**`flow_edge_type_for(flow_t *f, const char *type)` → `const flow_edge_type *`**
Looks up the registered edge-type vtable matching `type`. Returns the vtable pointer, or `NULL` if none is registered.

### Nodes

Create, move, query, and measure nodes; read node geometry. Re-fetch any node pointer after an add (the array may realloc).

**`flow_measure_node(flow_t *f, flow_node *n)` → `void`**
(Re)computes `n`'s `w`/`h` via its type's `measure` callback. Skipped (size left untouched) when the node carries `FLOW_EXPLICIT_SIZE`.

**`flow_add_node(flow_t *f, const char *type, flow_pt pos, void *data)` → `int`**
Adds a node of `type` at world `pos` as a top-level (parentless) node, then measures it. Returns the new node id, or `-1` on OOM (graph unchanged). An unknown/`NULL` `type` is accepted and stored (falling back to `"default"` when `NULL`). `data` is a borrowed user pointer — never freed by flow. Journaled (one undo step).

**`flow_move_node(flow_t *f, int id, flow_pt pos)` → `void`**
Moves node `id` to world `pos`, clamping to the node extent when set and (for `FLOW_EXTENT_PARENT` nodes) keeping children inside the parent. Programmatic move is unconditional (ignores the `FLOW_NODRAG` user gate). Journaled.

**`flow_get_node(flow_t *f, int id)` → `flow_node *`**
Returns a pointer to node `id` within the node array, or `NULL` if absent. The pointer is invalidated by any subsequent `flow_add_node` (the array may realloc) — re-fetch after adds.

**`flow_node_count(flow_t *f)` → `int`**
Returns the number of nodes in the graph (including hidden ones).

**`flow_nodes(flow_t *f)` → `flow_node *`**
Returns a pointer to the contiguous node array (length `flow_node_count`), or whatever the array currently is. Invalidated by any add that grows the array.

**`flow_node_abs(flow_t *f, const flow_node *n)` → `flow_pt`**
Returns `n`'s absolute world position, resolving the parent chain (cycle-guarded). For a top-level node this equals its stored position.

**`flow_node_rect_abs(flow_t *f, const flow_node *n)` → `flow_rect`**
Returns `n`'s absolute world rectangle (`flow_node_abs` origin plus the node's `w`/`h`).

**`flow_node_pos(const flow_node *n)` → `flow_pt`**
Returns `n`'s relative (stored) position — the companion to `flow_node_abs`. For a top-level node the two coincide.

### Edges

Create, query, relabel, reconnect, and remove edges. Edge labels are owned (strdup'd) by flow.

**`flow_add_edge(flow_t *f, int src, int dst, const char *sh, const char *th)` → `int`**
Adds an edge from node `src` (handle `sh`) to node `dst` (handle `th`). Returns the new edge id, or `-1` on OOM or if the connection is rejected (self-edge, missing node, duplicate `(source,target,handles)`, or installed validator veto). `NULL` handles are stored as `""`. Recorded (journaled) only on success.

**`flow_get_edge(flow_t *f, int id)` → `flow_edge *`**
Returns a pointer to edge `id` within the edge array, or `NULL` if absent. Invalidated by any edge add that grows the array.

**`flow_edge_count(flow_t *f)` → `int`**
Returns the number of edges in the graph (including hidden ones).

**`flow_edges(flow_t *f)` → `flow_edge *`**
Returns a pointer to the contiguous edge array (length `flow_edge_count`). Invalidated by any add that grows the array.

**`flow_set_edge_label(flow_t *f, int edge, const char *label)` → `void`**
Sets edge `edge`'s label, freeing the prior one; `label` is strdup'd, and `NULL` clears it. Journaled (set-label).

**`flow_reconnect_edge(flow_t *f, int edge, int endpoint_node, const char *handle, int which)` → `void`**
Repoints one endpoint of `edge` (`which`: 0=source, 1=target) to `endpoint_node`/`handle`. Revalidated like `flow_add_edge` (rejects self-edge + duplicate `(source,target,handles)`); on rejection the edge is left unchanged.

**`flow_remove_node(flow_t *f, int id)` → `void`**
Removes node `id`, cascading to its incident edges and (recursively) its child nodes, freeing the removed edges' labels. Borrowed node `data` is dropped, never freed. Journaled (the whole subtree is snapshotted as one step). Callers must re-fetch node/edge pointers afterward.

**`flow_remove_edge(flow_t *f, int id)` → `void`**
Removes edge `id`, freeing its label; no-op if `id` is absent. Journaled. Re-fetch edge pointers afterward.

### Selection & focus

Selection is a true set over the `FLOW_SELECTED` node flag, plus a single hovered node and a single keyboard-focused node.

**`flow_select_node(flow_t *f, int id, int additive)` → `void`**
Selects node `id`. With `additive=0` other selections are cleared first; with `additive=1` it is added without clearing. Fires `on_selection_change` only on an actual change.

**`flow_toggle_node(flow_t *f, int id)` → `void`**
Toggles node `id` in the selection set (adds if unset, removes if set); never clears other selections.

**`flow_clear_selection(flow_t *f)` → `void`**
Clears the whole `FLOW_SELECTED` set — both nodes and edges. Fires `on_selection_change` only on an actual change.

**`flow_selected_node(flow_t *f)` → `int`**
Returns the first selected node id (insertion order), or `-1` if none selected.

**`flow_selected_edge(flow_t *f)` → `int`**
Returns the first selected edge id, or `-1` if none selected.

**`flow_select_edge(flow_t *f, int id, int additive)` → `void`**
Sets `FLOW_SELECTED` on edge `id`. When `!additive`, node selection and other edge selection are cleared first (node/edge selection are mutually exclusive).

**`flow_selected_count(flow_t *f)` → `int`**
Returns the number of selected (`FLOW_SELECTED`) nodes.

**`flow_selected_nodes(flow_t *f, int *out, int max)` → `int`**
Fills `out[]` with up to `max` selected node ids in insertion order and returns the total selected count (which may exceed `max`).

**`flow_set_hover(flow_t *f, int node)` → `void`**
Sets `FLOW_HOVERED` on `node` (clearing it from others); `-1` clears all hover.

**`flow_hovered_node(flow_t *f)` → `int`**
Returns the first hovered node id, or `-1` if none.

**`flow_focused_node(flow_t *f)` → `int`**
Returns the current keyboard-focused node id, or `-1` if none. Focus is transient (not journaled, not persisted).

**`flow_set_focus(flow_t *f, int id)` → `void`**
Sets keyboard focus to node `id`; `id<0` or a hidden/absent node clears focus to `-1`. Re-centers the viewport on the node if it is fully offscreen.

**`flow_focus_next(flow_t *f)` → `void`**
Tab built-in: moves focus to the next visible node in insertion order (wrapping; hidden skipped). Frames the node if it is offscreen.

**`flow_focus_prev(flow_t *f)` → `void`**
Shift-Tab built-in: moves focus to the previous visible node (wrapping; hidden skipped). Frames the node if it is offscreen.

### Dragging & auto-pan

Configure the drag-time auto-pan band. (Live drag state is driven through `flow_feed`/the input layer.)

**`flow_set_autopan(flow_t *f, int margin, int speed)` → `void`**
Tunes the object-drag auto-pan band (defaults 3/2): `margin` is the band width in cells, `speed` the pan step per motion event. Negatives clamp to 0; `margin` 0 disables auto-pan. Transient.

### Connections & handles

In-flight connection gestures (source-handle drag to a target handle) plus handle geometry and hit-testing.

**`flow_handle_anchor(flow_t *f, const flow_node *n, const flow_handle *h)` → `flow_pt`**
Returns the world-absolute cell of handle `h` on node `n` (TOP/RIGHT/BOTTOM/LEFT side midpoints plus the handle's `along` offset).

**`flow_node_handle_count(flow_t *f, int node)` → `int`**
Returns the number of handles declared by `node`'s registered type.

**`flow_node_handle_at(flow_t *f, int node, int idx)` → `const flow_handle *`**
Returns the `idx`-th handle of `node`'s type, or `NULL` if out of range.

**`flow_hit_handle(flow_t *f, flow_pt screen, int *out_node)` → `int`**
Returns the handle index (and node id via `out_node`) at the given `screen` cell, considering only hovered/selected/connecting nodes; returns `-1` if no handle is hit.

**`flow_begin_connection(flow_t *f, int node, const char *handle)` → `int`**
Enters connection mode dragging from `node`'s source `handle`. Returns `0` on success, `-1` if it is not a valid source.

**`flow_update_connection(flow_t *f, flow_pt screen)` → `int`**
Moves the in-flight connection's free end to `screen` and returns the hovered candidate target node id, or `-1` if none. Recomputes the validity cache.

**`flow_connection_valid(flow_t *f)` → `int`**
Returns `1` if the current in-flight candidate drop would be accepted (a resolved handle under the cursor), else `0` (no connection, self, duplicate, or validator veto). Recomputed each `flow_update_connection`.

**`flow_end_connection(flow_t *f, int node, const char *handle)` → `int`**
Completes the in-flight connection by adding an edge from the source to `node`/`handle` and clears connecting state. Returns the new edge id, or `-1` if the edge was rejected.

**`flow_cancel_connection(flow_t *f)` → `void`**
Aborts the in-flight connection, clearing all connecting state.

**`flow_connecting(flow_t *f)` → `int`**
Returns `1` while a connection or its preview is in flight, else `0`.

**`flow_set_connection_validator(flow_t *f, flow_connection_validator fn, void *user)` → `void`**
Installs the `isValidConnection` gate `fn` (with `user`) consulted for every add/reconnect attempt after the structural rejects. `NULL` (the default) allows all. Transient (not persisted); suspended across `flow_load`.

### Groups / subflows

Parent-relative coordinates and group-container management; absolute world positions are preserved across reparenting.

**`flow_set_parent(flow_t *f, int child, int parent)` → `void`**
Reparents `child` under `parent` (`-1` detaches to top level), rewriting `child`'s stored position so its absolute world position is unchanged. Rejects cycles (`parent==child` or `parent` a descendant of `child`) and missing ids. Journaled (reparent).

**`flow_group(flow_t *f, const int *ids, int n)` → `int`**
Creates a group container enclosing the `n` nodes named in `ids` (world bbox plus padding) and reparents each under it (absolute positions preserved). Returns the new group id, or `-1` if none of the ids are valid. Journaled as one step.

**`flow_ungroup(flow_t *f, int id)` → `void`**
Reparents every direct child of group `id` out to the group's own parent (absolute preserved), then removes the now-childless container — children survive (unlike `flow_remove_node`). Journaled.

**`flow_is_ancestor(flow_t *f, int maybe_ancestor, int node)` → `int`**
Returns `1` if `maybe_ancestor` is `node` itself or any ancestor of `node`, else `0`.

### Undo / redo

A capped inverse-op command journal; one journal command equals one user-visible undo step.

**`flow_undo(flow_t *f)` → `void`**
Pops the top command and applies its inverse; no-op if the undo stack is empty. Clears transient bits (`FLOW_SELECTED | FLOW_DRAGGING | FLOW_HOVERED`) on restored nodes; durable gates/size flags survive.

**`flow_redo(flow_t *f)` → `void`**
Re-applies the last undone command; no-op if the redo stack is empty.

**`flow_can_undo(flow_t *f)` → `int`**
Returns nonzero if at least one undo step is available.

**`flow_can_redo(flow_t *f)` → `int`**
Returns nonzero if at least one redo step is available.

**`flow_undo_depth(flow_t *f)` → `int`**
Returns the count of undo steps available (the journal depth); `0` when empty or journaling is disabled.

**`flow_redo_depth(flow_t *f)` → `int`**
Returns the count of redo steps available.

**`flow_top_op(flow_t *f)` → `int`**
Returns the `flow_cmd_kind` of the last op of the top undo command (the most recent recorded mutation), or `-1` if the undo stack is empty.

**`flow_set_undo_limit(flow_t *f, int max_commands)` → `void`**
Caps history depth in steps (default 128); evicting the oldest command frees its label copies (node `data` pointers are dropped, never freed). `0` disables journaling entirely; negatives clamp to 0.

### Viewport, zoom & coordinates

Reading the viewport, world↔screen mapping, pan, pointer-centered zoom, centering, fit-to-content, bounds queries, the optional extent clamps, and the level-of-detail tier. Pan/zoom/fit/center fire `on_viewport_change` only on an actual change and clamp zoom to `[zmin, zmax]`; none are journaled.

**`flow_view_get(flow_t *f)` → `flow_viewport`**
Returns the current viewport (pan + zoom) by value.

**`flow_to_screen(flow_t *f, flow_pt world_abs)` → `flow_pt`**
Maps an absolute world point to its screen cell under the current viewport.

**`flow_to_world(flow_t *f, flow_pt screen)` → `flow_pt`**
Maps a screen cell to its absolute world point under the current viewport.

**`flow_hit_node(flow_t *f, flow_pt screen)` → `int`**
Returns the id of the topmost node at the given `screen` cell, or `-1` if none.

**`flow_pan(flow_t *f, int dx, int dy)` → `void`**
Pans the viewport by `dx`/`dy` cells, re-clamping against the translate extent when set.

**`flow_set_zoom(flow_t *f, float zoom, flow_pt screen_center)` → `void`**
Sets the viewport zoom, clamped to `[zmin, zmax]`, while keeping the cell under `screen_center` fixed (computed in float so it stays anchored across the scale change).

**`flow_zoom_in(flow_t *f, flow_pt screen_center)` → `void`**
Multiplies the current zoom by `FLOW_ZOOM_STEP`, pointer-centered on `screen_center` (delegates to `flow_set_zoom`, so clamped).

**`flow_zoom_out(flow_t *f, flow_pt screen_center)` → `void`**
Divides the current zoom by `FLOW_ZOOM_STEP`, pointer-centered on `screen_center` (delegates to `flow_set_zoom`, so clamped).

**`flow_zoom(flow_t *f)` → `float`**
Returns the current viewport zoom.

**`flow_set_zoom_limits(flow_t *f, float zmin, float zmax)` → `void`**
Overrides the default `[zmin, zmax]` clamp. A non-positive `zmin` is replaced by `FLOW_ZOOM_MIN`; a `zmax` below `zmin` is raised to `zmin`. Re-clamps the current zoom, centered on the viewport.

**`flow_set_center(flow_t *f, int wx, int wy, float zoom)` → `void`**
Pans so world `(wx, wy)` lands on screen center. A `zoom > 0` re-zooms (clamped to `[zmin, zmax]`); a `zoom <= 0` keeps the present zoom (falling back to `1.0` when zoom is still `0`).

**`flow_fit_view(flow_t *f, int margin)` → `void`**
Picks the zoom and pan so `flow_bounds` fits with `margin` cells of padding; no-op when the graph is empty.

**`flow_fit_bounds(flow_t *f, flow_rect r, int margin)` → `void`**
Frames world rect `r` with `margin` cells of padding; no-op when `r.w<=0` or `r.h<=0`.

**`flow_bounds(flow_t *f)` → `flow_rect`**
Returns the union rect of all visible node rects (hidden nodes are view-skipped); a zero rect when the graph is empty.

**`flow_bounds_of(flow_t *f, const int *ids, int n)` → `flow_rect`**
Returns the union of the absolute rects of the `n` nodes named in `ids` — including hidden nodes (an explicit-id model-level query, unlike `flow_bounds`). Missing ids are skipped; a zero rect on `n<=0`, `NULL` ids, or when none resolve.

**`flow_set_node_extent(flow_t *f, flow_rect world)` → `void`**
Sets the node-extent clamp so future `flow_move_node` targets keep the node rect inside `world` (flushing to the exceeded edge). Future moves only; `w<=0` or `h<=0` disables it (the default). Transient.

**`flow_set_translate_extent(flow_t *f, flow_rect world)` → `void`**
Sets the translate-extent clamp so pan/zoom/fit keep the visible world window inside `world`, re-clamping the current view immediately. `w<=0`/`h<=0` disables it (the default). Transient.

**`flow_lod_for_zoom(flow_t *f, float zoom)` → `int`**
Returns the level-of-detail tier for `zoom`: `0` = full, `1` = collapsed.

### Theming & color modes

The engine-chrome color preset (default / light / dark).

**`flow_set_color_mode(flow_t *f, flow_color_mode mode)` → `void`**
Re-seeds the engine theme tokens from a fixed preset for `mode` (`FLOW_COLOR_DEFAULT` reproduces the legacy fg 7 / bg 0 / grid 8 literals byte-for-byte). Transient view-state: never saved, never journaled.

**`flow_color_mode_get(flow_t *f)` → `flow_color_mode`**
Returns the last color mode set (calloc-zero default is `FLOW_COLOR_DEFAULT`).

### Chrome (minimap / controls / toolbars / background / status bar / resizer)

Opt-in built-in widgets and the whole-canvas lock; all are transient chrome — never saved, never journaled.

**`flow_set_statusbar(flow_t *f, int enabled)` → `void`**
Toggles the built-in bottom help/status line.

**`flow_set_minimap(flow_t *f, int enabled, flow_corner corner, int w, int h)` → `void`**
Enables/disables the minimap widget and sets its corner anchor and `w`×`h` size.

**`flow_set_background(flow_t *f, flow_bg_variant variant, int gap)` → `void`**
Sets the world-aligned background grid `variant` (dots/lines/cross) and cell `gap`; pass `FLOW_BG_NONE` to disable (the default).

**`flow_set_controls(flow_t *f, int enabled, flow_corner corner)` → `void`**
Enables/disables the corner-anchored Controls bar (`[+][-][fit][lock]`) and sets its `corner`. Off by default.

**`flow_set_locked(flow_t *f, int on)` → `void`**
Sets the whole-canvas lock: when on, node drag/connect/reconnect/marquee/click-select arming is suppressed (pan and zoom still work). Transient.

**`flow_locked(flow_t *f)` → `int`**
Returns nonzero while the canvas is locked.

**`flow_set_node_toolbar(flow_t *f, const flow_toolbar_action *actions, int n)` → `void`**
Installs a selection-anchored node-toolbar action strip (drawn above the single selected node). The `actions` array is borrowed (no copy) and must outlive its installation; `NULL`/`0` disarms.

**`flow_set_edge_toolbar(flow_t *f, const flow_toolbar_action *actions, int n)` → `void`**
Installs a floating edge-toolbar action bar on the single selected edge (anchored above the route midpoint). The `actions` array is borrowed and must outlive its installation; `NULL`/`0` disarms.

**`flow_set_resizer(flow_t *f, int enabled)` → `void`**
Enables/disables the SE-corner resize grip drawn on the lone selected node (at LOD 0, while unlocked). Off by default; dragging the grip resizes via the journaled `flow_set_node_size` rail (one undo step).

**`flow_set_helper_lines(flow_t *f, int on)` → `void`**
Enables/disables alignment helper lines and snap-to-guide during a single-node drag (off by default — with `on==0` the drag path is byte-for-byte the unsnapped behavior). Guides clear on release. Transient.

### Per-node interaction gates, hidden / animated flags, explicit size

Durable per-element configuration toggles plus the view-level hidden flag and the per-edge animation flag.

**`flow_set_node_hidden(flow_t *f, int id, int hidden)` → `void`**
Sets/clears `FLOW_HIDDEN` on node `id` (view-level: skipped by render, hit-test, marquee, bounds/fit, minimap; still seen by traversal/serialize). Hiding a selected node deselects it. Not journaled, not persisted.

**`flow_set_edge_hidden(flow_t *f, int id, int hidden)` → `void`**
Sets/clears `FLOW_HIDDEN` on edge `id` (skipped by render and hit-test; an edge with a hidden endpoint cascades to hidden too). Not journaled, not persisted.

**`flow_set_edge_animated(flow_t *f, int id, int on)` → `void`**
Sets (`on!=0`) or clears (`on==0`) `FLOW_ANIMATED` (marching-ants) on edge `id`; no-op on unknown id. Arms the redraw clock. Not journaled, not persisted (re-arm after `flow_load`).

**`flow_set_node_draggable(flow_t *f, int id, int on)` → `void`**
`on==0` sets `FLOW_NODRAG` (node held fixed under user drag and shift-arrow nudge); `on!=0` clears it. No-op on unknown id. Gates user interaction only — programmatic moves are unaffected. Not journaled, but the bit is undo-durable and persisted.

**`flow_set_node_selectable(flow_t *f, int id, int on)` → `void`**
`on==0` sets `FLOW_NOSELECT` (not selectable by pointer/keyboard; the public select API stays open); `on!=0` clears it. No-op on unknown id. Bit is undo-durable and persisted.

**`flow_set_node_deletable(flow_t *f, int id, int on)` → `void`**
`on==0` sets `FLOW_NODELETE` (survives delete-selection as a root; direct remove + cascade unaffected); `on!=0` clears it. No-op on unknown id. Bit is undo-durable and persisted.

**`flow_set_node_size(flow_t *f, int id, int w, int h)` → `void`**
Sets an explicit node size (setting `FLOW_EXPLICIT_SIZE`, which skips auto-measure) that persists across save/load. `w`/`h` clamp to `>= 1`; no-op on unknown id. Journaled (`FLOW_CMD_RESIZE_NODE`, coalescing within an open transaction) so a resize gesture is one undo step.

### Selection clipboard & marquee

In-process copy/cut/paste/duplicate over the selection set, plus rectangular selection and the marquee default mode.

**`flow_copy_selection(flow_t *f)` → `void`**
Deep-snapshots the selected nodes plus intra-selection edges into the clipboard (no graph change, no callbacks). Node `data` is aliased (borrowed); edge labels are dup'd. Resets the paste-offset generation.

**`flow_cut_selection(flow_t *f)` → `void`**
Equivalent to `flow_copy_selection` followed by `flow_delete_selection`.

**`flow_paste(flow_t *f)` → `int`**
Re-mints the clipboard contents with fresh ids at a growing cumulative offset as one undo step, reparenting pasted children whose original parent was also pasted, restoring edge type + label, and selecting the result. Returns the number of nodes pasted (`0` if the clipboard is empty). Pasted nodes are re-measured; pasted edges remain validator-gated (a violating edge is dropped).

**`flow_duplicate_selection(flow_t *f)` → `int`**
Snapshots and pastes the current selection in one shot at a `+1,+1` offset, leaving the clipboard untouched. Returns the number of nodes added.

**`flow_select_in_rect(flow_t *f, flow_rect world, flow_select_mode mode, int additive)` → `int`**
Selects nodes contained (`FLOW_SELECT_FULL`) or intersecting (`FLOW_SELECT_PARTIAL`) the `world` rect; `!additive` clears the selection first. Returns the count selected.

**`flow_set_marquee_mode(flow_t *f, flow_select_mode mode)` → `void`**
Sets the default selection mode for the shift-drag marquee (defaults to `FLOW_SELECT_PARTIAL`).

### Callbacks & key bindings

The library/app seam: lifecycle/interaction callbacks, the pre-dispatch key hook, and the keyboard command registry/built-ins.

**`flow_set_callbacks(flow_t *f, flow_callbacks cb)` → `void`**
Installs the callback struct `cb` (by value) carrying the app's observers and `user` pointer.

**`flow_set_key_hook(flow_t *f, flow_key_hook fn, void *user)` → `void`**
Installs a pre-dispatch key hook `fn` (with `user`) called at the top of `flow_dispatch_key` with the raw byte window; it returns bytes consumed (0 = pass-through). `NULL` (the default) means no hook. Transient.

**`flow_set_key_hook_modal(flow_t *f, int on)` → `void`**
Sets modal key capture: when on (and a hook is installed), a key sequence the hook does not consume is dropped rather than falling through to bindings/built-ins. Inert with no hook installed; default off (input byte-identical to unmodal). Transient.

**`flow_bind_key(flow_t *f, const char *seq, flow_key_fn fn, void *user)` → `void`**
Registers/overrides a key binding for `seq` (matched before built-ins, longest-sequence first). Up to 32 bindings, `seq` `<= 7` bytes; over-limit binds are silently ignored.

**`flow_dispatch_key(flow_t *f, const char *seq, int n)` → `int`**
Runs a binding or built-in for one key sequence and returns the bytes consumed (`>0`), or `0` if unhandled (when key-hook modal is set with a hook installed, an unconsumed sequence returns its dropped length `>= 1` instead of `0`).

**`flow_delete_selection(flow_t *f)` → `void`**
Built-in: removes the selected node(s), then the selected edge. Journaled as one step.

**`flow_add_node_center(flow_t *f, const char *type, void *data)` → `int`**
Adds a node of `type` at the world point under the viewport center. Returns the new node id (or `-1` on OOM, like `flow_add_node`). `data` is borrowed. Journaled as one step.

### Animation clock

A deterministic tick-based redraw clock — the only animation clock; wall-clock never enters model/render.

**`flow_tick(flow_t *f)` → `void`**
Advances the animation clock (`++tick`). Does no IO, render, or `time()` — the testable seam.

**`flow_ticks(flow_t *f)` → `unsigned`**
Returns the current tick value (consumers derive dash phase as `tick % period`).

**`flow_set_tick_ms(flow_t *f, int ms)` → `void`**
Sets the redraw interval (ms) used when frames are armed (default 100); `<= 0` clamps to 1 (never 0, so it never busy-spins). Transient.

### Persistence (save / load)

JSON round-trip of the whole graph (implemented in `flow_json.h`).

**`flow_save(flow_t *f, const char *path)` → `int`**
Writes the graph as JSON to `path`. Returns `0` on success, `-1` on an I/O or encode error.

**`flow_load(flow_t *f, const char *path)` → `int`**
Resets `f` then rebuilds the graph from the JSON at `path`. Returns `0` on success, `-1` on an open or parse error. Clears the undo journal and suspends the connection validator across the rebuild. Hidden/animated flags reload as 0 (re-arm afterward); node `data` is not restored by flow.

### Edge hit-testing & endpoints

Edge geometry helpers (defined in `flow_render.h`, declared here because they need the render anchor helpers).

**`flow_hit_edge(flow_t *f, flow_pt screen, int tol)` → `int`**
Returns the topmost edge whose routed path passes within Chebyshev distance `tol` of `screen`, or `-1` if none.

**`flow_edge_endpoint_screen(flow_t *f, const flow_edge *e, int which, flow_pt *out)` → `int`**
Writes the screen cell of edge `e`'s source (`which=0`) or target (`which=1`) endpoint into `out`, matching the renderer exactly. Returns `1` on success, `0` otherwise.

---

## Type registration — custom nodes & edges (vtables)

### Registering the built-in types

Convenience entry point that installs flow's built-in node and edge vtables in one call.

**`flow_register_defaults(flow_t *f)` → `void`**
Registers the built-in node types `"default"` and `"group"`, and the built-in edge types `"default"` and `"straight"`, on `f` (by calling `flow_register_node_type`/`flow_register_edge_type` for each). The registered `flow_node_type`/`flow_edge_type` vtables are borrowed static structs — flow stores the pointers and never frees them. No return value.

---

## Queries & hit-testing

All functions here are MODEL-level, read-only queries over `f->edges` / `f->nodes` — no view-state filtering (hidden nodes/edges are included), no engine state, and no allocation. They share a fill-buffer idiom: the TRUE total count is returned, while only `out[0..min(count,max)-1]` is written (`out` may be `NULL` with `max` 0 for a count-only query). Emitted ids follow insertion order.

### Graph traversal

Walk `f->edges` to find neighbors or incident edges of a node; a missing node id returns 0 with `out` untouched.

**`flow_incomers(flow_t *f, int node, int *out, int max)` → `int`**
Fills `out` with the distinct source nodes of edges pointing TO `node` (multi-edges collapse to one entry). Returns the total count of such sources; if `node` is not found, returns 0. No allocation; `out` may be `NULL` with `max` 0 for count-only.

**`flow_outgoers(flow_t *f, int node, int *out, int max)` → `int`**
Fills `out` with the distinct target nodes reachable FROM `node` (same dedup as `flow_incomers`). Returns the total count; if `node` is not found, returns 0. No allocation; `out` may be `NULL` with `max` 0 for count-only.

**`flow_connected_edges(flow_t *f, int node, int *out, int max)` → `int`**
Fills `out` with every edge id incident on `node` (either endpoint, no dedup). Returns the total count; if `node` is not found, returns 0. No allocation; `out` may be `NULL` with `max` 0 for count-only.

### Spatial hit-tests

Sweep all nodes by their absolute rect (`flow_node_rect_abs`), using a closed "touching edges count" intersection convention. Ancestor pairs are not filtered.

**`flow_intersecting_nodes(flow_t *f, flow_rect world, int *out, int max)` → `int`**
Fills `out` with every node whose absolute rect intersects `world`. Returns the total count. No allocation; `out` may be `NULL` with `max` 0 for count-only.

**`flow_node_intersections(flow_t *f, int node, int *out, int max)` → `int`**
Fills `out` with the nodes intersecting `node`'s absolute rect, EXCLUDING `node` itself. Returns the total count; if `node` is not found, returns 0. No allocation; `out` may be `NULL` with `max` 0 for count-only.

### Label search

Case-insensitive (ASCII) substring match over the optional `label()` vtable accessor.

**`flow_find_nodes(flow_t *f, const char *needle, int *out, int max)` → `int`**
Fills `out` with the nodes whose label contains `needle` as a case-insensitive substring. An empty `needle` (`""`) matches every labeled node; a type whose `label` is `NULL` never matches. Returns the total match count, or 0 if `needle` is `NULL`. No allocation; `out` may be `NULL` with `max` 0 for count-only.

---

## Geometry helpers

Pure coordinate math with no I/O. Covers world/screen projection through a viewport and axis-aligned rectangle predicates and unions.

### Points and viewport projection

Convert between world coordinates and screen coordinates through a `flow_viewport` (offset + zoom).

**`flow_project(flow_viewport v, flow_pt world)` → `flow_pt`**
Projects a world-space point to screen space by `world * v.zoom + v.offset`, rounding each axis to the nearest integer.

**`flow_unproject(flow_viewport v, flow_pt screen)` → `flow_pt`**
Inverse of `flow_project`: maps a screen-space point back to world space, rounding each axis to the nearest integer. A zero `v.zoom` is treated as `1.0f` to avoid division by zero.

### Rectangles

Axis-aligned `flow_rect` predicates and combination. Containment and intersection use half-open/closed conventions noted per function.

**`flow_rect_contains(flow_rect r, flow_pt p)` → `int`**
Returns nonzero if point `p` lies inside `r`. The rect is treated as half-open: the left/top edges are included, the right/bottom edges (`r.x + r.w`, `r.y + r.h`) are excluded.

**`flow_rect_intersects(flow_rect a, flow_rect b)` → `int`**
Returns nonzero if `a` and `b` overlap. Uses a closed convention: touching edges count as overlap.

**`flow_rect_union(flow_rect a, flow_rect b)` → `flow_rect`**
Returns the smallest axis-aligned rectangle enclosing both `a` and `b`.

---

## Auto-layout

Auto-layout is a stateless transform from the graph model to node positions: it re-measures every node, computes new coordinates, and commits them through `flow_move_node` (the sole position-write path). The library holds no engine state between calls. The whole commit is bracketed as exactly one undo step, and grouped nodes are laid out per-parent partition so a container settles before its children commit.

### Force-directed and layered layout

The primary entry takes a fully-defaulted options struct (zero-init means force mode with every parameter defaulted); the two wrappers are spec-literal shapes over it.

**`flow_layout(flow_t *f, flow_layout_opts opts)` → `void`**
Arranges the graph in place, writing every node's new position through `flow_move_node`. Re-measures all nodes first, then lays out each parent partition (force-directed when `opts.mode` is `FLOW_LAYOUT_FORCE`, layered/Sugiyama-style when `FLOW_LAYOUT_LAYERED`). No-op (returns immediately) when the graph has 0 or 1 nodes; single-node partitions are left untouched. The entire layout is one undo step. When `opts.fit_after` is nonzero, calls `flow_fit_view(f, opts.margin)` afterward; otherwise the viewport is untouched. Deterministic and RNG-free on the default path. Does not free or take ownership of any user data.

**`flow_layout_force(flow_t *f, flow_force_opts opts)` → `void`**
Thin wrapper that runs `flow_layout` in force-directed (`FLOW_LAYOUT_FORCE`) mode, forwarding `opts.iterations`, `opts.k`, and `opts.gravity`. All other layout options default. Same no-op, undo, and ownership behavior as `flow_layout`.

**`flow_layout_layered(flow_t *f, flow_layered_dir dir, int gap_x, int gap_y)` → `void`**
Thin wrapper that runs `flow_layout` in layered (`FLOW_LAYOUT_LAYERED`) mode with the given `dir` (`FLOW_LR` or `FLOW_TB`). `gap_x` / `gap_y` are **axis-bound** spacings in cells — horizontal (X) and vertical (Y) respectively — so the inter-node vs inter-rank roles depend on `dir`: with `FLOW_TB`, `gap_x` separates sibling nodes within a rank and `gap_y` separates ranks; with `FLOW_LR` the roles swap (`gap_y` within a rank, `gap_x` between ranks). Each `<= 0` defaults to `4`. All other layout options default. Same no-op, undo, and ownership behavior as `flow_layout`.

---

## Edge routing

An edge router takes the two endpoints (world points `s`, `t` plus their handle sides `sp`, `tp`) and appends connectivity-glyph cells to a `flow_route *out`; the caller owns and frees `out->cells`. Each router also sets `out->label_anchor` and terminates the run with a directional arrowhead at the target end.

### Route buffer

The shared sink that routers push cells into.

**`flow_route_push(flow_route *r, int x, int y, uint32_t ch)` → `void`**
Appends a cell `(x, y, ch)` to `r`, growing `r->cells` (doubling, starting at cap 16) when full. On allocation failure the old buffer is kept and the cell is silently dropped (the route truncates) rather than leaking or NULL-dereferencing — so `r->count` may end up short of the requested cells under OOM.

### Routers

The two built-in routing strategies; both ignore `sp`/`tp`, append cells via `flow_route_push`, set `out->label_anchor`, and place an arrowhead glyph on the final cell pointing along the approach direction.

**`flow_route_orthogonal(flow_pt s, flow_pos sp, flow_pt t, flow_pos tp, flow_route *out)` → `void`**
Routes an orthogonal (axis-aligned) step path from `s` to `t` — a two-corner Z/step route bending at the mid column or mid row (the middle leg degenerates to a point when `s` and `t` share a row or column): H-V-H at the mid column when the run is at least as wide as tall (`adx >= ady`), otherwise V-H-V at the mid row — emitting box-drawing connectivity glyphs. Sets `out->label_anchor` to the middle path point. Frees its internal scratch buffer; the caller frees `out->cells`. The `sp` and `tp` arguments are unused.

**`flow_route_straight(flow_pt s, flow_pos sp, flow_pt t, flow_pos tp, flow_route *out)` → `void`**
Routes a direct integer-DDA line from `s` to `t`, stepping the dominant axis one cell per step (diagonal steps allowed), with glyphs `─` horizontal, `│` vertical, and `╲`/`╱` diagonal. A degenerate `s == t` emits exactly one cell. Sets `out->label_anchor` to the midpoint of `s` and `t`. Same heap/route-cell ownership as `flow_route_orthogonal` — the caller frees `out->cells`. The `sp` and `tp` arguments are unused.

### Predefined edge types

Two `extern const flow_edge_type` vtables pair a type name with one of the routers above, ready to register as edge types. `flow_default_edge_type` is `{ "default", flow_route_orthogonal }`; `flow_straight_edge_type` is `{ "straight", flow_route_straight }`.

---

## Rendering

### Composing the model to a cell buffer

Renders the whole editor scene into a caller-provided cell buffer for the host to diff and flush.

**`flow_render(flow_t *f, flow_cell *out, int cols, int rows)` → `void`**
Composes the full scene into `out` (a `cols`×`rows` cell array): clears to the theme, then paints the background grid, edges (under nodes), nodes in depth-aware order, handle markers, the focus ring, the in-flight connection preview, the marquee box, alignment guides, the minimap, the resizer grip, controls, node/edge toolbars, the app `on_overlay` callback, and the status bar. `out` is borrowed and must be at least `cols * rows` cells — flow writes into it but never frees it. Resets and refills the widget hit-rect cache (`f->nwidgets`) each call so subsequent hit-tests match exactly what was drawn. Not journaled.

---

## Cell buffer primitives

### UTF-8 codec

Encode and decode single Unicode code points to/from UTF-8 for the cell buffer's per-cell `uint32_t` characters.

**`flow_utf8(uint32_t cp, char out[5])` → `int`**
Encodes code point `cp` into `out` as a NUL-terminated UTF-8 sequence. Returns the byte length written (1–4); `out` must hold at least 5 bytes (4 + NUL). Caller owns `out`; no allocation.

**`flow_utf8_decode(const char *s, uint32_t *cp)` → `int`**
Decodes the leading UTF-8 sequence at `s` into `*cp` and returns the number of bytes consumed (1–4). Continuation checks short-circuit on a missing trailing byte, so a truncated sequence stops at the NUL rather than over-reading; an invalid lead byte (or any malformed continuation) yields the raw byte as `*cp` and returns 1.

### Cell buffer put/clear

Write directly into a raw `flow_cellbuf` grid — bounds-checked but with no surface clip-rect.

**`flow_cellbuf_clear(flow_cellbuf *cb, uint8_t fg, uint8_t bg)` → `void`**
Fills every cell of `cb` with a space glyph, the given `fg`/`bg`, and zero attributes. No allocation; operates in place.

**`flow_cellbuf_put(flow_cellbuf *cb, int x, int y, uint32_t ch, uint8_t fg, uint8_t bg, uint8_t attr)` → `void`**
Writes glyph `ch` with `fg`/`bg`/`attr` at buffer cell `(x,y)`. No-op if `(x,y)` is outside `cb`'s `w`×`h` bounds. Operates in place; no allocation.

### Surface drawing

Clipped drawing through a `flow_surface`: each write applies the surface's logical (`w`/`h`), clip-rect (`clip_*`), and physical buffer bounds before touching the cell.

**`flow_put(flow_surface *s, int x, int y, uint32_t ch, uint8_t fg, uint8_t bg, uint8_t attr)` → `void`**
Writes glyph `ch` with `fg`/`bg`/`attr` at surface-local `(x,y)`. No-op if `(x,y)` falls outside the surface's logical box, the clip rect, or the underlying buffer. Operates in place.

**`flow_text(flow_surface *s, int x, int y, const char *utf8, uint8_t fg, uint8_t bg, uint8_t attr)` → `void`**
Decodes the `utf8` string and draws it left-to-right starting at `(x,y)`, advancing one cell per code point. Each cell goes through `flow_put`, so off-surface glyphs are dropped. Operates in place; no allocation.

**`flow_box(flow_surface *s, int x, int y, int w, int h, uint8_t fg, uint8_t bg, unsigned style)` → `void`**
Draws a box-drawing-character rectangle of size `w`×`h` at `(x,y)`; `style & FLOW_BOLD` sets the bold attribute on the border cells. No-op if `w < 2` or `h < 2`. Edges are clipped via `flow_put`; operates in place.

### Surface dimensions

Read the surface's logical drawing extent.

**`flow_surface_w(const flow_surface *s)` → `int`**
Returns the surface's logical width (`s->w`).

**`flow_surface_h(const flow_surface *s)` → `int`**
Returns the surface's logical height (`s->h`).

### Damage diff

Serialize the difference between two cell buffers into a terminal escape sequence.

**`flow_diff_emit(const flow_cell *front, const flow_cell *back, int cols, int rows)` → `char *`**
Compares `back` against `front` over a `cols`×`rows` grid and returns a newly allocated, NUL-terminated string of ANSI escapes that repaints only the changed cells (cursor moves plus `\x1b[0`-reset style runs carrying BOLD/REVERSE and 256-color fg/bg). Caller owns the returned buffer and must free it. An all-equal diff returns an empty (but non-NULL) string.

---

## Input feed

The model is driven entirely by mouse events: the host parses raw terminal bytes into a `flow_mouse_event` (or synthesizes one), then feeds it to the interaction handler. The two functions split that pipeline into a pure byte parser and the stateful dispatcher.

### Parsing terminal input

Decode one SGR-1006 mouse escape sequence into a structured event without touching the model.

**`flow_parse_mouse(const char *s, int n, flow_mouse_event *ev)` → `int`**
Parses one SGR-1006 mouse sequence at `s` (length `n`) into `*ev`. Returns the number of bytes consumed on a complete sequence, or `0` if `s` is not a complete mouse sequence (too short, wrong `\x1b[<` prefix, an unexpected byte, or an incomplete tail) — in which case the caller should treat the bytes otherwise and `*ev` is not filled. Pure: reads only the input buffer; touches no `flow_t`.

### Feeding events to the model

Apply a parsed or synthetic mouse event, advancing all interaction state (hover, drag, pan, zoom, selection, marquee, connect, reconnect, resize) and firing the registered callbacks.

**`flow_handle_mouse(flow_t *f, const flow_mouse_event *ev)` → `void`**
Dispatches one mouse event into `f`, driving the press/motion/release interaction state machine: wheel pans (or Ctrl+wheel zooms about the pointer), presses arm widgets/handles/reconnect/resize/node-drag/marquee, motion moves/pans/marquees, and release classifies the gesture as a click, drag-to-reparent, or connection completion. Borrows `ev` (read-only); it is not retained. Gesture-scoped mutations (node moves, resizes, reconnects, reparenting) are journaled as one undo step per gesture; observer callbacks (`on_node_click`, `on_pane_click`, `on_node_context`, `on_connect_end`, and so on) fire synchronously during dispatch.

---

## Terminal control (POSIX)

These entry points are POSIX-only (termios, ioctl, signals) and drive the interactive terminal backend; the headless embed path never calls them.

### Raw mode and alt-screen

Enter and leave the interactive terminal state — raw mode plus the alternate screen and mouse reporting.

**`flow_term_setup(void)` → `void`**
Saves the current terminal attributes, disables `ECHO` and `ICANON` (raw mode), then switches to the alternate screen, clears it, hides the cursor, and enables SGR mouse reporting (click, drag, wheel). Registers an `atexit` hook (once) and installs restore-on-signal handlers so a fatal signal or an `exit()` mid-run still restores the terminal; deliberately-ignored signals (e.g. under `nohup`) are left alone. Marks the terminal as owned until restore.

### Restore

Reverse the setup, returning the terminal to its saved state.

**`flow_term_restore(void)` → `void`**
Flushes pending output, emits the byte-exact inverse of `flow_term_setup`'s sequences (disable mouse modes, reset SGR, show cursor, leave the alternate screen), and restores the saved terminal attributes, then removes the signal handlers. Restores the terminal before disarming so a signal racing teardown still cleans up. Idempotent with respect to the active flag.

### Terminal size

Query the current terminal dimensions.

**`flow_term_size(int *cols, int *rows)` → `int`**
Writes the terminal width and height into `*cols` and `*rows`. Returns `0` on success, or `-1` if the `TIOCGWINSZ` ioctl fails or reports zero columns (in which case `*cols`/`*rows` are not written).

---

## Run loop & presentation

### Present & diff-flush

The frame-output primitives: render the current model, diff against the previous frame, and emit the escape string either back to the host or straight to stdout.

**`flow_render_diff(flow_t *f)` → `char *`**
Renders the current model, diffs it against the previous frame, advances the front buffer, and returns a malloc'd absolute-positioned CSI/SGR escape string. The caller owns the returned string and must free it. Returns `""` (empty but non-NULL) when nothing changed. No stdout/termios coupling — for host-owned loops where the host writes the string to its own fd and frees it.

**`flow_present(flow_t *f)` → `void`**
The stdout-bound convenience over `flow_render_diff`: renders the diff, writes it to stdout, flushes, and frees the escape string internally.

### The embedder seam

The input seam: hand raw terminal bytes to flow without running its loop, so a host loop can own the fd and clock.

**`flow_feed(flow_t *f, const char *b, int n)` → `void`**
Parses `n` bytes of raw terminal input from `b` and dispatches them: SGR mouse events, registered/built-in keys, arrow-key pan, Shift-Tab focus, Shift-arrow selection nudge (one undo step), `+`/`-` zoom, and lone ESC (cancels in-flight connection, exits space-pan, clears selection and keyboard focus). User key bindings registered via the key registry take precedence over the built-in arrow/zoom handling.

### Blocking run loop

The all-in-one driver for hosts that want flow to own the terminal and the event loop.

**`flow_run(flow_t *f)` → `void`**
Takes over the terminal: queries terminal size and resizes, sets up the terminal, marks the model running, and presents the first frame. Then blocks in a `poll`-driven loop — waking on input (fed through `flow_feed` and re-presented) or, while frames are armed (in-flight drag/autopan or any `FLOW_ANIMATED` edge), on a `tick_ms` timeout that advances the clock, replays autopan, and redraws. Idle state blocks forever with zero wakeups. `EINTR` (e.g. SIGWINCH/SIGCONT) is retried rather than treated as an exit. Restores the terminal on return.

---

## Alphabetical index

All 157 public functions:

`flow_add_edge`, `flow_add_node`, `flow_add_node_center`, `flow_begin_connection`, `flow_bind_key`, `flow_bounds`, `flow_bounds_of`, `flow_box`, `flow_can_redo`, `flow_can_undo`, `flow_cancel_connection`, `flow_cellbuf_clear`, `flow_cellbuf_put`, `flow_clear_selection`, `flow_color_mode_get`, `flow_connected_edges`, `flow_connecting`, `flow_connection_valid`, `flow_copy_selection`, `flow_cut_selection`, `flow_delete_selection`, `flow_diff_emit`, `flow_dispatch_key`, `flow_duplicate_selection`, `flow_edge_count`, `flow_edge_endpoint_screen`, `flow_edge_type_for`, `flow_edges`, `flow_end_connection`, `flow_feed`, `flow_find_nodes`, `flow_fit_bounds`, `flow_fit_view`, `flow_focus_next`, `flow_focus_prev`, `flow_focused_node`, `flow_free`, `flow_get_edge`, `flow_get_node`, `flow_group`, `flow_handle_anchor`, `flow_handle_mouse`, `flow_hit_edge`, `flow_hit_handle`, `flow_hit_node`, `flow_hovered_node`, `flow_incomers`, `flow_intersecting_nodes`, `flow_is_ancestor`, `flow_layout`, `flow_layout_force`, `flow_layout_layered`, `flow_load`, `flow_locked`, `flow_lod_for_zoom`, `flow_measure_node`, `flow_move_node`, `flow_new`, `flow_node_abs`, `flow_node_count`, `flow_node_handle_at`, `flow_node_handle_count`, `flow_node_intersections`, `flow_node_pos`, `flow_node_rect_abs`, `flow_node_type_for`, `flow_nodes`, `flow_outgoers`, `flow_pan`, `flow_parse_mouse`, `flow_paste`, `flow_present`, `flow_project`, `flow_put`, `flow_reconnect_edge`, `flow_rect_contains`, `flow_rect_intersects`, `flow_rect_union`, `flow_redo`, `flow_redo_depth`, `flow_register_defaults`, `flow_register_edge_type`, `flow_register_node_type`, `flow_remove_edge`, `flow_remove_node`, `flow_render`, `flow_render_diff`, `flow_resize`, `flow_route_orthogonal`, `flow_route_push`, `flow_route_straight`, `flow_run`, `flow_save`, `flow_select_edge`, `flow_select_in_rect`, `flow_select_node`, `flow_selected_count`, `flow_selected_edge`, `flow_selected_node`, `flow_selected_nodes`, `flow_set_autopan`, `flow_set_background`, `flow_set_callbacks`, `flow_set_center`, `flow_set_color_mode`, `flow_set_connection_validator`, `flow_set_controls`, `flow_set_edge_animated`, `flow_set_edge_hidden`, `flow_set_edge_label`, `flow_set_edge_toolbar`, `flow_set_focus`, `flow_set_helper_lines`, `flow_set_hover`, `flow_set_key_hook`, `flow_set_key_hook_modal`, `flow_set_locked`, `flow_set_marquee_mode`, `flow_set_minimap`, `flow_set_node_deletable`, `flow_set_node_draggable`, `flow_set_node_extent`, `flow_set_node_hidden`, `flow_set_node_selectable`, `flow_set_node_size`, `flow_set_node_toolbar`, `flow_set_parent`, `flow_set_resizer`, `flow_set_statusbar`, `flow_set_tick_ms`, `flow_set_translate_extent`, `flow_set_undo_limit`, `flow_set_zoom`, `flow_set_zoom_limits`, `flow_surface_h`, `flow_surface_w`, `flow_term_restore`, `flow_term_setup`, `flow_term_size`, `flow_text`, `flow_tick`, `flow_ticks`, `flow_to_screen`, `flow_to_world`, `flow_toggle_node`, `flow_top_op`, `flow_undo`, `flow_undo_depth`, `flow_ungroup`, `flow_unproject`, `flow_update_connection`, `flow_utf8`, `flow_utf8_decode`, `flow_view_get`, `flow_zoom`, `flow_zoom_in`, `flow_zoom_out`.
