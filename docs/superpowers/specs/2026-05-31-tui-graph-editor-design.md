# TUI Graph Editor — Design Spec
_2026-05-31_

## Overview

A terminal-based interactive graph editor built on top of [tuibox](https://github.com/Cubified/tuibox), inspired by xyflow. Primary use case: network topology visualisation — nodes represent devices (servers, routers, switches), edges represent connections, clicking a node reveals device info.

Fully interactive: drag nodes, draw connections between nodes by dragging port handles, add/remove nodes, pan an infinite canvas, load/save topologies as JSON.

---

## Architecture

### Coordinate System

Two spaces:

- **World space** — where nodes live. Integer `(col, row)` units, origin `(0,0)`, unbounded.
- **Screen space** — what gets drawn. `screen = world − viewport`. Viewport is `(pan_x, pan_y)` updated on arrow keypresses.

Transform: `screen_x = world_x - pan_x`, `screen_y = world_y - pan_y`.

### Approach

A **single full-screen `ui_box_t`** covers the terminal and renders everything. Graph state lives in our own C structs; tuibox handles terminal setup (raw mode, alternate screen, mouse escape sequences) and feeds mouse/keyboard events into our handlers.

This avoids tuibox's flat box list having to track a dynamic graph and sidesteps the O(n) position-update problem that would arise from a box-per-node approach.

### Core Data Structures

```c
typedef struct {
  int id;
  int wx, wy;        // world-space position (col, row)
  int w, h;          // size in terminal characters
  char label[64];
  char type[32];     // "router", "switch", "server", etc.
  char info[512];    // detail text shown in info panel
} node_t;

typedef struct {
  int from_id, to_id;
} edge_t;

typedef struct {
  node_t *nodes;
  int node_count, node_cap;
  edge_t *edges;
  int edge_count, edge_cap;

  int pan_x, pan_y;

  int selected_node;   // -1 if none
  int info_open;

  // drag state
  int dragging;        // node id being dragged, -1 if not dragging
  int drag_ox, drag_oy;

  // connection state
  int connecting;      // source node id if mid-connection, -1 otherwise
  int connect_port;    // 0=top, 1=right, 2=bottom, 3=left
} graph_t;
```

The canvas `ui_box_t` holds `graph_t *` as `data1`. Its `draw`, `onclick`, and `onhover` callbacks are the only tuibox hooks used for graph logic.

---

## Rendering

The canvas `draw` function runs top-to-bottom each frame, writing ANSI escape sequences directly.

**Render order (back to front):**
1. Edges
2. Node bodies
3. Port handles (only on hovered node)
4. Info panel overlay (screen-space, not world-space)

### Nodes

Each node renders as a unicode box:

```
┌─────────────────┐
│ 🖥  web-server-1 │
│ server          │
└─────────────────┘
```

Selected nodes get a bold/bright border. Only nodes whose screen-space rect intersects `[0, ws_col) × [0, ws_row)` are drawn (viewport culling).

Node size: width = `max(label length, type length) + 4`, height = 4.

### Edges

Orthogonal routing: leave from the center of the source port, travel horizontally then vertically (or V-then-H) to the target port. Bend point at the midpoint between the two nodes; axis chosen by whichever dimension has greater distance.

Characters used: `─` `│` `┌` `┐` `└` `┘` `┬` `┴` `├` `┤` `┼` for corners and T/cross junctions.

Edges are drawn in world space then clipped to the viewport before output.

### Port Handles

Four `◉` markers at top/right/bottom/left center of a node's border, visible only when that node is hovered. Clicking and dragging one initiates a connection; a live preview line (dashed `╌` `╎`) follows the cursor until released.

### Info Panel

Fixed bottom-right overlay (screen-space). Shows `node.label`, `node.type`, and `node.info` as formatted text. Appears on node select, dismissed by `Esc` or clicking outside.

---

## Interaction Model

| Action | Input |
|---|---|
| Pan canvas | Arrow keys (2 cells per press) |
| Select node | Left-click node body |
| Drag node | Click-hold node body, move mouse |
| Connect nodes | Click-drag from `◉` port handle to another node |
| Add node | `n` — places node at viewport center, prompts for label |
| Delete node | `Delete` or `x` while node selected (removes node + its edges) |
| Delete edge | `Delete` while hovering an edge |
| Open info panel | Click node (auto-opens) |
| Close info panel | `Esc` or click elsewhere |
| Save | `s` — writes to loaded file, or prompts for filename |
| Quit | `q` |

**Drag logic:** `onclick` stores `drag_ox = x − node.wx`, `drag_oy = y − node.wy`. `onhover` while mouse is down sets `node.wx = screen_x + pan_x − drag_ox`. Edges follow automatically since they are computed from node positions each frame.

**Connection logic:** dragging from a port handle sets `graph.connecting = node_id` and `graph.connect_port`. A preview line is drawn each hover event. Releasing on another node's body creates an edge; releasing on empty space cancels.

---

## File Structure

```
tui-graph-editor/
├── tuibox.h          — upstream, unchanged
├── main.c            — entry point, tuibox init, argument parsing, event loop
├── graph.h / graph.c — graph_t, node/edge CRUD, coordinate helpers
├── render.c          — canvas draw function, edge router, port handle drawing
├── interact.c        — onclick/onhover handlers, drag, connect, pan, keyboard
├── json.c            — load/save topology (hand-rolled, no deps)
├── Makefile
└── docs/
    └── superpowers/specs/
        └── 2026-05-31-tui-graph-editor-design.md
```

## Build

```
gcc -o tui-graph main.c graph.c render.c interact.c json.c -lm
```

No external dependencies beyond tuibox (header-only).

## Topology JSON Format

```json
{
  "nodes": [
    {
      "id": 1,
      "x": 10,
      "y": 5,
      "label": "web-server-1",
      "type": "server",
      "info": "IP: 192.168.1.10\nStatus: up\nOS: Ubuntu 22.04"
    }
  ],
  "edges": [
    { "from": 1, "to": 2 }
  ]
}
```

If no file is given as a CLI argument, the editor starts with an empty canvas.

---

## Out of Scope

- Zoom (terminal character grid makes sub-cell scaling impractical)
- Multiple edge styles (orthogonal only)
- Undo/redo
- Node grouping/subgraphs
