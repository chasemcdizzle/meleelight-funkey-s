// port/sim/device/skip-attrib/sk_sampler.c — M4 task 8: on-device
// kernel-counter sampler for the skip-stall attribution instrument.
//
// DESIGN (AGENT-LOG iter 74 pre-registration): the FunKey-S is
// SINGLE-CORE, so a concurrent sampler competes with the paced app —
// this sampler is therefore (a) fork-free (held fds + pread, never a
// shell pipeline), (b) RAM-buffered (zero file I/O until told to stop),
// (c) run only in the matrix arms that measure its own perturbation
// against the nosampler control arm (iter-62 probe-order lesson).
//
//   sk_sampler --out F --pid-file P --stop-file S --period-ms N
//              --max-samples M
//
// Every N ms (absolute schedule, CLOCK_MONOTONIC) it preads the FULL
// text of /proc/stat, /proc/interrupts and /proc/softirqs into RAM
// with a CLOCK_MONOTONIC ns stamp. When the stop file appears (or M
// samples are reached) it writes everything to --out and exits 0.
//
// OUTPUT GRAMMAR (load-bearing — paired with correlate-skips.js):
//   repeated blocks:
//     SAMPLE <idx> <mono_ns>\n
//     FILE stat <bytes>\n<raw bytes>
//     FILE interrupts <bytes>\n<raw bytes>
//     FILE softirqs <bytes>\n<raw bytes>
//   terminator (written ONLY after every block above):
//     SAMPLER DONE <count> <overflows>\n
// Length-prefixed raw blocks make truncation mechanically detectable;
// a read that fills the whole 4 KiB slot is counted as an overflow
// (the correlator dies loud on any overflow — a clipped counter table
// must never read as a small delta). Slot size is deliberately small:
// the device has 64 MB and the paced app is live — the sampler's whole
// RAM footprint stays < 5 MB so memory pressure cannot become the
// thing being measured.
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define SK_FILES 3
#define SK_CAP 4096 // per-file per-sample slot (measured ~2.2 KiB max)

static const char *sk_paths[SK_FILES] = {"/proc/stat", "/proc/interrupts",
                                         "/proc/softirqs"};
static const char *sk_names[SK_FILES] = {"stat", "interrupts", "softirqs"};

static void die(const char *what) {
  fprintf(stderr, "sk_sampler: %s\n", what);
  exit(1);
}

static uint64_t now_ns(void) {
  struct timespec ts;
  if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) die("clock_gettime failed");
  return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

static void sleep_until_ns(uint64_t target) {
  for (;;) {
    const uint64_t now = now_ns();
    if (now >= target) return;
    const uint64_t rem = target - now;
    struct timespec ts;
    ts.tv_sec = (time_t)(rem / 1000000000ull);
    ts.tv_nsec = (long)(rem % 1000000000ull);
    nanosleep(&ts, 0); // EINTR: loop re-derives the remainder
  }
}

int main(int argc, char **argv) {
  const char *outPath = 0, *pidPath = 0, *stopPath = 0;
  long periodMs = -1, maxSamples = -1;
  for (int i = 1; i < argc; i++) {
    const char *a = argv[i];
    const int hasV = i + 1 < argc;
    if (strcmp(a, "--out") == 0 && hasV) outPath = argv[++i];
    else if (strcmp(a, "--pid-file") == 0 && hasV) pidPath = argv[++i];
    else if (strcmp(a, "--stop-file") == 0 && hasV) stopPath = argv[++i];
    else if (strcmp(a, "--period-ms") == 0 && hasV) periodMs = strtol(argv[++i], 0, 10);
    else if (strcmp(a, "--max-samples") == 0 && hasV) maxSamples = strtol(argv[++i], 0, 10);
    else die("bad argument");
  }
  if (!outPath || !pidPath || !stopPath || periodMs <= 0 || periodMs > 10000 ||
      maxSamples <= 0 || maxSamples > 100000) {
    die("usage: sk_sampler --out F --pid-file P --stop-file S "
        "--period-ms N --max-samples M");
  }

  int fds[SK_FILES];
  for (int k = 0; k < SK_FILES; k++) {
    fds[k] = open(sk_paths[k], O_RDONLY);
    if (fds[k] < 0) die("cannot open a /proc counter file");
  }

  // RAM buffers up front: samples * files * slot + stamps.
  char *buf = malloc((size_t)maxSamples * SK_FILES * SK_CAP);
  uint64_t *stamps = malloc((size_t)maxSamples * sizeof *stamps);
  size_t *lens = malloc((size_t)maxSamples * SK_FILES * sizeof *lens);
  if (!buf || !stamps || !lens) die("oom");

  {
    FILE *pf = fopen(pidPath, "w");
    if (!pf) die("cannot write pid file");
    fprintf(pf, "%ld\n", (long)getpid());
    if (fclose(pf) != 0) die("pid file close failed");
  }

  long count = 0, overflows = 0;
  const uint64_t t0 = now_ns();
  for (long i = 0; i < maxSamples; i++) {
    sleep_until_ns(t0 + (uint64_t)i * (uint64_t)periodMs * 1000000ull);
    if (access(stopPath, F_OK) == 0) break;
    stamps[count] = now_ns();
    for (int k = 0; k < SK_FILES; k++) {
      char *slot = buf + ((size_t)count * SK_FILES + (size_t)k) * SK_CAP;
      const ssize_t n = pread(fds[k], slot, SK_CAP, 0);
      if (n < 0) die("pread on a /proc counter file failed");
      if (n == SK_CAP) overflows++; // clipped — flagged, judged host-side
      lens[(size_t)count * SK_FILES + (size_t)k] = (size_t)n;
    }
    count++;
  }

  FILE *of = fopen(outPath, "w");
  if (!of) die("cannot open --out");
  for (long i = 0; i < count; i++) {
    if (fprintf(of, "SAMPLE %ld %" PRIu64 "\n", i, stamps[i]) < 0) die("write failed");
    for (int k = 0; k < SK_FILES; k++) {
      const size_t n = lens[(size_t)i * SK_FILES + (size_t)k];
      if (fprintf(of, "FILE %s %zu\n", sk_names[k], n) < 0) die("write failed");
      if (fwrite(buf + ((size_t)i * SK_FILES + (size_t)k) * SK_CAP, 1, n, of) != n) {
        die("write failed");
      }
    }
  }
  if (fprintf(of, "SAMPLER DONE %ld %ld\n", count, overflows) < 0) die("write failed");
  if (fclose(of) != 0) die("--out close/flush failed");
  unlink(pidPath);
  return 0;
}
