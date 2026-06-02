#ifndef FLOWTEST_H
#define FLOWTEST_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int flowtest_pass = 0, flowtest_fail = 0;
#define ASSERT(cond, msg) do { if (cond) flowtest_pass++; \
  else { flowtest_fail++; printf("  FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); } } while (0)
#define ASSERT_INT(got, want, msg) do { long _g=(long)(got), _w=(long)(want); \
  if (_g==_w) flowtest_pass++; \
  else { flowtest_fail++; printf("  FAIL: %s: got %ld want %ld (%s:%d)\n", msg,_g,_w,__FILE__,__LINE__); } } while (0)
#define ASSERT_STR(got, want, msg) do { const char *_g=(got), *_w=(want); \
  if (strcmp(_g,_w)==0) flowtest_pass++; \
  else { flowtest_fail++; printf("  FAIL: %s:\n   got: <%s>\n  want: <%s> (%s:%d)\n",msg,_g,_w,__FILE__,__LINE__); } } while (0)
static int flowtest_report(const char *name) {
  printf("%s: %d passed, %d failed\n", name, flowtest_pass, flowtest_fail);
  return flowtest_fail ? 1 : 0;
}
/* Snapshot: compare actual against tests/snapshots/<name>.txt; create it on first run. */
static inline void flowtest_snapshot(const char *name, const char *actual) {
  char path[256]; snprintf(path, sizeof path, "tests/snapshots/%s.txt", name);
  FILE *f = fopen(path, "rb");
  if (!f) {
    f = fopen(path, "wb");
    if (!f) { flowtest_fail++; printf("  FAIL: cannot write snapshot %s\n", path); return; }
    fputs(actual, f); fclose(f);
    printf("  [snapshot created: %s] — verify it manually\n", name); flowtest_pass++; return;
  }
  fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
  char *buf = (char*)malloc(n + 1); size_t got = fread(buf, 1, n, f); buf[got] = 0; fclose(f);
  if (strcmp(buf, actual) == 0) flowtest_pass++;
  else { flowtest_fail++; printf("  FAIL snapshot %s:\n--- expected ---\n%s\n--- actual ---\n%s\n", name, buf, actual); }
  free(buf);
}
#define SNAPSHOT(name, actual) flowtest_snapshot(name, actual)
#endif
