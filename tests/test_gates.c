#define FLOW_IMPLEMENTATION
#include "../flow.h"
#include "flowtest.h"
#include <string.h>

/* inc-8 #1 — per-element interaction gates: FLOW_NODRAG / FLOW_NOSELECT /
   FLOW_NODELETE (xyflow draggable/selectable/deletable=false). Negative-polarity
   bits in the free 64/128/256 range: zero-init == permissive xyflow default.
   The gates veto USER INTERACTION only — programmatic move/select/remove stay
   unconditional. Persisted (named bools emitted only when set); undo-durable
   (outside the flow_undo transient-clear mask); setters not journaled. */

static void feed(flow_t *f, const char *s) { flow_feed(f, s, (int)strlen(s)); }
static void press_at(flow_t *f, int sx, int sy)   { char b[32]; snprintf(b, sizeof b, "\x1b[<0;%d;%dM",  sx + 1, sy + 1); feed(f, b); }
static void move_to(flow_t *f, int sx, int sy)    { char b[32]; snprintf(b, sizeof b, "\x1b[<32;%d;%dM", sx + 1, sy + 1); feed(f, b); }
static void release_at(flow_t *f, int sx, int sy) { char b[32]; snprintf(b, sizeof b, "\x1b[<0;%d;%dm",  sx + 1, sy + 1); feed(f, b); }

/* fixed-size default node at world (x,y); zoom 1, no pan => screen == world */
static int add_node(flow_t *f, int x, int y, const char *label, int w, int h) {
  int id = flow_add_node(f, "default", (flow_pt){x, y}, (void*)label);
  flow_node *n = flow_get_node(f, id); n->w = w; n->h = h;
  return id;
}

static char *slurp(const char *path) {
  FILE *fp = fopen(path, "rb"); if (!fp) return NULL;
  fseek(fp, 0, SEEK_END); long n = ftell(fp); fseek(fp, 0, SEEK_SET);
  char *buf = (char*)malloc((size_t)n + 1); size_t got = fread(buf, 1, (size_t)n, fp); buf[got] = 0;
  fclose(fp); return buf;
}

int main(void) {
  /* ===== NODELETE ===== */

  /* 1 (HEADLINE, run FIRST): a protected node is skipped by delete-selection and
     the call RETURNS — proving the :1225 re-query loop terminates. A hang here
     prints no report line, so the failure mode is unambiguous. */
  {
    flow_t *f = flow_new(80, 30); flow_register_defaults(f);
    int P = add_node(f, 10, 5, "P", 4, 3);
    int Q = add_node(f, 30, 5, "Q", 4, 3);
    flow_set_node_deletable(f, P, 0);            /* protect P */
    flow_select_node(f, P, 0); flow_select_node(f, Q, 1);
    flow_delete_selection(f);                    /* MUST return (no infinite loop) */
    ASSERT(flow_get_node(f, P) != NULL, "NODELETE: protected P survives delete-selection");
    ASSERT(flow_get_node(f, Q) == NULL, "NODELETE: unprotected Q deleted");
    ASSERT_INT(flow_node_count(f), 1, "NODELETE: exactly one node remains");
    /* INTENTIONAL: the survivor is left DESELECTED. The pre-deselect pass that terminates the
       re-query loop clears FLOW_SELECTED on protected nodes; we accept that over xyflow's
       selection-retention because selection is transient/un-journaled. Pinned so it stays a
       decision, not a drift. */
    ASSERT(!(flow_get_node(f, P)->flags & FLOW_SELECTED), "NODELETE: protected survivor is left deselected (loop-termination tradeoff)");
    flow_free(f);
  }

  /* 2: all-selected-protected — the loop must break immediately, nothing removed. */
  {
    flow_t *f = flow_new(80, 30); flow_register_defaults(f);
    int P = add_node(f, 10, 5, "P", 4, 3);
    int R = add_node(f, 30, 5, "R", 4, 3);
    flow_set_node_deletable(f, P, 0); flow_set_node_deletable(f, R, 0);
    flow_select_node(f, P, 0); flow_select_node(f, R, 1);
    flow_delete_selection(f);                    /* all protected => immediate break */
    ASSERT_INT(flow_node_count(f), 2, "NODELETE: all-protected selection deletes nothing, no hang");
    flow_free(f);
  }

  /* 3: a protected DESCENDANT of a deleted root is still cascade-removed (xyflow
     parity: deletability gates the acted-on root, not cascade victims). */
  {
    flow_t *f = flow_new(80, 30); flow_register_defaults(f);
    int G = add_node(f, 10, 5, "G", 4, 3);
    int C = add_node(f, 12, 7, "C", 4, 3);
    flow_set_parent(f, C, G);
    flow_set_node_deletable(f, C, 0);            /* protected child */
    flow_select_node(f, G, 0);                   /* delete the parent root only */
    flow_delete_selection(f);
    ASSERT(flow_get_node(f, G) == NULL, "NODELETE: deleted parent root gone");
    ASSERT(flow_get_node(f, C) == NULL, "NODELETE: protected descendant cascade-removed with parent");
    flow_free(f);
  }

  /* 4: direct flow_remove_node is the imperative primitive — ignores deletable. */
  {
    flow_t *f = flow_new(80, 30); flow_register_defaults(f);
    int P = add_node(f, 10, 5, "P", 4, 3);
    flow_set_node_deletable(f, P, 0);
    flow_remove_node(f, P);
    ASSERT(flow_get_node(f, P) == NULL, "NODELETE: direct flow_remove_node ignores the gate");
    flow_free(f);
  }

  /* 5: the gate bit survives undo (lives outside flow_undo's transient-clear mask). */
  {
    flow_t *f = flow_new(80, 30); flow_register_defaults(f);
    int P = add_node(f, 10, 5, "P", 4, 3);
    flow_set_node_deletable(f, P, 0);
    flow_remove_node(f, P);                       /* recorded mutator */
    flow_undo(f);
    flow_node *p = flow_get_node(f, P);
    ASSERT(p != NULL, "NODELETE: undo restores the removed node");
    ASSERT(p && (p->flags & FLOW_NODELETE), "NODELETE: restored node keeps its gate bit through undo");
    flow_free(f);
  }

  /* ===== NODRAG ===== */

  /* 6: a NODRAG node is still selectable by a plain (no-move) click. */
  {
    flow_t *f = flow_new(80, 30); flow_register_defaults(f);
    int A = add_node(f, 10, 5, "A", 4, 3);
    flow_set_node_draggable(f, A, 0);
    press_at(f, 11, 6); release_at(f, 11, 6);
    ASSERT(flow_get_node(f, A)->flags & FLOW_SELECTED, "NODRAG: node still selectable by click");
    ASSERT_INT(flow_selected_node(f), A, "NODRAG: A is the selection");
    flow_free(f);
  }

  /* 7: dragging a NODRAG node moves it nowhere (drag never arms). */
  {
    flow_t *f = flow_new(80, 30); flow_register_defaults(f);
    int A = add_node(f, 10, 5, "A", 4, 3);
    flow_set_node_draggable(f, A, 0);
    press_at(f, 11, 6); move_to(f, 16, 6); release_at(f, 16, 6);   /* attempt drag +5 */
    flow_node *a = flow_get_node(f, A);
    ASSERT_INT(a->pos.x, 10, "NODRAG: x unchanged after drag attempt");
    ASSERT_INT(a->pos.y, 5,  "NODRAG: y unchanged after drag attempt");
    flow_free(f);
  }

  /* 8: in a multi-selection drag, a NODRAG sibling is held fixed while peers move. */
  {
    flow_t *f = flow_new(80, 30); flow_register_defaults(f);
    int A = add_node(f, 10, 5, "A", 4, 3);
    int B = add_node(f, 30, 5, "B", 4, 3);
    flow_set_node_draggable(f, A, 0);
    flow_select_node(f, A, 0); flow_select_node(f, B, 1);          /* both selected */
    press_at(f, 31, 6); move_to(f, 35, 6); release_at(f, 35, 6);  /* drag B by +4 */
    ASSERT_INT(flow_get_node(f, B)->pos.x, 34, "NODRAG: draggable B moves by +4");
    ASSERT_INT(flow_get_node(f, A)->pos.x, 10, "NODRAG: protected sibling A held fixed");
    flow_free(f);
  }

  /* 9: programmatic flow_move_node ignores NODRAG (layout/center/undo path unaffected). */
  {
    flow_t *f = flow_new(80, 30); flow_register_defaults(f);
    int A = add_node(f, 10, 5, "A", 4, 3);
    flow_set_node_draggable(f, A, 0);
    flow_move_node(f, A, (flow_pt){20, 9});
    flow_node *a = flow_get_node(f, A);
    ASSERT_INT(a->pos.x, 20, "NODRAG: programmatic move ignores the gate (x)");
    ASSERT_INT(a->pos.y, 9,  "NODRAG: programmatic move ignores the gate (y)");
    flow_free(f);
  }

  /* 10: shift-arrow nudge skips a NODRAG selection root (keyboard parity). */
  {
    flow_t *f = flow_new(80, 30); flow_register_defaults(f);
    int A = add_node(f, 10, 5, "A", 4, 3);
    int B = add_node(f, 30, 5, "B", 4, 3);
    flow_set_node_draggable(f, A, 0);
    flow_select_node(f, A, 0); flow_select_node(f, B, 1);
    flow__nudge_selection(f, 1, 0);                               /* shift-right nudge */
    ASSERT_INT(flow_get_node(f, A)->pos.x, 10, "NODRAG: nudge skips the protected root");
    ASSERT_INT(flow_get_node(f, B)->pos.x, 31, "NODRAG: nudge still moves the free root");
    flow_free(f);
  }

  /* ===== NOSELECT ===== */

  /* 11: a NOSELECT node is not selected by a plain click, and the click does NOT
     fall through to clear the existing selection. */
  {
    flow_t *f = flow_new(80, 30); flow_register_defaults(f);
    int A = add_node(f, 10, 5, "A", 4, 3);
    int B = add_node(f, 30, 5, "B", 4, 3);
    flow_set_node_selectable(f, A, 0);
    flow_select_node(f, B, 0);                                    /* B pre-selected */
    press_at(f, 11, 6); release_at(f, 11, 6);
    ASSERT(!(flow_get_node(f, A)->flags & FLOW_SELECTED), "NOSELECT: A not selected by click");
    ASSERT(flow_get_node(f, B)->flags & FLOW_SELECTED, "NOSELECT: existing selection preserved (no fall-through clear)");
    flow_free(f);
  }

  /* 12: modifier (shift/ctrl) clicks on a NOSELECT node select nothing. */
  {
    flow_t *f = flow_new(80, 30); flow_register_defaults(f);
    int A = add_node(f, 10, 5, "A", 4, 3); (void)A;
    flow_set_node_selectable(f, A, 0);
    feed(f, "\x1b[<4;12;7M");  feed(f, "\x1b[<4;12;7m");          /* shift-click A */
    ASSERT_INT(flow_selected_count(f), 0, "NOSELECT: shift-click selects nothing");
    feed(f, "\x1b[<16;12;7M"); feed(f, "\x1b[<16;12;7m");         /* ctrl-click A */
    ASSERT_INT(flow_selected_count(f), 0, "NOSELECT: ctrl-click selects nothing");
    flow_free(f);
  }

  /* 13: marquee (flow_select_in_rect) excludes NOSELECT nodes. */
  {
    flow_t *f = flow_new(80, 30); flow_register_defaults(f);
    int A = add_node(f, 10, 5, "A", 4, 3);
    int B = add_node(f, 16, 5, "B", 4, 3);
    flow_set_node_selectable(f, A, 0);
    int n = flow_select_in_rect(f, (flow_rect){8, 3, 20, 8}, FLOW_SELECT_PARTIAL, 0);
    ASSERT_INT(n, 1, "NOSELECT: marquee selects only the selectable node");
    ASSERT(!(flow_get_node(f, A)->flags & FLOW_SELECTED), "NOSELECT: A excluded from marquee");
    ASSERT(flow_get_node(f, B)->flags & FLOW_SELECTED, "NOSELECT: free B selected by marquee");
    flow_free(f);
  }

  /* 14: the public flow_select_node primitive stays UNGATED (paste/host escape hatch). */
  {
    flow_t *f = flow_new(80, 30); flow_register_defaults(f);
    int A = add_node(f, 10, 5, "A", 4, 3);
    flow_set_node_selectable(f, A, 0);
    flow_select_node(f, A, 0);
    ASSERT(flow_get_node(f, A)->flags & FLOW_SELECTED, "NOSELECT: public flow_select_node ignores the gate");
    flow_free(f);
  }

  /* 15: Enter focus-select skips a NOSELECT focused node (keyboard parity). */
  {
    flow_t *f = flow_new(80, 30); flow_register_defaults(f);
    int A = add_node(f, 10, 5, "A", 4, 3);
    flow_set_node_selectable(f, A, 0);
    flow_set_focus(f, A);
    feed(f, "\r");                                               /* Enter: select focus */
    ASSERT(!(flow_get_node(f, A)->flags & FLOW_SELECTED), "NOSELECT: Enter does not select the focused gated node");
    flow_free(f);
  }

  /* 16: selectability is orthogonal to hover (handles stay reachable to connect). */
  {
    flow_t *f = flow_new(80, 30); flow_register_defaults(f);
    int A = add_node(f, 10, 5, "A", 4, 3);
    flow_set_node_selectable(f, A, 0);
    flow_set_hover(f, A);
    ASSERT_INT(flow_hovered_node(f), A, "NOSELECT: node still hoverable");
    flow_free(f);
  }

  /* ===== persistence ===== */

  /* 17: gates round-trip through save -> load -> save, byte-identical. */
  {
    flow_t *f = flow_new(80, 30); flow_register_defaults(f);
    int A = add_node(f, 10, 5, "A", 4, 3);
    int B = add_node(f, 30, 5, "B", 4, 3);
    int C = add_node(f, 50, 5, "C", 4, 3);
    flow_set_node_draggable(f, A, 0);
    flow_set_node_selectable(f, B, 0);
    flow_set_node_deletable(f, C, 0);
    ASSERT_INT(flow_save(f, "/tmp/flow_gates_a.json"), 0, "persist: save ok");
    flow_t *g = flow_new(80, 30); flow_register_defaults(g);
    ASSERT_INT(flow_load(g, "/tmp/flow_gates_a.json"), 0, "persist: load ok");
    ASSERT(flow_get_node(g, A)->flags & FLOW_NODRAG,   "persist: NODRAG round-trips");
    ASSERT(flow_get_node(g, B)->flags & FLOW_NOSELECT, "persist: NOSELECT round-trips");
    ASSERT(flow_get_node(g, C)->flags & FLOW_NODELETE, "persist: NODELETE round-trips");
    ASSERT_INT(flow_save(g, "/tmp/flow_gates_b.json"), 0, "persist: re-save ok");
    char *sa = slurp("/tmp/flow_gates_a.json"), *sb = slurp("/tmp/flow_gates_b.json");
    ASSERT(sa && sb, "persist: both files readable");
    if (sa && sb) ASSERT_STR(sa, sb, "persist: save->load->save byte-identical");
    free(sa); free(sb); flow_free(f); flow_free(g);
  }

  /* 18: absent gate keys parse permissive (a default node engages no bit). */
  {
    flow_t *f = flow_new(80, 30); flow_register_defaults(f);
    int A = add_node(f, 10, 5, "A", 4, 3);
    flow_save(f, "/tmp/flow_gates_plain.json");
    char *s = slurp("/tmp/flow_gates_plain.json");
    ASSERT(s && strstr(s, "draggable")  == NULL, "persist: default node emits no draggable key");
    ASSERT(s && strstr(s, "selectable") == NULL, "persist: default node emits no selectable key");
    ASSERT(s && strstr(s, "deletable")  == NULL, "persist: default node emits no deletable key");
    free(s);
    flow_t *g = flow_new(80, 30); flow_register_defaults(g);
    flow_load(g, "/tmp/flow_gates_plain.json");
    flow_node *a = flow_get_node(g, A);
    ASSERT(a && !(a->flags & (FLOW_NODRAG | FLOW_NOSELECT | FLOW_NODELETE)), "persist: absent keys => permissive");
    flow_free(f); flow_free(g);
  }

  /* ===== NOSELECT-but-draggable (selectable=false, draggable=true): the dragged node
     is never added to the selection, so the drag path must key off drag_node, NOT the
     selection count (regression guards for the inc-8 #1 review findings). ===== */

  /* 19: a selectable=false node with an EMPTY selection still drags itself (single path). */
  {
    flow_t *f = flow_new(80, 30); flow_register_defaults(f);
    int N = add_node(f, 10, 5, "N", 4, 3);
    flow_set_node_selectable(f, N, 0);                 /* draggable stays true */
    press_at(f, 11, 6); move_to(f, 15, 6); release_at(f, 15, 6);
    ASSERT_INT(flow_get_node(f, N)->pos.x, 14, "NOSELECT+draggable: solo drag moves the node itself");
    ASSERT_INT(flow_selected_count(f), 0, "NOSELECT+draggable: dragging it selects nothing");
    flow_free(f);
  }

  /* 20: grabbing a selectable=false node amid a stale multi-selection must drag THAT node,
     not the unrelated selected set (HIGH finding). */
  {
    flow_t *f = flow_new(80, 30); flow_register_defaults(f);
    int A = add_node(f, 10, 5, "A", 4, 3);
    int B = add_node(f, 16, 5, "B", 4, 3);
    int N = add_node(f, 40, 5, "N", 4, 3);
    flow_set_node_selectable(f, N, 0);
    flow_select_node(f, A, 0); flow_select_node(f, B, 1);          /* stale multi-selection {A,B} */
    press_at(f, 41, 6); move_to(f, 45, 6); release_at(f, 45, 6);  /* grab+drag N by +4 */
    ASSERT_INT(flow_get_node(f, N)->pos.x, 44, "NOSELECT+draggable: the grabbed node moves");
    ASSERT_INT(flow_get_node(f, A)->pos.x, 10, "NOSELECT+draggable: unrelated selection A NOT dragged");
    ASSERT_INT(flow_get_node(f, B)->pos.x, 16, "NOSELECT+draggable: unrelated selection B NOT dragged");
    flow_free(f);
  }

  /* 21: a selectable=false node still drag-to-reparents on drop (MEDIUM finding). */
  {
    flow_t *f = flow_new(80, 30); flow_register_defaults(f);
    int G = flow_add_node(f, "group", (flow_pt){10, 5}, NULL);
    flow_node *gn = flow_get_node(f, G); gn->w = 30; gn->h = 15;   /* footprint x[10..39] y[5..19] */
    int N = add_node(f, 50, 5, "N", 4, 3);
    flow_set_node_selectable(f, N, 0);
    ASSERT_INT(flow_get_node(f, N)->parent, -1, "precondition: N starts top-level");
    press_at(f, 51, 6); move_to(f, 25, 12); release_at(f, 25, 12); /* drop N inside G */
    ASSERT_INT(flow_get_node(f, N)->parent, G, "NOSELECT+draggable: drag-to-reparent still works");
    flow_free(f);
  }

  return flowtest_report("test_gates");
}
