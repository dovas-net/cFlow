/* ===== cells, surface, box, utf8, damage diff ===== */
enum { FLOW_BOLD = 1u, FLOW_REVERSE = 2u, FLOW_DIM = 4u, FLOW_UNDERLINE = 8u };
#define FLOW_FG 7
#define FLOW_BG 0
/* Grid "light" comes from a dim 256-color fg, NOT FLOW_DIM: flow_diff_emit
   serializes only BOLD/REVERSE attrs but always the ;38;5;<fg> color path, so a
   DIM-attr grid would look full-intensity on a real terminal. 8 = bright black.
   Lives here (module 3, ahead of flow_model/flow_render) so flow_new can seed
   theme.grid_fg from it — relocated from flow_render.h. */
#define FLOW_BG_GRID_FG 8

/* engine-chrome theme (inc-7 #1): a fixed, minimal token set. Each field is an
   xterm-256 index (the only color channel flow_diff_emit emits, see :105). NO
   per-element override table (YAGNI). The DEFAULT preset's values EQUAL the legacy
   FLOW_FG/FLOW_BG/FLOW_BG_GRID_FG literals, so converting the render call-sites to
   read f->theme.* is a pure indirection — every existing golden stays byte-identical.
   Transient view-state: never saved, never journaled. */
typedef struct {
  uint8_t fg, bg;               /* canvas foreground / background (clear, edges, handles, ...) */
  uint8_t grid_fg;              /* background-grid dot/line fg (was FLOW_BG_GRID_FG) */
  uint8_t handle;               /* handle marker color (== fg in every preset for now) */
  uint8_t handle_valid;         /* DEFINED here, CONSUMED by pkg2 connect-feedback (green) */
  uint8_t handle_invalid;       /* DEFINED here, CONSUMED by pkg2 connect-feedback (red) */
  uint8_t accent;               /* selection/bold accent — DEFINED, left UNWIRED this increment */
  uint8_t edge_fg;              /* edge path fg (== fg in every preset for now) */
  uint8_t widget_fg, widget_bg; /* DEFINED here, CONSUMED by pkg3-5 controls/toolbars chrome */
} flow_theme;

typedef enum { FLOW_COLOR_DEFAULT, FLOW_COLOR_LIGHT, FLOW_COLOR_DARK } flow_color_mode;

typedef struct { uint32_t ch; uint8_t fg, bg, attr; } flow_cell;

typedef struct { flow_cell *cells; int w, h; } flow_cellbuf;
/* clip_x/clip_y/clip_w/clip_h are in BUFFER coords (an absolute window inside cb);
   flow_put ANDs them in alongside the logical (w/h) and physical (cb-bounds) clips.
   For an unclipped surface set the clip to the full buffer {0,0,cb->w,cb->h}. An
   EMPTY clip (w<=0 or h<=0) suppresses every write — that's how a child fully outside
   its ancestor frame is cut away. */
struct flow_surface { flow_cellbuf *cb; int ox, oy, w, h; int clip_x, clip_y, clip_w, clip_h; };
typedef struct flow_surface flow_surface;

/* route output type — declared here so the edge vtable can reference it; impl in flow_route.h */
typedef struct { uint32_t ch; int x, y; } flow_route_cell;
typedef struct { flow_route_cell *cells; int count, cap; flow_pt label_anchor; } flow_route;

int  flow_utf8(uint32_t cp, char out[5]);
int  flow_utf8_decode(const char *s, uint32_t *cp);
void flow_cellbuf_clear(flow_cellbuf *cb, uint8_t fg, uint8_t bg);
void flow_cellbuf_put(flow_cellbuf *cb, int x, int y, uint32_t ch, uint8_t fg, uint8_t bg, uint8_t attr);
void flow_put(flow_surface *s, int x, int y, uint32_t ch, uint8_t fg, uint8_t bg, uint8_t attr);
void flow_text(flow_surface *s, int x, int y, const char *utf8, uint8_t fg, uint8_t bg, uint8_t attr);
void flow_box(flow_surface *s, int x, int y, int w, int h, uint8_t fg, uint8_t bg, unsigned style);
int  flow_surface_w(const flow_surface *s);
int  flow_surface_h(const flow_surface *s);
char *flow_diff_emit(const flow_cell *front, const flow_cell *back, int cols, int rows);

#ifdef FLOW_IMPLEMENTATION
int flow_utf8(uint32_t cp, char out[5]) {
  if (cp < 0x80) { out[0] = (char)cp; out[1] = 0; return 1; }
  if (cp < 0x800) { out[0] = (char)(0xC0 | (cp >> 6)); out[1] = (char)(0x80 | (cp & 0x3F)); out[2] = 0; return 2; }
  if (cp < 0x10000) { out[0] = (char)(0xE0 | (cp >> 12)); out[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
    out[2] = (char)(0x80 | (cp & 0x3F)); out[3] = 0; return 3; }
  out[0] = (char)(0xF0 | (cp >> 18)); out[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
  out[2] = (char)(0x80 | ((cp >> 6) & 0x3F)); out[3] = (char)(0x80 | (cp & 0x3F)); out[4] = 0; return 4;
}
int flow_utf8_decode(const char *s, uint32_t *cp) {
  const unsigned char *u = (const unsigned char*)s;
  /* Each continuation check short-circuits before the next byte is read, so a
     truncated trailing sequence stops at the NUL instead of over-reading. */
  if (u[0] < 0x80) { *cp = u[0]; return 1; }
  if ((u[0] & 0xE0) == 0xC0) {
    if ((u[1] & 0xC0) != 0x80) { *cp = u[0]; return 1; }
    *cp = ((uint32_t)(u[0]&0x1F)<<6)|(u[1]&0x3F); return 2;
  }
  if ((u[0] & 0xF0) == 0xE0) {
    if ((u[1] & 0xC0) != 0x80 || (u[2] & 0xC0) != 0x80) { *cp = u[0]; return 1; }
    *cp = ((uint32_t)(u[0]&0x0F)<<12)|((uint32_t)(u[1]&0x3F)<<6)|(u[2]&0x3F); return 3;
  }
  if ((u[0] & 0xF8) == 0xF0) {
    if ((u[1]&0xC0)!=0x80 || (u[2]&0xC0)!=0x80 || (u[3]&0xC0)!=0x80) { *cp = u[0]; return 1; }
    *cp = ((uint32_t)(u[0]&0x07)<<18)|((uint32_t)(u[1]&0x3F)<<12)|((uint32_t)(u[2]&0x3F)<<6)|(u[3]&0x3F); return 4;
  }
  *cp = u[0]; return 1;   /* invalid lead byte */
}
void flow_cellbuf_clear(flow_cellbuf *cb, uint8_t fg, uint8_t bg) {
  for (int i = 0; i < cb->w * cb->h; i++) { cb->cells[i].ch = ' '; cb->cells[i].fg = fg; cb->cells[i].bg = bg; cb->cells[i].attr = 0; }
}
void flow_cellbuf_put(flow_cellbuf *cb, int x, int y, uint32_t ch, uint8_t fg, uint8_t bg, uint8_t attr) {
  if (x < 0 || y < 0 || x >= cb->w || y >= cb->h) return;
  flow_cell *c = &cb->cells[y * cb->w + x]; c->ch = ch; c->fg = fg; c->bg = bg; c->attr = attr;
}
void flow_put(flow_surface *s, int x, int y, uint32_t ch, uint8_t fg, uint8_t bg, uint8_t attr) {
  if (x < 0 || y < 0 || x >= s->w || y >= s->h) return;            /* logical clip to node box */
  int bx = s->ox + x, by = s->oy + y;                              /* buffer-space cell */
  if (bx < s->clip_x || by < s->clip_y ||
      bx >= s->clip_x + s->clip_w || by >= s->clip_y + s->clip_h) return; /* clip-rect clip */
  flow_cellbuf_put(s->cb, bx, by, ch, fg, bg, attr);              /* physical clip to buffer  */
}
void flow_text(flow_surface *s, int x, int y, const char *u, uint8_t fg, uint8_t bg, uint8_t attr) {
  int i = 0; while (*u) { uint32_t cp; int n = flow_utf8_decode(u, &cp); u += n; flow_put(s, x + i, y, cp, fg, bg, attr); i++; }
}
void flow_box(flow_surface *s, int x, int y, int w, int h, uint8_t fg, uint8_t bg, unsigned style) {
  uint8_t a = (style & FLOW_BOLD) ? FLOW_BOLD : 0;
  if (w < 2 || h < 2) return;
  flow_put(s, x, y, 0x250C, fg, bg, a);
  flow_put(s, x+w-1, y, 0x2510, fg, bg, a);
  flow_put(s, x, y+h-1, 0x2514, fg, bg, a);
  flow_put(s, x+w-1, y+h-1, 0x2518, fg, bg, a);
  for (int i = 1; i < w-1; i++) { flow_put(s, x+i, y, 0x2500, fg, bg, a); flow_put(s, x+i, y+h-1, 0x2500, fg, bg, a); }
  for (int j = 1; j < h-1; j++) { flow_put(s, x, y+j, 0x2502, fg, bg, a); flow_put(s, x+w-1, y+j, 0x2502, fg, bg, a); }
}
int flow_surface_w(const flow_surface *s) { return s->w; }
int flow_surface_h(const flow_surface *s) { return s->h; }

static void flow__sb(char **out, size_t *cap, size_t *len, const char *s, int n) {
  if (*len + (size_t)n + 1 > *cap) { while (*len + (size_t)n + 1 > *cap) *cap *= 2; *out = (char*)realloc(*out, *cap); }
  memcpy(*out + *len, s, n); *len += n; (*out)[*len] = 0;
}
char *flow_diff_emit(const flow_cell *front, const flow_cell *back, int cols, int rows) {
  size_t cap = 64, len = 0; char *out = (char*)malloc(cap); out[0] = 0;
  int cury = -1, curx = -1, have_style = 0; uint8_t lfg = 0, lbg = 0, lattr = 0;
  char tmp[64], u[5];
  for (int y = 0; y < rows; y++) for (int x = 0; x < cols; x++) {
    int i = y * cols + x; flow_cell b = back[i], f = front[i];
    if (b.ch == f.ch && b.fg == f.fg && b.bg == f.bg && b.attr == f.attr) continue;
    if (y != cury || x != curx) { int n = snprintf(tmp, sizeof tmp, "\x1b[%d;%dH", y+1, x+1); flow__sb(&out,&cap,&len,tmp,n); }
    if (!have_style || b.fg != lfg || b.bg != lbg || b.attr != lattr) {
      int k = 0; k += snprintf(tmp+k, sizeof tmp-k, "\x1b[0");
      if (b.attr & FLOW_BOLD) k += snprintf(tmp+k, sizeof tmp-k, ";1");
      if (b.attr & FLOW_REVERSE) k += snprintf(tmp+k, sizeof tmp-k, ";7");
      k += snprintf(tmp+k, sizeof tmp-k, ";38;5;%u;48;5;%um", b.fg, b.bg);
      flow__sb(&out,&cap,&len,tmp,k); lfg=b.fg; lbg=b.bg; lattr=b.attr; have_style=1;
    }
    uint32_t ch = b.ch ? b.ch : ' '; int n = flow_utf8(ch, u); flow__sb(&out,&cap,&len,u,n);
    curx = x + 1; cury = y;
  }
  return out;
}
#endif
