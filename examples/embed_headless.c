/* embed_headless — drive flow from your OWN event loop, with no flow_run, no
   termios, and no stdout coupling in the model path.
 *
 * The interactive convenience (flow_run) owns the terminal: it sets raw mode,
 * polls stdin, and writes frames to stdout. When you embed flow inside an app
 * that already has an event loop (a game tick, a GUI host, an ssh session, a
 * test), you don't want any of that. You want two pure calls:
 *
 *   · flow_feed(f, bytes, n)  — hand the model input bytes from WHATEVER source
 *                               you have (your own read(), a socket, a script).
 *                               Same parser flow_run uses; no terminal required.
 *   · flow_render_diff(f)     — render the model, diff it against the previous
 *                               frame, advance the front buffer, and RETURN a
 *                               malloc'd escape string. You write that string to
 *                               whatever fd you own (a pty, a file, a network
 *                               socket), then free() it. "" means nothing moved.
 *
 * flow_present is literally flow_render_diff + fputs(stdout). The model,
 * geometry, render, route, layout, and JSON layers are pure C — only flow_run /
 * flow_present / flow_term_* touch POSIX. Everything below is portable.
 *
 * This file runs a fixed, deterministic script (so it works in CI with no TTY):
 * it builds a graph, then emits a sequence of frames as you feed input. The
 * escape bytes go to stdout — pipe it into a real terminal to actually see it
 * draw ( `./examples/embed_headless` in a terminal ), or redirect to a file to
 * inspect the diff stream. A human-readable trace goes to stderr. */
#define FLOW_IMPLEMENTATION
#include "../flow.h"
#include <stdio.h>

/* "your loop": ask flow for the next frame and write it to the fd you own.
   Here the host fd is stdout; in a real embed it might be a pty or socket. */
static void host_frame(flow_t *f, const char *what) {
  char *esc = flow_render_diff(f);                 /* render + diff + advance front buffer */
  fputs(esc, stdout);                              /* host owns the write — no fflush policy imposed */
  fprintf(stderr, "  %-18s %4zu escape bytes\n", what, strlen(esc));
  free(esc);                                       /* caller owns the string */
}

int main(void) {
  /* The host picks the surface size — there is no terminal to query. */
  flow_t *f = flow_new(80, 24);
  flow_register_defaults(f);

  int a = flow_add_node(f, "default", (flow_pt){ 4,  3}, (void*)"ingest");
  int b = flow_add_node(f, "default", (flow_pt){34, 12}, (void*)"transform");
  int c = flow_add_node(f, "default", (flow_pt){58,  4}, (void*)"sink");
  flow_add_edge(f, a, b, "out", "in");
  flow_add_edge(f, b, c, "out", "in");

  fprintf(stderr, "embed_headless: %d nodes, %d edges, %dx%d surface — no flow_run, no termios\n",
          flow_node_count(f), flow_edge_count(f), 80, 24);

  /* The host's event loop: each step injects input from its own source via
     flow_feed, then pulls a frame via flow_render_diff. None of this blocks on
     or touches a terminal. */
  host_frame(f, "initial frame");                  /* first frame = the whole scene (front starts blank) */

  /* A mouse click is just bytes too: SGR-1006 press+release inside 'ingest'
     (world (4,3), default view) — done before the camera moves it. */
  flow_feed(f, "\x1b[<0;6;5M", 9);
  flow_feed(f, "\x1b[<0;6;5m", 9);
  host_frame(f, "click select");

  flow_feed(f, "\x1b[C", 3);   host_frame(f, "pan right (arrow)");   /* arrow-key pan, same bytes a TTY sends */
  flow_feed(f, "+",     1);    host_frame(f, "zoom in (+)");         /* keyboard zoom, screen-centered */
  flow_feed(f, "+",     1);    host_frame(f, "zoom in (+)");

  host_frame(f, "no-op frame");                    /* nothing changed -> empty diff (0 bytes) */

  fprintf(stderr, "final: zoom %.2f, %d node(s) selected\n",
          (double)flow_zoom(f), flow_selected_count(f));

  flow_free(f);
  return 0;
}
