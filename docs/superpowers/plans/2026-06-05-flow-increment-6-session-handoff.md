# flow — Increment-6 session handoff (inc-5 COMPLETE on branch `increment-5`; merge pending)

**Read this, then PLAN increment-6 first** — same as last time: no workpackages spec
exists for inc-6. The next session's first tasks are (1) settling the `increment-5`
branch (merge to `main` is the USER'S call — fast-forward, 13 commits including the
spec docs commit), then (2) writing `2026-06-XX-flow-increment-6-workpackages.md` via
the verified fan-out (drift re-verification FIRST — it caught layout + autopan landed
and dropped animated-edges for cause last time; writers grounded in landed code;
per-package citation verifiers; cross-package consistency pass — **and this time
VERIFY the consistency fixes actually merge into the assembled doc**: inc-5's
assembly silently dropped all four `fixed_sections`, repaired post-hoc), then user
sign-off on the spine, then branch `increment-6`.

## ⚠ Commit attribution (standing rule — overrides everything older)

**NO `Co-Authored-By` trailers on commits. Not Claude, not anyone.** The user
(dovas-net) is the sole author — end the commit message after the body. (Persisted
in auto-memory as `commit-no-coauthor`.)

## Where things stand

Branch: `increment-5` @ the increment-5 docs commit — **all 11 packages + the
integration pass landed**, one commit per package, all three stretch packages kept:
minimap-visible-bounds (`abe88f8`), validator-load-suspend (`b2bf177`),
marquee-world-anchor (`3747539`), view-frame (`c943d31`), tab-focus (`3d437ba`),
keyboard-move (`5ac1989`), copy-paste (`19a4a5d`), helper-lines (`3236985`),
viewport-culling (`9015e15`), node-search (`df226a5`), edge-events-inflight
(`e5a1052`), integration (`79e976b`). The spec
(`2026-06-04-flow-increment-5-workpackages.md`, committed `e86e3f8` on `main`)
carries a **post-execution Execution record addendum** with the deviation log.
`main` is still ahead of `origin/main` (push is the user's call); `increment-5`
has NOT been merged.

Suite: **30 test files** (inc-5 added test_focus, test_clipboard, test_helper,
test_culling, test_search), all green; whole-suite ASan/UBSan clean; goldens locked
byte-identical with THREE added (`render_focus` — an attr-map golden, R/. per cell;
`render_helper_snap`; `cull_crossing_edge` — deliberately minted PRE-cull as the
output-invariance proof). Demos build warning-free. Working tree clean except
untracked `.claude/` (tooling — NEVER stage it).

## What increment-5 added (seams the next increment builds on)

- **`flow_set_key_hook`** (flow_model.h) — THE modal-input seam: a pre-dispatch GATE
  on `struct flow` (validator pattern) at the top of `flow_dispatch_key`, before
  registry and built-ins. Returns BYTES CONSUMED (0 = pass-through) — the same
  currency dispatch returns and `flow_feed` advances by. Never sees mouse.
- **`label()` vtable accessor** — appended LAST to `flow_node_type` (ABI-append,
  zero-init = unsearchable). default/group/topo-device supply it.
  `flow_find_nodes` (flow_query.h): case-folded substring, model-level, fill-buffer.
- **Framing trio** — `flow_bounds_of` (explicit-id subset bounds, HIDDEN INCLUDED —
  the deliberate model-level divergence from view-level `flow_bounds`, pinned by
  test), `flow_fit_bounds`, `flow_set_center` (zoom<=0 keeps current; clamps zoom
  itself BEFORE the seam — the seam clamps offset only). `flow__fit_rect` is the
  factored one-definition fit math.
- **Keyboard focus** — `int focus_node` id field (NOT a flag; bit 32u still free),
  invalidated on delete AND hide; Tab/`\x1b[Z`/Enter; ESC's branch now has FOUR
  idempotent actions (cancel connection + exit space-pan + clear selection + clear
  focus); focus ring = REVERSE attr post-pass AFTER the handle markers (OR-in keeps
  ◉ glyphs inside the ring); frame-on-focus via flow_set_center iff FULLY offscreen
  (footprint test — render/hit/frame can never disagree).
- **Clipboard** — `clip` sub-struct on `struct flow` (deep snaps; ABS pos resolved
  at copy; labels dup'd, node data ALIASED); paste re-mints via the PUBLIC add path
  (never `flow__insert_node_at`); survives `flow_load` BY OMISSION from
  `flow__graph_reset`; freed once in `flow_free`. Keys y/c/p/d.
- **Helper guides** — `flow_set_helper_lines` opt-in (calloc OFF = landed drag
  byte-identical); snap-then-record two-pass in the single-node drag branch;
  guides honor `flow__node_visible` (a hidden node never seeds a guide).
- **Render-only cull** — screen-FOOTPRINT intersect in the node render loop only
  (never in the shared visibility choke points); edges deliberately not culled.
- **The fall-through rule** (contract reversal, inc-4 #6 deferral closed): a
  mid-flight press is consumed IFF resolution COMPLETED on a target node; every
  cancel falls through to normal classification — `on_connect_end` first, then the
  element event, WITH the full click path's side effects (pane-cancel clears the
  selection — locked by test).

## Increment-6 candidate material

Known limitations recorded in inc-5 commit bodies + the spec addendum:

1. **Live-demo `q` vs modal input**: `flow_run` checks `q` BEFORE `flow_feed`
   (src/flow_run.h run loop), so typing a label containing `q` in the live palette
   quits. A real fix routes quit through the dispatch/hook-aware path — a run-loop
   redesign, possibly paired with the redraw-clock work below.
2. **Animated edges still blocked on a clock**: the run loop blocks on `read()` with
   no timer tick. A poll()-based loop with a redraw interval would unblock marching
   ants AND give modal UIs a caret blink. Sizeable; its own package(s).
3. **Pasted group containers re-measure to content size** (the flow_load
   convention) — a paste of a group shows a collapsed frame until the app re-derives
   (topo's post-load loop is the pattern). Engine-side w/h restore is a design
   decision, not a bug fix.
4. **Palette modal capture**: v1 passes control bytes/CSIs through (Tab focus and
   Shift-arrows act behind an open palette). Full capture = byte-count CSI parsing
   in the demo hook, or an engine-side "modal consumes all" hook mode.
5. **Trailing-edge helper guides** draw at the shared boundary column (x+w, one past
   the box) while leading guides sit on the border cell — revisit if it reads oddly.

Plus the remaining inventory shortlist (RE-VERIFY against landed code first — drift
expected): NodeToolbar/EdgeToolbar (L), theme/palette struct (L), NodeResizer (L),
DevTools HUD (M), self-loop edges (L), whiteboard tools (L), collision/separation
pass (drag-end AABB), richer layout engines, per-element interaction gates
(draggable/selectable/deletable — the candidate-adjacent item from inc-5 planning;
flag bit 32u is still free).

## Wrinkles — facts verified during inc-5 that the next increment builds on

- **`flow_rect_intersects` is CLOSED** (edge-touch counts) — the cull and marquee
  math rely on it; `flow_rect_contains` and the test-side `rects_share_cell` are
  half-open. Three conventions in play; check before reusing.
- **Footprints are CONSTANT-SIZE in screen space** (only position scales with zoom)
  — any world-rect visibility test UNDER-covers at zoom<1 (the #9 trap, locked by
  the fractional-zoom regression test).
- **`flow_cellbuf_put` overwrites attr** — a post-pass that must compose over
  existing glyphs ORs into `cb.cells[...].attr` directly (the focus ring does).
- **The undo journal entry for `u` in tests**: with no nudge/journal entries, a
  stray `u` feed pops the node ADD itself — guard derefs after undo in RED states
  (bit twice: pkg-2 and pkg-6 segfaults in the RED runs, both test-side).
- **`-Wmissing-field-initializers` sweep is now a standing gate**: vtable
  ABI-appends silently warn in every old positional initializer (caught topo.c,
  test_json.c, test_culling.c). `make test 2>&1 | grep -i warn` — only
  `tests/test_model.c:8` ("missing field 'save'") is the carved-out exception, and
  its message must STAY 'save' (clang reports the first missing field).
- **`ISIG` stays on in raw mode** (flow_term clears only ECHO|ICANON) — ^C/^V are
  never bindable; plain letters only.
- **amalgamate `modules=`** unchanged this increment:
  `flow_head flow_geom flow_cell flow_model flow_undo flow_query flow_view
  flow_layout flow_route flow_types flow_render flow_json flow_input flow_term
  flow_run`. Cross-module forward decls follow the `flow_undo` precedent
  (`flow_set_center` is declared in model.h, defined in view.h, same TU).
- **Key occupancy after inc-5** — dispatch built-ins: `Delete x n f ? Space u ^r`
  + `\t \r y c p d`; feed CSI: arrows + `Z` + `1;2A/B/C/D`; zoom `+ = - _`;
  lone-ESC; `q` in flow_run. Demos: `l g G` (topo+flowchart), `h H a /`
  (flowchart). Statusbar tail gained ` Tab:focus` (past the cols=30 lock).
- **`struct flow` additions** (zero-init block): `focus_node` (explicit -1 in
  flow_new), `marquee_anchor_world`, `helper_on` + `helper` guides,
  `key_hook_fn/user`, `clip` (the ONLY heap-owning one — freed in flow_free, NOT
  in flow__graph_reset).

## Build / test workflow — DO THIS EXACTLY (counts updated)

- **`flow.h` is GENERATED — never hand-edit it.** Edit `src/*.h`, then
  `sh tools/amalgamate.sh` (`make test` regenerates automatically).
- Run the suite: `make test` (30 tests). New test file → append its stem to
  `Makefile` `TESTS=`.
- **ASan/UBSan gate required per package, across the WHOLE suite**:
  `cc -std=c11 -Wall -Wextra -fsanitize=address,undefined -fno-sanitize-recover=all
  -g tests/test_X.c -o /tmp/x -lm && /tmp/x` (loop over tests/test_*.c).
- **NEW gate: warnings sweep** — `make test 2>&1 | grep -i warn` must show ONLY the
  carved-out test_model.c:8 'save' warning; `make demos` must be warning-free.
- Snapshots in `tests/snapshots/` are committed and LOCKED byte-identical. The
  snapshot-trap discipline: compile-RED for packages with new API; behavioral-RED
  with mechanical discriminator asserts (and the SNAPSHOT line added only after
  GREEN) when there is no new API to fail compilation on (pkg-1 precedent).
- **Pre-existing benign warning:** `tests/test_model.c:8` — OUT OF SCOPE; its
  message must remain "missing field 'save'".
- Commit: `feat(flow): <id> — <one-line summary>` + a body recording deliberate
  scope calls AND deviations from the spec. **No trailers.** Stage explicitly
  (every package this increment had a "forgettable file" — Makefile or a golden);
  never `git add .`; never `.claude/`.

## TDD discipline (the user expects it)

1. Test first → compile-RED or behavioral-RED → implement → GREEN. Neutralization
   checks for contract asserts (pkg-3 proved its asserts discriminate by reverting
   the fix; the technique caught a vacuous-risk in inc-4 too).
2. Full suite + whole-suite ASan/UBSan + warnings sweep + `make demos` + goldens
   check before claiming done; re-run gates in the MAIN session before each commit.
3. Call the `advisor` tool BEFORE substantive design work and AGAIN before
   committing. This increment it caught: the dead `marquee_anchor` write-only field
   (#3), the focus-ring/handle-marker attr overwrite gap (#5), the missing odd-
   dimension float-halves lock (#4), the test-warning blind spot (#10), and the
   fall-through side-effect surface needing a selection assert (#11). Findings are
   leads, not verdicts — verify before acting.
4. Confirm with the user between packages unless told to run the whole spine (the
   inc-5 user said "run whole spine" — do not assume it carries over).

## To resume

> Continue flow into increment-6. Read
> `docs/superpowers/plans/2026-06-05-flow-increment-6-session-handoff.md`. First
> settle the `increment-5` branch (merge to main is my call — ask), then WRITE the
> increment-6 workpackages spec (re-verify the candidate material against landed
> code; fan out writers/verifiers grounded in the merged state; VERIFY the
> consistency fixes land in the assembled doc this time), commit it as docs, get my
> sign-off on the spine, then branch `increment-6` and execute package-by-package —
> TDD, one package per commit, NO co-author trailers.
