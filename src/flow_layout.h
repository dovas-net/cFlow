/* ===== auto-layout: stateless model -> positions transform (spec §10) =====
   flow_layout(f, opts) arranges the graph through the PUBLIC mutators only:
   re-measures via flow_measure_node, writes positions via flow_move_node (the sole
   position-write path), optionally frames via flow_fit_view. Holds no engine state —
   no field on struct flow. Grouped nodes (parent != -1) are laid out among their
   siblings in parent-LOCAL space and committed as parent_abs + local through
   flow_move_node's absolute-in contract; the container itself is laid out as an
   ordinary node of ITS parent's partition and is never resized.
   Fully deterministic: insertion-order iteration, index/id tie-breaks, RNG-free
   circular initial placement; float math rounds to cells only at commit.
   The whole commit is bracketed in flow__undo_begin/end, so one layout == ONE
   undo step (the seam spec §7/#8 left open — undo landed first, the bracket
   falls to layout). */

typedef enum { FLOW_LAYOUT_FORCE, FLOW_LAYOUT_LAYERED } flow_layout_mode;
typedef enum { FLOW_LR, FLOW_TB } flow_layered_dir;

/* Zero-init is a valid request: {0} == FORCE mode with every param defaulted. */
typedef struct {
  flow_layout_mode mode;       /* FORCE (default 0) or LAYERED */
  int      iterations;         /* FORCE: <=0 -> ~200 */
  float    k;                  /* FORCE: ideal edge length in cells; <=0 -> derived from node sizes */
  float    gravity;            /* FORCE: centroid pull keeping components bounded; <=0 -> default
                                  ({0} must mean defaults, so 0 cannot mean "no gravity") */
  unsigned seed;               /* reserved for future jitter; the default path is RNG-free */
  flow_layered_dir dir;        /* LAYERED: FLOW_LR (default 0) or FLOW_TB */
  int      gap_x, gap_y;       /* LAYERED: inter-node / inter-rank spacing in cells; <=0 -> 4 */
  int      fit_after;          /* nonzero -> flow_fit_view(f, margin) after committing */
  int      margin;
} flow_layout_opts;

void flow_layout(flow_t *f, flow_layout_opts opts);   /* PRIMARY entry; mutates positions via flow_move_node */

/* Thin spec-literal wrappers (spec §10 shapes) over flow_layout: */
typedef struct { int iterations; float k; float gravity; } flow_force_opts;
void flow_layout_force(flow_t *f, flow_force_opts opts);                          /* -> flow_layout FORCE */
void flow_layout_layered(flow_t *f, flow_layered_dir dir, int gap_x, int gap_y);  /* -> flow_layout LAYERED */

#ifdef FLOW_IMPLEMENTATION
typedef struct { int a, b; } flow__lay_edge;            /* one intra-partition edge, LOCAL member indices */
typedef struct { float bary; int id, v; } flow__lay_ord; /* barycenter sort entry; unique id = total order */

static int flow__lay_ord_cmp(const void *pa, const void *pb) {
  const flow__lay_ord *a = (const flow__lay_ord*)pa, *b = (const flow__lay_ord*)pb;
  if (a->bary < b->bary) return -1;
  if (a->bary > b->bary) return 1;
  return a->id - b->id;                                 /* unique final tiebreak: qsort is unstable */
}

/* LAYERED (Sugiyama-style): (1) deterministic back-edge drop via iterative DFS in
   index order, (2) longest-path ranks via Kahn (smallest local index first),
   (3) a few barycenter ordering sweeps (ties by node id), (4) integer coordinate
   assignment — ranks march along the flow axis by max extent + gap, nodes stack
   along the cross axis by extent + gap, each rank centered on the widest one.
   Coordinates are PURE INTEGER (float only in the barycenter sort key) so the
   committed cells are identical across -O2 and sanitizer builds. Writes local
   top-left positions into out[]. */
static void flow__layout_layered(flow_t *f, const int *mem, int m,
                                 const flow__lay_edge *ed, int ne,
                                 flow_layered_dir dir, int gx, int gy, flow_pt *out) {
  /* out-adjacency in edge-array order (offset/list form keeps edge order) */
  int *outdeg = (int*)calloc((size_t)m, sizeof(int));
  int *adjoff = (int*)malloc((size_t)(m + 1) * sizeof(int));
  int *adjlist = ne ? (int*)malloc((size_t)ne * sizeof(int)) : NULL;
  char *drop = (char*)calloc((size_t)(ne ? ne : 1), 1);
  for (int e = 0; e < ne; e++) outdeg[ed[e].a]++;
  adjoff[0] = 0;
  for (int v = 0; v < m; v++) adjoff[v + 1] = adjoff[v] + outdeg[v];
  { int *cur = (int*)calloc((size_t)m, sizeof(int));
    for (int e = 0; e < ne; e++) adjlist[adjoff[ed[e].a] + cur[ed[e].a]++] = e;
    free(cur); }
  /* (1) back-edge drop: iterative DFS, roots and out-edges in index/edge order.
     An edge into an ON-STACK node closes a cycle -> dropped (deterministically,
     by traversal order) so rank assignment can never loop. */
  char *state = (char*)calloc((size_t)m, 1);            /* 0 unvisited, 1 on-stack, 2 done */
  int *stkv = (int*)malloc((size_t)m * sizeof(int));
  int *stke = (int*)malloc((size_t)m * sizeof(int));
  for (int r = 0; r < m; r++) {
    if (state[r]) continue;
    int sp = 0; stkv[0] = r; stke[0] = 0; state[r] = 1; sp = 1;
    while (sp > 0) {
      int v = stkv[sp - 1];
      if (stke[sp - 1] < outdeg[v]) {
        int e = adjlist[adjoff[v] + stke[sp - 1]++];
        int t = ed[e].b;
        if (state[t] == 1) drop[e] = 1;                 /* back-edge */
        else if (state[t] == 0) { state[t] = 1; stkv[sp] = t; stke[sp] = 0; sp++; }
      } else { state[v] = 2; sp--; }
    }
  }
  /* (2) longest-path ranks: Kahn over kept edges, smallest ready index first */
  int *indeg = (int*)calloc((size_t)m, sizeof(int));
  int *rank = (int*)calloc((size_t)m, sizeof(int));
  char *done = (char*)calloc((size_t)m, 1);
  for (int e = 0; e < ne; e++) if (!drop[e]) indeg[ed[e].b]++;
  for (int processed = 0; processed < m; processed++) {
    int v = -1;
    for (int i = 0; i < m; i++) if (!done[i] && indeg[i] == 0) { v = i; break; }
    if (v == -1) for (int i = 0; i < m; i++) if (!done[i]) { v = i; break; } /* unreachable after the DFS drop; terminates regardless */
    done[v] = 1;
    for (int j = 0; j < outdeg[v]; j++) {
      int e = adjlist[adjoff[v] + j];
      if (drop[e]) continue;
      int t = ed[e].b;
      if (rank[v] + 1 > rank[t]) rank[t] = rank[v] + 1;
      indeg[t]--;
    }
  }
  int maxrank = 0;
  for (int i = 0; i < m; i++) if (rank[i] > maxrank) maxrank = rank[i];
  int R = maxrank + 1;
  /* (3) rank buckets (initial order: ascending local index) + barycenter sweeps */
  int *rcount = (int*)calloc((size_t)R, sizeof(int));
  int *roff = (int*)malloc((size_t)(R + 1) * sizeof(int));
  int *rlist = (int*)malloc((size_t)m * sizeof(int));
  int *pos = (int*)malloc((size_t)m * sizeof(int));     /* node -> position within its rank */
  for (int i = 0; i < m; i++) rcount[rank[i]]++;
  roff[0] = 0;
  for (int r = 0; r < R; r++) roff[r + 1] = roff[r] + rcount[r];
  { int *cur = (int*)calloc((size_t)R, sizeof(int));
    for (int i = 0; i < m; i++) { int r = rank[i]; rlist[roff[r] + cur[r]] = i; pos[i] = cur[r]++; }
    free(cur); }
  flow__lay_ord *ord = (flow__lay_ord*)malloc((size_t)m * sizeof *ord);
  for (int sweep = 0; sweep < 4; sweep++) {             /* alternate down/up sweeps */
    int down = !(sweep & 1);
    for (int rr = 0; rr < R; rr++) {
      int r = down ? rr : R - 1 - rr;
      int adj = down ? r - 1 : r + 1;
      if (adj < 0 || adj >= R) continue;
      int cnt = rcount[r];
      for (int i = 0; i < cnt; i++) {
        int v = rlist[roff[r] + i];
        float sum = 0; int nb = 0;                      /* neighbors in the adjacent rank, either direction */
        for (int e = 0; e < ne; e++) {
          int u = -1;
          if (ed[e].a == v) u = ed[e].b;
          else if (ed[e].b == v) u = ed[e].a;
          if (u >= 0 && rank[u] == adj) { sum += (float)pos[u]; nb++; }
        }
        ord[i].bary = nb ? sum / (float)nb : (float)pos[v];  /* no neighbors: hold position */
        ord[i].id = f->nodes[mem[v]].id; ord[i].v = v;
      }
      qsort(ord, (size_t)cnt, sizeof *ord, flow__lay_ord_cmp);
      for (int i = 0; i < cnt; i++) { rlist[roff[r] + i] = ord[i].v; pos[ord[i].v] = i; }
    }
  }
  /* (4) integer coordinates. Flow axis: ranks march by max extent + gap (LR: gap_x,
     TB: gap_y). Cross axis: stack by extent + the other gap, centering each rank
     against the tallest/widest one (integer division — deterministic). */
  int *ext = (int*)calloc((size_t)R, sizeof(int));      /* per-rank flow-axis extent */
  int *tot = (int*)calloc((size_t)R, sizeof(int));      /* per-rank cross-axis total */
  int maxtot = 0;
  for (int r = 0; r < R; r++) {
    for (int i = 0; i < rcount[r]; i++) {
      flow_node *n = &f->nodes[mem[rlist[roff[r] + i]]];
      int fe = dir == FLOW_LR ? n->w : n->h;            /* flow-axis extent */
      int ce = dir == FLOW_LR ? n->h : n->w;            /* cross-axis extent */
      if (fe > ext[r]) ext[r] = fe;
      tot[r] += ce + (i ? (dir == FLOW_LR ? gy : gx) : 0);
    }
    if (tot[r] > maxtot) maxtot = tot[r];
  }
  int off = 0;
  for (int r = 0; r < R; r++) {
    int c = (maxtot - tot[r]) / 2;
    for (int i = 0; i < rcount[r]; i++) {
      int v = rlist[roff[r] + i];
      flow_node *n = &f->nodes[mem[v]];
      if (dir == FLOW_LR) { out[v].x = off; out[v].y = c; c += n->h + gy; }
      else                { out[v].x = c; out[v].y = off; c += n->w + gx; }
    }
    off += ext[r] + (dir == FLOW_LR ? gx : gy);
  }
  free(outdeg); free(adjoff); free(adjlist); free(drop); free(state); free(stkv); free(stke);
  free(indeg); free(rank); free(done); free(rcount); free(roff); free(rlist); free(pos);
  free(ord); free(ext); free(tot);
}

/* FORCE (Fruchterman-Reingold): deterministic circular initial placement by local
   index (RNG-free — coincident-start degeneracy is impossible), ~200 iterations of
   pairwise repulsion (k^2/d), edge attraction (d^2/k) and a centroid gravity pull,
   displacement capped by a linearly cooling temperature. All float; rounds the node
   BOX CENTER to cells only at commit. UBSan guard: pairwise distance is clamped to
   an epsilon minimum BEFORE any divide. Writes local top-left positions into out[]. */
static void flow__layout_force(flow_t *f, const int *mem, int m,
                               const flow__lay_edge *ed, int ne,
                               const flow_layout_opts *o, flow_pt *out) {
  (void)o->seed;                                        /* reserved: default path is RNG-free */
  int iters = o->iterations > 0 ? o->iterations : 200;
  float k = o->k;
  if (k <= 0.0f) {                                      /* derive from node sizes: boxes must clear each other */
    int maxdim = 1;
    for (int i = 0; i < m; i++) {
      flow_node *n = &f->nodes[mem[i]];
      if (n->w > maxdim) maxdim = n->w;
      if (n->h > maxdim) maxdim = n->h;
    }
    k = 1.5f * (float)maxdim;
    if (k < 8.0f) k = 8.0f;
  }
  float grav = o->gravity > 0.0f ? o->gravity : 0.05f;
  float *px = (float*)malloc((size_t)m * sizeof(float));
  float *py = (float*)malloc((size_t)m * sizeof(float));
  float *dx = (float*)malloc((size_t)m * sizeof(float));
  float *dy = (float*)malloc((size_t)m * sizeof(float));
  float R = k * (float)m / 6.2831853f;                  /* ~k of arc between neighbors */
  if (R < k) R = k;
  for (int i = 0; i < m; i++) {
    float ang = 6.2831853f * (float)i / (float)m;
    px[i] = R * cosf(ang); py[i] = R * sinf(ang);
  }
  float t0 = 2.0f * k;
  for (int it = 0; it < iters; it++) {
    float t = t0 * (1.0f - (float)it / (float)iters);   /* linear cooling */
    if (t < 0.05f) t = 0.05f;
    for (int i = 0; i < m; i++) { dx[i] = 0; dy[i] = 0; }
    for (int i = 0; i < m; i++)                         /* repulsion: every pair */
      for (int j = i + 1; j < m; j++) {
        float ddx = px[i] - px[j], ddy = py[i] - py[j];
        if (ddx == 0.0f && ddy == 0.0f) {               /* coincident: deterministic index nudge */
          ddx = 0.05f * (float)(j - i); ddy = 0.03f * (float)(i + j + 1);
        }
        float d = sqrtf(ddx * ddx + ddy * ddy);
        if (d < 0.5f) d = 0.5f;                         /* epsilon clamp BEFORE dividing (UBSan) */
        float fr = k * k / d, ux = ddx / d, uy = ddy / d;
        dx[i] += ux * fr; dy[i] += uy * fr;
        dx[j] -= ux * fr; dy[j] -= uy * fr;
      }
    for (int e = 0; e < ne; e++) {                      /* attraction along edges */
      int i = ed[e].a, j = ed[e].b;
      float ddx = px[i] - px[j], ddy = py[i] - py[j];
      float d = sqrtf(ddx * ddx + ddy * ddy);
      if (d < 0.5f) d = 0.5f;
      float fa = d * d / k, ux = ddx / d, uy = ddy / d;
      dx[i] -= ux * fa; dy[i] -= uy * fa;
      dx[j] += ux * fa; dy[j] += uy * fa;
    }
    float cx = 0, cy = 0;                               /* gravity toward the partition centroid */
    for (int i = 0; i < m; i++) { cx += px[i]; cy += py[i]; }
    cx /= (float)m; cy /= (float)m;
    for (int i = 0; i < m; i++) { dx[i] += (cx - px[i]) * grav; dy[i] += (cy - py[i]) * grav; }
    for (int i = 0; i < m; i++) {                       /* apply, capped at temperature */
      float len = sqrtf(dx[i] * dx[i] + dy[i] * dy[i]);
      if (len <= 0.0f) continue;
      float step = len < t ? len : t;
      px[i] += dx[i] / len * step; py[i] += dy[i] / len * step;
    }
  }
  for (int i = 0; i < m; i++) {                         /* commit: box CENTER at the computed point */
    flow_node *n = &f->nodes[mem[i]];
    out[i].x = (int)lroundf(px[i] - (float)n->w / 2.0f);
    out[i].y = (int)lroundf(py[i] - (float)n->h / 2.0f);
  }
  free(px); free(py); free(dx); free(dy);
}

void flow_layout(flow_t *f, flow_layout_opts opts) {
  if (f->nnodes <= 1) return;                           /* empty/single-node graph: full no-op */
  for (int i = 0; i < f->nnodes; i++) flow_measure_node(f, &f->nodes[i]); /* sizes reflect current data */
  int gx = opts.gap_x > 0 ? opts.gap_x : 4;
  int gy = opts.gap_y > 0 ? opts.gap_y : 4;
  /* distinct parents ordered by (depth asc, first appearance): a partition's parent
     is committed before the partition itself, so child commits read settled
     parent_abs. (flow_group appends containers AFTER their children, so first
     appearance alone would visit the children's partition too early.) */
  int np = 0;
  int *plist = (int*)malloc((size_t)f->nnodes * sizeof(int));
  int *pdepth = (int*)malloc((size_t)f->nnodes * sizeof(int));
  for (int i = 0; i < f->nnodes; i++) {
    int p = f->nodes[i].parent, seen = 0;
    for (int j = 0; j < np; j++) if (plist[j] == p) { seen = 1; break; }
    if (seen) continue;
    flow_node *pn = p == -1 ? NULL : flow_get_node(f, p);
    plist[np] = p; pdepth[np] = pn ? flow__node_depth(f, pn) + 1 : 0; np++;
  }
  for (int i = 1; i < np; i++)                          /* stable insertion sort by depth */
    for (int j = i; j > 0 && pdepth[j - 1] > pdepth[j]; j--) {
      int tp = plist[j]; plist[j] = plist[j - 1]; plist[j - 1] = tp;
      int td = pdepth[j]; pdepth[j] = pdepth[j - 1]; pdepth[j - 1] = td;
    }
  int *mem = (int*)malloc((size_t)f->nnodes * sizeof(int));        /* member node indices */
  flow_pt *out = (flow_pt*)malloc((size_t)f->nnodes * sizeof(flow_pt));
  flow__lay_edge *ed = f->nedges ? (flow__lay_edge*)malloc((size_t)f->nedges * sizeof *ed) : NULL;
  flow__undo_begin(f);                                  /* the WHOLE layout = ONE undo step */
  for (int pi = 0; pi < np; pi++) {
    int p = plist[pi], m = 0;
    for (int i = 0; i < f->nnodes; i++) if (f->nodes[i].parent == p) mem[m++] = i;
    if (m <= 1) continue;                               /* single-node partition: leave it be */
    int ne = 0;                                         /* intra-partition edges, edge-array order */
    for (int i = 0; i < f->nedges; i++) {
      int a = -1, b = -1;
      for (int j = 0; j < m; j++) {
        if (f->nodes[mem[j]].id == f->edges[i].source) a = j;
        if (f->nodes[mem[j]].id == f->edges[i].target) b = j;
      }
      if (a >= 0 && b >= 0 && a != b) { ed[ne].a = a; ed[ne].b = b; ne++; }
    }
    if (opts.mode == FLOW_LAYOUT_LAYERED) flow__layout_layered(f, mem, m, ed, ne, opts.dir, gx, gy, out);
    else                                  flow__layout_force(f, mem, m, ed, ne, &opts, out);
    int mnx = out[0].x, mny = out[0].y;                 /* normalize: bbox min -> (0,0) top-level, */
    for (int i = 1; i < m; i++) {                       /* (1,1) inside a container (border pad)   */
      if (out[i].x < mnx) mnx = out[i].x;
      if (out[i].y < mny) mny = out[i].y;
    }
    int bx = (p == -1 ? 0 : 1) - mnx, by = (p == -1 ? 0 : 1) - mny;
    flow_pt pa = { 0, 0 };
    if (p != -1) { flow_node *pn = flow_get_node(f, p); if (pn) pa = flow_node_abs(f, pn); }
    for (int i = 0; i < m; i++)                         /* absolute-in commit: parent_abs + local */
      flow_move_node(f, f->nodes[mem[i]].id,
                     (flow_pt){ pa.x + out[i].x + bx, pa.y + out[i].y + by });
  }
  flow__undo_end(f);
  free(plist); free(pdepth); free(mem); free(out); free(ed);
  if (opts.fit_after) flow_fit_view(f, opts.margin);    /* viewport untouched without it */
}
void flow_layout_force(flow_t *f, flow_force_opts opts) {
  flow_layout_opts o = {0};
  o.mode = FLOW_LAYOUT_FORCE;
  o.iterations = opts.iterations; o.k = opts.k; o.gravity = opts.gravity;
  flow_layout(f, o);
}
void flow_layout_layered(flow_t *f, flow_layered_dir dir, int gap_x, int gap_y) {
  flow_layout_opts o = {0};
  o.mode = FLOW_LAYOUT_LAYERED;
  o.dir = dir; o.gap_x = gap_x; o.gap_y = gap_y;
  flow_layout(f, o);
}
#endif
