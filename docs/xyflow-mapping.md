# flow — migration guide for xyflow (React Flow) developers

`flow` is an independent, from-scratch C reimplementation of the *concepts* behind
[xyflow](https://github.com/xyflow/xyflow) (React Flow) — nodes, edges, handles, connections,
selection, viewport, groups, undo, layout, persistence — rebuilt for the terminal. xyflow is
JS/TS rendering to SVG/DOM in a browser; `flow` is a single C99 header rendering box-drawing
glyphs to a `cols`×`rows` cell grid. No xyflow code is shared or ported (it is a different
language and runtime) — only the API *shape* is modeled on it, so the mental model transfers
even though every call is C. This guide maps each xyflow concept to its `flow` equivalent and
is honest about where the terminal makes `flow` render or behave differently. For per-function
signatures see [`docs/API.md`](API.md); for the theming row see [`docs/theming.md`](theming.md);
for a copy-paste start see the [README Quickstart](../README.md#quickstart).

Status legend: **Done** = full equivalent; **Partial** = the feature exists but the terminal
changes how it renders or behaves; **By-design-different** = a deliberate model difference, not
a gap; **Not-supported** = no equivalent today.

## Concept → flow API → Status

| xyflow (React Flow) concept | flow API | Status |
|---|---|---|
| `<ReactFlow>` mount + run loop | `flow_new` + `flow_run` | Done |
| `nodeTypes` / `edgeTypes` registries | `flow_register_node_type` / `flow_register_edge_type` (fn-pointer vtables) | Done |
| Custom node component (render + measure) | `flow_node_type` vtable: `measure` + `render` fn pointers (`src/flow_types.h:32`) | Done |
| `nodes` / `edges` state | `flow_add_node` / `flow_add_edge` (+ `flow_remove_node` / `flow_remove_edge`) | Done |
| Handles + `onConnect` | gesture `flow_begin_connection` / `flow_update_connection` / `flow_end_connection`; the `onConnect` analog is the `on_connect` callback in `flow_callbacks` | Done |
| `isValidConnection` | `flow_set_connection_validator` (`flow_connection_validator` fn) | Done |
| Edge reconnect | `flow_reconnect_edge` (`which`: 0=source, 1=target) | Done |
| Edge labels | `flow_set_edge_label` (engine-owned, strdup'd) | Done |
| Selection: single / multi | `flow_select_node` / `flow_select_edge` (`additive` flag), `flow_selected_nodes` | Done |
| Selection: marquee (box) | `flow_select_in_rect` + `flow_set_marquee_mode` (`FLOW_SELECT_FULL` / `FLOW_SELECT_PARTIAL`) | Done |
| Node drag + auto-pan | drag via `flow_handle_mouse` / `flow_feed`; band tuned by `flow_set_autopan` | Done |
| `fitView` | `flow_fit_view` (+ `flow_fit_bounds`) | Done |
| `zoomIn` / `zoomOut` / `zoomTo` | `flow_zoom_in` / `flow_zoom_out` / `flow_set_zoom` (discrete `FLOW_ZOOM_STEP`, pointer-centered, clamped to `[zmin, zmax]`) | Done |
| Pan / `setViewport` | `flow_pan` / `flow_set_center` / `flow_view_get` | Done |
| `<MiniMap>` | `flow_set_minimap` | Done |
| `<Controls>` | `flow_set_controls` (`[+][-][fit][lock]` bar) | Done |
| `<Background>` (dots / lines / cross) | `flow_set_background` (`flow_bg_variant`: `FLOW_BG_NONE` / `DOTS` / `LINES` / `CROSS`) | Done |
| Subflows / groups | `flow_group` / `flow_ungroup` / `flow_set_parent` | Done |
| Undo / redo | `flow_undo` / `flow_redo` / `flow_can_undo` / `flow_can_redo` | Done |
| Auto-layout (`dagre`/`elk`-style) | `flow_layout_force` (force-directed) / `flow_layout_layered` (Sugiyama-style) | Done |
| `toObject` / JSON persistence | `flow_save` / `flow_load` (versioned JSON, `"version":1`) | Done |
| Theming / color modes | `flow_set_color_mode` (`FLOW_COLOR_DEFAULT` / `LIGHT` / `DARK`) + theme tokens — see [`docs/theming.md`](theming.md) | Partial — palette-index colors (SGR / xterm-256), not arbitrary RGB; appearance is up to the terminal's palette |
| Edge types: straight / step | `flow_straight_edge_type` / `flow_default_edge_type` (orthogonal step) | Done |
| Edge types: bezier / smoothstep | — | Partial — routed as box-drawing corners (no curves) |
| `snapToGrid` / `snapGrid` | `flow_set_helper_lines` (neighbor-edge alignment guides only) | Partial — alignment guides, not grid quantization |
| Sub-cell zoom / magnification | — | By-design-different — one glyph = one cell; zoom changes spacing + level-of-detail |
| Smooth / eased zoom & pan tweens | — | Not-supported |
| Wide-char / CJK / emoji labels | codepoint-per-cell measurement | Partial — measured one codepoint per cell; wide glyphs misalign |
| Windows / non-TTY interactive run | `flow_run` is POSIX-only; portable core embeddable | By-design-different — interactive path POSIX-only; model/render/layout/JSON are pure C |

## Divergences (terminal honesty)

These are the places a React Flow developer's expectations will not carry over verbatim. Each
is a consequence of rendering to a fixed character grid, not an oversight.

### Theming → palette-index colors, not arbitrary RGB

xyflow themes with CSS — arbitrary RGB/HSL colors. `flow`'s theme tokens are all `uint8_t`
(`flow_theme` in `src/flow_cell.h`): each is a terminal SGR / xterm-256 *palette index*, because
`flow_diff_emit` only ever emits the `;38;5;<fg>;48;5;<bg>` color path. So a token value is a
palette slot, not a literal color — index 7 is "white-ish", index 0 is "black-ish", but the
actual pixels are whatever the user's terminal palette maps that slot to. `flow_set_color_mode`
switches between three engine-chrome presets (`FLOW_COLOR_DEFAULT` / `LIGHT` / `DARK`); there is
no per-element override table. See [`docs/theming.md`](theming.md) for the full token list and
the same caveat.

### Bezier / smoothstep edges → box-drawing corners

xyflow draws edges as SVG bezier or smoothstep paths with sub-pixel curvature. `flow` has no
sub-cell geometry, so both built-in routers emit box-drawing connectivity glyphs:
`flow_default_edge_type` routes an orthogonal two-corner step path (`flow_route_orthogonal`)
and `flow_straight_edge_type` routes a direct integer-DDA line (`flow_route_straight`, with
`─ │ ╲ ╱`). There is no curved-edge router — a "bezier" edge in xyflow becomes hard-cornered
box-drawing here. Custom routers are still possible via `flow_register_edge_type`, but they too
push discrete cells.

### `snapToGrid` → alignment guides, not grid quantization

React Flow's `snapToGrid`/`snapGrid` quantizes every node position to a fixed grid step. `flow`
has **no positional quantization**. The nearest equivalent is `flow_set_helper_lines`, which —
during a single-node drag — shows alignment helper lines and snaps the dragged node to a
*neighbor node's* edge/center when they line up. Guides clear on release. It aligns against
other nodes, not against an absolute grid; with `flow_set_helper_lines(f, 0)` (the default) the
drag path is byte-for-byte unsnapped.

### Sub-cell zoom → one glyph = one cell

xyflow zoom is a continuous CSS transform that magnifies the SVG. `flow` cannot magnify a
glyph: one glyph always occupies exactly one terminal cell. Zoom (`flow_set_zoom` /
`flow_zoom_in` / `flow_zoom_out`) is **discrete** — it multiplies/divides by `FLOW_ZOOM_STEP`,
pointer-centered, clamped to `[zmin, zmax]` — and what changes is the *spacing* between nodes
plus the level-of-detail tier (`flow_lod_for_zoom`: `0` full, `1` collapsed). Node bodies do not
grow or shrink. This is by design, not a limit to be lifted.

### Smooth / eased tweens → not implemented

There are no eased zoom/pan animations. Viewport changes apply in one step. The only animation
in the engine is the deterministic, tick-based marching-ants on edges marked with
`flow_set_edge_animated`, driven by the integer clock (`flow_tick` / `flow_ticks`) — never
wall-clock. If you relied on xyflow's animated viewport transitions, budget for instantaneous
jumps instead.

### Wide-char / CJK / emoji labels → mis-measured

Text measurement and layout assume **one Unicode code point = one cell**
(`flow_node_type.measure` and `flow_text` advance one cell per code point). UTF-8 is decoded
correctly (`flow_utf8_decode`), but wide East-Asian glyphs, CJK, and emoji — which occupy two
terminal columns — are measured as one, so node widths and edge anchoring will misalign for
such labels. ASCII and single-width Unicode are exact.

### Windows / non-TTY interactive → POSIX-only run loop

The interactive run loop owns a POSIX terminal: `flow_run`, `flow_present`, and the
`flow_term_*` functions use termios, `ioctl`, and signals, so the all-in-one interactive path is
Linux/macOS only. Everything else — the model, geometry, cell-buffer rendering, edge routing,
auto-layout, and JSON — is portable C. To run on Windows or a non-TTY host, skip `flow_run` and
drive the model yourself: feed input bytes with `flow_feed` and pull frames with
`flow_render_diff` (or read the raw `flow_cell` grid via `flow_render`), writing to whatever fd
you own. See the README's "Embedding in your own event loop" section. Note that `flow_load` is
the only entry that parses untrusted bytes — treat saved-graph input accordingly
([`SECURITY.md`](../SECURITY.md)).

## Hello graph: side by side

The minimal two-node, one-edge graph. xyflow (React Flow), JSX:

```jsx
const nodes = [
  { id: 'a', position: { x: 4,  y: 3  }, data: { label: 'node A' } },
  { id: 'b', position: { x: 34, y: 12 }, data: { label: 'node B' } },
];
const edges = [{ id: 'e', source: 'a', target: 'b' }];
return <ReactFlow nodes={nodes} edges={edges} />;
```

The same graph in `flow` C (mirrors the [README Quickstart](../README.md#quickstart) exactly):

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

Build with C99 or later, linking the math library:

```sh
cc app.c -std=c11 -lm -o app
```

Key shape differences from the JSX: nodes are addressed by an integer **id** returned from
`flow_add_node` (not a caller-chosen string), the node `data` pointer is **borrowed** (flow never
frees it), edges name a source/target **id** plus handle strings (`""` = the type's default
handle), and `flow_run` is a *blocking* loop that owns the terminal — there is no React render
tree or reconciler. For a host that already has an event loop, replace `flow_run` with the
embeddable `flow_feed` + `flow_render_diff` pair (see [`docs/API.md`](API.md)).

## See also

- [`docs/API.md`](API.md) — per-function reference for every symbol named above.
- [`docs/theming.md`](theming.md) — color modes and theme tokens.
- [`README.md`](../README.md) — features, Quickstart, and the embedding seam.
- [`docs/ARCHITECTURE.md`](ARCHITECTURE.md) — portable-core vs. POSIX-terminal layering.
