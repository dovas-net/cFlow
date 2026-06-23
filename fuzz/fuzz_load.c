/* libFuzzer harness for flow's only untrusted-bytes entry point: flow_load, followed by a
   flow_render of the loaded graph (the edge-router walk is the DoS surface, so loading without
   rendering would miss it). Drives ASan + UBSan + libFuzzer over the JSON parser and the render
   path; -timeout / -rss_limit_mb make libFuzzer flag any hang or memory blow-up (e.g. an
   unclipped far-coordinate route) as a finding.

   Apple clang does NOT ship -fsanitize=fuzzer; use the open-source LLVM clang
   (Homebrew: /opt/homebrew/opt/llvm/bin/clang). The `fuzz` Makefile target wires this up:

     make fuzz                                   # 30s smoke run over fuzz/corpus
     make fuzz FUZZ_TIME=300                      # longer campaign
     make fuzz FUZZ_CC=/opt/homebrew/opt/llvm/bin/clang

   flow_load takes a PATH, so each input is written to a temp file first. That file I/O caps
   throughput; a future flow_load-from-memory entry point would let the harness call it directly. */
#define FLOW_IMPLEMENTATION
#include "../flow.h"
#include <stdint.h>
#include <stddef.h>
#include <unistd.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  char path[] = "/tmp/flow_fuzz_XXXXXX";
  int fd = mkstemp(path);
  if (fd < 0) return 0;
  if (write(fd, data, size) != (ssize_t)size) { close(fd); unlink(path); return 0; }
  close(fd);

  flow_t *f = flow_new(80, 24);
  if (f) {
    flow_register_defaults(f);
    if (flow_load(f, path) == 0) {                 /* only valid JSON reaches the render path */
      flow_cell *buf = (flow_cell*)calloc((size_t)80 * 24, sizeof(flow_cell));
      if (buf) { flow_render(f, buf, 80, 24); free(buf); }
    }
    flow_free(f);
  }
  unlink(path);
  return 0;
}
