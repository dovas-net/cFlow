# flow — Increment-5 session handoff (post-inc-4 merge; PLANNING NOT YET DONE)

**Read this, then PLAN increment-5 first** — unlike the inc-4 handoff, there is NO
workpackages spec yet. The first task of the next session is writing
`2026-06-XX-flow-increment-5-workpackages.md` (theme + spine + per-package
Entry points / API additions / Design notes / Test plan / Acceptance / Depends on),
grounded in the LANDED code on `main`, then executing it package-by-package. The
inc-4 spec was produced by a verified fan-out — writers grounded in landed code,
per-package citation verifiers, a cross-package consistency pass — and that worked;
repeat it under ultracode, or write it solo with the same per-section grounding.

## ⚠ Commit attribution (standing rule — overrides everything older)

**NO `Co-Authored-By` trailers on commits. Not Claude, not anyone.** The user
(dovas-net) is the sole author via git config — end the commit message after the
body. (Persisted in auto-memory as `commit-no-coauthor`.)

## Where things stand

Branch: `main` @ `160e3d8` — **increment-4 is complete and merged** (fast-forward;
the `increment-4` branch is deleted). The full spine landed, one commit per package:
esc-selection (`5d169f9`), marquee-autopan (`d5437f5`), extent-clamps (`2cd3991`),
parent-extent (`5228a9b`), viewport-events (`ffff3a5`), edge-events (`38a5b0d`),
connect-lifecycle (`43dfb55`), graph-traversal (`0b7acb8`), valid-connection
(`8cb0b49`), intersect-query (`328d73a`), hidden (`af0748b`), integration pass
(`160e3d8`). Local `main` is ahead of `origin/main` by 25 commits — push is the
user's call.

Suite: **25 tests** (inc-4 added `test_extents`, `test_viewport_events`,
`test_query`), all green; whole-suite ASan/UBSan clean; goldens locked &
byte-identical (inc-4's only snapshot delta: the ADDED `render_hidden`); three
demos build warning-free (`hello_flow`, `topo`, `flowchart`). Working tree clean
except untracked `.claude/` (tooling — NEVER stage it).

Start increment-5 on a fresh branch (e.g. `increment-5`) from `main` once the spec
exists.

## What increment-4 added (seams the next increment builds on)

- **`flow__view_set`** (flow_model.h) — THE viewport seam: every ox/oy/zoom write
  (pan, zoom, fit, load-restore, extent re-clamp) routes through it; clamps FIRST
  (translate extent — degenerate axis pins to the d3 centered midpoint), then fires
  `on_viewport_change` iff the viewport actually changed. No depth guard:
  CONVERGING re-entrant callbacks only (documented at the seam).
- **Extents** — `flow_set_node_extent` / `flow_set_translate_extent` +
  `FLOW_EXTENT_PARENT` (16u). Node/parent clamps run in flow_move_node BEFORE
  journaling (undo replays CLAMPED targets), node-extent first, parent second
  (last-applied wins on disjoint ranges). Extents are TRANSIENT (not saved/journaled).
- **`flow_callbacks` tail** (pinned order, zero-init append-safe):
  `…, on_nodes_delete, on_viewport_change, on_edge_click, on_edge_context,
  on_edge_dblclick, on_connect_start, on_connect_end, user`. Future fields keep
  appending BEFORE `user`.
- **Connection validator** — `flow_set_connection_validator`; lives on `struct flow`
  (`validator_fn`/`validator_user`), NOT in callbacks (it's a gate, not an
  observer). Gates add_edge AND reconnect_edge after the structural rejects.
- **`src/flow_query.h`** — NEW module (amalgamate `modules=` slot: AFTER
  `flow_undo`, BEFORE `flow_view`): incomers/outgoers/connected_edges +
  intersecting_nodes/node_intersections. All MODEL-level (hidden included),
  insertion-order, fill-buffer idiom (true count past max; NULL/0 legal).
- **`FLOW_HIDDEN`** (8u) — view-level skip through TWO choke points:
  `flow__node_visible` (render, hit, marquee, bounds, minimap, handles) and
  `flow__edge_visible` (render + hit, with endpoint cascade). Hide deselects
  (nodes: sig-gated event; edges: eventless — see wrinkles). Not journaled/persisted.

## Increment-5 candidate material

Library deferrals recorded in inc-4 commit bodies (all deliberate scope calls):

1. **World-stable marquee** (#2's divergence): `marquee_anchor` is SCREEN-pinned,
   so under sustained edge-band autopan the world rect TRANSLATES instead of
   growing — the selected set is not world-stable. xyflow world-pins the anchor.
   Fix = re-anchor in world coords; touches the marquee motion branch + tests.
2. **Edge events during an in-flight connection** (#6/#7 conflict-note, never
   implementable in inc-4's file scopes): a press on an edge mid-flight is CONSUMED
   by the gesture resolution (`flow__resolve_connection_at` returns). Firing
   on_connect_end THEN the edge event needs a `flow_input.h` press-fall-through
   with analyzed implications for connectOnClick, pane-click, and node-completion.
   The pinned current contract is tested in test_connect ("cross-event pin").
3. **Minimap all-hidden degenerate** (cosmetic): the world-window guard checks
   `f->nnodes`, not visible-count, so an all-hidden + panned-away graph mis-scales
   the viewport rectangle. Half-line fix + test if it ever matters.
4. **Validator × flow_load** (contract documented, could become engine behavior):
   flow_load rebuilds edges THROUGH flow_add_edge (`src/flow_json.h`, the
   `flow_add_edge` call in the edges loop), so an active validator re-gates and can
   SILENTLY DROP loaded edges. v1 contract: validator is transient — set after
   load. A future option is engine-side validator suspension during load.

Plus the [feature inventory](2026-06-03-xyflow-feature-inventory.md) shortlist —
re-verify against landed code before planning (the inc-4 session caught two stale
entries; expect more drift now).

## Wrinkles — facts verified during inc-4 that the next increment builds on

- **`flow__sel_sig` hashes NODE selection only.** Edge-only selection changes fire
  no `on_selection_change` (hide-deselect of an edge is eventless for exactly this
  reason). If edge selection ever needs events, the sig (or a parallel edge sig)
  must change — its own package, with the ids-array contract decided.
- **Edge ENDPOINT cells are the reconnect affordance**, selection NOT required:
  left-press on an endpoint arms a reconnect drag; its no-move release selects the
  edge SILENTLY. Edge click/dblclick events fire on BODY cells only ("routed path
  minus the endpoint cells"). Right-click does NOT arm reconnect, so
  `on_edge_context` DOES fire at endpoints. Tests use mid-path cells via
  `flow_edge_endpoint_screen` midpoints with `flow_hit_edge(...,0)` preconditions.
- **`flow_cancel_connection` is guarded** (`!conn_active` returns silently) — the
  lone-ESC branch calls it unconditionally; without the guard every ESC would fire
  `on_connect_end`. Don't remove it.
- **`on_connect_end` target semantics**: success AND validator/duplicate reject
  pass the ATTEMPTED target node; cancels/empty/self drops pass -1. Fires AFTER
  `flow__undo_end`.
- **`flow_load` fires `on_viewport_change` MID-REBUILD** (viewport restores before
  nodes): the callback must not query nodes. Spec-pinned; don't "fix".
- **Transitive viewport emitters**: `flow_set_zoom_limits` (via its re-clamp) and
  `flow__autopan` (via flow_pan) fire `on_viewport_change` — correct, tested.
- **Snapshot-trap technique (USE IT)**: `flowtest_snapshot` creates-and-passes on
  first run, so a RED run can lock a garbage golden. Inc-4's `hidden` package
  avoided it by making the RED a COMPILE failure (tests call the not-yet-existing
  API), so the suite physically cannot run before the implementation exists; the
  golden is created on the first real GREEN run, mechanically guarded (strchr
  asserts), eyeballed, then re-run to confirm the lock.
- **Spec drift is real**: #10's example coordinates were internally inconsistent
  (its own assertions contradicted its rects). Hand-verify every spec example
  against the geometry/code at implementation time; record corrections in the test
  file comment + commit body.
- **Key occupancy**: library built-ins unchanged (`x n f ? Space u \x12 Delete`
  dispatch; `+/=/-/_` arrows lone-ESC in feed; `q` in run). Demos bind `l g G`
  (both) and now `h H` (flowchart only — hide/show-all). The library added NO new
  built-in keys in inc-4; statusbar string unchanged.
- **amalgamate `modules=`** (tools/amalgamate.sh) current value:
  `flow_head flow_geom flow_cell flow_model flow_undo flow_query flow_view
  flow_layout flow_route flow_types flow_render flow_json flow_input flow_term
  flow_run`. A NEW src module must be added there explicitly (no glob).
- **Carried from inc-3/4 (still true):** the `render_statusbar` golden (cols=30)
  locks only the first ~28 visible columns — statusbar help changes must APPEND
  past column 30, never edit the prefix. Group containers persist x/y/parent but
  NOT w/h or label — `demos/topo.c` re-derives them post-load (the reconstruction
  loop); any group-serialization work must not be surprised by it.

## Build / test workflow — DO THIS EXACTLY (unchanged, counts updated)

- **`flow.h` is GENERATED — never hand-edit it.** Edit `src/*.h`, then
  `sh tools/amalgamate.sh` (`make test` regenerates automatically).
- Run the suite: `make test` (25 tests). New test file → append its stem to
  `Makefile` `TESTS=`.
- **ASan/UBSan gate required per package, across the WHOLE suite**:
  `cc -std=c11 -Wall -Wextra -fsanitize=address,undefined -fno-sanitize-recover=all
  -g tests/test_X.c -o /tmp/x -lm && /tmp/x` (loop over tests/test_*.c).
- **zsh quirk:** an unquoted shell var is NOT word-split — inline flags or `${=VAR}`.
- Snapshots in `tests/snapshots/` are committed and LOCKED byte-identical
  (`render_hidden` now among them). The goldens gate per package: existing files
  untouched; a package ADDING a golden must stage it (an unstaged golden re-creates
  itself on fresh checkouts and locks nothing).
- **Pre-existing benign warning:** `tests/test_model.c:8` "missing field 'save'
  initializer" — OUT OF SCOPE; do not touch.
- Tests `#define FLOW_IMPLEMENTATION` and may poke `struct flow` internals
  (`f->journal.n` step-counting; `f->view.ox` floats — `ASSERT_F` with fabsf).
- Commit: `feat(flow): <id> — <one-line summary>` + a body recording deliberate
  scope calls. **No trailers.** Stage explicitly; never `git add .`; never `.claude/`.

## TDD discipline (the user expects it)

1. Test first → compile-RED or behavioral-RED → implement → GREEN. Prefer
   compile-RED for snapshot-bearing packages (see the snapshot-trap technique).
2. Full suite + whole-suite ASan/UBSan + `make demos` + goldens check before
   claiming done; re-run gates in the MAIN session before each commit.
3. Call the `advisor` tool BEFORE substantive design work and AGAIN before
   committing. This increment it caught: a false acceptance claim (#2's
   world-stability), an unjournaled-target redo bug class (#3, pre-empted), a
   vacuous-test risk proven real by neutralization (#4), an overclaiming
   termination comment (#5), a spec-conflict resolution (#7), an untested
   self-added behavior (#11 edge deselect), and the validator×load footgun (#9).
   Findings are leads, not verdicts — one proposed check was argued down in inc-3;
   verify before acting.
4. Confirm with the user between packages unless told to run the whole spine.

## How to use agents on this (ultracode notes)

- **Keep the spine sequential** — one package per commit is a user requirement.
  Don't parallelize ACROSS packages.
- **Spec-writing is where fan-out pays**: per-package writers grounded in landed
  code, citation verifiers re-checking every file:line, a cross-package
  consistency pass (callback ordering, flag bits, module slots, conflict notes).
  Line citations shift as packages land — re-grep at implementation time.
- Within a package, parallel VERIFICATION over acceptance bullets + risky seams;
  verify each finding before acting. Run gates in the main session yourself.

## To resume

> Continue flow into increment-5. Read
> `docs/superpowers/plans/2026-06-04-flow-increment-5-session-handoff.md`. First
> WRITE the increment-5 workpackages spec (pick the theme from the deferrals +
> re-verified inventory shortlist; fan out writers/verifiers grounded in `main`),
> commit it as docs, get my sign-off on the spine, then branch `increment-5` and
> execute package-by-package — TDD, one package per commit, NO co-author trailers.
