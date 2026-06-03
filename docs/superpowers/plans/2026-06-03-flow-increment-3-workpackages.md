# flow — Increment 3 Work Packages (fresh-context handoff)

_2026-06-03 · generated from a multi-agent scoping pass against the current `flow.h`_

> **How to use this in a clean chat.** Open a fresh session in this repo and say which package you want
> (e.g. *"implement the serialize package from the increment-3 handoff doc"*). The agent should first read,
> in order: `flow.h` (current amalgamated library — real signatures), `docs/superpowers/specs/2026-06-02-c-xyflow-flow-design.md`
> (full design), and this file. Then follow the repo's TDD loop: edit `src/*.h`, regenerate with
> `make flow.h` (never hand-edit `flow.h`), add a `tests/test_*.c` using `tests/flowtest.h` (`ASSERT`/`SNAPSHOT`),
> drive interactions with synthetic SGR via `flow_feed`, and gate on `make test` **plus** AddressSanitizer/UBSan
> before committing. Each package below is self-contained and ends in a working, tested build.
>
> **Running these with ultracode:** the four major pillars split into a near-zero-overlap pair you can run from
> day one (`serialize`, `straight-edge`) and a SERIAL SPINE keyed on the coordinate model (`groups` →
> `undo-redo` → `auto-layout`). The small input/event packages (`events`, `space-pan`, `auto-pan`) all edit the
> same `flow_handle_mouse` the spine rewrites, so they sequence after `groups`. Use the **Execution order**,
> **Dependency graph**, and **Parallelization** sections below. One package per fresh chat is the simplest safe
> default; only `serialize` and `straight-edge` are genuinely parallel-worktree-safe.

## Current state (what's already built, on `main`)

Single-header C library, amalgamated from `src/` via `tools/amalgamate.sh` (modules order:
`flow_head flow_geom flow_cell flow_model flow_view flow_route flow_types flow_render flow_input flow_term flow_run`).
**Increment 2 landed 6 of its 7 packages** — `background`, `keys-commands`, `multiselect`, `connections`,
`edge-interaction`, and `zoom` are all merged and tested; the seventh, `serialize`, was fully spec'd but
**never implemented** (verified: no `tests/test_json.c`, no `flow_save`/`flow_json` in `flow.h`, `test_json`
absent from the Makefile `TESTS=` list). It is re-scoped here against the post-zoom/handles/labels/parent
`flow.h`, which differs materially from the original inc-2 spec (see the serialize package).

What is on `main` today: pure core (transform/bounds/hit-test), engine + node/edge CRUD with validation,
node/edge **vtables** + default types, **cell compositor + damage-diff** renderer, terminal raw-mode +
**SGR-1006 mouse**, **panning**, **node drag**, **multi-select + marquee** (true selection set, selected-last
two-pass draw, multi-node group drag), **port handles + connect-drag** (`flow_handle_anchor`, live dashed
preview, `connectOnClick`, `on_connect`), **edge interaction** (`flow_hit_edge`/select/delete/reconnect/labels),
**keyboard dispatch** (`flow_bind_key`/`flow_dispatch_key` + built-ins `x`/`n`/`f`/`?`, registry-before-builtin),
**real float zoom** (pointer-centered `flow_set_zoom`, `flow_zoom_in/out`, `flow_fit_view`, LOD-collapse render,
zoom-aware `flow_hit_node`/minimap), and a **background grid widget**. Demos: `hello_flow.c`, `topo.c`
(custom `device` node type + right-click details panel). Tests: 14 `test_*` programs, all ASan/UBSan-clean.

**Settled mutator surface** (what increment 3 builds on, all confirmed in `src/flow_model.h`):
`flow_add_node`/`flow_add_edge`, `flow_remove_node` (cascades children + incident edges, frees labels, line 350),
`flow_remove_edge` (line 340), `flow_move_node` (top-level only, `pos==absolute`, line 192-194),
`flow_reconnect_edge` (line 309), `flow_set_edge_label` (line 331), `flow_end_connection` (routes through
`flow_add_edge`, line 522), `flow_add_node_center` (line 378), `flow_delete_selection` (line 372),
`flow_fit_view` (zoom-aware, line 385), `flow_dispatch_key` (registry-before-builtin, line 412).

**Known carry-overs** (which package fixes each is noted inline): `flow_move_node` is **top-level only**
(`src/flow_model.h:194` comment "top-level nodes only, so pos == absolute"); multi-drag applies a **flat
per-node world delta** to every selected node (`src/flow_input.h:162-169`) which double-moves a parent+child
pair once nesting is live; there is **no `group` built-in type / no `flow_set_parent`**; **no undo/redo**;
**no auto-layout**; **no JSON persistence** and **no graph-reset** (`flow_free` never frees `node->data`);
edge router has only the **orthogonal** built-in (`flow_default_edge_type`, `src/flow_route.h:7`) — the spec's
**straight** edge type is unbuilt; `flow_callbacks` ships **5** of the spec's 7 callbacks
(`src/flow_model.h:103-107`) — `on_node_dblclick`/`on_selection_change`/`on_nodes_delete` are missing;
**Space-drag pan** and **auto-pan near edge** are unbuilt; `demos/flowchart.c` (the groups+layout showcase)
does not exist.

## Execution order (recommended)

1. **serialize** — M; the one truly independent pillar. New `src/flow_json.h` + a trailing `flow_node_type` append + one amalgamate line; touches no `flow_input.h`/`flow_render.h`/`flow_handle_mouse`/`flow_feed`. Start it in a parallel worktree day one and land it whenever convenient; it unblocks save/load demos without waiting on the spine.
2. **straight-edge** — S; the other root. Adds a `flow_route_straight` router + a registered `flow_edge_type{"straight",…}` so edges with `type=="straight"` draw a direct stepped line. Touches only `flow_route.h`/`flow_types.h` registration — no module, no `flow_handle_mouse`, no struct flow. Parallel-safe with serialize.
3. **groups** — XL; the FOUNDATIONAL coordinate-model change and the head of the serial spine. Makes `flow_move_node` parent-relative (absolute-in, converts internally), adds the `group` built-in + `flow_set_parent`/`flow_group`/`flow_ungroup`, reworks render z-order to depth-aware, fixes the multi-drag double-move to selection-roots, and adds per-node clip. Everything after it on the spine depends on its settled mutators and its absolute-in `flow_move_node` contract. MUST precede undo and auto-layout.
4. **events** — M; the three missing observer callbacks (`on_node_dblclick`, `on_selection_change`, `on_nodes_delete`). `on_selection_change` must fire from every selection mutator (select/toggle/clear/marquee/delete) and `on_nodes_delete` from `flow_delete_selection`/`flow_remove_node` — the same mutator set undo wraps — so settle events BEFORE undo journals those paths. Touches `flow_handle_mouse` (dblclick timing) so sequence after groups settles drag classification.
5. **space-pan** — S; hold-Space forces drag-to-pan. Small `flow_handle_mouse`/`flow_feed` modifier change; lands after groups owns the press classification.
6. **auto-pan** — M; viewport pans toward a node/connection drag cursor nearing a screen edge. Edits `flow_handle_mouse` motion (the same gesture branches groups reworks), so after groups.
7. **undo-redo** — L; the inverse-op command journal. Instruments the now-settled + grouped mutator set (including `flow_set_parent`/`group`/`ungroup` via a `FLOW_CMD_REPARENT`) and brackets the final drag/reconnect gestures in `flow_handle_mouse`. Follows groups (its MOVE command must replay the absolute-in `flow_move_node` consistently) and events (so it wraps the same selection/delete paths events already fires from).
8. **auto-layout** — XL; the capstone. Pure model→positions transform via `flow_layout(f, opts)` (force-directed + layered) through `flow_move_node`, respecting group-local space and optionally `flow_fit_view`. Low file-overlap (new `src/flow_layout.h`) but high SEMANTIC dependency: it needs the settled, framable, optionally-grouped model and is the natural single-undo-command consumer once undo lands.
9. **flowchart-demo** — S; the deliverable that proves groups + auto-layout together. Gated on both; lands LAST.

**Dependency graph**

```
DECLARED vs REAL: every JSON depends_on points only at the SETTLED inc-2 surface (or claims [] outright).
That under-states the inter-package edges — exactly the trap inc-2 flagged. The real DAG, synthesized from
each scope's conflicts_with + design_notes:

  serialize ─────┐
                 ├── true roots (no inter-inc-3 edges; only the settled inc-2 surface)
  straight-edge ─┘

  groups ──> undo-redo            (HARD: undo must journal flow_set_parent/group/ungroup as FLOW_CMD_REPARENT,
                                          and groups makes flow_move_node parent-relative-internally — undo's
                                          MOVE command must store/replay ABSOLUTE consistently with that)
  groups ··> auto-layout          (SOFT: auto-layout's group-local pass relies on parent-relative coords;
                                          re-validate against groups' container-rect contract)
  groups ──> events               (events' dblclick edits flow_handle_mouse; let groups settle drag
                                          classification first — same-file serial contention)
  groups ──> space-pan, auto-pan  (both edit flow_handle_mouse press/motion that groups reworks)

  events ··> undo-redo            (SOFT/ordering: on_selection_change/on_nodes_delete fire from the SAME
                                          select/toggle/clear/marquee/delete + remove paths undo wraps —
                                          settle the callback firing before undo instruments them)

  undo-redo ··> auto-layout       (SOFT: spec §10/§11 wants the whole flow_layout call to be ONE coalesced
                                          undo command — undo wraps flow_layout; no edit inside layout)

  {groups, auto-layout} ──> flowchart-demo   (HARD: the demo showcases both; lands after both)

Legend: ──> hard (correctness / same-file serial); ··> soft (semantic / ordering). Roots with no in-edges:
serialize, straight-edge, groups. Sink: flowchart-demo. groups is the spine head; everything editing
flow_handle_mouse or the mutator surface rebases on it.
```

**File-conflict matrix**

File-touch counts across the nine `files_touched` lists, hottest first:

- **src/flow_model.h struct flow / mutators** — 5/9 (groups adds helpers + clip plumbing; events adds nothing structural but extends every selection mutator; undo appends the `journal` sub-struct + instruments ~10 mutators in place; serialize adds `flow__graph_reset` + the `flow_node_type` append; auto-layout touches NONE — stateless). The mutator bodies are the contested region: undo records inside them, events fires callbacks from them, groups changes `flow_move_node`. Serialize SERIAL after groups settles the `flow_node_type` shape.
- **src/flow_input.h flow_handle_mouse** — 5/9 (groups reworks multi-drag→selection-roots + reparent-on-drop; events adds dblclick timing; space-pan adds Space modifier; auto-pan adds edge-proximity motion; undo adds the `flow__undo_begin/end` brackets). The single most contended interactive region. Required final ordering: groups owns the drag classification rewrite; events/space-pan/auto-pan layer modifiers; undo brackets the SETTLED gestures last. Strictly serial.
- **src/flow_render.h flow_render** — 2/9 (groups: depth-aware node-loop reorder + per-node ancestor clip; straight-edge: none — the edge loop already calls `et->route`, so a registered straight router needs no render edit). groups is the only structural render change; it rewrites the two-pass node loop into a depth-then-selected order. HIGH but single-owner.
- **src/flow_cell.h flow_surface** — 1/9 (groups adds `clip_x/clip_y/clip_w/clip_h` to `struct flow_surface` and ANDs them into `flow_put`; every aggregate initializer must add the 4 fields). Single owner; audit all `flow_surface` initializers when groups lands.
- **src/flow_run.h flow_feed** — 2/9 (undo: 'u'/Ctrl-r route automatically via the existing `flow_dispatch_key` — NO `flow_feed` edit; space-pan: Space key-state tracking). flow_feed is nearly untouched because the keyboard seam (`flow_dispatch_key`) already exists.
- **tools/amalgamate.sh modules=** — 3/9 (serialize: `flow_json` after `flow_render`; undo: `flow_undo` after `flow_model`; auto-layout: `flow_layout` after `flow_view`) — three DIFFERENT lines, trivially coexist; just don't clobber each other's insertion. straight-edge needs NO module (registers into existing flow_route/flow_types).
- **src/flow_route.h / src/flow_types.h** — 1/9 each (straight-edge owns both: a `flow_route_straight` def + a `flow_register_edge_type` call). No contention.
- **flow_node_type struct** — 1/9 (serialize appends `save`/`load` hooks as fields 6/7; trailing zero-init keeps `flow_default_node_type` and topo's `DEVICE` valid). Single owner; groups adds a `flow_group_node_type` *instance* but does NOT change the struct shape, so no clash.
- **Makefile TESTS / demos/topo.c** — 6/9 append-only TESTS entries; 4/9 append showcase wiring to `demos/topo.c` (serialize save/load, undo status hint, auto-layout call). Low-stakes textual merges; defer all topo wiring to a single final integration pass.

**Parallelization strategy**

Be honest: like inc-2 this is mostly a SERIAL SPINE, not a fan-out — 5/9 packages edit `flow_handle_mouse`
and the mutator bodies are co-edited by groups/events/undo.

SAFE TO RUN IN A PARALLEL WORKTREE (fresh-context chat) from day one:
- **serialize** — touches no `flow_input.h`/`flow_render.h`/`flow_handle_mouse`/`flow_feed`. Surfaces: new `src/flow_json.h`, a trailing append to `flow_node_type`, one `amalgamate.sh` line (`flow_json` after `flow_render`), one Makefile `TESTS` entry, `demos/topo.c`. Develop in `git worktree add ../flow-serialize` against `main`; rebase only the `flow_node_type` append + amalgamate line at merge (both append-only). It does collide with **undo** on one seam — `flow_load`'s reset MUST clear the undo journal — but that wiring is owned by whoever lands second (see Gaps).
- **straight-edge** — touches only `flow_route.h` + `flow_types.h` registration (no module, no struct flow, no input). Fully parallel with serialize and with the spine.

MUST BE SERIAL (single spine, in the recommended order):
- **groups → events → space-pan → auto-pan → undo-redo → auto-layout → flowchart-demo.** Each rebases on the previous: groups owns `flow_handle_mouse` drag classification + the render node loop + `flow_surface`; events/space-pan/auto-pan layer onto the settled drag machine; undo instruments the now-final mutator + gesture set; auto-layout consumes the framable/grouped model; the demo needs both groups and layout.

PRACTICAL CADENCE: each chat = one package = one feature branch off the previous spine tip. After each:
`make` (regenerates `flow.h` via amalgamate — never hand-edit `flow.h`), `make test`, then the ASan/UBSan loop
(`cc -std=c11 -fsanitize=address,undefined -fno-sanitize-recover=all -g tests/test_X.c -o /tmp/st_X -lm && /tmp/st_X`)
before merging. Keep `serialize`/`straight-edge` worktrees rebasing on the spine tip periodically so the
`flow_node_type` append and edge-type registration stay current. Because every package regenerates `flow.h`,
never resolve a `flow.h` conflict by hand — resolve `src/` then re-run amalgamate.

**Gaps / overlaps flagged**

- **serialize × undo (cross-package seam):** `flow_load` calls `flow__graph_reset` FIRST; if undo has landed, that reset MUST also call a `flow__journal_clear` (free pending command label-copies, drop borrowed data ptrs, zero the stacks), otherwise undo would invert against a stale/replaced graph. Whoever lands SECOND wires this one line into the reset. Until undo exists, serialize's reset stands alone.
- **groups × undo (the real reason undo follows groups, not just "settled surface"):** groups KEEPS `flow_move_node` absolute-in but converts to parent-relative internally — undo's `FLOW_CMD_MOVE` must store/replay the SAME absolute coordinates `flow_move_node` accepts, and undo must add `FLOW_CMD_REPARENT` to invert `flow_set_parent`/`flow_group`/`flow_ungroup` (group/ungroup mutate `parent` AND add/remove the container, so ungroup's inverse is a composite: re-add the container + re-parent the children). Build undo AFTER groups so it journals the settled set.
- **flow_handle_mouse is the serial contention zone:** groups (multi-drag→selection-roots + reparent-on-drop), events (dblclick timing), space-pan (Space modifier), auto-pan (edge-proximity motion), and undo (txn brackets) ALL edit it. Land groups first to settle drag classification; layer the modifiers; bracket the FINAL gestures with undo last. Agree the press hit-order up front: handle → edge endpoint → node body → pane (unchanged from inc-2), with Space forcing pan and dblclick detected on the node-body click path.
- **selection mutators are co-owned by events and undo:** `on_selection_change` fires from `flow_select_node`/`flow_toggle_node`/`flow_clear_selection`/`flow_select_in_rect`/`flow_delete_selection`; `on_nodes_delete` from `flow_delete_selection`/`flow_remove_node`. undo records inverses in the SAME functions. Sequence events before undo so undo instruments already-firing paths rather than racing them.
- **amalgamate.sh modules= edited by THREE packages** (serialize `flow_json` after `flow_render`; undo `flow_undo` after `flow_model`; auto-layout `flow_layout` after `flow_view`) on different lines — coordinate so none clobbers another's insertion, exactly the inc-2 zoom+serialize note. straight-edge adds NO module.
- **buildability / amalgamation ordering (a function may only call one defined at/before it):** undo's `flow_undo`/`flow_redo` APPLY inverses by calling mutators in `flow_model.h`, so they must be DEFINED in `src/flow_undo.h` (after `flow_model`), while their DECLARATIONS and the pure-data record/txn primitives live in `flow_model.h`'s header section. auto-layout's `flow_layout.h` goes AFTER `flow_view` so `flow_move_node`/`flow_measure_node`/`flow_bounds`/`flow_fit_view` prototypes are in scope. serialize's `flow_json.h` goes AFTER `flow_render` and needs only `flow_model`/`flow_types`.
- **flow_surface struct append (groups):** adding clip fields to `struct flow_surface` (`src/flow_cell.h:8`) forces every aggregate initializer (in `flow_render.h`, the minimap inner surface, overlay, statusbar) to add 4 fields; set them to full-buffer for all non-node surfaces so minimap/overlay/statusbar snapshots are byte-identical. Re-run ALL snapshot tests after.
- **NO IN-TIER COVERAGE GAP after this set:** the four pillars + the four overlooked-but-in-spec items (straight-edge, events, space-pan, auto-pan) + the flowchart demo close every remaining in-spec feature. The only intentional non-builds are listed under "Deferred beyond increment 3".

**Deferred beyond increment 3**

- **flow_each_node iterator (§14, XS):** the API-summary lists `flow_each_node(f, cb, user)`, but the shipped index API (`flow_get_node` + `flow_node_count` + `flow_edge_count`, `flow.h` accessors) is the intentional equivalent — this is a doc/API-shape reconciliation, not a feature gap. Add the iterator only if a consumer demands the callback shape; otherwise leave the index API as-is.
- **BEZIER / SMOOTHSTEP curved edge types (§1 non-goal):** explicitly cut — "curved edges (bezier/smoothstep) collapse to box-drawing corners." The orthogonal router (and the new straight router) are the complete edge-type set. Do NOT add a curved router.
- **Smooth / eased / sub-cell zoom magnification (§1, §16):** cut — a glyph is always one cell; zoom is spacing + LOD only, never magnification or tween. Real-scale position zoom + LOD already shipped in inc-2.
- **Wide / double-width device glyphs (§16):** cut — wide emoji break cell alignment in the diff renderer; the topo demo stays single-width.
- **Animation smoothness beyond the cell grid (§1):** cut.
- **ncurses / any external rendering dependency (overview goal):** cut — pure ANSI escapes, single header, no deps.

---

## Packages

### 1. JSON serialization: flow_save / flow_load + node-data vtable hooks  `[M]`  ·  id: `serialize`

**Goal.** Add dependency-free JSON persistence. `flow_save(f, path)` writes durable graph structure (each node
id/type/pos.x/pos.y/parent; each edge source/target/sourceHandle/targetHandle/type/label; viewport ox/oy/zoom
as **floats**) plus opaque `node->data` via optional save/load vtable hooks on `flow_node_type`.
`flow_load(f, path)` resets the graph and rebuilds it (re-measuring node w/h, restoring ids and bumping
`nextid`/`nexteid`) so the topo demo's device struct round-trips byte-for-byte. New `src/flow_json.h` module,
amalgamated after `flow_render` and before `flow_input`.

**User value.** Users persist a graph to a `.json` file and reload it later (and hand-edit/diff it), with custom
node types round-tripping their own data via two small vtable hooks. **Real zoom and pan survive the round-trip**
(not just `zoom==1`). The topo demo gains load-on-start / save-on-quit so a topology survives across runs with
device data fully restored.

**Files touched.**
  - src/flow_model.h
  - src/flow_json.h
  - src/flow_types.h
  - tools/amalgamate.sh
  - tests/test_json.c
  - tests/snapshots/json_basic.txt
  - Makefile
  - demos/topo.c

**Entry points (existing functions to extend).**
  - `flow_node_type` (`src/flow_model.h:20`) — append two trailing fn-ptr fields `save`/`load` AFTER `handle_count`; trailing zero-init keeps all 5-field initializers valid
  - `flow_default_node_type` (`src/flow_types.h:26`) — 5-field initializer `{ "default", flow__default_measure, flow__default_render, flow_default_handles, 2 }` stays valid via trailing zeros; this is the real default-type init, NOT `flow_register_defaults`
  - `flow_free` (`src/flow_model.h:155-159`) — its edge-label free loop is the pattern reused by the new private `flow__graph_reset` (frees `edges[].label`, frees node/edge arrays, zeroes counts/caps and ids); `flow_free` still must NOT free `node->data`
  - `flow_add_node` / `flow_add_edge` / `flow_measure_node` (`src/flow_model.h`) — `flow_load` rebuilds via add_node then overwrites `n->id`/`n->parent` and re-measures; add_edge for structure then patch type/label/handles
  - `flow_view_get` / direct `f->view` assignment (`src/flow_model.h`) — read viewport for save (ox/oy/zoom floats), set `f->view = {ox,oy,zoom}` on load
  - `demos/topo.c` — device struct (`label[24],kind[12],ip[20],status[12],os[24]`); extend `DEVICE` initializer to add `dev_save`/`dev_load`; `main` wires `flow_load` on start + `flow_save` on quit
  - `tools/amalgamate.sh:5` — insert `flow_json` into `modules=` between `flow_render` and `flow_input`
  - `Makefile:6` — append `test_json` to `TESTS=`

**API additions.**
```c
/* public API (declared in src/flow_model.h next to flow_view_get/flow_free; implemented in src/flow_json.h) */
int flow_save(flow_t *f, const char *path);  /* writes JSON; 0 ok, -1 on I/O/encode error */
int flow_load(flow_t *f, const char *path);  /* resets f then rebuilds from JSON; 0 ok, -1 on open/parse error */

/* flow_node_type gains TWO trailing optional hooks (struct append in src/flow_model.h,
   AFTER `const flow_handle *handles; int handle_count;`). flow_node_type.type is
   `const char *`, so these signatures compose cleanly: */
void (*save)(const flow_node *n, FILE *out);        /* optional: write node->data as a single JSON value */
void (*load)(flow_node *n, const char *data_json);  /* optional: parse node->data from the captured "data" value span (NUL-terminated copy) */

/* tiny hand-rolled JSON helpers in src/flow_json.h (file-local statics; reachable in
   the per-file test because it #defines FLOW_IMPLEMENTATION and #includes flow.h): */
static void flow__json_str(FILE *out, const char *s);                 /* emit a quoted, escaped JSON string */
typedef struct { const char *p; const char *end; } flow_json_rd;      /* cursor over an in-memory buffer */
static int  flow__json_find(const char *obj, const char *end, const char *key, flow_json_rd *out); /* value of key within one object brace-span; 1 if found */
static int  flow__json_int(flow_json_rd r, int *v);                   /* parse int (strtol) */
static int  flow__json_float(flow_json_rd r, float *v);              /* parse float (strtof) — NEW vs original spec: viewport is float now */
static int  flow__json_strv(flow_json_rd r, char *buf, int cap);      /* parse + unescape a string into a fixed buffer */
static int  flow__json_raw(flow_json_rd r, const char **start, int *len); /* capture a raw value span (the data sub-object/array/scalar) */
static int  flow__json_array(const char *obj, const char *end, const char *key, flow_json_rd *out); /* locate an array value span by key */
static int  flow__json_iter(flow_json_rd *arr, const char **elem, int *len); /* yield successive top-level element spans of an array */

/* private graph-reset used by flow_load (src/flow_model.h, static, FLOW_IMPLEMENTATION): */
static void flow__graph_reset(flow_t *f);  /* free edges[].label, free node/edge arrays, zero counts/caps, reset nextid/nexteid=1; leaves view/types/cb/widgets intact; does NOT free node->data (app owns it) */
```

**Design notes.**

VALIDATED + UPDATED against the CURRENT `flow.h` (the original inc-2 package-7 spec predates real zoom / handles /
labels / parent). What DIFFERS from the original spec, called out explicitly:

(A) **VIEWPORT IS NOW REAL FLOAT ZOOM.** `flow_viewport = {float ox,oy,zoom}` (`src/flow_geom.h:5`) and
`flow_new` sets `view.zoom=1`, zmin/zmax. Do NOT assume `zoom==1`. Persist all three as floats:
`"viewport":{"ox":%g,"oy":%g,"zoom":%g}` — use `%.9g` for lossless round-trip (`%g` alone can lose precision and
break the save→load→save byte-identical acceptance). Reader uses `strtof`, not the original spec's strtol-only
`flow_json_int` — hence the NEW `flow__json_float` helper. On load set `f->view = {ox,oy,zoom}`; do NOT touch
`f->zmin`/`f->zmax` (those are limits, ephemeral config, kept from the live engine).

(B) **EDGES NOW CARRY MORE DURABLE FIELDS.** `flow_edge` (`src/flow_model.h:13-14`) =
`{id,source,target, source_handle[16], target_handle[16], type[16], char *label, void *data, flags}`. Persist
id/source/target/sourceHandle/targetHandle/type/label. `label` is heap `char*` (freed in `flow_free` /
`flow__graph_reset`) — write only when non-NULL; **omit the key** (do NOT write `"label":""`) so a NULL label
round-trips back to NULL, not `""`. `type[16]` and the two `handle[16]` buffers are fixed arrays: write always
(empty string is the legitimate default), read with `flow__json_strv` into sized buffers. EDGE `void* data` has
**no** save/load hook in this scope (only NODES get data hooks) — edge data is app-owned and NOT persisted;
document this as an intentional boundary.

(C) **NODES NOW CARRY `parent`.** `flow_node` (`src/flow_model.h:12`) =
`{id, type[32], pos, parent(default -1), w, h, data, flags}`. Persist id/type/pos.x/pos.y/parent + optional data.
On load, restore `parent` EXACTLY (`flow_node_abs` at `src/flow_model.h:216` walks the parent chain, so child
coords are stored RELATIVE — persist `n->pos` verbatim, never `flow_node_abs`). w/h are NOT persisted:
`flow_measure_node` recomputes them after load (and after the data hook runs, since device measure reads
`n->data`). `nextid` must become `max(node id)+1`; `nexteid = max(edge id)+1`, so post-load adds don't collide.

(D) **`flow_node_type` APPEND TARGET.** Current shape (`src/flow_model.h:20`) is
`{type, measure, render, handles, handle_count}`. Append `save`,`load` as fields 6 and 7. Both existing
initializers — `flow_default_node_type` (`src/flow_types.h:26`, 5 fields) and topo's `DEVICE` — stay valid
because C zero-fills trailing members. CORRECTION vs the original spec entry-point: the initializer to
leave-valid is the `const flow_node_type flow_default_node_type` literal in `flow_types.h`, not
`flow_register_defaults` (a function).

(E) **EPHEMERAL STATE — MUCH BIGGER NOW.** Persist NOTHING of: node/edge flags
(FLOW_SELECTED/DRAGGING/HOVERED), `node->data` unless a save hook exists, edge `data` ever, w/h (recomputed),
and ALL the inc-2 engine state on `struct flow` (`src/flow_model.h:121-141`): zmin/zmax, all mouse/drag fields
(drag_node, dragging_pan, drag_grab, last_mouse, drag_last_world, mouse_down, down_node, moved, down_pos,
down_modsel), marquee_*, conn_*, reconnect_*, keys[]/nkeys, statusbar, minimap, bg, cb. The writer simply never
emits these. Add a test that greps the output for `"flags"`/`"selected"`/`"zmin"`/`"marquee"` and asserts absent.

(F) **MODULE PLACEMENT.** amalgamate order is now
`flow_head flow_geom flow_cell flow_model flow_view flow_route flow_types flow_render flow_input flow_term flow_run`
(`tools/amalgamate.sh:5`). `flow_view` ALREADY EXISTS (added by zoom). serialize only inserts `flow_json` AFTER
`flow_render` and BEFORE `flow_input`. `flow_json` needs `flow_model` (struct flow, add_node/edge, measure_node)
and `flow_node_type` — all defined at/before `flow_render`, so placement is valid. `FILE*` is available
everywhere (`stdio.h` in `flow_head.h`), so the FILE* hook signatures and fopen/fprintf compile.

WRITER: pure `FILE*` fprintf, no buffer building. `flow__json_str` does the 7 mandatory escapes
(`\" \\ \b \f \n \r \t`) plus other control chars (<0x20) as `\uXXXX`. Node `"data"` is produced verbatim by the
type's `save()` hook (same FILE*, writes one JSON value); if no save hook, omit the `"data"` key. Stable,
hand-editable, shallow format:
```
{"version":1,
 "viewport":{"ox":0,"oy":0,"zoom":1},
 "nodes":[{"id":1,"type":"device","x":6,"y":3,"parent":-1,"data":{...}}],
 "edges":[{"id":1,"source":1,"target":2,"sourceHandle":"","targetHandle":"","type":"","label":"L"}]}
```

READER: no DOM, no recursive descent. Targeted key lookup within an object's brace span (`flow__json_find` scans
top-level keys, skipping nested `{}`/`[]` and strings so it never matches a key inside `data`). `flow__json_array`
+ `flow__json_iter` walk the nodes/edges arrays element-by-element (each element is itself a brace-span passed
back to `flow__json_find`). Ints via strtol, floats via strtof, strings unescaped into fixed buffers sized to the
struct fields (`type[32]`, `handle[16]`, edge `type[16]`). `flow__json_raw` returns the span of the `"data"`
value; `flow_load` NUL-terminates a copy and passes it to the type's `load()` hook, which mallocs+assigns
`node->data`; THEN `flow_measure_node` re-runs (needs the data for device measure). A NUL-terminated copy (not a
raw `{p,end}` pair) keeps the hook API trivial for app authors.

LOAD LIFETIME (the subtle hazard): `flow_load` calls `flow__graph_reset` FIRST. The reset / `flow_free` free edge
labels but NEVER `node->data`. A `load()` hook that mallocs `node->data` therefore creates app-owned memory the
library will not free — this matches today's contract. Document loudly. The demo's `dev_load` mallocs a device
per node; the demo frees them (or relies on process exit). The library will NEVER free arbitrary `void*`. Parse
preserves array order = insertion order = draw order. Empty arrays are valid (count 0).

NO OVERLAP CONFIRMED: serialize touches no `flow_input.h`/`flow_render.h`/`flow_run.h`/`flow_handle_mouse`/
`flow_feed`. Its only shared edit surfaces are (1) the `flow_node_type` struct append, (2) one amalgamate.sh
module line, (3) one Makefile TESTS line, (4) `demos/topo.c` showcase wiring — all append-only / non-contended.

**Test plan.**
  - tests/test_json.c (new, per-file: `#define FLOW_IMPLEMENTATION`, `#include "../flow.h"`, `#include "flowtest.h"`, report via `flowtest_report`; added to Makefile TESTS).
  - Writer golden: build 2 default nodes + 1 edge, `flow_pan` + `flow_set_zoom` to a non-1 float zoom, `flow_save` to a temp path under `/tmp`; read the file back; `SNAPSHOT("json_basic", buf)` so the exact text incl. float viewport is golden-captured and hand-verified once.
  - JSON string escaping unit: `flow__json_str` into `tmpfile()` for inputs containing quote, backslash, newline, tab, and a control char (`\x01`); rewind+read; `ASSERT_STR` each equals the expected escaped form.
  - Structure round-trip: `flow_save` graph A, `flow_load` into a fresh `flow_t` B (`flow_register_defaults`); `ASSERT_INT` node_count/edge_count equal; each node id/pos.x/pos.y/parent equal; `ASSERT_STR` each node->type; each edge source_handle/target_handle/type; `ASSERT_INT` source/target.
  - Viewport FLOAT round-trip (NEW): set view to ox=12.5, oy=-7.25, zoom=2.0, save, load into B, `ASSERT` view.ox/oy/zoom within 1e-4 — proves float persistence, not `zoom==1`.
  - Custom-data round-trip (headline): register a 'device' type with `dev_save`/`dev_load` mirroring topo's 5-field struct; add a node whose device has all fields populated, including one value with a quote and a space; save, load into B; `ASSERT_STR` every device field round-tripped AND `ASSERT_INT` recomputed w/h match A's (proves `load()` ran then `flow_measure_node` re-ran on the restored data).
  - Edge label round-trip: set an edge label (heap via `flow_set_edge_label`), save, load, `ASSERT_STR` label equal; also assert a NULL-label edge omits the `"label"` key (strstr the JSON) and loads back as `label==NULL` (no `""` artifact, no crash).
  - Edge handles/type round-trip: add an edge with non-empty source/target handle and a non-default edge type string; round-trip; `ASSERT_STR` all three.
  - Id preservation + bump: load a graph whose max node id is 7 and max edge id is 4; `ASSERT` next `flow_add_node` id==8 and next `flow_add_edge` id==5.
  - Reset-on-load: pre-populate B with junk nodes/edges + a heap edge label, `flow_load` over it; `ASSERT` B's contents equal A's and (under ASan) the pre-existing edge label was freed.
  - Failure paths: `flow_load(nonexistent)` returns -1 and leaves f unchanged; `flow_load` on a truncated/garbage file returns -1 without crashing (ASan: no over-read past buffer end — feed a buffer ending mid-string and mid-number).
  - Ephemeral-not-persisted: select a node + drive a drag via `flow_feed`, set zoom limits, save; strstr to `ASSERT` it contains none of: `"flags"`, `"selected"`, `"zmin"`, `"zmax"`, `"marquee"`, `"w":`, `"h":`.
  - Empty graph: `flow_new` + `flow_register_defaults`, no nodes/edges, save → valid JSON with empty arrays; load into B → count 0, no crash.
  - save→load→save byte-identical: save A to f1, load into B, save B to f2, `ASSERT_STR` f1==f2 (the float-precision guard for `%.9g`).
  - `make test` green; ASan/UBSan gate: `cc -std=c11 -fsanitize=address,undefined -fno-sanitize-recover=all -g tests/test_json.c -o /tmp/tj -lm && /tmp/tj` reports 0 errors; remove temp JSON files at end of main.

**Acceptance.**
  - `make test` passes with `test_json` in the Makefile TESTS list; all snapshots reviewed and committed (`json_basic.txt`).
  - A manual ASan+UBSan build of `tests/test_json.c` runs clean: no leaks from `load()`-allocated `node->data` within the test, no leak of pre-existing edge labels across `flow_load`'s reset, no heap-buffer-overflow on garbage/truncated input, no UB.
  - `flow.h` is regenerated by `make` and contains the `src/flow_json.h` section; `tools/amalgamate.sh` lists `flow_json` AFTER `flow_render` and BEFORE `flow_input`.
  - Round-trip is lossless for ALL durable fields incl. FLOAT viewport (ox/oy/zoom), node parent, and edge sourceHandle/targetHandle/type/label: a save→load→save cycle produces byte-identical JSON.
  - Ephemeral state never appears in the output JSON.
  - The two new trailing hooks do not break any existing initializer: `flow_default_node_type` and topo's `DEVICE` both still compile (5-field init, trailing zero-fill); every existing test/demo builds unchanged.
  - `flow_load` on a missing or malformed file returns -1 and leaves the engine usable (no partial corruption / no crash).
  - topo demo loads its graph from a JSON file when present and saves on quit, with device data fully restored via `dev_save`/`dev_load` wired into the `DEVICE` initializer; demo builds with `make demos`.

**Depends on.** nothing (the one true root — only the settled inc-2 surface: float `flow_viewport`, edge handle/type/label fields, node `parent`, and the `flow_node_type` shape `{type,measure,render,handles,handle_count}`).
**Conflicts with.**
  - **undo-redo** — `flow_load`'s reset must also clear the undo journal (`flow__journal_clear`) once undo lands, or undo inverts against a replaced graph; whoever lands second wires it.
  - Any future package that ADDS a durable field to `flow_node`/`flow_edge` — must extend the JSON schema + reader/writer in lockstep.
  - Any future package that ADDS fields to `flow_node_type` after save/load — shares the trailing-struct-append surface; order the appends so initializers stay zero-init-valid.
  - `demos/topo.c` — appended to by several showcase packages; defer all topo wiring to a single final integration pass.

**Carry-overs fixed.**
  - Adds the private graph-reset path (`flow__graph_reset`) that `flow_load` needs — fills the gap that `flow_free` was the only teardown; explicitly documents that `node->data` ownership stays app-side (library never frees `void*` node/edge data).
  - Activates persistence of the now-real durable surface that did not exist when the original inc-2 spec was written: float viewport zoom (not `zoom==1`), edge source/target handles + type + label, and node parent.

---

### 2. STRAIGHT edge type (built-in second router)  `[S]`  ·  id: `straight-edge`

**Goal.** Add the spec's second built-in edge type. §5 lists TWO built-ins — "a default orthogonal edge AND a
straight edge" — but only `flow_default_edge_type` (orthogonal) is registered (`src/flow_route.h:7`,
`src/flow_types.h:29`). Add a `flow_route_straight` router and register a
`flow_edge_type { "straight", flow_route_straight }` so any edge whose `type[16]` is `"straight"` draws a direct,
grid-stepped line (Bresenham/diagonal stepping) from source anchor to target anchor with an arrowhead, instead
of the orthogonal box-drawing path.

**User value.** A user (or app author) can set an edge's `type` to `"straight"` and get a clean diagonal-stepped
direct connector — the listed-but-unbuilt second built-in — without writing a custom router. The orthogonal type
remains the default; both ship registered by `flow_register_defaults`.

**Files touched.**
  - src/flow_route.h
  - src/flow_types.h
  - tests/test_route.c
  - tests/test_render.c
  - tests/snapshots/render_straight_edge.txt
  - Makefile (only if a dedicated test_straight is added; otherwise extend test_route/test_render)

**Entry points (existing functions to extend).**
  - `flow_route_orthogonal` (`src/flow_route.h:31`) — sibling template: same `void route(flow_pt s, flow_pos sp, flow_pt t, flow_pos tp, flow_route *out)` signature, same `flow_route_push` + `out->label_anchor` contract
  - `flow_route_push` (`src/flow_route.h:9`) — used per stepped cell
  - `flow_default_edge_type` (`src/flow_route.h:7`) — the existing registered instance; add a parallel `flow_straight_edge_type`
  - `flow_register_defaults` (`src/flow_types.h:29`) — currently registers `flow_default_edge_type`; add a `flow_register_edge_type(f, &flow_straight_edge_type)` call
  - `flow_render` edge loop (`src/flow_render.h:159`) — NO edit needed: it already resolves `et = flow_edge_type_for(f, e->type)` and calls `et->route`, so a registered type is picked up automatically

**API additions.**
```c
/* ---- src/flow_route.h ---- */
void flow_route_straight(flow_pt s, flow_pos sp, flow_pt t, flow_pos tp, flow_route *out);
/* direct line s->t, stepped to the cell grid (diagonal steps allowed), arrowhead glyph at the target end;
   sets out->label_anchor to the path midpoint. Same heap/route-cell ownership contract as flow_route_orthogonal. */
extern const flow_edge_type flow_straight_edge_type;  /* { "straight", flow_route_straight } */
```

**Design notes.**

The edge-type vtable already exists and is keyed by the edge's `type[16]` string. `flow_render`'s edge loop
(`src/flow_render.h:159`) and `flow_hit_edge` both resolve the router via `flow_edge_type_for` — so registering a
second type is sufficient; no render or hit-test edit is required (the orthogonal path already proved this seam).

ROUTER: implement `flow_route_straight` with integer DDA / Bresenham stepping between the source and target
SCREEN cells (the renderer passes screen-projected `s`/`t`, same as orthogonal). Allow diagonal steps (move x
and y together when both deltas are nonzero) so the line reads as direct rather than staircased. Choose glyphs
from the box-drawing/line set already used by orthogonal: `─` for horizontal runs, `│` for vertical, and a
diagonal approximation (`╲`/`╱` U+2572/U+2571, or fall back to the nearest of `─`/`│`) for diagonal steps —
verify codepoints render in the diff emitter (only `.ch` matters for snapshots). Arrowhead at the target end
matching the orthogonal type's convention (e.g. `▶`/`◀`/`▲`/`▼` by approach direction, or the existing arrow
glyph orthogonal uses). Set `out->label_anchor` to the geometric midpoint so edge labels (from
`edge-interaction`) still place correctly. Free/own `out->cells` exactly as `flow_route_orthogonal` does (the
renderer frees `rt.cells` each frame). Degenerate cases: zero-length (s==t) pushes a single cell; a pure
horizontal or vertical line reduces to the same glyphs orthogonal would use.

REGISTRATION: add `flow_straight_edge_type` next to `flow_default_edge_type` and register it in
`flow_register_defaults`. The default `type[16]` on a new edge stays `""` → resolves to `flow_default_edge_type`
(orthogonal), so existing graphs are unaffected; only edges explicitly set to `"straight"` use the new router.

NO struct flow change, NO module, NO `flow_handle_mouse`/`flow_feed` edit — this is purely additive to the
route/types layer. Independent of every other inc-3 package.

**Test plan.**
  - tests/test_route.c (extend): call `flow_route_straight` for known endpoints — a pure-horizontal pair (assert the cell run is `─` glyphs at the right y), a pure-vertical pair (`│`), and a true diagonal (assert stepped cells progress monotonically in both x and y and the count ≈ max(|dx|,|dy|)+1). Assert `out->label_anchor` ≈ midpoint. Assert s==t pushes exactly one cell. Free `out.cells` each case (ASan).
  - tests/test_render.c (extend): two nodes A(10,5) B(30,12); add an edge, `set type "straight"`; render; SNAPSHOT("render_straight_edge", str) — eyeball a direct diagonal connector with an arrowhead, distinct from the orthogonal golden. Also assert an edge left at default type still renders orthogonal (regression: existing `render_two_edge` snapshot unchanged).
  - Registration: `flow_register_defaults` then assert `flow_edge_type_for(f, "straight")` is non-NULL and `flow_edge_type_for(f, "")` resolves to the orthogonal default.
  - `make test` green; ASan/UBSan gate on test_route/test_render — no leaks of route cells, no UB from the stepping loop.

**Acceptance.**
  - An edge with `type=="straight"` renders as a direct grid-stepped line with an arrowhead; an edge left at the default type still renders orthogonal (no regression to `render_two_edge` / existing route asserts).
  - `flow_straight_edge_type` is registered by `flow_register_defaults`; `flow_edge_type_for(f, "straight")` resolves it.
  - `flow_route_straight` sets `out->label_anchor` so labels still place; owns/frees `out->cells` per the existing route contract (ASan clean).
  - `make test` passes including the new snapshot; `flow.h` regenerated by amalgamate (no module added; edits in `flow_route.h`/`flow_types.h` only).

**Depends on.** nothing (independent of the four pillars — purely additive to the route/types vtable layer).
**Conflicts with.**
  - Any package that rewrites the `flow_render` edge loop or `flow_hit_edge` route resolution — none in this set does (groups touches the NODE loop, not the edge loop).

**Carry-overs fixed.**
  - Builds the spec §5 "straight edge" built-in that was listed but never registered (only orthogonal shipped) — closes the documented two-built-in-edge-types gap.

---

### 3. Subflows / groups — parent-relative coordinates + group container nodes  `[XL]`  ·  id: `groups`

**Goal.** Make `flow_node.parent` a fully live editing dimension: child positions become parent-relative, a
built-in `group` container node renders a frame around its children, `flow_group`/`flow_ungroup`/`flow_set_parent`
mutators manage membership with coordinate conversion, dragging a group moves its subtree, and hit-test + render
z-order + marquee + drag all respect nesting. `flow_move_node` keeps its **ABSOLUTE-in** contract (converts to
relative internally) so every existing top-level caller and test is byte-identical, while child nodes now move
correctly. The flat-world-delta multi-drag is reworked to move only **selection roots** so a parent+child
selection no longer double-moves the child.

**User value.** A user can box up a set of nodes into a labeled group container, drag the whole group (children
follow), drag a loose node onto a group to nest it (it stays visually put) or out to detach it, and ungroup to
flatten without losing the children. Children render clipped inside their parent's frame and never get hit-stolen
by the container behind them. This is the foundational subflow capability xyflow bundles, and it unblocks undo's
`reparent` command and auto-layout's "respect group local space".

**Files touched.**
  - src/flow_model.h
  - src/flow_types.h
  - src/flow_render.h
  - src/flow_input.h
  - src/flow_cell.h
  - Makefile
  - tests/test_groups.c
  - demos/flowchart.c (groundwork; full demo lands in flowchart-demo)
  - flow.h (GENERATED — produced by `make flow.h`, never hand-edited)

**Entry points (existing functions to extend).**
  - `src/flow_model.h`: `flow_set_parent` / `flow_group` / `flow_ungroup` (new mutators); `flow_move_node` (`:192-194`, KEEP absolute-in, convert abs→relative internally); `flow__node_order` (new depth-aware ordering helper); `flow_hit_node` (descendants-first ordering); `flow_is_ancestor` / `flow__node_depth` (new helpers)
  - `src/flow_types.h`: `flow_group_node_type` + `flow__group_measure`/`flow__group_render`; `flow_register_defaults` (`:26-29`) registers `group`
  - `src/flow_render.h`: `flow_render` node draw loop (`:170-186`, parent-before-child ordering dominates selected-last among siblings) + per-node surface clip to ancestor frames; `flow__minimap` unaffected (already uses `flow_node_abs`)
  - `src/flow_input.h`: `flow_handle_mouse` MULTI-DRAG branch (`:162-169`, apply delta to selection ROOTS only); RELEASE branch (`:179-218`, drag-over-group reparent on drop / detach)
  - `src/flow_cell.h`: `flow_surface` (`:8`) gains clip fields; `flow_put` (`:61`) honors them

**API additions.**
```c
/* ---- src/flow_model.h (declarations in the non-impl section, defs under FLOW_IMPLEMENTATION) ---- */

/* Reparent `child` under `parent` (-1 detaches to top level). Converts child->pos
   so its ABSOLUTE world position is unchanged across the move:
   new_rel = flow_node_abs(child) - flow_node_abs(parent)  (parent==-1 => new_rel = abs).
   Rejects cycles: no-op if parent==child or parent is a descendant of child, or if
   either id is missing. Array order/ids unchanged (parent is a field flip). */
void flow_set_parent(flow_t *f, int child, int parent);

/* Create a `group` container node whose frame encloses the given node ids (world bbox
   + padding), reparent each given id under it (flow_set_parent, preserving abs), and
   return the new group's id (-1 if count<=0 or all ids missing). The group's w/h are
   set from the enclosing bbox AFTER add (flow_measure_node does NOT clobber them — the
   group type's measure is a no-op that leaves caller-set w/h intact). */
int  flow_group(flow_t *f, const int *ids, int n);

/* Dissolve a group: reparent every direct child OUT to the group's own parent
   (flow_set_parent(child, group->parent), preserving abs), THEN remove the now-childless
   container. Children SURVIVE (the critical difference from flow_remove_node, which
   cascade-deletes children). No-op if id is missing or not a group-type node. */
void flow_ungroup(flow_t *f, int id);

/* Walk the parent chain: 1 if `maybe_ancestor` is `node` or any ancestor of `node`. */
int  flow_is_ancestor(flow_t *f, int maybe_ancestor, int node);

/* Relative (stored) position accessor — spec §9; trivial getter, the absolute
   companion flow_node_abs already exists at src/flow_model.h:216. */
flow_pt flow_node_pos(const flow_node *n);

/* ---- src/flow_types.h ---- */
extern const flow_node_type flow_group_node_type;   /* built-in "group"; registered by flow_register_defaults */

/* ---- src/flow_cell.h: flow_surface gains a clip rect (in BUFFER coords). Default
   {0,0,cb->w,cb->h} = no extra clip, so every existing surface initializer that
   aggregate-inits must be updated; flow_put gains a clip-rect test in addition to the
   existing logical (w/h) and physical (cb bounds) clips. ---- */
struct flow_surface { flow_cellbuf *cb; int ox, oy, w, h; int clip_x, clip_y, clip_w, clip_h; };
```

**Design notes.**

THE HEADLINE DECISION — `flow_move_node` stays ABSOLUTE-in. Verified against every call site:
`flow_add_node_center` passes world-abs (top-level); single-drag (`src/flow_input.h:173`) passes
`w - drag_grab` (an absolute target); multi-drag (`:166-167`) passes `pos+delta`; every existing test is
top-level where abs==relative. So the only change to `flow_move_node` (`src/flow_model.h:192-194`): look up the
node, compute `parent_abs` (`flow_node_abs` of the parent, or `{0,0}` if `parent==-1`), store
`n->pos = {pos.x - parent_abs.x, pos.y - parent_abs.y}`. For top-level nodes `parent_abs=={0,0}` so this is
byte-identical to today (`n->pos = pos`) — zero migration for all current callers/tests. Strict superset, and it
makes drag-to-reparent stable (keep feeding absolute mouse coords; the node stays under the cursor as set_parent
recomputes relative).

THE BIGGEST TRAP — array order is no longer a valid z-order or hit-order once groups exist. `flow_group` appends
the container at the array tail ⇒ highest index ⇒ currently drawn LAST (topmost) and hit FIRST. Both are
backwards: render needs parent-BEFORE-child (frame under children); hit-test needs child-before-parent (topmost
first) so a group never steals hits from its own children. FIX: introduce a depth-aware visit order. Cheapest
correct approach that PRESERVES the array (so `flow_get_node`/ids/serialize are untouched): build a transient
index list each pass, ordered by a stable key — depth ascending for render / depth descending for hit, ties
broken by array index (preserves insertion order within a depth, which selected-last and existing snapshots
depend on). `depth` = length of parent chain (`flow__node_depth`). For render's selected-last requirement:
parent-before-child must DOMINATE; selected-last applies only AMONG SIBLINGS (same parent). Concretely the render
loop (`src/flow_render.h:170-186`) becomes: visit in (depth asc, then unselected-before-selected, then array
index) order — a selected group still draws under its children because children are deeper. This is the
centerpiece; it looks free but isn't. (N nodes ⇒ O(N²) chain-walks for depth are fine at this scale.)

CLIPPING — `flow_surface` model: ox/oy is the physical buffer offset, w/h is the logical box size, and `flow_put`
(`src/flow_cell.h:61`) already does logical clip (x<w) + physical clip (cb bounds). To clip a child to its
ancestor frames we must clip the TOP/LEFT too, which shrinking w/h cannot do without moving the origin (breaking
renderers that draw at local (0,0)). DECISION: add a clip rect (clip_x/clip_y/clip_w/clip_h in BUFFER coords) to
`flow_surface` and AND it into `flow_put`. The render loop computes each node's clip as the buffer-space
intersection of all ancestor footprints (`flow__node_footprint` walks abs already, so this composes with zoom for
free) and the screen rect; top-level nodes get the full-screen clip (no behavior change). MIGRATION COST: every
aggregate initializer of `flow_surface` must add the 4 clip fields — audit `flow_render.h` (node surf, minimap s,
overlay ov, statusbar s) and the minimap inner `flow_surface s`; topo.c `on_overlay` receives a `flow_surface*`
(no init — safe). Set clip to full-buffer for all non-node surfaces. Re-run ALL snapshot tests (test_render,
test_marquee, etc.) — goldens MUST NOT move for top-level-only scenes.

GROUP NODE TYPE — `flow__group_render` draws just a box frame (+ optional label from `node->data`) and at LOD
draws a collapsed marker like default; `flow__group_measure` is a NO-OP (leaves caller-set w/h intact) because
`flow_add_node` calls `flow_measure_node` which would otherwise clobber the bbox-derived size. `flow_group`
therefore: add the node, then set w/h from the bbox, then reparent the members. Group `handle_count = 0`
(containers aren't connectable in v1) so `flow_hit_handle`/edge anchoring is untouched.

MULTI-DRAG double-move fix (carryover from `src/flow_input.h:162-169`): the flat per-node world delta is correct
ONLY for top-level nodes. With nesting, if a parent and its descendant are both selected, moving the parent
ALREADY moves the child via relative coords — applying the delta to the child too double-moves it. FIX: in the
multi-drag branch, apply the delta only to SELECTION ROOTS = selected nodes that have NO selected ancestor
(`flow_is_ancestor` check against the selected set). Still uses `flow_move_node` (absolute-in:
`pos = node_abs + delta`), so a root's children follow for free.

DRAG-TO-REPARENT (`flow_handle_mouse` release, `src/flow_input.h:179-218`): on a single-node drag release,
hit-test the drop point for a `group` node under the node (`flow_hit_node`, skipping the dragged node and its own
descendants to avoid self-parenting); if a group is hit and differs from the current parent,
`flow_set_parent(dragged, group)` — abs preserved so it stays put. If dropped on empty pane while currently
parented, optionally detach to top level (`flow_set_parent(dragged, -1)`). Conservative for v1: reparent on drop
only for single-node drags (multi-node reparent deferred), and only target `group`-type nodes, matching xyflow.

set_parent cycle guard: reject if `parent==child` or `flow_is_ancestor(f, child, parent)`. `flow_remove_node`
(`src/flow_model.h:350`) already cascades children and re-finds by id each step — unchanged, still correct with
deeper trees.

NO new DURABLE node fields: `parent` already exists and (once serialize lands) is persisted, so the JSON schema
is UNCHANGED — serialize is orthogonal. The only load-time obligation is registering the `group` type (the app's
responsibility, like any custom type) — note this, but it requires no code here.

**Test plan.**
  - tests/test_groups.c (new; add to Makefile TESTS=). Build gate: `make test` PLUS `cc -std=c11 -fsanitize=address,undefined -fno-sanitize-recover=all -g tests/test_groups.c -o /tmp/g -lm && /tmp/g`.
  - `flow_move_node` absolute-in invariant: top-level node → after `flow_move_node(id,P)`, `flow_node_abs == P` AND `n->pos == P` (byte-identical to pre-change). Child node under group at parent_abs G → `flow_move_node(child, P)` yields `flow_node_abs(child)==P` and `n->pos == P-G`.
  - `flow_set_parent` preserves abs: node at world W, parent group at world G; `flow_set_parent(node,group)` → `flow_node_abs(node)` still == W and `n->pos == W-G`. Detach (parent -1) → `n->pos == W`.
  - `flow_set_parent` cycle guard: `flow_set_parent(a,a)` no-op; build a→b→c chain then `flow_set_parent(a,c)` no-op; assert chain intact.
  - `flow_group`: group three nodes → returns id>0, all three have `parent==gid`, each node's `flow_node_abs` unchanged, group w/h enclose the bbox (+padding); `flow_node_count` increased by exactly 1.
  - `flow_ungroup` SURVIVAL regression (critical): group two nodes, ungroup → both children SURVIVE (`flow_get_node` non-NULL), their parent reset to the group's old parent, abs unchanged, group node gone, count decreased by exactly 1. Contrast control: `flow_remove_node(gid)` DELETES both children (assert count drops by 3).
  - Multi-drag selection-roots (no double-move): parent group + its child both selected; simulate multi-drag delta D via `flow_handle_mouse` motion → child abs shifts by exactly D, NOT 2D; an unrelated selected top-level node also shifts by D.
  - Hit-test ordering: child node fully inside a group frame → `flow_hit_node` over the child returns the CHILD id, not the group. A cell on the group frame but outside any child returns the group.
  - `flow_node_abs` nesting depth: a→b→c (3 levels), assert `flow_node_abs(c)` == sum of the three relative positions.
  - SNAPSHOT (flow_render): a group framing two child nodes — golden auto-captured first run; assert children render clipped inside the frame (a child positioned to overflow is cut at the frame edge) and the group frame draws UNDER the children. Separate SNAPSHOT: selected group + unselected children — children still draw on top of the (selected) group frame.
  - Regression: re-run the full existing suite — test_render/test_mouse/test_select/test_marquee/test_zoom/test_model goldens and asserts MUST be unchanged (all current scenes are top-level ⇒ abs==relative, full-screen clip ⇒ byte-identical).

**Acceptance.**
  - `flow_move_node` retains its absolute-in contract: every existing top-level caller and test passes byte-for-byte unchanged; child nodes move correctly (abs preserved).
  - `flow_set_parent`, `flow_group`, `flow_ungroup`, `flow_is_ancestor`, `flow_node_pos` exist with the documented signatures; `flow_ungroup` preserves children (does NOT cascade-delete them); cycles are rejected.
  - Built-in `group` node type is registered by `flow_register_defaults`; renders a clipping frame around its children; its measure does not clobber caller-set bbox size.
  - Render z-order draws parent-before-child with selected-last applying only among siblings; hit-test returns the topmost descendant (a group never steals its own child's hit).
  - Children render clipped to their ancestor frame(s) at any zoom (clip composes through `flow__node_footprint` / unified projection).
  - Multi-drag moves only selection roots: a parent+child both selected move the child by exactly the delta (no double-move).
  - Single-node drag onto a `group` reparents it on drop (stays visually put); drop on empty pane detaches a parented node.
  - `make test` passes AND the new test_groups.c passes clean under ASan/UBSan; all pre-existing snapshot goldens are unmoved.
  - `flow.h` is regenerated via `make flow.h` (amalgamation order unchanged: new helpers live in `flow_model.h` before `flow_render.h`/`flow_input.h`); `flow.h` is never hand-edited.

**Depends on.**
  - settled inc-2 surface: unified projection + `flow__node_footprint` / `flow__handle_screen` / `flow__edge_screen_ends` (clip + child footprints leverage these)
  - settled inc-2 surface: `flow_node_abs` parent-chain walk (`src/flow_model.h:216`, already additive; groups makes it load-bearing for editing)
  - settled inc-2 surface: `flow_render` two-pass selected-last draw order (`src/flow_render.h:170-186`; groups generalizes it to depth-then-selected)
  - settled inc-2 surface: `flow_remove_node` cascade (`:350`; `flow_ungroup` must AVOID it); multi-node group drag in `flow_handle_mouse` (`:162-169`; groups reworks its flat-delta to selection-roots)
  - NONE of the other inc-3 packages — groups is foundational and lands FIRST in the spine (undo's reparent command and auto-layout's group-local-space both depend on groups' settled mutators; serialize is orthogonal — parent is already durable).

**Conflicts with.**
  - **undo-redo** — must wrap the reparent/group/ungroup mutators groups introduces (build undo AFTER groups so it journals the settled set as `FLOW_CMD_REPARENT`).
  - **auto-layout** — "respects group local space" depends on groups' parent-relative coords; sequence after groups.
  - **events / space-pan / auto-pan** — all edit `flow_handle_mouse` press/motion/release that groups reworks; serialize after groups.
  - any package editing `flow_surface` in `src/flow_cell.h` (groups adds clip fields).

**Carry-overs fixed.**
  - `flow_move_node` TOP-LEVEL-ONLY limitation (`flow_model.h:194` comment) — now converts absolute-in to parent-relative internally, so child nodes move correctly while top-level callers stay byte-identical.
  - multiselect handoff note (inc-2 plan line 350) "multi-drag applies the same per-node delta, correct for top-level nodes" — multi-drag now applies the delta to SELECTION ROOTS ONLY, fixing the parent+child double-move.
  - spec §9 sub-flows: `flow_node_pos` / `flow_set_parent` / group container / drag-to-reparent / children clip to parent rect — the previously-unimplemented grouping surface is now wired (coordinate-extent clamping is the one §9 item left for a follow-up).

---

### 4. Events / observer callbacks: on_node_dblclick, on_selection_change, on_nodes_delete  `[M]`  ·  id: `events`

**Goal.** Complete the spec §8 `flow_callbacks` observer set. The struct ships 5 of 7 callbacks
(`src/flow_model.h:103-107`: on_overlay / on_node_context / on_node_click / on_pane_click / on_connect). Add the
three missing: `on_node_dblclick` (double-click detection in `flow_handle_mouse` via click-timing + same-cell),
`on_selection_change` (fired whenever the FLOW_SELECTED node set changes — from every select/toggle/clear/marquee/
delete path), and `on_nodes_delete(ids, n)` (fired from `flow_delete_selection` / `flow_remove_node`).

**User value.** App authors get the full observer API: react to a node double-click (e.g. open an editor), keep a
side panel in sync as the selection changes, and clean up app-side resources when nodes are deleted — without
polling. This closes the spec's events/observer gap (the shipped surface had only the 5 click/connect/overlay
callbacks).

**Files touched.**
  - src/flow_model.h
  - src/flow_input.h
  - tests/test_events.c
  - Makefile
  - demos/topo.c
  - flow.h

**Entry points (existing functions to extend).**
  - `flow_callbacks` (`src/flow_model.h:103-109`) — append the three fn-ptr fields (trailing zero-init keeps existing aggregate initializers valid)
  - `flow_select_node` / `flow_toggle_node` / `flow_clear_selection` / `flow_select_in_rect` (`src/flow_model.h`) — fire `on_selection_change` AFTER the FLOW_SELECTED set actually changes
  - `flow_delete_selection` (`:372`) / `flow_remove_node` (`:350`) — fire `on_nodes_delete` with the removed ids; `flow_delete_selection` already snapshots selected ids (`:375`) — collect them for the callback before removal
  - `flow_handle_mouse` (`src/flow_input.h`, release/click path `:179-218`) — add double-click detection (timestamp + last-click cell/id) on the node-body click arm
  - `struct flow` (`src/flow_model.h:121-141`) — add `last_click` time + `last_click_node` for dblclick timing (additive fields)

**API additions.**
```c
/* ---- appended to struct flow_callbacks in src/flow_model.h ---- */
void (*on_node_dblclick)(flow_t *f, int node, void *user);            /* left double-click on a node body */
void (*on_selection_change)(flow_t *f, const int *ids, int n, void *user); /* fired after the FLOW_SELECTED node set changes; ids in insertion order */
void (*on_nodes_delete)(flow_t *f, const int *ids, int n, void *user);     /* fired with the node ids about to be / just removed */
```

**Design notes.**

CALLBACK STRUCT APPEND: `flow_callbacks` (`src/flow_model.h:103-109`) is set wholesale via
`flow_set_callbacks(f, cb)` (`:294`). Appending three trailing fn-ptr fields keeps every existing aggregate
initializer valid (C zero-fills trailing members) and the topo demo's callbacks struct unaffected. All three are
optional — guard each fire with `if (f->cb.X)`.

on_selection_change — the SUBTLE one. It must fire from EVERY path that mutates the FLOW_SELECTED node set:
`flow_select_node` (replace + additive), `flow_toggle_node`, `flow_clear_selection`, `flow_select_in_rect`
(marquee, called per-motion), and the delete paths (selection shrinks when nodes vanish). Fire AFTER the mutation
and only if the set actually changed (compare a cheap signature — e.g. selected-count + a running XOR/sum of
selected ids — before vs after, or a dirty flag set by the FLOW_SELECTED writes) to avoid spurious fires on
no-op selects. CRITICAL for the marquee live-select path: `flow_select_in_rect` runs every motion event — fire
only when the set changed that motion, or the callback floods. Build the `ids` array from a single insertion-order
scan into a small stack/scratch buffer (cap to nnodes); pass `(ids, count)`.

on_nodes_delete — `flow_delete_selection` (`:372-377`) loops `flow_selected_node` then `flow_remove_node`;
collect the selected node ids into a scratch array BEFORE the removal loop and fire once with the full set (so a
multi-delete is one callback, not N). `flow_remove_node` called directly (not via delete_selection) should also
fire for the cascade it removes — but to avoid N fires during `flow_delete_selection`'s loop, gate `flow_remove_node`'s
own fire behind a `suppress`-style flag set by `flow_delete_selection` (mirror the pattern undo will use), OR
simpler: have `flow_delete_selection` fire the aggregate and have `flow_remove_node` fire only its own
(direct-call) removals — document the chosen representation. The cascade children removed by `flow_remove_node`
SHOULD appear in the ids it reports (the app may hold resources for them).

on_node_dblclick — detect in `flow_handle_mouse` on the node-body click arm (a press+release on the same node
with no drag, `src/flow_input.h:179-218`). Track `last_click_node` + a timestamp (`struct flow` additive fields);
if a second click on the SAME node arrives within a threshold (e.g. ~400ms — but tests have no wall clock, so
detect via a click-COUNT or feed two clicks and assert the second fires dblclick when the first was on the same
node; keep the timing source mockable/simple). Fire `on_node_dblclick` AND still fire the normal `on_node_click`
for the first click (xyflow fires both click and dblclick) — document the firing order. Do NOT change selection
semantics; dblclick is purely observational.

NO new mutators, NO module, NO render edit. The only `flow_handle_mouse` change is dblclick bookkeeping on the
existing click arm — coordinate with groups (which also edits the release arm) and sequence AFTER groups settles
drag classification, so the dblclick detection layers on the final click path.

**Test plan.**
  - tests/test_events.c (new; per-file FLOW_IMPLEMENTATION + flowtest.h; added to Makefile TESTS).
  - on_selection_change fires on select/toggle/clear: register a counting cb; `flow_select_node(a)` → fires once with `{a}`; `flow_toggle_node(b)` → fires with `{a,b}`; `flow_toggle_node(a)` → fires with `{b}`; `flow_clear_selection` → fires with `{}`; a no-op `flow_select_node(b)` when b already the sole selection does NOT fire (changed-only gate).
  - on_selection_change from marquee: shift-drag a box over two nodes via `flow_feed` SGR; assert the cb fires (once per actual change, not per motion event with no change) and the final ids match the enclosed set.
  - on_nodes_delete aggregate: select 2 nodes, `flow_delete_selection` → on_nodes_delete fires ONCE with both ids (order = insertion). `flow_remove_node(parent-with-child)` directly → on_nodes_delete reports the cascade set (parent + child).
  - on_node_dblclick: feed two clicks on the same node cell via `flow_feed`; assert on_node_dblclick fired once (and on_node_click fired for the first); two clicks on DIFFERENT nodes do NOT fire dblclick.
  - Optional-callback safety: NULL callbacks (default zero-init) cause no crash on any of the above paths.
  - Existing-init regression: the inc-2 callbacks aggregate initializers (topo.c, tests) still compile with the three appended trailing fields.
  - `make test` green; ASan/UBSan gate on test_events — no leaks from the scratch id buffers (use stack or free), no UB.

**Acceptance.**
  - `flow_callbacks` gains `on_node_dblclick` / `on_selection_change` / `on_nodes_delete` as trailing fields; every existing aggregate initializer still compiles.
  - `on_selection_change` fires after EVERY selection-set change (select/toggle/clear/marquee/delete) and ONLY when the set actually changed (no spurious fires, no per-motion flood).
  - `on_nodes_delete` fires once per delete operation with the full removed id set (a multi-delete is one callback; a cascade reports its children).
  - `on_node_dblclick` fires on a second click on the same node within the dblclick window; click and dblclick both fire for the qualifying second click.
  - All three callbacks are optional (NULL-safe); selection/delete semantics are otherwise unchanged (no regression to test_select/test_marquee/test_keys).
  - `make test` passes including test_events; ASan/UBSan clean; `flow.h` regenerated by amalgamate.

**Depends on.**
  - **groups** (ordering) — events edits `flow_handle_mouse`; let groups settle drag classification first (same-file serial contention).
  - settled inc-2 selection mutators (`flow_select_node`/`flow_toggle_node`/`flow_clear_selection`/`flow_select_in_rect`) and `flow_delete_selection`/`flow_remove_node` — instrumented in place.

**Conflicts with.**
  - **undo-redo** — records inverses in the SAME selection/delete mutators events fires from; settle events first so undo instruments already-firing paths.
  - Any package editing `flow_handle_mouse` click/release classification (groups, space-pan, auto-pan) — keep the dblclick bookkeeping on the final click arm.

**Carry-overs fixed.**
  - Completes the spec §8 observer API: adds the three callbacks (`on_node_dblclick`, `on_selection_change`, `on_nodes_delete`) the shipped 5-callback struct omitted — the events/observer gap the audit flagged.

---

### 5. Space-drag pan  `[S]`  ·  id: `space-pan`

**Goal.** Implement the spec §8 interaction-table "Space-drag" pan trigger. Holding Space forces a drag — on a
node OR on the empty pane — to PAN the viewport instead of moving the node / selecting, matching the common
design-tool convention (and xyflow's `panOnDrag` + space gesture).

**User value.** A user can hold Space and drag anywhere (including over nodes) to pan the canvas, without
accidentally moving a node or starting a marquee — the standard "grab the canvas" gesture.

**Files touched.**
  - src/flow_input.h
  - src/flow_run.h
  - src/flow_model.h
  - tests/test_space_pan.c
  - Makefile
  - flow.h

**Entry points (existing functions to extend).**
  - `flow_feed` (`src/flow_run.h`) — track Space key-down / key-up to set a `space_held` modifier on `struct flow` (terminals send no key-up, so handle this pragmatically — see notes)
  - `flow_handle_mouse` (`src/flow_input.h`, press classification `:74-130`) — when `space_held`, force the pan path on press regardless of whether a node is under the cursor
  - `struct flow` (`src/flow_model.h:121-141`) — add `int space_held` (additive)

**API additions.**
```c
/* No new PUBLIC engine function is strictly required — Space routes through the existing
   flow_dispatch_key built-in path and the modifier lives on struct flow. Optional convenience: */
void flow_set_space_pan(flow_t *f, int enabled);  /* optional: force/clear the pan-on-drag mode programmatically (mirrors the Space toggle) */
/* struct flow gains an additive field: int space_held;  (zero-init via calloc = off) */
```

**Design notes.**

THE TERMINAL CONSTRAINT (the honest hard part): a TTY in raw mode delivers Space as a single byte (`0x20`) on
PRESS but has NO key-up event — so a literal "hold Space" cannot be tracked the way a GUI would. Two viable
models, pick and document:
  (A) **Space as a sticky pan-mode toggle** — pressing Space toggles `space_held`; while on, the next drag pans
      and (xyflow-like) the cursor/status hints "pan mode"; pressing Space again (or Esc) exits. Deterministic
      and testable.
  (B) **Space-armed for the next gesture** — pressing Space arms `space_held=1`; the next press consumes it
      (pan that drag), then auto-clears on release.
  Recommend (A) for predictability and easy testing; expose it through `flow_dispatch_key` so a binding can
  override (registry-before-builtin, `src/flow_model.h:412`). Document the deviation from a true GUI hold —
  this is the terminal-faithful analog, not a literal modifier.

INPUT WIRING: in `flow_handle_mouse` PRESS (`src/flow_input.h:74`), if `f->space_held`, skip handle/node/edge
classification and arm `dragging_pan` (the existing pan path the empty-pane press already uses) so a drag pans;
on release, clear the per-gesture state as today (and, for model B, clear `space_held`). Plain (no-Space) presses
are entirely unchanged — the only added branch is `if (f->space_held)` at the top of the press arm. The wheel/
arrow pan paths are untouched.

`space_held` lives on `struct flow` (additive int, zero-init via calloc). Space routing goes through
`flow_dispatch_key` as a built-in `seq==" "` (0x20) so it coexists with the existing key built-ins and is
overridable. Make sure Space does NOT collide with any existing built-in (it doesn't — `x`/`n`/`f`/`?`/Delete
only).

**Test plan.**
  - tests/test_space_pan.c (new; added to Makefile TESTS).
  - Toggle: `flow_feed(f, " ", 1)` sets `space_held`; a press-drag over a NODE (synthetic SGR) PANS (view offset changes, node pos unchanged); `flow_feed(f, " ", 1)` again clears it; the same drag now MOVES the node (regression that normal behavior returns).
  - Space-drag over empty pane pans (already the default) — assert no regression.
  - Space + drag does NOT select/marquee: assert `flow_selected_count` unchanged across a space-drag.
  - Binding override: `flow_bind_key(f, " ", custom_fn, ...)` then `flow_feed(f, " ", 1)` runs the custom fn instead of toggling space-pan (registry-before-builtin).
  - `make test` green; ASan/UBSan clean on test_space_pan.

**Acceptance.**
  - Holding/toggling Space (per the documented terminal model) makes a drag pan the viewport, including over a node, with the node's position unchanged and no selection/marquee triggered.
  - Plain (no-Space) drag behavior is byte-identical to today (node-drag, marquee, pane-pan all unchanged).
  - Space routes through `flow_dispatch_key` and is overridable via `flow_bind_key` (registry-before-builtin).
  - `make test` passes including test_space_pan; ASan/UBSan clean; `flow.h` regenerated by amalgamate.

**Depends on.**
  - **groups** (ordering) — both edit `flow_handle_mouse` press classification; land after groups owns it.
  - settled inc-2 pan path (`dragging_pan`) and key seam (`flow_dispatch_key`/`flow_bind_key`).

**Conflicts with.**
  - **auto-pan**, **events**, **undo-redo** — all touch `flow_handle_mouse`; keep the `if (space_held)` branch at the top of the press arm so it composes with the others.

**Carry-overs fixed.**
  - Implements the spec §8 "Space-drag" pan trigger (previously absent) — the terminal-faithful analog of the GUI space-pan gesture.

---

### 6. Auto-pan near viewport edge during drags  `[M]`  ·  id: `auto-pan`

**Goal.** Implement the spec §8 "auto-pan near edge during drags" interaction. When a node-drag or
connection-drag cursor nears a screen border, the viewport pans toward it so off-screen targets become
reachable, matching xyflow's autopan-on-connect / drag-near-edge behavior.

**User value.** A user dragging a node (or pulling a connection) toward the edge of the view sees the canvas
scroll to follow, so they can drop onto or connect to a node that started off-screen without manually panning
first.

**Files touched.**
  - src/flow_input.h
  - src/flow_model.h
  - tests/test_autopan.c
  - Makefile
  - flow.h

**Entry points (existing functions to extend).**
  - `flow_handle_mouse` (`src/flow_input.h`, MOTION arms `:131-178`) — during a node-drag (`drag_node != -1`), a connection-drag (`conn_active`), or a reconnect-drag (`reconnect_edge != -1`), check cursor proximity to the buffer edge and pan when within a margin
  - `flow_pan` (`src/flow_model.h`) — the existing viewport pan applied each auto-pan step
  - `struct flow` (`src/flow_model.h:121-141`) — optionally add `int autopan_margin` / `int autopan_speed` config (additive); zero-init = a sensible default

**API additions.**
```c
/* Auto-pan is an internal MOTION-time effect; no new public function is required. Optional config setter: */
void flow_set_autopan(flow_t *f, int margin, int speed);  /* optional: edge margin (cells) + pan step (cells/event); <=0 -> defaults */
/* struct flow gains additive fields: int autopan_margin, autopan_speed;  (zero-init -> clamp to defaults on read) */
```

**Design notes.**

WHERE: auto-pan is a MOTION-time effect. In `flow_handle_mouse`'s motion branch, when an interactive drag is in
flight (`drag_node != -1` OR `conn_active` OR `reconnect_edge != -1`), compute the cursor's distance to each
buffer edge (`ev->x`, `ev->y` vs `0`/`cols-1`/`rows-1`). If within `autopan_margin` cells of an edge, call
`flow_pan(f, dx, dy)` by `autopan_speed` cells in that direction (sign toward the off-screen region). Because the
drag uses ABSOLUTE world targets (`flow_move_node` for nodes, `conn_end`/route for connections), the dragged
object stays under the cursor as the view pans — RE-APPLY the drag's position AFTER the pan in the same motion so
it doesn't lag (recompute `flow_move_node(drag_node, world(cursor) - grab)` / `conn_end = cursor` post-pan). This
is the subtle correctness point: pan THEN re-place, or the node visually drifts.

THE TICK PROBLEM (the honest hard part): true auto-pan continues while the cursor is HELD near the edge even with
no new mouse events. A terminal/SGR stream only delivers motion on actual movement, so a stationary-near-edge
cursor produces no events to drive continued panning. Two models, pick and document:
  (A) **Event-driven only** — pan one `autopan_speed` step per motion event while near the edge. Simple, fully
      testable via synthetic SGR (each fed motion = one step), but only pans while the mouse keeps moving.
  (B) **Run-loop ticked** — `flow_run` (or a `flow_tick`) advances auto-pan on a timer while a drag is active and
      the cursor is near the edge, even without new events. More faithful but needs a tick hook in `flow_run.h`
      and is harder to unit-test deterministically.
  Recommend (A) for v1 (event-driven, deterministic, no `flow_run` edit), and note (B) as the faithful follow-up.
  Document the chosen model in the acceptance.

CONFIG: `autopan_margin` (default ~3 cells) and `autopan_speed` (default ~2 cells/step) as additive `struct flow`
fields, zero-init to defaults inside `flow_new` (or clamp-on-read so calloc-zero means "use default"). Optionally
a `flow_set_autopan(f, margin, speed)` setter; keep it minimal. Auto-pan must NOT fire for a pure pane-pan drag
(that already moves the view) or a marquee — only for object drags where following the cursor is the point.

**Test plan.**
  - tests/test_autopan.c (new; added to Makefile TESTS).
  - Node-drag near edge pans: start dragging a node (synthetic SGR press on node), feed a motion event with the cursor at `(cols-1, mid)` (right edge); assert the view offset shifted right (auto-pan fired) AND the node stayed under the cursor (its abs == world(cursor) - grab).
  - No auto-pan in the interior: a motion event well inside the margin does NOT change the view offset (only moves the node).
  - Connection-drag auto-pan: begin a connection (`conn_active`), motion near an edge → view pans and `conn_end` tracks the cursor post-pan.
  - No auto-pan for pane-pan / marquee: a pane-pan drag near the edge does NOT double-pan via auto-pan; a marquee drag near the edge does NOT auto-pan.
  - Direction correctness: cursor near the TOP edge pans up; near the LEFT edge pans left; corners pan diagonally.
  - `make test` green; ASan/UBSan clean on test_autopan.

**Acceptance.**
  - During a node-drag, connection-drag, or reconnect-drag, a cursor within the auto-pan margin of a buffer edge pans the viewport toward that edge (event-driven model documented), and the dragged object stays under the cursor (pan-then-replace).
  - Auto-pan does NOT fire in the interior, nor for pane-pan or marquee drags.
  - Pan direction is correct for all four edges and corners.
  - `make test` passes including test_autopan; ASan/UBSan clean; `flow.h` regenerated by amalgamate.

**Depends on.**
  - **groups** (ordering) — edits the `flow_handle_mouse` motion arm that groups reworks; land after groups.
  - settled inc-2 `flow_pan`, the drag/connection/reconnect motion machinery, and the absolute-in `flow_move_node`.

**Conflicts with.**
  - **space-pan**, **events**, **undo-redo** — all touch `flow_handle_mouse`; auto-pan adds a motion-time branch, keep it gated on `drag_node!=-1 || conn_active || reconnect_edge!=-1`.

**Carry-overs fixed.**
  - Implements the spec §8 "auto-pan near edge during drags" interaction (previously absent), making off-screen drag/connect targets reachable.

---

### 7. Undo/redo command journal over the settled mutator surface  `[L]`  ·  id: `undo-redo`

**Goal.** Add a capped, inverse-op command journal that wraps the now-settled + grouped graph mutator set so every
editing action is undoable/redoable. Each mutator (add/remove node, add/remove edge, move, reconnect, set-label,
connect, **reparent/group/ungroup**) records an inverse command keyed on STABLE ids; `flow_undo`/`flow_redo`
replay inverses; a multi-node group drag, an add-node-center, and a delete-selection each collapse into ONE undo
via transaction brackets. Bind 'u' (undo) and Ctrl-r (redo) through the EXISTING `flow_dispatch_key` built-in path
(overridable via `flow_bind_key` — do NOT redefine `flow_bind_key`). The redo stack clears on any new mutation.
Viewport zoom/pan/fit is deliberately NOT journaled (spec §11 omits it).

**User value.** A user can press 'u' to undo and Ctrl-r to redo: a deleted node (with its edges and child nodes)
comes back exactly as it was, a dragged group snaps back in one step, a group/ungroup or reparent reverts, a
created/reconnected/relabeled edge reverts, and a new node disappears — all keyed on stable ids so references
stay valid. App authors get `flow_undo`/`flow_redo`/`flow_can_undo`/`flow_can_redo` to wire their own undo UI.

**Files touched.**
  - src/flow_model.h
  - src/flow_undo.h
  - src/flow_input.h
  - tools/amalgamate.sh
  - tests/test_undo.c
  - Makefile
  - demos/topo.c
  - flow.h

**Entry points (existing functions to extend).**
  - `flow_add_node` / `flow_add_edge` (`src/flow_model.h`) — record an add command (inverse = remove-by-id); add_edge records on success only (returns -1 on reject)
  - `flow_move_node` (`:192`) — record a move (old_pos→new_pos); coalesce within an open txn keyed on node id. NOTE: groups made this absolute-in; the MOVE command stores/replays the SAME absolute coords `flow_move_node` accepts.
  - `flow_remove_node` (`:350`) — BEFORE the cascade, snapshot the whole subtree (descendants + incident edges; ids/indices/data ptr/dup'd labels) as ONE command; suppress recording during the internal cascade
  - `flow_remove_edge` (`:340`) — snapshot the edge (id, index, endpoints, handles, type, dup'd label) before shift-removal; inverse = positional re-insert
  - `flow_reconnect_edge` (`:309`) — record old (endpoint,handle,which) before commit; record only when applied (early-returns on reject)
  - `flow_set_edge_label` (`:331`) — capture OLD label (dup) BEFORE it frees it (record-before-mutate); inverse restores old label (incl NULL)
  - `flow_end_connection` (`:522`) — connect routes through `flow_add_edge`, so its add-edge command records automatically; wrap in a txn so the connect is one undo
  - `flow_add_node_center` (`:378`) / `flow_delete_selection` (`:372`) — open/close a txn so each is one undo
  - **`flow_set_parent` / `flow_group` / `flow_ungroup` (groups)** — record `FLOW_CMD_REPARENT` (and composite for group/ungroup); see notes
  - `flow_dispatch_key` (`:412`) — add 'u' and Ctrl-r ('\x12') as built-ins AFTER the registry check (so `flow_bind_key` overrides win)
  - `flow_handle_mouse` (`src/flow_input.h`) — call `flow__undo_begin` at drag start (single + multi) and reconnect start; `flow__undo_end` at release, so a whole gesture is one undo
  - `flow_free` (`src/flow_model.h:155`) — tear down the journal: free every command's dup'd labels and the command arrays (does NOT free `node->data` ptrs, matching today's contract)

**API additions.**
```c
/* ---- declared in src/flow_model.h (header section, before #ifdef FLOW_IMPLEMENTATION) ---- */
/* Public undo/redo API (spec §11). Definitions live in src/flow_undo.h (after flow_model in
   the amalgamation) because applying an inverse calls the mutators, defined in flow_model.h. */
void flow_undo(flow_t *f);        /* pop the top command, apply its inverse; no-op if empty */
void flow_redo(flow_t *f);        /* re-apply the last undone command; no-op if redo stack empty */
int  flow_can_undo(flow_t *f);
int  flow_can_redo(flow_t *f);
void flow_set_undo_limit(flow_t *f, int max_commands); /* cap history depth (default ~128); evicting the
                                     oldest frees its label copies (drops, never frees, node->data ptrs).
                                     0 = disable journaling entirely. */

/* internal recording/txn primitives, DECLARED in flow_model.h, DEFINED in flow_undo.h.
   Pure data push — they never call mutators, so they are safe to invoke from flow_model.h
   even though flow_undo/flow_redo (which DO call mutators) are defined later. */
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

/* ---- struct flow gains (additive; appended inside the brace block in flow_model.h:121-141) ----
     struct {
       struct flow__cmd *items; int n, cap;   // undo stack (top = items[n-1])
       struct flow__cmd *redo;  int rn, rcap;  // redo stack
       int limit;                              // max depth (default 128)
       int applying;                           // re-entrancy guard: 1 while undo/redo replays
       int suppress;                           // 1 during flow_remove_node's internal cascade
       int txn_depth;                          // >0 inside a coalescing transaction
       int txn_base;                           // stack index where the current txn began
     } journal;
   (struct flow__cmd is the tagged-union command record, defined in flow_model.h alongside flow_cmd_kind.) */
```

**Design notes.**

DECISION (locked by the task AND spec §11): inverse-op COMMAND JOURNAL keyed on stable ids, NOT full snapshots.
The engine does NOT own `node->data`, so a deep full-graph snapshot can't safely deep-copy or free data; a command
journal only HOLDS the borrowed `void*` across the undo window and reattaches the exact pointer on undo-of-delete
(identical to today's `flow_remove_node`/`flow_free` contract — neither frees data). The vocabulary is MIXED:
leaf inverse-ops for add/move/add_edge/reconnect/set_label/reparent, and a SUBTREE SNAPSHOT for remove_node
(because `flow_remove_node` inlines incident-edge removal and recurses into children, `src/flow_model.h:350-371`,
so it cannot be composed from leaf inverses).

BUILDABILITY LINCHPIN (amalgamation order
`flow_head flow_geom flow_cell flow_model flow_view flow_route flow_types flow_render flow_input flow_term flow_run`):
a function may only call one defined at/before it. `flow_undo`/`flow_redo` APPLY inverses by calling mutators
(`flow_add_edge`, `flow_move_node`, etc.) and positional-insert helpers — all in `flow_model.h` — so they must be
DEFINED AFTER flow_model. Resolution: (a) DECLARE `flow_undo`/`flow_redo`/`flow_can_undo`/`flow_can_redo`/
`flow_set_undo_limit` AND the internal primitives `flow__undo_begin`/`flow__undo_end` + the record helpers in
`flow_model.h`'s HEADER section. (b) DEFINE the pure-data RECORD primitives and `flow__undo_begin`/`end` in
`flow_model.h`'s impl block (they only push data — safe). (c) DEFINE `flow_undo`/`flow_redo`/etc. and the
positional-insert helpers in a NEW `src/flow_undo.h`, inserted into `tools/amalgamate.sh` modules list immediately
AFTER `flow_model` (precedent: zoom inserted `flow_view` after `flow_model`). `flow_dispatch_key` stays in
`flow_model.h` (`:412`) and calls `flow_undo`/`flow_redo` — declared there, defined later in the same TU — links
fine in the single-TU amalgamation.

ID + POSITION PRESERVATION (the #1 correctness trap): `flow_add_node`/`flow_add_edge` mint fresh ids
(`nextid++`/`nexteid++`), so they CANNOT restore a removed object — the restored node/edge must keep its EXACT
original id (so `edge.source/target` and `node.parent` stay valid) AND its original array index (`flow_render` and
`flow_hit_node` iterate by insertion order; spec §13 wants apply→undo equality). So the apply side uses
positional-insert helpers (in `flow_undo.h`): `flow__insert_node_at(f, index, snap)` /
`flow__insert_edge_at(f, index, snap)` that `flow__grow` + memmove a hole at `index`, copy the snapped struct
verbatim (id, type, pos, parent, w, h, data ptr, flags-cleared-of-ephemeral), and bump the count. Each
ADD/REMOVE command also saves/restores `f->nextid`/`f->nexteid`. On REMOVE_NODE undo, re-insert the subtree in
ascending original-index order. Acceptance is "field-and-order equality + restored id counters," tested
field-wise — NOT a literal memcmp of `struct flow`.

REMOVE_NODE = ONE subtree-snapshot command: before any deletion, walk the subtree exactly as `flow_remove_node`
will (all transitive children via `parent==id`, plus every edge incident to any removed node), capturing each as
a `flow__node_snap`/`flow__edge_snap` (id, index, full struct, DUP'd label copy for edges). Set
`f->journal.suppress=1` across the actual cascade so the inlined removals don't each record, then push the single
subtree command. Undo positionally re-inserts nodes then edges (ascending index), reattaching the exact
`node->data` pointers and dup'd labels.

REPARENT (NEW, from groups): `flow_set_parent` records `FLOW_CMD_REPARENT {child, old_parent, old_pos, new_parent,
new_pos}` — undo restores `old_parent` + `old_pos` via the (groups-provided) mutator, redo re-applies. `flow_group`
= a txn bracketing (add container + N reparents) so one undo dissolves it; `flow_ungroup` = a txn bracketing
(N reparents-out + remove container) — but remove-container during ungroup must snapshot the container so undo can
re-add it (the container has no children at removal time, so a plain ADD_NODE inverse suffices). Document the
composite representation. CRITICAL: groups must land FIRST so these mutators are settled before undo journals them.

LIFETIME CONTRACT: `node->data` (void*) — journal HOLDS the borrowed pointer across the undo window, reattaches on
undo-of-delete, NEVER frees it; on eviction/redo-clear the pointer is dropped (no new leak vs today). Edge label
(char*) — engine OWNS it; the journal DUPS labels into commands. CRITICAL ordering: in `flow_set_edge_label`
(`:331-339`), capture the OLD label (malloc+memcpy — the codebase avoids strdup under -std=c11) BEFORE the
existing `free(e->label)` (record-before-mutate). The journal frees its label copies on eviction, redo-clear, and
in `flow_free`. ASan-sensitive path.

RE-ENTRANCY + SUPPRESSION: every record call is gated `if (!journal.applying && !journal.suppress &&
journal.limit != 0)`. `flow_undo`/`flow_redo` set `applying=1` around replay. `flow_remove_node` sets `suppress=1`
around its cascade. Test: undo-of-remove does not itself grow the undo stack.

COALESCING via TXN BRACKETS: `flow__undo_begin` increments `txn_depth` and records `txn_base` on the 0→1 edge;
`flow__undo_end` decrements. While `txn_depth>0`, a MOVE for an id already moved this txn COALESCES (keep first
old_pos, overwrite new_pos). The group-drag loop (`src/flow_input.h:162-169`) calls `flow_move_node` per node per
motion; bracketing the whole gesture collapses it. To make the entire multi-node drag a SINGLE undo step, the txn
collapses its span into one composite command on close (document the representation). `flow_handle_mouse` opens
the bracket when a drag actually begins (`drag_node` set, or `reconnect_edge` set) and closes on release.
`flow_add_node_center` and `flow_delete_selection` each open/close their own txn. RESULT: `flow_run.h` is
UNTOUCHED; `flow_input.h` gains only the two bracket calls; 'u'/Ctrl-r route automatically because `flow_feed`
already calls `flow_dispatch_key` — no `flow_run`/`flow_feed` edit.

KEYS: in `flow_dispatch_key` (`:412`), AFTER the registry scan and alongside the existing built-ins:
`seq[0]=='u'` → `flow_undo(f); return 1;` and Ctrl-r == byte `0x12` → `flow_redo(f); return 1;`. Do NOT touch
`flow_bind_key`. Ctrl-r as `0x12` is a single byte, longest-match safe.

REDO CLEAR + CAP: any new RECORDED mutation clears the redo stack (freeing its label copies). Pushing past limit
evicts `items[0]` (free owned labels, drop data ptrs). `flow_set_undo_limit(f, 0)` disables journaling.

VIEWPORT EXCLUDED (deliberate non-goal): spec §11's command list omits viewport. `src/flow_view.h` is NOT touched;
zoom/pan/fit are not undoable. State this so a reviewer doesn't flag the omission.

serialize SEAM: if serialize has landed, `flow_load`'s `flow__graph_reset` MUST also call a `flow__journal_clear`
(free pending command label-copies, drop borrowed data, zero the stacks) or undo would invert against a stale
graph. Whoever lands second wires it.

auto-layout SEAM (forward): spec §10/§11 wants the whole `flow_layout` call as ONE coalesced command — auto-layout
wraps its commit in a txn (or undo wraps `flow_layout`); no edit inside layout, just a documented seam.

**Test plan.**
  - tests/test_undo.c (new; per-file FLOW_IMPLEMENTATION + flowtest.h; added to Makefile TESTS).
  - Add/undo/redo with id+index preservation: add a node → `flow_can_undo()==1`; undo → gone, count==before; redo → back with the SAME id and SAME array index.
  - Remove-with-subtree round-trip: node A with child B and three incident edges; `flow_remove_node(A)`; undo → A and B back (same ids), all 3 edges back (same ids, source/target/handles), `node->data` pointers identical (pointer equality), edge labels restored, insertion order preserved.
  - Move undo: `flow_move_node(A,p1)→p2`; undo restores p1; redo restores p2 (absolute coords).
  - REPARENT undo (groups): `flow_set_parent(child, group)` → undo restores old parent + old_pos (abs preserved); `flow_group(ids)` → one undo dissolves it (children survive at their original parents); `flow_ungroup(g)` → one undo re-groups them.
  - Group-drag = ONE undo (synthetic SGR): shift-select A+B, press-drag A across several motions, release; ONE undo restores BOTH positions; an unselected C never moved; redo re-applies in one step.
  - Connect undo/redo: drag-connect A→B; edge_count 1; undo → 0; redo → edge back with same source/target/handles.
  - Reconnect undo: repoint target to C; undo restores original target + target_handle; a rejected reconnect records nothing.
  - Set-label undo incl NULL↔str: "L1" then "L2"; two undos walk L2→L1→NULL; ASan clean.
  - add-node-center = one undo; delete-selection = one undo (restores all).
  - Redo cleared on new mutation; cap eviction frees the evicted label copy (ASan); floor/ceiling no-ops; keys via `flow_feed` ('u' / '\x12'); `flow_bind_key('u',...)` override wins; re-entrancy (inverse replay records nothing); id-counter integrity after undo-all/redo-all then add.
  - Teardown: history with pending undo+redo carrying dup'd labels + borrowed data, then `flow_free` → ASan/UBSan clean.
  - Gate: `make test` green PLUS `cc -std=c11 -fsanitize=address,undefined -fno-sanitize-recover=all -g tests/test_undo.c -o /tmp/tu -lm && /tmp/tu` 0 errors; existing suites stay green (recording is gated + additive).

**Acceptance.**
  - `make test` passes including test_undo.c; no existing test regresses (recording gated by applying/suppress/limit, otherwise transparent).
  - ASan+UBSan clean: no leaks from edge-label copies, no use-after-free across remove→undo→redo, `node->data` never freed by the journal, journal fully torn down by `flow_free`.
  - `flow_undo` after any single mutator restores node/edge FIELD-and-ORDER equality (ids, type, pos, parent, source/target, handles, type, label) and restores `f->nextid`/`f->nexteid`; restored objects keep ORIGINAL ids and indices; `flow_redo` re-applies.
  - Removing a node with children + incident edges, then undo, restores the entire subtree (identical data pointers, restored labels) as ONE undo step in original order.
  - `flow_set_parent`/`flow_group`/`flow_ungroup` are undoable (`FLOW_CMD_REPARENT` / composite); a multi-node drag, add-node-center, connect, and delete-selection each collapse into exactly ONE undo step.
  - 'u' undoes and Ctrl-r (`0x12`) redoes via `flow_feed`/`flow_dispatch_key`; a `flow_bind_key('u',...)` override takes precedence; `flow_bind_key` is NOT redefined.
  - Redo clears on new recorded mutation; history is capped (`flow_set_undo_limit`); eviction frees label copies without freeing data; limit 0 disables journaling.
  - Viewport zoom/pan/fit is NOT journaled (spec §11); `src/flow_view.h` untouched.
  - `flow.h` regenerated by amalgamate (`src/flow_undo.h` after `flow_model`); demos build with `make demos` after a status-hint touch.

**Depends on.**
  - **groups** (HARD) — must journal `flow_set_parent`/`flow_group`/`flow_ungroup`; groups makes `flow_move_node` parent-relative-internally, so the MOVE command must replay absolute consistently.
  - **events** (ordering) — `on_selection_change`/`on_nodes_delete` fire from the same selection/delete mutators undo wraps; settle events first.
  - settled inc-2 mutator surface (add/remove node+edge, move, reconnect, set_label, connect, add_node_center, delete_selection) — instrumented in place; `flow_handle_mouse` drag/reconnect gestures gain the brackets; `flow_bind_key`/`flow_dispatch_key` + `flow_feed` already calling `flow_dispatch_key`.

**Conflicts with.**
  - **serialize** — `flow_load`'s reset MUST also clear the journal (`flow__journal_clear`); whoever lands second wires it.
  - **auto-layout** — a layout pass must push exactly ONE command (spec §10); coordinate the txn bracketing / command enum.
  - Any package adding DURABLE `flow_node`/`flow_edge` fields — snapshots capture the full struct (covered automatically), but a heap-owned field (like label) must be dup'd/freed in the snapshot paths in lockstep.
  - Any package editing `flow_handle_mouse` drag classification (groups, space-pan, auto-pan, events) — must keep the `flow__undo_begin/end` brackets paired around the gesture.

**Carry-overs fixed.** (none — undo is net-new capability over the settled set)

---

### 8. Auto-layout: unified flow_layout() with force-directed + layered modes  `[XL]`  ·  id: `auto-layout`

**Goal.** Add a pure model→positions transform behind a single `flow_layout(flow_t*, flow_layout_opts)` entry
point with a mode enum, implementing BOTH a deterministic force-directed (Fruchterman-Reingold) variant and a
layered (Sugiyama-style: longest-path/topo rank, barycenter ordering, coordinate assignment) variant. Layout
operates strictly THROUGH the public mutator `flow_move_node` (not privileged), re-measures via
`flow_measure_node`, optionally calls `flow_fit_view`, lays out children within their parent's local space when
nodes are grouped, and is fully deterministic (no unseeded RNG; force-directed uses an explicit seeded PRNG /
deterministic circular initial layout).

**User value.** A user or app author can call `flow_layout(f, opts)` to auto-arrange a graph: pick
`FLOW_LAYOUT_LAYERED` for a clean ranked DAG layout (LR or TB) like dagre, or `FLOW_LAYOUT_FORCE` for an organic
spread, then optionally `fit_after` to frame the result. It respects existing group containment and produces the
same result every run for the same inputs+seed, so it's safe in demos, tests, and persisted graphs.

**Files touched.**
  - src/flow_layout.h
  - tools/amalgamate.sh
  - tests/test_layout.c
  - Makefile
  - tests/snapshots/layout_layered_lr.txt
  - tests/snapshots/layout_layered_tb.txt
  - demos/topo.c
  - flow.h

**Entry points (existing functions to extend).**
  - `flow_layout` (NEW, in `src/flow_layout.h`) — the unified public entry; switches on `opts.mode` to `flow__layout_force`/`flow__layout_layered`, then optionally `flow_fit_view`
  - `flow_move_node` (`src/flow_model.h:192`, settled) — the ONLY position-write path; called once per node (parent-relative for grouped children, absolute for top-level — composes for free with groups' absolute-in contract)
  - `flow_measure_node` (settled) — re-measure every node before layout so w/h reflect current data
  - `flow_bounds` + `flow_fit_view` (`:385`, settled) — `fit_after` support
  - `flow_nodes`/`flow_node_count`/`flow_edges`/`flow_edge_count`/`flow_get_node` (settled accessors) — iterate in insertion order (determinism) and read adjacency
  - `flow_node_rect_abs` (`:222`) / `flow_node_abs` (`:216`) — read sizes; the additive parent walk makes group-local positioning compose
  - `tools/amalgamate.sh` `modules=` — insert `flow_layout` AFTER `flow_view`, BEFORE `flow_route`
  - `Makefile` `TESTS=` — append `test_layout`; `demos/topo.c` — one `flow_layout` showcase call (defer to final integration pass)

**API additions.**
```c
/* ===== src/flow_layout.h — auto-layout: model -> positions, via flow_move_node ===== */

typedef enum { FLOW_LAYOUT_FORCE, FLOW_LAYOUT_LAYERED } flow_layout_mode;
typedef enum { FLOW_LR, FLOW_TB } flow_layered_dir;

/* Zero-init is a valid request: {0} == FORCE mode with all-zero params, clamped to defaults. */
typedef struct {
  flow_layout_mode mode;       /* FORCE (default 0) or LAYERED */
  int      iterations;         /* <=0 -> default (~200) */
  float    k;                  /* ideal edge length in cells; <=0 -> derived from node count + sizes */
  float    gravity;            /* pull toward centroid so components stay bounded; <0 -> default */
  unsigned seed;               /* explicit PRNG seed for the deterministic initial layout */
  flow_layered_dir dir;        /* FLOW_LR (default 0) or FLOW_TB */
  int      gap_x, gap_y;       /* inter-node / inter-rank spacing in cells; <=0 -> defaults */
  int      fit_after;          /* nonzero -> flow_fit_view(f, margin) after committing */
  int      margin;
} flow_layout_opts;

void flow_layout(flow_t *f, flow_layout_opts opts);   /* PRIMARY entry; mutates positions via flow_move_node */

/* Thin spec-literal wrappers (spec §10 shapes) over flow_layout: */
typedef struct { int iterations; float k; float gravity; } flow_force_opts;
void flow_layout_force(flow_t *f, flow_force_opts opts);                       /* -> flow_layout FORCE */
void flow_layout_layered(flow_t *f, flow_layered_dir dir, int gap_x, int gap_y); /* -> flow_layout LAYERED */
```

**Design notes.**

API SHAPE (load-bearing, deviates from spec on purpose): the TASK mandates "a `flow_layout(...)` API with a mode
enum"; spec §10 lists two separate functions with different param shapes. Honor the task: `flow_layout(f, opts)`
is PRIMARY, folding the per-mode params into `flow_layout_opts`. Keep `flow_layout_force`/`flow_layout_layered` as
thin wrappers so spec-literal callers still work. Call out this deviation in the commit/handoff. ALL typedefs
live in `src/flow_layout.h` — do NOT add any field to `struct flow` and do NOT touch `src/flow_model.h` (layout
holds no engine state; it is a stateless transform over the model).

MODULE PLACEMENT: new `src/flow_layout.h`; insert into `tools/amalgamate.sh` `modules=` AFTER `flow_view` and
BEFORE `flow_route`. `flow_layout` only calls `flow_move_node`/`flow_measure_node`/`flow_bounds`/`flow_fit_view`/
accessors (all declared in `flow_model.h`, with `fit_view`'s body in `flow_model.h` and zoom helpers in
`flow_view.h`) — placing after `flow_view` guarantees every consumed prototype is in scope. CONFLICT NOTE:
serialize and undo also edit `amalgamate.sh modules=` (different lines) — coordinate; all three append to Makefile
TESTS=.

DETERMINISM (hard requirement): iterate nodes/edges strictly in array (insertion) order everywhere; break ALL
ties by node index/id. Force-directed places node i on a circle by index (angle = 2π·i/N, radius derived from N
and k) so coincident-start degeneracy is impossible and runs are reproducible — `opts.seed` is retained for
future jitter but the default path is RNG-free; any randomness MUST go through a seeded LCG-style PRNG. Layout
runs in float, rounds to integer cells only at the final `flow_move_node` commit (spec: "compute in float, round
to cells on commit").

FORCE-DIRECTED (Fruchterman-Reingold): default iterations ~200, k = C·sqrt(area/N) or `opts.k` if >0. Each
iteration: repulsive force between every pair (fr = k²/d), attractive force along each edge (fa = d²/k), plus a
gravity pull toward the centroid (`opts.gravity`) so disconnected components don't fly apart. CRITICAL UBSan
guard: clamp pairwise distance to an epsilon min (`d = max(d, 0.5f)`) BEFORE dividing — two nodes at the same
cell would otherwise divide-by-zero / NaN, which `-fsanitize=undefined` catches. Cool the max displacement per
iteration (temperature schedule). Commit final float positions rounded to cells via `flow_move_node`, offsetting
by w/2,h/2 so the node BOX center sits at the computed point. Empty/single-node graphs are no-ops.

LAYERED (Sugiyama-style): (1) RANK by longest-path on a topological order. The model is NOT guaranteed acyclic —
detect cycles and break back-edges DETERMINISTICALLY (when DFS/Kahn finds an edge into an in-progress node, drop/
reverse it by array order) so rank assignment can never loop. (2) ORDER within each rank by barycenter (mean of
neighbor positions in the adjacent rank), a couple of sweeps, ties by node id. (3) ASSIGN coordinates: ranks
march along the flow axis spaced by gap (gap_y for TB ranks / gap_x for LR ranks, plus node extent), within-rank
nodes spaced by the cross gap plus node width/height. FLOW_LR → ranks increase in x, nodes stack in y; FLOW_TB →
ranks increase in y, nodes stack in x. Use real node w/h so wide nodes don't overlap. Defaults: gap_x/gap_y ~4
cells when `opts` value <=0.

GROUPS (forward-compat, testable NOW): "respect groups" works via the existing `parent` field +
`flow_node_abs` additive walk — NO public reparent API is needed here. Partition nodes by parent: top-level
(parent==-1) laid out in absolute/world space; each group's children laid out among themselves in PARENT-LOCAL
space and committed with parent-relative pos (because `flow_move_node` writes pos verbatim — once groups landed,
absolute-in — and `flow_node_abs` adds the parent offset). Do NOT resize/move the parent container (defers to
groups). If no node is grouped, this reduces to a single top-level pass. NOTE: once groups lands, re-validate the
group-local pass against groups' container-rect contract.

UNDO (out of scope, documented seam): spec §11 lists "layout (snapshots all positions)" as an undo command, but
that is undo's responsibility. Layout stays undo-agnostic and commits through `flow_move_node`; when undo lands it
wraps the whole `flow_layout` call as one coalesced command (or layout opens a single txn). No history hook here.

FIT: when `opts.fit_after`, call `flow_fit_view(f, opts.margin)` after all positions are committed.

**Test plan.**
  - tests/test_layout.c (NEW; per-file FLOW_IMPLEMENTATION + flowtest.h; register defaults; add to Makefile TESTS). Build red-first against undefined `flow_layout`, then green.
  - LAYERED rank correctness: DAG A→B, A→C, B→D, C→D; `flow_layout(f, {.mode=FLOW_LAYOUT_LAYERED, .dir=FLOW_LR, .gap_x=4, .gap_y=2})`; assert rank ordering via x (A.x < B.x == C.x < D.x) and no two nodes share a cell (`flow_node_rect_abs` pairwise, no `flow_rect_intersects`).
  - LAYERED TB: same DAG, `.dir=FLOW_TB`; assert ranks march in y and within-rank nodes separate in x.
  - LAYERED cycle robustness (must not hang): A→B→C→A; layout returns (wall-clock-bounded), every node finite, no overlaps — proves deterministic back-edge breaking.
  - LAYERED snapshot goldens: fixed small DAG; render after layout and SNAPSHOT("layout_layered_lr") + ("layout_layered_tb") via the test_render `cells_to_string` helper. Layered is integer/topological so exact goldens are stable.
  - FORCE invariants (NOT exact goldens — float results are platform-fragile): tight cluster (A-B-C connected) + isolated pair (D-E) far apart; `flow_layout(f, {.mode=FLOW_LAYOUT_FORCE, .iterations=300, .seed=1})`; assert (a) connected nodes end CLOSER than unconnected, (b) no two share a cell, (c) all positions finite, (d) bounds finite/bounded (gravity).
  - FORCE determinism: run twice with identical opts on identical input; assert every node lands at the IDENTICAL committed cell.
  - FORCE coincident-input guard: two nodes at the EXACT same pos, FORCE 1 iteration; assert no NaN/inf and they separate — the UBSan gate (epsilon min-distance).
  - GROUPS (parent-relative): parent P + two children with parent=P (set directly); layout LAYERED; assert children's stored pos is parent-RELATIVE while `flow_node_abs` places them inside/near P, and P itself not moved/resized. Mixed graph lays out both partitions without cross-contamination.
  - fit_after: layout with `.fit_after=1, .margin=2`; assert every node's projected screen rect lies within [2, cols-2]×[2, rows-2].
  - Edge cases: empty graph → no-op; single node → no crash, finite pos. Wrappers consistent with equivalent `flow_layout` opts.
  - `make test` green; ASan/UBSan gate: `cc -std=c11 -fsanitize=address,undefined -fno-sanitize-recover=all -g tests/test_layout.c -o /tmp/tl -lm && /tmp/tl` clean — no divide-by-zero/NaN from FR repulsion, no leaks from scratch buffers.

**Acceptance.**
  - `flow_layout(f, opts)` LAYERED arranges a DAG into monotonic ranks (LR or TB) with no overlapping cells; cyclic graphs handled by deterministic back-edge breaking (no hang, every node placed).
  - `flow_layout(f, opts)` FORCE produces connected-closer, components-bounded layout, no shared cells, no NaN/inf, bit-for-bit reproducible given the same seed+iterations.
  - Layout writes positions ONLY through `flow_move_node` and re-measures via `flow_measure_node` first; adds NO field to `struct flow` and edits no hot file (flow_render/flow_input/flow_run untouched).
  - Grouped nodes (parent != -1) laid out in parent-local space (committed parent-relative); parent neither moved nor resized.
  - `opts.fit_after` frames the result via `flow_fit_view(f, opts.margin)`; without it the viewport is unchanged.
  - Wrappers `flow_layout_force`/`flow_layout_layered` delegate to `flow_layout` and match for equivalent opts.
  - Empty and single-node graphs are no-ops.
  - `make test` green including test_layout + layered snapshots; no existing test/snapshot regresses (purely additive).
  - ASan+UBSan clean: no divide-by-zero/NaN in FR repulsion (epsilon guard), no scratch-buffer leaks.
  - `flow.h` regenerated by amalgamate (`src/flow_layout.h` after `flow_view`); demos build after one showcase call.

**Depends on.**
  - **groups** (SOFT) — group-local layout relies on parent-relative coords; re-validate against groups' container contract once it lands. (Lays out within existing `parent` containers even before groups, but the absolute-in `flow_move_node` from groups is what makes parent-relative commit clean.)
  - **undo-redo** (SOFT) — spec §10/§11 wants the whole layout as one coalesced undo command; undo wraps `flow_layout` (no edit here, documented seam).
  - settled inc-2: `flow_move_node` (sole position-write path; HARD), `flow_measure_node` (HARD), `flow_bounds`+`flow_fit_view` (HARD for fit_after), `flow_node_rect_abs`/`flow_node_abs` + accessors (HARD — adjacency + sizes + the parent walk).

**Conflicts with.**
  - **serialize** / **undo-redo** — all edit `tools/amalgamate.sh modules=` (different lines) and append to Makefile TESTS=; coordinate the insertions.
  - **groups** (SOFT) — when subflows' container-resize semantics land, layout's group-local pass should be re-validated.

**Carry-overs fixed.** (none — auto-layout is net-new capability; it is the capstone consuming the finished model)

---

### 9. flowchart.c demo — groups + auto-layout showcase  `[S]`  ·  id: `flowchart-demo`

**Goal.** Add the third demo (`demos/flowchart.c`) the spec §2/§15 calls for — the deliverable that proves groups
+ auto-layout together. It builds a flowchart-style graph (decision/process nodes), boxes a subset into a `group`
container, runs `flow_layout` (layered) to arrange it, and is fully interactive (drag, group/ungroup keys, layout
key). NOT a fifth engine feature — a deliverable GATED on groups AND auto-layout.

**User value.** A runnable third demo that shows off the two capstone features end-to-end: a user launches
`demos/flowchart`, sees an auto-laid-out flowchart with a labeled group container, and can drag nodes, nest/unnest
into the group, and re-run the layout — concrete proof the grouping + layout subsystems work together.

**Files touched.**
  - demos/flowchart.c
  - Makefile (add the `flowchart` target to the `demos:` rule)
  - tests/test_flowchart.c (optional smoke test; or extend an existing smoke test)

**Entry points (existing functions to extend).**
  - `demos/topo.c` / `demos/hello_flow.c` — structural templates for the demo skeleton (flow_new, register_defaults, on_overlay panel, flow_run loop)
  - `Makefile` `demos:` rule — add `$(CC) $(CFLAGS) demos/flowchart.c -o demos/flowchart $(LIBS)`
  - `flow_group`/`flow_ungroup`/`flow_set_parent` (groups) — wired to keys (e.g. 'g'/'G' via `flow_bind_key`)
  - `flow_layout`/`flow_layout_layered` (auto-layout) — wired to a key (e.g. 'l') and/or run once at startup
  - `flow_set_callbacks` / `on_overlay` — draw a key-hint panel

**API additions.**
```c
/* None — flowchart.c is a DELIVERABLE (a demo), not an engine package. It consumes only the
   public API from groups (flow_group/flow_ungroup/flow_set_parent), auto-layout (flow_layout/
   flow_layout_layered), and the settled inc-2 surface (flow_new/flow_run/flow_feed/flow_bind_key/
   flow_set_callbacks). No new library symbol is introduced. */
```

**Design notes.**

This is glue, not engine work. Build a small flowchart graph (a handful of default or lightly-customized nodes
with edges forming a DAG — start → process → decision → branches). Group a subset of nodes into a labeled `group`
container via `flow_group`. Call `flow_layout(f, {.mode=FLOW_LAYOUT_LAYERED, .dir=FLOW_TB, .fit_after=1, .margin=2})`
at startup so the demo opens framed and arranged. Bind keys via `flow_bind_key`: 'l' to re-run layout, 'g' to
group the current selection (`flow_selected_nodes` → `flow_group`), 'G' to ungroup the selected group, plus the
built-in drag/select/connect/zoom from the engine. Draw a one-line key-hint via `on_overlay` (or rely on the
built-in statusbar). Keep it dependency-free and ASan/UBSan-clean like the other demos.

Because it exercises `flow_group` + `flow_layout` + drag-to-reparent, it doubles as an integration smoke test for
the two capstone packages. Optionally add a tiny `tests/test_flowchart.c` that constructs the same graph headless
and asserts layout produced non-overlapping ranks + the group encloses its children (reuse the test_groups /
test_layout assertion style) — or fold a smoke assertion into an existing test.

LANDS LAST: depends on both groups and auto-layout being merged. Defer any `demos/topo.c` showcase-merge to the
same final integration pass.

**Test plan.**
  - `make demos` builds `demos/flowchart` clean (added to the `demos:` rule).
  - Run `demos/flowchart` under ASan/UBSan with a short synthetic input feed (or just startup + quit) → no leaks/UB, exit 0.
  - Optional tests/test_flowchart.c: build the demo's graph headless, `flow_group` a subset, `flow_layout` LAYERED; assert no node-cell overlaps, ranks monotonic, and the group rect encloses its children (`flow_node_rect_abs` containment). Add to Makefile TESTS if created.
  - The 'l'/'g'/'G' key bindings invoke layout/group/ungroup (verify via `flow_feed` in the optional test).

**Acceptance.**
  - `demos/flowchart.c` exists and builds with `make demos`; it opens auto-laid-out (layered) and framed, with a labeled group container around a node subset.
  - The demo is interactive: drag nodes, group/ungroup via keys, re-run layout via a key; drag-to-reparent into the group works (from groups).
  - Running the demo (startup→quit, or a short synthetic feed) is ASan/UBSan-clean.
  - If a headless smoke test is added, `make test` includes it and it passes; otherwise the demo build itself is the gate.

**Depends on.**
  - **groups** (HARD) — uses `flow_group`/`flow_ungroup`/`flow_set_parent` + the `group` built-in type + drag-to-reparent.
  - **auto-layout** (HARD) — uses `flow_layout`/`flow_layout_layered`.
  - settled inc-2: `flow_run`/`flow_feed`/`flow_bind_key`/`on_overlay`/zoom/select for the interactive shell.

**Conflicts with.**
  - `demos/topo.c` showcase merges (serialize/undo/auto-layout) — defer all demo wiring to a single final integration pass; `demos/flowchart.c` is a new file so it only collides on the Makefile `demos:` rule (append-only).

**Carry-overs fixed.**
  - Delivers the spec §2/§15 `flowchart.c` third demo (the groups + auto-layout proof) that `demos/` was missing.

---
