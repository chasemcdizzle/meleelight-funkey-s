// port/gfx/gfx_app.c — the platform-seam replay app (M3 task 4).
//
// gfx_replay.c's replay loop (itself the cited sim_main.c loop) rebuilt
// on the CLAUDE.md platform seam: render EVERY frame through the
// port/gfx compositor and PRESENT through whichever backend TU this
// binary linked (headless = host/CI, SDL2 = host dev window, SDL1.2 =
// the FunKey device — exactly ONE per target).
//
//   gfx_app --trace t.txt --simdata s.txt --gfxdata g.txt
//           --vfxdata v.txt --glyphs gl.txt --anim-dir D
//           --seed N --p1 N --p2 N --stage N --frames N
//           --out stream.txt --timing timing.txt
//           [--cpu --difficulty N --ai-bridge f]
//           [--pace 0|1] [--budget-ns N]
//           [--shot-frame N --shot-ppm f.ppm --shot-pgm f.pgm]
//           [--live --record-trace t.json --record-keys k.txt
//            [--ready-file f]]
//           [--tapjump-off-p1] [--legible]
//           [--sndpack pack.bin [--audio-samples N]]
//
// --legible (M4 task 3): the stage-surface legibility clamp
// (GFX_LEGIBLE_MIN_DEV_PX, gfx.h) — the DEVICE-path flag; browser-IoU
// paths never pass it.
//
// AUDIO (M3 task 6): --sndpack loads a SNDPACK1 SFX pack (snd_mixer.h)
// and starts the platform audio seam (44100/S16LSB/2ch, buffer
// --audio-samples, default 512 — the measured spike config; other
// values are the negative-testing / PLAN §7 fallback seam). The sim's
// sound events reach the mixer through the ml_snd_sink chokepoint
// (ml_events.h) — the mixer only READS the event plane (no RNG, no sim
// writes; the checksum stream must be unchanged, which the checks
// prove with verify-stream). On the headless backend the pack loads
// and events are bookkept deterministically but no callback thread
// runs (accept-and-idle; granted spec reports 0/0/0). Post-run the app
// prints a SECOND summary line (grammar load-bearing, see below).
//
// LIVE MODE (M3 task 5): --live replaces the trace with the S1 input
// layer (s1_input.h — PLAN §6 chord table over PlatformInput) at the
// pollInputs seam: slot 0 = S1(polled keys), slot 1 = a neutral human
// row, slots 2/3 absent. EVERY frame's injected rows are RAM-recorded
// and written post-run as --record-trace in the golden trace JSON
// format (array of [row0,row1,null,null]; numbers via ml_sb_num =
// String(x), so JSON.parse reproduces the EXACT doubles the live sim
// consumed) — the session replays as an ordinary trace. --record-keys
// (MANDATORY in live mode; iter 53, review-51 M4) additionally records
// the RAW PlatformInput key state as one bitmask per frame (RAM
// uint16, written post-run as one lowercase %04x line per frame) — the
// SOCD live witness: the coverage judge pairs frame f's raw keys with
// frame f's recorded row (same pin, same loop iteration — pairing is
// exact by construction) and asserts opposed d-pad cardinals resolved
// to a neutral axis through the REAL uinput->SDL path. BIT LAYOUT IS
// LOAD-BEARING (paired with judge-s1-coverage.js):
//   bit0 up, bit1 down, bit2 left, bit3 right, bit4 a, bit5 b,
//   bit6 x, bit7 y, bit8 start, bit9 l, bit10 r, bit11 menu, bit12 quit
// --ready-file is written right before the frame loop (the host
// launches the uinput injector only after it exists). Requires
// --pace 1 (a live session is wall-clock by definition); --cpu is
// rejected. Quit/menu requests are
// recorded-and-ignored (fixed frame count — same rationale as trace
// replay). --tapjump-off-p1 sets gameSettings tapJumpOffp1=1 after
// setup (the S1 contract, fix_plan §M3 "Input"); every replay of a
// recorded session must pass the same flag.
//
// LIVE LOOP (fix_plan §M3 task 4):
//   - wall-clock schedule: deadline(f) = t0 + (f+1)*budget (absolute —
//     no drift). The SIM always runs; when the sim finishes past its
//     frame deadline the RENDER+PRESENT are skipped (the ~30-line
//     frameskip valve — sim at wall-clock rate, render skippable;
//     PLAN §5 / funkey-envelope §1). Skips are counted, flagged
//     per-frame in the timing artifact, and listed on stderr post-run.
//   - --pace 1 (the live device mode) sleeps to each frame deadline
//     after presenting; --pace 0 (host/CI mode) runs flat out and
//     disables the valve (every frame renders — deterministic dumps).
//   - NO file I/O inside the frame loop (CLAUDE.md tmpfs gotcha): the
//     checksum stream, the per-frame timing rows and the screenshot are
//     RAM-buffered and written AFTER the loop. Timing writes go to
//     tmpfs on the device; judgment is host-side (judge-render-timing.js).
//   - SCREENSHOT: at --shot-frame the app copies its OWN framebuffer +
//     ink plane (RAM staging; render is FORCED on that frame even if
//     the valve would skip) and writes PPM/PGM post-run — never the
//     kernel fb (240x720 triple-page, wrong-page reads; CLAUDE.md).
//   - platform_poll runs every frame (event pump); input is recorded
//     into a PlatformInput but not yet consumed — M3 task 5 wires it
//     into the pollInputs seam. Quit requests are ignored during a
//     trace-fed replay (a truncated stream must never look like a pass).
//
// TIMING ROWS: "<sim> <render> <present> <skipped>" ns per frame
// (render/present are 0 on a skipped frame). full = sim+render+present;
// pacing sleep and the poll pump are excluded from all buckets (work
// time, not schedule time).
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "../sim/sim/sim.h"
#include "../sim/ml_events.h"
#include "../sim/ml_js.h"
#include "../sim/ml_ser.h"
#include "gfx.h"
#include "gfx_vfx.h"
#include "platform.h"
#include "s1_input.h"
#include "snd_mixer.h" // header-only (s1_input.h precedent) — M3 task 6

#define ML_BOOT_DRAWS 465 // the qjs boot pin (oracle/qjs/replay.sh)

void gfx_fatal(const char *what) { sim_fatal(what); }
// (sim_fatal is noreturn; the attribute on the gfx_fatal decl is satisfied)

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

// --- timing --------------------------------------------------------------------------

static uint64_t now_ns(void) {
  struct timespec ts;
  if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
    sim_fatal("clock_gettime(CLOCK_MONOTONIC) failed");
  }
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

// --- RAM-staged screenshot writers (post-run; gfx_dump_ppm conventions) ---------------

static void write_shot_ppm(const uint16_t *fb, const char *path) {
  FILE *f = fopen(path, "wb");
  if (!f) sim_fatal("shot: cannot open ppm for writing");
  fprintf(f, "P6\n%d %d\n255\n", RAST_W, RAST_H);
  for (int i = 0; i < RAST_W * RAST_H; i++) {
    const uint16_t p = fb[i];
    const uint8_t rgb[3] = {
        (uint8_t)(((p >> 11) & 31) << 3),
        (uint8_t)(((p >> 5) & 63) << 2),
        (uint8_t)((p & 31) << 3),
    };
    if (fwrite(rgb, 1, 3, f) != 3) sim_fatal("shot: ppm write failed");
  }
  if (fclose(f) != 0) sim_fatal("shot: ppm close failed");
}

static void write_shot_pgm(const uint8_t *ink, const char *path) {
  FILE *f = fopen(path, "wb");
  if (!f) sim_fatal("shot: cannot open pgm for writing");
  fprintf(f, "P5\n%d %d\n255\n", RAST_W, RAST_H);
  for (int i = 0; i < RAST_W * RAST_H; i++) {
    const uint8_t v = ink[i] ? 255 : 0;
    if (fwrite(&v, 1, 1, f) != 1) sim_fatal("shot: pgm write failed");
  }
  if (fclose(f) != 0) sim_fatal("shot: pgm close failed");
}

// --- live-session input recording (M3 task 5) ------------------------------------------
// One 22-key Input object in the golden trace JSON shape (gen-trace.js
// neutral() key order). Numbers go through ml_sb_num (ECMAScript
// String(x) — shortest round-trip, task 15), so JSON.parse returns the
// exact bit patterns the live sim consumed.

static void rec_bool(MlSb *sb, const char *key, bool v, bool comma) {
  ml_sb_putc(sb, '"');
  ml_sb_puts(sb, key);
  ml_sb_puts(sb, v ? "\":true" : "\":false");
  if (comma) ml_sb_putc(sb, ',');
}

static void rec_num(MlSb *sb, const char *key, double v, bool comma) {
  ml_sb_putc(sb, '"');
  ml_sb_puts(sb, key);
  ml_sb_puts(sb, "\":");
  ml_sb_num(sb, v);
  if (comma) ml_sb_putc(sb, ',');
}

static void rec_input(MlSb *sb, const MlInput *in) {
  ml_sb_putc(sb, '{');
  rec_bool(sb, "a", in->a, true);
  rec_bool(sb, "b", in->b, true);
  rec_bool(sb, "x", in->x, true);
  rec_bool(sb, "y", in->y, true);
  rec_bool(sb, "z", in->z, true);
  rec_bool(sb, "r", in->r, true);
  rec_bool(sb, "l", in->l, true);
  rec_bool(sb, "s", in->s, true);
  rec_bool(sb, "du", in->du, true);
  rec_bool(sb, "dr", in->dr, true);
  rec_bool(sb, "dd", in->dd, true);
  rec_bool(sb, "dl", in->dl, true);
  rec_num(sb, "lsX", in->lsX, true);
  rec_num(sb, "lsY", in->lsY, true);
  rec_num(sb, "csX", in->csX, true);
  rec_num(sb, "csY", in->csY, true);
  rec_num(sb, "lA", in->lA, true);
  rec_num(sb, "rA", in->rA, true);
  rec_num(sb, "rawX", in->rawX, true);
  rec_num(sb, "rawY", in->rawY, true);
  rec_num(sb, "rawcsX", in->rawcsX, true);
  rec_num(sb, "rawcsY", in->rawcsY, false);
  ml_sb_putc(sb, '}');
}

static void rec_frame(MlSb *sb, bool first, const MlInput *p0,
                      const MlInput *p1) {
  if (!first) ml_sb_puts(sb, ",\n");
  ml_sb_putc(sb, '[');
  rec_input(sb, p0);
  ml_sb_putc(sb, ',');
  rec_input(sb, p1);
  ml_sb_puts(sb, ",null,null]");
}

// --- main -----------------------------------------------------------------------------

static Gfx g_gfx; // big (framebuffer + anim tables); static, not stack
static uint16_t g_shot_fb[RAST_W * RAST_H];
static uint8_t g_shot_ink[RAST_W * RAST_H];

// --- audio wiring (M3 task 6) ---------------------------------------------------
static SndMixer g_mix;

// The ml_snd_sink chokepoint target: runs on the sim (main) thread at
// sound-event enqueue time; the lock brackets the mixer-state mutation
// against the audio callback (headless: no-op lock, no callback).
static void app_snd_sink(const char *name) {
  platform_audio_lock();
  snd_event(&g_mix, name);
  platform_audio_unlock();
}

int main(int argc, char **argv) {
  const char *tracePath = 0, *simdataPath = 0, *bridgePath = 0;
  const char *gfxdataPath = 0, *animDir = 0, *outPath = 0, *timingPath = 0;
  const char *vfxdataPath = 0, *glyphsPath = 0;
  const char *shotPpm = 0, *shotPgm = 0;
  const char *recordPath = 0, *readyPath = 0, *keysPath = 0;
  const char *sndpackPath = 0;
  long seed = -1, p1 = -1, p2 = -1, stage = -1, frames = -1, difficulty = 3;
  long shotFrame = -1;
  long audioSamples = 512; // spike verdict default; flag = testing seam
  bool audioSamplesGiven = false;
  long pace = 1;
  uint64_t budgetNs = 16666667ull; // 60 fps frame budget (default)
  bool cpu = false;
  bool live = false;
  bool tapJumpOffP1 = false;
  bool legible = false; // M4 task 3: stage-surface legibility (device path)
  for (int i = 1; i < argc; i++) {
    const char *a = argv[i];
    const bool hasV = i + 1 < argc;
    if (strcmp(a, "--trace") == 0 && hasV) tracePath = argv[++i];
    else if (strcmp(a, "--simdata") == 0 && hasV) simdataPath = argv[++i];
    else if (strcmp(a, "--gfxdata") == 0 && hasV) gfxdataPath = argv[++i];
    else if (strcmp(a, "--vfxdata") == 0 && hasV) vfxdataPath = argv[++i];
    else if (strcmp(a, "--glyphs") == 0 && hasV) glyphsPath = argv[++i];
    else if (strcmp(a, "--anim-dir") == 0 && hasV) animDir = argv[++i];
    else if (strcmp(a, "--out") == 0 && hasV) outPath = argv[++i];
    else if (strcmp(a, "--timing") == 0 && hasV) timingPath = argv[++i];
    else if (strcmp(a, "--shot-ppm") == 0 && hasV) shotPpm = argv[++i];
    else if (strcmp(a, "--shot-pgm") == 0 && hasV) shotPgm = argv[++i];
    else if (strcmp(a, "--shot-frame") == 0 && hasV) shotFrame = strtol(argv[++i], 0, 10);
    else if (strcmp(a, "--ai-bridge") == 0 && hasV) bridgePath = argv[++i];
    else if (strcmp(a, "--seed") == 0 && hasV) seed = strtol(argv[++i], 0, 10);
    else if (strcmp(a, "--p1") == 0 && hasV) p1 = strtol(argv[++i], 0, 10);
    else if (strcmp(a, "--p2") == 0 && hasV) p2 = strtol(argv[++i], 0, 10);
    else if (strcmp(a, "--stage") == 0 && hasV) stage = strtol(argv[++i], 0, 10);
    else if (strcmp(a, "--frames") == 0 && hasV) frames = strtol(argv[++i], 0, 10);
    else if (strcmp(a, "--difficulty") == 0 && hasV) difficulty = strtol(argv[++i], 0, 10);
    else if (strcmp(a, "--pace") == 0 && hasV) pace = strtol(argv[++i], 0, 10);
    else if (strcmp(a, "--budget-ns") == 0 && hasV) budgetNs = strtoull(argv[++i], 0, 10);
    else if (strcmp(a, "--cpu") == 0) cpu = true;
    else if (strcmp(a, "--live") == 0) live = true;
    else if (strcmp(a, "--record-trace") == 0 && hasV) recordPath = argv[++i];
    else if (strcmp(a, "--record-keys") == 0 && hasV) keysPath = argv[++i];
    else if (strcmp(a, "--ready-file") == 0 && hasV) readyPath = argv[++i];
    else if (strcmp(a, "--tapjump-off-p1") == 0) tapJumpOffP1 = true;
    else if (strcmp(a, "--legible") == 0) legible = true;
    else if (strcmp(a, "--sndpack") == 0 && hasV) sndpackPath = argv[++i];
    else if (strcmp(a, "--audio-samples") == 0 && hasV) {
      audioSamples = strtol(argv[++i], 0, 10);
      audioSamplesGiven = true;
    }
    else {
      fprintf(stderr, "gfx_app: bad argument %s\n", a);
      return 1;
    }
  }
  // --live: S1 keys replace the trace; recording is mandatory (trace
  // AND the raw-key sidecar — the SOCD witness, review-51 M4); the
  // session is wall-clock by definition (--pace 1); no CPU slot.
  if (!((tracePath != 0) ^ live) || (live && (!recordPath || !keysPath ||
      pace != 1 || cpu)) || (!live && (recordPath || readyPath || keysPath)) ||
      !simdataPath || !gfxdataPath || !vfxdataPath || !glyphsPath ||
      !animDir || !outPath ||
      !timingPath || seed < 0 || p1 < 0 || p2 < 0 || stage < 0 ||
      frames <= 0 || (cpu && !bridgePath) || (pace != 0 && pace != 1) ||
      budgetNs == 0 ||
      (shotFrame > 0) != (shotPpm && shotPgm) ||
      (shotFrame > 0 && shotFrame > frames) ||
      (audioSamplesGiven && !sndpackPath) ||
      audioSamples <= 0 || audioSamples > 65535) {
    fprintf(stderr,
            "usage: gfx_app --trace t.txt --simdata s.txt --gfxdata g.txt "
            "--vfxdata v.txt --glyphs gl.txt "
            "--anim-dir D --seed N --p1 N --p2 N --stage N --frames N "
            "--out stream.txt --timing timing.txt "
            "[--cpu --difficulty N --ai-bridge f] [--pace 0|1] "
            "[--budget-ns N] [--shot-frame N --shot-ppm f --shot-pgm f] "
            "[--live --record-trace t.json --record-keys k.txt "
            "[--ready-file f]] [--tapjump-off-p1] [--legible] "
            "[--sndpack pack.bin [--audio-samples N]]\n");
    return 1;
  }
  // RAM-buffer overflow guards (sim_main.c --timing precedent, iter 45):
  // frames bounds every post-run buffer; cap hard, die loud, never wrap.
  if (frames > 1000000L) sim_fatal("gfx_app: --frames exceeds the buffer cap (10^6)");

  sim_boot_page(&G);
  sim_data_load(simdataPath);
  sim_data_register();
  if (!live) load_trace(tracePath);

  gfx_data_load(&g_gfx.data, gfxdataPath);
  gfx_load_anim(&g_gfx, animDir, (int)p1);
  gfx_load_anim(&g_gfx, animDir, (int)p2);
  gfx_vfx_load(vfxdataPath);
  gfx_glyphs_load(glyphsPath);

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

  // vfx sink BEFORE sim_setup_match (gfx_replay.c note: the boot
  // entrance/start events fire inside it).
  gfx_init(&g_gfx, (int)stage, backgroundType);
  // M4 task 3: stage-surface legibility clamp (gfx.h rationale). The
  // device path passes --legible on BOTH ends of its host<->device shot
  // bit-compare; browser-IoU-compared paths never pass it.
  g_gfx.legibility = legible ? 1 : 0;
  gfx_vfx_install(&g_gfx);

  sim_setup_match(&G, (int)p1, (int)p2, cpu ? 1 : 0, (int)difficulty,
                  (int)stage);
  // S1 contract (fix_plan §M3 "Input"): tapJumpOffp1 = true — the
  // number 1 is the settings-domain value (action_state_shortcuts.h:
  // tap-jump arms fire only while the value == 0). Applied AFTER
  // setup, exactly like the prototype's gameSettings write; replays of
  // a recorded live session must pass the same flag.
  if (tapJumpOffP1) G.sim.tapJumpOff[0] = 1;
  G.rngStateAtFrame1 = G.rng.a;

  if (platform_init("meleelight") != 0) {
    sim_fatal("platform_init failed (display unavailable)");
  }

  // Audio (M3 task 6): pack load dies loud on ANY structural defect;
  // start dies loud on any renegotiation/open failure — audio never
  // silently runs degraded or not at all while claiming otherwise.
  if (sndpackPath) {
    snd_pack_load(&g_mix, sndpackPath);
    if (platform_audio_start(snd_mix_fill, &g_mix, (int)audioSamples) != 0) {
      sim_fatal("platform_audio_start failed");
    }
    ml_snd_sink = app_snd_sink; // the sound-event chokepoint tap
  }

  // RAM buffers (post-run flush; NO file I/O in the frame loop)
  typedef struct { uint64_t sim, render, present; uint8_t skipped; } FrameNs;
  FrameNs *tim = malloc((size_t)frames * sizeof *tim);
  const size_t streamCap = (size_t)frames * 80 + 128;
  char *stream = malloc(streamCap);
  if (!tim || !stream) sim_fatal("oom (RAM buffers)");
  size_t streamLen = 0;
  long skips = 0;
  long presentFails = 0; // platform_present nonzero rc count (review-50 M2)
  bool shotTaken = false;

  // Live-session recording (RAM; flushed post-run — no frame-loop I/O).
  MlSb rec;
  ml_sb_init(&rec);
  const MlInput neutralRow = nullInput();
  if (live) ml_sb_puts(&rec, "[\n");
  // Raw-key sidecar (RAM; the SOCD live witness — header bit layout,
  // paired with judge-s1-coverage.js).
  uint16_t *rawKeys = 0;
  if (live) {
    rawKeys = malloc((size_t)frames * sizeof *rawKeys);
    if (!rawKeys) sim_fatal("oom (raw-key sidecar buffer)");
  }

  // Ready marker: written AFTER every load + platform_init, BEFORE the
  // frame loop — the host launches the uinput injector when it appears.
  if (readyPath) {
    FILE *rf = fopen(readyPath, "w");
    if (!rf) sim_fatal("cannot open --ready-file for writing");
    if (fputs("READY\n", rf) == EOF) sim_fatal("--ready-file write failed");
    if (fclose(rf) != 0) sim_fatal("--ready-file close failed");
  }

  PlatformInput pin;
  MlInput liveRow;
  char hex[65];
  const uint64_t tStart = now_ns();
  for (long f = 0; f < frames; f++) {
    platform_poll(&pin); // event pump (live mode consumes it below)
    const uint64_t deadline = tStart + (uint64_t)(f + 1) * budgetNs;

    const MlInput *rows[4];
    if (live) {
      // The pollInputs seam: slot 0 = the S1 chord table over the
      // polled keys, slot 1 = a neutral human row, slots 2/3 absent.
      // Recorded for EVERY frame (starting window included — the
      // replay feeds the same rows; the sim ignores them there just
      // like it did live). Quit/menu: recorded-and-ignored.
      liveRow = s1_input_row(&pin);
      rows[0] = &liveRow;
      rows[1] = &neutralRow;
      rows[2] = 0;
      rows[3] = 0;
      rec_frame(&rec, f == 0, &liveRow, &neutralRow);
      // Raw-key sidecar: the SAME pin the S1 row above was resolved
      // from — sidecar[f] and trace frame f pair exactly by
      // construction. Bit layout: header comment (paired with the
      // coverage judge).
      rawKeys[f] = (uint16_t)((pin.up ? 1u : 0u) | (pin.down ? 2u : 0u) |
                              (pin.left ? 4u : 0u) | (pin.right ? 8u : 0u) |
                              (pin.a ? 16u : 0u) | (pin.b ? 32u : 0u) |
                              (pin.x ? 64u : 0u) | (pin.y ? 128u : 0u) |
                              (pin.start ? 256u : 0u) | (pin.l ? 512u : 0u) |
                              (pin.r ? 1024u : 0u) | (pin.menu ? 2048u : 0u) |
                              (pin.quit ? 4096u : 0u));
    } else {
      const long idx = f < g_trace_len - 1 ? f : g_trace_len - 1;
      const TraceRow *row = &g_trace[idx];
      for (int i = 0; i < 4; i++)
        rows[i] = row->present[i] ? &row->in[i] : 0;
    }
    G.frame = f + 1;

    const uint64_t t0 = now_ns();
    sim_game_tick(&G, rows);
    sim_frame_hash(&G, hex);
    const uint64_t t1 = now_ns();

    // Frameskip valve: the sim ran; if it finished past this frame's
    // deadline, skip render+present to catch up (never the sim). The
    // pinned screenshot frame always renders (evidence beats one skip).
    const bool isShot = shotFrame > 0 && f + 1 == shotFrame;
    const bool skip = pace == 1 && t1 > deadline && !isShot;
    uint64_t t2 = t1, t3 = t1;
    if (!skip) {
      gfx_render_frame(&g_gfx, &G);
      t2 = now_ns();
      if (platform_present(g_gfx.rz.fb) != 0) presentFails++;
      t3 = now_ns();
      if (isShot) {
        memcpy(g_shot_fb, g_gfx.rz.fb, sizeof g_shot_fb);
        memcpy(g_shot_ink, g_gfx.rz.ink, sizeof g_shot_ink);
        shotTaken = true;
      }
    } else {
      skips++;
    }
    tim[f].sim = t1 - t0;
    tim[f].render = t2 - t1;
    tim[f].present = t3 - t2;
    tim[f].skipped = skip ? 1 : 0;

    const int w = snprintf(stream + streamLen, streamCap - streamLen,
                           "F %ld %s\n", f + 1, hex);
    if (w < 0 || (size_t)w >= streamCap - streamLen) {
      sim_fatal("stream buffer overflow");
    }
    streamLen += (size_t)w;

    if (pace == 1) sleep_until_ns(deadline);
  }
  const uint64_t tEnd = now_ns();

  if (G.hasBridge && ml_ai_bridge_peek(&G.bridge) != 0) {
    sim_fatal("AI bridge has unconsumed entries after the last frame");
  }

  const uint32_t total = draws_between(G.rngStateAtReset, G.rng.a);
  const uint32_t outside = draws_between(G.rngStateAtReset, G.rngStateAtFrame1);
  {
    const int w = snprintf(stream + streamLen, streamCap - streamLen,
                           "RNG %" PRIu32 " %" PRIu32 "\nSIM OK\n", total,
                           outside);
    if (w < 0 || (size_t)w >= streamCap - streamLen) {
      sim_fatal("stream buffer overflow (trailer)");
    }
    streamLen += (size_t)w;
  }

  // Audio teardown BEFORE platform_quit (SDL_Quit would tear the audio
  // subsystem down under us); stats are read after the callback thread
  // is joined by platform_audio_stop.
  PlatformAudioStats astats;
  memset(&astats, 0, sizeof astats);
  if (sndpackPath) {
    ml_snd_sink = 0;
    platform_audio_stop();
    platform_audio_stats(&astats);
  }

  platform_quit();

  // --- post-run flushes (the ONLY file writes) ---------------------------------
  FILE *of = fopen(outPath, "w");
  if (!of) sim_fatal("cannot open --out file for writing");
  if (fwrite(stream, 1, streamLen, of) != streamLen) {
    sim_fatal("--out write failed");
  }
  if (fclose(of) != 0) sim_fatal("--out close/flush failed");

  FILE *tf = fopen(timingPath, "w");
  if (!tf) sim_fatal("cannot open --timing file for writing");
  for (long f = 0; f < frames; f++) {
    if (fprintf(tf, "%" PRIu64 " %" PRIu64 " %" PRIu64 " %u\n", tim[f].sim,
                tim[f].render, tim[f].present, (unsigned)tim[f].skipped) < 0) {
      sim_fatal("--timing write failed");
    }
  }
  if (fclose(tf) != 0) sim_fatal("--timing close/flush failed");

  if (shotFrame > 0) {
    if (!shotTaken) sim_fatal("shot frame never rendered");
    write_shot_ppm(g_shot_fb, shotPpm);
    write_shot_pgm(g_shot_ink, shotPgm);
  }

  if (live) {
    ml_sb_puts(&rec, "\n]\n");
    FILE *rf = fopen(recordPath, "w");
    if (!rf) sim_fatal("cannot open --record-trace for writing");
    if (fwrite(rec.buf, 1, rec.len, rf) != rec.len) {
      sim_fatal("--record-trace write failed");
    }
    if (fclose(rf) != 0) sim_fatal("--record-trace close/flush failed");
    // Raw-key sidecar: exactly `frames` lines, each EXACTLY 4 lowercase
    // hex digits — the judge's whitelist grammar (paired change).
    FILE *kf = fopen(keysPath, "w");
    if (!kf) sim_fatal("cannot open --record-keys for writing");
    for (long f = 0; f < frames; f++) {
      if (fprintf(kf, "%04x\n", (unsigned)rawKeys[f]) < 0) {
        sim_fatal("--record-keys write failed");
      }
    }
    if (fclose(kf) != 0) sim_fatal("--record-keys close/flush failed");
  }
  free(rawKeys);
  ml_sb_free(&rec);

  // post-run summary (stderr). GRAMMAR IS LOAD-BEARING (iter 52,
  // review-50 M2/M3 + the PROCESS §3 whitelist-grammar rule): the check
  // scripts parse this line with an anchored full-line pattern —
  //   ^gfx_app: <frames> frames, <skips> render skips, <fails> failed
  //    presents, wall <ms> ms, pace=<pace> budget=<ns> ns$
  // and gate on failed presents == 0 and the device wall-clock window.
  // Any change here is a paired change with those parsers.
  fprintf(stderr,
          "gfx_app: %ld frames, %ld render skips, %ld failed presents, "
          "wall %" PRIu64 " ms, pace=%ld budget=%" PRIu64 " ns\n",
          frames, skips, presentFails,
          (tEnd - tStart) / 1000000ull, pace, budgetNs);
  // Audio summary (printed iff --sndpack). GRAMMAR IS LOAD-BEARING
  // (PROCESS §3 whitelist-grammar rule): port/gfx/judge-audio-summary.js
  // matches this line with an anchored full-line pattern —
  //   ^gfx_app audio: <cbs> callbacks, <underruns> underruns,
  //    <badlen> badlen, <starts> voice starts, <stops> voice stops,
  //    <steals> steals, rate=<r> samples=<s> channels=<c>$
  // — and gates on underruns/badlen/cbs-window/starts/stops. rate/
  // samples/channels are the GRANTED spec (0/0/0 on headless — never a
  // faked device spec). Any change here is a paired change with that
  // judge.
  if (sndpackPath) {
    fprintf(stderr,
            "gfx_app audio: %" PRIu64 " callbacks, %" PRIu64 " underruns, "
            "%" PRIu64 " badlen, %" PRIu64 " voice starts, %" PRIu64
            " voice stops, %" PRIu64 " steals, rate=%d samples=%d "
            "channels=%d\n",
            astats.cbs, astats.underruns, astats.badlen, g_mix.starts,
            g_mix.stops, g_mix.steals, astats.rate, astats.samples,
            astats.channels);
  }
  if (skips > 0) {
    fprintf(stderr, "gfx_app: skipped frames:");
    int listed = 0;
    for (long f = 0; f < frames && listed < 64; f++) {
      if (tim[f].skipped) {
        fprintf(stderr, " %ld", f + 1);
        listed++;
      }
    }
    if (skips > 64) fprintf(stderr, " ... (%ld total)", skips);
    fprintf(stderr, "\n");
  }
  gfx_render_prof_dump(); // empty unless built -DMLFK_RENDER_PROF
  free(tim);
  free(stream);
  return 0;
}
