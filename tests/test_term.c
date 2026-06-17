/* test_term — async-signal-safe terminal restore (H4). Verifies the restore
   sequence, the write()-based raw restore (the only path a signal handler may
   take), and that install/remove save+restore the PREVIOUS handlers exactly.
   No signal is ever raised here — the actual kill path is manual/integration. */
#define FLOW_IMPLEMENTATION
#include "../flow.h"
#include "flowtest.h"
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <fcntl.h>

int main(void) {
  /* (1) the restore sequence is the exact inverse of flow_term_setup's enables:
     mouse off (1006/1002/1000), SGR reset, cursor on, leave the alt-screen. */
  ASSERT_STR(flow__restore_seq,
             "\x1b[?1006l\x1b[?1002l\x1b[?1000l\x1b[0m\x1b[?25h\x1b[?1049l",
             "restore seq is the byte-exact inverse of setup");

  /* (2) flow__term_restore_raw emits EXACTLY that sequence via write() (the
     async-signal-safe path — printf/fflush are NOT AS-safe). Capture it by
     redirecting the stdout fd to a temp file. We first snapshot the real termios
     into flow__saved_tio so the raw restore's tcsetattr is a no-op even when the
     suite is run from an interactive terminal. */
  {
    tcgetattr(STDIN_FILENO, &flow__saved_tio);   /* no-op restore on a real tty; fails harmlessly on a pipe */
    FILE *tmp = tmpfile();
    ASSERT(tmp != NULL, "tmpfile for stdout capture");
    fflush(stdout);
    int saved = dup(STDOUT_FILENO);
    dup2(fileno(tmp), STDOUT_FILENO);
    flow__term_active = 1;
    flow__term_restore_raw();
    dup2(saved, STDOUT_FILENO);
    close(saved);
    flow__term_active = 0;
    fseek(tmp, 0, SEEK_SET);
    char buf[128]; size_t n = fread(buf, 1, sizeof buf - 1, tmp); buf[n] = 0; fclose(tmp);
    ASSERT_STR(buf, flow__restore_seq, "restore_raw write()s exactly the restore seq");
  }

  /* (3) install replaces SIGINT/SIGTERM with flow's handler; remove restores the
     PREVIOUS disposition exactly. Proven via sigaction queries — no raise(). */
  {
    struct sigaction ign, dfl, cur;
    memset(&ign, 0, sizeof ign); ign.sa_handler = SIG_IGN; sigemptyset(&ign.sa_mask);
    memset(&dfl, 0, sizeof dfl); dfl.sa_handler = SIG_DFL; sigemptyset(&dfl.sa_mask);
    sigaction(SIGINT, &ign, NULL);                 /* seed known priors */
    sigaction(SIGTERM, &dfl, NULL);

    flow__install_signal_handlers();
    sigaction(SIGINT, NULL, &cur);
    ASSERT(cur.sa_handler == flow__signal_handler, "install: SIGINT -> flow handler");
    sigaction(SIGTERM, NULL, &cur);
    ASSERT(cur.sa_handler == flow__signal_handler, "install: SIGTERM -> flow handler");

    flow__remove_signal_handlers();
    sigaction(SIGINT, NULL, &cur);
    ASSERT(cur.sa_handler == SIG_IGN, "remove: SIGINT restored to prior SIG_IGN");
    sigaction(SIGTERM, NULL, &cur);
    ASSERT(cur.sa_handler == SIG_DFL, "remove: SIGTERM restored to prior SIG_DFL");
    sigaction(SIGINT, &dfl, NULL);                 /* don't leak SIG_IGN into later code */
  }

  /* (4) double install is a no-op — the saved prior handler is not clobbered. */
  {
    struct sigaction dfl, cur;
    memset(&dfl, 0, sizeof dfl); dfl.sa_handler = SIG_DFL; sigemptyset(&dfl.sa_mask);
    sigaction(SIGINT, &dfl, NULL);
    flow__install_signal_handlers();
    flow__install_signal_handlers();               /* guarded second call */
    flow__remove_signal_handlers();
    sigaction(SIGINT, NULL, &cur);
    ASSERT(cur.sa_handler == SIG_DFL, "idempotent install: prior handler still restored");
  }

  /* (5) integration — the REAL signal path (a unit test can't fake it): a child
     owns a /dev/null-backed (non-tty) terminal, then takes a SIGINT. The handler
     must restore the terminal (emit the restore seq as its LAST output) and
     re-raise so the child dies WITH SIGINT status — never reaching _exit(99). */
  {
    int p[2];
    ASSERT_INT(pipe(p), 0, "pipe for child output capture");
    pid_t pid = fork();
    ASSERT(pid >= 0, "fork child");
    if (pid == 0) {                                /* CHILD: no ASSERT here (its counters are lost) */
      int dn = open("/dev/null", O_RDWR);
      if (dn >= 0) dup2(dn, STDIN_FILENO);         /* non-tty stdin: setup's tcsetattr is a no-op, real tty untouched */
      dup2(p[1], STDOUT_FILENO);                   /* capture setup + restore escapes */
      close(p[0]); close(p[1]);
      flow_term_setup();
      raise(SIGINT);
      _exit(99);                                   /* unreachable iff the handler re-raises */
    }
    close(p[1]);
    char out[1024]; size_t tot = 0; ssize_t r;
    while (tot < sizeof out - 1 && (r = read(p[0], out + tot, sizeof out - 1 - tot)) > 0) tot += (size_t)r;
    out[tot] = 0; close(p[0]);
    int st = 0; waitpid(pid, &st, 0);
    ASSERT(WIFSIGNALED(st) && WTERMSIG(st) == SIGINT, "child dies from the re-raised SIGINT, not exit(99)");
    size_t rl = strlen(flow__restore_seq);
    ASSERT(tot >= rl && memcmp(out + tot - rl, flow__restore_seq, rl) == 0,
           "handler emits the restore seq as its LAST output before dying");
  }

  return flowtest_report("test_term");
}
