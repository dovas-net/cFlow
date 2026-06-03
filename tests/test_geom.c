#define FLOW_IMPLEMENTATION
#include "../flow.h"
#include "flowtest.h"
int main(void) {
  flow_viewport v = {0, 0, 1};
  flow_pt p = flow_project(v, (flow_pt){3, 4});
  ASSERT_INT(p.x, 3, "project identity x"); ASSERT_INT(p.y, 4, "project identity y");
  v.ox = 10; v.oy = -2;
  p = flow_project(v, (flow_pt){3, 4});
  ASSERT_INT(p.x, 13, "project offset x"); ASSERT_INT(p.y, 2, "project offset y");
  flow_pt w = flow_unproject(v, (flow_pt){13, 2});
  ASSERT_INT(w.x, 3, "unproject x"); ASSERT_INT(w.y, 4, "unproject y");
  flow_rect r = {0, 0, 4, 3};
  ASSERT(flow_rect_contains(r, (flow_pt){0, 0}), "contains top-left");
  ASSERT(flow_rect_contains(r, (flow_pt){3, 2}), "contains inside");
  ASSERT(!flow_rect_contains(r, (flow_pt){4, 0}), "excludes right edge");
  ASSERT(!flow_rect_contains(r, (flow_pt){0, 3}), "excludes bottom edge");
  flow_rect u = flow_rect_union((flow_rect){0, 0, 2, 2}, (flow_rect){5, 5, 2, 2});
  ASSERT_INT(u.x, 0, "union x"); ASSERT_INT(u.y, 0, "union y");
  ASSERT_INT(u.w, 7, "union w"); ASSERT_INT(u.h, 7, "union h");

  /* flow_rect_intersects: overlap true; disjoint false; edge-touch counts;
     containment implies intersects (symmetric). */
  flow_rect r1 = {0, 0, 4, 4}, r2 = {2, 2, 4, 4};   /* overlap in (2,2)..(3,3) */
  ASSERT(flow_rect_intersects(r1, r2), "overlapping rects intersect");
  ASSERT(flow_rect_intersects(r2, r1), "intersects is symmetric");
  flow_rect r3 = {10, 10, 2, 2};                     /* clearly disjoint from r1 */
  ASSERT(!flow_rect_intersects(r1, r3), "disjoint rects do not intersect");
  ASSERT(!flow_rect_intersects(r3, r1), "disjoint symmetric");
  flow_rect r4 = {4, 0, 4, 4};                        /* r1 right edge x=4 touches r4 left edge x=4 */
  ASSERT(flow_rect_intersects(r1, r4), "edge-touching rects count as intersecting");
  flow_rect big = {0, 0, 10, 10}, small = {3, 3, 2, 2};  /* big contains small */
  ASSERT(flow_rect_intersects(big, small), "containment implies intersects");
  ASSERT(flow_rect_intersects(small, big), "containment implies intersects (sym)");
  return flowtest_report("test_geom");
}
