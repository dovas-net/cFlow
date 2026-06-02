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
  free(r.cells);
  return flowtest_report("test_route");
}
