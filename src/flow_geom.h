/* ===== geometry (pure, no I/O) ===== */
typedef struct { int x, y; } flow_pt;
typedef struct { int x, y, w, h; } flow_rect;
typedef enum { FLOW_TOP, FLOW_RIGHT, FLOW_BOTTOM, FLOW_LEFT } flow_pos;
typedef struct { float ox, oy, zoom; } flow_viewport;

flow_pt   flow_project(flow_viewport v, flow_pt world);
flow_pt   flow_unproject(flow_viewport v, flow_pt screen);
int       flow_rect_contains(flow_rect r, flow_pt p);
flow_rect flow_rect_union(flow_rect a, flow_rect b);

#ifdef FLOW_IMPLEMENTATION
flow_pt flow_project(flow_viewport v, flow_pt world) {
  flow_pt s;
  s.x = (int)lroundf(world.x * v.zoom + v.ox);
  s.y = (int)lroundf(world.y * v.zoom + v.oy);
  return s;
}
flow_pt flow_unproject(flow_viewport v, flow_pt screen) {
  float z = (v.zoom == 0.0f) ? 1.0f : v.zoom;
  flow_pt w;
  w.x = (int)lroundf((screen.x - v.ox) / z);
  w.y = (int)lroundf((screen.y - v.oy) / z);
  return w;
}
int flow_rect_contains(flow_rect r, flow_pt p) {
  return p.x >= r.x && p.x < r.x + r.w && p.y >= r.y && p.y < r.y + r.h;
}
flow_rect flow_rect_union(flow_rect a, flow_rect b) {
  int x0 = a.x < b.x ? a.x : b.x, y0 = a.y < b.y ? a.y : b.y;
  int ax1 = a.x + a.w, ay1 = a.y + a.h, bx1 = b.x + b.w, by1 = b.y + b.h;
  int x1 = ax1 > bx1 ? ax1 : bx1, y1 = ay1 > by1 ? ay1 : by1;
  flow_rect r = { x0, y0, x1 - x0, y1 - y0 };
  return r;
}
#endif
