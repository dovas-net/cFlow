# flow — Increment-3 session handoff (post-#7; remaining work, for an ULTRACODE session)

**Read this, then execute the rest of the increment-3 spine: #8 `auto-layout`, #9
`flowchart-demo`, then the final integration pass.** The user executes the spine
package-by-package, TDD, ONE commit per package, confirming the next step between packages.
The next session runs under **ultracode** (multi-agent orchestration) — see "How to use
agents on this" at the bottom; the discipline (TDD, gates, one commit per package) is
unchanged, agents or not.

## ⚠ Commit attribution (standing rule — overrides everything older)

**NO `Co-Authored-By` trailers on commits. Not Claude, not anyone.** The user (dovas-net) is
the sole author via git config — end the commit message after the body. (Also persisted in
auto-memory as `commit-no-coauthor`.)

## Where things stand

Branch: `increment-3`. Spine order and status:

1. `serialize` — ✅ `f3f87a0`
2. `straight-edge` — ✅ `498d071`
3. `groups` — ✅ `438ea0d`
4. `events` — ✅ `142fcde`
5. `space-pan` — ✅ `c8a518c`
6. `auto-pan` — ✅ `051cc62`
7. `undo-redo` — ✅ `74b6045` (built this session; 154-assert test_undo; whole suite ASan/UBSan clean)
8. **`auto-layout` — ⬅ NEXT (not started)** `[XL]`
9. `flowchart-demo` — not started `[S]` (HARD-gated on #8)
10. **Final integration pass** — the accumulated deferred wiring (list below), then branch finish (merge/PR — user's call)

Working tree clean except untracked `.claude/` (tooling — NEVER stage it) and this handoff doc.
Suite: **20 tests**, all green; goldens locked & byte-identical; demos build.

## The spec (authoritative)

`docs/superpowers/plans/2026-06-03-flow-increment-3-workpackages.md`. Each package section has
**Entry points / API additions / Design notes / Test plan / Acceptance / Depends on**. Follow it.

- **#8 `auto-layout`** is at **lines 1085–1239**. Summary: NEW `src/flow_layout.h` — a stateless
  model→positions transform behind `flow_layout(f, flow_layout_opts)` with a mode enum
  (`FLOW_LAYOUT_FORCE` = deterministic Fruchterman-Reingold, `FLOW_LAYOUT_LAYERED` = Sugiyama:
  longest-path rank → barycenter order → coordinate assign), plus thin spec-literal wrappers
  `flow_layout_force`/`flow_layout_layered`. Positions written ONLY via `flow_move_node`;
  re-measure first; optional `fit_after` → `flow_fit_view(f, margin)`; group-local layout for
  children; determinism is a HARD requirement (insertion-order iteration, index/id tie-breaks,
  circular RNG-free initial placement, float math rounded to cells only at commit). UBSan trap
  called out in the spec: epsilon-clamp pairwise distance (`d = max(d, 0.5f)`) before dividing.
- **#9 `flowchart-demo`** is at **lines 1241–1313**. Glue, not engine: `demos/flowchart.c`
  (build via the Makefile `demos:` rule), startup layered layout + fit, a labeled group, keys
  `l`/`g`/`G` via `flow_bind_key`, optional headless `tests/test_flowchart.c`. No new library symbol.

## ⚠ #8 wrinkles — where the spec text is STALE vs. the landed code (verified this session)

The #8 spec section was written before groups/undo landed. Three corrections:

1. **`flow_move_node` is ABSOLUTE-in** (groups, `src/flow_model.h`): it takes a world-absolute
   target and stores parent-relative itself. So layout's group-local pass computes child
   positions in parent-LOCAL space but must COMMIT them as `parent_abs + local` through
   `flow_move_node` — do NOT pass parent-relative coords to it (the spec's "committed with
   parent-relative pos" phrasing predates the absolute-in contract; its own parenthetical
   acknowledges this). `flow_node_abs(f, parent)` gives the offset.
2. **The undo seam is now #8's to wire**: spec #7 lines 1041–1042 and #8 lines 1196–1198 both
   leave it as a documented seam — "auto-layout wraps its commit in a txn (or undo wraps
   `flow_layout`)" — and undo landed first, so the bracket falls to #8. Without it, an N-node layout journals N separate
   undo steps (and a >128-node layout would churn the whole default-cap journal). Fix is two
   lines in `flow_layout`: bracket the commit loop with `flow__undo_begin(f); … flow__undo_end(f);`
   — both are declared in `flow_model.h`'s header section and callable from `flow_layout.h`
   (amalgamated later, same TU). MOVE ops coalesce per node id ONLY inside an open txn, so the
   bracket is what makes the whole layout ONE undo step. Add a test: layout → `f->journal.n`
   grew by exactly 1 → one `flow_undo` restores every pre-layout position.
3. **Module insertion point**: `tools/amalgamate.sh:5` `modules=` is an explicit ordered list
   (NOT a glob) and now reads
   `flow_head flow_geom flow_cell flow_model flow_undo flow_view flow_route flow_types flow_render flow_json flow_input flow_term flow_run`.
   Insert `flow_layout` AFTER `flow_view`, BEFORE `flow_route` (per spec) — i.e. between
   `flow_view` and `flow_route` in that list. A module may only call what an EARLIER module
   defined; layout needs `flow_move_node`/`flow_measure_node`/`flow_bounds`/`flow_fit_view`/
   accessors (all in `flow_model.h`) and the undo brackets (declared `flow_model.h`, defined
   `flow_model.h` impl) — all earlier. ✓

Other #8 facts that interact with landed code:
- `flow_measure_node` writes (`n->w/h`) and viewport changes are NOT journaled — only
  `flow_move_node` records. That's correct/intended for layout.
- Determinism tests run layout twice → two journal commands; harmless.
- **NEW goldens**: `layout_layered_lr.txt` / `layout_layered_tb.txt` — `flowtest_snapshot`
  CREATES a missing golden on first run and reports `[snapshot created] — verify it manually`.
  Eyeball them, then COMMIT them with the package. Existing goldens stay locked: a layout
  package must leave `git status tests/snapshots/` showing ONLY the two new files.
- FORCE mode: invariant assertions only (connected-closer, no shared cells, finite, reproducible
  bit-for-bit for same seed) — NO float goldens (platform-fragile). The spec's test plan
  (lines 1202–1214) is the checklist.
- Do NOT add fields to `struct flow`; do NOT touch `flow_model/render/input/run` (acceptance,
  line 1219). All layout typedefs live in `src/flow_layout.h`.

## #9 facts

- Key occupancy as of #7 — built-ins in `flow_dispatch_key` (`src/flow_model.h`, AFTER the
  registry loop, so `flow_bind_key` overrides win): `x` `n` `f` `?` Space `u` Ctrl-r(`\x12`)
  Delete(`\x1b[3~`). `flow_feed` additionally consumes `+`/`=`/`-`/`_` (zoom), arrows (pan),
  lone ESC (cancel connection); `flow_run` reserves `q` (quit). **`l` `g` `G` are FREE** for the
  demo's bind_key registrations as the spec suggests.
- Demo skeleton templates: `demos/topo.c` / `demos/hello_flow.c` (flow_new → register_defaults
  → build graph → callbacks/overlay → flow_run). Makefile `demos:` rule is append-only.
- The `group` node type has no save/load hooks and no handles (containers unconnectable, v1).
- `u`/Ctrl-r already work in every demo for free (built-ins via flow_feed → dispatch).

## Final integration pass (after #9 — single commit, the accumulated deferrals)

Recorded in commit bodies as deliberate deferrals; this is the consolidated list:

1. `demos/topo.c` showcase wiring for: groups, events, space-pan, auto-pan, undo, and one
   `flow_layout` showcase call (#8 defers its demo touch here too).
2. Statusbar help line: mention Space (pan) and `u`/Ctrl-r (undo/redo). ⚠ Golden risk is
   NARROW (verified): the only statusbar golden is `render_statusbar.txt`, rendered at
   cols=30 (`tests/test_keys.c:146`), so only the first ~28 visible columns of the help
   string (` n:add  x:del  f:fit  ?:help`) are locked. APPENDING new hints past column 30
   leaves that golden byte-identical (real terminals are wider, so the hints still show);
   editing the locked prefix means deliberately regenerating it (delete + rerun + eyeball
   + commit).
3. Space-pan: statusbar "pan mode" hint; Esc-to-exit alias.
4. Auto-pan: public `flow_set_autopan(f, margin, speed)` setter (apps currently have no
   disable/tune knob; fields `f->autopan_margin/speed` exist, defaults 3/2 in `flow_new`).
5. (vNext, NOT this pass: run-loop-ticked auto-pan continuation — documented follow-up only.)

## Build / test workflow — DO THIS EXACTLY (learned, non-obvious)

- **`flow.h` is GENERATED — never hand-edit it.** Edit `src/*.h`, then `sh tools/amalgamate.sh`.
  Tests/demos compile against `flow.h`. (Makefile's `flow.h:` target wildcard-deps on `src/*.h`,
  so `make test` regenerates automatically; new `src/flow_layout.h` is picked up by the wildcard
  but MUST also be added to the amalgamate `modules=` list — the script does not glob.)
- Run the suite: `make test` (20 tests now; #8 makes it 21). New test file → append its stem to
  `Makefile` `TESTS=`.
- **ASan/UBSan gate required by every package's acceptance**, run across the WHOLE suite
  (shared model code means everything is affected):
  `cc -std=c11 -Wall -Wextra -fsanitize=address,undefined -fno-sanitize-recover=all -g tests/test_X.c -o /tmp/x -lm && /tmp/x`
- **zsh quirk:** an unquoted shell var is NOT word-split, so `cc $FLAGS ...` passes all flags as
  one bogus argument. Inline the flags, or use `${=VAR}`.
- Snapshot goldens in `tests/snapshots/` are committed and LOCKED — existing ones must stay
  byte-identical; #8 deliberately ADDS two (see above).
- **Pre-existing benign warning:** `tests/test_model.c:8` "missing field 'save' initializer" —
  trailing zero-init, fine. OUT OF SCOPE; do not touch.
- Tests `#define FLOW_IMPLEMENTATION` then include `flow.h`, so they can poke `struct flow`
  internals directly (e.g. `f->journal.n` step-counting in test_undo — reuse that pattern for
  #8's one-undo-step assertion).
- Commit: `feat(flow): <id> — <one-line summary>` + a body recording deliberate scope calls.
  **No trailers.** Stage explicitly (src files + flow.h + Makefile + tools/amalgamate.sh +
  tests/... + new goldens); never `git add .`; never stage `.claude/`.

## TDD discipline (the user expects it)

1. Write the test file first. Undefined `flow_layout` → compile-RED (clang errors on implicit
   declarations). Then decls + stubs → behavioral RED. Then implement → GREEN.
2. Full suite + whole-suite ASan/UBSan + `make demos` + goldens check before claiming done.
3. Call the `advisor` tool BEFORE substantive design work and AGAIN before committing — it
   earns its keep (this session it caught that FLOW_CMD_REMOVE_EDGE was implemented but never
   exercised by any test; previous sessions it caught a callback-contract bug and a wrong
   default). Fold fixes in TDD-style.

## #7 surface that #8/#9 build on (settled this session)

- Journal: `struct flow` gained a `journal` block (`items/n/cap`, `redo/rn/rcap`, `limit`
  default 128, `applying`, `suppress`, `txn_depth`, `txn_base`). Public:
  `flow_undo/flow_redo/flow_can_undo/flow_can_redo/flow_set_undo_limit` (limit 0 disables).
  Internal txn brackets: `flow__undo_begin/end` (nestable; no-ops while replaying).
- Every mutator records (gated on `!applying && !suppress && limit != 0`): add/remove
  node+edge, move (ABSOLUTE coords, coalesced per id in open txn), reconnect, set-label,
  reparent. REMOVE_NODE = one whole-subtree snapshot. `flow_load` clears the journal and
  suppresses recording during rebuild.
- Replay is silent by design: observer callbacks (`on_nodes_delete`/`on_selection_change`)
  are cb_suppress-gated during undo/redo.
- Gesture brackets live in `flow_handle_mouse` (drag arm/release + reconnect arm/release) —
  #8 must NOT touch these; layout adds its own bracket pair inside `flow_layout` only.

## How to use agents on this (ultracode notes)

- **Keep the spine sequential**: #8 → commit → #9 → commit → integration pass → commit. One
  package per commit is a user requirement, not a style choice. Don't parallelize ACROSS
  packages.
- **Within #8**, the package is one new file (`src/flow_layout.h`) + one test file — parallel
  EDITING buys nothing (single-file contention), but parallel VERIFICATION buys a lot:
  fan out reviewers/refuters over (a) determinism (run-twice bit-identity, tie-breaks),
  (b) UBSan traps (zero-distance, NaN/inf, empty/single-node), (c) groups composition
  (absolute-in commit, parent untouched), (d) undo seam (exactly one step, restore-all),
  (e) spec-acceptance conformance (lines 1216–1226 as a checklist). Adversarial verify
  findings before acting on them.
- **Read-only fan-out is safe anytime** (spec extraction, cross-checking claims against code).
  Worktree isolation is only needed if agents must mutate files concurrently — avoid that here.
- Run gates (suite/ASan/demos/goldens) in the MAIN session before each commit; don't trust an
  agent's claim that gates pass — re-run them.

## To resume

> Continue the flow increment-3 spine. Read
> `docs/superpowers/plans/2026-06-03-flow-increment-3-session-handoff.md`, then implement work
> package #8 `auto-layout` (spec lines 1085–1239) via TDD and commit it (NO co-author
> trailers). Then, after confirmation: #9 `flowchart-demo` (lines 1241–1313), then the final
> integration pass.
