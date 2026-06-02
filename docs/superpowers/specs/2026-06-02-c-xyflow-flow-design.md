# `flow` — a C-based xyflow

_Design spec · 2026-06-02_

## Overview

`flow` is a **single-header C library** for building interactive node-graph editors in
the terminal — the C analog of [xyflow](https://github.com/xyflow/xyflow) (React Flow).
Consumer code supplies nodes, edges, and custom node/edge **renderers**; the library owns
the canvas, the viewport (pan + zoom), hit-testing, selection, dragging, connecting,
sub-flows, undo/redo, auto-layout, and the cell-accurate terminal renderer.

This supersedes the earlier `2026-05-31-tui-graph-editor-design.md`, which described a
single application. That application — a **network-topology editor** — survives here as the
flagship *demo* built on top of `flow`, which is how we prove the API is genuinely reusable.

### Goals

- A reusable engine, not an app. The network editor is one consumer; `hello_flow.c` and a
  `flowchart.c` demo are others.
- Faithful to xyflow's model and interaction vocabulary, adapted honestly to a character grid.
- Self-contained: one header, no external dependencies, no ncurses — pure ANSI escapes.
- A **pure, headless-testable core**: geometry, model, routing, and layout never touch the
  terminal, so they unit-test without a TTY; rendering targets an in-memory cell buffer, so
  it **snapshot-tests** without a TTY.

### Non-goals / honest terminal limits

- **Sub-cell rendering.** A glyph cannot be smaller than one cell. Zoom is therefore
  expressed as inter-node spacing + level-of-detail (see §7), not smooth magnification, and
  curved edges (`bezier`/`smoothstep`) collapse to box-drawing corners.
- **Animation smoothness** beyond the cell grid (e.g. eased zoom tweens) is out of scope.

---

## 1. Architecture: the xyflow split, ported

xyflow is a monorepo: `@xyflow/system` (framework-agnostic core) + `@xyflow/react` (bindings).
The research that grounds this spec found that the *system* core is itself **two layers**:

1. **Pure arithmetic** (zero DOM): `getNodesBounds`, `pointToRendererPoint`,
   `getViewportForBounds`, `snapPosition`, `clamp`. → ports to C **verbatim**.
2. **"Math + an event source"** (`XYDrag`, `XYPanZoom`, `XYHandle`, `XYMinimap`,
   `XYResizer`): geometry math wired to d3 wheel/drag/pointer handlers. The only
   framework-specific part is the event plumbing. → in C we keep the math and replace d3
   with our terminal mouse/key decode.

`flow` mirrors this exactly:

```
┌─────────────────────────────────────────────────────────────────────┐
│ flow.h  (the single-header artifact, amalgamated from src/)          │
│                                                                       │
│  PURE CORE (no terminal I/O — unit-testable)                          │
│    geometry/transform · model+mutators · hit-test · bounds ·          │
│    edge routing · auto-layout · undo/redo                             │
│                                                                       │
│  COMPOSITOR (targets a cell buffer — snapshot-testable)               │
│    flow_cell grid · flow_surface · z-ordered compose · damage diff    │
│                                                                       │
│  TERMINAL LAYER (the only part that needs a TTY)                      │
│    raw-mode + SGR mouse/key decode (vendored from tuibox) ·           │
│    interaction state machine · flow_run / flow_present                │
└─────────────────────────────────────────────────────────────────────┘
        ▲ consumed by
   demos/topo.c · demos/hello_flow.c · demos/flowchart.c
```

The core simplification, confirmed against reactflow.dev: React's **controlled-state +
change-descriptor** contract (`onNodesChange`/`applyNodeChanges`) exists only because React
state is immutable and owned outside the library. In C we own one store and mutate it
directly. We keep only the *discipline* — every change goes through a small set of mutator
functions so **rendering is always a pure read of the store**. The change vocabulary returns,
deliberately, as the **undo/redo command layer** (§11).

---

## 2. Source layout & packaging

`flow` ships as a single header but is **developed modularly** (the brainstorming principle:
small, focused, independently understandable units). A trivial amalgamation step concatenates
`src/` into the distributable `flow.h`, which is the build product committed for consumers.

```
flow.h                  ← BUILD PRODUCT: amalgamated single header (committed)
src/
  flow_vec.h            ← rxi/vec dynamic arrays (vendored, MIT)
  flow_term.h           ← termios raw mode + SGR mouse/key decode (vendored from tuibox, MIT)
  flow_geom.h           ← pure: flow_pt/rect, transform, bounds, clamp, snap, hit-test math
  flow_cell.h           ← cell buffer, flow_surface, compose, damage diff
  flow_model.h          ← flow_t store, node/edge/handle/group, mutators, accessors
  flow_types.h          ← node/edge vtable registries + default node & edge types
  flow_route.h          ← orthogonal + straight edge routers, arrowheads, labels
  flow_view.h           ← viewport, zoom, fit_view, level-of-detail
  flow_input.h          ← interaction state machine (pan/select/drag/connect/reconnect/marquee)
  flow_widget.h         ← background, minimap, panel/overlay hook
  flow_undo.h           ← command history
  flow_layout.h         ← force-directed + layered auto-layout
  flow_json.h           ← structure (de)serialization; node-data via vtable hooks
  flow_run.h            ← flow_feed / flow_render / flow_present / flow_run, keybindings
tools/amalgamate.sh     ← emits flow.h from src/ in dependency order
demos/
  hello_flow.c          ← ~40-line minimal editor (proves the API is small to consume)
  topo.c                ← FLAGSHIP: network-topology editor (the 2026-05-31 spec, rebuilt)
  flowchart.c           ← groups + auto-layout demo
tests/
  flowtest.h            ← tiny dependency-free assert harness
  test_geom.c           ← transforms, bounds, hit-test, fit-view, routing, layout, undo
  test_render.c         ← cell-buffer snapshots vs golden ASCII
  test_input.c          ← synthetic SGR input → asserted state changes
  snapshots/*.txt       ← golden render outputs
Makefile                ← amalgamate → build demos → build+run tests
README.md               ← rewritten for flow (currently still tuibox's README)
```

`tuibox.h` is **removed** from the repo; its ~100 lines of raw-mode setup and SGR decode are
vendored into `flow_term.h` with attribution. `vec` (rxi) and the tuibox snippets are MIT;
their license notices are retained in `flow.h`.

Single TU rule: exactly one translation unit `#define FLOW_IMPLEMENTATION` before including
`flow.h`. Each demo and each test file does this.

---

## 3. Coordinate system

Two spaces, integer cells throughout the *stored* model:

- **World space** — where nodes live, in integer `(col,row)` cells. Origin `(0,0)`,
  unbounded. Integer positions mean **snap-to-grid is automatic** and rendering is always
  crisp. A child node's stored position is **relative to its parent** (§9); its absolute
  world position is the sum up the parent chain.
- **Screen space** — terminal cells actually drawn.

The viewport carries a float zoom and float offset for smooth accumulation, but stored node
positions remain integer cells; only the *projection* uses float math and rounds:

```
flow_viewport { float ox, oy; float zoom; }   /* zoom default 1, clamp [zmin, zmax] */

screen = round(world_abs * zoom + (ox, oy))         /* flow_to_screen  */
world  = round((screen - (ox, oy)) / zoom)          /* flow_to_world   */
```

Auto-layout and force simulation compute in float, then round to cells on commit, so the
model invariant (positions are valid integer cells) always holds. All projection funnels
through `flow_to_screen`/`flow_to_world` so the rest of the codebase is zoom-agnostic.

---

## 4. Core data model

State lives in one engine-owned store. Nodes/edges are kept in `vec` dynamic arrays. The
renderer iterates them each frame; interaction handlers mutate them through mutators.

```c
typedef struct { int x, y; } flow_pt;
typedef struct { int x, y, w, h; } flow_rect;
typedef enum { FLOW_TOP, FLOW_RIGHT, FLOW_BOTTOM, FLOW_LEFT } flow_pos;

enum { FLOW_SELECTED=1, FLOW_DRAGGING=2, FLOW_HOVERED=4 };

typedef enum { FLOW_HANDLE_SOURCE, FLOW_HANDLE_TARGET, FLOW_HANDLE_BOTH } flow_handle_kind;
typedef struct {                 /* connection anchor declared by a node TYPE */
  char id[16];
  flow_handle_kind kind;
  flow_pos pos;                  /* which side of the node */
  int along;                     /* offset along that side (multiple handles per side) */
} flow_handle;

typedef struct {
  int      id;                   /* monotonically assigned int; unique; stable */
  char     type[32];             /* selects node-type vtable */
  flow_pt  pos;                  /* relative to parent, else world */
  int      parent;               /* parent node id, or -1 */
  int      w, h;                 /* CACHED from vtable->measure — never DOM-measured */
  void    *data;                 /* opaque consumer payload */
  unsigned flags;                /* engine-managed: SELECTED|DRAGGING|HOVERED */
} flow_node;

typedef struct {
  int      id, source, target;   /* node ids */
  char     source_handle[16];    /* "" => default/nearest */
  char     target_handle[16];
  char     type[16];             /* "" => default router */
  char    *label;                /* optional, heap; NULL if none */
  void    *data;
  unsigned flags;                /* SELECTED */
} flow_edge;
```

Deliberate divergences from xyflow (all confirmed as correct simplifications by the research):
**int ids** (not strings), **integer cell positions** (not float pixels), and **sizes computed
by `measure()`** rather than DOM-measured — which collapses xyflow's entire
`measured`/`width`/`height`/`NodeDimensionChange` machinery, a browser-only artifact.

**Mutators** are the sole write path (each records an undo command, §11):

```c
int  flow_add_node(flow_t*, const char *type, flow_pt pos, void *data);
void flow_remove_node(flow_t*, int id);     /* cascades incident edges AND children */
void flow_move_node(flow_t*, int id, flow_pt pos);
void flow_set_parent(flow_t*, int child, int parent);   /* -1 detaches */
int  flow_add_edge(flow_t*, int src, int dst, const char *sh, const char *th);
void flow_remove_edge(flow_t*, int id);
void flow_reconnect_edge(flow_t*, int edge, int endpoint_node, const char *handle, int which);
void flow_set_edge_label(flow_t*, int edge, const char *label);
```

`flow_add_edge` ports xyflow's `addEdge` validation: reject self-edges, reject missing
endpoints, reject duplicate `(source,target,handles)`.

**Persistence boundary** (informs §12): persist *durable* fields (node id/type/pos/parent +
vtable-serialized data; edge source/target/handles/type/label; viewport). Never persist
*ephemeral* UI state (drag/connect/marquee in-flight, hover, selection, undo stack).

---

## 5. Type system — vtables are xyflow's custom components

xyflow's `nodeTypes`/`edgeTypes` registries (type-string → React component) become **vtables
of function pointers**, registered on the engine.

```c
typedef struct { float zoom; unsigned flags; int lod; } flow_render_ctx;

typedef struct {                 /* analog of a custom node component */
  const char *type;
  void (*measure)(const flow_node*, int *w, int *h);
  void (*render )(const flow_node*, flow_surface*, flow_render_ctx);  /* draws at local (0,0) */
  const flow_handle *handles; int handle_count;
  void (*save)(const flow_node*, FILE*);          /* optional: serialize node->data */
  void (*load)(flow_node*, const char *json);     /* optional: parse node->data    */
} flow_node_type;

typedef struct {                 /* analog of a custom edge component + path generator */
  const char *type;
  void (*route)(flow_pt s, flow_pos sp, flow_pt t, flow_pos tp, flow_route *out);
} flow_edge_type;

void flow_register_node_type(flow_t*, const flow_node_type*);
void flow_register_edge_type(flow_t*, const flow_edge_type*);
```

A node renderer draws into a **`flow_surface`** — a clipped sub-rect view of the compositor.
The engine projects the node's absolute rect, clips it to the viewport, and hands the renderer
a surface whose `(0,0)` is the node's top-left, so renderers never deal with world/screen/zoom
themselves (exactly how RF transforms the node container for you).

```c
typedef struct flow_surface flow_surface;
void flow_put (flow_surface*, int x, int y, uint32_t ch, uint8_t fg, uint8_t bg, uint8_t attr);
void flow_text(flow_surface*, int x, int y, const char *utf8, uint8_t fg, uint8_t bg, uint8_t attr);
void flow_box (flow_surface*, int x, int y, int w, int h, uint8_t fg, uint8_t bg, unsigned style);
int  flow_surface_w(const flow_surface*);
int  flow_surface_h(const flow_surface*);
```

**Handle anchors are computed analytically** — no DOM measure: `TOP → (wx+w/2, wy)`,
`RIGHT → (wx+w, wy+h/2)`, etc., recomputed every frame, so `useUpdateNodeInternals` has no
analog. **Built-in types**: a `default` node (titled box with N/E/S/W handles) and `group`
(§9); a `default` orthogonal edge and a `straight` edge.

---

## 6. Rendering — the cell compositor

A double-buffered grid of `flow_cell { uint32_t ch; uint8_t fg, bg, attr; }` (256-color;
`attr` bitflags: bold/reverse/dim/underline). Each frame:

1. Clear the back buffer.
2. Compose in xyflow's z-order: **background → group containers → edges → nodes →
   handles (on hovered node) → connection preview / marquee → minimap → overlay (panels)**.
   Selected items draw last within their layer (the cell analog of `elevateNodesOnSelect`).
3. **Damage diff**: compare back vs front buffer; emit ANSI cursor-moves + SGR + glyphs only
   for changed cells (coalescing runs, tracking cursor position to skip redundant moves).
4. Swap buffers.

This is the damage-tracked renderer tuibox's per-box string cache could not provide, and it is
the reason rendering is testable:

```c
void flow_render (flow_t*, flow_cell *out, int cols, int rows);  /* compose only — no TTY  */
void flow_present(flow_t*);                                      /* compose + diff + flush */
```

`flow_render` lets tests compose a known scene into memory and compare to a golden ASCII
snapshot (§13). `flow_present` is what the run loop calls.

---

## 7. Viewport, zoom, and level-of-detail

Zoom is a first-class, fully implemented feature. The transform is the genuine xyflow one
(`screen = world·zoom + pan`), with pointer-centered zoom, min/max clamp, and fit-view:

```c
void   flow_pan(flow_t*, int dx, int dy);
void   flow_set_zoom(flow_t*, float zoom, flow_pt screen_center);  /* keeps cell under cursor fixed */
void   flow_zoom_in(flow_t*);  void flow_zoom_out(flow_t*);
void   flow_fit_view(flow_t*, int margin);     /* getViewportForBounds: choose zoom+pan from bounds */
float  flow_zoom(flow_t*);
flow_pt flow_to_screen(flow_t*, flow_pt world_abs);
flow_pt flow_to_world (flow_t*, flow_pt screen);
```

The one irreducible terminal limit — a glyph is always one cell — means zoom is expressed as:

- **Spacing**: node *positions* scale with zoom, so zooming out contracts the graph and
  zooming in spreads nodes apart. (Box glyph sizes stay content-driven; they do not shrink
  below readability.)
- **Level of detail**: the engine passes the current `zoom`/`lod` in `flow_render_ctx`. The
  default node type renders **collapsed** (a single marker or truncated label) below a zoom
  threshold and a **full box** above it. Custom types may key off `lod` similarly. This is the
  honest analog of pixel-zoom's "small far away, large up close," and the minimap reuses the
  same downsample path.

`fit_view` and pointer-centered zoom are pure math (`getViewportForBounds`) and unit-tested.

---

## 8. Interaction model

A single state machine consumes decoded mouse/key events. States: `IDLE`, `PANNING`,
`DRAGGING` (one or many nodes), `CONNECTING` (from a source handle), `RECONNECTING` (an edge
endpoint), `MARQUEE`. Hit-test order: handle → node body → edge → pane.

| Action | Input | Notes |
|---|---|---|
| Pan | drag empty pane · arrows · wheel (SGR 64/65) · Space-drag | whole-cell; auto-pan near edge during drags |
| Select node/edge | left-click | clears others |
| Multi-select | Ctrl/⇧-click | toggle/add; modifier bits ride in the SGR mouse encoding |
| Marquee | ⇧-drag on pane | `full` (rect-contains) vs `partial` (rect-intersects) modes |
| Drag node(s) | click-hold + move | 1-cell threshold before engaging; selection moves together |
| Connect | drag handle→handle, **or** click-handle then click-handle | live preview line; validation as `flow_add_edge` |
| Reconnect | drag an edge endpoint to a new handle | updates source/target |
| Delete | `Delete`/`x` on selection; `Delete` on hovered edge | nodes cascade edges + children |
| Zoom | `+`/`-`, wheel+Ctrl | pointer-centered |
| Fit view | `f` | |
| Undo / Redo | `u` / `Ctrl-r` | |

`connectOnClick` (click a handle, then click another) is offered alongside drag because it is a
markedly better connection UX on a terminal than press-drag-release.

The library exposes events so consumers react without the library knowing app concerns:

```c
typedef struct {
  void (*on_node_click)(flow_t*, int node, void *u);
  void (*on_node_dblclick)(flow_t*, int node, void *u);
  void (*on_connect)(flow_t*, int src, int dst, void *u);
  void (*on_selection_change)(flow_t*, void *u);
  void (*on_nodes_delete)(flow_t*, const int *ids, int n, void *u);
  void (*on_pane_click)(flow_t*, flow_pt world, void *u);
  void (*on_overlay)(flow_t*, flow_surface *screen, void *u);   /* draw HUD/panels last */
  void *user;
} flow_callbacks;
void flow_set_callbacks(flow_t*, flow_callbacks);
void flow_bind_key(flow_t*, const char *seq, void (*fn)(flow_t*, void*), void *u);
```

---

## 9. Sub-flows / node groups

xyflow bundles grouping; we port it. A **group** is an ordinary node (built-in `group` type)
that has children. Children store positions **relative to the parent**; absolute position is
the parent's absolute plus the child's relative, walking the chain. Consequences:

- Dragging a group moves its subtree for free (children are relative).
- Z-order draws a parent before its children; children clip to the parent's rect.
- Optional **coordinate extent** clamps a child within its parent.
- Dragging a node over a group re-parents it (`flow_set_parent`), converting coordinates so it
  stays put visually; dragging out detaches.

```c
flow_pt flow_node_pos(const flow_node*);          /* relative */
flow_pt flow_node_abs(flow_t*, const flow_node*); /* absolute world */
```

---

## 10. Auto-layout

xyflow does **not** bundle layout (it documents integrating dagre/elk); a faithful port
likewise treats layout as a consumer convenience that operates *through the public mutators*
(`flow_move_node`), so it is not privileged and is undoable as one command. Two built-ins:

```c
typedef struct { int iterations; float k; float gravity; } flow_force_opts;
void flow_layout_force(flow_t*, flow_force_opts);      /* Fruchterman–Reingold, rounds to cells */

typedef enum { FLOW_LR, FLOW_TB } flow_layered_dir;
void flow_layout_layered(flow_t*, flow_layered_dir, int gap_x, int gap_y);  /* longest-path rank
                                                       + barycenter ordering; good for DAGs */
```

Both respect groups (laying out children within a parent's local space). Layout runs in float,
commits rounded cell positions, and pushes a single undo command.

---

## 11. Undo / redo

The change vocabulary xyflow uses for its controlled contract returns here as a **command
history**. Every mutator records an inverse command; consecutive same-gesture moves coalesce
into one (so a drag is one undo). History is a capped vector.

```c
void flow_undo(flow_t*);  void flow_redo(flow_t*);
int  flow_can_undo(flow_t*); int flow_can_redo(flow_t*);
```

Commands: move, add/remove node (remove snapshots the node + its incident edges + children),
add/remove edge, reparent, set-label, layout (snapshots all positions). Redo stack clears on a
new mutation.

---

## 12. Serialization

The library serializes **structure** (nodes: id/type/pos/parent; edges:
source/target/handles/type/label; viewport) as hand-rolled JSON (no deps). Opaque node `data`
is serialized via the node type's optional `save`/`load` vtable hooks; types without them
persist structure only.

```c
int flow_save(flow_t*, const char *path);
int flow_load(flow_t*, const char *path);
```

The `topo` demo's device node type implements `save`/`load` for label/type/info, reproducing —
and extending with groups/labels — the JSON format from the 2026-05-31 spec.

---

## 13. Testing strategy

The pure-core + cell-buffer-compositor split makes the whole library testable headless:

- **`test_geom`** (unit): world↔screen round-trips across zoom levels; `flow_bounds`;
  hit-test; `fit_view` math; orthogonal & straight routing glyphs; force & layered layout
  determinism (fixed seed); subflow absolute-position; `flow_add_edge` validation/dedup;
  undo/redo invariants (apply→undo restores byte-equal state).
- **`test_render`** (snapshot): compose known scenes via `flow_render` into a cell buffer,
  stringify, diff against `snapshots/*.txt` goldens — nodes, edges, groups, selection styling,
  minimap, LOD at low zoom. No TTY.
- **`test_input`** (interaction): drive `flow_feed` with synthetic SGR sequences; assert state
  (drag moves node by delta, marquee selects the right set, handle-drag creates the edge,
  endpoint-drag reconnects, `u` reverts). No TTY.

A tiny dependency-free `flowtest.h` (`ASSERT`/`ASSERT_EQ`, counts, exit code) keeps with the
no-deps ethos. `make test` amalgamates, builds, and runs all three.

---

## 14. Public API summary

```c
flow_t *flow_new(int cols, int rows);
void    flow_free(flow_t*);
void    flow_resize(flow_t*, int cols, int rows);

void flow_register_node_type(flow_t*, const flow_node_type*);
void flow_register_edge_type(flow_t*, const flow_edge_type*);

/* mutators */            /* selection */              /* viewport */
flow_add_node             flow_select_node             flow_pan
flow_remove_node          flow_select_edge             flow_set_zoom / zoom_in / zoom_out
flow_move_node            flow_clear_selection         flow_fit_view
flow_set_parent           flow_selected_nodes          flow_to_screen / flow_to_world
flow_add_edge
flow_remove_edge          /* queries */                /* widgets */
flow_reconnect_edge       flow_get_node / get_edge     flow_set_background
flow_set_edge_label       flow_each_node               flow_set_minimap
                          flow_bounds                  /* overlay via callbacks.on_overlay */
/* undo */                flow_hit_node / hit_edge
flow_undo / flow_redo
flow_can_undo / can_redo  /* layout */                 /* lifecycle */
                          flow_layout_force            flow_feed     (inject input / tests)
/* persistence */         flow_layout_layered          flow_render   (compose to buffer)
flow_save / flow_load                                  flow_present  (diff + flush)
                          /* events */                 flow_run      (raw mode + loop)
                          flow_set_callbacks
                          flow_bind_key
```

`flow_run` is the convenience entry (raw mode, read loop, present, restore on exit);
`flow_feed`/`flow_render`/`flow_present` are the decomposed pieces used by embedders and tests.

---

## 15. Build order (all features in final scope)

Phased only for sane sequencing — nothing is punted to a "later version."

0. **Scaffold** — `src/` skeleton, vec + term vendored, amalgamate script, Makefile, `flowtest.h`.
1. **Pure core** — geometry/transform, model + mutators, bounds, hit-test → `test_geom` green.
2. **Compositor** — cell buffer, `flow_surface`, compose, diff, `flow_render` → static snapshots.
3. **Type system + routing** — node/edge vtables, default types, orthogonal + straight routers, arrowheads.
4. **Terminal + interaction** — term decode, pan, select (single/multi/marquee), node drag (multi/threshold/autopan), `flow_run`/`flow_feed`, `hello_flow.c`, `test_input`.
5. **Connections** — handles, connect (drag + click), preview, validation, edge select, reconnection, edge labels.
6. **Zoom** — full transform, pointer-centered, min/max, `fit_view`, LOD rendering.
7. **Widgets** — background (dots/lines/cross), minimap (+ viewport rect), overlay hook.
8. **Sub-flows / groups** — parent/child, relative coords, extents, drag-to-reparent.
9. **Undo / redo** — command history + gesture coalescing.
10. **Auto-layout** — force-directed + layered.
11. **Serialization** — structure JSON + node-data vtable hooks.
12. **Flagship + polish** — `topo.c` network editor, `flowchart.c`, README rewrite.

Tests grow with each phase.

---

## 16. Risks & mitigations

- **Single-header size.** Mitigated by modular `src/` development + amalgamation; `flow.h` is a
  generated artifact, not hand-edited.
- **Zoom expectations.** "Full zoom" on a grid means spacing + LOD, not magnification; this is
  stated up front in README and the demo, with the glyph-size invariant made explicit.
- **UTF-8 width.** Box-drawing and device glyphs may be wide/ambiguous; the compositor tracks
  cell width and the default types stick to single-width glyphs to keep alignment exact.
- **Scope.** Large but cohesive — every module shares the store/compositor/transform core, so
  the implementation plan stages it (§15) rather than splitting into independent specs.
```
