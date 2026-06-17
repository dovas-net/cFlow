/* ===== edge routing: orthogonal step router via path-connectivity glyphs ===== */
void flow_route_push(flow_route *r, int x, int y, uint32_t ch);
void flow_route_orthogonal(flow_pt s, flow_pos sp, flow_pt t, flow_pos tp, flow_route *out);
void flow_route_straight(flow_pt s, flow_pos sp, flow_pt t, flow_pos tp, flow_route *out);
extern const flow_edge_type flow_default_edge_type;
extern const flow_edge_type flow_straight_edge_type;  /* { "straight", flow_route_straight } */

#ifdef FLOW_IMPLEMENTATION
const flow_edge_type flow_default_edge_type  = { "default",  flow_route_orthogonal };
const flow_edge_type flow_straight_edge_type = { "straight", flow_route_straight };

void flow_route_push(flow_route *r, int x, int y, uint32_t ch) {
  if (r->count >= r->cap) {                              /* no-leak realloc: drop the cell on OOM (route truncates) rather than NULL-deref/leak the old buffer */
    int cap = r->cap ? r->cap * 2 : 16;
    flow_route_cell *p = (flow_route_cell*)FLOW_REALLOC(r->cells, (size_t)cap * sizeof *r->cells);
    if (!p) return;                                      /* OOM: keep old buffer; this cell is dropped */
    r->cells = p; r->cap = cap;
  }
  r->cells[r->count].x = x; r->cells[r->count].y = y; r->cells[r->count].ch = ch; r->count++;
}
static void flow__addpt(flow_pt **a, int *n, int *cap, int x, int y) {
  if (*n > 0 && (*a)[*n-1].x == x && (*a)[*n-1].y == y) return;
  if (*n >= *cap) {                                      /* no-leak realloc: drop the point on OOM rather than NULL-deref/leak */
    int c = *cap ? *cap * 2 : 32;
    flow_pt *p = (flow_pt*)FLOW_REALLOC(*a, (size_t)c * sizeof(flow_pt));
    if (!p) return;                                      /* OOM: keep old buffer; this point is dropped */
    *a = p; *cap = c;
  }
  (*a)[*n].x = x; (*a)[*n].y = y; (*n)++;
}
/* direction bit from delta (neighbor relative to cell): N=1 S=2 E=4 W=8 */
static unsigned flow__dir(int dx, int dy) { unsigned m = 0; if (dy < 0) m|=1; if (dy > 0) m|=2; if (dx > 0) m|=4; if (dx < 0) m|=8; return m; }
static uint32_t flow__glyph(unsigned m) {
  switch (m) {
    case 4: case 8: case 12: return 0x2500;   /* - (E,W,EW) */
    case 1: case 2: case 3:  return 0x2502;   /* | (N,S,NS) */
    case 6:  return 0x250C;  /* E|S */
    case 10: return 0x2510;  /* W|S */
    case 5:  return 0x2514;  /* E|N */
    case 9:  return 0x2518;  /* W|N */
    default: return 0x2500;
  }
}
void flow_route_orthogonal(flow_pt s, flow_pos sp, flow_pt t, flow_pos tp, flow_route *out) {
  (void)sp; (void)tp;
  flow_pt *p = NULL; int n = 0, cap = 0;
  int dx = t.x - s.x, dy = t.y - s.y, adx = dx<0?-dx:dx, ady = dy<0?-dy:dy;
  int sx = dx >= 0 ? 1 : -1, sy = dy >= 0 ? 1 : -1;
  if (adx >= ady) {                 /* H-V-H, bend at mid column */
    int mx = (s.x + t.x) / 2;
    for (int x = s.x; ; x += sx) { flow__addpt(&p,&n,&cap,x,s.y); if (x == mx) break; }
    for (int y = s.y; ; y += sy) { flow__addpt(&p,&n,&cap,mx,y); if (y == t.y) break; }
    for (int x = mx; ; x += sx) { flow__addpt(&p,&n,&cap,x,t.y); if (x == t.x) break; }
  } else {                          /* V-H-V, bend at mid row */
    int my = (s.y + t.y) / 2;
    for (int y = s.y; ; y += sy) { flow__addpt(&p,&n,&cap,s.x,y); if (y == my) break; }
    for (int x = s.x; ; x += sx) { flow__addpt(&p,&n,&cap,x,my); if (x == t.x) break; }
    for (int y = my; ; y += sy) { flow__addpt(&p,&n,&cap,t.x,y); if (y == t.y) break; }
  }
  for (int i = 0; i < n; i++) {
    unsigned m = 0;
    if (i > 0)     m |= flow__dir(p[i-1].x - p[i].x, p[i-1].y - p[i].y);
    if (i < n - 1) m |= flow__dir(p[i+1].x - p[i].x, p[i+1].y - p[i].y);
    flow_route_push(out, p[i].x, p[i].y, flow__glyph(m));
  }
  if (out->count > 0) {             /* arrowhead at the target end, pointing along the approach (count>0 ⟹ n>0, so p[] is valid; guards against OOM-dropped pushes leaving count==0) */
    int ax, ay;
    if (n >= 2) { ax = p[n-1].x - p[n-2].x; ay = p[n-1].y - p[n-2].y; }
    else        { ax = t.x - s.x;           ay = t.y - s.y; }
    uint32_t arrow = ax > 0 ? 0x25B6 : ax < 0 ? 0x25C0 : ay > 0 ? 0x25BC : 0x25B2;
    out->cells[out->count - 1].ch = arrow;
    out->label_anchor = p[n/2];
  }
  FLOW_FREE(p);
}
/* STRAIGHT: direct integer-DDA line s->t, stepping the dominant axis by one cell
   per step (diagonal steps allowed). Glyphs: ─ horizontal, │ vertical, ╲/╱ diagonal.
   Arrowhead at the target end (same convention as orthogonal). label_anchor = midpoint.
   Same heap/route-cell ownership as flow_route_orthogonal (caller frees out->cells). */
void flow_route_straight(flow_pt s, flow_pos sp, flow_pt t, flow_pos tp, flow_route *out) {
  (void)sp; (void)tp;
  int dx = t.x - s.x, dy = t.y - s.y;
  int adx = dx < 0 ? -dx : dx, ady = dy < 0 ? -dy : dy;
  int sx = dx > 0 ? 1 : dx < 0 ? -1 : 0, sy = dy > 0 ? 1 : dy < 0 ? -1 : 0;
  int N = adx > ady ? adx : ady;
  if (N == 0) {                       /* degenerate s==t: exactly one cell */
    flow_route_push(out, s.x, s.y, 0x2500);
    out->label_anchor = s;
    return;
  }
  int px = s.x, py = s.y;
  for (int i = 0; i <= N; i++) {
    int x = s.x + sx * ((adx * i + N / 2) / N);
    int y = s.y + sy * ((ady * i + N / 2) / N);
    uint32_t ch;
    if (i == 0) ch = 0x2500;          /* provisional; fixed up from the next step */
    else {
      int ddx = x - px, ddy = y - py;
      if (ddy == 0)      ch = 0x2500;                 /* ─ horizontal */
      else if (ddx == 0) ch = 0x2502;                 /* │ vertical   */
      else if ((ddx > 0) == (ddy > 0)) ch = 0x2572;   /* ╲ down-right / up-left */
      else               ch = 0x2571;                 /* ╱ down-left / up-right */
      if (out->count > 0) out->cells[out->count - 1].ch = ch;  /* glyph of a cell = the step LEAVING it (guard: i=0 push may have been OOM-dropped) */
    }
    flow_route_push(out, x, y, ch);
    px = x; py = y;
  }
  if (out->count > 0) {               /* arrowhead at the target end, by approach direction */
    int ax, ay;
    if (out->count >= 2) { ax = out->cells[out->count-1].x - out->cells[out->count-2].x;
                           ay = out->cells[out->count-1].y - out->cells[out->count-2].y; }
    else                 { ax = dx; ay = dy; }
    uint32_t arrow = ax > 0 ? 0x25B6 : ax < 0 ? 0x25C0 : ay > 0 ? 0x25BC : 0x25B2;
    out->cells[out->count - 1].ch = arrow;
  }
  out->label_anchor = (flow_pt){ (s.x + t.x) / 2, (s.y + t.y) / 2 };
}
#endif
