/* ===== undo/redo: appliers for the command journal (spec §11) =====
   Recording primitives live in flow_model.h (pure data push); this module holds
   everything that APPLIES inverses — it calls the mutators, so it must be
   amalgamated immediately AFTER flow_model.

   Replay contract: flow_undo applies a command's ops in REVERSE order via their
   inverses, flow_redo re-applies forward. While replaying, journal.applying gates all
   recording (a replay never journals itself) and cb_suppress gates the observer
   callbacks (on_nodes_delete / on_selection_change) — an app mutating mid-replay
   could not be journaled consistently, so replay is silent by design.
   Lifetime: node->data is BORROWED across the undo window and reattached verbatim on
   undo-of-delete (never freed — same contract as flow_remove_node/flow_free); edge
   labels are engine-OWNED, so snapshots carry dup'd copies and every re-insert dups
   again (the snapshot keeps its copy for the next replay round). */
#ifdef FLOW_IMPLEMENTATION
/* positional re-insert: open a hole at snap->index and copy the snapped struct back
   verbatim (id, type, pos, parent, w/h, data ptr), clearing ephemeral flags. Restores
   both the ORIGINAL id (so edge endpoints / parent refs stay valid) and the ORIGINAL
   array index (render/hit iterate by insertion order; spec §13 apply→undo equality). */
static void flow__insert_node_at(flow_t *f, const flow__node_snap *s) {
  f->nodes = (flow_node*)flow__grow(f->nodes, &f->capnodes, f->nnodes + 1, sizeof(flow_node));
  int at = s->index; if (at < 0) at = 0; if (at > f->nnodes) at = f->nnodes;
  memmove(&f->nodes[at + 1], &f->nodes[at], (size_t)(f->nnodes - at) * sizeof(flow_node));
  f->nodes[at] = s->node;
  f->nodes[at].flags &= ~(FLOW_SELECTED | FLOW_DRAGGING | FLOW_HOVERED);
  f->nnodes++;
}
static void flow__insert_edge_at(flow_t *f, const flow__edge_snap *s) {
  f->edges = (flow_edge*)flow__grow(f->edges, &f->capedges, f->nedges + 1, sizeof(flow_edge));
  int at = s->index; if (at < 0) at = 0; if (at > f->nedges) at = f->nedges;
  memmove(&f->edges[at + 1], &f->edges[at], (size_t)(f->nedges - at) * sizeof(flow_edge));
  f->edges[at] = s->edge;
  f->edges[at].label = flow__dup(s->label_copy);      /* fresh engine-owned copy; snap keeps its own */
  f->edges[at].flags &= ~(FLOW_SELECTED | FLOW_DRAGGING | FLOW_HOVERED);
  f->nedges++;
}
/* remove a single SLOT by id (no cascade): the inverse of an ADD — in a journal-
   consistent history the object has no dependents left at undo time (anything that
   referenced it was journaled later and therefore already undone). */
static void flow__remove_node_slot(flow_t *f, int id) {
  for (int i = 0; i < f->nnodes; i++) {
    if (f->nodes[i].id != id) continue;
    memmove(&f->nodes[i], &f->nodes[i + 1], (size_t)(f->nnodes - i - 1) * sizeof(flow_node));
    f->nnodes--;
    return;
  }
}
static void flow__remove_edge_slot(flow_t *f, int id) {
  for (int i = 0; i < f->nedges; i++) {
    if (f->edges[i].id != id) continue;
    FLOW_FREE(f->edges[i].label);
    memmove(&f->edges[i], &f->edges[i + 1], (size_t)(f->nedges - i - 1) * sizeof(flow_edge));
    f->nedges--;
    return;
  }
}
/* apply one op in the given direction (redo=1 forward, redo=0 inverse). Value-replay
   ops go through the PUBLIC mutators (validation + parent-relative math for free);
   structural restores use the positional-insert/slot helpers so ids and indices come
   back exactly. Each op also restores the id counters for its direction, so
   undo-all → add re-mints the original ids (spec: id-counter integrity). */
static void flow__apply_op(flow_t *f, flow__op *op, int redo) {
  switch (op->kind) {
    case FLOW_CMD_ADD_NODE:
      if (redo) flow__insert_node_at(f, &op->u.node.snap);
      else {
        flow_node *n = flow_get_node(f, op->u.node.snap.id);
        if (n) {                                      /* snapshot at UNDO time: captures post-add direct
                                                         field writes (e.g. flow_group's container w/h) */
          op->u.node.snap.node = *n;
          op->u.node.snap.index = (int)(n - f->nodes);
          flow__remove_node_slot(f, op->u.node.snap.id);
        }
      }
      break;
    case FLOW_CMD_REMOVE_NODE:
      if (redo) flow_remove_node(f, op->u.subtree.root);  /* applying-gated: cascades without re-recording */
      else {                                          /* ascending original index rebuilds insertion order */
        for (int i = 0; i < op->u.subtree.nn; i++) flow__insert_node_at(f, &op->u.subtree.nodes[i]);
        for (int i = 0; i < op->u.subtree.ne; i++) flow__insert_edge_at(f, &op->u.subtree.edges[i]);
      }
      break;
    case FLOW_CMD_ADD_EDGE:
      if (redo) flow__insert_edge_at(f, &op->u.edge.snap);
      else {
        flow_edge *e = flow_get_edge(f, op->u.edge.snap.id);
        if (e) {                                      /* refresh snap (label re-dup'd) for the next redo */
          FLOW_FREE(op->u.edge.snap.label_copy);
          op->u.edge.snap.edge = *e;
          op->u.edge.snap.label_copy = flow__dup(e->label);
          op->u.edge.snap.edge.label = op->u.edge.snap.label_copy;
          op->u.edge.snap.index = (int)(e - f->edges);
          flow__remove_edge_slot(f, op->u.edge.snap.id);
        }
      }
      break;
    case FLOW_CMD_REMOVE_EDGE:
      if (redo) flow__remove_edge_slot(f, op->u.edge.snap.id);
      else      flow__insert_edge_at(f, &op->u.edge.snap);
      break;
    case FLOW_CMD_MOVE_NODE:                          /* MOVE replays the SAME absolute coords it accepted */
      flow_move_node(f, op->u.move.id, redo ? op->u.move.to : op->u.move.from);
      break;
    case FLOW_CMD_RECONNECT_EDGE:
      if (redo) flow_reconnect_edge(f, op->u.reconnect.id, op->u.reconnect.to_node,
                                    op->u.reconnect.to_handle, op->u.reconnect.which);
      else      flow_reconnect_edge(f, op->u.reconnect.id, op->u.reconnect.from_node,
                                    op->u.reconnect.from_handle, op->u.reconnect.which);
      break;
    case FLOW_CMD_SET_LABEL:
      flow_set_edge_label(f, op->u.label.id, redo ? op->u.label.to : op->u.label.from);
      break;
    case FLOW_CMD_REPARENT: {
      flow_set_parent(f, op->u.reparent.child,
                      redo ? op->u.reparent.to_parent : op->u.reparent.from_parent);
      flow_node *c = flow_get_node(f, op->u.reparent.child);  /* exact stored-rel restore (field equality) */
      if (c) c->pos = redo ? op->u.reparent.to_pos : op->u.reparent.from_pos;
      break;
    }
    case FLOW_CMD_RESIZE_NODE:                        /* replay the SAME w/h it accepted; applying-gated => no re-record */
      flow_set_node_size(f, op->u.resize.id, redo ? op->u.resize.tw : op->u.resize.fw,
                                             redo ? op->u.resize.th : op->u.resize.fh);
      if (!redo && !op->u.resize.from_explicit) {     /* undo of a once-auto node: the setter re-set the flag — drop it
                                                         again so a later data change re-measures (apply→undo equality) */
        flow_node *n = flow_get_node(f, op->u.resize.id);
        if (n) n->flags &= ~(unsigned)FLOW_EXPLICIT_SIZE;
      }
      break;
  }
  if (redo) { f->nextid = op->nid1; f->nexteid = op->eid1; }
  else      { f->nextid = op->nid0; f->nexteid = op->eid0; }
}
int  flow_can_undo(flow_t *f) { return f->journal.n > 0; }
int  flow_can_redo(flow_t *f) { return f->journal.rn > 0; }
/* inc-6 #7: read-only journal introspection — pure reads of journal.n/.rn and the top
   command's last op kind; safe any frame (no applying/txn guards needed — reads never
   mutate, and an empty/txn-open journal is handled by the n==0 / nops==0 early returns). */
int  flow_undo_depth(flow_t *f) { return f->journal.n; }
int  flow_redo_depth(flow_t *f) { return f->journal.rn; }
int  flow_top_op(flow_t *f) {
  if (f->journal.n == 0) return -1;
  struct flow__cmd *c = &f->journal.items[f->journal.n - 1];
  return c->nops > 0 ? (int)c->ops[c->nops - 1].kind : -1;
}
void flow_undo(flow_t *f) {
  if (f->journal.n <= 0 || f->journal.applying || f->journal.txn_depth > 0) return; /* no mid-gesture undo */
  struct flow__cmd c = f->journal.items[--f->journal.n];
  f->journal.applying = 1; f->cb_suppress++;
  for (int i = c.nops - 1; i >= 0; i--) flow__apply_op(f, &c.ops[i], 0);
  f->cb_suppress--; f->journal.applying = 0;
  struct flow__cmd *redo = (struct flow__cmd*)flow__grow(f->journal.redo, &f->journal.rcap,
                                                         f->journal.rn + 1, sizeof *f->journal.redo);
  if (!redo) { flow__cmd_free(&c); return; }     /* OOM: inverse applied; drop the redo entry (no crash, no leak) */
  f->journal.redo = redo;
  f->journal.redo[f->journal.rn++] = c;
}
void flow_redo(flow_t *f) {
  if (f->journal.rn <= 0 || f->journal.applying || f->journal.txn_depth > 0) return;
  struct flow__cmd c = f->journal.redo[--f->journal.rn];
  f->journal.applying = 1; f->cb_suppress++;
  for (int i = 0; i < c.nops; i++) flow__apply_op(f, &c.ops[i], 1);
  f->cb_suppress--; f->journal.applying = 0;
  /* redo is NOT a new mutation: push straight back (no eviction needed — record clears
     redo first, so n + rn never exceeds the cap), redo stack preserved for chains. */
  struct flow__cmd *items = (struct flow__cmd*)flow__grow(f->journal.items, &f->journal.cap,
                                                          f->journal.n + 1, sizeof *f->journal.items);
  if (!items) { flow__cmd_free(&c); return; }    /* OOM: redo applied; drop the undo entry (no crash, no leak) */
  f->journal.items = items;
  f->journal.items[f->journal.n++] = c;
}
void flow_set_undo_limit(flow_t *f, int max_commands) {
  if (max_commands < 0) max_commands = 0;             /* floor clamp */
  f->journal.limit = max_commands;
  if (max_commands == 0) { flow__journal_clear(f); return; }  /* 0 disables journaling entirely */
  while (f->journal.n > max_commands) {               /* evict oldest down to the new cap */
    flow__cmd_free(&f->journal.items[0]);
    memmove(&f->journal.items[0], &f->journal.items[1],
            (size_t)(f->journal.n - 1) * sizeof *f->journal.items);
    f->journal.n--;
    if (f->journal.txn_base > 0) f->journal.txn_base--;      /* keep an OPEN gesture txn aimed right */
    else if (f->journal.txn_base == 0) f->journal.txn_base = -1;  /* its command was evicted: reopen lazily */
  }
}
#endif
