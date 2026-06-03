#define FLOW_IMPLEMENTATION
#include "../flow.h"
#include "flowtest.h"

/* render a cell buffer to a newline-joined ASCII/UTF-8 string (right-trimmed per row),
   matching the convention used by test_render.c's snapshots. */
static char *cells_to_string(const flow_cell *c, int cols, int rows) {
  char *s = (char*)malloc((size_t)cols * rows * 4 + rows + 1); int len = 0; char u[5];
  for (int y = 0; y < rows; y++) {
    int last = -1;
    for (int x = 0; x < cols; x++) { uint32_t ch = c[y*cols+x].ch; if (ch && ch != ' ') last = x; }
    for (int x = 0; x <= last; x++) { uint32_t ch = c[y*cols+x].ch; if (!ch) ch = ' ';
      int n = flow_utf8(ch, u); memcpy(s+len, u, n); len += n; }
    s[len++] = '\n';
  }
  s[len] = 0; return s;
}

int main(void) {
  /* ---- flow_move_node ABSOLUTE-in invariant: TOP-LEVEL ---- */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    int a = flow_add_node(f, "default", (flow_pt){5, 5}, (void*)"A");
    flow_move_node(f, a, (flow_pt){20, 9});
    flow_node *n = flow_get_node(f, a);
    flow_pt abs = flow_node_abs(f, n);
    ASSERT(abs.x == 20 && abs.y == 9, "top-level move: abs == target");
    ASSERT(n->pos.x == 20 && n->pos.y == 9, "top-level move: pos == target (byte-identical to pre-groups)");
    ASSERT(flow_node_pos(n).x == 20 && flow_node_pos(n).y == 9, "flow_node_pos returns stored rel pos");
    flow_free(f);
  }

  /* ---- flow_move_node ABSOLUTE-in invariant: CHILD under a parent at abs G ---- */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    int p = flow_add_node(f, "default", (flow_pt){10, 4}, (void*)"P");   /* parent abs G=(10,4) */
    int c = flow_add_node(f, "default", (flow_pt){0, 0}, (void*)"C");
    flow_set_parent(f, c, p);
    flow_move_node(f, c, (flow_pt){25, 11});                             /* absolute target */
    flow_node *cn = flow_get_node(f, c);
    flow_pt abs = flow_node_abs(f, cn);
    ASSERT(abs.x == 25 && abs.y == 11, "child move: abs == absolute target");
    ASSERT(cn->pos.x == 15 && cn->pos.y == 7, "child move: pos == target - parent_abs (15,7)");
    flow_free(f);
  }

  /* ---- flow_set_parent preserves abs; detach restores abs as pos ---- */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    int g = flow_add_node(f, "default", (flow_pt){8, 6}, (void*)"G");    /* "parent" at world (8,6) */
    int n = flow_add_node(f, "default", (flow_pt){20, 14}, (void*)"N");  /* world W=(20,14) */
    flow_set_parent(f, n, g);
    flow_node *nn = flow_get_node(f, n);
    flow_pt abs = flow_node_abs(f, nn);
    ASSERT(abs.x == 20 && abs.y == 14, "set_parent: abs unchanged (W)");
    ASSERT(nn->pos.x == 12 && nn->pos.y == 8, "set_parent: pos == W - G (12,8)");
    flow_set_parent(f, n, -1);                                           /* detach */
    nn = flow_get_node(f, n);
    abs = flow_node_abs(f, nn);
    ASSERT(abs.x == 20 && abs.y == 14, "detach: abs still W");
    ASSERT(nn->pos.x == 20 && nn->pos.y == 14, "detach: pos == W");
    flow_free(f);
  }

  /* ---- cycle guard: self-parent, and reparent an ancestor under its descendant ---- */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    int a = flow_add_node(f, "default", (flow_pt){0, 0}, (void*)"a");
    int b = flow_add_node(f, "default", (flow_pt){5, 0}, (void*)"b");
    int c = flow_add_node(f, "default", (flow_pt){10, 0}, (void*)"c");
    flow_set_parent(f, b, a);                          /* a -> b */
    flow_set_parent(f, c, b);                          /* b -> c, chain a>b>c */
    flow_set_parent(f, a, a);                          /* self-parent: no-op */
    ASSERT_INT(flow_get_node(f, a)->parent, -1, "self-parent rejected (a still top-level)");
    flow_set_parent(f, a, c);                          /* a under its own descendant c: cycle, no-op */
    ASSERT_INT(flow_get_node(f, a)->parent, -1, "ancestor-under-descendant rejected (chain intact)");
    ASSERT_INT(flow_get_node(f, b)->parent, a, "chain intact: b->a");
    ASSERT_INT(flow_get_node(f, c)->parent, b, "chain intact: c->b");
    ASSERT_INT(flow_is_ancestor(f, a, c), 1, "flow_is_ancestor(a,c) true");
    ASSERT_INT(flow_is_ancestor(f, c, a), 0, "flow_is_ancestor(c,a) false");
    ASSERT_INT(flow_is_ancestor(f, a, a), 1, "flow_is_ancestor(a,a) true (self)");
    flow_free(f);
  }

  /* ---- flow_group: encloses members, reparents, preserves abs, +1 node ---- */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    int a = flow_add_node(f, "default", (flow_pt){10, 5}, (void*)"A");   /* (10,5,5,3) */
    int b = flow_add_node(f, "default", (flow_pt){30, 5}, (void*)"B");   /* (30,5,5,3) */
    int c = flow_add_node(f, "default", (flow_pt){12, 12}, (void*)"C");  /* (12,12,5,3) */
    flow_pt aa = flow_node_abs(f, flow_get_node(f, a));
    flow_pt ba = flow_node_abs(f, flow_get_node(f, b));
    flow_pt ca = flow_node_abs(f, flow_get_node(f, c));
    int before = flow_node_count(f);
    int ids[3] = { a, b, c };
    int gid = flow_group(f, ids, 3);
    ASSERT(gid > 0, "flow_group returns a valid id");
    ASSERT_INT(flow_node_count(f), before + 1, "flow_group adds exactly 1 node");
    ASSERT_INT(flow_get_node(f, a)->parent, gid, "A reparented under group");
    ASSERT_INT(flow_get_node(f, b)->parent, gid, "B reparented under group");
    ASSERT_INT(flow_get_node(f, c)->parent, gid, "C reparented under group");
    flow_pt aa2 = flow_node_abs(f, flow_get_node(f, a));
    flow_pt ba2 = flow_node_abs(f, flow_get_node(f, b));
    flow_pt ca2 = flow_node_abs(f, flow_get_node(f, c));
    ASSERT(aa.x==aa2.x && aa.y==aa2.y, "A abs unchanged after group");
    ASSERT(ba.x==ba2.x && ba.y==ba2.y, "B abs unchanged after group");
    ASSERT(ca.x==ca2.x && ca.y==ca2.y, "C abs unchanged after group");
    /* bbox of members: x 10..35 (35 = 30+5), y 5..15 (15 = 12+3) -> w=25,h=10; +pad 1 each side */
    flow_node *g = flow_get_node(f, gid);
    flow_pt gabs = flow_node_abs(f, g);
    ASSERT(gabs.x == 9 && gabs.y == 4, "group top-left = bbox - pad (9,4)");
    ASSERT(g->w == 27 && g->h == 12, "group w/h enclose bbox + 2*pad (27x12)");
    /* group frame must enclose all member abs rects */
    flow_rect gr = flow_node_rect_abs(f, g);
    ASSERT(gr.x <= 10 && gr.y <= 5 && gr.x+gr.w >= 35 && gr.y+gr.h >= 15, "frame encloses member bbox");
    /* measure no-op: re-measuring the group must NOT clobber caller-set w/h */
    flow_measure_node(f, g);
    ASSERT(g->w == 27 && g->h == 12, "flow__group_measure write-back preserves w/h");
    flow_free(f);
  }

  /* ---- flow_ungroup SURVIVAL vs flow_remove_node CASCADE ---- */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    int a = flow_add_node(f, "default", (flow_pt){10, 5}, (void*)"A");
    int b = flow_add_node(f, "default", (flow_pt){20, 5}, (void*)"B");
    flow_pt aa = flow_node_abs(f, flow_get_node(f, a));
    flow_pt ba = flow_node_abs(f, flow_get_node(f, b));
    int ids[2] = { a, b };
    int gid = flow_group(f, ids, 2);
    int after_group = flow_node_count(f);     /* 3: A,B,group */
    flow_ungroup(f, gid);
    ASSERT_INT(flow_node_count(f), after_group - 1, "ungroup removes exactly 1 (the container)");
    ASSERT(flow_get_node(f, a) != NULL, "ungroup: A survives");
    ASSERT(flow_get_node(f, b) != NULL, "ungroup: B survives");
    ASSERT(flow_get_node(f, gid) == NULL, "ungroup: group node gone");
    ASSERT_INT(flow_get_node(f, a)->parent, -1, "ungroup: A reparented to group's old parent (-1)");
    ASSERT_INT(flow_get_node(f, b)->parent, -1, "ungroup: B reparented to group's old parent (-1)");
    flow_pt aa2 = flow_node_abs(f, flow_get_node(f, a));
    flow_pt ba2 = flow_node_abs(f, flow_get_node(f, b));
    ASSERT(aa.x==aa2.x && aa.y==aa2.y, "ungroup: A abs unchanged");
    ASSERT(ba.x==ba2.x && ba.y==ba2.y, "ungroup: B abs unchanged");
    flow_free(f);

    /* CONTRAST control: flow_remove_node on a group cascade-deletes its children */
    flow_t *g = flow_new(80, 24); flow_register_defaults(g);
    int a2 = flow_add_node(g, "default", (flow_pt){10, 5}, (void*)"A");
    int b2 = flow_add_node(g, "default", (flow_pt){20, 5}, (void*)"B");
    int ids2[2] = { a2, b2 };
    int gid2 = flow_group(g, ids2, 2);
    int before_rm = flow_node_count(g);       /* 3 */
    flow_remove_node(g, gid2);
    ASSERT_INT(flow_node_count(g), before_rm - 3, "remove_node cascade deletes group + 2 children");
    ASSERT(flow_get_node(g, a2) == NULL, "remove_node: child A deleted");
    ASSERT(flow_get_node(g, b2) == NULL, "remove_node: child B deleted");
    flow_free(g);
  }

  /* ---- flow_node_abs nesting depth: a>b>c, abs(c) == sum of rel positions ---- */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    int a = flow_add_node(f, "default", (flow_pt){3, 2}, (void*)"a");
    int b = flow_add_node(f, "default", (flow_pt){0, 0}, (void*)"b");
    int c = flow_add_node(f, "default", (flow_pt){0, 0}, (void*)"c");
    flow_set_parent(f, b, a); flow_set_parent(f, c, b);
    /* set explicit relative positions */
    flow_get_node(f, a)->pos = (flow_pt){3, 2};
    flow_get_node(f, b)->pos = (flow_pt){4, 5};
    flow_get_node(f, c)->pos = (flow_pt){6, 7};
    flow_pt abs = flow_node_abs(f, flow_get_node(f, c));
    ASSERT(abs.x == 3+4+6 && abs.y == 2+5+7, "abs(c) == sum of three relative positions (13,14)");
    flow_free(f);
  }

  /* ---- hit-test ordering: child inside group returns CHILD; frame-only cell returns GROUP ---- */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);  /* zoom 1, no pan: screen==world */
    int child = flow_add_node(f, "default", (flow_pt){10, 5}, (void*)"X");  /* (10,5,5,3) */
    int ids[1] = { child };
    int gid = flow_group(f, ids, 1);   /* group frame: (9,4,7,5) -> x9..15 y4..8 */
    /* a cell inside the child (e.g. its center 12,6) must return the CHILD, not the group */
    ASSERT_INT(flow_hit_node(f, (flow_pt){12, 6}), child, "hit inside child returns child (not group behind)");
    /* a cell on the group frame but outside the child (9,4 top-left corner) returns the group */
    ASSERT_INT(flow_hit_node(f, (flow_pt){9, 4}), gid, "hit on group frame (no child) returns group");
    flow_free(f);
  }

  /* ---- multi-drag selection-roots: parent group + its child both selected ---- */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    int child = flow_add_node(f, "default", (flow_pt){12, 7}, (void*)"X");
    int loose = flow_add_node(f, "default", (flow_pt){40, 7}, (void*)"L");  /* unrelated top-level */
    int ids[1] = { child };
    int gid = flow_group(f, ids, 1);   /* group at (11,6), child rel (1,1) */
    flow_pt child_abs0 = flow_node_abs(f, flow_get_node(f, child));
    flow_pt loose_abs0 = flow_node_abs(f, flow_get_node(f, loose));
    flow_pt group_abs0 = flow_node_abs(f, flow_get_node(f, gid));
    /* select group + child + loose (a 3-set; group is the root of {group,child}) */
    flow_select_node(f, gid, 0);
    flow_select_node(f, child, 1);
    flow_select_node(f, loose, 1);
    ASSERT_INT(flow_selected_count(f), 3, "multi-drag setup: 3 selected");
    /* press inside the group frame (a group-frame cell, not the child) so drag_node==group,
       then motion by +6/+4 world, then release. group is selected so the set is kept. */
    flow_pt gp = group_abs0;                      /* group top-left corner cell (frame only) */
    char buf[32];
    snprintf(buf, sizeof buf, "\x1b[<0;%d;%dM", gp.x + 1, gp.y + 1);   /* press (SGR 1-based) */
    flow_feed(f, buf, (int)strlen(buf));
    snprintf(buf, sizeof buf, "\x1b[<32;%d;%dM", gp.x + 6 + 1, gp.y + 4 + 1);  /* motion +6,+4 */
    flow_feed(f, buf, (int)strlen(buf));
    snprintf(buf, sizeof buf, "\x1b[<0;%d;%dm", gp.x + 6 + 1, gp.y + 4 + 1);   /* release */
    flow_feed(f, buf, (int)strlen(buf));
    flow_pt child_abs1 = flow_node_abs(f, flow_get_node(f, child));
    flow_pt loose_abs1 = flow_node_abs(f, flow_get_node(f, loose));
    flow_pt group_abs1 = flow_node_abs(f, flow_get_node(f, gid));
    ASSERT(group_abs1.x == group_abs0.x + 6 && group_abs1.y == group_abs0.y + 4, "group root moved by delta");
    ASSERT(child_abs1.x == child_abs0.x + 6 && child_abs1.y == child_abs0.y + 4,
           "child moved by EXACTLY delta (no double-move via parent+self)");
    ASSERT(loose_abs1.x == loose_abs0.x + 6 && loose_abs1.y == loose_abs0.y + 4,
           "unrelated selected top-level node also moved by delta");
    flow_free(f);
  }

  /* ---- drag-to-reparent: drag a loose node onto a group nests it (abs preserved) ---- */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    int member = flow_add_node(f, "default", (flow_pt){10, 5}, (void*)"M");
    int ids[1] = { member };
    int gid = flow_group(f, ids, 1);   /* group frame around (10,5,5,3): (9,4,7,5) x9..15 y4..8 */
    int loose = flow_add_node(f, "default", (flow_pt){40, 14}, (void*)"L");
    ASSERT_INT(flow_get_node(f, loose)->parent, -1, "loose starts top-level");
    /* drag loose so its body lands ON the group frame. press inside loose, drag to a group cell. */
    char buf[32];
    flow_pt la = flow_node_abs(f, flow_get_node(f, loose));   /* (40,14) */
    snprintf(buf, sizeof buf, "\x1b[<0;%d;%dM", la.x + 1 + 1, la.y + 1 + 1);  /* press inside loose body */
    flow_feed(f, buf, (int)strlen(buf));
    /* drag so the cursor (and node) lands inside the group frame interior (e.g. 11,6) */
    snprintf(buf, sizeof buf, "\x1b[<32;%d;%dM", 11 + 1, 6 + 1);   /* motion to group cell */
    flow_feed(f, buf, (int)strlen(buf));
    snprintf(buf, sizeof buf, "\x1b[<0;%d;%dm", 11 + 1, 6 + 1);    /* release on group */
    flow_feed(f, buf, (int)strlen(buf));
    ASSERT_INT(flow_get_node(f, loose)->parent, gid, "drag-to-reparent: loose nested under group on drop");
    flow_free(f);

    /* detach: drag a parented node out to empty pane -> back to top level */
    flow_t *g = flow_new(80, 24); flow_register_defaults(g);
    int m2 = flow_add_node(g, "default", (flow_pt){10, 5}, (void*)"M");
    int ids2[1] = { m2 };
    int gid2 = flow_group(g, ids2, 1);
    ASSERT_INT(flow_get_node(g, m2)->parent, gid2, "m2 starts parented");
    char b2[32];
    flow_pt ma = flow_node_abs(g, flow_get_node(g, m2));   /* (10,5) */
    snprintf(b2, sizeof b2, "\x1b[<0;%d;%dM", ma.x + 1 + 1, ma.y + 1 + 1);  /* press inside m2 */
    flow_feed(g, b2, (int)strlen(b2));
    snprintf(b2, sizeof b2, "\x1b[<32;%d;%dM", 50 + 1, 18 + 1);  /* drag far to empty pane */
    flow_feed(g, b2, (int)strlen(b2));
    snprintf(b2, sizeof b2, "\x1b[<0;%d;%dm", 50 + 1, 18 + 1);   /* release on empty */
    flow_feed(g, b2, (int)strlen(b2));
    ASSERT_INT(flow_get_node(g, m2)->parent, -1, "drag-to-empty-pane detaches a parented node");
    flow_free(g);
  }

  /* ---- SNAPSHOT 1: group framing two children; one child overflows the frame (clipped),
          and the frame draws UNDER the children (children overdraw the frame border). ---- */
  {
    int cols = 24, rows = 10;
    flow_cell *buf = (flow_cell*)malloc((size_t)cols * rows * sizeof(flow_cell));
    flow_t *f = flow_new(cols, rows); flow_register_defaults(f);
    /* inside child fully within the frame; overflow child sticks out past the right/bottom edge */
    int inside  = flow_add_node(f, "default", (flow_pt){3, 2}, (void*)"in");   /* (3,2,8,3) */
    int overflow= flow_add_node(f, "default", (flow_pt){14, 5}, (void*)"over");/* (14,5,8,3) overflows */
    int ids[2] = { inside, overflow };
    int gid = flow_group(f, ids, 2);
    flow_node *g = flow_get_node(f, gid);
    flow_node_pos(g);  /* exercise getter */
    /* shrink the group frame so the 'over' child overflows its bottom/right edge:
       set explicit smaller w/h than the bbox to force visible clipping. */
    g->w = 13; g->h = 6;   /* frame (2,1,13,6) -> x2..14 y1..6; 'over' at (14,5,8,3) overflows right+bottom */
    flow_render(f, buf, cols, rows);
    char *s = cells_to_string(buf, cols, rows);
    SNAPSHOT("render_group_clip", s);
    free(s);
    /* DISCRIMINATING clip assertions. Clip rect = frame footprint (2,1,13,6) -> valid
       buffer x in [2,15). The 'over' child (14,5,8,3) renders its label "over" via
       flow_text at surface-local (2,1) = buffer (16,6): unclipped that cell is 'o', but
       x=16 is past the clip's right edge (15) so it is CUT -> blank. (A no-clip build
       would show 'o' here, so this cell genuinely tests the clip.) */
    ASSERT_INT(buf[6*cols + 16].ch, ' ', "overflow child's label 'o' clipped past frame edge");
    /* the child's LEFT border at x=14 (the clip boundary, still inside) SURVIVES and
       overdraws the frame's bottom-right corner -> proves child-drawn-over-frame at the edge. */
    ASSERT_INT(buf[6*cols + 14].ch, 0x2502, "child left border survives at clip edge, over the frame corner");
    free(buf); flow_free(f);
  }

  /* ---- SNAPSHOT 2: SELECTED group + unselected children — children still draw ON TOP
          of the selected group frame (parent-before-child dominates selected-last). ---- */
  {
    int cols = 24, rows = 10;
    flow_cell *buf = (flow_cell*)malloc((size_t)cols * rows * sizeof(flow_cell));
    flow_t *f = flow_new(cols, rows); flow_register_defaults(f);
    int child = flow_add_node(f, "default", (flow_pt){4, 3}, (void*)"C");   /* (4,3,5,3) */
    int ids[1] = { child };
    int gid = flow_group(f, ids, 1);   /* frame (3,2,7,5) x3..9 y2..6 */
    /* shift the child's stored rel pos LEFT/UP by 1 so its border COINCIDES with the
       frame's border (child top-left (3,2) == frame top-left). With the group selected and
       drawn UNDER the child, the child's corner glyph must overwrite the frame corner. */
    flow_get_node(f, child)->pos.x -= 1; flow_get_node(f, child)->pos.y -= 1;  /* child now at abs (3,2) */
    flow_select_node(f, gid, 0);       /* select the GROUP only */
    ASSERT(flow_get_node(f, gid)->flags & FLOW_SELECTED, "group is selected");
    ASSERT(!(flow_get_node(f, child)->flags & FLOW_SELECTED), "child is NOT selected");
    flow_render(f, buf, cols, rows);
    /* (3,2) is BOTH the frame top-left corner AND the child top-left corner. Both are ┌
       (0x250C), but the child draws LAST (parent-before-child) and is UNSELECTED, so the
       cell must carry NO bold attr — proving the child (not the selected group) won. */
    ASSERT_INT(buf[2*cols + 3].ch, 0x250C, "overlap corner is a box corner glyph");
    ASSERT_INT(buf[2*cols + 3].attr & FLOW_BOLD, 0, "overlap corner is UNbold => child drew over selected group frame");
    char *s = cells_to_string(buf, cols, rows);
    SNAPSHOT("render_group_selected_under", s);
    free(s);
    free(buf); flow_free(f);
  }

  return flowtest_report("test_groups");
}
