#ifndef MLFK_GFX_ATTRIB_H
#define MLFK_GFX_ATTRIB_H
// port/gfx/attrib.h — the `--attrib` skip-attribution row sampler,
// SHARED by every paced app that can produce a stall worth attributing.
//
// Lifted VERBATIM out of gfx_app.c (M4 task 8) in M4 task 14 increment
// 3a so that port/foh/foh_dev.c — the binary that runs the 12 full-game
// legs, i.e. where the stalls the gate actually fails on occur — emits
// the SAME rows from the SAME code. Two producers of one pinned format
// is exactly the drift hazard a shared header removes (CLAUDE.md HARD
// RULE 8: class fix over a second copy).
//
// ROW GRAMMAR (pinned; the consumer is the UNCHANGED
// port/sim/device/skip-attrib/correlate-skips.js, whose AROW regex is
// /^([0-9]+) ([0-9]+) ([0-9]+) ([0-9]+) ([0-9]+) ([0-9]+)$/):
//
//     <mono_ns> <raw_ns> <nvcsw> <nivcsw> <minflt> <majflt>
//
// exactly frames+1 lines — one sampled at the START of every frame plus
// one tail row after the loop. mono/raw are strictly increasing; the
// four rusage counters are cumulative and non-decreasing. The
// correlator DIES on any violation, so a producer that drifts is caught
// mechanically rather than silently mis-attributing.
//
// CALLER CONTRACT (why the cost is structurally irrelevant): the sample
// must be taken OUTSIDE the app's own sim/render/present timing
// brackets — as the first statement of the frame loop body, and once
// after the loop. Then the instrument cannot inflate any number the
// pinned timing judge computes; it only consumes pacing slack (~2
// clock_gettime + 1 getrusage against a 16.667 ms budget).
//
// now_raw_ns/attrib_sample die loud on failure — a silently-zero row
// would read as a plausible measurement (fail-loud beats fail-plausible).

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h> // sysconf(_SC_PAGESIZE) for the pre-fault

#include "../sim/sim/sim.h" // sim_fatal

typedef struct {
  uint64_t mono, raw;
  uint64_t nvcsw, nivcsw, minflt, majflt;
} AttribRow;

// CLOCK_MONOTONIC — the same clock every app's own now_ns() reads, so
// attrib rows and the app's frame timestamps share one timebase.
static inline uint64_t attrib_mono_ns(void) {
  struct timespec ts;
  if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
    sim_fatal("clock_gettime(CLOCK_MONOTONIC) failed");
  }
  return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

// CLOCK_MONOTONIC_RAW — NTP-unslewed. Its drift against CLOCK_MONOTONIC
// is what lets the correlator rule out clock adjustment as a "stall".
static inline uint64_t now_raw_ns(void) {
  struct timespec ts;
  if (clock_gettime(CLOCK_MONOTONIC_RAW, &ts) != 0) {
    sim_fatal("clock_gettime(CLOCK_MONOTONIC_RAW) failed");
  }
  return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

// RUSAGE_SELF sums ALL threads of the process (audio callback thread and
// the music reader thread included). That is deliberate: the question
// the counters answer is "did anything else run / fault during this
// frame", and a sibling thread of ours stealing the single core is one
// of the pre-registered hypotheses, not noise to be filtered out.
static inline void attrib_sample(AttribRow *row) {
  struct rusage ru;
  row->mono = attrib_mono_ns();
  row->raw = now_raw_ns();
  if (getrusage(RUSAGE_SELF, &ru) != 0) {
    sim_fatal("getrusage(RUSAGE_SELF) failed");
  }
  row->nvcsw = (uint64_t)ru.ru_nvcsw;
  row->nivcsw = (uint64_t)ru.ru_nivcsw;
  row->minflt = (uint64_t)ru.ru_minflt;
  row->majflt = (uint64_t)ru.ru_majflt;
}

// attrib_alloc — allocate AND PRE-FAULT the frames+1 row buffer.
// The pre-fault is not tidiness (review-110-1 finding 2): a freshly
// malloc'd ~170 KB buffer is first TOUCHED one page at a time from
// inside the paced frame loop, so ~40 minor faults would land in the
// per-frame `minflt` column — the instrument would be measuring itself,
// in exactly the counter used to rule paging in or out.
//
// The touch is a VOLATILE store per page, not a memset (review-110-3
// finding 1): every field is overwritten before it is ever read, so a
// plain memset is legal for the compiler to drop, and gcc in particular
// folds malloc+memset into calloc — which may hand back lazily-zeroed
// pages and pre-fault nothing at all. A volatile store cannot be
// elided, so the faults are guaranteed to land here rather than in the
// paced loop.
//
// Returns 0 on OOM or an out-of-domain count (review-110-3 finding 2:
// this is a shared header, so it defends its own contract instead of
// trusting each caller's separate frame cap); the caller owns the
// fail-loud.
static inline AttribRow *attrib_alloc(long frames) {
  if (frames < 0) return 0;
  const size_t n = (size_t)frames + 1;
  if (n > (size_t)-1 / sizeof(AttribRow)) return 0;
  const size_t bytes = n * sizeof(AttribRow);
  AttribRow *rows = (AttribRow *)malloc(bytes);
  if (!rows) return 0;
  const long pg = sysconf(_SC_PAGESIZE);
  const size_t step = pg > 0 ? (size_t)pg : 4096u;
  volatile unsigned char *p = (volatile unsigned char *)rows;
  for (size_t off = 0; off < bytes; off += step) p[off] = 0;
  p[bytes - 1] = 0; // the final partial page
  return rows;
}

// The ONE writer of the pinned row grammar (both producers call this, so
// a format change is impossible to make in only one of them).
// Returns 0 on success; the caller owns the fail-loud.
static inline int attrib_flush(const char *path, const AttribRow *rows,
                               long frames) {
  FILE *af = fopen(path, "w");
  if (!af) return -1;
  for (long f = 0; f <= frames; f++) {
    if (fprintf(af,
                "%" PRIu64 " %" PRIu64 " %" PRIu64 " %" PRIu64 " %" PRIu64
                " %" PRIu64 "\n",
                rows[f].mono, rows[f].raw, rows[f].nvcsw, rows[f].nivcsw,
                rows[f].minflt, rows[f].majflt) < 0) {
      fclose(af);
      return -2;
    }
  }
  return fclose(af) != 0 ? -3 : 0;
}

#endif // MLFK_GFX_ATTRIB_H
