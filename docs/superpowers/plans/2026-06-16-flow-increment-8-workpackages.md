# flow — Increment-8 work packages: direct manipulation

Five work packages turning `flow` from a graph you *configure* into one you *handle*: per-element
interaction **gates** (draggable / selectable / deletable), **explicit node sizing** that survives
save/load, interactive **resize handles**, **drag-end separation** so dropped nodes never overlap,
and **multi-node drag-to-reparent**. The throughline is xyflow parity for direct manipulation —
every gate and gesture matches React Flow's `draggable`/`selectable`/`deletable` node config and its
drag/resize semantics, while the engine's determinism discipline (existing output never moves; new
behavior is OFF/permissive by default) is preserved byte-for-byte.

Each package was drafted against the LANDED code on `increment-7` by a verified recon fan-out
(flag/enum budget, every drag/select/delete site, persistence + journaling rails, test idiom)
grounded in source with file:line citations; **re-verify line numbers at implementation time** —
package 1 adds enum bits, three setters, and JSON emit/parse, so packages 2–5 rebase against it.

**Approved going in** (do not re-litigate; Open questions surfaces only genuine sub-forks): inc-8 is
the DIRECT-MANIPULATION layer = five packages in the order below; the three gate flags consume the
RESERVED bits 64/128/256 (the inc-7 flags ledger pinned 64u "RESERVED for the deferred per-element
interaction gates"); gates are **persisted** but transient view state (theme/lock/selection) remains
unpersisted; package 1 is **render-neutral** (no protected-node tint or lock glyph — that re-mints
every render golden; defer visuals); `FLOW_EXTENT_PARENT` is NOT migrated onto the on-disk rail in
inc-8 #1 (named follow-up in #2).

## Current state (what's already built, on `increment-7`)

Increments 1–7 delivered the interactive engine, the programmable platform, the keyboard/editing
layer, the run-loop/real-time layer, and the **feel layer** (theme tokens with light/dark presets,
live connection feedback, the widget hit-test seam + Controls bar + node/edge toolbars + a flow-level
lock). Relevant to inc-8:

- **The shared node/edge flag enum** (`src/flow_model.h:2`) stops at `FLOW_ANIMATED = 32u`. Bits
  **64u / 128u / 256u are free** and were explicitly RESERVED by inc-7 for these gates. `flow_node`
  carries an `unsigned flags;` (`:17`).
- **Drag** is armed at the threshold-cross in `src/flow_input.h:255-263` (single place `drag_node`
  goes from -1 to armed), single-drag moves at `:282-377`, multi-drag shifts selection ROOTS at
  `:296-308`, drop/reparent at `:443` (guarded `flow_selected_count(f)==1`). `flow_move_node`
  (`src/flow_model.h:726`) is the shared ABSOLUTE-in commit path for drag, center (`:1236`),
  keyboard nudge (`flow__nudge_selection`, `:769-782`), layout (`flow_layout.h:309`), and undo
  replay (`flow_undo.h:102`).
- **Selection** has NO single chokepoint: `flow_select_node` `|=` (`:1036`), `flow_toggle_node`
  `^=` (`:1041`), `flow_select_in_rect` writes `|=` directly (`:1074`). Interaction callers:
  modifier-select (`flow_input.h:221-222`), selectNodesOnDrag (`:263`), marquee (`:281`), plain
  click (`:415`), Enter focus-select (`flow_model.h:1509`). Programmatic caller: paste (`:1327`).
- **Delete** funnels through `flow_delete_selection` (`:1210`) from x (`:1493`), Delete (`:1492`),
  and cut (`:1340`, which copies *before* deleting). Its removal loop re-queries
  `flow_selected_node` every iteration (`:1225`); `flow_remove_node` (`:1162`) cascades children
  unconditionally (`:1185-1190`); `on_nodes_delete` builds its id list via `flow__sel_or_ancestor`
  BEFORE removal (`:1216-1222`).
- **Persistence** (`src/flow_json.h`): `flow_save` emits `id/type/x/y/parent` (+ optional data hook)
  per node (`:50-59`) — **no flags field**. The golden `tests/snapshots/json_basic.txt` pins those
  exact bytes. `flow_load` (`:330-358`) restores id/parent/data and re-measures w/h; load runs
  journal-suppressed (`:302`).
- **Undo** restores the whole snapshotted `flow_node` struct and clears ONLY
  `FLOW_SELECTED|FLOW_DRAGGING|FLOW_HOVERED` (`src/flow_undo.h:25` node, `:34` edge) — so
  `FLOW_EXTENT_PARENT` and any new behavior bit survive undo/redo verbatim (the durable-flag
  precedent). Setters for config flags (`flow_set_node_hidden`, `flow_set_edge_animated`,
  `:1434`) are NOT journaled.
- **Auto-measure**: `flow_measure_node` (~`:672`) recomputes w/h from the type's `measure()` at add,
  paste, and load; w/h are NOT persisted (re-derived; 4×3 fallback).

## Execution order (recommended)

Sequential spine, one commit per package. **#1 lands first** — it adds the three enum bits, the
setter API, and the JSON gate keys that #2 extends with the size keys (so the persistence schema
grows once per package, never re-touched). **#2 before #3** — explicit-size persistence is the crux
the resize handles write through. #4 and #5 are independent drag-end refinements.

| # | id | size | block |
|---|----|------|-------|
| 1 | `element-gates` | M | A — FLOW_NODRAG / FLOW_NOSELECT / FLOW_NODELETE + persistence |
| 2 | `explicit-size` | M | A — user-set w/h that skips auto-measure + persists |
| 3 | `node-resizer` | L | B — resize handles on the inc-7 widget seam (requires #2) |
| 4 | `drag-separation` | M | B — un-overlap on drop, shares the drag undo step |
| 5 | `multi-reparent` | M | B — lift the single-node reparent guard |

## Cross-cutting rules (pinned)

- **Determinism throughline:** existing output never moves; new behavior is OFF/permissive by
  default. The gate flags are NEGATIVE (`FLOW_NODRAG`/`NOSELECT`/`NODELETE`) so calloc-zero ==
  xyflow-permissive (draggable/selectable/deletable all true) — `flow_add_node` needs no seed line.
- **Render-neutral (package 1):** no tint, no lock glyph, no decoration for protected nodes. Any
  render change re-mints every `render_*` golden. Visuals (if ever) are a later package.
- **Persistence rail:** gate flags persist as named xyflow bools emitted ONLY when set; absent =>
  permissive on parse. `FLOW_EXTENT_PARENT` stays journal-durable-only on disk for now (follow-up
  in #2, which already touches node-field persistence). No raw `"flags":int` (couples bytes to enum
  order; could inject transient bits).
- **Flags ledger:** 64u/128u/256u consumed by #1. The new bits MUST stay OUT of the undo
  transient-clear masks (`flow_undo.h:25,:34`) — using the free range and not editing those masks
  keeps them undo-durable for free.
- **Discipline (unchanged):** TDD red-first; `flow.h` is GENERATED (edit `src/`, run
  `tools/amalgamate.sh`; `modules=` UNTOUCHED); whole-suite ASan/UBSan gate per package; `-Wall
  -Wextra` clean (except the carved-out `tests/test_model.c:8`); `make demos` builds; one commit per
  package, sole author, NO Co-Authored-By trailers; stage explicitly, never `git add .`, never stage
  `.claude/`.

---

## 1 — `element-gates` (M): per-element interaction gates

Three behavior flags ported from xyflow node config: `FLOW_NODRAG` (draggable=false),
`FLOW_NOSELECT` (selectable=false), `FLOW_NODELETE` (deletable=false). They gate **pointer/keyboard
interaction only** — programmatic moves, programmatic selection, and direct removal stay
unconditional, exactly as React Flow's flags gate the user gesture, not the imperative API.

### Flag bits and polarity

Append to the enum at `src/flow_model.h:2`:
`FLOW_NODRAG = 64u, FLOW_NOSELECT = 128u, FLOW_NODELETE = 256u`. These are DURABLE behavior bits
(persisted + undo-durable, unlike transient `SELECTED`/`DRAGGING`/`HOVERED`). Negative polarity is
deliberate: zero-init == permissive default, so call sites read in positive xyflow terms via the
setters while no `flow_new`/`flow_add_node` seed is needed.

### Public API (house style — three bool setters, NOT a bitset)

Declared beside `flow_set_node_hidden` (`src/flow_model.h:211-213`), implemented beside
`flow_set_edge_animated` (`:1438`) in the tiny `if (!n) return; if (on) clear; else set;` shape:

```c
void flow_set_node_draggable(flow_t *f, int id, int on);  /* on==0 sets FLOW_NODRAG  */
void flow_set_node_selectable(flow_t *f, int id, int on); /* on==0 sets FLOW_NOSELECT */
void flow_set_node_deletable(flow_t *f, int id, int on);  /* on==0 sets FLOW_NODELETE */
```

Positive verb over a negative flag is INTENTIONAL (documented in the header) — it keeps zero-init
permissive while the call site reads in parity vocabulary. Setters are NOT journaled (config
toggle, like `flow_set_edge_animated`); the bits ARE undo-durable and ARE persisted.

### NODRAG — exactly two input-layer veto sites

xyflow's `getDragItems` includes a node only when `(selected || isTarget) && !parentSelected &&
draggable`, and XYDrag's d3 `.filter()` rejects the gesture before `startDrag` for a non-draggable
target. Mapped to flow:

- **SITE 1 — arm-time suppression** (`src/flow_input.h:255-263`): inside the threshold-cross arm
  block, before `f->drag_node = f->down_node;`, if the grabbed node is `FLOW_NODRAG` skip arming
  drag_node, the grab-offset capture, `flow__undo_begin`, and selectNodesOnDrag. drag_node stays -1,
  so the gesture neither drags nor pans, and the RELEASE takes the click path. This covers
  single-drag move (`:282-377`), helper-snap (`:320-375`), and drop/reparent (`:443`)
  **transitively** (all gated on `drag_node!=-1`) — adding gates there would risk the byte-locked
  drag goldens. Grabbing a NODRAG node directly => the WHOLE drag is suppressed, siblings included.
- **SITE 2 — multi-drag mover filter** (`:296-308`): after the `FLOW_SELECTED` check, add
  `if (f->nodes[i].flags & FLOW_NODRAG) continue;`. A NODRAG sibling inside a multi-selection drag
  is held FIXED while draggable peers move.

`flow_move_node` and all its programmatic callers (center `:1236`, layout, undo replay) stay
UNTOUCHED — draggable=false stops user drag only. The handle-grab (`:183`) and reconnect (`:199`)
paths are orthogonal and NOT gated (a NODRAG node still connects).

### NOSELECT — internal predicate + five interaction sites, public API ungated

Add `static int flow__node_selectable(flow_node *n){ return n && !(n->flags & FLOW_NOSELECT); }`
near `flow__sel_or_ancestor` (`:1029`). Apply at: (1) modifier-select `flow_input.h:219-223` — gate
toggle/additive AND **do not set `down_modsel=1`** when not selectable (leaving it set wrongly
suppresses on_node_click and arms group drag); (2) selectNodesOnDrag `:263` — gate the select, leave
drag-arming; (3) plain click `:414-416` — gate the select INSIDE the `down_node!=-1` branch (still
fire on_node_click; the mutually-exclusive `:424 else` means staying in-branch can't reach
clear/pane_click, so current selection is preserved); (4) Enter focus-select `flow_model.h:1509`;
(5) marquee `flow_select_in_rect:1065` — `if (n->flags & FLOW_NOSELECT) continue;` beside the
existing visibility skip.

The public primitives `flow_select_node`/`flow_toggle_node`/`flow_clear_selection` (`:1030-1048`)
stay UNGATED — paste (`:1327`) and host apps rely on programmatic selection (xyflow parity:
selectable=false blocks pointer interaction, not `node.selected = true`). Hover is NOT gated (a
NOSELECT node's handles must stay reachable to connect).

### NODELETE — one pre-deselect pass fixes the infinite loop AND the callback

The hazard: `flow_delete_selection`'s loop re-queries `flow_selected_node` each iteration
(`:1225`); merely skipping a protected node leaves it `FLOW_SELECTED` and re-returned forever (hang).
The fix is a single index pass inserted **after sig capture (`:1215`), before the on_nodes_delete
id-build (`:1216`)**:

```c
for (int i = 0; i < f->nnodes; i++)
  if (f->nodes[i].flags & FLOW_NODELETE) f->nodes[i].flags &= ~FLOW_SELECTED;
```

Ordering is load-bearing and does double duty: (a) `flow_selected_node` now skips protected roots so
the `:1225` loop terminates with **zero change to the loop**; (b) a surviving protected root is no
longer selected so it is correctly EXCLUDED from the `on_nodes_delete` list (no lie). A protected
node WITH a selected ancestor is still cascade-removed and still satisfies `flow__sel_or_ancestor`
via the ancestor, so it stays in the list correctly.

`flow_remove_node` (`:1162`) and its child-cascade loop (`:1185-1190`) stay **unconditional**:
direct programmatic removal ignores deletable, and a protected DESCENDANT of a deleted parent is
still cascade-removed (xyflow: deletability gates the acted-on node, not cascade victims). Cut
(`:1340`) copies before this pass runs, so a protected node is copied-then-survives.

### Persistence (RESOLVED: persist) and journaling

**PERSIST** the three gates. Discriminator is cost-asymmetry, not the HIDDEN/ANIMATED "flags are
ephemeral" precedent: HIDDEN/ANIMATED reloading off is cosmetic and self-heals; `deletable=false`
silently dropped is a correctness/safety regression (Delete nukes a node the author protected).
xyflow treats these as durable node config. Emit named bools **only when the bit is set** (after the
parent field, `flow_json.h:55`): `,"draggable":false` / `,"selectable":false` / `,"deletable":false`
— a default node emits nothing new, so `json_basic.txt` stays byte-identical. Parse (after
`n->parent` at `:344`): only the literal `false` sets the bit; absent => permissive. No raw
`"flags":int` (couples bytes to enum order; could inject transient bits from a hand-edited file).
`FLOW_EXTENT_PARENT` is NOT migrated to disk in #1 — named follow-up in #2.

**JOURNALING:** the setter is NOT journaled (config toggle, matching `flow_set_edge_animated`). The
bits are journal-durable **for free** — undo restores the whole struct and clears only
`SELECTED|DRAGGING|HOVERED` (`flow_undo.h:25`), so undoing a delete brings a protected node back
still protected. The one MUST: keep 64/128/256 OUT of the clear masks at `:25` and `:34`.

### Tests

New file `tests/test_gates.c` (mirror `tests/test_helper.c`: `FLOW_IMPLEMENTATION` + `flow.h` +
`flowtest.h`, brace-scoped scenarios, `press_at`/`move_to`/`release_at`, ends with
`flowtest_report("test_gates")`); append `test_gates` to `Makefile` `TESTS`. Set gate bits via the
public setters (and assert via `flow_get_node(f,id)->flags`). `NODELETE_skips_and_no_hang` is written
FIRST so a hang (no report line) is unambiguous. Persistence cases extend the round-trip idiom in
`tests/test_json.c`; `json_basic_golden_untouched` proves emit-only-when-set keeps the golden
byte-identical. (Full named list in the test plan.)

### Open questions for sign-off

1. **Keyboard gates** (recommended: gate both for xyflow parity). `flow__nudge_selection`
   (`flow_model.h:769-782`) should skip `FLOW_NODRAG` nodes; Enter focus-select (`:1509`) should
   skip `FLOW_NOSELECT`. The nudge gate is the one input-layer edit that lives in `flow_model.h` and
   is byte-identical to the multi-drag filter — duplicate the `continue`, do not factor out a shared
   mover (the codebase deliberately did not). Flagged because it is the only cross-file gesture edit
   beyond the input layer.
2. **Moved-NODRAG-grab selection divergence** (accept). With arm-time suppression, grabbing a NODRAG
   node and MOVING leaves it unselected (release takes the click path with `moved==1`, a no-op);
   xyflow's native click still selects on grab. A no-move click still selects via `:415`. Acceptable.

---

## 2 — `explicit-size` (M): user-set node size that persists

SKETCH. Add `FLOW_EXPLICIT_SIZE` (next free bit, 512u) and guard the top of `flow_measure_node`
(~`src/flow_model.h:672-676`) with `if (n->flags & FLOW_EXPLICIT_SIZE) return;` so a user-sized node
skips auto-measure at every call site (add ~`:681`, paste, load `:357`) without touching each
caller. A `flow_set_node_size(f, id, w, h)` setter sets w/h + the bit (clamped ≥1). The crux is
persistence: w/h are NOT emitted today (re-derived on load, else 4×3), so #2 extends the JSON node
object with `,"w":N,"h":N` emitted **only when `FLOW_EXPLICIT_SIZE` is set** (golden-safe, same
emit-when-set discipline as #1's gate keys), restored on parse before/after measure. **This is the
natural home for the deferred `FLOW_EXTENT_PARENT`-on-disk follow-up** flagged in #1. Land before #3
— resize handles write through this path. Render-neutral. New cases in `test_json` (size round-trip)
and a small `test_gates`/`test_model` measure-skip assertion.

---

## 3 — `node-resizer` (L): interactive resize handles

SKETCH. Resize markers ride the **inc-7 widget hit-test seam** (the screen-space chrome branch at
the top of the left-press block in `src/flow_input.h`), drawn on the selected node's corners/edges
and **lock-gated** (no resize while `flow_locked`). Drag math is integer-cell (the same `flow_to_world`
delta idiom as node drag), writing through #2's `flow_set_node_size` (which sets `FLOW_EXPLICIT_SIZE`
so the new size persists and survives auto-measure). One resize gesture = **one undo step** (mirror
the drag `flow__undo_begin`/`flow__undo_end` bracket). Requires #2. New goldens at an
explicitly-resized state; widget tests extend `test_render` + the seam test in `test_input`.

---

## 4 — `drag-separation` (M): un-overlap on drop

SKETCH. On drag end (the drop handler around `src/flow_input.h:443-475`), run an AABB
un-overlap/separation sweep so a node dropped onto a peer is nudged clear. The sweep MUST run
**BEFORE** the `flow__undo_end` at ~`:472` so the separation shares the SAME undo step as the drag
(one undo reverts both the move and the un-overlap). Because drag is already redraw-armed
(`flow__drag_in_flight` ~`:486`), separation is an instant on-drop snap, not animated. Reuse
`flow_query`'s intersection sweep (`flow_rect_intersects`) rather than a new collision pass.
Render-neutral except the post-drop positions. Tests extend `test_input` with a drop-onto-overlap
scenario asserting separated positions + single undo step.

---

## 5 — `multi-reparent` (M): drag a multi-selection into a group

SKETCH. Lift the `flow_selected_count(f)==1` guard on the drop/reparent branch
(`src/flow_input.h:443`) and iterate selection ROOTS (the same root-walk the multi-drag mover uses),
reparenting each into the dropped-on group. `flow_set_parent` (`src/flow_model.h:798-815`) already
inherits the safety this needs: cycle-safe via `flow_is_ancestor` (~`:804`), undo-recorded (~`:810`),
and absolute-position-preserving (~`:814`), so a multi-reparent is N cycle-checked, abs-preserved
re-parents inside the existing drag undo step. NODRAG/NOSELECT compose naturally (a NODRAG root is
never the drag target; a NOSELECT node is never in the selection). Tests extend `test_input` with a
multi-select drag-into-group asserting all roots reparent and one undo reverts.