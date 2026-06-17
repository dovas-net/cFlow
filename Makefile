CC=cc
CFLAGS=-std=c11 -O2 -Wall -Wextra
LIBS=-lm
TESTS=test_smoke test_geom test_cell test_model test_route test_render test_input test_mouse test_select test_marquee test_keys test_connect test_edge test_zoom test_json test_groups test_events test_space_pan test_autopan test_undo test_layout test_flowchart test_extents test_viewport_events test_query test_focus test_clipboard test_helper test_culling test_search test_clock test_animated test_gates test_explicit_size test_node_resizer test_embed

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

clean:
	rm -f demos/hello_flow demos/topo demos/flowchart examples/embed_headless $(addprefix tests/,$(TESTS)) flow.h
.PHONY: all demos examples test clean
