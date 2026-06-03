#define FLOW_IMPLEMENTATION
#include "../flow.h"
#include "flowtest.h"

static int is_box_glyph(uint32_t c) {
  return c==0x2500||c==0x2502||c==0x250C||c==0x2510||c==0x2514||c==0x2518
       ||c==0x25B6||c==0x25C0||c==0x25BC||c==0x25B2;
}
int main(void) {
  flow_route r = {0};
  /* horizontal: source (0,0) -> target (6,0) */
  flow_route_orthogonal((flow_pt){0,0}, FLOW_RIGHT, (flow_pt){6,0}, FLOW_LEFT, &r);
  ASSERT(r.count > 0, "horizontal route has cells");
  int all_y0 = 1; for (int i = 0; i < r.count; i++) if (r.cells[i].y != 0) all_y0 = 0;
  ASSERT(all_y0, "horizontal route stays on row 0");
  ASSERT_INT(r.cells[r.count-1].ch, 0x25B6, "last cell is right-arrow");
  for (int i = 0; i < r.count; i++) ASSERT(is_box_glyph(r.cells[i].ch), "only box/arrow glyphs");
  free(r.cells); r = (flow_route){0};

  /* vertical: (0,0) -> (0,6) */
  flow_route_orthogonal((flow_pt){0,0}, FLOW_BOTTOM, (flow_pt){0,6}, FLOW_TOP, &r);
  int all_x0 = 1; for (int i = 0; i < r.count; i++) if (r.cells[i].x != 0) all_x0 = 0;
  ASSERT(all_x0, "vertical route stays on col 0");
  ASSERT_INT(r.cells[r.count-1].ch, 0x25BC, "last cell is down-arrow");
  free(r.cells); r = (flow_route){0};

  /* diagonal: connectivity (each step differs by 1 on exactly one axis) */
  flow_route_orthogonal((flow_pt){0,0}, FLOW_RIGHT, (flow_pt){6,4}, FLOW_LEFT, &r);
  ASSERT(r.count > 0, "diagonal route has cells");
  int connected = 1;
  for (int i = 1; i < r.count; i++) {
    int dx = abs(r.cells[i].x - r.cells[i-1].x), dy = abs(r.cells[i].y - r.cells[i-1].y);
    if (!((dx == 1 && dy == 0) || (dx == 0 && dy == 1))) connected = 0;
  }
  ASSERT(connected, "diagonal route is orthogonally connected");
  free(r.cells); r = (flow_route){0};

  /* ---- straight router ---- */
  /* pure-horizontal: (0,0)->(6,0); all non-arrow cells are ─, last is right-arrow */
  flow_route_straight((flow_pt){0,0}, FLOW_RIGHT, (flow_pt){6,0}, FLOW_LEFT, &r);
  ASSERT_INT(r.count, 7, "straight horizontal has |dx|+1 cells");
  { int y0 = 1, dash = 1;
    for (int i = 0; i < r.count; i++) { if (r.cells[i].y != 0) y0 = 0;
      if (i < r.count-1 && r.cells[i].ch != 0x2500) dash = 0; }
    ASSERT(y0, "straight horizontal stays on row 0");
    ASSERT(dash, "straight horizontal body is ─ glyphs"); }
  ASSERT_INT(r.cells[r.count-1].ch, 0x25B6, "straight horizontal ends in right-arrow");
  ASSERT_INT(r.label_anchor.x, 3, "straight horizontal label_anchor at midpoint x");
  ASSERT_INT(r.label_anchor.y, 0, "straight horizontal label_anchor at midpoint y");
  free(r.cells); r = (flow_route){0};

  /* pure-vertical: (0,0)->(0,6); all non-arrow cells are │, last is down-arrow */
  flow_route_straight((flow_pt){0,0}, FLOW_BOTTOM, (flow_pt){0,6}, FLOW_TOP, &r);
  ASSERT_INT(r.count, 7, "straight vertical has |dy|+1 cells");
  { int x0 = 1, bar = 1;
    for (int i = 0; i < r.count; i++) { if (r.cells[i].x != 0) x0 = 0;
      if (i < r.count-1 && r.cells[i].ch != 0x2502) bar = 0; }
    ASSERT(x0, "straight vertical stays on col 0");
    ASSERT(bar, "straight vertical body is │ glyphs"); }
  ASSERT_INT(r.cells[r.count-1].ch, 0x25BC, "straight vertical ends in down-arrow");
  free(r.cells); r = (flow_route){0};

  /* true diagonal: (0,0)->(6,4); monotonic progress in both axes, count == max(|dx|,|dy|)+1 */
  flow_route_straight((flow_pt){0,0}, FLOW_RIGHT, (flow_pt){6,4}, FLOW_LEFT, &r);
  ASSERT_INT(r.count, 7, "straight diagonal count == max(|dx|,|dy|)+1");
  { int mono = 1;
    for (int i = 1; i < r.count; i++) {
      int ddx = r.cells[i].x - r.cells[i-1].x, ddy = r.cells[i].y - r.cells[i-1].y;
      if (ddx < 0 || ddy < 0) mono = 0;                 /* never step backwards */
      if (ddx > 1 || ddy > 1) mono = 0;                 /* at most one cell per axis per step */
      if (ddx == 0 && ddy == 0) mono = 0;               /* always advance */
    }
    ASSERT(mono, "straight diagonal steps progress monotonically in both axes"); }
  ASSERT_INT(r.cells[0].x, 0, "straight diagonal starts at source x");
  ASSERT_INT(r.cells[0].y, 0, "straight diagonal starts at source y");
  ASSERT_INT(r.cells[r.count-1].x, 6, "straight diagonal reaches target x");
  ASSERT_INT(r.cells[r.count-1].y, 4, "straight diagonal reaches target y");
  ASSERT_INT(r.cells[r.count-1].ch, 0x25B6, "straight diagonal ends in right-arrow");
  ASSERT_INT(r.label_anchor.x, 3, "straight diagonal label_anchor midpoint x");
  ASSERT_INT(r.label_anchor.y, 2, "straight diagonal label_anchor midpoint y");
  free(r.cells); r = (flow_route){0};

  /* degenerate s==t pushes exactly one cell */
  flow_route_straight((flow_pt){4,4}, FLOW_RIGHT, (flow_pt){4,4}, FLOW_LEFT, &r);
  ASSERT_INT(r.count, 1, "straight s==t pushes exactly one cell");
  ASSERT_INT(r.cells[0].x, 4, "straight s==t cell at source x");
  ASSERT_INT(r.cells[0].y, 4, "straight s==t cell at source y");
  ASSERT_INT(r.label_anchor.x, 4, "straight s==t label_anchor x");
  ASSERT_INT(r.label_anchor.y, 4, "straight s==t label_anchor y");
  free(r.cells);

  return flowtest_report("test_route");
}
