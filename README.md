# flow

[![license: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
![single-header C99](https://img.shields.io/badge/single--header-C99-green.svg)

`flow` is a single-header C library for building interactive node-graph editors in the
terminal — a C/terminal analog of [xyflow](https://github.com/xyflow/xyflow) (React Flow).
Dependency-free: pure ANSI escapes, links only `-lm`.

> Try it: `make && ./demos/topo` — a small network-topology editor (live pan, mouse
> drag, zoom, minimap). `./demos/hello_flow` is the minimal showcase.

<!-- TODO: embed a recorded GIF of ./demos/topo here once captured (vhs/asciinema). -->

## Features

Working today (each maps to a familiar xyflow concept):

- **Nodes & edges** with a custom-type system — measure/render vtables of function pointers
  (`flow_register_node_type` / `flow_register_edge_type`), the C analog of `nodeTypes`/`edgeTypes`.
- **Handles, connect & reconnect**, with a pluggable connection validator
  (`flow_set_connection_validator`, the analog of `isValidConnection`).
- **Edge labels** and straight / orthogonal (step) routing.
- **Selection**: single, multi, and marquee.
- **Drag** with auto-pan; per-element gates (draggable / selectable / deletable);
  an SE-corner **resizer** with explicit, persisted node sizes.
- **Viewport**: pan (arrows / drag / scroll), pointer-centered **zoom** with limits,
  **fit-view**, and level-of-detail collapsing.
- **Minimap, controls, status bar, and node/edge toolbars.**
- **Subflows / groups** (`flow_group` / `flow_ungroup` / `flow_set_parent`).
- **Undo / redo** via a coalescing command journal.
- **Auto-layout**: force-directed and layered (Sugiyama-style).
- **JSON save / load** (versioned format) and **theming** (DEFAULT / LIGHT / DARK,
  theme tokens, dot/line/cross backgrounds).

Honest terminal limits: one glyph = one cell (no sub-cell magnification — zoom changes
spacing + level-of-detail), edges render as box-drawing corners (no bezier curves), and
labels are measured one codepoint per cell (wide/CJK/emoji glyphs will misalign).

## Install

`flow` is a single header. Copy `flow.h` into your project — that's the whole install.
(A tagged release also attaches `flow.h` as a downloadable asset.)

## Quickstart

In **exactly one** `.c` file, define `FLOW_IMPLEMENTATION` before the include to compile the
implementation. Every other file includes `flow.h` **without** the macro (declarations only).

```c
#define FLOW_IMPLEMENTATION
#include "flow.h"

int main(void) {
  flow_t *f = flow_new(80, 24);
  flow_register_defaults(f);
  int a = flow_add_node(f, "default", (flow_pt){4, 3},   (void*)"node A");
  int b = flow_add_node(f, "default", (flow_pt){34, 12}, (void*)"node B");
  flow_add_edge(f, a, b, "", "");
  flow_run(f);            /* arrow keys pan, q quits */
  flow_free(f);
}
```

Build (requires **C99 or later**; `-lm` is required on Linux):

```sh
cc app.c -std=c11 -lm -o app
```

## Requirements & platform

- **C99 or later**, `-lm`.
- The **interactive run-loop** (`flow_run` / `flow_feed` / `flow_present` / `flow_term_*`)
  needs a **POSIX terminal** (Linux / macOS).
- The **model, geometry, rendering, routing, layout, and JSON layers are portable C** and
  can be embedded in your own event loop with your own I/O (Windows / non-TTY hosts can use
  these without the run-loop). Not yet validated on MSVC.
- **Not thread-safe**: serialize calls per `flow_t`; separate instances are independent.

## Developing

`flow.h` is **generated** from `src/` by `tools/amalgamate.sh` — edit `src/`, never `flow.h`.

```sh
make            # regenerate flow.h and build the demos
make test       # run the headless test suite (35 suites, snapshot goldens)
```

See `docs/superpowers/specs/2026-06-02-c-xyflow-flow-design.md` for the full design, and
[`CHANGELOG.md`](CHANGELOG.md) for release history.

## Credits

Concepts and API shape are modeled on [xyflow](https://github.com/xyflow/xyflow) /
React Flow (MIT, webkid GmbH) — `flow` is an independent C reimplementation and includes
no xyflow code. The terminal raw-mode and escape-sequence approach was inspired by
[tuibox](https://github.com/Cubified/tuibox) (Cubified) and implemented independently from
the standard termios idiom and ANSI/SGR escape sequences.

## License

MIT — see [`LICENSE`](LICENSE).
