// target_main.c — headless entry point for the TARGET-TEST sim
// (fix_plan §M4 task 11; the sim_main.c twin for gameMode 5).
//
// Replays ONE target golden trace end-to-end and emits BOTH streams:
//   sim_host_target --trace <trace.txt> --simdata <simdata.txt>
//                   --seed <u32> --char <0-4> --tstage <0-9> --frames <n>
//                   [--dump-frames a,b]
// stdout: per frame (1..n) "F <frame> <sha256hex>"  — the UNCHANGED
//         CHECKSUM.md spec-v1 player/article envelope (sim_ser.c), then
//         "T <frame> <sha256hex>"  — the target-plane envelope
//         (target_play.h tp_target_frame_hash; iter-63 separate-stream
//         convention), then "RNG <rngCalls> <rngCallsOutsideStep>", then
//         "SIM OK".
// (port/goldens-m4/wrap-target.js splits this into a verify-stream.js
// run JSON + a target-plane run JSON; the UNCHANGED oracle verifier
// judges the player stream, verify-target-stream.js the target plane —
// writer != checker.)
//
// Boot RNG parity: identical to sim_main.c — 465 boot draws, counters
// reset before setup, tp_setup_target consumes the ONE off-step draw
// (startTargetGame's background draw, targetplay.js:187 — the startGame
// twin, so rngCallsOutsideStep == 1 exactly like VS goldens).
//
// The trace loader + draw counting are VERBATIM LIFTS from sim_main.c
// (:52-173) — driver plumbing, not sim logic; sim_main.c is excluded
// from this build (two mains), so the lift is the smallest change that
// leaves the frozen TU untouched.
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../ml_events.h"
#include "target_play.h"

#define ML_BOOT_DRAWS 465 // the qjs boot pin (oracle/qjs/replay.sh)

// --- trace loading (trace-to-txt.js format; LIFTED from sim_main.c:52-159) ------

typedef struct {
  bool present[4];
  MlInput in[4];
} TraceRow;

static TraceRow *g_trace;
static long g_trace_len;

static uint64_t parse_hex16(const char *s) {
  uint64_t v = 0;
  for (int k = 0; k < 16; k++) {
    const char c = s[k];
    v <<= 4;
    if (c >= '0' && c <= '9') v |= (uint64_t)(c - '0');
    else if (c >= 'a' && c <= 'f') v |= (uint64_t)(c - 'a' + 10);
    else sim_fatal("trace: bad hex16 token");
  }
  return v;
}

static const char *tok_next(const char *s, bool *bout, double *dout,
                            bool isNum) {
  while (*s == ' ') s++;
  if (*s == 0 || *s == '|') sim_fatal("trace: short slot token list");
  if (isNum) {
    const uint64_t bits = parse_hex16(s);
    double d;
    memcpy(&d, &bits, 8);
    *dout = d;
    return s + 16;
  }
  if (*s != '0' && *s != '1') sim_fatal("trace: bad bool token");
  *bout = *s == '1';
  return s + 1;
}

static const char *parse_slot(const char *s, MlInput *in) {
  *in = nullInput();
  double d = 0;
  bool b = false;
  s = tok_next(s, &in->a, &d, false);
  s = tok_next(s, &in->b, &d, false);
  s = tok_next(s, &in->x, &d, false);
  s = tok_next(s, &in->y, &d, false);
  s = tok_next(s, &in->z, &d, false);
  s = tok_next(s, &in->r, &d, false);
  s = tok_next(s, &in->l, &d, false);
  s = tok_next(s, &in->s, &d, false);
  s = tok_next(s, &in->du, &d, false);
  s = tok_next(s, &in->dr, &d, false);
  s = tok_next(s, &in->dd, &d, false);
  s = tok_next(s, &in->dl, &d, false);
  s = tok_next(s, &b, &in->lsX, true);
  s = tok_next(s, &b, &in->lsY, true);
  s = tok_next(s, &b, &in->csX, true);
  s = tok_next(s, &b, &in->csY, true);
  s = tok_next(s, &b, &in->lA, true);
  s = tok_next(s, &b, &in->rA, true);
  s = tok_next(s, &b, &in->rawX, true);
  s = tok_next(s, &b, &in->rawY, true);
  s = tok_next(s, &b, &in->rawcsX, true);
  s = tok_next(s, &b, &in->rawcsY, true);
  while (*s == ' ') s++;
  return s;
}

static void load_trace(const char *path) {
  FILE *f = fopen(path, "r");
  if (!f) sim_fatal("cannot open trace");
  size_t cap = 4096;
  g_trace = malloc(cap * sizeof *g_trace);
  if (!g_trace) sim_fatal("oom");
  char *line = NULL;
  size_t lcap = 0;
  ssize_t n;
  g_trace_len = 0;
  while ((n = getline(&line, &lcap, f)) > 0) {
    if (line[n - 1] == '\n') line[--n] = 0;
    if (n == 0) continue;
    if ((size_t)g_trace_len == cap) {
      cap *= 2;
      g_trace = realloc(g_trace, cap * sizeof *g_trace);
      if (!g_trace) sim_fatal("oom");
    }
    TraceRow *row = &g_trace[g_trace_len++];
    const char *s = line;
    for (int i = 0; i < 4; i++) {
      if (*s == '|' || *s == 0) {
        row->present[i] = false;
        row->in[i] = nullInput();
      } else {
        row->present[i] = true;
        s = parse_slot(s, &row->in[i]);
      }
      if (i < 3) {
        if (*s != '|') sim_fatal("trace: expected slot separator");
        s++;
      } else if (*s != 0) {
        sim_fatal("trace: trailing bytes on a frame line");
      }
    }
  }
  free(line);
  fclose(f);
  if (g_trace_len == 0) sim_fatal("trace: empty");
}

// --- draw counting (LIFTED from sim_main.c:163-173) --------------------------------

static uint32_t mulberry_inv(void) {
  const uint32_t k = 0x6D2B79F5u;
  uint32_t x = k;
  for (int i = 0; i < 5; i++) x *= 2u - k * x;
  return x;
}

static uint32_t draws_between(uint32_t from, uint32_t to) {
  return (to - from) * mulberry_inv();
}

// --- main --------------------------------------------------------------------------

int main(int argc, char **argv) {
  const char *tracePath = 0, *simdataPath = 0;
  const char *dumpFrames = 0;
  bool posDump = false; // diagnostic: per-frame "P f x y state" (stderr)
  long seed = -1, charId = -1, tstage = -1, frames = -1;
  for (int i = 1; i < argc; i++) {
    const char *a = argv[i];
    const bool hasV = i + 1 < argc;
    if (strcmp(a, "--trace") == 0 && hasV) tracePath = argv[++i];
    else if (strcmp(a, "--simdata") == 0 && hasV) simdataPath = argv[++i];
    else if (strcmp(a, "--seed") == 0 && hasV) seed = strtol(argv[++i], 0, 10);
    else if (strcmp(a, "--char") == 0 && hasV) charId = strtol(argv[++i], 0, 10);
    else if (strcmp(a, "--tstage") == 0 && hasV) tstage = strtol(argv[++i], 0, 10);
    else if (strcmp(a, "--frames") == 0 && hasV) frames = strtol(argv[++i], 0, 10);
    else if (strcmp(a, "--dump-frames") == 0 && hasV) dumpFrames = argv[++i];
    else if (strcmp(a, "--pos-dump") == 0) posDump = true;
    else {
      fprintf(stderr, "sim_host_target: bad argument %s\n", a);
      return 1;
    }
  }
  if (!tracePath || !simdataPath || seed < 0 || charId < 0 || charId > 4 ||
      tstage < 0 || tstage > 9 || frames <= 0) {
    fprintf(stderr,
            "usage: sim_host_target --trace t.txt --simdata s.txt --seed N "
            "--char 0-4 --tstage 0-9 --frames N [--dump-frames a,b]\n");
    return 1;
  }

  sim_boot_page(&G);
  sim_data_load(simdataPath);
  sim_data_register();
  load_trace(tracePath);

  ml_active_rng = &G.rng;
  ml_rng_seed(&G.rng, (uint32_t)seed);
  for (int k = 0; k < ML_BOOT_DRAWS; k++) (void)ml_rng_next(&G.rng);
  G.rngStateAtReset = G.rng.a;

  tp_setup_target(&G, (int)charId, (int)tstage);
  G.rngStateAtFrame1 = G.rng.a;

  char hex[65], thex[65];
  for (long f = 0; f < frames; f++) {
    // pagelib.js:93-96 — held-last past trace end
    const long idx = f < g_trace_len - 1 ? f : g_trace_len - 1;
    const TraceRow *row = &g_trace[idx];
    // slots 1-3 must be null in a target trace (single-player mode)
    if (row->present[1] || row->present[2] || row->present[3]) {
      sim_fatal("target trace with a non-null slot 1-3 row");
    }
    G.frame = f + 1;
    tp_game_tick_target(&G, row->present[0] ? &row->in[0] : 0);
    sim_frame_hash(&G, hex);
    tp_target_frame_hash(&G, thex);
    printf("F %ld %s\n", f + 1, hex);
    printf("T %ld %s\n", f + 1, thex);
    if (posDump) {
      // trace-design diagnostic ONLY (stderr; never on the judged stream)
      fprintf(stderr, "P %ld %.6f %.6f %s d=%d\n", f + 1,
              G.sim.player[0].phys.pos.x, G.sim.player[0].phys.pos.y,
              G.sim.player[0].actionState, (int)TP.targetsDestroyed);
    }
    if (dumpFrames) {
      char tok[24];
      snprintf(tok, sizeof tok, ",%ld,", f + 1);
      char wrapped[24 + 4];
      snprintf(wrapped, sizeof wrapped, ",%s,", dumpFrames);
      if (strstr(wrapped, tok)) {
        size_t elen = 0;
        const char *env = sim_frame_envelope(&elen);
        fprintf(stderr, "E %ld\t%s\n", f + 1, env);
        (void)elen;
        // target-plane diagnostic: destroyed flags + count + timer bits
        uint64_t mtBits;
        memcpy(&mtBits, &G.matchTimer, 8);
        fprintf(stderr, "ET %ld\t%d/%d matchTimer=%016" PRIx64 "\n", f + 1,
                (int)TP.targetsDestroyed, TP.targetCount, mtBits);
      }
    }
  }

  const uint32_t total = draws_between(G.rngStateAtReset, G.rng.a);
  const uint32_t outside = draws_between(G.rngStateAtReset, G.rngStateAtFrame1);
  printf("RNG %" PRIu32 " %" PRIu32 "\n", total, outside);
  // TFIN: the target-plane finals (targetsDestroyed + endTargetGame) for
  // wrap-target.js -> verify-target-stream.js's finals pin. targetsDestroyed
  // is an integer count in the golden domain (never fractional here).
  printf("TFIN %d %s\n", (int)TP.targetsDestroyed, TP.endTargetGame ? "T" : "F");
  printf("SIM OK\n");
  return 0;
}
