// sim_main.c — headless entry point for the integrated sim (M2 task 17).
//
// Replays ONE golden trace end-to-end and emits the CHECKSUM.md stream:
//   sim_host --trace <trace.txt> --simdata <simdata.txt> --seed <u32>
//            --p1 <char> --p2 <char> --stage <id> --frames <n>
//            [--cpu --difficulty <n> --ai-bridge <file>]
// stdout: "F <frame> <sha256hex>" per frame (1..n), then
//         "RNG <rngCalls> <rngCallsOutsideStep>", then "SIM OK".
// (wrap-run.js turns this into a verify-stream.js-compatible run JSON;
// the UNCHANGED oracle verifier judges it — writer != checker.)
//
// Boot RNG parity (CHECKSUM.md §6 + the qjs boot pin, CLAUDE.md M0 task
// 6): the page burns exactly 465 seeded draws before match setup (menu
// plane; mulberry32 is never re-seeded), the harness resets the counters
// (run.js:150), and startGame consumes ONE off-step draw. Draw counts are
// recovered from the mulberry32 STATE DELTA: state advances by the odd
// additive constant 0x6D2B79F5 per draw, so
// draws = (state_end - state_start) * inverse(0x6D2B79F5) mod 2^32 —
// exact, with no wrapper around the hot ml_rng_next path.
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sim.h"
#include "../ml_events.h"

#define ML_BOOT_DRAWS 465 // the qjs boot pin (oracle/qjs/replay.sh)

// --- trace loading (trace-to-txt.js format) -------------------------------------

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

// token order (trace-to-txt.js): a b x y z r l s du dr dd dl then
// lsX lsY csX csY lA rA rawX rawY rawcsX rawcsY
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

// --- draw counting ----------------------------------------------------------------

static uint32_t mulberry_inv(void) {
  // Newton iteration for the inverse of the odd constant mod 2^32
  const uint32_t k = 0x6D2B79F5u;
  uint32_t x = k; // 3 bits correct
  for (int i = 0; i < 5; i++) x *= 2u - k * x;
  return x;
}

static uint32_t draws_between(uint32_t from, uint32_t to) {
  return (to - from) * mulberry_inv();
}

// --- main --------------------------------------------------------------------------

int main(int argc, char **argv) {
  const char *tracePath = 0, *simdataPath = 0, *bridgePath = 0;
  const char *dumpFrames = 0; // diagnostic: comma-separated frame list
  long seed = -1, p1 = -1, p2 = -1, stage = -1, frames = -1, difficulty = 3;
  bool cpu = false;
  for (int i = 1; i < argc; i++) {
    const char *a = argv[i];
    const bool hasV = i + 1 < argc;
    if (strcmp(a, "--trace") == 0 && hasV) tracePath = argv[++i];
    else if (strcmp(a, "--simdata") == 0 && hasV) simdataPath = argv[++i];
    else if (strcmp(a, "--ai-bridge") == 0 && hasV) bridgePath = argv[++i];
    else if (strcmp(a, "--seed") == 0 && hasV) seed = strtol(argv[++i], 0, 10);
    else if (strcmp(a, "--p1") == 0 && hasV) p1 = strtol(argv[++i], 0, 10);
    else if (strcmp(a, "--p2") == 0 && hasV) p2 = strtol(argv[++i], 0, 10);
    else if (strcmp(a, "--stage") == 0 && hasV) stage = strtol(argv[++i], 0, 10);
    else if (strcmp(a, "--frames") == 0 && hasV) frames = strtol(argv[++i], 0, 10);
    else if (strcmp(a, "--difficulty") == 0 && hasV) difficulty = strtol(argv[++i], 0, 10);
    else if (strcmp(a, "--cpu") == 0) cpu = true;
    else if (strcmp(a, "--dump-frames") == 0 && hasV) dumpFrames = argv[++i];
    else {
      fprintf(stderr, "sim_host: bad argument %s\n", a);
      return 1;
    }
  }
  if (!tracePath || !simdataPath || seed < 0 || p1 < 0 || p2 < 0 ||
      stage < 0 || frames <= 0 || (cpu && !bridgePath)) {
    fprintf(stderr,
            "usage: sim_host --trace t.txt --simdata s.txt --seed N --p1 N "
            "--p2 N --stage N --frames N [--cpu --difficulty N "
            "--ai-bridge f]\n");
    return 1;
  }

  sim_boot_page(&G);
  sim_data_load(simdataPath);
  sim_data_register();
  load_trace(tracePath);

  // seeded mulberry32 + the 465 boot draws (uncounted: counters reset
  // right before setupMatch, run.js:150)
  ml_active_rng = &G.rng;
  ml_rng_seed(&G.rng, (uint32_t)seed);
  for (int k = 0; k < ML_BOOT_DRAWS; k++) (void)ml_rng_next(&G.rng);
  G.rngStateAtReset = G.rng.a;

  if (cpu) {
    if (ml_ai_bridge_load(&G.bridge, bridgePath) != 0) {
      sim_fatal("AI bridge artifact failed to load");
    }
    if (G.bridge.seed != (uint32_t)seed || G.bridge.boot != ML_BOOT_DRAWS) {
      sim_fatal("AI bridge header (seed/boot) disagrees with the run");
    }
    G.hasBridge = true;
  }

  sim_setup_match(&G, (int)p1, (int)p2, cpu ? 1 : 0, (int)difficulty,
                  (int)stage);
  G.rngStateAtFrame1 = G.rng.a;

  char hex[65];
  for (long f = 0; f < frames; f++) {
    // pagelib.js:93-96 — held-last past trace end
    const long idx = f < g_trace_len - 1 ? f : g_trace_len - 1;
    const TraceRow *row = &g_trace[idx];
    const MlInput *rows[4];
    for (int i = 0; i < 4; i++) rows[i] = row->present[i] ? &row->in[i] : 0;
    G.frame = f + 1;
    sim_game_tick(&G, rows);
    sim_frame_hash(&G, hex);
    printf("F %ld %s\n", f + 1, hex);
    if (dumpFrames) {
      // diagnostic envelope dump (stderr — stdout is wrap-run's contract)
      char tok[24];
      snprintf(tok, sizeof tok, ",%ld,", f + 1);
      char wrapped[24 + 4];
      snprintf(wrapped, sizeof wrapped, ",%s,", dumpFrames);
      if (strstr(wrapped, tok)) {
        size_t elen = 0;
        const char *env = sim_frame_envelope(&elen);
        fprintf(stderr, "E %ld\t%s\n", f + 1, env);
        (void)elen;
      }
    }
  }

  if (G.hasBridge && ml_ai_bridge_peek(&G.bridge) != 0) {
    sim_fatal("AI bridge has unconsumed entries after the last frame");
  }

  const uint32_t total = draws_between(G.rngStateAtReset, G.rng.a);
  const uint32_t outside = draws_between(G.rngStateAtReset, G.rngStateAtFrame1);
  printf("RNG %" PRIu32 " %" PRIu32 "\n", total, outside);
  printf("SIM OK\n");
  return 0;
}
