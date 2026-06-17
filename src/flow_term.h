/* ===== terminal: raw mode, alt-screen, size (impl-only POSIX deps) ===== */
void flow_term_setup(void);
void flow_term_restore(void);
int  flow_term_size(int *cols, int *rows);

#ifdef FLOW_IMPLEMENTATION
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <errno.h>
static struct termios flow__saved_tio;
static volatile sig_atomic_t flow__term_active = 0;   /* 1 while we own raw mode + the alt-screen */
/* The restore sequence — the byte-exact inverse of flow_term_setup's enables:
   disable SGR/1002/1000 mouse, reset SGR, show cursor, leave the alt-screen. Held
   as a static const so the signal handler can emit it with write() — printf/fputs
   allocate/lock and are NOT async-signal-safe. */
static const char flow__restore_seq[] = "\x1b[?1006l\x1b[?1002l\x1b[?1000l\x1b[0m\x1b[?25h\x1b[?1049l";
/* Async-signal-safe terminal restore: write() + tcsetattr only (both AS-safe). */
static void flow__term_restore_raw(void) {
  ssize_t w = write(STDOUT_FILENO, flow__restore_seq, sizeof flow__restore_seq - 1); (void)w;
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &flow__saved_tio);
}
/* Signals we restore-the-terminal-then-die on. SIGINT/SIGTERM are the must-haves
   (Ctrl-C, kill); the rest restore the terminal before a crash cores/aborts so a
   segfault doesn't leave the user staring at a dead raw-mode terminal. */
static const int flow__sig_list[] = { SIGINT, SIGTERM, SIGHUP, SIGQUIT, SIGSEGV, SIGABRT, SIGBUS, SIGFPE, SIGILL };
#define FLOW__NSIG ((int)(sizeof flow__sig_list / sizeof flow__sig_list[0]))
static struct sigaction flow__old_sa[FLOW__NSIG];
static volatile sig_atomic_t flow__sig_installed = 0;
static void flow__signal_handler(int sig) {
  int saved = errno;                                  /* a handler must not perturb errno */
  if (flow__term_active) flow__term_restore_raw();
  errno = saved;
  /* Restore the PREVIOUS disposition and re-raise: the original action (default
     core/terminate, or a debugger's / ASan's handler) then runs with correct
     status — better than forcing SIG_DFL, which would suppress those reports. */
  for (int i = 0; i < FLOW__NSIG; i++)
    if (flow__sig_list[i] == sig) { sigaction(sig, &flow__old_sa[i], NULL); break; }
  raise(sig);
}
static void flow__install_signal_handlers(void) {
  if (flow__sig_installed) return;                    /* idempotent: don't clobber the saved priors */
  struct sigaction sa; memset(&sa, 0, sizeof sa);
  sa.sa_handler = flow__signal_handler; sigemptyset(&sa.sa_mask); sa.sa_flags = 0;
  for (int i = 0; i < FLOW__NSIG; i++) sigaction(flow__sig_list[i], &sa, &flow__old_sa[i]);
  flow__sig_installed = 1;
}
static void flow__remove_signal_handlers(void) {
  if (!flow__sig_installed) return;
  for (int i = 0; i < FLOW__NSIG; i++) sigaction(flow__sig_list[i], &flow__old_sa[i], NULL);
  flow__sig_installed = 0;
}
static void flow__term_atexit(void) {                 /* exit() mid-run skips flow_term_restore — catch it */
  if (flow__term_active) flow__term_restore_raw();
}
void flow_term_setup(void) {
  struct termios raw; tcgetattr(STDIN_FILENO, &flow__saved_tio); raw = flow__saved_tio;
  raw.c_lflag &= ~(ECHO | ICANON);
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
  /* alt-screen, clear, hide cursor; enable SGR mouse (click+drag+wheel) */
  printf("\x1b[?1049h\x1b[2J\x1b[?25l\x1b[?1000h\x1b[?1002h\x1b[?1006h"); fflush(stdout);
  flow__term_active = 1;
  { static int atexit_done = 0; if (!atexit_done) { atexit(flow__term_atexit); atexit_done = 1; } }
  flow__install_signal_handlers();   /* scoped to the terminal path: the headless embed path never calls setup */
}
void flow_term_restore(void) {
  flow__remove_signal_handlers();
  flow__term_active = 0;
  fflush(stdout);                                     /* drain any buffered frame before the raw write */
  flow__term_restore_raw();                           /* same bytes the signal handler would emit */
}
int flow_term_size(int *cols, int *rows) {
  struct winsize ws;
  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) != 0 || ws.ws_col == 0) return -1;
  *cols = ws.ws_col; *rows = ws.ws_row; return 0;
}
#endif
