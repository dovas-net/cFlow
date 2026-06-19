# Changelog

All notable changes to this project are documented here. The format is based on
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and this project adheres
to [Semantic Versioning](https://semver.org/spec/v2.0.0.html). While at `0.x`, the
public API may change between minor versions; it will be locked at `1.0.0`.

## [Unreleased]

## [0.3.0] - 2026-06-19

"Contributor-ready & documented." Adds continuous integration, a clean public/private
header boundary, and a full documentation set. No breaking changes to the public C API.

### Added
- **Continuous integration.** `.github/workflows/ci.yml` runs five jobs: a build+test
  matrix (`{ubuntu, gcc}`, `{ubuntu, clang}`, `{macos, clang}`) under `-Werror`; an
  **amalgamation-drift gate** (`check-amalgamation`) that regenerates `flow.h` and
  rejects a stale committed header; an ASan+UBSan sanitizer job; a `make cpp`
  C++-consumption job; and an advisory `clang-tidy` lint job.
- **Documentation set.** `docs/API.md` (per-function reference for all public symbols),
  `docs/xyflow-mapping.md` (React Flow → flow concept map + terminal divergences),
  `docs/theming.md` (color modes, theme tokens, backgrounds), `docs/ARCHITECTURE.md`
  (amalgamation model, module map, portable-core/POSIX boundary), `CONTRIBUTING.md`, and
  `SECURITY.md`. The README gains a screenshot, CI/release badges, and a documentation
  index.

### Changed
- **Private internals no longer leak into the public header.** The seven `flow__*`
  symbols (`flow__frames_armed`, `flow__undo_begin`/`_end`, and the undo
  snapshot/op/cmd types) moved inside the `FLOW_IMPLEMENTATION` guard; a
  declarations-only preprocess of `flow.h` now exposes zero `flow__` symbols.
  `flow_cmd_kind` stays public.

### Fixed
- **Route out-of-memory hardening.** `flow_route_push` / `flow__addpt` no longer corrupt
  the route buffer on a realloc-into-self failure; three arrowhead writers guard
  `out->count > 0`. New `test_oom` route case.
- Corrected the `flow_layout_opts` `gap_x` / `gap_y` doc comment: they are axis-bound
  (X / Y), and the inter-node vs inter-rank role flips with `dir`.

## [0.2.0] - 2026-06-17

"Embeddable & robust." Makes `flow` a well-behaved library to drop into a host
application: a terminal-free embed path, crash-safe terminal handling, allocator
hooks, signaled out-of-memory, and C++ consumption. No breaking changes to the
public C API.

### Added
- **Headless embed path.** `flow_render_diff(f)` renders, diffs against the previous
  frame, advances the front buffer, and returns the escape string for the host to write
  to its own fd — drive the model from your own event loop with `flow_feed` +
  `flow_render_diff`, no `flow_run`/termios. `flow_present` is now this plus
  `fputs(stdout)`. New `examples/embed_headless.c` and a README "Embedding in your own
  event loop" section.
- **Crash-safe terminal restore.** The terminal path installs async-signal-safe
  `SIGINT`/`SIGTERM`/`SIGHUP`/`SIGQUIT` and crash (`SIGSEGV`/`SIGABRT`/…) handlers plus an
  `atexit` hook that restore raw mode / alt-screen / cursor / mouse-tracking, then chain
  to the prior disposition (a deliberately-ignored signal is preserved). Removed on
  `flow_term_restore`; never installed on the headless path.
- **Allocator + assert hooks.** `FLOW_MALLOC` / `FLOW_CALLOC` / `FLOW_REALLOC` /
  `FLOW_FREE` (override the set together) and `FLOW_ASSERT` route all of `flow`'s heap
  traffic to an embedder's arena/tracker.
- **C++ consumption.** The header is `extern "C"`-guarded and compiles as a C++
  translation unit (clang/gcc, `-std=c++17`); a C-built `flow.o` links against C++
  callers. New `make cpp` check. (MSVC C++ is not yet supported — the color-preset table
  uses C99 array designators.)
- Test suite grown 35 → 39 (`test_embed`, `test_term`, `test_alloc`, `test_oom`).

### Changed
- **Out-of-memory is signaled, not fatal.** `flow_new` returns `NULL`,
  `flow_add_node` / `flow_add_edge` return `-1`, and `flow__grow` no longer leaks the old
  block on `realloc` failure. The header preamble documents the OOM policy and its
  boundary (render/layout scratch and snapshot buffers still assume success).

### Fixed
- `flow_undo` / `flow_redo` no longer dereference NULL (crash + leak) when growing the
  journal stacks under memory pressure; the history entry is dropped instead.

## [0.1.0] - 2026-06-17

First public release. `flow` is a single-header C library (`flow.h`) for building
interactive node-graph editors in the terminal — a C/terminal analog of
[xyflow](https://github.com/xyflow/xyflow) (React Flow). Dependency-free (pure ANSI
escapes; links `-lm`).

### Added
- Single-header distribution: `#define FLOW_IMPLEMENTATION` in one translation unit,
  generated from `src/` by `tools/amalgamate.sh`.
- Nodes and edges with a custom-type system (measure/render vtables of function pointers).
- Handles with connect / reconnect and a pluggable connection validator.
- Edge labels and straight / orthogonal (step) edge routing.
- Selection: single, multi, and marquee.
- Node dragging with auto-pan; per-element interaction gates (draggable / selectable /
  deletable); SE-corner node resizer with explicit, persisted node sizes.
- Viewport: pan (arrows / drag / scroll), pointer-centered zoom with limits, fit-view,
  and level-of-detail collapsing.
- Minimap, controls, status bar, and node/edge toolbars.
- Subflows / node groups (`flow_group` / `flow_ungroup` / `flow_set_parent`).
- Undo / redo via a command journal with coalescing.
- Force-directed and layered (Sugiyama-style) auto-layout.
- JSON save / load (versioned save format).
- Theming: DEFAULT / LIGHT / DARK color modes, theme tokens, and backgrounds (dots /
  lines / cross).
- A damage-diffed cell compositor and a run-loop (`flow_run` / `flow_feed` / `flow_present`).
- Test suite: 35 headless suites with byte-exact UTF-8 snapshot goldens.

### Notes
- The interactive run-loop (`flow_run` / `flow_feed` / `flow_present` / `flow_term_*`)
  requires a POSIX terminal (Linux / macOS). The model, geometry, rendering, routing,
  layout, and JSON layers are portable C99 and embeddable with the host's own I/O.

[Unreleased]: https://github.com/dovas-net/cFlow/compare/v0.3.0...HEAD
[0.3.0]: https://github.com/dovas-net/cFlow/compare/v0.2.0...v0.3.0
[0.2.0]: https://github.com/dovas-net/cFlow/compare/v0.1.0...v0.2.0
[0.1.0]: https://github.com/dovas-net/cFlow/releases/tag/v0.1.0
