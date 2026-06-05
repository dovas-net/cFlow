# flow — Increment-6 work packages: the run-loop & real-time increment

Eight work packages + a final integration pass turning `flow_run`'s blocking byte-pump into a
**real event loop** — a poll-based tick with a redraw clock — and landing everything that loop
unblocks: animated (marching-ants) edges, dispatch-routed quit that a modal can veto, engine-side
modal key capture (closing the palette's Delete-destroys-selection leak), a DevTools HUD overlay,
and (stretch) tick-driven auto-pan. The two inc-5 deferral fixes open the spine: pasted-group
size restore and the helper-guide trailing-edge convention.

Each package section was drafted against the LANDED code on `main` @ `c0830bb` by a verified
fan-out (drift re-verification of all 14 candidate items → per-package writers grounded in
source → per-package citation verifiers → a cross-package consistency pass applied to THIS
assembled file and re-verified in it); re-verify line numbers at implementation time (earlier
packages shift later lines).

**Drift absorbed** (verified against `main` @ `c0830bb`, 2026-06-05): the full layout subsystem
(LAYERED + FORCE engines) and `flow_set_autopan` are LANDED — the inventory's "unlanded" layout
rows are stale; "richer layout engines" now means strictly the d3-hierarchy/flextree/elk class
and is NOT in this increment. Re-verified OPEN and deliberately deferred to future increments:
Node/EdgeToolbar, theme/palette struct, NodeResizer, self-loop edges, whiteboard tools,
drag-end collision pass, per-element interaction gates (the drift pass recorded
implementation-shape notes for each in the session-handoff trail).

## Current state (what's already built, on `main`)

Increments 1–5 delivered the interactive engine, the programmable platform, and the
keyboard/editing layer — 30 green tests, byte-locked snapshot goldens, three demos
(`hello_flow`, `topo`, `flowchart`):

- **Model & interaction:** nodes/edges with vtable types (incl. `label()`), selection
  (click/modifier/marquee + ESC-clears), drag (multi-drag, reparent, extent clamps, alignment
  helper-lines + snap), port-handle connections + reconnect + lifecycle events + the in-flight
  fall-through rule, zoom/LOD, space-pan, event-driven auto-pan, key registry + built-ins +
  the pre-dispatch key hook (`flow_set_key_hook` — returns bytes consumed, never sees mouse).
- **Keyboard & editing:** Tab/Shift-Tab focus ring (+ frame-on-focus via `flow_set_center`),
  Shift-arrow nudge, y/c/p/d clipboard (deep snaps, paste via public add path, survives
  `flow_load`), `/` search palette (demo) over `flow_find_nodes`.
- **Events & gates:** `flow_callbacks` tail pinned `…, on_nodes_delete, on_viewport_change,
  on_edge_click, on_edge_context, on_edge_dblclick, on_connect_start, on_connect_end, user`;
  connection validator + key hook are GATES on `struct flow`, not callbacks.
- **Queries & visibility:** `flow_query.h` (graph + intersection queries, fill-buffer idiom);
  `FLOW_HIDDEN` through the two choke points; render-only node cull (screen-footprint test).
- **Structure & persistence:** groups, JSON save/load, undo/redo journal, deterministic
  auto-layout (layered + force).
- **Render:** orthogonal + straight routers, minimap (visible-bounds guard), background grid,
  focus-ring post-pass, helper-guide overlay, statusbar (cols=30 golden locks the prefix;
  ` Tab:focus` tail landed).
- **Run loop (the THIS-increment target):** `flow_run` = raw-mode + blocking `read()` pump
  (`src/flow_run.h:74-86`): mouse-CSI parse → raw `q` pre-scan → `flow_feed` → `flow_present`.
  No timer, no tick, no poll; quit is a raw byte scan a hook cannot veto.

## Execution order (recommended)

Four blocks; sequential spine, one commit per package. **Stretch** packages are safely cuttable
at sign-off — no non-stretch package depends on one.

| # | id | size | block |
|---|----|------|-------|
| 1 | `paste-group-remeasure` | S | A — deferral fixes |
| 2 | `helper-trailing-edge` | S | A |
| 3 | `quit-routing` | S | B — run-loop foundation |
| 4 | `redraw-clock` | L | B |
| 5 | `animated-edges` | M | C — unblocked by the clock |
| 6 | `palette-modal-capture` | S/M | C |
| 7 | `devtools-hud` | M | D — chrome |
| 8 | `tick-autopan` | S | D — **stretch** |
| — | final integration pass | — | D |

Hard dependencies: **#5 and #8 require #4** (the tick counter / poll loop). **#3 lands BEFORE
#4** so the loop rework is written against a dispatch-reachable quit (no raw-scan carry-over).
#6 completes the quit story (#3 makes `q` hook-vetoable; #6 makes the palette actually veto it)
but shares no code dependency with #4. Golden risk: #4 must keep every existing golden
byte-identical (the loop is render-invisible); #2 deliberately RE-MINTS `render_helper_snap`
(recorded below); #5 ADDS goldens at fixed tick values.

**Final integration pass** (single commit, after the last surviving package): demo wiring
deferred by individual packages — flowchart palette goes modal (#6), an animated-edge showcase
toggle, a HUD toggle key (#7), demo help-text updates, plus accumulated deferrals recorded in
package commit bodies.

## Cross-cutting rules (pinned — every package honors these)

- **THE DETERMINISM RULE (new this increment, non-negotiable):** wall-clock time NEVER enters
  the model or render layers. Animation state is an explicit tick counter on `struct flow`,
  advanced ONLY by the run loop (or by tests, directly). `flow_render` stays a pure function of
  (model, view, tick) — goldens are minted at pinned tick values and stay byte-reproducible.
  No `time()`, `clock_gettime()`, or `gettimeofday()` outside `src/flow_run.h`.
- **The loop contract (#4):** `poll(STDIN, timeout)` replaces blocking `read()`. On readable:
  read + feed exactly as today (byte semantics unchanged). On timeout: advance the tick and
  re-present IFF something needs frames (animation active, or a future consumer arms it) —
  an idle graph with no animation blocks indefinitely exactly as today (zero idle wakeups).
  The non-IO loop logic (tick advance + present decision) lives in a TESTABLE seam — tests
  drive it directly; `flow_run` stays the only untestable IO shell.
- **Quit routing (#3):** the raw `q` pre-scan (`src/flow_run.h:82`) is DELETED. `q` becomes a
  dispatch BUILT-IN (sets `f->running = 0`) — behind the key hook gate (a modal can swallow it)
  and behind the registry (an app binding `q` overrides quit). `f->running` stays the loop
  condition.
- **Modal capture (#6):** engine-side. A modal-mode hook receives EVERY non-mouse byte
  sequence (CSIs included) before any other consumer; unconsumed input while modal is DROPPED,
  not fall-through. The headline bug it closes: Delete (`\x1b[3~`) reaching
  `flow_delete_selection` through an open palette.
- **Key map.** Input-path order: mouse CSI → `flow_dispatch_key` (hook → registry longest-first
  → built-ins) → feed CSI switch → zoom keys → lone-ESC. Dispatch built-ins after inc-6:
  `Delete`/`x` del, `n` add, `f` fit, `?` help, `Space` pan-toggle, `u` undo, `^r` redo, `\t`
  focus-next, `\r` select-focused, `y c p d` clipboard (landed) **+ `q` quit (#3)**. Feed CSI:
  `A/B/C/D` bare-arrow pan, `Z` focus-prev, `1;2A/B/C/D` Shift-arrow nudge (landed; unchanged).
  Demo bindings: `l g G h H / a` (landed) + integration-pass picks for the animation showcase and
  HUD toggle (collision-checked against this table). `ISIG` stays on — plain letters only.
- **Flags ledger:** #5 consumes **bit 32u as `FLOW_ANIMATED`** (single shared node/edge flag
  namespace, `src/flow_model.h:2`) — the next-free bit becomes 64u. No other package consumes
  a flag bit. (Future per-element interaction gates start at 64u.)
- **`struct flow` additions (zero-init block):** #4's tick counter + interval (`tick`, `tick_ms`);
  #8's `last_cursor` (arming is a recomputed predicate `flow__frames_armed`, no stored armed-state
  field); #6's modal flag (beside the existing `key_hook_fn/user`). All zero-init-safe, none heap-owning,
  none journaled, none saved. The clipboard remains the only heap-owning member.
- **Gates vs observers:** NO `flow_callbacks` field is added this increment — the tick is loop
  state, not an event (no `on_tick` in v1; recorded as a future option). The callbacks tail is
  untouched.
- **Statusbar:** NO package appends a statusbar hint this increment (quit/HUD/animation are
  demo-help or overlay concerns). `render_statusbar` stays byte-identical.
- **Goldens:** #2 re-mints `render_helper_snap` (deliberate, the ONLY re-mint; recorded in its
  section + commit body). #5 mints new goldens at pinned ticks. Every other existing golden
  stays byte-identical at every package boundary.
- **Render compose order** (unchanged contract): background → edges (#5 animates dash phase
  here, tick-derived) → nodes (cull) → focus ring → handles → connection preview → marquee →
  helper guides → minimap → app overlay (#7's HUD draws here) → statusbar.
- **HUD layering (#7):** the HUD is an app-side overlay composed via `on_overlay` — engine
  additions are limited to read-only accessors (journal depths at most); the statusbar and
  engine render passes are untouched.
- **Undo:** nothing in this increment journals — tick state, modal state, and animation flags
  are all transient (precedent: extents are transient). #1 (paste-group size restore) writes
  w/h INSIDE the existing paste undo bracket — one step, no new op kind.
- **JSON:** flags are NEVER emitted by `flow_save` (`src/flow_json.h:7`), so `FLOW_ANIMATED`
  does NOT survive save/load — an app re-arms edges after `flow_load`, the same convention as
  `FLOW_HIDDEN` (verified by the #5 writer); tick and modal state are NEVER saved either.
- **`TESTS=` accounting:** baseline 30 stems. #4 appends `test_clock` →31, #5 appends
  `test_animated` →32 (per-section claims are relative-to-predecessor). #1 extends
  `test_clipboard`, #2 extends `test_helper` (+ golden re-mint), #3 and #6 extend
  `test_keys`/`test_search` respectively, #7 extends `test_undo` (the journal accessors read
  `f->journal`, which `test_undo` already pokes; there is NO render behavior to assert), #8
  extends `test_autopan`.
- **Discipline (unchanged):** TDD red-first (compile-RED where new API exists; behavioral-RED
  with mechanical discriminators otherwise; snapshot lines added only after GREEN); `flow.h` is
  GENERATED (edit `src/`, run `tools/amalgamate.sh`; `modules=` is UNTOUCHED this increment —
  no new module); whole-suite ASan/UBSan gate per package; warnings sweep
  (`make test 2>&1 | grep -i warn` → only the carved-out `tests/test_model.c:8` 'save' line;
  `make demos` warning-free); one commit per package, sole author, NO trailers; never stage
  `.claude/`.

## Open questions for sign-off

Each has a recommendation baked into its section; flagged here because they are user-visible.

1. **#1 restore scope:** UNCONDITIONAL post-add w/h restore for every pasted node
   (recommended — a paste is a faithful clone; provably inert for content-measured types,
   which re-measure from the same `data`) vs a gated manual-size flag (rejected — bit 32u is
   #5's). The companion `flow_load` size leg (JSON v1 carries no w/h; apps re-derive container
   size post-load) stays DELIBERATELY deferred — confirm.
2. **#2 golden re-mint:** accept the deliberate `render_helper_snap` re-mint — recommended.
   The existing golden scene is a LEADING snap the fix would not change, so the package
   RE-TARGETS scene #8 to a trailing snap (the single re-mint then shows the fix: both guides
   land ON their border cells, `x` and `x+w-1`). Zoom≠1 guide inexactness is documented as a
   known limitation, not fixed here.
3. **#3 quit default:** `q` as an engine BUILT-IN (recommended — overridable via registry,
   vetoable via hook) vs leaving quit demo-bound. Riders: `q` CONSUMES even when `running` is
   already 0 (the consume-regardless-of-effect convention); NO public
   `flow_request_quit`/`flow_stop` in v1 — `f->running` stays `flow_run`'s private liveness
   flag (dispatch-q off the loop is a consumed no-op).
4. **#4 tick rate:** default redraw interval when frames are armed (recommendation: 100 ms /
   10 Hz — marching ants read well at TUI cell granularity; configurable via
   `flow_set_tick_ms`).
5. **#4 idle behavior:** with nothing armed, `poll(timeout=-1)` blocks indefinitely
   (recommended — zero idle wakeups, exact today-behavior) vs always ticking. Riders: arming
   is a RECOMPUTED PREDICATE `flow__frames_armed` (recommended over an arm/disarm counter —
   #5 and #8 each add one OR clause, no paired-disarm bookkeeping to strand); NO `on_tick`
   callback in v1 (the tick is a pull getter, `flow_ticks`); split-CSI reassembly across read
   boundaries stays DEFERRED (poll preserves today's byte semantics exactly).
6. **#5 flag vs field:** `FLOW_ANIMATED` as shared-namespace flag bit 32u (recommended) vs a
   per-edge field (ABI-append). RESOLVED rider: flags are never emitted by `flow_save`
   (`src/flow_json.h:7`) — animated state does NOT survive load; apps re-arm (the
   `FLOW_HIDDEN` convention).
7. **#5 ant pattern:** dash phase on PATH-CELL INDEX — `(c + tick) % period`, period 2 —
   NOT coordinate parity (index is router-agnostic, so a diagonal straight edge MARCHES
   instead of blinking); off-phase cells are SKIPPED (background shows through); animation
   suppressed at LOD 1.
8. **#6 modal API shape:** separate boolean setter `flow_set_key_hook_modal(f, on)`
   (recommended — a mode argument would break the 3-arg `flow_set_key_hook` ABI already used
   by tests and demos; a magic hook return is implicit). Riders: ESC-close stays DEMO-HOOK
   policy (the engine contract is policy-free: hook-first + drop-unconsumed); the public
   `flow_dispatch_key` doc gains the modal-drop clause.
9. **#7 HUD scope:** include the minimal read-only journal-introspection API —
   `flow_undo_depth` / `flow_redo_depth` / `flow_top_op` (recommended; the ChangeLogger pane
   is the only HUD leg with engine value) vs demo-only over existing accessors (degrades the
   package to S). Riders: `flow_top_op` of a coalesced gesture returns the LAST op's kind
   (the gesture's final effect); engine tests live in `test_undo` (per the frame's `TESTS=` pin — there is
   no render behavior to assert); the demo HUD toggle key is an integration-pass pick
   (F2 recommended).
10. **#8 stretch:** tick-autopan in or out (requires #4; `src/flow_input.h:58-66` documents
    the follow-up). Riders: coarse arm-at-motion-site (keeps `flow__autopan` unchanged; costs
    at most one no-pan tick when a drag pauses in the interior); reconnect-drag ticks are
    PAN-ONLY (repoint happens on release — no live rubber-band to move); the predicate/seam
    SYMBOL NAMES (`flow__frames_armed`, the tick seam) are #4's to pin — #8 conforms by
    adding its OR clause to whatever #4 names (no stored arm field exists to coordinate).

## Packages

### 1. Paste/duplicate size restore (group-container collapse fix)  `[S]`  ·  id: `paste-group-remeasure`

**Goal.** Pasted and duplicated nodes lose their width/height: the paste core re-mints each node through the public `flow_add_node` path (`src/flow_model.h:1169`), which calls `flow_measure_node` (`src/flow_model.h:596`) and overwrites any size. For the built-in **group** container this is the visible bug — `flow__group_measure` is a write-back no-op (`src/flow_types.h:37`) and `flow_add_node` zeroes the node before measuring (`src/flow_model.h:594`), so a pasted group echoes back **0×0** and collapses (inc-5 known-limitation #3). The clipboard snap already deep-copies the source `w/h` at copy time (`flow__node_snap` embeds the whole `flow_node`, `src/flow_model.h:237`, filled at `src/flow_model.h:1138`). Fix: one localized post-add write in the mint loop copying the snap's `w/h` onto the freshly-added node — mirroring the exact direct-write idiom `flow_group` already uses (`src/flow_model.h:747`) and `flow_load` is the inverse of (`src/flow_json.h:357`). The write lands **inside the existing paste undo bracket** (`flow__undo_begin`/`flow__undo_end`, `src/flow_model.h:1165`/`1198`), and because the `ADD_NODE` op snapshots the LIVE node at undo time (`src/flow_undo.h:64-74`), the restored size round-trips undo/redo with **no new undo op kind**.

**User value.** Copy-paste and duplicate of a group container (and of any manually-resized node) now reproduce the source size instead of collapsing to a degenerate box. Today an app author must hand-re-derive container size after paste exactly as the topo demo already does after load (`demos/topo.c:186-200`); this closes the paste leg of that workaround at the engine. The change is invisible to every node whose size IS its measured size (content-measured types are unaffected — see Design notes), so no existing behavior or golden moves.

**Files touched.**
  - src/flow_model.h (the `flow__paste_snaps` mint loop)
  - tests/test_clipboard.c (extend; `TESTS=` stem unchanged — already listed)

**Entry points (existing functions to extend).**
  - `flow__paste_snaps` mint loop (`src/flow_model.h:1167-1175`) — the single shared paste/duplicate core. Add the `w/h` restore immediately after the `flow_get_node` re-fetch (`src/flow_model.h:1170`) and beside the existing `FLOW_EXTENT_PARENT` carry-forward (`src/flow_model.h:1173`), inside the same `pn` null guard. Both `flow_paste` (`src/flow_model.h:1213`) and `flow_duplicate_selection` (`src/flow_model.h:1221`) route through this one core, so a single edit covers both — the `d` built-in is `flow_duplicate_selection` via `flow_feed` (`tests/test_clipboard.c:236`), confirming duplicate shares the paste path.
  - The mint-loop comment at `src/flow_model.h:1170-1172` ("re-measured by add; manual w/h resizes do NOT survive (the flow_load convention)") is the recorded limitation this package retires — replace it to describe the restore.

**API additions.**
```c
/* No new public API, no new flag, no new undo op kind, no struct flow field,
   no flow_callbacks field. The fix is one private post-add write inside the
   existing flow__paste_snaps mint loop (src/flow_model.h:1167-1175):
       if (pn) { pn->w = ns[i].node.w; pn->h = ns[i].node.h; }
   captured by the ADD_NODE snap already in the paste's undo bracket. */
```

**Design notes.**

*The fix is a post-`add` direct write, mirroring `flow_group` exactly (the seam).* `flow_add_node` always re-measures (`src/flow_model.h:596`), so the ONLY place size can be (re)imposed is AFTER the add returns. `flow_group` already does precisely this: it adds the container (`src/flow_model.h:745`), re-fetches (`src/flow_model.h:746` — "array may have realloc'd"), then direct-writes `g->w = gw; g->h = gh;` (`src/flow_model.h:747`). The paste loop already re-fetches `pn` for the same realloc reason (`src/flow_model.h:1170`) and already does one post-add direct write (the `FLOW_EXTENT_PARENT` carry at `src/flow_model.h:1173`); the `w/h` restore is the same idiom, one line, in the same `if (pn)` guard. The snap supplies the values verbatim: `flow__node_snap.node` is a full `flow_node` (`src/flow_model.h:237`) holding the source `w/h`. **Trap:** re-fetch `pn` AFTER `flow_add_node` (never cache a pointer from before the add) — the add can `flow__grow`/realloc the node array, dangling any prior pointer; the loop already obeys this, the new write must stay inside that re-fetched scope.

*Why the group collapses to 0×0 (NOT 4×3) — the precise mechanism.* The group type DOES register a measure hook, `flow__group_measure` (`src/flow_types.h:37`, in `flow_group_node_type` at `src/flow_types.h:51`), so `flow_measure_node` takes the hook branch (`src/flow_model.h:589`), NOT the `else { n->w = 4; n->h = 3; }` default (`src/flow_model.h:590`). The hook is a write-back no-op (`*w = n->w; *h = n->h;`) whose contract is "echo the caller-set bbox back so an empty body doesn't zero it" (`src/flow_types.h:33-36`). But `flow_add_node` `memset`s the node to zero before measuring (`src/flow_model.h:594`), so the hook reads `n->w==0, n->h==0` and writes 0×0 back. This matches the topo demo's own framing — "a loaded container would measure 0x0" (`demos/topo.c:182-183`) — and is why a "4×3 collapsed box" description (in the source claim) is wrong: a pasted group is a **zero-size** box. The restore overwrites that 0×0 with the snap's true size; correctness does not depend on which measure branch ran, because the post-add write is unconditional.

*Restore is UNCONDITIONAL for every pasted node, and that is correct (the scope decision).* I do NOT gate the restore on type or on a "manual size" flag. The snap holds the EXACT `w/h` the source node had at copy time (`src/flow_model.h:1138`), and a paste/duplicate is a faithful clone — reproducing that size is the definition of correct. For the two affected classes (group containers; default/plain nodes a caller manually resized via `pn->w = …`, the pattern at `tests/test_model.c:151`) the snap size is the truth and re-measure is the bug. For **content-measured hooked types** (e.g. topo's `device`, `dev_measure` at `demos/topo.c:33`, deriving size from `n->data`) the restore is a *no-op-equivalent*: `flow_add_node` re-measures from the SAME borrowed `data` pointer the snap carries (`ns[i].node.data`, passed at `src/flow_model.h:1169`), so the snap's `w/h` already equals what re-measure just produced — writing it back changes nothing. Thus unconditional restore is strictly correct for the broken cases and inert for the working ones. **Trap:** this relies on `data` being passed through to the pasted node (it is — `src/flow_model.h:1169`); if a future change deep-copied `data` and a measure depended on identity, the equality would still hold because measure reads `data`'s CONTENT, not its address.

*Why NOT a `FLOW_MANUAL_SIZE` flag gating `flow_measure_node` (rejected — and frame-forbidden).* The drift seam floated a general flag on bit 32u that suppresses re-measure for app-resized nodes. That is over-scope for this deferral AND collides with the frame's flags ledger: **bit 32u is reserved for `FLOW_ANIMATED` by package #5** (the flags-ledger pin in Cross-cutting rules, `src/flow_model.h:2` — the node-flag enum stops at `FLOW_EXTENT_PARENT = 16u`, next free bit 32u). "No other package consumes a flag bit" (the flags-ledger pin in Cross-cutting rules). A flag would also be a behavior-change to the public `flow_add_node`/`flow_measure_node` contract, far beyond the "localized post-add write" the frame pins for this package (the Undo pin in Cross-cutting rules: "#1 … writes w/h INSIDE the existing paste undo bracket — one step, no new op kind"). The unconditional in-loop restore needs no flag, no enum bit, and touches no public API — it stays an S.

*Undo/redo round-trips the restored size for free (the frame-pinned correctness check).* The frame requires verifying the size lands in the `ADD_NODE` structural snapshot so undo/redo survives (the Undo pin in Cross-cutting rules). It does, by construction: the whole paste is one txn (`flow__undo_begin` at `src/flow_model.h:1165` … `flow__undo_end` at `src/flow_model.h:1198`), and the `ADD_NODE` op snapshots the LIVE node struct at UNDO time, not at record time — `flow__apply_op` does `op->u.node.snap.node = *n;` (`src/flow_undo.h:70`) with the comment "snapshot at UNDO time: captures post-add direct field writes (e.g. flow_group's container w/h)" (`src/flow_undo.h:68-69`). On REDO, `flow__insert_node_at` restores that captured struct verbatim (`f->nodes[at] = s->node;`, `src/flow_undo.h:24`), which now carries the restored `w/h`. So the post-add write is journaled automatically — **no new op kind, no journal plumbing**, exactly as `flow_group`'s container `w/h` already round-trips (the precedent the undo comment names). This is the same mechanism the existing "paste is ONE undo step" test exercises (`tests/test_clipboard.c:119-134`).

*This is the paste leg of the `flow_load` re-measure convention.* `flow_load` deliberately re-measures from restored `data` (`src/flow_json.h:357`, "recompute w/h from restored data") because JSON v1 carries no `w/h` and the group type has no save/load hooks (`src/flow_types.h:48-50`) — so the app re-derives container size post-load (topo's loop, `demos/topo.c:186-200`). Paste is DIFFERENT: the clipboard is in-memory and DOES carry `w/h` (`src/flow_model.h:237`), so paste need not, and now does not, discard it. This package closes the paste gap WITHOUT touching `flow_load` (the JSON-size limitation and the app-side load re-derive stay as-is; that is a separate, deliberately-deferred concern, untouched here).

**Test plan.** Extend `tests/test_clipboard.c` (new numbered blocks appended after block 12; stem already in `TESTS=`). RED strategy per the discipline pin: each assertion is **behavioral-RED with a mechanical discriminator** — before the fix the mint loop has no `w/h` write, so a pasted group/resized node reports the collapsed size and every size assertion below fails on the pre-fix build; they pass only after the one-line restore. No golden involved (paste size is a model query, not a render snapshot).

  1. **Group container size survives paste.** `flow_new(80,24)`, `flow_register_defaults`. `int g = flow_add_node(f,"group",(flow_pt){10,10},NULL);` then `flow_node *gn = flow_get_node(f,g); gn->w = 20; gn->h = 8;` (the `flow_group` post-add idiom, mirrored from `tests/test_model.c:151`). `flow_select_node(f,g,0); flow_copy_selection(f);` `ASSERT_INT(flow_paste(f),1,…)`. Locate the pasted group at abs `(11,11)` via the file's `node_at` helper (`tests/test_clipboard.c:24-31`); assert `flow_get_node(f,pg)->w == 20` and `->h == 8`. **RED:** pre-fix the pasted group measures 0×0 (`src/flow_types.h:37` on a zeroed node), so `w==0 != 20` — discriminator is the literal size, not a glyph.
  2. **Manually-resized default node survives paste.** Add a `"default"` node "A", then `pn->w = 30; pn->h = 5;` (a caller resize that `flow_measure_node` would otherwise overwrite to `len+4 × 3`, i.e. `5 × 3`). Copy, paste, locate the clone, assert `->w == 30 && ->h == 5`. **RED:** pre-fix the clone re-measures to `5 × 3` (`flow__default_measure`, `src/flow_types.h:8-12`), so `30 != 5`. Confirms the fix is not group-special-cased.
  3. **Content-measured (hooked) type is unaffected — restore is inert.** Reuse the default node WITHOUT a manual resize: add "Hello", do NOT touch `w/h`, copy, paste; assert the clone's `->w == flow_get_node(f,src)->w` and equals the measured `strlen("Hello")+4 == 9`. Proves unconditional restore equals re-measure when size IS the measured size (the no-op-equivalent argument) — guards against any regression for ordinary nodes.
  4. **Duplicate (`d`) shares the fix.** Build a sized group (as block 1), `flow_select_node(f,g,0)`, `flow_duplicate_selection(f)` (NOT copy/paste). Assert the duplicate at `(11,11)` has the restored `w/h`. Then drive it through the key path: `flow_select_node(...); flow_feed(f,"d",1);` and assert the next clone keeps size — proving the `d` built-in (which calls `flow_duplicate_selection`, precedent `tests/test_clipboard.c:236`) inherits the fix because both route through `flow__paste_snaps`.
  5. **Size round-trips undo/redo (the frame-pinned check).** Sized group as block 1; `int j0 = f->journal.n; flow_paste(f); ASSERT_INT(f->journal.n, j0+1, "one step")` (precedent `tests/test_clipboard.c:127-129`). `flow_undo(f)`; assert the pasted group is gone (`flow_node_count` back to 1). `flow_redo(f)`; relocate the re-pasted group and assert `->w == 20 && ->h == 8`. **RED-relevant:** proves the restored size is captured by the `ADD_NODE` snap at undo time (`src/flow_undo.h:70`) and reinstated on redo (`src/flow_undo.h:24`) — if the write were outside the txn or lost, redo would show 0×0.
  6. **ASan/UBSan clean.** The restore allocates nothing (two scalar field copies); every block `flow_free`s its graph. Whole-suite ASan+UBSan gate per package.

**Acceptance.**
  - `make test` passes including the new `tests/test_clipboard.c` blocks; `tests/test_clipboard` stays the same `TESTS=` stem (no count change — the `TESTS=` accounting pin in Cross-cutting rules: "#1 extends test_clipboard").
  - Whole-suite ASan + UBSan clean.
  - **Every existing golden byte-identical.** This package touches no render path; pasted-node SIZE is a model field, and content-measured nodes (which the render goldens exercise) are unaffected by the inert restore (block 3). `render_statusbar` untouched (the Statusbar pin in Cross-cutting rules).
  - The retired limitation comment at `src/flow_model.h:1170-1172` is rewritten to describe the restore (no stale "manual w/h resizes do NOT survive" claim left in source).
  - `make demos` warning-free (demos unedited; topo's post-load re-derive loop at `demos/topo.c:186-200` stays — it addresses the separate JSON-no-size case, not paste).
  - `flow.h` regenerated via `tools/amalgamate.sh` (edit lands in `src/flow_model.h`, an existing module; `modules=` untouched per the Discipline pin in Cross-cutting rules).

**Depends on.** Nothing hard. Builds only on landed code: the clipboard snap (`src/flow_model.h:237`, `1138`), the `flow__paste_snaps` core (`src/flow_model.h:1160`), the `ADD_NODE` undo-time snapshot (`src/flow_undo.h:64-74`), and the `flow_group` direct-write precedent (`src/flow_model.h:747`). Lands first in Block A (the Execution order table), so no earlier inc-6 edit shifts its lines.

**Conflicts with.**
  - **#5 `animated-edges`** — flags-ledger interaction, NOT a code conflict: #5 consumes node/edge flag bit **32u as `FLOW_ANIMATED`** (the flags-ledger pin in Cross-cutting rules, enum at `src/flow_model.h:2`). This package deliberately does NOT add any flag (the rejected `FLOW_MANUAL_SIZE` would have taken 32u). Recorded so the consistency pass confirms #1 leaves bit 32u free for #5.
  - No other inc-6 package touches `flow__paste_snaps` (`src/flow_model.h:1160-1201`) or the clipboard. #3/#6 touch dispatch/feed key paths; #4 adds the tick to struct flow's zero-init block (this package adds no struct field); #2/#7/#8 touch render/helper/HUD/autopan. Disjoint regions. As a Block-A package landing first, later same-file (`src/flow_model.h`) packages rebase line offsets off this edit, not the reverse.

**Carry-overs fixed.**
  - Closes inc-5 known-limitation **#3** (pasted group containers collapse). After this, paste and duplicate reproduce the source `w/h` for group containers and manually-sized nodes; the engine no longer requires the app-side post-paste size re-derive. The companion `flow_load` size limitation (JSON v1 carries no `w/h`, app re-derives — `demos/topo.c:182-200`, `src/flow_json.h:357`) is a SEPARATE deferral and stays open, untouched by this package.

---

### 2. Helper-guide trailing-edge convention fix  `[S]`  ·  id: `helper-trailing-edge`

**Goal.** Make alignment-helper guides land on a box's drawn border cell on BOTH edges. Today the leading guide (a candidate's left/top edge, `cr.x` / `cr.y`) is recorded as the box's near-edge world value and projects ONTO the border cell, while the trailing guide (right/bottom edge, `cr.x+cr.w` / `cr.y+cr.h`, `src/flow_input.h:293`) is the rect's far-edge value — one world unit PAST the last drawn border cell (border at `x+w-1`, `src/flow_cell.h:80,83,84`; far edge `x+w`, `src/flow_model.h:799`) — and projects one column/row past the box (`src/flow_render.h:315-323`). This closes inc-5 known-limitation #5 by normalizing the *stored* trailing-guide value to `cr.x+cr.w-1` / `cr.y+cr.h-1` at record time, so a self-aligned pair of identical boxes shows both guides flush on their border cells instead of bracketing the box. Snap MATH is untouched and stays internally consistent.

**User value.** A drag whose right or bottom edge snaps to a neighbor's right/bottom edge currently paints the guide one cell to the right of (or one row below) where the eye expects it — visually it floats off the box it claims to align to, while a left/top snap sits flush. After this fix both directions read identically: the dashed guide overlays the shared border cell. This is the only inconsistency left in the inc-5 helper-line overlay and the last open item in its deferral list.

**Files touched.**
  - src/flow_input.h
  - tests/test_helper.c
  - tests/snapshots/render_helper_snap.txt (deliberate re-mint — the increment's ONLY golden re-mint)

**Entry points (existing functions to extend).**
  - `flow_handle_mouse` single-node-drag guide RECORD sweep (`src/flow_input.h:288-306`) — the second sweep that fills `f->helper.vert[]` / `f->helper.horz[]` after the snap is applied. The vertical store is `src/flow_input.h:294-299`, the horizontal store `src/flow_input.h:300-304`. This is the ONLY place a guide value is written; normalizing here fixes the whole pipeline with no render change.
  - The snap-DELTA math (`src/flow_input.h:273-276` x, `src/flow_input.h:278-282` y) and the snap APPLY (`src/flow_input.h:285-286`) — read but NOT modified. They compare the dragged box's trailing edge `t.x+bw` against the candidate trailing edge `cr.x+cr.w`, both "one-past," so snapping them equal coincides the two right-border cells (each at value−1) correctly. The fix must leave this comparison on the one-past values.
  - `flow_render` guide overlay (`src/flow_render.h:315-323`) — read but NOT modified; it projects each stored world int verbatim via `flow_to_screen` (`src/flow_model.h:887`, a thin wrapper over `flow_project`, `src/flow_geom.h:14-18`) and stamps a full column/row, clamping off-screen guides at `src/flow_render.h:317,322`. Because the fix changes only the stored value, this loop is byte-for-byte unchanged in code.

**API additions.**
```c
/* No new public API. No new exported symbol, no new flag, no flow_callbacks
   field, no struct flow field. The fix is a record-time normalization of two
   existing stored values in flow_handle_mouse (src/flow_input.h:294-304); the
   flow.helper struct (src/flow_model.h:329) keeps its current shape. */
```

**Design notes.**

*The asymmetry, exactly.* A box at world `(x,y)` size `w×h` draws its left/top border at screen `(x,y)` and its right/bottom border at `(x+w-1, y+h-1)` (`src/flow_cell.h:80,83,84`) — it occupies screen columns `[x, x+w-1]`. But `flow_node_rect_abs` returns `{a.x,a.y,n->w,n->h}`, far edge `x+w` (`src/flow_model.h:799`), so `ce[1]=cr.x+cr.w` (`src/flow_input.h:272`) and the recorded trailing value `ex[1]=cr.x+cr.w` (`src/flow_input.h:293`) is ONE past the last drawn border cell. `flow_project` adds no edge bias (`src/flow_geom.h:16`), so at zoom 1 the stored `x+w` projects to screen column `x+w` — one right of the `x+w-1` border. The leading value `cr.x` projects onto the border cell. Genuine, and uncovered by any test (all three guide-value assertions are leading edges: `tests/test_helper.c:51,65,91` assert 10, 10, 5).

*Why record-time normalization (option b), not render-time `sx-1` (option a) — and the trap that breaks the tie.* The drift notes offer two fixes: (a) keep `cr.x+cr.w` stored and subtract 1 at render with a per-guide leading/trailing side tag added to the `helper` struct (`src/flow_model.h:329`); (b) store `cr.x+cr.w-1` at record time and leave render untouched. I pick (b) for three reasons. FIRST, the stored array has NO world-coordinate consumer that (b) would corrupt: a full grep shows `f->helper.vert[]/horz[]` is read ONLY by the render projection (`src/flow_render.h:315-323`), the record/clear sites in `src/flow_input.h`, and tests that assert exact stored ints — there is no "these are world edges" contract to preserve, so the value is purely a render input and shifting the trailing one by 1 is safe. SECOND, (a) widens the `helper` struct and threads a tag through both record and render; (b) is a one-token change at each of two store sites, render stays byte-identical in code, and the frame's "render compose order unchanged" pin holds trivially. THIRD — the tie-breaking trap — BOTH options are exactly correct only at zoom 1 and equally wrong at zoom≠1, so (a) buys no correctness: the box renders at CONSTANT screen size (`n->w × n->h`, the footprint precedent at `src/flow_model.h:826-834`), so its right border is at screen `round(x·z+ox)+w-1`, whereas any world-projected guide is `round((x+w−δ)·z+ox)`; these coincide only at `z=1` for any δ. The leading guide is the lucky exception (it shares the node's top-left projection anchor, so it is flush at all zooms). Since (a)'s `sx-1` and (b)'s `world-1` are identical at zoom 1 and both diverge at other zooms, the simpler change wins. The residual zoom≠1 imperfection is a pre-existing property of projecting a world line against a constant-screen-size box and is explicitly OUT of scope here (recorded below).

*Keep snap math on the one-past values; normalize ONLY the stored value.* The match condition that decides WHETHER an edge coincides (`ex[k] == t.x || ex[k] == t.x + bw`, `src/flow_input.h:295`; `ey[k]` at `src/flow_input.h:300`) and the snap deltas (`src/flow_input.h:274,280`) MUST stay on the un-normalized world values `cr.x+cr.w` / `cr.y+cr.h`, because the dragged box's trailing edge is also expressed one-past as `t.x+bw` / `t.y+bh`. Touching either side would shift the snap target by a cell and re-mint goldens that must stay locked. The change is surgical: compute a stored value `int sv = (k == 1) ? ex[k] - 1 : ex[k];` (k==1 is the trailing edge by construction at `src/flow_input.h:293`) and use `sv` in BOTH the dedup compare (`src/flow_input.h:297`) and the store (`src/flow_input.h:298`); mirror with `ey[k]` for horz (`src/flow_input.h:302-303`). The dedup MUST compare normalized-to-normalized — comparing the new stored `sv` against a raw `ex[k]` would mis-dedup. Leading edges (k==0) are unchanged, so every leading assertion and the leading-snap goldens stay byte-identical.

*The desirable merge this enables.* After normalization, a leading guide at column V and a trailing guide at column V from two different candidates now normalize to the SAME stored value and the existing dedup loop (`src/flow_input.h:297`) collapses them to one line — which is correct, since both want the same screen column. Pre-fix they stored V and V+1 and drew two adjacent lines for what is visually one shared border. No code beyond the normalization is needed; the dedup already handles it once both sides speak the same coordinate.

*Why this re-mints `render_helper_snap` and nothing else.* The existing golden scene (`tests/test_helper.c:147-167`) is a LEADING snap (B.left → A.left=10), which the fix does not touch — left as-is it would stay byte-identical. To make the deliberate, single re-mint the frame pins (the Goldens pin in Cross-cutting rules; open question 2) actually exercise the fix, this package RE-TARGETS scene #8 to a TRAILING snap so the existing golden re-mints with the trailing guide moved one column left, onto the border cell. No NEW golden file is added (keeping "the only re-mint is `render_helper_snap`" literally true); the leading path keeps full coverage through the value assertions in scenes 1–6.

**Test plan.** TDD red-first; `test_helper` already in `TESTS=` (`Makefile:4`) so no Makefile change. Two pieces: a behavioral-RED value assertion (a fresh focused scene) and the deliberate golden re-mint (scene #8 re-targeted).

  1. **RED discriminator — trailing value normalizes (new focused scene, no snapshot).** Reuse the `mk` fixture (`tests/test_helper.c:33-40`): A at world (10,5) 4×3 → right border col 13, far edge 14. Start B (6×5) to the lower-right and drag so B's RIGHT edge snaps to A's right edge: B.left target = 8 (8+6 = 14 = A far edge), grab offset (2,2), `press_at(f,42,22)` then `move_to(f, 8+2, 22)`. Assert `flow_node_abs(f,B).x == 8` (snap landed — proves the UNTOUCHED snap math still coincides the trailing edges), `f->helper.nvert == 1`, and `f->helper.vert[0] == 13`. The 13 is the RED line: the un-fixed build records 14 (`cr.x+cr.w`), so this assertion fails before the normalization and passes after — a mechanical discriminator with no golden. Add the symmetric Y case (B.bottom → A.bottom): A bottom border row 7, far edge 8; assert `f->helper.horz[0] == 7` (pre-fix 8).
  2. **Leading path unchanged (regression guard).** Scenes 1, 2, 4 (`tests/test_helper.c:44-95`) already assert leading guides at 10, 10, 5; they MUST still pass unchanged (the fix never touches k==0). Add one explicit assertion that a leading snap stores the near-edge verbatim with no −1 applied (e.g. re-assert `vert[0] == 10` after a left→left snap) so a reviewer sees the asymmetry of the fix is intentional.
  3. **Dedup merge of coincident leading+trailing (new scene).** Construct a candidate whose RIGHT far-edge world value and another candidate's LEFT edge both resolve to the same border column after normalization, drag so both coincide, and assert `f->helper.nvert == 1` (one merged line), not 2. This guards the "compare normalized-to-normalized" trap in the dedup change; pre-fix it would record two adjacent columns.
  4. **Golden re-mint — scene #8 re-targeted to a trailing guide.** Keep cols=40, rows=12, A at (10,5) 4×3, B at (30,10) 6×5 (`tests/test_helper.c:148-154`). Change the drag so B's RIGHT edge snaps to A's right edge: grab `press_at(f,32,11)` (offset chosen so prospective B.left ≈ 8), `move_to` so B.left = 8; assert `f->helper.nvert == 1` and `f->helper.vert[0] == 13` (the new on-border value). Keep the glyph guard `strstr(s, "\xe2\x95\x8e") != NULL` (`tests/test_helper.c:162`) and the `SNAPSHOT("render_helper_snap", s)` call (`tests/test_helper.c:163`). Regenerate `tests/snapshots/render_helper_snap.txt`: the dashed `╎` column moves from one-past A's right border to ON it (col 13), so A's `─┐` right border and B's `─┐` right border now share the guide column. RED strategy: the snapshot diff IS the red — the un-fixed build draws the guide at col 14, the test's `vert[0]==13` assertion fails first (compile-clean, behavioral-RED), and only after the fix is the new golden staged. The byte change to the golden is the deliberate re-mint; record it in the commit body.
  5. **No collateral golden drift.** Every other snapshot in the suite (all of `tests/test_render.c`, the leading-edge expectations elsewhere) stays byte-identical — the fix only alters trailing-edge stored values, which no other golden exercises. Run the full snapshot suite and confirm exactly one golden file changed (`render_helper_snap.txt`).
  6. **ASan/UBSan clean.** The change allocates nothing (an `int` temp per store); each test graph `flow_free`d. Whole-suite ASan + UBSan per the package gate.

**Acceptance.**
  - `make test` passes including the extended tests/test_helper.c.
  - Whole-suite ASan + UBSan clean (per-package gate).
  - EXACTLY ONE golden changes: `tests/snapshots/render_helper_snap.txt` (the deliberate trailing-edge re-mint). Every other golden byte-identical — verified by a suite-wide snapshot run.
  - The new `vert[0]==13` / `horz[0]==7` assertions FAIL on the pre-fix build and PASS after (red-first proven).
  - Snap behavior unchanged: all leading-snap and no-snap assertions (`tests/test_helper.c:44-145`) pass unchanged; the drag still lands on the same world coordinate (the trailing-snap target world position is identical — only the recorded guide value shifts by 1).
  - `render_statusbar` golden untouched (this package appends no statusbar hint, per the STATUSBAR pin).
  - `make demos` warning-free (no demo touched).
  - flow.h regenerated via tools/amalgamate.sh (edits land in src/flow_input.h, an existing module; `modules=` untouched).

**Depends on.** Nothing hard. Builds only on already-landed inc-5 #8 code: the helper struct (`src/flow_model.h:329`), the `flow_set_helper_lines` opt-in (`src/flow_model.h:1267-1270`), the record sweep (`src/flow_input.h:288-306`), and the guide render overlay (`src/flow_render.h:315-323`).

**Conflicts with.**
  - No code conflict with any other inc-6 package. This package edits ONLY the two guide-store statements in `src/flow_input.h:294-304` and re-mints one golden; nothing else in inc-6 touches `flow_handle_mouse`'s helper-record sweep, the `flow.helper` struct, or `render_helper_snap.txt`. `quit-routing` / `palette-modal-capture` touch the dispatch/feed KEY paths in `flow_input.h` (not the mouse-drag branch); `redraw-clock` / `animated-edges` touch the render loop's edge/tick path (`src/flow_render.h` edges section), disjoint from the guide overlay at `src/flow_render.h:315-323`. If `animated-edges` or a later same-file edit shifts `src/flow_render.h` line offsets, only the citations here move — the guide-overlay code is unchanged by this package, so there is no merge-content conflict.

**Carry-overs fixed.**
  - Closes inc-5 known-limitation #5 (helper-guide trailing-edge convention), recorded at `docs/superpowers/plans/2026-06-05-flow-increment-6-session-handoff.md:94-95`. After this, leading and trailing alignment guides both render flush on the box's drawn border cell at zoom 1; the residual zoom≠1 world-line-vs-constant-screen-size offset (which also affects the pre-existing leading guide identically in spirit) is documented as a separate, out-of-scope limitation rather than silently "fixed."

---

### 3. Dispatch-routed quit (hook-vetoable, registry-overridable)  `[S]`  ·  id: `quit-routing`

**Goal.** Move the quit decision out of `flow_run`'s untestable IO shell and into `flow_dispatch_key`. DELETE the raw `q` pre-scan at `src/flow_run.h:82` (`for (int i = 0; i < n; i++) if (buf[i] == 'q') f->running = 0;`) and ADD a one-byte `q` built-in to the step-(2) chain in `flow_dispatch_key` (`src/flow_model.h:1335-1351`) that sets `f->running = 0` and returns 1. Because that built-in sits BEHIND the step-0 key-hook gate (`src/flow_model.h:1321-1324`) and the registry (`src/flow_model.h:1327-1333`), quit becomes vetoable by a modal (the hook swallows `q`) and overridable by an app (`flow_bind_key(f, "q", …)`) for free — closing inc-5 known-limitation #1: "quit is a raw byte scan a hook cannot veto."

**User value.** A modal (the `/` palette) can now keep `q` as a literal query character instead of quitting the app mid-search, and an embedder can rebind `q` to "are you sure?" or to any other action through the existing registry — neither was possible while the raw pre-scan decided quit before dispatch ran. The change is also what lets #4 rewrite the run loop against a dispatch-reachable quit (no raw-scan carry-over to thread through the new poll path), and it makes the statusbar's existing `q:quit` advertisement (`src/flow_render.h:340`) honest.

**Files touched.**
  - src/flow_run.h
  - src/flow_model.h
  - tests/test_keys.c (extended — no new stem)

**Entry points (existing functions to extend).**
  - `flow_run` (`src/flow_run.h:73-87`) — DELETE the raw pre-scan line at `src/flow_run.h:82`. The loop guard `while (f->running)` (`src/flow_run.h:79`) and the `f->running = 1` arm (`src/flow_run.h:76`) are UNCHANGED; only the byte scan goes. The `flow_feed` call (`src/flow_run.h:83`) and `flow_present` (`src/flow_run.h:84`) stay exactly as today.
  - `flow_dispatch_key` step-(2) built-in chain (`src/flow_model.h:1335-1351`) — ADD `if (seq[0] == 'q') { f->running = 0; return 1; }` as a single-byte built-in. Placement among the other single-byte built-ins (`x`/`n`/`f`/`?`/space/`u`/`\x12`/`\t`/`y`/`c`/`p`/`d`/`\r`) is order-insensitive — each is a disjoint first-byte test and the multi-byte Delete (`src/flow_model.h:1335`) already runs longest-first ahead of them. The new built-in sits below the hook gate (`src/flow_model.h:1321-1324`) and below the registry match (`src/flow_model.h:1327-1333`), inheriting veto and override.
  - The stale `(3) unhandled: q, …` comment at `src/flow_model.h:1352` drops `q` (now handled); `q` no longer falls to `return 0` at `src/flow_model.h:1353`.
  - The stale `flow_feed` call-site comment at `src/flow_run.h:22` ("registry/built-in keys (NOT bare arrows, NOT 'q')") drops the `NOT 'q'` clause — `q` IS a built-in after this package.
  - `f->running` field on `struct flow` (`src/flow_model.h:310`) — written by the new built-in; no new field, no zero-init-block addition.

**API additions.**
```c
/* No new public API. Quit becomes a flow_dispatch_key BUILT-IN (single byte 'q'
   setting f->running = 0), reachable via the same path as every other built-in.
   No flow_request_quit() / flow_stop() is added: f->running has no defined
   meaning outside flow_run's loop (calloc-zero in flow_new, set to 1 only in
   flow_run, src/flow_model.h:525 / src/flow_run.h:76), so a public quit symbol
   would have nothing to act on for a never-ran embedder. Embedders that drive
   their own loop poke f->running (tests do) or bind 'q' via the registry.
   No new flag, no flow_callbacks field, no struct flow field, no exported symbol,
   no Makefile/modules= change. */
```

**Design notes.**

*The corrected mechanism (do NOT inherit the handoff's narration).* The handoff said "the hook never gets a chance to consume `q`." That is FALSE and the section must not repeat it. Today `flow_feed` runs UNCONDITIONALLY right after the scan — there is no `if (f->running)` guard between `src/flow_run.h:82` and `src/flow_run.h:83` — so the key hook DOES execute on a typed `q` and the palette's printable branch DOES consume it (`demos/flowchart.c:152-154` returns 1). The app quits anyway because the raw scan at `src/flow_run.h:82` already cleared `running` and the `while (f->running)` re-check (`src/flow_run.h:79`) fails on the next iteration. Accurate statement: **quit is decided by a raw byte pre-scan that runs independent of and prior to dispatch — even a hook that consumes `q` cannot veto it.** This framing rules out the naive fix: merely moving the scan to AFTER `flow_feed` still quits, because the scan reads the raw `q` byte regardless of whether dispatch consumed it. The real fix removes the raw scan entirely and routes quit through dispatch, so the step-0 hook gate (`src/flow_model.h:1321-1324`) short-circuits it for free.

*Quit moves from the untestable IO shell to the testable seam (why this is the whole point).* Today the ONLY quit mechanism in the engine is the raw scan inside `flow_run` (`src/flow_run.h:82`) — `flow_dispatch_key` has no quit built-in (`q` returns 0 at `src/flow_model.h:1352`) and there is no `flow_request_quit`/`flow_stop`/`flow_quit` anywhere in `src/`. `flow_run` is the one function tests can't drive (raw mode + blocking `read()`). After this package, `flow_feed(f, "q", 1)` reaches the built-in and clears `running`, so quit is exercisable in `test_keys.c` with no terminal. This is also the precondition #4 relies on: when the blocking `read()` becomes `poll()`, there is no raw-scan branch to re-thread — quit already lives in dispatch.

*Q1 — does `q` consume (return 1) when `running` is already 0? YES, unconditionally — no inner guard.* The built-in is `if (seq[0] == 'q') { f->running = 0; return 1; }` with NO `if (f->running)` around it. The codebase's settled convention is that a built-in claiming a key consumes it whether or not the action had an effect: the Enter built-in returns 1 "consumed even with no focus (no-op)" (`src/flow_model.h:1348-1350`), and `f`/`x`/`u` on an empty graph likewise return their byte count. The honest-contract argument clinches it: an embedder calling `flow_dispatch_key` directly and reading the return to decide "did the engine handle this byte" must see 1 for `q`, or they double-handle it (e.g. echo it into their own input). The trap to avoid: gating the return on `running` would make `q` mean "consumed" in `flow_run` but "pass-through" before the loop starts — a state-dependent contract that breaks the `i`-advance accounting in `flow_feed` (`src/flow_run.h:23`, `dk > 0 ? i += dk`).

*Q2 — `f->running` is `flow_run`'s PRIVATE liveness flag; dispatch-`q` on a never-ran embedder is a consumed no-op.* `flow_new` calloc-zeros the whole struct (`src/flow_model.h:525`) and never touches `running`; `flow_run` is the only writer that sets it to 1 (`src/flow_run.h:76`). So for an embedder who never calls `flow_run`, `running` is permanently 0, and dispatch-`q` does `0 → 0` (no observable effect) while still returning 1. This is correct and intended: `running` is the run-loop's "I am alive" bit, not a public quit-request channel. There is deliberately no public API to flip it (see API additions): an embedder driving its own loop reads/writes `running` directly (tests do exactly this, per the pinned DISCIPLINE rule that tests may poke `struct flow` internals) or binds its own `q` semantics via the registry. The trap: do NOT promote `running` to a documented public quit-request field this increment — its semantics are loop-scoped, and #4 is the package that owns the loop's state model.

*Veto and override come for free from the existing chain order.* The dispatch order is hook gate → registry longest-first → built-ins (`src/flow_model.h:1321-1351`). Putting `q` in the built-in tier means: (a) a modal whose hook returns >0 for `q` short-circuits before the built-in ever runs (`src/flow_model.h:1322-1323`) — that is the "modal vetoes quit" property the brief wants; (b) `flow_bind_key(f, "q", app_fn, …)` registers a seq that the registry match claims at `src/flow_model.h:1333` before the built-in tier — that is "app override." Neither needs a new gate or flag; the seams already exist. This mirrors the landed `x`-override precedent exactly (`tests/test_keys.c:120-127`: a custom `x` binding wins over the delete built-in).

*What `q` means for the palette (narrative accuracy for the demo bug, but demo wiring is deferred).* The `/` palette registers `fc_pal_hook` via `flow_set_key_hook` (`demos/flowchart.c:213`). When the palette is OPEN, a typed printable `q` (0x71) hits the hook's printable branch (`demos/flowchart.c:152-154`), is appended to the query, and returns 1 — the hook gate short-circuits, the `q` built-in never runs, the app does NOT quit. When the palette is CLOSED, the hook early-returns 0 (`demos/flowchart.c:133`), dispatch falls through to the new `q` built-in, and the app quits. So this package both (a) makes `q` routable-and-vetoable and (b) closes the inc-5 typing-`q`-in-the-query bug for the palette's printable path. The full modal-capture story (every key/CSI captured while modal, unconsumed input DROPPED not fall-through) is #6's job; demo help-text and any showcase wiring are deferred to the final integration pass. No `demos/` file is edited in THIS package.

*Statusbar is untouched and becomes honest.* `render_statusbar` already advertises `q:quit` at full width (`src/flow_render.h:340`), and the cols=30 golden locks only the `… ?:help` prefix (`src/flow_render.h:335-337`), so `q:quit` sits past column 30 and is not in the golden. Per the Statusbar pin in Cross-cutting rules this package APPENDS no statusbar hint and does not touch `render_statusbar`; the existing advertisement — written assuming the run-loop scan — simply becomes truthful now that dispatch quits. Zero golden change. (Recorded here to preempt a consistency reviewer asking why quit-routing leaves the quit-advertising statusbar alone.)

**Test plan.** Extend `tests/test_keys.c` (frame pin: #3 extends `test_keys`, no new stem; `TESTS=` count unchanged). RED strategy per the discipline pin: behavioral-RED with mechanical discriminators (`q` already exists as a byte, so no compile-RED — the new behavior is what dispatch does with it). Every running assertion sets `f->running = 1` FIRST so the precondition discriminates: today `flow_feed(f, "q", 1)` does NOT clear `running` (the raw scan lives in `flow_run`, not `flow_feed`; dispatch returns 0 and `q` falls to a bare `i++`), so an assert without the explicit arm passes both before and after the change and is a FALSE green.

  1. **INVERT the existing isolation assertion (the RED headline).** The block at `tests/test_keys.c:137-147` currently asserts `flow_dispatch_key(f, "q", 1) == 0` with comment "'q' not consumed". CHANGE the comment ("…arrows still pan, 'q' now quits via dispatch") and CHANGE the assertion to `ASSERT_INT(flow_dispatch_key(f, "q", 1), 1, "'q' consumed by dispatch (quit built-in)")`. Keep the bare-arrow assertion at `tests/test_keys.c:145` (`flow_dispatch_key(f, "\x1b[A", 3) == 0`) intact — arrows are still NOT dispatch built-ins. This is the line that fails RED on the un-patched build.
  2. **Dispatch-`q` clears `running`.** Fresh `flow_new`/`flow_register_defaults`; set `f->running = 1` (poke per discipline pin); `flow_feed(f, "q", 1)`; `ASSERT_INT(f->running, 0, "feed 'q' cleared running via dispatch built-in")`. Discriminator: without the built-in, `running` stays 1 (RED) because `flow_feed` never scanned `q`.
  3. **Hook-consumed `q` does NOT quit (modal veto).** Register a test-local hook that returns 1 for byte `q` and 0 otherwise (mirrors the palette's printable-consume + the hook gate at `src/flow_model.h:1321-1324`) via `flow_set_key_hook`. Set `f->running = 1`; `flow_feed(f, "q", 1)`; `ASSERT_INT(f->running, 1, "hook-swallowed 'q' does NOT quit (modal veto)")`; also assert the hook's own counter incremented to prove it actually ran and consumed. Discriminator: with the raw-scan still present this would be 0 (the old bug); with the built-in correctly gated behind the hook it is 1.
  4. **Registry-bound `q` overrides quit (app override).** `flow_bind_key(f, "q", custom_fn, NULL)` where `custom_fn` bumps a counter (mirror the landed `x`-override test at `tests/test_keys.c:120-127`). Set `f->running = 1`; `flow_feed(f, "q", 1)`; assert the counter is 1 ("custom 'q' binding ran") AND `f->running == 1` ("registry overrides the quit built-in"). Discriminator: the built-in must sit below the registry; if mis-ordered, `running` goes 0.
  5. **Embedder semantics: dispatch-`q` is consumed but a no-op when never-ran (locks Q1+Q2 together).** Fresh graph, do NOT touch `running` (calloc-zero, 0). `ASSERT_INT(flow_dispatch_key(f, "q", 1), 1, "'q' consumed even with running already 0")` and `ASSERT_INT(f->running, 0, "running unchanged: 0 stays 0, no underflow/flip")`. Proves Q1 (consume unconditionally) and Q2 (no defined effect off the loop).
  6. ASan/UBSan clean (whole-suite gate): each sub-case `flow_free`s its graph; the built-in allocates nothing.

**Acceptance.**
  - `make test` passes; `tests/test_keys.c` cases 1–5 pass, and case 1's inverted assertion fails on the pre-patch build (RED→GREEN confirmed).
  - Whole-suite ASan + UBSan clean (per-package gate).
  - ALL existing goldens byte-identical — this package renders nothing and does not touch `render_statusbar`; the cols=30 `render_statusbar` golden (`tests/test_keys.c:158`) and its locked prefix (`tests/test_keys.c:202`, already including `q:quit`) are unchanged.
  - `make demos` warning-free (no `demos/` file is edited).
  - Warnings sweep: `make test 2>&1 | grep -i warn` shows only the carved-out `tests/test_model.c:8` 'save' line.
  - `flow.h` regenerated via `tools/amalgamate.sh` (edits land in `src/flow_run.h` and `src/flow_model.h`, both existing modules; `modules=` UNTOUCHED).
  - Sole author, no trailers; never stage `.claude/`.

**Depends on.** Nothing hard. Builds only on already-landed seams: the step-0 key-hook gate (`src/flow_model.h:1321-1324`, inc-5 #10), the registry match (`src/flow_model.h:1327-1333`), the built-in chain (`src/flow_model.h:1335-1351`), and `f->running` + its loop guard (`src/flow_model.h:310`, `src/flow_run.h:79`). Lands BEFORE #4 (`redraw-clock`) per the execution order so the loop rework is written against a dispatch-reachable quit.

**Conflicts with.**
  - **#4 `redraw-clock`** — shares `src/flow_run.h`'s `flow_run` body. This package DELETES `src/flow_run.h:82`; #4 replaces the blocking `read()` (`src/flow_run.h:80`) with `poll()` and restructures the `while (f->running)` body (`src/flow_run.h:79-85`). #3 lands first (pinned), so #4 rebases onto a `flow_run` that already has no raw scan — the shared line is the loop body, and #4 must NOT reintroduce any byte-level quit handling. No conflict in `src/flow_model.h`: #4 adds a tick counter to the zero-init block (`src/flow_model.h` ~310-347), disjoint from the `q` built-in at `src/flow_model.h:1335-1351`.
  - **#6 `palette-modal-capture`** — completes the quit story this package opens (this makes `q` hook-vetoable; #6 makes the palette actually capture ALL input while modal and DROP unconsumed bytes). Shares the `flow_dispatch_key` hook-gate region (`src/flow_model.h:1321-1324`) conceptually but no line conflict: #3 adds a built-in below the gate; #6 changes what the gate/hook does for modal mode. They compose — #6 must keep the `q` built-in reachable when NOT modal.
  - No conflict with #1/#2/#5/#7/#8: those touch the paste undo bracket, the helper-guide render pass, edge animation/flags, the HUD overlay, and autopan respectively — none touch `flow_run`'s quit path or the `q` built-in.

**Carry-overs fixed.**
  - Closes inc-5 known-limitation #1 ("quit is a raw byte scan a hook cannot veto"; `src/flow_run.h:82`). After this package, quit is a dispatch built-in behind the hook gate and registry: a modal can swallow `q`, an app can rebind it, and the run loop no longer scans raw bytes for quit. It also removes the last raw-byte special-case from `flow_run`, clearing the path the `redraw-clock` loop rework (#4) is written against.

---

### 4. Poll-based run loop + tick counter + redraw clock  `[L]`  ·  id: `redraw-clock`

**Goal.** Turn `flow_run`'s blocking byte-pump (`src/flow_run.h:73-87`) into a **poll-driven event loop with a deterministic tick clock**. Replace the bare `read(STDIN_FILENO, …)` at `src/flow_run.h:80` with `poll(&pfd, 1, timeout_ms)`. On a *readable* wakeup, read + `flow_feed` + `flow_present` exactly as today (byte semantics unchanged — same buffer, same one-read-one-feed shape). On a *timeout* wakeup, advance a new `unsigned tick` counter on `struct flow` and re-`flow_present` **IFF frames are armed** (an arming predicate that is `0` today, so an idle graph with nothing armed blocks indefinitely — zero idle wakeups, exactly today's behavior). The non-IO decision logic — the present-decision predicate and the tick-advance — is factored into a small **TESTABLE seam** (`flow_tick`, `flow__frames_armed`) that tests drive directly; `flow_run` stays the only untestable IO shell. Add a tick-interval setter (`flow_set_tick_ms`, default 100 ms). New test `tests/test_clock.c` (`TESTS=` 30 → 31). **Every existing golden byte-identical** — the loop is render-invisible.

**User value.** This is the package that unblocks real-time. Today a graph re-renders ONLY when a byte arrives (`src/flow_run.h:79-85`): marching-ants edges (#5), a caret blink in a modal box, and tick-driven auto-pan (#8) are all impossible because nothing advances time between keystrokes. After this package, an armed consumer gets a steady 10 Hz frame clock that is *deterministic* (the tick is an integer counter, not wall-clock — so goldens minted at a pinned tick stay byte-reproducible) and *idle-cheap* (nothing armed ⇒ `poll` blocks forever ⇒ zero CPU at rest, no busy-spin). The headless seam (`flow_tick`) means animation logic is unit-testable without a TTY — the determinism rule's whole point.

**Files touched.**
  - src/flow_model.h  (struct flow zero-init block: `tick`, `tick_ms`; setter decl + the `flow_tick` / arming predicate decls)
  - src/flow_run.h    (the poll loop in `flow_run`; the `flow_tick` + `flow__frames_armed` impls; `flow_set_tick_ms` impl)
  - src/flow_term.h   (unchanged — see *Terminal mode stays untouched*; listed because the package OWNS the IO-mode question and pins the answer "no change")
  - tests/test_clock.c (new)
  - Makefile (`TESTS=` gains `test_clock`)

**Entry points (existing functions to extend).**
  - `flow_run` (`src/flow_run.h:73-87`) — the only run loop in the codebase (all three demos call it: `demos/hello_flow.c:14`, `demos/topo.c:222`, `demos/flowchart.c:230`). The `while (f->running)` body at `src/flow_run.h:79-85` is rewritten around `poll`; `flow_term_setup`/`flow_present`/`flow_term_restore` bracketing (`src/flow_run.h:75-77,86`) is unchanged.
  - `flow_present` (`src/flow_run.h:7-14`) — the complete diff+flush redraw (render into back buffer → `flow_diff_emit` → `fputs`+`fflush` → `memcpy` front). This is the *exact unit a redraw clock ticks*; the timeout branch calls this same function, so there is no new render entry point. Because it diffs against `f->front`, a timeout-present with no model/view change emits an empty escape string (the diff is empty) — cheap and flicker-free.
  - `flow_feed` (`src/flow_run.h:15-72`) — unchanged. The readable branch feeds the read buffer verbatim, so all input semantics (mouse CSI, dispatch, arrows, nudge, zoom, lone-ESC) are byte-for-byte today's.
  - `struct flow` zero-init block (`src/flow_model.h:305-361`) and `flow_new`'s explicit-default block (`src/flow_model.h:524-534`) — `tick` is calloc-zero (correct start); `tick_ms` needs an explicit default in `flow_new` (0 would mean "poll forever even when armed").

**API additions.**
```c
/* Deterministic redraw clock (inc-6 #4). The tick is the ONLY animation clock:
   wall-clock time NEVER enters the model or render layers (THE DETERMINISM RULE).
   flow_run advances `tick` on each poll timeout while frames are armed; tests call
   flow_tick directly. flow_render stays a pure function of (model, view, tick). */

void     flow_tick(flow_t *f);          /* advance the animation clock by one frame: ++f->tick.
                                           The TESTABLE seam — the run loop calls it on a poll
                                           timeout; tests call it directly to drive animation
                                           deterministically. No IO, no render, no time(). */
unsigned flow_ticks(flow_t *f);         /* read the current tick (fill-not-needed scalar getter;
                                           render/consumers derive dash phase = tick % period). */
void     flow_set_tick_ms(flow_t *f, int ms); /* redraw interval in ms when frames are armed
                                           (default 100 = 10 Hz). <=0 clamps to 1 (never 0:
                                           a 0 poll timeout would busy-spin). Tick rate only;
                                           idle behavior is governed by the arming predicate. */

/* Private (flow__ prefix): the present-decision predicate. Returns nonzero when the loop
   must wake on a timer (something needs frames). v1 returns 0 (nothing armed → poll blocks
   indefinitely). #5 ORs in "any FLOW_ANIMATED edge exists"; #8 ORs in "an object drag is in
   flight". Declared in flow_model.h, defined in flow_run.h. */
int      flow__frames_armed(flow_t *f);
```

**Design notes.**

*The testable seam — why a function, not inline loop code (the determinism rule made concrete).* THE DETERMINISM RULE (frame pin) says wall-clock never enters model/render and the tick is the only animation clock. The trap a naive implementation falls into: putting `++some_counter` and the present-decision inline in `flow_run`'s loop, where it can only be exercised by a real TTY with real timing — untestable, and the first place a `time()` call sneaks in "just to make ants smooth." So the non-IO logic is carved into two pure functions. `flow_tick(f)` is `++f->tick;` and *nothing else* — no read, no render, no `clock_gettime`. The run loop's timeout branch is exactly `flow_tick(f); flow_present(f);`. Tests construct a graph, call `flow_tick` N times, and assert `flow_ticks(f) == N` and that `flow_render`'s output at tick N matches a golden — the entire animation contract verified with zero IO. `flow_run` keeps ONLY the parts that need a TTY (`poll`, `read`, term setup/restore); it is the one untestable shell, by construction minimal. WHY a counter on `struct flow` and not a `static`: two `flow_t` instances (a test can hold several) must tick independently, and the value must be inspectable per-graph — a file `static` would be a hidden global, breaking both. WHY `unsigned`: monotone wrap is defined behavior in C (`unsigned` overflow wraps mod 2^32), so `tick % period` is always well-defined; a signed counter could UB-overflow after ~248 days at 100 Hz. Trap recorded: do NOT make `flow_tick` also present — keeping advance and present separate is what lets a test advance the clock without a render buffer, and lets #5's golden tests pin a tick *then* render once.

*The arming predicate — recomputed, not a counter (and why).* The brief asks: who arms frames, and is it a counter or a recomputed predicate? I choose a **recomputed predicate** (`flow__frames_armed(f)`), and the reason is composition-safety. Two future consumers arm frames independently: #5 wants frames whenever ANY `FLOW_ANIMATED` edge exists, #8 wants frames only DURING a drag. A counter (`f->frames_armed_count`, arm = `++`, disarm = `--`) demands every arm be paired with exactly one disarm on every exit path — and the disarm paths are exactly the error-prone ones (drag cancelled by lone-ESC at `src/flow_run.h:69`, connection dropped, node deleted mid-drag, the FLOW_ANIMATED flag cleared by `flow_load`). A single missed decrement strands the count > 0 forever, and the loop busy-renders at 10 Hz for the rest of the session — the exact "burns cycles" failure open question 5 rejects, and one that is *silent* (no crash, just a warm laptop). A recomputed predicate has no paired-bookkeeping: it ORs *current* conditions each time the loop is about to block, so the answer is always exactly right by construction and self-heals the instant a condition clears. The cost — re-scanning edges for the FLOW_ANIMATED bit each timeout — is `O(nedges)` at most 10×/sec, negligible at TUI scale (and #5 can early-out on the first animated edge). For THIS package the predicate is the single line `return 0;` (no consumer has landed), so v1 idle behavior is *identical to today*: with nothing armed, `poll` is called with timeout `-1` (infinite) and blocks until a byte arrives — zero idle wakeups (open question 5, recommended). The seam is shaped so #5 and #8 each add one `||` clause and touch nothing else.

*The poll loop — poll() over select(), and the exact control flow.* I specify **`poll`**, not `select`. Reasons: (a) `select` needs an `fd_set` rebuilt every iteration plus the `nfds = fd+1` ritual and the historical `FD_SETSIZE` 1024-fd cap; `poll` takes a one-element `struct pollfd` array and an `int timeout` in milliseconds directly — a cleaner fit for a single STDIN fd. (b) `poll`'s timeout is already milliseconds (matching `tick_ms`), so no `struct timeval` conversion. The loop becomes:
```
while (f->running) {
  int timeout = flow__frames_armed(f) ? f->tick_ms : -1;   /* -1 = block forever (idle) */
  struct pollfd pfd = { .fd = STDIN_FILENO, .events = POLLIN };
  int pr = poll(&pfd, 1, timeout);
  if (pr < 0) { if (errno == EINTR) continue; break; }      /* signal (e.g. SIGWINCH); retry */
  if (pr == 0) { flow_tick(f); flow_present(f); continue; } /* TIMEOUT: advance clock, redraw */
  /* READABLE */
  int n = (int)read(STDIN_FILENO, buf, sizeof buf);
  if (n <= 0) break;
  flow_feed(f, buf, n);                                     /* q-quit now a dispatch built-in (#3) */
  flow_present(f);
}
```
The `timeout` is recomputed every iteration so a consumer arming/disarming between wakeups takes effect on the very next block. WHY `EINTR`→`continue` (new, and load-bearing): the bare `read()` today returns ≤0 on `EINTR` and the loop `break`s (`src/flow_run.h:81`) — acceptable when only a typed byte wakes it, but a poll loop that can wake on `SIGWINCH` (terminal resize) or `SIGCONT` must not exit on a benign interrupt. Trap: forgetting `EINTR` makes the app quit when the window is resized. `errno` requires `#include <errno.h>` in the impl block (poll itself needs `#include <poll.h>`). WHY recompute the pollfd each iteration rather than hoist it: `poll` may modify `pfd.revents`, and re-initializing is free; hoisting saves nothing and risks a stale `revents` read.

*The q-quit interaction with #3 (and why #3 lands first).* The frame pins #3 BEFORE #4 precisely so this loop is written against a dispatch-reachable quit. Today the loop has a raw pre-scan `for (int i…) if (buf[i] == 'q') f->running = 0;` at `src/flow_run.h:82` — a label containing `q` quits the app, and a hook cannot veto it. #3 DELETES that line and makes `q` a `flow_dispatch_key` built-in setting `f->running = 0` (behind the hook gate and registry). My loop body therefore has NO `q` scan — it is just `flow_feed(f, buf, n)`, and `f->running` is mutated from inside dispatch. `f->running` stays the loop condition (`src/flow_run.h:79`), unchanged. Trap if #3 has NOT landed when #4 is implemented: re-deriving the loop, do not silently re-introduce the raw `q` scan — the two packages share `src/flow_run.h:79-85` and the quit mechanism is #3's; #4 only owns the read→poll swap and the timeout branch. (Recorded under *Conflicts with*.)

*Terminal mode stays untouched — VMIN/VTIME are NOT the mechanism (verified).* The drift JSON flagged "check VMIN/VTIME" — I verified `flow_term_setup` (`src/flow_term.h:11-17`) clears ONLY `ECHO | ICANON` (`src/flow_term.h:13`) and never touches `c_cc[VMIN]`/`c_cc[VTIME]`; they sit at the inherited defaults. A VTIME-based read timeout was the *other* way to build a redraw clock (set `VTIME` so `read` returns after a deciseconds gap). I reject it for two reasons: (a) `VTIME` granularity is deciseconds (100 ms units), too coarse to retune via `flow_set_tick_ms`; (b) it couples the timer to terminal-driver state instead of an explicit, testable `poll` timeout. `poll` keeps all timing in user code (testable, ms-granular) and needs **zero termios changes** — so `src/flow_term.h` is untouched. Critically, **`ISIG` is left on** (`src/flow_term.h:13` clears only `ECHO|ICANON`), so Ctrl-C still signals under the new loop — the poll loop preserves this for free (it changes *when* `read` is called, not the terminal mode). The `EINTR`→`continue` above is what keeps a stray signal from killing the loop while preserving Ctrl-C's SIGINT default-terminate. Pin honored: "No `time()`, `clock_gettime()`, or `gettimeofday()` outside `src/flow_run.h`" — and in fact this package adds NONE anywhere; the only new syscall is `poll`, whose timeout is `tick_ms`, an integer the app sets, never read from a clock.

*The partial-CSI split-read story (verified, and why poll preserves it exactly).* The brief asks whether today's code handles a CSI split across two read wakeups. **Verified answer: it does NOT reassemble split CSIs, and it never did — and that is unchanged here.** `flow_run` reads up to 64 bytes into a fresh stack `buf` (`src/flow_run.h:78,80`) and feeds that whole buffer in one `flow_feed` call (`src/flow_run.h:83`); it carries NO residual bytes between iterations. Inside `flow_feed`, every multibyte parse is bounds-guarded against the *current* buffer length: mouse CSI needs `i+2 < n` (`src/flow_run.h:18`), the arrow switch needs `i+2 < n` (`src/flow_run.h:24`), the Shift-arrow nudge needs `i+5 < n` (`src/flow_run.h:42`). So if a sequence's tail falls past `n`, the guard fails and the bytes fall through — in the worst case to the lone-ESC branch (`src/flow_run.h:69`), which the code itself documents as an ACCEPTED trade-off (`src/flow_run.h:64-68`): "a CSI split by a read() boundary exactly after its ESC byte reads as a lone ESC … terminals write sequences atomically, so this is theoretical." **poll changes nothing about this**: poll only governs *when* `read` returns; the buffer handling, the one-read-one-feed shape, and the bounds guards are all in `flow_feed`/`flow_run`'s read path, which I keep byte-identical. A poll loop does NOT increase split-read risk (it does not read smaller chunks). I deliberately do NOT add cross-read CSI reassembly in this package — it is the exact ESC-timeout state machine the existing comment defers as "out of scope for v1" (`src/flow_run.h:68`), and bolting it onto the loop rework would change input semantics and risk a golden. Recorded as a future option, untouched here.

*struct flow additions (zero-init-safe, transient, never journaled/saved).* Two ints in the zero-init block (`src/flow_model.h:305-361`), beside the existing transient state (the pin lists "#4's tick counter + interval (`tick`, `tick_ms`)"): `unsigned tick;` (animation clock) and `int tick_ms;` (redraw interval). `tick` is calloc-zero = the correct starting frame, no `flow_new` line needed. `tick_ms` MUST get an explicit default in `flow_new` (`src/flow_model.h:524-534`) — `f->tick_ms = 100;` beside `f->autopan_margin = 3;` (`src/flow_model.h:530`) — because zero means "armed but poll-forever," a latent bug. Neither is heap-owning (clipboard stays the only heap member, pin honored), neither is journaled (tick state is transient — precedent: extents and zoom are transient, `src/flow_model.h:311` / pin "Undo: nothing in this increment journals"), neither is saved (the JSON pin: "tick … state are NEVER saved"). I deliberately do NOT add a `frames_armed` *field* — the recomputed-predicate decision above means there is no armed-state to store; the predicate reads existing model state (#5 reads edge flags, #8 reads drag fields). So the struct grows by exactly two scalars.

*No on_tick observer in v1 (gates-vs-observers pin honored).* The drift JSON notes the `flow_callbacks` tail (`src/flow_model.h:264-284`, closing at `src/flow_model.h:284`) is the natural append point for an `on_tick`/`on_frame` observer. I deliberately do NOT add one — the frame pins it ("the tick is loop state, not an event … no `on_tick` in v1; recorded as a future option. The callbacks tail is untouched"). The tick is read by consumers via `flow_ticks(f)` (a pull getter), not pushed via a callback. This keeps the callbacks tail byte-identical (`on_connect_end` at `src/flow_model.h:278` stays last before `*user`) and avoids an ABI-append this increment. Recorded as a future option: an `on_tick(f, tick, user)` observer for apps that want a per-frame hook.

*Render is not touched by this package.* `flow_render` already takes the model and view; the tick reaches it via `f->tick` (consumers read `flow_ticks`). THIS package adds the clock but no render consumer — #5 is the first to derive a dash phase from `tick`. So every render path (`src/flow_render.h`) is unchanged and every existing golden is byte-identical (the loop is render-invisible; a timeout-present with no state change diffs to empty). The acceptance bar below makes "all goldens byte-identical" the primary signal.

**Test plan.** New file `tests/test_clock.c`; append `test_clock` to `TESTS=` in the Makefile (currently 30 stems ending `test_search` at `Makefile:4` → 31). Harness idiom: `#define FLOW_IMPLEMENTATION` + `#include "../flow.h"` + `#include "flowtest.h"` (precedent `tests/test_focus.c:1-3`), `ASSERT_INT`/`ASSERT`/`SNAPSHOT` from `tests/flowtest.h`, and the local `cells_to_string` render idiom from `tests/test_render.c:5-15` for the determinism golden. RED strategy per the discipline pin: **compile-RED** — the test references `flow_tick`, `flow_ticks`, `flow_set_tick_ms` (and pokes `f->tick`/`f->tick_ms` directly, allowed by the "tests may poke struct flow internals" rule), none of which exist before implementation, so the suite fails to compile until the API lands. Snapshot lines are added only after GREEN.

  1. **Tick advances deterministically (the seam).** `flow_new(30, 8)`; `ASSERT_INT(flow_ticks(f), 0, "tick starts at 0")` (calloc-zero). Call `flow_tick(f)` three times; `ASSERT_INT(flow_ticks(f), 3, "three ticks advance to 3")`. Poke `f->tick` directly and confirm `flow_ticks` reads it back (proves the getter is a thin read, not a copy). This is the determinism rule's core: a test drives the clock with no IO.
  2. **flow_tick does NOT render or block (purity).** With no callbacks and no TTY, call `flow_tick(f)` 1000× in a tight loop; assert it returns promptly and `flow_ticks(f) == 1000`. Proves `flow_tick` is pure counter-advance — no `flow_present`, no `read`, no sleep. (Mechanical discriminator: a `flow_tick` that accidentally called `flow_present` would write escape bytes to stdout / touch `f->front`; assert `f->front` is unchanged across the loop by memcmp against a pre-loop copy of the `cols*rows` cell buffer.)
  3. **tick_ms default + setter clamp.** Fresh `flow_new`; `ASSERT_INT(f->tick_ms, 100, "default interval 100ms")` (the `flow_new` default). `flow_set_tick_ms(f, 250)`; `ASSERT_INT(f->tick_ms, 250, "setter stores ms")`. `flow_set_tick_ms(f, 0)`; `ASSERT_INT(f->tick_ms, 1, "0 clamps to 1, never busy-spin")`. `flow_set_tick_ms(f, -5)`; `ASSERT_INT(f->tick_ms, 1, "negative clamps to 1")`. Guards the open question 4 default and the zero-timeout busy-spin trap.
  4. **Arming predicate is OFF in v1 (idle = block forever).** Fresh graph with nodes/edges but no consumer armed; `ASSERT_INT(flow__frames_armed(f), 0, "nothing armed in v1")`. This is the contract #5 and #8 extend; the test pins v1's "zero idle wakeups" guarantee (open question 5, recommended) and is the discriminator that catches an accidental `return 1` (which would busy-render). After #5/#8 land, this assertion is updated in those packages — recorded as the seam test.
  5. **Render is a pure function of (model, view, tick) — determinism golden.** `flow_new(30, 8)`, `flow_register_defaults`, add one node "A" at `(0,0)`; `cells_to_string` the frame at tick 0 (`SNAPSHOT("clock_tick0", s)`). Advance `flow_tick(f)` 7×; render again with NO model/view change; assert the new frame is byte-identical to tick 0 (`ASSERT_STR(s7, s0, "render unchanged by tick when nothing animates")`). Proves the tick does NOT leak into render for non-animated content — the byte-identical-goldens guarantee in microcosm, and the hook #5 hangs its tick-derived dash phase on.
  6. **Existing goldens unchanged (the primary acceptance signal, asserted by the whole suite).** No new assertion here — `make test` running every existing snapshot test (test_render, test_flowchart, …) IS the byte-identical gate. The loop change is render-invisible; if any golden moves, this package is wrong. (Stated in Acceptance.)
  7. ASan/UBSan clean: the new fields allocate nothing; `flow_tick`/`flow_set_tick_ms` allocate nothing; the test `flow_free`s every graph and copies `f->front` onto the stack only. Whole-suite sanitizer gate per the discipline pin.

**Acceptance.**
  - `make test` passes including `tests/test_clock.c` (31 stems).
  - Whole-suite ASan + UBSan clean (per-package gate).
  - **ALL existing goldens byte-identical** — the package's primary risk and primary bar. Every snapshot in `tests/test_render.c`, `tests/test_flowchart.c`, and all others MUST be unchanged: the loop rework adds no render path and the tick does not enter `flow_render` (no consumer lands here). No golden is regenerated; `test_clock`'s own `clock_tick0` snapshot is the only new golden, staged with this package.
  - `render_statusbar` golden (cols=30) is byte-identical — this package APPENDS no statusbar hint (the STATUSBAR pin: tick/quit/HUD are not statusbar concerns).
  - The `flow_callbacks` tail is byte-identical — no `on_tick` added (gates-vs-observers pin).
  - `src/flow_term.h` is unchanged (terminal mode untouched; ISIG stays on, Ctrl-C still signals).
  - `make demos` warning-free; demos are NOT edited (no demo arms frames in this package — that wiring is the final integration pass / #5's showcase). The poll loop is a drop-in: demos keep calling `flow_run`.
  - `flow.h` regenerated via `tools/amalgamate.sh` (edits land in `src/flow_model.h`, `src/flow_run.h` — both existing modules; `modules=` UNTOUCHED, no new module, per the discipline pin).
  - Warnings sweep: `make test 2>&1 | grep -i warn` yields only the carved-out `tests/test_model.c:8` 'save' line; the new `<poll.h>`/`<errno.h>` includes and the `poll` call compile warning-free under `-std=c11 -Wall -Wextra` (POSIX feature-test: the codebase already uses `termios`/`ioctl` in `src/flow_term.h` under the same flags, so the implicit `_DEFAULT_SOURCE` exposure that works there works for `poll`).

**Depends on.** **Hard: #3 `quit-routing`** — #3 lands BEFORE #4 (frame execution order) so the loop is rewritten against a dispatch-reachable quit: #3 deletes the raw `q` pre-scan at `src/flow_run.h:82` and makes `q` a `flow_dispatch_key` built-in setting `f->running = 0`. #4's loop body must NOT carry that scan forward. Otherwise builds only on landed code: `flow_present` (`src/flow_run.h:7-14`), `flow_feed` (`src/flow_run.h:15-72`), the `struct flow` zero-init block (`src/flow_model.h:305-361`), `flow_new` defaults (`src/flow_model.h:524-534`).

**Conflicts with.**
  - **`quit-routing` (#3)** — shared file/region `src/flow_run.h:79-85` (the `while (f->running)` loop body). #3 owns the quit mechanism (deletes the `q` scan at `src/flow_run.h:82`, makes `q` a built-in); #4 owns the read→poll swap and the timeout branch in the SAME body. They edit the same lines; #3 lands first, then #4 rebases its poll rewrite onto the post-#3 loop (which already has no `q` scan). `f->running` (`src/flow_model.h:310`) stays the loop condition for both.
  - **`animated-edges` (#5)** — shared seam `flow__frames_armed` (`src/flow_run.h`, declared `src/flow_model.h`): #5 adds the first `||` clause ("any `FLOW_ANIMATED` edge exists", consuming flag bit 32u per the flags ledger). #5 also derives a dash phase from `flow_ticks(f)` in `src/flow_render.h`. No code overlap beyond extending the predicate this package defines; #5 updates test_clock's case-4 assertion. #5 is the first render consumer of `tick`.
  - **`tick-autopan` (#8, stretch)** — shared seam `flow__frames_armed`: #8 adds the "object drag in flight" `||` clause and reads `f->tick`/last-cursor during drags (the follow-up documented at `src/flow_input.h:58-66`). Composes with #5's clause as an OR (the recomputed-predicate design's purpose). Hard-depends on this package's tick counter + poll loop.
  - **`devtools-hud` (#7)** — no code conflict. #7's HUD is an `on_overlay` app-side overlay; it may *display* `flow_ticks(f)` (a read-only getter) but adds no loop or struct conflict. Recorded so a reviewer confirms the HUD reads the tick, never advances it.
  - No conflict with `paste-group-remeasure` / `helper-trailing-edge` / `palette-modal-capture`: those touch the paste undo bracket, `render_helper_snap`, and the key-hook/modal path respectively — disjoint from the loop and the two new struct scalars. #6's modal flag joins the same zero-init block but a different region (beside `key_hook_fn`, `src/flow_model.h:335`).

**Carry-overs fixed.** Closes session-handoff limitation #2's animation half (`docs/superpowers/plans/2026-06-05-flow-increment-6-session-handoff.md:84`): "Animated edges still blocked on a clock: the run loop blocks on read() with no timer tick. A poll()-based loop with a redraw interval would unblock marching ants AND give modal UIs a caret blink." This package lands exactly that poll loop + redraw interval; the marching-ants render consumer is #5 (inventory item 15), and the modal caret blink is unblocked for any future modal UI. Also resolves the `src/flow_input.h:63` deferral note ("the run-loop-ticked [auto-pan] model is a documented follow-up") at the seam level — #8 is the consumer that finishes it.

---

### 5. Marching-ants animated edges  `[M]`  ·  id: `animated-edges`

**Goal.** Land the oldest for-cause deferral: in-grid **marching-ants** animation on edges flagged `FLOW_ANIMATED`. A new shared-namespace flag bit (`FLOW_ANIMATED = 32u`, `src/flow_model.h:2`) plus a public per-edge setter (`flow_set_edge_animated`, mirroring `flow_set_edge_hidden` at `src/flow_model.h:1291-1301`); the render side turns the edge draw loop (`src/flow_render.h:179-197`) into a tick-derived dash cycle that skips off-phase PATH cells so the marching gaps walk along the routed path each frame. The animation is driven solely by `#4`'s tick counter on `struct flow` (read directly as `f->tick` inside the edge loop — no `flow_render` signature change, no wall-clock), suppressed at LOD 1, and an edge becoming/ceasing animated arms/disarms the redraw clock through `#4`'s frames-needed predicate.

**User value.** Direction and liveness on an edge — xyflow's `Edge.animated` / dashdraw — is the standard "this connection is active / data is flowing" affordance, and it is the first thing the run-loop rework (`#4`) exists to unblock (the handoff dropped animated-edges "for cause last time": no clock — `docs/superpowers/plans/2026-06-05-flow-increment-6-session-handoff.md:84`). Statically the library already proves the dashed render (the connection preview, `src/flow_render.h:282-284`); this package adds the time axis: a per-edge opt-in, headless and scriptable (`flow_set_edge_animated`), that any demo or app can toggle. Non-animated edges and tick 0 are byte-identical to today, so the feature is invisible until armed.

**Files touched.**
  - src/flow_model.h (flag enum `:2`; setter decl beside `flow_set_edge_hidden` `:204`; setter body beside `:1291-1301`)
  - src/flow_render.h (edge draw loop `:179-197`)
  - tests/test_animated.c (new)
  - Makefile (`TESTS=` gains `test_animated`)

**Entry points (existing functions to extend).**
  - `flow_render` edge loop (`src/flow_render.h:179-197`) — the dash cycle is inserted into the existing per-cell put at `src/flow_render.h:189-190`. `lod` is NOT in scope here (the node loop computes `int lod = flow__lod_for(f, f->view.zoom);` at `src/flow_render.h:206`, AFTER this loop), so the LOD-1 suppression test recomputes `flow__lod_for(f, f->view.zoom)` (`src/flow_model.h:829`) once before the loop.
  - `flow_set_edge_hidden` (`src/flow_model.h:1291-1301`) — the exact precedent the new setter mirrors: look up by id via `flow_get_edge`, `return` on miss, set/clear the flag bit on `e->flags`. The animated setter is simpler (no selection/focus side-effects) but ADDS the arming call (see Design notes).
  - `flow_route_push` cell order (`src/flow_route.h:12-14`) and both routers (`flow_route_orthogonal` `src/flow_route.h:34-65`, `flow_route_straight` `src/flow_route.h:70-107`) — both emit `rt.cells[]` in source→target path order, which is the axis the dash marches along (see Design notes — this is WHY the phase is keyed on cell index, not coordinate).
  - `#4`'s tick counter + frames-needed predicate on `struct flow` (the zero-init block, `src/flow_model.h:305-343`) — this package READS `f->tick` in render and feeds `#4`'s predicate the "any animated edge?" answer (see Depends on for the exact seam contract).

**API additions.**
```c
/* Per-edge marching-ants opt-in. Mirrors flow_set_edge_hidden (src/flow_model.h:204):
   on != 0 sets FLOW_ANIMATED on the edge's flags, on == 0 clears it; no-op if id
   is unknown. Setting/clearing arms/disarms the redraw clock via #4's frames-needed
   path. NOT journaled, NOT persisted (see Design notes — flags are ephemeral). */
void flow_set_edge_animated(flow_t *f, int id, int on);

/* Flag ledger: FLOW_ANIMATED consumes bit 32u (1u<<5) in the single shared
   node/edge flag namespace; next-free becomes 64u (frame pin). */
enum { /* …, FLOW_EXTENT_PARENT = 16u, */ FLOW_ANIMATED = 32u };  /* src/flow_model.h:2 */
```

**Design notes.**

*The flag bit (32u, shared namespace — verified free).* The flag enum at `src/flow_model.h:2` tops out at `FLOW_EXTENT_PARENT = 16u`; `32u` (`1u<<5`) is unused (no `32u`/`<<5` flag use anywhere in `src/`). Per the frame's flags ledger this package consumes `32u` as `FLOW_ANIMATED` and the next-free bit becomes `64u`. The namespace is shared between nodes and edges (one `unsigned flags` on each of `flow_node` `src/flow_model.h:17` and `flow_edge` `src/flow_model.h:19`); `FLOW_ANIMATED` is meaningful on edges only — a node carrying the bit is harmless (the node render path never reads it), exactly as `FLOW_EXTENT_PARENT` is meaningful on nodes only. The trap a per-edge *field* would have hit instead: an ABI-append to `flow_edge` reorders nothing but forces every `flow_edge` initializer and the JSON loader to learn a new member; the flag rides the existing `flags` word that hit-test, selection, and hidden already share, so zero existing initializer changes.

*The setter mirrors `flow_set_edge_hidden`, minus the side-effects, plus arming.* `flow_set_edge_hidden` (`src/flow_model.h:1291-1301`) is the flag-setter precedent: `flow_get_edge` → `return` on NULL → `e->flags |= FLAG` / `e->flags &= ~(unsigned)FLAG`. `flow_set_edge_animated` copies that skeleton. It does NOT replicate hidden's deselect-on-hide logic (`src/flow_model.h:1296`) — animation is orthogonal to selection and visibility; an animated edge stays selectable, hittable, and (if also `FLOW_SELECTED`) draws bold over its marching path. The hidden setter's skeleton needs NO arming addition at all: because `#4`'s `flow__frames_armed` predicate is recomputed each loop iteration and reads the edge `FLOW_ANIMATED` flags directly, setting/clearing the flag here is automatically picked up on the next poll — the setter stores no armed state and calls nothing. This is correct under multiple animated edges and under `flow_load` (see next two notes) by construction: the predicate scans current edge flags, so clearing one animated edge cannot disarm while a SECOND still carries the flag, and `flow_load` (which drops all `FLOW_ANIMATED` flags) implicitly disarms with no stale arm. Trap avoided: a naive `f->frames_armed = 1` on set / `= 0` on clear would wrongly disarm while a SECOND animated edge still needs frames.

*Why the dash phase is keyed on PATH-CELL INDEX, not coordinate parity (the router trap).* The static connection preview at `src/flow_render.h:283` dashes with `((x + y) & 1) == 0` — every-other cell by coordinate parity. That is safe there ONLY because the preview is always orthogonal (`flow_route_orthogonal` is called directly at `src/flow_render.h:279`), where stepping one cell along an axis flips `(x+y)` parity every cell. It does NOT generalize: `flow_route_straight` (`src/flow_route.h:82-96`) on a 45° diagonal advances BOTH x and y by ±1 each step, so `(x + y)` changes by ±2 (or 0) per cell and its parity is CONSTANT along the whole edge — a coordinate-parity dash would make a diagonal edge BLINK as a unit, not march, and non-45° diagonals would dash in irregular constant-parity runs. The brief requires the treatment to read on orthogonal AND straight routers. The fix is router-agnostic: both routers push `rt.cells[]` in source→target order (`flow_route_push` `src/flow_route.h:12-14`, appended in path order by every loop in `src/flow_route.h:41-48` and `:82-96`), so phasing on the cell index `c` walks the gap monotonically along the path regardless of geometry. The lit/skip decision becomes `((c + f->tick) % period)` — on an orthogonal run `c` increments one per cell so it still alternates per cell (matching the preview's visual cadence); on a diagonal it now MARCHES instead of blinking.

*The exact glyph treatment: SKIP off-phase path cells (do not stamp a space).* For an animated edge at LOD 0, the per-cell put at `src/flow_render.h:189-190` is gated: a cell is drawn (its router glyph stamped via `flow_cellbuf_put`) when `((c + f->tick) % 2) == 0`, and SKIPPED otherwise — the `flow_cellbuf_put` is simply not called for off-phase cells. Skip, not blank: a skipped cell keeps whatever is already in the back buffer, which is the cleared `' '` (`flow_cellbuf_clear` sets `.ch = ' '`, `src/flow_cell.h:60`) or, where the background grid is active, the grid glyph drawn first (`src/flow_render.h:175`) — so the marching gaps show the same backdrop the preview gaps do, never an opaque overwrite. `period = 2` (the every-other-cell pattern the preview already proves reads at TUI cell granularity); the modulus is named so a future longer ant pattern is a one-constant change. TWO cells are exempt from the skip and ALWAYS drawn: (1) the arrowhead — the last router cell (`rt.cells[rt.count-1]`, overwritten with the arrow glyph by both routers, `src/flow_route.h:60-62` and `:103-104`) stays solid so the edge never loses its direction marker mid-phase; gate the skip on `c < rt.count - 1`. (2) The label loop (`src/flow_render.h:191-194`) is already a SEPARATE put after the path loop, so it stays solid for free — no change there. Selected-edge bold (`attr`, `src/flow_render.h:188`) is unchanged: bold rides the cells that ARE drawn.

*LOD-1 suppression (draw solid, ignore tick).* Below `FLOW_LOD_THRESHOLD` (`flow__lod_for == 1`, `src/flow_model.h:829`) the dash cycle is suppressed: the edge draws its full solid path exactly as today. Rationale: at LOD 1 nodes collapse to a 1×1 marker and the scene is a zoomed-out overview where per-cell marching is illegible and the dash gaps would read as a broken edge, not motion. Mechanically, since the edge loop does NOT otherwise consult `lod` (it routes identically at every zoom — `src/flow_render.h:179-197` has no lod branch), suppression is the cleanest gate: compute `int lod = flow__lod_for(f, f->view.zoom);` once before the loop and apply the dash skip ONLY when `lod == 0 && (e->flags & FLOW_ANIMATED)`. A non-animated edge, or any edge at LOD 1, takes the unmodified solid path — so the LOD-1 frame is tick-invariant by construction (a property the test locks).

*Tick is render input, read directly — no signature change, no wall-clock.* `flow_render(f, out, cols, rows)` already has `f` in scope; the dash reads `f->tick` (`#4`'s counter) the same way the focus post-pass reads `f->focus_node`. Edges do not receive `flow_render_ctx` (only the node `render` vtable does, `src/flow_model.h:24`), so there is nothing to thread — the field read is the whole plumbing. This honors the determinism rule: `flow_render` stays a pure function of (model, view, **tick**); no `time()`/`clock_gettime()` enters render. Goldens are minted at pinned tick values and stay byte-reproducible because `f->tick` only advances via the run loop or a direct test poke.

*JSON: `FLOW_ANIMATED` does NOT persist — this RESOLVES frame open question 6 (false branch).* Frame OQ6 / the JSON pin (Cross-cutting rules; open question 6) delegate to this writer: "`FLOW_ANIMATED` persists through save/load like other flags **IF flags already round-trip**." Verified against `src/flow_json.h`: flags do NOT round-trip. The save header states it outright — "Ephemeral state (**flags**, selection, w/h, …) is NEVER emitted" (`src/flow_json.h:7`); the edge save loop emits only id/source/target/handles/type/label (`src/flow_json.h:62-75`, no `flags`); the edge load loop reads only those same keys (`src/flow_json.h:366-381`, never touches `e->flags`). So `FLOW_ANIMATED` is dropped on save and an edge comes back from `flow_load` un-animated. This is CONSISTENT with the existing flag treatment, not a deviation: `FLOW_HIDDEN` is documented as a view-level, non-persisted flag in the same comment block (`src/flow_model.h:4-7`), and animation is transient run state by the determinism rule. User-visible consequence (open question below): a saved-and-reloaded graph must be re-armed by the app (`flow_set_edge_animated` after `flow_load`), exactly as it must re-hide or re-select. The wording matters for the consistency pass: this RESOLVES the delegated conditional as false; it does not contradict a hard pin (the pin's prose is itself conditional on the round-trip the writer was told to verify).

*No journaling, no callback.* Per the frame's undo and gates-vs-observers rules, the animated flag is not journaled (transient, like extents and the hidden flag) and adds NO `flow_callbacks` field — there is no `on_animate`/`on_tick` observer; the tick is loop state, not an event. The callbacks tail (`src/flow_model.h` ending `on_connect_end`/`user`) is untouched.

**Test plan.** New file `tests/test_animated.c`; append `test_animated` to `TESTS=` in the Makefile (relative-to-predecessor: `#4` appends `test_clock` → 31, this package → 32). Use the `cells_to_string` helper idiom from `tests/test_render.c:5-15` and `SNAPSHOT`/`strchr` mechanical guards (precedent: the FLOW_HIDDEN render test). RED strategy per the discipline pin: the setter assertions are compile-RED (the symbol `flow_set_edge_animated` and `FLOW_ANIMATED` do not exist yet); the render discriminator is behavioral-RED with a mechanical cell-DIFFERS check; snapshot lines are added only after GREEN. Tests may poke `struct flow` internals (set `f->tick` directly) per the discipline rule.

  1. **Setter sets/clears the flag (compile-RED on the new symbol).** `flow_new`, `flow_register_defaults`, two nodes A,B, edge `e = flow_add_edge`. `flow_set_edge_animated(f, e, 1)`; assert `flow_get_edge(f, e)->flags & FLOW_ANIMATED`. `flow_set_edge_animated(f, e, 0)`; assert `!(flow_get_edge(f, e)->flags & FLOW_ANIMATED)`. Unknown id is a silent no-op: `flow_set_edge_animated(f, 99999, 1)` must not crash and must change nothing. Proves the mirror of `flow_set_edge_hidden`.
  2. **Tick-0 golden (the static baseline).** Same graph, edge animated, `f->tick = 0`. `flow_render`; `cells_to_string`; `SNAPSHOT("animated_tick0", s)`. At tick 0 the dash draws the EVEN-index cells of the path (`(c + 0) % 2 == 0`). This is the pinned-tick golden the determinism rule requires.
  3. **Tick-k discriminator: a specific cell DIFFERS (behavioral-RED, mechanical).** Render the SAME animated edge at `f->tick = 0` into buffer `b0` and at `f->tick = 1` into buffer `b1` (re-render the same graph, do not reuse). Find an interior path cell that is a router glyph (e.g. a horizontal-run cell, `0x2500`) at one phase: assert there EXISTS a cell index `i` in the buffer where `b0[i].ch != b1[i].ch` AND that cell is on the edge path (not a node box) — concretely, scan the row between the two boxes and assert at least one column toggles between its router glyph and `' '` across the two ticks. This is the "a specific cell DIFFERS at tick k != 0" discriminator; it fails RED before the dash cycle exists (today every tick renders identically) and proves the ants march.
  4. **Arrowhead and label survive every phase.** Animated edge with a `label`. For `tick` in {0,1}: `flow_render`; assert the frame still contains exactly one arrowhead glyph (`0x25B6/25C0/25BC/25B2`, the precedent count check from `tests/test_render.c:37-40`) and `strchr(s, <label-char>) != NULL`. Proves the `c < rt.count - 1` arrowhead exemption and that the separate label loop is untouched by the skip.
  5. **LOD-1 suppression is tick-invariant.** Same animated edge, `flow_set_zoom(f, 0.5f, anchor)` (below `FLOW_LOD_THRESHOLD` 0.6, so `flow__lod_for == 1`; read back via `flow_zoom(f)` per `tests/test_zoom.c:22-23`). Render at `f->tick = 0` (→ `s0`) and `f->tick = 1` (→ `s1`); assert `strcmp(s0, s1) == 0`. Proves animation is suppressed at LOD 1 (the edge draws solid, ignoring tick).
  6. **Non-animated edge is byte-identical across ticks (the goldens-invariance proof).** A NON-animated edge (default), `f->tick = 0` (→ `s0`) and `f->tick = 7` (→ `s7`); assert `strcmp(s0, s7) == 0`. This is the mechanical guarantee behind "all existing goldens stay byte-identical": every current snapshot renders only non-animated edges at the default tick, so none can move.
  7. **Animation does not persist through save/load (OQ6 resolved-false lock).** Animate edge `e`, `flow_save` to a temp path, `flow_load` into a fresh graph, assert the reloaded edge's `flags & FLOW_ANIMATED` is 0 (flags are not emitted, `src/flow_json.h:7,62-75,366-381`). Documents the user-visible re-arm requirement and guards against a future loader silently learning flags.
  8. ASan/UBSan clean: the dash adds no allocation (it only skips puts and reads `f->tick`); `flow_free` each graph, free every `cells_to_string` buffer.

**Acceptance.**
  - `make test` passes including `tests/test_animated.c` (`TESTS=` → 32, after `#4`'s `test_clock` → 31).
  - Whole-suite ASan + UBSan clean (per-package gate).
  - **All existing goldens byte-identical** — `FLOW_ANIMATED` defaults off and the dash only diverges from the solid path when `(e->flags & FLOW_ANIMATED) && lod == 0 && (c + f->tick) % 2 != 0`; every existing snapshot renders non-animated edges and (in tests) the default tick, so the edge draw at `src/flow_render.h:189-190` emits the identical cells. This is the package's primary acceptance risk, same bar as the inc-5 culling package.
  - NEW goldens (`animated_tick0`, and any added in step 3) are minted at pinned tick values and staged as part of this package; the discriminator assert (step 3) is mechanical, not a golden.
  - `render_statusbar` golden untouched — per the statusbar rule this package appends NO hint (the animation showcase toggle is a demo-help concern, deferred to the integration pass).
  - `make demos` warning-free — demos are NOT edited here (the showcase toggle that calls `flow_set_edge_animated` is deferred to the final integration pass per the frame; the engine setter lands and is exercised only by the test).
  - flow.h regenerated via `tools/amalgamate.sh` (edits land in existing modules `src/flow_model.h` + `src/flow_render.h`; `modules=` untouched, no new module).

**Depends on.** HARD on `#4` (`redraw-clock`), for two concrete seams that `#4`'s section must name so the consistency pass can match them:
  1. **`f->tick`** — a monotonically advancing counter on `struct flow`'s zero-init block, the value this package reads in the edge loop for the dash phase. Read-only here; advanced only by the run loop (or, in tests, poked directly).
  2. **The frames-needed predicate** — `#4` decides per loop iteration whether to keep ticking. This package requires that decision to DERIVE "animation active" from edge state rather than from a sticky boolean this setter sets, so that (a) clearing one animated edge does not stop frames while another remains, and (b) `flow_load` (which drops all `FLOW_ANIMATED` flags) implicitly disarms with no stale arm. The clean contract is already pinned by `#4`: `#4` defines the recomputed predicate `flow__frames_armed(f)` returning `0` in v1, and this package adds the first `||` clause to it — `flow__frames_armed` ORs in "any `FLOW_ANIMATED` edge exists" (a trivial edge scan at the pinned 10 Hz; early-out on the first animated edge). `flow_set_edge_animated` itself stores NO armed state and calls NO #4 function — the predicate, recomputed each loop iteration by `#4`, derives "animation active" directly from the edge `FLOW_ANIMATED` flags this setter sets/clears (no refcount: #4 rejects arm/disarm counters as strand-prone). **This is the one cross-package coupling: #5 extends the `flow__frames_armed` predicate `#4` defines, called out here so the consistency pass matches the seam name.** No dependency on `#3`/`#6`.

**Conflicts with.**
  - **`redraw-clock` (`#4`)** — shared `struct flow` and shared loop semantics: `#4` adds `f->tick` + the frames-needed state to the zero-init block (`src/flow_model.h:305-343`); this package READS `f->tick` and calls `#4`'s arming seam. No line collision (distinct members), but a hard ordering + API-shape dependency — `#4` lands first and pins the predicate (see Depends on).
  - **`palette-modal-capture` (`#6`)** — both add to `struct flow`'s zero-init block (`#6` a modal flag, this package nothing beyond the flag bit), and both can be active at once (an animated edge under an open modal). No code overlap: `#6` is input-path, this is render-path; the modal does not gate `f->tick` advance. Note recorded so a reviewer confirms neither moves a tick read into the input path.
  - **`devtools-hud` (`#7`)** — composes after edges in the render order (`src/flow_render.h` app-overlay slot) and may surface `f->tick` in a HUD pane; read-only on the same field, no write conflict. Disjoint regions.
  - No conflict with `paste-group-remeasure`/`helper-trailing-edge`/`quit-routing`/`tick-autopan`: those touch the paste undo bracket, the helper guide overlay, the dispatch quit path, and the input/autopan path respectively; this package touches only the flag enum + edge setter (`src/flow_model.h`) and the edge draw loop (`src/flow_render.h`) and adds a standalone test. (`#8 tick-autopan` shares the `f->tick` read but in the input layer, not render — no shared line.)

**Carry-overs fixed.**
  - Closes the oldest for-cause deferral: the inventory's open Animated-edge row (`docs/superpowers/plans/2026-06-03-xyflow-feature-inventory.md:117`, `:362`) and backlog item 15 (`:418` — "Per-edge flag + dash glyph cycle on the run loop's tick; the connection preview already proves the dashed render"), and the handoff's limitation #2 / "dropped animated-edges for cause last time" (`docs/superpowers/plans/2026-06-05-flow-increment-6-session-handoff.md:84`). The clock prerequisite (`#4`) and this consumer together complete xyflow's `Edge.animated` / dashdraw for the in-grid (cell-granularity) case; sub-cell tween smoothness stays cut per spec §1.

---

### 6. Engine-side modal key capture (palette stops leaking Delete)  `[S/M]`  ·  id: `palette-modal-capture`

**Goal.** Close inc-5 known-limitation #4: a CSI typed into the open `/` palette leaks past the hook and acts on the graph behind it. The headline instance is destructive — **Delete (`\x1b[3~`) falls through the palette's key hook (`return 0` at `demos/flowchart.c:136`) all the way to the `flow_delete_selection` built-in (`src/flow_model.h:1335`), so pressing Delete mid-search DESTROYS the current selection.** Same leak path also runs bare-arrow pan (`src/flow_run.h:26-29`), Shift-Tab focus-prev (`src/flow_run.h:30`), Shift-arrow nudge (`src/flow_run.h:42-52`), and Tab focus-next (`src/flow_model.h:1343`). Fix it ENGINE-SIDE with a modal flag on `struct flow`: while modal, the existing key hook stays first-to-see (it already sees arrow CSIs — `src/flow_run.h:22`), and any byte sequence the hook does NOT consume is DROPPED inside `flow_dispatch_key`, never falling through to a binding, a built-in, the feed-level CSI switch, zoom, or lone-ESC. Mouse is structurally exempt (parsed before dispatch). The demo palette adopting the new mode is integration-pass work; this package ships the engine contract + tests.

**User value.** A modal UI built on `flow_set_key_hook` becomes airtight with one call — `flow_set_key_hook_modal(f, 1)` — instead of the demo having to byte-count every CSI in its hook (the `demos/flowchart.c:113-115` v1 limitation comment names exactly this missing work). The palette can stop typing Delete into the graph, stop having bare arrows pan the canvas out from under the search, and (after #3) stop `q` quitting the app mid-search. Apps with no modal UI are completely unaffected: the flag defaults OFF via calloc and the input path is byte-for-byte what it is today.

**Files touched.**
  - src/flow_model.h — modal field on `struct flow`; `flow_set_key_hook_modal` decl + def; the drop branch + `flow__seq_len` helper at the `flow_dispatch_key` hook site; one doc clause on the public `flow_dispatch_key` contract.
  - tests/test_search.c — extends the existing hook test block (no new `TESTS=` stem; frame `TESTS=` pin keeps the count at 30).
  - flow.h is GENERATED — re-run `tools/amalgamate.sh` after editing src/ (`modules=` UNTOUCHED — no new module; the helper lives inside `flow_model`).

**Entry points (existing functions to extend).**
  - `flow_dispatch_key` (`src/flow_model.h:1316-1354`) — the modal drop sits at the existing hook call site (`src/flow_model.h:1321-1324`), immediately after the `if (c > 0) return c;`. No other line in this function changes.
  - `flow_set_key_hook` / `key_hook_fn,key_hook_user` (setter `src/flow_model.h:1271-1273`; fields `src/flow_model.h:335`) — the new setter and field MIRROR these exactly (ABI-append precedent, see Design notes).
  - `flow_feed` (`src/flow_run.h:15-72`) — NOT edited. The drop works entirely through `flow_dispatch_key`'s return value, which `flow_feed`'s existing `if (dk > 0) { i += dk; continue; }` (`src/flow_run.h:22-23`) already honors. The elegance of the design is that the feed path is untouched (see Design notes).
  - `flow_parse_mouse` (`src/flow_input.h:17-36`) — the byte-count-CSI PRECEDENT only (returns bytes consumed or 0); NOT called from here (it lives in a later module — see the placement trap in Design notes).

**API additions.**
```c
/* modal key capture (inc-6 #6): when ON, a sequence the key hook does NOT consume
   is DROPPED inside flow_dispatch_key instead of falling through to bindings /
   built-ins / flow_feed's CSI switch. Engine state, NOT a hook parameter — every
   existing flow_key_hook stays valid; the typedef is unchanged. Mirrors
   flow_set_key_hook's ABI-append shape and flow_set_helper_lines's boolean setter.
   INERT unless a hook is installed (modal-with-NULL-hook drops nothing). Default
   OFF (calloc 0): modal-off input is byte-identical to today. TRANSIENT — not
   journaled, not saved. */
void flow_set_key_hook_modal(flow_t *f, int on);
/* (flow_key_hook typedef at src/flow_model.h:177 is UNCHANGED.
    flow_set_key_hook(f, fn, user) — the 3-arg setter — is UNCHANGED.) */
```

**Design notes.**

*Why a separate setter, not a mode-arg or a magic return (open question 8).* Three shapes were on the table; the separate boolean setter wins on ABI and on honoring the frame's own pins. (a) The frame's struct-additions pin (the `struct flow` additions pin in Cross-cutting rules) explicitly says "#6's modal flag beside the existing key_hook_fn/user" — a NEW field, not a signature change. (b) It mirrors how the hook itself was added — `flow_set_key_hook` is a standalone symbol writing a standalone field (`src/flow_model.h:1271-1273`), and it mirrors the existing boolean toggle `flow_set_helper_lines(f, on)` (`src/flow_model.h:1267-1269`). REJECT the **mode-argument** (`flow_set_key_hook(f, fn, user, modal)`): it breaks the live 3-arg ABI — `tests/test_search.c:44,54,60,68` and `demos/flowchart.c` all call the 3-arg form, and the frame's "modal off byte-identical" bar forbids touching them. REJECT the **magic hook-return** (e.g. a sentinel like `INT_MIN` meaning "I'm modal, drop"): it overloads the bytes-consumed contract (`src/flow_model.h:172-174`), contradicts the struct-flag pin, and pushes the drop burden back into every hook — the precise demo-side work the engine-side fix exists to ELIMINATE. The trap with all three is forgetting that modal-ness is engine policy, not hook data: keeping the `flow_key_hook` typedef (`src/flow_model.h:177`) unchanged is half of "every existing hook stays valid."

*The mechanism — the drop lives in flow_dispatch_key, and flow_feed is untouched.* At the hook call site (`src/flow_model.h:1321-1324`), after the existing consume check, add one branch:
```c
  if (f->key_hook_fn) {
    int c = f->key_hook_fn(f, seq, n, f->key_hook_user);
    if (c > 0) return c;
    if (f->key_hook_modal) return flow__seq_len(seq, n);  /* drop unconsumed while modal */
  }
```
Returning a positive byte count makes `flow_dispatch_key` report "consumed N bytes." `flow_feed` already advances and `continue`s on a positive dispatch return (`src/flow_run.h:22-23`), so a dropped sequence NEVER reaches: the registry/built-ins below it (`src/flow_model.h:1326-1353`, incl. the Delete built-in at `:1335` and Tab at `:1343`), the feed-level CSI switch (`src/flow_run.h:24-53`, incl. bare-arrow pan, Shift-Tab `Z`, Shift-arrow nudge), the zoom keys (`src/flow_run.h:56-57`), or lone-ESC (`src/flow_run.h:69`). The trap this AVOIDS: an alternate design that intercepts in `flow_feed` would have to re-implement the "is this a sequence the hook owns" logic that `flow_dispatch_key` already owns, and would fight the mouse-parse-first ordering. Putting the drop where the hook already runs means `flow_feed` is not edited at all — state that as the design's whole point.

*The footgun: gate the drop behind `if (f->key_hook_fn)`.* The drop branch sits INSIDE the `if (f->key_hook_fn)` block deliberately. If modal-on with a NULL hook dropped sequences, the engine would silently eat ALL non-mouse input and the app would appear frozen — a nasty, hard-to-diagnose footgun for anyone who flips modal before installing a hook (or after clearing it). Modal is therefore defined as INERT without a hook: the doc and a dedicated test pin this. (`flow_set_key_hook_modal` does not touch `key_hook_fn`, so order of the two setter calls is free.)

*The drop must be sequence-ATOMIC — hence `flow__seq_len`.* Dropping only the ESC byte of `\x1b[3~` would leave `[`, `3`, `~` to re-enter `flow_feed`'s loop as three separate bytes; a printable-appending hook (the palette's path at `demos/flowchart.c:152`) would then inject `[3~` straight into the query — a worse bug than the one we're closing. So the drop must return the FULL sequence length. No length helper exists today (`flow_parse_mouse` at `src/flow_input.h:17` is the byte-count precedent but parses ONLY the `\x1b[<` mouse form and returns 0 on anything else), so add a tiny private `flow__seq_len(const char *seq, int n)` that returns: for a CSI (`seq[0]==0x1b && n>=2 && seq[1]=='['`) the count from ESC through the first final byte in `0x40..0x7e` (`@`–`~`, the CSI terminator class — this spans `A/B/C/D` arrows = 3, `Z` = 3, `3~` Delete = 4, `1;2A` Shift-arrow = 6, all the leaking forms), clamped to `n`; otherwise `1` (any single control byte or printable). PLACEMENT TRAP: this helper MUST live in `flow_model.h` (just above `flow_dispatch_key`), NOT in `flow_input.h` beside `flow_parse_mouse` — the amalgamation order is `… flow_model … flow_input …` (`tools/amalgamate.sh:5`), so a flow_model caller cannot see a flow_input definition without a forward declaration. Self-contained in flow_model is the clean placement; `flow_parse_mouse` is the spiritual precedent, not a dependency.

*Split-CSI clamp (reuse the existing trade-off).* If a CSI is split across a `read()` boundary exactly after its ESC, `n` may not cover the final byte; `flow__seq_len` clamps to `n` and drops what it has. This is the SAME accepted trade-off `flow_feed`'s lone-ESC path already documents ("terminals write sequences atomically, so this is theoretical … a real fix is an ESC-timeout state machine; out of scope for v1" — `src/flow_run.h:64-68`). Cite that convention rather than inventing a new one. A full ESC-timeout state machine remains out of scope.

*Exactly what modal bypasses, and the mouse exemption (per the brief).* Modal does NOT change WHO sees bytes first — the hook is already the very first consumer today (`flow_dispatch_key` calls it before bindings/built-ins, `src/flow_model.h:1318-1324`; the drift evidence confirms the hook fires for arrow CSIs at `src/flow_run.h:22`). Modal changes only what happens to UNCONSUMED bytes: drop, not fall-through. Because the drop is a positive `flow_dispatch_key` return, it suppresses the registry, every built-in, AND — via `flow_feed`'s `continue` — the feed-level CSI switch, zoom keys, and lone-ESC. The leak set it closes, enumerated: bare arrows (`src/flow_run.h:26-29`), Shift-Tab `\x1b[Z` (`src/flow_run.h:30`), Delete `\x1b[3~` (`src/flow_model.h:1335`), Shift-arrow nudge `\x1b[1;2{A..D}` (`src/flow_run.h:42-52`), Tab `\t` (`src/flow_model.h:1343`), and any other control byte (the demo's `return 0` tail, `demos/flowchart.c:156`). **Mouse is exempt by construction:** `flow_feed` parses the `\x1b[<` mouse CSI and `continue`s BEFORE it ever reaches `flow_dispatch_key` (`src/flow_run.h:18-21`, dispatch at `:22`), so a mouse event never reaches the hook OR the modal gate — modal capture is keyboard-only, matching the hook's own "sees every key/escape sequence but NOT mouse" contract (`src/flow_model.h:175`).

*The `q` story needs #3, and the drop is not what captures `q`.* `q` is special: today the run loop scans the raw read buffer for `q` and sets `running = 0` BEFORE `flow_feed` runs (`src/flow_run.h:82`), so `q` reaches neither the hook nor the modal gate — no engine-side mode can capture it while that scan exists. #3 (quit-routing) DELETES that scan and makes `q` a dispatch built-in behind the hook. THAT is what makes `q` hook-vetoable; this package's modal drop is for the unconsumed CSIs, not `q`. Note the subtlety for the demo: `q` is printable (0x71), so the palette hook CONSUMES it into the query (`demos/flowchart.c:152`, returns >0) — the modal-drop branch is never even reached for `q`. So "q does not quit while the palette is open" is delivered by the hook's existing printable-append path AS SOON AS #3 stops intercepting `q` upstream. That is why #3 is a HARD dependency for the `q` behavior (and only for it — Delete/arrows/Tab need no #3).

*Modal state is an int FIELD, not a node flag bit (correcting the drift JSON).* The drift JSON floats "bit 32u free for modal" — that was the alternative DEMO-variant (a flag-based modal mode the demo would read) and is superseded by the frame. The flags-ledger pin (the flags-ledger pin in Cross-cutting rules) assigns bit 32u to #5's `FLOW_ANIMATED` and states "no other package consumes a flag bit." So modal state is a plain `int key_hook_modal` on `struct flow` in the `key_hook_fn` neighborhood (`src/flow_model.h:335`), calloc-defaulted to 0. This is not a frame contradiction — the drift never mandated the bit; the field is the right shape and the frame is correct, so no frame amendment is recorded. The field is zero-init-safe, non-heap, not journaled, not saved (per the struct-additions and undo pins).

*ESC-close stays a demo-hook decision (open question 8's rider).* The engine contract is deliberately policy-free: hook-first + drop-unconsumed-while-modal, nothing more. Whether ESC CLOSES the palette is a hook decision the demo already owns — its hook consumes lone ESC and clears `fc_pal.open` (`demos/flowchart.c:135-137`). Keeping palette semantics (close-on-ESC, append-printable, Enter-selects) entirely in the demo hook means the engine ships one orthogonal capability that any modal UI reuses, not a palette baked into the engine.

*Public-contract doc clause.* `flow_dispatch_key` is public (`src/flow_model.h:189`) and its return contract is documented at `src/flow_model.h:171-176` / `:189` / `:1318-1320`. Add one clause: "while `key_hook_modal` is set and a hook is installed, an unconsumed sequence returns its dropped length (≥1) instead of 0." Doc-only, but the function is public so the contract must say it.

**Test plan.** Extends the `flow_set_key_hook` block in `tests/test_search.c` (the existing hook test at `tests/test_search.c:38-73`); NO new `TESTS=` stem (frame `TESTS=` pin keeps `Makefile:4` at 30 stems, ending `test_search`). Use the existing `feed()` helper (`tests/test_search.c:14`) and `ASSERT_INT`. RED strategy per the discipline pin: the new setter `flow_set_key_hook_modal` does not exist yet, so every test below is COMPILE-RED until the symbol lands; the headline assertion is additionally BEHAVIORAL-RED with a mechanical discriminator (selection survives Delete — today it is destroyed).

  1. **Headline — Delete does NOT delete while modal.** `flow_new(80,24)`, `flow_register_defaults`, add a node, `flow_select_node` it (`flow_selected_count(f)==1`). Install a palette-style hook that consumes printable+Backspace+Enter+lone-ESC and returns 0 for CSIs (mirroring `demos/flowchart.c:131-157` — or reuse a minimal hook returning 0 for any `0x1b`-led seq). `flow_set_key_hook_modal(f, 1)`. `feed(f, "\x1b[3~")`. ASSERT `flow_selected_count(f) == 1` AND `flow_node_count(f) == 1` — the Delete was dropped, selection and node survive. (Behavioral-RED: with no modal gate, this CSI reaches `flow_delete_selection` at `src/flow_model.h:1335` and the node count drops to 0.)
  2. **Unconsumed CSI is DROPPED, not fallen-through — bare arrow does not pan.** Same modal hook. Record `f->view.ox` (tests may poke struct flow internals per the discipline pin). `feed(f, "\x1b[C")` (right arrow). ASSERT `f->view.ox` unchanged — modal dropped it, so `flow_pan` (`src/flow_run.h:28`) never ran. Repeat the discriminator with Shift-Tab `feed(f, "\x1b[Z")`: focus-prev (`src/flow_run.h:30`) must NOT fire — assert via the focus id if a node is focused, or simply that no observable state moved.
  3. **Atomic drop — the dropped CSI does NOT desync into the query.** Install a hook that APPENDS every printable to a static buffer and returns 1 for printables, 0 otherwise (the desync canary). Modal on. Clear the buffer. `feed(f, "\x1b[3~")`. ASSERT the buffer is still EMPTY — proves `flow__seq_len` returned the full 4 and `flow_feed` skipped the whole CSI, rather than dropping one byte and re-feeding `[3~` into the printable path. (Without atomicity, the buffer would contain `[3~`.)
  4. **Modal-on with a NULL hook is INERT (the footgun).** `flow_set_key_hook(f, NULL, NULL)` then `flow_set_key_hook_modal(f, 1)`. Select a node. `feed(f, "\x1b[3~")`. ASSERT `flow_node_count` dropped (Delete still deleted) — modal with no hook drops NOTHING; input behaves exactly as un-modal. Guards the `if (f->key_hook_fn)` gate.
  5. **Modal OFF is byte-identical to today.** Install the same consuming hook but `flow_set_key_hook_modal(f, 0)` (or never call it — calloc default). Select a node, `feed(f, "\x1b[3~")`; ASSERT the node IS deleted (CSI fell through, today's behavior). `feed(f, "\x1b[C")`; ASSERT `f->view.ox` DID change (bare arrow panned). This is the "modal off == today" pin made executable.
  6. **A consumed sequence is unaffected by modal.** Modal on, consuming hook. `feed` a printable the hook consumes (returns 1); ASSERT it was consumed exactly as without modal (hook's `>0` return short-circuits before the drop branch — the drop only fires on the hook's 0). Confirms modal does not change the consume path.
  7. **(depends on #3) `q` does not quit while modal.** AFTER #3 lands (`q` a dispatch built-in, raw scan at `src/flow_run.h:82` deleted): `f->running = 1`, modal on, palette hook installed. `feed(f, "q")`. ASSERT `f->running == 1` — the hook consumed `q` (printable append, returns >0) before the `q`-quit built-in could run. RED until #3; gate this assertion behind #3 in the integration order. (Pre-#3 this cannot pass because `q` is intercepted upstream of `flow_feed` entirely — record that, do not write a passing-today variant.)
  8. ASan/UBSan clean: `flow__seq_len` allocates nothing and only reads `seq[0..n)`; the test leaks nothing (`flow_free` each graph). The split-CSI clamp guarantees `flow__seq_len` never reads past `n` — assert nothing here, but the ASan whole-suite gate catches an over-read.

**Acceptance.**
  - `make test` passes including the extended `tests/test_search.c`; `TESTS=` stays at 30 stems (no new file).
  - Whole-suite ASan + UBSan clean (per-package gate).
  - ALL existing goldens byte-identical: this package adds NO render and NO statusbar hint (per the statusbar pin — modal capture is an input concern with no user-facing render). `render_statusbar` and every snapshot are untouched.
  - Modal OFF is provably byte-identical to today (test 5); calloc default is OFF.
  - `make demos` warning-free — demos are NOT edited in this package (the palette going modal is integration-pass work); the engine change is internal to `flow_dispatch_key` + the new setter.
  - `flow.h` regenerated via `tools/amalgamate.sh` (edit lands in `src/flow_model.h`, an existing module; `modules=` unchanged).
  - The public `flow_dispatch_key` contract doc (`src/flow_model.h:171-176`/`:189`) gains the modal-drop clause.

**Depends on.**
  - **HARD: #3 `quit-routing`** — for the `q`-capture behavior ONLY. `q` is hook-vetoable while modal only after #3 deletes the raw `q` scan (`src/flow_run.h:82`) and makes `q` a dispatch built-in. Test 7 is RED until #3 lands. Every other behavior (Delete, arrows, Tab, Shift-arrows) needs nothing beyond already-landed code.
  - Builds on landed inc-5 #10 infrastructure: the key hook field + setter (`src/flow_model.h:335,1271-1273`), the hook call site (`src/flow_model.h:1321-1324`), and `flow_feed`'s dispatch-return honoring (`src/flow_run.h:22-23`).

**Conflicts with.**
  - **#3 `quit-routing`** — both touch `flow_dispatch_key` (`src/flow_model.h:1316-1354`): #3 ADDS a `q` built-in in the built-ins block (`src/flow_model.h:1335-1353` region) and DELETES the raw scan at `src/flow_run.h:82`; #6 adds the modal drop at the hook site (`src/flow_model.h:1321-1324`). Disjoint regions of the same function, but whichever lands second rebases line offsets. Land #3 first (it is earlier in the execution order and is #6's hard dependency).
  - **#5 `animated-edges`** — shares the flags namespace decision: #5 consumes flag bit 32u (`FLOW_ANIMATED`, `src/flow_model.h:2`); #6 deliberately does NOT consume a flag bit (modal is an `int` field at `src/flow_model.h:335`). No code conflict — recorded so a reviewer confirms #6 did not also grab a bit.
  - **#4 `redraw-clock`** — both add zero-init members to `struct flow` (#4 the tick counter + interval `tick`/`tick_ms` — arming is a recomputed predicate, no stored field; #6 `int key_hook_modal`). Same zero-init block (`src/flow_model.h:305-340` region), append-only, no ordering dependency; whichever lands second rebases struct line offsets.
  - No conflict with #1 `paste-group-remeasure`, #2 `helper-trailing-edge`, #7 `devtools-hud`, or #8 `tick-autopan`: those touch the paste undo bracket, helper-guide render, the `on_overlay` HUD, or `flow_input.h` autopan — none touch `flow_dispatch_key`'s hook site or the `key_hook_*` struct neighborhood.

**Carry-overs fixed.**
  - Closes inc-5 known-limitation #4 (modal key capture), documented verbatim in the demo at `demos/flowchart.c:113-115` ("control bytes and CSIs pass through — Tab focus, Shift-arrows, and bare-arrow pan still act while the palette is open"). After this, the demo's palette can drop that limitation comment in the integration pass by calling `flow_set_key_hook_modal(f, 1)` — the byte-count CSI parsing the comment said "would be needed here" now lives once in the engine (`flow__seq_len`), not duplicated in every hook.

---

### 7. DevTools HUD overlay (ViewportLogger + NodeInspector + counts + ChangeLogger)  `[M]`  ·  id: `devtools-hud`

**Goal.** Ship xyflow's DevTools panes — `ViewportLogger`, `NodeInspector`, element `counts`, and a `ChangeLogger` — as a single toggleable **app-side overlay** drawn through the existing `on_overlay` callback (`src/flow_model.h:265`, invoked at `src/flow_render.h:327`), NOT the statusbar. Three of the four panes compose over already-public read accessors and need ZERO new engine surface. The fourth pane, `ChangeLogger`, is the only one with engine value: it surfaces the undo journal's *recent change*, for which the engine today exposes only the `flow_can_undo`/`flow_can_redo` booleans (`src/flow_undo.h:124-125`). This package adds a minimal, value-returning, read-only **journal-introspection API** (undo depth, redo depth, top-op kind) and grounds the HUD in `demos/flowchart.c`'s overlay; the actual demo toggle key is deferred to the integration pass.

**User value.** A node-graph app developer building on `flow` has no in-frame way to see what the engine is doing: where the viewport is parked (`ox/oy/zoom`), what the focused/selected node's fields are, how many nodes/edges exist, or whether the last `u`/`^r` actually moved the journal. xyflow ships exactly this as `@xyflow/react`'s DevTools (`ViewportLogger`/`NodeInspector`/`ChangeLogger`) and the inventory confirms none of it ships here (`docs/superpowers/plans/2026-06-03-xyflow-feature-inventory.md:269`, unchecked). The HUD is the standard "what is the engine seeing right now" affordance — invaluable while writing custom node types whose `render()` only fires when on-screen (the cull from inc-5 `viewport-culling`). The journal accessors are also reusable headless API: any app can show an undo-depth indicator or gate a "you have unsaved changes" prompt off `flow_undo_depth(f) > 0`.

**Files touched.**
  - src/flow_model.h (declare the three journal accessors beside `flow_can_undo`/`flow_can_redo` at `src/flow_model.h:218-219`)
  - src/flow_undo.h (define them beside `flow_can_undo`/`flow_can_redo` at `src/flow_undo.h:124-125`)
  - tests/test_undo.c (extend — engine accessors only; see *Test placement*. Matches the `TESTS=` accounting pin in Cross-cutting rules)
  - demos/flowchart.c (HUD overlay composed in `fc_overlay`, `demos/flowchart.c:169-182`; the toggle-key BINDING is deferred to the integration pass — see *Demo host*)
  - flow.h regenerated via tools/amalgamate.sh (edits land in `flow_model`/`flow_undo`, existing modules; `modules=` UNTOUCHED)

**Entry points (existing functions to extend).**
  - `on_overlay` callback (`src/flow_model.h:265`, "draw HUD/panels last"), invoked at `src/flow_render.h:327` over a full-screen surface `{ &cb, 0, 0, cols, rows, 0, 0, cols, rows }` AFTER the minimap (`src/flow_render.h:326`) and BEFORE the statusbar (`src/flow_render.h:331-342`). This is the spec-designated, xyflow-`<Panel>`-equivalent z-slot for an app HUD. The HUD is composed inside `demos/flowchart.c`'s existing `fc_overlay` (`demos/flowchart.c:169`), the working precedent.
  - Read accessors the HUD consumes, ALL already public — VERIFIED to exist, no new surface for these three panes:
    - *ViewportLogger* — `flow_view_get` (`src/flow_model.h:889`, decl `:83`) returns `flow_viewport { float ox, oy, zoom; }` (`src/flow_geom.h:5`); `flow_zoom` (`src/flow_view.h:11`, decl `:5`) is the zoom-only shortcut.
    - *counts* — `flow_node_count` (`src/flow_model.h:789`, decl `:53`), `flow_edge_count` (`src/flow_model.h:790`, decl `:54`).
    - *NodeInspector* — `flow_focused_node` (`src/flow_model.h:1422`, decl `:105`), `flow_selected_count` (`src/flow_model.h:942`, decl `:124`), `flow_selected_nodes` (`src/flow_model.h:943`, decl `:125`) to pick the subject id; then `flow_get_node` (`src/flow_model.h`, decl `:51`) for `n->id/type/pos/parent/w/h/flags`, `flow_node_abs` (decl `:57`) / `flow_node_pos` (`src/flow_model.h:676`, decl `:59`) for coords. Optional label read via the `label()` vtable through `flow_node_type_for` (`src/flow_query.h:99-100` is the precedent path).
  - Drawing primitives, all public and exercised by `demos/topo.c:152-159`: `flow_box` (`src/flow_cell.h:26`), `flow_text` (`:25`), `flow_put` (`:24`), `flow_surface_w`/`flow_surface_h` (`:27-28`) for bottom-right anchoring.

**API additions.**
```c
/* Minimal read-only journal introspection (open question 9) — the ONLY new engine surface,
   feeding the ChangeLogger pane. Value-returning: never a pointer into the journal,
   so the flow__-prefixed internals (struct flow__cmd / flow__op, src/flow_model.h:242-261)
   stay opaque and ABI-free. Declared beside flow_can_undo/flow_can_redo at
   src/flow_model.h:218-219; defined beside them at src/flow_undo.h:124-125. */
int flow_undo_depth(flow_t *f);   /* count of UNDO steps available  (== f->journal.n)  */
int flow_redo_depth(flow_t *f);   /* count of REDO steps available  (== f->journal.rn) */
int flow_top_op(flow_t *f);       /* flow_cmd_kind of the LAST op of the top undo command,
                                     i.e. the most recent recorded mutation; -1 if the undo
                                     stack is empty (n==0). The returned int is a flow_cmd_kind
                                     enumerator (src/flow_model.h:230-235), an EXISTING public
                                     enum above the FLOW_IMPLEMENTATION guard — no new type. */
```

**Design notes.**

*Why `on_overlay`, NOT the statusbar (the host question, settled).* The candidate's "WHERE TO LOOK FIRST: statusbar" hint is wrong and the drift pass refuted it. `render_statusbar` (`src/flow_render.h:331-342`) is bottom-row-only, a SINGLE help string locked to a golden prefix at cols=30 (`src/flow_render.h:336-341`), and the frame's STATUSBAR rule forbids any package from appending to it this increment (the Statusbar pin in Cross-cutting rules). A multi-line viewport/inspector/counts/change panel cannot live there. The correct host is the SEPARATE overlay pass `on_overlay`, drawn over a full-screen `{0,0,cols,rows}` surface (`src/flow_render.h:327`) at the same z-slot xyflow's `<Panel>`/DevTools occupy — after the minimap, before the statusbar, so the HUD never fights the engine's own chrome. The frame's HUD-layering pin (the HUD-layering pin in Cross-cutting rules) names exactly this: "an app-side overlay composed via `on_overlay` — engine additions are limited to read-only accessors; the statusbar and engine render passes are untouched." The demos already prove the host works for HUD-shaped content: `demos/topo.c:140-160` draws a boxed device-details panel anchored bottom-right via `flow_surface_w/h`. The trap: `on_overlay` runs on a surface whose origin is the buffer (`ox=oy=0`), so the HUD draws in absolute screen coords — anchor to `flow_surface_w(s)`/`flow_surface_h(s)`, not to a node-local surface.

*Why the journal accessors are the package's only engine work (open question 9, M not S).* Three of the four panes are pure composition over public accessors (verified above) — that alone would be an S, demo-only package needing zero engine surface (the drift `sizing` field says exactly this). The M comes entirely from the `ChangeLogger` pane: the undo journal (`f->journal`, `src/flow_model.h:352-360`) records EVERY mutation as a `struct flow__cmd` op span, but the only public window into it is `flow_can_undo`/`flow_can_redo` (`src/flow_undo.h:124-125`) — two booleans. There is no way to read undo *depth*, redo *depth*, or what the last change *was*. The inventory flags this precise gap: "Undo journal is the data source (records every mutation); no logger panel exposes/streams it" (`docs/superpowers/plans/2026-06-03-xyflow-feature-inventory.md:241`). So the engine work is three minimal accessors over the journal's existing fields. The frame recommends including them (open question 9: "the journal accessors — they are the only HUD leg with engine value"); I follow that. The degrade path is stated under *Acceptance*.

*Value-return, never a journal pointer (the ABI trap).* `flow_undo_depth`/`flow_redo_depth` return `f->journal.n`/`f->journal.rn` — plain ints. `flow_top_op` returns the `flow_cmd_kind` of the top command's last op. CRITICALLY it returns the ENUM VALUE, not a `flow__op*` or `flow__cmd*`: those types are `flow__`-prefixed engine internals (`src/flow_model.h:242-261`) living BELOW the `#ifdef FLOW_IMPLEMENTATION` guard (`src/flow_model.h:304`); exposing a pointer would pin their layout as public ABI and forfeit the "read-only/minimal" contract. `flow_cmd_kind` itself is an existing public enum ABOVE the guard (`src/flow_model.h:230-235`), so returning it adds NO new public type. The journal stays fully opaque.

*Which op of a coalesced command, and the empty/disabled/mid-gesture states (the polling trap).* The HUD polls `on_overlay` every present — including a pristine graph, a journaling-disabled graph, and mid-gesture. Each must return a sane value:
  - **Empty stack** (`f->journal.n == 0`): `flow_top_op` returns `-1` (a sentinel outside the `flow_cmd_kind` range, which starts at `FLOW_CMD_ADD_NODE == 0`). A pristine `flow_new` graph and a fully-undone graph both report `-1`. `flow_undo_depth`/`flow_redo_depth` return 0 — never negative (`journal.n`/`rn` are unsigned-in-spirit counters, floored at 0 by the journal logic).
  - **Journaling disabled** (`flow_set_undo_limit(f, 0)`, `src/flow_undo.h:148-151`): the limit-0 path calls `flow__journal_clear`, so `journal.n == 0` and all three accessors report the empty state — correct, the HUD shows "no history."
  - **Mid-gesture** (`journal.txn_depth > 0`, `src/flow_model.h:358`): an OPEN coalescing transaction's command is the top item being appended to; reading `journal.n`/`items[n-1]` is still well-defined (the txn command exists in `items`), so the accessors are safe to call any frame. They do NOT need the `applying`/`txn_depth` guards that `flow_undo`/`flow_redo` use (`src/flow_undo.h:127,137`) — those guard MUTATION; pure reads of count/kind never mutate.
  - **WHICH op** — a coalesced command is an op SPAN (`struct flow__cmd { flow__op *ops; int nops; }`, `src/flow_model.h:261`); e.g. `flow_group` records REPARENT + ADD as one command. `flow_top_op` returns the LAST op's kind (`items[n-1].ops[items[n-1].nops - 1].kind`), because for a "most recent change" read the trailing op is the gesture's final effect (the ADD for a group), which reads more intuitively in a ChangeLogger than the opening REPARENT. Documented in the API comment so the choice is contractual, not incidental.

*`flow_can_undo`/`flow_can_redo` stay; the depth accessors are additive.* I do NOT replace the booleans — existing tests and `demos` use them, and `flow_can_undo(f) == (flow_undo_depth(f) > 0)` by construction (`src/flow_undo.h:124`). The depth accessors are a strict superset. No deprecation, no churn.

*Demo host: flowchart, toggle deferred to integration (the Final integration pass, Execution order).* `demos/flowchart.c` is the right home: it already wires `on_overlay` (`fc_overlay`, `demos/flowchart.c:169-182,215`), has undo/redo live (`u`/`^r` built-ins) so the ChangeLogger has something to show, and is the richest editing demo. The HUD panes compose into `fc_overlay` exactly as `topo.c`'s detail panel does (`demos/topo.c:152-159`). The ACTUAL toggle key is collision-checked but its BINDING is deferred to the integration pass per the frame's demo-wiring rule (the Final integration pass, Execution order). Key-map check against the frame's taken set — built-ins `Delete/x n f ? Space u ^r \t \r y c p d` + `q` (#3); flowchart demo bindings `l g G h H / a` (`demos/flowchart.c:205-212`). Recommended HUD toggle: **`F2`** (the conventional debug-overlay key, an unbound CSI/function-key sequence that collides with nothing in the letter-only built-in set) or, if a plain letter is preferred for `ISIG`-letter consistency (the Key map pin in Cross-cutting rules), **`i`** ("inspect" — currently unbound). I recommend `F2`; the integration pass picks and re-checks against the final table.

*Goldens are byte-identical, trivially.* This package touches ZERO engine render passes — `flow_render` (`src/flow_render.h`) is unmodified, `render_statusbar` (`src/flow_render.h:331-342`) is unmodified, no statusbar hint is appended (the Statusbar pin in Cross-cutting rules), no golden is re-minted. The HUD is demo-side `on_overlay` content (untested by goldens), and the engine change is three pure-read accessors that emit nothing. Every existing snapshot in `tests/` stays identical at this package boundary by construction.

*No struct flow / flag / callback additions.* The accessors READ `f->journal` (existing, `src/flow_model.h:352-360`); they add no field to `struct flow`, consume no flag bit (bit 32u stays for #5's `FLOW_ANIMATED`, the flags-ledger pin in Cross-cutting rules), and add no `flow_callbacks` field (the tail is pinned, the Gates-vs-observers pin in Cross-cutting rules). So there is zero overlap with the zero-init-block packages #4 (tick counter) and #6 (modal flag).

**Test plan.**

*Test placement (matches the frame's `TESTS=` pin).* The frame assigns #7 to `test_undo` (the `TESTS=` accounting pin in Cross-cutting rules) — and the code agrees: this package adds NO render behavior, only journal-introspection read accessors, so `test_render` would have nothing to assert. The accessors read `f->journal.n/.rn` and op kinds; their natural home is `tests/test_undo.c`, which ALREADY pokes `f->journal.n` directly (`tests/test_undo.c:6-7`, e.g. `:36`). I extend `test_undo.c`, exactly as the frame pins. NO new `TESTS=` stem is added (the Makefile already lists exactly 30 stems at `Makefile:4`, the frame baseline per the `TESTS=` accounting pin in Cross-cutting rules — `+0 relative to predecessor`).

RED strategy (per the discipline pin): the new accessors are NEW public API, so the tests are **compile-RED** — `tests/test_undo.c` referencing `flow_undo_depth`/`flow_redo_depth`/`flow_top_op` fails to compile until the symbols exist in `src/flow_undo.h` (and amalgamated into `flow.h`). Snapshot lines (none here — engine accessors emit no cells) are irrelevant. New asserts are appended to existing blocks in `test_undo.c`.

  1. **Fresh-engine empty state.** `flow_new(80,24)`, `flow_register_defaults`. Assert `flow_undo_depth(f) == 0`, `flow_redo_depth(f) == 0`, `flow_top_op(f) == -1` (the empty sentinel). Mirror against the landed booleans: `flow_can_undo(f) == 0` (precedent `tests/test_undo.c:20-21`). RED: undefined symbol until the accessors land.
  2. **Depth tracks the stack; top-op reports the last kind.** Add two nodes (precedent `tests/test_undo.c:33-36`). Assert `flow_undo_depth(f) == 2` (matches the existing `f->journal.n == 2` poke at `tests/test_undo.c:36`, proving the accessor == the internal counter), `flow_redo_depth(f) == 0`, `flow_top_op(f) == FLOW_CMD_ADD_NODE`. `flow_undo(f)`; assert `flow_undo_depth(f) == 1`, `flow_redo_depth(f) == 1`, `flow_top_op(f) == FLOW_CMD_ADD_NODE` (the surviving first add). `flow_redo(f)`; assert depths `2/0` again.
  3. **Top-op reflects op KIND across mutation types.** `flow_move_node` an existing node; assert `flow_top_op(f) == FLOW_CMD_MOVE_NODE`. `flow_add_edge`; assert `flow_top_op(f) == FLOW_CMD_ADD_EDGE`. Proves `flow_top_op` reads the actual top command's kind, not a constant.
  4. **Coalesced command returns its LAST op (the documented choice).** Build a selection and `flow_group` it (records REPARENT + ADD as ONE command — `src/flow_undo.h` REPARENT/ADD appliers, `src/flow_model.h:234`). Assert `flow_undo_depth(f)` incremented by exactly 1 (one user-visible step, the journal contract `tests/test_undo.c:6`), and `flow_top_op(f) == FLOW_CMD_ADD_NODE` (the trailing op — the group container's ADD — NOT the opening REPARENT). Discriminator that locks the last-op-not-first-op contract.
  5. **Journaling-disabled and fully-undone both read empty.** `flow_set_undo_limit(f, 0)` (`src/flow_undo.h:148-151`); assert all three accessors report the empty state (`0/0/-1`) and the graph still mutates (add a node — count rises, depth stays 0). Separately, undo every step on a small graph; assert `flow_undo_depth(f) == 0`, `flow_top_op(f) == -1`. Proves the empty sentinel for both reachable empty states.
  6. **ASan/UBSan clean (whole-suite gate).** The accessors allocate nothing and only index `journal.items[n-1].ops[nops-1]` when `n > 0` (guarded by the `-1` early return) — no OOB read on an empty or txn-open journal. `flow_free` each graph; no leak.

**Acceptance.**
  - `make test` passes including the extended `tests/test_undo.c` (no new `TESTS=` stem — `Makefile:4` already lists exactly 30 stems; this package adds 0).
  - Whole-suite ASan + UBSan clean (per-package gate).
  - **ALL existing goldens byte-identical** — trivially: this package touches zero engine render passes, `render_statusbar` (`src/flow_render.h:331-342`) is unmodified, no golden re-mint, no statusbar hint appended (the Statusbar pin in Cross-cutting rules). The HUD is demo-side `on_overlay` content.
  - `make demos` warning-free: `demos/flowchart.c` gains HUD composition in `fc_overlay` (`demos/flowchart.c:169`) using only existing public accessors + the three new ones; no new warning (the carved-out `tests/test_model.c:8` `-Wmissing-field-initializers` diagnostic — message "missing field 'save'", a COMPILER report of the vtable's first missing field, not file text — is the only allowed warnings-sweep hit).
  - flow.h regenerated via tools/amalgamate.sh (edits in `src/flow_model.h` + `src/flow_undo.h`, existing modules; `modules=` UNTOUCHED, the Discipline pin in Cross-cutting rules).
  - **Degrade path (open question 9).** If the journal accessors are CUT at sign-off, the package degrades to **demo-only S**: the ViewportLogger/NodeInspector/counts panes still ship over the existing public accessors (verified above), the ChangeLogger pane drops to the two `flow_can_undo`/`flow_can_redo` booleans only, `tests/test_undo.c` gains no extension, and the engine surface is untouched. The HUD still lands; only the change-introspection depth is lost.

**Depends on.** Nothing hard. Builds only on landed code: the `on_overlay` host (`src/flow_model.h:265`, `src/flow_render.h:327`), the read accessors (all cited above), the journal struct (`src/flow_model.h:352-360`, `flow_cmd_kind` `:230-235`), and the surface primitives (`src/flow_cell.h:24-28`). No dependence on the run-loop rework (#3/#4) — the HUD reads on every present regardless of how the present is driven. (A future on_tick refresh is not needed: `on_overlay` already runs each `flow_present`.)

**Conflicts with.**
  - **None on engine fields.** This package adds no `struct flow` field, no flag bit, no `flow_callbacks` member — so it does NOT touch the zero-init block that #4 (`redraw-clock`, tick counter + interval) and #6 (`palette-modal-capture`, modal flag) extend (the `struct flow` additions pin in Cross-cutting rules). Disjoint.
  - **`redraw-clock` (#4)** — same FILE region risk only: #4 reworks `flow_run`/the present loop; this package adds accessors in `src/flow_undo.h` (untouched by #4) and declarations in `src/flow_model.h:218-219` (a stable header zone #4 does not edit). Whichever lands later in `src/flow_model.h` rebases declaration line offsets only; no semantic conflict.
  - **`animated-edges` (#5)** — shares the flag namespace (`src/flow_model.h:2`) conceptually but this package consumes NO flag bit, so #5's claim on bit 32u (`FLOW_ANIMATED`) is unaffected (the flags-ledger pin in Cross-cutting rules). No conflict.
  - **Integration pass** — the HUD toggle key BINDING in `demos/flowchart.c` is deferred to the integration pass (the Final integration pass, Execution order), collision-checked against the final key map there; recommended `F2` (or `i`).

**Carry-overs fixed.**
  - Closes the inventory's open DevTools rows: item 21 "DevTools overlay HUD (NodeInspector + ViewportLogger + ChangeLogger) — M" (`docs/superpowers/plans/2026-06-03-xyflow-feature-inventory.md:424`), the unchecked "DevTools UI" row (`:269`), and the unchecked "DevTools: ChangeLogger" row whose noted gap was "no logger panel exposes/streams" the journal (`:241`). After this, all three panes ship and the journal exposes its depth + most-recent-change tag for the first time.

---

### 8. Tick-driven auto-pan during object drags  `[S]`  ·  id: `tick-autopan`

**Goal.** Close the documented gap at `src/flow_input.h:58-66`: today auto-pan is purely event-driven — `flow__autopan` (`src/flow_input.h:67-72`) advances ONE step per *motion event*, so a cursor parked in the margin band stops panning the instant the mouse stops moving ("the run-loop-ticked model is a documented follow-up", `src/flow_input.h:62-63`). This package adds the ticked half: while an OBJECT drag (node / multi-node / connection / reconnect / marquee) holds the cursor in the band, the run loop's tick (#4) re-applies the autopan step at the LAST KNOWN cursor cell every frame, so off-screen targets keep scrolling into reach without the user wiggling the mouse. **STRETCH — cuttable at sign-off; nothing depends on it.**

**User value.** "Drag a node to the far edge and hold" finally works the way every desktop canvas does: the view keeps scrolling under a stationary held cursor until the user lets go or pulls back into the interior. Without it, reaching an off-screen drop target requires a continuous mouse jiggle to manufacture motion events — discoverable only by accident, and impossible to hold steady. The connection/reconnect/marquee drags get the same continuous-scroll affordance for free, since they share the one autopan path.

**Files touched.**
  - src/flow_input.h
  - src/flow_model.h (one zero-init-safe field on `struct flow`)
  - tests/test_autopan.c (extended — `test_autopan` already in `TESTS=`, `Makefile:4`)

**Entry points (existing functions to extend).**
  - `flow__autopan` (`src/flow_input.h:67-72`) — reused **as-is**, unchanged. It is the single source of truth for "is the cursor in a live band, and which way does it pan" (incl. the degenerate-axis guard `2*m < cols/rows`, `src/flow_input.h:69-70`, and margin-0-disable). The tick replays a motion through it; it is never re-implemented.
  - `flow_handle_mouse` (`src/flow_input.h:73-412`), MOTION branch (`src/flow_input.h:173-314`) — the new helper replays a synthetic `FLOW_MOUSE_MOTION` event through this function so the tick re-applies pan **and** re-places the dragged object via the exact existing path (`flow__autopan` then `flow_update_connection` / `flow_move_node` / `flow_select_in_rect`). The four eligible motion sites that already call `flow__autopan` (`src/flow_input.h:180, 186, 215, 225`) gain a one-line `f->last_cursor = scr;` beside the call so the tick has a fresh in-band cursor to replay; arming itself is implicit — #4's recomputed `flow__frames_armed` predicate gets an OR-clause reading the live drag state (see Design notes), with no stored arm flag.
  - `flow_new` zero-init block (`src/flow_model.h:524-532`) — `last_cursor` is calloc-zero-safe; no new explicit initializer is strictly required, but it lives in the same transient-interaction-state group as `down_pos`/`marquee_cur` (`src/flow_model.h:316, 323`).

**API additions.**
```c
/* No new PUBLIC API. The tick entry point is a private static helper in
   flow_input.h (visible to flow_run.h: flow_input precedes flow_run in
   modules=, tools/amalgamate.sh:5), driven by #4's poll loop and called
   directly by tests (struct-flow internals-poke is blessed):

       static void flow__autopan_tick(flow_t *f);

   One zero-init-safe struct flow field (transient; not saved/journaled):
       flow_pt last_cursor;   // last autopan-eligible motion cell (screen)
   No `frames_armed` field is added: #4's `flow__frames_armed` is a RECOMPUTED PREDICATE
   (not a stored flag), and this package extends it with one `||` clause that reads the
   existing in-flight-drag state directly (mouse_down + conn_active / reconnect_edge /
   marquee_on / object-drag-in-flight). See *Arming* in Design notes. */
```

**Design notes.**

*Why a synthetic motion event, not a bare `flow__autopan` call (the load-bearing decision and its trap).* The literal scope wording — "reuses `flow__autopan` as-is" — read naively as "call `flow__autopan(f, last_cursor)` on each tick" produces a **visible bug** for every drag that re-places its object from world-under-cursor. Walk a node-drag parked at screen `(79,12)` on an 80-wide buffer: `flow__autopan` does `flow_pan(f, -2, 0)` (`src/flow_input.h:69, 71`), `ox` drops 2, and the node's fixed world point now projects 2 cells LEFT of the stationary cursor. The node is never re-placed, so it slides AWAY from the cursor every tick and snaps back the instant the mouse twitches — the opposite of "drag the node off-screen". The existing motion path solves this by panning FIRST then re-placing at the post-pan `world(cursor)` (`src/flow_input.h:223-225, 251-252`), so the node stays glued. The tick must do the same. The clean way to get it uniformly, for all four drag types, is to replay a synthetic motion at the stored cursor through `flow_handle_mouse` — which runs `flow__autopan` **unchanged, in its existing pan-first slot**, then the branch's own re-placement. This honors "reuse `flow__autopan` as-is" exactly (the helper is byte-for-byte untouched); it just feeds it via the motion path the terminal failed to send. The connection drag survives pan-only because `conn_end` is screen coords (`src/flow_model.h:333`, set at `flow_update_connection`, `src/flow_model.h:1492`) and re-hits at the cursor; but node/multi/marquee re-place from world and would drift — so replay, not bare-call, is mandatory. *This is not a frame deviation: the brief's "if possible" never mandated a standalone call, and the determinism pin is upheld (no wall clock enters; the tick is #4's counter).*

*The synthetic event reads only `type/x/y` — `mods` is irrelevant.* The MOTION branch (`src/flow_input.h:173-314`) derives everything from `flow_pt scr = { ev->x, ev->y }` (`src/flow_input.h:89`) and never reads `ev->mods` (verified: zero `ev->mods` references in 173-314; the marquee branch at `src/flow_input.h:209-221` included — its mode came from `f->marquee_mode`, pinned at press). So `flow_mouse_event ev = { FLOW_MOUSE_MOTION, 0, last_cursor.x, last_cursor.y, 0u }` replays correctly for node, multi-node, connection, reconnect, AND marquee drags. No per-drag-type special-casing in the tick.

*Pan/no-pan is `flow__autopan`'s call; arm/disarm is the recomputed predicate — never a re-implemented in-band test (the second trap).* If `flow__autopan_tick` wrote its own `scr.x < m || scr.x >= cols - m` predicate to decide whether to keep ticking, it would DRIFT from `flow__autopan`'s actual pan decision — most sharply on the degenerate-axis guard (`2*m < extent`, `src/flow_input.h:69-70`) and margin-0-disable (`src/flow_input.h:69-70`, `flow_set_autopan` clamp at `src/flow_model.h:1303-1305`): a re-implemented test could arm-but-not-pan on a dead axis. The fix keeps two single sources of truth: whether the view moves at all is `flow__autopan`'s call on each replay, and whether the clock keeps ticking is #4's recomputed `flow__frames_armed` predicate. So the tick simply replays the synthetic motion, letting `flow__autopan` make the real pan/no-pan call (a no-pan tick is a cheap empty present); the tick re-implements neither the band test nor a disarm. Disarm is NOT a stored flag the tick clears: `flow__frames_armed` re-evaluates next iteration and stops firing on its own once the drag ends (released / no conn / reconnect / marquee), so the loop self-disarms with no `frames_armed` bit to reset. The cases below are the outcomes of that predicate plus `flow__autopan`'s own decision, terminated uniformly without enumerating them:
  - released (`mouse_down=0`, no conn/reconnect/marquee) → motion returns early at `src/flow_input.h:189` → drag no longer in flight → predicate stops firing → disarm;
  - pane-pan → `dragging_pan` branch pans by `scr - last_mouse = 0` (`src/flow_input.h:311-313`) → no autopan, and pane-pan never armed in the first place (it is not an object drag — see below) → predicate stays 0;
  - cursor pulled to interior (drag still live) → `flow__autopan` computes `dx=dy=0` → no pan, but the in-flight drag keeps the predicate firing → no runaway, cheap empty present per tick until release;
  - dead axis / margin-0 (drag still live) → `flow__autopan` no-ops on that axis → no pan there, predicate still firing on the live drag → cheap empty present, no runaway.
A held-in-interior drag keeps the predicate firing (the drag is still in flight) but each tick no-pans into a cheap empty present; the loop fully self-disarms the instant the gesture ends, when `flow__frames_armed` stops firing — so #8 need not touch any release site, there being no stored flag to clear.

*Arming: extend `#4`'s `flow__frames_armed` predicate with the in-flight-drag `||` clause; record `last_cursor` at the eligible motion sites (coarse, keeps `flow__autopan` literally as-is).* The four motion call sites — conn (`src/flow_input.h:180`), reconnect (`src/flow_input.h:186`), marquee (`src/flow_input.h:215`), node/multi (`src/flow_input.h:225`) — each gain `f->last_cursor = scr;` beside the existing `flow__autopan(f, scr)`, and `flow__frames_armed` ORs in "an object drag is in flight" (read from the existing drag state: `mouse_down` + `conn_active`/`reconnect_edge`/`marquee_on`/object-drag), with no stored arm flag. This is the approach that leaves `flow__autopan`'s signature and body untouched (the brief's "reuse as-is"). The cost when a drag pauses in the interior is a cheap empty present per tick: the in-flight-drag clause keeps the clock armed, each tick no-pans into an empty diff, and the clock releases only when the gesture ends. That is acceptable and NOT an idle-pin violation — an active drag is live interaction, not an idle graph (open question 5 in the frame concerns the idle, no-drag case, which arms nothing here). The PANE-pan and SPACE-pan drags are excluded by construction: they reach neither call site (`flow__autopan` is only in the conn / reconnect / marquee / object-drag branches, never in the `dragging_pan` branch at `src/flow_input.h:311-313`), so they never arm — the existing rule "pane-pan drags never auto-pan" (`src/flow_input.h:63-64`, `tests/test_autopan.c:109-129`) carries straight through to the tick with no extra guard.

*`last_cursor` is a genuinely new field — no existing one serves.* Verified against the struct (`src/flow_model.h:305-360`): `marquee_cur` is marquee-only (`src/flow_model.h:323`), `conn_end` connection-only (`src/flow_model.h:333`), `last_mouse` pane-pan-only (`src/flow_model.h:312`, and pane-pan never autopans), `down_pos` is the press cell not the current cursor (`src/flow_model.h:316`). A node-drag or reconnect-drag stores NO current screen cursor anywhere. So `flow_pt last_cursor` is required; it is written at every autopan-eligible motion (the same four sites) so a tick can never read a stale `{0,0}` — `last_cursor` is written at every autopan-eligible motion, so an armed tick always has a fresh cursor from a real in-band motion.

*Reconnect ticks pan-only, by design.* The reconnect drag re-places no object — it just tracks movement and repoints on release (`src/flow_input.h:184-188`, "nothing to re-place: release hit-tests at the cursor"). The replayed motion runs `flow__autopan` then returns at `src/flow_input.h:187`; the view scrolls, no rubber-band follows the cursor (there is none). This matches the real-motion behavior exactly. Recorded as an intended difference, not a gap.

*No undo / threshold re-entry on replay.* Mid-drag, `f->moved == 1` already, so the threshold block that holds `flow__undo_begin` (`src/flow_input.h:190-208`) is skipped on every replayed motion — no new undo bracket, no re-classification, no `down_*` re-arming. The tick is a pure continuation of the in-flight gesture. Nothing here journals (frame Undo pin: tick/animation state is transient).

*Observer semantics are unchanged by ticking.* The replay re-runs `flow_move_node` / `flow_select_in_rect` / `flow_update_connection` — the same mutators every REAL motion runs, so the tick adds no new observer behavior, only fires them while the cursor holds still (the whole point). `on_viewport_change` firing once per panning tick is CORRECT (the viewport genuinely changed; sig-gated at `src/flow_model.h:632`). A pan-only marquee tick that re-selects the SAME set fires no `on_selection_change` — it is sig-gated and returns early when `flow__sel_sig` is unchanged (`src/flow_model.h:899-908`, specifically the early return at `:903`), the same world-stable-marquee path inc-5 #3 built on this post-autopan ordering. So a no-op tick is observer-silent except for the legitimate viewport change.

**Test plan.** Extends tests/test_autopan.c (already in `TESTS=`, `Makefile:4`). Reuses the file's existing synthetic-SGR helpers `press_at`/`move_to`/`release_at`/`press_shift_at`/`move_shift_to` and the `org(f)` view-offset probe (`tests/test_autopan.c:16-22`). Mouse-position determinism: every assertion drives ticks + synthetic mouse events directly; no wall clock, no `flow_run` (per the determinism pin).
  - **Compile-RED:** the new tests call `flow__autopan_tick(f)`, an unresolved symbol pre-implementation → the file fails to link. This is the red signal for the helper's existence.
  - **Behavioral-RED discriminator (the assertion that separates this design from the broken pan-only one):** after impl, the node must STAY GLUED to the stationary cursor across N ticks. A naive "call `flow__autopan` only" impl would pan the view but leave the node behind — caught by the node-position assertion in test 1, which checks the node's abs pos tracks the (panned) world-under-cursor, not just that the view moved.
  1. **Ticks keep panning AND keep the node glued.** `flow_new(80,24)`; `flow_register_defaults`; add node "A" at `(10,5)`; `press_at(11,6)` (grab offset 1,1); `move_to(79,12)` (right band) — assert `org(f).x == o0.x - 2` and node abs `== (80,11)` (the landed single-event result, `tests/test_autopan.c:40-44`). Now WITHOUT another mouse event, `flow__autopan_tick(f)` once → assert `org(f).x == o0.x - 4` (a second `-2` step accrued from the tick) AND the node abs followed: `flow_get_node(f,a)->pos.x` advanced to keep the node under the stationary cursor `(79,12)` (recompute = `world(79,12) - grab` at the new `ox`). Tick a second time → `org(f).x == o0.x - 6`, node still glued. Proves: ticks pan, and the object tracks (the discriminator).
  2. **Disarm on release — no runaway pan.** Same setup, drag into the band so the predicate fires (assert `flow__frames_armed(f) != 0`), then `release_at(79,12)`. `flow__autopan_tick(f)` → assert `org(f)` UNCHANGED from its pre-tick value and `flow__frames_armed(f) == 0` (the predicate no longer fires once the drag ends). Tick again → still stable. Guards against ticking forever after the gesture ends.
  3. **No pan when the cursor returns to the interior.** Drag into the right band (armed), then `move_to(40,12)` (interior — `tests/test_autopan.c:45-48` shows interior motion does not pan). Record `org(f)`. `flow__autopan_tick(f)` → assert `org(f)` unchanged; tick again → still unchanged. The drag is still live so `flow__frames_armed(f) != 0` (the clock stays armed by the in-flight drag); it is `flow__autopan` no-panning at the interior cursor — NOT a disarm — that prevents the runaway, so a held-in-interior drag burns no further pans.
  4. **Marquee ticks pan and the rect stays world-anchored.** `press_shift_at(50,10)`; `move_shift_to(79,22)` (bottom-right bands) — assert the landed `org(f) == o0 - (2,2)` (`tests/test_autopan.c:137-139`) and `flow__frames_armed(f) != 0`. `flow__autopan_tick(f)` → assert `org(f) == o0 - (4,4)` (both axes stepped again) and `f->marquee_on == 1` (gesture still live). Proves marquee replays correctly via the synthetic motion (mods-independent path).
  5. **Connection-drag ticks pan; `conn_end` stays at the cursor.** `flow_set_hover(f,a)`; `press_at(14,6)` (right handle); `move_to(79,12)` (`tests/test_autopan.c:83-87`) — assert `f->conn_active == 1`, `org(f).x == o0.x - 2`. `flow__autopan_tick(f)` → assert `org(f).x == o0.x - 4`, `f->conn_active == 1`, and `f->conn_end.x == 79` (free end still screen-pinned at the cursor — the pan never moves it).
  6. **Reconnect-drag ticks pan-only (no object follows).** Build `a→b` edge; arm reconnect at the target endpoint (`tests/test_autopan.c:93-104`); `move_to(79,12)` → `org(f).x == o0.x - 2`, `flow__frames_armed(f) != 0`. `flow__autopan_tick(f)` → `org(f).x == o0.x - 4`, `f->reconnect_edge == e` (still armed), and the edge's `target` UNCHANGED (no repoint until release). Confirms intended pan-only-for-reconnect.
  7. **Pane-pan and space-pan never arm.** `press_at(50,10)`; `move_to(79,10)` (pane-pan into the band, `tests/test_autopan.c:111-114`) — assert `flow__frames_armed(f) == 0`. `flow__autopan_tick(f)` → `org(f)` unchanged (no autopan from a pane drag). Repeat with `feed(f," ")` space-pan armed (`tests/test_autopan.c:120-126`) → `flow__frames_armed(f) == 0`, tick is a no-op. The "pane-pan never auto-pans" rule survives ticking.
  8. **Degenerate axis stays dead under ticks.** `flow_new(24,6)` (`2*3 >= 6` → y-axis dead, `tests/test_autopan.c:174-187`); node-drag with cursor in the x-band but y in the (dead) top band; `flow__autopan_tick(f)` repeatedly → x keeps panning while in-band, `org(f).y` NEVER moves. Proves the tick reads `flow__autopan`'s guard (the live pan decision), not a re-implemented predicate — `flow__autopan` no-pans the dead y-axis on every replay rather than any disarm logic enumerating it.
  9. **ASan/UBSan clean:** the helper allocates nothing (it stack-builds one `flow_mouse_event`); each graph `flow_free`d. No leaks, no UB.

**Acceptance.**
  - `make test` passes including the extended tests/test_autopan.c.
  - Whole-suite ASan + UBSan clean (per-package gate).
  - **Every existing golden byte-identical.** This package touches no render path — it only re-runs interaction mutators that already exist. `render_helper_snap`, `render_statusbar`, and all snapshot goldens are untouched; this package APPENDS no statusbar hint (autopan is an interaction concern, per the STATUSBAR pin).
  - Every pre-existing assertion in tests/test_autopan.c still passes unchanged (the event-driven path is byte-for-byte preserved; `flow__autopan` is not edited).
  - `make demos` warning-free (demos are not edited; the tick is wired by #4 / the integration pass, not here).
  - `tools/amalgamate.sh` regenerates flow.h (edits land in src/flow_input.h + src/flow_model.h, existing modules; `modules=` UNTOUCHED).
  - Warnings sweep clean except the carved-out `tests/test_model.c:8` 'save' line.

**Depends on.** **HARD on #4 (`redraw-clock`).** #8 supplies the per-tick work; #4 owns the loop that calls it. The concrete interface #8 needs from #4: (a) the recomputed predicate `flow__frames_armed(f)` that #4's poll loop reads to choose its timeout (#4's loop-contract Design note — a not-yet-landed site, cited by contract not line) — #8 extends it with one `||` clause, "an object drag is in flight" (read from the existing drag state: `mouse_down` + `conn_active`/`reconnect_edge`/`marquee_on`/object-drag), so #4 needs no arm flag and #8 stores none; the predicate self-disarms the instant the gesture ends, with no paired-disarm site to strand. (b) #4's testable tick seam calls `flow__autopan_tick(f)` once per timeout tick; #5 advances nothing in the loop (it reads `f->tick` at render time), so there is no ordering to coordinate. The exact symbol name of the predicate and seam is **#4's to pin** (`flow__frames_armed` / the tick seam); #8 conforms by adding its clause to whatever #4 names — the contract (one OR clause on the recomputed predicate + a per-tick hook) is what binds. Also builds on already-landed `flow__autopan` (`src/flow_input.h:67-72`) and `flow_handle_mouse` (`src/flow_input.h:73-412`).

**Conflicts with.**
  - **#4 `redraw-clock`** — shares the `struct flow` zero-init block (`src/flow_model.h:305-360`): #4 adds the tick counter (`tick`, `tick_ms`) and the recomputed `flow__frames_armed` predicate (NOT a stored arm field); #8 adds only `last_cursor` and extends `flow__frames_armed` with its in-flight-drag OR clause. No arm-flag field to coordinate. Both must land in dependency order (#4 first). #8 also depends on #4's seam existing, so #8 is the LAST functional package and rebases onto whatever #4 named.
  - **#5 `animated-edges`** — also a #4-tick consumer; both run inside #4's per-tick seam. No code conflict (disjoint state: #5 advances a dash-phase from the tick counter and touches the edge render; #8 replays a motion and touches the view/drag state). Note recorded so a reviewer confirms the seam invokes both and neither assumes it runs alone.
  - **No conflict** with #1/#2/#3/#6/#7: #1-#2 touch paste/helper, #3/#6 touch dispatch/feed key paths, #7 is an overlay. #8 touches only the autopan motion sites in src/flow_input.h, two transient struct flow fields, and tests/test_autopan.c.

**Carry-overs fixed.**
  - Closes the deferral recorded verbatim at `src/flow_input.h:62-63`: "A terminal delivers no events for a stationary cursor, so this only advances while the mouse keeps moving (the run-loop-ticked model is a documented follow-up)." After this package the run loop ticks the autopan step at the held cursor, so a stationary in-band cursor keeps panning — the follow-up the comment promised, scoped to object drags and gated on the same `flow__autopan` decision the event path already uses.
