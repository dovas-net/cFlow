#define FLOW_IMPLEMENTATION
#include "../flow.h"
#include "flowtest.h"

/* ---- a 'device' node type mirroring topo's 5-field struct, with save/load hooks ---- */
typedef struct {
  char label[24];
  char kind[12];
  char ip[20];
  char status[12];
  char os[24];
} device;

static void dev_measure(const flow_node *n, int *w, int *h) {
  const device *d = (const device*)n->data;
  int len = d ? (int)strlen(d->label) : 0;
  if (d && (int)strlen(d->kind) > len) len = (int)strlen(d->kind);
  *w = len + 4; *h = 4;
}
static void dev_render(const flow_node *n, flow_surface *s, flow_render_ctx c) { (void)n;(void)s;(void)c; }
static void dev_save(const flow_node *n, FILE *out) {
  const device *d = (const device*)n->data;
  fputs("{\"label\":", out);  flow__json_str(out, d->label);
  fputs(",\"kind\":", out);   flow__json_str(out, d->kind);
  fputs(",\"ip\":", out);     flow__json_str(out, d->ip);
  fputs(",\"status\":", out); flow__json_str(out, d->status);
  fputs(",\"os\":", out);     flow__json_str(out, d->os);
  fputc('}', out);
}
static void dev_load(flow_node *n, const char *data_json) {
  device *d = (device*)calloc(1, sizeof *d);
  const char *end = data_json + strlen(data_json);
  flow_json_rd fld;
  if (flow__json_find(data_json, end, "label", &fld))  flow__json_strv(fld, d->label,  (int)sizeof d->label);
  if (flow__json_find(data_json, end, "kind", &fld))   flow__json_strv(fld, d->kind,   (int)sizeof d->kind);
  if (flow__json_find(data_json, end, "ip", &fld))     flow__json_strv(fld, d->ip,     (int)sizeof d->ip);
  if (flow__json_find(data_json, end, "status", &fld)) flow__json_strv(fld, d->status, (int)sizeof d->status);
  if (flow__json_find(data_json, end, "os", &fld))     flow__json_strv(fld, d->os,     (int)sizeof d->os);
  n->data = d;   /* app-owned; library never frees it */
}
static const flow_node_type DEVICE = {
  "device", dev_measure, dev_render, flow_default_handles, 2, dev_save, dev_load
};

/* free every device the load hook mallocked, then free the flow */
static void free_devices_and_flow(flow_t *f) {
  int nc = flow_node_count(f);
  flow_node *ns = flow_nodes(f);
  for (int i = 0; i < nc; i++)
    if (strcmp(ns[i].type, "device") == 0) free(ns[i].data);
  flow_free(f);
}

/* slurp a file into a fresh malloc'd NUL-terminated buffer (caller frees) */
static char *slurp(const char *path) {
  FILE *fp = fopen(path, "rb");
  if (!fp) return NULL;
  fseek(fp, 0, SEEK_END); long n = ftell(fp); fseek(fp, 0, SEEK_SET);
  char *b = (char*)malloc((size_t)n + 1);
  size_t g = fread(b, 1, (size_t)n, fp); fclose(fp);
  b[g] = 0; return b;
}

int main(void) {
  const char *P_BASIC = "/tmp/flow_json_basic.json";
  const char *P_A     = "/tmp/flow_json_a.json";
  const char *P_B     = "/tmp/flow_json_b.json";
  const char *P_DEV   = "/tmp/flow_json_dev.json";
  const char *P_GARB  = "/tmp/flow_json_garbage.json";

  /* ---- Writer golden: 2 default nodes + 1 edge, non-1 float zoom + pan ---- */
  {
    flow_t *f = flow_new(80, 24);
    flow_register_defaults(f);
    flow_add_node(f, "default", (flow_pt){6, 3}, (void*)"a");
    flow_add_node(f, "default", (flow_pt){30, 10}, (void*)"b");
    flow_add_edge(f, 1, 2, "out", "in");
    flow_pan(f, 5, -3);
    flow_set_zoom(f, 1.5f, (flow_pt){0, 0});
    ASSERT_INT(flow_save(f, P_BASIC), 0, "flow_save basic ok");
    char *buf = slurp(P_BASIC);
    ASSERT(buf != NULL, "basic file readable");
    if (buf) { SNAPSHOT("json_basic", buf); free(buf); }
    flow_free(f);
  }

  /* ---- JSON string escaping unit ---- */
  {
    FILE *tf = tmpfile();
    flow__json_str(tf, "a\"b\\c\nd\te\x01");
    rewind(tf);
    char got[64] = {0}; size_t g = fread(got, 1, sizeof got - 1, tf); got[g] = 0;
    fclose(tf);
    ASSERT_STR(got, "\"a\\\"b\\\\c\\nd\\te\\u0001\"", "escaping: quote/backslash/nl/tab/ctrl");
  }

  /* ---- Structure round-trip + viewport float round-trip ---- */
  {
    flow_t *a = flow_new(80, 24);
    flow_register_defaults(a);
    int n1 = flow_add_node(a, "default", (flow_pt){6, 3}, (void*)"a");
    int n2 = flow_add_node(a, "default", (flow_pt){30, 10}, (void*)"b");
    flow_get_node(a, n2)->parent = n1;   /* child: relative pos preserved */
    flow_add_edge(a, n1, n2, "out", "in");
    /* non-exact floats so %.9g / strtof is actually exercised by byte-identical */
    a->view.ox = 12.5f; a->view.oy = -7.25f;
    a->view.zoom = 1.2f * 1.2f * 1.2f;   /* 1.728, not exactly float-representable */
    ASSERT_INT(flow_save(a, P_A), 0, "save A");

    flow_t *b = flow_new(80, 24);
    flow_register_defaults(b);
    ASSERT_INT(flow_load(b, P_A), 0, "load A into B");
    ASSERT_INT(flow_node_count(b), 2, "node count preserved");
    ASSERT_INT(flow_edge_count(b), 1, "edge count preserved");
    flow_node *b1 = flow_get_node(b, n1), *b2 = flow_get_node(b, n2);
    ASSERT(b1 && b2, "both node ids preserved");
    if (b1 && b2) {
      ASSERT_INT(b1->pos.x, 6, "node1 x"); ASSERT_INT(b1->pos.y, 3, "node1 y");
      ASSERT_INT(b1->parent, -1, "node1 parent -1");
      ASSERT_INT(b2->pos.x, 30, "node2 x"); ASSERT_INT(b2->pos.y, 10, "node2 y");
      ASSERT_INT(b2->parent, n1, "node2 parent preserved");
      ASSERT_STR(b1->type, "default", "node1 type");
    }
    flow_edge *be = flow_get_edge(b, 1);
    ASSERT(be != NULL, "edge id preserved");
    if (be) {
      ASSERT_INT(be->source, n1, "edge source"); ASSERT_INT(be->target, n2, "edge target");
      ASSERT_STR(be->source_handle, "out", "edge sourceHandle");
      ASSERT_STR(be->target_handle, "in", "edge targetHandle");
    }
    flow_viewport v = flow_view_get(b);
    ASSERT(v.ox > 12.4999f && v.ox < 12.5001f, "viewport ox float round-trip");
    ASSERT(v.oy > -7.2501f && v.oy < -7.2499f, "viewport oy float round-trip");
    ASSERT(v.zoom > 1.7279f && v.zoom < 1.7281f, "viewport zoom float round-trip");

    /* save->load->save byte-identical (float-precision guard for %.9g) */
    ASSERT_INT(flow_save(b, P_B), 0, "save B");
    char *sa = slurp(P_A), *sb = slurp(P_B);
    ASSERT(sa && sb, "both saved files readable");
    if (sa && sb) ASSERT_STR(sa, sb, "save->load->save byte-identical");
    free(sa); free(sb);
    flow_free(a); flow_free(b);
  }

  /* ---- Custom device-data round-trip (headline): quote + space inside a value ---- */
  {
    static device da = { "edge \"sw\"", "switch", "10.0.0.1", "up", "IOS 15" };
    flow_t *a = flow_new(80, 24);
    flow_register_defaults(a);
    flow_register_node_type(a, &DEVICE);
    int id = flow_add_node(a, "device", (flow_pt){4, 2}, &da);
    int aw = flow_get_node(a, id)->w, ah = flow_get_node(a, id)->h;
    ASSERT_INT(flow_save(a, P_DEV), 0, "save device graph");

    flow_t *b = flow_new(80, 24);
    flow_register_defaults(b);
    flow_register_node_type(b, &DEVICE);
    ASSERT_INT(flow_load(b, P_DEV), 0, "load device graph");
    flow_node *bn = flow_get_node(b, id);
    ASSERT(bn != NULL, "device node loaded");
    if (bn) {
      device *bd = (device*)bn->data;
      ASSERT(bd != NULL, "device data restored");
      if (bd) {
        ASSERT_STR(bd->label, "edge \"sw\"", "device label (quote+space)");
        ASSERT_STR(bd->kind, "switch", "device kind");
        ASSERT_STR(bd->ip, "10.0.0.1", "device ip");
        ASSERT_STR(bd->status, "up", "device status");
        ASSERT_STR(bd->os, "IOS 15", "device os");
      }
      ASSERT_INT(bn->w, aw, "recomputed w matches (load ran then measure ran)");
      ASSERT_INT(bn->h, ah, "recomputed h matches");
    }
    flow_free(a);
    free_devices_and_flow(b);
  }

  /* ---- Edge label round-trip + NULL-label omit ---- */
  {
    flow_t *a = flow_new(80, 24);
    flow_register_defaults(a);
    int n1 = flow_add_node(a, "default", (flow_pt){0, 0}, (void*)"a");
    int n2 = flow_add_node(a, "default", (flow_pt){20, 0}, (void*)"b");
    int n3 = flow_add_node(a, "default", (flow_pt){40, 0}, (void*)"c");
    int e1 = flow_add_edge(a, n1, n2, "", "");   /* labeled */
    flow_add_edge(a, n2, n3, "", "");            /* NULL label */
    flow_set_edge_label(a, e1, "L1");
    ASSERT_INT(flow_save(a, P_A), 0, "save label graph");
    char *js = slurp(P_A);
    ASSERT(js != NULL, "label json readable");
    /* exactly one "label" key (the non-null edge); NULL-label edge omits it */
    int labels = 0;
    if (js) { const char *p = js; while ((p = strstr(p, "\"label\"")) != NULL) { labels++; p++; } }
    ASSERT_INT(labels, 1, "only the non-null label key is written");
    free(js);

    flow_t *b = flow_new(80, 24);
    flow_register_defaults(b);
    ASSERT_INT(flow_load(b, P_A), 0, "load label graph");
    flow_edge *be1 = flow_get_edge(b, e1);
    ASSERT(be1 && be1->label, "labeled edge has label");
    if (be1 && be1->label) ASSERT_STR(be1->label, "L1", "label round-trip");
    /* the second edge (next id) must have NULL label, no "" artifact */
    flow_edge *bes = flow_edges(b);
    int nullok = 0;
    for (int i = 0; i < flow_edge_count(b); i++)
      if (bes[i].id != e1) { nullok = (bes[i].label == NULL); }
    ASSERT_INT(nullok, 1, "NULL-label edge loads back as label==NULL");
    flow_free(a); flow_free(b);
  }

  /* ---- Edge handles/type round-trip ---- */
  {
    flow_t *a = flow_new(80, 24);
    flow_register_defaults(a);
    int n1 = flow_add_node(a, "default", (flow_pt){0, 0}, (void*)"a");
    int n2 = flow_add_node(a, "default", (flow_pt){20, 0}, (void*)"b");
    int e1 = flow_add_edge(a, n1, n2, "out", "in");
    snprintf(flow_get_edge(a, e1)->type, 16, "%s", "straight");
    ASSERT_INT(flow_save(a, P_A), 0, "save handles/type graph");

    flow_t *b = flow_new(80, 24);
    flow_register_defaults(b);
    ASSERT_INT(flow_load(b, P_A), 0, "load handles/type graph");
    flow_edge *be = flow_get_edge(b, e1);
    ASSERT(be != NULL, "edge present");
    if (be) {
      ASSERT_STR(be->source_handle, "out", "sourceHandle round-trip");
      ASSERT_STR(be->target_handle, "in", "targetHandle round-trip");
      ASSERT_STR(be->type, "straight", "edge type round-trip");
    }
    flow_free(a); flow_free(b);
  }

  /* ---- Id preservation + bump: max node id 7, max edge id 4 ---- */
  {
    flow_t *a = flow_new(80, 24);
    flow_register_defaults(a);
    int n1 = flow_add_node(a, "default", (flow_pt){0, 0}, (void*)"a");
    int n2 = flow_add_node(a, "default", (flow_pt){20, 0}, (void*)"b");
    flow_get_node(a, n1)->id = 7;   /* force max ids */
    flow_get_node(a, n2)->id = 3;
    int e1 = flow_add_edge(a, 7, 3, "", "");
    int e2 = flow_add_edge(a, 3, 7, "", "");
    flow_get_edge(a, e1)->id = 4;
    flow_get_edge(a, e2)->id = 2;
    ASSERT_INT(flow_save(a, P_A), 0, "save id graph");

    flow_t *b = flow_new(80, 24);
    flow_register_defaults(b);
    ASSERT_INT(flow_load(b, P_A), 0, "load id graph");
    int newn = flow_add_node(b, "default", (flow_pt){5, 5}, (void*)"x");
    ASSERT_INT(newn, 8, "next node id is max+1 == 8");
    int newe = flow_add_edge(b, 8, 7, "", "");
    ASSERT_INT(newe, 5, "next edge id is max+1 == 5");
    flow_free(a); flow_free(b);
  }

  /* ---- Non-sequential ids: load must map fields to the RIGHT element ----
     node ids [2,3], edge ids [2,4] — exercises the transient-id aliasing hazard
     where a later load iteration's transient id collides with an already-rewritten
     earlier node/edge. Distinct positions per node prove correct mapping. */
  {
    flow_t *a = flow_new(80, 24);
    flow_register_defaults(a);
    flow_add_node(a, "default", (flow_pt){11, 1}, (void*)"X");  /* array[0] -> id 2 */
    flow_add_node(a, "default", (flow_pt){22, 2}, (void*)"Y");  /* array[1] -> id 3 */
    flow_nodes(a)[0].id = 2;   /* set by INDEX to avoid id-search aliasing in setup itself */
    flow_nodes(a)[1].id = 3;
    flow_add_edge(a, 2, 3, "out", "in");
    flow_add_edge(a, 3, 2, "", "");
    flow_edges(a)[0].id = 2;
    flow_edges(a)[1].id = 4;
    ASSERT_INT(flow_save(a, P_A), 0, "save non-seq id graph");

    flow_t *b = flow_new(80, 24);
    flow_register_defaults(b);
    ASSERT_INT(flow_load(b, P_A), 0, "load non-seq id graph");
    flow_node *bx = flow_get_node(b, 2), *by = flow_get_node(b, 3);
    ASSERT(bx && by, "both non-seq node ids present");
    if (bx && by) {
      ASSERT_INT(bx->pos.x, 11, "id2 maps to X pos.x (no aliasing)");
      ASSERT_INT(bx->pos.y, 1,  "id2 maps to X pos.y");
      ASSERT_INT(by->pos.x, 22, "id3 maps to Y pos.x (no aliasing)");
      ASSERT_INT(by->pos.y, 2,  "id3 maps to Y pos.y");
    }
    flow_edge *be1 = flow_get_edge(b, 2), *be2 = flow_get_edge(b, 4);
    ASSERT(be1 && be2, "both non-seq edge ids present");
    if (be1 && be2) {
      ASSERT_INT(be1->source, 2, "edge id2 source"); ASSERT_INT(be1->target, 3, "edge id2 target");
      ASSERT_STR(be1->source_handle, "out", "edge id2 sourceHandle (no aliasing)");
      ASSERT_INT(be2->source, 3, "edge id4 source"); ASSERT_INT(be2->target, 2, "edge id4 target");
    }
    /* byte-identical round-trip on non-sequential ids */
    ASSERT_INT(flow_save(b, P_B), 0, "save non-seq B");
    char *sa = slurp(P_A), *sb = slurp(P_B);
    ASSERT(sa && sb, "non-seq files readable");
    if (sa && sb) ASSERT_STR(sa, sb, "non-seq save->load->save byte-identical");
    free(sa); free(sb);
    flow_free(a); flow_free(b);
  }

  /* ---- Reset-on-load: pre-populated B with a heap edge label gets freed (ASan) ---- */
  {
    flow_t *a = flow_new(80, 24);
    flow_register_defaults(a);
    int an1 = flow_add_node(a, "default", (flow_pt){1, 1}, (void*)"a");
    int an2 = flow_add_node(a, "default", (flow_pt){9, 1}, (void*)"b");
    flow_add_edge(a, an1, an2, "", "");
    ASSERT_INT(flow_save(a, P_A), 0, "save reset-src");

    flow_t *b = flow_new(80, 24);
    flow_register_defaults(b);
    int bn1 = flow_add_node(b, "default", (flow_pt){0, 0}, (void*)"junk1");
    int bn2 = flow_add_node(b, "default", (flow_pt){5, 5}, (void*)"junk2");
    int be = flow_add_edge(b, bn1, bn2, "", "");
    flow_set_edge_label(b, be, "stale-heap-label");   /* must be freed by reset on load */
    ASSERT_INT(flow_load(b, P_A), 0, "load over junk");
    ASSERT_INT(flow_node_count(b), 2, "B contents replaced: 2 nodes");
    ASSERT_INT(flow_edge_count(b), 1, "B contents replaced: 1 edge");
    ASSERT(flow_get_node(b, an1) != NULL, "B has A's node ids after reset");
    flow_free(a); flow_free(b);
  }

  /* ---- Failure paths ---- */
  {
    flow_t *b = flow_new(80, 24);
    flow_register_defaults(b);
    flow_add_node(b, "default", (flow_pt){0, 0}, (void*)"keep");
    ASSERT_INT(flow_load(b, "/tmp/does_not_exist_flow_xyz.json"), -1, "load nonexistent -> -1");
    ASSERT_INT(flow_node_count(b), 1, "nonexistent load leaves f unchanged");
    flow_free(b);

    /* truncated/garbage: buffer ending mid-number with unbalanced braces */
    FILE *gf = fopen(P_GARB, "wb");
    fputs("{\"version\":1,\"viewport\":{\"ox\":12.5,\"oy\":-3", gf);   /* mid-number, no close */
    fclose(gf);
    flow_t *c = flow_new(80, 24);
    flow_register_defaults(c);
    flow_add_node(c, "default", (flow_pt){1, 1}, (void*)"survivor");
    ASSERT_INT(flow_load(c, P_GARB), -1, "truncated garbage -> -1");
    ASSERT_INT(flow_node_count(c), 1, "malformed load leaves graph untouched (no partial corruption)");
    flow_free(c);

    /* second garbage: ends mid-string inside a node (unterminated string, no over-read) */
    gf = fopen(P_GARB, "wb");
    fputs("{\"nodes\":[{\"id\":1,\"type\":\"defa", gf);
    fclose(gf);
    flow_t *d = flow_new(80, 24);
    flow_register_defaults(d);
    ASSERT_INT(flow_load(d, P_GARB), -1, "mid-string garbage -> -1, no over-read");
    flow_free(d);
  }

  /* ---- Ephemeral-not-persisted ---- */
  {
    flow_t *f = flow_new(80, 24);
    flow_register_defaults(f);
    int n1 = flow_add_node(f, "default", (flow_pt){2, 2}, (void*)"a");
    flow_add_node(f, "default", (flow_pt){20, 2}, (void*)"b");
    flow_select_node(f, n1, 0);                 /* set FLOW_SELECTED */
    flow_set_hover(f, n1);                       /* set FLOW_HOVERED */
    flow_set_zoom_limits(f, 0.5f, 3.0f);         /* zmin/zmax */
    flow_feed(f, "\x1b[<0;5;5M", 9);             /* drive a press (drag state) */
    ASSERT_INT(flow_save(f, P_A), 0, "save ephemeral graph");
    char *js = slurp(P_A);
    ASSERT(js != NULL, "ephemeral json readable");
    if (js) {
      ASSERT(strstr(js, "\"flags\"")    == NULL, "no flags key");
      ASSERT(strstr(js, "\"selected\"") == NULL, "no selected key");
      ASSERT(strstr(js, "\"zmin\"")     == NULL, "no zmin key");
      ASSERT(strstr(js, "\"zmax\"")     == NULL, "no zmax key");
      ASSERT(strstr(js, "\"marquee\"")  == NULL, "no marquee key");
      ASSERT(strstr(js, "\"w\":")       == NULL, "no w key");
      ASSERT(strstr(js, "\"h\":")       == NULL, "no h key");
      free(js);
    }
    flow_free(f);
  }

  /* ---- Empty graph ---- */
  {
    flow_t *a = flow_new(80, 24);
    flow_register_defaults(a);
    ASSERT_INT(flow_save(a, P_A), 0, "save empty graph");
    flow_t *b = flow_new(80, 24);
    flow_register_defaults(b);
    ASSERT_INT(flow_load(b, P_A), 0, "load empty graph");
    ASSERT_INT(flow_node_count(b), 0, "empty: 0 nodes");
    ASSERT_INT(flow_edge_count(b), 0, "empty: 0 edges");
    int idn = flow_add_node(b, "default", (flow_pt){0, 0}, (void*)"x");
    ASSERT_INT(idn, 1, "empty load: next id is 1");
    flow_free(a); flow_free(b);
  }

  /* clean up temp files */
  remove(P_BASIC); remove(P_A); remove(P_B); remove(P_DEV); remove(P_GARB);

  return flowtest_report("test_json");
}
