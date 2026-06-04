/* auto-layout: flow_layout (FORCE + LAYERED) — model -> positions via flow_move_node.
   Spec §10 / increment-3 package #8. LAYERED is integer/topological so exact snapshot
   goldens are stable; FORCE asserts invariants only (float results are platform-fragile).
   Tests poke f->journal.n directly (per-file FLOW_IMPLEMENTATION) for the one-undo-step
   bracket, same pattern as test_undo.c. */
#define FLOW_IMPLEMENTATION
#include "../flow.h"
#include "flowtest.h"
#include <math.h>

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

/* STRICT cell-sharing check (half-open): true only if the rects share >=1 cell.
   NOT flow_rect_intersects — that uses the closed convention where touching edges
   count as overlap, which would false-positive adjacent boxes. */
static int rects_share_cell(flow_rect a, flow_rect b) {
  return a.x < b.x + b.w && b.x < a.x + a.w &&
         a.y < b.y + b.h && b.y < a.y + a.h;
}
/* assert no two nodes in f share a cell; returns number of overlapping pairs */
static int overlapping_pairs(flow_t *f) {
  int bad = 0;
  for (int i = 0; i < flow_node_count(f); i++)
    for (int j = i + 1; j < flow_node_count(f); j++) {
      flow_node *a = &flow_nodes(f)[i], *b = &flow_nodes(f)[j];
      /* skip container-vs-descendant pairs: children sit INSIDE their group's rect */
      if (flow_is_ancestor(f, a->id, b->id) || flow_is_ancestor(f, b->id, a->id)) continue;
      if (rects_share_cell(flow_node_rect_abs(f, a), flow_node_rect_abs(f, b))) bad++;
    }
  return bad;
}
static float center_dist(flow_t *f, int ida, int idb) {
  flow_node *a = flow_get_node(f, ida), *b = flow_get_node(f, idb);
  flow_rect ra = flow_node_rect_abs(f, a), rb = flow_node_rect_abs(f, b);
  float ax = ra.x + ra.w / 2.0f, ay = ra.y + ra.h / 2.0f;
  float bx = rb.x + rb.w / 2.0f, by = rb.y + rb.h / 2.0f;
  return sqrtf((ax - bx) * (ax - bx) + (ay - by) * (ay - by));
}
static int pos_finite(flow_t *f) {       /* int coords are always finite; guard vs absurd blowup */
  for (int i = 0; i < flow_node_count(f); i++) {
    flow_node *n = &flow_nodes(f)[i];
    flow_pt a = flow_node_abs(f, n);
    if (a.x < -100000 || a.x > 100000 || a.y < -100000 || a.y > 100000) return 0;
  }
  return 1;
}

/* diamond DAG: A->B, A->C, B->D, C->D. Returns ids via out[4]. */
static flow_t *make_diamond(int out[4]) {
  flow_t *f = flow_new(80, 24); flow_register_defaults(f);
  out[0] = flow_add_node(f, "default", (flow_pt){50, 20}, (void*)"A");
  out[1] = flow_add_node(f, "default", (flow_pt){3, 7},   (void*)"B");
  out[2] = flow_add_node(f, "default", (flow_pt){60, 2},  (void*)"C");
  out[3] = flow_add_node(f, "default", (flow_pt){10, 18}, (void*)"D");
  flow_add_edge(f, out[0], out[1], "", "");
  flow_add_edge(f, out[0], out[2], "", "");
  flow_add_edge(f, out[1], out[3], "", "");
  flow_add_edge(f, out[2], out[3], "", "");
  return f;
}

int main(void) {
  /* ---- LAYERED LR: rank ordering via x; ranks share x; no shared cells ---- */
  {
    int id[4]; flow_t *f = make_diamond(id);
    flow_layout_opts o = {0}; o.mode = FLOW_LAYOUT_LAYERED; o.dir = FLOW_LR; o.gap_x = 4; o.gap_y = 2;
    flow_layout(f, o);
    flow_pt a = flow_node_abs(f, flow_get_node(f, id[0]));
    flow_pt b = flow_node_abs(f, flow_get_node(f, id[1]));
    flow_pt c = flow_node_abs(f, flow_get_node(f, id[2]));
    flow_pt d = flow_node_abs(f, flow_get_node(f, id[3]));
    ASSERT(a.x < b.x, "LR: rank0 A left of rank1 B");
    ASSERT_INT(b.x, c.x, "LR: B and C share a rank (same x)");
    ASSERT(c.x < d.x, "LR: rank1 C left of rank2 D");
    ASSERT(b.y != c.y, "LR: within-rank nodes separate in y");
    ASSERT_INT(overlapping_pairs(f), 0, "LR: no two nodes share a cell");
    flow_free(f);
  }

  /* ---- LAYERED TB: ranks march in y; within-rank separates in x ---- */
  {
    int id[4]; flow_t *f = make_diamond(id);
    flow_layout_opts o = {0}; o.mode = FLOW_LAYOUT_LAYERED; o.dir = FLOW_TB; o.gap_x = 4; o.gap_y = 2;
    flow_layout(f, o);
    flow_pt a = flow_node_abs(f, flow_get_node(f, id[0]));
    flow_pt b = flow_node_abs(f, flow_get_node(f, id[1]));
    flow_pt c = flow_node_abs(f, flow_get_node(f, id[2]));
    flow_pt d = flow_node_abs(f, flow_get_node(f, id[3]));
    ASSERT(a.y < b.y, "TB: rank0 A above rank1 B");
    ASSERT_INT(b.y, c.y, "TB: B and C share a rank (same y)");
    ASSERT(c.y < d.y, "TB: rank1 C above rank2 D");
    ASSERT(b.x != c.x, "TB: within-rank nodes separate in x");
    ASSERT_INT(overlapping_pairs(f), 0, "TB: no two nodes share a cell");
    flow_free(f);
  }

  /* ---- LAYERED cycle robustness: A->B->C->A must not hang ---- */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    int a = flow_add_node(f, "default", (flow_pt){0, 0}, (void*)"A");
    int b = flow_add_node(f, "default", (flow_pt){0, 0}, (void*)"B");
    int c = flow_add_node(f, "default", (flow_pt){0, 0}, (void*)"C");
    flow_add_edge(f, a, b, "", ""); flow_add_edge(f, b, c, "", ""); flow_add_edge(f, c, a, "", "");
    flow_layout_opts o = {0}; o.mode = FLOW_LAYOUT_LAYERED; o.dir = FLOW_LR;
    flow_layout(f, o);                                  /* returning AT ALL proves back-edge breaking */
    ASSERT(pos_finite(f), "cycle: every node placed finite");
    ASSERT_INT(overlapping_pairs(f), 0, "cycle: no shared cells");
    /* deterministic break: dropped back-edge is by array order, so ranks are a.x<b.x<c.x */
    flow_pt pa = flow_node_abs(f, flow_get_node(f, a));
    flow_pt pb = flow_node_abs(f, flow_get_node(f, b));
    flow_pt pc = flow_node_abs(f, flow_get_node(f, c));
    ASSERT(pa.x < pb.x && pb.x < pc.x, "cycle: A->B->C kept, C->A dropped (array order)");
    flow_free(f);
  }

  /* ---- LAYERED snapshot goldens (integer/topological => byte-stable) ---- */
  {
    int id[4]; flow_t *f = make_diamond(id);
    flow_resize(f, 30, 10);
    flow_layout_opts o = {0}; o.mode = FLOW_LAYOUT_LAYERED; o.dir = FLOW_LR; o.gap_x = 4; o.gap_y = 2;
    flow_layout(f, o);
    flow_cell *buf = (flow_cell*)malloc((size_t)30 * 10 * sizeof(flow_cell));
    flow_render(f, buf, 30, 10);
    char *s = cells_to_string(buf, 30, 10);
    SNAPSHOT("layout_layered_lr", s);
    free(s); free(buf); flow_free(f);
  }
  {
    /* NOTE on the TB golden: node POSITIONS are what layout owns and they are exact
       (A top / B,C mid-rank / D bottom). The edge paths slashing through box borders
       are the default LEFT/RIGHT handles routing around a vertical arrangement — a
       router/handle concern, expected and out of layout's scope. */
    int id[4]; flow_t *f = make_diamond(id);
    flow_resize(f, 30, 14);
    flow_layout_opts o = {0}; o.mode = FLOW_LAYOUT_LAYERED; o.dir = FLOW_TB; o.gap_x = 4; o.gap_y = 2;
    flow_layout(f, o);
    flow_cell *buf = (flow_cell*)malloc((size_t)30 * 14 * sizeof(flow_cell));
    flow_render(f, buf, 30, 14);
    char *s = cells_to_string(buf, 30, 14);
    SNAPSHOT("layout_layered_tb", s);
    free(s); free(buf); flow_free(f);
  }

  /* ---- FORCE invariants: connected-closer, no shared cells, finite, bounded ---- */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    int a = flow_add_node(f, "default", (flow_pt){0, 0},  (void*)"A");
    int b = flow_add_node(f, "default", (flow_pt){1, 1},  (void*)"B");
    int c = flow_add_node(f, "default", (flow_pt){2, 2},  (void*)"C");
    int d = flow_add_node(f, "default", (flow_pt){70, 0}, (void*)"D");
    int e = flow_add_node(f, "default", (flow_pt){70, 20},(void*)"E");
    flow_add_edge(f, a, b, "", ""); flow_add_edge(f, b, c, "", ""); flow_add_edge(f, a, c, "", "");
    flow_add_edge(f, d, e, "", "");
    flow_layout_opts o = {0}; o.mode = FLOW_LAYOUT_FORCE; o.iterations = 300; o.seed = 1;
    flow_layout(f, o);
    /* (a) every connected pair ends closer than every unconnected pair */
    float conn_max = 0, unconn_min = 1e9f;
    int ids[5] = { a, b, c, d, e };
    int adj[5][5] = {{0}};
    adj[0][1] = adj[1][0] = 1; adj[1][2] = adj[2][1] = 1; adj[0][2] = adj[2][0] = 1; adj[3][4] = adj[4][3] = 1;
    for (int i = 0; i < 5; i++)
      for (int j = i + 1; j < 5; j++) {
        float dd = center_dist(f, ids[i], ids[j]);
        if (adj[i][j]) { if (dd > conn_max) conn_max = dd; }
        else           { if (dd < unconn_min) unconn_min = dd; }
      }
    ASSERT(conn_max < unconn_min, "FORCE: connected pairs end closer than unconnected");
    ASSERT_INT(overlapping_pairs(f), 0, "FORCE: no two nodes share a cell");
    ASSERT(pos_finite(f), "FORCE: all positions finite");
    flow_rect bnd = flow_bounds(f);
    ASSERT(bnd.w > 0 && bnd.w < 200 && bnd.h > 0 && bnd.h < 200, "FORCE: bounds stay bounded (gravity)");
    flow_free(f);
  }

  /* ---- FORCE determinism: identical input + opts => identical committed cells ---- */
  {
    flow_layout_opts o = {0}; o.mode = FLOW_LAYOUT_FORCE; o.iterations = 150; o.seed = 7;
    flow_pt run1[4], run2[4], other[4];
    int id[4];
    flow_t *f = make_diamond(id);
    flow_layout(f, o);
    for (int i = 0; i < 4; i++) run1[i] = flow_node_abs(f, flow_get_node(f, id[i]));
    flow_layout(f, o);                                  /* run AGAIN on the same instance */
    for (int i = 0; i < 4; i++) run2[i] = flow_node_abs(f, flow_get_node(f, id[i]));
    flow_free(f);
    int id2[4];
    flow_t *g = make_diamond(id2);                      /* and on a FRESH identical instance */
    flow_layout(g, o);
    for (int i = 0; i < 4; i++) other[i] = flow_node_abs(g, flow_get_node(g, id2[i]));
    flow_free(g);
    int same12 = 1, same13 = 1;
    for (int i = 0; i < 4; i++) {
      if (run1[i].x != run2[i].x || run1[i].y != run2[i].y) same12 = 0;
      if (run1[i].x != other[i].x || run1[i].y != other[i].y) same13 = 0;
    }
    ASSERT(same12, "FORCE determinism: re-run on same instance lands identical cells");
    ASSERT(same13, "FORCE determinism: fresh identical instance lands identical cells");
  }

  /* ---- FORCE coincident-input guard: same pos, 1 iteration -> finite + separated ---- */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    int a = flow_add_node(f, "default", (flow_pt){10, 10}, (void*)"A");
    int b = flow_add_node(f, "default", (flow_pt){10, 10}, (void*)"B");
    flow_add_edge(f, a, b, "", "");
    flow_layout_opts o = {0}; o.mode = FLOW_LAYOUT_FORCE; o.iterations = 1;
    flow_layout(f, o);                                  /* UBSan gate: epsilon min-distance */
    ASSERT(pos_finite(f), "coincident: no NaN/inf blowup");
    flow_pt pa = flow_node_abs(f, flow_get_node(f, a));
    flow_pt pb = flow_node_abs(f, flow_get_node(f, b));
    ASSERT(pa.x != pb.x || pa.y != pb.y, "coincident: the two nodes separate");
    flow_free(f);
  }

  /* ---- GROUPS: children laid out in parent-LOCAL space; P not moved/resized ---- */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    int p = flow_add_node(f, "group", (flow_pt){20, 5}, (void*)"box");
    flow_node *pn = flow_get_node(f, p); pn->w = 30; pn->h = 12;   /* container box (group measure echoes) */
    int c1 = flow_add_node(f, "default", (flow_pt){1, 1}, (void*)"c1");
    int c2 = flow_add_node(f, "default", (flow_pt){2, 2}, (void*)"c2");
    flow_set_parent(f, c1, p); flow_set_parent(f, c2, p);          /* set directly (spec) */
    flow_add_edge(f, c1, c2, "", "");
    flow_layout_opts o = {0}; o.mode = FLOW_LAYOUT_LAYERED; o.dir = FLOW_LR; o.gap_x = 4; o.gap_y = 2;
    flow_layout(f, o);
    pn = flow_get_node(f, p);
    ASSERT(pn->pos.x == 20 && pn->pos.y == 5, "groups: P not moved (single top-level partition)");
    ASSERT(pn->w == 30 && pn->h == 12, "groups: P not resized");
    flow_node *n1 = flow_get_node(f, c1), *n2 = flow_get_node(f, c2);
    /* stored pos is parent-RELATIVE: small positive offsets, NOT world coords */
    ASSERT(n1->pos.x >= 1 && n1->pos.y >= 1 && n1->pos.x < 30, "groups: c1 stored pos is parent-relative");
    ASSERT(n2->pos.x >= 1 && n2->pos.y >= 1 && n2->pos.x < 40, "groups: c2 stored pos is parent-relative");
    flow_pt a1 = flow_node_abs(f, n1), a2 = flow_node_abs(f, n2);
    ASSERT(a1.x >= 20 && a1.y >= 5, "groups: c1 abs lands inside/near P");
    ASSERT(a2.x > a1.x, "groups: layered rank order applies inside the group (c1->c2)");
    ASSERT_INT(overlapping_pairs(f), 0, "groups: children do not overlap");
    flow_free(f);
  }

  /* ---- GROUPS mixed: top-level pass + group-local pass, no cross-contamination ---- */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    int c1 = flow_add_node(f, "default", (flow_pt){30, 8},  (void*)"c1");
    int c2 = flow_add_node(f, "default", (flow_pt){40, 12}, (void*)"c2");
    int grp = flow_group(f, (int[]){ c1, c2 }, 2);                 /* container appended AFTER children */
    int e1 = flow_add_node(f, "default", (flow_pt){0, 0},   (void*)"E");
    int e2 = flow_add_node(f, "default", (flow_pt){70, 20}, (void*)"F");
    flow_add_edge(f, e1, e2, "", "");
    flow_add_edge(f, c1, c2, "", "");
    flow_layout_opts o = {0}; o.mode = FLOW_LAYOUT_LAYERED; o.dir = FLOW_LR; o.gap_x = 4; o.gap_y = 2;
    flow_layout(f, o);
    flow_node *g = flow_get_node(f, grp);
    flow_node *n1 = flow_get_node(f, c1), *n2 = flow_get_node(f, c2);
    ASSERT(n1->parent == grp && n2->parent == grp, "mixed: children still grouped");
    ASSERT(n1->pos.x >= 1 && n1->pos.y >= 1, "mixed: c1 stored parent-relative (laid out locally)");
    flow_pt a1 = flow_node_abs(f, n1);
    flow_pt gp = flow_node_abs(f, g);
    ASSERT(a1.x >= gp.x, "mixed: c1 abs composes from the (possibly moved) parent");
    ASSERT(pos_finite(f), "mixed: all nodes placed finite");
    ASSERT_INT(overlapping_pairs(f), 0, "mixed: no cross-partition overlaps");
    flow_free(f);
  }

  /* ---- fit_after: framed within margin ---- */
  {
    int id[4]; flow_t *f = make_diamond(id);
    flow_layout_opts o = {0}; o.mode = FLOW_LAYOUT_LAYERED; o.dir = FLOW_LR; o.gap_x = 4; o.gap_y = 2;
    o.fit_after = 1; o.margin = 2;
    flow_layout(f, o);
    for (int i = 0; i < 4; i++) {
      flow_node *n = flow_get_node(f, id[i]);
      flow_pt s = flow_to_screen(f, flow_node_abs(f, n));
      ASSERT(s.x >= 2 && s.y >= 2 && s.x + n->w <= 80 - 2 && s.y + n->h <= 24 - 2,
             "fit_after: projected node rect within [margin, size-margin]");
    }
    flow_free(f);
  }
  /* ...and WITHOUT fit_after the viewport is untouched */
  {
    int id[4]; flow_t *f = make_diamond(id);
    flow_viewport before = flow_view_get(f);
    flow_layout_opts o = {0}; o.mode = FLOW_LAYOUT_LAYERED;
    flow_layout(f, o);
    flow_viewport after = flow_view_get(f);
    ASSERT(before.ox == after.ox && before.oy == after.oy && before.zoom == after.zoom,
           "no fit_after: viewport unchanged");
    flow_free(f);
  }

  /* ---- edge cases: empty + single-node graphs are no-ops ---- */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    flow_layout_opts o = {0};
    flow_layout(f, o);                                   /* empty: must not crash or journal */
    ASSERT_INT(f->journal.n, 0, "empty graph: nothing journaled");
    int a = flow_add_node(f, "default", (flow_pt){7, 9}, (void*)"A");
    int steps = f->journal.n;
    o.mode = FLOW_LAYOUT_LAYERED;
    flow_layout(f, o);
    flow_node *n = flow_get_node(f, a);
    ASSERT(n->pos.x == 7 && n->pos.y == 9, "single node: position untouched (no-op)");
    ASSERT_INT(f->journal.n, steps, "single node: nothing journaled");
    flow_free(f);
  }

  /* ---- wrappers delegate to flow_layout with equivalent opts ---- */
  {
    int id1[4], id2[4];
    flow_t *f1 = make_diamond(id1), *f2 = make_diamond(id2);
    flow_layout_opts o = {0}; o.mode = FLOW_LAYOUT_LAYERED; o.dir = FLOW_TB; o.gap_x = 3; o.gap_y = 5;
    flow_layout(f1, o);
    flow_layout_layered(f2, FLOW_TB, 3, 5);
    int same = 1;
    for (int i = 0; i < 4; i++) {
      flow_pt p1 = flow_node_abs(f1, flow_get_node(f1, id1[i]));
      flow_pt p2 = flow_node_abs(f2, flow_get_node(f2, id2[i]));
      if (p1.x != p2.x || p1.y != p2.y) same = 0;
    }
    ASSERT(same, "flow_layout_layered wrapper == flow_layout LAYERED");
    flow_free(f1); flow_free(f2);
  }
  {
    int id1[4], id2[4];
    flow_t *f1 = make_diamond(id1), *f2 = make_diamond(id2);
    flow_layout_opts o = {0}; o.mode = FLOW_LAYOUT_FORCE; o.iterations = 120; o.k = 9.0f; o.gravity = 0.04f;
    flow_layout(f1, o);
    flow_force_opts fo = { 120, 9.0f, 0.04f };
    flow_layout_force(f2, fo);
    int same = 1;
    for (int i = 0; i < 4; i++) {
      flow_pt p1 = flow_node_abs(f1, flow_get_node(f1, id1[i]));
      flow_pt p2 = flow_node_abs(f2, flow_get_node(f2, id2[i]));
      if (p1.x != p2.x || p1.y != p2.y) same = 0;
    }
    ASSERT(same, "flow_layout_force wrapper == flow_layout FORCE");
    flow_free(f1); flow_free(f2);
  }

  /* ---- undo seam (handoff #8 wrinkle 2): whole layout is ONE undo step, routed
     through a GROUPED graph so depth-ordered commit interacts with reverse-order undo ---- */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    int c1 = flow_add_node(f, "default", (flow_pt){30, 8},  (void*)"c1");
    int c2 = flow_add_node(f, "default", (flow_pt){40, 12}, (void*)"c2");
    int grp = flow_group(f, (int[]){ c1, c2 }, 2);
    int e1 = flow_add_node(f, "default", (flow_pt){0, 0},   (void*)"E");
    int e2 = flow_add_node(f, "default", (flow_pt){70, 20}, (void*)"F");
    flow_add_edge(f, c1, c2, "", ""); flow_add_edge(f, e1, e2, "", "");
    int ids[5] = { c1, c2, grp, e1, e2 };
    flow_pt before_pos[5]; int before_parent[5];
    for (int i = 0; i < 5; i++) { flow_node *n = flow_get_node(f, ids[i]);
      before_pos[i] = n->pos; before_parent[i] = n->parent; }
    int steps_before = f->journal.n;
    flow_layout_opts o = {0}; o.mode = FLOW_LAYOUT_LAYERED; o.dir = FLOW_LR; o.gap_x = 4; o.gap_y = 2;
    flow_layout(f, o);
    ASSERT_INT(f->journal.n, steps_before + 1, "layout journals exactly ONE undo step");
    flow_undo(f);
    int restored = 1;
    for (int i = 0; i < 5; i++) {
      flow_node *n = flow_get_node(f, ids[i]);
      if (n->pos.x != before_pos[i].x || n->pos.y != before_pos[i].y) restored = 0;
      if (n->parent != before_parent[i]) restored = 0;
    }
    ASSERT(restored, "one flow_undo restores every pre-layout position");
    flow_free(f);
  }

  return flowtest_report("test_layout");
}
