#define FLOW_IMPLEMENTATION
#include "../flow.h"
#include "flowtest.h"
#include <string.h>

/* Key hook gate + label vtable + find query (inc-5 #10, engine half).
   flow_set_key_hook installs a pre-dispatch interceptor on struct flow (a GATE,
   the validator precedent — not a callbacks observer) that sees every key/escape
   sequence BEFORE flow_bind_key bindings and built-ins and returns BYTES CONSUMED
   (0 = pass-through). flow_find_nodes is a model-level case-insensitive substring
   query over the new optional label() vtable accessor — hidden included,
   insertion order, fill-buffer idiom, no allocation. */

static void feed(flow_t *f, const char *s) { flow_feed(f, s, (int)strlen(s)); }

static int g_bound = 0;
static void bound_g(flow_t *f, void *u) { (void)f; (void)u; g_bound++; }

static int hook_hits = 0;
static int hook_consume1(flow_t *f, const char *seq, int len, void *user) {
  (void)f; (void)seq; (void)len; (void)user; hook_hits++; return 1;
}
static int hook_pass(flow_t *f, const char *seq, int len, void *user) {
  (void)f; (void)seq; (void)len; (void)user; hook_hits++; return 0;
}
/* consumes TWO bytes when the seq starts with 'A' (multibyte-consume contract) */
static int hook_two(flow_t *f, const char *seq, int len, void *user) {
  (void)f; (void)user;
  if (seq[0] == 'A' && len >= 2) return 2;
  return 0;
}

/* a type with NO label accessor (explicit NULL here; the ABI-append zero-init
   contract is separately proven by test_model.c's old 5-field initializer) */
static void nl_measure(const flow_node *n, int *w, int *h) { (void)n; *w = 3; *h = 1; }
static const flow_node_type NOLAB = { "nolab", nl_measure, NULL, NULL, 0, NULL, NULL, NULL };

int main(void) {
  /* ---- hook ordering: consuming hook beats registry AND built-ins ---- */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    int a = flow_add_node(f, "default", (flow_pt){5, 5}, (void*)"A");
    flow_bind_key(f, "g", bound_g, NULL);
    flow_set_key_hook(f, hook_consume1, NULL);
    g_bound = 0; hook_hits = 0;
    feed(f, "g");
    ASSERT_INT(hook_hits, 1, "hook saw the byte");
    ASSERT_INT(g_bound, 0, "consuming hook beats the registry binding");
    flow_select_node(f, a, 0);
    feed(f, "x");
    ASSERT_INT(flow_node_count(f), 1, "consuming hook beats the built-in delete");

    /* pass-through: returning 0 lets dispatch continue */
    flow_set_key_hook(f, hook_pass, NULL);
    g_bound = 0;
    feed(f, "g");
    ASSERT_INT(g_bound, 1, "pass-through hook lets the binding fire");

    /* byte-count consume: returning 2 advances flow_feed past BOTH bytes */
    flow_set_key_hook(f, hook_two, NULL);
    g_bound = 0;
    feed(f, "Ag");                      /* hook eats 'A' AND 'g' in one bite */
    ASSERT_INT(g_bound, 0, "2-byte consume swallowed the trailing byte");
    feed(f, "g");                       /* hook returns 0 for 'g' alone */
    ASSERT_INT(g_bound, 1, "  next feed reaches the binding normally");

    /* clearing the hook restores normal dispatch */
    flow_set_key_hook(f, NULL, NULL);
    g_bound = 0;
    feed(f, "g");
    ASSERT_INT(g_bound, 1, "cleared hook: binding fires again");
    flow_free(f);
  }

  /* ---- flow_find_nodes: match, order, case, hidden, NULL-label, fill ---- */
  {
    flow_t *f = flow_new(80, 24); flow_register_defaults(f);
    flow_register_node_type(f, &NOLAB);
    int alpha    = flow_add_node(f, "default", (flow_pt){0, 0},  (void*)"alpha");
    int beta     = flow_add_node(f, "default", (flow_pt){20, 0}, (void*)"beta");
    int alphabet = flow_add_node(f, "default", (flow_pt){40, 0}, (void*)"alphabet");
    flow_add_node(f, "nolab", (flow_pt){60, 0}, (void*)"alphaNOT");  /* label()==NULL: unsearchable */
    int out[8];

    ASSERT_INT(flow_find_nodes(f, "alph", out, 8), 2, "substring match count");
    ASSERT_INT(out[0], alpha,    "insertion order: alpha first");
    ASSERT_INT(out[1], alphabet, "  alphabet second");

    ASSERT_INT(flow_find_nodes(f, "BET", out, 8), 2, "case-insensitive: BET matches beta+alphabet");
    ASSERT_INT(out[0], beta, "  beta first (insertion order)");

    ASSERT_INT(flow_find_nodes(f, "", out, 8), 3, "empty needle matches every LABELED node (nolab skipped)");

    flow_set_node_hidden(f, alpha, 1);
    ASSERT_INT(flow_find_nodes(f, "alph", out, 8), 2, "hidden INCLUDED (model-level query)");
    flow_set_node_hidden(f, alpha, 0);

    /* fill-buffer idiom: true total past max; NULL/0 legal */
    int small[1] = { -7 };
    ASSERT_INT(flow_find_nodes(f, "alph", small, 1), 2, "true total returned past max");
    ASSERT_INT(small[0], alpha, "  only out[0] written");
    ASSERT_INT(flow_find_nodes(f, "alph", NULL, 0), 2, "NULL/0 count-only query");

    ASSERT_INT(flow_find_nodes(f, "zzz", out, 8), 0, "no match -> 0");
    flow_free(f);
  }

  return flowtest_report("test_search");
}
