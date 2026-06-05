# flow — Increment-5 work packages: the keyboard & editing-productivity increment

Eleven work packages + a final integration pass adding the **human-using-the-editor layer** over
increment-4's events & graph API: keyboard-first navigation (Tab focus, Shift-arrow nudge), editing
productivity (selection clipboard, alignment helper lines, node search), framing primitives
(subset bounds / fit-bounds / set-center), render-only viewport culling — and **all four inc-4
deferrals addressed**: three as the opening fix block (#1–#3), the fourth as the explicit
stretch contract-reversal (#11).

Each package section was drafted against the LANDED code on `main` @ `a7d513d` by a verified
fan-out (drift re-verification of the inventory shortlist + deferrals + invariants → per-package
writers grounded in source → per-package citation verifiers → a cross-package consistency pass);
re-verify line numbers at implementation time (earlier packages shift later lines).

**Drift absorbed** (inventory items found LANDED since the inc-3 audit, hence excluded): the full
layout subsystem (`src/flow_layout.h` — dispatcher + LAYERED + FORCE engines, `demos/flowchart.c`,
47-test `test_layout`) and the `flow_set_autopan` setter (`src/flow_model.h:1046`). Excluded for
cause: per-edge animated/marching-ants edges — `flow_run`'s loop blocks on `read()` with no timer
tick (`src/flow_run.h:57`), so the feature needs a run-loop redesign, not an M package.

## Current state (what's already built, on `main`)

Increments 1–4 delivered the interactive engine AND the programmable platform — 25 green tests,
byte-locked snapshot goldens, three demos (`hello_flow`, `topo`, `flowchart`):

- **Model & interaction:** nodes/edges with vtable types, selection (click/modifier/marquee +
  ESC-clears), drag (multi-drag, reparent, extent clamps incl. `FLOW_EXTENT_PARENT`), port-handle
  connections + reconnect + connect-lifecycle events, zoom/LOD, space-pan, auto-pan
  (`flow_set_autopan`), key registry + built-ins.
- **Events & gates:** `flow_callbacks` tail pinned `…, on_nodes_delete, on_viewport_change,
  on_edge_click, on_edge_context, on_edge_dblclick, on_connect_start, on_connect_end, user`;
  the engine-level connection validator (`flow_set_connection_validator` — a GATE on `struct
  flow`, not an observer); `flow__view_set` — THE viewport seam (clamps first, fires
  `on_viewport_change` iff changed).
- **Queries & visibility:** `src/flow_query.h` (incomers/outgoers/connected-edges +
  intersection queries; MODEL-level, fill-buffer idiom); `FLOW_HIDDEN` view-level skip through
  `flow__node_visible` / `flow__edge_visible` (render, hit, marquee, bounds, minimap, handles).
- **Structure & persistence:** groups, JSON save/load, undo/redo journal, deterministic
  auto-layout (`flow_layout`: layered + force).
- **Render:** orthogonal + straight routers, minimap, background grid, statusbar (cols=30 golden
  locks the prefix; hints append).

## Execution order (recommended)

Six blocks; sequential spine, one commit per package. **Stretch** packages are safely cuttable at
sign-off — no non-stretch package depends on one.

| # | id | size | block |
|---|----|------|-------|
| 1 | `minimap-visible-bounds` | S | A — deferral fixes |
| 2 | `validator-load-suspend` | S | A |
| 3 | `marquee-world-anchor` | M | A |
| 4 | `view-frame` | M | B — framing foundation |
| 5 | `tab-focus` | M | C — keyboard-first |
| 6 | `keyboard-move` | S | C — **stretch** |
| 7 | `copy-paste` | M | D — editing productivity |
| 8 | `helper-lines` | M | D |
| 9 | `viewport-culling` | S | E — render perf |
| 10 | `node-search` | M | E — **stretch** |
| 11 | `edge-events-inflight` | M | F — **stretch, contract reversal** |
| — | final integration pass | — | F |

Hard dependencies: **#5 and #10 require #4** (`flow_set_center` / `flow_fit_bounds`). Golden
risk: #9 must keep every existing golden byte-identical (its acceptance proves the cull
output-invisible); #1, #5, #8 ADD goldens (created post-GREEN, staged in their commits); #3 and
#11 INVERT pinned tests — deliberate contract reversals, recorded in their sections.

**Final integration pass** (single commit, after the last surviving package): demo wiring
deferred by individual packages — demo call sites for `flow_set_center` / `flow_fit_bounds` /
`flow_bounds_of` (#4), a demo toggle key for `flow_set_helper_lines` (#8), demo help-text updates
for the new keys (Tab focus, `y/c/p/d`), a #11 showcase if it survives sign-off (the `/` palette
is wired by #10 itself) — plus the accumulated deferrals recorded in package commit bodies.

## Cross-cutting rules (pinned — every package honors these)

- **Key map (final, collision-verified).** Input-path order: mouse CSI → `flow_dispatch_key`
  (NEW #10 key hook at its TOP, then registry longest-first, then built-ins) → feed CSI switch →
  zoom keys → lone-ESC. Dispatch built-ins after inc-5: `Delete`/`x` del, `n` add, `f` fit, `?`
  help, `Space` pan-toggle, `u` undo, `^r` redo (landed) **+ `\t` focus-next, `\r`
  select-focused (#5), `y` copy / `c` cut / `p` paste / `d` duplicate (#7)**. Feed CSI switch:
  `A/B/C/D` bare-arrow pan (landed) **+ `Z` focus-prev (#5 Shift-Tab `\x1b[Z`), `1;2A/B/C/D`
  Shift-arrow nudge (#6, 6-byte guard, disjoint from bare arrows)**. Demo bindings: `l g G h H`
  (landed) **+ `/` palette-open (#10, flowchart only)**. `ISIG` stays on in raw mode
  (`src/flow_term.h:13` clears only `ECHO|ICANON`) — `^C`/`^V` unavailable, hence plain letters.
- **Lone-ESC story** — one branch, four idempotent actions after #5: cancel connection + exit
  space-pan + clear selection (landed) + clear focus (#5). #10's palette consumes ESC earlier
  (via the hook in dispatch) so an open palette closes instead.
- **Statusbar** — append-only past the locked cols=30 prefix. The ONLY library-bar appender is
  #5 (` Tab:focus`, at the tail); #7 MAY append ` d:dup` (recommended skip); all others
  consciously skip. `render_statusbar` stays byte-identical for every package.
- **`struct flow` field-insertion cluster** (zero-init block `src/flow_model.h:247-282`): #3
  `marquee_anchor_world` and #8's helper-guide fields BOTH append at the marquee block `:263` —
  they land sequentially and the later rebases (mutually cross-noted). #5 `focus_node` (sentinel
  `-1`, explicit init in `flow_new` beside `drag_node`); #10 `key_hook_fn/user` beside the
  validator fields `:266`; #7's clipboard struct is the ONLY heap-owning addition — it frees in
  `flow_free` and deliberately NOT in `flow__graph_reset` (clipboard survives `flow_load`).
- **Flags:** NO package consumes a flag bit — `32u` stays the next-free bit (single shared
  node/edge namespace, `src/flow_model.h:2`). Focus is an `int` id field, not a flag.
- **Gates vs observers:** NO `flow_callbacks` field is added this increment. #10's key hook and
  the landed validator are GATES on `struct flow` with setters. The callbacks tail is untouched.
- **Viewport seam:** #4's framing functions write ox/oy/zoom ONLY through `flow__view_set`
  (clamp-first, sig-gated `on_viewport_change`). Framing is never journaled.
- **Layering:** `flow_find_nodes` (#10) and `flow_bounds_of` (#4) are MODEL-level (hidden
  included, fill-buffer idiom); #9's cull is RENDER-LOOP-ONLY — never inside the shared
  `flow__node_visible` / `flow__edge_visible` choke points (bounds/minimap/hit share them); #9
  culls NODES only — edges keep routing (an edge between two off-screen nodes can cross the
  viewport; routed-path-bbox culling recorded as a future option).
- **Render compose order** (unchanged contract, where new output sits): background → edges →
  nodes (#9 culls here; #5's focus ring is a post-pass right after) → handles → connection
  preview → marquee box (#3 reprojects it) → #8 guide overlay → minimap (#1's guard) → app
  overlay → statusbar (#5's hint).
- **Validator asymmetry (deliberate, cross-documented):** #2 SUSPENDS the validator across
  `flow_load`'s edge rebuild (a load must reproduce the saved graph); #7's paste stays GATED (a
  paste violating a live validator should drop those edges).
- **Undo idiom:** #6 and #7 bracket multi-element mutations in `flow__undo_begin/end` as ONE
  step (precedent: `flow_layout`, `src/flow_layout.h:284,312`).
- **`TESTS=` accounting:** baseline 25 stems; five packages append exactly one each in landing
  order — #5 `test_focus` →26, #7 `test_clipboard` →27, #8 `test_helper` →28, #9 `test_culling`
  →29, #10 `test_search` →30. Per-section "25 → 26" claims are relative-to-predecessor. The
  other six packages extend existing test files (no contention: #3 owns test_marquee/test_autopan
  edits, #11 owns test_connect, #6 test_keys, #4 test_viewport_events).
- **Discipline (unchanged):** TDD red-first (compile-RED for snapshot-bearing packages where
  applicable — #1 documents why behavioral-RED with mechanical discriminators is correct there);
  `flow.h` is GENERATED (edit `src/`, run `tools/amalgamate.sh`; `modules=` is untouched this
  increment); whole-suite ASan/UBSan gate per package; existing goldens byte-identical; one
  commit per package, sole author, NO trailers.

## Open questions for sign-off

Each has a recommendation baked into its section; flagged here because they are user-visible.

1. **#10 modal capture (the consistency pass's emergent finding):** the v1 palette hook consumes
   printables/Backspace/Enter/ESC only — Tab focus (#5), Shift-Tab, and Shift-arrow nudge (#6)
   act BEHIND an open palette. Full capture requires the demo hook to byte-count-parse multibyte
   CSIs. Recommendation: ship the v1 limitation, documented.
2. **#10 palette content:** should the demo palette include group containers in match results
   (draft: yes — the `branches` group is searchable once the group type gains `label()`)?
3. **#11 widened event surface:** under the unified fall-through rule, a mid-flight cancel on the
   SOURCE node fires `on_node_click` / `on_node_context`, and a fall-through onto another source
   handle fires a fresh `on_connect_start` right after the cancel's `on_connect_end`.
   Recommendation: allow (one rule, no special cases).
4. **#11 rejected-completion presses:** a validator/duplicate-REJECTED completion (landed on a
   node, no edge added) CONSUMES the press (draft rule: landed-on-a-node = consume).
5. **#3 rounding:** accept ±1-cell render-box alignment at zoom≠1 (matches the world rect the
   selection actually uses) rather than special-casing zoom==1.
6. **#7 copy scope:** copy snapshots ONLY the explicitly selected set — unselected children of a
   copied parent do NOT come along (xyflow brings children; deferred deliberately).
7. **#8 alignment axes:** edge-only (L/R/T/B) in v1 — center alignment deferred (integer-cell
   centers truncate for odd widths; needs a rounding convention).
8. **#5 focus affordances:** focus ring = `FLOW_REVERSE` on the focused node's border (selection
   stays `FLOW_BOLD`, hover stays ◉ handle-reveal); Enter-only activation (Space stays
   sticky-pan); frame only FULLY-offscreen nodes.

## Packages

### 1. Minimap visible-bounds guard  `[S]`  ·  id: `minimap-visible-bounds`

**Goal.** Fix the inc-4 deferral: the minimap world-window guard in `flow__minimap` tests `f->nnodes` (`src/flow_render.h:52`) — the TOTAL node count — instead of a VISIBLE bound. When every node is hidden but `nnodes > 0`, the guard takes the union branch and unions the screen viewport with `flow_bounds(f)`'s zero rect `{0,0,0,0}`, stretching the world window `W` and mis-scaling the drawn viewport rectangle. Replace the `nnodes` predicate with a test on `flow_bounds`'s own (visible-only) return, so an all-hidden graph falls to the `: vp` branch exactly like an empty one.

**User value.** With the minimap on and the whole graph hidden (a legitimate transient state — bulk-hide a selection, or a filter that hides everything), panning away from the origin currently draws a mis-scaled viewport rectangle inside the minimap panel (in the tested pan(40,40) case the rect is vertically compressed because the union inflates the world-window height; with a diagonal pan past both frame dimensions it shrinks on both axes). The fix makes the viewport rect track the real screen window, so the minimap stays a faithful overview in every visibility state, not just when at least one node is showing.

**Files touched.**
  - src/flow_render.h
  - tests/test_render.c
  - tests/snapshots/render_minimap_all_hidden.txt (NEW golden — staged in this commit)
  - Makefile (NOT touched — `test_render` already in `TESTS=`, `Makefile:4`)

**Entry points (existing functions to extend).**
  - `flow__minimap` world-window guard (`src/flow_render.h:52`) — the line `flow_rect W = f->nnodes ? flow_rect_union(flow_bounds(f), vp) : vp;`. Replace the `f->nnodes` predicate; compute `flow_bounds(f)` ONCE and branch on its dimensions.
  - `flow_bounds` (`src/flow_model.h:700-710`) — already returns the union of VISIBLE node rects and a zero rect `{0,0,0,0}` when the graph is empty OR fully hidden (the `seeded` flag stays 0 at `src/flow_model.h:703-708`). Its return is the correct discriminator; reuse it, do not recount.
  - the node-dot loop (`src/flow_render.h:61-71`) — already skips hidden nodes via `flow__node_visible` (`src/flow_render.h:63`). NO edit: the dots are already correct; only the `W` guard is wrong.

**API additions.**
```c
/* No new public API — a 2-line internal change to flow__minimap (src/flow_render.h:52).
   flow_bounds (src/flow_model.h:700) is already exported and already visible-only. */
```

**Design notes.**

*Predicate.* Use `flow_rect b = flow_bounds(f); flow_rect W = (b.w > 0 || b.h > 0) ? flow_rect_union(b, vp) : vp;` — computing `b` once (the current line calls `flow_bounds(f)` inline inside the union, so this is also one fewer traversal). `(b.w > 0 || b.h > 0)` is the exact "is there a visible footprint" test: `flow_bounds` returns `{0,0,0,0}` (both dims 0) in precisely the empty-OR-all-hidden case and a positive-area rect otherwise (`src/flow_model.h:700-710`). This is chosen over a fresh visible-node recount because `flow_bounds` already IS that recount — reusing its return is the layering-correct seam (the brief's reuse note) and avoids a second loop over `f->nnodes`.

*Why the bug bites.* `flow_rect_union` (`src/flow_geom.h:35-41`) has no empty-rect short-circuit: it takes `min(x0)`/`max(x1)` componentwise, so unioning `{0,0,0,0}` with a panned viewport treats the zero rect as a point AT the world origin. For the test setup — `flow_unproject` (`src/flow_geom.h:20-26`) gives `vp = {-40,-40,40,12}` under `pan(40,40)` — `union({0,0,0,0}, {-40,-40,40,12}) = {-40,-40,40,40}`: `W.x`/`W.y` are UNCHANGED at `-40` (the viewport already extends left/up of the origin), and `W.w` is UNCHANGED at `40` (vp's right edge sits exactly on the world origin, `x=0`, so the origin point adds nothing horizontally). Only `W.h` inflates (`12 -> 40`), because vp's bottom edge sits below the world origin. The viewport-rectangle cells `vx/vy/vx2/vy2` (`src/flow_render.h:55-56`) are then computed against that taller `W`, so the rect is vertically COMPRESSED (its bottom edge maps short of the interior bottom) — not mislocated, not shrunk on both axes here. (General case: a pan with `px > cols` AND `py > rows` would also inflate `W.w` and pull both origins, shrinking on both axes; the chosen `pan(40,40)` inflates only the height, which the bottom-row asserts target.) The fix makes `W == vp` in the all-hidden case, so the viewport rect maps to the full minimap interior.

*RED technique — behavioral, NOT compile-RED.* The pinned discipline prefers compile-RED for snapshot packages, but this package adds NO public API, so there is nothing not-yet-existing for a test to reference; compile-RED is inapplicable here. Worse, a snapshot-ONLY test is unsafe: `flowtest_snapshot` (`tests/flowtest.h:20-28`) CREATES the golden from whatever output it sees on first run and PASSES — run red-first against the buggy code it would bake in the BUGGY frame and then go RED on the fix, inverting the signal. So this package mirrors the existing `render_hidden` precedent (`tests/test_render.c:349-352`), which pairs `strchr` guards with its SNAPSHOT precisely "so even the snapshot-creation run proves the skip": a MECHANICAL assert is the red-first discriminator, and the new golden is created AFTER the fix and staged for visual lock only.

*The mechanical discriminator.* With `pan(40,40)` on the 40-wide frame, the buggy `W = {-40,-40,40,40}` and the fixed `W = vp = {-40,-40,40,12}` share `W.w = 40` and the entire TOP edge (`vx = 0`, `vy = 0`, `vx2 = (-1+40)*10/40 = 9 = iw-1`). They differ ONLY in `vy2`: buggy `W.h = 40` gives `vy2 = (12-1)*4/40 = 1`; fixed `W.h = 12` gives `vy2 = (12-1)*4/12 = 3 = ih-1` (`src/flow_render.h:55-56`). So the top row and the upper part of the side verticals are NOT discriminators — only the BOTTOM edge and the LOWER part of the side verticals exist solely in the fixed rect. The fixed rect reaches the interior bottom row (`1+vy2 = interior row ih-1`) and its left vertical descends to that row; the buggy rect stops two interior rows short. Asserting on those lower cells is RED before the fix, GREEN after.

*Test setup reproduces the bug.* The clamp `flow__clamp_view_offset` (`src/flow_model.h:520-528`) early-returns when `translate_extent` is unset (`e.w <= 0`, the zero-init default, `src/flow_model.h:522`), so `flow_pan` (routes through `flow__view_set`, `src/flow_model.h:774`/`538`) with no extent freely yields nonzero `ox/oy` — the panned-away state is reachable.

*Golden safety.* `(b.w > 0 || b.h > 0)` differs from `f->nnodes` ONLY in the all-hidden case. Every existing minimap test keeps ≥1 node visible — the two-node minimap (`tests/test_render.c:44-55`) and the C-hidden test where A and B stay visible (`tests/test_render.c:360-367`) — so `flow_bounds` is nonzero there, the union branch is taken identically, and every existing render golden (including the cols=30 `render_statusbar` and the zoom==1 minimap-asserts constraint flagged at `src/flow_render.h:47-48`) stays byte-identical. Only `render_minimap_all_hidden` is new.

**Test plan.**
  - Extend `tests/test_render.c` (already in `TESTS=`): add a block after the existing hidden block (`tests/test_render.c:337-369`). Use a minimap-sized frame, not the 30x8 default (minimap needs `bw,bh >= 4`, `src/flow_render.h:30`):
    1. `flow_t *am = flow_new(40, 12); flow_register_defaults(am);` and a buffer `40*12`.
    2. Add two nodes near the origin, e.g. `na = flow_add_node(am, "default", (flow_pt){0,0}, "A")`, `nb = flow_add_node(am, "default", (flow_pt){8,4}, "B")`.
    3. `flow_set_minimap(am, 1, FLOW_CORNER_BR, 12, 6);` (so `bw=12, bh=6, iw=10, ih=4`).
    4. Hide BOTH: `flow_set_node_hidden(am, na, 1); flow_set_node_hidden(am, nb, 1);`.
    5. `flow_pan(am, 40, 40);` (pan far away; no extent set, so it sticks — `src/flow_model.h:522`).
    6. `flow_render(am, amb, 40, 12);`.
    7. Compute the minimap interior: BR corner at `ox=40-12=28, oy=12-6=6`; interior spans screen `x=29..38` (`iw=10`), `y=7..10` (`ih=4`). After the fix `vy2 = ih-1 = 3`, so the viewport rect's bottom edge lands on interior row `y = oy+1+vy2 = 10`. ASSERT a NON-corner cell of the bottom interior row carries the horizontal line glyph: `ASSERT_INT(amb[10*40 + 33].ch, 0x2500, "vp bottom line at interior")` (`x=33` is interior col 4, between the `x=29`/`x=38` corners that the vertical loop overwrites with `0x2502`). This is the discriminator: the buggy rect (`vy2=1`) leaves screen `y=10` blank.
    8. ASSERT the left interior-column carries the vertical line glyph DOWN to the lower interior: `ASSERT_INT(amb[9*40 + 29].ch, 0x2502, "vp left line reaches lower interior")` (`x=29` is `1+vx` projected by the surface origin, `y=9` is surface row 3). The fixed vertical spans `y=7..10` so it reaches `y=9`; the buggy vertical (`vy2=1`) stops at `y=8`, leaving `y=9` blank. RED against the buggy `W`, GREEN after the fix. (Do NOT assert the top row or the corner cells `x=29`/`x=38` of any row: the top row is identical in both rects, and the vertical loop at `src/flow_render.h:60` overwrites the horizontal-line corners with `0x2502` regardless.)
    9. ASSERT zero node dots (both hidden): `int dots=0; for (...) if (ch==0x2022||ch==0x25C9) dots++; ASSERT_INT(dots, 0, "all-hidden minimap draws no node dots")` — guards that the dot loop's hidden-skip (`src/flow_render.h:63`) is intact (this is a guard, not a buggy-vs-fixed discriminator: dot count is 0 in both).
    10. `SNAPSHOT("render_minimap_all_hidden", s);` where `s = cells_to_string(amb, 40, 12)` — the golden is created from POST-FIX output and committed.
    11. `free(...); flow_free(am);`.
  - Run RED first: the mechanical asserts in steps 7-8 fail against current `main` (before editing `flow__minimap`) — they target the bottom row / lower vertical that the buggy compressed rect omits. Apply the fix; they pass; then the SNAPSHOT line creates `render_minimap_all_hidden.txt`.
  - Whole-suite ASan/UBSan: the change is a value-only swap (no new allocation/index); the new test allocs are freed.

**Acceptance.**
  - `make test` passes, including the new mechanical asserts (steps 7-9) and the new snapshot.
  - All-hidden + panned minimap: the viewport rectangle reaches the full minimap interior (`0x2500` on the bottom interior row, `0x2502` down to the lower-left interior — the cells the buggy compressed rect omits), no node dots.
  - `render_minimap_all_hidden.txt` is created from post-fix output and STAGED in this commit (new golden).
  - Every EXISTING golden in `tests/snapshots/` is byte-identical — verified by the predicate-equivalence argument (the change differs from `nnodes` only when all nodes are hidden, which no existing minimap test exercises).
  - Whole-suite ASan and UBSan clean.
  - `flow.h` regenerated via `tools/amalgamate.sh` (edit only `src/flow_render.h`; no module added, no `modules=` change).
  - Demos untouched → no `make demos` warning surface changes; statusbar golden untouched (no new binding → this package APPENDS no hint, consciously skipping per the pinned statusbar rule).

**Depends on.** nothing — the visible-only `flow_bounds` semantics it reuses already landed (inc-4 #11, `src/flow_model.h:700-710`).

**Conflicts with.**
  - `viewport-culling` — also edits `src/flow_render.h` (the render loop). Different region (its work is in the node/edge draw loop, not `flow__minimap`); same-file, trivial rebase.
  - `helper-lines` — if it adds render output in `src/flow_render.h`. Different region (`flow__minimap` vs. the helper-line draw path); same-file, trivial rebase.
  - `marquee-world-anchor` — if its marquee-rect work touches `src/flow_render.h`. Different region; same-file, trivial rebase.
  - No conflict with any package on the dispatch/feed key path or struct flow's zero-init block — this package edits neither.

**Carry-overs fixed.**
  - Closes inc-4 deferral "minimap all-hidden world-window guard": the minimap viewport rectangle is now correct in the empty, fully-hidden, and partially-hidden cases uniformly, completing the view-level hidden-skip contract for the minimap (the dot loop was already correct; the guard now matches it).

---

### 2. Validator suspension during flow_load  `[S]`  ·  id: `validator-load-suspend`

**Goal.** Suspend the connection validator across `flow_load`'s edge-rebuild loop so a validator left set on the engine cannot silently drop edges that already exist in the saved file. `flow_load` rebuilds every edge through `flow_add_edge` (`src/flow_json.h:369`), whose engine gate (`src/flow_model.h:665`) re-runs the user's validator on each loaded edge; a reject returns `-1` and the loop's `continue` (`src/flow_json.h:370`) drops it. Save `validator_fn`/`validator_user`, NULL the fn before the edges loop, restore after — bracketing the rebuild exactly as the existing `f->journal.suppress++/--` bracket (`src/flow_json.h:302`, `:390`) already does for journaling.

**User value.** Loading a saved graph must reproduce it faithfully. Today, an app that installs a validator (e.g. "no cross-layer edges") and then calls `flow_load` gets a corrupted load — any saved edge the validator would reject vanishes with no error, no callback, and `flow_load` still returns `0`. The documented workaround is to manually clear the validator before every load and reinstall it after (`src/flow_model.h:125-127`); this package removes that footgun so a load round-trips regardless of validator state.

**Files touched.**
  - src/flow_json.h
  - src/flow_model.h (comment only — the transient-validator contract at lines 124-127)
  - tests/test_json.c
  - Makefile (not touched — test_json already in TESTS list at `Makefile:4`)

**Entry points (existing functions to extend).**
  - `flow_load` (`src/flow_json.h:279`) — add a validator save/NULL/restore bracket inside the post-validity rebuild section, paired with the existing journaling bracket
  - `f->journal.suppress++` (`src/flow_json.h:302`) and `f->journal.suppress--` (`src/flow_json.h:390`) — the precedent bracket; the new validator save/restore parallels these two sites exactly, so both live strictly inside the rebuild section after the parse-validity gate at `src/flow_json.h:296`
  - `f->validator_fn` / `f->validator_user` (`src/flow_model.h:266`) — the two scalar fields saved, NULLed, and restored
  - the validator gate in `flow_add_edge` (`src/flow_model.h:665`) — short-circuits to allow-all once `f->validator_fn` is NULL; no edit, the load just makes its guard false
  - `flow_connection_validator` typedef comment (`src/flow_model.h:124-127`) — the documented TRANSIENT contract, amended to the new behavior

**API additions.**
```c
/* No new public API. flow_load's signature is unchanged; the suspension is an internal
   save/NULL/restore of the existing f->validator_fn / f->validator_user fields
   (src/flow_model.h:266). No new struct flow field, no new flag, no new setter. */
```

**Design notes.**

Mechanism: **(a) save/NULL/restore the validator fields**, not (b) a gate-consulted suspend flag. Rationale: (a) mirrors the journaling bracket already proven correct in this exact function (`f->journal.suppress++/--` at `src/flow_json.h:302`/`:390`), adds zero `struct flow` state, and localizes the entire change to `flow_load`. Mechanism (b) would add a field to the zero-init block, edit BOTH gate sites (`src/flow_model.h:665` and the reconnect gate at `:891`) to consult it, and pay a per-call branch forever for a concern that is purely load-scoped — strictly worse cost for an identical observable outcome. The pinned GATES-vs-OBSERVERS rule keeps the validator on `struct flow` with a setter (`flow_set_connection_validator`, `src/flow_model.h:1016`); (a) touches those same fields locally and respects that placement.

Leak-safety of the early-return paths: the suspension bracket sits entirely AFTER the parse-validity gate at `src/flow_json.h:296`. Every failure path that returns before the rebuild — open/seek/read failures (`src/flow_json.h:283-289`) and the structure-invalid return (`src/flow_json.h:296`) — returns BEFORE both the save and the NULL, so `validator_fn` is never disturbed on those paths. Between the save and the restore (`f->journal.suppress--`/`free(buf)`/`return 0` at `src/flow_json.h:390-392`) there is no early `return` and no allocation that can fail-and-bail, so the restore is unconditionally reached on the success path — identical control-flow guarantees to the journaling bracket it parallels. The save reads the LIVE fields rather than assuming NULL, so restoring re-establishes whatever validator the app had installed.

Scope: only `flow_add_edge` (`src/flow_model.h:656`) is called during load, so NULLing the fn covers the whole rebuild. `flow__graph_reset` (`src/flow_model.h:467`) simply never touches `validator_fn`/`validator_user` — the comment at `src/flow_model.h:463-464` ("Leaves view/types/cb/widgets intact") covers view/types/cb/widgets, and the validator (a separate `struct flow` field, NOT `cb`, per the pinned GATES-vs-OBSERVERS rule) survives the reset by omission — so the validator outlives the reset and must be restored by `flow_load` itself, not by the reset. The reconnect gate (`src/flow_model.h:891`) is NOT exercised by load (load only adds, never reconnects) and is left untouched — its validator contract is unchanged and a post-load `flow_reconnect_edge` still gates normally.

Structural-reject ordering (drives the test design): both gates run the user validator LAST, after the fast structural rejects — `flow_add_edge` is self (`src/flow_model.h:657`) → missing-node (`:658`) → duplicate (`:660-662`) → validator (`:665`); `flow_reconnect_edge` is missing (`:876`) → self (`:882`) → duplicate (`:883-888`) → validator (`:891`). So any assert meant to prove "the validator rejected this" only does so when the operation is otherwise structurally valid (distinct existing nodes, non-self, non-duplicate). The test steps below are constructed to reach the validator gate, not stop short at a structural reject.

Contract comment: `src/flow_model.h:125-127` currently reads that the validator is TRANSIENT and "a validator left set across a load re-gates every loaded edge and can silently drop them — clear it before (or set it after) flow_load." This is amended to state that `flow_load` now suspends the validator across the edge rebuild so a set validator does NOT affect a load, and that the validator remains installed and active for `flow_add_edge`/reconnect calls AFTER the load returns. The "Not persisted" sentence stays (the validator is still never written to JSON).

**Test plan.** Append one block to tests/test_json.c (before the final `return`; reuse the existing `/tmp/flow_json_*.json` path constants at `tests/test_json.c:65-69` and the `flow_set_connection_validator` API; match the file's `flow_edge_count`/`flow_node_count` accessor style). Makefile TESTS= unchanged (test_json already listed at `Makefile:4`).
  1. Build graph `a`: 3 nodes `n1,n2,n3`, edges `n1->n2` and `n2->n3`; `flow_save(a, P_A)` asserts `0`.
  2. Build a fresh `b` with two of its OWN real nodes `bn1,bn2`, install a reject-all validator via `flow_set_connection_validator(b, reject_all, NULL)` where `reject_all` returns `0` unconditionally; sanity-assert it is active and REACHES the gate: `flow_add_edge(b, bn1, bn2, "", "")` (distinct existing nodes, non-dup) returns `-1` and `flow_edge_count(b) == 0` BEFORE the load. (`bn1/bn2` are wiped by the load's `flow__graph_reset`, so this pre-population is harmless.)
  3. `flow_load(b, P_A)` asserts `0`.
  4. Assert `flow_edge_count(b) == 2` — ALL saved edges loaded despite the reject-all validator (the suspension worked; without the fix this is `0`).
  5. Assert `flow_node_count(b) == 3` (nodes unaffected — they never hit the validator gate).
  6. Assert the validator is STILL active after load: `flow_add_edge(b, n1, n3, "", "")` — `n1->n3` is non-self and non-duplicate (only `n1->n2`,`n2->n3` exist), so it reaches the gate at `src/flow_model.h:665` and the restored reject-all rejects it: returns `-1` and `flow_edge_count(b)` stays `2`.
  7. Reconnect-gate unaffected: find the loaded `n1->n2` edge id `eid` (`flow_get_edge`/search by source==n1,target==n2), then `flow_reconnect_edge(b, eid, n3, "", 0)` — reconnect the SOURCE (which=0) to `n3`, prospective `n3->n2`, which is non-self and non-duplicate (not in {`n1->n2`,`n2->n3`}), so it reaches the validator gate at `src/flow_model.h:891` and is rejected; assert the edge's source is UNCHANGED (`== n1`), proving the still-active validator gated the reconnect and load suspension did not bleed into the reconnect path.
  8. Early-return leak check (failed open): build a fresh `c` with two real nodes `cn1,cn2`, install reject-all, call `flow_load(c, "/tmp/does_not_exist_flow_xyz.json")` (assert `-1`, mirroring `tests/test_json.c:330`) — the nodes survive (failed-open returns at `src/flow_json.h:283`, before `flow__graph_reset` at `:301`). Then assert `flow_add_edge(c, cn1, cn2, "", "")` returns `-1` (reaches the gate at `:665`): a failed-open load left the validator untouched (never saved-then-not-restored).
  9. Early-return leak check (parse failure): the block writes its OWN truncated-garbage file (e.g. `{"version":1,"viewport":{"ox":12.5,"oy":-3` — mid-number, unbalanced, structure-invalid) so the test does not depend on the rewritten state of `P_GARB`; build a fresh `d` with two real nodes `dn1,dn2`, install reject-all, `flow_load(d, <own garbage path>)` (assert `-1`, the structure-invalid early return at `src/flow_json.h:296` is before `flow__graph_reset` at `:301`, so the nodes survive). Then assert `flow_add_edge(d, dn1, dn2, "", "")` returns `-1` (reaches `:665`) — the early return is before the bracket, so the validator is intact.
  10. ASan/UBSan clean: no leaks (the suspension touches only scalar fields; the loaded edges and their labels are freed by `flow_free`; the temp garbage file from step 9 is `fclose`d before load).

**Acceptance.**
  - `make test` passes including the new test_json block; existing test_json asserts unchanged.
  - With a reject-all validator installed, `flow_load` of a 2-edge file yields exactly 2 edges (was 0); the validator is verifiably active again after `flow_load` returns (gates a new structurally-valid `flow_add_edge`, step 6) AND the reconnect gate at `src/flow_model.h:891` still fires post-load (step 7 reaches it with a non-self/non-dup prospective edge).
  - Failed-open and parse-invalid loads leave `validator_fn`/`validator_user` byte-for-byte unchanged (the post-load `flow_add_edge` between surviving nodes still rejects — no save-without-restore on any early-return path).
  - Whole-suite ASan/UBSan clean.
  - Existing render/JSON goldens byte-identical — this package adds no golden and changes no serialized bytes (the validator is never written to JSON; save output is untouched).
  - flow.h regenerated via tools/amalgamate.sh (edits in src/flow_json.h + a comment in src/flow_model.h; no module added).
  - `make demos` warning-free is N/A (no demo touched).

**Depends on.** nothing (operates on the already-landed `flow_load`, `flow_add_edge` gate, and `validator_fn`/`validator_user` fields).

**Conflicts with.**
  - edge-events-inflight — if that package adds observer fires inside `flow_load`'s edge loop or alters `flow_add_edge`'s post-success path, the two share `src/flow_json.h`'s edge-rebuild region (`src/flow_json.h:356-385`) and `flow_add_edge` (`src/flow_model.h:656`); coordinate ordering so the validator-suspend save/restore brackets the whole loop OUTSIDE any added per-edge event logic.
  - Any package touching `flow_load`'s suppression brackets or `struct flow`'s zero-init field block around `validator_fn` (`src/flow_model.h:266`) — none other in this set does; this package adds no field, only locally saves/NULLs/restores existing ones.

**Carry-overs fixed.**
  - Resolves inc-4 deferral #4 (`deferral-validator-load`): `flow_load` no longer silently drops saved edges when a validator is installed; the documented manual clear-before/set-after workaround (`src/flow_model.h:125-127`) is retired and the contract comment updated to the suspend-during-load behavior.

---

### 3. World-stable marquee anchor  `[M]`  ·  id: `marquee-world-anchor`

**Goal.** Fix inc-4 deferral #1: make the shift-drag marquee selection world-stable under sustained edge-band auto-pan. Today the marquee anchor is stored and re-projected in SCREEN space, so when `flow__autopan` pans the view mid-drag the world rect TRANSLATES (chases the moving screen anchor) instead of GROWING — the selected set silently changes as the viewport scrolls. (At zoom 1 the rect WIDTH `|scr − anchor|` is pan-independent because both `wa` and `wc` subtract the SAME current `ox`/`oy`, while the rect ORIGIN `min(anchor,scr) − ox` slides as `ox` changes — translate, not grow.) Store a world-space anchor captured once at threshold-cross (`flow_to_world(down_pos)`), build the world rect from that fixed world anchor plus `flow_to_world(scr)` each frame, and re-project the world anchor back to screen for the live render box. This copies the world-delta pattern node-drag already uses (`drag_grab` / `drag_last_world`).

**User value.** A user can shift-drag a marquee toward a viewport edge, let auto-pan carry the view across a large graph, and have the selection keep growing to cover every node swept since the press — exactly as xyflow does. Under the landed behavior the early part of the sweep is silently dropped as the view scrolls, which is surprising and makes off-screen multi-select unreliable.

**Files touched.**
  - src/flow_model.h (one new zero-init field in the marquee state block)
  - src/flow_input.h (threshold-cross capture, motion-branch rect build, comment fix)
  - src/flow_render.h (re-project world anchor to screen for the live box)
  - tests/test_marquee.c (new world-stability subtest; helper fns duplicated locally — see Test plan)
  - tests/test_autopan.c (the two marquee subtests' comments/asserts stay valid — see Test plan)
  - Makefile (not touched — test_marquee and test_autopan are already in TESTS=, count stays 25)

**Entry points (existing functions to extend).**
  - marquee state block — `int marquee_active, marquee_on; flow_pt marquee_anchor, marquee_cur;` at `src/flow_model.h:263` (comment "marquee: armed intent / live; screen coords"): append a `flow_pt marquee_anchor_world` field here; this block is the zero-init append site and shares the calloc default with `marquee_anchor`.
  - threshold-cross arm — `flow_handle_mouse`, `src/flow_input.h:175` (`f->marquee_on = 1; f->marquee_anchor = f->down_pos;`): also capture `f->marquee_anchor_world = flow_to_world(f, f->down_pos);` once here, the same place node-drag captures `drag_grab` / `drag_last_world` (`src/flow_input.h:178-180`).
  - live marquee branch — `flow_handle_mouse`, `src/flow_input.h:189-199`: `flow__autopan(f, scr)` (`:193`) and `f->marquee_cur = scr` (`:194`) are unchanged; replace the anchor unprojection at `:195` (`flow_pt wa = flow_to_world(f, f->marquee_anchor)`) with the stored world anchor; the rect math at `:196-198` and `flow_select_in_rect(f, wr, ...)` at `:199` are otherwise untouched.
  - now-false comment — `src/flow_input.h:190-192` ("the rect below is recomputed from the NEW view, so the marquee chases the screen-coordinate anchor as the world scrolls and the selection tracks it"): this is the exact false claim the fix inverts; rewrite it to state the world anchor is fixed at threshold-cross so the rect GROWS under auto-pan.
  - live render box — `flow_render` marquee draw, `src/flow_render.h:256-258` (`int x0 = f->marquee_anchor.x ...`; comment at `:254-255` says "anchor/cur are SCREEN coords"): re-project the world anchor via `flow_to_screen(f, f->marquee_anchor_world)` each frame for `x0/y0` so the drawn border stays aligned after a pan; `marquee_cur` (`:257-258`) is already current screen and stays as-is.
  - projection seam — `flow_to_world` / `flow_to_screen` (decls `src/flow_model.h:78-79`, defs `:775-776`): inverse pair that round through `v.zoom` (`flow_project` / `flow_unproject`, `src/flow_geom.h:14-26`); both are used directly, no edit.
  - marquee-arm precondition — marquee only arms on a shift-drag of the EMPTY pane: `flow_hit_node` must return -1 AND the press carry `FLOW_MOD_SHIFT` (`src/flow_input.h:140,151-152`). So a node CANNOT sit under the press cell (it would be hit-tested and arm node-select instead) — the test geometry places swept nodes BESIDE the anchor, never under it.
  - release clears — `src/flow_input.h:254` (reconnect path) and `:329` (main release) already reset `marquee_active`/`marquee_on`; the new world field needs no explicit reset (it is only read while `marquee_on`, and `marquee_anchor` itself is never reset either — re-armed fresh each gesture at `:175`).

**API additions.**
```c
/* No new public API. One internal zero-init field (flow_pt marquee_anchor_world)
   appended to the marquee state block in struct flow at src/flow_model.h:263.
   Edits are confined to the marquee arm/motion branch and the live render box. */
```

**Design notes.**

NEW FIELD, NOT A REPLACEMENT. `marquee_anchor` (screen) is kept and `marquee_anchor_world` is added beside it. The screen anchor still records the press point and the world anchor is its one-time projection; keeping both avoids re-deriving screen from world (which would round twice through zoom) and keeps the render box's `marquee_cur` companion in the same screen namespace. The field lands in the `src/flow_model.h:263` block so the calloc-zero default applies and no init code is needed; it is only meaningful while `marquee_on`, which is armed (and the world anchor captured) together at `src/flow_input.h:175`.

CAPTURE ONCE AT THRESHOLD-CROSS, NOT PER-FRAME. The world anchor is set exactly where `marquee_on` flips to 1 (`src/flow_input.h:175`), before any auto-pan has run, so it pins the press point's world position under the original viewport. The motion branch then reads it verbatim every frame — never re-unprojects the screen anchor — so `flow__autopan` moving `f->view.ox/oy` no longer slides the anchor. This is the same shape as node-drag's `drag_grab` (captured once at `:178-179`, reused at `:230`): grab in world at arm-time, apply against the live world cursor each motion.

RECT GROWS BECAUSE ONLY ONE CORNER MOVES IN WORLD. With the world anchor fixed and `wc = flow_to_world(f, scr)` recomputed against the panned view, the post-pan cursor maps to a world point further from the fixed anchor, so the normalized rect at `src/flow_input.h:196-198` expands; `flow_select_in_rect` (`:199`) then re-selects the larger world region, restoring the swept-since-press set. (`flow_select_in_rect` runs with `additive=0` each frame, so a node stays selected only while it remains inside the CURRENT frame's rect — the fixed world anchor keeps the anchor-side corner covering it.) No change to the rect math or to `flow_select_in_rect` is needed — only the source of `wa` changes.

RENDER BOX RE-PROJECTS THE WORLD ANCHOR. The live box at `src/flow_render.h:256-258` must draw in screen space, but a stale screen `marquee_anchor` would now disagree with the world rect after a pan (the screen anchor MUST move as the view pans to track the grown rect). Project the fixed world anchor back through `flow_to_screen` each frame so the drawn border tracks the grown rect; this is the inverse of the capture and keeps the visual box aligned with the selection. `flow_to_screen` rounds through zoom identically to how `flow_to_world` rounded on capture, so at zoom≠1 the round-trip can differ from the raw press cell by ±1 — acceptable for a 1-cell-glyph overlay and asserted in the test plan.

ZOOM-SAFE BY CONSTRUCTION. Both projections go through `flow_project` / `flow_unproject` (`src/flow_geom.h:14-26`), which scale by `v.zoom` (guarding `zoom==0` in `flow_unproject` at `:21`). Capturing in world and re-projecting in screen is exactly the round-trip xyflow uses; the test plan keeps `zoom != 1` in at least one assert so the reprojection rounding is exercised, not bypassed by an identity transform.

**Test plan.**
  - tests/test_marquee.c — add a new "WORLD-STABLE ANCHOR (inc-5 deferral #1)" subtest (this file already asserts selection membership via `flow_get_node(f,a)->flags & FLOW_SELECTED`, lines 30-32/119-120, so it is the natural home; test_autopan.c asserts only on `org(f)` offset). The test `#define FLOW_IMPLEMENTATION` already in this file (`tests/test_marquee.c:1`) lets it poke `f->view.ox` / `f->marquee_anchor_world` directly. NOTE: test_marquee.c uses raw `flow_feed` SGR sequences (e.g. `tests/test_marquee.c:27`) and does NOT have the `press_shift_at` / `move_shift_to` / `org` helpers — those are `static` in test_autopan.c (`tests/test_autopan.c:13-22`), not in `flowtest.h`. So this subtest either (a) drives the gesture with raw `flow_feed` SGR (press shift = `\x1b[<4;sx;syM`, motion shift = `\x1b[<36;sx;syM`, release = `\x1b[<4;sx;sym`, all 1-based), or (b) duplicates the ~4 helper fns locally at the top of test_marquee.c; pick (b) for readability and read `f->view.ox` directly for the pan-precondition assert (no `org` needed). Coordinates below assume a fresh `flow_new(80,24)` (calloc → `view.ox=view.oy=0`, `view.zoom=1`, `src/flow_model.h:152-153`), so at arm-time screen == world; margin=3 / speed=2 (`src/flow_input.h:60`), right band `scr.x>=77`, bottom band `scr.y>=21`, each in-band motion pans `(-2,-2)` (`src/flow_input.h:61-63`):
    1. `flow_new(80, 24)`, `flow_register_defaults`; add node A at world `(52, 12)` (a 5×3 rect x52..56 y12..14 — just BESIDE the anchor on the grow side, NOT under the press cell, which would arm node-select not marquee) and node Far at world `(95, 30)` (off-screen bottom-right at press time; NOT hidden so it stays rect-eligible — enters the grown rect only after sustained pan).
    2. Arm the marquee with a shift-press on EMPTY pane at screen `(50,10)` (anchor world = `(50,10)`) and a shift-move to `(79,22)` (right+bottom bands); assert `f->marquee_on == 1` and that `f->view.ox` / `f->view.oy` DID change from 0 (precondition: auto-pan fired; each in-band step is `-2`).
    3. Drive several more shift-moves to `(79,22)` in the band to accumulate pan steps (≈8 to bring `79+2k >= 95`), then assert `flow_get_node(f, A)->flags & FLOW_SELECTED` — the node beside the ORIGINAL anchor's world position is STILL selected (rect grew, did not translate off it). This is the inverted contract: under the old screen-anchor code A would drop out once the view scrolled the rect origin past world x=52.
    4. Assert `flow_selected_count(f) >= 2` via `ASSERT(flow_selected_count(f) >= 2, "...")` (there is no `>=` ASSERT_INT macro in flowtest.h — use plain `ASSERT`) — both A and Far now inside the grown world rect, proving the rect expanded across the pan rather than sliding.
    5. ZOOM≠1 assert: rebuild fresh, `flow_set_zoom(f, 2.0f, (flow_pt){40, 12})` (the setter is 3-arg `flow_set_zoom(flow_t*, float, flow_pt screen_center)`, `src/flow_view.h:2,17` — NOT one-arg), repeat a short armed sweep, and assert that `flow_to_screen(f, f->marquee_anchor_world)` lands within ±1 cell of the original press cell AND that the swept node stays selected — exercises the world↔screen round-trip through zoom for both the rect and the render box.
    6. Release at an interior cell; assert `f->marquee_on == 0` and `flow_selected_count(f)` is unchanged by the release (finalize only clears state, `src/flow_input.h:323-324,329`).
  - tests/test_autopan.c — the two marquee subtests at `tests/test_autopan.c:131-142` ("marquee near edge DOES auto-pan") and `:144-158` ("marquee pan stability") assert on `org(f)` (the viewport offset) and on the pan delta, NOT on selection membership; the fix changes which nodes are selected, not whether/how far the view pans, so both subtests stay byte-identical and continue to pass. No edit; they remain the auto-pan-fires guard while test_marquee.c owns the new world-stability guard.
  - `make test` green; whole-suite ASan/UBSan clean (the new field is POD, the only new allocation path is the unchanged `flow_select_in_rect`).
  - Makefile TESTS= unchanged (count stays 25); no new golden (the marquee box is an interactive overlay, not a snapshot-locked render).

**Acceptance.**
  - After sustained edge-band auto-pan during a shift-drag marquee, a node BESIDE the press point's original world anchor remains in the selection (rect GREW), and at least one node first reachable only after the pan is also selected — verified by the new test_marquee.c subtest.
  - At `zoom != 1`, `flow_to_screen(f, f->marquee_anchor_world)` round-trips to within ±1 cell of the original press cell and the swept node stays selected.
  - The two test_autopan.c marquee subtests (`tests/test_autopan.c:131-158`) still pass byte-for-byte (auto-pan still fires and is stable); the false comment at `src/flow_input.h:190-192` is rewritten to describe the grow-not-translate behavior.
  - `make test` passes (whole suite); ASan and UBSan whole-suite gates clean.
  - Existing goldens byte-identical (no render golden touched; the live marquee box is not snapshot-locked); no new golden staged.
  - flow.h regenerated via tools/amalgamate.sh (edits in flow_model / flow_input / flow_render modules, all already in modules=); demos build warning-free (no demo touched).

**Depends on.** Nothing hard. Logically follows `marquee-autopan` (inc-4 #2), which introduced the auto-pan-during-marquee path this package corrects — but that work is already landed on main, so there is no in-increment dependency.

**Conflicts with.**
  - `viewport-culling` — both touch src/flow_render.h's render loop; this package edits only the marquee-box block (`src/flow_render.h:256-258`), culling edits the node/edge draw loops, so the edits are disjoint but land in the same file: rebase-coordinate, keep the marquee-box edit isolated.
  - `keyboard-move` / `tab-focus` / `copy-paste` / `node-search` — any package editing src/flow_input.h's key paths or the `struct flow` zero-init field block shares those files with this package's new `marquee_anchor_world` field (`src/flow_model.h:263`) and the motion-branch edit (`src/flow_input.h:175,189-199`). The field append is at the marquee block (not the key-registry block at `src/flow_model.h:271`) and the input edit is in `flow_handle_mouse` (not `flow_dispatch_key` / `flow_feed`), so conflicts are textual-only; resolve by landing the field append independently of any key-path edit.
  - `helper-lines` — STRONGEST mutual overlap *(post-execution correction; an earlier draft disclaimed it)*: #8 appends its `helper_on`/guide fields at the SAME `struct flow` marquee zero-init block this package's `marquee_anchor_world` lands in, AND both edit `flow_input.h` motion branches (this package the marquee branch, #8 the single-node drag branch). Land sequentially; the later rebases the field order.
  - `edge-events-inflight` — same-file `flow_input.h` line-shift risk *(post-execution correction)*: #11 edits the press branch and `flow__resolve_connection_at` while this package edits the motion branch; disjoint hunks, trivial rebase.
  - No gate, callback, flag, viewport-write (`flow__view_set`), or query seam is touched, so no conflict with `minimap-visible-bounds`, `validator-load-suspend`, or `view-frame`.

**Carry-overs fixed.**
  - Resolves inc-4 deferral #1 ("marquee world-stability"): the inc-4 §2 design notes and acceptance (`docs/superpowers/plans/2026-06-04-flow-increment-4-workpackages.md:168,182`) claimed the screen-pinned anchor kept the selection world-stable; that claim was false (caught in inc-4 review, recorded as a deliberate deferral in the inc-5 handoff). This package makes the claim true by world-pinning the anchor, and corrects the now-false in-code comment at `src/flow_input.h:190-192`.

---

### 4. Framing primitives: subset bounds, fit-bounds, set-center  `[M]`  ·  id: `view-frame`

**Goal.** Add the three framing primitives the rest of increment-5 builds on: `flow_bounds_of(f, ids, n)` (bounds of an explicit node subset), `flow_fit_bounds(f, r, margin)` (the standalone rect-arg fit, factored out of `flow_fit_view`), and `flow_set_center(f, wx, wy, zoom)` (pan a world point to the screen centre, optionally re-zooming). All three are pure framing — no model mutation, no journaling — and every viewport write routes through the `flow__view_set` seam (`src/flow_model.h:538`).

**User value.** Today the only framing entry point is `flow_fit_view(f, margin)` (`src/flow_model.h:1000`), which can only frame the whole graph. Apps that want to focus a selection, jump to a search hit, or pan to a known coordinate have to reach into `f->view` and re-derive the clamp/center math by hand. These primitives expose the same getViewportForBounds math `flow_fit_view` already uses, but parameterized by a caller-chosen subset, rect, or center — the foundation `tab-focus` (frame-on-focus) and `node-search` (frame the match) consume directly. *(Post-execution correction: an earlier draft named `minimap-visible-bounds` "drag-to-center" here — a phantom; #1 is a 2-line guard fix that lands first and calls none of this API.)*

**Files touched.**
  - src/flow_model.h
  - src/flow_view.h
  - tests/test_viewport_events.c
  - Makefile (not touched — `test_viewport_events` already in TESTS, `Makefile:4`)

**Entry points (existing functions to extend).**
  - `flow_fit_view` (`src/flow_model.h:1000-1015`) — refactor its zoom→clamp→center→`flow__view_set` tail (lines 1008-1014) into a shared static helper `flow__fit_rect`; the body becomes its `nnodes==0` guard + `flow_bounds(f)` + the helper. Behavior stays byte-identical (the comment at `src/flow_model.h:1004` already promises "one editable definition").
  - `flow_bounds` (`src/flow_model.h:700-710`) — `flow_bounds_of` is its explicit-subset sibling: same `flow_node_rect_abs` (`src/flow_model.h:687`) + `flow_rect_union` (`src/flow_geom.h:11`) union, but iterating caller ids and WITHOUT the `flow__node_visible` skip at `src/flow_model.h:705` (see Design notes).
  - `flow__view_set` (`src/flow_model.h:538-544`) — the single viewport-commit seam; clamps the OFFSET (`flow__clamp_view_offset`) then fires `on_viewport_change` iff the viewport actually changed. `flow_set_center` and `flow__fit_rect` both terminate here. Note it clamps offset only — NOT zoom (see Design notes / Trap).
  - `flow_set_zoom` (`src/flow_view.h:17-28`) — placement precedent and the `z0 = view.zoom==0 ? 1.0f : view.zoom` lazy-1.0 guard at `src/flow_view.h:18` that `flow_set_center` mirrors; `flow_set_center` lands beside it in the view.h mutator block. The seam is already reachable from view.h (`flow_set_zoom` calls it at `src/flow_view.h:26`).
  - `flow_intersecting_nodes` / `flow_node_intersections` (`src/flow_query.h:20-21`) — fill-buffer query family whose robustness conventions `flow_bounds_of` borrows (`NULL`/`max==0` legal; "missing id -> 0", i.e. skip).

**API additions.**
```c
/* src/flow_model.h (public decl block near flow_fit_view, src/flow_model.h:145):
   subset bounds + standalone rect-fit live in model.h because both read node data. */
flow_rect flow_bounds_of(flow_t *f, const int *ids, int n);
  /* union of the absolute rects of the n nodes named in ids[]; INCLUDES hidden nodes
     (explicit-id query — see Design notes). Missing ids are skipped. Returns the zero
     rect {0,0,0,0} when n<=0, ids==NULL, or no named id resolves. */
void flow_fit_bounds(flow_t *f, flow_rect r, int margin);
  /* frame world rect r with `margin` cells of padding per side, zoom clamped to
     [zmin,zmax]; no-op when r.w<=0 || r.h<=0. Shares flow_fit_view's math. */

/* src/flow_view.h (beside the zoom mutators, src/flow_view.h:2): pure viewport transform. */
void flow_set_center(flow_t *f, int wx, int wy, float zoom);
  /* pan so world point (wx,wy) lands on the screen centre; zoom>0 re-zooms (clamped to
     [zmin,zmax]) before centering, zoom<=0 keeps the current zoom. Routes through the seam. */
```

**Design notes.**

`flow_bounds_of` INCLUDES hidden nodes — the deliberate divergence from `flow_bounds`. The LAYERING rule's "bounds = VIEW-level" applies to the implicit `flow_bounds()` choke point (no ids → "everything currently visible"), which is why it skips `FLOW_HIDDEN` at `src/flow_model.h:705`. `flow_bounds_of` takes EXPLICIT ids, which classifies it as a MODEL-level query (LAYERING: "queries are MODEL-level, hidden included"): the caller named those nodes deliberately, so the result must contain what they named even if hidden — matching the `flow_query.h` family. It borrows that family's robustness too: `n<=0`/`ids==NULL` → zero rect, and missing ids skip via `flow_get_node`→NULL (mirroring `flow_node_intersections`' "missing id -> 0", `src/flow_query.h:21`). `flow_node_rect_abs` (`src/flow_model.h:687`) returns ABSOLUTE coords, so a child id contributes its parent-offset rect for free — no group special-casing.

`flow_fit_bounds` FACTORS, it does not mirror. A new static `flow__fit_rect(flow_t *f, flow_rect b, int margin)` holds the zoom→clamp→center→`flow__view_set` tail currently inlined at `src/flow_model.h:1008-1014`. `flow_fit_view` becomes `nnodes==0` guard (`src/flow_model.h:1005`) + `flow_bounds(f)` + `b.w<=0||b.h<=0` guard (`src/flow_model.h:1007`) + `flow__fit_rect`. `flow_fit_bounds` becomes its own `r.w<=0||r.h<=0` guard + `flow__fit_rect`. One definition honors the `src/flow_model.h:1004` comment, and `flow_fit_view` stays byte-identical so the existing fit test (`tests/test_viewport_events.c:66-79`) still passes unchanged.

`flow_set_center` clamps ZOOM ITSELF before the seam. The seam (`src/flow_model.h:538-544`) clamps OFFSET only (`flow__clamp_view_offset`) and does NOT clamp zoom — every zoom-writing caller clamps first (`flow_fit_view` at `src/flow_model.h:1011-1012`, `flow_set_zoom` at `src/flow_view.h:24-25`). So `flow_set_center` resolves its effective zoom as: if `zoom<=0` keep `f->view.zoom` (sentinel = "keep current"); else clamp `zoom` to `[f->zmin, f->zmax]`. It then mirrors the `src/flow_view.h:18` lazy-1.0 guard (`z0 = view.zoom==0 ? 1.0f : view.zoom`) before computing `ox = cols/2.0f - wx*z`, `oy = rows/2.0f - wy*z` (FLOAT halves, mirroring `flow_fit_view`'s `f->cols/2.0f` at `src/flow_model.h:1013` — `flow_set_center` has its OWN centering code, it does NOT route through `flow__fit_rect`, so it must use float division or it drifts a half cell on odd cols/rows), and commits through `flow__view_set`. (On a fresh flow `view.zoom` is already 1 — `src/flow_model.h:447` — so the guard is belt-and-suspenders, matching precedent and costing nothing.) The "clamp first" contract still holds end-to-end: translate-extent clamping happens inside the seam after the centering offset is computed, exactly as in `flow_fit_view` (`src/flow_model.h:1013` comment "extent wins over centering").

Placement is split, grounded in the existing split. `flow_fit_view` lives in model.h because it reads node data via `flow_bounds`; `flow_set_zoom` lives in view.h because it is a pure viewport transform. Following that line: `flow_bounds_of` (pairs with `flow_bounds`) and `flow_fit_bounds` (shares the model.h `flow__fit_rect` helper) go in flow_model.h; `flow_set_center` (no node reads, pure pan/zoom) goes in flow_view.h beside the zoom family, where the seam is already in scope (`src/flow_view.h:26`).

Not journaled, no flag, no callbacks field, no statusbar hint. Framing is viewport state, which is never undo state (`tests/test_viewport_events.c:131`; spec §11) — no `flow__undo_begin/end`. No new flag (FLAGS rule: next free bit 32u stays free). The only event is the existing `on_viewport_change`, fired by the seam — no new `flow_callbacks` field (GATES vs OBSERVERS rule). This package binds no key, so it APPENDS no statusbar hint and the cols=30 `render_statusbar` golden stays byte-identical (STATUSBAR rule permits skipping).

**Test plan.**
  - Extend tests/test_viewport_events.c (the dedicated viewport-events suite; `ASSERT_F`/`fabsf` helper already at `tests/test_viewport_events.c:26`). The three new functions do not exist yet, so the suite is compile-RED until the implementation lands. Makefile TESTS= UNCHANGED — `test_viewport_events` is already listed (Makefile:4). New blocks:
    1. **set_center centers + fires once.** Fresh `flow_new(80,24)`; set `on_viewport_change` counter to 0; `flow_set_center(f, 100, 50, -1.0f)` (keep zoom=1). Assert `vp_fires==1`; assert `f->view.ox ≈ 40 - 100*1 == -60` and `f->view.oy ≈ 12 - 50*1 == -38` (via `ASSERT_F`/`fabsf`; no extent set, so `flow__clamp_view_offset` early-returns at `src/flow_model.h:522` and does not pin). Call `flow_set_center(f, 100, 50, -1.0f)` again → assert `vp_fires==1` (identical result, fires-iff-changed).
    2. **set_center explicit zoom clamps to zmax.** `flow_set_zoom_limits(f, 0.5f, 2.0f)`; `flow_set_center(f, 0, 0, 100.0f)`; assert `f->view.zoom ≈ 2.0f` (clamped, not 100) and `f->view.ox ≈ 40 - 0 == 40`.
    3. **set_center zoom<=0 keeps current.** `flow_zoom_in(f, …)` to a non-1 zoom `z`; `flow_set_center(f, 10, 10, 0.0f)`; assert `f->view.zoom ≈ z` (unchanged) and centering uses `z` (`ox ≈ 40 - 10*z`).
    4. **set_center clamp-first under extent.** `flow_set_translate_extent(f, (flow_rect){0,0,40,40})` (a degenerate axis like `tests/test_viewport_events.c:118`); reset counter; `flow_set_center` to a target whose computed offset the extent pins back; assert the delivered `vp_last.ox` is the PINNED value (clamp before compare), not the raw centering offset.
    5. **fit_bounds == fit_view for the all-node rect.** Add 3 nodes (reuse the `tests/test_viewport_events.c:69-71` fixture). Snapshot `flow_fit_view(f,2)` → record `view`. Reset view (re-`flow_new` + same nodes or pan away), then `flow_fit_bounds(f, flow_bounds(f), 2)`; assert resulting `ox`,`oy`,`zoom` each `ASSERT_F`-equal to the `flow_fit_view` snapshot (shared `flow__fit_rect` on the same rect ⇒ identical viewport).
    6. **fit_bounds zero-rect no-op.** Capture `view`; `flow_fit_bounds(f, (flow_rect){5,5,0,0}, 2)`; assert `vp_fires` unchanged and `view` byte-identical (`w<=0` guard). (Optional companion: a real-change `flow_fit_bounds` asserting `vp_fires==1`; otherwise the fires-on-change path for fit_bounds is covered transitively via the shared seam + set_center #1.)
    7. **bounds_of subset excludes non-listed.** Three nodes a,b,c at known positions; `int ids[1]={a}`; `flow_rect r = flow_bounds_of(f, ids, 1)`; assert `r` equals `flow_node_rect_abs` of a alone (does not span to b/c).
    8. **bounds_of INCLUDES a hidden id (the divergence assert).** `flow_set_node_hidden(f, b, 1)`; `int ids[2]={a,b}`; assert `flow_bounds_of(f, ids, 2)` spans both a and b — i.e. differs from `flow_bounds(f)` which would skip hidden b. This is the load-bearing test for the LAYERING decision.
    9. **bounds_of with a child (abs coords).** Make c a child of a parent at offset; `ids={c}`; assert the returned rect is c's ABSOLUTE rect (parent offset applied), equal to `flow_node_rect_abs(f, flow_get_node(f,c))`.
    10. **bounds_of degenerate.** Assert `flow_bounds_of(f, NULL, 0)`, `flow_bounds_of(f, ids, 0)`, and `flow_bounds_of(f, &missing, 1)` (id never added) each return the zero rect `{0,0,0,0}` with no crash.
    11. **not journaled.** After several `flow_set_center` / `flow_fit_bounds` calls, assert `f->journal.n` unchanged (mirrors `tests/test_viewport_events.c:131`).
  - Whole-suite ASan/UBSan clean: no OOB on the `ids[]` loop, no leak.

**Acceptance.**
  - `make test` passes including all new asserts; the suite is compile-RED before the implementation (tests reference `flow_bounds_of`/`flow_fit_bounds`/`flow_set_center`, none yet defined).
  - `flow_fit_view`'s existing test (`tests/test_viewport_events.c:66-79`) and the journal test (`:131`) still pass unchanged — the `flow__fit_rect` refactor is behavior-preserving.
  - Whole-suite ASan + UBSan build is clean (the `ids[]` subset loop is the new memory surface).
  - All EXISTING goldens are byte-identical — this package adds no golden and no statusbar hint (cols=30 `render_statusbar` golden untouched, `src/flow_render.h:285`).
  - `make demos` is warning-free (demos are not touched in this package; integration wiring is deferred).
  - flow.h regenerated from src/ via `tools/amalgamate.sh` (edits in `src/flow_model.h` and `src/flow_view.h`, both already in the `modules=` list); no new module.
  - One commit, no trailers.

**Depends on.** Nothing — view-frame is the foundation primitive. It uses only already-landed seams (`flow__view_set`, `flow_bounds`, `flow_node_rect_abs`, `flow_rect_union`, the `flow_set_zoom` clamp pattern).

**Conflicts with.**
  - `tab-focus` and `node-search` — the REAL downstream consumers *(post-execution correction: an earlier draft named #1/#9 here — phantoms; #1 calls none of this API and #9 only READS the view)*: #5's frame-on-focus calls `flow_set_center`, #10's Enter framing calls `flow_bounds_of`/`flow_set_center`. Land view-frame first — an ordering dependency, not a file conflict.
  - `marquee-world-anchor` and `helper-lines` — if either edits the flow_model.h viewport-projection / fit-bounds region or the flow_view.h mutator block, rebase against this package's `flow__fit_rect` extraction and the new `flow_set_center` in view.h. Mutual note: whichever lands second re-applies against the refactored `flow_fit_view` body.
  - `node-search` — binds `/` and frames its match via `flow_set_center`/`flow_fit_bounds`; coordinate so its demo-side framing call matches this signature.
  - LAYERING strain (flagged per the pinned preamble): `flow_bounds_of` leans on the cross-cutting rule's "queries are MODEL-level, hidden included" clause OVER its "bounds = VIEW-level" clause — it is the explicit-id bounds query that intentionally does NOT skip `FLOW_HIDDEN`. Resolved in Design notes (the implicit `flow_bounds()` stays view-level; `flow_bounds_of` is model-level like `flow_query.h`); test #8 is the load-bearing assert that pins the decision. Surfaced here so no later package re-litigates the divergence as a bug.

**Carry-overs fixed.**
  - Generalizes the single whole-graph `flow_fit_view` into reusable subset/rect/center framing, honoring the `src/flow_model.h:1004` "one editable definition" promise by factoring the shared math instead of copy-pasting it.

---

### 5. Tab focus traversal + focus ring + frame-on-focus  `[M]`  ·  id: `tab-focus`

**Goal.** Add keyboard focus traversal: an `int focus_node` id field on `struct flow` (sentinel `-1`), with Tab / Shift-Tab cycling the VISIBLE nodes in insertion order, a focus-ring render affordance on the focused node, Enter selecting it, lone-ESC additionally clearing focus, and an auto-frame (xyflow `autoPanOnNodeFocus`) that re-centres the viewport when the newly-focused node is fully offscreen. Focus is the keyboard analog of mouse hover: it lets a user drive the whole graph from the keyboard with no pointer.

**User value.** A user with no mouse (or who prefers the keyboard) can walk every node with Tab, see which node is "current" via the ring, press Enter to select it, and trust that focusing a node beyond the viewport brings it into view. This closes the last big keyboard gap for headless/SSH/tmux terminal sessions.

**Files touched.**
  - src/flow_model.h (struct field + zero-init, `flow_focus_next`/`flow_focus_prev`/`flow_set_focus`/`flow_focused_node`, Tab + Enter built-ins in `flow_dispatch_key`, focus invalidation in `flow_remove_node` and `flow_set_node_hidden`)
  - src/flow_run.h (Shift-Tab CSI `\x1b[Z` in `flow_feed`, lone-ESC focus clear)
  - src/flow_render.h (focus-ring post-pass + one appended statusbar hint)
  - tests/test_focus.c (new)
  - tests/snapshots/render_focus.txt (new golden)
  - Makefile (TESTS += test_focus — was 25, becomes 26)

**Entry points (existing functions to extend).**
  - `struct flow` field block (`src/flow_model.h:254-267`) — add `int focus_node;` alongside the other interaction-state ids (`drag_node`, `conn_node`, `reconnect_edge`); it is an id, NOT a flag (PINNED).
  - `flow_new` zero-init (`src/flow_model.h:445-455`) — add `f->focus_node = -1;` on the same line group that sets `f->drag_node = -1; ... f->conn_node = -1;` (`src/flow_model.h:449`). `calloc` zeroes it, so the explicit `-1` is the sentinel.
  - `flow_dispatch_key` built-in table (`src/flow_model.h:1071-1080`) — add a `seq[0] == '\t'` branch calling `flow_focus_next(f)` AND a `seq[0] == '\r'` branch selecting the focused node, both placed among the existing single-byte built-ins (`x`/`n`/`f`/`?`/`u`, lines 1072-1078) and before the unhandled fallthrough at line 1080. Enter is a single byte and must be registry-overridable, so it belongs here beside Tab — not in `flow_feed` (where today `\r` is an unhandled no-op).
  - `flow_feed` CSI switch (`src/flow_run.h:24-32`) — add `case 'Z': flow_focus_prev(f); i += 3; continue;` next to the arrow cases A/B/C/D (`src/flow_run.h:25-31`); Shift-Tab is `\x1b[Z`, a CSI that today falls to `default: break` (`src/flow_run.h:30`) and is then half-swallowed by the lone-ESC branch.
  - `flow_feed` lone-ESC branch (`src/flow_run.h:47`) — append `f->focus_node = -1;` to the existing idempotent ESC action (which already does `flow_cancel_connection(f); f->space_held = 0; flow_clear_selection(f);`).
  - `flow_remove_node` cleanup tail (`src/flow_model.h:961-968`) — after the node slot is removed, invalidate focus if the removed id (or a cascaded descendant) was focused. There is NO existing delete-time `drag_node` invalidation to mirror (`drag_node` is reset only at mouse press/release in `src/flow_input.h:100,141,253,328`), so this site is NEW; it parallels how the function already re-notifies selection at line 968.
  - `flow_set_node_hidden` (`src/flow_model.h:1019-1033`) — in the `hidden` branch (after `n->flags |= FLOW_HIDDEN`, line 1024), clear `f->focus_node` if `id == f->focus_node`, mirroring how that same branch already deselects a hidden node (lines 1025-1029).
  - Node render loop (`src/flow_render.h:201-217`) and handle-marker post-pass (`src/flow_render.h:219-232`) — the ring draws as a NEW post-pass after the node bodies, reading `f->focus_node` (the node renderer's `ctx.flags` carries only `n->flags`, `src/flow_render.h:213`, and focus is not a flag, so the renderer cannot draw it itself).
  - `flow_set_hover` / `flow_hovered_node` (`src/flow_model.h:1140-1147`) — the single-node-highlight precedent the focus accessors mirror (clear-others-then-set), but focus stores an id field instead of scanning a flag.
  - `flow__view_set` (`src/flow_model.h:538`) / `flow_set_center` (from package `view-frame`) — the framing seam frame-on-focus calls; all viewport writes route through `flow__view_set` (PINNED), and `flow_set_center` (package #4) wraps it.

**API additions.**
```c
/* focus traversal (keyboard analog of hover; id field, sentinel -1) */
int  flow_focused_node(flow_t *f);          /* current focused node id, or -1 */
void flow_set_focus(flow_t *f, int id);     /* focus a specific node (id<0 or hidden/absent => clears to -1); frames if offscreen */
void flow_focus_next(flow_t *f);            /* Tab: next VISIBLE node in insertion order, wrapping; frames if offscreen */
void flow_focus_prev(flow_t *f);            /* Shift-Tab: previous VISIBLE node, wrapping; frames if offscreen */
```

**Design notes.**

FOCUS IS AN ID, NOT A FLAG (PINNED). `int focus_node` lives in the interaction-state block (`src/flow_model.h:254-267`) beside `drag_node`/`conn_node`, init `-1` in `flow_new` (`src/flow_model.h:449`). Single value (one node focused at a time), so there is no per-node scan and no shared flag bit consumed — `flow_focused_node` is a one-line getter returning the field. This mirrors the hover contract (`flow_set_hover` clears others then sets one, `src/flow_model.h:1140-1142`) but stored as an id, which is exactly why it needs explicit invalidation on delete/hide (a flag would vanish with the node).

TRAVERSAL ORDER & VISIBILITY GATE. Tab/Shift-Tab walk `f->nodes` in insertion order (the array order `flow_nodes`/`flow_node_count` expose, `src/flow_model.h:677-679`), skipping `!flow__node_visible(f, n)` (`src/flow_model.h:692`) so hidden nodes are never focusable — same VIEW-level gate as render/hit/marquee (PINNED: focus is a view concern). `flow_focus_next` finds the current focus's array index, advances to the next visible node, and wraps; with focus `-1` it starts at the first visible node; with zero visible nodes it stays `-1`. `flow_focus_prev` is the mirror. Insertion order (not z-order) keeps the cycle stable across selection changes, matching the query layer's documented insertion-order contract.

ENTER SELECTS, REPLACE (not additive). Enter `\r` is a single-byte built-in in `flow_dispatch_key` (beside Tab, `src/flow_model.h:1072-1078`) that calls `flow_select_node(f, f->focus_node, /*additive=*/0)` (`src/flow_model.h:101,809`), which clears other selections then selects the focused node, firing the sig-gated `on_selection_change` (no new callback — PINNED). Rationale: xyflow's Enter/Space focus-activate selects the single focused element, and keyboard focus is inherently a single cursor, so replace matches the mental model (the focus ring IS the cursor; Enter "commits" it). Additive multi-select stays the domain of marquee/Shift-click. If `focus_node == -1`, Enter is a no-op (the branch returns 1 without mutating). Placing Enter in the dispatch table (not `flow_feed`) keeps it registry-overridable and matches the PARSE-SPLIT principle below; `\r` is currently an unhandled no-op in both `flow_feed` and the run loop, so nothing collides.

TAB vs SHIFT-TAB PARSE SPLIT. Tab is the single byte `\t`, not a CSI, so it belongs in `flow_dispatch_key` beside the other plain-letter built-ins (`x`/`n`/`f`, `src/flow_model.h:1072-1074`) — this also lets a demo `flow_bind_key("\t", ...)` override it via the registry's longest-match-first precedence (`src/flow_model.h:1059-1069`). Shift-Tab is the 3-byte CSI `\x1b[Z`; `flow_dispatch_key` has no CSI built-ins (its multibyte case is only Delete `\x1b[3~`, `src/flow_model.h:1071`), and the arrow CSIs are already handled in `flow_feed`'s switch (`src/flow_run.h:25-31`), so `case 'Z'` joins them there for consistency. This split is forced, not stylistic: today `\x1b[Z` reaches `default: break` (`src/flow_run.h:30`), is NOT caught by the lone-ESC branch (gated on `b[i+1] != '['`, `src/flow_run.h:47`), and the trailing `i++` (`src/flow_run.h:48`) consumes only the ESC byte, leaving `[Z` to mis-parse — the `case 'Z'` fixes that by consuming all 3 bytes.

FOCUS RING AS A RENDER POST-PASS. The ring draws AFTER node bodies (`src/flow_render.h:201-217`), as its own loop reading `f->focus_node` (gated by `flow__node_visible`), exactly like the handle-marker post-pass at `src/flow_render.h:219-232` and the marquee box at `:254-269` — because the node renderer only receives `ctx.flags = n->flags` (`src/flow_render.h:213`) and focus is not a flag, so the body renderer is blind to it. Affordance: re-stamp the focused node's border cells with the `FLOW_REVERSE` attr (the focused node's screen rect from `flow_node_rect_abs` + `flow_to_screen`, `src/flow_render.h:207-208`), giving an inverse-video outline distinct from selection's `FLOW_BOLD` (`src/flow_types.h:15`) and from hover's handle-reveal (the ◉ marker post-pass, `src/flow_render.h:219-232`). At LOD-collapse the rect is the 1x1 marker, so the ring reverses that single cell. This is a new golden (compile-RED): the test references the not-yet-built ring, so the suite can't run until the post-pass exists.

FRAME-ON-FOCUS (autoPanOnNodeFocus). When focus lands on a node whose screen rect is fully outside the viewport, re-centre via `flow_set_center` (package `view-frame`, which routes through `flow__view_set`, PINNED). Offscreen test uses the SCREEN rect: project the focused node via `flow_node_rect_abs` → `flow_to_screen` (the render projection, `src/flow_render.h:207-208`) and frame iff that rect does not intersect `[0,cols) x [0,rows)`. Screen-space (not a world-rect compare) is chosen because it already accounts for zoom/LOD the same way render does, so "looks offscreen" and "is framed" never disagree. A partially-visible node is left alone (no jump), matching xyflow's "only if not visible" guard. `flow_set_focus`/`_next`/`_prev` all run the same offscreen-frame check after updating `focus_node`.

STATUSBAR HINT (append-only, PINNED). The live help string (`src/flow_render.h:285`) gains one short hint, e.g. ` Tab:focus`, appended at the TAIL after `^r:redo`. The `render_statusbar` golden is rendered at cols=30 (`tests/snapshots/render_statusbar.txt`, line 6 ` n:add  x:del  f:fit  ?:help`) and locks only that 28-col prefix; the new hint lands far past column 30, so that golden stays byte-identical. No edit to the locked prefix.

UNDO. None of the focus operations mutate graph data (focus is transient view/interaction state, like hover and `drag_node`), so nothing journals and no `flow__undo_begin/end` bracket is needed. Enter's selection change is not journaled either: `flow_select_node` (`src/flow_model.h:809-817`) only flips `FLOW_SELECTED` flags and calls `flow__notify_selection` — verified no `flow__rec_push`/journal write — so selection isn't undoable in v1.

**Test plan.** New file tests/test_focus.c (compile-RED: it `#define FLOW_IMPLEMENTATION`s, includes the amalgamated `flow.h`, calls `flow_focus_next` etc. and asserts the new golden, so it cannot build/run until the implementation lands and `flow.h` is regenerated). Append `test_focus` to Makefile `TESTS=` (25 → 26).
  1. Build 3 nodes a,b,c (insertion order). Assert `flow_focused_node(f) == -1` initially.
  2. Feed `"\t"`; assert `flow_focused_node(f) == a` (Tab from -1 lands on first visible).
  3. Feed `"\t"` twice more; assert focus == b then c. Feed `"\t"` again; assert focus == a (wrap).
  4. Feed `"\x1b[Z"` (Shift-Tab); assert focus == c (prev wraps backward).
  5. Hide b via `flow_set_node_hidden(f, b, 1)`. From focus a, feed `"\t"`; assert focus == c (hidden b skipped).
  6. `flow_set_focus(f, b)` while b hidden; assert `flow_focused_node(f) == -1` (cannot focus a hidden node).
  7. Focus a, then `flow_remove_node(f, a)`; assert `flow_focused_node(f) == -1` (delete invalidates focus).
  8. Focus a, then `flow_set_node_hidden(f, a, 1)`; assert focus == -1 (hide invalidates focus).
  9. Enter selects: focus c, set an `on_selection_change` counter to 0, feed `"\r"`; assert `flow_selected_node(f) == c`, `flow_selected_count(f) == 1`, counter == 1. Pre-select b additively first, then Enter on c; assert only c selected (replace, not additive).
  10. ESC clears focus: focus c, feed `"\x1b"`; assert `flow_focused_node(f) == -1` (and selection still cleared per the esc-selection contract).
  11. Frame-on-focus: place node d far offscreen (world pos well outside the viewport), `flow_set_focus(f, d)`; assert the focused node's projected `flow_to_screen` rect now intersects `[0,cols) x [0,rows)`. Place a node already onscreen, capture `f->view.ox`/`oy` (floats, via `fabsf`), focus it; assert the viewport did NOT move (no jump for visible nodes).
  12. Ring golden: render a small scene with one focused node into a buffer, compare to tests/snapshots/render_focus.txt (the `flowtest_snapshot` helper auto-creates it on first run; the focused border shows the reverse-video ring; an unfocused render of the same scene must differ only in the focused node's border cells).
  13. ASan/UBSan: focus across delete/hide/wrap with no leaks or OOB (the array-index search must survive a node count of 0 and all-hidden).

**Acceptance.**
  - `flow_focused_node`, `flow_set_focus`, `flow_focus_next`, `flow_focus_prev` exported and behaving per the test plan.
  - Tab / Shift-Tab cycle visible nodes (insertion order, wrapping, skipping hidden); Enter selects the focused node (replace); lone-ESC clears focus; focus invalidated to -1 on delete and on hide of the focused node.
  - Focus ring renders as a post-pass over the focused node's border; `make` produces the new `render_focus.txt` golden (auto-created on first run via `flowtest_snapshot`), which is STAGED; the existing `render_statusbar.txt` golden is byte-identical (hint appended past col 30).
  - Frame-on-focus re-centres only fully-offscreen nodes; visible nodes cause no viewport change.
  - `make test` passes all 26 suites (test_focus added); whole-suite ASan/UBSan clean; `make demos` warning-free (demos touched only if a focus hint is wired demo-side — none required here).
  - flow.h regenerated via `tools/amalgamate.sh` (no new module; edits in flow_model/flow_render/flow_run only).
  - One commit, no trailers.

**Depends on.**
  - `view-frame` (package #4) — frame-on-focus calls its `flow_set_center` (which does not yet exist on main; the feature inventory confirms `setCenter` unlanded). If #4 lands the framing seam under a different name, this package binds to that name. Until #4 lands, the offscreen-frame call site is the only coupling; the traversal/ring/accessors are independent.

**Conflicts with.**
  - `copy-paste` — also binds plain-letter built-ins (y/c/p/d) in `flow_dispatch_key` (`src/flow_model.h:1071-1080`); tab-focus now occupies `\t` AND `\r` in that same table, so whichever lands second rebases its branches onto the other's edit. Letter/byte namespaces are disjoint.
  - `keyboard-move` — shares ONLY the `flow_feed` CSI switch (`src/flow_run.h:24-32`): tab-focus adds `case 'Z'` (Shift-Tab); keyboard-move adds the Shift-arrow CSIs `\x1b[1;2A/B/C/D`. keyboard-move does NOT touch `flow_dispatch_key` (its keys are CSIs, feed-only), whereas tab-focus's `\t`/`\r` live in the dispatch table — so the only merge contact is the feed switch, and whichever lands second rebases its `case` onto the other's. CSI namespaces are disjoint.
  - `esc-selection` (landed) / any package re-touching the lone-ESC branch (`src/flow_run.h:47`) — tab-focus appends `f->focus_node = -1;` to that one line; a co-touching package must keep all four idempotent actions.
  - `validator-load-suspend` / any package adding a zero-init field to `struct flow` and `flow_new` (`src/flow_model.h:254-267,445-455`) — tab-focus adds `int focus_node;` + its `-1` init; co-touching packages must keep both field-block and `flow_new` edits.
  - `viewport-culling` — edits the render node loop region (`src/flow_render.h:201-217`); tab-focus adds a post-pass right after it. Both touch `flow_render.h`; insertions are adjacent, not overlapping.

**Carry-overs fixed.**
  - Closes `autoPanOnNodeFocus` and keyboard focus traversal, flagged unlanded in the feature inventory (drift item `17-tab-focus`: "grep 'focus|ring' src/ = ZERO hits"; inventory rows for `autoPanOnNodeFocus` and `nodesFocusable + Tab focus ring` both `[ ]`). Lone-Tab, previously swallowed as a no-op at `flow_feed`'s trailing `i++` (`src/flow_run.h:48`), now traverses focus.

---

### 6. Shift-arrow selection nudge  `[S]`  ·  id: `keyboard-move`

**Goal.** Add Shift-arrow keyboard nudging to `flow_feed`: the four sequences `"\x1b[1;2A/B/C/D"` (PINNED) move the current FLOW_SELECTED node set by exactly 1 world cell (A up, B down, C right, D left), while bare arrows keep panning the viewport (locked by `test_keys.c:141-145`). Movement routes through `flow_move_node` (`src/flow_model.h:551`) so node-extent and parent-extent clamps apply to nudges, and a multi-node selection moves rigidly as one undo step.

**User value.** A keyboard-only user can reposition the selection one cell at a time without reaching for the mouse — pixel-precise placement that complements coarse mouse drags. Plain arrows stay panning, so the two motions never fight: pan moves the camera, Shift+pan moves the nodes.

**Files touched.**
  - src/flow_run.h  (Shift-arrow parse + dispatch in `flow_feed`)
  - src/flow_model.h  (new `static` selection-roots mover beside `flow_move_node`)
  - tests/test_keys.c  (nudge asserts; already in `TESTS`)
  - Makefile (not touched — `test_keys` already in `TESTS=`, `Makefile:4`)

**Entry points (existing functions to extend).**
  - `flow_feed` arrow-pan switch (`src/flow_run.h:24-32`) — add a 6-byte `"\x1b[1;2{A,B,C,D}"` guard. The existing 3-byte switch reads `b[i+2]` ∈ {A,B,C,D}; the Shift form has `b[i+2]=='1'`, so the two never collide and coexist cleanly.
  - `flow_dispatch_key` (`src/flow_model.h:1059-1081`) — runs FIRST in `flow_feed` (`src/flow_run.h:22-23`); the nudge parse is placed AFTER it, mirroring the documented order for `+/-` (`src/flow_run.h:33-34`: "Placed AFTER `flow_dispatch_key` so a user `flow_bind_key` override still wins via the registry"). `test_keys.c:131-133` proves `flow_feed` already sees a 6-byte CSI (`"\x1b[1;5A"`) whole and that a registry binding for it wins (the win is asserted at line 133).
  - `flow_move_node` (`src/flow_model.h:551`) — the sole position-write path; clamps node-extent (`:560-566`) then parent-extent (`:571-580`) BEFORE journaling (the landed clamp-then-record contract), and fires no per-move observer. Absolute-in: `pos` is a world-absolute target (`:552-554`), so a root nudge is `flow_node_abs(n) + delta`.
  - multi-drag roots loop (`src/flow_input.h:213-225`) — the mirror source: skip a selected node that has a STRICT selected ancestor (`:215-222`), else move it to `node_abs + delta`. The new model-level mover reproduces this walk.
  - `flow_selected_count` (`src/flow_model.h:830`) — empty-selection guard before opening the undo bracket.
  - `flow__undo_begin` / `flow__undo_end` — multi-element-as-one-step precedent: `flow_layout` brackets a whole multi-node commit (`src/flow_layout.h:284,312`).

**API additions.**
```c
/* No new PUBLIC API. One file-static helper (model-level, beside flow_move_node):
   moves every selection ROOT (a FLOW_SELECTED node with no STRICT selected ancestor)
   by (dx,dy) world cells via flow_move_node. Mirrors src/flow_input.h:213-225 so a
   selected child of a selected parent is not double-moved. Stateless: no struct flow
   field, no new flag. */
static void flow__nudge_selection(flow_t *f, int dx, int dy);
```

**Design notes.**

*Delta signs (pinned by the test, not by pan).* `flow_project` is `screen.y = world.y*zoom + oy` (`src/flow_geom.h:16-17`), so world-y grows DOWNWARD. The nudge deltas are therefore A→`(0,-1)`, B→`(0,+1)`, C→`(+1,0)`, D→`(-1,0)`, each 1 world cell (grounded in `flow_move_node`'s absolute-in contract, `src/flow_model.h:552-554`). These are NOT copied from the bare-arrow `flow_pan` args (`src/flow_run.h:26-29`): for EVERY key the nudge delta is the exact negation of the pan arg — pan A passes `dy=+1` to `oy` while the nudge applies world `dy=-1`; pan C/D pass `dx=∓1` while the nudge applies `±1` (so even the x-sign is inverted, not just y). The inversion is expected: pan moves the camera (`flow_pan` adds to `oy`, `src/flow_model.h:774`; with `screen.y = world.y*zoom + oy`, `oy+1` raises `screen.y`, sliding content DOWN under a fixed cursor) while the nudge moves the node. The nudge sign is therefore pinned by an exact post-nudge position assert in the test, not derived from pan.

*Parse site.* The 6-byte guard sits beside the arrow switch in `flow_feed` and AFTER `flow_dispatch_key`, reusing the registry-override-wins ordering already documented at `src/flow_run.h:33-34`. Grounding the site here (rather than as a `flow_dispatch_key` built-in) keeps it adjacent to its sibling CSI cases and matches how arrows and `+/-` are already handled in `flow_feed`.

*Consume-always.* A recognized Shift-arrow consumes all 6 bytes (`i += 6; continue;`) on EVERY path — including empty-selection no-op — so the byte tail never re-enters the loop as junk (e.g. a stray `[1;2A` re-parse). The no-op is "consumed and did nothing," never "fall through."

*Roots-only mover, mirrored not shared.* The helper lands model-level (`src/flow_model.h`, after `flow_move_node` at `:585`) because position mutation is model-layer per the LAYERING rule. It MIRRORS the drag roots walk (`src/flow_input.h:213-225`) rather than refactoring it into a shared function: extracting a shared mover would dirty the byte-locked drag/marquee goldens and widen this small package's blast radius. The intentional duplication is noted here and in Conflicts.

*Rigid multi-move = one undo step.* `flow_feed` guards `flow_selected_count(f) > 0` BEFORE `flow__undo_begin`, then calls the helper, then `flow__undo_end` (precedent `src/flow_layout.h:284,312`). Single- and multi-node selections each emit exactly one journal step; the empty selection never opens a bracket, so `journal.n` is provably unchanged (no reliance on whether an empty begin/end elides).

*Empty selection = silent no-op (no pan fallback).* A pan fallback was rejected: it would make Shift-arrow behavior selection-dependent (nudge-when-selected, pan-when-not), surprising and untestable as a single contract. Empty selection consumes the bytes and does nothing — no move, no journal step, no viewport change.

*Eventless and selection-preserving.* `flow_move_node` fires no observer (drag callbacks do not exist; `src/flow_model.h:551-585`), so the nudge is eventless — noted explicitly. It calls no `flow_select_*` function, so the FLOW_SELECTED set is untouched and `on_selection_change` also stays silent.

*Stateless.* No new struct flow field and no new flag (satisfies the FLAGS/zero-init pinned rule). Focus state (`focus_node`) belongs to tab-focus, not here.

**Test plan.** Extend `tests/test_keys.c` (already in `Makefile:4` `TESTS=` — Makefile unchanged). Red is runtime-behavioral, not compile-RED: there is no new public API and no golden, so pre-implementation the 6-byte sequence is junk-consumed and the position asserts fail. New block after the existing arrow-isolation block (`test_keys.c:137-145`):

  1. Add a node at a known world pos (e.g. `{10,10}`), `flow_select_node(f, a, 0)`; record `f->journal.n` as `j0`.
  2. Feed `"\x1b[1;2A"` (6 bytes); assert `flow_node_pos` is `{10,9}` (up = world-y −1).
  3. Feed `"\x1b[1;2B"`; assert pos `{10,10}` (down = +1, back to start).
  4. Feed `"\x1b[1;2C"`; assert pos `{11,10}` (right = +1). Feed `"\x1b[1;2D"`; assert `{10,10}` (left = −1).
  5. Assert `f->journal.n == j0 + 4` (one step per press).
  6. Feed `"u"` (undo) once; assert the last nudge is reversed (pos `{11,10}`), confirming the press journaled an undoable step.
  7. MULTI-RIGID: select two top-level nodes `a@{0,0}` and `b@{5,0}` additively; feed `"\x1b[1;2C"`; assert both moved to `{1,0}` and `{6,0}` (rigid, identical delta).
  8. NO-DOUBLE-MOVE: `flow_group` a child under a parent (`src/flow_model.h:619`), select BOTH parent and child; record child abs; feed `"\x1b[1;2B"`; assert the child's ABS moved by exactly +1 in y (parent moved; child followed via relative coords; not +2).
  9. CHILD-EXTENT CLAMP: set the child `FLOW_EXTENT_PARENT` and position it flush to the parent's bottom edge; select only the child; feed `"\x1b[1;2B"` (into the wall); assert the child abs is unchanged (clamped, grounded in `src/flow_model.h:571-580`).
  10. PLAIN-ARROW PAN STILL WORKS: record `f->view.oy`; feed `"\x1b[A"` (3 bytes); assert `oy` changed (bare arrow still pans, extends `test_keys.c:141-145`).
  11. EMPTY-SELECTION NO-OP: `flow_clear_selection(f)`; record `j` and `oy`; feed `"\x1b[1;2A"`; assert no node moved, `f->journal.n == j` (no step), and `f->view.oy` unchanged (consumed, did NOT fall through to pan).

**Acceptance.**
  - `make test` passes including all new `test_keys` asserts; whole-suite ASan+UBSan clean (no leak from the bracketed multi-move, no UB in the roots walk).
  - Each Shift-arrow moves the selection exactly 1 world cell in the verified direction; bare arrows still pan (no regression to `test_keys.c:141-145`).
  - A multi-node selection nudges rigidly as ONE undo step (`f->journal.n` +1/press); `flow_undo` restores prior positions.
  - A selected child of a selected parent is not double-moved; extent clamps apply to nudges.
  - Empty selection is a silent no-op: no move, no journal step, no viewport change; the 6 bytes are consumed.
  - No `on_selection_change` and no per-move event fires (nudge is eventless).
  - EXISTING goldens byte-identical (no golden touched; `render_statusbar` cols=30 prefix unchanged — this package skips the statusbar hint). `make demos` warning-free (no demo touched). flow.h regenerated (edits in `src/flow_run.h` + `src/flow_model.h`, both existing modules — no new module).

**Depends on.** Nothing — uses only landed API (`flow_move_node`, `flow_selected_count`, `flow__undo_begin/end`) and the existing `flow_feed` key path.

**Conflicts with.**
  - **tab-focus** — both edit the `flow_feed` key path in `src/flow_run.h` (tab-focus adds Tab/Shift-Tab/Enter parsing; this adds the Shift-arrow guard). Mutual note: land order is arbitrary but the second package rebases its CSI guard beside the first; both append cases to the same switch region (`src/flow_run.h:24-47`), no shared lines.
  - **copy-paste** — light shared-file note: copy-paste edits `flow_dispatch_key` built-ins (`src/flow_model.h:1071-1078`) while this adds a `static` helper near `flow_move_node` (`src/flow_model.h:585`) in the same file; different regions, no overlap, but both touch `src/flow_model.h`.

**Carry-overs fixed.**
  - Completes keyboard parity for node motion: the keyboard could pan and zoom but never move a node; Shift-arrows close that gap, reusing the same extent-clamp and one-undo-step contracts as mouse drag.

---

### 7. Selection clipboard: copy / cut / paste / duplicate  `[M]`  ·  id: `copy-paste`

**Goal.** Add an in-process selection clipboard: `flow_copy_selection`, `flow_cut_selection`, `flow_paste`, `flow_duplicate_selection`, plus the built-in keys `y`/`c`/`p`/`d`. Copy deep-snapshots the selected nodes and the intra-selection edges (both endpoints selected) into a clipboard owned by `struct flow`; paste re-mints them through `flow_add_node`/`flow_add_edge` with fresh ids and a cumulative offset, as one undo step, leaving the pasted elements selected. The clipboard reuses the undo snapshot machinery (`flow__node_snap`/`flow__edge_snap`, `src/flow_model.h:188-189`) for storage, but pastes through the PUBLIC mint path — never the id-preserving `flow__insert_node_at` — so ids never collide.

**User value.** Users can duplicate a sub-graph (`d`), or copy/cut (`y`/`c`) a selection and stamp it elsewhere (`p`), exactly as in xyflow. Cut-then-paste-elsewhere is a move; repeated paste cascades copies with a growing offset. The clipboard survives any graph mutation (it is a deep copy), so paste works even after the source nodes are deleted.

**Files touched.**
  - src/flow_model.h (clipboard fields in `struct flow`; the four public functions; `y`/`c`/`p`/`d` built-ins in `flow_dispatch_key`; `flow__clipboard_clear` in `flow_free`)
  - tests/test_clipboard.c (new)
  - Makefile (TESTS= 25 → 26: append `test_clipboard`)
  - src/flow_render.h (optional one-hint statusbar append — see Design notes; stays past column 30)

**Entry points (existing functions to extend).**
  - `flow_dispatch_key` (`src/flow_model.h:1059`) — append four built-ins to the longest-first chain after `x`/`n`/`f`/`?`/`Space`/`u`/`^r` (`src/flow_model.h:1071-1078`): `if (seq[0]=='y') { flow_copy_selection(f); return 1; }` and likewise `c`→`flow_cut_selection`, `p`→`flow_paste`, `d`→`flow_duplicate_selection`. Registry bindings still win (`src/flow_model.h:1069`).
  - `struct flow` zero-init block (`src/flow_model.h:248-281`) — add a `clip` sub-struct (snapshot arrays + counts + a `gen` paste-offset counter). `flow_new` uses `calloc` (`src/flow_model.h:446`), so the empty clipboard needs no explicit init line.
  - `flow_free` (`src/flow_model.h:456`) — add a `flow__clipboard_clear(f)` call beside the existing edge-label free loop (`src/flow_model.h:459`). The clipboard is deliberately NOT cleared in `flow__graph_reset` (`src/flow_model.h:467`) so it survives `flow_load`.
  - `flow_selected_nodes` (`src/flow_model.h:108`, defn `:831`) / `flow_selected_count` (`:830`) — enumerate the node selection (insertion order) for copy.
  - `flow_add_node` (`src/flow_model.h:502`) / `flow_add_edge` (`src/flow_model.h:656`) — mint fresh ids and record undo on success; paste's re-id + re-insert path.
  - `flow_set_edge_label` (`src/flow_model.h:132`, defn `:904`) — reapply each snapshot's dup'd label onto the freshly minted edge (`flow_add_edge` takes no label param; `src/flow_model.h:656`).
  - `flow_set_parent` (`src/flow_model.h:601`) — second-pass reparent of pasted children whose original parent is also pasted; preserves absolute position across the move (`src/flow_model.h:617`) and records its own REPARENT op into the open txn.
  - `flow_node_abs` (`src/flow_model.h:681`) — convert each selected node's relative pos to absolute for the offset paste; dangling-parent children land at absolute coords.
  - `flow_delete_selection` (`src/flow_model.h:970`) — the delete half of cut: fires `on_nodes_delete` once (`:976-981`) and coalesces into ONE undo step (`:983-988`).
  - `flow__undo_begin`/`flow__undo_end` (`src/flow_model.h:348,352`) — bracket the whole paste as one undo step (precedent: `flow_add_node_center`, `src/flow_model.h:992,997`).
  - `flow__dup` (`src/flow_model.h:290`) / `flow__edge_snap.label_copy` (`src/flow_model.h:189`) — the dup'd-label ownership the clipboard storage reuses.
  - `flow__notify_selection` (`src/flow_model.h:789`) + `cb_suppress` (`src/flow_model.h:262`) — fire `on_selection_change` exactly once for the pasted selection (mirror `flow_delete_selection`'s sig-capture / suppress / single-notify shape, `src/flow_model.h:975,984-989`).

**API additions.**
```c
/* In-process selection clipboard. Copy/cut deep-snapshot the selected nodes plus the
   intra-selection edges (BOTH endpoints selected); node->data is ALIASED (borrowed,
   same contract as the undo snapshots, src/flow_model.h:188), edge labels are dup'd.
   Paste re-mints with fresh ids + a cumulative offset as one undo step and selects the
   result. The clipboard survives graph mutations (deep copy) and flow_load; it is freed
   only in flow_free. */
void flow_copy_selection(flow_t *f);       /* snapshot selection into the clipboard (no graph change, no callbacks) */
void flow_cut_selection(flow_t *f);        /* = copy_selection then flow_delete_selection */
int  flow_paste(flow_t *f);                /* mint clipboard contents at a growing offset; returns nodes pasted (0 if clipboard empty) */
int  flow_duplicate_selection(flow_t *f);  /* snapshot+paste in one shot at +1,+1; clipboard left UNTOUCHED; returns nodes added */
```

**Design notes.**

STORAGE vs PASTE PATH (the load-bearing split). The clipboard *stores* `flow__node_snap`/`flow__edge_snap` (`src/flow_model.h:188-189`), reusing `flow__dup` for the dup'd edge labels and a `flow__op_free`-style teardown (`src/flow_model.h:294`). But paste *re-mints* through `flow_add_node`/`flow_add_edge` — NEVER `flow__insert_node_at`/`flow__insert_edge_at` (`src/flow_undo.h:20,28`), which restore the ORIGINAL id (`src/flow_undo.h:24,32`) and would collide with live nodes. The brief's "strongest reuse" note lists `insert_at`; that is for the storage struct + label ownership, not the paste path. Stated explicitly so an implementer does not reach for the id-preserving helper.

PASTE ALGORITHM (absolute-space, three passes). (1) For each selected node in `flow_selected_nodes` order: `new_abs = flow_node_abs(src) + offset`; mint `flow_add_node(type, new_abs, data)` (which sets `parent = -1`, `src/flow_model.h:506`); record `old_id → new_id`. (2) For each pasted node whose ORIGINAL parent is also in the map: `flow_set_parent(new_id, map[old_parent])` — this preserves the new absolute pos (`src/flow_model.h:617`) and converts it to parent-relative. Children whose parent is NOT in the map stay root at `new_abs` — that IS the dangling-parent resolution, and why the brief flags `flow_node_abs`. (3) For each clipboard edge: mint `flow_add_edge(map[src], map[dst], sh, th)`, then `flow_set_edge_label(neweid, snap.label)` to restore the dup'd label (the SET_LABEL op coalesces into the txn). Whole sequence brackets in `flow__undo_begin`/`flow__undo_end` → ONE undo step.

NODE DATA IS ALIASED, NOT COPIED. Nodes carry only `char type[32]` (inline) and a BORROWED `void *data` (`src/flow_model.h:17`) — there is no node label and no way to deep-copy an opaque `void*`. Pasted nodes therefore alias the source `node->data`, which is the EXACT undo borrowed-data contract (`src/flow_model.h:188` "node.data borrowed"; `src/flow_undo.h:11-13`). Only edge labels (`char *label`, engine-OWNED, `src/flow_model.h:19`) are deep-copied via `flow__dup`. Cleanup (`flow__clipboard_clear`): free each snapshot's `label_copy`, never touch node `data`.

INTRA-SELECTION EDGES ONLY (xyflow parity). Copy snapshots an edge iff BOTH endpoints are in the selected-node set — edge SELECTION flags are irrelevant (a lone selected edge copies nothing). This keeps the pasted sub-graph self-consistent and avoids dangling endpoints.

`on_connect` NOT FIRED ON PASTE — and that is correct. `flow_add_edge` (`src/flow_model.h:656-674`) records undo but does NOT fire `on_connect`; that callback fires only in the interactive `flow_end_connection` path (defn `src/flow_model.h:1174`, fire `src/flow_model.h:1188` — there is no `flow_finish_connection`). So paste mints edges with ZERO `on_connect` events and needs no `cb_suppress` for it — paste is not an interactive connect gesture. (Verified by reading both sites.)

EXACTLY ONE `on_selection_change`. Paste must not emit a flurry of selection events. Mirror `flow_delete_selection` (`src/flow_model.h:975,984-989`): capture `sig = flow__sel_sig(f)` up front, `cb_suppress++` around the clear-then-select-pasted block, `cb_suppress--`, then a single `flow__notify_selection(f, sig)`. Because the pasted nodes change the node-only sig (`flow__sel_sig` hashes nodes only — definition `src/flow_model.h:781-785`, the loop iterates `f->nnodes` and ORs node ids), the fire lands once.

VALIDATOR INTERPLAY (deliberate asymmetry vs `validator-load-suspend`). Pasted edges route through `flow_add_edge`'s validator gate (`src/flow_model.h:665`), and paste DELIBERATELY does NOT suspend it. Unlike load — where re-gating loaded edges can silently drop a persisted graph and `validator-load-suspend` correctly suppresses the validator — a paste that violates a LIVE validator SHOULD have those edges rejected. Result: partial paste (all nodes land; rejected edges are silently dropped, consistent with `flow_add_edge` returning -1). Documented as intentional and cross-referenced to `validator-load-suspend`.

CUMULATIVE OFFSET. The clipboard holds a `gen` counter, reset to 0 on copy/cut. Each `flow_paste` uses offset `(+gen+1, +gen+1)` then increments `gen`, so consecutive pastes cascade instead of stacking on the same cells. Duplicate uses a fixed `+1,+1` (it is a one-shot, not a sequence).

DUPLICATE LEAVES THE CLIPBOARD UNTOUCHED. Factor a static core `flow__paste_snaps(f, nodes, nn, edges, ne, offset)`. `flow_paste` calls it on `f->clip` (and bumps `gen`); `flow_duplicate_selection` snapshots the current selection into a LOCAL temp, calls the core at `+1,+1`, frees the temp — so `f->clip` is never read or written. Clipboard-untouched falls out for free, and duplicate works with an empty clipboard.

CUT. Copy is a pure snapshot (no graph change, no callbacks); cut then calls `flow_delete_selection`, which fires `on_nodes_delete` once (`src/flow_model.h:976-981`) and is one undo step (`:983-988`). The clipboard is an independent deep copy, so paste-after-source-delete still works (borrowed `data` is re-aliased, edge labels re-dup'd). Undo-of-cut restores the originals with their original ids (`flow__insert_node_at` copies the snapped struct verbatim, `src/flow_undo.h:24`); the clipboard is unaffected.

HIDDEN. Hiding a node clears `FLOW_SELECTED` (`src/flow_model.h:1025-1027`) and edges likewise (`:1039`), so hidden elements can never be in the selection — copy/cut never snapshot a hidden element. No special-casing needed; noted for completeness.

STATUSBAR (optional). May append at most one short hint (e.g. ` d:dup`) past the locked cols=30 prefix in `flow_render.h:285`. The render_statusbar golden is rendered at cols=30 and only locks the `" n:add … ?:help"` prefix (`src/flow_render.h:280-282`); that prefix is 28 bytes, so column 30 already lands in the trailing `q:quit`-onward suffix and an appended hint past it keeps the golden byte-identical. Append in package landing order; this package MAY skip the hint.

**Test plan.** New file tests/test_clipboard.c (`#define FLOW_IMPLEMENTATION`); append `test_clipboard` to Makefile `TESTS=` (25 → 26). Compile-RED: the test calls the not-yet-existing API, so the suite cannot build before the implementation lands.
  1. **Copy/paste round-trip + intra-edge + label.** Add nodes a,b with an edge a→b carrying `flow_set_edge_label(e,"hi")`; select a and b; `flow_copy_selection(f)`; `flow_paste(f)` returns 2; assert `flow_node_count == 4`, `flow_edge_count == 2`; find the pasted edge and assert its label is `"hi"` and a DISTINCT pointer from the source edge's `label` (deep copy).
  2. **Re-id freshness.** Assert every pasted node id differs from a and b, and the pasted edge endpoints reference the two NEW node ids (not a/b).
  3. **Offset is cumulative.** Record source a's abs pos; first paste's matching node sits at `+1,+1`; a second `flow_paste(f)` (without re-copy) sits at `+2,+2` relative to the SAME source a (the clipboard still holds the original snapshot) — assert via `flow_node_abs` deltas, proving `gen` cascades (offset = `gen+1` per paste).
  4. **Cut deletes; paste restores.** Select a,b; `flow_cut_selection(f)`; assert `flow_node_count == 0`, `flow_edge_count == 0`; `flow_paste(f)` returns 2; assert two nodes + one edge exist again (fresh ids).
  5. **Duplicate one-shot, clipboard untouched.** Select a distinct node z and `flow_copy_selection(f)` (z into the clipboard); then select a,b and `flow_duplicate_selection(f)` returns 2; assert a,b duplicated; then `flow_paste(f)` pastes Z (the still-held clipboard), proving duplicate did not overwrite `f->clip`.
  6. **Paste after source delete.** Select a,b; copy; `flow_delete_selection(f)`; assert count 0; `flow_paste(f)` returns 2 and restores nodes+edge with the dup'd label — clipboard is a deep copy.
  7. **Paste is ONE undo step.** Snapshot `f->journal.n` before; `flow_paste(f)`; assert `f->journal.n` increased by exactly 1; `flow_undo(f)` returns the graph to its pre-paste node/edge counts in a single step.
  8. **Validator-rejected pasted edge.** Copy a,b (+edge); install a validator rejecting that src/dst (`flow_set_connection_validator`); `flow_paste(f)` returns 2 nodes but `flow_edge_count` shows the edge was DROPPED (partial paste); both nodes still landed.
  9. **Dangling-parent paste.** Parent b under a non-selected container g (`flow_set_parent(b,g)`); select ONLY b; copy; paste; assert the pasted node has `parent == -1` and its `flow_node_abs` equals b's abs `+1,+1` (root at absolute coords; b's original parent g is not in the paste map).
  10. **Pasted elements become the selection, one event.** Register an `on_selection_change` counter; copy a,b; reset counter to 0; `flow_paste(f)`; assert the two pasted node ids are the new selected set (`flow_selected_nodes`) and the counter incremented by exactly 1.
  11. **Empty clipboard.** Fresh graph, no copy: `flow_paste(f)` returns 0, mutates nothing (`f->journal.n` unchanged), fires no `on_selection_change`.
  12. **ASan/UBSan ownership.** Copy a labeled-edge selection, paste twice, `flow_load` a different graph (clipboard survives), paste again, then `flow_free` — clean under ASan/UBSan (clipboard freed once, no double-free of `label_copy`, node `data` never freed).

**Acceptance.**
  - `make test` passes including all of test_clipboard.c; the file was compile-RED before the implementation (asserts the TDD discipline).
  - Whole-suite ASan + UBSan clean: clipboard `label_copy` dup'd on copy and freed once in `flow_free`; node `data` never freed (borrowed); no leak across `flow_load`; no double-free on paste-after-source-delete.
  - All EXISTING goldens stay byte-identical; if the optional ` d:dup` hint is added it lands past column 30 so render_statusbar's cols=30 golden is unchanged (no new golden staged); if the prefix is touched at all, the regenerated golden is staged in the same commit.
  - `flow_paste` returns nodes-pasted; `flow_duplicate_selection` returns nodes-added and leaves `f->clip` byte-for-byte unchanged.
  - Paste is one undo step (`f->journal.n` +1) and one `on_selection_change`; paste fires NO `on_connect`.
  - `make demos` warning-free (demos untouched — keys are library built-ins; no demo wiring needed).
  - flow.h regenerated via tools/amalgamate.sh (no new module; all edits in existing modules).
  - One commit, no trailers.

**Depends on.** Nothing hard-blocks. Cross-references `validator-load-suspend` (documents the deliberate non-suspension asymmetry; can land in either order). Independent of the undo machinery it reuses (already landed).

**Conflicts with.**
  - `tab-focus` — shares the `flow_dispatch_key` built-in/key chain (`src/flow_model.h:1071-1078`) AND the `struct flow` zero-init field block (`src/flow_model.h:248-281`, this package adds the `clip` fields, tab-focus adds `focus_node`). Mutual textual conflict; coordinate insertion order in both blocks. Key namespace is non-overlapping (`y`/`c`/`p`/`d` here vs `\t`/`\x1b[Z`/`\r` there) and clears of demo bindings `l`/`g`/`G`/`h`/`H`.
  - `keyboard-move` — also extends `flow_dispatch_key`/`flow_feed` (Shift-arrow CSI seqs); no key collision with the plain letters here, but the dispatch chain is a shared edit site.
  - Any package appending a statusbar hint (`helper-lines`, `view-frame`, `node-search`, `tab-focus`, `keyboard-move`) — `flow_render.h:285` is a shared line; appends follow package landing order, at most one short hint each, all past column 30.

**Carry-overs fixed.**
  - Closes the inventory gap (drift item `14-copy-paste`): NO clipboard/copy/cut/paste/duplicate existed; every prior "copy"/"dup" hit was undo `label_copy`/`flow__dup` memory ownership or `flow_add_edge`'s duplicate-edge guard — this package adds the real selection clipboard, reusing exactly that snapshot machinery.

---

### 8. Alignment helper lines + snap on drag  `[M]`  ·  id: `helper-lines`

**Goal.** Add xyflow-style alignment guides to a live single-node drag: while a node is being dragged, detect when the dragged node's bounding rect aligns (within a 1-cell tolerance) on any edge — left / right / top / bottom — with another VISIBLE node's rect, draw full-row/full-column guide rules across the viewport for the active alignments, and SNAP the drag onto the guide. The snap is integer arithmetic applied to the prospective world position BEFORE `flow_move_node`, inside the single-node motion branch (`src/flow_input.h:228-231`). The whole feature is gated behind a new engine toggle `flow_set_helper_lines(f, on)`, default OFF.

**User value.** Dragging a node to "line it up" with a neighbor is otherwise eyeball-and-nudge work that the integer cell grid makes finicky. With helper lines on, the dragged node visibly snaps to a shared edge and a guide rule shows exactly which alignment fired — the standard precision affordance every node editor (xyflow, Figma, draw.io) ships.

**Files touched.**
  - src/flow_model.h (struct flow transient fields near `:263`; the `flow_set_helper_lines` decl by the other widget setters at `:244`; the impl by `flow_set_connection_validator` at `:1016`)
  - src/flow_input.h (snap arithmetic + guide capture in the single-node drag branch `:228-231`; guide clear in the release path `:328-329` and the reconnect-release at `:252-254`)
  - src/flow_render.h (guide-rule overlay between the marquee block `:256-269` and the minimap `:271`)
  - tests/test_helper.c (new)
  - tests/snapshots/render_helper_snap.txt (new golden — staged with the package)
  - Makefile (TESTS 25 -> 26: append `test_helper`)
  - flow.h regenerated via tools/amalgamate.sh (generated artifact — committed, not hand-edited)

**Entry points (existing functions to extend).**
  - `flow_handle_mouse` single-node motion branch (`src/flow_input.h:228-231`) — currently `flow_pt w = flow_to_world(f, scr); flow_move_node(f, f->drag_node, (flow_pt){ w.x - f->drag_grab.x, w.y - f->drag_grab.y });` (the move call itself at `:229-230`). This is the only place the dragged node's world target is computed; the snap correction and guide capture wrap it. The multi-drag branch (`:204-227`) is deliberately untouched.
  - `flow_handle_mouse` release fall-through (`src/flow_input.h:328-329`) — the line that resets `f->drag_node = -1` etc.; add the guide-state clear here, and mirror it in the reconnect-release reset at `:252-254`.
  - The drag-arm site (`src/flow_input.h:176-184`) — sets `f->drag_node` and `f->drag_grab` (`:179`); read-only here, it defines the drag lifecycle the test harness drives (press -> motion threshold -> motion -> release).
  - `flow_render` (`src/flow_render.h:166`) — the guide overlay slots in after the marquee box (`:256-269`) and before `if (f->minimap.enabled)` (`:271`), matching the compose contract "overlays nodes/edges, sits under minimap and statusbar."
  - `flow__node_visible` (`src/flow_model.h:692`) — the view-layer skip choke point; the candidate sweep gates on it so hidden nodes never seed a guide. It is `static`, but `flow_model` precedes BOTH `flow_render` and `flow_input` in `tools/amalgamate.sh modules=` (`...flow_model...flow_render flow_json flow_input...`), so it is in TU scope at the candidate-sweep call site — the only NEW call site, which lives in the flow_input drag branch. (The render overlay needs no visibility check: it merely strokes already-captured world lines.)
  - `flow_node_rect_abs` (`src/flow_model.h:687`) — yields the dragged node's and each candidate's absolute world rect for edge comparison.
  - `flow_to_screen` (`src/flow_model.h:78`) — projects a captured world guide line (an x or a y world coordinate) to a screen column/row for the overlay.
  - `flow_set_connection_validator` (`src/flow_model.h:1016`) — the precedent shape for the new setter: a one-line `f->field = arg;` mutator on struct flow, no callback, transient/unjournaled.

**API additions.**
```c
/* alignment helper lines + snap-to-guide during a single-node drag (xyflow helperLines).
   Off by default: with on==0 the drag path is a byte-for-byte no-op (no snap, no guides),
   so every existing drag test and render golden is unaffected. Transient: not saved/journaled. */
void flow_set_helper_lines(flow_t *f, int on);
```

**Design notes.**

DEFAULT OFF (decided). The setter is opt-in like every other widget/clamp in the model: `flow_set_minimap`, `flow_set_background`, `flow_set_node_extent`, `flow_set_translate_extent` (`src/flow_model.h:240,244,76,77`). The decisive reason is golden-safety by construction: with `helper_on == 0` the single-node branch reduces to the exact landed `flow_move_node` call, so no snap can perturb a drag-based test and no guide glyph can appear in a render. `flow_new` allocates the engine with `calloc(1, sizeof *f)` (`src/flow_model.h:446`), so the zero-init OFF default is guaranteed by the allocator (same mechanism the `node_extent`/`validator_fn` "calloc default" comments rely on, `:253,266`). I verified there is no existing golden that snapshots a live node-drag: `render_marquee.txt` sets `marquee_on` directly (tests/test_render.c:172), and the snapshot set under tests/snapshots holds no `drag_node`-active render — so the brief's "grep for mid-drag goldens" check resolves to zero, and OFF makes the question moot regardless.

SCOPE: SINGLE-NODE DRAG ONLY (decided). Snap and guides live exclusively in the `else` branch at `src/flow_input.h:228-231`, taken when `flow_selected_count(f) <= 1`. The multi-drag branch (`:204-227`) shifts a SET by a per-motion world delta with no single anchor rect, so "which node aligns" is ambiguous and the set's bounds move every frame. xyflow's helper lines are single-node-biased for exactly this reason; single-node is the cheap-correct v1.

CANDIDATES: VISIBLE-ONLY, SELF-EXCLUDED (decided). The sweep is a helper-lines-internal loop gating on `flow__node_visible` (`src/flow_model.h:692`), NOT `flow_intersecting_nodes` (`src/flow_query.h:20`). The query primitives are MODEL-level and deliberately include hidden nodes (the layering rule + the comment at `src/flow_query.h:16-19`), so reusing them would let a FLOW_HIDDEN node seed a guide — a view-layer bug. Guides are a VIEW behavior, so they route through the same `flow__node_visible` choke point as render, hit-test, and marquee. The dragged node itself is skipped by id. (Refinement, not a blocker: descendants of the dragged node could also be excluded via `flow_is_ancestor`, precedent `src/flow_input.h:312` — deferred; a child aligning with its moving parent is harmless in v1.)

ALIGNMENT EDGES: L / R / T / B (decided). Per axis the dragged rect's two edges (`x` and `x+w`) are compared against each candidate's two edges; a match within `|delta| <= 1` cell registers a guide at the candidate's edge coordinate. NOTE the consequence for the tests: when two nodes share a dimension exactly, aligning their LEFT edges also aligns their RIGHT edges (same width => `x+w` coincides too), registering TWO guides — so the test fixtures deliberately give the two nodes widths/heights that differ by MORE THAN the 1-cell tolerance to isolate a single matched edge. Center-x / center-y alignment is deferred (see open_questions): under the integer cell model `center = x + w/2` truncates for odd widths, so two differently-sized nodes never share a clean center cell — it needs a half-cell rounding policy that does not exist yet.

SNAP ARITHMETIC: PER-AXIS, INTEGER, BEFORE THE MOVE (decided). The branch first computes the prospective top-left `t = { w.x - grab.x, w.y - grab.y }` (the current landed value) and the prospective rect `{ t.x, t.y, n->w, n->h }`. For each axis independently it finds the nearest candidate edge within tolerance and, if found, corrects `t` on that axis so the dragged edge coincides (snapping either the leading or trailing edge — whichever matched). Only then does `flow_move_node(f, f->drag_node, t)` run. `flow_move_node`'s own `node_extent` clamp (`src/flow_model.h:76`) still wins over snap — acceptable: a snap that would exit the extent is simply clamped back.

STATE (decided). Append a transient sub-block to struct flow at the marquee zero-init region (`src/flow_model.h:263`): `int helper_on;` plus a tiny fixed-cap store of active guide world lines, e.g. `struct { int vert[8], nvert, horz[8], nhorz; } helper;` (world x-coords of active vertical rules, world y-coords of active horizontal rules, counts). calloc zero-init gives OFF + zero guides for free; it is NOT journaled and NOT serialized (mirrors the validator/extent transients, `src/flow_model.h:253,266`). The single-node branch refills the counts each motion; the release path zeroes the counts (a captured guide must never outlive the gesture).

RENDER LAYER (decided). The overlay draws after the marquee box (`src/flow_render.h:256-269`) and before the minimap (`:271`), so guides overlay edges/nodes/handles yet sit under the minimap and the statusbar — grounded in the documented compose order (background `:171`, edges `:175`, nodes `:204`, handles `:221`, conn-preview `:236`, marquee `:256`, minimap `:271`, statusbar `:276`). Node-drag and marquee are mutually exclusive states, so order relative to the marquee box is moot. For each active vertical guide, project its world x via `flow_to_screen` and stroke the full viewport column; likewise each horizontal guide strokes the full row. GLYPH: vertical `╎` (0x254E) and horizontal `╌` (0x254C) — dashed box-drawing, distinct from the marquee's `▒` (0x2592, `:262`), the handle/minimap `◉` (0x25C9, `:230`), and the SOLID background-grid `│`/`─` (0x2502/0x2500, defined inside `flow__background` at `src/flow_render.h:20-21`; the `:170-171` site is only the call). The dashed pair reads as a transient guide and gives the test a single unambiguous codepoint to `strchr` for.

**Test plan.**
  - New file tests/test_helper.c; append `test_helper` to Makefile `TESTS=` (25 -> 26). The file `#define`s FLOW_IMPLEMENTATION so it can poke struct flow internals and assert exact node positions (and resolve a node id to a `flow_node*` via `flow_get_node`, since `flow_node_abs` takes a `const flow_node*`, not an id — `src/flow_model.h:54/681`). Compile-RED: the suite calls `flow_set_helper_lines` (and reads `f->helper.nvert`/`f->helper.nhorz`) which do not exist yet, so test_helper cannot build — let alone pass — before the implementation lands.
  1. ALIGNMENT DETECTED AT EXACT OFFSET. Node A at world (10,5) `w=4 h=3` (x-edges {10,14}) and B at (10,20) `w=6 h=5` (x-edges {10,16} when B.left=10). WIDTHS DIFFER BY >1 ON PURPOSE: with equal widths the right edges would also align and register a second guide. `flow_set_helper_lines(f,1)`. Drive a drag of B: press on B's body, motion to cross the move threshold, then a motion that places B's left edge at world x=10 exactly (B.y held at 20, so its y-edges {20,25} stay clear of A's {5,8} — no stray horizontal guide). Only B.left=10 matches A.left=10 (B.right=16 is 6/2 from A's edges; A.right=14 is 4 from B.left). Assert `f->helper.nvert == 1` and `f->helper.vert[0] == 10`.
  2. SNAP PULLS THE DRAG. Same A (`w=4`, x-edges {10,14}) and B (`w=6`); place the cursor so B's prospective left edge lands at world x=11 (one cell off A.left=10, inside tolerance; B.right=17 is 7/3 from A's edges, no competing snap). After the motion, assert `flow_node_abs(f, flow_get_node(f, B)).x == 10` — the drag was pulled onto the guide, not left at 11.
  3. NO-SNAP OUTSIDE TOLERANCE. Same A (`w=4`, x-edges {10,14}) and B (`w=6`, x-edges {17,23} at this position); place B's prospective left edge at world x=17, which is >1 from EVERY edge of A {10,14} (B.left 17: 7/3 away; B.right 23: 13/9 away). Assert `flow_node_abs(f, flow_get_node(f, B)).x == 17` (unsnapped) and that neither 10 nor 14 is among `f->helper.vert[0..nvert-1]`.
  4. PER-AXIS INDEPENDENCE. A at (10,5) `w=4 h=3` (y-edges {5,8}); B at x=40 `w=6 h=5` (x-edges {40,46}, far from A's {10,14} so `nvert` stays 0), driven so its prospective TOP edge lands at world y=6 (delta 1 to A.top=5 -> snaps; B.bottom 11 vs A's {5,8} is 6/3 away — HEIGHTS DIFFER BY >1 so bottom edges never co-align). Assert `flow_node_abs(f, flow_get_node(f, B)).y == 5` (top snapped), `flow_node_abs(f, flow_get_node(f, B)).x == 40` (x untouched), and `nhorz == 1 && horz[0] == 5 && nvert == 0`.
  5. HIDDEN CANDIDATE IGNORED. `flow_set_node_hidden(f, A, 1)`; repeat test 1's aligned motion. Assert `f->helper.nvert == 0` and B is NOT snapped — guides honor `flow__node_visible`, unlike `flow_intersecting_nodes`.
  6. GUIDES CLEARED ON RELEASE. After an aligned motion that sets `nvert >= 1`, feed the mouse RELEASE. Assert `f->helper.nvert == 0 && f->helper.nhorz == 0` and `f->drag_node == -1`.
  7. TOGGLE GATES EVERYTHING (doubles as the golden-safety proof). `flow_set_helper_lines(f,0)`; repeat test 2's one-cell-off aligned motion. Assert `flow_node_abs(f, flow_get_node(f, B)).x == 11` (NO snap), `f->helper.nvert == 0`, and that rendering this OFF mid-drag frame is byte-identical to the same frame rendered without ever touching the helper API.
  8. MID-DRAG RENDER GOLDEN (compile-RED snapshot). With helper ON and an active vertical guide, `flow_render` into a buffer and compare to tests/snapshots/render_helper_snap.txt (staged). Mechanically guard first: `ASSERT(strchr(dump, <utf8 of 0x254E>) != NULL)` so a glyph regression fails loudly before the human-eyeballed golden diff.
  - ASan/UBSan: the candidate sweep and guide stores are fixed-cap and stack/struct-resident (no heap), so the suite must run clean under both sanitizers.

**Acceptance.**
  - `make test` passes including all of tests/test_helper.c; TESTS lists 26 stems.
  - Whole-suite ASan and UBSan are clean (no leaks, no UB) — the new sweep allocates nothing.
  - All EXISTING goldens are byte-identical (default OFF guarantees it); the one NEW golden tests/snapshots/render_helper_snap.txt is staged in the same commit.
  - `flow_set_helper_lines` is declared once (model.h public block) and defined once (model.h impl); flow.h is regenerated by tools/amalgamate.sh in the commit.
  - With helper lines ON, a single-node drag whose edge comes within one cell of a visible neighbor's matching edge snaps onto it and draws a full-row/column dashed guide; OFF reproduces the exact landed drag behavior.
  - `make demos` builds warning-free (demos optionally call `flow_set_helper_lines`; if a demo wires it, that demo is the only demo touched).
  - One commit for the package, no trailers.

**Depends on.** Nothing hard. Builds on landed inc-4 primitives only: `flow_node_rect_abs` (`src/flow_model.h:687`), `flow__node_visible` (`:692`), `flow_to_screen` (`:78`), and the `flow_set_connection_validator` setter shape (`:1016`).

**Conflicts with.**
  - `marquee-world-anchor` — STRONGEST overlap: both edit flow_input.h motion branches AND both append fields to struct flow's zero-init block at `src/flow_model.h:263`. Coordinate field ordering and land sequentially; whichever lands second rebases its appended sub-block past the first's.
  - `tab-focus`, `copy-paste` — both append to the same struct flow zero-init region (`:263`): tab-focus adds the `int focus_node` sentinel field, copy-paste adds a clipboard buffer. No semantic clash with `helper_on`/`helper`, but the appends touch the same lines — sequence and rebase.
  - `viewport-culling`, `minimap-visible-bounds` — both touch the flow_render.h compose region; my guide overlay sits immediately before the minimap call (`src/flow_render.h:271`), so a package that reorders or culls around that seam must preserve the overlay's "after marquee, before minimap" slot.
  - `keyboard-move` — no file overlap, but both drive `flow_move_node`. Asymmetry to preserve: snap is mouse-drag-only in v1; a Shift-arrow nudge moves by exact integer cells and deliberately does NOT consult helper guides.

**Carry-overs fixed.**
  - Closes the inc-3-audit drift "13-helper-lines" (UNLANDED): the drag path moved nodes unbounded with no guide detection or snap (`src/flow_input.h:228-231`, the bare `flow_move_node` at `:229-230`); this package adds the missing alignment/snap layer on top of the inc-4 intersection/visibility primitives that did not exist at that audit.

---

### 9. Render-only viewport culling  `[S]`  ·  id: `viewport-culling`

**Goal.** Skip fully-offscreen NODES in the node render loop (`src/flow_render.h:204-215`) before the `flow__node_clip` + `nt->render` dispatch, by testing each node's SCREEN FOOTPRINT against the buffer rect `{0,0,cols,rows}`. This is xyflow's `onlyRenderVisibleElements` for v1, applied to NODES ONLY: edges keep routing unconditionally so a connection between two offscreen nodes on opposite sides still draws its on-screen segment. Critically, the test is in SCREEN space, not world space: a node's drawn footprint is a constant `n->w × n->h` SCREEN rect regardless of zoom (`src/flow_model.h:715-722`, `src/flow_render.h:212`), so a world-rect test would under-cover the footprint at `zoom < 1` and wrongly drop on-screen cells (see Design notes).

**User value.** On large graphs the per-frame cost drops from "project + clip + dispatch `render()` for every node" to "project + footprint-intersect, then clip + dispatch only for nodes whose drawn footprint overlaps the buffer." At TUI scale this is a constant-factor win, but it also stops off-screen node `render()` callbacks (custom node types) from running needlessly every frame. The change is invisible to output: anything that previously contributed a cell still does.

**Files touched.**
  - src/flow_render.h
  - tests/test_culling.c (new)
  - Makefile (TESTS= gains `test_culling`)

**Entry points (existing functions to extend).**
  - `flow_render` node loop (`src/flow_render.h:204-215`) — insert the cull test immediately after the `flow__node_visible` gate at `src/flow_render.h:206`, before the `flow_to_screen` / `flow__node_clip` / `nt->render` work at `src/flow_render.h:208-214`. `lod` is already in scope (`src/flow_render.h:202`).
  - `flow__node_footprint` (`src/flow_model.h:718-722`) — supplies the EXACT screen rect the renderer draws (projected top-left, constant `n->w × n->h` at lod 0, single 1×1 cell at lod 1). This is the same helper `flow_hit_node` tests against (`src/flow_model.h:768`), so reusing it makes the cull provably consistent with hit-testing.
  - `flow_rect_intersects` (`src/flow_geom.h:30-34`) — the reject test; closed convention ("edge-touch counts as overlap", `src/flow_geom.h:31`), which is exactly the intersect-not-contain semantic we need.

**API additions.**
```c
/* No new public API. The cull is a private, render-loop-local intersect test
   inserted into flow_render (src/flow_render.h:204-215). No new exported symbol,
   no new flag, no flow_callbacks field, no struct flow field. */
```

**Design notes.**

*Where the test goes (RENDER-LOOP-ONLY, per PINNED LAYERING).* The cull is inserted inside `flow_render`'s node loop, NOT into `flow__node_visible` / `flow__edge_visible` (`src/flow_model.h:692-699`). Those two choke points are shared by hit-test, marquee, `flow_bounds`, and the minimap; putting a viewport test there would make off-screen nodes un-hittable and exclude them from bounds/fit — exactly the drift the choke-point design forbids. The drift evidence (`22-culling`) names these as the *reuse seam*, but the PINNED rule and the brief override: viewport culling is render-loop-only. I REUSE `flow__node_footprint` to *compute* the rect but TEST in the loop, never inside the helper, so the layering pin holds. The insertion is a single `flow_rect fp = flow__node_footprint(f, n, lod); if (!flow_rect_intersects(fp, (flow_rect){0,0,cols,rows})) continue;` — `lod` is already in scope at `src/flow_render.h:202`.

*Why SCREEN space, not WORLD space (the trap).* A node's drawn footprint is a CONSTANT screen-size rect: `flow__node_footprint` returns `{ s.x, s.y, n->w, n->h }` at lod 0 (`src/flow_model.h:722`), and the render `surf` is built with the same constant `n->w, n->h` (`src/flow_render.h:212`). The header says it outright — "only POSITION scales with zoom" (`src/flow_model.h:715-716`). So at `zoom < 1` the footprint covers `n->w/zoom` WORLD units, WIDER than the node's world rect (`n->w`). A world-rect-vs-world-viewport test (the original draft) UNDER-covers the drawn area by `n->w·(1-zoom)/zoom` cells — unbounded by node width, not the ≤1 cell a `lroundf` margin could fix. Hand-verified failing case: `zoom=0.75` (above the 0.6 LOD threshold, so full render), `ox=0`, `cols=30`, label width 10, world `x=-12` → `flow_project` puts the footprint at screen x ∈ [-9, 0], so the node's right border draws at on-screen col 0, yet the inflated world-VP intersect CULLS it (recomputed numerically). No existing golden catches this: zoom-1 goldens have world-rect == footprint, and the z=0.5 goldens are LOD-collapsed to a 1×1 marker. Testing the screen footprint against `{0,0,cols,rows}` is exact and zoom-correct by construction.

*Why no margin is needed.* `flow_rect_intersects` uses the closed / edge-touch convention with a `+w` (not `+w-1`) span on each side (`src/flow_geom.h:30-34`), so it already OVER-includes by one cell per edge. A footprint that touches the buffer on even one edge passes. The over-inclusion only ever ADMITS a borderline node into the render path, where the per-cell `flow_cellbuf_put` bounds clamp (`src/flow_cell.h:63`) and `flow__node_clip` (`src/flow_render.h:211`) decide what actually draws — so the cull can never drop a visible cell. No rounding analysis, no inflation, no world-VP construction.

*Straddling nodes keep drawing (intersect-not-contain).* A node whose footprint overlaps the buffer on even one edge passes the intersect test and proceeds to the existing per-cell clamp path. Off-screen suppression for the straddling case stays exactly where it is today — per cell, via `flow__node_clip` (`src/flow_render.h:211`) and `flow_cellbuf_put`'s bounds clamp (`src/flow_cell.h:63`) — so the on-screen sliver is byte-identical.

*Edges are NOT culled (v1).* The edge loop (`src/flow_render.h:175-193`) is left entirely untouched. An edge between two offscreen nodes on opposite sides of the viewport routes a path THROUGH the visible region; culling by endpoint visibility would erase it. Routed-path-bbox culling (reject an edge whose *router output* bbox misses the viewport) is a sound future optimization but needs the route computed first, so it saves only the `flow_cellbuf_put` writes, not the `et->route` call — deferred and recorded as a future option, not done here.

*Handles and labels need no extra work.* Node labels are drawn by the node's own `render()` inside the culled dispatch, so node-cull covers them. The separate handle-reveal loop (`src/flow_render.h:221-232`) is gated by `flow__node_handles_visible` (`src/flow_model.h:1121-1124`), which requires `HOVERED | SELECTED | conn_node` — an off-screen node could be selected and reach this loop, but it writes its `◉` via `flow_cellbuf_put` (`src/flow_render.h:230`), which clamps per-cell, so a fully-offscreen selected node emits zero handle cells today and after the change. I deliberately do NOT add a cull there: re-projecting per handle would duplicate work for a loop that already self-suppresses, and the in-flight connection preview (`src/flow_render.h:234-252`) must stay visible even when its source node is off-screen.

**Test plan.**
  - New file tests/test_culling.c; append `test_culling` to `TESTS=` in the Makefile (currently 25 stems, ending `test_query` at `Makefile:4` → 26). Compile-RED: the test calls `flow_render` and asserts the new cull semantics; the goldens-byte-identical gate (below) is the red signal — it fails the instant the cull drops a cell the un-culled build produced. Use the existing `cells_to_string` helper idiom from `tests/test_render.c:5-15` and `SNAPSHOT` / `strchr` mechanical guards (precedent: the FLOW_HIDDEN test at `tests/test_render.c:347-358`).
  1. **Off-screen node produces zero cells.** `flow_new(30, 8)`, `flow_register_defaults`, add node "A" at `(0,0)` and node "B" at `(5000,5000)` (default render gives w=len+4, h=3, so "B" → 5×3 at world (5000,5000), far off-screen). `flow_render`; `cells_to_string`. Assert `strchr(s, 'A') != NULL` and `strchr(s, 'B') == NULL` (B's payload label is its unique char). This is the zero-cells claim.
  2. **Off-screen node is still model-queryable.** Same graph. Assert `flow_get_node(f, idB) != NULL` (model layer sees the culled node) and `flow_node_count(f) == 2`; assert `flow_bounds(f)` still includes B's rect (bounds is model-level, not render-culled — `src/flow_model.h:700-710`). Additionally pan/zoom B into view and assert `flow_hit_node(f, <B's footprint cell>) == idB`. Proves the cull did NOT leak into the choke points.
  3. **Straddling node draws its on-screen sliver.** Add node "S" at world `(-2,1)` (default → w=5, footprint screen x ∈ [-2,2] at zoom 1, so cols 0–2 are on-screen). `flow_render`; assert `strchr(s, 'S') != NULL` is NOT required (label sits at surface col 2 = screen col 0, may or may not be the label glyph) — instead assert at least one non-blank cell appears in screen cols 0–2 of row 1 (the box's right border / interior), or `SNAPSHOT("cull_straddle", s)` locking the visible sliver. Proves intersect-not-contain.
  4. **Viewport-crossing edge between two offscreen nodes still renders.** Place node L far left off-screen and node R far right off-screen with the viewport between them; add edge L→R. `flow_render`; assert the frame contains edge-path glyphs in the interior (e.g. a row of the snapshot contains the horizontal connector `0x2500` `─`, asserted via its UTF-8 bytes, or `SNAPSHOT("cull_crossing_edge", s)` locking the through-line). Proves edges are NOT endpoint-culled.
  5. **Fractional-zoom regression (the world-rect trap).** Reproduce the z<1 case that a world-rect cull would mis-drop: `flow_set_zoom(f, 0.75f, anchor)` (the public API; read back via `flow_zoom(f)` — `tests/test_zoom.c:22-23`). NOTE `flow_set_zoom` shifts `ox` to keep `anchor` fixed, so after the call read `f->view.ox` (tests may poke struct flow internals per the pinned DISCIPLINE rule) and place a node so its footprint's right border lands on screen col 0 while its world rect is entirely left of the world viewport — recompute `flow_project` by hand for the resulting `ox`. `flow_render`; assert col 0 of the node's row is non-empty. Guards exactly the screen-vs-world design decision (a node whose footprint pokes on-screen at z<1 must NOT be culled).
  6. ASan/UBSan clean: the footprint computation and intersect test allocate nothing; the test must leak nothing (`flow_free` each graph).

**Acceptance.**
  - `make test` passes including tests/test_culling.c.
  - Whole-suite ASan + UBSan clean (per-package gate).
  - **ALL existing goldens byte-identical** — this is the package's primary risk and primary acceptance bar. Every snapshot in tests/test_render.c (single/two-node/edge/minimap/background/group/label/zoom frames) MUST be unchanged, because every node those goldens render has a footprint overlapping the buffer and so survives the intersect test (at any zoom, since the test is in screen space). No golden is regenerated; no new golden is staged for the cull beyond test_culling's own new snapshots (which are staged as part of this package).
  - The minimap (`src/flow_render.h:28-72`) and the render_statusbar golden (cols=30, `src/flow_render.h:276-285`) are untouched — this package APPENDS no statusbar hint (a render-internal optimization has no user-facing key, so per the STATUSBAR rule it MAY skip, and does).
  - `make demos` warning-free (demos are not edited; the loop change is internal to flow_render).
  - flow.h regenerated via tools/amalgamate.sh (edit lands in src/flow_render.h, an existing module; no module-list change).

**Depends on.** Nothing hard. It builds only on already-landed code (`flow__node_footprint` `src/flow_model.h:718-722`, `flow_rect_intersects` `src/flow_geom.h:30-34`, and the inc-4 `flow__node_visible` gate at `src/flow_model.h:692`).

**Conflicts with.**
  - **minimap-visible-bounds** — this package now culls in SCREEN space via `flow__node_footprint` and builds NO world viewport, so the earlier "both mirror the `src/flow_render.h:49-51` world-VP idiom" conflict largely dissolves. The only remaining mutual note: do not let either edit silently change the minimap's scaling window `W` (`src/flow_render.h:52`), which is golden-locked.
  - **view-frame** — adds new framing functions that write the viewport through `flow__view_set`; this package only READS `f->view` (indirectly, via `flow__node_footprint` → `flow_to_screen`) inside the render loop and writes nothing, so there is no viewport-seam conflict, but both touch viewport-derived geometry and should land in dependency order if view-frame changes projection.
  - **marquee-world-anchor** — shares the `flow__node_visible` / footprint choke points conceptually; no code conflict because marquee stays in the model/view layers and this cull stays render-loop-local. Note recorded so a reviewer confirms neither moved a viewport test into the shared choke point.
  - `tab-focus` and `helper-lines` — same-file `src/flow_render.h` rebase notes *(post-execution correction; an earlier draft disclaimed both)*: #5's focus-ring post-pass sits immediately AFTER the node loop this package culls in; #8's guide overlay sits before the minimap call. Disjoint regions from the node-loop cull insert, but whichever lands later rebases line offsets.
  - No conflict with keyboard-move / copy-paste / node-search / edge-events-inflight / validator-load-suspend: those touch the dispatch/feed key paths, struct flow's zero-init block, or flow_input.h; this package touches only the `flow_render` node loop and adds a standalone test.

**Carry-overs fixed.**
  - Closes the inc-4 `22-culling` drift item ("partial": flag-skip exists, viewport-cull absent). After this, off-screen nodes are no longer projected-into-a-surface/clipped/dispatched every frame — the missing `onlyRenderVisibleElements` half of the inc-4 choke-point work, scoped to nodes-only and render-only as the PINNED rules require, and culling on the EXACT screen footprint `flow_hit_node` already uses so render and hit-test stay consistent.

---

### 10. Key hook gate + label vtable + find query + demo palette  `[M]`  ·  id: `node-search`

**Goal.** Ship a node search/command palette in three smallest-API-first pieces: (1) an engine-level pre-dispatch **key hook** gate (`flow_set_key_hook`) that lets a modal UI intercept input before any binding or built-in fires; (2) an engine **find query** — `flow_find_nodes`, case-insensitive substring match over a new optional `label()` vtable accessor (`flow_node_type.label`, ABI-appended after `load`); (3) a `'/'`-driven command palette in `demos/flowchart.c` built on those two primitives plus view-frame's framing API.

**User value.** A graph with more than a screenful of nodes is unnavigable by pan alone. Typing `/` then a few letters of a node's label, watching the match list narrow live, and hitting Enter to select-and-frame the hit is the standard "jump to node" affordance (xyflow's search/minimap-click analog). The key hook is the reusable seam any modal demo UI (rename box, inline editor) needs; the find query is reusable headless API for app-side search.

**Files touched.**
  - src/flow_model.h (vtable `label` member; `flow_key_hook` typedef + `flow_set_key_hook` decl/def; `key_hook_fn`/`key_hook_user` struct flow fields; hook call at top of `flow_dispatch_key`)
  - src/flow_types.h (default + group type initializers gain a `label` accessor)
  - src/flow_query.h (`flow_find_nodes` decl + impl)
  - demos/flowchart.c (palette: `'/'` binding, hook, incremental match, overlay draw)
  - tests/test_search.c (NEW — engine: hook ordering + find query)
  - tests/test_flowchart.c (palette drive via the existing `FLOWCHART_TEST` harness)
  - Makefile (TESTS= gains `test_search`: 25 → 26; `test_flowchart` already listed)

**Entry points (existing functions to extend).**
  - `flow_dispatch_key` (`src/flow_model.h:1059-1081`) — insert the hook call at the very top, AFTER the `if (n <= 0) return 0;` guard (`src/flow_model.h:1060`) and BEFORE the registry scan (`src/flow_model.h:1061-1069`), so the hook precedes BOTH `flow_bind_key` bindings and the built-ins (`src/flow_model.h:1071-1078`).
  - `flow_feed` (`src/flow_run.h:15-50`) — UNCHANGED. It already calls `flow_dispatch_key(f, b+i, n-i)` and advances `i += dk` (`src/flow_run.h:22-23`); the hook rides that exact return-is-byte-count plumbing. Mouse CSI is parsed at `src/flow_run.h:18-21` BEFORE dispatch, so the hook never sees mouse sequences (documented asymmetry below).
  - `flow_node_type` vtable struct (`src/flow_model.h:22-28`) — append `label` as the last member after `load` (`src/flow_model.h:27`).
  - `flow_node_type_for` (`src/flow_model.h:489-492`) — the existing type-resolve used by `flow_find_nodes` to reach a node's `label()`.
  - `flow_default_node_type` / `flow_group_node_type` initializers (`src/flow_types.h:27,46`) — append the accessor (both already read `n->data` as the label at `src/flow_types.h:9,14,34`).
  - `flow_set_connection_validator` (`src/flow_model.h:1016-1018`) — the gate-on-`struct flow` precedent the key hook mirrors (typedef `src/flow_model.h:128-130`, fields `src/flow_model.h:266`, setter at `:1016`).
  - `flow_nodes` / `flow_node_count` (`src/flow_model.h:679,677`) — node iteration for the substring sweep.
  - `flow_select_node` (`src/flow_model.h:809`) — palette's Enter selects the first match.
  - `flow_bind_key` (`src/flow_model.h:1050-1057`) + `fc_overlay` (`demos/flowchart.c:110-115`) — demo seam for `'/'` and the match-list draw.
  - `flowchart_setup` (`demos/flowchart.c:121-153`) + the `FLOWCHART_TEST` harness include (`tests/test_flowchart.c:10-11`) — palette wiring + its headless test.

**API additions.**
```c
/* (1) ENGINE GATE — pre-dispatch key interceptor on struct flow (gate, NOT a
   callbacks observer; mirrors flow_set_connection_validator at src/flow_model.h:1016).
   Returns BYTES CONSUMED: 0 = pass-through (dispatch continues to registry+built-ins);
   a positive count = consumed (dispatch returns it verbatim, flow_feed advances i by it).
   Sees EVERY key/escape sequence before bindings and built-ins; does NOT see mouse
   (flow_feed parses mouse at src/flow_run.h:18-21 before calling dispatch). NULL = no
   hook (calloc default), zero overhead. TRANSIENT: not journaled, not persisted. */
typedef int (*flow_key_hook)(flow_t *f, const char *seq, int len, void *user);
void flow_set_key_hook(flow_t *f, flow_key_hook fn, void *user);

/* (2a) ENGINE — optional label accessor, APPENDED LAST to the flow_node_type vtable
   (after `load`, src/flow_model.h:27). Zero-init safe: NULL = node has no searchable
   label. Returns a NUL-terminated C-string owned by the node (lifetime = the node). */
/* appended inside struct flow_node_type (src/flow_model.h:22-28):
       const char *(*label)(const flow_node *n);                            */

/* (2b) ENGINE QUERY — case-insensitive substring match over label(), MODEL-level
   (hidden INCLUDED, per the layering rule), insertion order, fill-buffer idiom
   (true total returned past max; out may be NULL with max 0). needle=="" matches
   every labeled node; a node whose type has no label() (NULL) never matches.
   No allocation (query.h contract). Lives in flow_query.h. */
int flow_find_nodes(flow_t *f, const char *needle, int *out, int max);
```

**Design notes.**

*Hook return contract = bytes consumed, not a bare boolean.* `flow_feed` advances `i += flow_dispatch_key(...)` (`src/flow_run.h:22-23`) and dispatch already returns a byte *count* (`return (int)bestlen;`, `src/flow_model.h:1069`). The hook returns the same currency: `0` = pass-through, a positive count = consumed-this-many-bytes, and `flow_dispatch_key` returns the hook's value verbatim. This lets a future modal correctly swallow a multi-byte CSI by returning its length; the palette here only ever consumes single bytes (printables, Backspace `0x7f`, Enter `\r`, lone ESC `\x1b`), so it returns 1 for those and 0 otherwise. Stating "nonzero = consumed" bare would be ambiguous about how far `i` advances — that ambiguity is the load-bearing detail, so the contract is pinned to byte-count.

*Hook placement — top of `flow_dispatch_key`, after the `n<=0` guard, before the registry.* Inserting at `src/flow_model.h:1060/1061` puts the hook ahead of both `flow_bind_key` user bindings (`src/flow_model.h:1061-1069`) and built-ins (`src/flow_model.h:1071-1078`), which is the requirement: a modal palette must see input the demo's `l/g/G/h/H` bindings and the library's `x/n/f` would otherwise claim. Because `flow_feed` handles mouse at `src/flow_run.h:18-21` BEFORE calling dispatch, the hook sees only key/escape input, never mouse — correct for a search palette, and documented as the asymmetry.

*Lone-ESC vs CSI inside the hook.* The palette's hook closes on a lone ESC but must let a CSI through so arrows still pan. It mirrors `flow_feed`'s own loneness test (`src/flow_run.h:47`): consume (`return 1`) only when `seq[0]==0x1b && (len<2 || seq[1]!='[')`; when `seq[1]=='['` it returns 0 so the arrow/mouse/Delete sequence falls through to the registry/feed branches. Without this the open palette would swallow arrow keys as bogus ESCs.

*`label` vtable placement and `flow_find_nodes` module.* The vtable STRUCT `flow_node_type` and `flow_node_type_for` both live in `flow_model.h` (`src/flow_model.h:22-28,489`), which `tools/amalgamate.sh` emits BEFORE `flow_query` (`modules=` order at `tools/amalgamate.sh:5`); the type INSTANCES in `flow_types.h` are emitted AFTER `flow_query` but that is irrelevant — `flow_find_nodes` calls `label()` through a runtime pointer, needing only the struct layout and `flow_node_type_for`, both already in scope. So `flow_find_nodes` lives in `flow_query.h` with no `modules=` change. Appending `label` LAST (after `load`) is zero-init/ABI-safe: every registered type that does not set it leaves `label==NULL` and is simply unsearchable; only `flow_default_node_type` and `flow_group_node_type` initializers (`src/flow_types.h:27,46`) gain an accessor, each returning `n->data ? (const char*)n->data : NULL` to match how those types already read the label (`src/flow_types.h:9,14,34`). Because the group type ALSO gains the accessor, the flowchart demo's labeled `group` container ("branches", `demos/flowchart.c:137`) becomes searchable too — accounted for in the demo test counts below.

*No `strcasestr` (not in C11).* `flow_find_nodes` does an allocation-free `tolower`-folded substring scan (manual two-pointer compare), honoring `flow_query.h`'s no-alloc contract (`src/flow_query.h:3-6`). Match is MODEL-level: hidden nodes ARE matched (the layering rule — `flow_query.h` queries never gate through `flow__node_visible`, `src/flow_model.h:692`); the asymmetry that the demo PALETTE skips hidden nodes is a VIEW-level UI choice made in the demo, documented there.

*Palette frames the match via view-frame (#4), not `flow_fit_view`.* The drift evidence confirms `flow_fit_view` always frames `flow_bounds(f)` over ALL visible nodes (`src/flow_model.h:700-710,1006`) and cannot frame a subset. Enter therefore `flow_select_node`s the first match then frames just that node via #4's subset framing (`flow_fit_bounds`/`flow_set_center` — #4 owns the final signatures). This is the one cross-package dependency.

*Statusbar: SKIP.* `'/'` is a demo-side binding (PINNED key-allocation: node-search is demo-only), so its hint belongs in the demo's `fc_overlay` help-text line (`demos/flowchart.c:112-113`), NOT the frozen library bar string at `src/flow_render.h:283-285`. The `render_statusbar` golden (the only statusbar golden, `tests/test_keys.c:158`) stays byte-identical; no flowchart screen golden exists (`test_flowchart.c` is assertion-only), so editing `fc_overlay`'s text is golden-safe. Per the STATUSBAR pinned rule, packages MAY skip, and this one does.

*No clock needed.* The palette redraws on every keystroke through the existing `flow_feed`+`flow_present` run loop (`src/flow_run.h:57-62`) — no timed tick, which is why this package is feasible where animated-edges (needing a redraw clock) was dropped.

**Test plan.**

  - NEW **tests/test_search.c** (engine; `#define FLOW_IMPLEMENTATION`, `#include "../flow.h"`, `flowtest.h`). Compile-RED first: it calls `flow_set_key_hook` and `flow_find_nodes` before they exist, so the suite physically cannot run until the implementation lands (snapshot-trap dodged — no golden in this file).
    1. **Hook ordering vs built-in.** Register defaults, add a node. Install a counting `flow_bind_key(f, "g", ...)` and a hook whose fn returns 1 (consume) and bumps a counter. Feed `"g"`; assert hook counter == 1, bound-key counter == 0 (hook ate it before the registry).
    2. **Hook ordering vs built-in `x`.** With the consuming hook installed and a node selected, feed `"x"`; assert `flow_node_count(f)` unchanged (built-in delete never reached) and hook counter incremented.
    3. **Pass-through.** Install a hook that returns 0; feed `"g"`; assert the bound key fired (counter == 1) — `flow_dispatch_key` continued past the hook.
    4. **Byte-count consume.** Hook returns 2 for a 2-byte seq; feed a 2-byte buffer; assert `flow_feed` advanced exactly 2 (drive a 3rd trailing byte that a single bound key would otherwise claim, assert it was NOT claimed).
    5. **Clear hook.** `flow_set_key_hook(f, NULL, NULL)`; feed `"g"`; assert the bound key fires again (hook fully removed).
    6. **find: match + insertion order.** Nodes labeled `"alpha"`,`"beta"`,`"alphabet"`. `flow_find_nodes(f, "alph", out, 8)` == 2; `out[0]`==alpha's id, `out[1]`==alphabet's id (insertion order, mirroring `tests/test_query.c:28-30`).
    7. **find: case-insensitive.** `flow_find_nodes(f, "BET", out, 8)` matches `"beta"` and `"alphabet"` == 2.
    8. **find: empty needle.** `flow_find_nodes(f, "", out, 8)` == count of labeled nodes.
    9. **find: hidden INCLUDED.** `flow_set_node_hidden(f, alpha, 1)`; `flow_find_nodes(f, "alph", out, 8)` still == 2 (model-level — `src/flow_query.h:3-6`).
    10. **find: NULL-label type skipped.** Register a node type with `label==NULL` (zero-init), add one; assert it never appears in any result and `""` needle does not count it.
    11. **find: fill-buffer / count past max.** `int small[1]; flow_find_nodes(f, "alph", small, 1)` returns true total 2, writes only `small[0]` (mirrors `tests/test_query.c:46-49`); `flow_find_nodes(f, "alph", NULL, 0)` returns 2 with no write.
  - Extend **tests/test_flowchart.c** (palette; via the existing `FLOWCHART_TEST` include at `tests/test_flowchart.c:10-11`, no TESTS change). The demo graph has SEVEN searchable labels: the six leaf nodes `start/validate/valid?/save/reject/done` (`demos/flowchart.c:123-128`) PLUS the labeled `group` container `branches` (`demos/flowchart.c:137`), which the new group-type `label` accessor makes searchable — every count assert below includes it.
    12. Feed `"/"`; assert palette open (poke the demo's exported/`extern` palette-open flag, or assert via a getter the package exposes).
    13. Feed `"s"`; assert the live match set (via `flow_find_nodes` over the demo's query buffer) is exactly `{start, save, branches}` (the three labels containing the substring `s`); feed `"a"`; assert it narrows to exactly `{save}` (only `save` contains `"sa"` — `start` has `"st"`, `branches` has no `"sa"`). Substring, not prefix: the match anchors anywhere in the label.
    14. Feed `"\x7f"` (Backspace); assert the query shrank back to `"s"` and the match set re-widened to `{start, save, branches}`.
    15. Feed `"\r"` (Enter); assert `flow_selected_node(f)` == the first match id (`flow_select_node` fired) and the palette closed.
    16. Reopen, feed `"\x1b"` (lone ESC); assert palette closed and selection unchanged; assert an arrow CSI fed while open still pans (hook returned 0 for the CSI — `down`/`up` ox/oy moves).
  - **Makefile:** append `test_search` to TESTS= (25 → 26).
  - Whole-suite ASan/UBSan: no leaks from the substring scan (stack buffers only) or the palette query buffer (fixed char array in the demo).

**Acceptance.**
  - `make test` passes with all new asserts; TESTS= count is 26 and `test_search` builds and runs.
  - With a consuming key hook installed, neither a `flow_bind_key` binding nor a built-in (`x`/`n`/`f`) fires for the consumed byte; with the hook cleared or returning 0, both fire normally.
  - `flow_find_nodes` returns the true total even past `max`, accepts `NULL`/`0`, matches case-insensitively in insertion order, includes hidden nodes, and skips NULL-`label` types.
  - The demo palette opens on `/`, narrows incrementally (substring match over all seven labeled nodes including the `branches` group), edits with Backspace, closes on ESC, and on Enter selects + frames the first match (via #4's framing API); arrows fed while the palette is open still pan.
  - Whole-suite ASan/UBSan clean.
  - Goldens byte-identical — `render_statusbar` untouched (no library statusbar edit; the hint lives in `fc_overlay`, and no flowchart screen golden exists).
  - `make demos` warning-free under `-Wall -Wextra` (`Makefile:2,11-14`), including the edited `demos/flowchart.c`.
  - `flow.h` regenerated via `tools/amalgamate.sh` (no `modules=` change; `flow_find_nodes` stays in the existing `flow_query` slot).

**Depends on.**
  - **view-frame** — Enter frames just the matched node via #4's subset-framing API (`flow_fit_bounds`/`flow_set_center`); #4 owns the final signatures (they are not on `main` — `flow_fit_view` cannot fit a subset, `src/flow_model.h:1006`, drift-confirmed). The engine pieces (hook + find query) have NO dependency and can land/test first; only the demo palette's frame-on-Enter step needs #4.
  - Uses already-landed `flow_select_node` (`src/flow_model.h:809`), `flow_bind_key`/`flow_dispatch_key` (`src/flow_model.h:1050-1081`), and the `flow_set_connection_validator` gate precedent (`src/flow_model.h:1016`).

**Conflicts with.**
  - **tab-focus** — also edits the `flow_dispatch_key`/`flow_feed` key path (its Tab/Shift-Tab/Enter handling) AND adds a `struct flow` field (`focus_node`) in the same zero-init block (by the validator fields, `src/flow_model.h:266`) where this package adds `key_hook_fn`/`key_hook_user`. Land order must keep both field appends and both dispatch insertions distinct. *(Post-execution correction — the consistency pass struck a false claim here: the hook DOES sit above tab-focus's dispatch handling, but the v1 palette hook consumes only printables/Backspace/Enter/lone-ESC, so Tab (`\t`, a control byte) and the Shift-Tab/Shift-arrow CSIs PASS THROUGH and act behind an open palette — the signed-off v1 limitation, documented in Design notes and the demo.)*
  - **keyboard-move** — adds Shift-arrow (`\x1b[1;2A/B/C/D`) handling in the dispatch/feed key path; mutual note: the key hook precedes it, so Shift-arrows are intercepted while a modal is open.
  - **copy-paste** — adds `y/c/p/d` plain-letter built-ins/bindings on the same key path; mutual note: the hook precedes these too, so the palette consumes printable letters before copy-paste can claim them while open.
  - No conflict with the flow_query.h queries (purely additive function), the vtable append (zero-init safe for all other types), or `flow_callbacks` (no callbacks field added — this is a gate, per the GATES-vs-OBSERVERS rule).

**Carry-overs fixed.**
  - Settles the inc-4 deferral's open discriminator (drift item `16-node-search`): fit-to-result was blocked because `flow_fit_view` cannot frame a subset (`src/flow_model.h:700-710,1006`); resolved by select-then-frame via view-frame (#4) rather than a fit-to-result variant inside this package.
  - Adds the reusable modal-input seam (`flow_set_key_hook`) the codebase lacked — any future demo modal (rename, inline edit) builds on it instead of fighting the binding/built-in precedence.

---

### 11. Edge events fall-through after in-flight connection resolution  `[M]`  ·  id: `edge-events-inflight`

**Goal.** Reverse the mid-flight press contract. Today a left-press while a connection is in flight is consumed by `flow__resolve_connection_at` plus an unconditional early `return` (`src/flow_input.h:97`): `on_connect_end` fires, the edge under the cell gets nothing. This package makes a press that resolves the gesture as a **cancel** (dropped on empty pane / on an edge cell / on the source / on self) FALL THROUGH to the normal press classification, so the gesture's `on_connect_end` fires FIRST and the subsequent release fires `on_edge_click` / `on_pane_click` / `on_edge_context` (or, when the cancel lands on a node body, `on_node_click` / `on_node_context`) SECOND — matching xyflow, where the connection gesture ends on pointerup and the element under the cursor then gets its own click. A press that COMPLETES the connection on a target node stays consumed (xyflow: a successful connect swallows the pointer event).

**User value.** A user mid-connection who decides to click an existing edge (to select it, open its context menu, or trigger an app action) no longer has that click silently eaten by the just-cancelled connection. The connection ends and the edge responds in one gesture, the same way clicking the pane mid-connection now both cancels AND reports the pane click. The interaction stops being a dead-end and matches every pointer-driven node editor users have seen.

**Files touched.**
  - src/flow_input.h
  - tests/test_connect.c
  - Makefile (no change: test_connect already in TESTS=)

**Entry points (existing functions to extend).**
  - `flow__resolve_connection_at` (`src/flow_input.h:40-49`) — currently returns `void`; on a target node (`tnode != -1 && tnode != f->conn_node`) it calls `flow_end_connection` (`src/flow_input.h:45`), else `flow_cancel_connection` (`src/flow_input.h:47`). Change its return type to `int` reporting the resolution outcome (completed-on-node vs cancel) so the press handler can branch. Its release-path caller (`src/flow_input.h:239`) ignores the return — unchanged.
  - press button-0 handler (`src/flow_input.h:94-97`) — the line `if (f->conn_active) { flow__resolve_connection_at(f, scr); return; }` is the contract pivot. Replace the unconditional `return` with: resolve, then `return` only if the resolution completed on a node; otherwise FALL THROUGH to the existing hit-precedence arming below (`src/flow_input.h:104-153`).
  - press button-2 (right-click) handler (`src/flow_input.h:83-92`) — `if (f->conn_active) { flow_cancel_connection(f); return; }` (`src/flow_input.h:84`) suppresses `on_edge_context` mid-flight. Apply the same rule: cancel, then fall through to the existing node/edge context dispatch (`src/flow_input.h:85-92`). (A right-click never completes a connection — `flow_cancel_connection` is the only outcome here — so this branch always falls through.)
  - the release classifier (`src/flow_input.h:258-295`) — UNCHANGED. It is where `on_node_click` (`:269`), `on_edge_click` (`:282`), `on_edge_dblclick` (`:284`), and `on_pane_click` (`:291`) actually fire. A fall-through press re-enters the arming block, and the matching release then runs this classifier normally; no new firing site is added.
  - `flow_end_connection` (`src/flow_model.h:1174-1194`) / `flow_cancel_connection` (`src/flow_model.h:1195-1200`) — UNCHANGED. They already fire `on_connect_end` AFTER `flow__undo_end` settles (`src/flow_model.h:1189-1192`) with the handoff-verified target semantics (success and validator/duplicate reject pass the attempted node; cancel passes `-1`, `src/flow_model.h:1199`). The edge event ordering after `on_connect_end` follows for free because resolution runs to completion before the fall-through arms anything.

**API additions.**
```c
/* No new public API. flow__resolve_connection_at (src/flow_input.h:40) is a static
   internal helper; its signature changes from void to int (outcome: 1 = completed on a
   target node and the press is consumed; 0 = cancelled, fall through to press classification).
   The edge/connect callbacks (on_edge_click/on_edge_context/on_edge_dblclick/on_pane_click/
   on_node_click/on_node_context/on_connect_end) and flow_end_connection/flow_cancel_connection
   are all already exported and unchanged. */
```

**Design notes.**

The brief poses five decisions; each is resolved below with the code in hand, then unified into ONE rule.

(a) *Node/handle completion must not double-fire as a node-click.* `flow__resolve_connection_at` (`src/flow_input.h:43-45`) calls `flow_end_connection` whenever a target node distinct from the source is under the cell. xyflow swallows the pointer event on a successful connect, so a press that ENDS on a node consumes and returns — never re-arming the node-click path below (`src/flow_input.h:140-150`). Decision: **fall through ONLY when the resolution did NOT complete on a node** (i.e. a cancel). The helper returns 1 on the `flow_end_connection` branch and 0 on the `flow_cancel_connection` branch; the press handler `return`s on 1, falls through on 0. Note `flow_end_connection` can itself return `-1` when the validator or a duplicate rejects the add (`src/flow_model.h:1187-1192`) — that is STILL a completion-on-a-node (the gesture targeted a node, `on_connect_end` carries the attempted target), so it consumes. The consume/fall-through split is decided by *where the gesture landed* (node distinct from source vs not), not by *whether an edge was added*; this matches the helper's own end-vs-cancel branch exactly and needs no second hit-test.

(b) *Pane-press mid-flight — and the full fall-through surface.* Today pane-press cancels and consumes; the cancel branch (`src/flow_input.h:47`) returns 0 under the new rule, so the press falls through, arms (`src/flow_input.h:104` sets `mouse_down = 1`), and on release the classifier's pane branch fires `on_pane_click` (`src/flow_input.h:291`). Decision: **yes, pane-cancel now fires `on_pane_click`** — this is the consistency the brief argues for. The whole point of a uniform rule is that anything the connection did NOT complete on becomes an ordinary press, so the fall-through surface is wider than just pane + edge. Spelled out so the spec matches the implementation: a cancel landing on the **source node body** (the `tnode == conn_node` self case) falls through to `flow_hit_node` (`src/flow_input.h:140`), so `down_node` = the source and the release fires **`on_node_click`** on it (`src/flow_input.h:267-269`); a mid-flight **right-click on the source** falls through to the node-context dispatch and fires **`on_node_context`** (`src/flow_input.h:85-88`); and a fall-through press that lands on ANOTHER node's source handle reaches `flow_begin_connection` (`src/flow_input.h:108-119`) — firing the cancel's `on_connect_end` THEN a fresh `on_connect_start` in one press. All three are consequences of the single rule (xyflow likewise clicks the underlying element after the gesture ends), not special cases; none needs extra code beyond the `int` return.

(c) *Right-click mid-flight.* The right-click branch (`src/flow_input.h:84`) cancels then returns, suppressing `on_edge_context`. Decision: **apply the identical rule** — cancel, then fall through to the existing node-context / edge-context dispatch (`src/flow_input.h:85-92`). A right-click is never a connection completion (it does not call `flow_end_connection`), so it ALWAYS falls through. The `on_connect_end` from the cancel fires before `on_edge_context` (or `on_node_context`), locking the same end-then-event order as the left path.

(d) *Double-resolution / re-entry.* The fall-through path must not re-enter resolve. After `flow_end_connection` (`src/flow_model.h:1178`) or `flow_cancel_connection` (`src/flow_model.h:1198`) clears `conn_active = 0`, the arming block's only `conn_active` test is already behind us, and `flow_begin_connection` is reached only on a fresh source-handle hit — so a cancelled press that lands on an edge cell arms a reconnect-or-node-or-pane press, never a re-entry of `flow__resolve_connection_at`. The `flow_cancel_connection` guard `if (!f->conn_active) return;` (`src/flow_model.h:1196`) means even a redundant second call is a silent no-op. No re-entry is possible: resolution runs to completion (clearing the flag) BEFORE control reaches the arming code.

(e) *connectOnClick (click-to-connect).* The second click that completes a click-to-connect gesture runs through this same gate (`src/flow_input.h:97`). When that second click lands on a target node it resolves via `flow_end_connection` → returns 1 → consumes, exactly as a drag-completion does; click-to-connect keeps working. Only a second click on EMPTY space (which already cancelled the gesture) now additionally reports the pane/edge click — the desired symmetry, not a regression.

**The unified rule (one sentence, applied to both mouse buttons):** *a mid-flight press consumes the press iff the resolution COMPLETED the connection on a target node distinct from the source; a resolution that CANCELS (empty pane, edge cell, source, self — and every right-click) falls through to normal press classification, whatever element that cell normally resolves to (pane, edge, node body, or even another node's source handle).* No contradiction surfaced: the consume-vs-fall-through axis aligns perfectly with the helper's existing end-vs-cancel branch, so the rule is implementable as a single `int` return with no extra hit-testing and no special cases. The package is therefore coherent and need not be cut on analysis grounds (the sign-off cut, if taken, would be a scope decision, not a correctness one).

Ordering guarantee: because resolution (and thus `on_connect_end`, fired synchronously inside `flow_end_connection`/`flow_cancel_connection`) fully precedes the fall-through arming, and the edge/pane/node event fires on the LATER release (or, for context, inside the same right-press dispatch), `on_connect_end` is always observed before any edge/pane/node event for the same physical gesture. The test locks this via a single ordered event log.

**Test plan.**

INVERT and extend the existing `CROSS-EVENT PIN` block in `tests/test_connect.c:376-406`. The block's header comment and its two pinning asserts (`:400` end-fires, `:404` edge-click-does-NOT-fire) are rewritten; no Makefile change (`test_connect` already in TESTS=). Add an ordered event-log fixture (e.g. a small `int log[]` / `int logn` that each callback appends a tag to: `END`, `ECLICK`, `ECTX`, `PANE`, `NCLICK`) alongside the existing `lc_reset` / `on_eclick` / `on_cend` harness so order is asserted, not just counts. The new state — the log buffer AND the new counters (`node_click_fires`, `pane_fires`, `ectx_fires`) — MUST be zeroed in `lc_reset` (`tests/test_connect.c:27-30`); these are file-static counters shared across every block in the file, so an un-reset addition leaks into the blocks above.

  1. **Edge press mid-flight now fires both, in order (the inversion).** Reuse the block's `c`→`d` edge `e2` and the mid-cell `mid` (`tests/test_connect.c:386-394`; `mid` is derived from `flow_edge_endpoint_screen`, not hardcoded, so it stays self-consistent with the node geometry). Wire `on_connect_end`, `on_edge_click` into the ordered log. Press `A:out` to go in flight (assert `start_fires == 1`, `src/flow_input.h` begin path). Press the edge mid-cell, then release on the same cell. Assert `end_fires == 1` AND `eclick_fires == 1` (was 0). Assert the log is exactly `{END, ECLICK}` — `on_connect_end` strictly before `on_edge_click`. Assert `end_eid == -1` (resolved as cancel: `flow_hit_node(f, mid) == -1`, the mid cell sits between `c`'s footprint and `d`'s footprint, no node under it) and that `e2` is now the selected edge (`flow_select_edge` ran in the fall-through release, `src/flow_input.h:280`).
  2. **Successful node-completion press does NOT also node-click.** Place source `A` and a target node `T` whose template carries a TARGET/BOTH handle (default-template nodes do — cf. `tests/test_connect.c:243` `LEFT@(30,6)`). Press `A:out` (in flight), then press `T`'s body. Assert `end_fires == 1` and `end_eid != -1` (edge added). Assert NO `on_node_click` fired (`node_click_fires == 0`) and `flow_edge_count` increased by exactly 1 — the completion was consumed, the node-click classifier (`src/flow_input.h:267-269`) never ran.
  3. **Pane-cancel press fires `on_pane_click`.** Wire `on_pane_click` into the log. Press `A:out` (in flight), then press a clearly empty cell (no node, no edge — assert `flow_hit_node == -1` and `flow_hit_edge(... ,1) == -1` as a precondition). Release on that cell. Assert `end_fires == 1`, `end_eid == -1`, `pane_fires == 1`, and the log is `{END, PANE}`.
  4. **Right-click mid-flight fires `on_edge_context` after cancel.** Wire `on_edge_context` and `on_connect_end` into the log. Press `A:out` (in flight) via a left-press. Send a right-press (button 2) on the edge mid-cell. Assert `end_fires == 1` (cancel from `src/flow_input.h:84`), `end_eid == -1`, `ectx_fires == 1` with the reported edge id `== e2`, and the log is `{END, ECTX}` — end strictly before context.
  5. **connectOnClick completion stays consumed.** Begin a click-to-connect (press-release on `A:out` arming `conn_active` without a drag, the existing `src/flow_input.h:242` armed-click path). Then press the target node `T` (the same default-template target from test 2, so its LEFT target handle lets `flow_end_connection` add the edge — `src/flow_model.h:1181-1185`). Assert `end_fires == 1`, `end_eid != -1`, edge count +1, and NO `on_node_click` and NO `on_edge_click` fired — the completing second click is consumed exactly like a drag completion.
  6. **No double-resolution.** In test 1's flow, after the in-flight cancel-and-fall-through, assert `flow_connecting(f) == 0` immediately after the resolving press, and assert `end_fires` stays `1` through the release (resolution ran once; the fall-through never re-entered `flow__resolve_connection_at`).

**Acceptance.**
  - The inverted `CROSS-EVENT PIN` block passes: mid-flight edge press fires `on_connect_end` THEN `on_edge_click`, asserted by an ordered log (not counts alone).
  - Successful node-completion and connectOnClick-completion presses fire NO `on_node_click` / `on_edge_click` (consumed); cancel presses (pane, edge cell) and all mid-flight right-clicks fall through and fire the matching edge/pane/node/context event AFTER `on_connect_end`.
  - `flow_connecting(f) == 0` after every resolving press; no test triggers a second resolve.
  - `make test` green (whole suite); ASan + UBSan whole-suite clean for the new asserts.
  - All EXISTING goldens byte-identical (this package fires only callbacks; it adds no render output, no statusbar hint, no new golden).
  - `flow.h` regenerated via `tools/amalgamate.sh` (edit only `src/flow_input.h`; no module list change).
  - `make demos` warning-free (no demo is touched by this package; the demo-side showcase, if any, lands in the increment's final integration pass).
  - One commit, sole author, no trailers.

**Depends on.** nothing hard. Lands cleanly on current `main`. If it ships in the same increment as `validator-load-suspend`, note that a validator-rejected completion still consumes (decision (a)); confirm against that package's suspend semantics, but no code dependency.

**Conflicts with.**
  - `node-search`, `keyboard-move`, `copy-paste`, `tab-focus` — these bind KEYS (`flow_dispatch_key` / `flow_feed`); this package touches only the MOUSE press path in `flow_input.h`. No overlap in the same function, but all five edit `src/flow_input.h` / its sibling key files; whichever lands later rebases trivially (disjoint hunks).
  - `marquee-world-anchor` — also edits `src/flow_input.h`, specifically the motion branch (`src/flow_input.h:189-199`) and a new `struct flow` zero-init field; this package edits the PRESS branch (`:83-97`) and `flow__resolve_connection_at` (`:40-49`). Disjoint hunks, but a mutual conflict note: both packages and `marquee-world-anchor` rebase against each other's `flow_input.h` line shifts.
  - `viewport-culling` — render-loop only; no shared lines, but flagged for the shared `src/flow_input.h` file if its culling reads the press path (it does not in the pinned design).
  - Any future package that reorders the button-0 or button-2 PRESS classification, or that changes `flow__resolve_connection_at`'s end-vs-cancel branch — none other in this set does.

**Carry-overs fixed.**
  - Retires the `#6 deferral` recorded at `tests/test_connect.c:376-379` (the cross-event pin), replacing the "press consumed, no edge event" contract with the xyflow-aligned "end then edge event" fall-through, and extends the same fall-through to the pane, node-body, and right-click mid-flight paths for a uniform rule.

---

## Execution record (post-execution addendum, 2026-06-05)

All eleven packages + the integration pass landed on branch `increment-5`, one commit each,
in spine order — including all three stretch packages (kept at sign-off):

| # | id | commit |
|---|----|--------|
| 1 | `minimap-visible-bounds` | `abe88f8` |
| 2 | `validator-load-suspend` | `b2bf177` |
| 3 | `marquee-world-anchor` | `3747539` |
| 4 | `view-frame` | `c943d31` |
| 5 | `tab-focus` | `3d437ba` |
| 6 | `keyboard-move` | `5ac1989` |
| 7 | `copy-paste` | `19a4a5d` |
| 8 | `helper-lines` | `3236985` |
| 9 | `viewport-culling` | `9015e15` |
| 10 | `node-search` | `df226a5` |
| 11 | `edge-events-inflight` | `e5a1052` |
| — | integration pass | `79e976b` |

Suite grew 25 → 30 test files (test_focus, test_clipboard, test_helper, test_culling,
test_search); three new goldens (`render_focus`, `render_helper_snap`, `cull_crossing_edge`);
every pre-existing golden byte-identical throughout.

**Deviations from this spec, recorded in the package commit bodies:**

- **#3** — the screen-space `marquee_anchor` field was REMOVED, not kept: post-fix it had
  zero readers, and the spec's keep-rationale ("avoids re-deriving screen from world") is
  contradicted by the render box, which must re-derive after a pan anyway.
- **#10** — the `label` vtable append forced edits outside the Files-touched list: a
  `-Wmissing-field-initializers` sweep caught `demos/topo.c` (gained a REAL `dev_label`
  accessor — devices are now searchable), `tests/test_json.c` and `tests/test_culling.c`
  (explicit NULLs). New standing gate: `make test` output swept for WARNINGS, not just
  failures — an ABI-append is exactly the change that introduces them silently.
- **#11** — the old consumed-press contract was pinned in a SECOND test site this spec's
  plan did not list (`test_connect`'s connectOnClick cancel-on-empty block), inverted
  coherently. The fall-through inherits the FULL click path — events AND side effects
  (pane-cancel clears the selection) — locked by test.
- **§3/§4/§9/§10 prose** — four cross-package consistency corrections produced at
  spec-writing time failed to merge into the committed doc (assembly bug); repaired
  in-place above, each marked *(post-execution correction)*.

**Known limitations carried forward (increment-6 candidate material):** live-demo `q`
quits even inside the palette (`flow_run` checks `q` before `flow_feed`); pasted group
containers re-measure to content size (the `flow_load` convention — app-side re-derive);
trailing-edge helper guides draw at the shared boundary column (half-open convention);
the v1 palette does not modal-block Tab/Shift-arrows (full capture needs byte-count CSI
parsing in the demo hook).
