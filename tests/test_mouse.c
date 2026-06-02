#define FLOW_IMPLEMENTATION
#include "../flow.h"
#include "flowtest.h"

int main(void) {
  flow_mouse_event ev;
  /* press: ESC [ < 0 ; 5 ; 3 M  -> left press at 1-based (5,3) => 0-based (4,2) */
  int u = flow_parse_mouse("\x1b[<0;5;3M", 9, &ev);
  ASSERT_INT(u, 9, "press consumed 9 bytes");
  ASSERT_INT(ev.type, FLOW_MOUSE_PRESS, "press type");
  ASSERT_INT(ev.button, 0, "left button");
  ASSERT_INT(ev.x, 4, "x 0-based"); ASSERT_INT(ev.y, 2, "y 0-based");
  /* release: lowercase m */
  flow_parse_mouse("\x1b[<0;5;3m", 9, &ev); ASSERT_INT(ev.type, FLOW_MOUSE_RELEASE, "release type");
  /* motion: button code 32 (motion bit) */
  flow_parse_mouse("\x1b[<32;5;3M", 10, &ev); ASSERT_INT(ev.type, FLOW_MOUSE_MOTION, "motion type");
  /* wheel up: code 64 */
  flow_parse_mouse("\x1b[<64;5;3M", 10, &ev); ASSERT_INT(ev.type, FLOW_MOUSE_WHEEL, "wheel type");
  ASSERT_INT(ev.button, 0, "wheel up -> button 0");
  /* modifiers: 0 + 4(shift) = 4 */
  flow_parse_mouse("\x1b[<4;5;3M", 9, &ev); ASSERT_INT(ev.mods & FLOW_MOD_SHIFT, FLOW_MOD_SHIFT, "shift mod");
  /* not a mouse sequence */
  ASSERT_INT(flow_parse_mouse("\x1b[A", 3, &ev), 0, "arrow key is not a mouse seq");

  /* ---- integration: drag a node ---- */
  flow_t *f = flow_new(80, 24); flow_register_defaults(f);
  int a = flow_add_node(f, "default", (flow_pt){10, 5}, (void*)"A");  /* rect (10,5,5,3), screen==world */
  /* press inside node A at screen (11,6) => SGR 1-based (12,7) */
  flow_feed(f, "\x1b[<0;12;7M", 10);
  /* drag to screen (21,6) => SGR (22,7): dx=+10 */
  flow_feed(f, "\x1b[<32;22;7M", 11);
  ASSERT_INT(flow_get_node(f, a)->pos.x, 20, "node dragged +10 in x");
  ASSERT_INT(flow_get_node(f, a)->pos.y, 5, "node y unchanged");
  flow_feed(f, "\x1b[<0;22;7m", 10);   /* release */

  /* ---- integration: drag empty canvas to pan ---- */
  flow_pt before = flow_to_screen(f, (flow_pt){0, 0});
  flow_feed(f, "\x1b[<0;51;11M", 11);   /* press empty at screen (50,10) */
  flow_feed(f, "\x1b[<32;54;11M", 12);  /* drag to (53,10): dx=+3 */
  flow_pt after = flow_to_screen(f, (flow_pt){0, 0});
  ASSERT_INT(after.x, before.x + 3, "pan follows drag +3");
  ASSERT_INT(after.y, before.y, "pan y unchanged");
  flow_feed(f, "\x1b[<0;54;11m", 11);   /* release */

  /* after release, a stray motion does nothing */
  flow_pt held = flow_to_screen(f, (flow_pt){0, 0});
  flow_feed(f, "\x1b[<32;60;11M", 12);
  ASSERT_INT(flow_to_screen(f, (flow_pt){0,0}).x, held.x, "no pan after release");

  flow_free(f);
  return flowtest_report("test_mouse");
}
