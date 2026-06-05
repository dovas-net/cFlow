# flow — Increment-6 EXECUTION handoff (spec signed off; packages #1–#3 of 8 landed)

**Read this, then the spec, then execute package #4.** The planning phase is DONE —
there is nothing to settle and nothing to spec. The single source of truth for every
package is `docs/superpowers/plans/2026-06-05-flow-increment-6-workpackages.md`
(committed `13964c2`, 887 lines): theme + current state + execution order +
cross-cutting rules + 10 signed-off open questions + 8 package sections. Re-verify its
line-number citations at implementation time — three packages have now shifted lines
(`flow_dispatch_key` starts at `src/flow_model.h:1322`, not the spec's 1316; #3's `q`
built-in added ~4 lines after the `d` duplicate line; `src/flow_run.h:82` no longer
exists — the raw scan is GONE).

## ⚠ Commit attribution (standing rule)

**NO `Co-Authored-By` trailers. Not Claude, not anyone.** dovas-net is the sole author —
end the commit message after the body. (Auto-memory: `commit-no-coauthor`.)

## Where things stand

- Branch: **`increment-6` @ `0ed7a43`** (packages #1–#3 landed). Branched from `main` @
  `13964c2` (the spec docs commit). `main` was 41 commits ahead of `origin/main` at
  branch time — **push is the user's call, never push unasked.** Working tree clean
  except untracked `.claude/` (tooling — NEVER stage it).
- Suite: **30 test files, all green** (test_keys 69, test_helper 43, test_clipboard 81
  asserts); whole-suite ASan/UBSan clean; demos warning-free.
- Goldens: `render_helper_snap.txt` was re-minted in #2 (the increment's ONLY planned
  re-mint — landed). Everything else locked byte-identical; #3 confirmed zero golden
  drift (it renders nothing).
- USER SIGN-OFF (given 2026-06-05, in-session): the full 8-package spine INCLUDING the
  #8 stretch; ALL 10 open-question recommendations as written in the spec; cadence =
  **CONFIRM WITH THE USER BETWEEN PACKAGES** (commit, report, stop — do not run ahead).

## The spine (signed off — sizes/blocks in the spec's Execution order table)

1. ✅ `paste-group-remeasure` — **LANDED `ea7c1dd`**: w/h restore in the
   `flow__paste_snaps` mint loop, folded into the existing `if (pn)` block beside the
   `FLOW_EXTENT_PARENT` carry. Behavioral-RED verified (10 failures).
2. ✅ `helper-trailing-edge` — **LANDED `4a6d0be`**: trailing guides normalized to the
   drawn border cell (far edge − 1) at RECORD time; snap match/delta stay on one-past;
   dedup compares normalized-to-normalized. Scene #8 re-targeted leading→trailing (the
   golden re-mint). ⚠ test_helper scene #12 creates the leading candidate FIRST with a
   width-1 dragged box — deliberate (commit body explains); do NOT "tidy" the order.
3. ✅ `quit-routing` — **LANDED `0ed7a43`**: raw `q` pre-scan DELETED from `flow_run`;
   `q` is a dispatch built-in (`f->running = 0; return 1;`) BEHIND the hook gate and
   registry — modal-vetoable, app-rebindable. Closes inc-5 known-limitation #1. The
   palette's "typing q quits mid-search" bug is closed for the printable path by
   analysis; showcase wiring stays in the final integration pass (no demos/ edit).
4. ▶ **NEXT: `redraw-clock` [L]** — THE foundation: poll loop + `flow_tick`/
   `flow_ticks`/`flow_set_tick_ms` + recomputed `flow__frames_armed` predicate
   (returns 0 in v1). New `tests/test_clock.c`, `TESTS=` 30 → 31. EINTR→continue is
   load-bearing (resize). Spec pins an advisor pre-design check is NOT re-required —
   but it's [L] and the only loop rewrite of the increment; calling advisor before
   the design is still the house style. **#3 already landed: the loop body has NO raw
   scan — #4 must NOT reintroduce any byte-level quit handling** (spec, Conflicts).
5. `animated-edges` [M] — `FLOW_ANIMATED` = bit 32u; dash phase on PATH-CELL INDEX
   `(c + tick) % 2`; new `tests/test_animated.c`, `TESTS=` 31 → 32; #5 adds the first
   `||` clause to the predicate AND updates test_clock's case-4 assertion.
6. `palette-modal-capture` [S/M] — `flow_set_key_hook_modal(f, on)`; modal = hook sees
   every non-mouse byte first, unconsumed input DROPPED. Headline: Delete (`\x1b[3~`)
   destroying the selection through an open palette. (#3 is landed, so test 7's `q`
   capture is now buildable; #6 must keep the `q` built-in reachable when NOT modal.)
7. `devtools-hud` [M] — `flow_undo_depth`/`flow_redo_depth`/`flow_top_op` (top = LAST op
   of a coalesced gesture); tests in **test_undo** (NOT test_render — frame amendment);
   HUD itself is demo-side `on_overlay`; toggle key is an integration-pass pick.
8. `tick-autopan` [S, stretch] — synthetic-motion replay per tick at `f->last_cursor`;
   extends the predicate with the in-flight-drag clause; extends test_autopan.
— Final integration pass (single commit): palette-goes-modal, animation showcase,
  HUD toggle, demo help text, accumulated commit-body deferrals.

## Per-package discipline (exactly as #1–#3 ran)

1. Read the spec section IN FULL + the entry-point code. TDD: tests FIRST →
   verify RED (right failures, right reasons) → minimal implement → GREEN.
2. Gates before commit, every package: `make test` (all stems); whole-suite ASan/UBSan
   (`for t in tests/test_*.c; do cc -std=c11 -Wall -Wextra
   -fsanitize=address,undefined -fno-sanitize-recover=all -g "$t" -o /tmp/x -lm &&
   /tmp/x; done`); warnings sweep `make test 2>&1 | grep -i warn` → ONLY
   `tests/test_model.c:8` "missing field 'save'" (a COMPILER diagnostic of the vtable's
   first missing field — 'save' does NOT appear in the file text; an inc-6 spec verifier
   once "fixed" this wrongly); `make demos` warning-free; `git status` shows NO snapshot
   changes (the increment's only re-mint, #2's `render_helper_snap`, is already landed).
3. **Advisor before substantive design and before each commit.** Catches so far this
   increment: the spine checkpoint belonging BEFORE the writer fan-out; the frames-armed
   field-vs-predicate two-voice residual; the test_model.c:8 carve-out mis-correction;
   #2's dedup-scene node-order trap; **#3's RED-count calibration — the spec's per-test
   "discriminator" prose can describe a `flow_run`-level mechanism UNREACHABLE from a
   `flow_feed`-driven test (tests 3/4 were green-both-phases regression locks, not RED
   discriminators).** General lesson: trace the pre-patch behavior of each planned
   assert before banking on a RED count; do NOT force-red a lock. Findings are leads —
   verify before acting.
4. Commit per package: `feat(flow): <id> — <one-line> (inc-6 #N)` + body with deliberate
   scope calls AND deviations from the spec (precedents: #1's if-block fold; #2's
   scene-#12 geometry; #3's RED reconciliation + citation shift). Stage explicitly;
   never `git add .`.
5. STOP after each commit; report; wait for the user's go-ahead (signed-off cadence).

## Wrinkles (standing list + this increment's additions)

- `flow.h` is GENERATED — edit `src/*.h`; `make test` regenerates. LSP diagnostics on
  `src/*.h` fragments ("unknown type name") are NOISE — modules only compile amalgamated.
- The spec references its own pins by NAME ("the Statusbar pin in Cross-cutting rules"),
  never by line number. Keep it that way in commit bodies too.
- Quit-test pattern (#3, applies to any future `running` assert): `f->running` is armed
  ONLY by `flow_run` — a quit assert that doesn't set `f->running = 1` first passes
  pre- AND post-patch (false green). The arm is load-bearing.
- `flow_paste` offsets by `clip.gen+1` (cascades); `flow_duplicate_selection` always +1.
- The handoff list inherited from inc-5 still applies verbatim: rect-convention trio
  (intersects CLOSED, contains half-open), constant-screen-size footprints (zoom<1
  under-cover trap), `flow_cellbuf_put` overwrites attr (post-passes OR into
  `cb.cells[].attr`), stray-`u`-pops-the-ADD in RED states, ISIG stays on,
  amalgamate `modules=` unchanged (NO new module in inc-6 either — test_clock is a TEST
  stem, `TESTS=` in the Makefile DOES grow), statusbar cols=30 prefix lock (NO statusbar
  appends this increment — pinned).

## To resume

> Continue flow increment-6 execution. Read
> `docs/superpowers/plans/2026-06-05-flow-increment-6-execution-handoff.md`, then the
> spec (`2026-06-05-flow-increment-6-workpackages.md`) section 4. Packages #1–#3 are
> landed; execute package #4 `redraw-clock` next — advisor pre-design check (it's the
> increment's only [L] and the loop rewrite), TDD red-first, new tests/test_clock.c +
> `TESTS=` 30 → 31, full gates, advisor before commit, one commit, NO trailers — then
> STOP and confirm with me before #5. The spine, all 10 open-question recommendations,
> and the confirm-between-packages cadence are already signed off; do not re-ask.
