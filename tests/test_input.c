#define FLOW_IMPLEMENTATION
#include "../flow.h"
#include "flowtest.h"
int main(void) {
  flow_t *f = flow_new(80, 24); flow_register_defaults(f);
  flow_pt base = flow_to_screen(f, (flow_pt){0, 0});
  flow_feed(f, "\x1b[C", 3);  /* right arrow: reveal content to the right -> content shifts left */
  flow_pt right = flow_to_screen(f, (flow_pt){0, 0});
  ASSERT_INT(right.x, base.x - 1, "right arrow pans content left by 1");
  flow_feed(f, "\x1b[D", 3);  /* left arrow undoes it */
  flow_pt back = flow_to_screen(f, (flow_pt){0, 0});
  ASSERT_INT(back.x, base.x, "left arrow pans content right by 1");
  flow_feed(f, "\x1b[B", 3);  /* down arrow */
  flow_pt down = flow_to_screen(f, (flow_pt){0, 0});
  ASSERT_INT(down.y, base.y - 1, "down arrow pans content up by 1");
  flow_free(f);
  return flowtest_report("test_input");
}
