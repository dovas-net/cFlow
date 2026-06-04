/* ===== viewport mutators: pointer-centered zoom, zoom limits, LOD wrapper ===== */
void  flow_set_zoom(flow_t *f, float zoom, flow_pt screen_center); /* clamps to [zmin,zmax]; keeps the cell under screen_center fixed */
void  flow_zoom_in(flow_t *f, flow_pt screen_center);             /* multiply zoom by FLOW_ZOOM_STEP, pointer-centered */
void  flow_zoom_out(flow_t *f, flow_pt screen_center);            /* divide  zoom by FLOW_ZOOM_STEP, pointer-centered */
float flow_zoom(flow_t *f);                                       /* current viewport zoom */
void  flow_set_zoom_limits(flow_t *f, float zmin, float zmax);    /* override the default [zmin,zmax] clamp range */
int   flow_lod_for_zoom(flow_t *f, float zoom);                   /* public form of the shared LOD helper: 0 = full, 1 = collapsed */

#ifdef FLOW_IMPLEMENTATION
float flow_zoom(flow_t *f) { return f->view.zoom; }
void flow_set_zoom_limits(flow_t *f, float zmin, float zmax) {
  if (zmin <= 0.0f) zmin = FLOW_ZOOM_MIN;       /* guard against non-positive scale */
  if (zmax < zmin) zmax = zmin;
  f->zmin = zmin; f->zmax = zmax;
  flow_set_zoom(f, f->view.zoom, (flow_pt){ f->cols / 2, f->rows / 2 }); /* re-clamp current zoom */
}
void flow_set_zoom(flow_t *f, float zoom, flow_pt screen_center) {
  float z0 = f->view.zoom == 0.0f ? 1.0f : f->view.zoom;
  /* FLOAT anchor (never flow_to_world, which rounds to int): the exact world point
     under screen_center stays under it after the scale change. */
  float wx = (screen_center.x - f->view.ox) / z0;
  float wy = (screen_center.y - f->view.oy) / z0;
  float z1 = zoom;
  if (z1 < f->zmin) z1 = f->zmin;
  if (z1 > f->zmax) z1 = f->zmax;
  f->view.ox = screen_center.x - wx * z1;
  f->view.oy = screen_center.y - wy * z1;
  f->view.zoom = z1;
  flow__clamp_view_offset(f);   /* translate_extent clamp AFTER the zoom write (zoom-aware range) */
}
void flow_zoom_in(flow_t *f, flow_pt screen_center)  { flow_set_zoom(f, f->view.zoom * FLOW_ZOOM_STEP, screen_center); }
void flow_zoom_out(flow_t *f, flow_pt screen_center) { flow_set_zoom(f, f->view.zoom / FLOW_ZOOM_STEP, screen_center); }
int flow_lod_for_zoom(flow_t *f, float zoom) { return flow__lod_for(f, zoom); }
#endif
