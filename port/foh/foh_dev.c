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
//   (foh.c:666-679), `launched` is set, and every line from the bridge
//   onward is the untouched launch seam. This is what lets
//   check-device-fullgame.sh replay all 12 leg-1 goldens through the
//   product binary — including g07/g08 (difficulty 5) and m02
//   (difficulty 9), which the FOH's own 1..4 slider domain
//   (foh.h:73-77) can NEVER select, so no .flow could ever drive them.
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

// --- match trace loading (sim_main.c:50-159, duplicated verbatim) ------------

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

// --- draw counting (sim_main.c:161-173) ---------------------------------------

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
// FLOW_SHOT_CAP rows for the FOH-phase shots (the flow loader refuses a
// flow with more) PLUS ONE for the VS-finish banner's own present, which
// is sampled from the C18 block below (R6, 2026-07-31, review-r6-r4 [LOW]).
// Without the +1 a flow using all 16 legal SHOT rows and then reaching a
// clock expiry would die on row overflow — i.e. the added witness would
// have quietly shrunk the legal shot count by one. Exactly ONE extra row is
// enough because the finish sample is guarded on `matchesRun == 0` (see the
// C18 block below): the witness file is scoped to the first match, the same
// scope the FOH artifact flush at :2348 already uses.
#define FBWIT_ROW_CAP (FLOW_SHOT_CAP + 1)
static FbWitRow g_fbwit_rows[FBWIT_ROW_CAP];
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
  if (g_nfbwit >= FBWIT_ROW_CAP) sim_fatal("fb witness: row overflow");
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

// --- live-session input recording (gfx_app.c:352-402 duplicated verbatim) -----

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
// snd_event_menu, NOT snd_event (A40, snd_mixer.h header note): the sim
// plane mints play ids off ml_events.c's counter, which only
// ml_sound_play() advances. A menu click that consumed a mixer play id
// desynced the two, and from then on every sim-stored id (marth's
// player.shieldBreakerID, FURAFURA's furaLoopID) named a voice that did
// not exist — so the looping shieldbreakercharge never stopped.
static void foh_snd(const char *name) {
  if (!g_have_audio) return;
  platform_audio_lock();
  snd_event_menu(&g_mix, name);
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
// Upstream finishGame (main.js:1498-1502) ends with
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

// --- match-exit verdict (punch-list C18 + C19 + B4 + A19) -------------------
//
// WHERE THE PLAYER LANDS when a match ends. Upstream has no results screen
// at all: finishGame (main.js:1420-1502) paints a banner, holds 2500 ms, then
// endGame (main.js:1372-1418) picks the destination BY GAMEMODE —
// gameMode 3 (VS) -> changeGamemode(2) = the CSS (main.js:1386-1388);
// gameMode 5 (targets) -> changeGamemode(7) = target select, which is the
// `targetTesting == false` side of that arm's branch (main.js:1389-1395) and
// the only reachable side in this port, whose sole writer of the flag pins it
// false (target_play.c:288 — see the target exit note below). So every
// natural end, and upstream's own A+L+R+Start quit combo (main.js:738-747),
// lands back on the SELECT screen for that match kind — never on the
// frontend, and never on the title.
//
// That is one destination problem wearing four hats, so it is one enum. The
// FunKey has no A+L+R+Start, so the pause overlay's QUIT TO VS SCREEN entry
// (C19) is simply this port's affordance for that upstream combo.
//
// Only MEX_OS leaves the process; the rest are handled in-process by the
// outer loop below (A19), which is what lets the CSS keep the player's
// selections exactly as upstream's endGame does.
typedef enum {
  MEX_NONE = 0, // match still running
  MEX_CSS,      // endGame from VS      -> character select
  MEX_TSS,      // endGame from targets -> target select
  MEX_TITLE,    // pause overlay QUIT TO MENU
  MEX_OS        // pause overlay QUIT TO OS, or SDL_QUIT
} MatchExit;
static MatchExit g_mexit;

// finishGame fired in a VS match (sim.h's ml_sim_finish_hook). Set from the
// matchTimer arm (main.js:348) or any DEAD* final-death arm (DEAD*.js:39);
// the driver's loop owns leaving, so upstream's tick body still finishes on
// the frame that fired, exactly as tdev_endgame_hook does for targets.
static int g_vsFinish;
// Which kind of match the live arm is running, so the one finishGame seam can
// route the way upstream's single finishGame does (main.js:1430 branches on
// gameMode 5 || 8 vs everything else).
static int g_tgt_live;
static void tdev_finish_game_hook(void) {
  if (g_tgt_live) {
    // TARGET mode: isFinalDeath() is unconditionally true there
    // (actionStateShortcuts.js:153), so falling off the stage IS the end of
    // the run, and upstream routes it through the SAME finishGame — whose
    // target branch paints "Failure" precisely because not every target was
    // destroyed (main.js:1461-1462). tp_finish_game is that body, already
    // real since iter 99; the guard keeps a death during the finish hold
    // from starting a second one.
    if (!g_tfin_fired) tp_finish_game(&G);
  } else {
    g_vsFinish = 1;
  }
}

// finishGame's VS banner (main.js:1470-1483: "Time!" when the clock ran out,
// else "Game!"). Same face, scale and centring recipe as
// gfx_target_banner_text (gfx_target.c:192-199) so the two match ends look
// like one game — this port's registered text surface is the FOH's own face,
// so no new gfx plane is introduced for it.
static void tdev_vs_banner_text(Raster *rz, int timeUp) {
  const RastCol white = {255, 255, 255, 256};
  const char *text = timeUp ? "TIME!" : "GAME!";
  const int scale = 4;
  const int tw = foh_text_width(text, scale);
  foh_text(rz, 120 - tw / 2, 120 - (7 * scale) / 2, scale, text, white);
}

// endGame (main.js:1372-1418) — the state reset upstream runs when a match
// ends, however it ended: finishGame's 2500 ms timeout, the A+L+R+Start quit
// combo, or target mode's START. Only the parts this port can OBSERVE are
// ported; every omission is NAMED here rather than silently dropped:
//
//   * clearScreen / drawStage (:1380-1381) and changeGamemode's drawCSSInit
//     are canvas ops with no analogue — the FOH owns its own render and
//     repaints whichever screen it lands on from FohState.
//   * positionPlayersInCSS (:1404) and the per-player inCSS / face / WAIT /
//     timer writes (:1412-1415) set sim player state this port's CSS never
//     reads (it draws its own dolls from FohState). ponytail: unobservable,
//     and the next sim_setup_match — upstream's own startGame — overwrites
//     all of it before anything could read it. The furaloop.stop that shares
//     that loop (:1407-1408) is NOT in this class and IS ported below: it is
//     a looping VOICE, not player state.
//   * lostStockQueue (:1374) is a documented render-only no-op here
//     (physics.h:44).
//   * pause / frameAdvance / findingPlayers (:1396-1403) are the browser
//     lobby's own plane: `pause` is upstream's per-port START latch, which
//     this port replaces outright with the release drain at the return site
//     (a one-port device cannot express a 4x2 latch); frameAdvance is the
//     debug single-step tool, absent here; findingPlayers drives the
//     gamepad-hotplug scan, and this port has one fixed input device.
//
// The music duck is NOT restored here (`changeVolume(MusicManager,
// masterVolume[1], 1)`, :1378): its only producer is interpretPause's 0.3x
// duck (main.js:853/856), and this port never applies that duck — the pause
// overlay freezes the whole loop instead — so there is nothing to undo. If a
// duck is ever added, this line has to come with it.
// --- A31: the rebinder's ONE integration point on the play path -----------
//
// Every MATCH-loop poll goes through here instead of platform_poll, so the
// whole binary downstream of it — the pause edge detector, the FunKey system
// menu edge, the S1 chord resolver, the raw-key sidecar — reads ONE input
// plane: the LOGICAL one. Remapping at the seam rather than at the chord
// table is what keeps the feature invisible to s1_input.h, to the three
// frozen chord tables and to every pinned S1 sweep: under the fresh-install
// identity binding this is a plain struct copy.
//
// It is deliberately NOT used by the FOH menu loop (:2151/:2173). Menu
// navigation must stay on the physical buttons — a player who has moved A
// somewhere else still has to be able to reach this screen and move it back.
// Port 0: the Controls screen edits port 0 only (ctl_style.h), and slot 0 is
// the only human port this device can have (fix_plan A32/A33).
static void poll_bound(PlatformInput *in) {
  platform_poll(in);
  ctl_bind_apply(0, in, in); // in-place is documented safe (ctl_style.c)
}

// foh_persist_save chokepoint. The control plane lives in ctl_style.c, not
// in FohState, so foh_persist_collect cannot reach it (foh_persist.h:125
// assigns these two stamps to the FOH). Every save goes through here so a
// Controls-screen change can never be written by one path and dropped by
// another.
static void tdev_persist_save(void) {
  g_persist.ctlStyle = (int)ctl_style_get();
  g_persist.modOnR = ctl_mod_on_r_get() ? 1 : 0;
  // A31: the binding table lives in the same TU and follows the same rule.
  for (int k = 0; k < CTL_BIND_PORTS; k++) {
    for (int i = 0; i < (int)CTL_BTN_COUNT; i++) {
      g_persist.bind[k][i] = ctl_bind_get(k, i);
    }
  }
  foh_persist_save(&g_persist);
}
// ...and its mirror. A save STAMPS the control plane out of ctl_style.c, so a
// load that does not INSTALL it back leaves the two disagreeing, and the very
// next save writes the process defaults over the player's persisted choice.
// review-mexit-r2 Medium found exactly that on --tooth-persist-finish, which
// loaded and then saved on an improved record; making load a chokepoint too
// closes the class rather than that one site (PROCESS/HARD RULE 8).
static void tdev_persist_load(void) {
  foh_persist_load(&g_persist);
  // ctl_style_set REFUSES an out-of-domain value, so a corrupt/older record
  // lands on the default rather than an unrepresentable style.
  ctl_style_set(g_persist.ctlStyle);
  ctl_mod_on_r_set(g_persist.modOnR != 0);
  // A31: ctl_bind_set_row REFUSES a row that is not a permutation, so a
  // corrupt/older record leaves the identity binding in place rather than a
  // table with an action missing from it.
  for (int k = 0; k < CTL_BIND_PORTS; k++) ctl_bind_set_row(k, g_persist.bind[k]);
}
static void tdev_end_game(GameState *g, FohState *f) {
  art_resetAArticles(&g->arts);     // resetAArticles() (:1376)
  hd_setPhantonQueue(&g->hq, 0, 0); // setPhantonQueue([]) (:1375)
  g->inp.playing = false;           // playing = false (:1378)
  // gameEnd = false (:1373). target_play.h:55 says outright that this is
  // reset ONLY by endGame — tp_setup_target does not touch it — so a second
  // target match started with it still true (review-mexit-r1 Medium).
  TP.gameEnd = false;
  // setTokenPosSnapToChar(0..3) (:1381-1384). This was listed as an
  // unobservable omission and it is not (review-mexit-r2 Medium): the port's
  // token rest position is PATH DEPENDENT (foh.h cssTokenRest), so a player
  // returning to the CSS sees the tokens where the last drag left them
  // instead of where endGame put them. Slot 2 is the snap; the loop stops at
  // 2 (deviation D6's two ports — upstream's calls for 2..3 address ports
  // this port does not have). Whichever token was in hand is released first
  // — carrying one out of a match is unreachable today (the launch path
  // leaves the CSS), but a stale carry would pin the token to the hand and
  // hide the snap.
  f->cssCarry = -1;
  for (int k = 0; k < 2; k++) f->cssTokenRest[k] = 2;
  // sounds.furaloop.stop(player[i].furaLoopID) for every ACTIVE player left
  // dizzy (:1410-1413). furaloop is a LOOPING voice, so before the A19
  // in-process return this was covered by the process exiting; now a match
  // that ends on the clock while somebody is in FURAFURA would carry the
  // loop over the select screen forever. Same id-routed call the two in-sim
  // stop arms make (FURAFURA.c:73, hit_detection.c:1195), so the mixer's
  // howler-parity id routing decides it, not a name sweep.
  for (int i = 0; i < 4; i++) {
    if (!(g->sim.playerType[i] > -1)) continue; // playerType[i] > -1 (:1411)
    if (strcmp(g->sim.player[i].actionState, "FURAFURA") == 0) {
      ml_sound_stop_id("furaloop.stop", 1, g->sim.player[i].furaLoopID);
    }
  }
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
      tdev_persist_save(); // :1445 setCookie on improvement
    }
    // finish sounds (newRecord/complete) = the registered task-12
    // acceptance-surface deferral.
  }
}

// --- the page-boot RNG, seeded ONCE per process (review-mexit-r2 High) -------
//
// Upstream seeds mulberry32 exactly once, when the PAGE loads, and burns the
// page's boot draws there. startGame does not re-seed and endGame does not
// re-seed — CLAUDE.md states it outright ("mulberry32 state is never
// re-seeded at setupMatch, so boot draw count misalignment silently shifts
// the in-match stream"), and it is why ML_BOOT_DRAWS exists at all. The A19
// in-process return made the old unconditional seed+burn wrong: every
// subsequent match restarted the page's random stream from the top, so match
// 2 replayed match 1's draws — the same background, the same tie-breaks —
// where upstream carries the stream forward.
//
// rngStateAtReset is a PER-MATCH mark, not a boot value: both match arms read
// draws_between(rngStateAtReset, rng.a) as "draws this match consumed", so it
// is re-latched at every launch while the stream itself is not disturbed.
// First launch is bit-identical to the old code, which is what keeps every
// frozen leg (all of which play exactly one match) byte-stable.
static bool g_rngBooted;
static void rng_boot_once(uint32_t seed) {
  if (!g_rngBooted) {
    g_rngBooted = true;
    ml_rng_seed(&G.rng, seed);
    for (int k = 0; k < ML_BOOT_DRAWS; k++) (void)ml_rng_next(&G.rng);
  }
  G.rngStateAtReset = G.rng.a;
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
    tdev_persist_load(); // load chokepoint: installs ctlStyle/modOnR too,
                         // so the improved-record save below stamps the
                         // PERSISTED controls back, not process defaults
                         // (review-mexit-r2 Medium)
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
  // punch-list A12b: the MENU/HOME button opens an overlay OUTSIDE a match.
  // OPT-IN, and deliberately not inferred from other argv.
  //
  // WHY A FLAG and not "--shots-dir is unset" (the first thing tried, and it
  // is WRONG): generated fk scripts drive the rig's SCREENSHOT MARKER on the
  // same MENU keysym (flow-to-fkscript.js:97 MARKER_SYM = "q"), and the live
  // play legs of check-device-target.sh press it while passing NO --shots-dir
  // at all. Inferring from shotsDir therefore ate those marker presses and
  // took the target live leg from 315 frames to the full 900 bound. An
  // explicit flag that ONLY the product launcher passes (port/gfx/opk/
  // mlfk-foh.sh) leaves every rig invocation byte-identical by construction.
  bool sysMenu = false;
  // EVIDENCE ONLY (punch-list C18). Upstream's match clock is 480 s, so the
  // natural finishGame end is EIGHT MINUTES of play — unscriptable in a
  // check and impractical to demonstrate. This shortens the REAL clock and
  // changes nothing else: the expiry still runs sim_tick.c's own
  // matchTimerTick arm, the same ml_sim_finish_hook, the same banner, the
  // same endGame. It proves the real path rather than standing in for it.
  // <= 0 means "leave upstream's 480 s alone", which is every non-demo run.
  double matchTimerSec = 0.0;
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
  // A28 (owner-reported buzz, root-caused 2026-08-24): 512 frames @ 44100 is
  // an 11.61 ms refill deadline — SHORTER THAN ONE 16.67 ms FRAME, so the ALSA
  // buffer starved continuously and the DMA re-played stale audio. The
  // derivation and the 512/1024/2048 table live at the definition site
  // (port/gfx/platform.h); judged by port/gfx/check-alsa-headroom.sh.
  long audioSamples = PLATFORM_AUDIO_SAMPLES_DEFAULT;
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
    else if (strcmp(a, "--system-menu") == 0) sysMenu = true;
    else if (strcmp(a, "--match-timer") == 0 && hasV) {
      // WHOLE-STRING, finite, in range. atof() accepted "abc" as 0,
      // "12xyz" as 12, and "inf"/"nan" as a clock that never expires —
      // an evidence knob that silently misreads is worse than none
      // (review-mexit-r1 Medium; PROCESS §3 whitelist-grammar rule).
      // Upper bound = upstream's own 480 s match clock (settings.js).
      const char *v = argv[++i];
      char *end = 0;
      errno = 0;
      const double sec = strtod(v, &end);
      if (end == v || *end != '\0' || errno == ERANGE || !(sec > 0.0) ||
          !(sec <= 480.0)) {
        fprintf(stderr,
                "foh_dev: --match-timer wants seconds in (0, 480], got %s\n",
                v);
        return 1;
      }
      matchTimerSec = sec;
    }
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
  // domain is the upstream SLIDER's 1..4 (foh.h:73-77, css.js:316-329),
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
      // review-mexit-r2 Medium: both of these are LIVE-PLAY-only arms and
      // were previously accepted-and-ignored everywhere else — the exact
      // "an out-of-domain ARGUMENT silently becomes a no-op" shape the argv
      // whitelist rule above exists to kill. --system-menu installs nothing
      // outside brLive (foh_sysmenu_hook is brLive-gated) and --match-timer
      // is only consulted on the brLive VS launch, so anywhere else they are
      // a malformed invocation that must die loudly, not look successful.
      // The remaining case — --match-timer on a live TARGET launch — cannot
      // be decided here (the launch kind is the PLAYER's choice, made in the
      // menus) and is refused at the launch dispatch instead.
      (sysMenu && !brLive) || (matchTimerSec > 0.0 && !brLive) ||
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
            " [--system-menu] [--match-timer SEC (VS launches only)]"
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
  // C30: the control plane is NOT part of FohState, so foh_persist_apply
  // cannot carry it — foh_persist.h:125 names that install as the FOH's own
  // duty and review-mexit-r1 caught it missing, which meant a save holding
  // Box silently played as Natural. It now lives inside tdev_persist_load,
  // the load chokepoint, so no site can load without it.
  tdev_persist_load();
  foh_persist_apply(&g_persist, &foh);
  if (direct) {
    // DIRECT MATCH: write the SAME FohState fields a menu launch writes
    // (foh.c:666-679's SSS-A arm), then mark it launched. Everything
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
  // MATCH-SUMMARY STATE — declared ABOVE the re-entry label on purpose.
  // A backward goto re-executes every declaration it jumps over, so leaving
  // these below the label silently RESET them on each pass: the A2 guard
  // stopped seeing that a match had been played (rc 4), and the teardown
  // summary reported the empty final pass instead of the match the player
  // actually played. Measured on device, not theorised.
  long matchSkips = 0, matchPresentFails = 0;
  // A11/A12: process exit code. Since A19 landed it is ALWAYS 0 — normal end,
  // and the OPK launcher returns the player to the frontend. It is not const
  // only because it is the function's single return value.
  //
  // review-mexit-r3/r5 Low: this used to say FOH_PAUSE_RC_MENU ("quit to
  // menu", which the launcher answered by re-running the app) was the other
  // possible value. It is not, and that constant and the launcher loop have
  // both been DELETED as scaffolding — QUIT TO MENU returns to the menus
  // IN-PROCESS via `foh_phase:`. See foh_pause.h.
  int quitRc = 0;
  // A19: how many matches this process has played. Distinguishes the A2
  // cross-guard (a flow that NEVER launched) from the player simply
  // leaving the menus after a match, which is a normal end.
  long matchesRun = 0;
  long matchFrames = 0; // frames ACTUALLY ticked (a live target
                        // quit/finish exit runs fewer than `frames`)
  uint64_t matchWallMs = 0;
  bool ranMatch = false;

  // A12b: install the SYSTEM menu hook HERE, not in the bridge arm below.
  // The FOH phase runs BEFORE that arm, so installing it there left the hook
  // NULL for the whole menu phase and MENU/HOME did nothing on the title
  // screen — measured on device, which is the exact symptom the owner
  // reported. Still live-play only: brLive is the same argv predicate.
  if (brLive) foh_sysmenu_hook = foh_sysmenu_open;

  // review-mexit-r3 Medium (M3) — ONE system-menu dispatch predicate for all
  // three loops (this file's FOH phase, the target match, the VS match).
  // The three sites had drifted: only the FOH phase carried `!shotsDir`, so
  // an evidence run that reached a match could still have had its marker
  // press eaten by the modal. `!shotsDir` is not a FOH-phase detail — it is
  // the rig contract for the WHOLE run: with --shots-dir the MENU keysym is
  // the screenshot marker (see the flag's note above), and a modal that
  // consumes it desynchronises the shot schedule wherever it opens.
  //
  // Declared above `foh_phase:` with the other cross-phase state (see the
  // MATCH-SUMMARY note): all three inputs are argv/install-time invariants,
  // so re-deriving it per pass would be equivalent, but the file's stated
  // convention is that anything read on both sides of the label lives here.
  // The per-site EDGE tests (qEdge / pin.menu && !prevPause.menu / !pin.start)
  // stay per-site: they are genuinely different input plumbing, not policy.
  const bool sysOk = sysMenu && !shotsDir && foh_sysmenu_hook;

  // A19 — THE in-process return point (punch-list C18/C19/B4/A19). Every
  // resource a second match needs to re-establish (RNG accounting relatch,
  // gfx loads,
  // sim/target setup) already lives inside the bridge arm below and is simply
  // re-run; the gfx loaders are idempotent or reset fixed pools, so this
  // leaks nothing. The FOH keeps the SAME FohState, which is the whole point:
  // upstream's endGame preserves the player's selections instead of rebooting
  // into the title.
foh_phase:;
  // NOT const: the A12b menu overlay freezes wall clock, and each re-entry
  // restarts this phase's pace epoch.
  uint64_t fohStart = now_ns();
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
      // A12b — the player's MENU button in the FOH MENUS. Reported by the
      // owner as "home button doesn't bring up the menu": the overlay was
      // wired only inside the match loops, and out here MENU drove nothing
      // but the evidence rig's screenshot marker.
      //
      // Gated on --system-menu, which ONLY the OPK launcher passes: the rig
      // drives its screenshot marker on this very keysym, so any inferred
      // discriminator eats marker presses (see the flag's note above).
      if (qEdge && sysOk) {
        uint64_t pausedNs = 0;
        const int quit =
            foh_sysmenu_hook(&g_rz, &pausedNs, &fohPresentFails);
        fohStart += pausedNs; // the pace epoch owes the player that time
        qEdge = false;        // consumed by the overlay, never a marker
        if (quit) {
          g_mexit = MEX_OS;
          break;
        }
        platform_poll(&prevPin); // post-overlay state, not the stale edge
        cur = prevPin;
      }
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
        // A31, MEASURED while wiring the rebinder: this arm named ONLY
        // options-gameplay, so on the PRODUCT binary a Controls-screen or
        // audio change was written to SD only if the player later happened
        // to B-exit an unrelated screen — i.e. it silently vanished on
        // restart. foh_app.c already carries the three-screen form and says
        // exactly that in its own note; the product path never got it.
        // Same class, one condition, both drivers now agree.
        if ((strcmp(ev->from, "options-gameplay") == 0 ||
             strcmp(ev->from, "options-audio") == 0 ||
             strcmp(ev->from, "controls-keyboard") == 0) &&
            strcmp(ev->cause, "b") == 0) {
          foh_persist_collect(&g_persist, &foh);
          tdev_persist_save();
        }
        if (g_have_music && !menuMusicOn && strcmp(ev->to, "menu-top") == 0 &&
            (strcmp(ev->from, "title") == 0 || strcmp(ev->from, "tss") == 0)) {
          // menu music ON at the join (main.js:388-390) — the track is
          // already prefilled; this is a lock-bracketed flag flip.
          //
          // "tss" is upstream's OTHER playMenuLoop into the menu top
          // (targetselect.js:76-81, the B-exit; review-mexit-r2 Medium).
          // It reaches here rather than needing its own program because the
          // in-process return below leaves the MENU track programmed-and-
          // silent on every non-CSS destination, i.e. it restores exactly
          // the precondition the boot establishes (mus_track_program(0, 0)).
          // The two upstream sites that are NOT ported here are the gamepad
          // join (main.js:452-464 — same arm, one input plane) and the target
          // BUILDER (targetselect.js:118-124 — scope-excluded, foh.h).
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
                  "turbo=%d lcancel=%d flashlcancel=%d walljump=%d "
                  "tapjump=%d,%d,%d,%d versus=%d",
                  t, foh.p1Char, foh.p2Char, foh.p2Type, foh.difficulty,
                  foh.stageSel, foh.turbo, foh.lCancelType, foh.flashOnLCancel,
                  foh.everyCharWallJump, foh.tapJumpOff[0], foh.tapJumpOff[1],
                  foh.tapJumpOff[2], foh.tapJumpOff[3], foh.versusMode);
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

  // Flush the FOH artifacts now (between phases; never inside a paced
  // loop — the match loop below is the paced surface that matters).
  //
  // FIRST PHASE ONLY (`matchesRun == 0`) — the ARTIFACT SCOPE rule stated in
  // full at the re-entry site below. Every FOH artifact this run produces
  // describes the FIRST phase, because the frozen expectation each rig holds
  // IS the first phase's: the shot rows are a one-shot script, and the trace
  // is a single FOHTRACE1 document whose tick axis restarts at 1 on every
  // re-entry. Writing them again would not add a second phase's evidence, it
  // would OVERWRITE exactly the evidence the rig came for.
  //
  // Measured, and the reason this guard exists: the FOHTRACE1 header is
  // emitted once at boot (:1928), BEFORE the `foh_phase:` label, while the
  // re-entry resets `g_tr.len` to 0. So a second phase's flush truncated
  // --flow-out to a HEADERLESS single line (`END <t> transitions=0`) — which
  // is not a trace at all: judge-foh-trace.js/normalize-foh-trace.js both
  // reject it, and check-device-target.sh's live leg [6b] lost the
  // `TLAUNCH ... char=2 tstage=0` line it exists to assert. Host witness:
  // port/foh/check-mexit-reentry.sh (tooth T1 proves it fails unguarded).
  if (matchesRun == 0) {
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
  }


  if (bridge) {
    if (!foh.launched) {
      // A19: once a match HAS been played, a FOH phase that ends without a
      // new launch is the player walking away from the select screen, not
      // the A2 "flow never launched" cross-guard. Only the FIRST pass can be
      // that failure, and the artifacts of the match already played stay
      // exactly as that match wrote them.
      if (matchesRun > 0) goto bridge_done;
      // A12b, review-mexit-r1 Medium: a player who boots the game and quits
      // straight out of the SYSTEM menu (or closes the window) never
      // launched anything either — but that is a DELIBERATE exit, not the
      // A2 cross-guard. Reporting it as rc 4 also skipped audio/music
      // teardown and platform_quit entirely. Only a bridge run that ends
      // for no stated reason is the failure.
      if (g_mexit == MEX_OS) goto bridge_done;
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
    // --match-timer shortens the VS match clock (the C18 evidence arm). A
    // TARGET run has no matchTimer expiry — its clock counts UP and its end
    // is tp_finish_game — so the flag has no meaning here and was silently
    // dropped (review-mexit-r2 Medium). The launch kind is only known now,
    // which is why this refusal cannot live in the argv validation above.
    if (tgtLive && matchTimerSec > 0.0) {
      fprintf(stderr, "foh_dev: --match-timer is a VS arm but the flow "
                      "performed a TARGET launch (cross-guard)\n");
      return 4;
    }
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
    // C18 — the finishGame seam (sim.h): the matchTimer expiry and all four
    // DEAD* final-death arms. ONE hook for both match kinds, routed inside
    // exactly as upstream's single finishGame branches on gameMode. NULL on
    // every evidence bridge, so all five sim-side traps stay loud there.
    g_tgt_live = (brLive && foh.targetMode) ? 1 : 0;
    if (brLive) ml_sim_finish_hook = tdev_finish_game_hook;
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
      rng_boot_once(seed);
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
        // MENU-SPEC DEVIATION D20 (owner-requested house rule). Guarded by
        // tgtLive exactly like its siblings: the recorded target goldens must
        // keep the settings they were recorded against.
        G.sim.everyCharWallJump = foh.everyCharWallJump != 0;
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
        if (foh_pause_hook) poll_bound(&prevPause); // VS arm's note
        uint64_t tStart = now_ns(); // NOT const: the overlay shifts it
        for (long f = 0; f < loopMax; f++) {
          poll_bound(&pin); // live: THE input; tverify: backend pump
          // A12: MENU opens the same overlay here. START does NOT — in
          // target mode it is upstream's own endGame quit (main.js:1013
          // -1015) via tp_endgame_hook, which is faithful and already
          // works; the overlay must not shadow it. NULL hook on tverify.
          // `!pin.start` (review-a11-3 L1): on a SIMULTANEOUS MENU+START
          // edge the overlay would open first and its release drain would
          // then swallow the START, shadowing that faithful arm. START wins.
          // A12b: MENU is the FunKey SYSTEM menu here too. START is NOT
          // shadowed — in target mode it is upstream's own endGame quit
          // (main.js:1013-1015) via tp_endgame_hook, which is faithful and
          // already works, so the `!pin.start` guard stays: on a
          // SIMULTANEOUS MENU+START edge the overlay's release drain would
          // otherwise swallow the START. START wins.
          if (sysOk && pin.menu && !prevPause.menu && !pin.start) {
            uint64_t pausedNs = 0;
            if (foh_sysmenu_hook(&g_gfx.rz, &pausedNs, &matchPresentFails)) {
              g_mexit = MEX_OS;
              pauseQuit = 1;
              tfinDeadline = 0;
              ticked = f;
              framesRun = f;
              break;
            }
            tStart += pausedNs;
            poll_bound(&pin);
          }
          prevPause = pin;
          const uint64_t deadline = tStart + (uint64_t)(f + 1) * budgetNs;
          const MlInput *row0;
          if (tgtLive) {
            // C30(a): the PLAYER'S chosen control style, not the pinned BOX
            // table. See the VS site below for the full note.
            liveRow = s1_input_row_style(&pin, ctl_style_get(),
                                         ctl_mod_on_r_get());
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
          // review-mexit-r4 M5, TARGET DECISION (adjudicated by measurement,
          // not by argument — the r3 note claimed this loop needed no
          // finish-frame exception and that claim was WRONG).
          //
          // The r3 reasoning was: gfx_target_frame redraws the whole scene and
          // gfx_target_banner_text re-composites COMPLETE!/FAILURE on top of
          // it on every non-skipped frame of the hold (the arm just below), so
          // one skipped frame presents nothing and the next unskipped frame
          // repaints correctly. True — but it assumes at least one frame of
          // the hold is unskipped. Under sustained catch-up NONE is: `skip` is
          // a pure `t1 > deadline` test, so a run far enough behind skips the
          // finish frame AND every tail frame; the per-frame banner
          // re-composite the r3 argument rests on sits INSIDE `if (!skip)`
          // (:2701-2703) and therefore never runs, and the wall-clock hold
          // set at :2783 is then drained by the sleep loop at :2814-2819
          // over a pre-finish raster. Without this fix the "banner is
          // already presented" claim at :2808 is false; it is true only
          // BECAUSE of the exemption below, which is what that comment now
          // says.
          //
          // MEASURED, and stated at exactly the width of the measurement
          // (.loop/mexit-r5-m5-measure.log — host twin, MLFK_HEADLESS_KEYS
          // scripted live target session, `--budget-ns 1` so every frame is
          // late by construction, run against the then-current source of this
          // file and against a COPY of it whose only delta is `!tfinFirst`
          // removed). PROVENANCE, stated because the log pins none
          // (review-mexit-r5 Low): that measurement PREDATES the comment edits
          // below it, and neither the log nor this note carries a source or
          // binary hash. Every change since was to COMMENTS in this block, so
          // the behavioural result still describes this code — but treat the
          // numbers as "measured on the same program", not "measured on these
          // exact bytes", and re-run it if the loop itself is edited:
          //   (1) the premise holds — `foh_dev match: 900 frames, 900 render
          //       skips`. Sustained catch-up skips EVERY frame, so a finish
          //       frame landing in such a window gets no composite at all.
          //   (2) the exception is correctly SCOPED — both builds report the
          //       identical 900/900, because that session never reaches a
          //       target finish, so `tfinFirst` is false throughout and the
          //       term changes nothing off the finish frame.
          // NOT measured, and true by construction rather than by run: that
          // the exempted frame is exactly ONE. No scripted live target session
          // destroys all ten targets, so the natural finish is not reachable
          // from a key script; the one-frame bound rests on tfinDeadline
          // becoming non-zero below, in this same iteration.
          //
          // The exemption is ONE frame, not the hold. tfinDeadline is declared
          // 0 at :2570, set non-zero at :2783 — below this point, in this same
          // iteration, on the same `g_tfin_fired && tfinDeadline == 0`
          // predicate — and the ONLY assignment that returns it to 0 is
          // :2596, on the system menu's QUIT arm, which `break`s out of the
          // loop three lines later and so can never be observed by a later
          // iteration. `tfinFirst` is therefore true on the finish frame and
          // on no other. `&& !g_tfin_fired` (the shape
          // r3 rejected, correctly) would instead disable skipping for the
          // ENTIRE ~180-frame tail and distort the skip accounting the perf
          // gate reads. This is the VS loop's `!g_vsFinish` exception in the
          // form the target loop's banner actually needs.
          const bool tfinFirst = tgtLive && g_tfin_fired && tfinDeadline == 0;
          const bool skip = pace == 1 && t1 > deadline && !tfinFirst;
          uint64_t t2 = t1, t3 = t1;
          if (!skip) {
            gfx_target_frame(&g_gfx, &G, &TP);
            // Post-finish the sim body is skipped (main.js:991 and
            // :1041-1044, relayed by target_play.h:115-120) and
            // gfx_target_frame would redraw the
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
          // LIVE EXITS. Both leave the match and RETURN TO THE FOH MENUS
          // in-process: each sets g_mexit = MEX_TSS (target select), which
          // the bridge tail below turns into the `foh_phase:` re-entry. That
          // is upstream's own destination — endGame's gameMode-5 arm
          // (main.js:1389-1395) does changeGamemode(7).
          //
          // STATED PRECISELY (this note used to overclaim). Upstream that arm
          // BRANCHES: `if (targetTesting) changeGamemode(4) else
          // changeGamemode(7)` (main.js:1390-1394), and changeGamemode(4) is
          // the TARGET BUILDER. This port carries the flag but never the
          // builder: `TP.targetTesting` has exactly ONE writer in the whole
          // tree — target_play.c:288, upstream's own
          // `targetTesting = test` with `test` hardwired false on the only
          // start path (targetplay.js:178-209, the builder `stageTemp` plane
          // is scope-excluded) — so the else arm is the only reachable one and
          // MEX_TSS is unconditionally right HERE. The CODE was already
          // correct; only this comment stated the destination as if upstream
          // had no branch at all, which later reads as a missing arm rather
          // than an absent feature.
          // STALE COMMENT REMOVED HERE (review-mexit-r3 Low): this block used
          // to say the match phase was terminal, so "ending the run returns
          // the player to the frontend rather than to the FOH menus", and
          // that "re-entering the menus needs the FOH/match outer loop no
          // play arm has". Both statements were true before punch-list
          // C18/C19/B4/A19 and are false now — that outer loop IS the
          // `foh_phase:` label, and this arm uses it. Nothing left here is a
          // deviation for the driver to register.
          //
          // STILL unapplied, and this part is unchanged: upstream endGame
          // (main.js:1372-1396) also resets gameEnd/lost-stock/phantom/
          // article state and sets playing=false; the hook applies none of
          // it. Unobservable because the driver leaves the match on the next
          // statement and a re-entry re-runs the whole sim/target setup from
          // scratch rather than resuming this G.
          //   (1) START while playing -> upstream endGame (main.js:1013
          //       -1015) via tp_endgame_hook. The terminating row is
          //       ROLLED BACK out of both artifacts: replaying it would
          //       re-enter the START edge with tp_endgame_hook NULL,
          //       which is target_play.c's loud trap by design. What is
          //       kept is exactly the prefix that replays.
          //   (2) the target game finished -> upstream stops the music
          //       and leaves 2500 ms later (main.js:1499-1502).
          if (tgtLive && g_tquit) {
            // B4: upstream's START quit IS endGame (main.js:1013-1015), and
            // endGame's gameMode-5 arm (main.js:1389-1395) lands on TARGET
            // SELECT — changeGamemode(7), the only reachable side of its
            // `targetTesting` branch in this port (see the exit note above).
            // Landing on the frontend here was the bug:
            // the quit left the process instead of the match.
            g_mexit = MEX_TSS;
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
            // finishGame -> endGame -> changeGamemode(7) (main.js:1389-1395)
            g_mexit = MEX_TSS;
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
        // banner is already presented — guaranteed by the finish frame's
        // skip exemption above (review-mexit-r4 M5), not by luck of the
        // pacing — and post-finish ticks are inert
        // (main.js:991/:1041-1044, relayed by target_play.h:115-120),
        // so waiting is equivalent to
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
    rng_boot_once(seed);

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
    // versusMode BEFORE the setup, never with the gameSettings below —
    // startGame reads it (main.js:1334); foh_app.c carries the same line and
    // the same reason.
    G.sim.versusMode = foh.versusMode;
    sim_setup_match(&G, foh.p1Char, foh.p2Char, foh.p2Type, foh.difficulty,
                    foh.stageSel);
    G.sim.turbo = foh.turbo != 0;
    G.sim.lCancelType = foh.lCancelType;
    for (int i = 0; i < 4; i++) G.sim.tapJumpOff[i] = foh.tapJumpOff[i];
    // MENU-SPEC DEVIATION D20 (owner-requested house rule).
    G.sim.everyCharWallJump = foh.everyCharWallJump != 0;
    // C18 evidence arm: shorten the REAL clock (see --match-timer above).
    // Live play only — the evidence bridges never pass it, and with the
    // finish hook NULL there the expiry would still be the loud trap.
    if (brLive && matchTimerSec > 0.0) G.matchTimer = matchTimerSec;
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
      if (foh_pause_hook) poll_bound(&prevPause);
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
        poll_bound(&pin);
        // A11/A12: START opens the modal pause overlay. MENU does NOT — since
        // A12b it opens the FunKey SYSTEM menu, dispatched by the arm just
        // below on its own button (review-mexit-r5 Low corrects this note,
        // which still described the pre-A12b single-overlay world). NULL hook
        // on every evidence bridge, so this is dead code for flow/trace-fed
        // runs by construction (install site above).
        // MENU/HOME is the FunKey SYSTEM menu now, in a match exactly as in
        // the FOH menus (A12b). Checked BEFORE the pause arm, and the two are
        // made mutually exclusive by `sysOpened` — NOT by updating prevPause
        // here. Doing that (the shape this replaced) made the pause arm's own
        // `!prevPause.start` edge test compare pin against ITSELF, so it was
        // constant-false and C19's overlay was unreachable on every frame.
        bool sysOpened = false;
        if (sysOk && pin.menu && !prevPause.menu) {
          uint64_t pausedNs = 0;
          if (foh_sysmenu_hook(&g_gfx.rz, &pausedNs, &matchPresentFails)) {
            g_mexit = MEX_OS; // the system menu's QUIT
            vsRan = f;
            break;
          }
          tStart += pausedNs;
          poll_bound(&pin);
          sysOpened = true;
        }
        if (!sysOpened && foh_pause_hook && pin.start && !prevPause.start) {
          uint64_t pausedNs = 0;
          const FohPauseResult pr =
              foh_pause_hook(&g_gfx.rz, &pausedNs, &matchPresentFails);
          tStart += pausedNs; // the pace epoch owes the player that time
          if (pr != FOH_PAUSE_RESUME) {
            // endGame's gameMode-3 destination is the CSS
            // (main.js:1386-1388), so QUIT TO SELECT lands there.
            g_mexit = (pr == FOH_PAUSE_QUIT_SELECT) ? MEX_CSS
                      : (pr == FOH_PAUSE_QUIT_MENU) ? MEX_TITLE
                                                    : MEX_OS;
            vsRan = f; // rows 0..f-1 are recorded; frame f never ticked
            break;
          }
          poll_bound(&pin); // post-overlay state, not the stale edge
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
          // C30(a) — THE integration point for the controls lane's three
          // styles. s1_input_row() is hard-pinned to the ratified BOX table
          // (s1_input.h), so until this call moved, ctl_style_get() had no
          // caller on the play path and the whole feature was unreachable
          // no matter what the Controls screen stored.
          //
          // The EVIDENCE rig app (port/gfx/gfx_app.c) deliberately keeps
          // s1_input_row(): it is the producer for judge-s1-coverage.js,
          // whose 24 pre-registered chord signatures AND its
          // `if (i.y || i.z ...) invBad++` invariant are BOX-only by
          // construction (Natural emits y/z and no C-layer). Style-switching
          // that producer would demand weakening a pinned judge, which
          // HARD RULE 3 forbids; the styles are proven host-side instead by
          // .loop/ctl-style-check.sh. The product path is foh_dev.c — this
          // site and the target one above.
          //
          // SCOPE, precisely (review-mexit-r3 Low): what is live is the
          // CONSUMER half of C30(a). The play path now reads the control
          // cells, so a style that is already set — i.e. one restored from
          // the persisted record by tdev_persist_load() — reaches the sim.
          // The PRODUCER half is still missing: ctl_style_set() /
          // ctl_mod_on_r_set() have exactly ONE caller in this binary, the
          // persist chokepoint at :1304, and no FOH UI caller at all — so a
          // player cannot change style from inside the game yet. C30(c), and
          // it is the menus lane's work, not this file's. Do not read this
          // note as "the feature is fully live".
          liveRow = s1_input_row_style(&gpin, ctl_style_get(),
                                       ctl_mod_on_r_get());
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
        // review-mexit-r3 Medium (M5): `!g_vsFinish` is the finish-frame
        // exception, the same shape the FOH loop's `&& !shotDue` uses
        // (:2268). sim_game_tick above can set g_vsFinish DURING this tick,
        // and the C18 block below paints TIME!/GAME! straight into g_gfx.rz
        // and presents it ONCE, then freezes for 2.5 s. Without this term a
        // catch-up skip on exactly that frame left gfx_render_frame unrun, so
        // the banner composited over the PREVIOUS frame still in the raster
        // and the player held a stale image for the whole hold. Upstream's
        // finishGame paints over the FINAL frame (main.js:1485-1497). Skip
        // accounting stays honest: this frame is genuinely rendered, so it is
        // correctly NOT counted in matchSkips.
        const bool skip = pace == 1 && t1 > deadline && !g_vsFinish;
        // THE ONE FRAME on which the HUD may draw a negative matchTimer
        // (gfx_vfx.h gfx_overlay_allow_timer_expiry). matchTimerTick
        // decrements before it tests, so the frame that fired finishGame
        // carries a clock up to one tick below zero, and unlike upstream —
        // whose rAF render is gated on `playing`, cleared by finishGame — this
        // port renders it.
        //
        // BRACKETED, not merely assigned (review-r3-r4 Medium): the finish
        // frame BREAKS out of this loop a few statements later, so a
        // set-and-forget would leave the permission armed for the rest of the
        // process and a TARGET match started afterwards could inherit it and
        // have a negative target clock silently clamped instead of trapped.
        // The window is now exactly this render; gfx_overlay_reset() clears it
        // too, so a path that never reaches the reset below still cannot leak.
        uint64_t t2 = t1, t3 = t1;
        gfx_overlay_allow_timer_expiry(g_vsFinish);
        if (!skip) {
          gfx_render_frame(&g_gfx, &G);
          t2 = now_ns();
          if (platform_present(g_gfx.rz.fb) != 0) matchPresentFails++;
          t3 = now_ns();
        } else {
          matchSkips++;
        }
        gfx_overlay_allow_timer_expiry(0);
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
        // C18 — finishGame fired inside THIS tick: matchTimer expiry
        // (main.js:348) or a DEAD* final death (DEAD*.js:39), the normal way
        // a VS match ends. Upstream paints the banner over the final frame
        // and holds it 2500 ms of WALL CLOCK with `playing` false, so the sim
        // is frozen and the render keeps the last frame; endGame then lands
        // on the CSS. Freezing this loop for the hold is behaviourally the
        // same and touches no checksummed TU, exactly as the pause overlay's
        // argument (foh_pause.h). Only the live arm can get here: the hook is
        // NULL for every evidence bridge, so the sim traps instead.
        if (g_vsFinish) {
          vsRan = f + 1; // this frame ran and IS part of the prefix
          // "Time!" when the clock ran out, else "Game!" (main.js:1470-1483)
          const int timeUp = (G.matchTimer <= 0);
          tdev_vs_banner_text(&g_gfx.rz, timeUp);
          // ...and the SFX upstream plays on the SAME branch
          // (main.js:1472 sounds.time / :1478 sounds.game). Both names are
          // in the shipped SND1 pack (pack-snd.js packs all 180); a missing
          // one is a loud pack error, never a silent skip.
          foh_snd(timeUp ? "time" : "game");
          // C18 evidence: the banner is NOT an overlay, so nothing else can
          // photograph it. Same MLFK_MENU_SHOT env as the overlays: unset in
          // the product launcher and in every evidence leg, and set by
          // EXACTLY TWO checks — port/foh/check-live-arms.sh, which drives
          // this arm to its real matchTimer expiry on the HOST twin and
          // measures this frame's ink against the committed font to prove
          // the banner says TIME!, and port/foh/check-device-foh.sh's [7b]
          // leg, which drives the same arm ON THE DEVICE.
          {
            const char *sd = getenv("MLFK_MENU_SHOT");
            if (sd && *sd) {
              char sp[512];
              snprintf(sp, sizeof sp, "%s/finish-banner.ppm", sd);
              write_shot_ppm(g_gfx.rz.fb, sp);
            }
          }
          if (platform_present(g_gfx.rz.fb) != 0) matchPresentFails++;
          // PRESENT WITNESS on the ONE present this block makes (R6,
          // 2026-07-31). The self-shot above photographs the RASTER, which a
          // dead or no-op presenter produces just as happily — on the host
          // that is all there is, and "the banner reached the panel" stayed
          // an inference for as long as this arm had never run on hardware.
          // fbwit_sample re-reads the DISPLAYED kernel-fb page and dies
          // in-app if it is not this frame, exactly as it does for the FOH
          // shots; the re-flush republishes the witness file the FOH phase
          // already wrote, because the rows are cumulative and the FOH
          // re-entry this block causes deliberately writes no artifacts
          // (matchesRun > 0 at :2337). Inert wherever --fb-witness is absent
          // — i.e. everywhere except the one leg that drives this arm — and
          // unreachable on a non-linux build, where fbwit_open already died
          // at startup rather than letting either pointer be set.
          //
          // FIRST MATCH ONLY, and that is the SAME ARTIFACT-SCOPE RULE the
          // FOH flush already obeys at :2348 (review-r6-r5 [LOW]). The rows
          // are cumulative and never reset, so an unguarded sample would let
          // a run that finishes a SECOND match append a SECOND
          // `finish-banner` row and re-flush FBWIT1 with duplicates — an
          // artifact whose row list no longer means "the flow's shots, plus
          // the finish". An earlier version of this note claimed the row cap
          // would catch that; it would not, because a five-shot flow has ten
          // rows of headroom. `matchesRun` is still 0 here (it increments at
          // :3545, after this block), so this is exactly "the first match".
          if ((g_fbwit_path || g_fbwit_raw) && matchesRun == 0) {
            fbwit_sample("finish-banner", g_gfx.rz.fb, f + 1);
            fbwit_flush(flowId);
          }
          // MusicManager.stopWhatisPlaying() (main.js:1498). This arm MUTES
          // ONLY: the lock-bracketed flag flip below, and nothing else. The
          // TARGET finish arm additionally signals the reader thread to quit
          // (g_mus_quit, :2780-2789); this one does not. The thread join stays
          // in teardown either way, never inside a paced loop.
          //
          // Both arms re-enter the FOH IN-PROCESS since A19, and the re-entry
          // site below stops, reprograms and restarts the reader for whichever
          // one ran — so this is NOT a "can the reader come back" question
          // (review-mexit-r7 Low; two earlier versions of this note got that
          // wrong in opposite directions). It is a cost question, and the two
          // holds differ: the target hold is the last thing before that match
          // is torn down, so stopping its SD refills is free, whereas this hold
          // sits 2.5 s in the middle of a still-live mixer whose reader the
          // very next match will want. Leaving a muted reader to do a few SD
          // refills across it is cheaper than tearing the thread down and
          // standing it back up.
          if (g_have_music) {
            platform_audio_lock();
            g_mix.music.on = 0;
            platform_audio_unlock();
          }
          // WALL-FIELD INFLATION, stated (review-mexit-r3 Low): this hold
          // nanosleeps INSIDE match scope, i.e. between tStart and the tEnd
          // that feeds `matchWallMs`, so the teardown summary's `wall <N> ms`
          // absorbs the full 2500 ms. The same is true of the target loop's
          // hold tail.
          //
          // Harmless, and measured rather than assumed. `wall` IS bounded by
          // two checks — check-device-foh.sh:1472 and
          // check-device-target.sh:1194, both [58000,66000] ms for a
          // 3600-frame leg — but neither leg can reach this hold: it is
          // live-arm-only (g_vsFinish requires the finish hook, which is NULL
          // for every evidence bridge; the target tail is tgtLive-gated), and
          // those legs are trace-fed. No judge derives a frame RATE from
          // `wall` either: the perf judge reads the per-frame --timing file
          // (percentiles.js, p50/p99 over sim+render+present), and held
          // frames write no timing row at all because the loop has already
          // broken. If some future check ever wants "seconds of PLAY",
          // subtract TFIN_HOLD_NS when g_vsFinish fired — do NOT move the
          // hold out of match scope, because to the player the hold is part
          // of the match, and either wall bound would then need re-pinning.
          const uint64_t hold = now_ns() + TFIN_HOLD_NS;
          while (now_ns() < hold) {
            struct timespec hs;
            hs.tv_sec = 0;
            hs.tv_nsec = 4000000L; // 4 ms
            nanosleep(&hs, 0);
          }
          g_mexit = MEX_CSS; // endGame: changeGamemode(2) (main.js:1386-1388)
          break;
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

  // --- A19: the in-process return (punch-list C18 + C19 + B4) --------------
  // endGame does NOT leave the game — it resets match state and changes
  // gameMode back to a SELECT screen (main.js:1386-1395). Only MEX_OS leaves
  // the process; everything else goes round again. See the MatchExit note.
  if (g_mexit == MEX_CSS || g_mexit == MEX_TSS || g_mexit == MEX_TITLE) {
    tdev_end_game(&G, &foh);
    // Putting the screen back on the LIVE struct is NOT enough on its own,
    // and that was measured: the select screen still holds the confirm that
    // launched this match, so the very next tick launches again with no input
    // at all. The device trace showed it exactly — a second `TLAUNCH 1`
    // immediately after the first match's START quit, with the held START
    // bleeding into match 2 as its frames 1-2.
    //
    // (review-mexit-r6b Low: a paragraph here used to describe the FIRST
    // attempt at the cure — foh_init for a "KNOWN-CLEAN" machine, with the
    // player's selections copied back over it. That implementation is gone;
    // review-mexit-r1 showed the copy-back was a whitelist that silently
    // dropped everything nobody listed. The paragraph outlived it and
    // contradicted the live-state reset explained immediately below, so it is
    // deleted rather than left to be read as current.)
    matchesRun++;
    // KEEP the state machine and reset only what a match actually
    // invalidates. The first shape of this (foh_init + a whitelist of
    // fields copied back) was WRONG and review-mexit-r1 caught it: a
    // whitelist silently discards everything nobody thought to list —
    // measured casualties were tssCursor (return from target N landed on
    // target 0), p1Difficulty, and the CSS hand/slider positions
    // (cssHandX/Y, cssSliderX[], cssCarry, cssHandType), which upstream
    // holds at MODULE scope and therefore never resets (css.js:64-75;
    // endGame at main.js:1372-1418 never touches them either).
    //
    // What a match DOES invalidate, and why each one:
    //   launched — the confirm that started THIS match is still latched,
    //     so without this the very next tick launches again with no input
    //     at all (measured on device: a second `TLAUNCH 1` immediately
    //     after the first match's START quit).
    //   bHold — the CSS B-back counter; a stale count would insta-back.
    //   prev — the edge snapshot; seeded from the post-drain input below
    //     so a still-held button is not read as a fresh press. This is
    //     upstream's `pause = [[true,true],...]` latch (main.js:1396).
    //   nev/nsnd — cleared by foh_tick anyway (foh.c:829-830); zeroed here so
    //     nothing between now and the next tick can read a stale event.
    foh.launched = false;
    foh.bHold = 0;
    foh.nev = 0;
    foh.nsnd = 0;
    foh.screen = (g_mexit == MEX_CSS)   ? FOH_CSS  // changeGamemode(2)
                 : (g_mexit == MEX_TSS) ? FOH_TSS  // changeGamemode(7)
                                        : FOH_TITLE;
    // MUSIC (main.js:1377-1388, review-mexit-r1 High): endGame ALWAYS
    // stops what is playing, and then — for gameMode 3 (VS) ONLY — starts
    // the menu loop. gameMode 5 (targets) deliberately does not, so target
    // select is silent upstream too, and that is carried verbatim.
    // Without this the stage/target track kept playing over the select
    // screen, and after a natural finish (which mutes below) nothing ever
    // came back.
    //
    // ONE shape for all three destinations (review-mexit-r2 High + Medium):
    // program the MENU track, audible only where upstream plays it. Two
    // defects this replaces:
    //   * mus_track_program was called with the reader thread STILL LIVE,
    //     which its contract forbids outright (:1030) — it fcloses g_mus_file
    //     and frees/replaces the ring the reader is concurrently reading.
    //     Both other switch sites already bracket it; this one did not.
    //   * the non-CSS arm only MUTED, leaving the stage/target track
    //     programmed. So MEX_TITLE's later title->menu-top flag flip unmuted
    //     the STAGE track over the menu, and TSS->menu-top (upstream's own
    //     playMenuLoop, targetselect.js:76) was silent or wrong-track.
    //     Programming the menu track silent restores the boot precondition
    //     the flag-flip arm is written against.
    if (g_have_music) {
      const int menuAudible = (g_mexit == MEX_CSS) ? 1 : 0;
      mus_reader_stop(); // MUST be stopped across a program (:1030)
      mus_track_program(0, menuAudible); // kMusTok[0] = menu
      mus_reader_start();
      // CSS: playMenuLoop (:1388). TSS: silent upstream (gameMode 5 takes
      // no playMenuLoop branch) — but re-armed, because its B-exit does.
      // TITLE is this port's own affordance and re-arms like a fresh boot.
      menuMusicOn = menuAudible != 0;
    }
    g_mexit = MEX_NONE;
    g_vsFinish = 0;
    g_tquit = 0;
    g_tfin_fired = 0;
    // ARTIFACT SCOPE (review-mexit-r2 Medium; corrected review-mexit-r3 High).
    // A multi-phase run produces ONE set of artifacts, and which phase each
    // one describes is stated here, once, for all of them. Three scopes, each
    // forced by what its consumer can accept:
    //
    // (1) FIRST PHASE — every FOH ARTIFACT: the flow-out trace, the shot
    //     PPMs, the framebuffer witness, and the `shots` count in the FOH
    //     summary. Not a preference: the FOHTRACE1 header is emitted ONCE at
    //     boot (:1928), before the `foh_phase:` label, and the FOH tick axis
    //     restarts at 1 on every re-entry (`endTick`/`t` are declared after
    //     the label). So an APPENDED trace would carry a second END after a
    //     terminal one plus a decreasing frame column, and a REWRITTEN one is
    //     headerless — both malformed, and both rejected by BOTH consumers:
    //     the append by judge-foh-trace.js:149 / normalize-foh-trace.js:107
    //     ("content after END"), the rewrite by judge-foh-trace.js:121 /
    //     normalize-foh-trace.js:79 (the exact `FOHTRACE1 flow=<id>` header).
    //     Host witness for the rewrite, both directions:
    //     port/foh/check-mexit-reentry.sh ([4] cmp, [5] T1). The shot rows
    //     are a one-shot script whose frozen expectation is likewise phase
    //     1's, so re-arming them would re-photograph and overwrite exactly
    //     the evidence a rig came for. One rule, one guard: the flush site
    //     above is wrapped in `matchesRun == 0`, and the shot schedule
    //     (g_nshotbuf, shotIdx, markerIdx) is deliberately NOT reset below.
    //     Getting this half-right is what review-mexit-r3 caught: the shot
    //     schedule was carved out correctly while the trace was still
    //     rewritten every phase, i.e. the two halves of one rule contradicted
    //     each other.
    //
    // (2) FINAL PHASE OF ITS KIND — the two stderr summary lines, with ONE
    //     documented exception. The match counters below are reset per phase,
    //     so the `foh_dev match:` line is wholly the LAST match's; before
    //     this, `frames`/`wall` were the last match's while `render skips`/
    //     `failed presents` were every match's, which is the mixing this
    //     reset removes. The `foh_dev foh:` line is per-field, and one field
    //     is deliberately NOT final-phase (review-mexit-r4): `ticks`,
    //     `transitions`, `render skips`, `failed presents` and `launched` are
    //     the FINAL FOH phase's, but `shots` is `g_nshotbuf`, which the shot
    //     schedule above deliberately does not re-arm, so it is the FIRST
    //     phase's count and stays that way for the same reason the trace does
    //     — every frozen expectation is phase 1's. So the line does span two
    //     scopes; it is stated here rather than claimed away. Grammar-safe:
    //     no field is added or removed, and a single-match run — which is
    //     every committed leg — produces byte-identical lines, which is why
    //     no consumer can observe the split.
    //
    // (3) LAST MATCH — the MATCH sinks, because each match's writer opens
    //     them with mode "w" and therefore truncates: --record-trace
    //     (:2867 target / :3409 VS), --record-keys (:2873 / :3415) and
    //     --bstate-out (:2471 / :3040). That is the honest scope — a replay
    //     trace is one match's input stream and cannot be concatenated — but
    //     it was previously true only by accident of the fopen mode, so it is
    //     written down here. Every committed leg plays exactly one match, so
    //     no committed expectation depends on which match won; a live
    //     acceptance session that plays several keeps the LAST one.
    g_tr.len = 0;
    g_tr_full = 0;
    transitions = 0;
    fohSkips = 0;
    fohPresentFails = 0;
    matchSkips = 0;
    matchPresentFails = 0;
    // ranMatch is NOT cleared: it means "this process played a match", which
    // stays true forever after and is what gates the teardown match summary.
    // Clearing it here swallowed that summary entirely (measured on device).
    // DRAIN every still-held input before handing control back. Whatever
    // ended this match is still down — START for target mode's endGame, A
    // for a pause-menu entry, or a direction the player never let go of —
    // and the screen we land on is edge-driven, so any of them would read
    // as a FRESH press on its very first frame. Upstream gets this for
    // free from endGame's `pause = [[true,true],...]` latch
    // (main.js:1396). review-mexit-r1 Medium: the earlier form waited
    // on a/b/start/menu only, so a held direction still leaked a cursor
    // move; platform_input_idle() covers the whole action-bearing set and
    // is shared with both overlays' drains (platform.h). Keep presenting
    // the last frame so the screen does not appear frozen while a finger
    // rests on a button.
    //
    // BOUNDED, and it shares the overlays' implementation (foh_pause.h's
    // foh_drain_release — the R3 precondition class fix). This used to be a
    // third hand-written unbounded `for (;;)`, and it carried a defect the
    // other two did not: its present was NOT latched, so a display that had
    // died kept being re-presented and re-counted every single iteration,
    // inflating `failed presents` — a field five committed rigs parse — by
    // one per drain frame instead of by one per dead display. The shared
    // drain latches it by construction.
    //
    // ONE deliberate difference, stated rather than claimed away: the shared
    // drain paces at the overlays' fixed 16.666667 ms period, where this site
    // used the run's own `--budget-ns`. It is observable only while something
    // is held (a released input exits on the first poll and never sleeps),
    // every committed invocation passes exactly 16666667 anyway (grep-
    // measured over port/foh, port/sim/device and port/gfx/opk), and a drain
    // is not a frame loop whose rate any judge derives anything from.
    bool drainQuit = false;
    bool drainStuck = false;
    int drainDead = 0; // no prior latch at this site: the match loop's own
                       // present failures are counted in matchPresentFails
    switch (foh_drain_release("match-exit-release", &g_gfx.rz, &fohPresentFails,
                              &drainDead, &prevPin)) {
      case FOH_DRAIN_QUIT: drainQuit = true; break;
      case FOH_DRAIN_TIMEOUT: drainStuck = true; break;
      case FOH_DRAIN_IDLE: break;
    }
    // review-mexit-r2 Low: SDL_QUIT is ONE-SHOT — platform_poll latches the
    // event once and the next poll reports `quit` false. Breaking out of the
    // drain and re-entering the FOH therefore ATE the player's close request:
    // the window/shutdown signal was consumed here and never seen again. A
    // quit is a quit whichever loop observes it, so it becomes MEX_OS and
    // falls straight through to teardown (audio stop, platform_quit) instead.
    if (drainQuit) {
      g_mexit = MEX_OS;
    } else {
      // Seed the FOH's edge snapshot from the state we actually hand it, and
      // note that this is correct on BOTH ways out of the drain — the two
      // differ in what the state IS, never in what has to be done with it
      // (review-r3-r1 Low; an earlier note asserted only the first case):
      //   IDLE    — platform_input_idle() SUCCEEDED, so the state is neutral
      //             on every action-bearing button and `prev == cur` is
      //             trivially edge-free;
      //   TIMEOUT — something is still down (the drain named it on stderr),
      //             and seeding `prev` WITH it down is exactly what stops it
      //             reading as a fresh press on the FOH's first tick. The
      //             button stays held until the player or the hardware lets
      //             go, which is the honest model of a stuck button.
      (void)drainStuck; // the verdict is identical; the distinction is the
                        // drain's own diagnostic line, not a branch here
      foh.prev = prevPin;
      cur = prevPin;
      goto foh_phase;
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
