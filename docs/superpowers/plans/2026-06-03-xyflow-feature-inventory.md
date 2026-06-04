# xyflow feature inventory — full surface vs. the `flow` C port

**Date:** 2026-06-03
**Branch audited:** `increment-3` (commits through `74b6045` undo-redo; handoff `a3d446d`)

**Method.** Swept reactflow.dev page-by-page — API reference (`<ReactFlow>` props, types, utils, hooks, components, the ReactFlowInstance), the Learn guides (concepts, customization, layouting, sub-flows, accessibility, performance, state-management, computing-flows, SSR, testing, devtools, whiteboard, multiplayer), the examples gallery (nodes / edges / interaction / layout / grouping / styling / whiteboard / misc), the shadcn UI-component registry, the Pro examples index, and the what's-new changelog — and audited each item against the full page index so nothing was sampled-by-omission. Every feature was then cross-referenced against the design spec (`docs/superpowers/specs/2026-06-02-c-xyflow-flow-design.md`, §§1–16), the increment-3 workpackages and session handoff, and `src/` on branch `increment-3`. Where the same feature surfaced from multiple angles (a prop page, a type page, an example, a hook), the entries were merged into one row carrying the richest description, the union of sources, and the best-evidenced status. Cited evidence (file:line, commit, or spec §) was spot-verified directly against the tree.

**Scope.** "xyflow" here means React Flow 12.x as documented on reactflow.dev — the richest documented surface of the xyflow org. Svelte Flow and the shared `@xyflow/system` core were treated as feature-equivalent (Svelte Flow is a thinner wrapper over the same system package) and were not independently swept.

## Legend

- `[x]` **implemented** — present and working in `src/` on `increment-3` (often with a documented delta vs. the React prop).
- `[~]` **spec'd** — defined in the design spec / workpackages, deliberately re-targeted (e.g. the change-descriptor contract → undo journal), or planned-but-unlanded (auto-layout, extent clamp).
- `[ ]` **candidate** — not built and not spec'd; a real future option.
- `[-]` **n/a-terminal** — no meaningful terminal analog (DOM/CSS/SVG/ARIA/touch/animation artifacts).

**Terminal fit:** `yes` (clean port) / `partial` (degraded or coarse port) / `no` (no analog).

### Summary — verified counts

| Layer | Count |
|---|---|
| Raw cross-referenced entries (companion artifact `2026-06-03-xyflow-feature-inventory.raw.json`) | 638 |
| — raw status: implemented / spec'd / candidate / n/a-terminal | 220 / 26 / 326 / 66 |
| Feature rows in this document (after semantic merge) | 280 |
| — by primary status `[x]` / `[~]` / `[ ]` / `[-]` | 110 / 13 / 124 / 33 |
| — rows carrying a split status (e.g. `[x]/[ ]` for a half-landed family) | 20 |
| Bundled drop-groups in "Explicitly dropped" (each bullet a whole family) | 10 |

Terminal fit across the 638 raw entries: 368 yes / 219 partial / 51 no.

A row here often merges several raw entries — the largest merge classes were the controlled change-pipeline family (`onNodesChange`/`useNodesState`/`applyNodeChanges`/NodeChange union all collapse to the spec §1 store decision), the connect/reconnect lifecycle events seen from props + types + examples, the fitView/FitViewOptions cluster, the snapToGrid/snapGrid pair, and the duplicated whats-new restatements of auto-pan, ariaRole, domAttributes, and z-index. Raw status counts are higher than row counts for the same reason (e.g. 220 raw "implemented" entries collapse into 110 `[x]` rows), and the raw artifact retains cross-slice duplicates and two statuses later corrected against source (below).

**Coverage audit.** A 4-agent pass verified each of the 638 raw entries is represented in this document (own row, merged row, or drop-group). It found and fixed: 2 features silently dropped in synthesis (`Edge.hidden`, start-marquee-over-a-node — restored as rows), and 2 raw entries whose "implemented" status the document had correctly downgraded — confirmed against source: MiniMap custom node renderer (config is `{enabled,w,h,corner}` only, flow_model.h:221) and controlled viewport / onViewportChange (no viewport callback exists in src).

---

## Core model: nodes

| Feature | xyflow source | Terminal fit | Status | Notes (analog / evidence) |
|---|---|---|---|---|
| Node store (controlled `nodes` + uncontrolled `defaultNodes`) | `<ReactFlow>` props; learn concepts | yes | [x] | One engine-owned store mutated directly; controlled/uncontrolled distinction collapsed by design. `flow_add_node`, `flow_node` store (flow_model.h); spec §1/§4. |
| `defaultNodes` seed prop (bulk initial array) | `<ReactFlow>` prop | yes | [ ] | No bulk-seed prop; caller loops `flow_add_node` after `flow_new` (topo.c, hello_flow.c). |
| Node.id (string) | Node type | yes | [x] | Monotonic int id, not string — deliberate (spec §4). `flow_get_node(f,id)`. |
| Node.position (parent-relative XYPosition) | Node type | yes | [x] | Integer cell `flow_pt pos`, parent-relative; abs via `flow_node_abs`. |
| Node.data (opaque payload) | Node type | yes | [x] | `void *data`; default type reads it as a C-string label (flow_types.h). |
| Node.type + built-ins (default/input/output/group) | Node type; concepts | yes | [x] | `type[32]` → vtable. Ships `default`+`group` only; input/output absent by design (spec §5). |
| Node.sourcePosition / targetPosition | Node type | yes | [x] | Handle side declared per-type via `flow_handle.pos`, not per-node; default out=RIGHT in=LEFT. |
| Node.selected | Node type | yes | [x] | `FLOW_SELECTED` flag; selected draws bold + last in layer. |
| Node.dragging | Node type | yes | [~] | `FLOW_DRAGGING` named in spec §4 but never set; live drag tracks `f->drag_node`. Flag only cleared on undo-restore. |
| Node.width/height (read-only, measured) + `measured` | Node type + Notes; multiplayer | partial | [x] | Inverted: cached `w,h` from `measure()` vtable, never DOM-measured (spec §4). One computed-size field collapses the whole measured/initial*/measured machinery. |
| Node.initialWidth/initialHeight (SSR pre-measure) | Node type | partial | [-] | Synchronous measure at add-time; no pre-measure window. |
| Node.parentId (sub-flows) | Node type | yes | [x] | `int parent`; depth-aware z-order; reparent. Commit 438ea0d; test_groups.c. |
| Node.extent (`'parent'` / CoordinateExtent / null) | Node type; sub-flows example | yes | [~] | Spec §9 names child-extent clamp; groups landed WITHOUT it. `flow_move_node` applies delta unbounded. |
| Node.expandParent | Node type | yes | [ ] | Grow parent rect on child drag-to-edge; group is sized once at creation. |
| Node.origin (NodeOrigin) | Node type + NodeOrigin | yes | [ ] | `pos` always top-left; no fractional anchor. `flow_add_node_center` is a placement helper, not a general origin. |
| Node.zIndex (user-set) | Node type | yes | [ ] | Draw order computed from depth+selected; no user zIndex term. |
| Node.draggable / selectable / deletable / connectable / focusable (per-node gates) | Node type; NodeProps | yes/partial | [ ] | No per-node interaction gates; all unconditional. Connectability is per-TYPE (handle_count). |
| Node.dragHandle (sub-region drag) | Node type + example | partial | [ ] | Any node-body press arms a drag; no sub-region restriction. |
| Node.resizing (transient) | Node type | yes | [ ] | No resize interaction exists (size is content-driven). |
| Node.hidden (visibility) | Node type + hidden example | yes | [ ] | No visibility flag; render/hit-test always include the node. Basis for expand/collapse. |
| Node.style / className | Node type | partial | [ ] | No per-node style/theme field; appearance fixed inside the type's `render()`. |
| Node.ariaLabel | Node type | partial | [ ] | No a11y/announce path; a status-line analog is feasible. |
| Node.ariaRole (default "group") | Node type; whats-new 12.7.0 | no | [-] | No DOM/AT tree in a TTY. |
| Node.domAttributes | Node type; whats-new 12.7.0 | no | [-] | No DOM element to attach attributes to. |
| NodeProps full prop set | NodeProps type | yes | [x] | `render(const flow_node*, flow_surface*, flow_render_ctx{zoom,flags,lod})`. Engine places the surface. Delta: ctx omits zIndex/selectable/deletable/draggable/dragHandle/isConnectable and split positionAbsoluteX/Y. |
| NodeProps.positionAbsoluteX/Y | NodeProps | yes | [x] | `flow_node_abs` applied by engine; renderer draws at local (0,0) (spec §5). |
| NodeProps.isConnectable (in-renderer) | NodeProps | yes | [ ] | Not in render ctx; connectability is type-level handle_count. |
| Custom node (auto-wrapped) + typed via generic | custom-nodes guide; NodeProps usage | yes/partial | [x] | Engine supplies the interactive shell; type provides measure/render. Per-type data-struct convention (topo `device`); no compile-time generic. |
| Update node data/properties (setNodes immutable map) | update-node example | yes | [x] | Direct in-place mutation via `flow_get_node`/`flow_move_node`/re-measure; immutability dance vanishes. (visibility not supported.) |
| Node intersection detection (`getIntersectingNodes`/`isNodeIntersecting`) | intersections example; instance | yes | [ ]/[x] | `isNodeIntersecting` predicate is `flow_rect_intersects` + `flow_node_rect_abs` (flow_geom.h:30); the multi-node QUERY API returning a list is unbuilt. |
| Shape nodes (circle/diamond/hexagon) | shapes Pro example | partial | [ ] | Only titled box + group ship; shapes buildable as custom types but none registered. |
| Rotatable node (CSS rotate) | rotatable-node example | no | [-] | Cells are axis-aligned; free rotation infeasible. |
| Node position animation (lerp tween) | node-position-animation Pro | partial | [-] | Animation smoothness cut (spec §1/§16); `flow_move_node` is instant. |
| Delete-middle-node graph repair | delete-middle-node example | yes | [ ] | `on_nodes_delete` fires, but no getIncomers/getOutgoers/getConnectedEdges helpers to bridge. |
| Custom-node hooks: useNodeId | custom-nodes guide | yes | [x] | `render()` receives `flow_node*`; `n->id` in scope, no context lookup. |
| Custom-node hooks: useNodesData / useNodeConnections / useReactFlow (in-renderer) | custom-nodes guide | yes | [ ] | `flow_get_node` exists, but `render()` has no `flow_t*` so other-node data/connections/instance are unreachable mid-draw. |
| Custom-node hook: useUpdateNodeInternals | custom-nodes guide; rotatable-node | partial | [-] | Anchors recomputed analytically each frame; no internals cache to invalidate (spec §5). |
| nodeTypes registry + referential-stability requirement | NodeTypes type; custom-nodes | yes/no | [x]/[-] | `flow_register_node_type` + `flow_node_type_for`. The React re-instantiation hazard does not exist (registered once). |
| InternalNode.internals (positionAbsolute / z / handleBounds / bounds / rootParentIndex / userNode) | InternalNode type | yes | [x] | Same derivations, recomputed per frame not cached: `flow_node_abs`, `flow__node_order`, `flow_handle_anchor`, `flow_node_rect_abs`. No `userNode` back-pointer (single struct, mutated in place). |

## Core model: edges

| Feature | xyflow source | Terminal fit | Status | Notes (analog / evidence) |
|---|---|---|---|---|
| Edge store (controlled + `defaultEdges`) | `<ReactFlow>` props | yes | [x] | `flow_edge` array; `flow_add_edge`/`flow_remove_edge`. `defaultEdges` bulk-seed prop absent. |
| Edge.id / source / target | Edge type | yes | [x] | Int id + two node-id ints; persisted (flow_json.h). |
| Edge.sourceHandle / targetHandle | Edge type | yes | [x] | `source_handle[16]`/`target_handle[16]`; resolved by name in `flow__edge_handle`. |
| Edge.type + built-ins (default/straight/step/smoothstep/simplebezier) | Edge type; edge-types example; concepts | partial | [x] | `type[16]` → router vtable. Ships `default`(orthogonal, =step) + `straight`; bezier/smoothstep/simplebezier cut (spec §1; routers registered flow_types.h:50-51). |
| Edge union variants + pathOptions (offset/borderRadius/curvature) | Edge `<T>` union; variant pages | yes/partial | [~] | One flat struct + router tag, not a discriminated union. Smoothstep/bezier variants spec-cut. |
| Edge.data (generic payload) | Edge type; animating-edges | yes | [x] | Opaque `void *data`; intentionally NOT persisted; no built-in router reads it. |
| Edge.label | Edge type | partial | [x] | Heap `char*` at midpoint; `flow_set_edge_label`; drawn UTF-8 at `label_anchor`. String only. |
| Edge.labelStyle / labelShowBg / labelBgStyle / labelBgPadding | Edge type | partial | [ ] | No label background or label styling drawn. |
| Edge.labelBgBorderRadius | Edge type | no | [-] | Sub-cell corner rounding has no analog. |
| Edge.markerStart / markerEnd + EdgeMarker config + MarkerType enum | Edge type; markers example; turbo-flow | partial | [x]/[ ] | Router auto-draws a directional target arrowhead glyph (▶◀▲▼) by approach direction. No per-edge marker field, no markerStart, no open/closed distinction, no color/size. |
| Custom SVG markers via `<defs>` | markers example | no | [-] | Vector marker artwork can't render in a cell; nearest analog is a single glyph. |
| Edge.style / className | Edge type | partial/no | [ ]/[-] | Fixed FLOW_FG/FLOW_BG; only selection toggles BOLD. No per-edge style; className is DOM-only. |
| Edge.interactionWidth (invisible hit path) | Edge type; BaseEdge | partial | [x] | `flow_hit_edge(f, screen, tol)` Chebyshev distance test; `tol` is a call-arg (1), not a per-edge field. |
| Edge.zIndex / elevateEdgesOnSelect | Edge type; `<ReactFlow>` props | yes | [ ] | All edges draw in one layer below nodes, array order; no per-edge z or selected-edge elevation. |
| Edge.selected | Edge type | yes | [x] | `FLOW_SELECTED` on `flow_edge.flags`; renders bold; mutually exclusive with node selection. |
| Edge.hidden (visibility flag) | Edge type | yes | [ ] | No edge visibility flag (`grep hidden src/` → no hits); render/hit-test always include the edge. Natural pair to the Node.hidden candidate for expand/collapse + culling. |
| Edge.deletable / selectable / focusable | Edge type | yes | [ ] | No per-edge gates; delete/select unconditional; no edge focus ring. |
| Edge.ariaLabel / ariaRole / domAttributes | Edge type | partial/no | [ ]/[-] | No a11y; ariaRole/domAttributes are DOM-only. |
| Edge.reconnectable (which-end gate) | Edge type; reconnect-edge | partial | [ ] | Both endpoints reconnect; per-edge which-end limiter field absent. |
| edgeTypes registry + custom edge contract (EdgeProps) | EdgeTypes type; custom-edges | yes | [x] | `flow_register_edge_type` + `flow_edge_type_for`; `route()` receives endpoints+facings (no full EdgeProps bag). |
| EdgeProps.sourceX/Y/targetX/Y + sourcePosition/targetPosition + handle ids | EdgeProps fields | yes | [x] | `flow__edge_screen_ends` resolves screen endpoints + side facings; router gets `s, sp, t, tp`. Handle ids resolved internally, not passed. |
| EdgeProps.markerStart/markerEnd (resolved url) / pathOptions passthrough | EdgeProps | partial/yes | [ ] | No marker value or options bag threaded through `route()`. |
| defaultEdgeOptions / DefaultEdgeOptions | `<ReactFlow>` prop; type; edge-intersection | partial/yes | [ ] | No edge-defaults merge; `flow_add_edge` sets fixed zero defaults. Most fields it would default don't exist. |
| defaultMarkerColor | `<ReactFlow>` prop | partial | [ ] | Arrowheads hardcoded, no color param. |
| Path utils: getStraightPath / getSmoothStepPath | utils | partial | [x] | `flow_route_straight` (DDA line ─│╲╱ + arrowhead + midpoint label) and `flow_route_orthogonal` (H-V-H box-drawing, borderRadius:0 step case). No offset/centerX/stepPosition. |
| Path utils: getBezierPath / getSimpleBezierPath | utils | partial | [~] | No curved router; spec §1 cuts bezier ("collapse to box-drawing corners"); workpackages defer. |
| Path utility tuple `[path, labelX, labelY, offsetX, offsetY]` | utils Returns | yes | [x] | `flow_route` out-struct: polyline + `label_anchor`. No offsetX/Y. |
| Custom SVG edge path strings (M/L/Q) | custom-edges | partial | [x] | A custom `route()` pushes any cell sequence via `flow_route_push`; unit is cells (no Q-curve primitive). |
| BaseEdge component + invisible interaction path | BaseEdge component | partial | [x] | Realized as the render edge loop + `flow_hit_edge` distance test, not a reusable component. |
| EdgeText / manual label positioning (labelX/labelY) | EdgeText; edge-labels | partial/yes | [x] | Label drawn inline at `label_anchor` (router midpoint). Left-anchored, not centered with -50% transform. |
| EdgeLabelRenderer (rich HTML labels) + interactive label control | EdgeLabelRenderer; edge-labels | partial/yes | [ ] | Plain-string inline labels only; generic `on_overlay` HUD exists but no edge-label overlay layer or midpoint button. |
| Animated edge (dashdraw / marching-ants) + animated SVG along path | Edge.animated; animating-edges; animated-svg-edge UI | partial | [ ] | No per-edge animated flag or time-cycled dash; static dashed pattern exists only for the connection preview. §1's cut targets sub-cell tweens; in-grid marching glyphs are unaddressed. |
| Animating a node along an edge (offsetPath) | animating-edges example | no | [-] | CSS offset-path / Web Animations API; nearest analog is manual per-frame stepping. |
| Edge intersection (node dragged over edge) | edge-intersection example | partial | [ ] | `flow_hit_edge` does point/path distance, but no node-rect-vs-edge check during drag. |
| Floating edges / simple floating edges (nearest-side endpoints) | floating-edges; simple-floating-edges | partial/yes | [ ] | Endpoints fixed to declared handle side; no dynamic nearest-side / perimeter-intersection helper. |
| Editable edge with draggable control points (Pro) | editable-edge Pro | partial | [ ] | `edge.data` exists; no waypoint storage / control-point drag / freeform connection. |
| Edge routing with libavoid (obstacle-avoiding) (Pro) | edge-routing Pro | partial | [ ] | Orthogonal router is a fixed mid-bend; no A*/grid obstacle avoidance. No external deps (overview goal). |
| EdgeToolbar / ButtonEdge / DataEdge UI components | edge-toolbar; button-edge UI; data-edge UI | partial/yes | [ ] | Generic `on_overlay` exists but no edge-anchored toolbar, midpoint button, or data-key→source-field label binding. |
| Self-connecting (loop-back) edge for source===target | custom-edges example | partial | [ ] | Precondition actively forbidden — `flow_add_edge` rejects self-edges (flow_model.h:544,747; test_connect.c). No self-loop router/glyph. Not a deliberate cut (not in §1 or "Deferred"); genuine future candidate needing a loop glyph beside the node. |
| Bidirectional offset edge (reciprocal edges on separate tracks) | custom-edges example | partial | [ ] | Architecturally blocked: `route()` signature receives only one edge's endpoints, no `flow_t*`/edge list, so it cannot detect a reverse edge. Would require a router-contract change. |
| Edge CSS class hooks | theming reference | no | [-] | DOM-only; only per-state styling is the inline selected→BOLD branch. |

## Handles & connections

| Feature | xyflow source | Terminal fit | Status | Notes (analog / evidence) |
|---|---|---|---|---|
| Handle concept + `<Handle>` component | concepts; component; custom-nodes | partial | [x] | Anchor cell on a node border declared by the type; glyph ◉ (0x25C9) at `flow__handle_screen`; analytic anchor per frame. |
| Handle.id | `<Handle>` prop | yes | [x] | `flow_handle.id[16]`; matched by `flow__handle_named`; stored on edges. |
| Handle.type (source/target) + typeless/BOTH (loose) | `<Handle>` prop; handles guide | yes | [x] | `flow_handle_kind {SOURCE,TARGET,BOTH}`; begin requires SOURCE/BOTH, end picks TARGET/BOTH. |
| Handle.position (4 sides) + multiple handles per node + `along` offset | `<Handle>` prop; handles guide; horizontal example | yes | [x] | `flow_pos` side + `along` integer offset; multiple handles per side (test_connect.c:28-34). |
| Handle.isConnectable / isConnectableStart / isConnectableEnd | `<Handle>` props | yes | [ ] | No boolean gates; eligibility comes solely from `flow_handle_kind`. |
| Handle.isValidConnection (per-handle) + global isValidConnection + validation pattern | `<Handle>`/`<ReactFlow>` props; validation example | yes | [ ] | No custom-predicate hook; only structural reject (self/missing/dup) inside `flow_add_edge`. |
| Handle.onConnect (per-handle) | `<Handle>` prop | yes | [ ] | Only a global `on_connect` exists. |
| NodeHandle / Handle geometry record (SSR dims) | NodeHandle/Handle types | yes | [x] | Declared (not measured) geometry IS the only model; anchor derived, w/h = 1 cell (spec §5). |
| Handle styling state classes (connectingfrom/connectingto/valid) | handles styling table | partial | [x] | `.connectingfrom` analog realized (source handle bolds mid-connect, flow_render.h:223-224); `.connectingto`/`.valid` absent (no validity predicate). |
| Handle pass-through HTML attrs / hiding via visibility / dynamic handles needing internals update | `<Handle>` rest props; handles guide | no | [-] | DOM/CSS escape hatches and the re-measure step do not exist; anchors stay analytically computable regardless of visibility. |
| ConnectionMode (Strict / Loose) | type + `<ReactFlow>` prop | yes | [ ] | No global mode flag; looseness is per-handle (BOTH) only. |
| connectOnClick (click-to-connect / easy connect) | `<ReactFlow>` prop | yes | [x] | First-class path: arm on handle click, commit on second click (flow_input.h:89-91; test_connect.c:114-135). Spec §8 calls it the better TUI UX. |
| Pattern: easy connect (whole node as target) | easy-connect example | partial | [x] | Drop on node body completes (auto-picks first TARGET/BOTH handle). No oversized-handle visual, handle hiding, or floating edge. |
| connectionMode / nodesConnectable / connectionRadius / connectionDragThreshold | `<ReactFlow>` props | yes/partial | [ ] | No global connectable toggle; exact-cell hit-test (no snap radius); connection arms immediately on press (implicit 1-cell). |
| Connection type + ConnectionState + useConnection | types; hook | yes | [x] | Internal in-flight state (`conn_active`/`conn_node`/`conn_handle`/`conn_end`); only `flow_connecting()` bool is public — no rich isValid/from/to accessor. |
| In-progress connection line rendering | learn ConnectionLine | partial | [x] | Dashed orthogonal rubber-band from source anchor to cursor (flow_render.h:229-246). |
| ConnectionLineType / connectionLineStyle / connectionLineComponent (custom preview) / connectionLineContainerStyle | types + props; custom-connectionline | partial/no | [ ]/[-] | Preview is a hardcoded orthogonal dashed line; no type selector, style hook, custom-preview callback. Container CSS is DOM-only. |
| useNodeConnections / useHandleConnections / getNodeConnections / getHandleConnections + HandleConnection/NodeConnection type | hooks; instance; types | yes | [ ] | Edge data carries the full NodeConnection content, but no filter-by-node/handle query exists. |
| Connection events: onConnect | type; events; `<Handle>` | yes | [x] | `on_connect(f, source, target, user)` fired after `flow_add_edge` (flow_model.h:1016; test_connect.c). Delta: passes ids, not a Connection record. |
| Connection events: onConnectStart / onConnectEnd (+FinalConnectionState) / onClickConnectStart/End | types; events; connection-events | yes | [ ] | Begin/end/cancel mechanics exist but emit no events; no FinalConnectionState. Blocks add-node-on-drop, temporary-edges, lifecycle-ordering patterns. |
| reconnectEdge util + addEdge util | utils | yes | [x] | `flow_reconnect_edge` (revalidated repoint) and `flow_add_edge` (reject self/missing/dup, monotonic id). |
| Connection-creation patterns: prevent cycles / connection limit / proximity / add-node-on-drop / temporary / multi-line | interaction & nodes & edges examples | yes/partial | [ ] | All blocked on missing pieces: edge-cycle DFS (only parent-cycle guard exists), useNodeConnections, connect-lifecycle events, custom preview component, multi-source state. |
| UI: BaseHandle / ButtonHandle / LabeledHandle | UI components | partial/yes | [-]/[ ] | Handles already share one default glyph (BaseHandle moot). No per-handle button affordance or adjacent label (handles carry no title field). |

## Viewport & navigation

| Feature | xyflow source | Terminal fit | Status | Notes (analog / evidence) |
|---|---|---|---|---|
| Viewport transform {x,y,zoom} + Viewport/Transform type | type; learn concepts; store transform | yes | [x] | Genuine float transform `flow_viewport{ox,oy,zoom}`; screen=round(world·zoom+offset); real continuous-scale zoom in a char grid (flow_geom.h:5). |
| Pane concept (drag-to-pan surface) | learn concepts | yes | [x] | Empty-pane drag → `dragging_pan`; `flow_pan` moves the camera. |
| width / height (container sizing) | `<ReactFlow>` props; learn | partial/yes | [x] | `flow_new(cols,rows)` / `flow_resize`; the cell rect is the required container size. |
| defaultViewport (initial camera) | `<ReactFlow>` prop | yes | [x] | `flow_new` sets zoom=1/ox=oy=0; settable via pan/set_zoom; `flow_load` restores. No single constructor arg. |
| Controlled viewport (`viewport` prop + onViewportChange) | `<ReactFlow>` props | yes | [ ] | `flow_view_get` reads; pan/set_zoom write; but no on-viewport-change callback fires. |
| panOnDrag (incl. mouse-button array) | `<ReactFlow>` prop; learn | partial | [x] | Left-drag empty pane pans, always-on; no per-button index gating. |
| panOnScroll / panOnScrollSpeed / PanOnScrollMode (axis lock) | props; type | partial/yes | [x]/[ ] | Bare wheel pans 4 directions, 1 cell/notch (the default). No speed scalar; no axis-lock enum. |
| zoomOnScroll + zoomActivationKeyCode | `<ReactFlow>` props; learn | partial | [x] | Ctrl+wheel zooms pointer-centered (real float scale); plain wheel pans. Activation key hardcoded Ctrl. |
| zoomOnPinch | `<ReactFlow>` prop | no | [-] | No multi-touch in terminal mouse protocol. |
| zoomOnDoubleClick | `<ReactFlow>` prop | partial | [ ] | Double-click detected but wired to `on_node_dblclick`, not zoom. |
| panActivationKeyCode (space-pan) | `<ReactFlow>` prop | yes | [x] | Hold-Space forces drag-to-pan over node/pane; sticky toggle (no TTY key-up). Commit c8a518c; test_space_pan.c. |
| minZoom / maxZoom | `<ReactFlow>` props | yes | [x] | `f->zmin`/`zmax` (defaults 0.25/4.0) clamp every zoom + fit; `flow_set_zoom_limits` (flow_view.h:11). |
| translateExtent (pan boundary) | `<ReactFlow>` prop | yes | [ ] | `flow_pan` applies delta unclamped; spec §3 leaves origin unbounded. |
| nodeExtent (node-placement boundary) | `<ReactFlow>` prop | yes | [ ] | No global position clamp; groups clip rendering not positions. |
| fitView (init + instance method) | `<ReactFlow>` prop; instance | yes | [x] | `flow_fit_view(f, margin)` computes bounds+center+clamped-zoom; bound to `f` key. No declarative init boolean. |
| FitViewOptions: padding | type; Padding type | partial | [x] | Single uniform int `margin` (cells); no fractional/percent/px-unit or per-side directional padding. |
| FitViewOptions: nodes subset / minZoom-maxZoom (per-call) / includeHiddenNodes | type | yes/partial | [ ] | `flow_fit_view` always fits ALL nodes, clamps to engine zmin/zmax; no per-call subset or override; no hidden concept. |
| FitViewOptions: duration / ease / interpolate + smooth viewport transitions | type; zoom-transitions example | no | [-] | Camera tweens cut (spec §1/§16); all camera fns snap instantly. |
| setViewport / getViewport / zoomIn / zoomOut / zoomTo / getZoom | instance methods | yes | [x] | `flow_set_zoom`(arbitrary clamped zoom), `flow_view_get`, `flow_zoom_in`/`out` (·/÷ FLOW_ZOOM_STEP, pointer-centered), `flow_zoom`. Animation options cut. |
| setCenter(x,y,{zoom}) | instance method | yes | [ ] | No `flow_set_center`; only fit-view centers (on all nodes). Buildable on pan/set_zoom. |
| fitBounds(rect) | instance method | yes | [ ] | Bounds→camera math inlined in `flow_fit_view` (consumes all-node bounds); no standalone rect-arg form. |
| getViewportForBounds (pure, non-mutating) | util; download-image | partial | [x] | The fit math exists but is inlined + mutates `f->view`; not exposed as a pure rect/size/clamp→viewport function. |
| viewportInitialized flag | instance | partial | [ ] | `flow_run` sizes the pane before the loop; no exposed readiness flag. |
| onMove / onMoveStart / onMoveEnd / onPaneScroll / useOnViewportChange | props; types; hook | yes/partial | [ ] | Pan/zoom mutate the view silently; no viewport-change callback family. |
| useViewport / store transform access (selector reads) | hooks; examples | yes | [x] | `flow_view_get` / `flow_zoom`; reactive re-render replaced by per-frame redraw + damage diff. |
| autoPanOnNodeDrag / autoPanOnConnect / autoPanSpeed | `<ReactFlow>` props; whats-new | yes | [x] | `flow__autopan` follows object drags (node + connect + reconnect) near a buffer edge; `autopan_speed`/`margin` fields (defaults 2/3). `flow_set_autopan` setter deferred to integration. Commit 051cc62; test_autopan.c. |
| autoPanOnSelection | `<ReactFlow>` prop; whats-new 2026-06-01 | yes | [ ] | Marquee branch deliberately does NOT auto-pan (flow_input.h:55). Same `flow__autopan`, gate it to marquee. |
| snapToGrid / snapGrid + group-snap-to-grid | `<ReactFlow>` props; whats-new | yes | [x] | 1-cell snap is inherent (integer cells, spec §3); multi-drag applies one shared delta so selection snaps rigidly. Configurable multi-cell `snapGrid` step absent. |
| Design-tool preset (panOnScroll+selectionOnDrag+panOnDrag=false) | learn the-viewport | partial | [ ] | Constituents ship (wheel-pan, space-pan, Ctrl-zoom, shift-marquee) but no bundled preset toggle; plain-left-drag marquee (selectionOnDrag) absent. |
| Contextual zoom / level-of-detail | contextual-zoom example | yes | [x] | The canonical TUI zoom model: `flow_render_ctx` carries zoom+lod; below `FLOW_LOD_THRESHOLD` (0.6) the default node collapses to a marker (`flow__lod_for`; test_zoom.c). |
| Touch-device support / DnD pointer-events choice | touch-device; drag-and-drop examples | no | [-] | No touch channel; the spinoff idea (click-handle-then-handle) shipped as connectOnClick. Single SGR mouse stream. |
| screenToFlowPosition / flowToScreenPosition | instance; drag-and-drop; whats-new | yes | [x] | `flow_to_world`/`flow_to_screen` (flow_unproject/project); snap-to-grid implicit (integer cells). |

## Selection & interaction

| Feature | xyflow source | Terminal fit | Status | Notes (analog / evidence) |
|---|---|---|---|---|
| elementsSelectable / nodesSelectable / edgesSelectable (global gates) | `<ReactFlow>` props | yes | [ ] | Click-select always on; no global disable. |
| selectNodesOnDrag | `<ReactFlow>` prop | yes | [x] | Drag-start selects an unselected node; already-selected keeps the set for group drag (flow_input.h:177-178). |
| selectionOnDrag (no-modifier marquee) | `<ReactFlow>` prop | yes | [ ] | Marquee only via Shift-drag; bare pane-drag pans. |
| Start box-selection while pointer is over a node | whats-new 12.9.0 (PR #5551) | yes | [ ] | In flow a press over a node always arms node-drag (flow_input.h:170); marquee arms only when `marquee_active`. Would need a modifier-gated override. |
| selectionMode (Full / Partial) | type; `<ReactFlow>` prop | yes | [x] | `FLOW_SELECT_PARTIAL`(intersects) vs `FLOW_SELECT_FULL`(contains); default PARTIAL; `flow_set_marquee_mode`. |
| nodesDraggable / draggable:false on children | `<ReactFlow>` prop; sub-flows | yes | [ ] | Drag always enabled; no global/per-node gate. |
| nodesFocusable / edgesFocusable + Tab focus ring + autoPanOnNodeFocus + focus-offscreen | `<ReactFlow>` props; a11y; whats-new 12.7.0 | yes | [ ] | No keyboard focus ring; arrows pan, not focus-traverse. Tab-focus-cycle + auto-pan-to-focus is a strong keyboard-first candidate. |
| deleteKeyCode / selectionKeyCode / multiSelectionKeyCode | `<ReactFlow>` props | partial | [x] | `x`+Delete delete; Shift-drag marquee; Ctrl-click toggle / Shift-click additive. Modifiers from SGR bits; keys hardcoded (`flow_bind_key` for custom), no null-disable. |
| KeyCode value forms (string / string[] / combo) | type | partial | [x] | `flow_bind_key` takes one ≤7-byte seq, longest-first; no array-of-alternatives; combos via SGR bits only. |
| nodeDragThreshold / nodeClickDistance / paneClickDistance | `<ReactFlow>` props | partial | [x] | 1-cell move flag distinguishes click vs drag (= default 1); 0-tolerance click; not configurable. |
| noDragClassName / noWheelClassName / noPanClassName + utility classes (nodrag/nopan/nowheel) | `<ReactFlow>` props; utility-classes | partial/no | [ ]/[-] | No per-region input-routing exemptions; class-name customization is DOM-only. |
| onNodeClick | Node events | yes | [x] | `on_node_click` on no-drag left-click over node body (flow_input.h:258). Payload is node id, no event object. |
| onNodeDoubleClick | Node events | partial | [x] | `on_node_dblclick` on two consecutive clicks on same node id; NO time threshold. |
| onNodeContextMenu + context-menu pattern | Node events; context-menu example | partial | [x] | `on_node_context(node, screen)` on right-click; topo.c wires a details panel via `on_overlay`. No edge-aware menu-flip positioning. |
| onNodeMouseEnter / MouseMove / MouseLeave | Node events | partial | [ ] | Mode 1002 only (button-event motion); no any-motion hover tracking. `FLOW_HOVERED` exists but not motion-driven. |
| onNodeDragStart / onNodeDrag / onNodeDragStop (+dual-fire with selection-drag) + NodeMouseHandler/OnNodeDrag signatures | Node events; types; migration note | yes | [ ] | Drag mechanics fully work (move/reparent) but emit no drag callbacks; `flow_callbacks` has no drag observer. |
| onEdgeClick / DoubleClick / ContextMenu / MouseEnter/Move/Leave + EdgeMouseHandler | Edge events; type | partial | [ ] | Edge click-select works; no edge event callbacks at all. |
| onPaneClick | Pane events | yes | [x] | `on_pane_click(world)` on no-drag click hitting neither node nor edge (flow_input.h:270-271). |
| onPaneContextMenu / onPaneScroll / onPaneMouseMove/Enter/Leave | Pane events | partial | [ ] | Right-click fires only when a node is hit; wheel consumed internally; no any-motion pane tracking. |
| onSelectionChange + useOnSelectionChange | `<ReactFlow>` prop; hook; type | yes | [x] | `on_selection_change(ids, n)` fires on actual change via `flow__notify_selection` from every selection mutator. Delta: NODE ids only, not the {nodes,edges} pair. Commit 142fcde. |
| onSelectionDragStart/Drag/Stop + SelectionDragHandler | Selection events; type | yes | [ ] | Group drag works (selection-roots delta); no selection-drag callbacks. |
| onSelectionStart / onSelectionEnd / onSelectionContextMenu | Selection events | yes/partial | [ ] | Marquee begin/finalize work but emit no box-select callbacks (only underlying selection-change fires). |
| Drag-and-drop from sidebar palette | drag-and-drop example | partial | [ ] | Nodes created via `flow_add_node`/`n` key, not a palette drop; `flow_to_world` (the conversion it needs) exists. |
| Alignment helper lines with snapping (Pro) | helper-lines Pro | yes | [ ] | No neighbor-alignment guide detection / snap-to-guide during drag. Strong TUI fit. |
| Keyboard navigation model (Tab/Enter/Space/Esc/arrows) | a11y learn | yes | [ ] | Keys exist but do other things: arrows pan, Space toggles space-pan, lone Esc cancels in-flight connection. No Tab focus, Enter/Space-select, arrow-move, Esc-clears-selection (Esc-clears/exit is a deferred integration item). |
| Interaction Props toggle demo | interaction-props example | yes | [ ] | The toggles it exercises (draggable/connectable/selectable/scroll modes) are all candidates; demo unbuildable today. |
| `nodeDragThreshold`/`connectionDragThreshold`/`reconnectRadius`/`paneClickDistance` (config knobs) | Common/Edge props | partial | [x]/[ ] | Behaviors present with fixed tolerances; the configurable props themselves absent. |

## Events & change system

| Feature | xyflow source | Terminal fit | Status | Notes (analog / evidence) |
|---|---|---|---|---|
| Controlled change pipeline: onNodesChange / onEdgesChange | `<ReactFlow>` props; types | yes | [~] | Deliberately collapsed (spec §1 lines 69-74): "we own one store and mutate it directly… the change vocabulary returns as the undo/redo command layer (§11)." |
| NodeChange (6 variants) / EdgeChange (4 variants) unions | types | yes | [~] | Re-targeted onto undo journal commands (add/remove/move/reconnect/set-label/reparent, flow_undo.h). DIMENSIONS variant explicitly cut (browser artifact, spec §4). |
| applyNodeChanges / applyEdgeChanges utils + useNodesState / useEdgesState wrappers | utils; hooks | yes | [~]/[ ] | The reducer-over-immutable-array contract is collapsed into direct mutators + journal; the named utils/hooks are candidates. |
| Combined onDelete | type | yes | [ ] | Only `on_nodes_delete`; no combined node+edge delete callback. |
| onBeforeDelete (abort/modify) + full delete-pipeline sequencing | types; behavior | yes | [ ] | No pre-delete guard; `flow_delete_selection`/`flow_remove_node` delete immediately. |
| onNodesDelete | type | yes | [x] | `on_nodes_delete(ids, n)` once per delete op (incl. cascaded descendants), `cb_suppress`-gated. Payload is int ids. Commit 142fcde. |
| onEdgesDelete | type | yes | [ ] | Cascaded incident edges fire nothing; only `on_nodes_delete` exists. |
| deleteElements (cascade deletion) + deleteKeyCode trigger | type/util; `<ReactFlow>` prop | yes | [x] | `flow_remove_node` cascades incident edges + child nodes; `flow_delete_selection` batch-deletes; Delete/`x` trigger. Synchronous (no Promise); per-id, not one batched-object call. |
| onInit / onInit-as-instance-source | prop; type | yes | [ ] | Caller already holds `flow_t*` from `flow_new`; no one-shot ready hook is provided. |
| onError | prop; type | yes | [ ] | Mutators reject silently (e.g. `flow_add_edge` returns -1); no error-observer pipeline. |
| debug (event logging) | `<ReactFlow>` prop | yes | [ ] | No debug/event-trace flag; would route to stderr/log. |
| experimental_useOnNodesChangeMiddleware | whats-new 12.10.0 | yes | [ ] | No middleware/interceptor over the mutator pipeline; mutators apply directly. |
| DevTools: ChangeLogger | devtools learn | yes | [ ] | Undo journal is the data source (records every mutation); no logger panel exposes/streams it. |

## Built-in components

| Feature | xyflow source | Terminal fit | Status | Notes (analog / evidence) |
|---|---|---|---|---|
| Built-in components as children-of-`<ReactFlow>` (plugin model) | concepts | yes | [x] | Registration-against-shared-state: `flow_set_background`, `flow_set_minimap`, `on_overlay`, composed in render z-order. 3 fixed built-ins, not a general plugin registry. |
| Background component + variants (dots/lines/cross) | component; type | yes | [x] | `flow__background` in viewport space; `FLOW_BG_NONE/DOTS/LINES/CROSS` glyphs · + ┼│─. `flow_set_background`. test_render.c. |
| Background gap | prop | partial | [x] | Single scalar `int gap`, both axes, clamped ≥1; no independent [x,y] tuple. |
| Background size / lineWidth | props | no | [-] | Sub-glyph radius / sub-cell stroke; a unit is always one cell. |
| Background offset / color / bgColor | props | partial | [ ] | Grid phase locked to world origin; pattern fg hardcoded color 8; cells use fixed FLOW_BG. |
| Background className / patternClassName / style | props | no | [-] | CSS/DOM-only. |
| Multiple layered backgrounds via id | prop; Examples | partial | [ ] | Only one background config stored (one variant, one gap, one pass). |
| MiniMap component + position | component; prop | partial/yes | [x] | `flow__minimap`: bordered overview, downsampled node dots, viewport rect, 4-corner anchor (`flow_corner`). spec §7/§14; test_render.c. |
| MiniMap mask (viewport rectangle) | props | partial | [x] | Viewport rect outline drawn (─│); no dimming overlay, no configurable mask color/stroke. |
| MiniMap node color/stroke/className/component/MiniMapNodeProps/bgColor/offsetScale/borderRadius/strokeWidth/custom-nodes | props; type; whats-new | partial/no | [ ]/[-] | Fixed glyphs (• / ◉ selected), no per-node color/shape callback; radius/stroke meaningless at 1 cell. |
| MiniMap interaction (onClick/onNodeClick/pannable/zoomable/inversePan/zoomStep) | props | partial | [ ] | Minimap is render-only; no hit-testing/interaction. |
| MiniMap ariaLabel | prop | no | [-] | No screen-reader layer. |
| Controls component + zoom/fit/lock buttons + position/orientation/children/styling | component; props | yes/partial/no | [ ]/[-] | No button-bar widget; underlying actions exist as keys/fns (`flow_zoom_in`/`out`, `flow_fit_view`); no lock mode. CSS/aria props are DOM-only. |
| ControlButton component | component | yes | [ ] | No button primitive; apps wire actions via `flow_bind_key`/`on_overlay`. |
| Panel component (fixed overlay) + position (8 anchors) + children | component; type; props | yes | [x]/[ ] | `on_overlay` is the spec-designated Panel analog (spec §14): full-screen viewport-untransformed surface drawn after minimap. App computes placement (no built-in anchoring; 8-position enum absent). className/style are CSS-only. |
| ViewportPortal (content in viewport space) + children | component; prop | partial | [ ] | No viewport-transformed content hook; `on_overlay` is fixed screen-space. App could call `flow_to_screen` manually. |
| NodeResizer + all props (nodeId/min-max/keepAspectRatio/autoScale/isVisible/shouldResize/styling) + lifecycle (onResizeStart/Resize/End) + ResizeParams/ResizeDragEvent | component; props; types; whats-new; node-resizer example | partial/yes/no | [ ]/[-] | No resize subsystem at all — node w/h are content-driven via `measure()`, never user-resized. `autoScale` impossible at 1-cell resolution. |
| NodeResizeControl + position/variant/resizeDirection/children | component; props | partial/yes | [ ] | No single-control resize building block. |
| NodeToolbar + all props (nodeId/isVisible/position/offset/align/non-scaling) + mutate-via-setNodes pattern | component; props; node-toolbar example | yes/partial | [ ] | No node toolbar; only `on_node_context` right-click affordance. Non-scaling would be automatic (1-cell glyphs). |
| EdgeToolbar + props (edgeId/x/y/isVisible/alignX/alignY) + midpoint-delete pattern | component; props; edge-toolbar example | yes | [ ] | No edge toolbar; midpoint (`label_anchor`) and edge delete/select exist but no anchored overlay. |
| UI registry nodes: BaseNode / NodeTooltip / NodeStatusIndicator / PlaceholderNode / DatabaseSchemaNode / NodeAppendix | UI components | yes/partial | [ ] | None ship. BaseNode (header/content/footer regions), placeholder (dashed box + click-to-create), DB-schema (per-row handles via `along`), appendix (adjacent strip — needs engine support past clip). Status spinner needs an animation loop (cut). |
| Node Search / command-palette UI | node-search UI | yes | [ ] | Primitives exist (`flow_select_node`, `flow_fit_view`, node iteration); no search UI. Excellent TUI feature. |
| DevTools UI (NodeInspector / ViewportLogger / bundled DevTools) | devtools learn + UI | yes | [ ] | All buildable via `on_overlay` + accessors; none ship. |
| ZoomSlider / ZoomSelect widgets | UI components | partial | [ ] | Drawable via `on_overlay`; underlying ops exist; keys +/-/f already cover the actions. |

## Programmatic API & queries

| Feature | xyflow source | Terminal fit | Status | Notes (analog / evidence) |
|---|---|---|---|---|
| getNodes / getNode / getInternalNode / getEdges / getEdge | instance methods | yes | [x] | `flow_nodes`/`flow_node_count`/`flow_get_node`/`flow_edges`/`flow_edge_count`/`flow_get_edge`. InternalNode = node + `flow_node_abs` + cached w/h. |
| addNodes / addEdges | instance methods | yes | [x] | `flow_add_node` (mints id, measures) / `flow_add_edge` (addEdge-style validation). Single-form; multi is a loop. Records undo. |
| setNodes / setEdges (bulk overwrite + updater) | instance methods | yes | [ ] | No bulk array-replace setter; spec §1 collapses the controlled contract into per-node mutators. |
| updateNode / updateEdge (merge/replace) | instance methods | yes | [x] | Partial-field via targeted mutators (`flow_move_node`, `flow_set_parent`, `flow_reconnect_edge`, `flow_set_edge_label`) + mutable `flow_get_node`/`flow_get_edge`. No single merge/replace entry-point. |
| updateNodeData / updateEdgeData (reactive data merge) | instance methods | yes | [ ] | `data` is opaque void*, set at add only; no merge/replace fn, no change notification. Edge data not persisted. |
| deleteElements (batched node+edge) | instance method | yes | [x] | Cascade implemented per-id (`flow_remove_node`/`flow_delete_selection`); synchronous, no single batched-object call returning deleted sets. |
| getIntersectingNodes (query) / isNodeIntersecting (predicate) | instance methods; utils | yes | [x]/[ ] | `flow_rect_intersects` + `flow_node_rect_abs` give the predicate; the multi-node QUERY API is unbuilt. |
| getNodesBounds / getViewportForBounds | utils; instance | yes/partial | [x] | `flow_bounds` (all nodes, top-left origin; no subset/nodeOrigin). getViewportForBounds math inlined in `flow_fit_view`. |
| getNodeConnections / getHandleConnections (graph queries) | instance methods | yes | [ ] | Edges iterable but no filter-by-node/handle query. |
| getIncomers / getOutgoers / getConnectedEdges | graph utils | yes | [ ] | No predecessor/successor/incident-edge utils; trivially buildable over `flow_edges`. |
| isNode / isEdge type-guards | utils | yes | [-] | Nodes and edges are distinct static C types in separate arrays; no mixed `element[]` to discriminate. The type system is the guard. |
| Rect / XYPosition types | types | yes | [x] | `flow_rect{x,y,w,h}` / `flow_pt{x,y}` (integer cells). |
| toObject / ReactFlowJsonObject shape | instance; type | yes | [x] | `flow_save` writes {version, viewport, nodes, edges} JSON; writes to a path, not an in-memory object. Snapshot-tested (json_basic.txt). |
| useReactFlow / `<ReactFlowProvider>` / multiple-flows / SSR-init props | hooks; provider component | yes | [x] | The `flow_t*` IS the instance/provider/store, passed directly; no React context. Several independent `flow_t*` are isolated by construction. `initialWidth/Height` SSR dance moot (TTY size known); router-persistence note n/a. |
| Hooks reading state: useNodes / useEdges / useViewport / useInternalNode / useNodeId / useStore / useStoreApi | hooks | yes | [x] | Accessors + per-frame render-and-diff replace the subscription model; `flow_t*` is the raw store handle; internal struct visible under FLOW_IMPLEMENTATION. |
| useNodesData / useNodeConnections (reactive) / useHandleConnections / useOnViewportChange | hooks | yes | [ ] | Reactive data/connection subscriptions and viewport-change phases not built. |
| useNodesInitialized / useUpdateNodeInternals | hooks | yes | [-] | Sizes/anchors are synchronous + analytic; no async-measure or stale-internals state to gate/invalidate (spec §5). |
| useKeyPress | hook | partial | [x] | `flow_bind_key`/`flow_dispatch_key` handle key EVENTS (the terminal's native model); only Space keeps sticky held-state — no generic held-state polling (no reliable keyup). |
| Generic typing of instance/hooks (NodeType/EdgeType) | typescript learn | yes | [x] | Runtime analog = `type` tag + per-type data struct + vtable registry; no compile-time generics. |
| Controlled vs uncontrolled model (which applies) + external state-manager integration + computing-flows | learn pages; examples | yes | [x]/[ ] | flow is decisively uncontrolled — it IS the centralized store with mutator actions read by renderers. Computing-flows (data propagation) needs three unbuilt primitives (useNodeConnections/useNodesData/updateNodeData) → candidate. |
| SSR/SSG static rendering | ssr-ssg learn | yes | [x] | `flow_render(f, out, cols, rows)` composes to a memory buffer with no TTY/loop — exactly the no-live-measurement situation. Used by every snapshot test. |
| Client image export (downloadImage) / server-side image (Puppeteer, Pro) | misc examples; Pro | no | [-] | DOM→PNG/canvas/headless-browser; nearest analog (dump cell buffer to text/ANSI) is a different mechanism. |

## Sub-flows & grouping

| Feature | xyflow source | Terminal fit | Status | Notes (analog / evidence) |
|---|---|---|---|---|
| parentId mechanics (relative coords, parent-before-child, move-parent-moves-children) | sub-flows learn | yes | [x] | `int parent`; `flow_set_parent` converts abs→relative; `flow_node_abs` walks chain; depth-aware render order. Commit 438ea0d. |
| group node type (handle-less container) + Labeled Group Node UI | sub-flows; UI component | yes | [x] | `flow_group_node_type` (handle_count 0) draws a box frame + optional label from `node->data`. Any node can be a parent. |
| Nested sub-flows (multi-level) | sub-flows example | yes | [x] | `flow_node_abs` recurses the full chain; `flow__node_depth` orders arbitrary nesting (test_groups.c 3-level). |
| extent:'parent' child clamping + CoordinateExtent geometry | sub-flows; types | yes | [~] | Spec §9 names it; groups landed WITHOUT the clamp. `flow_set_parent`/drag apply no parent-bounds clamp. |
| elevateNodesOnSelect (selected raised z) | `<ReactFlow>` prop | yes | [x] | Selected siblings draw last within their depth layer (spec §6; commit 438ea0d). |
| ZIndexMode (auto/basic/manual) / zIndexMode prop | type; whats-new 12.10.0 | yes/partial | [ ] | Fixed depth+selected ordering; no configurable z-index policy enum. |
| Dynamic parent-child attach/detach by drag (Pro) | parent-child-relation Pro | yes | [x] | Drag-to-reparent: hit-test group under dropped node (skip self/descendants) → `flow_set_parent` preserving abs; drop-on-pane detaches (flow_input.h:179-218). |
| Selection-box grouping / ungrouping (Pro) | selection-grouping Pro | yes | [x] | `flow_group(ids, n)` sizes container from member bbox + reparents; `flow_ungroup` dissolves. test_groups.c. Group/Ungroup buttons are app UI. |
| Edge z-index over parented nodes / edges-above-nodes in subflow | sub-flows learn; whats-new 12.8.0 | partial | [ ] | Fixed compose order draws all edges below all nodes; no per-edge z or parented-edge-above rule. |
| Prevent cross-parent child overlap / collision | whats-new 12.9.0 | yes | [ ] | No overlap-avoidance pass anywhere. |
| draggable:false on child | sub-flows learn | yes | [ ] | No per-node draggable gate. |

## Layouting

| Feature | xyflow source | Terminal fit | Status | Notes (analog / evidence) |
|---|---|---|---|---|
| No built-in layouting (positions explicit) | layouting learn | yes | [~] | Spec §10 mirrors xyflow: layout is a consumer convenience through `flow_move_node`. WP#8 adds two opt-in algorithms; not yet landed (no `src/flow_layout.h`; `FLOW_LAYOUT`/`flow_layout` absent from src — verified). |
| Layered/Dagre layout + direction switching + static-vs-dynamic distinction | dagre example; learn | partial/yes | [~] | Spec §10 `FLOW_LAYOUT_LAYERED` (Sugiyama longest-path + barycenter, dir LR/TB, gap_x/gap_y) = dagre analog. WP#8 lines 1124-1186. Unlanded. |
| d3-force physics layout + non-overlapping insertion (Pro) | learn; force-layout Pro | partial/yes | [~] | Spec §10 `FLOW_LAYOUT_FORCE` (Fruchterman-Reingold, compute in float, round to cells on commit). Unlanded. |
| Auto-layout hook with switchable engines (Pro) | auto-layout Pro | partial | [~] | `flow_layout(f, opts)` dispatcher: mode enum + dir + gaps (WP#8). Two engines, same pattern. Unlanded. |
| Edge routing libraries (orthogonal/smart-edge/elk routing) | layouting learn | partial | [x] | Ships orthogonal + straight routers; custom routers via vtable. Smart/A* grid routing past these two unbuilt. |
| Horizontal/mixed flow direction via handle positions | horizontal example | yes | [x] | `flow_handle.pos` (TOP/RIGHT/BOTTOM/LEFT) + `along`; analytic anchors per side. |
| d3-hierarchy / entitree-flex / d3-flextree / Cola.js / elkjs (distinct engines) | learn; examples | partial | [ ] | Outside the two-algorithm scope (force + layered). Genuine future candidates for a richer layout engine. |
| elkjs port-constrained routing (FIXED_ORDER) | elkjs-multiple-handles example | partial | [ ] | Per-node handles exist; no port-constrained layout/routing. |
| Rectangular collision force / node-collision auto-resolution | learn; node-collisions example | yes | [ ] | AABB separation (run on drag-end) is a strong TUI fit; distinct from FR repulsion inside the specd layout. No collision pass exists. |
| Dynamic layouting with placeholder nodes (Pro) | dynamic-layouting Pro | partial | [ ] | Placeholder-construction UX unscoped; relies on a d3-hierarchy engine flow lacks. |
| Expand/collapse tree (useExpandCollapse) (Pro) | expand-collapse Pro | yes | [ ] | No hidden/collapse field; LOD collapse is zoom-driven, unrelated. Ideal TUI fit (file-tree triangles). |

## Whiteboard

| Feature | xyflow source | Terminal fit | Status | Notes (analog / evidence) |
|---|---|---|---|---|
| Freehand draw (perfect-freehand) (Pro) | freehand-draw Pro | no | [-] | Smooth sub-cell vector curves; spec §1 non-goal. Nearest analog: a coarse box-drawing polyline. |
| Lasso (freeform) selection | lasso-selection example | partial | [ ] | Point-in-polygon ports; rectangular marquee already does FULL/PARTIAL modes; freeform lasso specifically is the candidate. |
| Eraser tool | eraser example | partial | [ ] | Trail-vs-element AABB ports; no eraser/trail/toBeDeleted mechanic (delete is via Delete/x). |
| Rectangle / shape drawing tool | rectangle example | yes | [ ] | Drag-to-draw a bordered region is a natural TUI primitive; no draw-to-create tool. |
| Tool-mode toggling pattern | whiteboard learn | yes | [ ] | No modal tool state (gesture states are transient); `flow_bind_key` could host it. |
| Whiteboard library guidance (tldraw/Excalidraw) | whiteboard learn | no | [-] | Documentation guidance, not a feature. |

## Persistence & serialization

| Feature | xyflow source | Terminal fit | Status | Notes (analog / evidence) |
|---|---|---|---|---|
| toObject / ReactFlowJsonObject + save & restore pattern | instance; examples; type | yes | [x] | `flow_save`/`flow_load` round-trip nodes (id/type/pos/parent/vtable-data), edges (source/target/handles/type/label), float viewport (%.9g). One-call restore (flow owns the store), simpler than React's manual re-apply. Commit f3f87a0; test_json.c round-trip. |
| Save & restore viewport (real zoom survives) | save-and-restore example | yes | [x] | `flow_save` writes ox/oy/zoom; `flow_load` restores `f->view`; float round-trip tested. |
| Ephemeral vs durable state partition | multiplayer learn | yes | [x] | flow's persistence boundary IS this partition (spec §4): durable fields only; test_json.c:369-373 asserts no flags/selected/zmin/marquee in JSON; w/h recomputed via measure. |
| Undo/redo snapshot history (Pro) | undo-redo Pro | yes | [x] | `flow_undo`/`flow_redo`/`flow_can_undo`/`flow_can_redo`/`flow_set_undo_limit`; inverse-op journal (ADD/REMOVE node+edge, MOVE, RECONNECT, SET_LABEL, REPARENT); gesture coalescing; `u`/Ctrl-r bindings. test_undo.c; commit 74b6045. |
| Copy / cut / paste of selection (Pro) | copy-paste Pro | yes | [ ] | No clipboard/copy/cut/paste/duplicate (the "copy" hits are label-memory helpers). |
| Collaborative multiplayer (Yjs/CRDT) (Pro) + architecture guidance + cursor presence | collaborative Pro; multiplayer learn | partial | [ ] | No networking/CRDT/sync; single-header no-deps single-user (spec §1). Large future subsystem; cursors → colored cell markers, gated on the networking layer. |

## Styling & theming

| Feature | xyflow source | Terminal fit | Status | Notes (analog / evidence) |
|---|---|---|---|---|
| className / style on container | `<ReactFlow>` props | no | [-] | No CSS in a terminal; styling is per-cell fg/bg/attr (`flow_cell`). |
| Required CSS stylesheet import / base.css vs style.css | learn concepts; base-style example | no | [-] | No external stylesheet; visual defaults baked into the compositor (single-header, no deps). |
| colorMode (light/dark/system) | type; dark-mode example | partial | [ ] | Could map to two ANSI palettes; colors hardcoded per renderer, no theme switch. |
| CSS variable theme tokens (--xy-*) + inline style props + overridable CSS classes | theming learn | partial/no | [ ]/[-] | No centralized theme struct or per-element style override; appearance owned by the type's `render()`. Class selectors are DOM-only; the enumerated element-STATES are a useful checklist (selected handled; connecting/valid not). |
| Utility class names configurable (nodrag/nopan/nowheel) | utility-classes note | no | [-] | Class strings are DOM-specific; the underlying region-exempt behavior (itself unbuilt) is what would port. |
| Animated edge (dashdraw) | theming; Edge.animated | partial | [ ] | (See Edges.) In-grid marching-ants is a cheap candidate; sub-cell smoothness cut. |
| Custom SVG markers / markerEnd | turbo-flow; whats-new | partial | [x] | Directional arrowhead glyph at target terminus is the markerEnd/cap analog; arbitrary SVG markers don't port. |
| TailwindCSS styling | tailwind example; whats-new | no | [-] | Browser CSS framework; the parallel point (nodes are consumer-controlled) IS realized via the `render()` vtable. |
| Turbo Flow (animated gradient borders) | turbo-flow example | no | [-] | Sub-cell/GPU gradients + glow; spec §1 non-goal. |
| proOptions / hideAttribution / attributionPosition | props; types | partial/yes | [ ] | No watermark to hide; no general Panel-position anchoring (overlay is one hook). |

## Accessibility & keyboard

| Feature | xyflow source | Terminal fit | Status | Notes (analog / evidence) |
|---|---|---|---|---|
| Keyboard navigation (Tab/Enter/Space/Esc/arrows) + disableKeyboardA11y | a11y learn; `<ReactFlow>` prop | yes | [ ] | (See Selection & interaction.) The xyflow keyboard-nav model is unbuilt; nothing to gate. |
| ARIA roles/attributes + aria-live region | a11y learn | no | [-] | No accessibility tree for a TTY; status-line announce region not built. |
| ariaLabelConfig (customizable UI/help strings) | `<ReactFlow>` prop; type; whats-new 12.7.0 | partial | [ ] | Statusbar help line is hardcoded, not a configurable label/string table. The string-table analog (for status-line hints) is the candidate. |
| autoPanOnNodeFocus | `<ReactFlow>` prop; whats-new | yes | [ ] | No focus model; auto-pan is drag-only. (Pairs with the Tab-focus candidate above.) |

## Performance

| Feature | xyflow source | Terminal fit | Status | Notes (analog / evidence) |
|---|---|---|---|---|
| onlyRenderVisibleElements (viewport culling) | `<ReactFlow>` prop | yes | [ ] | Render loop visits every node; only per-node ancestor-clip + `flow_put` bounds-clip exist, no viewport-bounds skip. Natural perf win. |
| Memoize node/edge components | performance learn | partial | [x] | Analog = damage-diff renderer: `flow_present` emits ANSI only for changed cells (spec §6). |
| Memoize callbacks/objects | performance learn | no | [-] | React reference-stability concern; C structs/function pointers are stable. |
| Don't read full nodes/edges in components (derived/indexed state) | performance learn | partial | [ ] | No separate derived-selection index; `on_selection_change` fires only on actual change (partial). Architectural. |
| Collapse large node trees via hidden | performance learn | yes | [ ] | No `hidden` field/culling; LOD collapses detail by zoom, not by flag. |
| Simplify node/edge styles | performance learn | no | [-] | No shadows/gradients; per-cell SGR already minimal + coalesced. |
| Stress test (large graph) | stress example | partial | [ ] | No N×M generator/benchmark; minZoom clamp (0.25) already handles the sub-cell case. |
| Testing: Cypress/Playwright | testing learn | partial | [x] | Headless analog built + exceeding it: `flow_render` → in-memory buffer for golden snapshots; `flow_feed` injects synthetic SGR for interaction tests (spec §13). No TTY. |
| Testing: Jest mockReactFlow shim / DOM-measurement requirement | testing learn; multiplayer | no/yes | [-]/[x] | DOM-measurement mocking unneeded; `flow_measure_node` computes w/h from `measure()`, never DOM. |

## App-level patterns (examples / Pro / ecosystem)

| Feature | xyflow source | Terminal fit | Status | Notes (analog / evidence) |
|---|---|---|---|---|
| Example gallery / Pro examples catalog | overview; pro/examples | partial | [~]/[-] | Aggregate meta-entries; constituents itemized above. Underlying features (serialize, undo, groups) landed; showcase `demos/flowchart.c` (groups+layout) pending (WP#9, gated on auto-layout). Tailwind/Turbo/collaborative/image-export are web-bound. |
| llms.txt AI documentation | whats-new 2026-03-19 | no | [-] | Docs-site meta-feature; no library surface. |

---

## Candidate shortlist for increment 4+

Status=candidate features with terminal_fit yes/partial, ordered by (user value ÷ effort). Sizes use the repo's S/M/L/XL workpackage scale.

1. **Esc clears selection + Esc exits** — **S.** Already a deferred integration item (handoff); `flow_clear_selection` exists, just bind lone-Esc. Closes a glaring keyboard gap.
2. **`flow_set_autopan(margin, speed)` setter** — **S.** Fields + behavior already exist (commit 051cc62); only the public tuning/disable knob is missing (deferred in integration list).
3. **autoPanOnSelection (marquee auto-pan)** — **S.** Same `flow__autopan` already called from node/connect/reconnect drags; just call it from the marquee branch (flow_input.h:183-189).
4. **Edge event callbacks (on_edge_click / context / select)** — **S.** Edge hit-test + select already work; add observer fields to `flow_callbacks` mirroring the node events. Unlocks edge-aware apps.
5. **Node visibility flag (`hidden`) + render/hit-test skip + edge cascade** — **M.** One flag bit; enables expand/collapse, the hidden example, and viewport culling. High reuse.
6. **Graph-traversal helpers (getIncomers / getOutgoers / getConnectedEdges / getNodeConnections)** — **M.** Pure scans over `flow_edges`; unblocks delete-middle-node repair, connection limits, computing-flows, prevent-cycles.
7. **isValidConnection predicate hook (engine-level)** — **S.** One function-pointer field; structural validation already centralized in `flow_add_edge`. Unlocks validation + prevent-cycles patterns.
8. **Node/edge intersection QUERY API (`flow_get_intersecting_nodes`)** — **S.** Primitives (`flow_rect_intersects`, `flow_node_rect_abs`) already exist and are used by marquee; expose a list-returning query.
9. **`extent:'parent'` child clamp** — **S.** Spec §9 already names it; pure clamp in `flow_set_parent`/drag. Completes grouping.
10. **Connect-lifecycle events (onConnectStart / onConnectEnd + FinalConnectionState)** — **M.** Begin/end/cancel mechanics already run; add events. Unlocks add-node-on-drop, temporary-edges, delete-edge-on-drop, lifecycle-ordering.
11. **Node-position extent / translateExtent clamps** — **S.** Pure min/max arithmetic on `flow_move_node` / `flow_pan`. Bounds an otherwise-infinite canvas.
12. **on_move / on_viewport_change callback family** — **S.** Add fields fired from `flow_pan`/`flow_set_zoom`/`flow_fit_view`. Enables viewport loggers, controlled-viewport mirrors.
13. **Alignment helper lines + snap-to-guide on drag** — **M.** Integer bound comparisons; guides as full row/column rules. Strong polish-per-effort.
14. **Copy / cut / paste of a selection** — **M.** Serialize selection → offset → re-id → reinsert via existing mutators + undo; in-process clipboard.
15. **Static dashed / marching-ants animated edge flag** — **M.** Per-edge flag + dash glyph cycle on the run loop's tick; the connection preview already proves the dashed render.
16. **Node-search / command palette (select + fit-to-result)** — **M.** Substring match + `flow_select_node` + `flow_fit_view`. Distinct, high-value TUI feature.
17. **Tab focus ring across nodes + autoPanOnNodeFocus** — **M.** New focused-node index + Tab binding + reuse `flow__autopan`. Keyboard-first navigation.
18. **NodeToolbar / EdgeToolbar (anchored action overlay)** — **L.** Selection-gated overlay anchored to node/edge rect, wired to mutators; constant-size is automatic. Composite over `on_overlay`.
19. **Theme/palette struct (colorMode + token table)** — **L.** Centralize the hardcoded per-renderer fg/bg into a theme struct; enables dark/light and per-element overrides.
20. **NodeResizer (keyboard- or mouse-driven node resize)** — **L.** New subsystem: resize gesture, min/max clamps, selection-gated corner/edge grab glyphs; requires breaking the content-driven-size assumption for resizable types.
21. **DevTools overlay HUD (NodeInspector + ViewportLogger + ChangeLogger)** — **M.** All three buildable on `on_overlay` + accessors + the undo journal as the change source.
22. **Viewport culling (onlyRenderVisibleElements)** — **M.** Rect-overlap skip before the per-node draw; pairs with `hidden` for large-graph perf.
23. **Self-loop edge (source===target) + loop glyph** — **L.** Requires relaxing the self-edge rejection for an opt-in case plus a small loop glyph router beside the node.
24. **Eraser / rectangle / lasso whiteboard tools + tool-mode state** — **L.** Modal tool state over `flow_bind_key` + AABB/point-in-polygon hit logic; the rectangle and lasso(full/partial) halves are the cleanest fits.

## Explicitly dropped (terminal-incompatible)

These na-terminal features have no meaningful char-cell analog (analog noted where one exists):

- **CSS layer:** `className`, `style`, `connectionLineStyle/ContainerStyle`, Edge/Node `style`/`className`, `labelStyle`/`labelBgStyle`, NodeResizer/Controls/Panel styling, Background `className`/`patternClassName`/`style`, MiniMap mask/aria styling, edge CSS class hooks, utility-class name customization — *analog: per-cell fg/bg/attr chosen in render code, no cascade.*
- **Required stylesheets** (`dist/style.css`, `base.css`) — self-contained compositor, no import step.
- **SVG vector artifacts:** custom `<defs>` markers, `labelBgBorderRadius`, MiniMap `nodeBorderRadius`/`nodeStrokeWidth`, Background `size`/`lineWidth`, NodeResizer `autoScale` — *sub-cell geometry; a unit is one cell.*
- **ARIA / accessibility tree:** ARIA roles/attributes, aria-live region, Node/Edge `ariaRole`, MiniMap/Controls `ariaLabel`, `domAttributes` on nodes/edges — no AT surface for a TTY; *closest analog is a status-line announce region (unbuilt).*
- **Touch / multi-touch:** `zoomOnPinch`, touch-device tap UX, Pointer-Events/Neodrag DnD variant — no touch channel in the SGR mouse stream; *analog: connectOnClick + single drag handler.*
- **Animation smoothness (spec §1/§16 cut):** FitViewOptions `duration`/`ease`/`interpolate`, smooth viewport transitions, viewport ease/interpolate, node-position animation, animating a node along an edge, Turbo gradient/glow — *analog: instant snap.*
- **Browser-page / DOM-lifecycle concerns:** `preventScrolling`, nodeTypes referential-stability, `useUpdateNodeInternals`/`useNodesInitialized`, dynamic-handles re-measure, hiding-handles-via-visibility, `initialWidth/Height`, ReactFlowProvider router-persistence, memoize-callbacks, Jest mockReactFlow shim — designed out by the analytic-anchor + synchronous-`measure()` model.
- **Image/server export:** client `downloadImage` (html-to-image), server-side Puppeteer image creation, shapes-as-PNG — *analog: dump the `flow_render` cell buffer to text/ANSI (a different mechanism).*
- **Whiteboard freehand (perfect-freehand) + tldraw/Excalidraw guidance** — sub-cell vector strokes; *analog: a coarse box-drawing polyline.*
- **TailwindCSS / `isNode`/`isEdge` type-guards / `proOptions` watermark / llms.txt / Pro-catalog & gallery meta-entries** — framework, static-typing, web-business, or documentation meta-features with no library surface; *analog for type-guards: the static C type system itself.*