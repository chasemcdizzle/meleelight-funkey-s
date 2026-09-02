// port/foh/foh_crashlog.c — see foh_crashlog.h for what this may and may not do.
#include "foh_crashlog.h"

#include <fcntl.h>
#include <signal.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define CL_PATH_MAX 512
static char g_path[CL_PATH_MAX];
static char g_sha[17];
static volatile sig_atomic_t g_in_handler;

// Hand-rolled because snprintf is not async-signal-safe. Appends `v` in hex to
// buf at *n, bounded.
static void cl_hex(char *buf, size_t cap, size_t *n, uint64_t v) {
  static const char *H = "0123456789abcdef";
  char tmp[17];
  int k = 0;
  if (v == 0) tmp[k++] = '0';
  while (v && k < 16) { tmp[k++] = H[v & 0xF]; v >>= 4; }
  while (k > 0 && *n + 1 < cap) buf[(*n)++] = tmp[--k];
}

static void cl_dec(char *buf, size_t cap, size_t *n, long v) {
  char tmp[24];
  int k = 0;
  if (v < 0) { if (*n + 1 < cap) buf[(*n)++] = '-'; v = -v; }
  if (v == 0) tmp[k++] = '0';
  while (v && k < 24) { tmp[k++] = (char)('0' + (v % 10)); v /= 10; }
  while (k > 0 && *n + 1 < cap) buf[(*n)++] = tmp[--k];
}

static void cl_str(char *buf, size_t cap, size_t *n, const char *s) {
  while (*s && *n + 1 < cap) buf[(*n)++] = *s++;
}

static void cl_handler(int sig, siginfo_t *si, void *uc) {
  (void)uc;
  // A fault INSIDE the handler must not loop forever writing lines.
  if (g_in_handler) _exit(128 + sig);
  g_in_handler = 1;

  char line[256];
  size_t n = 0;
  cl_str(line, sizeof line, &n, "CRASH sig=");
  cl_dec(line, sizeof line, &n, sig);
  cl_str(line, sizeof line, &n, " code=");
  cl_dec(line, sizeof line, &n, si ? si->si_code : 0);
  cl_str(line, sizeof line, &n, " addr=0x");
  cl_hex(line, sizeof line, &n, si ? (uint64_t)(uintptr_t)si->si_addr : 0);
  // The program counter is where the fault happened and is the whole point of
  // the line, but ucontext_t's layout is platform-specific and getting it
  // wrong is a crash inside a crash handler. It is therefore NOT read here.
  // `addr` plus the log's last line localises well enough in practice, and
  // when it does not, the honest fix is to add the one platform's mcontext
  // offset deliberately rather than to guess at a portable one.
  cl_str(line, sizeof line, &n, " bin=");
  cl_str(line, sizeof line, &n, g_sha);
  cl_str(line, sizeof line, &n, " up=");
  {
    struct timespec ts;
    // CLOCK_MONOTONIC via clock_gettime is async-signal-safe on Linux.
    if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0) {
      cl_dec(line, sizeof line, &n, (long)ts.tv_sec);
    } else {
      cl_str(line, sizeof line, &n, "?");
    }
  }
  if (n + 1 < sizeof line) line[n++] = '\n';

  if (g_path[0]) {
    const int fd = open(g_path, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd >= 0) {
      ssize_t w = write(fd, line, n);
      (void)w;
      // The card is the point: without this the line can sit in page cache
      // and be lost to the power-off that usually follows.
      fsync(fd);
      close(fd);
    }
  }
  // Also to stderr, which the launcher copies to the card on a normal exit —
  // belt and braces, and it is what a host-side run sees.
  { ssize_t w = write(2, line, n); (void)w; }

  // Re-raise with the DEFAULT action so the exit status still reports the
  // signal (opk.rc keeps saying 139), and any core dump policy still applies.
  signal(sig, SIG_DFL);
  raise(sig);
  _exit(128 + sig);
}

void foh_crashlog_install(const char *dir, const char *binsha) {
  g_path[0] = 0;
  if (dir && *dir) {
    size_t n = 0;
    cl_str(g_path, sizeof g_path, &n, dir);
    cl_str(g_path, sizeof g_path, &n, "/mlfk-crash.log");
    if (n + 1 < sizeof g_path) g_path[n] = 0; else g_path[0] = 0;
  }
  memset(g_sha, 0, sizeof g_sha);
  if (binsha) {
    size_t i = 0;
    for (; i < sizeof g_sha - 1 && binsha[i]; i++) g_sha[i] = binsha[i];
  }
  struct sigaction sa;
  memset(&sa, 0, sizeof sa);
  sa.sa_sigaction = cl_handler;
  sigemptyset(&sa.sa_mask);
  // SA_NODEFER off (the default) so a second identical fault cannot recurse;
  // g_in_handler covers the cross-signal case.
  sa.sa_flags = SA_SIGINFO;
  static const int kSigs[] = {SIGSEGV, SIGBUS, SIGILL, SIGFPE, SIGABRT};
  for (size_t i = 0; i < sizeof kSigs / sizeof kSigs[0]; i++) {
    sigaction(kSigs[i], &sa, 0);
  }
}
