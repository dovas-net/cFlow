# flow — Increment-4 work packages: the Events & graph API increment

Eleven work packages + a final integration pass turning flow's settled interaction engine into a
**programmable platform**: observer events for edges / connections / the viewport, an engine-level
connection validator, graph-traversal and spatial queries, extent clamps, and node/edge visibility.
Theme chosen from the [feature inventory](2026-06-03-xyflow-feature-inventory.md) shortlist
(its top value÷effort items, minus the two already landed by the increment-3 integration pass).

Each package section was drafted against the LANDED code on `main` and its `file:line` citations
verified there; re-verify line numbers at implementation time (earlier packages shift later lines).

## Current state (what's already built, on `main`)

Increments 1–3 delivered the full interactive engine — 22 green tests, byte-locked snapshot
goldens, three demos (`hello_flow`, `topo`, `flowchart`):

- **Model & interaction:** nodes/edges with vtable types, selection (click/modifier/marquee),
  drag (multi-drag, drag-to-reparent), port-handle connections + endpoint reconnect, zoom/LOD,
  space-pan, auto-pan (`flow_set_autopan`), key registry (`flow_bind_key`) + built-ins.
- **Structure:** groups (parent-relative coords, containers, depth z-order), JSON persistence
  (`flow_save`/`flow_load`, node-data hooks), undo/redo (capped inverse-op journal, txn brackets).
- **Layout:** `flow_layout()` — deterministic force-directed + layered modes, group-local passes,
  one-undo-step commit.
- **Observers so far** (`flow_callbacks`): `on_overlay`, `on_node_context`, `on_node_click`,
  `on_pane_click`, `on_connect`, `on_node_dblclick`, `on_selection_change`, `on_nodes_delete`.
- **Render:** orthogonal + straight edge routers, minimap, background grid, statusbar
  (PAN-mode indicator; help hints appended past the locked golden's column 30).

## Execution order (recommended)

Four blocks; sequential spine, one commit per package:

| # | id | size | block |
|---|----|------|-------|
| 1 | `esc-selection` | S | A — interaction foundations |
| 2 | `marquee-autopan` | S | A |
| 3 | `extent-clamps` | S | A |
| 4 | `parent-extent` | S | A |
| 5 | `viewport-events` | S | B — observer events |
| 6 | `edge-events` | S | B |
| 7 | `connect-lifecycle` | M | B |
| 8 | `graph-traversal` | M | C — graph query API |
| 9 | `valid-connection` | S | C |
| 10 | `intersect-query` | S | C |
| 11 | `hidden` | M | D — engine flag (golden-churn risk → last) |
| — | final integration pass | — | D — accumulated deferrals + demo wiring |

Hard dependency: **#10 requires #8** (same new module `src/flow_query.h`). Soft ordering: **#3
before #5** — extent-clamps lands its clamps at the existing view-write sites; viewport-events then
*relocates* them inside its new `flow__view_set` seam (both packages specify both orders, but this
is the pinned one). #11 lands last among features: it touches the render/hit loops, so it carries
the only golden risk in the set.

**Final integration pass** (single commit, after #11): the accumulated deferrals recorded in
package commit bodies, plus demo wiring — prevent-cycles validation in `demos/flowchart.c`
(compose #8's reachability with #9's validator), an edge context-menu/ticker entry in
`demos/topo.c` (#6), a hidden-toggle showcase (#11), and any statusbar hint additions (append-only
past column 30; the `render_statusbar` golden locks the prefix).

## Cross-cutting rules (pinned — every package honors these)

- **Flag bits:** the node-flags enum (`src/flow_model.h:2`) gains `FLOW_HIDDEN = 8u` (#11) and
  `FLOW_EXTENT_PARENT = 16u` (#4). Both packages edit the same enum line — mutual conflict note;
  whichever lands second rebases trivially.
- **`flow_callbacks` append rule:** #5, #6, #7 all add observer fields. Each package APPENDS its
  fields immediately BEFORE the trailing `void *user;`, in package landing order — final tail:
  `…, on_viewport_change, on_edge_click, on_edge_context, on_edge_dblclick, on_connect_start,
  on_connect_end, user`. Zero-init stays valid (`flow_callbacks cb = {0}` keeps working).
- **Model vs view layering:** `hidden` is a VIEW-level concept — render, hit-tests, handles,
  marquee, bounds/fit, minimap skip hidden elements. MODEL-level operations — traversal and
  intersection queries (#8/#10), layout, serialize — include them. Hidden is neither journaled
  nor persisted in v1.
- **Engine-level validation:** the #9 validator gates `flow_add_edge` AND `flow_reconnect_edge`
  (programmatic too) — a deliberate divergence from xyflow's interactive-only gating.
- **Discipline (unchanged from increment-3):** TDD red-first per package; `flow.h` is GENERATED
  (edit `src/`, run `tools/amalgamate.sh` — `modules=` is an explicit ordered list, #8 inserts
  `flow_query` AFTER `flow_undo`, BEFORE `flow_view`); new test files append to `Makefile`
  `TESTS=`; whole-suite ASan/UBSan gate per package; existing goldens stay byte-identical;
  one commit per package, sole author, NO trailers.

## Packages

### 1. ESC clears selection  `[S]`  ·  id: `esc-selection`

**Goal.** Extend the lone-ESC branch in `flow_feed` (currently at `src/flow_run.h:45`) to also clear the selection, matching xyflow's behavior. The ESC key already cancels an in-flight connection and exits space-pan mode; add a third idempotent action: clearing the FLOW_SELECTED set via `flow_clear_selection`.

**User value.** Users expect ESC (a universal escape key) to restore a blank slate: no in-flight connection, no pan mode, AND no selection. The landed behavior is a two-thirds solution; this package completes the semantic contract to match standard UI conventions (xyflow, most graph editors).

**Files touched.**
  - src/flow_run.h
  - tests/test_select.c
  - Makefile (not touched — test_select already in TESTS list)

**Entry points (existing functions to extend).**
  - `flow_feed` lone-ESC branch (`src/flow_run.h:45`) — add a `flow_clear_selection(f)` call alongside the existing `flow_cancel_connection(f)` and `f->space_held = 0`
  - `flow_clear_selection` (`src/flow_model.h:690`) — already exists and fires `on_selection_change` sig-gated; no edit needed, just invoke it
  - `flow__notify_selection` (`src/flow_model.h:656`) — sig-gating already ensures the callback fires only on actual selection change (no event if selection was already empty)

**API additions.**
```c
/* No new public API — flow_clear_selection already exists at src/flow_model.h:690 and is already exported.
   The ESC branch in flow_feed gains a call to the existing function. */
```

**Design notes.**

The lone-ESC branch at `src/flow_run.h:45` currently documents a read()-boundary trade-off: a CSI split exactly after its ESC byte reads as a lone ESC here, though terminals write sequences atomically (a real fix requires an ESC-timeout state machine, out of scope for v1). This comment is PRESERVED as-is; it remains valid and ACCEPTED.

ESC is UNCONDITIONAL and IDEMPOTENT, like the landed pattern: it always attempts to cancel the connection (no-op if none), toggle off space_held (no-op if already off), AND clear selection (no-op if already empty). The sig-gated callback ensures `on_selection_change` fires once per ESC only if the selection set actually changes — calling `flow_clear_selection` when the selection is already empty results in no callback fire (verified by `flow__sel_sig` and `flow__notify_selection`'s early return at `src/flow_model.h:658`).

The `flow_clear_selection` function (src/flow_model.h:690) already clears both node AND edge selections, so a single call handles the full set. No `cb_suppress` gating is needed because this is not an aggregate mutator — it is a leaf operation that fires its own callback via the sig-gating contract already in place.

**Test plan.**
  - Extend tests/test_select.c (the dedicated selection test): add a new test block after the multi-select block:
    1. Select two nodes via `flow_select_node(f, a, 0)` and `flow_select_node(f, b, 1)`.
    2. Assert `flow_selected_count(f) == 2` (precondition).
    3. Set a callback `on_selection_change` counter to 0.
    4. Feed `"\x1b"` (lone ESC, not a CSI prefix).
    5. Assert `flow_selected_count(f) == 0` (selection cleared).
    6. Assert the callback counter == 1 (event fired once on the change).
    7. Feed `"\x1b"` again (ESC with nothing selected).
    8. Assert `flow_selected_count(f) == 0` (still empty).
    9. Assert the callback counter == 1 (NO additional event; sig-gated).
  - ASan/UBSan clean: no leaks from the selection mutation.

**Acceptance.**
  - Feeding `"\x1b"` (lone ESC) to a graph with a non-empty selection clears all selected nodes and edges, and fires `on_selection_change` exactly once.
  - Feeding `"\x1b"` to a graph with an empty selection fires NO `on_selection_change` event (sig-gated).
  - The selection is cleared without disturbing any other state (panning, hovering, node positions).
  - `flow_feed` + `flow_present` still render without error; `make test` passes including the new asserts.
  - flow.h regenerated (no module added; edit only in src/flow_run.h).

**Depends on.** nothing (uses the already-landed `flow_clear_selection` API and sig-gating pattern).

**Conflicts with.**
  - Any package that rewrites the ESC branch in `flow_feed` or changes `flow_clear_selection` behavior — none in this set does.

**Carry-overs fixed.**
  - Completes the ESC-as-universal-escape semantic (cancel connection + exit pan + clear selection) per xyflow convention.

---

### 2. Marquee auto-pan on selection  `[S]`  ·  id: `marquee-autopan`

**Goal.** Enable auto-panning during shift-drag marquee selection, matching the existing auto-pan behavior already implemented for node-drag, connection-drag, and reconnect-drag. Currently the marquee drag explicitly gates out auto-pan (`src/flow_input.h:55` comment "callers gate: … marquee drags never auto-pan"). Integrate the `flow__autopan(f, scr)` call into the marquee motion branch so the viewport pans when the marquee cursor enters the edge bands, just as it does for other object drags.

**User value.** A user can now marquee-select nodes beyond the initial viewport bounds without stopping the drag to manually pan, enabling fluid large-graph selection across the visible boundary. The UX becomes consistent with node-drag and connection-drag, both of which already auto-pan.

**Files touched.**
  - src/flow_input.h
  - tests/test_autopan.c
  - Makefile (no change: test_autopan already exists)

**Entry points (existing functions to extend).**
  - `flow__autopan` (`src/flow_input.h:58`) — the internal auto-pan seam; called by node-drag, connection-drag, and reconnect-drag motion branches; add a call in the marquee motion branch
  - `flow_handle_mouse` (`src/flow_input.h:64`) — marquee motion lives in the `if (f->marquee_on)` block (`src/flow_input.h:183`); the block updates `f->marquee_cur = scr` and calls `flow_select_in_rect(f, wr, …)` — insert the `flow__autopan(f, scr)` call before the rect calculation so the pan occurs before re-selection
  - `flow_to_world` (`flow.h:814`) — calls `flow_unproject` from `src/flow_geom.h:20`; used implicitly in the marquee rect calculation (`src/flow_input.h:185`); no direct edit needed, but the section's design notes must explain the coordinate stability

**API additions.**
```c
/* No new public API. The flow__autopan call and header comment edit are internal. */
```

**Design notes.**

MARQUEE COORDINATES & VIEWPORT STABILITY: The marquee rect is calculated each frame from two screen-coordinate points: `marquee_anchor` (set on drag arm, `src/flow_input.h:169`) and `marquee_cur` (updated per motion, `src/flow_input.h:184`). The live rect logic (lines 185–189) calls `flow_to_world()` on both, converting screen to world via the current viewport's offset and zoom. 

When `flow__autopan(f, scr)` fires and calls `flow_pan(f, dx, dy)`, it updates `f->view.ox` and `f->view.oy`. On the next motion event, `flow_to_world()` recomputes the world-coordinate anchor using the NEW viewport parameters, so the anchor's world position shifts. This is the desired behavior: the marquee "chases" the screen-coordinate anchor as the world scrolls, keeping the same world-coordinate region selected. **The selection remains world-stable; nodes at the same world coords stay selected even if the viewport pans.** This matches the group-drag path (`src/flow_input.h:190–221`), which also pans then re-places via post-pan world coords.

IMPLEMENTATION: Insert `flow__autopan(f, scr)` at the **start** of the marquee motion block (before line 185, the rect calculation), not after. This matches the node-drag pattern (`src/flow_input.h:191–193`), where the pan precedes the re-selection. The autopan function is idempotent (no pan if the cursor is interior), so the call is safe.

UPDATE COMMENT: Edit line 55 of `src/flow_input.h` from "Callers gate: pane-pan and marquee drags never auto-pan" to "Callers gate: pane-pan drags never auto-pan (marquee drags auto-pan as of increment 4)" to document the change.

**Test plan.**
  - tests/test_autopan.c (revert/invert): The existing subtest at line 130–140, `"marquee drag near the edge does NOT auto-pan"`, asserts that `org(f).x` and `org(f).y` remain unchanged when the marquee cursor enters the edge bands. **Invert these assertions:** after the shift-drag to the bottom-right bands, assert that `org(f).x` and `org(f).y` DID pan (e.g., `ASSERT_INT(org(f).x, o0.x - 2, "marquee near right edge: x DOES auto-pan")` and the corresponding y assertion). This is the deliberate contract reversal pinned in increment-4.
  - MARQUEE WORLD-STABILITY subtest (new): Create a node A at world (10, 5), render and grab anchor at screen (50, 10). Drag the marquee to screen (79, 22) near the bottom-right bands. Record the pan delta (`dp_x = org_pre.x - org_post.x`, etc.). Continue the drag to screen (60, 15), an interior position. Assert that `org(f)` remains at `(o0.x - dp_x, o0.y - dp_y)` — the pan is stable and doesn't reverse. Close the marquee by releasing. This confirms that the world-coordinate rect stays consistent through the auto-pan and interior motions.
  - SMALL BUFFER DEGENERATE (inherit from line 142–156): Confirm that a tiny buffer (where margins overlap) still doesn't auto-pan its disabled axis, even with marquee.
  - `make test` green; ASan/UBSan on test_autopan — no heap leaks from the select-in-rect calls.

**Acceptance.**
  - A shift-drag marquee near a viewport edge now auto-pans the same way node-drag does; the viewport pans smoothly in the edge bands.
  - The marquee selection stays world-stable: the set of selected nodes does not change as the viewport pans, because the marquee rect is recalculated in world coords after each pan.
  - The test `"marquee drag near the edge does NOT auto-pan"` is inverted to assert it DOES, confirming the contract change is intentional (noted in the commit body).
  - `make test` passes including the inverted autopan subtest and the new world-stability subtest; `flow.h` regenerated (edit in `src/flow_input.h` only).

**Depends on.** nothing (independent; only edits the marquee motion branch of the existing `flow_handle_mouse`).

**Conflicts with.**
  - Any package that rewrites the marquee motion branch or the `flow__autopan` internal function — none in the set do.

**Carry-overs fixed.**
  - Removes the explicit gate documented at `src/flow_input.h:55`, aligning the marquee UX with the spec's requirement that auto-pan is uniform across all object drags.

---

### 3. Extent clamps  `[S]`  ·  id: `extent-clamps`

**Goal.** Implement node and viewport extent constraints: `flow_set_node_extent` clamps every
`flow_move_node` target so the node rect stays inside a world-space boundary; `flow_set_translate_extent`
clamps pan/zoom so the visible viewport stays inside a separate boundary. Both are optional per-engine
limits (sentinel: extent.w≤0 or h≤0 = disabled, the default on zero-init `struct flow`). Encode the
clamp math precisely — zoom-aware for the translate extent (visible world size = cols/zoom × rows/zoom;
cap ox/oy; behavior when extent is smaller than the window).

**User value.** Graph apps can confine node placement to a restricted region (e.g. a canvas boundary
or a staging area) and lock the viewport within viewing bounds (e.g. prevent panning off-graph).
Both are common in interactive graph editors; xyflow's `nodeExtent` and `translateExtent` enable this.
Specs §6.1/§6.2 list them as distinct; v1 ships with both unconstrained (zero-init silent disable).

**Files touched.**
  - src/flow_model.h (two new flow_rect fields; clamping logic in flow_move_node, flow_pan, flow_set_zoom, flow_fit_view)
  - src/flow_view.h (if viewport-events lands first, clamping lives inside flow__view_set seam; else flow_set_zoom/flow_fit_view inline the logic)
  - tests/test_extents.c (new test file for extent-specific tests)
  - Makefile (append test_extents to TESTS=)

**Entry points (existing functions to extend).**
  - `flow_move_node` (`src/flow_model.h:463`) — after parent-relative conversion, clamp node rect against node_extent
  - `flow_pan` (`src/flow_model.h:641`) — clamp ox/oy after the delta
  - `flow_set_zoom` (`src/flow_view.h:17`) — already clamps to [zmin,zmax]; add translate_extent viewport clamp AFTER zoom is set
  - `flow_fit_view` (`src/flow_model.h:863`) — after computing ox/oy, clamp them against translate_extent bounds

**API additions.**
```c
/* ---- src/flow_model.h (struct field additions) ---- */
/* Inside struct flow (around flow_viewport view; flow_rect fields) */
flow_rect node_extent;        /* clamp target for flow_move_node; w<=0 or h<=0 = disabled */
flow_rect translate_extent;   /* clamp viewport pan/zoom; w<=0 or h<=0 = disabled */

/* ---- src/flow_model.h (or flow_view.h if viewport-events lands first) ---- */
void flow_set_node_extent(flow_t *f, flow_rect world);      /* set world-space node constraint (e.g. canvas bounds); zero rect = disabled */
void flow_set_translate_extent(flow_t *f, flow_rect world); /* set world-space viewport constraint; zero rect = disabled */

/* ---- Helper (inline in flow_model.h impl, not public) ---- */
static flow_rect flow__clamp_viewport(flow_t *f, float ox, float oy);
/* Returns (ox, oy) clamped so the visible world window [ox/z, oy/z, ox/z+cols/z, oy/z+rows/z]
   stays within translate_extent. If translate_extent is disabled, returns (ox, oy) unchanged. */
```

**Design notes.**

NODE EXTENT: A disabled extent (w≤0 or h≤0) is a no-op; an enabled extent is a world-space rect.
When `flow_move_node(f, id, pos)` commits a parent-relative target, the node's rect must fit
inside extent. Clamping is AFTER parent-relative conversion (absolute world space): read the
node's final absolute position, compute its bounding rect (abs_x, abs_y, n->w, n->h), then
clamp so the rect fits. For a target that would exceed the extent, shift the node so its rect
just touches the limit (e.g. if a node's right edge exceeds extent.x+extent.w, set node.x so
node.x+node.w == extent.x+extent.w, i.e. flush right).

TRANSLATE EXTENT: a disabled extent is a no-op. When enabled, the viewport's visible world
window (projected screen→world at the four corners of the display) must fit inside translate_extent.
At zoom z, the visible world size is cols/z × rows/z (floating-point). SIGN CONVENTION — derive
from `flow_project` (`src/flow_geom.h:14`: screen = world·z + ox): world = (screen − ox)/z, so
the visible window top-left is (−ox/z, −oy/z) and the bottom-right is ((cols−ox)/z, (rows−oy)/z).
Keeping that window inside the extent gives the ox clamp range
`[cols − (extent.x+extent.w)·z, −extent.x·z]` (and the analog for oy) — note LARGER world x
means SMALLER ox (`flow_feed`'s right-arrow calls `flow_pan(f, -1, 0)` for exactly this reason).
Cross-check: `flow_fit_view` (`src/flow_model.h:876`) computes `ox = cols/2 − center·z`, the same
inverted form. If the visible window is
LARGER than the extent on an axis (cols/z > extent.w or rows/z > extent.h), both inequalities
cannot hold — PINNED resolution (d3-zoom's translateExtent convention, which xyflow inherits):
CENTER the extent in the window on that axis, i.e. the ox/oy clamp degenerates to the single
value that puts extent's midpoint at the window's midpoint. Deterministic, no oscillation, and
pan attempts on that axis become no-ops until zoom-in makes the window smaller than the extent.
Same for y, per-axis independently.

CLAMPING POINTS: (1) `flow_move_node` — after parent-relative write, clamp the resulting
absolute position. (2) `flow_pan(f, dx, dy)` — after f->view.ox/oy += dx/dy, clamp them.
(3) `flow_set_zoom` — after setting f->view.zoom, clamp f->view.ox/oy via the helper.
(4) `flow_fit_view` — after computing the fit's ox/oy, clamp them before writing to f->view.

VIEWPORT-EVENTS COORDINATION: If viewport-events lands first, this package's
`flow__clamp_viewport` should be called INSIDE the flow__view_set seam (the centralizing
mutator). If extent-clamps lands first, flow__view_set will call this helper on every
view.ox/oy/zoom write. Either order works; the spec notes it as a conflict name.

UNDO: node_extent and translate_extent are VIEW-like metadata, NOT journaled (like zoom limits).
Persisting them in save/load is v2; v1 leaves them transient (set after flow_load if needed).

**Test plan.**
  - tests/test_extents.c (new):
    - Node extent disabled (zero rect): `flow_move_node` is unclamped (regression on old test).
    - Node extent enabled: place a node at (10,10) with w=4,h=3; set node_extent = {0,0,20,20}.
      Try to move node to (18,18) — rect would span to (22,21), exceeds extent. Assert final
      position clamps so rect is {16,17,4,3} (flush to extent edges: 16+4==20, 17+3==20).
      Try move to (-5,-5) → clamp to {0,0,4,3}.
    - Translate extent disabled: `flow_pan` / `flow_set_zoom` / `flow_fit_view` unaffected
      (regression).
    - Translate extent enabled (e.g. {0,0,100,100}); window (cols=30, rows=10, zoom=1.0).
      Visible world at default ox=0,oy=0 = [0,0,30,10]. Pan the VIEW right by 100 cells
      (`flow_pan(f, -100, 0)` — right-arrow semantics; ox would become -100, visible
      [100,0,130,10]) — exceeds extent.x+extent.w=100, so ox clamps to 30-100·1 = -70
      (visible [70,0,100,10] flush right).
    - Translate extent smaller than window: cols=60, extent={0,0,40,40}, zoom=1.0 (visible
      60×10, wider than extent.w=40). Pan attempts on x are no-ops: ox stays pinned at the
      single centered value ox = cols/2 − (extent.x + extent.w/2)·z = 30 − 20 = +10 (the
      flow_fit_view form); assert ox unchanged after flow_pan(±50, 0) and that y (where
      rows/z = 10 < 40) still pans within its normal clamp range oy ∈ [10−40, 0] = [−30, 0]
      (per-axis independence).
    - `flow_fit_view` fits graph, then clamp: place nodes, fit (sets zoom/pan), assert result
      fits within translate_extent (if set).
    - ASan/UBSan: no leaks, no UB from rect math.

**Acceptance.**
  - `flow_set_node_extent(f, {0,0,100,100})` is set; `flow_move_node(f, id, {95,95})` with
    node w=10,h=10 clamps so the rect stays in [0,0,100,100] — final pos {90,90} (or similar
    flush-to-edge).
  - `flow_set_translate_extent(f, {0,0,100,100})` is set; `flow_pan(f, dx, dy)` and
    `flow_set_zoom` enforce the visible window stays inside the extent.
  - Extent disabled (default; zero w or h) is a no-op: all moves/pans/zooms behave as before
    (no regression to existing tests).
  - `flow_fit_view` respects translate_extent after computing fit zoom/pan.
  - `make test` passes; whole-suite ASan/UBSan clean.
  - `flow.h` regenerated (no new module; edits in `flow_model.h` only, or `flow_view.h`
    depending on viewport-events order).

**Depends on.** nothing (can land before or after viewport-events; coordinates via seam choice).

**Conflicts with.**
  - `viewport-events` — both touch every view.ox/oy/zoom write site. If viewport-events lands
    first, extent-clamps clamping moves INSIDE the flow__view_set seam (centralizing mutator);
    if extent-clamps lands first, viewport-events wraps the existing clamping calls. Explicit
    coordination needed; not a blocker (one-line refactor to move the clamping logic either way).

**Carry-overs fixed.**
  - Implements the xyflow feature spec §6.1 (nodeExtent) and §6.2 (translateExtent) — bounds
    constraints on node placement and viewport pan/zoom, completing the v1 interactive constraint
    set (zoom limits already present, selection/hover flags present, etc.).

---

### 4. Parent-extent child clamp  `[S]`  ·  id: `parent-extent`

**Goal.** Add a new flag bit `FLOW_EXTENT_PARENT=16u` to the flags enum (`src/flow_model.h:2`) that gates child-node clamping. When set on a node with `parent != -1`, `flow_move_node` clamps the target position so the child's rect (width, height) stays fully inside the parent's absolute rect. Drag interactions and layout positioning both respect the clamp automatically.

**User value.** A user (or app author) can mark a node with the `FLOW_EXTENT_PARENT` flag to keep it constrained within its parent container. The drag path and layout algorithms honor the constraint without explicit app coordination; groups or structured hierarchies can enforce spatial containment at the model level.

**Files touched.**
  - src/flow_model.h (flags enum, flow_move_node)
  - tests/test_model.c (or dedicated tests/test_extent.c if needed)
  - Makefile (only if a dedicated test_extent.c is added; otherwise extend test_model)

**Entry points (existing functions to extend).**
  - Flags enum (`src/flow_model.h:2`) — add `FLOW_EXTENT_PARENT=16u` alongside existing `FLOW_SELECTED=1u, FLOW_DRAGGING=2u, FLOW_HOVERED=4u`
  - `flow_move_node` (`src/flow_model.h:463`) — after computing the parent-relative delta, clamp the target to keep `child_rect_abs` inside `parent_rect_abs`; applies AFTER node-extent clamp if both flags are set
  - `flow_node_rect_abs` (`src/flow_model.h:50`) — existing abs-rect query, used by clamp logic to fetch parent bounds
  - Flow model + layout interaction (`src/flow_layout.h:32` `flow_layout` / lines 300–310 normalize/commit path) — document that layout positions are normalized to (1,1) inside containers, which naturally satisfies the clamp for typical graphs; if layout output overflows the container, the clamp now restricts it instead of expanding (behavioral interaction)

**API additions.**
```c
/* ---- src/flow_model.h:2 (flags enum) ---- */
enum { FLOW_SELECTED = 1u, FLOW_DRAGGING = 2u, FLOW_HOVERED = 4u, FLOW_EXTENT_PARENT = 16u };
```

**Design notes.**

The flag is public and set directly on `n->flags` (idiomatic in the codebase: `n->flags |= FLOW_EXTENT_PARENT;` / `n->flags &= ~FLOW_EXTENT_PARENT;`). No setter helper is needed.

CLAMP LOGIC: In `flow_move_node`, after storing the parent-relative position, check if the child has the flag set and a valid parent. If so, compute the child's absolute rect (pos + w,h) and the parent's absolute rect; clamp the child's target so it stays inside [parent.x, parent.y, parent.x+parent.w, parent.y+parent.h]. The clamped position is converted back to parent-relative and stored. This preserves the parent-chain walk already in `flow_move_node` (lines 470–472).

ORDERING: The parent clamp applies AFTER the node-extent clamp (if both flags are present). Node-extent clamps to the view; parent-extent clamps to the parent container. The sequences are: (1) compute the unclamped target, (2) apply node-extent clamp if set, (3) apply parent-extent clamp if set.

LAYOUT INTERACTION: `flow_layout` normalizes all positions in a partition to a local bbox with min→(0,0) for top-level or (1,1) for containers (src/flow_layout.h:300–305). These local coordinates are then added to the parent's absolute position (line 309: `pa.x + out[i].x + bx`). For typical node sizes and gap values, the result naturally stays inside the parent. However, if the layout algorithm positions a child to overflow the parent (e.g., an unusual size ratio), the clamp now restricts the overflow instead of letting the parent expand. This is a deliberate behavioral change: layouts commit via `flow_move_node`, so the clamp is transparent to the layout code itself.

**Test plan.**
  - tests/test_model.c (extend): create a parent node P(10,10) w=30 h=20; a child C(15,15) w=4 h=3 with `FLOW_EXTENT_PARENT` set; call `flow_move_node(f, C.id, (20,28)` — target would overflow bottom-right; assert `flow_get_node(f, C.id)->pos` is clamped so `C.x + C.w <= P.x + P.w` and `C.y + C.h <= P.y + P.h` (accounting for parent-relative math). Try moves to all four edges and corners; assert clamping in each case. Try a move that does NOT overflow (e.g., (15,15)` still in bounds); assert no clamp applied.
  - Flags interaction: set `FLOW_EXTENT_PARENT` on an unparented node; call `flow_move_node` with a far target — assert NO clamp (parent==-1 guards the logic). Toggle the flag off; assert subsequent moves are unclamped.
  - Layout with clamp: create a small parent (10,10) w=5 h=5; two children (11,11) and (12,12) w=2 h=2 each; set `FLOW_EXTENT_PARENT` on both; call `flow_layout` with default opts; assert all children are clamped inside the parent (spot-check a few positions; no child corner exceeds parent bounds).
  - ASan/UBSan gate: `make test` with address+undefined sanitizers; no leaks or UB from the clamp logic.

**Acceptance.**
  - A node with `FLOW_EXTENT_PARENT` set and `parent != -1` is clamped by `flow_move_node` to stay inside its parent's absolute rect.
  - Clamping does not apply if `parent==-1` or the flag is clear.
  - Parent-extent clamp applies AFTER node-extent clamp (if present).
  - Layout's transparent use of `flow_move_node` respects the clamp without layout-specific logic; overflowing layouts now clamp instead of overflow.
  - Tests verify clamping on all four edges, corner overflow cases, flag-off/parent-absent guards, and layout interaction.
  - `make test` passes; ASan/UBSan clean.

**Depends on.** nothing (independent; only extends the flags enum and flow_move_node).

**Conflicts with.**
  - `hidden` package — both edit line 2 of src/flow_model.h (the flags enum). Order: `parent-extent` adds `FLOW_EXTENT_PARENT=16u`; `hidden` adds `FLOW_HIDDEN=8u`. Both fit in the same enum; the edit is a simple add to the list. Use a deliberate conflict note in the commit message: "NOTE: hidden package also edits this enum line; mutual conflict resolved by both appending distinct flag bits."

**Carry-overs fixed.**
  - Spec §9 names "extent:'parent'" as a node containment mode; this package realizes it at the model level, enabling structured hierarchies to enforce spatial boundaries.

---

### 5. on_viewport_change observer  `[S]`  ·  id: `viewport-events`

**Goal.** Centralize all viewport mutations (pan, zoom, fit-view, load-restore) through an internal seam and fire an `on_viewport_change` callback when the viewport actually changes (all three components: ox, oy, zoom differ from the prior state). The callback runs after the mutation completes, enabling the app to react to view state changes without polling.

**User value.** An app author can attach an `on_viewport_change` callback and know that every significant viewport change — pan (interactive, wheel, autopan), zoom (pointer-centered, zoom-limits re-clamp, fit-view), or restore-on-load — fires exactly once per change, with the new viewport delivered. Apps can sync a minimap, a view-indicator, remote collaborators' viewports, or undo-stack metadata to viewport state without race conditions. No-op pans (e.g., `flow_pan(0,0)`) fire nothing (changed-only gate, like `on_selection_change`).

**Files touched.**
  - src/flow_model.h (internal seam: flow__view_set; callback field in flow_callbacks; struct flow depth guard if needed)
  - src/flow_view.h (flow_set_zoom routes through seam)
  - src/flow_json.h (flow_load routes restore through seam)
  - tests/test_viewport_events.c (new file)
  - Makefile (append test_viewport_events to TESTS=)

**Entry points (existing functions to extend).**
  - `flow_pan` (`src/flow_model.h:641`) — implemented inline; route through seam
  - `flow_set_zoom` (`src/flow_view.h:2`) — writes view fields at lines 26–28; route through seam
  - `flow_fit_view` (`src/flow_model.h:863`) — writes view fields at lines 876–878; route through seam
  - `flow_load` viewport restore (`src/flow_json.h:312`) — writes view fields; route through seam
  - `flow_callbacks` (`src/flow_model.h:181`) — add callback field
  - `flow_set_callbacks` (`src/flow_model.h:192`) — already exists, no edit needed (just docs that the new field is optional/NULL)
  - `struct flow` internals (`src/flow_model.h:203`) — optional depth guard (see Design notes)

**API additions.**
```c
/* ---- src/flow_model.h: added callback field ---- */
typedef struct {
  /* ... existing fields on_overlay, on_node_context, ... on_nodes_delete ... */
  void (*on_viewport_change)(flow_t *f, flow_viewport vp, void *user); /* fired after a pan/zoom/fit/load mutates the viewport, only on actual change (all three of ox/oy/zoom must differ from prior state) */
  void *user;
} flow_callbacks;

/* ---- internal seam (defined in flow_model.h, used by flow_view.h and flow_json.h) ---- */
static void flow__view_set(flow_t *f, float ox, float oy, float zoom);
/* Internal: route all viewport mutations (pan, zoom, fit, load) through this seam.
   Compares the new viewport (ox, oy, zoom) against the current f->view.
   If all three differ, or all three are identical (no-op), only fires the callback
   if at least one component changed.
   Caller must NOT hold a re-entrancy guard (analysis below); flow__view_set handles
   any internal recursion via callback via cb_suppress pattern if it emerges. */
```

**Design notes.**

VIEWPORT CENTRALIZATION: All mutations write view fields:
  1. `flow_pan(f, dx, dy)` — currently `f->view.ox += dx; f->view.oy += dy;` (src/flow_model.h:641)
  2. `flow_set_zoom(f, zoom, center)` — writes `f->view.ox`, `f->view.oy`, `f->view.zoom` (src/flow_view.h:26–28)
  3. `flow_fit_view(f, margin)` — writes all three (src/flow_model.h:876–878)
  4. `flow_load` viewport restore (src/flow_json.h:307–312) — reads from JSON, writes all three

The seam `flow__view_set(f, ox, oy, zoom)` is internal to flow_model.h and inlined (no export). It:
  - Saves the old viewport: `flow_viewport old = f->view;`
  - Sets new values: `f->view.ox = ox; f->view.oy = oy; f->view.zoom = zoom;`
  - Fires the callback ONLY if any of the three changed: `if (memcmp(&old, &f->view, sizeof(flow_viewport)) != 0) { if (f->cb.on_viewport_change) f->cb.on_viewport_change(f, f->view, f->cb.user); }`

CALLER EDITS:
  - `flow_pan`: replace `f->view.ox += dx; f->view.oy += dy;` with `flow__view_set(f, f->view.ox + dx, f->view.oy + dy, f->view.zoom);`
  - `flow_set_zoom` (lines 26–28): replace the three direct assignments with `flow__view_set(f, ox, oy, z1);` (recompute `ox`/`oy` inline from the math, then pass all three)
  - `flow_fit_view` (lines 876–878): replace the three assignments with `flow__view_set(f, ..., ..., z);`
  - `flow_load` (line 312): replace the three assignments with `flow__view_set(f, ox, oy, zoom);`

RE-ENTRANCY ANALYSIS: A callback that calls `flow_pan`, `flow_set_zoom`, etc. will recursively invoke `flow__view_set` in the middle of a prior callback. No infinite loop occurs (each call compares against the CURRENT state, so a second call with identical args fires nothing). However, the callback firing a pan/zoom that triggers a SECOND callback may confuse the app author. Solution: optional depth guard (name `cb_vpview_depth`), patterned on `cb_suppress` (src/flow_model.h:216). If recursion is observed in tests, add `int cb_viewport_depth;` to struct flow, initialize to 0, increment/decrement around the callback, and skip firing if `cb_viewport_depth > 0`. Decision: test first, add guard ONLY if needed (no guard in base implementation).

NO UNDO/REDO: viewport is deliberately excluded from journaling (spec §11; comment at src/flow_model.h:130). The seam does not call `flow__rec_gate` or journal anything.

NO NEW STATE: the seam reads `f->view` (now-live) and the callback receives the new viewport. No extra struct fields for "prior" viewport are needed.

**Test plan.**
  - tests/test_viewport_events.c (new file): include flowtest.h, define a `vp_log` struct with counters + saved viewports
  - `flow_pan` fires: two nodes, no selection; pan right by 5, assert one fire with updated ox; pan by (0,0), assert no new fire
  - `flow_set_zoom` fires: zoom_in at (40,12), assert one fire with new zoom; zoom_in again, assert one more; zoom with same value (re-clamp to current), assert no new fire
  - `flow_fit_view` fires: three nodes, fit_view(2), assert one fire with updated zoom+pan
  - `flow_load` fires: save a file with viewport (ox=10, oy=20, zoom=1.5), load it, assert one fire with ox==10, oy==20, zoom==1.5
  - Callback mutates view (recursion test): callback invokes `flow_pan(f, 1, 1)`; assert no infinite loop, and the viewport after callback returns includes both the initial + recursive pan
  - No-op pan: `flow_pan(0, 0)` asserts zero fires (changed-only gate)
  - Multiple concurrent panners (e.g., arrow keys + autopan): each flow_pan call fires independently (no coalescing)
  - All tests: ASan/UBSan gate for leaks/UB
  - Snapshot: none required (callback is not render-layer)

**Acceptance.**
  - `on_viewport_change` callback is registered via `flow_set_callbacks`; fired after each actual viewport change (pan, zoom, fit, load)
  - No-op mutations (e.g., `flow_pan(0,0)`, same zoom) do NOT fire
  - Callback receives the final viewport state (all three fields)
  - Recursion via callback (callback pans/zooms): no infinite loop; the second pan/zoom fires its own callback independently
  - `flow_load` restore fires the callback (viewport is restored BEFORE callback, so the callback sees the loaded viewport)
  - `make test` passes; all viewport-event test cases + existing suite (no regression to pan/zoom/fit/load behavior)
  - `flow.h` regenerated by amalgamate (edits in flow_model.h, flow_view.h, flow_json.h only; no new module)

**Depends on.** nothing (independent; orthogonal to all four pillars)

**Conflicts with.**
  - `viewport-events` first; other packages (`edge-events`, `connect-lifecycle`) that append callback fields do so AFTER `on_viewport_change` in the struct and document their ordering in their respective Conflicts sections (see the cross-cutting `flow_callbacks` append rule)
  - `extent-clamps` (#3) — both packages touch every view-write site (`flow_pan`/`flow_set_zoom`/`flow_fit_view`). Pinned order is #3 → #5: extent-clamps lands its clamps at the existing sites first; THIS package then relocates them INSIDE the new `flow__view_set` seam (clamp before compare-and-fire, so a clamped-to-same-value write fires nothing). If landed in reverse, #3 adds its clamps inside the already-landed seam instead.

**Carry-overs fixed.**
  - Enables future packages (e.g., minimap-sync, remote-viewport-broadcast) to react to view changes centrally via a callback, closing a documented pattern gap (app had to poll `flow_view_get` or rely on manual UI updates)

---

### 6. Edge observer events (click, context, dblclick)  `[S]`  ·  id: `edge-events`

**Goal.** Add the three missing edge event callbacks mirroring the node observer trio: `on_edge_click`, `on_edge_context`, `on_edge_dblclick` fields in `flow_callbacks`. Edge clicks fire when the user releases a no-drag left-click on an edge's routed path; edge context fires on right-click; edge dblclick fires on two consecutive no-drag clicks on the same edge id. Implement by extending the existing `flow_handle_mouse` left-click and right-click branches to check edges, fire callbacks in the same event order as nodes (select → click → dblclick), and mirror the dblclick consumption pattern via a new `last_click_edge` field.

**User value.** An app author can register observers for edge interactions — e.g., to edit edge properties on click, show a context menu on right-click, or trigger a detailed view on double-click — without polling or building custom mouse handlers. The interaction model is consistent with the node observer API.

**Files touched.**
  - src/flow_model.h (flow_callbacks struct, struct flow's last_click_edge field, flow__graph_reset)
  - src/flow_input.h (flow_handle_mouse left/right-click branches)
  - tests/test_events.c (extend with edge observer tests)
  - Makefile (only if a dedicated test_edge_events is added; otherwise extend test_events)

**Entry points (existing functions to extend).**
  - `flow_callbacks` struct (`src/flow_model.h:181`) — add three new function pointers
  - `struct flow` (`src/flow_model.h:203`) — add `int last_click_edge` field (mirrors `last_click_node:215`)
  - `flow__graph_reset` (`src/flow_model.h:420`) — clear `last_click_edge` on graph reload (mirrors line 427's node pattern)
  - `flow_handle_mouse` left-click release branch (`src/flow_input.h:248-274`) — after node click/dblclick, check edge and fire observers
  - `flow_handle_mouse` right-click branch (`src/flow_input.h:82-86`) — after node context check, check edge and fire observer

**API additions.**
```c
/* ---- src/flow_model.h ---- */
/* Extend flow_callbacks struct with: */
void (*on_edge_context)(flow_t *f, int edge, flow_pt screen, void *user); /* right-click on an edge */
void (*on_edge_click)(flow_t *f, int edge, void *user);                   /* left-click (no drag) on an edge body */
void (*on_edge_dblclick)(flow_t *f, int edge, void *user);               /* left double-click on an edge (fires AFTER on_edge_click) */
```

**Design notes.**

CALLBACKS: add three new function pointers to `flow_callbacks` (alongside the existing node trio). Each is NULL-safe; callers check before invoking.

CLICK EVENT ORDER: edges follow the node pattern exactly. On a no-drag left-click on an edge body:
  1. `flow_select_edge(f, edge_id, 0)` — clears node + other edge selection (mutual exclusivity)
  2. `on_edge_click` fires (if registered)
  3. `on_edge_dblclick` fires if this edge's id matches `last_click_edge` (then `last_click_edge` is consumed)
  4. Otherwise `last_click_edge` is updated to this edge's id for the next click pair

DBLCLICK STATE: add `int last_click_edge` field to `struct flow`, initialized to -1 in `flow_new`. Follows the node semantics exactly:
  - Consumed (cleared to -1) after firing `on_edge_dblclick`
  - Cleared when any OTHER click fires (node, pane, or different edge)
  - Cleared on `flow_load` (in `flow__graph_reset`) to prevent stale id collision with reloaded edges
  - NOT journaled in undo (dblclick state is UI-only, not model state)

RIGHT-CLICK PRECEDENCE: currently `flow_handle_mouse` line 82-86 checks only nodes. Extend to check edges AFTER nodes:
  1. If `conn_active`, cancel connection (unchanged)
  2. Check `flow_hit_node(f, scr)` — if a node hit and `on_node_context` registered, fire and return
  3. Check `flow_hit_edge(f, scr, 1)` (reuse the same tolerance as left-click path) — if an edge hit and `on_edge_context` registered, fire and return

This preserves node precedence while adding edge support. If neither hit, the right-click is silent (no pane context callback exists).

LEFT-CLICK PATH: extend line 265-273 (the "a click, not a drag" block):
  - Current code: checks node (line 256-264), then edge (line 266-268), then pane (line 270-271)
  - Node branch already fires `on_node_click` + `on_node_dblclick`
  - Edge branch: after `flow_select_edge`, add `on_edge_click` and `on_edge_dblclick` logic mirroring the node branch exactly (dblclick pair tracking, consumption, last_click_edge update)
  - Pane branch: unchanged (clears selection, fires `on_pane_click`)
  - No change to hit ordering (handles → nodes → edges → pane remains intact)

NO struct flow change beyond `last_click_edge`, NO new module, NO undo/persist (dblclick state is ephemeral).

**Test plan.**
  - tests/test_events.c (extend): 
    - Add `edge_click_fires`, `edge_click_last`, `edge_dbl_fires`, `edge_dbl_last`, `edge_context_fires` to `ev_log`
    - Static callback stubs: `on_edge_click(f, edge, u)`, `on_edge_dblclick(f, edge, u)`, `on_edge_context(f, edge, screen, u)`
    - **On-click test**: Create two edges A, B at different paths; click A's routed cell → `on_edge_click` fires with edge id A, `on_edge_dblclick` does NOT fire. Click A again on same path → both `on_edge_click` and `on_edge_dblclick` fire, pair consumed. Click A a 3rd time → only `on_edge_click` (pair reset). Assert selection replaced (edges mutual-exclusive with nodes).
    - **Dblclick break test**: Click edge A, then click different edge B → `on_edge_dblclick` does NOT fire for B (pair broken). Click pane between two clicks on A → pair broken.
    - **Context test**: Right-click on an edge's routed cell → `on_edge_context` fires with edge id and screen coords. Right-click on a node takes precedence (fires `on_node_context`, NOT `on_edge_context`).
    - **State reset test**: Click an edge A (set `last_click_edge = A`), save/load graph, click the same id (reused in reload) — `on_edge_dblclick` does NOT fire (state was reset).
    - **NULL-callback safety**: all edge observer paths run with callbacks NULL (no crash).
  - Assert ASan/UBSan clean across all paths.

**Acceptance.**
  - `on_edge_click` fires after `flow_select_edge` on a no-drag left-click; `on_edge_dblclick` fires on the 2nd consecutive click on the same edge id (then pair is consumed).
  - `on_edge_context` fires on right-click over an edge's routed path; right-click over a node takes precedence (fires `on_node_context` instead).
  - `last_click_edge` is cleared on any non-pair click, on `flow_load`, and NOT persisted/journaled.
  - `make test` passes all new assertions; ASan/UBSan clean.
  - `flow.h` regenerated via amalgamate (edits in `flow_model.h`/`flow_input.h` only, no module added).

**Depends on.** nothing (purely additive to the observer callback seam).

**Conflicts with.**
  - `viewport-events` and `connect-lifecycle` also append fields to `flow_callbacks` — each must declare this mutual conflict and coordinate callback struct layout (see the cross-cutting `flow_callbacks` append rule).
  - `connect-lifecycle` (#7) — SEMANTIC interaction, pinned: clicking/right-clicking an edge while a connection is IN FLIGHT first ends the gesture (`on_connect_end` with `edge_id=-1` fires, per #7), THEN the edge event fires from the same press. Whichever package lands second adds the cross-event ordering test (in-flight connect + edge click → both events, in that order).

**Carry-overs fixed.**
  - Closes the asymmetry: nodes have three observer events (click, context, dblclick); edges now have the same trio, enabling consistent app-side event handling for both entity types.

---

### 7. Connect lifecycle events  `[M]`  ·  id: `connect-lifecycle`

**Goal.** Add onConnectStart / onConnectEnd analog callbacks to fire at the lifecycle boundaries of
an interactive connection gesture, so apps can track and react to connection attempts and cancellations
(drops on empty space, Esc abort, validation reject) as well as successful edges.

**User value.** An app author can subscribe to `on_connect_start` (fired when a source handle is grabbed) and
`on_connect_end` (fired at ALL exits: successful edge creation, drop/cancel, validation reject) to implement
connection UI feedback (e.g. flash a "connection rejected" toast, disable drop zones dynamically, count
connection attempts for analytics). The existing `on_connect` callback remains success-only, firing BETWEEN
start and end with the completed edge id.

**Files touched.**
  - src/flow_model.h (flow_callbacks struct, flow_begin_connection, flow_end_connection, flow_cancel_connection)
  - tests/test_connect.c (extend with lifecycle matrix)
  - tests/test_keys.c (extend ESC path: verify on_connect_end fires on Esc cancel)

**Entry points (existing functions to extend).**
  - `flow_callbacks` struct at `src/flow_model.h:181-191` — add two callback fields
  - `flow_begin_connection` (`src/flow_model.h:990`) — fire on_connect_start on successful source handle bind (0 return)
  - `flow_end_connection` (`src/flow_model.h:1007`) — fire on_connect_end AFTER flow__undo_end when the txn is settled
  - `flow_cancel_connection` (`src/flow_model.h:1025`) — fire on_connect_end with eid=-1
  - flow_feed / flow_run ESC path (`src/flow_run.h:45`) — already calls flow_cancel_connection on lone-ESC (no new call needed; the end callback fires via the cancel path)

**API additions.**
```c
/* ---- src/flow_model.h, flow_callbacks struct ---- */
typedef struct {
  void (*on_overlay)(flow_t *f, flow_surface *screen, void *user);
  void (*on_node_context)(flow_t *f, int node, flow_pt screen, void *user);
  void (*on_node_click)(flow_t *f, int node, void *user);
  void (*on_pane_click)(flow_t *f, flow_pt world, void *user);
  void (*on_connect_start)(flow_t *f, int source_node, const char *handle, void *user);
    /* fired when flow_begin_connection succeeds: source handle grabbed, preview in flight */
  void (*on_connect)(flow_t *f, int source, int target, void *user);
    /* a connection was created (after flow_add_edge) */
  void (*on_connect_end)(flow_t *f, int edge_id, int source, int target, void *user);
    /* fired at ALL connection gesture exits: edge_id=-1 if no edge (cancel/drop/reject),
       source=conn source node, target=candidate node under drop (-1 if none); called AFTER
       on_connect (if successful) and AFTER undo txn settlement so journal is consistent */
  void (*on_node_dblclick)(flow_t *f, int node, void *user);
  void (*on_selection_change)(flow_t *f, const int *ids, int n, void *user);
  void (*on_nodes_delete)(flow_t *f, const int *ids, int n, void *user);
  void *user;
} flow_callbacks;
```

**Design notes.**

CONNECTION LIFECYCLE (all paths):
1. User presses a source handle → `flow_begin_connection` called from flow_handle_mouse, returns 0 (success) or -1
   (not a valid source handle). If success, fire `on_connect_start(f, source_node, handle_id, user)` immediately.
2. User moves the cursor, hovering over candidate targets → `flow_update_connection` tracks hover state (no event).
3. User releases/completes (one of three):
   a) Hit a valid target → `flow_end_connection(f, target_node, handle)` called, adds edge via `flow_add_edge`,
      fires `on_connect(f, src, target, user)` inside the undo txn (line 1021), then fires
      `on_connect_end(f, edge_id, src, target, user)` AFTER `flow__undo_end` returns (new code).
   b) Drop on empty space or the source itself → `flow__resolve_connection_at` (flow_input.h:40) calls
      `flow_cancel_connection` via the else branch (line 47), which fires `on_connect_end(f, -1, src, -1, user)`.
   c) Hit an invalid target (self-edge, duplicate) → `flow_add_edge` returns -1, fires
      `on_connect_end(f, -1, src, target_candidate, user)` where target_candidate is the node under the cursor
      (or -1 if none).

ESC ABORT:
- User presses Esc during connection → `flow_cancel_connection` called (flow_run.h:45), fires
  `on_connect_end(f, -1, src, -1, user)` unconditionally (no target was under the cursor).

RECONNECT DRAGS (v1 NO EVENTS):
- `flow_reconnect_edge` is a distinct gesture (initiated by grabbing an existing edge endpoint) and does NOT
  fire on_connect_start/on_connect_end in this version. Xyflow separates reconnect events; flow's v1 treats
  reconnect as a pure model mutation (no lifecycle). A future increment may add on_reconnect_start/on_reconnect_end.

ORDERING GUARANTEE (successful connection):
- `on_connect_start` (press source handle) → `on_connect` (inside undo txn, edge added) →
  `on_connect_end` (after undo txn closes). This ensures the caller can rely on undo state being settled
  when on_connect_end fires.

CANDIDATE TARGET TRACKING:
- At drop time (on_connect_end), the `target` arg is the node id that was under the cursor (if any) at the
  moment the connection resolved. For Esc or empty-space drops, target=-1. For rejected adds (duplicate,
  self-edge), target is the attempted target node (-1 if the drop was on empty space). This mirrors xyflow's
  FinalConnectionState analog.

ISVALIDCONNECTION AS ENGINE-LEVEL PREDICATE:
- The package presumes isValidConnection logic already exists in flow_add_edge (self-check, duplicate-check at
  lines 545–550). This package fires events around those checks; no new validation layer is added. Validation
  is a MODEL concern (same checks as flow_reconnect_edge uses), not a VIEW concern.

**Test plan.**
  - tests/test_connect.c (extend):
    * SUCCESSFUL CONNECT: press source A's handle, move over target B, release. Assert
      on_connect_start(A, handle_id) fires on press; on_connect(A, B) fires on add; on_connect_end(edge_id, A, B)
      fires after. Verify edge_id is the added edge's id, source=A, target=B.
    * DROP ON EMPTY SPACE: press source A, move off all nodes, release. Assert on_connect_start fires on press;
      on_connect_end(eid=-1, A, target=-1) fires on release (no on_connect, no edge added).
    * DROP ON SOURCE SELF: press source A, release while still hovering A. Assert on_connect_start fires;
      on_connect_end(-1, A, -1) fires; no on_connect, no edge.
    * VALIDATION REJECT (duplicate): Add edge A→B. Press A again, move to B, release. Assert
      on_connect_start(A, handle) fires; on_connect_end(eid=-1, A, B) fires; on_connect does NOT fire (reject).
    * ESC CANCEL: press source A, press Esc. Assert on_connect_start fires on press; on_connect_end(-1, A, -1)
      fires on Esc; no on_connect.
    * RECONNECT DRAG ISOLATION: grab an existing edge's endpoint, drag to a new target, release. Assert
      NO on_connect_start, on_connect, or on_connect_end fires (reconnect is model-only in v1).
  - tests/test_keys.c (extend): verify Esc during connection fires on_connect_end(-1, ...) and clears state.

**Acceptance.**
  - on_connect_start fires once when a source handle is pressed (flow_begin_connection returns 0).
  - on_connect fires iff a new edge is successfully added (flow_add_edge returns eid != -1), inside the undo txn.
  - on_connect_end fires on ALL gesture exits (success, cancel, drop, reject) with eid=-1 for non-success cases,
    fired AFTER flow__undo_end so the txn is settled.
  - on_connect_end receives edge_id (new edge id or -1), source node id, and target node id (the candidate
    under the cursor at drop, or -1 if none / not applicable).
  - Esc during connection fires on_connect_end(-1, src, -1) without firing on_connect.
  - Reconnect drags fire no on_connect_* events (model-only, no lifecycle in v1).
  - `make test` passes; no regressions to test_connect on_connect assertions (on_connect still fires success-only,
    between on_connect_start and on_connect_end).

**Depends on.**
  - nothing (builds on the settled flow_begin_connection / flow_end_connection / flow_cancel_connection contract).

**Conflicts with.**
  - viewport-events (both extend flow_callbacks; coordinate field ordering in the typedef).
  - edge-events (both may add callback fields; coordinate ordering).
  - Any package that rewrites flow_end_connection or flow_cancel_connection — none in increment 4 does.

**Carry-overs fixed.**
  - Adds the xyflow onConnectStart / onConnectEnd pattern to flow, enabling reactive connection UI
    (rejection feedback, analytics) that the spec lists under user-facing interaction features.

---

### 8. Graph traversal: incomers, outgoers, connected edges  `[M]`  ·  id: `graph-traversal`

**Goal.** Add three new query APIs to efficiently retrieve the set of nodes and edges related to a given node — the incoming and outgoing connections analogous to xyflow's `getIncomers`, `getOutgoers`, and `getConnectedEdges`. These are model-level queries that enable traversal, layout, and batch-operation patterns without manually iterating all edges.

**User value.** An app author can call `flow_incomers(f, node_id, out, max)` to get the distinct set of nodes feeding INTO a node; `flow_outgoers(...)` to get nodes this node feeds TO; and `flow_connected_edges(...)` to get all edge IDs touching a node — all with automatic deduplication (multiple edges between the same pair yield one entry in incomers/outgoers, but each edge in connected_edges). This enables graph layout, dependency resolution, and rule-based workflows without exposing internal iteration logic.

**Files touched.**
  - src/flow_query.h (NEW)
  - tests/test_query.c (NEW)
  - Makefile (TESTS= append test_query)
  - tools/amalgamate.sh (modules= insert flow_query AFTER flow_undo, BEFORE flow_view)

**Entry points (existing functions referenced by implementation).**
  - `flow_get_node` (`src/flow_model.h:43`) — used to validate node ids and anchor traversal
  - `flow_edges` (`src/flow_model.h:48`) — query iterates all edges to collect incident connections
  - `flow_selected_nodes` (`src/flow_model.h:698`) — template for the fill-buffer+count idiom (insertion order, may exceed max)
  - `flow_edge_count` / direct f->nedges (`src/flow_model.h:46`) — bounds the edge iteration
  - `flow_node_count` / direct f->nnodes (`src/flow_model.h:45`) — present in iteration examples (not modified)

**API additions.**
```c
/* ---- src/flow_query.h ---- */
int flow_incomers(flow_t *f, int node, int *out, int max);
/* Return count of distinct source nodes with an edge TO node; write IDs (insertion order) into out[0..count-1].
   If count > max, only out[0..max-1] filled; caller may re-query with larger max to get all.
   Multi-edges between the same pair (different handle pairs) yield ONE entry in incomers.
   Missing node id (no such node) returns 0; all nodes are queried (model-level query, no filtering).
   Dedup: distinct (source node id) = one entry. */

int flow_outgoers(flow_t *f, int node, int *out, int max);
/* Return count of distinct target nodes reachable FROM node; write IDs (insertion order) into out[0..count-1].
   Same count-exceeds-max, dedup, and missing-id semantics as flow_incomers.
   Multi-edges from the same source to the same target yield ONE entry in outgoers. */

int flow_connected_edges(flow_t *f, int node, int *out, int max);
/* Return count of edge IDs incident to node (either source or target endpoint).
   Write edge IDs (insertion order) into out[0..count-1]; count may exceed max.
   Unlike incomers/outgoers: EACH edge is listed once (no dedup across handles).
   Edges touching the node on EITHER endpoint are included (source OR target == node).
   Missing node id returns 0; all edges are queried (model-level query, no filtering). */
```

**Design notes.**

The three queries live in a new module `src/flow_query.h`, positioned AFTER `flow_undo` and BEFORE `flow_view` in the amalgamate module list. This sequencing ensures the module can call `flow_model.h` functions (all earlier: flow_get_node, flow_edges, flow_edge_count, flow_node_count) without forward references.

DEDUP SEMANTICS: The key distinction between incomers/outgoers and connected_edges is deduplication. When multiple edges connect the same pair of nodes with different handle IDs (e.g., source handles "out1" and "out2" both targeting "in"), this is legal per `flow_add_edge`'s duplicate rule (dup = same source, target, AND handles). Incomers/outgoers report ONE entry per distinct NODE PAIR (the source or target id), collapsing multi-edges. Connected_edges lists EACH edge id once, so the same two nodes may contribute 2+ entries to the edge list.

MISSING NODE: If node does not exist (not in f->nodes list), all three return 0 (no edges, no neighbors). The out array is not modified on a missing-node query.

INSERTION ORDER: IDs are returned in the order they are encountered when iterating f->edges[0..nedges-1]. This allows snapshots and tests to be deterministic without imposing a sort.

Implementation iterates f->edges once, collecting source and target nodes (excluding the query node itself, and deduping by node id for incomers/outgoers). The count-exceeds-max pattern matches `flow_selected_nodes`: always return the true total count, write only min(count, max) to the output buffer.

**Test plan.**
  - tests/test_query.c (NEW): create a small graph (e.g., 1→2, 1→3, 2→3, 4→3 with mixed multi-edges); test `flow_incomers(f, 3, out, max)` returns {1,2,4} (or {4,2,1} depending on edge insertion order) with count=3; test with max < count to verify partial write and correct total return; test multi-edges (e.g., two edges 1→2 with different handles) yield one entry in incomers for node 2; test connected_edges for node 2 includes both edge ids; test missing node returns 0 and does not write out; test a node with no incoming edges returns 0.
  - Snapshot insertion-order behavior: add three edges (A→B, C→B, A→C); assert `flow_incomers(f, B, out, 10)` returns count=2 with out[0]=A, out[1]=C (in creation order, not id-sorted).
  - ASan: all queries allocate nothing (stack-only iteration), so no leaks expected; test exercises all branches (missing node, zero-count, partial fill).
  - One integration test: create a small DAG (nodes 1,2,3,4 with edges forming a fork-join), call all three queries on the same node, verify edges match the union of incomers/outgoers endpoints.

**Acceptance.**
  - `flow_incomers(f, node, out, max)` returns the count of distinct sources; out[] is filled insertion-order up to max; multi-edges between the same pair yield one entry.
  - `flow_outgoers(f, node, out, max)` returns the count of distinct targets; same out-filling and dedup behavior.
  - `flow_connected_edges(f, node, out, max)` returns the count of ALL incident edges (each edge once, no dedup); out[] is filled insertion-order up to max.
  - Missing node id returns 0 for all three; all nodes and edges are queried (model-level queries with no state filtering).
  - `tests/test_query.c` passes with full coverage (incomers, outgoers, connected_edges, multi-edges, missing node, overflow); `make test` green including ASan/UBSan.
  - `flow.h` regenerated by amalgamate.sh with flow_query inserted between flow_undo and flow_view.

**Depends on.** nothing (all dependencies are earlier: flow_model, flow_undo already landed).

**Conflicts with.**
  - `edge-events` (appends to flow_callbacks): no conflict; queries are read-only, callbacks are notifications.
  - `viewport-events` and `connect-lifecycle` (append to flow_callbacks, add struct flow fields): no conflict; queries do not modify engine state.
  - `intersect-query` (#10) EXTENDS this same new module (`src/flow_query.h`) — this package owns the module creation + the single `amalgamate.sh modules=` edit; #10 adds functions to the existing file only.

**Carry-overs fixed.**
  - Implements the spec's implicit §8 "graph traversal API" — enables efficient neighbor and edge-incident queries without exposing f->edges iteration or requiring manual dedup logic.
  - Forward note: once `hidden` (#11) lands, these queries still include hidden nodes/edges — MODEL-level per the cross-cutting layering rule (#11's section owns that contract).

---

### 9. isValidConnection predicate (engine-level validator) `[S]` · id: `valid-connection`

**Goal.** Add an engine-level isValidConnection predicate: a callback function that gates edge creation in both programmatic (`flow_add_edge`, `flow_reconnect_edge`) and interactive (`flow_end_connection`) paths. This is a deliberate divergence from xyflow's interactive-only gating — in flow, validation happens at the ADDING step, not at the UI level.

**User value.** An app author can register a validator (e.g., a reachability check to prevent cycles) that blocks invalid connections before they are added to the graph. The default (NULL) allows all connections, preserving the current behavior. The validator sees the full connection context (source node, target node, and the specific handle ids involved) and can make domain-specific decisions.

**Files touched.**
  - src/flow_model.h (struct flow additions, flow_add_edge/flow_reconnect_edge checks, validator-setter)
  - tests/test_model.c (extend or tests/test_edge.c)

**Entry points (existing functions to extend).**
  - `flow_add_edge` (`src/flow_model.h:544`) — insert validator check AFTER structural rejects (self-edge, missing nodes, duplicate edges) and BEFORE `flow__rec_add_edge` record
  - `flow_reconnect_edge` (`src/flow_model.h:740`) — insert validator check AFTER its own structural rejects (self-edge, duplicate) and BEFORE the record call at line 755
  - `flow_end_connection` (`src/flow_model.h:1007`) — NO EDIT needed: rejection of `flow_add_edge` is transparent to the caller; validator-rejected edge already leaves the graph unchanged and clears eid, triggering no on_connect callback

**API additions.**
```c
/* ---- src/flow_model.h ---- */
/* isValidConnection predicate: return 1 to allow, 0 to reject. Called at the ENGINE level
   for EVERY add/reconnect attempt (programmatic or interactive). NULL = allow all (default).
   Receives source/target node ids and the specific handle names involved; source_handle/target_handle
   are "" if none. */
typedef int (*flow_connection_validator)(flow_t *f, int source, int target,
                                         const char *source_handle, const char *target_handle,
                                         void *user);

void flow_set_connection_validator(flow_t *f, flow_connection_validator fn, void *user);
/* set the validator predicate (and its user context). NULL fn disables validation (allow all). */
```

**Design notes.**

The validator is a GATE, not an observer — it decides whether a mutation succeeds, not reacts to one. Therefore it lives in struct flow as two fields (`validator_fn` and `validator_user`), NOT in `flow_callbacks` (which are post-hoc observers). This separation is important: callbacks fire after a successful mutation (on_connect fires only on a successful add_edge); the validator runs before the mutation ever takes effect.

VALIDATOR CHECKS: `flow_add_edge` (line 544) already rejects self-edges (line 545), missing nodes (line 546), and duplicate edges (lines 548–550). The validator check inserts AFTER line 550 (the duplicates loop) and BEFORE the append (line 551). Similarly, `flow_reconnect_edge` (line 740) computes prospective endpoints and validates them against the same structural rules (lines 748–754); the validator check inserts AFTER line 754 (the duplicate check) and BEFORE the record at line 755. Order matters: structural rejects are fast and come first; the validator (which may call expensive user code like reachability queries) runs only on structurally valid proposals.

REJECTED BEHAVIOR: When a validator returns 0 (reject), `flow_add_edge` silently returns -1 and appends nothing. `flow_reconnect_edge` silently returns (no mutation). The undo journal records nothing (the record call is skipped). This is identical to the rejection contract of structural checks: silent, graph unchanged, no side effects.

INTERACTION WITH CONNECT-LIFECYCLE: When an interactive connection (`flow_end_connection`, line 1007) calls `flow_add_edge` (line 1020) and the edge is rejected (structural or validator), `eid` is -1. The subsequent check at line 1021 (`if (eid != -1 && f->cb.on_connect)`) prevents the callback from firing. This is correct: the validator-rejected connection never succeeds, so on_connect does not fire. No special wiring is needed; the composition works out of the box.

NULL VALIDATOR: The default (set at `flow_new`, initialized to NULL) allows all connections. Callers that never set a validator experience zero overhead (a single NULL check before the function call).

**Test plan.**
  - tests/test_model.c (extend) or new tests/test_edge.c:
    - Validator sees correct args: create two nodes A(0,0) B(1,1); set a validator that records (src, tgt, sh, th) on each call; `flow_add_edge(f, a, b, "out", "in")`; assert the validator was called with those exact args.
    - Reject blocks add_edge: set a validator that always returns 0; `flow_add_edge(f, a, b, "out", "in")`; assert return is -1, edge count unchanged, no record in journal.
    - Reject blocks reconnect_edge: create an edge e(a→b); set a validator that rejects (always return 0); `flow_reconnect_edge(f, eid, a, "out", 0)` (repoint source); assert edge.source is still b (unchanged), no record.
    - NULL validator = current behavior (regression): set validator to NULL; add/reconnect/end_connection work without validation (all succeed).
    - Validator that rejects self-loops never fires: set a validator that records call count; `flow_add_edge(f, a, a, ...)` (self-edge); assert validator.call_count == 0 (structural reject came first).
    - connect-lifecycle composition: create nodes A, B; set a validator that rejects; begin_connection(A), update_connection(screen), end_connection(B, "in"); assert no edge added, on_connect never called (both idempotent).
  - `make test` green; ASan/UBSan clean on test_model/test_edge — no leaks from validator invocations.

**Acceptance.**
  - A registered validator is called on every edge add/reconnect (programmatic or interactive).
  - Validator return 0 silently rejects (graph unchanged, no journal record, no callback fire).
  - Validator return 1 allows the edge to be added (existing behavior continues).
  - Structural rejects (self-edge, missing nodes, duplicates) bypass the validator (fast path first).
  - NULL validator (default) = no validation (all allowed, zero overhead).
  - `flow_set_connection_validator(f, NULL, NULL)` restores the default allow-all behavior.
  - `make test` passes including validator rejection cases; `flow.h` regenerated by amalgamate.

**Depends on.** nothing (purely an additive gate in the add/reconnect paths, independent of all inc-4 packages).

**Conflicts with.**
  - Any package that rewrites `flow_add_edge` or `flow_reconnect_edge` core logic — none in this set does; both are only extended at the validation point, not restructured.

**Carry-overs fixed.**
  - Enables the "prevent-cycles via graph-traversal reachability" flagship use case (graph-traversal package lands first, then a cycle-prevention validator is integrated in a follow-up pass; this package is the seam).

---

### 10. Intersection query (EXTENDS flow_query)  `[S]`  ·  id: `intersect-query`

**Goal.** Add `flow_intersecting_nodes` and `flow_node_intersections` to src/flow_query.h (a new module, depending on graph-traversal landing first). These mirror xyflow's `getIntersectingNodes` API: given a world-space rect, return all nodes whose absolute rectangles intersect it; a convenience variant returns nodes intersecting a given node's rect. Both use the same closed edge-touch semantics as flow_rect_intersects.

**User value.** Apps can query "what nodes overlap this region?" — a fundamental operation for spatial queries, bounding-box hit-tests, and intersection logic. Combined with graph-traversal's connected-component walk, this enables region-aware analysis (e.g., "all nodes in this zone and their transitive targets").

**Files touched.**
  - src/flow_query.h (new module, extended from graph-traversal base)
  - tests/test_query.c (new; or extend if graph-traversal test already created it)
  - Makefile (add test_query to TESTS if file is new)
  - tools/amalgamate.sh (modules= insert flow_query AFTER flow_undo, BEFORE flow_view — no edit here, coordinate with graph-traversal to add the module once)

**Entry points (existing functions to extend).**
  - `flow_rect_intersects` (src/flow_geom.h:10) — the closed-convention rect overlap predicate, already used by marquee selection (src/flow_model.h:717)
  - `flow_node_rect_abs` (src/flow_model.h:50, impl line 572) — compute absolute world bounding rect for a node
  - `flow_get_node` (src/flow_model.h:43, impl line 560) — resolve a node by id
  - `flow_is_ancestor` (src/flow_model.h:65, impl line 481) — ancestry test, cited for filtering in design notes (apps call this to exclude ancestor/child pairs if desired)

**API additions.**
```c
/* ---- src/flow_query.h ---- */
int flow_intersecting_nodes(flow_t *f, flow_rect world, int *out, int max);
/* Fill out[] (up to max elements) with ids of all nodes whose absolute rectangles
   intersect the world rect (closed convention: touching edges count). Returns total
   count (may exceed max). Insertion order. Model-level op: all nodes included.
   Parent/child rects may legitimately overlap — no filtering applied; apps use
   flow_is_ancestor to exclude ancestor pairs if desired. */

int flow_node_intersections(flow_t *f, int node, int *out, int max);
/* Convenience: fill out[] with ids of all nodes intersecting the given node's
   absolute rect, EXCLUDING the node itself. Closed convention. Returns count.
   E.g., to find all nodes touching node 42: count = flow_node_intersections(f, 42, ids, max). */
```

**Design notes.**

RECT SEMANTICS: Both functions use `flow_rect_intersects` from flow_geom.h (line 30), which implements a closed convention: touching edges count as overlap. This is consistent with marquee selection's PARTIAL mode (src/flow_model.h:717, `flow_rect_intersects(world, nr)`) and is documented in the geom header comment on line 10 ("touching edges count"). The query inherits this contract — cite the geom comment in the API doc.

MODEL-LEVEL OP: This is a model-level utility (like traversal, layout, serialize) returning all nodes regardless of any view-layer filtering. Unlike marquee selection (which will skip hidden nodes at the view level), this query returns all nodes. This design supports apps that need the full model state for backend analysis while marquee selection remains a view-layer concept. Document this distinction if a hidden/view-filtering feature is added later.

PARENT/CHILD OVERLAP: Group containers and their children often have overlapping world rects (a child nested inside its parent, or a parent group-bbox encompassing children). This query does NOT filter ancestor pairs — it returns them both. Apps that wish to exclude ancestors can walk the output and use `flow_is_ancestor(f, a, b)` to filter; cite this function in a design note so examples are discoverable.

INSERTION ORDER: Iterate f->nodes[i] in array order, building the output buffer. Same idiom as `flow_selected_nodes` (src/flow_model.h:698).

NO STRUCT CHANGE, NO CALLBACKS: A pure query function; no new struct fields or event hooks.

**Test plan.**
  - tests/test_query.c (new file or extend if graph-traversal already created one): define mkgraph() helper (3-4 nodes at known rects, e.g. A: (10,5, 5,3), B: (30,5, 5,3), C: (20,10, 4,4), D: (10,10, 6,3) forming overlaps and touch-edges).
  - **Overlap test**: query rect (15,6, 10,10) (overlaps A, B, C partially, misses D); assert returned ids count==3 and contain a, b, c in insertion order; assert d is NOT included.
  - **Touch-edges test**: query a rect (14,5, 1, 3) that touches A's right edge. A at (10,5,5,3) spans x∈[10,15), so the query rect at x∈[14,15) touches the right boundary; verify edge-touching counts (tight test to prove closed semantics).
  - **Exclude-self test**: call `flow_node_intersections(f, a, ...)` on node A (rect 10,5,5,3); assert it returns only nodes that overlap A's rect and excludes A itself; verify count==number of overlapping neighbors.
  - **Missing id**: call `flow_node_intersections(f, 9999, ...)` with a nonexistent id; assert returns 0 (no error, graceful).
  - **Parent/child pair**: add a child node to A; both will have overlapping rects; query a large rect encompassing both; verify both A and the child are returned (document this idiom).
  - **Empty graph**: call on an empty flow_t; assert returns 0.
  - **Buffer overflow**: call with max=1 on a multi-node overlap; assert returns the true count (≥2) but only fills out[0]; verify buffer-overflow free (no write past max).
  - **ASan/UBSan**: Manual test: `cc -std=c11 -fsanitize=address,undefined -fno-sanitize-recover=all tests/test_query.c flow.h && ./a.out` must pass (no leaks, undefined behavior, buffer overflows).

**Acceptance.**
  - `flow_intersecting_nodes(f, rect, out, max)` returns all nodes whose absolute rects intersect rect (closed semantics), insertion order, including all nodes, up to max elements; return value is true count (may exceed max).
  - `flow_node_intersections(f, node, out, max)` returns all nodes intersecting that node's rect except the node itself; handles missing ids gracefully (returns 0).
  - Closed-convention semantics (edge-touch counts) tested and documented; cites `flow_rect_intersects` comment.
  - Model-level design ratified: all nodes returned, not filtered by view state; documented distinction from view-layer filtering.
  - Parent/child overlap idiom documented and working.
  - `make test` passes including test_query; `flow.h` is regenerated by amalgamate (no module edit needed here, coordinate with graph-traversal to add the module once).
  - Manual ASan/UBSan run: no leaks, no undefined behavior.

**Depends on.**
  - `graph-traversal` (COORDINATION NOTE): The spec assumes flow_query.h will be part of a larger graph-traversal package or will land after it. Sequencing: graph-traversal lands first (if it exists), then intersect-query extends or reuses its module. The flow_query.h module insertion in amalgamate.sh (AFTER flow_undo, BEFORE flow_view) must be coordinated at commit time.

**Conflicts with.**
  - Any package that edits the node iteration loop in flow_model or changes `flow_rect_intersects` semantics — none in this set do.
  - `marquee-autopan` (ordering note only): once marquee lands its "skip hidden" logic at the view layer, this query's "include all nodes" contract at the model layer must be documented side-by-side to clarify which layer (view vs model) each API serves.

**Carry-overs fixed.**
  - None (new capability, no spec debt closed).

---

### 11. Node + edge hidden flags  `[M]`  ·  id: `hidden`

**Goal.** Add FLOW_HIDDEN=8u to the flags enum (src/flow_model.h:2) and implement view-level skip logic so hidden nodes and edges do not render, participate in hit-tests, appear in marquee selection, contribute to bounds, or show in the minimap, while remaining visible to model-level operations (traversal, layout, serialize). Hidden flags are NOT persisted in v1 (documented); selecting a node and then hiding it deselects it via sig-gated change notification.

**User value.** Applications can toggle node/edge visibility without removing them from the graph, enabling progressive disclosure UIs, focus modes, and detailed filtering. The hidden state survives the session but not the file, clarifying the semantic boundary between VIEW and MODEL semantics — a pattern the library encodes elsewhere (e.g. selection, zoom).

**Files touched.**
  - src/flow_model.h (flags enum, flow_set_node_hidden, flow_set_edge_hidden, flow_bounds filter, flow_select_in_rect filter)
  - src/flow_render.h (node render loop skip, edge render loop skip + cascade check, minimap loop skip, handle markers skip)
  - tests/test_render.c (new snapshot render_hidden)
  - tests/test_model.c (extend with hidden setter, bounds, selection)
  - Makefile (no change if tests extend existing files)

**Entry points (existing functions to extend).**
  - `FLOW_SELECTED/FLOW_DRAGGING/FLOW_HOVERED enum` (`src/flow_model.h:2`) — add FLOW_HIDDEN=8u on the same line; symmetric mutual-conflict note with FLOW_EXTENT_PARENT=16u
  - `flow_bounds` (`src/flow_model.h:573`) — filter out FLOW_HIDDEN nodes from the union loop
  - `flow_select_in_rect` (`src/flow_model.h:703`) — skip FLOW_HIDDEN nodes in the loop
  - `flow_render` node loop (`src/flow_render.h:200`) — skip FLOW_HIDDEN nodes; same gate as the one used by flow_hit_node
  - `flow_render` edge loop (`src/flow_render.h:172`) — skip edge if FLOW_HIDDEN set OR either endpoint node FLOW_HIDDEN
  - `flow__minimap` (`src/flow_render.h:61`) — skip FLOW_HIDDEN nodes in the dot loop
  - `flow__node_handles_visible` (`src/flow_model.h:956`) — extend gate: also skip if node is FLOW_HIDDEN (used by handle markers render + flow_hit_handle)
  - `flow_hit_node` (`src/flow_model.h:629`) — via footprint: same skip gate as render (mirrored footprint choke point)
  - `flow_hit_edge` (`src/flow_render.h:122`) — check FLOW_HIDDEN flag + cascade before routing

**API additions.**
```c
/* ---- src/flow_model.h ---- */
enum { FLOW_SELECTED = 1u, FLOW_DRAGGING = 2u, FLOW_HOVERED = 4u, FLOW_HIDDEN = 8u };
/* FLOW_HIDDEN (8u) and FLOW_EXTENT_PARENT (16u) both edit this same line — mutual conflict. */

void flow_set_node_hidden(flow_t *f, int id, int hidden);
/* Hide or show a node by id. A hidden node is skipped by render, hit-test, marquee, bounds, and minimap
   (VIEW-level semantics); traversal/layout/serialize still see it (MODEL-level). If the node is selected,
   hiding it clears FLOW_SELECTED and fires sig-gated on_selection_change. hidden flag does not survive
   save/load in v1. */

void flow_set_edge_hidden(flow_t *f, int id, int hidden);
/* Hide or show an edge by id. A hidden edge is skipped by render and hit-test; edges whose source or
   target node is hidden are also skipped (cascade). Hidden does not survive save/load in v1. */
```

**Design notes.**

GATES & CHOKE POINTS:
  The core gate is `(n->flags & FLOW_HIDDEN)` for nodes. Rather than sprinkle this throughout render and hit, we introduce ONE choke point: all node skip logic (render, hit, bounds, marquee) gates through a static helper `flow__node_visible(f, n)` returning `!(n->flags & FLOW_HIDDEN)`, mirroring `flow__node_footprint` precedent so hidden and render can never drift.
  
  For edges: BOTH the edge's own FLOW_HIDDEN flag AND cascade logic (endpoint hidden → edge hidden) check before rendering and hit-testing. The cascade is specified at the render and hit sites: `if ((e->flags & FLOW_HIDDEN) || (sn && sn->flags & FLOW_HIDDEN) || (tn && tn->flags & FLOW_HIDDEN)) skip`.

BOUNDS AND EMPTY CASE:
  `flow_bounds` filters hidden nodes from the union loop — if all nodes are hidden, bounds returns a zero-rect `{0,0,0,0}`. `flow_fit_view` already guards on `nnodes == 0` (line 868) and `b.w <= 0 || b.h <= 0` (line 870), so a zero-rect causes a no-op — no regression.

SELECTION SEMANTICS:
  When a node is hidden, if it was selected, `flow_set_node_hidden(f, id, 1)` clears the FLOW_SELECTED flag and fires `flow__notify_selection` with sig-gated change detection. A hidden-then-shown node does NOT reselect (hiding discards state); this is idempotent and consistent with marquee skip.

HANDLES:
  The choke point `flow__node_handles_visible` (line 956) gates handle marker render and `flow_hit_handle` lookup. Extend its condition: `(n->flags & (FLOW_HOVERED | FLOW_SELECTED)) || n->id == f->conn_node` AND NOT `(n->flags & FLOW_HIDDEN)`. Hidden nodes never show handle markers, even if selected.

UNDO/PERSIST:
  FLOW_HIDDEN flag writes are NOT journaled (no `flow__record_op` in `flow_set_node_hidden`/`flow_set_edge_hidden`; the setters are UI-transient, like zoom). On `flow_load`, all nodes/edges load with flags=0 (spec: only FLOW_SELECTED/DRAGGING/HOVERED are journaled or persisted, not FLOW_HIDDEN). Document this in the API comment: "hidden flag does not survive save/load in v1".

MINIMAP DOTS:
  The minimap loop (line 61) iterates all nodes; extend with `if (n->flags & FLOW_HIDDEN) continue;` before computing the dot position.

**Test plan.**
  - tests/test_render.c (extend): create a graph with A, B (visible), C (hidden). Render. SNAPSHOT("render_hidden", str) — verify C does NOT appear; A and B render normally. Create a second test: hide A (mid-graph); render; verify A gone but the edge from A still hidden (cascade). Also verify a hidden node's handles do NOT render even if selected (see below).
  - tests/test_model.c (extend): call `flow_set_node_hidden(f, nodeA, 1)`. Assert `flow_hit_node(f, screen_at_A)` returns -1 (not hittable). Assert `flow_bounds` shrinks to exclude A (or is zero if all hidden). Test selection-clear: select nodeA, hide it, assert `flow_selected_count == 0` and on_selection_change fired (verify via a dummy callback).
  - tests/test_marquee.c (extend): draw a marquee that overlaps both visible and hidden nodes, assert only visible nodes are selected.
  - tests/test_json.c (extend): save a graph with a hidden node. Load it. Assert the node is present but NOT hidden (hidden flag dropped). Verify bounds and render match the "all visible" state.
  - tests/test_edge.c (extend): hide a source node; render/hit the edge; verify edge is skipped (cascade). Show the source; render/hit again; edge appears.
  - Edge cascade hit-test: add edge A→B; hide A; call `flow_hit_edge(f, screen_on_edge)` assert -1. Unhide A, hit again, returns edge id.
  - `make test` passes; ASan/UBSan gate (no leaks, no UB in hidden-skip paths).

**Acceptance.**
  - A node with FLOW_HIDDEN set does not render, participate in hit-tests (flow_hit_node returns -1 at a hidden node's screen position), appear in marquee selection (flow_select_in_rect skips), or contribute to bounds (flow_bounds excludes it).
  - An edge with FLOW_HIDDEN set, or either endpoint node hidden, does not render or participate in hit-test; both skips are verified by test_render and test_edge snapshots.
  - Hidden nodes and edges do NOT appear in minimap dots.
  - Hiding a selected node clears its FLOW_SELECTED flag and fires on_selection_change (verify selection_clear_on_hide test).
  - `flow_set_node_hidden` and `flow_set_edge_hidden` are public API; undo does NOT journal them (v1 design).
  - Hidden flags do not survive save/load (verified by test_json load golden).
  - All existing snapshots remain LOCKED and unchanged (render_single, render_two_edge, minimap, etc. — no regression).
  - New snapshot render_hidden captures the hidden-node skip behavior.
  - `make test` passes including test_render render_hidden snapshot; `flow.h` regenerated (edits in flow_model.h, flow_render.h only; no new module).

**Depends on.** Nothing (independent of the four pillars; purely additive flag bit and view-level filtering).

**Conflicts with.**
  - `parent-extent` package: both edit src/flow_model.h:2 (the flags enum line), adding FLOW_HIDDEN=8u and FLOW_EXTENT_PARENT=16u respectively. Land `hidden` first; `parent-extent` rebases and conflicts only on line 2 (a documentation/review note, not a code hazard).
  - Any package that rewrites flow_render node/edge loops or flow_hit_node/edge — none in this set does.

**Carry-overs fixed.**
  - Closes the long-standing gap of "can I hide a node without deleting it?" — a common UX pattern in node editors (focus mode, layer toggles, etc.). v1 deliberate restriction (no undo, no persist) keeps implementation minimal and clarifies the boundary.