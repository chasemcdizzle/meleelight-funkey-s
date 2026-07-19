// port/gfx/gfx_replay.c — render-on golden replay (M3 task 3).
//
// A sim_host-DERIVED replay driver: the exact sim_main.c loop (trace
// parsing, boot-draw burn, tick + frame hash, "F <n> <hash>" stream +
// RNG/SIM OK trailer — duplicated with citation; port/sim/sim/sim_main.c
// itself is under concurrent review and untouched) PLUS the port/gfx
// renderer invoked on EVERY frame after the tick. The stdout stream is
// wrap-run.js/verify-stream.js-compatible, so the UNCHANGED oracle
// verifier proves the renderer did not perturb the sim (the renderer
// also only ever sees `const GameState *`).
//
//   gfx_replay --trace t.txt --simdata s.txt --gfxdata g.txt
//              --anim-dir DIR --seed N --p1 N --p2 N --stage N --frames N
//              [--cpu --difficulty N --ai-bridge f]
//              [--render-frames 30,100,...] [--render-out DIR]
//
// Renders every frame; writes fNNNN.ppm + fNNNN.pgm (ink mask) only for
// the frames listed in --render-frames. Render-only wall-clock stats go
// to stderr (informational; the render is NOT part of any bit-exact
// surface).
//
// backgroundType: upstream startGame runs
// setBackgroundType(Math.round(Math.random())) with the ONE off-step
// seeded draw (CHECKSUM.md §6). sim_setup_match burns that draw for the
// stream; the VALUE (which picks boxFill via drawBackgroundInit) is
// recovered here by drawing from a COPY of the RNG state just before
// setup — the live stream is untouched.
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "../sim/sim/sim.h"
#include "../sim/ml_events.h"
#include "../sim/ml_js.h"
#include "gfx.h"
#include "gfx_vfx.h"

#define ML_BOOT_DRAWS 465 // the qjs boot pin (oracle/qjs/replay.sh)

void gfx_fatal(const char *what) { sim_fatal(what); }
// (sim_fatal is noreturn; the attribute on the gfx_fatal decls is satisfied)

// --- trace loading (sim_main.c:39-148, duplicated verbatim) --------------------

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

// --- draw counting (sim_main.c:150-162) -------------------------------------------

static uint32_t mulberry_inv(void) {
  const uint32_t k = 0x6D2B79F5u;
  uint32_t x = k;
  for (int i = 0; i < 5; i++) x *= 2u - k * x;
  return x;
}

static uint32_t draws_between(uint32_t from, uint32_t to) {
  return (to - from) * mulberry_inv();
}

// --- render-frame selection ---------------------------------------------------------

#define MAX_DUMP 64
static long g_dump[MAX_DUMP];
static int g_ndump;

static void parse_render_frames(const char *s) {
  g_ndump = 0;
  while (*s) {
    char *end;
    const long v = strtol(s, &end, 10);
    if (end == s || v <= 0) sim_fatal("--render-frames: bad frame number");
    if (g_ndump >= MAX_DUMP) sim_fatal("--render-frames: too many frames");
    g_dump[g_ndump++] = v;
    s = end;
    if (*s == ',') s++;
    else if (*s != 0) sim_fatal("--render-frames: bad separator");
  }
}

static int is_dump_frame(long f) {
  for (int k = 0; k < g_ndump; k++) {
    if (g_dump[k] == f) return 1;
  }
  return 0;
}

// --- timing ---------------------------------------------------------------------------

static uint64_t now_ns(void) {
  struct timespec ts;
  if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
    sim_fatal("clock_gettime(CLOCK_MONOTONIC) failed");
  }
  return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

static int cmp_u64(const void *a, const void *b) {
  const uint64_t x = *(const uint64_t *)a, y = *(const uint64_t *)b;
  return (x > y) - (x < y);
}

// --- main -----------------------------------------------------------------------------

static Gfx g_gfx; // big (framebuffer + anim tables); static, not stack

int main(int argc, char **argv) {
  const char *tracePath = 0, *simdataPath = 0, *bridgePath = 0;
  const char *gfxdataPath = 0, *animDir = 0, *renderOut = 0;
  const char *vfxdataPath = 0, *glyphsPath = 0, *injectPath = 0;
  long seed = -1, p1 = -1, p2 = -1, stage = -1, frames = -1, difficulty = 3;
  bool cpu = false;
  for (int i = 1; i < argc; i++) {
    const char *a = argv[i];
    const bool hasV = i + 1 < argc;
    if (strcmp(a, "--trace") == 0 && hasV) tracePath = argv[++i];
    else if (strcmp(a, "--simdata") == 0 && hasV) simdataPath = argv[++i];
    else if (strcmp(a, "--gfxdata") == 0 && hasV) gfxdataPath = argv[++i];
    else if (strcmp(a, "--vfxdata") == 0 && hasV) vfxdataPath = argv[++i];
    else if (strcmp(a, "--glyphs") == 0 && hasV) glyphsPath = argv[++i];
    else if (strcmp(a, "--inject") == 0 && hasV) injectPath = argv[++i];
    else if (strcmp(a, "--anim-dir") == 0 && hasV) animDir = argv[++i];
    else if (strcmp(a, "--render-out") == 0 && hasV) renderOut = argv[++i];
    else if (strcmp(a, "--render-frames") == 0 && hasV) parse_render_frames(argv[++i]);
    else if (strcmp(a, "--ai-bridge") == 0 && hasV) bridgePath = argv[++i];
    else if (strcmp(a, "--seed") == 0 && hasV) seed = strtol(argv[++i], 0, 10);
    else if (strcmp(a, "--p1") == 0 && hasV) p1 = strtol(argv[++i], 0, 10);
    else if (strcmp(a, "--p2") == 0 && hasV) p2 = strtol(argv[++i], 0, 10);
    else if (strcmp(a, "--stage") == 0 && hasV) stage = strtol(argv[++i], 0, 10);
    else if (strcmp(a, "--frames") == 0 && hasV) frames = strtol(argv[++i], 0, 10);
    else if (strcmp(a, "--difficulty") == 0 && hasV) difficulty = strtol(argv[++i], 0, 10);
    else if (strcmp(a, "--cpu") == 0) cpu = true;
    else {
      fprintf(stderr, "gfx_replay: bad argument %s\n", a);
      return 1;
    }
  }
  if (!tracePath || !simdataPath || !gfxdataPath || !animDir ||
      !vfxdataPath || !glyphsPath || seed < 0 ||
      p1 < 0 || p2 < 0 || stage < 0 || frames <= 0 || (cpu && !bridgePath) ||
      (g_ndump > 0 && !renderOut)) {
    fprintf(stderr,
            "usage: gfx_replay --trace t.txt --simdata s.txt --gfxdata g.txt "
            "--vfxdata v.txt --glyphs gl.txt "
            "--anim-dir D --seed N --p1 N --p2 N --stage N --frames N "
            "[--cpu --difficulty N --ai-bridge f] [--inject i.txt] "
            "[--render-frames a,b --render-out D]\n");
    return 1;
  }

  sim_boot_page(&G);
  sim_data_load(simdataPath);
  sim_data_register();
  load_trace(tracePath);

  gfx_data_load(&g_gfx.data, gfxdataPath);
  gfx_load_anim(&g_gfx, animDir, (int)p1);
  gfx_load_anim(&g_gfx, animDir, (int)p2);
  gfx_vfx_load(vfxdataPath);
  gfx_glyphs_load(glyphsPath);
  if (injectPath) gfx_vfx_inject_load(injectPath);

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

  // Peek startGame's background draw from a COPY of the RNG state (the
  // live stream is untouched; sim_setup_match burns the real draw).
  MlRng peek = G.rng;
  const int backgroundType = (int)js_round(ml_rng_next(&peek));

  // vfx sink BEFORE sim_setup_match: the boot entrance/start events fire
  // inside it (main.js initializePlayers / startGame drawVfx sites), and
  // upstream's vfxQueue carries them into frame 1's render.
  gfx_init(&g_gfx, (int)stage, backgroundType);
  gfx_vfx_install(&g_gfx);

  sim_setup_match(&G, (int)p1, (int)p2, cpu ? 1 : 0, (int)difficulty,
                  (int)stage);
  G.rngStateAtFrame1 = G.rng.a;

  uint64_t *rns = malloc((size_t)frames * sizeof *rns);
  if (!rns) sim_fatal("oom (render timing buffer)");

  char hex[65];
  for (long f = 0; f < frames; f++) {
    const long idx = f < g_trace_len - 1 ? f : g_trace_len - 1;
    const TraceRow *row = &g_trace[idx];
    const MlInput *rows[4];
    for (int i = 0; i < 4; i++) rows[i] = row->present[i] ? &row->in[i] : 0;
    G.frame = f + 1;
    sim_game_tick(&G, rows);
    sim_frame_hash(&G, hex);

    gfx_vfx_inject_fire(f + 1); // synthetic coverage (post-tick, pre-render;
                                // the browser capture injects at the same point)
    const uint64_t t0 = now_ns();
    gfx_render_frame(&g_gfx, &G); // every frame (perturbation exposure + timing)
    rns[f] = now_ns() - t0;

    if (g_ndump && is_dump_frame(f + 1)) {
      char path[1200];
      snprintf(path, sizeof path, "%s/f%04ld.ppm", renderOut, f + 1);
      gfx_dump_ppm(&g_gfx, path);
      snprintf(path, sizeof path, "%s/f%04ld.pgm", renderOut, f + 1);
      gfx_dump_ink_pgm(&g_gfx, path);
    }
    printf("F %ld %s\n", f + 1, hex);
  }

  if (G.hasBridge && ml_ai_bridge_peek(&G.bridge) != 0) {
    sim_fatal("AI bridge has unconsumed entries after the last frame");
  }

  // render-only wall stats (informational)
  qsort(rns, (size_t)frames, sizeof *rns, cmp_u64);
  uint64_t sum = 0;
  for (long f = 0; f < frames; f++) sum += rns[f];
  fprintf(stderr,
          "render-only ns: avg=%" PRIu64 " p50=%" PRIu64 " p99=%" PRIu64
          " max=%" PRIu64 " (n=%ld, host)\n",
          sum / (uint64_t)frames, rns[frames / 2],
          rns[(long)((double)frames * 0.99)], rns[frames - 1], frames);
  free(rns);

  const uint32_t total = draws_between(G.rngStateAtReset, G.rng.a);
  const uint32_t outside = draws_between(G.rngStateAtReset, G.rngStateAtFrame1);
  printf("RNG %" PRIu32 " %" PRIu32 "\n", total, outside);
  printf("SIM OK\n");
  return 0;
}
