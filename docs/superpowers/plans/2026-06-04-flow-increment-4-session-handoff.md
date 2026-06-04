# flow — Increment-4 session handoff (post-inc-3 merge; the Events & graph API increment)

**Read this, then execute the increment-4 spine** — 11 packages + a final integration pass, spec'd
in `docs/superpowers/plans/2026-06-04-flow-increment-4-workpackages.md` (authoritative; each
package has Entry points / API additions / Design notes / Test plan / Acceptance / Depends on).
Execute package-by-package, TDD, ONE commit per package, confirming between packages. Works under
**ultracode** (agent notes at the bottom) or solo; the discipline is unchanged either way.

## ⚠ Commit attribution (standing rule — overrides everything older)

**NO `Co-Authored-By` trailers on commits. Not Claude, not anyone.** The user (dovas-net) is the
sole author via git config — end the commit message after the body. (Also persisted in
auto-memory as `commit-no-coauthor`.)

## Where things stand

Branch: `main` @ `0d7d3a6` — **increment-3 is complete and merged** (fast-forward; the
`increment-3` branch is deleted). The full spine landed: serialize, straight-edge, groups,
events, space-pan, auto-pan, undo-redo, auto-layout (`3c75368`), flowchart-demo (`b85b3ce`),
integration pass (`0d7d3a6`). Local `main` is ahead of `origin/main` — push is the user's call.

Suite: **22 tests**, all green; whole-suite ASan/UBSan clean; goldens locked & byte-identical;
three demos build (`hello_flow`, `topo`, `flowchart`). Working tree clean except untracked
`.claude/` (tooling — NEVER stage it).

Start increment-4 on a fresh branch (e.g. `increment-4`) from `main`.

## The spine (spec is authoritative; this is the map)

| # | id | size | block |
|---|----|------|-------|
| 1 | `esc-selection` | S | A foundations |
| 2 | `marquee-autopan` | S | A |
| 3 | `extent-clamps` | S | A |
| 4 | `parent-extent` | S | A |
| 5 | `viewport-events` | S | B observers |
| 6 | `edge-events` | S | B |
| 7 | `connect-lifecycle` | M | B |
| 8 | `graph-traversal` | M | C query API (NEW module `src/flow_query.h`) |
| 9 | `valid-connection` | S | C |
| 10 | `intersect-query` | S | C (HARD-gated on #8 — same module) |
| 11 | `hidden` | M | D (only golden-risk package → last) |
| — | final integration pass | — | accumulated deferrals + demo wiring |

Pinned order #3 → #5 (extent clamps land first; viewport-events relocates them inside its
`flow__view_set` seam). The cross-cutting rules section of the spec pins flag values
(`FLOW_HIDDEN=8u`, `FLOW_EXTENT_PARENT=16u`), the `flow_callbacks` append-before-`user` rule,
the hidden model/view layering, and the engine-level validator divergence — read it first.

## Shortlist drift (inventory vs. landed code — verified 2026-06-04)

The feature inventory's "Candidate shortlist for increment 4+" was written BEFORE the inc-3
integration pass landed. Two corrections:

1. **Shortlist #2 (`flow_set_autopan`) is DONE** — landed in `0d7d3a6` with clamping + tests
   (`tests/test_autopan.c`, the setter block). Do not re-plan it.
2. **Shortlist #1 is HALF done** — "Esc exits" landed (`0d7d3a6`: lone Esc exits space-pan, with
   a documented read()-boundary trade-off comment in `flow_feed`); "Esc clears selection" did NOT
   land — it is increment-4 package #1.

## Wrinkles — facts verified this session that inc-4 packages build on

- **`flow_feed`'s lone-ESC branch** (`src/flow_run.h`) now carries an ACCEPTED-trade-off comment
  (CSI split at a read() boundary reads as lone ESC; do NOT "fix" it — the stricter check breaks
  the common 1-byte-read case; an adversarial reviewer already proposed and lost that argument).
  Package #1 extends THIS branch.
- **`test_autopan.c:130-137`** is the "marquee drag near the edge does NOT auto-pan" block, and
  `src/flow_input.h:8` + `:55` comments say marquee never auto-pans — package #2 deliberately
  INVERTS that contract (test + comments updated with it; record in the commit body).
- **Key occupancy:** library built-ins unchanged (`x n f ? Space u \x12 Delete` in dispatch;
  `+/=/-/_` arrows lone-ESC in feed; `q` in run). `l`/`g`/`G` are bound by the DEMOS only
  (`topo.c`, `flowchart.c`) — the library keeps them free; inc-4 adds NO new built-in keys.
- **Statusbar help string** is now `" n:add  x:del  f:fit  ?:help  q:quit  SPC:pan  u:undo
  ^r:redo "`, and while `space_held` the bar shows a PAN-mode line instead. The
  `render_statusbar` golden (cols=30) locks only the first ~28 visible columns — APPEND past
  column 30, never edit the prefix.
- **`flow_callbacks` is zero-init append-safe** — `flow_callbacks cb = {0}` everywhere; packages
  #5/#6/#7 append fields BEFORE the trailing `user` (cross-cutting rule pins the order).
- **Flags-not-persisted precedent:** node/edge `flags` don't survive `flow_save`/`flow_load`
  (selection already doesn't); `hidden` (#11) inherits that contract documented. Related landed
  wrinkle: group containers persist x/y/parent but NOT w/h or label — `demos/topo.c` re-derives
  them post-load (the reconstruction loop). #11's save/load test should not be surprised by it.
- **Not-journaled precedent:** selection and viewport changes are deliberately NOT journaled
  (spec §11); `hidden` flag writes (#11) and viewport events (#5) follow it.
- **amalgamate `modules=`** (`tools/amalgamate.sh:5`) current value:
  `flow_head flow_geom flow_cell flow_model flow_undo flow_view flow_layout flow_route
  flow_types flow_render flow_json flow_input flow_term flow_run`.
  Package #8 inserts `flow_query` AFTER `flow_undo`, BEFORE `flow_view` (the only modules= edit
  in the increment; #10 extends the same file with no further edit).
- **Spec line-number citations** were verified against `main` @ `0d7d3a6` — but earlier packages
  shift later line numbers; re-grep at implementation time rather than trusting them blind.

## Final integration pass (after #11 — single commit, accumulate as you go)

Deferrals recorded in package commit bodies, plus the wiring named in the spec header:
prevent-cycles validation in `demos/flowchart.c` (#8 reachability + #9 validator), edge
context-menu/ticker entry in `demos/topo.c` (#6), hidden-toggle showcase (#11), statusbar hints
if any (append-only). Then branch finish (merge/PR — user's call).

## Build / test workflow — DO THIS EXACTLY (unchanged from inc-3, counts updated)

- **`flow.h` is GENERATED — never hand-edit it.** Edit `src/*.h`, then `sh tools/amalgamate.sh`
  (`make test` regenerates automatically; a NEW src module must ALSO be added to the
  `modules=` list — the script does not glob).
- Run the suite: `make test` (22 tests now; #8 makes it 23 via `test_query`). New test file →
  append its stem to `Makefile` `TESTS=`.
- **ASan/UBSan gate required by every package's acceptance**, across the WHOLE suite:
  `cc -std=c11 -Wall -Wextra -fsanitize=address,undefined -fno-sanitize-recover=all -g
  tests/test_X.c -o /tmp/x -lm && /tmp/x`
- **zsh quirk:** an unquoted shell var is NOT word-split — inline the flags or use `${=VAR}`.
- Snapshot goldens in `tests/snapshots/` are committed and LOCKED byte-identical. #11 may ADD
  `render_hidden`; nothing else in the set should touch snapshots.
- **Pre-existing benign warning:** `tests/test_model.c:8` "missing field 'save' initializer" —
  OUT OF SCOPE; do not touch.
- Tests `#define FLOW_IMPLEMENTATION` and may poke `struct flow` internals (e.g. `f->journal.n`
  step-counting — reuse for event/validator no-journal assertions).
- Commit: `feat(flow): <id> — <one-line summary>` + a body recording deliberate scope calls.
  **No trailers.** Stage explicitly; never `git add .`; never stage `.claude/`.

## TDD discipline (the user expects it)

1. Test first → compile-RED or behavioral-RED → implement → GREEN. If a stub creates a snapshot
   golden during RED, DELETE it before the real implementation runs (learned in inc-3: stub
   goldens are garbage and `flowtest_snapshot` happily locks them).
2. Full suite + whole-suite ASan/UBSan + `make demos` + goldens check before claiming done.
3. Call the `advisor` tool BEFORE substantive design work and AGAIN before committing. This
   session it caught a commit-hygiene set and correctly pushed back on a wrong adversarial-review
   fix; previous sessions it caught untested ops and contract bugs.

## How to use agents on this (ultracode notes)

- **Keep the spine sequential** — one package per commit is a user requirement. Don't
  parallelize ACROSS packages.
- **Within a package**, parallel VERIFICATION pays: fan out adversarial reviewers over the
  package's acceptance bullets + the riskiest seams, then VERIFY each finding before acting —
  inc-3's fan-outs produced both true catches (a user-reachable crash, a persistence hole) and
  one wrong fix proposal that had to be argued down. Findings are leads, not verdicts.
- Run gates (suite/ASan/demos/goldens) in the MAIN session before each commit; don't trust an
  agent's claim that gates pass — re-run them.
- The spec itself was written by a verified fan-out (writers grounded in landed code, per-package
  citation verifiers, a cross-package consistency pass) — trust its structure, re-verify its line
  numbers as you go.

## To resume

> Continue flow into increment-4. Read
> `docs/superpowers/plans/2026-06-04-flow-increment-4-session-handoff.md`, branch `increment-4`
> from `main`, then implement package #1 `esc-selection` per
> `docs/superpowers/plans/2026-06-04-flow-increment-4-workpackages.md` via TDD and commit it
> (NO co-author trailers). Then, after confirmation: #2 onward in spine order.
