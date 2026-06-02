/* ===== terminal: raw mode, alt-screen, size (impl-only POSIX deps) ===== */
void flow_term_setup(void);
void flow_term_restore(void);
int  flow_term_size(int *cols, int *rows);

#ifdef FLOW_IMPLEMENTATION
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>
static struct termios flow__saved_tio;
void flow_term_setup(void) {
  struct termios raw; tcgetattr(STDIN_FILENO, &flow__saved_tio); raw = flow__saved_tio;
  raw.c_lflag &= ~(ECHO | ICANON);
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
  printf("\x1b[?1049h\x1b[2J\x1b[?25l"); fflush(stdout);   /* alt-screen, clear, hide cursor */
}
void flow_term_restore(void) {
  printf("\x1b[0m\x1b[?25h\x1b[?1049l"); fflush(stdout);
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &flow__saved_tio);
}
int flow_term_size(int *cols, int *rows) {
  struct winsize ws;
  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) != 0 || ws.ws_col == 0) return -1;
  *cols = ws.ws_col; *rows = ws.ws_row; return 0;
}
#endif
