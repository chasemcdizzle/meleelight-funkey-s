// port/foh/foh_dev.c — the FOH DEVICE/TWIN app (fix_plan §M4 task 10;
// pre-registration AGENT-LOG iter 93).
//
// The same FOH screen machine (foh.c) the host check drives, now with
// TWO input modes:
//   --input flow — the host TWIN: committed FLOW1 rows feed foh_tick
//     frame-indexed (foh_app.c semantics verbatim; the emitted trace is
//     byte-identical to foh_app's and is cmp'd against the frozen
//     flows/<id>.expect by the device check's host leg). Used headless
//     to produce the device run's REFERENCES (trace, shots, bstate,
//     stream, mixer voice-start counts).
//   --input poll — the DEVICE path (the review-88 M3 DEFER-BOUND
//     binding): input comes ONLY from platform_poll — fk_input's uinput
//     keysyms through SDL1.2 — and the flow file contributes ONLY its
//     SHOT schedule. The emitted FOHTRACE1 trace carries device tick
//     numbers; the check judges it with judge-foh-trace.js (grammar,
//     frame-agnostic) plus NORMALIZED-SEQUENCE byte-equality vs the
//     SAME frozen .expect (normalize-foh-trace.js — frame-field elision
//     only), so a backend key-translation swap dies against the frozen
//     traces.
//
// SHOTS in poll mode (pre-registered judgment, AGENT-LOG iter 93):
//   - a SHOT row whose frame precedes the flow's first non-neutral
//     input row is TICK-INDEXED: taken at exactly that tick (startup/
//     title are input-independent; the startup progress bar is
//     tick-deterministic, so the device shot is byte-identical to the
//     twin's);
//   - every other SHOT is a Q-MARKER shot: the derived fk script
//     injects a `q` (MENU) press at the SHOT row's time — `q` is
//     consumed by NO FOH arm — and the app captures on the q EDGE with
//     the machine state settled (byte-identical to the twin's shot of
//     the same state). A q edge with no pending marker shot is a LOUD
//     death (an injection/schedule mismatch must never pass silently).
//
// MENU SFX + MUSIC SELECTION (M4 task 10; upstream citations at the
// foh.c emission sites): FohState.snd tokens go to the snd_mixer under
// platform_audio_lock; the MENU music track starts at the
// title->menu-top transition (main.js:388-390, ports==0 playMenuLoop;
// startup/title silent); at LAUNCH the stage track per main.js:
// 1341-1360 (0 battlefield / 1 yStory / 2 pStadium / 3 dreamland /
// 4 finald / 5 fod), Start-once->Loop-repeat sprite windows from
// sounds.json VERBATIM (the task-7 mixer channel; ring pre-filled
// between the menu and match loops, never inside a paced loop). Music
// manifest grammar (strict, fail-closed):
//   track <token> <pcm-path> <volbits16> <so> <sd> <lo> <ld>
// tokens menu|battlefield|ystory|pstadium|dreamland|fdest|fountain;
// a track this run USES must be present (missing used token = death;
// unused tokens may be absent).
//
// BRIDGES (the foh_app.c launch seam verbatim — FOH state, never CLI
// params, feeds sim_setup_match; RNG seeded + 465 boot draws burned
// only at the launch seam):
//   --bridge state  — BRIDGE-STATE witness file, then exit (device
//                     legs f02/f03/f05).
//   --bridge verify — full trace-fed match with live render (+ audio/
//                     music when configured), paced, frameskip valve,
//                     RAM-buffered stream/timing (gfx_app.c loop shape;
//                     the stream is judged by the UNCHANGED
//                     wrap-run.js + verify-stream.js vs the frozen
//                     golden — device leg f01).
//   --bridge live   — the OPK PLAY path: live S1 match at the launch
//                     seam (s1_input.h chord table over platform_poll,
//                     recording mandatory — the gfx_app --live
//                     contract; --tapjump-off-p1 presets the FOH
//                     options state per the Chase-ratified S1
//                     contract). Not driven by the mechanical check
//                     this iteration (registered; the acceptance
//                     playthrough + task-14 gate own it).
//
// Post-run summary grammars (stderr; LOAD-BEARING, PROCESS §3 — parsed
// anchored by check-device-foh.sh):
//   foh_dev foh: <t> ticks, <n> transitions, <s> shots, <k> render
//    skips, <f> failed presents, launched=<0|1>
//   foh_dev match: <fr> frames, <k> render skips, <f> failed presents,
//    wall <ms> ms, pace=<p> budget=<ns> ns
//   foh_dev audio: <cbs> callbacks, <u> underruns, <b> badlen, <st>
//    voice starts, <sp> voice stops, <sl> steals, rate=<r> samples=<s>
//    channels=<c>
//   foh_dev music: <out> out frames, <st> starves, <re> refills,
//    ring=32768 chunk=16384   (counters SUMMED across the menu and
//    match tracks — the channel restarts at the LAUNCH switch)
#include <inttypes.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "../gfx/gfx.h"
#include "../gfx/gfx_vfx.h"
#include "../gfx/platform.h"
#include "../gfx/s1_input.h"
#include "../gfx/snd_mixer.h"
#include "../sim/ml_events.h"
#include "../sim/ml_js.h"
#include "../sim/ml_ser.h"
#include "../sim/sim/sim.h"
#include "foh.h"

#define ML_BOOT_DRAWS 465 // the qjs boot pin (oracle/qjs/replay.sh)

void gfx_fatal(const char *what) { sim_fatal(what); }

// --- flow script (FLOW1; foh_app.c loader duplicated verbatim) --------------

typedef struct {
  long frame;
  PlatformInput in;
} FlowRow;

typedef struct {
  long frame;
  char name[33];
} FlowShot;

#define FLOW_ROW_CAP 512
#define FLOW_SHOT_CAP 16

static FlowRow g_rows[FLOW_ROW_CAP];
static int g_nrows;
static FlowShot g_shots[FLOW_SHOT_CAP];
static int g_nshots;
static long g_flow_frames;

static void flow_die(const char *path, int lineNo, const char *what) {
  fprintf(stderr, "foh_dev: flow %s line %d: %s\n", path, lineNo, what);
  exit(2);
}

static PlatformInput parse_buttons(const char *path, int lineNo,
                                   const char *tok) {
  PlatformInput in;
  memset(&in, 0, sizeof in);
  if (strcmp(tok, "-") == 0) return in;
  bool seen[26] = {false};
  for (const char *p = tok; *p; p++) {
    const char c = *p;
    if (c < 'A' || c > 'Z' || seen[c - 'A']) {
      flow_die(path, lineNo, "bad button token (chars from UDLRABXYSKNQ, "
                             "no duplicates, or '-')");
    }
    seen[c - 'A'] = true;
    switch (c) {
      case 'U': in.up = true; break;
      case 'D': in.down = true; break;
      case 'L': in.left = true; break;
      case 'R': in.right = true; break;
      case 'A': in.a = true; break;
      case 'B': in.b = true; break;
      case 'X': in.x = true; break;
      case 'Y': in.y = true; break;
      case 'S': in.start = true; break;
      case 'K': in.l = true; break;
      case 'N': in.r = true; break;
      case 'Q': in.menu = true; break;
      default:
        flow_die(path, lineNo, "bad button letter (UDLRABXYSKNQ only)");
    }
  }
  return in;
}

static void load_flow(const char *path) {
  FILE *f = fopen(path, "r");
  if (!f) {
    fprintf(stderr, "foh_dev: cannot open flow %s\n", path);
    exit(2);
  }
  char line[256];
  int lineNo = 0;
  bool sawHeader = false, sawEnd = false;
  long lastIFrame = 0, lastShotFrame = 0;
  g_nrows = 0;
  g_nshots = 0;
  g_flow_frames = 0;
  while (fgets(line, sizeof line, f)) {
    lineNo++;
    size_t n = strlen(line);
    if (n == 0 || line[n - 1] != '\n') {
      flow_die(path, lineNo, "unterminated or overlong line");
    }
    line[--n] = 0;
    if (sawEnd) flow_die(path, lineNo, "content after END");
    if (!sawHeader) {
      if (strcmp(line, "FLOW1") != 0) {
        flow_die(path, lineNo, "first line must be exactly FLOW1");
      }
      sawHeader = true;
      continue;
    }
    if (line[0] == '#') continue; // comment (validated: starts '#')
    char tok[64], arg[64];
    long fr;
    if (sscanf(line, "I %ld %63s", &fr, tok) == 2 &&
        (int)strlen(line) == snprintf(NULL, 0, "I %ld %s", fr, tok)) {
      if (g_nrows >= FLOW_ROW_CAP) flow_die(path, lineNo, "too many I rows");
      if (fr <= 0 || (g_nrows > 0 && fr <= lastIFrame)) {
        flow_die(path, lineNo, "I frames must be positive and strictly "
                               "increasing");
      }
      if (g_nrows == 0 && fr != 1) {
        flow_die(path, lineNo, "the first I row must be frame 1");
      }
      g_rows[g_nrows].frame = fr;
      g_rows[g_nrows].in = parse_buttons(path, lineNo, tok);
      g_nrows++;
      lastIFrame = fr;
      continue;
    }
    if (sscanf(line, "SHOT %ld %63s", &fr, arg) == 2 &&
        (int)strlen(line) == snprintf(NULL, 0, "SHOT %ld %s", fr, arg)) {
      if (g_nshots >= FLOW_SHOT_CAP) flow_die(path, lineNo, "too many SHOTs");
      if (fr <= 0 || (g_nshots > 0 && fr <= lastShotFrame)) {
        flow_die(path, lineNo, "SHOT frames must be positive and strictly "
                               "increasing");
      }
      size_t alen = strlen(arg);
      if (alen == 0 || alen > 32) flow_die(path, lineNo, "SHOT name length");
      for (const char *p = arg; *p; p++) {
        if (!((*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9') ||
              *p == '-')) {
          flow_die(path, lineNo, "SHOT name must match [a-z0-9-]+");
        }
      }
      for (int k = 0; k < g_nshots; k++) {
        if (strcmp(g_shots[k].name, arg) == 0) {
          flow_die(path, lineNo, "duplicate SHOT name");
        }
      }
      g_shots[g_nshots].frame = fr;
      strcpy(g_shots[g_nshots].name, arg);
      g_nshots++;
      lastShotFrame = fr;
      continue;
    }
    if (sscanf(line, "END %ld", &fr) == 1 &&
        (int)strlen(line) == snprintf(NULL, 0, "END %ld", fr)) {
      if (fr <= 0 || fr < lastIFrame || fr < lastShotFrame) {
        flow_die(path, lineNo, "END frame must cover every I/SHOT frame");
      }
      g_flow_frames = fr;
      sawEnd = true;
      continue;
    }
    flow_die(path, lineNo, "line matches no FLOW1 form (I/SHOT/END/#)");
  }
  fclose(f);
  if (!sawHeader || !sawEnd || g_nrows == 0) {
    fprintf(stderr, "foh_dev: flow %s: missing FLOW1 header, I rows, or "
                    "END\n", path);
    exit(2);
  }
}

// --- match trace loading (sim_main.c:39-148, duplicated verbatim) ------------

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

// --- draw counting (sim_main.c:150-162) ---------------------------------------

static uint32_t mulberry_inv(void) {
  const uint32_t k = 0x6D2B79F5u;
  uint32_t x = k;
  for (int i = 0; i < 5; i++) x *= 2u - k * x;
  return x;
}

static uint32_t draws_between(uint32_t from, uint32_t to) {
  return (to - from) * mulberry_inv();
}

// --- timing (gfx_app.c) -------------------------------------------------------

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

// --- shot writer (foh_app.c write_shot_ppm verbatim) ---------------------------

static void write_shot_ppm(const uint16_t *fb, const char *path) {
  FILE *f = fopen(path, "wb");
  if (!f) sim_fatal("cannot open shot ppm for writing");
  fprintf(f, "P6\n%d %d\n255\n", RAST_W, RAST_H);
  for (int k = 0; k < RAST_W * RAST_H; k++) {
    const uint16_t px = fb[k];
    const uint8_t rgb[3] = {(uint8_t)(((px >> 11) & 0x1F) << 3),
                            (uint8_t)(((px >> 5) & 0x3F) << 2),
                            (uint8_t)((px & 0x1F) << 3)};
    if (fwrite(rgb, 1, 3, f) != 3) sim_fatal("shot ppm write failed");
  }
  if (fclose(f) != 0) sim_fatal("shot ppm close failed");
}

// --- live-session input recording (gfx_app.c:381-431 duplicated verbatim) -----

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

// --- audio wiring (gfx_app.c shape) --------------------------------------------

static Gfx g_gfx;   // big (framebuffer + anim tables); static, not stack
static Raster g_rz; // the FOH raster (foh_app.c precedent)
static SndMixer g_mix;
static bool g_have_audio;

static void app_snd_sink(const char *name) {
  const size_t n = strlen(name);
  if (n > 5 && strcmp(name + n - 5, ".stop") == 0) return; // id sink owns it
  platform_audio_lock();
  snd_event(&g_mix, name);
  platform_audio_unlock();
}

static void app_snd_stop_sink(const char *token, int hasId, double id) {
  platform_audio_lock();
  snd_event_stop_id(&g_mix, token, hasId, id);
  platform_audio_unlock();
}

// menu-plane SFX chokepoint (FohState.snd tokens; foh.c citations).
static void foh_snd(const char *name) {
  if (!g_have_audio) return;
  platform_audio_lock();
  snd_event(&g_mix, name);
  platform_audio_unlock();
}

// --- music: manifest + file-backed reader + track switching --------------------

typedef struct {
  bool present;
  char path[512];
  uint16_t gainQ8;
  uint64_t so, sd, lo, ld; // sounds.json sprite ms values VERBATIM
} MusTrack;

// token order: menu + the main.js:1341-1360 stage switch (oracle ids)
static const char *kMusTok[7] = {"menu",      "battlefield", "ystory",
                                 "pstadium",  "dreamland",   "fdest",
                                 "fountain"};
static MusTrack g_mus_tracks[7];
static bool g_have_music;

static void mus_die(const char *path, int lineNo, const char *what) {
  fprintf(stderr, "foh_dev: music manifest %s line %d: %s\n", path, lineNo,
          what);
  exit(2);
}

// strict numeral (0|[1-9][0-9]*; the gfx_app.c mus_scan_u64 grammar)
static uint64_t mus_num(const char *path, int lineNo, const char *s) {
  if (*s < '0' || *s > '9') mus_die(path, lineNo, "bad numeral");
  if (s[0] == '0' && s[1] != 0) mus_die(path, lineNo, "leading zero");
  uint64_t v = 0;
  for (const char *p = s; *p; p++) {
    if (*p < '0' || *p > '9') mus_die(path, lineNo, "bad numeral digit");
    if (v > (~0ull - 9ull) / 10ull) mus_die(path, lineNo, "numeral overflow");
    v = v * 10ull + (uint64_t)(*p - '0');
  }
  return v;
}

static void load_music_manifest(const char *path) {
  FILE *f = fopen(path, "r");
  if (!f) {
    fprintf(stderr, "foh_dev: cannot open music manifest %s\n", path);
    exit(2);
  }
  char line[1024];
  int lineNo = 0;
  int rows = 0;
  while (fgets(line, sizeof line, f)) {
    lineNo++;
    size_t n = strlen(line);
    if (n == 0 || line[n - 1] != '\n') {
      mus_die(path, lineNo, "unterminated or overlong line");
    }
    line[--n] = 0;
    if (n == 0 || line[0] == '#') continue;
    // track <token> <pcm-path> <volbits16> <so> <sd> <lo> <ld>
    char tok[16], pcm[512], vb[64], a1[32], a2[32], a3[32], a4[32];
    if (sscanf(line, "track %15s %511s %63s %31s %31s %31s %31s", tok, pcm,
               vb, a1, a2, a3, a4) != 7 ||
        (int)strlen(line) == 0 ||
        (int)strlen(line) != snprintf(NULL, 0, "track %s %s %s %s %s %s %s",
                                      tok, pcm, vb, a1, a2, a3, a4)) {
      mus_die(path, lineNo, "line fails the exact 8-field track grammar");
    }
    int idx = -1;
    for (int k = 0; k < 7; k++) {
      if (strcmp(tok, kMusTok[k]) == 0) { idx = k; break; }
    }
    if (idx < 0) mus_die(path, lineNo, "unknown track token");
    if (g_mus_tracks[idx].present) mus_die(path, lineNo, "duplicate token");
    if (strlen(vb) != 16) mus_die(path, lineNo, "volbits not 16 hex digits");
    uint64_t bits = 0;
    for (int k = 0; k < 16; k++) {
      const char c = vb[k];
      bits <<= 4;
      if (c >= '0' && c <= '9') bits |= (uint64_t)(c - '0');
      else if (c >= 'a' && c <= 'f') bits |= (uint64_t)(c - 'a' + 10);
      else mus_die(path, lineNo, "volbits digit not lowercase hex");
    }
    double vol;
    memcpy(&vol, &bits, 8);
    if (!(vol >= 0.0 && vol <= 1.0)) mus_die(path, lineNo, "volume outside [0,1]");
    MusTrack *t = &g_mus_tracks[idx];
    t->present = true;
    strcpy(t->path, pcm);
    t->gainQ8 = (uint16_t)(vol * 256.0 + 0.5); // the pack-snd.js formula
    t->so = mus_num(path, lineNo, a1);
    t->sd = mus_num(path, lineNo, a2);
    t->lo = mus_num(path, lineNo, a3);
    t->ld = mus_num(path, lineNo, a4);
    rows++;
  }
  if (ferror(f)) mus_die(path, lineNo, "read error");
  fclose(f);
  if (rows == 0) mus_die(path, 0, "no track rows");
}

// file-backed SndMusicRead (gfx_app.c mus_file_read verbatim shape)
static FILE *g_mus_file;
static uint64_t g_mus_file_frames;

static void mus_file_read(void *ud, uint64_t fileFrame, int16_t *dst,
                          uint32_t frames) {
  (void)ud;
  if (fileFrame + frames > g_mus_file_frames) {
    sim_fatal("music: read past the PCM file (program/segmentation bug)");
  }
  if (fseeko(g_mus_file, (off_t)(fileFrame * 4ull), SEEK_SET) != 0) {
    sim_fatal("music: reader seek failed");
  }
  if (fread(dst, 4, frames, g_mus_file) != frames) {
    sim_fatal("music: reader short read (truncated/unreadable PCM)");
  }
}

// reader thread (gfx_app.c mus_reader_main shape; C11 atomics per the
// review-87 M2 class rule)
static atomic_int g_mus_quit;
static atomic_int g_mus_reader_done;
static pthread_t g_mus_thread;
static bool g_mus_thread_live;
// track-switch counter carry (the channel restarts at LAUNCH)
static uint64_t g_mus_prev_out, g_mus_prev_starves, g_mus_prev_refills;

static void *mus_reader_main(void *arg) {
  (void)arg;
  for (;;) {
    const int quit = atomic_load_explicit(&g_mus_quit, memory_order_acquire);
    platform_audio_lock();
    const uint64_t cons = g_mix.music.outPos >> 1;
    const uint64_t wr = g_mix.music.wr;
    platform_audio_unlock();
    if (quit) {
      atomic_store_explicit(&g_mus_reader_done, 1, memory_order_release);
      return 0;
    }
    if (SND_MUSIC_RING_FRAMES - (wr - cons) >= SND_MUSIC_CHUNK_FRAMES) {
      snd_music_fill(&g_mix.music, wr, SND_MUSIC_CHUNK_FRAMES, mus_file_read,
                     0);
      platform_audio_lock();
      g_mix.music.wr = wr + SND_MUSIC_CHUNK_FRAMES;
      g_mix.music.refills++;
      platform_audio_unlock();
    } else {
      struct timespec ts;
      ts.tv_sec = 0;
      ts.tv_nsec = 25000000L; // 25 ms poll (the task-7 cadence)
      nanosleep(&ts, 0);
    }
  }
}

static void mus_reader_start(void) {
  atomic_store_explicit(&g_mus_quit, 0, memory_order_release);
  atomic_store_explicit(&g_mus_reader_done, 0, memory_order_release);
  if (pthread_create(&g_mus_thread, 0, mus_reader_main, 0) != 0) {
    sim_fatal("music: pthread_create(reader) failed");
  }
  g_mus_thread_live = true;
}

static void mus_reader_stop(void) {
  if (!g_mus_thread_live) return;
  atomic_store_explicit(&g_mus_quit, 1, memory_order_release);
  const uint64_t deadline = now_ns() + 5000000000ull; // the task-7 bound
  while (!atomic_load_explicit(&g_mus_reader_done, memory_order_acquire)) {
    if (now_ns() >= deadline) {
      sim_fatal("music: reader thread did not exit within the teardown "
                "deadline (wedged SD read or quit-flag defect)");
    }
    struct timespec ts;
    ts.tv_sec = 0;
    ts.tv_nsec = 2000000L;
    nanosleep(&ts, 0);
  }
  if (pthread_join(g_mus_thread, 0) != 0) {
    sim_fatal("music: pthread_join(reader) failed");
  }
  g_mus_thread_live = false;
}

// Configure the channel for track idx: open the PCM, cfg + prefill the
// whole ring, publish wr. `on` controls audibility (the menu track is
// prepared with on=0 at boot and flipped on at title->menu-top —
// main.js:390; the stage track goes straight to on=1 at LAUNCH).
// MUST be called with the reader STOPPED; caller restarts it.
static void mus_track_program(int idx, int on) {
  MusTrack *t = &g_mus_tracks[idx];
  if (!t->present) {
    fprintf(stderr, "foh_dev: music manifest has no '%s' row but the run "
                    "needs it\n", kMusTok[idx]);
    exit(2);
  }
  if (g_mus_file) { fclose(g_mus_file); g_mus_file = 0; }
  g_mus_file = fopen(t->path, "rb");
  if (!g_mus_file) sim_fatal("music: cannot open track PCM");
  if (fseeko(g_mus_file, 0, SEEK_END) != 0) sim_fatal("music: seek failed");
  const off_t msz = ftello(g_mus_file);
  if (msz <= 0) sim_fatal("music: empty PCM");
  // carry the counters across the switch (summed for the summary line)
  platform_audio_lock();
  g_mus_prev_out += g_mix.music.outPos;
  g_mus_prev_starves += g_mix.music.starves;
  g_mus_prev_refills += g_mix.music.refills;
  free(g_mix.music.ring); // cfg mallocs a fresh ring (leak guard)
  snd_music_cfg(&g_mix.music, t->gainQ8, t->so, t->sd, t->lo, t->ld,
                (uint64_t)msz);
  g_mix.music.on = 0; // silent until published below (never a torn ring)
  platform_audio_unlock();
  g_mus_file_frames = g_mix.music.fileFrames;
  // prefill UNLOCKED (producer-owned slots; the channel is off)
  snd_music_fill(&g_mix.music, 0, SND_MUSIC_RING_FRAMES, mus_file_read, 0);
  platform_audio_lock();
  g_mix.music.wr = SND_MUSIC_RING_FRAMES;
  g_mix.music.on = on;
  platform_audio_unlock();
}

// stage id (oracle order) -> music track index (main.js:1341-1360)
static int mus_stage_track(int stage) {
  switch (stage) {
    case 0: return 1; // battlefield (main.js:1344)
    case 1: return 2; // yStory (main.js:1347)
    case 2: return 3; // pStadium (main.js:1350)
    case 3: return 4; // dreamland (main.js:1353)
    case 4: return 5; // finald (main.js:1356)
    case 5: return 6; // fod (main.js:1359)
    default: sim_fatal("music: bad stage id"); return -1;
  }
}

// --- FOH trace RAM buffer -------------------------------------------------------

static MlSb g_tr; // FOHTRACE1 lines (RAM; flushed after the FOH phase)

static void tr_line(const char *fmt, ...) {
  char buf[256];
  va_list ap;
  va_start(ap, fmt);
  const int w = vsnprintf(buf, sizeof buf, fmt, ap);
  va_end(ap);
  if (w < 0 || w >= (int)sizeof buf) sim_fatal("trace line overflow");
  ml_sb_puts(&g_tr, buf);
  ml_sb_putc(&g_tr, '\n');
}

// --- shot RAM staging -------------------------------------------------------------

typedef struct {
  char name[33];
  uint16_t fb[RAST_W * RAST_H];
} ShotBuf;

static ShotBuf g_shotbuf[FLOW_SHOT_CAP];
static int g_nshotbuf;

static void shot_capture(const char *name, const uint16_t *fb, long tick) {
  if (g_nshotbuf >= FLOW_SHOT_CAP) sim_fatal("shot buffer overflow");
  strcpy(g_shotbuf[g_nshotbuf].name, name);
  memcpy(g_shotbuf[g_nshotbuf].fb, fb, sizeof g_shotbuf[0].fb);
  g_nshotbuf++;
  tr_line("SHOT %ld %s", tick, name);
}

// --- main --------------------------------------------------------------------------

int main(int argc, char **argv) {
  const char *flowPath = 0, *inputMode = 0, *flowOut = 0, *shotsDir = 0;
  const char *readyPath = 0, *bridge = 0;
  const char *simdataPath = 0, *bstateOut = 0;
  const char *tracePath = 0, *outPath = 0, *timingPath = 0;
  const char *gfxdataPath = 0, *vfxdataPath = 0, *glyphsPath = 0;
  const char *animDir = 0;
  const char *sndpackPath = 0, *musicManifest = 0;
  const char *recordPath = 0, *keysPath = 0;
  long seed = -1, frames = -1, fohMax = -1;
  long audioSamples = 512;
  bool audioSamplesGiven = false;
  long pace = 1;
  uint64_t budgetNs = 16666667ull;
  bool cpuLive = false, legible = false, tapJumpOffP1 = false;
  for (int i = 1; i < argc; i++) {
    const char *a = argv[i];
    const bool hasV = i + 1 < argc;
    if (strcmp(a, "--flow") == 0 && hasV) flowPath = argv[++i];
    else if (strcmp(a, "--input") == 0 && hasV) inputMode = argv[++i];
    else if (strcmp(a, "--flow-out") == 0 && hasV) flowOut = argv[++i];
    else if (strcmp(a, "--shots-dir") == 0 && hasV) shotsDir = argv[++i];
    else if (strcmp(a, "--ready-file") == 0 && hasV) readyPath = argv[++i];
    else if (strcmp(a, "--foh-max") == 0 && hasV) fohMax = strtol(argv[++i], 0, 10);
    else if (strcmp(a, "--bridge") == 0 && hasV) bridge = argv[++i];
    else if (strcmp(a, "--simdata") == 0 && hasV) simdataPath = argv[++i];
    else if (strcmp(a, "--bstate-out") == 0 && hasV) bstateOut = argv[++i];
    else if (strcmp(a, "--seed") == 0 && hasV) seed = strtol(argv[++i], 0, 10);
    else if (strcmp(a, "--trace") == 0 && hasV) tracePath = argv[++i];
    else if (strcmp(a, "--frames") == 0 && hasV) frames = strtol(argv[++i], 0, 10);
    else if (strcmp(a, "--out") == 0 && hasV) outPath = argv[++i];
    else if (strcmp(a, "--timing") == 0 && hasV) timingPath = argv[++i];
    else if (strcmp(a, "--gfxdata") == 0 && hasV) gfxdataPath = argv[++i];
    else if (strcmp(a, "--vfxdata") == 0 && hasV) vfxdataPath = argv[++i];
    else if (strcmp(a, "--glyphs") == 0 && hasV) glyphsPath = argv[++i];
    else if (strcmp(a, "--anim-dir") == 0 && hasV) animDir = argv[++i];
    else if (strcmp(a, "--sndpack") == 0 && hasV) sndpackPath = argv[++i];
    else if (strcmp(a, "--music-manifest") == 0 && hasV) musicManifest = argv[++i];
    else if (strcmp(a, "--record-trace") == 0 && hasV) recordPath = argv[++i];
    else if (strcmp(a, "--record-keys") == 0 && hasV) keysPath = argv[++i];
    else if (strcmp(a, "--pace") == 0 && hasV) pace = strtol(argv[++i], 0, 10);
    else if (strcmp(a, "--budget-ns") == 0 && hasV) budgetNs = strtoull(argv[++i], 0, 10);
    else if (strcmp(a, "--cpu-live") == 0) cpuLive = true;
    else if (strcmp(a, "--legible") == 0) legible = true;
    else if (strcmp(a, "--tapjump-off-p1") == 0) tapJumpOffP1 = true;
    else if (strcmp(a, "--audio-samples") == 0 && hasV) {
      audioSamples = strtol(argv[++i], 0, 10);
      audioSamplesGiven = true;
    } else {
      fprintf(stderr, "foh_dev: bad argument %s\n", a);
      return 1;
    }
  }
  const bool inFlow = inputMode && strcmp(inputMode, "flow") == 0;
  const bool inPoll = inputMode && strcmp(inputMode, "poll") == 0;
  const bool brState = bridge && strcmp(bridge, "state") == 0;
  const bool brVerify = bridge && strcmp(bridge, "verify") == 0;
  const bool brLive = bridge && strcmp(bridge, "live") == 0;
  if (!flowPath || !flowOut || !inputMode || (!inFlow && !inPoll) ||
      (bridge && !brState && !brVerify && !brLive) ||
      // poll mode is wall-clock by definition and needs its tick budget
      (inPoll && (pace != 1 || fohMax <= 0)) ||
      (inFlow && (fohMax > 0 || readyPath)) ||
      // bridge modes need the sim data plane + seed + the state witness
      (bridge && (!simdataPath || seed < 0 || !bstateOut)) ||
      (!bridge && (simdataPath || seed >= 0 || bstateOut)) ||
      // verify needs the golden trace + stream/timing sinks + render data
      (brVerify && (!tracePath || frames <= 0 || !outPath || !timingPath ||
                    !gfxdataPath || !vfxdataPath || !glyphsPath || !animDir)) ||
      // live needs render data + bounded frames + mandatory recording
      (brLive && (!recordPath || !keysPath || frames <= 0 || !gfxdataPath ||
                  !vfxdataPath || !glyphsPath || !animDir || pace != 1 ||
                  cpuLive || !inPoll)) ||
      (!brVerify && !brLive &&
       (tracePath || frames > 0 || outPath || timingPath || gfxdataPath ||
        vfxdataPath || glyphsPath || animDir || legible)) ||
      (!brLive && (recordPath || keysPath || tapJumpOffP1)) ||
      (brVerify && !tracePath) || (cpuLive && !brVerify) ||
      (pace != 0 && pace != 1) || budgetNs == 0 ||
      (audioSamplesGiven && !sndpackPath) ||
      audioSamples <= 0 || audioSamples > 65535 ||
      (musicManifest && !sndpackPath)) {
    fprintf(stderr,
            "usage: foh_dev --flow f.flow --input flow|poll --flow-out t.txt"
            " [--shots-dir D] [--ready-file f] [--foh-max N (poll)]"
            " [--pace 0|1] [--budget-ns N]"
            " [--bridge state|verify|live --simdata s --seed N"
            " --bstate-out b]"
            " [verify: --trace t --frames N --out o --timing tim"
            " --gfxdata g --vfxdata v --glyphs gl --anim-dir D [--legible]"
            " [--cpu-live]]"
            " [live: --frames N --record-trace t.json --record-keys k.txt"
            " --gfxdata ... [--legible] [--tapjump-off-p1]]"
            " [--sndpack p [--audio-samples N]] [--music-manifest m.txt]\n");
    return 1;
  }
  if (frames > 1000000L) sim_fatal("foh_dev: --frames exceeds the buffer cap");
  if (fohMax > 1000000L) sim_fatal("foh_dev: --foh-max exceeds the tick cap");

  load_flow(flowPath);

  // flow id = basename minus .flow (foh_app.c verbatim)
  const char *base = strrchr(flowPath, '/');
  base = base ? base + 1 : flowPath;
  char flowId[64];
  {
    size_t blen = strlen(base);
    if (blen < 6 || blen - 5 >= sizeof flowId ||
        strcmp(base + blen - 5, ".flow") != 0) {
      fprintf(stderr, "foh_dev: --flow must end in .flow\n");
      return 1;
    }
    memcpy(flowId, base, blen - 5);
    flowId[blen - 5] = 0;
    for (const char *p = flowId; *p; p++) {
      if (!((*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9') ||
            *p == '-')) {
        fprintf(stderr, "foh_dev: flow id must match [a-z0-9-]+\n");
        return 1;
      }
    }
  }

  // poll-mode shot split (pre-registered rule): SHOT rows before the
  // flow's FIRST NON-NEUTRAL input row are tick-indexed; the rest are
  // q-marker shots consumed in order.
  long firstInputFrame = 0;
  for (int k = 0; k < g_nrows; k++) {
    const PlatformInput *r = &g_rows[k].in;
    if (r->up || r->down || r->left || r->right || r->a || r->b || r->x ||
        r->y || r->start || r->l || r->r || r->menu) {
      firstInputFrame = g_rows[k].frame;
      break;
    }
  }
  if (inPoll && firstInputFrame == 0) firstInputFrame = g_flow_frames + 1;

  if (bridge) {
    sim_boot_page(&G);
    sim_data_load(simdataPath);
    sim_data_register();
  }

  if (platform_init("meleelight-foh") != 0) {
    sim_fatal("platform_init failed");
  }

  g_have_audio = sndpackPath != 0;
  g_have_music = musicManifest != 0;
  if (sndpackPath) {
    snd_pack_load(&g_mix, sndpackPath);
    if (musicManifest) {
      load_music_manifest(musicManifest);
      // prepare the MENU track before audio starts: prefilled, on=0
      // (silent through startup/title — main.js:390 starts menu music
      // only at the title->menu-top join).
      mus_track_program(0, 0);
    }
    if (platform_audio_start(snd_mix_fill, &g_mix, (int)audioSamples) != 0) {
      sim_fatal("platform_audio_start failed");
    }
    ml_snd_sink = app_snd_sink; // match-plane chokepoints (gfx_app shape)
    ml_snd_stop_id_sink = app_snd_stop_sink;
    if (musicManifest) mus_reader_start();
  }

  ml_sb_init(&g_tr);
  tr_line("FOHTRACE1 flow=%s", flowId);

  // ready marker: written right before the FOH loop — the rig launches
  // fk_input only after it appears (check-device-input.sh handshake).
  if (readyPath) {
    FILE *rf = fopen(readyPath, "w");
    if (!rf) sim_fatal("cannot open --ready-file for writing");
    if (fputs("READY\n", rf) == EOF) sim_fatal("--ready-file write failed");
    if (fclose(rf) != 0) sim_fatal("--ready-file close failed");
  }

  FohState foh;
  foh_init(&foh);
  if (brLive && tapJumpOffP1) {
    // The S1 contract preset (PLAN §6, Chase-ratified): the options
    // screen SHOWS it on and the player may toggle; LAUNCH consumes
    // the FOH state like every other setting.
    foh.tapJumpOff[0] = 1;
  }
  long transitions = 0;
  long fohSkips = 0, fohPresentFails = 0;
  int rowIdx = 0, shotIdx = 0;   // flow-mode input rows + row-driven shots
  int markerIdx = 0;             // poll-mode q-marker shot cursor
  // poll mode: advance markerIdx past the tick-indexed shots
  if (inPoll) {
    while (markerIdx < g_nshots &&
           g_shots[markerIdx].frame < firstInputFrame) {
      markerIdx++;
    }
  }
  const int firstMarkerIdx = markerIdx;
  PlatformInput cur, prevPin;
  memset(&cur, 0, sizeof cur);
  memset(&prevPin, 0, sizeof prevPin);
  bool menuMusicOn = false;
  long fohTicks = 0;
  const long fohLimit = inPoll ? fohMax : g_flow_frames;
  const uint64_t fohStart = now_ns();
  long endTick = 0;
  for (long t = 1; t <= fohLimit; t++) {
    fohTicks = t;
    const uint64_t deadline = fohStart + (uint64_t)t * budgetNs;
    bool qEdge = false;
    if (inPoll) {
      PlatformInput pin;
      platform_poll(&pin); // THE device input path (the M3 binding)
      qEdge = pin.menu && !prevPin.menu;
      prevPin = pin;
      cur = pin;
    } else {
      while (rowIdx < g_nrows && g_rows[rowIdx].frame == t) {
        cur = g_rows[rowIdx].in;
        rowIdx++;
      }
    }
    foh_tick(&foh, &cur);
    bool launched = false;
    for (int e = 0; e < foh.nev; e++) {
      const FohEvent *ev = &foh.ev[e];
      if (ev->kind == FOH_EV_TRANS) {
        transitions++;
        tr_line("T %ld %s %s %s", t, ev->from, ev->to, ev->cause);
        if (g_have_music && !menuMusicOn && strcmp(ev->to, "menu-top") == 0 &&
            strcmp(ev->from, "title") == 0) {
          // menu music ON at the join (main.js:388-390) — the track is
          // already prefilled; this is a lock-bracketed flag flip.
          platform_audio_lock();
          g_mix.music.on = 1;
          platform_audio_unlock();
          menuMusicOn = true;
        }
      } else if (ev->kind == FOH_EV_SEL) {
        if (ev->sval) tr_line("S %ld %s %s", t, ev->field, ev->sval);
        else tr_line("S %ld %s %d", t, ev->field, ev->val);
      } else {
        launched = true;
        tr_line("LAUNCH %ld p1=%d p2=%d p2type=%d difficulty=%d stage=%d "
                "turbo=%d lcancel=%d tapjump=%d,%d,%d,%d versus=0",
                t, foh.p1Char, foh.p2Char, foh.p2Type, foh.difficulty,
                foh.stageSel, foh.turbo, foh.lCancelType, foh.tapJumpOff[0],
                foh.tapJumpOff[1], foh.tapJumpOff[2], foh.tapJumpOff[3]);
      }
    }
    for (int k = 0; k < foh.nsnd; k++) foh_snd(foh.snd[k]);

    // shot due this tick? (forces render through the valve). The whole
    // shot machinery is armed ONLY with --shots-dir: the OPK PLAY path
    // runs without it, so the player's MENU button is a no-op there
    // (never the marker-mismatch death — that death is for the rig,
    // where the q schedule is machine-generated).
    bool shotDue = false;
    if (shotsDir && inPoll) {
      if (shotIdx < g_nshots && g_shots[shotIdx].frame < firstInputFrame &&
          g_shots[shotIdx].frame == t) {
        shotDue = true; // tick-indexed shot
      }
      if (qEdge) {
        if (markerIdx >= g_nshots) {
          sim_fatal("foh_dev: q edge with no pending marker shot "
                    "(injection/schedule mismatch)");
        }
        shotDue = true;
      }
    } else if (shotsDir) {
      if (shotIdx < g_nshots && g_shots[shotIdx].frame == t) shotDue = true;
    }

    const uint64_t tNow = now_ns();
    const bool skip = pace == 1 && tNow > deadline && !shotDue;
    if (!skip) {
      foh_render(&foh, &g_rz);
      if (platform_present(g_rz.fb) != 0) fohPresentFails++;
    } else {
      fohSkips++;
    }
    if (shotDue) {
      if (inPoll) {
        if (qEdge) {
          shot_capture(g_shots[markerIdx].name, g_rz.fb, t);
          markerIdx++;
        } else {
          shot_capture(g_shots[shotIdx].name, g_rz.fb, t);
          shotIdx++;
        }
      } else {
        // flow mode may have several SHOT rows on one tick frame? the
        // loader enforces strictly increasing frames — exactly one.
        shot_capture(g_shots[shotIdx].name, g_rz.fb, t);
        shotIdx++;
      }
    }
    endTick = t;
    if (launched && inPoll) break; // device: END right after LAUNCH
    if (pace == 1) sleep_until_ns(deadline);
  }
  tr_line("END %ld transitions=%ld", inPoll ? endTick : g_flow_frames,
          transitions);
  (void)fohTicks;
  (void)firstMarkerIdx;

  // flush the FOH artifacts now (between phases; never inside a paced
  // loop — the match loop below is the paced surface that matters)
  {
    FILE *tf = fopen(flowOut, "w");
    if (!tf) sim_fatal("cannot open --flow-out for writing");
    if (fwrite(g_tr.buf, 1, g_tr.len, tf) != g_tr.len) {
      sim_fatal("--flow-out write failed");
    }
    if (fclose(tf) != 0) sim_fatal("--flow-out close/flush failed");
  }
  if (shotsDir) {
    for (int k = 0; k < g_nshotbuf; k++) {
      char path[600];
      if (snprintf(path, sizeof path, "%s/%s.ppm", shotsDir,
                   g_shotbuf[k].name) >= (int)sizeof path) {
        sim_fatal("shot path overflow");
      }
      write_shot_ppm(g_shotbuf[k].fb, path);
    }
  }

  long matchSkips = 0, matchPresentFails = 0;
  uint64_t matchWallMs = 0;
  bool ranMatch = false;

  if (bridge) {
    if (!foh.launched) {
      fprintf(stderr, "foh_dev: --bridge given but the flow never "
                      "launched\n");
      return 4;
    }
    if (brVerify && cpuLive != (foh.p2Type == 1)) {
      fprintf(stderr, "foh_dev: --cpu-live must match the FOH P2 type "
                      "(cross-guard)\n");
      return 4;
    }
    if (brVerify) load_trace(tracePath);

    // seed + boot draws ONLY at the launch seam (foh_app.c verbatim)
    ml_active_rng = &G.rng;
    ml_rng_seed(&G.rng, (uint32_t)seed);
    for (int k = 0; k < ML_BOOT_DRAWS; k++) (void)ml_rng_next(&G.rng);
    G.rngStateAtReset = G.rng.a;

    if (brVerify || brLive) {
      gfx_data_load(&g_gfx.data, gfxdataPath);
      gfx_load_anim(&g_gfx, animDir, foh.p1Char);
      gfx_load_anim(&g_gfx, animDir, foh.p2Char);
      gfx_vfx_load(vfxdataPath);
      gfx_glyphs_load(glyphsPath);
      // peek startGame's background draw from a COPY (gfx_app.c)
      MlRng peek = G.rng;
      const int backgroundType = (int)js_round(ml_rng_next(&peek));
      gfx_init(&g_gfx, foh.stageSel, backgroundType);
      g_gfx.legibility = legible ? 1 : 0;
      gfx_vfx_install(&g_gfx); // BEFORE sim_setup_match (boot events)
    }

    // THE BRIDGE POINT: every parameter from the FOH state.
    sim_setup_match(&G, foh.p1Char, foh.p2Char, foh.p2Type, foh.difficulty,
                    foh.stageSel);
    G.sim.turbo = foh.turbo != 0;
    G.sim.lCancelType = foh.lCancelType;
    for (int i = 0; i < 4; i++) G.sim.tapJumpOff[i] = foh.tapJumpOff[i];
    G.rngStateAtFrame1 = G.rng.a;

    // BRIDGE-STATE witness (read back FROM GameState; foh_app.c verbatim)
    {
      FILE *bf = fopen(bstateOut, "w");
      if (!bf) sim_fatal("cannot open --bstate-out for writing");
      uint64_t phantomBits;
      memcpy(&phantomBits, &G.sim.phantomThreshold, 8);
      if (fprintf(bf,
                  "BRIDGE-STATE p1=%d p2=%d p2type=%d difficulty=%d stage=%d "
                  "turbo=%d lcancel=%d tapjump=%d,%d,%d,%d "
                  "phantom=%016" PRIx64 "\n",
                  (int)G.sim.characterSelections[0],
                  (int)G.sim.characterSelections[1], (int)G.sim.playerType[1],
                  (int)G.cpuDifficulty[1], (int)G.stageSelect,
                  G.sim.turbo ? 1 : 0, (int)G.sim.lCancelType,
                  (int)G.sim.tapJumpOff[0], (int)G.sim.tapJumpOff[1],
                  (int)G.sim.tapJumpOff[2], (int)G.sim.tapJumpOff[3],
                  phantomBits) < 0) {
        sim_fatal("--bstate-out write failed");
      }
      if (fclose(bf) != 0) sim_fatal("--bstate-out close/flush failed");
    }

    if (brVerify || brLive) {
      // MUSIC SWITCH at the launch seam (main.js:1341: stop what is
      // playing, then the stage track) — reader stopped, track
      // reprogrammed + prefilled BETWEEN the loops, reader restarted.
      if (g_have_music) {
        mus_reader_stop();
        mus_track_program(mus_stage_track(foh.stageSel), 1);
        mus_reader_start();
      }

      typedef struct { uint64_t sim, render, present; uint8_t skipped; } FrameNs;
      FrameNs *tim = 0;
      if (brVerify) {
        tim = malloc((size_t)frames * sizeof *tim);
        if (!tim) sim_fatal("oom (timing buffer)");
      }
      const size_t streamCap = (size_t)frames * 80 + 128;
      char *stream = malloc(streamCap);
      if (!stream) sim_fatal("oom (stream buffer)");
      size_t streamLen = 0;
      char hex[65];
      MlSb rec;
      ml_sb_init(&rec);
      uint16_t *rawKeys = 0;
      const MlInput neutralRow = nullInput();
      MlInput liveRow;
      if (brLive) {
        ml_sb_puts(&rec, "[\n");
        rawKeys = malloc((size_t)frames * sizeof *rawKeys);
        if (!rawKeys) sim_fatal("oom (raw-key sidecar buffer)");
      }
      PlatformInput pin;
      const uint64_t tStart = now_ns();
      for (long f = 0; f < frames; f++) {
        platform_poll(&pin);
        const uint64_t deadline = tStart + (uint64_t)(f + 1) * budgetNs;
        const MlInput *rows[4];
        if (brLive) {
          liveRow = s1_input_row(&pin);
          rows[0] = &liveRow;
          rows[1] = &neutralRow;
          rows[2] = 0;
          rows[3] = 0;
          rec_frame(&rec, f == 0, &liveRow, &neutralRow);
          rawKeys[f] = (uint16_t)((pin.up ? 1u : 0u) | (pin.down ? 2u : 0u) |
                                  (pin.left ? 4u : 0u) |
                                  (pin.right ? 8u : 0u) | (pin.a ? 16u : 0u) |
                                  (pin.b ? 32u : 0u) | (pin.x ? 64u : 0u) |
                                  (pin.y ? 128u : 0u) |
                                  (pin.start ? 256u : 0u) |
                                  (pin.l ? 512u : 0u) | (pin.r ? 1024u : 0u) |
                                  (pin.menu ? 2048u : 0u) |
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
        const bool skip = pace == 1 && t1 > deadline;
        uint64_t t2 = t1, t3 = t1;
        if (!skip) {
          gfx_render_frame(&g_gfx, &G);
          t2 = now_ns();
          if (platform_present(g_gfx.rz.fb) != 0) matchPresentFails++;
          t3 = now_ns();
        } else {
          matchSkips++;
        }
        if (tim) {
          tim[f].sim = t1 - t0;
          tim[f].render = t2 - t1;
          tim[f].present = t3 - t2;
          tim[f].skipped = skip ? 1 : 0;
        }
        const int w = snprintf(stream + streamLen, streamCap - streamLen,
                               "F %ld %s\n", f + 1, hex);
        if (w < 0 || (size_t)w >= streamCap - streamLen) {
          sim_fatal("stream buffer overflow");
        }
        streamLen += (size_t)w;
        if (pace == 1) sleep_until_ns(deadline);
      }
      const uint64_t tEnd = now_ns();
      matchWallMs = (tEnd - tStart) / 1000000ull;
      ranMatch = true;

      const uint32_t total = draws_between(G.rngStateAtReset, G.rng.a);
      const uint32_t outside =
          draws_between(G.rngStateAtReset, G.rngStateAtFrame1);
      {
        const int w = snprintf(stream + streamLen, streamCap - streamLen,
                               "RNG %" PRIu32 " %" PRIu32 "\nSIM OK\n", total,
                               outside);
        if (w < 0 || (size_t)w >= streamCap - streamLen) {
          sim_fatal("stream buffer overflow (trailer)");
        }
        streamLen += (size_t)w;
      }
      if (brVerify) {
        FILE *of = fopen(outPath, "w");
        if (!of) sim_fatal("cannot open --out for writing");
        if (fwrite(stream, 1, streamLen, of) != streamLen) {
          sim_fatal("--out write failed");
        }
        if (fclose(of) != 0) sim_fatal("--out close/flush failed");
        FILE *tf = fopen(timingPath, "w");
        if (!tf) sim_fatal("cannot open --timing for writing");
        for (long f = 0; f < frames; f++) {
          if (fprintf(tf, "%" PRIu64 " %" PRIu64 " %" PRIu64 " %u\n",
                      tim[f].sim, tim[f].render, tim[f].present,
                      (unsigned)tim[f].skipped) < 0) {
            sim_fatal("--timing write failed");
          }
        }
        if (fclose(tf) != 0) sim_fatal("--timing close/flush failed");
      }
      if (brLive) {
        ml_sb_puts(&rec, "\n]\n");
        FILE *rf = fopen(recordPath, "w");
        if (!rf) sim_fatal("cannot open --record-trace for writing");
        if (fwrite(rec.buf, 1, rec.len, rf) != rec.len) {
          sim_fatal("--record-trace write failed");
        }
        if (fclose(rf) != 0) sim_fatal("--record-trace close/flush failed");
        FILE *kf = fopen(keysPath, "w");
        if (!kf) sim_fatal("cannot open --record-keys for writing");
        for (long f = 0; f < frames; f++) {
          if (fprintf(kf, "%04x\n", (unsigned)rawKeys[f]) < 0) {
            sim_fatal("--record-keys write failed");
          }
        }
        if (fclose(kf) != 0) sim_fatal("--record-keys close/flush failed");
      }
      free(tim);
      free(stream);
      free(rawKeys);
      ml_sb_free(&rec);
    }
  }

  // audio teardown BEFORE platform_quit (gfx_app.c ordering)
  PlatformAudioStats astats;
  memset(&astats, 0, sizeof astats);
  uint64_t musOut = 0, musStarves = 0, musRefills = 0;
  if (sndpackPath) {
    ml_snd_sink = 0;
    ml_snd_stop_id_sink = 0;
    if (g_have_music) {
      mus_reader_stop();
      musOut = g_mus_prev_out + g_mix.music.outPos;
      musStarves = g_mus_prev_starves + g_mix.music.starves;
      musRefills = g_mus_prev_refills + g_mix.music.refills;
    }
    platform_audio_stop();
    platform_audio_stats(&astats);
  }
  platform_quit();

  // summaries (grammar LOAD-BEARING — header note)
  fprintf(stderr,
          "foh_dev foh: %ld ticks, %ld transitions, %d shots, %ld render "
          "skips, %ld failed presents, launched=%d\n",
          endTick, transitions, g_nshotbuf, fohSkips, fohPresentFails,
          foh.launched ? 1 : 0);
  if (ranMatch) {
    fprintf(stderr,
            "foh_dev match: %ld frames, %ld render skips, %ld failed "
            "presents, wall %" PRIu64 " ms, pace=%ld budget=%" PRIu64 " ns\n",
            frames, matchSkips, matchPresentFails, matchWallMs, pace,
            budgetNs);
  }
  if (sndpackPath) {
    fprintf(stderr,
            "foh_dev audio: %" PRIu64 " callbacks, %" PRIu64 " underruns, "
            "%" PRIu64 " badlen, %" PRIu64 " voice starts, %" PRIu64
            " voice stops, %" PRIu64 " steals, rate=%d samples=%d "
            "channels=%d\n",
            astats.cbs, astats.underruns, astats.badlen, g_mix.starts,
            g_mix.stops, g_mix.steals, astats.rate, astats.samples,
            astats.channels);
  }
  if (g_have_music) {
    fprintf(stderr,
            "foh_dev music: %" PRIu64 " out frames, %" PRIu64 " starves, %"
            PRIu64 " refills, ring=%u chunk=%u\n",
            musOut, musStarves, musRefills, (unsigned)SND_MUSIC_RING_FRAMES,
            (unsigned)SND_MUSIC_CHUNK_FRAMES);
  }
  ml_sb_free(&g_tr);
  return 0;
}
