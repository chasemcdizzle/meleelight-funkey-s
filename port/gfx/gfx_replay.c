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

// The tunnel leg keeps its own sample list (its frame numbers index a
// standalone deterministic animation, not the match).
static long g_tdump[MAX_DUMP];
static int g_ntdump;

static long parse_tunnel_frames(const char *s) {
  long last = 0;
  g_ntdump = 0;
  while (*s) {
    char *end;
    const long v = strtol(s, &end, 10);
    if (end == s || v <= last) sim_fatal("--bg-tunnel-frames: not strictly ascending positives");
    if (g_ntdump >= MAX_DUMP) sim_fatal("--bg-tunnel-frames: too many frames");
    g_tdump[g_ntdump++] = v;
    last = v;
    s = end;
    if (*s == ',') s++;
    else if (*s != 0) sim_fatal("--bg-tunnel-frames: bad separator");
  }
  if (g_ntdump == 0) sim_fatal("--bg-tunnel-frames: empty list");
  return last; // run length = the highest sampled frame
}

static int is_tunnel_dump_frame(long f) {
  for (int k = 0; k < g_ntdump; k++) {
    if (g_tdump[k] == f) return 1;
  }
  return 0;
}

// --- U1 background-parity dumps -------------------------------------------------------
//
// The BG2 (drawStars/drawTunnel) plane is inked and handed to bg_sink by
// gfx_bg.c once per frame; we write it out on the sampled frames only,
// as fNNNN.bg.pgm — same P5 240x240 grammar as the fg .pgm, so iou.js
// judges it with the same downscale/band machinery.
static const char *g_bgOut;   // NULL = sink not armed
static long g_bgFrame;        // frame number the sink is currently serving
static int g_bgTunnelLeg;     // 1 while the standalone tunnel leg runs

static void star_sink(Gfx *g) {
  if (!g_bgOut || g_bgTunnelLeg) return; // stars exist only on type 0
  if (!is_dump_frame(g_bgFrame)) return;
  char path[1200];
  snprintf(path, sizeof path, "%s/f%04ld.star.pgm", g_bgOut, g_bgFrame);
  gfx_dump_ink_pgm(g, path);
}

static void bg_sink(Gfx *g) {
  if (!g_bgOut) return;
  if (!(g_bgTunnelLeg ? is_tunnel_dump_frame(g_bgFrame) : is_dump_frame(g_bgFrame))) return;
  char path[1200];
  snprintf(path, sizeof path, "%s/f%04ld.bg.pgm", g_bgOut, g_bgFrame);
  gfx_dump_ink_pgm(g, path);
}

// U3 ARTICLE-ONLY plane — the foreground twin of star_sink. gfx_render.c
// fires this with the ink plane holding exactly the renderArticles pass,
// written out as fNNNN.art.pgm in the same P5 240x240 grammar as the
// other planes, so iou.js judges it with the same downscale/band
// machinery and no geometric argument about where a laser may appear.
static const char *g_artOut; // NULL = sink not armed
static void art_sink(Gfx *g) {
  if (!g_artOut || !is_dump_frame(g_bgFrame)) return;
  char path[1200];
  snprintf(path, sizeof path, "%s/f%04ld.art.pgm", g_artOut, g_bgFrame);
  gfx_dump_ink_pgm(g, path);
  // review-134 indep-2 M1: also publish the SAVED pre-article plane, i.e.
  // the ink the isolation lifted out of the live plane and must put back.
  // Without it the C-side containment arm was a tautology (see
  // gfx_render_article_pre's note); with it, iou.js can assert that the
  // final fg plane still CONTAINS everything drawn before the article pass,
  // which is false the moment the OR-back is removed or truncated.
  const uint8_t *pre = gfx_render_article_pre();
  if (pre == NULL) sim_fatal("gfx_replay: article sink fired with no saved plane");
  snprintf(path, sizeof path, "%s/f%04ld.artpre.pgm", g_artOut, g_bgFrame);
  gfx_dump_plane_pgm(pre, path);
}

// The BG1 gradient, judged by COLOUR and OBSERVATIONALLY: gfx_bg.c fires
// this straight after the gradient rows are laid down, while the
// framebuffer holds the gradient and nothing else, so what we dump is
// the real shipped output — row loop, canvas->device mapping, clip
// range and RGB565 packing included (review-u1 r1 H2). One line per
// DEVICE row of the letterbox band; GFX_K is exactly 0.2, so device row
// y is canvas row (y - GFX_DY) * 5 with no resampling slack.
static const char *g_gradPath;

static void grad_sink(Gfx *g) {
  if (!g_gradPath) return;
  FILE *f = fopen(g_gradPath, "wb");
  if (!f) sim_fatal("gfx_replay: cannot open --bg-grad for writing");
  int lo = 0, hi = 0;
  const uint8_t (*obs)[3] = 0;
  gfx_bg_grad_observed(&lo, &hi, &obs);
  if (lo != g->rz.clipY0 || hi != g->rz.clipY1) {
    sim_fatal("gfx_replay: gradient row loop did not cover the clip range");
  }
  fprintf(f, "BGGRAD1\n");
  for (int y = g->rz.clipY0; y < g->rz.clipY1; y++) {
    const uint16_t p = g->rz.fb[(size_t)y * RAST_W + (RAST_W / 2)];
    // the row must be uniform: rast_fill_row_opaque covers [0,RAST_W),
    // so sampling one column is representative only if it really did
    for (int x = 0; x < RAST_W; x++) {
      if (g->rz.fb[(size_t)y * RAST_W + (size_t)x] != p) {
        sim_fatal("gfx_replay: BG1 gradient row is not uniform across x");
      }
    }
    const unsigned fr = (unsigned)(uint8_t)(((p >> 11) & 31) << 3);
    const unsigned fg = (unsigned)(uint8_t)(((p >> 5) & 63) << 2);
    const unsigned fb = (unsigned)(uint8_t)((p & 31) << 3);
    // CROSS-CHECK: the 8-bit colour the row loop reported must be exactly
    // what the framebuffer it wrote quantizes to. This is what keeps the
    // 8-bit plane an OBSERVATION of the real path rather than a second,
    // unverified opinion about it (review-u1 r1 H2 / r2 M1).
    if (((unsigned)(obs[y][0] >> 3) << 3) != fr ||
        ((unsigned)(obs[y][1] >> 2) << 2) != fg ||
        ((unsigned)(obs[y][2] >> 3) << 3) != fb) {
      sim_fatal("gfx_replay: observed gradient row disagrees with the framebuffer it wrote");
    }
    // "R <device row> <fb r> <fb g> <fb b> <obs r> <obs g> <obs b>"
    fprintf(f, "R %d %u %u %u %u %u %u\n", y, fr, fg, fb,
            (unsigned)obs[y][0], (unsigned)obs[y][1], (unsigned)obs[y][2]);
  }
  fprintf(f, "END\n");
  if (fclose(f) != 0) sim_fatal("gfx_replay: --bg-grad close failed");
  g_gradPath = 0; // first frame only; the gradient is static per match
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
  const char *bgGradPath = 0, *bgTunnelOut = 0;
  long bgTunnelFrames = 0;
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
    else if (strcmp(a, "--bg-out") == 0 && hasV) g_bgOut = argv[++i];
    else if (strcmp(a, "--bg-grad") == 0 && hasV) bgGradPath = argv[++i];
    else if (strcmp(a, "--bg-tunnel-out") == 0 && hasV) bgTunnelOut = argv[++i];
    else if (strcmp(a, "--bg-tunnel-frames") == 0 && hasV)
      bgTunnelFrames = parse_tunnel_frames(argv[++i]);
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
      (g_ndump > 0 && !renderOut) || (g_bgOut && g_ndump == 0) ||
      ((bgTunnelOut != 0) != (bgTunnelFrames > 0))) {
    fprintf(stderr,
            "usage: gfx_replay --trace t.txt --simdata s.txt --gfxdata g.txt "
            "--vfxdata v.txt --glyphs gl.txt "
            "--anim-dir D --seed N --p1 N --p2 N --stage N --frames N "
            "[--cpu --difficulty N --ai-bridge f] [--inject i.txt] "
            "[--render-frames a,b --render-out D] "
            "[--bg-out D] [--bg-grad f] [--bg-tunnel-out D --bg-tunnel-frames N]\n");
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
  // U1: arm the BG2 ink sink before the first render. Unarmed the
  // background pass is ink-suppressed exactly as before.
  if (g_bgOut) { gfx_bg_ink_sink(bg_sink); gfx_bg_star_sink(star_sink); }
  // U3: arm the article-only plane whenever fg dumps are being taken.
  if (renderOut && g_ndump) { g_artOut = renderOut; gfx_render_article_sink(art_sink); }
  if (bgGradPath) { g_gradPath = bgGradPath; gfx_bg_grad_sink(grad_sink); }

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
    g_bgFrame = f + 1; // which frame bg_sink is serving
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

  // review-u1 r2 M4: read this back OUT OF THE RENDERER after init, so it
  // is the value the render path will actually dispatch on, not the
  // local we intended to pass it.
  fprintf(stderr, "bg selected backgroundType %d\n", g_gfx.backgroundType);

  const uint32_t total = draws_between(G.rngStateAtReset, G.rng.a);
  const uint32_t outside = draws_between(G.rngStateAtReset, G.rngStateAtFrame1);
  printf("RNG %" PRIu32 " %" PRIu32 "\n", total, outside);

  // U1 tunnel leg. backgroundType is drawn from the seeded stream at
  // startGame (main.js:1322 setBackgroundType(Math.round(Math.random()))),
  // so which background a match gets is a per-SEED coin flip, not a stage
  // property. MEASURED over the committed goldens: g01/g02/g03/g05/g06/
  // g07/g08/m02 select type 0 and g04 (seed 7344) and m01 (seed 8114)
  // select type 1 — but this rig replays g01 only, so drawTunnel is
  // unreachable from the golden it runs. It consumes NO randomness (only
  // ang and circleSize advance), so a standalone leg pairs it with the
  // browser exactly from pristine module state. REGISTERED RESIDUAL:
  // because both sides are FORCED to type 1 here, this leg proves
  // drawTunnel's GEOMETRY, not the type-1 dispatch; the dispatch is
  // checked separately by the browser-vs-C backgroundType equality
  // assert on the live golden. Runs after the match so nothing above can
  // see it; the stream and every fg artifact are already written.
  if (bgTunnelOut) {
    g_gfx.backgroundType = 1;
    gfx_bg_reset();
    g_bgOut = bgTunnelOut;
    g_bgTunnelLeg = 1;
    gfx_bg_ink_sink(bg_sink);
    for (long f = 1; f <= bgTunnelFrames; f++) {
      g_bgFrame = f;
      rast_clear(&g_gfx.rz, 0, 0, 0, (int)GFX_DY, (int)(GFX_DY + 750.0 * GFX_K));
      gfx_render_background(&g_gfx);
    }
    // stderr: stdout is the checksum stream (wrap-run.js parses it, and
    // check-render.sh cmp's it a-vs-b) and must stay byte-unchanged.
    fprintf(stderr, "bg tunnel leg: %ld frames, %d sampled\n", bgTunnelFrames, g_ntdump);
  }

  printf("SIM OK\n");
  return 0;
}
