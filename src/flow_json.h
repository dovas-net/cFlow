/* ===== JSON persistence: flow_save / flow_load (dependency-free, hand-rolled) =====
 *
 * Durable structure only — see the serialize package spec for the boundary:
 *   viewport ox/oy/zoom (FLOATS, %.9g for lossless save->load->save),
 *   nodes id/type/x/y/parent + optional opaque data (via flow_node_type.save/load),
 *   edges id/source/target/sourceHandle/targetHandle/type/label.
 * Ephemeral state (flags, selection, w/h, drag/marquee/conn, zoom limits, callbacks,
 * widgets) is NEVER emitted; w/h are recomputed by flow_measure_node on load.
 * node->data and edge->data are app-owned: the library never frees arbitrary void*.
 * A node's "data" is produced verbatim by its type's save() hook (one JSON value);
 * on load, the captured "data" span is NUL-terminated and handed to load(), which
 * mallocs+assigns node->data (app-owned). Edge data has NO hook — not persisted. */

#ifdef FLOW_IMPLEMENTATION

/* ---- writer ---- */

/* Emit a quoted, escaped JSON string: the 7 mandatory escapes plus other control
   chars (<0x20) as \uXXXX. Cast to unsigned char so high-bit bytes pass through. */
static void flow__json_str(FILE *out, const char *s) {
  fputc('"', out);
  for (const unsigned char *p = (const unsigned char*)s; *p; p++) {
    unsigned char c = *p;
    switch (c) {
      case '"':  fputs("\\\"", out); break;
      case '\\': fputs("\\\\", out); break;
      case '\b': fputs("\\b", out);  break;
      case '\f': fputs("\\f", out);  break;
      case '\n': fputs("\\n", out);  break;
      case '\r': fputs("\\r", out);  break;
      case '\t': fputs("\\t", out);  break;
      default:
        if (c < 0x20) fprintf(out, "\\u%04x", (unsigned)c);
        else          fputc((int)c, out);
    }
  }
  fputc('"', out);
}

int flow_save(flow_t *f, const char *path) {
  if (!f || !path) return -1;
  FILE *out = fopen(path, "wb");
  if (!out) return -1;
  flow_viewport v = flow_view_get(f);
  fputs("{\"version\":1,\n", out);
  fprintf(out, " \"viewport\":{\"ox\":%.9g,\"oy\":%.9g,\"zoom\":%.9g},\n",
          (double)v.ox, (double)v.oy, (double)v.zoom);
  /* nodes */
  fputs(" \"nodes\":[", out);
  for (int i = 0; i < f->nnodes; i++) {
    flow_node *n = &f->nodes[i];
    if (i) fputs(",\n          ", out);
    fprintf(out, "{\"id\":%d,\"type\":", n->id);
    flow__json_str(out, n->type);
    fprintf(out, ",\"x\":%d,\"y\":%d,\"parent\":%d", n->pos.x, n->pos.y, n->parent);
    const flow_node_type *t = flow_node_type_for(f, n->type);
    if (t && t->save) { fputs(",\"data\":", out); t->save(n, out); }
    fputc('}', out);
  }
  fputs("],\n", out);
  /* edges */
  fputs(" \"edges\":[", out);
  for (int i = 0; i < f->nedges; i++) {
    flow_edge *e = &f->edges[i];
    if (i) fputs(",\n          ", out);
    fprintf(out, "{\"id\":%d,\"source\":%d,\"target\":%d,\"sourceHandle\":",
            e->id, e->source, e->target);
    flow__json_str(out, e->source_handle);
    fputs(",\"targetHandle\":", out);
    flow__json_str(out, e->target_handle);
    fputs(",\"type\":", out);
    flow__json_str(out, e->type);
    if (e->label) { fputs(",\"label\":", out); flow__json_str(out, e->label); } /* omit key when NULL */
    fputc('}', out);
  }
  fputs("]}\n", out);
  int err = ferror(out);
  if (fclose(out) != 0) err = 1;
  return err ? -1 : 0;
}

/* ---- reader ---- (no DOM; targeted key lookup within object brace-spans) */

typedef struct { const char *p; const char *end; } flow_json_rd;

/* Skip one JSON value starting at *pp (which must point at its first byte, with
   leading whitespace already consumed). String-aware (skips \" escapes) and
   nest-aware (balances {} / []). Bounds every step by `end`. Advances *pp past
   the value. Safe on truncated input: stops at end without over-reading. */
static void flow__json_skip(const char **pp, const char *end) {
  const char *p = *pp;
  if (p >= end) { *pp = p; return; }
  if (*p == '"') {                                  /* string */
    p++;
    while (p < end && *p != '"') { if (*p == '\\' && p + 1 < end) p++; p++; }
    if (p < end) p++;                               /* consume closing quote */
  } else if (*p == '{' || *p == '[') {              /* nested object/array */
    int depth = 0;
    while (p < end) {
      char c = *p;
      if (c == '"') {                               /* skip strings inside */
        p++;
        while (p < end && *p != '"') { if (*p == '\\' && p + 1 < end) p++; p++; }
        if (p < end) p++;
        continue;
      }
      if (c == '{' || c == '[') depth++;
      else if (c == '}' || c == ']') { depth--; p++; if (depth == 0) break; continue; }
      p++;
    }
  } else {                                          /* scalar: number / true / false / null */
    while (p < end && *p != ',' && *p != '}' && *p != ']') p++;
  }
  *pp = p;
}

/* skip whitespace, bounded by end */
static void flow__json_ws(const char **pp, const char *end) {
  const char *p = *pp;
  while (p < end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) p++;
  *pp = p;
}

/* Find the value of top-level `key` inside the object spanning [obj,end).
   `obj` points at (or before) the opening '{'. Scans top-level keys only, skipping
   nested {}/[] and strings, so it never matches a key inside a nested data object.
   On success fills *out with the value span [p,end) (p at the value's first byte,
   value-end is the enclosing object's end) and returns 1; else 0. */
static int flow__json_find(const char *obj, const char *end, const char *key, flow_json_rd *out) {
  const char *p = obj;
  flow__json_ws(&p, end);
  if (p < end && *p == '{') p++;                    /* enter the object */
  size_t klen = strlen(key);
  for (;;) {
    flow__json_ws(&p, end);
    if (p >= end || *p == '}') return 0;
    if (*p == ',') { p++; continue; }
    if (*p != '"') return 0;                        /* malformed: not a key */
    const char *kstart = p + 1;
    const char *kp = kstart;
    while (kp < end && *kp != '"') { if (*kp == '\\' && kp + 1 < end) kp++; kp++; }
    size_t kactual = (size_t)(kp - kstart);
    const char *afterkey = (kp < end) ? kp + 1 : kp;
    flow__json_ws(&afterkey, end);
    if (afterkey < end && *afterkey == ':') afterkey++;
    flow__json_ws(&afterkey, end);
    if (kactual == klen && strncmp(kstart, key, klen) == 0) {
      out->p = afterkey; out->end = end; return 1;
    }
    p = afterkey;
    flow__json_skip(&p, end);                       /* skip this value, continue */
  }
}

/* parse an int value (strtol); 1 on success */
static int flow__json_int(flow_json_rd r, int *v) {
  const char *p = r.p; flow__json_ws(&p, r.end);
  if (p >= r.end) return 0;
  char *e = NULL; long l = strtol(p, &e, 10);
  if (e == p) return 0;
  *v = (int)l; return 1;
}

/* parse a float value (strtof — lossless w/ %.9g writer); 1 on success */
static int flow__json_float(flow_json_rd r, float *v) {
  const char *p = r.p; flow__json_ws(&p, r.end);
  if (p >= r.end) return 0;
  char *e = NULL; float fv = strtof(p, &e);
  if (e == p) return 0;
  *v = fv; return 1;
}

/* parse + unescape a JSON string value into buf (clamped to cap, always NUL-term);
   1 on success, 0 if the value is not a string. */
static int flow__json_strv(flow_json_rd r, char *buf, int cap) {
  if (cap <= 0) return 0;
  const char *p = r.p; flow__json_ws(&p, r.end);
  if (p >= r.end || *p != '"') { buf[0] = 0; return 0; }
  p++;
  int n = 0;
  while (p < r.end && *p != '"') {
    char c = *p++;
    if (c == '\\' && p < r.end) {
      char esc = *p++;
      switch (esc) {
        case '"':  c = '"';  break;
        case '\\': c = '\\'; break;
        case '/':  c = '/';  break;
        case 'b':  c = '\b'; break;
        case 'f':  c = '\f'; break;
        case 'n':  c = '\n'; break;
        case 'r':  c = '\r'; break;
        case 't':  c = '\t'; break;
        case 'u': {                                  /* \uXXXX: only emit the low byte (ASCII control range) */
          unsigned val = 0; int got = 0;
          for (; got < 4 && p < r.end; got++) {
            char h = *p; int d;
            if (h >= '0' && h <= '9') d = h - '0';
            else if (h >= 'a' && h <= 'f') d = h - 'a' + 10;
            else if (h >= 'A' && h <= 'F') d = h - 'A' + 10;
            else break;
            val = val * 16 + (unsigned)d; p++;
          }
          c = (char)(val & 0xFF);
          break;
        }
        default: c = esc; break;
      }
    }
    if (n < cap - 1) buf[n++] = c;
  }
  buf[n] = 0;
  return 1;
}

/* capture the raw span of a value (the node "data" sub-object/array/scalar);
   start/len delimit exactly the value bytes. 1 on success. */
static int flow__json_raw(flow_json_rd r, const char **start, int *len) {
  const char *p = r.p; flow__json_ws(&p, r.end);
  if (p >= r.end) return 0;
  const char *s = p;
  flow__json_skip(&p, r.end);
  *start = s; *len = (int)(p - s);
  return 1;
}

/* locate an array value span by key: out->p points at the value (the '['),
   out->end is the object end. 1 if found and the value is an array. */
static int flow__json_array(const char *obj, const char *end, const char *key, flow_json_rd *out) {
  flow_json_rd v;
  if (!flow__json_find(obj, end, key, &v)) return 0;
  const char *p = v.p; flow__json_ws(&p, v.end);
  if (p >= v.end || *p != '[') return 0;
  out->p = p; out->end = v.end;
  return 1;
}

/* iterate successive top-level element spans of an array. On first call arr->p must
   point at the '['; this function advances arr->p past each yielded element. Yields
   elem/len for the next element and returns 1; returns 0 at end of array. */
static int flow__json_iter(flow_json_rd *arr, const char **elem, int *len) {
  const char *p = arr->p;
  if (p < arr->end && *p == '[') p++;               /* enter array on first call */
  flow__json_ws(&p, arr->end);
  if (p >= arr->end || *p == ']') { arr->p = p; return 0; }
  if (*p == ',') { p++; flow__json_ws(&p, arr->end); }
  if (p >= arr->end || *p == ']') { arr->p = p; return 0; }
  const char *s = p;
  flow__json_skip(&p, arr->end);
  *elem = s; *len = (int)(p - s);
  arr->p = p;                                       /* leave cursor after this element */
  return 1;
}

/* Structural validity check: a single top-level object with balanced, string-aware
   {}/[] nesting and no unterminated strings. Run BEFORE flow__graph_reset so a
   malformed/truncated file returns -1 with the live graph untouched (no partial
   corruption). flow__json_skip stops silently at `end` on truncation and can't
   signal imbalance, so this dedicated pass tracks depth explicitly. */
static int flow__json_valid(const char *p, const char *end) {
  flow__json_ws(&p, end);
  if (p >= end || *p != '{') return 0;
  int depth = 0;
  while (p < end) {
    char c = *p;
    if (c == '"') {
      p++;
      while (p < end && *p != '"') { if (*p == '\\' && p + 1 < end) p++; p++; }
      if (p >= end) return 0;                       /* unterminated string */
      p++; continue;
    }
    if (c == '{' || c == '[') depth++;
    else if (c == '}' || c == ']') { if (--depth < 0) return 0; }
    p++;
  }
  return depth == 0;
}

int flow_load(flow_t *f, const char *path) {
  if (!f || !path) return -1;
  /* (1) read the whole file FIRST — a failed open must leave f untouched. */
  FILE *in = fopen(path, "rb");
  if (!in) return -1;
  if (fseek(in, 0, SEEK_END) != 0) { fclose(in); return -1; }
  long sz = ftell(in);
  if (sz < 0) { fclose(in); return -1; }
  if (fseek(in, 0, SEEK_SET) != 0) { fclose(in); return -1; }
  char *buf = (char*)malloc((size_t)sz + 1);
  if (!buf) { fclose(in); return -1; }
  size_t got = fread(buf, 1, (size_t)sz, in);
  fclose(in);
  buf[got] = 0;                                     /* NUL sentinel for strtol/strtof */
  const char *root = buf, *end = buf + got;

  /* (2) validate structure BEFORE touching f — malformed => -1, graph untouched. */
  if (!flow__json_valid(root, end)) { free(buf); return -1; }

  /* (3) only now wipe the graph and rebuild. flow__graph_reset clears the undo journal
     (undo must not invert against a replaced graph) and the rebuild below runs with
     recording suppressed — a load is not an edit, so it journals NOTHING. */
  flow__graph_reset(f);
  f->journal.suppress++;

  /* viewport (floats; keep zmin/zmax limits from the live engine) */
  flow_json_rd vp;
  if (flow__json_find(root, end, "viewport", &vp)) {
    float ox = f->view.ox, oy = f->view.oy, zoom = f->view.zoom;
    flow_json_rd field;
    if (flow__json_find(vp.p, vp.end, "ox", &field))   flow__json_float(field, &ox);
    if (flow__json_find(vp.p, vp.end, "oy", &field))   flow__json_float(field, &oy);
    if (flow__json_find(vp.p, vp.end, "zoom", &field)) flow__json_float(field, &zoom);
    flow__view_set(f, ox, oy, zoom);   /* restore-on-load fires on_viewport_change (graph is mid-rebuild: callback must not query nodes) */
  }

  int maxnid = 0, maxeid = 0;

  /* nodes */
  flow_json_rd narr;
  if (flow__json_array(root, end, "nodes", &narr)) {
    const char *elem; int elen;
    while (flow__json_iter(&narr, &elem, &elen)) {
      const char *eend = elem + elen;
      char type[32] = "default";
      flow_json_rd fld;
      if (flow__json_find(elem, eend, "type", &fld)) flow__json_strv(fld, type, (int)sizeof type);
      int id = 0, x = 0, y = 0, parent = -1;
      if (flow__json_find(elem, eend, "id", &fld))     flow__json_int(fld, &id);
      if (flow__json_find(elem, eend, "x", &fld))      flow__json_int(fld, &x);
      if (flow__json_find(elem, eend, "y", &fld))      flow__json_int(fld, &y);
      if (flow__json_find(elem, eend, "parent", &fld)) flow__json_int(fld, &parent);
      flow_add_node(f, type, (flow_pt){ x, y }, NULL);
      flow_node *n = &f->nodes[f->nnodes - 1];       /* just-appended (index, not id-search:
        we overwrite n->id below, so flow_get_node on the transient id could alias an
        already-overwritten earlier node and mutate the wrong one) */
      if (id > 0) n->id = id;
      n->parent = parent;
      /* data hook BEFORE measure (device measure reads n->data) */
      const flow_node_type *t = flow_node_type_for(f, n->type);
      if (t && t->load) {
        flow_json_rd dv;
        if (flow__json_find(elem, eend, "data", &dv)) {
          const char *ds; int dl;
          if (flow__json_raw(dv, &ds, &dl)) {
            char *copy = (char*)malloc((size_t)dl + 1);
            if (copy) { memcpy(copy, ds, (size_t)dl); copy[dl] = 0; t->load(n, copy); free(copy); }
          }
        }
      }
      flow_measure_node(f, n);                       /* recompute w/h from restored data */
      if (n->id > maxnid) maxnid = n->id;
    }
  }

  /* edges */
  flow_json_rd earr;
  if (flow__json_array(root, end, "edges", &earr)) {
    const char *elem; int elen;
    while (flow__json_iter(&earr, &elem, &elen)) {
      const char *eend = elem + elen;
      char sh[16] = "", th[16] = "", etype[16] = "";
      int id = 0, src = 0, tgt = 0;
      flow_json_rd fld;
      if (flow__json_find(elem, eend, "id", &fld))           flow__json_int(fld, &id);
      if (flow__json_find(elem, eend, "source", &fld))       flow__json_int(fld, &src);
      if (flow__json_find(elem, eend, "target", &fld))       flow__json_int(fld, &tgt);
      if (flow__json_find(elem, eend, "sourceHandle", &fld)) flow__json_strv(fld, sh, (int)sizeof sh);
      if (flow__json_find(elem, eend, "targetHandle", &fld)) flow__json_strv(fld, th, (int)sizeof th);
      if (flow__json_find(elem, eend, "type", &fld))         flow__json_strv(fld, etype, (int)sizeof etype);
      int neid = flow_add_edge(f, src, tgt, sh, th);
      if (neid == -1) continue;                      /* rejected (missing endpoint / dup / self): skip */
      flow_edge *e = &f->edges[f->nedges - 1];       /* just-appended (index, not id-search; see node loop) */
      if (id > 0) e->id = id;
      snprintf(e->type, sizeof e->type, "%s", etype);
      flow_json_rd lf;
      if (flow__json_find(elem, eend, "label", &lf)) {  /* present => set; absent => leave NULL */
        char lbl[1024];
        if (flow__json_strv(lf, lbl, (int)sizeof lbl)) {
          size_t ln = strlen(lbl) + 1;
          e->label = (char*)malloc(ln);
          if (e->label) memcpy(e->label, lbl, ln);
        }
      }
      if (e->id > maxeid) maxeid = e->id;
    }
  }

  f->nextid  = maxnid + 1;                           /* post-load adds don't collide */
  f->nexteid = maxeid + 1;

  f->journal.suppress--;
  free(buf);
  return 0;
}

#endif /* FLOW_IMPLEMENTATION */
