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
  /* utf8 round-trip */
  char u[5]; int n = flow_utf8(0x250C, u); ASSERT_INT(n, 3, "box char is 3 bytes");
  uint32_t cp; int m = flow_utf8_decode("A", &cp); ASSERT_INT(m, 1, "ascii 1 byte"); ASSERT_INT(cp, 'A', "ascii cp");
  m = flow_utf8_decode("\xe2\x94\x8c", &cp); ASSERT_INT(m, 3, "box decode bytes"); ASSERT_INT(cp, 0x250C, "box decode cp");
  /* truncated trailing multibyte: a lone 3-byte lead before NUL must not over-read */
  m = flow_utf8_decode("\xe2", &cp); ASSERT_INT(m, 1, "truncated lead consumes 1 byte"); ASSERT_INT(cp, 0xe2, "truncated lead cp");
  m = flow_utf8_decode("\xe2\x94", &cp); ASSERT_INT(m, 1, "lead+1cont-before-NUL consumes 1 byte");

  /* surface put + box + text into a cell buffer */
  flow_cell buf[10 * 4];
  flow_cellbuf cb = { buf, 10, 4 };
  flow_cellbuf_clear(&cb, FLOW_FG, FLOW_BG);
  flow_surface s = { &cb, 0, 0, 6, 3, 0, 0, 10, 4 };  /* full-buffer clip (cb is 10x4) */
  flow_box(&s, 0, 0, 6, 3, FLOW_FG, FLOW_BG, 0);
  flow_text(&s, 2, 1, "hi", FLOW_FG, FLOW_BG, 0);
  ASSERT_INT(buf[0].ch, 0x250C, "top-left corner");
  ASSERT_INT(buf[5].ch, 0x2510, "top-right corner");
  ASSERT_INT(buf[1*10+2].ch, 'h', "label h"); ASSERT_INT(buf[1*10+3].ch, 'i', "label i");

  /* off-surface writes are clipped, not crashes */
  flow_put(&s, 100, 100, 'Z', FLOW_FG, FLOW_BG, 0);
  flow_put(&s, -5, -5, 'Z', FLOW_FG, FLOW_BG, 0);

  char *str = cells_to_string(buf, 10, 4);
  SNAPSHOT("cell_box_hi", str);   /* expect:  ┌────┐ / │ hi │ / └────┘ */
  free(str);

  /* diff emitter */
  flow_cell front[6], back[6];
  for (int i = 0; i < 6; i++) { front[i] = back[i] = (flow_cell){ ' ', FLOW_FG, FLOW_BG, 0 }; }
  char *e = flow_diff_emit(front, back, 3, 2); ASSERT_STR(e, "", "identical -> empty diff"); free(e);
  back[1*3 + 2].ch = 'X';   /* 0-based row1 col2 -> 1-based 2;3 */
  e = flow_diff_emit(front, back, 3, 2);
  ASSERT(strstr(e, "\x1b[2;3H") != NULL, "cursor to 2;3");
  ASSERT(strstr(e, "X") != NULL, "emits changed glyph");
  free(e);

  return flowtest_report("test_cell");
}
