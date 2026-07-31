// port/foh/foh_app.c — the host FOH driver (fix_plan §M4 task 9;
// pre-registration AGENT-LOG iter 88).
//
// Drives the FOH screen machine (foh.c) with a committed FLOW script
// (deterministic per-frame input rows — the §M4 conventions' menu
// verification approach (a)), emits the structural transition trace
// (FOHTRACE1, judged by port/foh/judge-foh-trace.js + cmp vs the frozen
// flows/<id>.expect), dumps per-screen screenshots (byte-stable x2 in
// the check), and — when the flow launches a match — runs the
// MATCH-LAUNCH BRIDGE: the FOH state (chars/type/difficulty/stage/
// settings), never CLI params, feeds sim_setup_match; the launched
// stream is judged by the PINNED-UNCHANGED wrap-run.js +
// verify-stream.js against the frozen golden (conventions (b)+(c);
// full-stream equality exceeds the required prefix).
//
// INPUT SEAM (foh.h note): on host the rows come from the flow script;
// task 10's device app feeds platform_poll instead — same machine.
// platform_present is still called every FOH frame (headless backend =
// the check's backend; the SDL backends are what task 10 links).
//
// RNG DISCIPLINE: the FOH machine consumes no RNG by construction; the
// seeded stream is created + boot-burned only at the launch seam, so
// the launched match's rngCalls match the harness domain exactly.
//
// Output contract: --out carries ONLY the stream lines wrap-run.js
// accepts ("F <n> <hash>" / "RNG a b" / "SIM OK"); all FOH output goes
// to --flow-out / --bstate-out; stdout stays silent.
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../sim/ml_events.h"
#include "../sim/sim/sim.h"
#include "../sim/target/target_play.h" // M4 task 12: target bridges
#include "foh.h"
#include "../gfx/ctl_style.h" // C30(c)
#include "foh_persist.h" // M4 task 13: the ONE persistence chokepoint

#define ML_BOOT_DRAWS 465 // the qjs boot pin (oracle/qjs/replay.sh)

void gfx_fatal(const char *what) { sim_fatal(what); }

// --- persistence (M4 task 13) ------------------------------------------------
// Loaded at boot, applied to the machine; saved at the upstream save
// points: the options-gameplay B-exit (gameplaymenu.js:29-33) and the
// finishGame record arm (main.js:1442-1445) via tp_finish_hook. The
// committed t01/t02 traces never reach the finish seam (iter-99
// refutation) — the hook is real wiring, exercised by the
// check-device-persist rig's --tooth-persist-finish arm (foh_dev) and
// the acceptance surface.
static FohPersist g_persist;

static void app_persist_finish_hook(GameState *g, bool complete) {
  if (!complete) return; // main.js:1431 complete arm only
  const int ch = (int)g->sim.characterSelections[0]; // :1442 operands
  const int ts = (int)TP.targetStagePlaying;
  if (foh_persist_record_update(&g_persist, ch, ts, g->matchTimer)) {
    foh_persist_save(&g_persist); // :1445 setCookie on improvement
  }
  // finish sounds (newRecord/complete) = the registered task-12
  // acceptance-surface deferral (menu-plane Howls, zero seeded draws).
}

// --- flow script (FLOW1; whitelist grammar, loud death) ---------------------

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
  fprintf(stderr, "foh_app: flow %s line %d: %s\n", path, lineNo, what);
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
    fprintf(stderr, "foh_app: cannot open flow %s\n", path);
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
    fprintf(stderr, "foh_app: flow %s: missing FLOW1 header, I rows, or "
                    "END\n", path);
    exit(2);
  }
}

// --- match trace loading (sim_main.c:39-148, duplicated verbatim — the
// gfx_app.c precedent) ---------------------------------------------------------

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

// --- shot dump (gfx_app.c write_shot_ppm, 565 -> 888 by shift) ---------------

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

static Raster g_rz; // 240x240 fb + ink (static: large)

int main(int argc, char **argv) {
  const char *flowPath = 0, *flowOut = 0, *shotsDir = 0;
  const char *bridge = 0; // "state" | "verify" | "tstate" | "tverify"
  const char *simdataPath = 0, *tracePath = 0, *outPath = 0, *bstateOut = 0;
  long seed = -1, frames = -1;
  bool cpuLive = false;
  for (int i = 1; i < argc; i++) {
    const char *a = argv[i];
    const bool hasV = i + 1 < argc;
    if (strcmp(a, "--flow") == 0 && hasV) flowPath = argv[++i];
    else if (strcmp(a, "--flow-out") == 0 && hasV) flowOut = argv[++i];
    else if (strcmp(a, "--shots-dir") == 0 && hasV) shotsDir = argv[++i];
    else if (strcmp(a, "--bridge") == 0 && hasV) bridge = argv[++i];
    else if (strcmp(a, "--simdata") == 0 && hasV) simdataPath = argv[++i];
    else if (strcmp(a, "--trace") == 0 && hasV) tracePath = argv[++i];
    else if (strcmp(a, "--out") == 0 && hasV) outPath = argv[++i];
    else if (strcmp(a, "--bstate-out") == 0 && hasV) bstateOut = argv[++i];
    else if (strcmp(a, "--seed") == 0 && hasV) seed = strtol(argv[++i], 0, 10);
    else if (strcmp(a, "--frames") == 0 && hasV) frames = strtol(argv[++i], 0, 10);
    else if (strcmp(a, "--cpu-live") == 0) cpuLive = true;
    else {
      fprintf(stderr, "foh_app: bad argument %s\n", a);
      return 1;
    }
  }
  const bool wantState = bridge && strcmp(bridge, "state") == 0;
  const bool wantVerify = bridge && strcmp(bridge, "verify") == 0;
  // M4 task 12: the TARGET twins — tstate = TBRIDGE-STATE witness only;
  // tverify = full trace-fed target replay emitting BOTH streams in the
  // EXACT target_main.c stdout grammar (wrap-target.js's producer
  // contract: F/T interleave + RNG + TFIN + SIM OK).
  const bool wantTState = bridge && strcmp(bridge, "tstate") == 0;
  const bool wantTVerify = bridge && strcmp(bridge, "tverify") == 0;
  const bool anyVerify = wantVerify || wantTVerify;
  if (!flowPath || !flowOut ||
      (bridge && !wantState && !wantVerify && !wantTState && !wantTVerify) ||
      // bridge modes need simdata + seed + the state witness sink
      (bridge && (!simdataPath || seed < 0 || !bstateOut)) ||
      // verify modes additionally need the golden trace/frames/stream sink
      (anyVerify && (!tracePath || frames <= 0 || !outPath)) ||
      (!anyVerify && (tracePath || frames > 0 || outPath)) ||
      (!wantVerify && cpuLive) ||
      (!bridge && (simdataPath || seed >= 0 || bstateOut))) {
    fprintf(stderr,
            "usage: foh_app --flow f.flow --flow-out trace.txt "
            "[--shots-dir D] [--bridge state --simdata s.txt --seed N "
            "--bstate-out b.txt] [--bridge verify --simdata s.txt --seed N "
            "--trace t.txt --frames N --out stream.txt --bstate-out b.txt "
            "[--cpu-live]] [--bridge tstate|tverify (target twins; same "
            "params as state/verify)]\n");
    return 1;
  }
  if (frames > 1000000L) sim_fatal("foh_app: --frames exceeds the buffer cap");

  load_flow(flowPath);

  // flow id = basename minus .flow ([a-z0-9-]+; the trace header pin)
  const char *base = strrchr(flowPath, '/');
  base = base ? base + 1 : flowPath;
  char flowId[64];
  {
    size_t blen = strlen(base);
    if (blen < 6 || blen - 5 >= sizeof flowId ||
        strcmp(base + blen - 5, ".flow") != 0) {
      fprintf(stderr, "foh_app: --flow must end in .flow\n");
      return 1;
    }
    memcpy(flowId, base, blen - 5);
    flowId[blen - 5] = 0;
    for (const char *p = flowId; *p; p++) {
      if (!((*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9') ||
            *p == '-')) {
        fprintf(stderr, "foh_app: flow id must match [a-z0-9-]+\n");
        return 1;
      }
    }
  }

  FILE *tf = fopen(flowOut, "w");
  if (!tf) sim_fatal("cannot open --flow-out for writing");
  if (fprintf(tf, "FOHTRACE1 flow=%s\n", flowId) < 0) {
    sim_fatal("--flow-out write failed");
  }

  if (platform_init("meleelight-foh") != 0) {
    sim_fatal("platform_init failed");
  }

  FohState foh;
  foh_init(&foh);
  // task 13: load persisted settings/records through the chokepoint
  // (loud reset-to-defaults on missing/corrupt — foh_persist.h) and
  // apply them to the machine. Hermetic checks point MLFK_PERSIST_DIR
  // at a fresh dir so flows start from defaults.
  foh_persist_load(&g_persist);
  foh_persist_apply(&g_persist, &foh);
  // C30(c): the control-style + Mod-shoulder cells are NOT FohState fields
  // (they are read by the input path in another TU), so the FOH owns these
  // two calls at the persist chokepoint rather than foh_persist_apply —
  // exactly as foh_persist.h specifies. Without them the Controls screen
  // would edit a setting that resets on every boot.
  ctl_style_set(g_persist.ctlStyle);
  ctl_mod_on_r_set(g_persist.modOnR != 0);
  foh_render_warm(&g_rz); // cold caches off the frame budget (foh_render.c)
  long transitions = 0;
  long launchFrame = 0;
  // CANONICAL SHOT PHASE (foh_look_canonical, foh_render.c): same split as
  // foh_dev.c — SHOT rows before the flow's FIRST NON-NEUTRAL input row are
  // tick-indexed on every target and judge the live animated frame; later
  // rows are the ones the device captures on a wall-clock q edge, so they
  // render at the look plane's resting phase on BOTH sides.
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
  int rowIdx = 0, shotIdx = 0;
  PlatformInput cur;
  memset(&cur, 0, sizeof cur);
  for (long f = 1; f <= g_flow_frames; f++) {
    while (rowIdx < g_nrows && g_rows[rowIdx].frame == f) {
      cur = g_rows[rowIdx].in;
      rowIdx++;
    }
    foh_tick(&foh, &cur);
    for (int e = 0; e < foh.nev; e++) {
      const FohEvent *ev = &foh.ev[e];
      if (ev->kind == FOH_EV_TRANS) {
        transitions++;
        if (fprintf(tf, "T %ld %s %s %s\n", f, ev->from, ev->to, ev->cause) <
            0) {
          sim_fatal("--flow-out write failed");
        }
        // task 13: the upstream options save points — gameplaymenu.js:
        // 29-33 (setCookie per gameSettings key on the B-exit) and
        // audiomenu.js:24-25 (setCookie soundsLevel/musicLevel on ITS
        // B-exit, with no meHost gate). Both are B-exits of an options
        // screen and both go through the one chokepoint.
        // C30(c) (review-r12 MAJOR): controls-keyboard's B-exit is a save
        // point too. Without it a style/shoulder edit was only persisted if
        // the user later happened to visit an UNRELATED options screen, so it
        // silently vanished on restart — the same class of bug as an
        // inaudible slider: the setting appears to take and does not survive.
        if ((strcmp(ev->from, "options-gameplay") == 0 ||
             strcmp(ev->from, "options-audio") == 0 ||
             strcmp(ev->from, "controls-keyboard") == 0) &&
            strcmp(ev->cause, "b") == 0) {
          foh_persist_collect(&g_persist, &foh);
          // C30(c): the twin of the load-side calls above (foh_persist.h).
          g_persist.ctlStyle = (int)ctl_style_get();
          g_persist.modOnR = ctl_mod_on_r_get() ? 1 : 0;
          foh_persist_save(&g_persist);
        }
      } else if (ev->kind == FOH_EV_SEL) {
        int w;
        if (ev->sval) {
          w = fprintf(tf, "S %ld %s %s\n", f, ev->field, ev->sval);
        } else {
          w = fprintf(tf, "S %ld %s %d\n", f, ev->field, ev->val);
        }
        if (w < 0) sim_fatal("--flow-out write failed");
      } else {
        launchFrame = f;
        int w;
        if (foh.targetMode) {
          // the target launch record (foh.h TLAUNCH note; iter 99)
          w = fprintf(tf, "TLAUNCH %ld char=%d tstage=%d\n", f, foh.p1Char,
                      foh.tssStage);
        } else {
          w = fprintf(tf,
                      "LAUNCH %ld p1=%d p2=%d p2type=%d difficulty=%d "
                      "stage=%d turbo=%d lcancel=%d flashlcancel=%d "
                      "walljump=%d tapjump=%d,%d,%d,%d "
                      "versus=0\n",
                      f, foh.p1Char, foh.p2Char, foh.p2Type, foh.difficulty,
                      foh.stageSel, foh.turbo, foh.lCancelType,
                      foh.flashOnLCancel, foh.everyCharWallJump,
                      foh.tapJumpOff[0], foh.tapJumpOff[1],
                      foh.tapJumpOff[2], foh.tapJumpOff[3]);
        }
        if (w < 0) sim_fatal("--flow-out write failed");
      }
    }
    if (shotsDir && shotIdx < g_nshots && g_shots[shotIdx].frame == f &&
        g_shots[shotIdx].frame >= firstInputFrame) {
      FohState look = foh;
      foh_look_canonical(&look);
      foh_render(&look, &g_rz);
    } else {
      foh_render(&foh, &g_rz);
    }
    if (platform_present(g_rz.fb) != 0) {
      sim_fatal("platform_present failed during a FOH frame");
    }
    while (shotIdx < g_nshots && g_shots[shotIdx].frame == f) {
      if (shotsDir) {
        char path[512];
        if (snprintf(path, sizeof path, "%s/%s.ppm", shotsDir,
                     g_shots[shotIdx].name) >= (int)sizeof path) {
          sim_fatal("shot path overflow");
        }
        write_shot_ppm(g_rz.fb, path);
      }
      if (fprintf(tf, "SHOT %ld %s\n", f, g_shots[shotIdx].name) < 0) {
        sim_fatal("--flow-out write failed");
      }
      shotIdx++;
    }
  }
  if (fprintf(tf, "END %ld transitions=%ld\n", g_flow_frames, transitions) <
      0) {
    sim_fatal("--flow-out write failed");
  }
  if (fclose(tf) != 0) sim_fatal("--flow-out close/flush failed");
  platform_quit();

  if (!bridge) return 0;

  // --- the match-launch bridge -------------------------------------------------
  if (!foh.launched) {
    fprintf(stderr, "foh_app: --bridge given but the flow never launched\n");
    return 4;
  }
  (void)launchFrame;
  // launch-kind cross-guards (fail closed; the --cpu-live class): a VS
  // bridge fed a target launch — or a target bridge fed a VS launch —
  // is a flow/bridge mismatch, never a silent re-route.
  if ((wantState || wantVerify) && foh.targetMode) {
    fprintf(stderr, "foh_app: --bridge %s but the flow performed a TARGET "
                    "launch (cross-guard)\n", bridge);
    return 4;
  }
  if ((wantTState || wantTVerify) && !foh.targetMode) {
    fprintf(stderr, "foh_app: --bridge %s but the flow performed a VS "
                    "launch (cross-guard)\n", bridge);
    return 4;
  }
  if (wantVerify && cpuLive != (foh.p2Type == 1)) {
    fprintf(stderr, "foh_app: --cpu-live must match the FOH P2 type "
                    "(cross-guard)\n");
    return 4;
  }

  sim_boot_page(&G);
  sim_data_load(simdataPath);
  sim_data_register();
  if (anyVerify) load_trace(tracePath);

  if (wantTState || wantTVerify) {
    // --- the TARGET launch bridge (target_main.c boot parity) --------------
    ml_active_rng = &G.rng;
    ml_rng_seed(&G.rng, (uint32_t)seed);
    for (int k = 0; k < ML_BOOT_DRAWS; k++) (void)ml_rng_next(&G.rng);
    G.rngStateAtReset = G.rng.a;
    // task 13: the finishGame record arm rides the REAL seam (host
    // twin of foh_dev's hook; never fires on the committed traces).
    tp_finish_hook = app_persist_finish_hook;
    // THE BRIDGE POINT: char + tstage from the FOH state, never CLI
    // (tp_setup_target consumes the ONE off-step background draw).
    tp_setup_target(&G, foh.p1Char, foh.tssStage);
    G.rngStateAtFrame1 = G.rng.a;
    // TBRIDGE-STATE witness: read back from GameState + the target
    // module (never from the FOH struct) — proves the launch plumbing
    // reached the target sim slice.
    {
      FILE *bf = fopen(bstateOut, "w");
      if (!bf) sim_fatal("cannot open --bstate-out for writing");
      if (fprintf(bf,
                  "TBRIDGE-STATE char=%d tstage=%d gamemode=%d targets=%d "
                  "playing=%d starting=%d stocks=%d\n",
                  (int)G.sim.characterSelections[0],
                  (int)TP.targetStagePlaying, (int)G.sim.gameMode,
                  TP.targetCount, G.inp.playing ? 1 : 0,
                  G.starting ? 1 : 0, (int)G.sim.player[0].stocks) < 0) {
        sim_fatal("--bstate-out write failed");
      }
      if (fclose(bf) != 0) sim_fatal("--bstate-out close/flush failed");
    }
    if (!wantTVerify) return 0;

    // Full target replay (target_main.c loop shape; stream file only —
    // the EXACT wrap-target.js producer grammar).
    const size_t streamCap = (size_t)frames * 160 + 160;
    char *stream = malloc(streamCap);
    if (!stream) sim_fatal("oom (stream buffer)");
    size_t streamLen = 0;
    char hex[65], thex[65];
    for (long f = 0; f < frames; f++) {
      const long idx = f < g_trace_len - 1 ? f : g_trace_len - 1;
      const TraceRow *row = &g_trace[idx];
      if (row->present[1] || row->present[2] || row->present[3]) {
        sim_fatal("target trace with a non-null slot 1-3 row");
      }
      G.frame = f + 1;
      tp_game_tick_target(&G, row->present[0] ? &row->in[0] : 0);
      sim_frame_hash(&G, hex);
      tp_target_frame_hash(&G, thex);
      const int w = snprintf(stream + streamLen, streamCap - streamLen,
                             "F %ld %s\nT %ld %s\n", f + 1, hex, f + 1, thex);
      if (w < 0 || (size_t)w >= streamCap - streamLen) {
        sim_fatal("stream buffer overflow");
      }
      streamLen += (size_t)w;
    }
    const uint32_t total = draws_between(G.rngStateAtReset, G.rng.a);
    const uint32_t outside =
        draws_between(G.rngStateAtReset, G.rngStateAtFrame1);
    {
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
    FILE *of = fopen(outPath, "w");
    if (!of) sim_fatal("cannot open --out for writing");
    if (fwrite(stream, 1, streamLen, of) != streamLen) {
      sim_fatal("--out write failed");
    }
    if (fclose(of) != 0) sim_fatal("--out close/flush failed");
    free(stream);
    return 0;
  }

  // Seed + boot draws only NOW (the FOH machine drew nothing; header
  // note): the launched stream's rng domain == the harness domain.
  ml_active_rng = &G.rng;
  ml_rng_seed(&G.rng, (uint32_t)seed);
  for (int k = 0; k < ML_BOOT_DRAWS; k++) (void)ml_rng_next(&G.rng);
  G.rngStateAtReset = G.rng.a;

  // THE BRIDGE POINT: every parameter below comes from the FOH state.
  sim_setup_match(&G, foh.p1Char, foh.p2Char, foh.p2Type, foh.difficulty,
                  foh.stageSel);
  // FOH gameSettings applied AFTER setup (which writes the defaults) —
  // the gfx_app --tapjump-off-p1 precedent.
  G.sim.turbo = foh.turbo != 0;
  G.sim.lCancelType = foh.lCancelType;
  for (int i = 0; i < 4; i++) G.sim.tapJumpOff[i] = foh.tapJumpOff[i];
  // phantomThreshold has no widget (MENU-SPEC §3.2) but IS persisted and
  // IS on the checksum surface, so the value the file round-trips is the
  // value the sim gets — not a second, independently-defaulted copy in
  // sim_boot.c:435. Both are 0.01 today; the point is that they cannot
  // silently disagree tomorrow (the qjs Number("")-zeroing class).
  G.sim.phantomThreshold = foh.phantomThreshold;
  G.rngStateAtFrame1 = G.rng.a;

  // BRIDGE-STATE witness: read back from the GameState (never from the
  // FOH struct) — proves the launch plumbing reached the sim slice.
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

  if (!wantVerify) return 0;

  // Full-stream replay (sim_main.c loop shape; stream file only).
  const size_t streamCap = (size_t)frames * 80 + 128;
  char *stream = malloc(streamCap);
  if (!stream) sim_fatal("oom (stream buffer)");
  size_t streamLen = 0;
  char hex[65];
  for (long f = 0; f < frames; f++) {
    const long idx = f < g_trace_len - 1 ? f : g_trace_len - 1;
    const TraceRow *row = &g_trace[idx];
    const MlInput *rows[4];
    for (int i = 0; i < 4; i++) rows[i] = row->present[i] ? &row->in[i] : 0;
    G.frame = f + 1;
    sim_game_tick(&G, rows);
    sim_frame_hash(&G, hex);
    const int w = snprintf(stream + streamLen, streamCap - streamLen,
                           "F %ld %s\n", f + 1, hex);
    if (w < 0 || (size_t)w >= streamCap - streamLen) {
      sim_fatal("stream buffer overflow");
    }
    streamLen += (size_t)w;
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
  FILE *of = fopen(outPath, "w");
  if (!of) sim_fatal("cannot open --out for writing");
  if (fwrite(stream, 1, streamLen, of) != streamLen) {
    sim_fatal("--out write failed");
  }
  if (fclose(of) != 0) sim_fatal("--out close/flush failed");
  free(stream);
  return 0;
}
