# Theming flow

How to recolor flow: color modes, the theme token struct, per-token overrides, and
the background grid. This is a conceptual guide — for exact per-function signatures
see [docs/API.md](API.md) ("Theming & color modes" and "Chrome").

Everything here is **transient view-state**: the theme and background are never saved
by `flow_save` and never journaled for undo. They are pure render config, re-applied
every `flow_render`.

> Colors are terminal SGR / xterm-256 indices, not RGB. `flow_diff_emit` only ever
> emits the `;38;5;<fg>;48;5;<bg>` color path (`src/flow_cell.h`), so what an index
> looks like is entirely up to the user's terminal palette. Index 7 is "white-ish",
> index 0 is "black-ish", but the actual pixels depend on the theme the user runs.
> Treat the values below as palette slots, not literal colors.

## Color modes

flow ships three engine-chrome presets, selected by the `flow_color_mode` enum
(`src/flow_cell.h`):

| Mode | Meaning |
| --- | --- |
| `FLOW_COLOR_DEFAULT` | The legacy preset: fg 7 / bg 0 / grid 8. Byte-identical to pre-theme flow. |
| `FLOW_COLOR_LIGHT` | Dark fg on a light canvas (bg 15). |
| `FLOW_COLOR_DARK` | Brighter fg (15) on a dark canvas (bg 0). |

Switch modes with the public setter (`src/flow_model.h`):

```c
void            flow_set_color_mode(flow_t *f, flow_color_mode mode);
flow_color_mode flow_color_mode_get(flow_t *f);   /* last mode set */
```

`flow_set_color_mode` **re-seeds the entire `f->theme` token struct** from a fixed
preset table (`src/flow_model.h:1175` — `f->theme = presets[mode]`). It overwrites
every token, not just fg/bg. An out-of-range mode is clamped to `FLOW_COLOR_DEFAULT`.
`flow_new` seeds `FLOW_COLOR_DEFAULT` at construction (calloc-zero would be a
black-on-black `{fg:0,bg:0}` theme), so the default is the legacy 7/0/8 look until you
change it. `flow_color_mode_get` returns the last mode set (calloc-zero default
`FLOW_COLOR_DEFAULT`).

The preset table is the source of truth for every mode's token values
(`src/flow_model.h`):

| Token | DEFAULT | LIGHT | DARK |
| --- | --- | --- | --- |
| `fg` | 7 | 0 | 15 |
| `bg` | 0 | 15 | 0 |
| `grid_fg` | 8 | 7 | 8 |
| `handle` | 7 | 0 | 15 |
| `handle_valid` | 2 | 2 | 2 |
| `handle_invalid` | 1 | 1 | 1 |
| `accent` | 7 | 0 | 15 |
| `edge_fg` | 7 | 0 | 15 |
| `widget_fg` | 7 | 0 | 15 |
| `widget_bg` | 0 | 15 | 0 |

## The `flow_theme` token struct

The theme is a flat, fixed token set — one `uint8_t` xterm-256 index per role. There is
no per-element override table (intentionally). Every field, with its role, from
`src/flow_cell.h`:

| Field | Role |
| --- | --- |
| `fg` | Canvas foreground: clear color, node frames/labels, edges, handle markers, focus ring, minimap, status bar. |
| `bg` | Canvas background: the clear color and the background of every painted cell. |
| `grid_fg` | Background-grid dot/line foreground (the dim grid color; DEFAULT 8 == `FLOW_BG_GRID_FG`). |
| `handle` | Handle marker color (the connection dots; `== fg` in every preset for now). |
| `handle_valid` | Handle color when an in-flight connection would be accepted (green, index 2). |
| `handle_invalid` | Handle color when an in-flight connection would be rejected (red, index 1). |
| `accent` | Selection/bold accent. **Defined but currently unwired** — not read by any render call-site yet (reserved). |
| `edge_fg` | Edge path + edge-label foreground (`== fg` in every preset for now). |
| `widget_fg` | Chrome-widget foreground (controls bar, node/edge toolbars). |
| `widget_bg` | Chrome-widget background. |

Ten tokens total. The named index constants seeding the DEFAULT preset live in
`src/flow_cell.h`:

```c
#define FLOW_FG 7         /* canvas foreground */
#define FLOW_BG 0         /* canvas background */
#define FLOW_BG_GRID_FG 8 /* grid dim fg — index 8 = bright black */
```

The DEFAULT preset's `fg`/`bg`/`grid_fg` equal these literals exactly, which is why
converting the render call-sites from the raw `FLOW_FG`/`FLOW_BG` constants to
`f->theme.*` was a pure indirection that left every snapshot golden byte-identical.

A few render call-sites still reference the raw `FLOW_FG`/`FLOW_BG` macros directly
(the built-in default-node and group-node render fns in `src/flow_types.h`); those node
bodies do **not** track the theme. Everything else — chrome, edges, grid, handles — reads
`f->theme.*` live.

## Customizing individual tokens

There is **no public setter** for individual tokens — `flow_set_color_mode` and
`flow_color_mode_get` are the only public theming functions. To override one token you
write the struct member directly:

```c
f->theme.grid_fg = 240;   /* a dimmer grey for the grid */
f->theme.edge_fg = 4;     /* blue edges */
```

Two placement rules make this work correctly:

1. **Placement TU.** `struct flow`'s body — including `theme` — is defined only inside
   the `#define FLOW_IMPLEMENTATION` block of `flow.h` (`src/flow_model.h`). In any
   other translation unit `flow_t` is an **incomplete type**, so `f->theme.*` will not
   compile. Per-token overrides must live in the same TU that defines
   `FLOW_IMPLEMENTATION`. (The public setters, being declared functions, work from any
   TU.)
2. **Ordering.** Apply overrides **after** `flow_new` *and* **after** any
   `flow_set_color_mode` call. Both seed the whole struct (`f->theme = presets[mode]`),
   so a token you set before a mode switch is silently overwritten. Set the mode first,
   then patch tokens.

Render call-sites read `f->theme.*` **live** every `flow_render` (see the grid, edge,
handle, minimap, and widget draws in `src/flow_render.h`), so a token you change takes
effect on the next frame with no extra call — and the built-in chrome (controls bar,
toolbars via `widget_fg`/`widget_bg`) tracks the active mode for free.

## Background grid

The world-aligned background grid is separate from the theme. It is **off by default**
and toggled with one setter (`src/flow_model.h`):

```c
void flow_set_background(flow_t *f, flow_bg_variant variant, int gap);
```

`gap` is the spacing between grid marks in world cells; it is clamped to `>= 1` (no
modulo-by-zero). The `flow_bg_variant` enum (`src/flow_cell.h` / `src/flow_model.h`):

| Variant | Renders |
| --- | --- |
| `FLOW_BG_NONE` | Nothing — disables the grid (the default). |
| `FLOW_BG_DOTS` | A middle dot `·` (U+00B7) at each `gap`×`gap` lattice point. |
| `FLOW_BG_CROSS` | A plus `+` (U+002B) at each lattice point. |
| `FLOW_BG_LINES` | Full ruled lines: `│` verticals on `gap` columns, `─` horizontals on `gap` rows, `┼` at intersections. |

The grid is **world-aligned**: each screen cell is projected back to world space, so the
marks stay pinned to world coordinates and slide correctly under pan
(`src/flow_render.h` — `flow__background`). Grid marks paint in `f->theme.grid_fg` on
`f->theme.bg`, so the grid recolors with the mode (e.g. LIGHT uses grid index 7, the
dark presets use 8).

The grid draws beneath edges and nodes. Like xyflow's `<Background>`, it is decorative;
unlike a GPU canvas there is no sub-cell density — one mark per `gap` cells, and zoom
changes spacing rather than scaling the glyph.

## Worked example

Dark mode with a dotted background, using only the public setters (compiles in any TU):

```c
flow_t *f = flow_new(cols, rows);
flow_register_defaults(f);

flow_set_color_mode(f, FLOW_COLOR_DARK);   /* re-seeds the whole theme to the DARK preset */
flow_set_background(f, FLOW_BG_DOTS, 4);    /* dots every 4 world cells */
```

To additionally tweak one token, do it **after** the mode switch, in the
`FLOW_IMPLEMENTATION` TU:

```c
/* same TU that does: #define FLOW_IMPLEMENTATION  #include "flow.h" */
flow_set_color_mode(f, FLOW_COLOR_DARK);
f->theme.grid_fg = 240;   /* override AFTER the mode seed, or it gets wiped */
```

A minimal run from a shell, using the bundled demos (the interactive run-loop is
POSIX-only):

```sh
make demos              # builds demos/hello_flow, demos/topo, demos/flowchart
./demos/flowchart
```

The on-screen result depends on the user's terminal palette — flow only emits the
xterm-256 indices above; the terminal maps them to actual colors.

## See also

- [docs/API.md](API.md) — per-function signatures for `flow_set_color_mode`,
  `flow_color_mode_get`, `flow_set_background`, and the rest of the chrome setters.
- [docs/ARCHITECTURE.md](ARCHITECTURE.md) — module layout and the render pipeline.
- [README.md](../README.md) — overview and quick start.
- [CONTRIBUTING.md](../CONTRIBUTING.md) — build/test conventions (`make`,
  `make test`, the amalgamation rule).
