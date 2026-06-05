# flow — Increment-6 EXECUTION handoff (spec signed off; package #1 of 8 landed)

**Read this, then the spec, then execute package #2.** The planning phase is DONE this
time — unlike the inc-5/inc-6 session handoffs, there is nothing to settle and nothing to
spec. The single source of truth for every package is
`docs/superpowers/plans/2026-06-05-flow-increment-6-workpackages.md` (committed `13964c2`,
887 lines): theme + current state + execution order + cross-cutting rules + 10 signed-off
open questions + 8 package sections. Re-verify its line-number citations at implementation
time (earlier packages shift later lines — #1's edit already shifted `src/flow_model.h`).

## ⚠ Commit attribution (standing rule)

**NO `Co-Authored-By` trailers. Not Claude, not anyone.** dovas-net is the sole author —
end the commit message after the body. (Auto-memory: `commit-no-coauthor`.)

## Where things stand

- Branch: **`increment-6` @ `ea7c1dd`** (package #1 landed). Branched from `main` @
  `13964c2` (the spec docs commit). `main` is ahead of `origin/main` by 41 commits —
  **push is the user's call, never push unasked.** Working tree clean except untracked
  `.claude/` (tooling — NEVER stage it).
- Suite: **30 test files, all green** (test_clipboard grew to 81 asserts); whole-suite
  ASan/UBSan clean; goldens locked byte-identical; demos warning-free.
- USER SIGN-OFF (given 2026-06-05, in-session): the full 8-package spine INCLUDING the
  #8 stretch; ALL 10 open-question recommendations as written in the spec; cadence =
  **CONFIRM WITH THE USER BETWEEN PACKAGES** (commit, report, stop — do not run ahead).

## The spine (signed off — sizes/blocks in the spec's Execution order table)

1. ✅ `paste-group-remeasure` — **LANDED `ea7c1dd`**: w/h restore in the
   `flow__paste_snaps` mint loop, folded into the existing `if (pn)` block beside the
   `FLOW_EXTENT_PARENT` carry. test_clipboard blocks 13–17. Behavioral-RED verified
   (10 failures, `got 0` / `got 5,3`).
2. ▶ **NEXT: `helper-trailing-edge` [S]** — trailing-guide convention fix. THE
   increment's only golden re-mint, realized by RE-TARGETING test_helper scene #8 from a
   leading to a trailing snap (the existing `render_helper_snap` scene is a LEADING snap
   the fix would NOT change — writer amendment, recorded in the spec section).
3. `quit-routing` [S] — delete the raw `q` pre-scan (`src/flow_run.h:82`), add the `q`
   dispatch built-in. **Call advisor for a pre-design check here** — first run-loop touch;
   the advisor flagged this as the next call that earns its keep.
4. `redraw-clock` [L] — THE foundation: poll loop + `flow_tick`/`flow_ticks`/
   `flow_set_tick_ms` + recomputed `flow__frames_armed` predicate (returns 0 in v1).
   New `tests/test_clock.c`, `TESTS=` 30 → 31. EINTR→continue is load-bearing (resize).
5. `animated-edges` [M] — `FLOW_ANIMATED` = bit 32u; dash phase on PATH-CELL INDEX
   `(c + tick) % 2`; new `tests/test_animated.c`, `TESTS=` 31 → 32; #5 adds the first
   `||` clause to the predicate AND updates test_clock's case-4 assertion.
6. `palette-modal-capture` [S/M] — `flow_set_key_hook_modal(f, on)`; modal = hook sees
   every non-mouse byte first, unconsumed input DROPPED. Headline: Delete (`\x1b[3~`)
   destroying the selection through an open palette.
7. `devtools-hud` [M] — `flow_undo_depth`/`flow_redo_depth`/`flow_top_op` (top = LAST op
   of a coalesced gesture); tests in **test_undo** (NOT test_render — frame amendment);
   HUD itself is demo-side `on_overlay`; toggle key is an integration-pass pick.
8. `tick-autopan` [S, stretch] — synthetic-motion replay per tick at `f->last_cursor`;
   extends the predicate with the in-flight-drag clause; extends test_autopan.
— Final integration pass (single commit): palette-goes-modal, animation showcase,
  HUD toggle, demo help text, accumulated commit-body deferrals.

## Per-package discipline (exactly as #1 ran)

1. Read the spec section IN FULL + the entry-point code. TDD: tests FIRST →
   verify RED (right failures, right reasons) → minimal implement → GREEN.
2. Gates before commit, every package: `make test` (all stems); whole-suite ASan/UBSan
   (`for t in tests/test_*.c; do cc -std=c11 -Wall -Wextra
   -fsanitize=address,undefined -fno-sanitize-recover=all -g "$t" -o /tmp/x -lm &&
   /tmp/x; done`); warnings sweep `make test 2>&1 | grep -i warn` → ONLY
   `tests/test_model.c:8` "missing field 'save'" (a COMPILER diagnostic of the vtable's
   first missing field — 'save' does NOT appear in the file text; an inc-6 spec verifier
   once "fixed" this wrongly); `make demos` warning-free; `git status` shows NO snapshot
   changes except deliberate re-mints recorded in the spec (#2's `render_helper_snap` is
   the only one).
3. **Advisor before substantive design and before each commit.** This session it caught:
   the spine checkpoint belonging BEFORE the writer fan-out, the frames-armed
   field-vs-predicate two-voice residual (3 spots + a bogus future-code citation), and
   the test_model.c:8 carve-out mis-correction. Findings are leads — verify before acting.
4. Commit per package: `feat(flow): <id> — <one-line> (inc-6 #N)` + body with deliberate
   scope calls AND deviations from the spec (#1's precedent: the if-block fold, the
   "inert iff data unchanged at paste" precision). Stage explicitly; never `git add .`.
5. STOP after each commit; report; wait for the user's go-ahead (signed-off cadence).

## Wrinkles (this session's additions to the standing list)

- `flow.h` is GENERATED — edit `src/*.h`; `make test` regenerates. LSP diagnostics on
  `src/*.h` fragments ("unknown type name") are NOISE — modules only compile amalgamated.
- The spec references its own pins by NAME ("the Statusbar pin in Cross-cutting rules"),
  never by line number. Keep it that way in commit bodies too.
- `flow_paste` offsets by `clip.gen+1` (cascades); `flow_duplicate_selection` always +1.
- The handoff list inherited from inc-5 still applies verbatim: rect-convention trio
  (intersects CLOSED, contains half-open), constant-screen-size footprints (zoom<1
  under-cover trap), `flow_cellbuf_put` overwrites attr (post-passes OR into
  `cb.cells[].attr`), stray-`u`-pops-the-ADD in RED states, ISIG stays on,
  amalgamate `modules=` unchanged (NO new module in inc-6 either), statusbar cols=30
  prefix lock (NO statusbar appends this increment — pinned).

## To resume

> Continue flow increment-6 execution. Read
> `docs/superpowers/plans/2026-06-05-flow-increment-6-execution-handoff.md`, then the
> spec (`2026-06-05-flow-increment-6-workpackages.md`). Package #1 is landed; execute
> package #2 `helper-trailing-edge` next — TDD red-first, full gates, advisor before
> commit, one commit, NO trailers — then STOP and confirm with me before #3. The spine,
> all 10 open-question recommendations, and the confirm-between-packages cadence are
> already signed off; do not re-ask.
