#define FLOW_IMPLEMENTATION
#include "../flow.h"
#include "flowtest.h"

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
  int cols = 30, rows = 8;
  flow_cell *buf = (flow_cell*)malloc((size_t)cols * rows * sizeof(flow_cell));

  /* single node */
  flow_t *f = flow_new(cols, rows); flow_register_defaults(f);
  flow_add_node(f, "default", (flow_pt){0, 0}, (void*)"hi");
  flow_render(f, buf, cols, rows);
  char *s1 = cells_to_string(buf, cols, rows);
  SNAPSHOT("render_single", s1);   /* expect a 6x3 box "hi" at top-left */
  free(s1); flow_free(f);

  /* two nodes + edge */
  flow_t *g = flow_new(cols, rows); flow_register_defaults(g);
  int a = flow_add_node(g, "default", (flow_pt){0, 1}, (void*)"A");
  int b = flow_add_node(g, "default", (flow_pt){15, 4}, (void*)"B");
  flow_add_edge(g, a, b, "", "");
  flow_render(g, buf, cols, rows);
  char *s2 = cells_to_string(buf, cols, rows);
  SNAPSHOT("render_two_edge", s2);  /* expect two boxes joined by a path ending in an arrow */
  int has_arrow = 0;
  for (int i = 0; i < cols * rows; i++) { uint32_t c = buf[i].ch;
    if (c==0x25B6||c==0x25C0||c==0x25BC||c==0x25B2) has_arrow = 1; }
  ASSERT(has_arrow, "edge renders an arrowhead");
  free(s2); flow_free(g);

  /* minimap: bordered box in the BR corner with node dots inside */
  flow_t *m = flow_new(40, 12); flow_register_defaults(m);
  flow_add_node(m, "default", (flow_pt){0, 0}, (void*)"A");
  flow_add_node(m, "default", (flow_pt){25, 8}, (void*)"B");
  flow_set_minimap(m, 1, FLOW_CORNER_BR, 12, 6);
  flow_cell *mb = (flow_cell*)malloc((size_t)40 * 12 * sizeof(flow_cell));
  flow_render(m, mb, 40, 12);
  ASSERT_INT(mb[6*40 + 28].ch, 0x250C, "minimap top-left corner at (28,6)");      /* 40-12, 12-6 */
  ASSERT_INT(mb[6*40 + 39].ch, 0x2510, "minimap top-right corner");
  int dots = 0;
  for (int i = 0; i < 40 * 12; i++) { uint32_t c = mb[i].ch; if (c == 0x2022 || c == 0x25C9) dots++; }
  ASSERT(dots >= 2, "minimap shows >=2 node dots");
  free(mb); flow_free(m);

  /* background grid: off by default — no grid glyph on an empty graph */
  {
    flow_t *bg = flow_new(cols, rows); flow_register_defaults(bg);
    flow_render(bg, buf, cols, rows);
    int grid = 0;
    for (int i = 0; i < cols * rows; i++) { uint32_t c = buf[i].ch;
      if (c==0x00B7 || c==0x002B || c==0x2502 || c==0x2500 || c==0x253C) grid = 1; }
    ASSERT(!grid, "background grid off by default");
    flow_free(bg);
  }

  /* background DOTS: membership + light fg color, on a small empty buffer */
  {
    int bc = 12, br = 6;
    flow_cell *bb = (flow_cell*)malloc((size_t)bc * br * sizeof(flow_cell));
    flow_t *bg = flow_new(bc, br); flow_register_defaults(bg);
    flow_set_background(bg, FLOW_BG_DOTS, 3);
    flow_render(bg, bb, bc, br);
    ASSERT_INT(bb[0*bc + 0].ch, 0x00B7, "DOTS grid point at (0,0)");
    ASSERT_INT(bb[0*bc + 0].fg, FLOW_BG_GRID_FG, "DOTS grid uses light fg");
    ASSERT_INT(bb[0*bc + 1].ch, ' ', "DOTS off-grid cell (1,0) blank");
    /* scroll snapshots: grid pans with the canvas */
    char *bp0 = cells_to_string(bb, bc, br);
    SNAPSHOT("bg_dots_pan0", bp0); free(bp0);
    flow_pan(bg, 1, 0);
    flow_render(bg, bb, bc, br);
    char *bp1 = cells_to_string(bb, bc, br);
    SNAPSHOT("bg_dots_pan1", bp1); free(bp1);
    free(bb); flow_free(bg);
  }

  /* background LINES: vertical, horizontal, intersection glyphs */
  {
    int bc = 12, br = 6;
    flow_cell *bb = (flow_cell*)malloc((size_t)bc * br * sizeof(flow_cell));
    flow_t *bg = flow_new(bc, br); flow_register_defaults(bg);
    flow_set_background(bg, FLOW_BG_LINES, 3);
    flow_render(bg, bb, bc, br);
    ASSERT_INT(bb[3*bc + 3].ch, 0x253C, "LINES intersection at (3,3)");
    ASSERT_INT(bb[1*bc + 3].ch, 0x2502, "LINES vertical-only at (3,1)");
    ASSERT_INT(bb[3*bc + 1].ch, 0x2500, "LINES horizontal-only at (1,3)");
    free(bb); flow_free(bg);
  }

  /* background CROSS: '+' at grid points, blank off-grid */
  {
    int bc = 12, br = 6;
    flow_cell *bb = (flow_cell*)malloc((size_t)bc * br * sizeof(flow_cell));
    flow_t *bg = flow_new(bc, br); flow_register_defaults(bg);
    flow_set_background(bg, FLOW_BG_CROSS, 3);
    flow_render(bg, bb, bc, br);
    ASSERT_INT(bb[0*bc + 0].ch, 0x002B, "CROSS grid point at (0,0)");
    ASSERT_INT(bb[0*bc + 1].ch, ' ', "CROSS off-grid cell (1,0) blank");
    free(bb); flow_free(bg);
  }

  /* background gap clamp / UBSan safety: gap 0 clamps to 1, every cell is a grid point */
  {
    int bc = 12, br = 6;
    flow_cell *bb = (flow_cell*)malloc((size_t)bc * br * sizeof(flow_cell));
    flow_t *bg = flow_new(bc, br); flow_register_defaults(bg);
    flow_set_background(bg, FLOW_BG_DOTS, 0);
    flow_render(bg, bb, bc, br);  /* must not divide/modulo by zero */
    ASSERT_INT(bb[0*bc + 0].ch, 0x00B7, "gap-clamp: (0,0) is a grid point");
    ASSERT_INT(bb[0*bc + 1].ch, 0x00B7, "gap-clamp: (1,0) is a grid point");
    free(bb); flow_free(bg);
  }

  /* background layering: a node box overwrites the grid (background is under nodes) */
  {
    int bc = 12, br = 6;
    flow_cell *bb = (flow_cell*)malloc((size_t)bc * br * sizeof(flow_cell));
    flow_t *bg = flow_new(bc, br); flow_register_defaults(bg);
    flow_set_background(bg, FLOW_BG_DOTS, 3);
    flow_add_node(bg, "default", (flow_pt){0, 0}, (void*)"n");
    flow_render(bg, bb, bc, br);
    ASSERT_INT(bb[0*bc + 0].ch, 0x250C, "node box overwrites grid at (0,0)");
    free(bb); flow_free(bg);
  }

  /* minimap + background both on: the grid must NOT bleed through the opaque
     panel. Empty graph 40x12, BR minimap 12x6, DOTS gap 3. Cell (33,9) is a
     minimap interior cell that is not a border, viewport line, or node dot,
     and would be a grid dot (33%3==0 && 9%3==0) without the opaque fill. */
  {
    flow_cell *mb = (flow_cell*)malloc((size_t)40 * 12 * sizeof(flow_cell));
    flow_t *bg = flow_new(40, 12); flow_register_defaults(bg);
    flow_set_minimap(bg, 1, FLOW_CORNER_BR, 12, 6);
    flow_set_background(bg, FLOW_BG_DOTS, 3);
    flow_render(bg, mb, 40, 12);
    ASSERT_INT(mb[9*40 + 33].ch, ' ', "grid does not bleed through minimap interior");
    free(mb); flow_free(bg);
  }

  /* selected-last draw order: two overlapping nodes; the later-inserted one (Y) is
     unselected, the earlier one (X) is selected. With selected-last, X draws on top
     in the overlap. cells_to_string only sees .ch, and bold is an attr, so place them
     overlapping with different offsets and also assert the overlap cell's glyph+attr. */
  {
    int sc = 12, sr = 6;
    flow_cell *sb = (flow_cell*)malloc((size_t)sc * sr * sizeof(flow_cell));
    flow_t *sf = flow_new(sc, sr); flow_register_defaults(sf);
    int x = flow_add_node(sf, "default", (flow_pt){0, 0}, (void*)"X");  /* rect (0,0,5,3) inserted first */
    flow_add_node(sf, "default", (flow_pt){2, 1}, (void*)"Y");          /* rect (2,1,5,3) inserted second, overlaps */
    flow_select_node(sf, x, 0);                                         /* X selected (earlier-inserted) */
    flow_render(sf, sb, sc, sr);
    /* overlap cell (2,2): X draws its bottom edge ─ (0x2500, BOLD); Y draws its left edge │.
       selected-last => X wins: glyph is ─ and it is BOLD. */
    ASSERT_INT(sb[2*sc + 2].ch, 0x2500, "selected node draws on top in overlap (X bottom edge)");
    ASSERT_INT(sb[2*sc + 2].attr & FLOW_BOLD, FLOW_BOLD, "on-top overlap cell is the selected (bold) node");
    char *ss = cells_to_string(sb, sc, sr);
    SNAPSHOT("render_selected_last", ss); free(ss);
    free(sb); flow_free(sf);
  }

  /* marquee border: set marquee_on with a known screen anchor/cur and render. The
     marquee strokes a distinct glyph (0x2592) clipped to the buffer. */
  {
    int mc = 12, mr = 6;
    flow_cell *mb2 = (flow_cell*)malloc((size_t)mc * mr * sizeof(flow_cell));
    flow_t *mf = flow_new(mc, mr); flow_register_defaults(mf);
    flow_render(mf, mb2, mc, mr);                 /* empty first to clear */
    /* directly drive the marquee state the input layer would set */
    flow_feed(mf, "\x1b[<4;3;2M", 9);            /* shift-press @ screen (2,1) -> anchor */
    flow_feed(mf, "\x1b[<36;9;5M", 11);          /* shift-motion to screen (8,4) -> live box */
    flow_render(mf, mb2, mc, mr);
    ASSERT_INT(mb2[1*mc + 2].ch, 0x2592, "marquee top-left corner glyph");
    ASSERT_INT(mb2[4*mc + 8].ch, 0x2592, "marquee bottom-right corner glyph");
    int marq = 0;
    for (int i = 0; i < mc * mr; i++) if (mb2[i].ch == 0x2592) marq++;
    ASSERT(marq >= 8, "marquee strokes a border (>=8 cells for a 7x4 box)");
    char *ms = cells_to_string(mb2, mc, mr);
    SNAPSHOT("render_marquee", ms); free(ms);
    flow_feed(mf, "\x1b[<4;9;5m", 9);            /* release to clear marquee state */
    free(mb2); flow_free(mf);
  }

  /* handle markers: a HOVERED node shows ◉ on its L/R borders; a non-hovered shows none */
  {
    int hc = 18, hr = 6;
    flow_cell *hb = (flow_cell*)malloc((size_t)hc * hr * sizeof(flow_cell));
    flow_t *hf = flow_new(hc, hr); flow_register_defaults(hf);
    int a = flow_add_node(hf, "default", (flow_pt){0, 1}, (void*)"A");   /* w=5,h=3; L@(0,2) R@(4,2) */
    flow_add_node(hf, "default", (flow_pt){10, 1}, (void*)"B");          /* not hovered: no markers */
    flow_set_hover(hf, a);
    flow_render(hf, hb, hc, hr);
    int markers = 0;
    for (int i = 0; i < hc * hr; i++) if (hb[i].ch == 0x25C9) markers++;
    ASSERT_INT(markers, 2, "hovered node shows exactly 2 ◉ handle markers");
    ASSERT_INT(hb[2*hc + 0].ch, 0x25C9, "LEFT handle marker on hovered node border");
    ASSERT_INT(hb[2*hc + 4].ch, 0x25C9, "RIGHT handle marker on hovered node border");
    /* B (not hovered, cols 10..14) shows no marker on its borders */
    ASSERT(hb[2*hc + 10].ch != 0x25C9 && hb[2*hc + 14].ch != 0x25C9, "non-hovered node has no markers");
    char *hs = cells_to_string(hb, hc, hr);
    SNAPSHOT("render_handles_hover", hs); free(hs);
    free(hb); flow_free(hf);
  }

  /* in-flight connection preview: dashed rubber-band from a source handle to a free
     cursor cell. No arrowhead; a dashed pattern is present. */
  {
    int pc = 24, pr = 6;
    flow_cell *pb = (flow_cell*)malloc((size_t)pc * pr * sizeof(flow_cell));
    flow_t *pf = flow_new(pc, pr); flow_register_defaults(pf);
    int a = flow_add_node(pf, "default", (flow_pt){0, 1}, (void*)"A");   /* RIGHT@(4,2) */
    flow_set_hover(pf, a);
    /* press A's RIGHT handle (screen (4,2) => SGR (5,3)), drag to a free cell (18,2)=>SGR(19,3) */
    flow_feed(pf, "\x1b[<0;5;3M", 9);
    flow_feed(pf, "\x1b[<32;19;3M", 11);
    flow_render(pf, pb, pc, pr);
    int arrow = 0, dashes = 0;
    for (int i = 0; i < pc * pr; i++) { uint32_t c = pb[i].ch;
      if (c==0x25B6||c==0x25C0||c==0x25BC||c==0x25B2) arrow = 1;
      if (c==0x2500 || c==0x2502 || c==0x250C || c==0x2510 || c==0x2514 || c==0x2518) dashes++; }
    ASSERT(!arrow, "preview shows NO arrowhead");
    ASSERT(dashes >= 2, "preview shows a dashed line (>=2 path cells)");
    char *ps = cells_to_string(pb, pc, pr);
    SNAPSHOT("render_connect_preview", ps); free(ps);
    flow_feed(pf, "\x1b[<0;19;3m", 9);   /* release on empty -> cancel */
    free(pb); flow_free(pf);
  }

  /* committed edge anchored on declared handles: A->B edge meets RIGHT-of-A and
     LEFT-of-B (via flow_handle_anchor), not the old fixed points. Neither node is
     hovered/selected, so no ◉ markers — the snapshot is pure edge anchoring. */
  {
    int ec = 24, er = 6;
    flow_cell *eb = (flow_cell*)malloc((size_t)ec * er * sizeof(flow_cell));
    flow_t *ef = flow_new(ec, er); flow_register_defaults(ef);
    int a = flow_add_node(ef, "default", (flow_pt){0, 1}, (void*)"A");   /* RIGHT border col 4, row 2 */
    int b = flow_add_node(ef, "default", (flow_pt){14, 1}, (void*)"B");  /* LEFT border col 14, row 2 */
    flow_add_edge(ef, a, b, "out", "in");
    flow_render(ef, eb, ec, er);
    int has_arrow = 0;
    for (int i = 0; i < ec * er; i++) { uint32_t c = eb[i].ch;
      if (c==0x25B6||c==0x25C0||c==0x25BC||c==0x25B2) has_arrow = 1; }
    ASSERT(has_arrow, "handle-anchored edge still renders an arrowhead");
    /* the edge leaves A's RIGHT border (row 2) at col 5 (one cell outside) */
    ASSERT_INT(eb[2*ec + 5].ch, 0x2500, "edge departs RIGHT-of-A on the handle row");
    char *es = cells_to_string(eb, ec, er);
    SNAPSHOT("render_edge_handle_anchors", es); free(es);
    free(eb); flow_free(ef);
  }

  /* edge label: a labelled A->B edge renders its label glyphs near label_anchor */
  {
    int lc = 24, lr = 6;
    flow_cell *lb = (flow_cell*)malloc((size_t)lc * lr * sizeof(flow_cell));
    flow_t *lf = flow_new(lc, lr); flow_register_defaults(lf);
    int a = flow_add_node(lf, "default", (flow_pt){0, 1}, (void*)"A");
    int b = flow_add_node(lf, "default", (flow_pt){14, 1}, (void*)"B");
    int e = flow_add_edge(lf, a, b, "out", "in");
    flow_set_edge_label(lf, e, "Lbl");
    flow_render(lf, lb, lc, lr);
    int found = 0;
    for (int i = 0; i < lc * lr; i++) if (lb[i].ch == 'L') found = 1;  /* first label glyph present */
    ASSERT(found, "edge label glyph 'L' rendered");
    char *ls = cells_to_string(lb, lc, lr);
    SNAPSHOT("render_edge_label", ls); free(ls);
    free(lb); flow_free(lf);
  }

  /* edge selected: the routed path carries FLOW_BOLD on its cells */
  {
    int xc = 24, xr = 6;
    flow_cell *xb = (flow_cell*)malloc((size_t)xc * xr * sizeof(flow_cell));
    flow_t *xf = flow_new(xc, xr); flow_register_defaults(xf);
    int a = flow_add_node(xf, "default", (flow_pt){0, 1}, (void*)"A");
    int b = flow_add_node(xf, "default", (flow_pt){14, 1}, (void*)"B");
    int e = flow_add_edge(xf, a, b, "out", "in");
    flow_select_edge(xf, e, 0);
    flow_render(xf, xb, xc, xr);
    int bold_cells = 0;
    for (int i = 0; i < xc * xr; i++) {
      uint32_t c = xb[i].ch;
      int is_path = (c==0x2500||c==0x2502||c==0x250C||c==0x2510||c==0x2514||c==0x2518||
                     c==0x25B6||c==0x25C0||c==0x25BC||c==0x25B2);
      if (is_path && (xb[i].attr & FLOW_BOLD)) bold_cells++;
    }
    ASSERT(bold_cells >= 1, "selected edge has >=1 bold routed cell");
    char *xs = cells_to_string(xb, xc, xr);
    SNAPSHOT("render_edge_selected", xs); free(xs);
    free(xb); flow_free(xf);
  }

  /* straight edge type: A(10,5)->B(30,12) with type "straight" renders a direct
     diagonal-stepped connector + arrowhead, distinct from the orthogonal golden. */
  {
    /* registration: orthogonal default + straight both resolve via flow_edge_type_for.
       NOTE: flow_edge_type_for("") returns NULL by design — the empty->orthogonal
       mapping lives at the render call site, so we probe the registered NAMES here. */
    flow_t *rf = flow_new(10, 10); flow_register_defaults(rf);
    const flow_edge_type *def = flow_edge_type_for(rf, "default");
    const flow_edge_type *str = flow_edge_type_for(rf, "straight");
    ASSERT(def != NULL, "orthogonal default edge type registered");
    ASSERT(def && def->route == flow_route_orthogonal, "default resolves to orthogonal router");
    ASSERT(str != NULL, "straight edge type registered");
    ASSERT(str && str->route == flow_route_straight, "straight resolves to straight router");
    flow_free(rf);

    int wc = 40, wr = 18;
    flow_cell *wb = (flow_cell*)malloc((size_t)wc * wr * sizeof(flow_cell));
    flow_t *wf = flow_new(wc, wr); flow_register_defaults(wf);
    int a = flow_add_node(wf, "default", (flow_pt){10, 5}, (void*)"A");
    int b = flow_add_node(wf, "default", (flow_pt){30, 12}, (void*)"B");
    int e = flow_add_edge(wf, a, b, "out", "in");
    flow_edge *ep = flow_get_edge(wf, e);
    snprintf(ep->type, sizeof ep->type, "%s", "straight");   /* no public setter; struct field, house style */
    flow_render(wf, wb, wc, wr);
    int arrow = 0, diag = 0;
    for (int i = 0; i < wc * wr; i++) { uint32_t c = wb[i].ch;
      if (c==0x25B6||c==0x25C0||c==0x25BC||c==0x25B2) arrow = 1;
      if (c==0x2571 || c==0x2572) diag = 1; }
    ASSERT(arrow, "straight edge renders an arrowhead");
    ASSERT(diag, "straight edge renders diagonal ╱/╲ glyphs");
    char *ws = cells_to_string(wb, wc, wr);
    SNAPSHOT("render_straight_edge", ws); free(ws);
    free(wb); flow_free(wf);
  }

  /* ---- hidden nodes/edges are skipped by render + minimap (inc-4 #11) ---- */
  {
    flow_t *h = flow_new(cols, rows); flow_register_defaults(h);
    int ha = flow_add_node(h, "default", (flow_pt){0, 0},  (void*)"A");
    int hb = flow_add_node(h, "default", (flow_pt){20, 0}, (void*)"B");
    int hc = flow_add_node(h, "default", (flow_pt){8, 4},  (void*)"C");
    flow_add_edge(h, ha, hb, "out", "in");
    int ecb = flow_add_edge(h, hc, hb, "out", "in");          /* cascade: C hidden -> edge gone */
    (void)ecb;
    flow_set_node_hidden(h, hc, 1);
    flow_render(h, buf, cols, rows);
    char *hs = cells_to_string(buf, cols, rows);
    /* mechanical guards so even the snapshot-creation run proves the skip: */
    ASSERT(strchr(hs, 'C') == NULL, "hidden node's label is nowhere in the frame");
    ASSERT(strchr(hs, 'A') != NULL && strchr(hs, 'B') != NULL, "visible nodes still render");
    SNAPSHOT("render_hidden", hs);   /* A and B + their edge; no C, no C->B edge */
    free(hs);
    /* un-hide: C reappears (no snapshot; behavioral) */
    flow_set_node_hidden(h, hc, 0);
    flow_render(h, buf, cols, rows);
    char *hs2 = cells_to_string(buf, cols, rows);
    ASSERT(strchr(hs2, 'C') != NULL, "un-hidden node renders again");
    free(hs2);
    /* minimap dots skip hidden: A,B visible + C hidden -> exactly 2 dots */
    flow_set_node_hidden(h, hc, 1);
    flow_set_minimap(h, 1, FLOW_CORNER_BR, 12, 6);
    flow_render(h, buf, cols, rows);
    int dots = 0;
    for (int i = 0; i < cols * rows; i++)
      if (buf[i].ch == 0x2022 || buf[i].ch == 0x25C9) dots++;
    ASSERT_INT(dots, 2, "minimap draws dots for the two VISIBLE nodes only");
    flow_free(h);
  }

  /* ---- minimap all-hidden + panned: viewport rect must track the screen window
     (inc-5 #1). The world-window guard must fall to the `: vp` branch when every
     node is hidden, exactly like an empty graph — flow_bounds returns {0,0,0,0}
     there, and unioning that zero rect treats it as a point AT the world origin,
     inflating W.h (12 -> 40 under pan(40,40)) and compressing the drawn rect. ---- */
  {
    flow_t *am = flow_new(40, 12); flow_register_defaults(am);
    flow_cell *amb = (flow_cell*)malloc((size_t)40 * 12 * sizeof(flow_cell));
    int na = flow_add_node(am, "default", (flow_pt){0, 0}, (void*)"A");
    int nb = flow_add_node(am, "default", (flow_pt){8, 4}, (void*)"B");
    flow_set_minimap(am, 1, FLOW_CORNER_BR, 12, 6);   /* bw=12 bh=6 -> iw=10 ih=4 */
    flow_set_node_hidden(am, na, 1);
    flow_set_node_hidden(am, nb, 1);
    flow_pan(am, 40, 40);                /* no translate extent set -> sticks */
    flow_render(am, amb, 40, 12);
    /* fixed W == vp == {-40,-40,40,12} -> vy2 = 11*4/12 = 3 = ih-1: the rect reaches
       the interior bottom row. Buggy W = {-40,-40,40,40} -> vy2 = 11*4/40 = 1: it
       stops two interior rows short. Top row + corners are identical in both rects —
       assert only the lower cells that exist solely in the fixed rect.
       BR minimap interior on 40x12: screen x=29..38, y=7..10. */
    ASSERT_INT(amb[10*40 + 33].ch, 0x2500, "vp bottom line at interior bottom row");
    ASSERT_INT(amb[9*40 + 29].ch, 0x2502, "vp left line reaches lower interior");
    int amdots = 0;
    for (int i = 0; i < 40 * 12; i++)
      if (amb[i].ch == 0x2022 || amb[i].ch == 0x25C9) amdots++;
    ASSERT_INT(amdots, 0, "all-hidden minimap draws no node dots");
    char *ams = cells_to_string(amb, 40, 12);
    SNAPSHOT("render_minimap_all_hidden", ams);  /* full-interior vp rect, no dots */
    free(ams);
    free(amb); flow_free(am);
  }

  /* ===== inc-7 #1: theme tokens + colorMode presets ===== */
  /* (1) a new graph defaults to FLOW_COLOR_DEFAULT == today's literals 7/0/8. */
  {
    flow_t *t = flow_new(30, 8); flow_register_defaults(t);
    flow_add_node(t, "default", (flow_pt){0, 0}, (void*)"A");
    ASSERT_INT(flow_color_mode_get(t), FLOW_COLOR_DEFAULT, "new graph defaults to FLOW_COLOR_DEFAULT");
    ASSERT_INT(t->theme.fg, 7, "default theme fg == legacy FLOW_FG");
    ASSERT_INT(t->theme.bg, 0, "default theme bg == legacy FLOW_BG");
    ASSERT_INT(t->theme.grid_fg, 8, "default theme grid_fg == legacy FLOW_BG_GRID_FG");
    flow_free(t);
  }

  /* (2) a colorMode swap moves the documented chrome cells' .fg/.bg — the
     cell-field discriminator. cells_to_string captures ONLY .ch, so a color swap
     is snapshot-invisible; assert the raw color bytes. Scene exercises distinct
     chrome layers (canvas clear / grid dot / statusbar). */
  {
    int tc = 30, tr = 8;
    flow_cell *tb = (flow_cell*)malloc((size_t)tc * tr * sizeof(flow_cell));
    flow_t *t = flow_new(tc, tr); flow_register_defaults(t);
    flow_add_node(t, "default", (flow_pt){0, 0}, (void*)"A");   /* 5x3 box at screen (0,0) */
    flow_set_background(t, FLOW_BG_DOTS, 4);
    flow_set_minimap(t, 1, FLOW_CORNER_BR, 8, 5);
    flow_set_statusbar(t, 1);
    /* witnesses: canvas-clear (10,1) [not grid, not node, not minimap], grid dot
       (8,0), statusbar row (2,7). */
    int CAN = 1*tc + 10, GRID = 0*tc + 8, SB = 7*tc + 2;

    flow_render(t, tb, tc, tr);
    ASSERT_INT(tb[CAN].ch, ' ', "witness canvas cell is blank");
    ASSERT_INT(tb[CAN].fg, 7, "DEFAULT canvas fg 7"); ASSERT_INT(tb[CAN].bg, 0, "DEFAULT canvas bg 0");
    ASSERT_INT(tb[GRID].ch, 0x00B7, "witness grid cell is a dot");
    ASSERT_INT(tb[GRID].fg, 8, "DEFAULT grid fg 8"); ASSERT_INT(tb[GRID].bg, 0, "DEFAULT grid bg 0");
    ASSERT_INT(tb[SB].fg, 7, "DEFAULT statusbar fg 7"); ASSERT_INT(tb[SB].bg, 0, "DEFAULT statusbar bg 0");

    /* LIGHT: bg flips light (15), fg goes dark (0); grid_fg -> 7. The bg flip is
       the headline — an fg-only nudge would be the weakest signal for colorMode. */
    flow_set_color_mode(t, FLOW_COLOR_LIGHT);
    flow_render(t, tb, tc, tr);
    ASSERT_INT(flow_color_mode_get(t), FLOW_COLOR_LIGHT, "mode is LIGHT");
    ASSERT_INT(tb[CAN].bg, 15, "LIGHT canvas bg flips to 15");
    ASSERT_INT(tb[CAN].fg, 0, "LIGHT canvas fg -> 0");
    ASSERT_INT(tb[CAN].ch, ' ', "LIGHT swap is color-only (canvas glyph unchanged)");
    ASSERT_INT(tb[GRID].bg, 15, "LIGHT grid bg flips to 15");
    ASSERT_INT(tb[GRID].fg, 7, "LIGHT grid fg -> 7");
    ASSERT_INT(tb[GRID].ch, 0x00B7, "LIGHT grid glyph unchanged");
    ASSERT_INT(tb[SB].bg, 15, "LIGHT statusbar bg flips to 15");

    /* DARK: bg stays dark (0), fg brightens (15) — distinct from LIGHT in bg. */
    flow_set_color_mode(t, FLOW_COLOR_DARK);
    flow_render(t, tb, tc, tr);
    ASSERT_INT(flow_color_mode_get(t), FLOW_COLOR_DARK, "mode is DARK");
    ASSERT_INT(tb[CAN].bg, 0, "DARK canvas bg stays 0");
    ASSERT_INT(tb[CAN].fg, 15, "DARK canvas fg brightens to 15");
    ASSERT_INT(tb[CAN].ch, ' ', "DARK swap is color-only (canvas glyph unchanged)");

    /* (3) DEFAULT round-trip is byte-identical — the contract that keeps goldens safe. */
    flow_set_color_mode(t, FLOW_COLOR_DEFAULT);
    flow_render(t, tb, tc, tr);
    ASSERT_INT(tb[CAN].fg, 7, "round-trip canvas fg back to 7"); ASSERT_INT(tb[CAN].bg, 0, "round-trip canvas bg back to 0");
    ASSERT_INT(tb[GRID].fg, 8, "round-trip grid fg back to 8"); ASSERT_INT(tb[GRID].bg, 0, "round-trip grid bg back to 0");
    ASSERT_INT(tb[SB].fg, 7, "round-trip statusbar fg back to 7"); ASSERT_INT(tb[SB].bg, 0, "round-trip statusbar bg back to 0");

    /* (4) node-body color is theme-INDEPENDENT (CLASS B node renderers are NOT
       themed). The node box corner at (0,0) is drawn by flow__default_render via
       FLOW_FG/FLOW_BG and must not move across a theme swap. */
    int NB = 0*tc + 0;
    ASSERT_INT(tb[NB].ch, 0x250C, "node box corner at (0,0)");
    uint8_t nb_fg = tb[NB].fg, nb_bg = tb[NB].bg;
    flow_set_color_mode(t, FLOW_COLOR_DARK);
    flow_render(t, tb, tc, tr);
    ASSERT_INT(tb[NB].fg, nb_fg, "node-body fg unchanged across theme swap (CLASS B)");
    ASSERT_INT(tb[NB].bg, nb_bg, "node-body bg unchanged across theme swap (CLASS B)");

    free(tb); flow_free(t);
  }

  /* (5) handle_valid/handle_invalid/widget_fg/widget_bg are seeded but inert this
     package; the rendered handle ◉ still uses theme.handle (== fg) at DEFAULT. */
  {
    int tc = 30, tr = 8;
    flow_cell *tb = (flow_cell*)malloc((size_t)tc * tr * sizeof(flow_cell));
    flow_t *t = flow_new(tc, tr); flow_register_defaults(t);
    ASSERT_INT(t->theme.handle_valid, 2, "default handle_valid seeded (green, pkg2)");
    ASSERT_INT(t->theme.handle_invalid, 1, "default handle_invalid seeded (red, pkg2)");
    ASSERT_INT(t->theme.widget_fg, 7, "default widget_fg seeded (pkg3-5)");
    ASSERT_INT(t->theme.widget_bg, 0, "default widget_bg seeded (pkg3-5)");
    int id = flow_add_node(t, "default", (flow_pt){2, 2}, (void*)"A");
    flow_select_node(t, id, 0);   /* a selected node reveals its handle markers */
    flow_render(t, tb, tc, tr);
    int found = -1;
    for (int i = 0; i < tc * tr; i++) if (tb[i].ch == 0x25C9) { found = i; break; }
    ASSERT(found >= 0, "selected node renders a handle marker");
    if (found >= 0) ASSERT_INT(tb[found].fg, t->theme.handle, "handle marker uses theme.handle (== fg) at DEFAULT");
    free(tb); flow_free(t);
  }

  free(buf);
  return flowtest_report("test_render");
}
