/* ===== edge routing: orthogonal step router via path-connectivity glyphs ===== */
void flow_route_push(flow_route *r, int x, int y, uint32_t ch);
void flow_route_orthogonal(flow_pt s, flow_pos sp, flow_pt t, flow_pos tp, flow_route *out);
extern const flow_edge_type flow_default_edge_type;

#ifdef FLOW_IMPLEMENTATION
const flow_edge_type flow_default_edge_type = { "default", flow_route_orthogonal };

void flow_route_push(flow_route *r, int x, int y, uint32_t ch) {
  if (r->count >= r->cap) { r->cap = r->cap ? r->cap * 2 : 16; r->cells = (flow_route_cell*)realloc(r->cells, r->cap * sizeof *r->cells); }
  r->cells[r->count].x = x; r->cells[r->count].y = y; r->cells[r->count].ch = ch; r->count++;
}
static void flow__addpt(flow_pt **a, int *n, int *cap, int x, int y) {
  if (*n > 0 && (*a)[*n-1].x == x && (*a)[*n-1].y == y) return;
  if (*n >= *cap) { *cap = *cap ? *cap * 2 : 32; *a = (flow_pt*)realloc(*a, *cap * sizeof(flow_pt)); }
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
  if (n > 0) {                      /* arrowhead at the target end, pointing along the approach */
    int ax, ay;
    if (n >= 2) { ax = p[n-1].x - p[n-2].x; ay = p[n-1].y - p[n-2].y; }
    else        { ax = t.x - s.x;           ay = t.y - s.y; }
    uint32_t arrow = ax > 0 ? 0x25B6 : ax < 0 ? 0x25C0 : ay > 0 ? 0x25BC : 0x25B2;
    out->cells[out->count - 1].ch = arrow;
    out->label_anchor = p[n/2];
  }
  free(p);
}
#endif
