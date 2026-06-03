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
  /* ---- project/unproject round-trip at zoom 2.0 and 0.5 ---- */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    flow_pt c = { 40, 12 };
    flow_set_zoom(f, 2.0f, c);
    ASSERT(flow_zoom(f) > 1.99f && flow_zoom(f) < 2.01f, "zoom set to 2.0");
    /* world point under c is fixed; projecting it back lands on c (within round) */
    flow_pt w = flow_to_world(f, c);
    flow_pt s = flow_to_screen(f, w);
    ASSERT(s.x - c.x <= 0 && c.x - s.x <= 0 && s.y - c.y <= 0 && c.y - s.y <= 0,
           "zoom 2.0: pointer cell round-trips to itself");
    flow_set_zoom(f, 0.5f, c);
    ASSERT(flow_zoom(f) > 0.49f && flow_zoom(f) < 0.51f, "zoom set to 0.5");
    flow_pt w2 = flow_to_world(f, c);
    flow_pt s2 = flow_to_screen(f, w2);
    ASSERT(s2.x - c.x <= 0 && c.x - s2.x <= 0 && s2.y - c.y <= 0 && c.y - s2.y <= 0,
           "zoom 0.5: pointer cell round-trips to itself");
    flow_free(f);
  }

  /* ---- pointer-centered invariant: node under cursor stays under cursor at 2.0 ---- */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    int id = flow_add_node(f, "default", (flow_pt){20, 8}, (void*)"N");  /* rect (20,8,5,3) */
    flow_node *n = flow_get_node(f, id);
    flow_pt center = { n->pos.x + n->w/2, n->pos.y + n->h/2 };  /* world == screen at zoom 1 */
    ASSERT_INT(flow_hit_node(f, center), id, "hit node at its center (zoom 1)");
    flow_set_zoom(f, 2.0f, center);
    ASSERT_INT(flow_hit_node(f, center), id, "pointer-centered zoom keeps node under cursor (2.0)");
    flow_free(f);
  }

  /* ---- no-drift across repeated pointer-centered calls: the float world point under
     the cursor is preserved (the genuine invariant; constant-size glyphs can drift off
     their geometric centre at high zoom, but the anchored world point must not). ---- */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    flow_pt cur = { 33, 11 };
    flow_viewport v0 = flow_view_get(f);
    float wx = (cur.x - v0.ox) / v0.zoom, wy = (cur.y - v0.oy) / v0.zoom;  /* fixed float anchor */
    for (int i = 0; i < 8; i++) {
      flow_set_zoom(f, flow_zoom(f) * 1.3f, cur);
      flow_viewport v = flow_view_get(f);
      float sx = wx * v.zoom + v.ox, sy = wy * v.zoom + v.oy;  /* project the anchor back to screen */
      float ex = sx - cur.x, ey = sy - cur.y; if (ex < 0) ex = -ex; if (ey < 0) ey = -ey;
      ASSERT(ex < 0.01f && ey < 0.01f, "repeated pointer-centered zoom: anchor world point does not drift");
    }
    flow_free(f);
  }

  /* ---- clamp: extremes saturate to limits; repeated in/out saturate ---- */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    flow_pt c = { 40, 12 };
    flow_set_zoom(f, 100.0f, c);
    ASSERT(flow_zoom(f) > 3.99f && flow_zoom(f) < 4.01f, "zoom 100 clamps to zmax 4.0");
    flow_set_zoom(f, 0.001f, c);
    ASSERT(flow_zoom(f) > 0.24f && flow_zoom(f) < 0.26f, "zoom 0.001 clamps to zmin 0.25");
    for (int i = 0; i < 50; i++) flow_zoom_in(f, c);
    ASSERT(flow_zoom(f) > 3.99f && flow_zoom(f) < 4.01f, "repeated zoom_in saturates zmax");
    for (int i = 0; i < 50; i++) flow_zoom_out(f, c);
    ASSERT(flow_zoom(f) > 0.24f && flow_zoom(f) < 0.26f, "repeated zoom_out saturates zmin");
    /* custom limits */
    flow_set_zoom_limits(f, 0.5f, 2.0f);
    flow_set_zoom(f, 100.0f, c);
    ASSERT(flow_zoom(f) > 1.99f && flow_zoom(f) < 2.01f, "custom zmax 2.0 honored");
    flow_set_zoom(f, 0.001f, c);
    ASSERT(flow_zoom(f) > 0.49f && flow_zoom(f) < 0.51f, "custom zmin 0.5 honored");
    flow_free(f);
  }

  /* ---- fit_view: wide spread fits within margin-padded screen, bounds centered ---- */
  {
    int cols = 80, rows = 24, margin = 2;
    flow_t *f = flow_new(cols, rows); flow_register_defaults(f);
    flow_add_node(f, "default", (flow_pt){200, 100}, (void*)"X");
    flow_add_node(f, "default", (flow_pt){260, 130}, (void*)"Y");
    flow_fit_view(f, margin);
    /* each node's projected rect within [margin, cols-margin] x [margin, rows-margin] */
    for (int i = 0; i < flow_node_count(f); i++) {
      flow_node *n = &flow_nodes(f)[i];
      flow_rect wr = flow_node_rect_abs(f, n);
      flow_pt tl = flow_to_screen(f, (flow_pt){wr.x, wr.y});
      flow_pt br = flow_to_screen(f, (flow_pt){wr.x + wr.w - 1, wr.y + wr.h - 1});
      ASSERT(tl.x >= margin && br.x <= cols - margin, "fit: node within x padding");
      ASSERT(tl.y >= margin && br.y <= rows - margin, "fit: node within y padding");
    }
    /* bounds center projects near screen center */
    flow_rect b = flow_bounds(f);
    flow_pt bc = flow_to_screen(f, (flow_pt){b.x + b.w/2, b.y + b.h/2});
    ASSERT(bc.x - cols/2 <= 1 && cols/2 - bc.x <= 1, "fit: bounds center ~ screen center x");
    ASSERT(bc.y - rows/2 <= 1 && rows/2 - bc.y <= 1, "fit: bounds center ~ screen center y");
    /* empty graph: no-op, no crash */
    flow_t *e = flow_new(cols, rows); flow_register_defaults(e);
    flow_fit_view(e, margin);
    ASSERT_INT(flow_node_count(e), 0, "fit empty no-op");
    flow_free(e); flow_free(f);
  }

  /* ---- hit_node at zoom != 1 (above threshold): full box still hittable ---- */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    int id = flow_add_node(f, "default", (flow_pt){10, 5}, (void*)"A");  /* rect (10,5,5,3) */
    flow_pt c = { 12, 6 };  /* inside, world==screen at zoom 1 */
    flow_set_zoom(f, 2.0f, c);   /* above LOD threshold -> full box */
    /* project the node's top-left; the full footprint must be hittable */
    flow_rect wr = flow_node_rect_abs(f, flow_get_node(f, id));
    flow_pt tl = flow_to_screen(f, (flow_pt){wr.x, wr.y});
    ASSERT_INT(flow_hit_node(f, tl), id, "zoom 2.0: top-left of full box hits");
    flow_pt mid = { tl.x + wr.w/2, tl.y + wr.h/2 };
    ASSERT_INT(flow_hit_node(f, mid), id, "zoom 2.0: interior of full box hits");
    flow_free(f);
  }

  /* ---- hit_node under LOD (zoom 0.5 < 0.6 threshold): footprint == drawn marker ---- */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    int id = flow_add_node(f, "default", (flow_pt){10, 5}, (void*)"A");  /* rect (10,5,5,3) */
    flow_set_zoom(f, 0.5f, (flow_pt){0, 0});   /* below threshold -> collapsed marker */
    ASSERT_INT(flow_lod_for_zoom(f, flow_zoom(f)), 1, "zoom 0.5 is LOD collapsed");
    flow_rect wr = flow_node_rect_abs(f, flow_get_node(f, id));
    flow_pt tl = flow_to_screen(f, (flow_pt){wr.x, wr.y});  /* the collapsed marker cell */
    ASSERT_INT(flow_hit_node(f, tl), id, "LOD: clicking drawn marker cell hits node");
    /* a cell where the FULL box would be (but isn't drawn at LOD) returns -1 */
    flow_pt full_interior = { tl.x + 2, tl.y + 1 };  /* would be box interior at full size */
    ASSERT_INT(flow_hit_node(f, full_interior), -1, "LOD: full-box cell (not drawn) returns -1");
    /* render: only ONE cell of this node is drawn (the collapsed marker) */
    int cols = 80, rows = 24;
    flow_cell *buf = (flow_cell*)malloc((size_t)cols * rows * sizeof(flow_cell));
    flow_render(f, buf, cols, rows);
    /* the cell at tl must be non-blank (the marker); the would-be interior is blank */
    ASSERT(buf[tl.y*cols + tl.x].ch != ' ' && buf[tl.y*cols + tl.x].ch != 0, "LOD: marker drawn at tl");
    ASSERT(buf[full_interior.y*cols + full_interior.x].ch == ' ', "LOD: no full box interior drawn");
    free(buf); flow_free(f);
  }

  /* ---- render_lod_low snapshot: 2-node scene at low zoom (collapsed markers) ---- */
  {
    int cols = 30, rows = 8;
    flow_cell *buf = (flow_cell*)malloc((size_t)cols * rows * sizeof(flow_cell));
    flow_t *f = flow_new(cols, rows); flow_register_defaults(f);
    flow_add_node(f, "default", (flow_pt){2, 2}, (void*)"alpha");
    flow_add_node(f, "default", (flow_pt){20, 8}, (void*)"beta");
    flow_set_zoom(f, 0.4f, (flow_pt){0, 0});   /* well below threshold */
    flow_render(f, buf, cols, rows);
    char *s = cells_to_string(buf, cols, rows);
    SNAPSHOT("render_lod_low", s);
    free(s); free(buf); flow_free(f);
  }

  /* ---- synthetic SGR: Ctrl+wheel zooms toward cursor; plain wheel still pans ---- */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    flow_pt cur = { 40, 12 };
    float z0 = flow_zoom(f);
    flow_pt w_before = flow_to_world(f, cur);
    /* Ctrl+wheel-up: SGR button = 64(wheel)|16(ctrl)|0(up) = 80 at 1-based (41,13) */
    flow_feed(f, "\x1b[<80;41;13M", 12);
    ASSERT(flow_zoom(f) > z0, "Ctrl+wheel-up increases zoom");
    flow_pt w_after = flow_to_world(f, cur);
    ASSERT(w_after.x == w_before.x && w_after.y == w_before.y,
           "Ctrl+wheel zoom keeps world point under cursor fixed");
    /* plain wheel-up: button = 64|0 = 64; zoom unchanged, offset changed (still pans) */
    float z1 = flow_zoom(f);
    flow_viewport v0 = flow_view_get(f);
    flow_feed(f, "\x1b[<64;41;13M", 12);
    ASSERT(flow_zoom(f) == z1, "plain wheel-up does NOT change zoom");
    flow_viewport v1 = flow_view_get(f);
    ASSERT(v1.oy != v0.oy, "plain wheel-up still pans (offset changes)");
    flow_free(f);
  }

  /* ---- '+' then '-' via flow_feed: zoom rises then returns toward original ---- */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    float z0 = flow_zoom(f);
    flow_feed(f, "+", 1);
    float z1 = flow_zoom(f);
    ASSERT(z1 > z0, "'+' increases zoom");
    flow_feed(f, "-", 1);
    float z2 = flow_zoom(f);
    ASSERT(z2 < z1, "'-' decreases zoom");
    ASSERT(z2 - z0 <= 0.001f && z0 - z2 <= 0.001f, "'+' then '-' returns to ~original zoom");
    flow_free(f);
  }

  /* ---- zoom>1: handle markers + edge endpoints stay ATTACHED to constant-size node
     bodies (regression: world-projected anchors used to float offset*(zoom-1) cells
     off the body). NON-CIRCULAR: assert the source endpoint sits at the projected
     top-left + (w-1) [on-body right border] + 1 [outward], NOT + (w-1)*zoom. ---- */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    int a = flow_add_node(f, "default", (flow_pt){0, 1}, (void*)"A");   /* rect (0,1,5,3); RIGHT 'out' @(4,2) */
    int b = flow_add_node(f, "default", (flow_pt){14, 1}, (void*)"B");  /* rect (14,1,5,3); LEFT 'in' @(14,2) */
    int e = flow_add_edge(f, a, b, "out", "in");
    flow_pt center = { 2, 2 };  /* inside A at zoom 1 */
    flow_set_zoom(f, 2.0f, center);
    flow_node *na = flow_get_node(f, a);
    flow_rect war = flow_node_rect_abs(f, na);
    flow_pt a_tl = flow_to_screen(f, (flow_pt){war.x, war.y});   /* A's projected top-left */
    int w = na->w;                                              /* constant glyph width (5) */
    int expect_x = a_tl.x + (w - 1) + 1;                        /* on-body right border + 1 outward */
    int buggy_x  = a_tl.x + (w - 1) * 2 + 1;                    /* old world-projected (zoom 2) */
    flow_pt ep;
    int ok = flow_edge_endpoint_screen(f, flow_get_edge(f, e), 0, &ep);
    ASSERT_INT(ok, 1, "zoom2: source endpoint resolves");
    ASSERT(ep.x - expect_x <= 1 && expect_x - ep.x <= 1,
           "zoom2: source endpoint sits within 1 cell of A's on-body right border");
    ASSERT(ep.x != buggy_x, "zoom2: source endpoint is NOT the old world-projected (drifted) position");
    /* flow_hit_handle at zoom 2 hits the on-body handle cell (A must be hover/selected) */
    flow_set_hover(f, a);
    flow_pt rh = { a_tl.x + (w - 1), a_tl.y + na->h / 2 };      /* RIGHT handle on the constant body */
    int hn = -2; int hi = flow_hit_handle(f, rh, &hn);
    ASSERT(hi >= 0 && hn == a, "zoom2: flow_hit_handle hits A's on-body RIGHT handle cell");
    /* a cell at the OLD drifted handle position must NOT hit the handle */
    flow_pt drifted = { a_tl.x + (w - 1) * 2, a_tl.y + na->h / 2 };
    int dn = -2; int di = flow_hit_handle(f, drifted, &dn);
    ASSERT(!(di >= 0 && dn == a), "zoom2: drifted (world-projected) cell does NOT hit the handle");
    flow_free(f);
  }

  /* ---- zoom2 snapshot: A->B edge meeting both constant-size boxes (eyeball) ---- */
  {
    int cols = 40, rows = 8;
    flow_cell *buf = (flow_cell*)malloc((size_t)cols * rows * sizeof(flow_cell));
    flow_t *f = flow_new(cols, rows); flow_register_defaults(f);
    flow_add_node(f, "default", (flow_pt){0, 0}, (void*)"A");
    flow_add_node(f, "default", (flow_pt){8, 0}, (void*)"B");
    flow_add_edge(f, 1, 2, "out", "in");
    flow_set_zoom(f, 2.0f, (flow_pt){0, 0});
    flow_render(f, buf, cols, rows);
    char *s = cells_to_string(buf, cols, rows);
    SNAPSHOT("render_edge_zoom2", s);
    free(s); free(buf); flow_free(f);
  }

  return flowtest_report("test_zoom");
}
