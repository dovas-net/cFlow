#define FLOW_IMPLEMENTATION
#include "../flow.h"
#include "flowtest.h"
#include <string.h>

/* inc-8 #2 — explicit-size: user-set node w/h that (a) survives auto-measure
   (FLOW_EXPLICIT_SIZE guards the top of flow_measure_node) and (b) persists
   across save/load (",w":N,"h":N emitted ONLY when the flag is set, so a
   default scene stays byte-identical to the json_basic golden). The setter
   flow_set_node_size clamps w,h >= 1 and journals a RESIZE_NODE op so a resize
   gesture (package 3) is one undo step. Paste carries the flag + size.
   This package also migrates FLOW_EXTENT_PARENT onto the same emit-when-set
   on-disk rail (",extentParent":true), the follow-up deferred from inc-8 #1. */

static char *slurp(const char *path) {
  FILE *fp = fopen(path, "rb"); if (!fp) return NULL;
  fseek(fp, 0, SEEK_END); long n = ftell(fp); fseek(fp, 0, SEEK_SET);
  char *buf = (char*)malloc((size_t)n + 1); size_t got = fread(buf, 1, (size_t)n, fp); buf[got] = 0;
  fclose(fp); return buf;
}

int main(void) {
  const char *P = "/tmp/flow_explicit_size.json";
  const char *P2 = "/tmp/flow_explicit_size2.json";

  /* ===== setter: clamp + flag ===== */

  /* 1: flow_set_node_size writes w/h and sets FLOW_EXPLICIT_SIZE */
  {
    flow_t *f = flow_new(80, 30); flow_register_defaults(f);
    int a = flow_add_node(f, "default", (flow_pt){5, 5}, (void*)"X");  /* auto: w=5,h=3 */
    ASSERT_INT(flow_get_node(f, a)->w, 5, "pre-set auto width");
    flow_set_node_size(f, a, 12, 7);
    ASSERT_INT(flow_get_node(f, a)->w, 12, "set width");
    ASSERT_INT(flow_get_node(f, a)->h, 7, "set height");
    ASSERT(flow_get_node(f, a)->flags & FLOW_EXPLICIT_SIZE, "FLOW_EXPLICIT_SIZE set");
    flow_free(f);
  }

  /* 2: w,h clamp to >= 1; unknown id is a no-op (no crash) */
  {
    flow_t *f = flow_new(80, 30); flow_register_defaults(f);
    int a = flow_add_node(f, "default", (flow_pt){5, 5}, (void*)"X");
    flow_set_node_size(f, a, 0, -4);
    ASSERT_INT(flow_get_node(f, a)->w, 1, "width clamps to 1");
    ASSERT_INT(flow_get_node(f, a)->h, 1, "height clamps to 1");
    flow_set_node_size(f, 999, 10, 10);              /* unknown id: no-op, no crash */
    ASSERT_INT(f->nnodes, 1, "unknown-id set is a no-op");
    flow_free(f);
  }

  /* ===== measure-skip ===== */

  /* 3: an explicitly-sized node is NOT resized by a later flow_measure_node */
  {
    flow_t *f = flow_new(80, 30); flow_register_defaults(f);
    int a = flow_add_node(f, "default", (flow_pt){5, 5}, (void*)"X");
    flow_set_node_size(f, a, 20, 9);
    flow_measure_node(f, flow_get_node(f, a));        /* would auto-set 5x3 absent the guard */
    ASSERT_INT(flow_get_node(f, a)->w, 20, "measure skipped: width preserved");
    ASSERT_INT(flow_get_node(f, a)->h, 9,  "measure skipped: height preserved");
    flow_free(f);
  }

  /* 4: a node WITHOUT explicit size still auto-measures (default unchanged) */
  {
    flow_t *f = flow_new(80, 30); flow_register_defaults(f);
    int a = flow_add_node(f, "default", (flow_pt){5, 5}, (void*)"hello");  /* w=9,h=3 */
    flow_measure_node(f, flow_get_node(f, a));
    ASSERT_INT(flow_get_node(f, a)->w, 9, "no flag: auto width");
    ASSERT_INT(flow_get_node(f, a)->h, 3, "no flag: auto height");
    flow_free(f);
  }

  /* ===== group interaction ===== */

  /* 5: flow_group auto-encloses via direct w/h write (no EXPLICIT_SIZE), so the
     measure-skip guard never fires for the container — auto-size is unaffected. */
  {
    flow_t *f = flow_new(80, 30); flow_register_defaults(f);
    int a = flow_add_node(f, "default", (flow_pt){5, 5},  (void*)"A");
    int b = flow_add_node(f, "default", (flow_pt){30, 12}, (void*)"B");
    int ids[2] = { a, b };
    int g = flow_group(f, ids, 2);
    flow_node *gn = flow_get_node(f, g);
    ASSERT(!(gn->flags & FLOW_EXPLICIT_SIZE), "group container has NO explicit-size flag");
    ASSERT(gn->w > 0 && gn->h > 0, "group auto-enclosed to a positive bbox");
    int gw = gn->w, gh = gn->h;
    flow_measure_node(f, gn);                         /* write-back no-op: container size stable */
    ASSERT_INT(flow_get_node(f, g)->w, gw, "group re-measure preserves enclose width");
    ASSERT_INT(flow_get_node(f, g)->h, gh, "group re-measure preserves enclose height");
    flow_free(f);
  }

  /* 6: even if flow_set_node_size is called on a group, the guard is inert
     (group measure is a write-back no-op) — size is honored, nothing breaks. */
  {
    flow_t *f = flow_new(80, 30); flow_register_defaults(f);
    int a = flow_add_node(f, "default", (flow_pt){5, 5}, (void*)"A");
    int ids[1] = { a };
    int g = flow_group(f, ids, 1);
    flow_set_node_size(f, g, 40, 20);
    ASSERT_INT(flow_get_node(f, g)->w, 40, "explicit group width honored");
    ASSERT(flow_get_node(f, g)->flags & FLOW_EXPLICIT_SIZE, "group flag set by setter");
    flow_measure_node(f, flow_get_node(f, g));
    ASSERT_INT(flow_get_node(f, g)->w, 40, "group explicit size survives re-measure");
    flow_free(f);
  }

  /* ===== undo: RESIZE journaled, one step, coalesced ===== */

  /* 7: a standalone flow_set_node_size is undoable to the prior size */
  {
    flow_t *f = flow_new(80, 30); flow_register_defaults(f);
    int a = flow_add_node(f, "default", (flow_pt){5, 5}, (void*)"X");  /* w=5,h=3 */
    flow_set_node_size(f, a, 18, 11);
    ASSERT(flow_can_undo(f), "resize is journaled (undo available)");
    flow_undo(f);
    ASSERT_INT(flow_get_node(f, a)->w, 5, "undo restores prior width");
    ASSERT_INT(flow_get_node(f, a)->h, 3, "undo restores prior height");
    flow_redo(f);
    ASSERT_INT(flow_get_node(f, a)->w, 18, "redo re-applies width");
    ASSERT_INT(flow_get_node(f, a)->h, 11, "redo re-applies height");
    flow_free(f);
  }

  /* 8: two resizes inside one flow__undo_begin/end collapse to ONE undo step
     (the package-3 gesture bracket) — one undo reverts to the pre-gesture size. */
  {
    flow_t *f = flow_new(80, 30); flow_register_defaults(f);
    int a = flow_add_node(f, "default", (flow_pt){5, 5}, (void*)"X");  /* w=5,h=3 */
    int d0 = flow_undo_depth(f);
    flow__undo_begin(f);
    flow_set_node_size(f, a, 10, 6);
    flow_set_node_size(f, a, 22, 13);
    flow__undo_end(f);
    ASSERT_INT(flow_undo_depth(f), d0 + 1, "coalesced resize = ONE undo step");
    flow_undo(f);
    ASSERT_INT(flow_get_node(f, a)->w, 5, "one undo reverts to pre-gesture width");
    ASSERT_INT(flow_get_node(f, a)->h, 3, "one undo reverts to pre-gesture height");
    flow_free(f);
  }

  /* 9: undo of a resize is durable for the flag too (EXPLICIT_SIZE stays set on
     the node after redo; outside the transient-clear mask) */
  {
    flow_t *f = flow_new(80, 30); flow_register_defaults(f);
    int a = flow_add_node(f, "default", (flow_pt){5, 5}, (void*)"X");
    flow_set_node_size(f, a, 14, 8);
    flow_undo(f);
    flow_redo(f);
    ASSERT(flow_get_node(f, a)->flags & FLOW_EXPLICIT_SIZE, "EXPLICIT_SIZE survives undo/redo");
    flow_free(f);
  }

  /* 9b: undo of the FIRST resize on a previously-AUTO node must restore the
     ABSENCE of FLOW_EXPLICIT_SIZE (apply->undo equality) — else a later data
     change would no longer re-measure. */
  {
    flow_t *f = flow_new(80, 30); flow_register_defaults(f);
    int a = flow_add_node(f, "default", (flow_pt){5, 5}, (void*)"hi");  /* auto w=6,h=3, NO flag */
    flow_set_node_size(f, a, 20, 12);
    flow_undo(f);
    ASSERT_INT(flow_get_node(f, a)->w, 6, "undo restores auto width");
    ASSERT(!(flow_get_node(f, a)->flags & FLOW_EXPLICIT_SIZE), "undo clears flag on once-auto node");
    flow_get_node(f, a)->data = (void*)"a-much-longer-label";   /* len 19 => w=23 */
    flow_measure_node(f, flow_get_node(f, a));
    ASSERT_INT(flow_get_node(f, a)->w, 23, "post-undo node re-measures (flag truly cleared)");
    flow_free(f);
  }

  /* 9c: converse — undo of a SECOND resize on an explicit node keeps the flag
     set and restores the first explicit size. */
  {
    flow_t *f = flow_new(80, 30); flow_register_defaults(f);
    int a = flow_add_node(f, "default", (flow_pt){5, 5}, (void*)"X");
    flow_set_node_size(f, a, 10, 6);                  /* first explicit */
    flow_set_node_size(f, a, 22, 13);                 /* second */
    flow_undo(f);                                     /* undo the second */
    ASSERT_INT(flow_get_node(f, a)->w, 10, "undo of 2nd resize restores 1st explicit width");
    ASSERT(flow_get_node(f, a)->flags & FLOW_EXPLICIT_SIZE, "flag preserved (node was already explicit)");
    flow_free(f);
  }

  /* ===== persistence: round-trip + golden safety ===== */

  /* 10: explicit size persists across save/load and re-measure on load is skipped */
  {
    flow_t *a = flow_new(80, 30); flow_register_defaults(a);
    int n = flow_add_node(a, "default", (flow_pt){6, 4}, (void*)"X");
    flow_set_node_size(a, n, 17, 9);
    ASSERT_INT(flow_save(a, P), 0, "save explicit-size graph");

    flow_t *b = flow_new(80, 30); flow_register_defaults(b);
    ASSERT_INT(flow_load(b, P), 0, "load explicit-size graph");
    flow_node *bn = &b->nodes[0];
    ASSERT_INT(bn->w, 17, "loaded width preserved");
    ASSERT_INT(bn->h, 9,  "loaded height preserved");
    ASSERT(bn->flags & FLOW_EXPLICIT_SIZE, "FLOW_EXPLICIT_SIZE restored on load");
    flow_free(a); flow_free(b);
  }

  /* 11: save->load->save is byte-identical with explicit size present */
  {
    flow_t *a = flow_new(80, 30); flow_register_defaults(a);
    int n = flow_add_node(a, "default", (flow_pt){6, 4}, (void*)"X");
    flow_set_node_size(a, n, 17, 9);
    ASSERT_INT(flow_save(a, P), 0, "save A");
    flow_t *b = flow_new(80, 30); flow_register_defaults(b);
    ASSERT_INT(flow_load(b, P), 0, "load A into B");
    ASSERT_INT(flow_save(b, P2), 0, "re-save B");
    char *sa = slurp(P), *sb = slurp(P2);
    ASSERT(sa && sb && strcmp(sa, sb) == 0, "save->load->save byte-identical");
    free(sa); free(sb);
    flow_free(a); flow_free(b);
  }

  /* 12: a default scene (no explicit size) emits NO "w"/"h" keys (golden-safe) */
  {
    flow_t *f = flow_new(80, 30); flow_register_defaults(f);
    flow_add_node(f, "default", (flow_pt){6, 3}, (void*)"hi");  /* auto-measured */
    ASSERT_INT(flow_save(f, P), 0, "save default scene");
    char *s = slurp(P);
    ASSERT(s != NULL, "read default scene");
    ASSERT(strstr(s, "\"w\":") == NULL, "default scene emits no \"w\" key");
    ASSERT(strstr(s, "\"h\":") == NULL, "default scene emits no \"h\" key");
    free(s);
    flow_free(f);
  }

  /* 13: an absent w/h on load => auto-measured as before (back-compat) */
  {
    /* hand-write a node object with no w/h and label "hello" (auto w=9,h=3) */
    FILE *fp = fopen(P, "wb");
    fputs("{\"version\":1,\n \"viewport\":{\"ox\":0,\"oy\":0,\"zoom\":1},\n"
          " \"nodes\":[{\"id\":1,\"type\":\"default\",\"x\":0,\"y\":0,\"parent\":-1}],\n"
          " \"edges\":[]}\n", fp);
    fclose(fp);
    flow_t *b = flow_new(80, 30); flow_register_defaults(b);
    ASSERT_INT(flow_load(b, P), 0, "load no-w/h node");
    /* default measure on NULL data => label "" len 1 => w=5,h=3 */
    ASSERT_INT(b->nodes[0].w, 5, "absent w => auto-measured");
    ASSERT_INT(b->nodes[0].h, 3, "absent h => auto-measured");
    ASSERT(!(b->nodes[0].flags & FLOW_EXPLICIT_SIZE), "absent w/h => no explicit flag");
    flow_free(b);
  }

  /* ===== paste carries flag + size ===== */

  /* 14: copy/paste of an explicitly-sized node carries BOTH the size and the
     flag (so the pasted clone survives re-measure and round-trips). */
  {
    flow_t *f = flow_new(80, 30); flow_register_defaults(f);
    int a = flow_add_node(f, "default", (flow_pt){5, 5}, (void*)"X");
    flow_set_node_size(f, a, 16, 10);
    flow_select_node(f, a, 0);
    flow_copy_selection(f);
    int before = f->nnodes;
    flow_paste(f);
    ASSERT_INT(f->nnodes, before + 1, "paste added one node");
    flow_node *clone = &f->nodes[f->nnodes - 1];
    ASSERT_INT(clone->w, 16, "pasted clone width carried");
    ASSERT_INT(clone->h, 10, "pasted clone height carried");
    ASSERT(clone->flags & FLOW_EXPLICIT_SIZE, "pasted clone carries EXPLICIT_SIZE flag");
    flow_measure_node(f, clone);                       /* survives re-measure */
    ASSERT_INT(flow_get_node(f, clone->id)->w, 16, "pasted clone survives re-measure");
    flow_free(f);
  }

  /* ===== FLOW_EXTENT_PARENT on disk (inc-8 #1 deferred follow-up) ===== */

  /* 15: extentParent persists round-trip, emit-when-set, byte-identical */
  {
    flow_t *a = flow_new(80, 30); flow_register_defaults(a);
    int g  = flow_add_node(a, "group",   (flow_pt){0, 0}, NULL);
    int ch = flow_add_node(a, "default", (flow_pt){2, 2}, (void*)"c");
    flow_set_parent(a, ch, g);
    flow_get_node(a, ch)->flags |= FLOW_EXTENT_PARENT;
    ASSERT_INT(flow_save(a, P), 0, "save extentParent graph");
    char *sa = slurp(P);
    ASSERT(sa && strstr(sa, "\"extentParent\":true") != NULL, "extentParent emitted when set");
    free(sa);

    flow_t *b = flow_new(80, 30); flow_register_defaults(b);
    ASSERT_INT(flow_load(b, P), 0, "load extentParent graph");
    int found = 0;
    for (int i = 0; i < b->nnodes; i++)
      if (b->nodes[i].flags & FLOW_EXTENT_PARENT) found = 1;
    ASSERT(found, "FLOW_EXTENT_PARENT restored on load");
    ASSERT_INT(flow_save(b, P2), 0, "re-save extentParent graph");
    char *s1 = slurp(P), *s2 = slurp(P2);
    ASSERT(s1 && s2 && strcmp(s1, s2) == 0, "extentParent save->load->save byte-identical");
    free(s1); free(s2);
    flow_free(a); flow_free(b);
  }

  /* 16: a node WITHOUT extentParent emits no extentParent key (golden-safe) */
  {
    flow_t *f = flow_new(80, 30); flow_register_defaults(f);
    flow_add_node(f, "default", (flow_pt){3, 3}, (void*)"n");
    ASSERT_INT(flow_save(f, P), 0, "save no-extent scene");
    char *s = slurp(P);
    ASSERT(s && strstr(s, "extentParent") == NULL, "no extentParent key when unset");
    free(s);
    flow_free(f);
  }

  return flowtest_report("test_explicit_size");
}
