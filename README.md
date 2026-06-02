# flow

`flow` is a single-header C library for building interactive node-graph editors in the
terminal — a C/terminal analog of [xyflow](https://github.com/xyflow/xyflow) (React Flow).
Dependency-free (pure ANSI escapes, `-lm` only).

## Status

Increment 1 (foundation): nodes, edges, custom node/edge types, an orthogonal edge router,
a damage-diffed cell compositor, arrow-key panning, and **mouse/trackpad interaction**
(drag a node to move it, drag the canvas to pan, scroll/wheel to pan). See
`docs/superpowers/specs/2026-06-02-c-xyflow-flow-design.md` for the full design and roadmap
(zoom, selection, drag, connect, subflows, undo/redo, auto-layout, minimap).

## Build & run

    make            # regenerate flow.h and build demos
    make test       # run the headless test suite
    ./demos/hello_flow

## Use

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

`flow.h` is generated from `src/` by `tools/amalgamate.sh`; edit `src/`, not `flow.h`.

## Credits

Terminal raw-mode and escape-sequence technique adapted from
[tuibox](https://github.com/Cubified/tuibox) (Cubified, MIT).
