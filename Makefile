CC=cc
CFLAGS=-std=c11 -O2 -Wall -Wextra
LIBS=-lm
TESTS=test_smoke test_geom test_cell test_model test_route test_render test_input test_mouse test_select test_marquee test_keys test_connect test_edge test_zoom test_json test_groups test_events test_space_pan test_autopan test_undo test_layout test_flowchart

all: flow.h demos

flow.h: $(wildcard src/*.h) tools/amalgamate.sh
	sh tools/amalgamate.sh

demos: flow.h
	$(CC) $(CFLAGS) demos/hello_flow.c -o demos/hello_flow $(LIBS)
	$(CC) $(CFLAGS) demos/topo.c -o demos/topo $(LIBS)
	$(CC) $(CFLAGS) demos/flowchart.c -o demos/flowchart $(LIBS)

test: flow.h
	@mkdir -p tests/snapshots
	@fail=0; for t in $(TESTS); do \
	  if [ -f tests/$$t.c ]; then \
	    $(CC) $(CFLAGS) tests/$$t.c -o tests/$$t $(LIBS) || { echo "BUILD FAIL $$t"; fail=1; continue; }; \
	    ./tests/$$t || fail=1; \
	  fi; \
	done; exit $$fail

clean:
	rm -f demos/hello_flow $(addprefix tests/,$(TESTS)) flow.h
.PHONY: all demos test clean
