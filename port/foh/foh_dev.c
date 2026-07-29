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
//                     seam (s1_input.h chord table over platform_poll;
//                     --tapjump-off-p1 presets the FOH options state
//                     per the Chase-ratified S1 contract).
//                     TWO live shapes, selected by the PRESENCE of a
//                     frame bound (punch-list C6; the C1 --foh-max rule
//                     applied one screen later):
//                       --frames N + --record-trace + --record-keys
//                         = BOUNDED and RECORDED. All three or none.
//                         The evidence shape: check-device-target.sh
//                         [6b]/[6c] (the A2/C1 regression guards).
//                       none of the three
//                         = UNBOUNDED and UNRECORDED. The player's
//                         shape: a match lasts as long as it lasts.
//                         Recording is REFUSED here, not dropped — it
//                         is a frames-sized RAM buffer (~444 B/frame,
//                         MlSb doubles) that cannot be sized without a
//                         bound and would OOM this 57 MB device at
//                         ~11.6 min of play.
//                     Not driven by the mechanical check
//                     this iteration (registered; the acceptance
//                     playthrough + task-14 gate own it).
//
// DIRECT MATCH (M4 task 14, iter 109 — the leg-1 engine's entry):
//   --p1 N --p2 N --p2type N --difficulty N --stage N  (all five, no
//   --flow/--flow-out/--input/--shots-dir/--foh-max/--fb-witness) runs
//   `--bridge verify|state` with NO FOH phase: the five values are
//   written into the SAME FohState fields the SSS-A launch arm writes
//   (foh.c:313-332), `launched` is set, and every line from the bridge
//   onward is the untouched launch seam. This is what lets
//   check-device-fullgame.sh replay all 12 leg-1 goldens through the
//   product binary — including g07/g08 (difficulty 5) and m02
//   (difficulty 9), which the FOH's own 1..4 slider domain
//   (foh.h:79-82) can NEVER select, so no .flow could ever drive them.
//   Direct mode accepts the harness domain 1..9; the FOH machine's
//   slider domain is untouched.
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
//   foh_dev mustrack: from=<tok|none> to=<tok> on=<0|1> pcm=<path>
//    (iter 101, review-99 M2 — ONE line per mus_track_program publish;
//    track-IDENTIFIED evidence: names the programmed track + the
//    transition so device checks bind the audible plane to the PINNED
//    per-track PCM bytes by path and witness the menu→targettest
//    switch at the TLAUNCH seam. NEW line only — every grammar above
//    is byte-unchanged. check-device-target.sh matches it with exact
//    full-line fixed-string greps.)
//
// KEYMAP SSOT (iter 95 H2; hardened iter 97, review-95 M-b): the
// logical-button → FLOW1 letter → device letter-keysym mapping lives
// at ONE compiled definition site — port/gfx/platform_keymap.h —
// consumed by platform_sdl1.c's platform_poll TRANSLATION ARM (the
// device input path) AND by this TU (`--dump-keymap` emits it
// verbatim; parse_buttons drives FLOW1 letters from it); the committed
// frozen copy is port/foh/keymap-frozen.txt. The device check cmp's
// the dump against the frozen file, sha-pins the file, proves the
// compiled table with a perturbed-COPY-build tooth (T12), and
// flow-to-fkscript.js consumes the SAME frozen file at runtime — a
// common-mode injector/backend swap now requires editing the pinned
// frozen mapping, and a poll-arm refactor cannot drift from the dump.
//
// PRESENT WITNESS (iter 95, review-93 H1): `--fb-witness <path>`
// (poll mode + shots only; linux/device builds) reads the KERNEL fb
// page displayed after platform_present at every sampled shot and
// byte-compares it (under the measured pixel transform) against the
// SUBMITTED Raster.fb — a dead/no-op presenter now DIES IN-APP
// instead of passing pre-present RAM shots. Witness rows flush to
// <path> in the strict FBWIT1 grammar the check re-judges. HONEST
// COVERAGE: the witness sees the kernel fb page, not the physical
// panel; only FOH-phase shot presents are sampled — match-phase
// (bridge verify/live) presents stay unwitnessed (task-14 note).
// `--fb-witness-raw <dir>` is the measurement instrument (dumps
// yoffset + all fb pages + the submitted buffer per shot, no
// judgment) used to pin the transform/page policy on a new kernel.
#include <errno.h>
#include <limits.h>
#include <inttypes.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "../gfx/attrib.h" // --attrib row sampler/writer (shared w/ gfx_app.c)
#include "../gfx/pace.h"   // pace_sleep_until_ns: frame pacing (shared w/ gfx_app.c)
#include "../gfx/gfx.h"
#include "../gfx/gfx_target.h" // M4 task 12: the target-mode compositor
#include "../gfx/gfx_vfx.h"
#include "../gfx/platform.h"
#include "../gfx/platform_keymap.h"
#include "../gfx/s1_input.h"
#include "../gfx/snd_mixer.h"
#include "../sim/ml_events.h"
#include "../sim/ml_js.h"
#include "../sim/ml_ser.h"
#include "../sim/sim/sim.h"
#include "foh.h"
#include "foh_pause.h"   // A11/A12: the in-match pause overlay (live only)
#include "foh_persist.h" // M4 task 13: the ONE persistence chokepoint

#define ML_BOOT_DRAWS 465 // the qjs boot pin (oracle/qjs/replay.sh)

void gfx_fatal(const char *what) { sim_fatal(what); }

// task 13: driver-held persisted state. Loaded at boot, applied to the
// machine; saved at the upstream save points (options B-exit,
// gameplaymenu.js:29-33; the finishGame record arm, main.js:1442-1445
// via tdev_finish_hook below). Hermetic checks point MLFK_PERSIST_DIR
// at a fresh dir; the product path (OPK launcher) uses /mnt/mlfk-data.
static FohPersist g_persist;

// --- keymap SSOT (iter 95 H2; single definition site iter 97, M-b) -----------
// The compiled logical-button table is port/gfx/platform_keymap.h's
// kPlatformKeymap — the SAME array platform_sdl1.c's platform_poll
// indexes at runtime. Emitted verbatim by --dump-keymap; frozen
// committed copy: port/foh/keymap-frozen.txt.

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
    int idx = -1;
    for (int k = 0; k < PLATFORM_KEYMAP_ROWS; k++) {
      if (kPlatformKeymap[k].flowLetter == c) { idx = k; break; }
    }
    if (idx < 0) {
      flow_die(path, lineNo, "bad button letter (UDLRABXYSKNQ only)");
    }
    *platform_keymap_field(&in, idx) = true;
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

// The frame-pacing wait now lives in port/gfx/pace.h (M4 task 14
// increment 3e) — one hybrid sleep+spin body shared with gfx_app.c, which
// had a byte-identical copy of the bare loop this replaces.

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

// --- present witness: kernel-fb page readback (iter 95, review-93 H1) ---------
// The FunKey kernel fb is 240x720 = 3 flip pages of RAST_W x RAST_H
// (CLAUDE.md envelope note). The device libSDL's SDL_Flip software-
// blits the 240x240 surface into the fb; FBIOPAN_DISPLAY is REJECTED
// by this kernel (.loop/m3-task4r52-probe-sdlflip.log), so the
// displayed-page policy and the pixel transform were MEASURED with the
// --fb-witness-raw instrument (iter-95 probe, .loop/m4-foh95-probe.log)
// and are PINNED here; any envelope drift = loud death, never a guess.
// MEASURED (iter-95 probe, .loop/m4-foh95-probe2.log + analysis in
// AGENT-LOG iter 95): vinfo = 240x240 vyres=720 bpp=16 ll=480,
// yoffset ALWAYS 0 (FBIOPAN_DISPLAY rejected — no panning), and
// read() on /dev/fb0 exposes ONLY the visible page (offsets past
// 115200 fail) whose bytes equal the submitted RGB565 buffer under
// the IDENTITY transform — uniquely among the 8 dihedral candidates
// (byte-exact vs the archived iter-93 startup frame).
#define FBWIT_DEV_PATH "/dev/fb0"
#define FBWIT_VYRES 720u // declared virtual height (measured; 3 pages)
#define FBWIT_LL 480u    // line_length bytes (measured)
// display (x,y) shows submitted pixel sub[fbwit_sub_index(x,y)]:
//   0 identity · 1 rot180 · 2 rot90cw · 3 rot90ccw · 4 hflip ·
//   5 vflip · 6 transpose · 7 anti-transpose
#define FBWIT_XFORM 0 // measured: identity, unique match

typedef struct {
  char name[33];
  long tick;
  uint32_t yoff;
} FbWitRow;

static const char *g_fbwit_path; // judged witness output (FBWIT1)
static const char *g_fbwit_raw;  // measurement dump dir (instrument)
static FbWitRow g_fbwit_rows[FLOW_SHOT_CAP];
static int g_nfbwit;

#ifdef __linux__
#include <fcntl.h>
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <unistd.h>

static int g_fb_fd = -1;

static void fbwit_open(void) {
  g_fb_fd = open(FBWIT_DEV_PATH, O_RDONLY);
  if (g_fb_fd < 0) sim_fatal("fb witness: cannot open " FBWIT_DEV_PATH);
}

static size_t fbwit_sub_index(int x, int y) {
  switch (FBWIT_XFORM) {
    case 0: return (size_t)y * RAST_W + (size_t)x;
    case 1: return (size_t)(RAST_H - 1 - y) * RAST_W + (size_t)(RAST_W - 1 - x);
    case 2: return (size_t)(RAST_H - 1 - x) * RAST_W + (size_t)y;
    case 3: return (size_t)x * RAST_W + (size_t)(RAST_W - 1 - y);
    case 4: return (size_t)y * RAST_W + (size_t)(RAST_W - 1 - x);
    case 5: return (size_t)(RAST_H - 1 - y) * RAST_W + (size_t)x;
    case 6: return (size_t)x * RAST_W + (size_t)y;
    case 7: return (size_t)(RAST_H - 1 - x) * RAST_W + (size_t)(RAST_W - 1 - y);
    default: sim_fatal("fb witness: bad transform pin"); return 0;
  }
}

// pread that tolerates PARTIAL reads (measured iter-95 probe: this
// kernel's fb driver returns short reads on large requests) — only a
// zero/negative return is fatal (a true read failure, never silence)
static void fbwit_pread_all(void *dst, size_t n, off_t off) {
  uint8_t *p = dst;
  while (n > 0) {
    const ssize_t r = pread(g_fb_fd, p, n, off);
    if (r <= 0) sim_fatal("fb witness: fb read failed");
    p += (size_t)r;
    off += (off_t)r;
    n -= (size_t)r;
  }
}

// read one RAST_W x RAST_H page starting at fb row `pageTop` (stride ll)
static void fbwit_read_page(uint32_t pageTop, uint32_t ll, uint16_t *dst) {
  if (ll == (uint32_t)RAST_W * 2) {
    fbwit_pread_all(dst, (size_t)RAST_W * RAST_H * 2, (off_t)pageTop * ll);
    return;
  }
  for (int y = 0; y < RAST_H; y++) {
    fbwit_pread_all(dst + (size_t)y * RAST_W, (size_t)RAST_W * 2,
                    (off_t)(pageTop + (uint32_t)y) * ll);
  }
}

static void fbwit_raw_dump(const char *name, const char *kind, int idx,
                           const void *buf, size_t n) {
  char p[640];
  if (snprintf(p, sizeof p, "%s/%s.%s%d.bin", g_fbwit_raw, name, kind, idx) >=
      (int)sizeof p) {
    sim_fatal("fb witness: raw path overflow");
  }
  FILE *f = fopen(p, "wb");
  if (!f) sim_fatal("fb witness: cannot open raw dump");
  if (fwrite(buf, 1, n, f) != n) sim_fatal("fb witness: raw dump write failed");
  if (fclose(f) != 0) sim_fatal("fb witness: raw dump close failed");
}

static void fbwit_sample(const char *name, const uint16_t *sub, long tick) {
  struct fb_var_screeninfo vi;
  struct fb_fix_screeninfo fi;
  static uint16_t page[RAST_W * RAST_H];
  if (ioctl(g_fb_fd, FBIOGET_VSCREENINFO, &vi) != 0) {
    sim_fatal("fb witness: FBIOGET_VSCREENINFO failed");
  }
  if (ioctl(g_fb_fd, FBIOGET_FSCREENINFO, &fi) != 0) {
    sim_fatal("fb witness: FBIOGET_FSCREENINFO failed");
  }
  if (g_fbwit_raw) {
    // measurement instrument: record the envelope + EVERY page + the
    // submitted buffer; no judgment here (host analysis pins the
    // transform/page policy from these dumps).
    char p[640];
    if (snprintf(p, sizeof p, "%s/vinfo.txt", g_fbwit_raw) >= (int)sizeof p) {
      sim_fatal("fb witness: raw path overflow");
    }
    FILE *f = fopen(p, "a");
    if (!f) sim_fatal("fb witness: cannot open vinfo.txt");
    if (fprintf(f,
                "shot=%s tick=%ld xres=%u yres=%u vyres=%u bpp=%u yoffset=%u "
                "xoffset=%u ll=%u\n",
                name, tick, vi.xres, vi.yres, vi.yres_virtual,
                vi.bits_per_pixel, vi.yoffset, vi.xoffset, fi.line_length) < 0) {
      sim_fatal("fb witness: vinfo write failed");
    }
    if (fclose(f) != 0) sim_fatal("fb witness: vinfo close failed");
    // sub FIRST (survives even if page reads fail), then every page
    // this kernel lets us read — measured: reads past the visible
    // page fail on this kernel; the instrument records what it can
    // instead of dying (it is diagnostic, never a judge).
    fbwit_raw_dump(name, "sub", 0, sub, (size_t)RAST_W * RAST_H * 2);
    uint32_t npages = vi.yres_virtual / RAST_H;
    if (npages > 4) npages = 4;
    for (uint32_t k = 0; k < npages; k++) {
      bool ok = true;
      for (int y = 0; y < RAST_H && ok; y++) {
        uint8_t *p = (uint8_t *)(page + (size_t)y * RAST_W);
        size_t nleft = (size_t)RAST_W * 2;
        off_t off = (off_t)(k * RAST_H + (uint32_t)y) * fi.line_length;
        while (nleft > 0) {
          const ssize_t r = pread(g_fb_fd, p, nleft, off);
          if (r <= 0) { ok = false; break; }
          p += (size_t)r;
          off += (off_t)r;
          nleft -= (size_t)r;
        }
      }
      if (ok) {
        fbwit_raw_dump(name, "page", (int)k, page, sizeof page);
      } else {
        fprintf(stderr, "foh_dev: fb witness raw: page %u unreadable on "
                        "this kernel (recorded)\n", k);
      }
    }
  }
  if (!g_fbwit_path) return;
  // judged mode: pinned envelope, displayed page from yoffset,
  // transform compare — mismatch is an IN-APP death (a dead/no-op
  // presenter must never pass).
  if (vi.xres != (uint32_t)RAST_W || vi.yres != (uint32_t)RAST_H ||
      vi.bits_per_pixel != 16 || vi.yres_virtual != FBWIT_VYRES ||
      fi.line_length != FBWIT_LL) {
    sim_fatal("fb witness: fb envelope differs from the measured pins");
  }
  // MEASURED PAN-REJECT POLICY PIN (iter 97, review-95 L-b): the
  // iter-95 probe measured yoffset ALWAYS 0 (FBIOPAN_DISPLAY rejected
  // — no panning ever happens on this kernel). Any other value is the
  // H1 wrong-page hazard resurfacing: die naming the drift. If the
  // kernel policy ever changes, re-measure with --fb-witness-raw and
  // re-pin via the reviewed channel.
  if (vi.yoffset != 0) {
    fprintf(stderr,
            "foh_dev: fb witness yoffset=%u != 0 — the measured "
            "pan-reject page policy drifted (re-measure with "
            "--fb-witness-raw, reviewed re-pin)\n",
            vi.yoffset);
    sim_fatal("fb witness: yoffset policy drift");
  }
  fbwit_read_page(vi.yoffset, FBWIT_LL, page);
  for (int y = 0; y < RAST_H; y++) {
    for (int x = 0; x < RAST_W; x++) {
      if (page[(size_t)y * RAST_W + (size_t)x] != sub[fbwit_sub_index(x, y)]) {
        // best-effort diagnostic dump (we are about to die loudly)
        char p[640];
        if (snprintf(p, sizeof p, "%s.fail", g_fbwit_path) < (int)sizeof p) {
          FILE *f = fopen(p, "wb");
          if (f) {
            fwrite(page, 2, (size_t)RAST_W * RAST_H, f);
            fwrite(sub, 2, (size_t)RAST_W * RAST_H, f);
            fclose(f);
          }
        }
        fprintf(stderr,
                "foh_dev: fb witness MISMATCH shot=%s tick=%ld yoff=%u at "
                "(%d,%d)\n",
                name, tick, vi.yoffset, x, y);
        sim_fatal("fb witness: displayed fb page != submitted frame "
                  "(dead/no-op presenter?)");
      }
    }
  }
  if (g_nfbwit >= FLOW_SHOT_CAP) sim_fatal("fb witness: row overflow");
  strcpy(g_fbwit_rows[g_nfbwit].name, name);
  g_fbwit_rows[g_nfbwit].tick = tick;
  g_fbwit_rows[g_nfbwit].yoff = vi.yoffset;
  g_nfbwit++;
}
#else
static void fbwit_open(void) {
  sim_fatal("--fb-witness/--fb-witness-raw require the linux kernel fb "
            "(device build)");
}
static void fbwit_sample(const char *name, const uint16_t *sub, long tick) {
  (void)name;
  (void)sub;
  (void)tick;
  sim_fatal("fb witness sampled on a non-linux build (unreachable)");
}
#endif

// witness rows flush (strict FBWIT1 grammar; the check re-judges it)
static void fbwit_flush(const char *flowId) {
  if (!g_fbwit_path) return;
  FILE *f = fopen(g_fbwit_path, "w");
  if (!f) sim_fatal("cannot open --fb-witness for writing");
  if (fprintf(f, "FBWIT1 flow=%s xform=%d ll=%u vyres=%u\n", flowId,
              (int)FBWIT_XFORM, (unsigned)FBWIT_LL, (unsigned)FBWIT_VYRES) <
      0) {
    sim_fatal("--fb-witness write failed");
  }
  for (int k = 0; k < g_nfbwit; k++) {
    if (fprintf(f, "W %ld %s yoff=%u eq=1\n", g_fbwit_rows[k].tick,
                g_fbwit_rows[k].name, g_fbwit_rows[k].yoff) < 0) {
      sim_fatal("--fb-witness write failed");
    }
  }
  if (fprintf(f, "END shots=%d\n", g_nfbwit) < 0) {
    sim_fatal("--fb-witness write failed");
  }
  if (fclose(f) != 0) sim_fatal("--fb-witness close/flush failed");
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

// Target mode has ONE active port (targetbuilder.js:25 slot 0), so its
// recording is the 1-slot row — the SHAPE load_trace's slot-1..3
// assertion demands. Precisely: the artifact is the recorder's JSON, so
// replaying a live target session through --bridge tverify still needs
// the ordinary trace-to-txt.js conversion (load_trace parses the text
// form); what the 1-slot row buys is that the converted trace is
// ACCEPTED rather than rejected at that assertion.
static void rec_frame_solo(MlSb *sb, bool first, const MlInput *p0) {
  if (!first) ml_sb_puts(sb, ",\n");
  ml_sb_putc(sb, '[');
  rec_input(sb, p0);
  ml_sb_puts(sb, ",null,null,null]");
}

// Raw-key sidecar bit layout (paired with judge-s1-coverage.js). ONE
// definition for both live arms — the expression is load-bearing and
// was previously written out per arm.
static uint16_t pin_bits(const PlatformInput *p) {
  return (uint16_t)((p->up ? 1u : 0u) | (p->down ? 2u : 0u) |
                    (p->left ? 4u : 0u) | (p->right ? 8u : 0u) |
                    (p->a ? 16u : 0u) | (p->b ? 32u : 0u) |
                    (p->x ? 64u : 0u) | (p->y ? 128u : 0u) |
                    (p->start ? 256u : 0u) | (p->l ? 512u : 0u) |
                    (p->r ? 1024u : 0u) | (p->menu ? 2048u : 0u) |
                    (p->quit ? 4096u : 0u));
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
// + targettest (music.js:102-113 playTargetTestLoop — M4 task 12; the
// device app switches to it at the TLAUNCH seam, the REGISTERED
// rewrite delta in foh.c/AGENT-LOG iter 99)
static const char *kMusTok[8] = {"menu",      "battlefield", "ystory",
                                 "pstadium",  "dreamland",   "fdest",
                                 "fountain",  "targettest"};
static MusTrack g_mus_tracks[8];
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
    for (int k = 0; k < 8; k++) {
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
// last-programmed track token for the mustrack transition witness
// (iter 101, review-99 M2; "none" until the first program)
static const char *g_mus_cur_tok = "none";

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
  // SD-swap pressure class: drop the range we just consumed (rationale,
  // measured domain and the start->loop re-read exceptions: snd_mixer.h).
  snd_music_drop_cache(g_mus_file, fileFrame, frames);
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
  snd_music_seq_hint(g_mus_file);
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
  // track-identity event (iter 101, review-99 M2): GRAMMAR IS
  // LOAD-BEARING (PROCESS §3; header note) — a NEW line, emitted at
  // the ONE publish point after the ring is live, so a logged program
  // is a completed program. The named pcm path is the path whose
  // bytes the device check sha-pins (track identity by path join).
  fprintf(stderr, "foh_dev mustrack: from=%s to=%s on=%d pcm=%s\n",
          g_mus_cur_tok, kMusTok[idx], on, t->path);
  g_mus_cur_tok = kMusTok[idx];
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
static int g_tr_full;

// HARD CEILING (review-c1 r1 [M]). g_tr is a doubling buffer and the FOH
// used to be capped at 18000 ticks, which bounded it implicitly. Making the
// play path unbounded removed that bound: a player alternating between two
// menu rows appends a ~32-byte transition per edge, so a long enough session
// grows this without limit and could OOM a device with ~38 MiB available —
// trading the timeout crash for a slower one, which is not a fix.
//
// 1 MiB is ~26k transition lines. Every rig leg writes well under 2 KiB —
// the argument is that trace lines are per-TRANSITION, not per-tick, and no
// leg performs thousands of transitions (the longest leg runs ~1800 ticks:
// check-device-foh.sh derives each leg's --foh-max from its flow's END, so
// f04-nav lands near 1797, NOT the 1400 the live legs pin). So no evidence
// path can reach this and no trace content moves. Past the cap the recorder
// TRUNCATES instead of growing, and says so in the file: a silently short
// evidence sink is worse than a loud one. It must not sim_fatal — killing
// the player's session is precisely the failure being fixed.
//
// The marker is the LAST line by design: g_tr_full also suppresses the
// post-loop END line, so a capped trace is structurally invalid and both
// consumers (judge-foh-trace.js, normalize-foh-trace.js) fail CLOSED on the
// missing END rather than reporting the cap. Accepted, not fixed: no
// evidence leg can reach the cap, and nothing reads the play path's trace
// (mlfk-foh.sh's copyback does not even collect it).
// ponytail: flat cap, not a ring — nothing reads the tail of this file.
#define TR_MAX (1u << 20)

// The marker's own bytes are RESERVED inside TR_MAX (review-c1 grok [L]).
// Appending it without reserving room let the buffer finish at TR_MAX + 34,
// which the doubling allocator would round to 2 MiB — a "hard ceiling" that
// is not one. Reserving makes the worst case land on TR_MAX exactly.
static const char kTrTrunc[] = "TRUNCATED trace buffer cap reached\n";

static void tr_line(const char *fmt, ...) {
  if (g_tr_full) return;
  char buf[256];
  va_list ap;
  va_start(ap, fmt);
  const int w = vsnprintf(buf, sizeof buf, fmt, ap);
  va_end(ap);
  if (w < 0 || w >= (int)sizeof buf) sim_fatal("trace line overflow");
  if (g_tr.len + (size_t)w + 1 > TR_MAX - (sizeof kTrTrunc - 1)) {
    ml_sb_puts(&g_tr, kTrTrunc);
    g_tr_full = 1;
    return;
  }
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

// --- target finish hook (M4 task 12) ----------------------------------------------
// Recorder for tp_finish_hook (target_play.c): fires ONLY through the
// REAL finishGame seam. NO committed flow can reach it (the AGENT-LOG
// iter-99 refutation: every all-broken run is authored-unreachable);
// the mechanical coverage lives in target_finish_probe. When it fires
// (live/acceptance play), the driver renders the end banner and emits
// the `foh_dev tfinish:` stderr line — the device check asserts this
// line is ABSENT on its green legs (count 0).
static int g_tfin_fired;
static int g_tfin_complete;
static long g_tfin_frame;
// Upstream finishGame (main.js:1499-1502) ends with
//   MusicManager.stopWhatisPlaying();
//   setTimeout(function() { endGame(input) }, 2500);
// so the banner is held for 2500 ms of WALL CLOCK with the music
// already stopped. Carried verbatim as a monotonic deadline (not a
// frame count — a skipped frame must not shorten the hold).
#define TFIN_HOLD_NS 2500000000ull
// The live arm may tick a BOUNDED TAIL past --frames so that a finish in
// the closing moments still gets its full wall-clock hold (upstream's
// timeout is wall-clock, not frame-budget). Every per-frame buffer in the
// target loop is sized for frames + this tail, so the tail is explicitly
// accounted for rather than silently truncated. 180 frames = 3 s >= 2.5 s.
#define TFIN_TAIL_FRAMES 180
// endGame (main.js:1013-1015) on the LIVE target path: the START-quit
// arm target_play.c traps by default is real here — the acceptance
// surface the trap was registered against. The hook only raises a flag;
// the driver's loop owns leaving, so the rest of upstream's tick body
// still runs on the quitting frame.
static int g_tquit;
static void tdev_endgame_hook(GameState *g) {
  (void)g;
  g_tquit = 1;
}
static void tdev_finish_hook(GameState *g, bool complete) {
  g_tfin_fired++;
  g_tfin_complete = complete ? 1 : 0;
  g_tfin_frame = (long)g->frame;
  // task 13: the finishGame record arm (main.js:1431-1445) — complete
  // only; operands read from GameState/TP exactly as upstream reads
  // characterSelections[targetPlayer]/targetStagePlaying/matchTimer.
  // Exercised by --tooth-persist-finish (below) and live/acceptance
  // play; never by the committed legs (iter-99 refutation).
  if (complete) {
    const int ch = (int)g->sim.characterSelections[0];
    const int ts = (int)TP.targetStagePlaying;
    if (foh_persist_record_update(&g_persist, ch, ts, g->matchTimer)) {
      foh_persist_save(&g_persist); // :1445 setCookie on improvement
    }
    // finish sounds (newRecord/complete) = the registered task-12
    // acceptance-surface deferral.
  }
}

// --- strict argv integer parse (review-109-1 M3) ------------------------------
// `strtol(s, 0, 10)` is a PERMISSIVE parser: "junk" yields 0 and "1junk"
// yields 1, so an out-of-domain ARGUMENT silently becomes an in-domain
// VALUE and reaches sim_setup_match. That is the whitelist-grammar rule's
// exact failure mode (CLAUDE.md / PROCESS §3) applied to argv. Every
// numeric flag in main's argv loop goes through here instead: whole-string
// decimal only, errno checked, no leading/trailing slop, no whitespace, no
// "+"/"0x"/octal surprises. Returns false on ANY deviation; callers die
// loudly naming the flag.
static bool parse_long_strict(const char *s, long *out) {
  if (!s || !*s) return false;
  const char *d = (*s == '-') ? s + 1 : s;
  if (!*d) return false;
  for (const char *q = d; *q; q++) {
    if (*q < '0' || *q > '9') return false;
  }
  errno = 0;
  char *end = 0;
  const long v = strtol(s, &end, 10);
  if (errno != 0 || !end || *end != '\0') return false;
  *out = v;
  return true;
}

// Same discipline for the one genuinely 64-bit flag (--budget-ns is a
// uint64_t). review-109-2 L8: routing it through `long` would REJECT
// valid budgets above LONG_MAX on the 32-bit ARM target, where long is
// 32 bits — a host/device behavioural split in an argv parser.
static bool parse_u64_strict(const char *s, uint64_t *out) {
  if (!s || !*s) return false;
  for (const char *q = s; *q; q++) {
    if (*q < '0' || *q > '9') return false; // no sign, no slop, no 0x
  }
  errno = 0;
  char *end = 0;
  const unsigned long long v = strtoull(s, &end, 10);
  if (errno != 0 || !end || *end != '\0') return false;
  *out = (uint64_t)v;
  return true;
}

static bool parse_u32_strict(const char *s, uint32_t *out) {
  uint64_t v = 0;
  if (!parse_u64_strict(s, &v)) return false;
  if (v > 0xffffffffULL) return false;
  *out = (uint32_t)v;
  return true;
}

// parse_int_strict — a strict parse that ALSO validates the value fits
// the narrow type before the cast (review-109-2 L8: on LP64
// `--tooth-finish-at 1 4294967296 0 …` parsed fine as a long and then
// narrowed to 0, which passed the 0..4 domain check downstream).
static bool parse_int_strict(const char *s, int *out) {
  long v = 0;
  if (!parse_long_strict(s, &v)) return false;
  if (v < INT_MIN || v > INT_MAX) return false;
  *out = (int)v;
  return true;
}

// --- main --------------------------------------------------------------------------

int main(int argc, char **argv) {
  // keymap SSOT dump arm (iter 95 H2; iter 97 M-b): emits THE compiled
  // platform_keymap.h table — the array the device poll arm indexes —
  // byte-exact against port/foh/keymap-frozen.txt (cmp'd every run).
  if (argc == 2 && strcmp(argv[1], "--dump-keymap") == 0) {
    printf("KEYMAP1\n");
    for (int k = 0; k < PLATFORM_KEYMAP_ROWS; k++) {
      printf("map %s %c %c\n", kPlatformKeymap[k].logical,
             kPlatformKeymap[k].flowLetter, kPlatformKeymap[k].keysym);
    }
    if (fflush(stdout) != 0) sim_fatal("--dump-keymap flush failed");
    return 0;
  }
  // --tooth-persist-finish <char 0-4> <tstage 0-9> <hex16 matchTimer>
  // (M4 task 13; the --tooth-music-wedge precedent): drives the REAL
  // finishGame seam — tp_finish_game -> tdev_finish_hook -> the
  // foh_persist chokepoint -> SD bytes — from a crafted complete
  // TP/GameState. A genuinely completing target run is
  // authored-unreachable in the committed flows (iter-99 refutation)
  // and the live finish is the acceptance surface; this arm is the
  // check's records-write instrument (pre-registered honest-coverage
  // note, AGENT-LOG iter 100).
  if (argc == 5 && strcmp(argv[1], "--tooth-persist-finish") == 0) {
    const char *cs = argv[2], *ts = argv[3], *bs = argv[4];
    if (strlen(cs) != 1 || cs[0] < '0' || cs[0] > '4' ||
        strlen(ts) != 1 || ts[0] < '0' || ts[0] > '9' ||
        strlen(bs) != 16) {
      fprintf(stderr, "foh_dev: --tooth-persist-finish wants <char 0-4> "
                      "<tstage 0-9> <hex16>\n");
      return 1;
    }
    uint64_t bits = parse_hex16(bs); // loud death on non-hex
    double t;
    memcpy(&t, &bits, 8);
    foh_persist_load(&g_persist);
    // craft the COMPLETE finish state (main.js:1431 strict equality)
    G.sim.characterSelections[0] = (double)(cs[0] - '0');
    TP.targetStagePlaying = ts[0] - '0';
    TP.targetCount = 1;
    TP.targetsDestroyed = 1.0;
    G.matchTimer = t;
    G.inp.playing = true;
    tp_finish_hook = tdev_finish_hook;
    tp_finish_game(&G);
    fprintf(stderr, "foh_dev tfinish: complete=%d frame=%ld\n",
            g_tfin_complete, g_tfin_frame);
    return 0;
  }
  const char *flowPath = 0, *inputMode = 0, *flowOut = 0, *shotsDir = 0;
  const char *readyPath = 0, *bridge = 0;
  const char *simdataPath = 0, *bstateOut = 0;
  const char *tracePath = 0, *outPath = 0, *timingPath = 0;
  // --attrib (M4 task 14 increment 3a): the skip-attribution row capture,
  // grammar + sampler owned by port/gfx/attrib.h. Verify-mode only.
  const char *attribPath = 0;
  const char *gfxdataPath = 0, *vfxdataPath = 0, *glyphsPath = 0;
  const char *animDir = 0;
  const char *sndpackPath = 0, *musicManifest = 0;
  const char *recordPath = 0, *keysPath = 0;
  long frames = -1, fohMax = -1;
  // --seed is a uint32 SEED, not a long (review-109-3 L7): routing it
  // through `long` REJECTED valid seeds 2147483648..4294967295 on the
  // 32-bit ARM target, and on LP64 accepted values above UINT32_MAX that
  // then silently WRAPPED at the ml_rng_seed cast. Value and presence are
  // tracked separately, so `-1` is a parse error rather than "absent".
  uint32_t seed = 0;
  bool seedGiven = false;
  long audioSamples = 512;
  bool audioSamplesGiven = false;
  long pace = 1;
  uint64_t budgetNs = 16666667ull;
  bool cpuLive = false, legible = false, tapJumpOffP1 = false;
  // DIRECT-MATCH ENTRY (M4 task 14, iter 109) — see the header block
  // "DIRECT MATCH". -1 == not given; the guard below requires all five
  // together and rejects the whole flow plane alongside them.
  long dmP1 = -1, dmP2 = -1, dmP2Type = -1, dmDiff = -1, dmStage = -1;
  // PRESENCE, tracked separately from VALUE (review-109-1 M3): with
  // value-only opt-in, `--p1 -1` read as "absent" (so a direct flag on a
  // flow invocation was silently ignored) and `--foh-max 0` read as
  // "absent" (so it evaded direct mode's flow-plane refusal). Each bit is
  // set exactly once; a repeated flag is corruption, not a last-wins
  // override.
  unsigned dmSeen = 0; // bits 0..4 == --p1 --p2 --p2type --difficulty --stage
  const unsigned DM_ALL = 0x1fu;
  bool fohMaxGiven = false;
  // Presence, not value: --frames absent is the PLAY path's opt-in to an
  // unbounded match (punch-list C6). Written at exactly one site, below.
  bool framesGiven = false;
  // review-100 M1 witness (--tooth-finish-at <frame> <char> <tstage>
  // <hex16>): fire the crafted tp_finish_game chain MID-FLOW at <frame>
  // (flow mode, no bridge), driving the REAL finishGame -> hook ->
  // chokepoint -> bound-FohState refresh so the SAME-PROCESS return to
  // target-select renders the new record. Same crafted state as the
  // standalone --tooth-persist-finish arm, injected into the live flow.
  long toothFinishFrame = -1;
  int toothFinishChar = -1, toothFinishTstage = -1;
  uint64_t toothFinishBits = 0;
  bool toothFinishGiven = false;
  // ARGN/ARGI: consume the next argv as a strict whole-string decimal into
  // <dst>, or die naming the flag and the offending token (review-109-1
  // M3). DMFLAG additionally records PRESENCE and refuses a repeat.
#define ARGN(dst)                                                            \
  do {                                                                       \
    if (!parse_long_strict(argv[++i], &(dst))) {                             \
      fprintf(stderr, "foh_dev: %s wants a decimal integer, got '%s'\n", a,  \
              argv[i]);                                                      \
      return 1;                                                              \
    }                                                                        \
  } while (0)
#define ARGI(dst)                                                            \
  do {                                                                       \
    if (!parse_int_strict(argv[++i], &(dst))) {                              \
      fprintf(stderr, "foh_dev: %s wants a decimal int, got '%s'\n", a,      \
              argv[i]);                                                      \
      return 1;                                                              \
    }                                                                        \
  } while (0)
#define DMFLAG(bit, dst)                                                     \
  do {                                                                       \
    if (dmSeen & (bit)) {                                                    \
      fprintf(stderr, "foh_dev: %s given more than once\n", a);              \
      return 1;                                                              \
    }                                                                        \
    dmSeen |= (bit);                                                         \
    ARGN(dst);                                                               \
  } while (0)
  for (int i = 1; i < argc; i++) {
    const char *a = argv[i];
    const bool hasV = i + 1 < argc;
    if (strcmp(a, "--flow") == 0 && hasV) flowPath = argv[++i];
    else if (strcmp(a, "--input") == 0 && hasV) inputMode = argv[++i];
    else if (strcmp(a, "--flow-out") == 0 && hasV) flowOut = argv[++i];
    else if (strcmp(a, "--shots-dir") == 0 && hasV) shotsDir = argv[++i];
    else if (strcmp(a, "--ready-file") == 0 && hasV) readyPath = argv[++i];
    else if (strcmp(a, "--foh-max") == 0 && hasV) { ARGN(fohMax); fohMaxGiven = true; }
    else if (strcmp(a, "--bridge") == 0 && hasV) bridge = argv[++i];
    else if (strcmp(a, "--simdata") == 0 && hasV) simdataPath = argv[++i];
    else if (strcmp(a, "--bstate-out") == 0 && hasV) bstateOut = argv[++i];
    else if (strcmp(a, "--seed") == 0 && hasV) {
      if (!parse_u32_strict(argv[++i], &seed)) {
        fprintf(stderr,
                "foh_dev: --seed wants a decimal integer in 0..4294967295, "
                "got '%s'\n", argv[i]);
        return 1;
      }
      seedGiven = true;
    }
    else if (strcmp(a, "--trace") == 0 && hasV) tracePath = argv[++i];
    else if (strcmp(a, "--frames") == 0 && hasV) { ARGN(frames); framesGiven = true; }
    else if (strcmp(a, "--out") == 0 && hasV) outPath = argv[++i];
    else if (strcmp(a, "--timing") == 0 && hasV) timingPath = argv[++i];
    else if (strcmp(a, "--attrib") == 0 && hasV) attribPath = argv[++i];
    else if (strcmp(a, "--gfxdata") == 0 && hasV) gfxdataPath = argv[++i];
    else if (strcmp(a, "--vfxdata") == 0 && hasV) vfxdataPath = argv[++i];
    else if (strcmp(a, "--glyphs") == 0 && hasV) glyphsPath = argv[++i];
    else if (strcmp(a, "--anim-dir") == 0 && hasV) animDir = argv[++i];
    else if (strcmp(a, "--sndpack") == 0 && hasV) sndpackPath = argv[++i];
    else if (strcmp(a, "--music-manifest") == 0 && hasV) musicManifest = argv[++i];
    else if (strcmp(a, "--record-trace") == 0 && hasV) recordPath = argv[++i];
    else if (strcmp(a, "--record-keys") == 0 && hasV) keysPath = argv[++i];
    else if (strcmp(a, "--fb-witness") == 0 && hasV) g_fbwit_path = argv[++i];
    else if (strcmp(a, "--fb-witness-raw") == 0 && hasV) g_fbwit_raw = argv[++i];
    else if (strcmp(a, "--pace") == 0 && hasV) ARGN(pace);
    else if (strcmp(a, "--budget-ns") == 0 && hasV) {
      if (!parse_u64_strict(argv[++i], &budgetNs)) {
        fprintf(stderr,
                "foh_dev: --budget-ns wants a non-negative decimal integer, "
                "got '%s'\n", argv[i]);
        return 1;
      }
    }
    else if (strcmp(a, "--tooth-finish-at") == 0 && i + 4 < argc) {
      ARGN(toothFinishFrame);
      ARGI(toothFinishChar);
      ARGI(toothFinishTstage);
      const char *bs = argv[++i];
      if (strlen(bs) != 16) {
        fprintf(stderr, "foh_dev: --tooth-finish-at wants <frame> <char 0-4> "
                        "<tstage 0-9> <hex16>\n");
        return 1;
      }
      toothFinishBits = parse_hex16(bs); // loud death on non-hex
      toothFinishGiven = true;
    }
    else if (strcmp(a, "--p1") == 0 && hasV) DMFLAG(0x01u, dmP1);
    else if (strcmp(a, "--p2") == 0 && hasV) DMFLAG(0x02u, dmP2);
    else if (strcmp(a, "--p2type") == 0 && hasV) DMFLAG(0x04u, dmP2Type);
    else if (strcmp(a, "--difficulty") == 0 && hasV) DMFLAG(0x08u, dmDiff);
    else if (strcmp(a, "--stage") == 0 && hasV) DMFLAG(0x10u, dmStage);
    else if (strcmp(a, "--cpu-live") == 0) cpuLive = true;
    else if (strcmp(a, "--legible") == 0) legible = true;
    else if (strcmp(a, "--tapjump-off-p1") == 0) tapJumpOffP1 = true;
    else if (strcmp(a, "--audio-samples") == 0 && hasV) {
      ARGN(audioSamples);
      audioSamplesGiven = true;
    } else {
      fprintf(stderr, "foh_dev: bad argument %s\n", a);
      return 1;
    }
  }
#undef DMFLAG
#undef ARGI
#undef ARGN
  const bool inFlow = inputMode && strcmp(inputMode, "flow") == 0;
  const bool inPoll = inputMode && strcmp(inputMode, "poll") == 0;
  const bool brState = bridge && strcmp(bridge, "state") == 0;
  const bool brVerify = bridge && strcmp(bridge, "verify") == 0;
  const bool brLive = bridge && strcmp(bridge, "live") == 0;
  // M4 task 12: the target twins (foh_app.c note) — tverify renders the
  // target-mode compositor live and emits BOTH streams in the exact
  // target_main.c stdout grammar for wrap-target.js.
  const bool brTState = bridge && strcmp(bridge, "tstate") == 0;
  const bool brTVerify = bridge && strcmp(bridge, "tverify") == 0;
  // --- DIRECT MATCH (M4 task 14, iter 109) ------------------------------
  // Any one of the five selection flags opts in; ALL five are then
  // required and the ENTIRE flow plane is refused, so no existing
  // invocation can reach direct mode by accident and direct mode can
  // never half-drive the FOH. The flags do NOT bypass the launch seam —
  // they populate the same FohState fields a menu launch writes (below,
  // right after foh_persist_apply), and every line from `if (bridge)`
  // onward runs BYTE-UNCHANGED.
  //
  // WHY THIS EXISTS (measured, iter 109): the FOH's own CPU-difficulty
  // domain is the upstream SLIDER's 1..4 (foh.h:79-82, css.js:316-329),
  // but the M4 exit gate's leg [1] must replay g07/g08 at difficulty 5
  // and m02 at difficulty 9. Those are STRUCTURALLY UNREACHABLE through
  // any menu flow, so no `.flow` can ever drive them. Direct mode
  // therefore accepts the HARNESS domain 1..9. This widens the CLI
  // surface ONLY; the FOH machine's slider domain is untouched, so the
  // menu-flow evidence (leg [2]) is unaffected.
  // opt-in is by PRESENCE, never by value (review-109-1 M3): `--p1 -1`
  // must enter direct mode and then FAIL its domain, not vanish.
  const bool direct = dmSeen != 0;
  const bool directBad =
      direct && (dmSeen != DM_ALL ||
                 dmP1 < 0 || dmP1 > 4 || dmP2 < 0 || dmP2 > 4 ||
                 dmP2Type < 0 || dmP2Type > 1 || dmDiff < 1 || dmDiff > 9 ||
                 dmStage < 0 || dmStage > 5 ||
                 // the whole flow plane is refused in direct mode, BY
                 // PRESENCE (`--foh-max 0` used to evade a `> 0` test)
                 flowPath || flowOut || inputMode || shotsDir || fohMaxGiven ||
                 g_fbwit_path || g_fbwit_raw || toothFinishGiven ||
                 // VS bridges only (the target plane has its own
                 // char/tstage selection and its own witness)
                 !(brVerify || brState));
  if (directBad ||
      (!direct &&
       (!flowPath || !flowOut || !inputMode || (!inFlow && !inPoll))) ||
      (bridge && !brState && !brVerify && !brLive && !brTState &&
       !brTVerify) ||
      // poll mode is wall-clock by definition and needs its tick budget —
      // EXCEPT the OPK PLAY path, which OMITS `--foh-max` and runs unbounded
      // (the fohLimit derivation below carries the measurement).
      //
      // ABSENCE is the opt-in, never a magic value, and a budget that IS
      // handed over is ALWAYS honoured. Both halves are load-bearing:
      //   - `--foh-max 0` stays rejected as the evasion it always was;
      //   - `--bridge live` alone cannot mean "unbounded", because the device
      //     target rig drives live play WITH a bound on purpose
      //     (check-device-target.sh [6b]/[6c], `--foh-max 1400`) — that leg
      //     IS the punch-list A2 regression guard, and an automated leg whose
      //     navigation fails must still terminate rather than hang the rig.
      // So the play path is distinguished by what it does NOT pass, which is
      // exactly the thing the launcher can be read for.
      (inPoll && (pace != 1 || (fohMaxGiven ? fohMax <= 0 : !brLive))) ||
      (inFlow && (fohMaxGiven || readyPath)) ||
      // bridge modes need the sim data plane + seed + the state witness
      (bridge && (!simdataPath || !seedGiven || !bstateOut)) ||
      (!bridge && (simdataPath || seedGiven || bstateOut)) ||
      // verify needs the golden trace + stream/timing sinks + render data
      ((brVerify || brTVerify) &&
       (!tracePath || frames <= 0 || !outPath || !timingPath ||
        !gfxdataPath || !vfxdataPath || !glyphsPath || !animDir)) ||
      // Live needs render data. The FRAME BOUND and the RECORDING are ONE
      // decision, and it is made by ABSENCE — the C1 --foh-max rule, one
      // screen later (punch-list C6, class instance #3):
      //   --frames GIVEN   => bounded match, recording MANDATORY (unchanged;
      //                       every evidence leg passes a bound, and a bounded
      //                       run is exactly what can be sized in RAM);
      //   --frames OMITTED => unbounded match, recording REFUSED.
      // REFUSED rather than silently dropped, because it cannot be honoured:
      // rec.json is a RAM buffer measured at ~444 B/frame and MlSb doubles, so
      // the peak is ~2x that — it eats this device's ~37 MB MemAvailable after
      // ~41,700 frames (~11.6 min). Accepting "unbounded + recording" would
      // trade the 3:00 exit the player saw for an OOM kill at ~12 min.
      // `--bridge live` alone must NOT mean unbounded: check-device-target.sh
      // [6b]/[6c] drive live play WITH a bound on purpose and [6b] IS the A2
      // regression guard. So, exactly as with --foh-max, the play path is
      // distinguished by what it does NOT pass, and a bound that IS handed
      // over is always honoured (`--frames 0` stays the rejection it was).
      (brLive && ((framesGiven ? (frames <= 0 || !recordPath || !keysPath)
                               : (recordPath || keysPath)) ||
                  !gfxdataPath || !vfxdataPath || !glyphsPath || !animDir ||
                  pace != 1 || cpuLive || !inPoll)) ||
      (!brVerify && !brLive && !brTVerify &&
       (tracePath || frames > 0 || outPath || timingPath || gfxdataPath ||
        vfxdataPath || glyphsPath || animDir || legible || attribPath)) ||
      // --attrib rides the --timing buffer's lifetime exactly: only the
      // brVerify arm allocates `tim`, runs the loop at the frame-start
      // sample site and flushes post-run (brTVerify is a DIFFERENT loop,
      // brLive writes no timing). Anywhere else the flag would silently
      // produce nothing, so it is rejected instead.
      (attribPath && !brVerify) ||
      (!brLive && (recordPath || keysPath || tapJumpOffP1)) ||
      // live consumes NO golden trace and writes NO evidence sink: it
      // records instead. Accepting these silently would let a malformed
      // invocation look successful while the requested input was never
      // read and the requested output never written.
      (brLive && (tracePath || outPath || timingPath)) ||
      (cpuLive && !brVerify) ||
      (pace != 0 && pace != 1) || budgetNs == 0 ||
      (audioSamplesGiven && !sndpackPath) ||
      audioSamples <= 0 || audioSamples > 65535 ||
      // the present witness samples SHOTS on the DEVICE input path only
      ((g_fbwit_path || g_fbwit_raw) && (!inPoll || !shotsDir)) ||
      // review-100 M1 witness: flow mode only, no bridge, in-domain
      (toothFinishGiven && (!inFlow || bridge || toothFinishFrame <= 0 ||
                            toothFinishChar < 0 || toothFinishChar > 4 ||
                            toothFinishTstage < 0 || toothFinishTstage > 9)) ||
      (musicManifest && !sndpackPath)) {
    fprintf(stderr,
            "usage: foh_dev --flow f.flow --input flow|poll --flow-out t.txt"
            " [--shots-dir D] [--ready-file f]"
            " [--foh-max N (poll; required EXCEPT with --bridge live, where"
            " omitting it means an unbounded FOH — the play path)]"
            " [--pace 0|1] [--budget-ns N]"
            " [--bridge state|verify|live --simdata s --seed N"
            " --bstate-out b]"
            " [verify: --trace t --frames N --out o --timing tim"
            " --gfxdata g --vfxdata v --glyphs gl --anim-dir D [--legible]"
            " [--cpu-live] [--attrib a.txt]]"
            " [live: --gfxdata ... [--legible] [--tapjump-off-p1]"
            " (bounded+recorded: --frames N --record-trace t.json"
            " --record-keys k.txt — ALL THREE or NONE; omitting all three"
            " means an unbounded, unrecorded match: the play path)]"
            " [--sndpack p [--audio-samples N]] [--music-manifest m.txt]"
            " [--fb-witness w.txt [--fb-witness-raw D]]"
            " [flow: --tooth-finish-at F C S hex16 (M1 witness)]"
            " | DIRECT MATCH (no flow): --p1 0-4 --p2 0-4 --p2type 0-1"
            " --difficulty 1-9 --stage 0-5 --bridge verify|state"
            " --simdata s --seed N --bstate-out b [verify: --trace ..."
            " --frames N --out o --timing tim --gfxdata ... [--cpu-live]]"
            " | --dump-keymap\n");
    return 1;
  }
  if (frames > 1000000L) sim_fatal("foh_dev: --frames exceeds the buffer cap");
  if (fohMax > 1000000L) sim_fatal("foh_dev: --foh-max exceeds the tick cap");

  char flowId[64];
  if (direct) {
    // no flow to load; the id names the mode in the trace/witness
    memcpy(flowId, "direct", 7);
  } else {
  load_flow(flowPath);

  // flow id = basename minus .flow (foh_app.c verbatim)
  const char *base = strrchr(flowPath, '/');
  base = base ? base + 1 : flowPath;
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
  } // end !direct (flow load + id derivation)

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
  if (firstInputFrame == 0) firstInputFrame = g_flow_frames + 1;

  if (bridge) {
    sim_boot_page(&G);
    sim_data_load(simdataPath);
    sim_data_register();
  }

  if (platform_init("meleelight-foh") != 0) {
    sim_fatal("platform_init failed");
  }
  if (g_fbwit_path || g_fbwit_raw) fbwit_open();

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

  // Backdrop caches built BEFORE the READY handshake and only when there is
  // a FOH phase to serve: `direct` mode runs zero FOH ticks (fohLimit below),
  // so warming there would dirty ~564 KiB of backdrop/LUT pages for nothing,
  // right before the memory-sensitive full-game match loop. Warming ahead of
  // READY also keeps the marker's meaning honest — the rig starts its
  // wall-clock fk schedule on it, so nothing costly may follow it.
  if (!direct) foh_render_warm(&g_rz);

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
  // task 13: load + apply the persisted plane through the chokepoint
  // (loud reset-to-defaults on missing/corrupt; foh_persist.h). The
  // brLive tapJumpOffP1 preset below intentionally stays AFTER the
  // apply — the S1 contract preset wins on the live path.
  foh_persist_load(&g_persist);
  foh_persist_apply(&g_persist, &foh);
  if (direct) {
    // DIRECT MATCH: write the SAME FohState fields a menu launch writes
    // (foh.c:313-332's SSS-A arm), then mark it launched. Everything
    // downstream — the seed + 465 boot draws, sim_setup_match, the
    // options plane, the music switch, the BRIDGE-STATE witness and the
    // paced match loop — is the untouched launch seam. The persisted
    // options plane applied just above still wins, exactly as it does
    // for a menu launch.
    foh.p1Char = (int)dmP1;
    foh.p2Char = (int)dmP2;
    foh.p2Type = (int)dmP2Type;
    foh.difficulty = (int)dmDiff;
    foh.stageSel = (int)dmStage;
    foh.sssCursor = (int)dmStage;
    foh.targetMode = false;
    foh.launched = true;
  }
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
  // direct mode has no FOH phase at all: 0 ticks, so the loop body below
  // never executes (chosen over wrapping 140 lines in an `if` — the
  // existing FOH code stays byte-identical for every flow caller).
  // THE PLAY PATH IS UNBOUNDED (punch-list C1, measured 2026-07-27).
  //
  // MEASURED DEFECT this kills: the OPK play launcher handed the FOH an
  // EVIDENCE bound (`--foh-max 18000`). At `--pace 1 --budget-ns 16666667`
  // that is exactly 300.0 s of menus, after which this loop simply RAN OUT,
  // fell through with `foh.launched == false`, and the bridge arm below
  // returned 4. To the player that is the app vanishing to the frontend —
  // reported as "the game crashed while I sat on CHARACTER SELECT". It is
  // NOT CSS-specific and NOT a fault: reproduced with zero input on the
  // title screen, the app exited between t=294 s and t=304 s with the same
  // rc 4 and the same stderr line the owner's SD log carries, while
  // MemAvailable sat flat at 37.76 MB for the whole 300 s (no leak, no OOM,
  // no counter wrap — those suspects are falsified, not assumed).
  //
  // CLASS (HARD RULE 8): an evidence-run bound governing the PLAY path.
  // Second recorded instance — punch-list A2 was the first (`--bridge live`
  // refused TARGET launches, so Target Test exited rc 4 at the launch seam;
  // same rc, same class, fixed the same way: live serves the player, the
  // evidence bridges keep their bounds). A menu has no business timing out,
  // and nothing here grows per tick to justify one: the FOH loop allocates
  // nothing, and its only RAM writer (tr_line -> g_tr) fires on transitions,
  // never per tick — which is exactly what the flat 18000-tick MemAvailable
  // trace measures.
  //
  // THIRD instance, and it was the very next screen: the MATCH loop carried
  // the same shape of bound (`--frames 10800` = exactly 180 s), so any match
  // past 3:00 ended and dropped the player back to the frontend. Closed as
  // punch-list C6, in the ORDER this comment used to say was required —
  // recording made opt-in FIRST, then the bound dropped. (The claim this
  // comment previously made about `attrib_alloc` was WRONG and is corrected
  // here for the record: `--attrib` is argv-restricted to `--bridge verify`
  // (see the validation above), so attrib_alloc never ran on the play path
  // at all. The frames-proportional buffers that DID run there were the
  // rec/rawKeys recording — ~444 B/frame, real — and the `--out` stream
  // buffer, which live wrote every frame and then freed UNREAD.)
  //
  // SCOPE, stated exactly: the ONLY invocation whose behaviour changes is
  // poll + `--bridge live` + NO `--foh-max` — which argv validation above
  // used to reject outright. Every invocation that passes a budget, live or
  // not, is bit-for-bit what it was, so no evidence leg moves.
  //
  // LONG_MAX - 1, i.e. ~414 days of continuous menu dwell at 60 fps. Stated
  // plainly rather than called "infinite": it is not a wall any player
  // reaches, and the deadline arithmetic below stays exact (t * budgetNs
  // peaks near 3.6e16 ns, far inside uint64).
  //
  // The `- 1` is not cosmetic (review-c1 r1 [L]): `t <= LONG_MAX` would run
  // the body at t == LONG_MAX and then `t++` is signed-overflow UB, so the
  // one path that is supposed to end cleanly is the one the compiler is free
  // to miscompile. Capping one short means the final increment lands exactly
  // on LONG_MAX, the test fails, and the loop exits defined.
  const long fohLimit =
      direct ? 0
             : (inPoll ? ((brLive && !fohMaxGiven) ? LONG_MAX - 1 : fohMax)
                       : g_flow_frames);
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
    // review-100 M1 witness: fire the crafted finishGame chain MID-FLOW
    // at the requested frame (the standalone --tooth-persist-finish
    // state, injected into the live flow). Drives the REAL
    // tp_finish_game -> tdev_finish_hook -> foh_persist chokepoint;
    // the chokepoint's record-time bound-FohState refresh (foh_persist
    // M1) then makes the later same-process tss-record shot render the
    // NEW record. tp_finish_game touches only G/TP, never foh — the
    // only effect on the FOH plane is the record refresh, so the flow
    // trace stays identical to a plain p02 run (hermeticity).
    if (toothFinishGiven && t == toothFinishFrame) {
      double tt;
      memcpy(&tt, &toothFinishBits, 8);
      G.sim.characterSelections[0] = (double)toothFinishChar;
      TP.targetStagePlaying = toothFinishTstage;
      TP.targetCount = 1;
      TP.targetsDestroyed = 1.0;
      G.matchTimer = tt;
      G.inp.playing = true;
      tp_finish_hook = tdev_finish_hook;
      tp_finish_game(&G);
    }
    bool launched = false;
    for (int e = 0; e < foh.nev; e++) {
      const FohEvent *ev = &foh.ev[e];
      if (ev->kind == FOH_EV_TRANS) {
        transitions++;
        tr_line("T %ld %s %s %s", t, ev->from, ev->to, ev->cause);
        // task 13: the upstream options save point (gameplaymenu.js:
        // 29-33 — setCookie per key on the B-exit). Committed device
        // legs run with MLFK_PERSIST_DIR on tmpfs, so this write never
        // sits on the SD inside the paced loop; the product path's SD
        // save here mirrors upstream's cookie write at the same UI
        // moment (registered, PORTABILITY row).
        if (strcmp(ev->from, "options-gameplay") == 0 &&
            strcmp(ev->cause, "b") == 0) {
          foh_persist_collect(&g_persist, &foh);
          foh_persist_save(&g_persist);
        }
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
        if (foh.targetMode) {
          // the target launch record (foh.h TLAUNCH note; iter 99)
          tr_line("TLAUNCH %ld char=%d tstage=%d", t, foh.p1Char,
                  foh.tssStage);
        } else {
          tr_line("LAUNCH %ld p1=%d p2=%d p2type=%d difficulty=%d stage=%d "
                  "turbo=%d lcancel=%d tapjump=%d,%d,%d,%d versus=0",
                  t, foh.p1Char, foh.p2Char, foh.p2Type, foh.difficulty,
                  foh.stageSel, foh.turbo, foh.lCancelType, foh.tapJumpOff[0],
                  foh.tapJumpOff[1], foh.tapJumpOff[2], foh.tapJumpOff[3]);
        }
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
    // CANONICAL SHOT PHASE (foh_look_canonical, foh_render.c): the pending
    // shot ROW decides. Rows before the flow's first non-neutral input are
    // tick-indexed — the same tick on both targets, so they keep judging the
    // live animated frame; every later row is a q-marker shot whose device
    // tick is wall-clock-derived, so it renders at the resting look phase.
    // The predicate is the row's frame, identical in flow and poll mode.
    int pendShot = -1;
    if (shotDue) pendShot = (inPoll && qEdge) ? markerIdx : shotIdx;
    const bool canonShot =
        shotDue && g_shots[pendShot].frame >= firstInputFrame;

    const uint64_t tNow = now_ns();
    const bool skip = pace == 1 && tNow > deadline && !shotDue;
    if (!skip) {
      if (canonShot) {
        FohState look = foh;
        foh_look_canonical(&look);
        foh_render(&look, &g_rz);
      } else {
        foh_render(&foh, &g_rz);
      }
      if (platform_present(g_rz.fb) != 0) fohPresentFails++;
    } else {
      fohSkips++;
    }
    if (shotDue) {
      if (inPoll) {
        const char *shotName;
        if (qEdge) {
          shotName = g_shots[markerIdx].name;
          markerIdx++;
        } else {
          shotName = g_shots[shotIdx].name;
          shotIdx++;
        }
        shot_capture(shotName, g_rz.fb, t);
        // present witness: the shot forced render+present this tick —
        // verify the DISPLAYED kernel-fb page carries this frame
        if (g_fbwit_path || g_fbwit_raw) fbwit_sample(shotName, g_rz.fb, t);
      } else {
        // flow mode may have several SHOT rows on one tick frame? the
        // loader enforces strictly increasing frames — exactly one.
        shot_capture(g_shots[shotIdx].name, g_rz.fb, t);
        shotIdx++;
      }
    }
    endTick = t;
    if (launched && inPoll) break; // device: END right after LAUNCH
    if (pace == 1) pace_sleep_until_ns(deadline);
  }
  tr_line("END %ld transitions=%ld", inPoll ? endTick : g_flow_frames,
          transitions);
  (void)fohTicks;
  (void)firstMarkerIdx;

  // flush the FOH artifacts now (between phases; never inside a paced
  // loop — the match loop below is the paced surface that matters)
  if (flowOut) { // NULL only in direct mode (no FOH phase to record)
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
  fbwit_flush(flowId);

  long matchSkips = 0, matchPresentFails = 0;
  // A11/A12: process exit code. 0 = normal end (the OPK launcher returns
  // the player to the frontend); FOH_PAUSE_RC_MENU = "quit to menu", which
  // the launcher answers by running the app again — the app boots into the
  // FOH, so a relaunch IS the menus. Only the live play path can set it.
  int quitRc = 0;
  long matchFrames = 0; // frames ACTUALLY ticked (a live target
                        // quit/finish exit runs fewer than `frames`)
  uint64_t matchWallMs = 0;
  bool ranMatch = false;

  if (bridge) {
    if (!foh.launched) {
      fprintf(stderr, "foh_dev: --bridge given but the flow never "
                      "launched\n");
      return 4;
    }
    // --bridge live is THE PLAY PATH: the launch kind is chosen by the
    // PLAYER at the real menus, not by a pinned flow, so live serves
    // BOTH kinds and dispatches on foh.targetMode (below). The OPK
    // launcher passes one fixed argv for a session in which the player
    // may pick VS *or* TARGET TEST — refusing one of them here is what
    // made "Target Test" quit the app at the launch seam (punch-list
    // A2; the app exited rc 4 the instant TLAUNCH happened).
    const bool tgtLive = brLive && foh.targetMode;
    // C6: the live arms record only when a --frames bound sized the
    // buffers. argv above makes recordPath, keysPath and framesGiven
    // stand or fall together under --bridge live, so this ONE predicate
    // governs every rec/rawKeys site in both match arms.
    const bool recording = recordPath != 0;
    // A11/A12 — THE pause-overlay install site, and the only one. The hook
    // is NULL by default (foh_pause.c), so every evidence bridge (state/
    // verify/tstate/tverify) leaves the overlay branches in both match
    // loops structurally unreachable: a flow- or trace-fed run cannot enter
    // the overlay, exactly as tp_endgame_hook keeps its live-only arm out
    // of the evidence legs (below). Not a runtime flag anyone can flip.
    if (brLive) foh_pause_hook = foh_pause_open;
    // launch-kind cross-guards (fail closed; the --cpu-live class).
    // The EVIDENCE bridges keep refusing: each carries a pinned witness
    // grammar plus a frozen expectation set for exactly one launch kind.
    if ((brState || brVerify) && foh.targetMode) {
      fprintf(stderr, "foh_dev: --bridge %s but the flow performed a "
                      "TARGET launch (cross-guard)\n", bridge);
      return 4;
    }
    if ((brTState || brTVerify) && !foh.targetMode) {
      fprintf(stderr, "foh_dev: --bridge %s but the flow performed a VS "
                      "launch (cross-guard)\n", bridge);
      return 4;
    }
    if (brVerify && cpuLive != (foh.p2Type == 1)) {
      fprintf(stderr, "foh_dev: --cpu-live must match the FOH P2 type "
                      "(cross-guard)\n");
      return 4;
    }
    if (brVerify || brTVerify) load_trace(tracePath);

    if (brTState || brTVerify || tgtLive) {
      // --- the TARGET launch bridge (target_main.c boot parity;
      // foh_app.c tstate/tverify twin with the LIVE render plane) ------
      ml_active_rng = &G.rng;
      ml_rng_seed(&G.rng, seed);
      for (int k = 0; k < ML_BOOT_DRAWS; k++) (void)ml_rng_next(&G.rng);
      G.rngStateAtReset = G.rng.a;
      if (brTVerify || tgtLive) {
        gfx_data_load(&g_gfx.data, gfxdataPath);
        gfx_load_anim(&g_gfx, animDir, foh.p1Char);
        gfx_vfx_load(vfxdataPath);
        gfx_glyphs_load(glyphsPath);
        // peek startTargetGame's background draw from a COPY (the
        // gfx_app.c class; tp_setup_target consumes the real draw)
        MlRng peek = G.rng;
        const int backgroundType = (int)js_round(ml_rng_next(&peek));
        gfx_target_init(&g_gfx, foh.tssStage, backgroundType);
        g_gfx.legibility = legible ? 1 : 0;
        gfx_vfx_install(&g_gfx); // BEFORE setup (boot entrance/start vfx)
      }
      tp_finish_hook = tdev_finish_hook;
      // The START-quit arm is REAL only on the live PLAY path; every
      // evidence bridge keeps target_play.c's loud trap (its goldens
      // never press START, so a START there is a genuine domain break).
      if (tgtLive) tp_endgame_hook = tdev_endgame_hook;
      // THE BRIDGE POINT: char + tstage from the FOH state, never CLI.
      tp_setup_target(&G, foh.p1Char, foh.tssStage);
      // tp_setup_target installs the harness cookie-domain gameSettings
      // DEFAULTS (target_play.c:319-325 zeroes lCancelType/turbo and all
      // four tapJumpOff slots). On the PLAY path those are the player's
      // settings, so the FOH options plane is reapplied exactly as the
      // VS arm does after sim_setup_match — otherwise the launcher's
      // mandatory --tapjump-off-p1 (the S1 contract) is silently
      // dropped and target controls differ from VS controls.
      // EVIDENCE arms are left on the defaults their frozen
      // TBRIDGE-STATE witness and target goldens were recorded against.
      if (tgtLive) {
        G.sim.turbo = foh.turbo != 0;
        G.sim.lCancelType = foh.lCancelType;
        for (int i = 0; i < 4; i++) G.sim.tapJumpOff[i] = foh.tapJumpOff[i];
      }
      G.rngStateAtFrame1 = G.rng.a;
      // TBRIDGE-STATE witness (read back from GameState + the target
      // module — foh_app.c's exact emission; frozen-cmp'd by the check)
      {
        FILE *bf = fopen(bstateOut, "w");
        if (!bf) sim_fatal("cannot open --bstate-out for writing");
        if (fprintf(bf,
                    "TBRIDGE-STATE char=%d tstage=%d gamemode=%d "
                    "targets=%d playing=%d starting=%d stocks=%d\n",
                    (int)G.sim.characterSelections[0],
                    (int)TP.targetStagePlaying, (int)G.sim.gameMode,
                    TP.targetCount, G.inp.playing ? 1 : 0,
                    G.starting ? 1 : 0, (int)G.sim.player[0].stocks) < 0) {
          sim_fatal("--bstate-out write failed");
        }
        if (tgtLive &&
            fprintf(bf,
                    "TBRIDGE-OPTS turbo=%d lcancel=%d tapjump=%d,%d,%d,%d\n",
                    G.sim.turbo ? 1 : 0, (int)G.sim.lCancelType,
                    (int)G.sim.tapJumpOff[0], (int)G.sim.tapJumpOff[1],
                    (int)G.sim.tapJumpOff[2], (int)G.sim.tapJumpOff[3]) < 0) {
          sim_fatal("--bstate-out options write failed");
        }
        if (fclose(bf) != 0) sim_fatal("--bstate-out close/flush failed");
      }
      if (brTVerify || tgtLive) {
        // MUSIC SWITCH at the launch seam: the targettest track
        // (music.js:102-113; the REGISTERED delta — upstream switches
        // at the menu.js:82 entry, the device app at TLAUNCH so the SD
        // ring prefill never runs inside the paced FOH loop).
        if (g_have_music) {
          mus_reader_stop();
          mus_track_program(7, 1); // kMusTok[7] = targettest
          mus_reader_start();
        }
        typedef struct { uint64_t sim, render, present; uint8_t skipped; } TFrameNs;
        // brTVerify writes --timing and --out; the live PLAY arm writes
        // neither (brLive's contract, argv-enforced) and RECORDS instead.
        TFrameNs *tim = 0;
        if (brTVerify) {
          tim = malloc((size_t)frames * sizeof *tim);
          if (!tim) sim_fatal("oom (timing buffer)");
        }
        // live may tick a bounded post-finish tail; buffers cover it.
        // No --frames => no bound (C6): LONG_MAX - 1, never LONG_MAX, so the
        // loop's own `f++` cannot sign-overflow (the C1 --foh-max form).
        // ~414 days at 60 fps on the 32-bit target. Every exit from an
        // unbounded target loop is a break arm, and each one sets both
        // `ticked` and `framesRun`, so the loopMax seeding below stays honest.
        const long loopMax =
            framesGiven ? frames + (tgtLive ? TFIN_TAIL_FRAMES : 0)
                        : LONG_MAX - 1;
        // The --out stream is a frames-SIZED RAM buffer that only --bridge
        // tverify flushes. It is gated on framesGiven — NOT on brTVerify —
        // and that distinction is load-bearing (review-c6 r1 HIGH): the
        // BOUNDED LIVE legs (check-device-target.sh [6b]/[6c], the A2/C1
        // regression guards) pass --frames, and they allocated and wrote
        // this buffer every frame before C6. Gating on the bridge mode would
        // have silently removed that per-frame snprintf from those legs,
        // changing their memory pressure and timing slack — an evidence leg
        // moving under a play-path fix. framesGiven reproduces the old
        // behaviour EXACTLY everywhere a bound exists (i.e. everywhere the
        // old code was reachable) and skips it only where it cannot be
        // sized at all: the unbounded play path.
        const size_t streamCap = framesGiven ? (size_t)loopMax * 160 + 160 : 0;
        char *stream = 0;
        if (framesGiven) {
          stream = malloc(streamCap);
          if (!stream) sim_fatal("oom (stream buffer)");
        }
        size_t streamLen = 0;
        char hex[65], thex[65];
        // ml_sb_init allocates 256 bytes up front. Pre-C6 it ran
        // UNCONDITIONALLY here, so it is gated on framesGiven — the SAME
        // predicate as the stream buffer, and for the same reason
        // (review-c6 r1 HIGH, restated by r3): every bounded mode, evidence
        // AND bounded-live, must keep the exact heap behaviour it had.
        // `recording` would have been too narrow — verify/tverify are
        // bounded but do not record, and they allocated this before.
        // Only the unbounded play path allocates zero recorder bytes;
        // ml_sb_free is safe on the zeroed struct (free(NULL)).
        MlSb rec = {0};
        if (framesGiven) ml_sb_init(&rec);
        uint16_t *rawKeys = 0;
        MlInput liveRow;
        if (tgtLive && recording) {
          ml_sb_puts(&rec, "[\n");
          rawKeys = malloc((size_t)loopMax * sizeof *rawKeys);
          if (!rawKeys) sim_fatal("oom (raw-key sidecar buffer)");
        }
        // Initialised to loopMax, NOT frames: if the loop runs to the very
        // end (a hold still pending at the tail's last frame) no break arm
        // executes, and loopMax rows have been appended to BOTH artifacts.
        // Seeding with `frames` would under-report `ticked` and desync
        // rec.json from keys.txt. Identical on the brTVerify arm, where
        // loopMax == frames.
        // Set when the pause overlay asked to leave (review-a11-3 L2): the
        // player has already chosen to go, so the completion HOLD must not
        // keep them for up to 2.5 s more and the post-loop banner present
        // must not put the overlay frame back on screen.
        int pauseQuit = 0;
        long framesRun = loopMax;  // RECORDED rows (the replay prefix)
        long ticked = loopMax;     // frames actually TICKED (>= framesRun)
        uint64_t tfinDeadline = 0; // set once, at the finish frame
        size_t recMark = 0;        // rec.len before this frame's row
        PlatformInput pin, prevPause;
        memset(&prevPause, 0, sizeof prevPause);
        if (foh_pause_hook) platform_poll(&prevPause); // VS arm's note
        uint64_t tStart = now_ns(); // NOT const: the overlay shifts it
        for (long f = 0; f < loopMax; f++) {
          platform_poll(&pin); // live: THE input; tverify: backend pump
          // A12: MENU opens the same overlay here. START does NOT — in
          // target mode it is upstream's own endGame quit (main.js:1013
          // -1015) via tp_endgame_hook, which is faithful and already
          // works; the overlay must not shadow it. NULL hook on tverify.
          // `!pin.start` (review-a11-3 L1): on a SIMULTANEOUS MENU+START
          // edge the overlay would open first and its release drain would
          // then swallow the START, shadowing that faithful arm. START wins.
          if (foh_pause_hook && pin.menu && !prevPause.menu && !pin.start) {
            uint64_t pausedNs = 0;
            const FohPauseResult pr =
                foh_pause_hook(&g_gfx.rz, &pausedNs, &matchPresentFails);
            tStart += pausedNs;
            if (pr != FOH_PAUSE_RESUME) {
              quitRc = (pr == FOH_PAUSE_QUIT_MENU) ? FOH_PAUSE_RC_MENU : 0;
              pauseQuit = 1;
              tfinDeadline = 0; // disarm any completion hold in progress
              ticked = f;       // frame f never ticked...
              framesRun = f;    // ...and never entered either artifact
              break;
            }
            platform_poll(&pin);
          }
          prevPause = pin;
          const uint64_t deadline = tStart + (uint64_t)(f + 1) * budgetNs;
          const MlInput *row0;
          if (tgtLive) {
            liveRow = s1_input_row(&pin);
            row0 = &liveRow;
            // slot 0 is the ONLY active port in target mode, so the
            // recording carries [p0,null,null,null] — exactly the shape
            // load_trace + the slot-1..3 assertion below accept, i.e. a
            // recorded live target session replays through --bridge
            // tverify unmodified.
            if (recording) {
              recMark = rec.len;
              rec_frame_solo(&rec, f == 0, &liveRow);
              rawKeys[f] = pin_bits(&pin);
            }
          } else {
            const long idx = f < g_trace_len - 1 ? f : g_trace_len - 1;
            const TraceRow *row = &g_trace[idx];
            if (row->present[1] || row->present[2] || row->present[3]) {
              sim_fatal("target trace with a non-null slot 1-3 row");
            }
            row0 = row->present[0] ? &row->in[0] : 0;
          }
          G.frame = f + 1;
          const uint64_t t0 = now_ns();
          tp_game_tick_target(&G, row0);
          sim_frame_hash(&G, hex);
          tp_target_frame_hash(&G, thex);
          const uint64_t t1 = now_ns();
          const bool skip = pace == 1 && t1 > deadline;
          uint64_t t2 = t1, t3 = t1;
          if (!skip) {
            gfx_target_frame(&g_gfx, &G, &TP);
            // Post-finish the sim body is skipped (target_play.h
            // :991/:1041-1044) and gfx_target_frame would redraw the
            // scene over the result, so the banner TEXT is composited
            // over every frame of the hold — the player SEES COMPLETE!/
            // FAILURE, not a frozen stage. gfx_target_banner is NOT used
            // here: it re-renders the whole frame first (gfx_target.c:
            // 206), which would double the render cost of every held
            // frame against a 16.67 ms budget.
            if (tgtLive && g_tfin_fired) {
              gfx_target_banner_text(&g_gfx.rz, g_tfin_complete);
            }
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
          if (stream) {
            const int w = snprintf(stream + streamLen, streamCap - streamLen,
                                   "F %ld %s\nT %ld %s\n", f + 1, hex, f + 1,
                                   thex);
            if (w < 0 || (size_t)w >= streamCap - streamLen) {
              sim_fatal("stream buffer overflow");
            }
            streamLen += (size_t)w;
          }
          if (pace == 1) pace_sleep_until_ns(deadline);
          // LIVE EXITS. Both are the app-boundary form of upstream's
          // "leave the target match": this build's match phase is
          // terminal (the VS live arm exits after its frame bound too),
          // so ending the run returns the player to the frontend rather
          // than to the FOH menus. A bounded deviation for the DRIVER to
          // register under BLOCKERS in docs/AGENT-LOG.md — this file must
          // not claim its own registration. ALSO unapplied: upstream
          // endGame (main.js:1372-1396) resets gameEnd/lost-stock/phantom/
          // article state, sets playing=false and changeGamemode(4); the
          // hook applies none of it, unobservable ONLY because the driver
          // leaves the match on the next statement. Re-entering the menus
          // needs the FOH/match outer loop no play arm has.
          //   (1) START while playing -> upstream endGame (main.js:1013
          //       -1015) via tp_endgame_hook. The terminating row is
          //       ROLLED BACK out of both artifacts: replaying it would
          //       re-enter the START edge with tp_endgame_hook NULL,
          //       which is target_play.c's loud trap by design. What is
          //       kept is exactly the prefix that replays.
          //   (2) the target game finished -> upstream stops the music
          //       and leaves 2500 ms later (main.js:1499-1502).
          if (tgtLive && g_tquit) {
            if (recording) rec.len = recMark; // roll the START row back out
            ticked = f + 1;    // this frame ran: ticked, rendered, paced
            framesRun = f;     // ...but it is NOT part of the replayable
                               // prefix (replaying it re-enters the START
                               // edge, which traps with a NULL hook)
            break;
          }
          if (tgtLive && g_tfin_fired && tfinDeadline == 0) {
            // MusicManager.stopWhatisPlaying() — the lock-bracketed flag
            // flip (the menu-music precedent); mus_reader_stop() joins a
            // thread and must never run inside the paced loop.
            if (g_have_music) {
              platform_audio_lock();
              g_mix.music.on = 0;
              platform_audio_unlock();
              // muting the mixer alone would leave mus_reader_main doing
              // SD refills for the whole hold; upstream's
              // stopWhatisPlaying() stops the source. Setting the quit
              // atomic is lock-free and safe here — the JOIN stays in
              // teardown's mus_reader_stop(), never inside the paced loop.
              atomic_store_explicit(&g_mus_quit, 1, memory_order_release);
            }
            tfinDeadline = now_ns() + TFIN_HOLD_NS;
          }
          if (tgtLive && tfinDeadline != 0 && now_ns() >= tfinDeadline) {
            ticked = f + 1;
            framesRun = f + 1;
            break;
          }
          // the ORDINARY --frames bound for live: the tail above exists
          // only to finish a hold already in progress. `framesGiven` — with
          // no bound this arm is the C6 symptom itself (the match ends and
          // the player is dropped back to the frontend mid-play); unbounded,
          // a target match ends on its own finish hold or on the pause
          // overlay's QUIT, which is what the player asked for.
          if (framesGiven && tgtLive && tfinDeadline == 0 && f + 1 >= frames) {
            ticked = f + 1;
            framesRun = f + 1;
            break;
          }
        }
        // The hold is WALL-CLOCK (upstream's setTimeout), but the frame
        // tail above is frame-COUNTED. Under catch-up pace_sleep_until_ns
        // is a no-op and post-finish ticks are nearly free, so the tail
        // can be spent in well under 2.5 s. Finish the hold here: the
        // banner is already presented and post-finish ticks are inert
        // (target_play.h :991/:1041-1044), so waiting is equivalent to
        // ticking and cannot truncate the banner the player sees.
        while (tgtLive && tfinDeadline != 0 && now_ns() < tfinDeadline) {
          struct timespec hs;
          hs.tv_sec = 0;
          hs.tv_nsec = 4000000L; // 4 ms
          nanosleep(&hs, 0);
        }
        const uint64_t tEnd = now_ns();
        matchWallMs = (tEnd - tStart) / 1000000ull;
        ranMatch = true;
        matchFrames = ticked;
        const uint32_t total = draws_between(G.rngStateAtReset, G.rng.a);
        const uint32_t outside =
            draws_between(G.rngStateAtReset, G.rngStateAtFrame1);
        if (stream) {
          const int w = snprintf(stream + streamLen, streamCap - streamLen,
                                 "RNG %" PRIu32 " %" PRIu32 "\nTFIN %d %s\n"
                                 "SIM OK\n",
                                 total, outside, (int)TP.targetsDestroyed,
                                 TP.endTargetGame ? "T" : "F");
          if (w < 0 || (size_t)w >= streamCap - streamLen) {
            sim_fatal("stream buffer overflow (trailer)");
          }
          streamLen += (size_t)w;
        }
        if (brTVerify) {
          FILE *of = fopen(outPath, "w");
          if (!of) sim_fatal("cannot open --out for writing");
          if (fwrite(stream, 1, streamLen, of) != streamLen) {
            sim_fatal("--out write failed");
          }
          if (fclose(of) != 0) sim_fatal("--out close/flush failed");
          FILE *tf2 = fopen(timingPath, "w");
          if (!tf2) sim_fatal("cannot open --timing for writing");
          for (long f = 0; f < frames; f++) {
            if (fprintf(tf2, "%" PRIu64 " %" PRIu64 " %" PRIu64 " %u\n",
                        tim[f].sim, tim[f].render, tim[f].present,
                        (unsigned)tim[f].skipped) < 0) {
              sim_fatal("--timing write failed");
            }
          }
          if (fclose(tf2) != 0) sim_fatal("--timing close/flush failed");
        }
        if (tgtLive && recording) {
          // the brLive mandatory-record contract, target shape
          if (framesRun == 0) {
            // START on the very first frame: the rolled-back prefix is
            // EMPTY. `[]` is the honest capture (nothing was played), but
            // it is not a replayable trace — load_trace rejects an empty
            // one — so say so instead of implying otherwise.
            fprintf(stderr, "foh_dev: live target quit on frame 1 — the "
                            "recorded capture is EMPTY (not replayable)\n");
          }
          ml_sb_puts(&rec, "\n]\n");
          FILE *rf = fopen(recordPath, "w");
          if (!rf) sim_fatal("cannot open --record-trace for writing");
          if (fwrite(rec.buf, 1, rec.len, rf) != rec.len) {
            sim_fatal("--record-trace write failed");
          }
          if (fclose(rf) != 0) sim_fatal("--record-trace close/flush failed");
          FILE *kf = fopen(keysPath, "w");
          if (!kf) sim_fatal("cannot open --record-keys for writing");
          for (long f = 0; f < framesRun; f++) {
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
        if (g_tfin_fired) {
          // the finish seam fired mid-replay (live/acceptance surface;
          // never on the committed legs — header note): show the
          // COMPLETE!/FAILURE banner + declare it on stderr.
          // live already composited the banner over every held frame
          if (!tgtLive) gfx_target_banner(&g_gfx, &G, &TP, g_tfin_complete);
          // pauseQuit: the raster still holds the OVERLAY frame, so
          // re-presenting would flash the pause menu back onto a screen
          // the player has already chosen to leave (review-a11-3 L2).
          if (!pauseQuit && platform_present(g_gfx.rz.fb) != 0) {
            matchPresentFails++;
          }
          fprintf(stderr, "foh_dev tfinish: complete=%d frame=%ld\n",
                  g_tfin_complete, g_tfin_frame);
        }
      }
      goto bridge_done;
    }

    // seed + boot draws ONLY at the launch seam (foh_app.c verbatim)
    ml_active_rng = &G.rng;
    ml_rng_seed(&G.rng, seed);
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
      // WARM-UP (menu-launch parity). The first platform_poll and the
      // first gfx_render_frame after gfx_init are COLD: measured on
      // device (iter-109 run 3, port/foh/build/device-fullgame/
      // g01.dev-tim.txt) the first poll cost ~8.5 ms and the first
      // render 43.16 ms against a 16.67 ms budget, which cost the direct
      // entry THREE frame-skips at match start — frame 1 skipped because
      // poll+sim alone passed the deadline, frames 3-4 skipped catching
      // up from the 43 ms render. A menu-driven launch reaches the match
      // through a long rendering FOH phase; the direct entry runs zero
      // FOH ticks (fohLimit above), so it primes the same paths here.
      //
      // NO PRE-MATCH GAME FRAME IS EVER PRESENTED (review-109-10 H1).
      // Presenting the warm render would push a pre-setup frame — empty
      // stage, 08:00 on the clock, no players, no entrance vfx — onto the
      // physical display, covered by no screenshot, no checksum and no
      // counter. The warm RENDER therefore goes to g_gfx.rz and is never
      // shown; frame 1's own render overwrites it.
      //
      // WHICH PATHS ARE ACTUALLY COLD, measured rather than assumed: on a
      // POLLED flow launch the FOH phase polls and presents every
      // non-skipped tick (the fohPresentFails site above), so its poll and
      // present are already warm at the launch seam — only gfx_render_frame
      // is cold, because the FOH phase renders menus through foh_render.c
      // and never calls it. The direct entry runs zero FOH ticks, so all
      // three are cold there. The warm-up is split to match: the render
      // pass runs on both entries, the poll+present pair only on the direct
      // entry. That leaves the flow path's visual behaviour byte-unchanged.
      //
      // KNOWN GAP, deliberately not closed here (review-109-12 L2): the
      // FOH phase polls only in --input poll mode, so a scripted
      // (non-polled) flow launch still meets the match loop with a cold
      // first poll. That path is outside the authoritative domain — every
      // device and live entry the gate drives is either direct or polled —
      // so widening the condition to `direct || !inPoll` was left to the
      // driver rather than added after the last review round.
      //
      // The direct entry's present warms on g_gfx.rz.fb BEFORE any render
      // — zero-initialised, i.e. black — and on that entry nothing has
      // ever been presented, so it puts no new content on the display
      // (review-109-10 H1's "explicit black framebuffer for direct
      // entry"). Its outcome is NOT discarded (review-109-10 L4): a
      // failure counts into matchPresentFails, which the gate requires to
      // be 0, so a warm-up present that fails fails the run rather than
      // hiding behind a later success.
      //
      // Frame 1 must meet ITS OWN 16.67 ms deadline, not merely avoid a
      // skip (review-109-11 H1: the skip predicate samples t1 after
      // sim+hash, so an over-budget frame 1 would record skipped=0 and
      // slip past a judge that only asserts p99 and skips). With poll,
      // present and render all warm, frame 1 is sim + render + present.
      // Its sim tick is cold by construction on EVERY entry path — it is
      // the first tick there is, ~8.15 ms measured against ~2.06 ms
      // steady — so frame 1 is expected at roughly 8.2 + 5.3 + 1.1 ms.
      // That is the measured prediction this warm-up is judged on
      // (.loop/m4-t109r3-prereg.md R7); it is not conceded slack.
      //
      // ONE pass (review-109-10 L5): one full frame first-touches both
      // raster planes and executes every pass exactly once. The earlier
      // "3 flip pages" justification was withdrawn — the device measures
      // yoffset always 0 with FBIOPAN_DISPLAY rejected, so extra presents
      // would not have touched extra pages, and nothing measured supports
      // a larger count.
      //
      // STATE NEUTRALITY IS BY RE-RUNNING THE CONSTRUCTORS, not by
      // per-field bookkeeping (review-109-10 M3 found the field an
      // earlier by-placement argument missed: gfx_render.c:299 leaves
      // g->fg2LineWidth at 4.0 — canvas-state emulation that persists
      // across frames — while a fresh gfx_init leaves the canvas default
      // 1.0, and gfx_vfx_install does not touch it). The warm-up is
      // therefore bracketed: gfx_init below re-establishes every Gfx
      // field it owns (stab, camera, backgroundType, fg2LineWidth), and
      // gfx_vfx_install re-establishes every module-static render plane
      // (vfx queue, render-local RNG, vm_reset, gfx_overlay_reset,
      // gfx_bg_reset). SCOPE OF THAT CLAIM, narrowed to what was actually
      // audited (review-109-12 L2): the two constructors own every
      // PERSISTENT, OUTPUT-AFFECTING field the warm render writes. They do
      // NOT own Raster's pixel/ink/clip/scratch state, which is instead
      // neutral because the first real frame's rast_clear overwrites every
      // Raster field before any read (ink/path state exits each pass
      // cleanly, the circle cache is deterministic) — audited, not
      // guaranteed by construction. A future pass that adds persistent
      // output-affecting state OUTSIDE both constructors would need this
      // re-audited; this comment is not a standing promise that it cannot.
      //
      // The warm render runs BEFORE sim_setup_match, so playerPresent is
      // 0 and the player/article passes are skipped by their own guards:
      // this primes the rasteriser, both planes, the stage/background/
      // overlay passes and the glyph atlas, but NOT the per-player anim
      // raster (same raster primitives, so the residual is expected
      // small — a measured prediction, .loop/m4-t109r3-prereg.md R2).
      //
      // The warm poll's PlatformInput is discarded. platform_poll is
      // level-based (SDL_GetKeyState) so no key state is lost; the only
      // edge it consumes is SDL_QUIT, which the FunKey has no window
      // manager to send (review-109-10 L4, dispositioned).
      if (direct) { // the FOH phase warms these on a flow launch
        PlatformInput warm;
        platform_poll(&warm);
        if (platform_present(g_gfx.rz.fb) != 0) matchPresentFails++;
      }
      gfx_render_frame(&g_gfx, &G); // offscreen: never presented
      gfx_init(&g_gfx, foh.stageSel, backgroundType); // undo the warm render
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
      // --attrib buffer (M4 task 14 increment 3a): frames+1 rows, one
      // per frame START plus one tail row. RAM-only until the post-run
      // flush — zero file I/O inside the paced loop, same discipline as
      // --timing (and allocated alongside it, before tStart, so no
      // allocation ever lands inside the paced window).
      AttribRow *attrib = 0;
      if (attribPath) {
        attrib = attrib_alloc(frames); // allocates AND pre-faults
        if (!attrib) sim_fatal("oom (attrib buffer)");
      }
      // framesGiven, not brVerify — see the target arm's note (review-c6 r1
      // HIGH): a bound is exactly the condition under which the pre-C6 code
      // allocated and wrote this buffer, so gating on it leaves every
      // bounded run (evidence AND bounded-live) bit-for-bit as it was.
      const size_t streamCap = framesGiven ? (size_t)frames * 80 + 128 : 0;
      char *stream = 0;
      if (framesGiven) {
        stream = malloc(streamCap);
        if (!stream) sim_fatal("oom (stream buffer)");
      }
      size_t streamLen = 0;
      char hex[65];
      MlSb rec = {0}; // see the target arm's note (review-c6 r1/r3)
      if (framesGiven) ml_sb_init(&rec);
      uint16_t *rawKeys = 0;
      const MlInput neutralRow = nullInput();
      MlInput liveRow;
      if (brLive && recording) {
        ml_sb_puts(&rec, "[\n");
        rawKeys = malloc((size_t)frames * sizeof *rawKeys);
        if (!rawKeys) sim_fatal("oom (raw-key sidecar buffer)");
      }
      // No --frames => no bound (C6). LONG_MAX - 1, never LONG_MAX, so the
      // loop's own `f++` cannot sign-overflow — the C1 --foh-max form,
      // ~414 days at 60 fps on the 32-bit target. An unbounded VS match ends
      // the way the player ends it: the pause overlay's RESUME/QUIT.
      const long matchMax = framesGiven ? frames : LONG_MAX - 1;
      // Frames actually ticked. Only the live arm can leave early (a pause-
      // overlay quit); every evidence arm runs the full bound.
      long vsRan = matchMax;
      PlatformInput pin, prevPause;
      // Seed the pause edge detector from the CURRENT button state: START
      // or MENU still held from the FOH launch must not open the overlay
      // on frame 0. platform_poll is level-based, so this loses nothing.
      // ONLY when the hook is installed (review-a11-1 M): an evidence
      // bridge must not gain even one extra backend event pump.
      memset(&prevPause, 0, sizeof prevPause);
      if (foh_pause_hook) platform_poll(&prevPause);
      // NOT const: the overlay freezes wall clock, and every deadline below
      // is derived from tStart. Without the shift, resuming would leave the
      // whole rest of the match "late" and the catch-up arm would skip
      // every render (matchSkips == frames, a black screen).
      uint64_t tStart = now_ns();
      for (long f = 0; f < matchMax; f++) {
        // FIRST statement of the body, i.e. OUTSIDE the t0..t3 brackets
        // below: the instrument can consume pacing slack but can never
        // inflate a number judge-render-timing.js computes.
        if (attrib) attrib_sample(&attrib[f]);
        platform_poll(&pin);
        // A11/A12: START or MENU opens the modal pause overlay. NULL hook
        // on every evidence bridge, so this is dead code for flow/trace-fed
        // runs by construction (install site above).
        if (foh_pause_hook &&
            ((pin.start && !prevPause.start) || (pin.menu && !prevPause.menu))) {
          uint64_t pausedNs = 0;
          const FohPauseResult pr =
              foh_pause_hook(&g_gfx.rz, &pausedNs, &matchPresentFails);
          tStart += pausedNs; // the pace epoch owes the player that time
          if (pr != FOH_PAUSE_RESUME) {
            quitRc = (pr == FOH_PAUSE_QUIT_MENU) ? FOH_PAUSE_RC_MENU : 0;
            vsRan = f; // rows 0..f-1 are recorded; frame f never ticked
            break;
          }
          platform_poll(&pin); // post-overlay state, not the stale edge
        }
        prevPause = pin;
        const uint64_t deadline = tStart + (uint64_t)(f + 1) * budgetNs;
        const MlInput *rows[4];
        if (brLive) {
          // START is the PORT-LOCAL pause button (foh_pause.h), so it never
          // reaches the sim's own pause machine. Feeding it would be strictly
          // harmful: sim_tick.c has no `playing || frameByFrame` gate, so a
          // START press could only reach interpretInputs' pastOffset arm
          // (ai_input.h:198-204) and FREEZE the 20-field input history until
          // START is pressed a second time, with no pause to compensate it.
          // Masked, `playing` stays true and pastOffset stays 1 — i.e. exactly
          // an upstream match in which nobody pressed pause, which is what an
          // out-of-sim frozen loop is equivalent to.
          //
          // Masked on ONE local copy feeding BOTH the S1 row and the raw-key
          // sidecar, so judge-s1-coverage.js:240-245's pairing invariant
          // (sidecar bit8 == the recorded row's `.s`, "copied verbatim from
          // the pin into the S1 row") still holds frame-by-frame. `pin`
          // itself keeps the true button state — the pause edge detector
          // above reads it. MENU is not masked: s1_input_row never reads it
          // and no pairing asserts it, so its raw bit stays honest.
          PlatformInput gpin = pin;
          gpin.start = false;
          liveRow = s1_input_row(&gpin);
          rows[0] = &liveRow;
          rows[1] = &neutralRow;
          rows[2] = 0;
          rows[3] = 0;
          if (recording) {
            rec_frame(&rec, f == 0, &liveRow, &neutralRow);
            rawKeys[f] = pin_bits(&gpin); // gpin, not pin — see the mask note
          }
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
        if (stream) {
          const int w = snprintf(stream + streamLen, streamCap - streamLen,
                                 "F %ld %s\n", f + 1, hex);
          if (w < 0 || (size_t)w >= streamCap - streamLen) {
            sim_fatal("stream buffer overflow");
          }
          streamLen += (size_t)w;
        }
        if (pace == 1) pace_sleep_until_ns(deadline);
      }
      const uint64_t tEnd = now_ns();
      if (attrib) attrib_sample(&attrib[frames]); // tail row
      matchWallMs = (tEnd - tStart) / 1000000ull;
      ranMatch = true;
      matchFrames = vsRan;

      const uint32_t total = draws_between(G.rngStateAtReset, G.rng.a);
      const uint32_t outside =
          draws_between(G.rngStateAtReset, G.rngStateAtFrame1);
      if (stream) {
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
        // --attrib flush: frames+1 rows through the ONE writer of the
        // pinned grammar (port/gfx/attrib.h), paired with the UNCHANGED
        // correlate-skips.js.
        if (attribPath) {
          switch (attrib_flush(attribPath, attrib, frames)) {
            case 0: break;
            case -1: sim_fatal("cannot open --attrib for writing");
            case -2: sim_fatal("--attrib write failed");
            default: sim_fatal("--attrib close/flush failed");
          }
        }
      }
      free(attrib);
      if (brLive && recording) {
        // Same honesty as the target arm: a pause-overlay quit on frame 1
        // leaves an EMPTY prefix, which load_trace rejects. Say so.
        if (vsRan == 0) {
          fprintf(stderr, "foh_dev: live VS quit on frame 1 — the recorded "
                          "capture is EMPTY (not replayable)\n");
        }
        ml_sb_puts(&rec, "\n]\n");
        FILE *rf = fopen(recordPath, "w");
        if (!rf) sim_fatal("cannot open --record-trace for writing");
        if (fwrite(rec.buf, 1, rec.len, rf) != rec.len) {
          sim_fatal("--record-trace write failed");
        }
        if (fclose(rf) != 0) sim_fatal("--record-trace close/flush failed");
        FILE *kf = fopen(keysPath, "w");
        if (!kf) sim_fatal("cannot open --record-keys for writing");
        // vsRan, not frames: a pause-overlay quit leaves the tail of
        // rawKeys[] uninitialised, and rec.json already stopped there.
        for (long f = 0; f < vsRan; f++) {
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
  bridge_done:;
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
            matchFrames, matchSkips, matchPresentFails, matchWallMs, pace,
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
  return quitRc;
}
