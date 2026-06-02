# flow — Increment 2 Work Packages (fresh-context handoff)

_2026-06-03 · generated from a multi-agent scoping pass against the current `flow.h`_

> **How to use this in a clean chat.** Open a fresh session in this repo and say which package you want
> (e.g. *"implement the Background package from the increment-2 handoff doc"*). The agent should first read,
> in order: `flow.h` (current amalgamated library — real signatures), `docs/superpowers/specs/2026-06-02-c-xyflow-flow-design.md`
> (full design), and this file. Then follow the repo's TDD loop: edit `src/*.h`, regenerate with
> `make flow.h` (never hand-edit `flow.h`), add a `tests/test_*.c` using `tests/flowtest.h` (`ASSERT`/`SNAPSHOT`),
> drive interactions with synthetic SGR via `flow_feed`, and gate on `make test` **plus** AddressSanitizer/UBSan
> before committing. Each package below is self-contained and ends in a working, tested build.
>
> **Running these with ultracode:** these packages mostly touch the same three core files
> (`src/flow_input.h`, `src/flow_render.h`, `src/flow_model.h`), so naive parallel execution will collide.
> Use the **Execution order** and **Parallelization** sections below — parallel-safe packages can run in
> separate git worktrees; the rest are serial. One package per fresh chat is the simplest safe default.

## Current state (what's already built, on `main`)

Single-header C library, amalgamated from `src/` via `tools/amalgamate.sh`. Done: pure core
(transform/bounds/hit-test), engine + node/edge CRUD with validation, node/edge **vtables** + default types,
**cell compositor + damage-diff** renderer (`flow_render`/`flow_present`), terminal raw-mode + **SGR-1006 mouse**,
**panning** (arrows/drag/scroll), **node drag**, **click-to-select** (+ drag threshold, selectNodesOnDrag),
**callbacks** (`on_overlay`/`on_node_context`/`on_pane_click`/`on_node_click`), **minimap**, and a flagship
`demos/topo.c` (custom `device` node type + right-click details panel). Tests: 9 `test_*` programs, all
ASan/UBSan-clean.

**Known carry-overs** (which package fixes each is noted inline): `flow_hit_node`/minimap assume `zoom==1`; edge anchors are fixed source-right/target-left; `flow_move_node` is top-level only; selection is single.

## Execution order (recommended)

1. background — S, lowest risk; edits flow_render only at the non-contended endpoint (grid drawn after flow_cellbuf_clear, before edges) + an additive struct flow.bg field. Lands the render-layering precedent cleanly and gets the easy win merged first.
2. keys-commands — MUST come early: it is the OWNER of the shared mutation/dispatch seam (flow_remove_node cascading children+edges, flow_remove_edge, flow_bind_key/flow_dispatch_key, flow_selected_edge, zoom==1 flow_fit_view, status-bar render pass appended after on_overlay). Several later packages consume these instead of redefining them.
3. multiselect — M; converts selection to a set (toggle/marquee/selected-last two-pass node loop, multi-node drag). Rewrites flow_handle_mouse press/motion/release and the flow_render node loop. Settle selection semantics before connections/edge-interaction layer on top.
4. connections — L; adds handle markers + connect-drag (handle hit-test at TOP of press) AND replaces the fixed source-right/target-left edge anchors with flow_handle_anchor. Must precede edge-interaction because edge-interaction's hit-test/endpoint math is written against the anchors connections changes.
5. edge-interaction — L; DEPENDS ON keys-commands (reuse flow_remove_node/flow_remove_edge/flow_bind_key/flow_selected_edge — do NOT redefine) and on connections (hit-order is handle->node->edge->pane; flow_hit_edge/flow_edge_endpoint_screen must recompute the now-handle-aware anchors). Adds edge select/delete/reconnect/labels only.
6. zoom — LAST; the integrator. Every other package deliberately keeps zoom==1 and defers generalization here. Zoom rebases onto settled code and makes flow_hit_node, flow_hit_handle, flow_hit_edge, marquee select, the minimap viewport rect, and flow_fit_view (supersedes keys-commands' zoom==1 stub) zoom-aware in one sweep, plus LOD render + Ctrl+wheel/+- keys.

**Dependency graph**

```
DECLARED vs REAL: all seven JSON packages claim depends_on:[]. That is wrong — their own api_additions/design_notes imply hard edges. Real DAG:

  background ──┐
               ├──> (independent, no consumers)
  serialize ───┘   (independent; only flow_json.h + trailing flow_node_type append + 1 amalgamate line)

  keys-commands ──> edge-interaction        (edge-interaction reuses flow_remove_node/edge, flow_bind_key, flow_selected_edge)
  keys-commands ··> zoom                     (zoom's real flow_fit_view SUPERSEDES keys-commands' zoom==1 stub — replace, not add)

  connections ──> edge-interaction           (edge hit-test/endpoint anchors depend on connections' flow_handle_anchor rewrite)

  multiselect ··> connections                (soft: both rewrite flow_handle_mouse press classification; sequence to avoid state-machine clashes)
  multiselect ··> edge-interaction           (soft: both extend flow_clear_selection / selection mutual-exclusivity)

  {background, keys-commands, multiselect, connections, edge-interaction} ──> zoom   (zoom is the integrator; generalizes hit_node/hit_handle/hit_edge/marquee/minimap that all others left at zoom==1)

Legend: ──> hard (link-collision or correctness); ··> soft (same-file state-machine / merge ordering). Roots with no in-edges: background, keys-commands, multiselect, serialize. Sink: zoom.
```

**File-conflict matrix**

File-touch counts across the seven files_touched lists, hottest first:

- src/flow_render.h — 6/7 (ALL except serialize). flow_render's compose sequence is the single most contended region. Distinct insert slots: background=grid (after clear, before edges, LOW conflict, endpoint); keys-commands=status bar (after on_overlay, LOW, endpoint); multiselect=node-loop two-pass + post-node marquee (MED); connections=handle pass + dashed preview pass + edge-anchor rewrite (HIGH); edge-interaction=edge-loop highlight + label draw (HIGH, same edge loop connections rewrote); zoom=node-loop lod + flow__minimap vp fix (HIGH, same node loop multiselect rewrote). => serialize the 3 HIGH editors (multiselect->connections->edge-interaction with zoom last); background+keys-commands are safe endpoint inserts.
- src/flow_model.h struct flow — 6/7 additive field-adds (background.bg; keys-commands key-table+statusbar; multiselect marquee_*; connections conn_*; edge-interaction reconnect_*; zoom zmin/zmax). Cheap if SERIAL (append to brace block). In parallel worktrees this is a same-region merge hazard. Plus shared-symbol collisions: flow_remove_node/flow_remove_edge/flow_bind_key/flow_selected_edge defined by BOTH keys-commands AND edge-interaction (link-time multiple-definition if both ship them); flow_fit_view defined by keys-commands AND zoom. flow_clear_selection extended by multiselect-adjacent + edge-interaction.
- src/flow_input.h flow_handle_mouse — 4/7 (multiselect, connections, edge-interaction, zoom). All rewrite the PRESS hit-order / event arms. Required press hit precedence after all land: handle (connections) -> edge endpoint (edge-interaction) -> node body -> pane; wheel branch gains Ctrl+zoom (zoom); press/motion gains shift/ctrl/marquee (multiselect). Must be serial; coordinate the single agreed hit-order.
- src/flow_run.h flow_feed — 3/7 (keys-commands adds flow_dispatch_key seam; edge-interaction routes x/DEL/CSI through it; zoom adds +/- keys). keys-commands establishes the dispatch table the other two register into — another reason keys-commands precedes.
- src/flow_types.h flow__default_render / default node type — 2/7 (connections attaches flow_default_handles; zoom adds LOD-collapsed branch).
- tools/amalgamate.sh — 2/7 (zoom inserts flow_view AFTER flow_model; serialize inserts flow_json AFTER flow_render) — DIFFERENT lines, trivially coexist; just don't clobber each other's edit.
- flow_node_type struct — 2/7 (serialize appends save/load hooks; connections leaves it alone but a future handles refactor could touch it). Trailing zero-init keeps existing initializers valid.
- demos/topo.c — 5/7 append showcase lines (connections, edge-interaction, zoom, serialize, background). Low-stakes textual merges; defer to a final integration pass.
- Makefile TESTS / tests/test_render.c / tests/snapshots — multiselect, connections, edge-interaction, zoom, background all add tests or extend test_render.c + drop new golden files. TESTS= list edits are append-only (easy); test_render.c is a shared file (serialize the blocks). New snapshot files never collide.

**Parallelization strategy**

Be honest: this set is mostly a SERIAL SPINE, not a parallel fan-out, because 6/7 packages edit flow_render.h and 6/7 edit struct flow.

SAFE TO RUN IN A PARALLEL WORKTREE (fresh-context chat) the whole time:
- serialize ONLY. It touches no flow_input.h / flow_render.h / flow_handle_mouse / flow_feed. Its surfaces are a brand-new src/flow_json.h, a trailing append to flow_node_type, one amalgamate.sh line (flow_json after flow_render), one Makefile TESTS entry, and demos/topo.c. Develop it in `git worktree add ../flow-serialize` against main from day one; rebase only the flow_node_type append + amalgamate line at merge (both append-only, ~2-line conflicts). This is the one package that genuinely overlaps zero contended code.

MUST BE SERIAL (single spine, in the recommended order):
- background, keys-commands, multiselect, connections, edge-interaction, zoom. Each rebases on the previous because they collide on flow_render.h compose order, struct flow body, flow_handle_mouse press hit-order, and (for the seam packages) flow_feed. Attempting these in parallel worktrees guarantees three-way merges in the two hottest files plus the symbol-collision landmines (flow_remove_node/flow_bind_key/flow_fit_view).

PRACTICAL CADENCE for fresh-context chats: each chat = one package = one feature branch off the previous package's merged tip. After each package: run `make` (regenerates flow.h via amalgamate — never hand-edit flow.h), `make test`, then the ASan/UBSan loop (cc -std=c11 -fsanitize=address,undefined -fno-sanitize-recover=all -g tests/test_X.c -o /tmp/st_X -lm && /tmp/st_X) before merging. Keep serialize's worktree rebasing on the spine tip periodically so its flow_node_type append stays current. Because every package regenerates flow.h, never resolve a flow.h conflict by hand — resolve src/ then re-run amalgamate.

**Gaps / overlaps flagged**

- DUPLICATE DEFINITIONS (link-time multiple-definition, not a merge nuisance): flow_remove_node, flow_remove_edge, flow_bind_key, and flow_selected_edge are each declared by BOTH keys-commands AND edge-interaction. Resolution: keys-commands OWNS them (its flow_remove_node is the richer one — cascades children AND incident edges); edge-interaction must DROP these and depend on keys-commands, keeping only edge-specific ops (flow_hit_edge, flow_select_edge, flow_reconnect_edge, flow_set_edge_label, flow_edge_endpoint_screen). Verified against the JSON: only these two packages define them — clean two-party dedupe.
- DUPLICATE flow_fit_view: defined by keys-commands (zoom==1 integer pan) and zoom (real, chooses zoom+pan). This is a planned supersede — zoom REPLACES the keys-commands impl, it must not ship a second definition. Flag so the executor edits the existing function rather than adding one.
- UNDECLARED DEPENDENCIES: every package reports depends_on:[] but the true edges are keys-commands->edge-interaction, connections->edge-interaction, and {all}->zoom. Do not take the all-[] graph at face value when scheduling.
- ANCHOR COUPLING: edge-interaction's flow_hit_edge/flow_edge_endpoint_screen are written against the fixed source-right/target-left anchors; connections rewrites those to flow_handle_anchor. Land connections first or edge-interaction is coded against anchors that connections invalidates.
- flow_handle_mouse PRESS hit-order is defined incrementally by multiselect, connections, and edge-interaction with no single owner. Agree the final precedence up front: handle (connections) -> edge endpoint (edge-interaction) -> node body -> pane; plus shift/ctrl/marquee arming (multiselect) and Ctrl+wheel zoom (zoom).
- demos/topo.c is appended to by 5 packages (connections, edge-interaction, zoom, serialize, background) — trivial textual merges; defer all showcase wiring to a single final integration pass to avoid repeated conflicts.
- amalgamate.sh edited by zoom (flow_view after flow_model) and serialize (flow_json after flow_render): different lines, but coordinate so neither overwrites the other's module-list insertion.
- NO IN-TIER COVERAGE GAP: the seven packages fully cover the editing/zoom/persistence/widget tier of the spec; nothing in-tier is missing. The only true absences are the deliberately deferred trio (see tier_notes).

**Deferred to increment 3** (after this set)

The deferred trio (undo/redo, subflows/groups, auto-layout) belongs AFTER this set, not within it:

- UNDO/REDO must wrap a SETTLED mutation surface. This set introduces nearly every mutator the editor will have: flow_remove_node/edge (keys-commands/edge-interaction), flow_reconnect_edge + flow_set_edge_label (edge-interaction), connect/flow_add_edge-via-handles (connections), multi-node group drag (multiselect), and viewport mutations zoom/pan/fit (zoom). Building undo now means re-instrumenting each of these as they land. Do it once, after, by snapshotting/journaling the now-complete mutator set.

- SUBFLOWS/GROUPS need real parent-relative coordinates, but this entire set leans on the documented zoom==1 + top-level-only assumptions: flow_move_node is top-level (pos==absolute), and multiselect's multi-drag applies a FLAT per-node world delta (correct only for top-level nodes). Groups would invalidate those assumptions and the hit-test/render footprint logic zoom just generalized. Land groups after zoom has unified the projection path so parent-relative + scale compose once.

- AUTO-LAYOUT is a pure model->positions pass that wants the graph to be fully EDITABLE (this set) and FRAMABLE (zoom's flow_fit_view to recentre after a relayout). Running it earlier means laying out a graph the user can't yet fully manipulate or frame, and it would need re-validation against every mutator added here. Best as a capstone consuming the finished model + view API.

---

## Packages

### 1. Background Grid Widget — flow_set_background (dots/lines/cross)  `[S]`  ·  id: `background`

**Goal.** Add a world-aligned background grid (dots/lines/cross variants with a configurable cell gap) drawn first in flow_render, under edges, so it scrolls with pan. Off by default; opt-in via flow_set_background.

**User value.** The user can call flow_set_background(f, FLOW_BG_DOTS, 4) (or LINES/CROSS) to get a light reference grid behind the graph that pans with the canvas, giving spatial orientation like xyflow's &lt;Background /&gt; component. Pass FLOW_BG_NONE to turn it off.

**Files touched.**
  - src/flow_model.h
  - src/flow_render.h
  - tests/test_render.c
  - tests/snapshots/bg_dots_pan0.txt
  - tests/snapshots/bg_dots_pan1.txt
  - demos/topo.c

**Entry points (existing functions to extend).**
  - flow_render (src/flow_render.h) — insert flow__background call after flow_cellbuf_clear, before the edge loop
  - flow__minimap (src/flow_render.h) — structural template for the new static flow__background helper
  - flow_set_minimap / struct flow.minimap (src/flow_model.h) — pattern to mirror for flow_set_background / struct flow.bg
  - flow_to_world (src/flow_model.h) — used per-cell to world-align the grid
  - flow_cellbuf_put (src/flow_cell.h) — used to write grid glyphs at full-buffer coordinates

**API additions.**
```c
typedef enum { FLOW_BG_NONE, FLOW_BG_DOTS, FLOW_BG_LINES, FLOW_BG_CROSS } flow_bg_variant;
void flow_set_background(flow_t *f, flow_bg_variant variant, int gap);
```

**Design notes.**

Mirror the existing minimap widget pattern (do NOT create flow_widget.h). In src/flow_model.h: declare flow_bg_variant + flow_set_background next to the minimap declarations; add state `struct { flow_bg_variant variant; int gap; } bg;` to `struct flow` (sits beside the existing `struct { int enabled,w,h; flow_corner corner; } minimap;`). Because flow_new uses calloc, FLOW_BG_NONE(=0) and gap=0 are the zero-init default, so the grid is off until set — matching current behavior. Implement the setter in flow_model.h's FLOW_IMPLEMENTATION block: `f->bg.variant = variant; f->bg.gap = gap < 1 ? 1 : gap;` — the gap>=1 clamp prevents the modulo-by-zero UB that the UBSan acceptance gate would catch.

Drawing lives in src/flow_render.h as a static helper `flow__background(flow_t *f, flow_cellbuf *cb)`, called inside flow_render immediately after `flow_cellbuf_clear(&cb, FLOW_FG, FLOW_BG)` and BEFORE the edge loop, gated on `f->bg.variant != FLOW_BG_NONE`. This realizes the spec's compose order (§6: background -> edges -> nodes -> minimap -> overlay). The helper writes directly via flow_cellbuf_put (full-buffer coords), like flow__minimap does.

World alignment: iterate every screen cell (sx in 0..cb->w-1, sy in 0..cb->h-1), map to world with `flow_pt w = flow_to_world(f, (flow_pt){sx,sy})`, and test grid membership against world coords so the pattern is anchored to the world origin and scrolls under pan. Membership: `int onx = (w.x % gap)==0, ony = (w.y % gap)==0;`. The `==0` test is sign-safe in C (for any k, k*gap % gap == 0 regardless of sign), so no negative-modulo bug. Per variant: DOTS -> put U+00B7 `·` only where (onx && ony); CROSS -> put U+002B `+` only where (onx && ony); LINES -> put U+2502 `│` where onx (vertical rules), U+2500 `─` where ony (horizontal rules), and U+253C `┼` at intersections where (onx && ony). All codepoints verified.

CRITICAL color choice (grounded in flow_diff_emit, flow.h lines 144-150): the diff emitter only serializes BOLD(;1) and REVERSE(;7) attrs — it NEVER emits FLOW_DIM or FLOW_UNDERLINE. So drawing the grid with the FLOW_DIM attr would render light in headless snapshots (which read only .ch) yet full-intensity on a real terminal — a false green. Instead make the grid "light" via a dim 256-color FOREGROUND, which DOES go through the emitted `;38;5;%um` path. Use a defined constant `#define FLOW_BG_GRID_FG 8` (256-color bright-black/grey) for the glyph fg, with bg = FLOW_BG and attr = 0. This is fully self-contained in the two files this package owns and requires no edit to flow_cell.h's diff emitter.

Grid is drawn under edges/nodes, so node boxes and edge routes overwrite grid cells — correct layering, no special clipping needed. No zoom handling: routing through flow_to_world means the grid is correct at zoom==1 (the only supported zoom today) without baking in any anti-zoom assumption; zoom-aware spacing/LOD is explicitly out of scope for this warm-up package and is NOT claimed as a carry-over fix.

**Test plan.**
  - Extend tests/test_render.c (NOT a new file — the Makefile TESTS= list and the ASan/UBSan loop are fixed; the minimap precedent puts widget tests inside test_render.c so they actually run). Add a block after the minimap block.
  - Default-off: flow_new + register_defaults, no flow_set_background; flow_render into a fresh buffer; ASSERT that no grid glyph (U+00B7/U+002B/U+2502/U+2500/U+253C) appears beyond what nodes/edges draw (use an empty graph: zero nodes so the buffer is all spaces) — proves grid is off by default.
  - DOTS membership + color: empty graph (no nodes/edges) on a small buffer (e.g. 12x6), flow_set_background(f, FLOW_BG_DOTS, 3), pan to 0; flow_render; ASSERT_INT the cell at screen (0,0) .ch == 0x00B7 (world origin is a grid point) and ASSERT_INT its .fg == FLOW_BG_GRID_FG (locks the light-color choice that snapshots cannot see); ASSERT_INT a non-grid cell e.g. (1,0) .ch == ' '.
  - DOTS scroll snapshot: empty graph 12x6, FLOW_BG_DOTS gap 3, render at pan 0 -> SNAPSHOT("bg_dots_pan0", str); then flow_pan(f,1,0), render -> SNAPSHOT("bg_dots_pan1", str); eyeball that the dot columns shift by one cell (proves world-anchored scrolling). Use the existing cells_to_string helper already in test_render.c.
  - LINES variant: empty graph, FLOW_BG_LINES gap 3; render; ASSERT the intersection cell at a known grid point (e.g. screen (3,3) with gap 3, pan 0) .ch == 0x253C, a vertical-only cell (3,1) .ch == 0x2502, a horizontal-only cell (1,3) .ch == 0x2500.
  - CROSS variant: empty graph, FLOW_BG_CROSS gap 3; render; ASSERT grid-point cell (0,0) .ch == 0x002B and an off-grid cell .ch == ' '.
  - gap clamp / UBSan safety: flow_set_background(f, FLOW_BG_DOTS, 0); flow_render must not divide by zero (gap clamped to 1); ASSERT every world cell becomes a grid point (cell (0,0) and (1,0) both == 0x00B7 since gap 1).
  - Layering: add one default node at world (0,0) over a DOTS grid; render; ASSERT the node's top-left corner cell .ch == 0x250C (node box overwrites the grid dot underneath) — confirms background is under nodes.

**Acceptance.**
  - `make test` passes; test_render reports all asserts passed and creates bg_dots_pan0/bg_dots_pan1 snapshots on first run (verify the two snapshots show a clean dot grid that shifts one column between pan0 and pan1).
  - ASan/UBSan clean: the increment-1 sanitizer loop (cc -fsanitize=address,undefined -fno-sanitize-recover=all on tests/test_render.c, run binary) reports no errors — specifically no modulo/divide-by-zero from gap, since the setter clamps gap>=1.
  - Grid is off by default (FLOW_BG_NONE/calloc zero-init); existing render_single, render_two_edge, and minimap snapshots are unchanged (no regression) because no demo/test sets a background unless explicitly testing it.
  - Grid glyph foreground is a dim 256-color value (FLOW_BG_GRID_FG) that survives flow_diff_emit's actually-emitted SGR path, verified by an ASSERT_INT on a grid cell's .fg (snapshots alone cannot prove this).
  - amalgamation regenerated: `make flow.h` runs tools/amalgamate.sh; edits are only in src/, never hand-editing flow.h.
  - demos/topo.c builds with `make demos` after adding one flow_set_background(f, FLOW_BG_DOTS, 4) line to showcase the widget.

**Depends on.** nothing  
**Conflicts with.**
  - Any package that edits src/flow_render.h's flow_render compose sequence (e.g. group-container layer, marquee/connection-preview layer, selected-last draw order) — they insert into the same z-order region.
  - Any package adding fields to `struct flow` in src/flow_model.h (merge hazard on the struct body, e.g. zoom state, multi-select state).

**Carry-overs fixed.**
  - (none)

---

### 2. Keyboard command dispatch: flow_bind_key + built-ins (delete/add/fit/help-bar) in flow_feed  `[M]`  ·  id: `keys-commands`

**Goal.** Add a reusable key-dispatch layer inside flow_feed (a flow_bind_key registry plus a built-in command set) so the editor responds to single-byte and escape-sequence keys: delete selected node ('x'/Delete) cascading its edges, delete selected edge, add-node at viewport center ('n'), fit-view ('f'), toggle a one-line built-in status/help bar ('?'), and keep 'q' quit. Also lands the flow_remove_node/flow_remove_edge mutators the delete commands require.

**User value.** The user can edit the graph from the keyboard without the mouse: press 'x' or Delete to remove the selected node (its edges disappear with it), 'n' to drop a new node in the middle of the view, 'f' to recentre the graph, '?' to toggle a help/status bar listing the keys, and 'q' to quit — and app authors get a flow_bind_key hook to add their own keys that other packages (edge-delete, zoom) reuse.

**Files touched.**
  - src/flow_model.h
  - src/flow_run.h
  - tests/test_keys.c
  - tests/test_model.c
  - Makefile
  - demos/hello_flow.c
  - flow.h

**Entry points (existing functions to extend).**
  - flow_feed (src/flow_run.h) — add flow_dispatch_key call between SGR-mouse parse and arrow-key pan
  - flow_run (src/flow_run.h) — keep its 'q' scan; main loop unchanged
  - flow_render (src/flow_render.h) — append built-in status-bar pass after f->cb.on_overlay
  - struct flow (src/flow_model.h) — add key-binding array + statusbar flag
  - flow_free (src/flow_model.h) — already frees edge labels; ensure remove paths free labels too
  - flow_selected_node / flow_clear_selection (src/flow_model.h) — model for flow_selected_edge
  - flow_bounds + flow_to_world (src/flow_model.h) — used by flow_fit_view and flow_add_node_center

**API additions.**
```c
void flow_remove_node(flow_t *f, int id);   /* cascades incident edges AND child nodes (recursively), frees edge labels */
void flow_remove_edge(flow_t *f, int id);    /* frees label; no-op if id absent */
int  flow_selected_edge(flow_t *f);          /* first edge with FLOW_SELECTED flag, or -1 */
typedef void (*flow_key_fn)(flow_t *f, void *user);
void flow_bind_key(flow_t *f, const char *seq, flow_key_fn fn, void *user);  /* register/override a key; matched before built-ins, longest seq first */
void flow_delete_selection(flow_t *f);       /* built-in: remove selected node(s) then selected edge */
int  flow_add_node_center(flow_t *f, const char *type, void *data);  /* add at world point under viewport center; returns id */
void flow_fit_view(flow_t *f, int margin);   /* zoom==1: pan so flow_bounds is centred with margin; no-op when empty */
void flow_set_statusbar(flow_t *f, int enabled);  /* toggle the built-in bottom help/status line */
int  flow_dispatch_key(flow_t *f, const char *seq, int n);  /* run a binding/built-in for one decoded key seq; returns bytes consumed (>0) or 0 if unhandled */
```

**Design notes.**

Maps xyflow's keyboard shortcuts (§8 table: Delete/x, n, f) and flow_bind_key (spec line 363) onto the terminal. Dispatch lives in flow_feed so synthetic-input tests drive it directly (the existing test_input/test_select/test_mouse pattern), not only via flow_run.

Dispatch order inside flow_feed's loop, per byte position i: (1) existing SGR-mouse parse (ESC [ <); (2) NEW flow_dispatch_key(f, b+i, n-i) which first scans the registry for the longest registered seq that is a prefix of the remaining bytes and runs its fn, else matches built-ins; if it consumes >0 bytes, advance i and continue; (3) existing arrow-key pan (ESC [ A/B/C/D) — keep, but note arrows can alternatively be exposed as bindable seqs "\x1b[A" etc.; for this package leave arrow-pan where it is and have flow_dispatch_key NOT claim bare arrows so behaviour is unchanged; (4) final i++. Built-in seqs: "x" and "\x1b[3~" (Delete/DECFNK) -> flow_delete_selection; "n" -> add-node-center with type "default" + a static label so it renders without app data; "f" -> flow_fit_view(f, 2); "?" -> toggle statusbar. 'q' quit stays in flow_run's own scan (unchanged) so library code never force-exits; flow_dispatch_key does NOT bind 'q'. A registered binding for a seq overrides the built-in for that seq (registry checked first), which is how the future zoom package binds '+'/'-' and an edge-delete package can rebind Delete.

flow_remove_node: find index; recursively remove child nodes (n->parent==id) first; remove incident edges (source==id||target==id), freeing each label; free the node's slot by shifting the array down (preserve insertion order, which flow_render and flow_hit_node depend on); decrement nnodes. flow_remove_edge: find by id, free label, shift down, decrement nedges. Both must keep flow_get_node/flow_nodes pointer-invalidation contract (callers re-fetch). flow_delete_selection iterates a snapshot of selected node ids (removal mutates the array) then removes flow_selected_edge if any.

flow_add_node_center: world center = flow_to_world(f, {cols/2, rows/2}); call flow_add_node then flow_move_node to centre the node's measured box on that point (offset by w/2,h/2). Uses f->cols/f->rows already stored on the engine.

flow_fit_view (zoom==1 only — honest carry-over, documented like flow_hit_node): b=flow_bounds(f); if empty return; set view.ox so b is centred: ox = (cols - b.w)/2 - b.x + margin-aware clamp, oy likewise; margin shrinks usable area. Pure integer pan; a later zoom package generalises it to also choose zoom (spec line 305 getViewportForBounds).

Status bar: a builtin drawn LAST in flow_render, after f->cb.on_overlay, so it never fights the app's overlay (topo.c draws panels via on_overlay). One reverse-video row at y=rows-1: " n:add  x:del  f:fit  ?:help  q:quit ". Drawn via flow_text/FLOW_REVERSE into a full-screen flow_surface, same idiom as flow__minimap. Engine struct gains: a small key-binding array {char seq[8]; flow_key_fn fn; void *user;} + count, and int statusbar. flow_free leaves bindings (caller-owned fns; engine owns only the small array, freed with the struct).

**Test plan.**
  - tests/test_keys.c (new, added to Makefile TESTS): build/link red first (undefined flow_bind_key/flow_remove_node/etc.), then green — following the per-file FLOW_IMPLEMENTATION + flowtest.h convention.
  - Delete via key: add 2 nodes + edge between them; flow_select_node(a); flow_feed(f, "x", 1); ASSERT flow_get_node(f,a)==NULL, ASSERT_INT(flow_node_count,1), ASSERT_INT(flow_edge_count,0) (cascade). Repeat with Delete seq flow_feed(f, "\x1b[3~", 4) on the other node.
  - flow_remove_node cascade unit (in test_model.c or test_keys.c): node a with two incident edges and a child node b (set b->parent=a); flow_remove_node(f,a); ASSERT child b also gone, both edges gone, surviving unrelated node/edge intact, and remaining ids unchanged.
  - Edge delete: select an edge (set FLOW_SELECTED on it), flow_selected_edge returns its id; flow_feed(f,"x",1) with no node selected removes only that edge; ASSERT_INT(flow_edge_count, before-1).
  - Add-node center: empty 80x24 flow; flow_feed(f,"n",1); ASSERT_INT(flow_node_count,1); n=flow_get_node of new id; ASSERT its node_rect_abs center ~= flow_to_world({40,12}) within 1 cell.
  - Fit view: nodes far off-screen (e.g. pos {200,100}); flow_feed(f,"f",1); ASSERT flow_bounds projected to screen lies fully within [0,cols)x[0,rows) and is centred (left margin ~= right margin within 1); empty flow: 'f' is a no-op (no crash, view unchanged).
  - flow_bind_key override + reuse: static int hits=0; bind 'g' -> fn that increments hits; flow_feed(f,"g",1); ASSERT_INT(hits,1). Bind 'x' to a custom fn and confirm it runs INSTEAD of built-in delete (selected node survives) — proves registry-before-builtin precedence other packages depend on. Bind a multi-byte seq "\x1b[1;5A" (Ctrl-Up) and confirm longest-prefix match consumes all bytes.
  - Dispatch isolation: flow_feed with arrow seqs (\x1b[A etc.) still pans and is NOT swallowed by flow_dispatch_key (re-run the existing test_input assertions inline); 'q' is NOT consumed by flow_dispatch_key (flow_dispatch_key(f,"q",1)==0).
  - Status bar snapshot: 30x6 buffer, statusbar enabled, flow_render, cells_to_string (same helper as test_render/test_cell), SNAPSHOT("render_statusbar", s) — golden bottom row shows the key hints; assert it does not overwrite a node drawn on row 0.
  - Status bar + app overlay coexistence: set an on_overlay that writes a marker char at (0,0); enable statusbar; render; ASSERT marker present AND status row present (builtin drawn after, on a different row).
  - make test passes all tests; rebuild every test with -fsanitize=address,undefined -g and run: no leaks (flow_remove_node/edge free labels; flow_free clean) and no UB. Specifically free-after-remove: remove all nodes then flow_free — ASan clean.

**Acceptance.**
  - make test is green including the new tests/test_keys.c and the extended tests/test_model.c.
  - Pressing 'x' or Delete on a selected node removes it and all incident edges and child nodes; pressing 'x' with only an edge selected removes that edge.
  - 'n' adds a default node centred in the current viewport; 'f' recentres flow_bounds on screen with margin; '?' toggles a one-line built-in status bar; 'q' still quits via flow_run.
  - flow_bind_key registers a key whose fn runs from flow_feed, and a binding overrides the matching built-in (verified by test).
  - All tests, recompiled with -fsanitize=address,undefined, run clean: no leaks, no UB (label frees on remove verified).
  - tools/amalgamate.sh regenerates flow.h; no hand edits to flow.h; demos still build (make demos).

**Depends on.** nothing  
**Conflicts with.**
  - Edge delete / edge selection package (also touches selection + flow_remove_edge wiring and may add edge hit-test in flow_input.h)
  - Zoom + fit-view-with-zoom package (will generalise flow_fit_view and flow_hit_node beyond zoom==1; both edit src/flow_model.h)
  - Multi-select / marquee package (touches flow_delete_selection's node-id snapshot loop and selection state)
  - Any package editing flow_render.h draw order (status bar adds a final draw pass) or flow_run.h (q-quit / main loop)

**Carry-overs fixed.**
  - Adds the missing flow_remove_node (cascades incident edges AND children, per spec §4 mutators) and flow_remove_edge mutators that the model lacked.
  - Establishes the single key-dispatch seam (flow_bind_key/flow_dispatch_key) the spec calls for, so edge-delete and zoom-key packages reuse it instead of each hacking flow_feed.
  - Does NOT fix the zoom==1 carry-overs: flow_fit_view is explicitly zoom==1-only (documented like flow_hit_node) and left for the zoom package to generalise.

---

### 3. Multi-select & Marquee (shift/ctrl-click, box-select, selected-last draw, multi-node drag)  `[M]`  ·  id: `multiselect`

**Goal.** Make selection a true set: shift/ctrl-click adds or toggles nodes, shift-drag on the empty pane draws a marquee that selects nodes (full=contains, partial=intersects), the whole selected set drags together, and selected nodes draw on top. Single-click and single-node-drag behavior is preserved exactly.

**User value.** A user can select many nodes at once (shift/ctrl-click to add or toggle, or shift-drag a box around them), see them all highlighted and drawn on top of unselected nodes, and move the whole group together by dragging any selected node. Clicking empty space still clears; plain click/drag of one node is unchanged.

**Files touched.**
  - src/flow_geom.h
  - src/flow_model.h
  - src/flow_input.h
  - src/flow_render.h
  - tests/test_select.c
  - tests/test_marquee.c
  - tests/test_render.c
  - Makefile
  - flow.h
  - tests/snapshots/render_marquee.txt
  - tests/snapshots/render_selected_last.txt

**Entry points (existing functions to extend).**
  - flow_handle_mouse (src/flow_input.h) — press/motion/release branches
  - flow_render (src/flow_render.h) — node draw loop + post-node marquee draw
  - flow_select_node / flow_clear_selection / flow_selected_node (src/flow_model.h) — extend with toggle/set queries
  - flow_new (src/flow_model.h) — init marquee_mode default
  - struct flow (src/flow_model.h) — add marquee state fields
  - flow_rect_union/flow_rect_contains (src/flow_geom.h) — sibling for flow_rect_intersects

**API additions.**
```c
int flow_rect_intersects(flow_rect a, flow_rect b); /* in src/flow_geom.h: true if a and b overlap (touching edges count, like rect_union/contains conventions) */
void flow_toggle_node(flow_t *f, int id); /* add to selection if unset, remove if set; never clears others */
int flow_selected_nodes(flow_t *f, int *out, int max); /* fill out[] with ids of all FLOW_SELECTED nodes in insertion order; return total count (may exceed max; out filled up to max) */
int flow_selected_count(flow_t *f); /* number of FLOW_SELECTED nodes */
typedef enum { FLOW_SELECT_PARTIAL, FLOW_SELECT_FULL } flow_select_mode; /* in src/flow_model.h */
int flow_select_in_rect(flow_t *f, flow_rect world, flow_select_mode mode, int additive); /* select nodes whose abs world rect is contained (FULL) or intersects (PARTIAL) world; if !additive clears first; returns count newly-considered-selected */
void flow_set_marquee_mode(flow_t *f, flow_select_mode mode); /* default mode for shift-drag marquee; defaults to FLOW_SELECT_PARTIAL */
```

**Design notes.**

Mirrors xyflow's SelectionMode (Partial/Full) and additive selection via modifier keys. Selection stays expressed as the existing FLOW_SELECTED node flag; no parallel list, so flow_selected_node/clear_selection keep working. flow_select_node's `additive` path already exists (sets flag without clearing); flow_toggle_node adds the toggle case for ctrl-click.

INPUT (src/flow_input.h, flow_handle_mouse — the single entry point):
- PRESS, button 0, on a node (down_node!=-1): if mods has SHIFT or CTRL, do an additive/toggle select immediately and DO NOT arm a plain replace-select-on-drag. Record this so a subsequent drag moves the whole set. With no mods, behavior is unchanged (classify on move/release as today).
- PRESS, button 0, empty pane (down_node==-1): if mods has SHIFT, arm a marquee (store marquee_active intent + anchor in down_pos); else arm pan as today.
- MOTION past threshold:
  - marquee armed -> set f->marquee_on=1 and update f->marquee_cur=scr (screen coords); live-select via flow_select_in_rect each motion using the world rect from anchor..cur and f->marquee_mode (additive=0 so it tracks the box; if SHIFT was held to ADD to an existing selection, pass additive=1 and union — keep it simple: replace within marquee, matching xyflow default).
  - node drag where down_node is selected and selection count>1 -> MULTI-DRAG: compute world delta from previous drag position and apply flow_move_node to every selected node by the same delta (store last applied drag world pos in drag_grab-relative terms). Single selected node path stays as the existing grab-offset move.
- RELEASE:
  - marquee_on -> finalize (selection already applied during motion); clear marquee_on/marquee_active; do NOT fire on_pane_click.
  - click (no move) with SHIFT/CTRL on a node -> already toggled on press; do nothing extra (don't fire on_node_click replace). Plain click unchanged.
  - reset all interaction + marquee state.

MODEL (src/flow_model.h): add struct fields `int marquee_active, marquee_on; flow_pt marquee_anchor, marquee_cur; flow_select_mode marquee_mode;` to struct flow (init marquee_mode=FLOW_SELECT_PARTIAL in flow_new). flow_select_in_rect iterates nodes, uses flow_node_rect_abs(f,n) (world rect; zoom==1 carry-over is fine here) and flow_rect_contains-of-corners / flow_rect_intersects against the world rect; sets/clears FLOW_SELECTED. Multi-drag delta math reuses flow_to_world.

RENDER (src/flow_render.h, flow_render): change the node loop from single insertion-order pass to TWO passes over the same array — first draw nodes WITHOUT FLOW_SELECTED, then nodes WITH FLOW_SELECTED — so selected draw on top (stable order within each pass). Edges and minimap unchanged. After nodes (before on_overlay), if f->marquee_on, draw the marquee rectangle: convert marquee_anchor/marquee_cur (already screen coords) to a normalized flow_rect and stroke its border with a distinct glyph (e.g. dashed/box using flow_cellbuf_put with FLOW_REVERSE attr or '·' U+00B7 corners) clipped to the buffer. Marquee draws under the overlay so app panels still win.

**Test plan.**
  - test_select.c (extend, keep existing asserts intact): SHIFT-click A then SHIFT-click B (\x1b[<4;col;rowM + m at each) -> flow_selected_count==2, both A and B have FLOW_SELECTED. CTRL-click A again (\x1b[<16;..M/m) -> A toggled off, count==1, B still selected. flow_selected_nodes(f,out,8) returns 1 and out[0]==B.
  - test_select.c: plain (no-mod) click B after a multi-selection -> count==1, only B selected (replace semantics preserved).
  - test_marquee.c (new): three nodes at known world rects; shift-drag a box (press \x1b[<4;..M on empty, motion \x1b[<36;..M to grow box, release \x1b[<4;..m). With FLOW_SELECT_PARTIAL a box clipping one node selects it; with flow_set_marquee_mode(FLOW_SELECT_FULL) a box that only partially overlaps that node does NOT select it but a fully-enclosing box does. Assert flow_selected_count for each.
  - test_marquee.c: shift-drag marquee does NOT fire on_pane_click (register a counting on_pane_click cb; assert it stays 0), while a plain no-shift empty click still fires it once.
  - test_marquee.c: multi-node drag — select A and B (shift-clicks), then drag A by +10/+3 via \x1b[<0;..M then \x1b[<32;..M then \x1b[<0;..m; assert BOTH A.pos and B.pos shifted by the same delta and an unselected C did not move.
  - test_geom (extend test_geom.c or fold into test_marquee): flow_rect_intersects — overlapping rects true; disjoint false; edge-touching matches contains/union convention; containment implies intersects.
  - test_render.c: SNAPSHOT render_selected_last — two overlapping nodes where the later-inserted one is unselected but the earlier one is selected; snapshot shows the selected (bold) box drawn on top. SNAPSHOT render_marquee — set f->marquee_on with a known anchor/cur and render; snapshot shows the marquee border. Capture goldens on first run and eyeball per repo convention.
  - Add test_marquee to Makefile TESTS; run `make test` and a sanitizer build `cc -std=c11 -fsanitize=address,undefined -g tests/test_marquee.c -o /tmp/tm -lm && /tmp/tm` (and same for test_select) to confirm ASan/UBSan clean.

**Acceptance.**
  - make test passes; new test_marquee + extended test_select/test_render all green; existing test_mouse/test_input/test_select asserts still pass (single-click and single-node-drag unchanged).
  - Selected nodes render on top of unselected nodes (render_selected_last snapshot verified); insertion order preserved within each draw pass.
  - Shift/Ctrl-click builds a multi-node selection (flow_selected_count/flow_selected_nodes reflect it); ctrl-click toggles individual nodes off.
  - Shift-drag on empty pane draws a marquee and selects the right set per FLOW_SELECT_PARTIAL vs FLOW_SELECT_FULL; marquee drag does not fire on_pane_click.
  - Dragging any selected node moves the entire selection by the same world delta; unselected nodes stay put.
  - ASan/UBSan builds of test_select and test_marquee run clean (no leaks/UB), including marquee state allocation-free reuse of fixed struct fields.

**Depends on.** nothing  
**Conflicts with.**
  - Zoom package (also edits flow_hit_node/flow_render/minimap and the same flow struct interaction state)
  - Edge-handle routing package (edits flow_render edge anchors)
  - Any package touching flow_input.h flow_handle_mouse (e.g. connect/reconnect, autopan)

**Carry-overs fixed.**
  - Resolves 'selection is single only (no multi/marquee)' and 'no selected-last draw order; nodes drawn in insertion order' — both directly addressed.
  - Partially resolves the selectNodesOnDrag single-node limitation by adding multi-node group drag.
  - Does NOT fix the zoom==1 assumption in flow_hit_node/minimap or flow_select_in_rect (left to the zoom package); does NOT change fixed edge anchors; does NOT add parent-relative move (flow_move_node still top-level — multi-drag applies the same per-node delta, which is correct for top-level nodes).

---

### 4. Connections: drag/click from port handles to create edges  `[L]`  ·  id: `connections`

**Goal.** Add xyflow-style connections: render port-handle markers on hovered/selected nodes from flow_node_type.handles, start a connection by pressing a source handle, draw a live dashed preview following the cursor, and complete on a target handle/node by calling flow_add_edge with the chosen source/target handle ids. Also offer connectOnClick and make the edge router anchor on the stored handles instead of the fixed source-right/target-left points.

**User value.** A user can hover a node to reveal its port handles, press-drag from an output port to another node's input port to create a directed edge (with a dashed rubber-band preview while dragging), or click one handle then another to connect without holding the mouse. Connections respect handle kinds/ids and reuse add_edge validation (no self/missing/duplicate edges), and edges visually attach at their real declared ports.

**Files touched.**
  - src/flow_model.h
  - src/flow_input.h
  - src/flow_render.h
  - src/flow_types.h
  - src/flow_route.h
  - tests/test_connect.c
  - tests/test_render.c
  - tests/snapshots/render_handles_hover.txt
  - tests/snapshots/render_connect_preview.txt
  - tests/snapshots/render_edge_handle_anchors.txt
  - demos/topo.c
  - Makefile
  - flow.h

**Entry points (existing functions to extend).**
  - flow_handle_mouse (src/flow_input.h) — add handle hit-test at press, conn_active branches in motion/release, connectOnClick
  - flow_feed (src/flow_run.h) — decode Esc to flow_cancel_connection
  - flow_render (src/flow_render.h) — new handle-marker pass + preview pass; replace fixed edge anchors with flow_handle_anchor
  - flow_add_edge (src/flow_model.h) — reuse for validation; tighten dup check to include handles
  - struct flow (src/flow_model.h) — add conn_active/conn_node/conn_handle/conn_end fields
  - flow_default_node_type (src/flow_types.h) — attach flow_default_handles
  - flow_route_orthogonal (src/flow_route.h) — receive real sp/tp from handle anchors
  - flow_callbacks (src/flow_model.h) — add on_connect
  - flow_set_minimap/flow_set_callbacks pattern in demos/topo.c — demo a connect interaction

**API additions.**
```c
flow_pt flow_handle_anchor(flow_t *f, const flow_node *n, const flow_handle *h); /* analytic cell anchor in world-abs coords: TOP->(x+w/2,y) RIGHT->(x+w-1,y+h/2) BOTTOM->(x+w/2,y+h-1) LEFT->(x,y+h/2), plus 'along' offset */
int flow_node_handle_count(flow_t *f, int node); /* number of declared handles for node's type */
const flow_handle *flow_node_handle_at(flow_t *f, int node, int idx); /* idx-th handle of node's type, or NULL */
int flow_hit_handle(flow_t *f, flow_pt screen, int *out_node); /* returns handle index + node id at screen cell, or -1; only hits handles on hovered/selected/connecting nodes */
void flow_set_hover(flow_t *f, int node); /* set FLOW_HOVERED on node (clears others); -1 clears all */
int flow_hovered_node(flow_t *f); /* first node with FLOW_HOVERED, or -1 */
int flow_begin_connection(flow_t *f, int node, const char *handle); /* enter CONNECTING from source handle; returns 0 ok, -1 if handle not a valid source */
int flow_update_connection(flow_t *f, flow_pt screen); /* move free end of in-flight preview to screen cell; returns hovered candidate target node id or -1 */
int flow_end_connection(flow_t *f, int node, const char *handle); /* complete: flow_add_edge(src,node,src_handle,handle); returns new edge id or -1; clears CONNECTING */
void flow_cancel_connection(flow_t *f); /* abort in-flight connection (Esc / right-click / drop on pane) */
int flow_connecting(flow_t *f); /* 1 while a connection/preview is in flight */
extern const flow_handle flow_default_handles[2]; /* default node type: one LEFT target 'in', one RIGHT source 'out' */
```

**Design notes.**

Maps xyflow's XYHandle + ConnectionState. (1) HANDLE MODEL: flow_handle already exists ({id[16],kind,pos,along}) and flow_node_type already carries handles/handle_count (currently NULL/0). flow_handle_anchor computes the cell analytically from flow_node_rect_abs per spec §5 (TOP/RIGHT/BOTTOM/LEFT + 'along' offset along the side), returning WORLD-ABS coords; callers project via flow_to_screen so it composes with pan (zoom!=1 stays a documented carry-over, same as flow_hit_node). Give the default node type two handles (flow_default_handles: LEFT 'in' target, RIGHT 'out' source) so the default node gains real ports without a custom type. (2) RENDER: in flow_render, after nodes draw, a new handle pass iterates nodes and, only when n->flags has FLOW_HOVERED or FLOW_SELECTED (xyflow shows handles on hover), draws the marker glyph U+25C9 (the spec's ◉) at flow_to_screen(flow_handle_anchor(...)) using flow_cellbuf_put so it sits on the node border, FLOW_BOLD when that handle is the active connecting source. This is a new layer slot in the documented z-order (edges->nodes->handles->preview->minimap->overlay). (3) PREVIEW: store in-flight connection state on struct flow (conn_active, conn_node, conn_handle[16], conn_end flow_pt screen). When conn_active, after the handle pass, route from the source anchor (screen) to conn_end with the existing flow_route_orthogonal but render its cells DASHED: emit only cells where (x+y)&1==0, or use a dashed glyph variant, so the preview reads as a rubber-band distinct from committed solid edges; no arrowhead until it lands on a target. (4) INTERACTION (extends flow_handle_mouse without breaking node-drag/select): on PRESS button 0, hit-test handles FIRST via flow_hit_handle (hit order handle->node body->pane per spec §8); if a source handle is hit, call flow_begin_connection and DO NOT arm the node-drag/pan path (skip setting down_node so existing drag/select code is untouched). While conn_active, MOTION calls flow_update_connection (updates conn_end + recomputes FLOW_HOVERED on the candidate target so its handles reveal). RELEASE while conn_active: if over a target handle/node, flow_end_connection -> flow_add_edge + on_connect callback; else flow_cancel_connection. connectOnClick: a click (press+release same cell) on a handle when NOT already connecting calls flow_begin_connection and stays armed across events; the next click on another handle/node completes it; a click on empty pane cancels. Right-click or 'Esc' (added in flow_feed) cancels. The existing mouse_down/moved/down_node/drag_node/dragging_pan fields are reused unchanged; conn_* fields are orthogonal so node-drag, pan, marquee-less select all keep working — the only branch added is 'if handle hit at press' and 'if conn_active' guards at the top of each event arm. (5) ROUTER ANCHORS (fixes carry-over): flow_render's edge loop replaces the hard-coded sa={right} / ta={left} with flow_handle_anchor when e->source_handle/target_handle name a declared handle (fall back to nearest-by-position, then to the old right/left default), and passes that handle's flow_pos as sp/tp to et->route so flow_route_orthogonal can later honor approach direction. on_connect added to flow_callbacks (spec §8 signature). FLOW_HOVERED finally gets a writer (flow_set_hover) so the render gate has a source of truth; flow_run's loop can set hover from motion in a follow-up, but tests drive it directly.

**Test plan.**
  - tests/test_connect.c (new, added to Makefile TESTS): flow_handle_anchor math — for a node at world (10,5) sized 5x3 with one LEFT and one RIGHT handle, assert LEFT anchor == (10,6) and RIGHT anchor == (14,6) and TOP/BOTTOM as specified; verify 'along' shifts the anchor along the side.
  - tests/test_connect.c: flow_default_handles wired — flow_node_handle_count(f, default_node) == 2; flow_node_handle_at returns a LEFT target 'in' and a RIGHT source 'out'.
  - tests/test_connect.c: flow_hit_handle — set FLOW_HOVERED on node A via flow_set_hover, assert flow_hit_handle at A's RIGHT-anchor screen cell returns that handle index + node id; assert it returns -1 when the node is NOT hovered/selected (handles only hittable when revealed); assert -1 on empty cell.
  - tests/test_connect.c (synthetic SGR via flow_feed): drag-connect happy path — two default nodes A(10,5) and B(30,5); hover A; press on A's RIGHT handle cell (SGR press), motion toward B (assert flow_connecting()==1 and B becomes FLOW_HOVERED), release on B's LEFT handle cell; assert flow_edge_count==1 and the new edge has source==A, target==B, source_handle=='out', target_handle=='in', and on_connect fired with (A,B).
  - tests/test_connect.c: connectOnClick path — click (press+release same cell) A's RIGHT handle, then click B's LEFT handle; assert one edge A->B created and flow_connecting()==0 afterwards; a click on empty pane mid-connection cancels (no edge, flow_connecting()==0).
  - tests/test_connect.c: validation reuse — attempt to connect A's handle back to A (self) and to a duplicate A->B; assert flow_add_edge rejections propagate (edge_count unchanged, flow_end_connection returns -1).
  - tests/test_connect.c: node-drag/select regression — repeat the existing test_select/test_mouse drag-a-node and pan-empty sequences with handles present; assert pressing the node BODY (not a handle) still moves/selects and pressing empty still pans (connection path not triggered).
  - tests/test_render.c (extend): SNAPSHOT('render_handles_hover') of a hovered default node showing ◉ markers on its L/R borders, and a non-hovered node showing none; SNAPSHOT('render_connect_preview') of an in-flight dashed preview from a source handle to a free cursor cell (assert no arrowhead, dashed pattern present); SNAPSHOT('render_edge_handle_anchors') of a committed A->B edge anchored at RIGHT-of-A and LEFT-of-B via flow_handle_anchor (replacing the fixed anchors).
  - ASan/UBSan gate: compile tests/test_connect.c with -std=c11 -fsanitize=address,undefined -fno-sanitize-recover=all -g -lm and run clean; also run the headless connect sequence under sanitizers (no leaks of flow_route preview cells — free rt.cells each frame).

**Acceptance.**
  - make test passes with new tests/test_connect.c and extended tests/test_render.c; all snapshots reviewed and committed.
  - Hovering or selecting a node reveals ◉ handle markers on its borders at the analytic anchors; non-hovered/unselected nodes show none.
  - Press-drag from a source handle to a target handle/node creates exactly one validated edge with the correct source_handle/target_handle, fires on_connect, and shows a dashed live preview (no arrowhead) while dragging.
  - connectOnClick (click source handle, then click target) creates the same edge; clicking empty pane mid-connection cancels with no edge.
  - Self-edge, missing-endpoint, and duplicate(source,target,handles) connections are rejected (flow_end_connection returns -1, edge_count unchanged).
  - Committed edges anchor at their declared handles (flow_handle_anchor) rather than the fixed source-right/target-left points; render snapshot reflects this.
  - Existing node-drag, pane-pan, wheel-pan, click-select, and right-click-context behaviors are unchanged (test_mouse/test_select still green).
  - Clean under ASan+UBSan for the full suite + a headless connect input sequence; no leaks/overflows; flow.h regenerated by amalgamate (src/ edited, never flow.h by hand).

**Depends on.** nothing  
**Conflicts with.**
  - Zoom generalization (touches flow_hit_node/minimap and would also need to scale flow_hit_handle/flow_handle_anchor)
  - Edge selection / reconnection (touches flow_render edge loop and flow_handle_mouse press hit-order)
  - Multi-select / marquee (touches flow_handle_mouse press/drag state machine)
  - Selected-last draw order refactor (touches flow_render node/handle layering)

**Carry-overs fixed.**
  - Fixes 'flow_render edge anchors are fixed (source right-center, target left-center; handles not yet used for routing)' by anchoring committed edges on stored/declared handles via flow_handle_anchor and passing real flow_pos to the router.
  - Activates the previously-unused flow_node_type.handles/handle_count fields and the unused FLOW_HOVERED flag (adds flow_set_hover/flow_hovered_node writers).
  - Partially tightens flow_add_edge duplicate detection toward handle-aware (source,target,handles) dedup per spec §4, instead of (source,target) only.

---

### 5. Edge interaction: select, delete (cascading), reconnect, and labels  `[L]`  ·  id: `edge-interaction`

**Goal.** Make edges first-class interactive objects: hit-test a point near any routed cell to an edge, click to select/highlight, press Delete/x to remove the selected edge or node (cascading incident edges), drag an edge endpoint to reconnect it to another node, and render an optional edge label at the router's label_anchor. Adds the minimal key-dispatch hook needed for Delete/x.

**User value.** A user can click an edge to select it (it highlights), press Delete or x to remove the selected edge — or a selected node, which also removes its connected edges — and grab an edge's endpoint and drag it onto a different node to rewire the graph. Edges can carry a visible label at their midpoint.

**Files touched.**
  - src/flow_model.h
  - src/flow_input.h
  - src/flow_render.h
  - src/flow_route.h
  - src/flow_run.h
  - flow.h
  - tests/test_edge.c
  - tests/test_render.c
  - Makefile
  - demos/topo.c

**Entry points (existing functions to extend).**
  - flow_handle_mouse (src/flow_input.h) — add endpoint-hit check before pan classification on press; add reconnect drag state on motion/release; route edge-click selection on release
  - flow_render (src/flow_render.h) — edge loop: highlight selected edge cells + draw e->label at rt.label_anchor
  - flow_feed (src/flow_run.h) — dispatch non-mouse/non-arrow bytes (x, DEL 0x7f, ESC[3~) through the new key table
  - flow_clear_selection / flow_select_node (src/flow_model.h) — extend to cover edge FLOW_SELECTED for mutual exclusivity
  - flow_route_orthogonal (src/flow_route.h) — already sets out->label_anchor; reused as-is for label placement
  - struct flow (src/flow_model.h) — add reconnect_edge/reconnect_which state and the key-dispatch table

**API additions.**
```c
int flow_hit_edge(flow_t *f, flow_pt screen, int tol);  /* topmost edge whose routed path passes within tol cells of screen; -1 if none */
void flow_select_edge(flow_t *f, int id, int additive);  /* sets FLOW_SELECTED on the edge; clears node+edge selection unless additive */
int flow_selected_edge(flow_t *f);  /* first selected edge id, or -1 */
void flow_remove_edge(flow_t *f, int id);  /* frees label, removes from edge array */
void flow_remove_node(flow_t *f, int id);  /* removes node and cascades all incident edges (top-level only for now, matching flow_move_node) */
void flow_reconnect_edge(flow_t *f, int edge, int endpoint_node, const char *handle, int which);  /* which: 0=source endpoint, 1=target endpoint; revalidates like flow_add_edge (no self/dup) */
void flow_set_edge_label(flow_t *f, int edge, const char *label);  /* strdup into edge->label; frees prior; NULL clears */
void flow_clear_selection(flow_t *f);  /* EXTEND existing: also clear FLOW_SELECTED on edges */
int flow_edge_endpoint_screen(flow_t *f, const flow_edge *e, int which, flow_pt *out);  /* screen cell of source(0)/target(1) anchor, as used by the renderer; returns 1 on success */
void flow_bind_key(flow_t *f, const char *seq, void (*fn)(flow_t *f, void *user), void *user);  /* register a key/byte-sequence handler dispatched by flow_feed */
```

**Design notes.**

Mirrors xyflow's edge selection, edges-deletable, reconnectable, and edge labels. HIT-TEST: extend the spec's order to handle -> node body -> edge -> pane. flow_hit_edge recomputes each edge's route exactly as flow_render does (source-right-center / target-left-center anchors in SCREEN space via flow_to_screen, calling the registered edge type's route()), then returns the topmost (last-drawn = last in array) edge with any routed cell within Chebyshev distance tol of screen; tol defaults to 1 at the call site. It must free rt.cells each iteration (route owns a heap array, see flow_route_orthogonal). SELECTION: edge flags reuse the existing FLOW_SELECTED bit (flow_edge already has unsigned flags). A single global selection: flow_select_edge clears node selection (and other edges) unless additive; flow_select_node likewise should clear edge selection (extend it) so node/edge selection are mutually exclusive — keep this minimal by having both clear-paths call the extended flow_clear_selection. flow_selected_edge scans edges for the flag. RENDER (flow_render edge loop, lines ~472-488): when e->flags & FLOW_SELECTED, draw routed cells with FLOW_BOLD attr (and/or FLOW_REVERSE) instead of 0 so the selected edge stands out; when e->label is non-NULL, draw it at rt.label_anchor (already produced by the router) via flow_cellbuf_put per glyph, centered/clipped, drawn after the path so it sits on top. Keep node draw order unchanged. RECONNECT: flow_handle_mouse press handler currently does flow_hit_node then treats -1 as pan. Insert an endpoint-hit check FIRST: compute both endpoint screen anchors via flow_edge_endpoint_screen for the topmost edge under the cursor (or near it) and, if the press is on/adjacent to an endpoint, enter a reconnect drag (new flow struct fields: int reconnect_edge, reconnect_which). On motion, do nothing destructive (live preview is optional/out-of-scope; keep the original edge shown). On release, hit-test the node under the cursor; if valid (not self, not creating a duplicate (source,target)), call flow_reconnect_edge(edge, hit_node, "", which); otherwise leave the edge unchanged. This reuses the existing press/moved/release classification machinery. KEYBOARD: add a tiny key-dispatch table to the flow struct (array of {char seq[8]; fn; user}) and have flow_feed match non-mouse, non-arrow byte runs against it (longest-match, single-byte keys like 'x' included). flow_run currently hard-codes 'q'; leave that, but route 'x' and the Delete byte (0x7f DEL and the ESC[3~ CSI sequence) through flow_feed dispatch. The built-in Delete/x handler: if flow_selected_edge != -1 remove that edge; else if flow_selected_node != -1 remove that node (cascading). DEBT FIXED: adds flow_remove_node/edge (none existed); cascading delete; edge hit-test (handle->node->edge->pane order partially realized: node then edge). CARRY-OVER respected: zoom==1 assumption stays (hit-edge uses same unscaled screen projection as renderer); reconnect handles top-level nodes only, consistent with flow_move_node. Edge array removal uses swap-or-shift; since flow_hit_edge/render iterate by index and edges are looked up by id via flow_get_edge, prefer order-preserving shift to keep draw order stable.

**Test plan.**
  - tests/test_edge.c (new, registered in Makefile TESTS): build a graph with two nodes A(10,5) B(30,5) and an edge; assert flow_hit_edge on a routed mid cell returns the edge id and a far-away point returns -1 (tol=1). Mirror test_select.c structure.
  - test_edge.c: flow_select_edge(e) then flow_selected_edge()==e and edge->flags & FLOW_SELECTED; selecting a node afterward clears the edge flag (mutual exclusivity); flow_clear_selection clears both.
  - test_edge.c: flow_set_edge_label(e,"L") sets edge->label=="L"; setting again frees+replaces (no leak under ASan); flow_set_edge_label(e,NULL) clears to NULL; flow_remove_edge then flow_get_edge(e)==NULL and edge_count decremented.
  - test_edge.c: flow_remove_node(A) drops A and the incident edge (edge_count==0, flow_get_node(A)==NULL); a second unrelated edge between other nodes survives (cascade correctness).
  - test_edge.c: flow_reconnect_edge(e, C, "", 1) repoints target to C (edge->target==C); reconnect creating a self-edge or duplicate (source,target) is rejected (edge unchanged); flow_edge_endpoint_screen returns the same source-right/target-left anchors the renderer uses.
  - test_edge.c (synthetic SGR via flow_feed, like test_mouse.c): click on an edge mid cell (press+release same cell) selects it (flow_selected_edge==e, node selection==-1). Compute the mid-cell screen coords from the known anchors.
  - test_edge.c (synthetic SGR reconnect): press on the target endpoint anchor cell, motion to a cell inside node C, release -> edge->target==C; press on endpoint then release over empty space leaves edge unchanged.
  - test_edge.c (keyboard via flow_feed): select an edge then feed "x" -> edge removed (edge_count 0); re-add, select node A then feed the Delete CSI "\x1b[3~" -> node A removed and incident edge cascaded. Verify flow_bind_key registers a custom handler that flow_feed invokes for its sequence.
  - tests/test_render.c: extend with a SNAPSHOT 'render_edge_label' (two nodes + edge with flow_set_edge_label) asserting the label glyphs appear near label_anchor, and a SNAPSHOT 'render_edge_selected' where the selected edge path carries FLOW_BOLD (assert at least one routed cell has attr&FLOW_BOLD).
  - Run make test (all existing suites must stay green) and the ASan/UBSan gate: cc -std=c11 -fsanitize=address,undefined -fno-sanitize-recover=all -g tests/test_edge.c -o /tmp/st_edge -lm && /tmp/st_edge — clean (the label strdup/free and reconnect paths are the leak-prone spots).

**Acceptance.**
  - make test passes including the new test_edge suite and the extended test_render snapshots; no existing test regresses.
  - ASan+UBSan clean (-fsanitize=address,undefined -fno-sanitize-recover=all) for test_edge, test_render, test_mouse, test_select, and the topo demo driven with synthetic input — no leaks from edge label strdup/free or route cell arrays, no use-after-free after flow_remove_node/edge.
  - flow_hit_edge returns the correct edge for a point within tol of any routed cell and -1 otherwise, using the same projection/route as flow_render.
  - Clicking an edge selects+highlights it and clears node selection; clicking a node clears edge selection (mutually exclusive single selection).
  - Delete/x removes the selected edge, or the selected node together with all its incident edges; verified via flow_feed synthetic input and direct API.
  - Dragging a valid edge endpoint onto another node updates that endpoint (source/target) and re-routes on next render; invalid drops (self/duplicate/empty) leave the edge unchanged.
  - An edge with a non-NULL label renders that label at the router's label_anchor; amalgamation (tools/amalgamate.sh via make) regenerates flow.h with no hand edits.

**Depends on.** nothing  
**Conflicts with.**
  - Zoom package (both generalize flow_hit_node/hit-test + renderer projection; this package keeps zoom==1, a zoom package must later make flow_hit_edge zoom-aware)
  - Connections package (handle-aware edge anchors and connect-drag share flow_render's edge-anchor code and flow_handle_mouse press classification; coordinate the endpoint/handle hit-test order)
  - Multi-select / marquee package (touches flow_clear_selection, flow_select_node, and selection semantics extended here to edges)
  - Any key-dispatch package (this adds a minimal flow_bind_key + flow_feed dispatch; a dedicated keybindings package would supersede/extend it — align on the table representation)

**Carry-overs fixed.**
  - Adds flow_remove_node and flow_remove_edge (no removal mutators existed before) with cascading incident-edge deletion.
  - Generalizes selection from node-only to node-or-edge via the shared FLOW_SELECTED bit and an extended flow_clear_selection.
  - Introduces the first keyboard-dispatch hook (flow_bind_key + flow_feed key handling) beyond the hard-coded 'q' in flow_run.
  - Begins realizing the spec's handle->node->edge->pane hit-test order (node then edge) ahead of the handles/connections package.

---

### 6. ZOOM: real-scale viewport, pointer-centered zoom, fit-view, and level-of-detail rendering  `[L]`  ·  id: `zoom`

**Goal.** Make viewport.zoom a real float scale used everywhere: pointer-centered flow_set_zoom keeping the cell under the cursor fixed, zoom_in/out with min/max clamp, fit_view from bounds, and level-of-detail collapse so node renderers draw a marker/short-label below a zoom threshold. Generalize flow_hit_node and the minimap to be correct at any zoom by sharing the exact footprint the renderer draws.

**User value.** The user can zoom the graph in and out (Ctrl+wheel toward the cursor, +/- keys, and a fit-to-content command), with the graph spreading apart / contracting via real position scaling, nodes collapsing to compact markers when zoomed far out, clicking still selecting the correct node at any zoom, and the minimap viewport rectangle correctly tracking what is on screen.

**Files touched.**
  - src/flow_view.h (new module: flow_set_zoom/zoom_in/zoom_out/fit_view/zoom + flow__lod_for/flow__node_footprint shared helpers + zoom min/max storage)
  - src/flow_model.h (flow_hit_node uses shared LOD-aware footprint; add zmin/zmax fields to struct flow, initialized in flow_new)
  - src/flow_render.h (node loop computes lod via flow__lod_for instead of hardcoded 0; flow__minimap viewport rect projected at real zoom)
  - src/flow_types.h (default node render gains collapsed branch keyed on ctx.lod)
  - src/flow_input.h (flow_handle_mouse: Ctrl+wheel zooms toward cursor; plain wheel still pans)
  - src/flow_run.h (flow_feed: +/=/-/_ keys zoom centered on screen center)
  - tools/amalgamate.sh (add flow_view to the explicit modules= list, after flow_model)
  - demos/topo.c (device render gains a collapsed marker branch on ctx.lod)
  - tests/test_zoom.c (new)
  - tests/test_render.c (extend: LOD-collapsed low-zoom snapshot; confirm existing zoom==1 snapshots/minimap asserts unchanged)
  - tests/test_select.c or tests/test_mouse.c (extend: Ctrl+wheel zoom, plain-wheel-still-pans regression)
  - Makefile (add test_zoom to TESTS)
  - tests/snapshots/render_lod_low.txt (new golden, auto-captured)

**Entry points (existing functions to extend).**
  - flow_hit_node (src/flow_model.h) — generalize footprint via shared helper
  - flow_render + flow__minimap (src/flow_render.h) — compute lod, fix minimap vp
  - flow_handle_mouse (src/flow_input.h) — Ctrl+wheel zoom branch
  - flow_feed (src/flow_run.h) — +/- key cases
  - flow__default_render (src/flow_types.h) — collapsed LOD branch
  - flow_new (src/flow_model.h) — init zmin/zmax
  - flow_project/flow_unproject (src/flow_geom.h) — already zoom-aware, reused unchanged (no edit)

**API additions.**
```c
void  flow_set_zoom(flow_t *f, float zoom, flow_pt screen_center);  /* clamps to [zmin,zmax]; keeps the cell under screen_center fixed */
void  flow_zoom_in(flow_t *f, flow_pt screen_center);  /* multiply zoom by FLOW_ZOOM_STEP, pointer-centered */
void  flow_zoom_out(flow_t *f, flow_pt screen_center); /* divide  zoom by FLOW_ZOOM_STEP, pointer-centered */
void  flow_fit_view(flow_t *f, int margin);           /* getViewportForBounds: pick zoom+pan so flow_bounds fits with `margin` cells of padding */
float flow_zoom(flow_t *f);                            /* current viewport zoom */
void  flow_set_zoom_limits(flow_t *f, float zmin, float zmax); /* override default clamp range */
int   flow_lod_for_zoom(flow_t *f, float zoom);        /* public form of the shared lod helper: 0 = full, 1 = collapsed (used by custom node types) */
```

**Design notes.**

MODEL DECISION (resolves the literal "scale node rects by zoom" wording against spec §7 + the sub-cell non-goal): a glyph cannot be smaller than one cell, so box GLYPH SIZE stays constant; only POSITION scales with zoom (already handled by flow_project: screen = round(world*zoom + offset)), and low zoom is expressed by LEVEL-OF-DETAIL collapse, not magnification. Therefore flow_geom.h needs NO change (project/unproject are already zoom-aware) and the fix to flow_hit_node is NOT a literal w*zoom; it is: keep the projected top-left, then use the EXACT footprint the renderer draws (full n->w x n->h above the LOD threshold; the collapsed marker size below it). To guarantee hit-test and render never drift, introduce two shared helpers in flow_view.h that BOTH flow_render's node loop and flow_hit_node call: flow__lod_for(f, zoom) -> 0/1 (1 when zoom < FLOW_LOD_THRESHOLD, default ~0.6), and flow__node_footprint(f, n, lod) -> flow_rect in screen coords (projects flow_node_rect_abs top-left, sets w/h to full size at lod 0 or the collapsed marker size, e.g. label-truncated width x 1, at lod 1).

POINTER-CENTERED ZOOM math (use FLOAT anchor, never flow_to_world which rounds to int cells and causes drift): given old zoom z0 and view.ox/oy, wx=(center.x-view.ox)/z0; wy=(center.y-view.oy)/z0; z1=clamp(z1,zmin,zmax); view.ox=center.x-wx*z1; view.oy=center.y-wy*z1; view.zoom=z1. Invariant to test: after flow_set_zoom centered on a node, flow_hit_node(f,center) returns the same node id as before.

FIT_VIEW math: B=flow_bounds(f); if (B.w<=0||B.h<=0) return; z=clamp(min((cols-2*margin)/(float)B.w, (rows-2*margin)/(float)B.h), zmin, zmax); view.zoom=z; view.ox=cols/2.0f-(B.x+B.w/2.0f)*z; view.oy=rows/2.0f-(B.y+B.h/2.0f)*z. Result: every node rect projects within [margin, cols-margin] x [margin, rows-margin] and bounds center maps to screen center.

MINIMAP fix (the ONE genuine literal zoom multiply): flow__minimap currently builds vp={w0.x,w0.y,cb->w,cb->h} assuming zoom==1. Replace with projecting both screen corners to world: w0=flow_to_world(f,{0,0}); w1=flow_to_world(f,{cb->w,cb->h}); vp={w0.x,w0.y,w1.x-w0.x,w1.y-w0.y}. At zoom==1 this yields w/h == cb->w/cb->h so the existing test_render minimap corner/dot asserts still pass; node dots already use flow_node_abs (world) and need no change.

LIMITS: add float zmin,zmax to struct flow, set in flow_new to defaults FLOW_ZOOM_MIN (~0.25) and FLOW_ZOOM_MAX (~4.0); FLOW_ZOOM_STEP ~1.2 per detent. Expose flow_set_zoom_limits for adjustability.

WIRING: flow_input.flow_handle_mouse — in the FLOW_MOUSE_WHEEL branch, if (ev->mods & FLOW_MOD_CTRL) zoom toward (ev->x,ev->y) (button 0=in,1=out) and return; otherwise keep the existing pan behavior unchanged (regression-tested). SGR encoding for Ctrl+wheel-up = button 64|16=80 -> "\x1b[<80;X;YM". flow_run.flow_feed — recognize '+'/'=' (zoom in) and '-'/'_' (zoom out) as single bytes, centered on screen center (cols/2,rows/2) since keyboard has no cursor; route through the same flow_zoom_in/out. LOD render: flow_render node loop replaces the hardcoded ctx={zoom,flags,0} with lod=flow__lod_for(f,f->view.zoom). default + device node renderers branch on ctx.lod: lod 0 draws the full box; lod 1 draws a single collapsed cell/short marker (e.g. first label char or a filled bullet) so a zoomed-out graph stays legible — the honest terminal analog of "small far away".

**Test plan.**
  - test_zoom.c (unit, flowtest.h): project/unproject round-trip at zoom 2.0 and 0.5 (flow_set_zoom then flow_to_screen/flow_to_world a known world point, ASSERT_INT the scaled+offset result).
  - test_zoom.c pointer-centered invariant: add a node, click-select via flow_hit_node at its center cell, call flow_set_zoom(2.0, center); ASSERT_INT(flow_hit_node(f, center), same id) — the cell under the cursor stays on the same node.
  - test_zoom.c clamp: flow_set_zoom(f, 100.0, c) then ASSERT flow_zoom(f)==zmax; flow_set_zoom(f, 0.001, c) -> zmin; repeated flow_zoom_in saturates at zmax; repeated flow_zoom_out saturates at zmin.
  - test_zoom.c fit_view: add nodes spanning a wide world rect, flow_fit_view(f, 2); for each node ASSERT its projected rect lies within [2, cols-2] x [2, rows-2]; ASSERT the bounds-center projects to ~screen center.
  - test_zoom.c hit_node at zoom!=1 AND under LOD: at zoom 0.5 (below threshold) ASSERT clicking the drawn collapsed marker cell hits the node, and clicking a cell where the full box WOULD be but is not drawn returns -1 — proves hit-test matches render footprint via the shared helper.
  - test_render.c snapshot SNAPSHOT("render_lod_low", ...): compose a 2-node scene at zoom 0.4 and snapshot the collapsed (marker) output; first run auto-captures golden, eyeball-verify it shows markers not full boxes.
  - test_render.c regression: existing render_single / render_two_edge snapshots and the minimap corner/dot asserts must still pass unchanged at zoom==1 (proves the minimap vp-projection change is a no-op at zoom 1).
  - test_select.c (or test_mouse.c) synthetic SGR: feed Ctrl+wheel-up "\x1b[<80;40;12M"; ASSERT flow_zoom increased and the world point under (40,12) is unchanged (pointer-centered); feed plain wheel-up "\x1b[<64;40;12M"; ASSERT zoom unchanged AND view offset changed (still pans) — the no-regression guard.
  - test_zoom.c / run: feed '+' then '-' bytes via flow_feed; ASSERT zoom rises then returns toward original, centered on (cols/2,rows/2).
  - ASan/UBSan: add test_zoom to the sanitizer loop (plan Task 10 Step 3b pattern: cc -fsanitize=address,undefined -fno-sanitize-recover=all).

**Acceptance.**
  - make test is green including the new test_zoom and extended test_render/test_select.
  - All existing tests still pass with zero changes to their assertions except the new additions (no regression in geom/cell/model/route/render/input/mouse/select).
  - Existing snapshots render_single.txt and render_two_edge.txt are byte-identical (zoom==1 path unchanged); render_lod_low.txt is newly captured and eyeball-verified to show collapsed markers.
  - Building and running every test (including test_zoom) and both demos under -fsanitize=address,undefined -fno-sanitize-recover=all produces no ASan/UBSan reports and exit 0.
  - flow_hit_node returns the correct node id at zoom 0.5, 1.0, and 2.0, and at LOD-collapsed zoom the hittable footprint equals the drawn marker (verified by the dedicated test).
  - After flow_set_zoom(z, c) the cell under c maps to the same world point it did before (pointer-centered, no drift across repeated calls).
  - flow_fit_view(margin) places all nodes within the margin-padded screen and centers the bounds.
  - Ctrl+wheel zooms toward the cursor; plain wheel still pans; +/- keys zoom centered on screen center.
  - flow.h is regenerated by tools/amalgamate.sh (flow_view present in the explicit modules list, placed after flow_model) — flow.h never hand-edited.

**Depends on.** nothing  
**Conflicts with.**
  - Any package touching src/flow_render.h (minimap/background widgets, selected-last draw order, connections preview) — both edit flow__minimap and the node render loop
  - Any package touching src/flow_input.h flow_handle_mouse (connections drag, marquee/multi-select) — both edit the mouse handler/wheel branch
  - Any package touching src/flow_run.h flow_feed (keybindings, undo/redo key wiring) — both add key cases
  - Any package touching src/flow_types.h default node render (handles/connection anchors) — both edit flow__default_render

**Carry-overs fixed.**
  - flow_hit_node no longer assumes zoom==1: it projects the node top-left via flow_to_screen and uses the shared LOD-aware footprint helper, so hit-testing is correct at any zoom and matches exactly what flow_render draws.
  - The minimap no longer assumes zoom==1: the viewport rectangle is computed by projecting both screen corners to world (w1-w0) instead of hardcoding cb->w x cb->h, so the on-screen rect is accurate at every zoom level.

---

### 7. Serialization: hand-rolled JSON flow_save/flow_load with node-data vtable hooks  `[M]`  ·  id: `serialize`

**Goal.** Add dependency-free JSON persistence: flow_save(f, path) writes durable graph structure (nodes id/type/pos/parent, edges source/target/handles/type/label, viewport) plus opaque node->data via optional save/load vtable hooks; flow_load(f, path) reconstructs the graph so the topo demo's device struct round-trips byte-for-byte.

**User value.** Users can persist a graph to a .json file and reload it later (and hand-edit/diff it), with custom node types round-tripping their own data via two small vtable hooks. The topo demo gains save-on-quit / load-on-start so a topology survives across runs.

**Files touched.**
  - src/flow_model.h
  - src/flow_json.h
  - tools/amalgamate.sh
  - tests/test_json.c
  - Makefile
  - demos/topo.c

**Entry points (existing functions to extend).**
  - flow_node_type (extend struct in src/flow_model.h with save/load hooks)
  - flow_free (reuse its edge-label free pattern in the private reset helper)
  - flow_add_node / flow_add_edge / flow_measure_node (used by flow_load to rebuild + re-measure)
  - flow_view_get (read viewport for save) and direct f->view set on load
  - flow_register_defaults (default node type initializer gains trailing NULL,NULL for save/load)
  - demos/topo.c (DEVICE node type initializer + main: wire dev_save/dev_load and flow_save/flow_load)
  - tools/amalgamate.sh modules list; Makefile TESTS list

**API additions.**
```c
int flow_save(flow_t *f, const char *path);  /* writes JSON; returns 0 on success, -1 on I/O/encode error */
int flow_load(flow_t *f, const char *path);  /* clears f then rebuilds from JSON; returns 0 on success, -1 on open/parse error */
/* added as two trailing fields of flow_node_type (struct change — keeps existing brace-init { type, measure, render, handles, handle_count } valid via trailing zeros): */
void (*save)(const flow_node *n, FILE *out);                 /* optional: write node->data as a JSON value */
void (*load)(flow_node *n, const char *data_json);           /* optional: parse node->data from the captured \"data\" sub-object text */
/* tiny JSON writer helpers (file-local static, but exposed for tests via the impl): */
void flow_json_str(FILE *out, const char *s);                /* emit a quoted/escaped JSON string */
/* tiny JSON reader: a cursor over an in-memory buffer */
typedef struct { const char *p; const char *end; } flow_json_rd;
int  flow_json_find(const char *obj, const char *key, flow_json_rd *out); /* locate value of key in current object; 1 if found */
int  flow_json_int(flow_json_rd r, int *v);                  /* parse an int value at cursor */
int  flow_json_strv(flow_json_rd r, char *buf, int cap);     /* parse a string value, unescaped, into buf */
int  flow_json_raw(flow_json_rd r, const char **start, int *len); /* capture a raw value span (e.g. the data sub-object) */
```

**Design notes.**

Mirrors xyflow's toObject()/JSON round-trip (nodes/edges/viewport) but hand-rolled in C with zero deps. Persistence boundary follows spec section 4: durable fields only (node id/type/pos/parent + vtable data; edge source/target/source_handle/target_handle/type/label; viewport ox/oy/zoom). Ephemeral state (flags = selection/dragging/hover, all the drag/down_* mouse-state fields, w/h which are recomputed by flow_measure_node, minimap/callbacks) is NEVER written.

FORMAT (stable, hand-editable):
{"version":1,"viewport":{"ox":0,"oy":0,"zoom":1},
 "nodes":[{"id":1,"type":"device","x":6,"y":3,"parent":-1,"data":{...}}],
 "edges":[{"id":1,"source":1,"target":2,"sourceHandle":"","targetHandle":"","type":"","label":"..."}]}
The node "data" value is produced verbatim by the type's save() hook (which gets the same FILE* and writes a JSON value); if no save hook, the "data" key is omitted. On load, the reader captures the raw text span of the "data" value (flow_json_raw) and passes it to the type's load() hook, which allocates and assigns node->data, then flow_measure_node recomputes w/h. Edge label is a heap char* (matches flow_edge.label, freed in flow_free); write it only when non-NULL.

WRITER: pure FILE* fprintf — no buffer building (avoids the realloc machinery in flow_diff_emit). flow_json_str does the 7 mandatory JSON escapes (\\" \\\\ \\b \\f \\n \\r \\t) plus control chars as \\uXXXX. Node ids are written as authored so edges keep referring to the right nodes.

READER: a minimal recursive-descent-free scanner. We don't build a DOM; we do targeted key lookups within an object's brace span (flow_json_find scans top-level keys, skipping nested {}/[] and strings). This is enough because the schema is fixed and shallow. Ints via strtol, strings unescaped into fixed buffers sized to the struct fields (type[32], handles[16], etc.). flow_json_raw returns the span of the data object so custom load() hooks parse their own fields with the same reader.

LOAD SEMANTICS / LIFETIME: flow_load must first reset the graph. There is currently NO public graph-clear and flow_free does NOT free node->data (only edge labels). To avoid a leak/double-free hazard introduced by load() allocating node->data, add a private static reset that frees edges[].label and the node/edge arrays and zeroes counts (reusing nextid/nexteid bump logic). Document clearly that node->data allocated by a load() hook is owned by the caller's type contract (same as today: the app owns data; flow_free never frees it). To keep load() round-trip self-consistent the demo's device load() mallocs a device and stores it; the demo frees them (or accepts process-exit cleanup) — we will NOT silently free arbitrary void* in the library. Node ids are restored exactly and f->nextid is set to max(id)+1 so subsequent adds don't collide. Parsing preserves insertion order (draw order).

STRUCT CHANGE / CONFLICT: appending save/load to flow_node_type is the conflict surface called out in the task — any package that also edits flow_node_type (zoom doesn't, but a future handles/connect package might) must rebase. Existing initializers like { \"default\", measure, render, NULL, 0 } and the topo DEVICE initializer stay valid because C zero-fills the two new trailing members.

MODULE PLACEMENT: new src/flow_json.h inserted into tools/amalgamate.sh module list after flow_render and before flow_input (it needs flow_model + flow_types declarations; ordering after render keeps it with the high-level API). Makefile TESTS gains test_json.

**Test plan.**
  - tests/test_json.c (per-file program: #define FLOW_IMPLEMENTATION then #include "../flow.h" and "flowtest.h", reports via flowtest_report).
  - Writer-only unit: build a graph (2 default nodes + 1 edge, pan the view), flow_save to a temp path under tests/ (e.g. tests/snapshots/.tmp_json), read the file back into a buffer, SNAPSHOT("json_basic", buf) so the exact JSON text is golden-captured and hand-verified once.
  - JSON string escaping unit: call flow_json_str into a temp FILE (tmpfile()), assert output for inputs containing quote, backslash, newline, tab equals the expected escaped form via ASSERT_STR.
  - Round-trip structure: flow_save graph A, flow_load into a fresh flow_t B (with flow_register_defaults), then ASSERT_INT node_count/edge_count equal, ASSERT_INT each node id/pos.x/pos.y/parent equal, ASSERT_STR each node->type and each edge source_handle/target_handle/type equal, and ASSERT viewport ox/oy/zoom equal.
  - Custom-data round-trip (the headline test): register a 'device' node type with save/load hooks identical to topo's struct, add a node whose data is a device with all five string fields populated incl. one containing a quote and a space, save, load into B, then ASSERT_STR every device field round-tripped and ASSERT_INT recomputed w/h match (proving load() ran + flow_measure_node re-ran).
  - Edge label round-trip: set an edge label (heap), save, load, ASSERT_STR label equal; also test the NULL-label case is omitted from JSON and loads back as NULL (no crash, no empty-string artifact).
  - Id preservation + nextid: load a graph whose max node id is 7, then flow_add_node and ASSERT_INT new id == 8 (no collision).
  - Failure paths: flow_load on a nonexistent path returns -1 and leaves f unchanged; flow_load on a truncated/garbage file returns -1 without crashing (run under ASan to prove no over-read past buffer end).
  - Ephemeral-not-persisted assertion: select a node and set its FLOW_SELECTED flag + simulate a drag via flow_feed, save, then grep the produced JSON in-test (strstr) to ASSERT it contains no "flags"/"selected" key.
  - Empty graph: save/load a flow_t with zero nodes/edges produces valid JSON with empty arrays and round-trips to count 0.
  - Run `make test` (whole suite stays green) and a manual ASan/UBSan build of test_json (cc -std=c11 -fsanitize=address,undefined tests/test_json.c -o /tmp/tj -lm && /tmp/tj) reporting 0 errors; clean up the temp JSON file at end of main.

**Acceptance.**
  - `make test` passes with new test_json included in the Makefile TESTS list.
  - A manual ASan+UBSan build of tests/test_json.c runs clean (no leaks from load()-allocated node->data within the test, no heap-buffer-overflow on the garbage-input case, no UB).
  - flow.h is regenerated by `make` (never hand-edited) and contains the src/flow_json.h section; tools/amalgamate.sh lists flow_json in dependency order.
  - Round-trip is lossless for all durable fields: a save->load->save cycle produces byte-identical JSON (verified in-test by comparing the two emitted files).
  - Ephemeral state (selection/flags/drag/hover/w/h-as-stored/minimap/callbacks) never appears in the output JSON.
  - topo demo loads its graph from a JSON file when present and saves on quit, with the device data fully restored (custom save/load hooks wired).

**Depends on.** nothing  
**Conflicts with.**
  - ZOOM/VIEW package (only if it changes flow_viewport field set persisted in viewport object)
  - any package that adds fields to flow_node_type (e.g. CONNECT/handles-routing) — shares the struct-append conflict surface
  - any package that adds/changes durable flow_node or flow_edge fields (e.g. node grouping/labels) — must extend the JSON schema in lockstep

**Carry-overs fixed.**
  - Adds a private graph-reset path (frees edge labels + node/edge arrays, zeroes counts) needed by flow_load — fills the gap that flow_free is the only teardown today; documents that node->data ownership stays app-side.

---
