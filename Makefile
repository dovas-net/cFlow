CC=cc
CXX=c++
CFLAGS=-std=c11 -O2 -Wall -Wextra
LIBS=-lm
TESTS=test_smoke test_geom test_cell test_model test_route test_render test_input test_mouse test_select test_marquee test_keys test_connect test_edge test_zoom test_json test_groups test_events test_space_pan test_autopan test_undo test_layout test_flowchart test_extents test_viewport_events test_query test_focus test_clipboard test_helper test_culling test_search test_clock test_animated test_gates test_explicit_size test_node_resizer test_cancel_gesture test_route_clip test_embed test_term test_alloc test_oom

all: flow.h demos examples

flow.h: $(wildcard src/*.h) tools/amalgamate.sh
	sh tools/amalgamate.sh

demos: flow.h
	$(CC) $(CFLAGS) demos/hello_flow.c -o demos/hello_flow $(LIBS)
	$(CC) $(CFLAGS) demos/topo.c -o demos/topo $(LIBS)
	$(CC) $(CFLAGS) demos/flowchart.c -o demos/flowchart $(LIBS)

examples: flow.h
	$(CC) $(CFLAGS) examples/embed_headless.c -o examples/embed_headless $(LIBS)

test: flow.h
	@mkdir -p tests/snapshots
	@fail=0; for t in $(TESTS); do \
	  if [ -f tests/$$t.c ]; then \
	    $(CC) $(CFLAGS) tests/$$t.c -o tests/$$t $(LIBS) || { echo "BUILD FAIL $$t"; fail=1; continue; }; \
	    ./tests/$$t || fail=1; \
	  fi; \
	done; exit $$fail

# C++ consumption check (H6). Not in `test` (needs a C++ compiler; CI gates it in Phase 2).
# 1) flow.h compiles+runs AS the implementation under C++ (enum-init fix + custom-type
#    C++ callbacks). 2) a C-compiled flow .o links against a C++ caller, proving extern "C".
# -std=c++17 only (no -Wall) on the impl TU: the hand-tuned color-preset table uses C99
# array designators, which clang/gcc accept as an extension (MSVC would not — see README).
cpp: flow.h
	$(CXX) -std=c++17 tests/cpp_smoke.cpp -o /tmp/flow_cpp_smoke $(LIBS) && /tmp/flow_cpp_smoke && echo "cpp_smoke: impl compiles+runs as C++ OK"
	$(CC)  $(CFLAGS) -c tests/cpp_link_impl.c -o /tmp/flow_link_impl.o
	$(CXX) -std=c++17 -Wall -Wextra tests/cpp_link_main.cpp /tmp/flow_link_impl.o -o /tmp/flow_cpp_link $(LIBS) && /tmp/flow_cpp_link && echo "cpp_link: C flow.o links against a C++ caller (extern \"C\") OK"

# libFuzzer harness over the untrusted flow_load + render path (the only attacker-reachable
# bytes). NOT in `test` — needs clang with -fsanitize=fuzzer; Apple clang lacks it, so on macOS
# pass FUZZ_CC=/opt/homebrew/opt/llvm/bin/clang. -timeout/-rss flag any hang or memory blow-up.
#   make fuzz                                   # 30s smoke over fuzz/corpus
#   make fuzz FUZZ_TIME=300                       # longer campaign
#   make fuzz FUZZ_CC=/opt/homebrew/opt/llvm/bin/clang
FUZZ_CC   ?= clang
FUZZ_TIME ?= 30
fuzz: flow.h
	$(FUZZ_CC) -std=c11 -g -O1 -fsanitize=address,undefined,fuzzer -fno-sanitize-recover=all fuzz/fuzz_load.c -o /tmp/flow_fuzz_load $(LIBS)
	@mkdir -p /tmp/flow_fuzz_corpus
	/tmp/flow_fuzz_load -max_total_time=$(FUZZ_TIME) -rss_limit_mb=2048 -timeout=10 -artifact_prefix=/tmp/flow_fuzz_ /tmp/flow_fuzz_corpus fuzz/corpus
	@echo "(discoveries written to /tmp/flow_fuzz_corpus; committed seeds in fuzz/corpus stay clean)"

clean:
	rm -f demos/hello_flow demos/topo demos/flowchart examples/embed_headless $(addprefix tests/,$(TESTS)) flow.h
.PHONY: all demos examples test cpp fuzz clean
