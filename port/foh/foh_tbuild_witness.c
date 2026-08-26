// port/foh/foh_tbuild_witness.c — A45 T3/T4's witness.
//
// WHAT IT ASSERTS IS WHAT THE OWNER WOULD DO. He clicked TARGET BUILDER and
// reported *"nothing happened"*. So this drives the REAL `foh_tick` from a
// cold `foh_init`, along the real menu path, and requires:
//
//   [1] menu-top row 2 OPENS THE EDITOR (it used to `deny` + refuse)
//   [2] the TARGET tool places a target where the crosshair is
//   [3] the MOVE tool grabs it, moves it, and drops it
//   [4] the DELETE tool removes it
//   [5] SAVE writes custom0.mlstage through the pause menu
//   [6] the file is BYTE-VALID: header, one code line, correct SUM
//   [7] target-select's slot 10 FLIPS TO THE CUSTOM PAGE (it used to refuse)
//       and A on the saved slot LAUNCHES it with tstage == 10
//   [8] every refusal is VISIBLE — a string in FohState.tbMsg, never a sound
//       alone. That failure mode is the reason the ticket exists.
//   [9] D43: writing one slot does not disturb another, on any path.
//
// NOTHING IS HAND-POKED. The only things written directly into FohState are
// the two the player's hand owns — the target-select cursor position and,
// in [2]/[3], the crosshair, which is exactly what holding a direction
// produces (holding it for the hundreds of frames a full traversal takes
// would make this check a stopwatch, not a proof; the crosshair's INTEGRATION
// is asserted separately in [2a] by actually holding the d-pad).
//
// Prints one line per assertion and `TBUILD OK` on success, exit 0. Any
// failure prints `TBUILD FAIL: ...` and exits 1. Its negative tests (teeth)
// live in port/foh/check-tbuild.sh.
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../gfx/raster.h"
#include "foh.h"
#include "foh_persist.h"
#include "foh_tbuild.h"

void gfx_fatal(const char *what) {
  fprintf(stderr, "TBUILD FAIL: gfx_fatal: %s\n", what);
  exit(3);
}

static int g_fails;
static void ok(const char *what) { printf("  ok  %s\n", what); }
static void bad(const char *what) {
  printf("TBUILD FAIL: %s\n", what);
  g_fails++;
}
static void want(int cond, const char *what) {
  if (cond) ok(what); else bad(what);
}

// --- driving the real machine ------------------------------------------------
static void tick_neutral(FohState *s, int n) {
  PlatformInput in;
  memset(&in, 0, sizeof in);
  for (int i = 0; i < n; i++) foh_tick(s, &in);
}

// SOUNDS ARE LATCHED ACROSS THE WHOLE PRESS. foh_tick clears s->nsnd at the
// top of every tick, and a press is two ticks (down, then up), so reading
// s->snd afterwards would only ever see the RELEASE frame — which is silent.
// That is a witness bug that would have made every sound assertion below
// vacuously false, so the queue is drained into g_snd on each tick instead.
#define WIT_SND_CAP 32
static const char *g_snd[WIT_SND_CAP];
static int g_nsnd;
static void snd_drain(const FohState *s) {
  for (int i = 0; i < s->nsnd && g_nsnd < WIT_SND_CAP; i++) {
    g_snd[g_nsnd++] = s->snd[i];
  }
}

static void press(FohState *s, size_t off) {
  PlatformInput in;
  g_nsnd = 0;
  memset(&in, 0, sizeof in);
  *(bool *)((char *)&in + off) = true;
  foh_tick(s, &in);
  snd_drain(s);
  memset(&in, 0, sizeof in);
  foh_tick(s, &in);
  snd_drain(s);
}
#define PRESS(s, field) press((s), offsetof(PlatformInput, field))

static void hold(FohState *s, size_t off, int n) {
  PlatformInput in;
  memset(&in, 0, sizeof in);
  *(bool *)((char *)&in + off) = true;
  for (int i = 0; i < n; i++) foh_tick(s, &in);
}
#define HOLD(s, field, n) hold((s), offsetof(PlatformInput, field), (n))

static bool sound_fired(const FohState *s, const char *name) {
  (void)s;
  for (int i = 0; i < g_nsnd; i++) {
    if (strcmp(g_snd[i], name) == 0) return true;
  }
  return false;
}

// Boot -> title -> menu-top, the real path.
static void to_menu(FohState *s) {
  foh_init(s);
  tick_neutral(s, 380); // startUpTimer 370 (startup.js:50)
  PRESS(s, start);      // title -> menu-top (main.js:385)
}

// ... and on into the editor via row 2 (menu.js:87-90).
static void to_builder(FohState *s) {
  to_menu(s);
  PRESS(s, down); // row 0 -> 1
  PRESS(s, down); // row 1 -> 2 (TARGET BUILDER)
  PRESS(s, a);
}

// Put the crosshair on a world point WITHOUT hand-poking: hold the d-pad.
// The grid is left at its default, so the landing point is the snapped one.
static bool crosshair_to_held(FohState *s, double wx, double wy, bool holdA) {
  PlatformInput in;
  for (int axis = 0; axis < 2; axis++) {
    double prev = 1e300;
    for (int i = 0; i < 4000; i++) {
      const double cur = axis == 0 ? s->tbX : s->tbY;
      const double aim = axis == 0 ? wx : wy;
      const double d = cur - aim;
      if (d > -0.5 && d < 0.5) break;
      if (cur == prev) break; // clamped — stop rather than spin
      prev = cur;
      memset(&in, 0, sizeof in);
      // X is the FINE modifier (D50), which is what makes a small target
      // reachable at all: without it one frame moves 5 units.
      in.x = true;
      in.a = holdA; // A45 T5: a drag is A HELD ACROSS the movement
      if (axis == 0) { if (cur < aim) in.right = true; else in.left = true; }
      else { if (cur < aim) in.up = true; else in.down = true; }
      foh_tick(s, &in);
    }
  }
  if (!holdA) tick_neutral(s, 1);
  const double dx = s->tbX - wx, dy = s->tbY - wy;
  return dx > -6.0 && dx < 6.0 && dy > -6.0 && dy < 6.0;
}

static bool crosshair_to(FohState *s, double wx, double wy) {
  return crosshair_to_held(s, wx, wy, false);
}

// A45 T5: press A at (x0,y0), drag to (x1,y1), release. The three-phase
// shape is upstream's own (initiate on the A EDGE, stretch while HELD,
// build on RELEASE) and is why a drag cannot be written as two presses.
static void drag(FohState *s, double x0, double y0, double x1, double y1) {
  PlatformInput in;
  crosshair_to(s, x0, y0);
  memset(&in, 0, sizeof in);
  in.a = true;
  foh_tick(s, &in); // the A edge: initiate
  snd_drain(s);
  crosshair_to_held(s, x1, y1, true);
  g_nsnd = 0;
  memset(&in, 0, sizeof in);
  foh_tick(s, &in); // release
  snd_drain(s);
  tick_neutral(s, 1);
}

// Count the segments of one collision list in the view.
static int tb_lines(const FohState *s, int kind) {
  static FohTbView v;
  if (!foh_tbuild_ops) return -1;
  foh_tbuild_ops->view(s, &v);
  int n = 0;
  for (int i = 0; i < v.nLine; i++) {
    if (v.lineKind[i] == kind) n++;
  }
  return n;
}

// Every COLLISION segment (the five physics lists), which is what a polygon
// adds to. Counting one list is wrong: which list an edge lands in depends
// on its ANGLE and on the loop's winding (:351-367), so a triangle need not
// produce a ground at all.
static int tb_collision(const FohState *s) {
  static FohTbView v;
  if (!foh_tbuild_ops) return -1;
  foh_tbuild_ops->view(s, &v);
  int n = 0;
  for (int i = 0; i < v.nLine; i++) {
    if (v.lineKind[i] != FOH_TB_H_LINE) n++;
  }
  return n;
}

static int tb_polys(const FohState *s) {
  static FohTbView v;
  if (!foh_tbuild_ops) return -1;
  foh_tbuild_ops->view(s, &v);
  return v.nPoly;
}

static int tb_ledges(const FohState *s) {
  static FohTbView v;
  if (!foh_tbuild_ops) return -1;
  foh_tbuild_ops->view(s, &v);
  return v.nLedge;
}

static int tb_damaged(const FohState *s) {
  static FohTbView v;
  if (!foh_tbuild_ops) return -1;
  foh_tbuild_ops->view(s, &v);
  int n = 0;
  for (int i = 0; i < v.nLine; i++) {
    if (v.lineDamage[i] >= 0) n++;
  }
  return n;
}

// X + shoulder is the TYPE cycle (DEVIATION D54): the same shoulder that
// cycles the tool when X is not held.
static void type_next(FohState *s) {
  PlatformInput in;
  g_nsnd = 0;
  memset(&in, 0, sizeof in);
  in.x = true;
  foh_tick(s, &in); // X down, no shoulder yet: no edge
  in.r = true;
  foh_tick(s, &in);
  snd_drain(s);
  memset(&in, 0, sizeof in);
  foh_tick(s, &in);
  snd_drain(s);
}

static int tb_targets(const FohState *s) {
  static FohTbView v;
  if (!foh_tbuild_ops) return -1;
  foh_tbuild_ops->view(s, &v);
  return v.nTarget;
}

// Walk the pause menu to a row and open it. Rows wrap, so DOWN reaches all.
static void pause_row(FohState *s, int row) {
  for (int i = 0; i < FOH_TB_PAUSE_ROWS && s->tbPauseRow != row; i++) {
    PRESS(s, down);
  }
}

// Cycle R until the wanted tool is current. Explicit, because a leg that
// ASSUMES which tool it inherited breaks the moment an earlier leg changes
// one — measured: it did, and it cascaded five assertions.
// FOH_TB_TOOL_IDS is the id SPACE (upstream's ten); the cycle may be
// shorter if a tool is not built. Bounding the walk by the id space is
// therefore always enough and never assumes the cycle's length.
static void tool_to(FohState *s, int tool) {
  for (int i = 0; i < FOH_TB_TOOL_IDS && s->tbTool != tool; i++) PRESS(s, r);
}

static void pane_row(FohState *s, int row) {
  for (int i = 0; i < FOH_TB_SLOTS && s->tbPaneRow != row; i++) PRESS(s, down);
}

// --- the .mlstage file, read as BYTES ----------------------------------------
// Deliberately NOT through the builder's own reader: [6] is asserting that
// what landed on disk matches A45 T2's published contract, and a reader
// checking its own writer would prove only that they agree with each other.
static long slurp(const char *path, char *buf, long cap) {
  FILE *f = fopen(path, "rb");
  if (!f) return -1;
  const long n = (long)fread(buf, 1, (size_t)cap, f);
  fclose(f);
  return n;
}

// --- the differential's FOH half --------------------------------------------
// `--probe <slot>` runs the BUILDER's reader over one slot and prints its
// verdict in the same shape port/foh/tbuild/mlstage_probe.c prints the SIM's.
// check-tbuild.sh runs both over one corpus and requires them to agree.
//
// It goes through `enter()`, the REAL load path a player takes, not a private
// entry point — a differential against a back door proves nothing about the
// door the player uses.
static int probe_mode(int slot) {
  static FohState s;
  foh_init(&s);
  if (!foh_tbuild_ops) {
    printf("FOHLOAD REFUSED engine unlinked\n");
    return 1;
  }
  foh_tbuild_ops->enter(&s, slot);
  if (s.tbSlot == slot) {
    static FohTbView v;
    foh_tbuild_ops->view(&s, &v);
    printf("FOHLOAD OK targets=%d sp=%d ground=%d\n", v.nTarget, v.nSp,
           v.nLine);
    return 0;
  }
  printf("FOHLOAD REFUSED %s\n", s.tbMsg ? s.tbMsg : "?");
  return 1;
}

int main(int argc, char **argv) {
  const char *dir = getenv("MLFK_PERSIST_DIR");
  if (!dir || !dir[0]) {
    fprintf(stderr, "TBUILD FAIL: MLFK_PERSIST_DIR must be set (hermetic)\n");
    return 3;
  }
  if (argc == 3 && strcmp(argv[1], "--probe") == 0) {
    return probe_mode(atoi(argv[2]));
  }
  char p0[1024], p3[1024];
  snprintf(p0, sizeof p0, "%s/custom0.mlstage", dir);
  snprintf(p3, sizeof p3, "%s/custom3.mlstage", dir);

  static FohState s;

  // --- [1] the ticket: TARGET BUILDER opens an editor ------------------------
  printf("== [1] menu-top row 2 opens the editor (was: deny + refused)\n");
  to_builder(&s);
  want(s.screen == FOH_TBUILD, "TARGET BUILDER -> the target-builder screen");
  want(strcmp(foh_screen_token(s.screen), "target-builder") == 0,
       "...and its token is target-builder");
  {
    // The refusal is GONE, not merely quieter: no event may name it.
    bool refused = false;
    for (int i = 0; i < s.nev; i++) {
      if (s.ev[i].sval && strcmp(s.ev[i].sval, "targetbuilder") == 0) {
        refused = true;
      }
    }
    want(!refused, "...and no `refused targetbuilder` event is emitted");
  }
  want(foh_tbuild_ops != 0, "the builder engine TU is linked");
  if (!foh_tbuild_ops) {
    printf("TBUILD FAIL: engine unlinked — nothing below can be judged\n");
    return 1;
  }
  want(tb_targets(&s) == 0, "a fresh document has no targets");

  // --- [2] TARGET: place --------------------------------------------------
  printf("== [2] the TARGET tool places targets at the crosshair\n");
  // A45 T5-T8 completed upstream's ten tools, so the port now opens where
  // UPSTREAM opens: `targetTool = 0` is POLYGON (targetbuilder.js:26/:36).
  // A45 T4 shipped three tools and renumbered them, and this assertion used
  // to encode that renumbering; with the set complete, upstream's own
  // default is the faithful one.
  want(s.tbTool == FOH_TB_TOOL_POLYGON, "the editor opens on POLYGON (:26)");
  tool_to(&s, FOH_TB_TOOL_TARGET);
  want(s.tbTool == FOH_TB_TOOL_TARGET, "R reaches the TARGET tool");
  {
    const double x0 = s.tbX, y0 = s.tbY;
    HOLD(&s, right, 4);
    tick_neutral(&s, 1);
    want(s.tbX != x0 || s.tbY != y0,
         "[2a] holding the d-pad MOVES the crosshair (real integration)");
    (void)y0;
  }
  // THE GRID IS REAL AND IT IS ON BY DEFAULT. gridSizes[0] is 80 canvas px
  // (targetbuilder.js:80), which at scale 3 is 26.67 WORLD units with a
  // (600 % 80)/3 phase — so an arbitrary world point is NOT reachable, by
  // design. Assert that, then cycle Y to free movement and aim precisely.
  want(s.tbGrid == 0, "the editor opens on the coarsest grid (:81 gridType)");
  {
    const double snapped = s.tbX;
    HOLD(&s, right, 1);
    tick_neutral(&s, 1);
    want(s.tbX == snapped || s.tbX - snapped > 20.0 ||
             snapped - s.tbX > 20.0,
         "...and one d-pad frame either snaps back or jumps a whole cell");
  }
  for (int i = 0; i < 4; i++) PRESS(&s, y);
  want(s.tbGrid == 4, "Y cycles the grid to FREE (:207-212, moved off z)");
  want(sound_fired(&s, "menuSelect"), "...playing menuSelect");
  want(crosshair_to(&s, 0.0, 20.0), "the crosshair reaches (0, 20)");
  PRESS(&s, a);
  want(tb_targets(&s) == 1, "A places one target");
  want(sound_fired(&s, "blunthit"), "...playing blunthit (:566)");
  want(crosshair_to(&s, -40.0, 20.0), "the crosshair reaches (-40, 20)");
  PRESS(&s, a);
  want(tb_targets(&s) == 2, "A places a second target");

  // --- [3] MOVE: grab, drag, drop -----------------------------------------
  printf("== [3] the MOVE tool grabs a target and moves it\n");
  PRESS(&s, r);
  want(s.tbTool == FOH_TB_TOOL_MOVE, "R cycles TARGET -> MOVE");
  tick_neutral(&s, 1);
  want(s.tbHoverKind != FOH_TB_H_NONE,
       "the crosshair sitting on a target HOVERS it (findTarget, :1465)");
  {
    static FohTbView v;
    foh_tbuild_ops->view(&s, &v);
    const double before = v.tx[1];
    // hold A across several frames — a grab is a HOLD, not an edge
    PlatformInput in;
    memset(&in, 0, sizeof in);
    in.a = true;
    foh_tick(&s, &in);
    want(s.tbHoldA, "A down starts the grab (holdingA, :597)");
    for (int i = 0; i < 30; i++) {
      memset(&in, 0, sizeof in);
      in.a = true;
      in.x = true;
      in.down = true;
      foh_tick(&s, &in);
    }
    tick_neutral(&s, 1); // release
    want(!s.tbHoldA, "releasing A ends the grab (:606-608)");
    foh_tbuild_ops->view(&s, &v);
    want(v.ty[1] != 20.0 || v.tx[1] != before,
         "...and the target moved with the crosshair (centerItem, :1550)");
    want(v.nTarget == 2, "...and MOVE created nothing");
  }

  // --- [4] DELETE ----------------------------------------------------------
  printf("== [4] the DELETE tool removes the hovered target\n");
  PRESS(&s, r);
  want(s.tbTool == FOH_TB_TOOL_DELETE, "R cycles MOVE -> DELETE");
  tick_neutral(&s, 1);
  want(s.tbHoverKind != FOH_TB_H_NONE, "DELETE hovers the target under the crosshair");
  PRESS(&s, a);
  want(tb_targets(&s) == 1, "A deletes it (:656-658 splice)");

  // --- [8a] the 11th target refuses, VISIBLY -------------------------------
  printf("== [8a] the 11th target refuses ON SCREEN (R2's safe half)\n");
  PRESS(&s, l); // DELETE -> MOVE
  PRESS(&s, l); // MOVE -> TARGET (upstream's 7 -> 6 -> 5)
  want(s.tbTool == FOH_TB_TOOL_TARGET, "L cycles back to TARGET");
  for (int i = 0; i < 12; i++) {
    // Every press is at a slightly different place, so none of them is a
    // no-op for a reason other than the cap.
    crosshair_to(&s, -60.0 + i * 10.0, 40.0);
    PRESS(&s, a);
  }
  want(tb_targets(&s) == 10,
       "the document stops at 10 targets — the SIM's ML_MAX_TARGETS");
  want(s.tbMsgTimer > 0 && s.tbMsg && strcmp(s.tbMsg, "10 targets max") == 0,
       "...and the refusal is a STRING ON SCREEN, not just a deny sound");
  want(sound_fired(&s, "deny"), "...with the deny sound as well");

  // --- [5] SAVE through the pause menu -------------------------------------
  printf("== [5] START -> SAVE -> slot 0 writes the file\n");
  PRESS(&s, start);
  want(s.tbPaused, "START opens the pause menu (:774-777)");
  want(sound_fired(&s, "pause"), "...playing `pause`");
  pause_row(&s, FOH_TB_PAUSE_SAVE);
  want(s.tbPauseRow == FOH_TB_PAUSE_SAVE, "the cursor reaches SAVE");
  PRESS(&s, a);
  want(s.tbPane == FOH_TB_PANE_SAVE, "SAVE opens the slot list");
  pane_row(&s, 0);
  PRESS(&s, a);
  want(s.tbMsg && strcmp(s.tbMsg, "saved") == 0, "slot 0 reports `saved`");
  want(!s.tbPaused, "...and the pause menu closes");
  want(s.tbSlot == 0, "...and the document now belongs to slot 0");

  // --- [6] what landed on disk --------------------------------------------
  printf("== [6] the file on disk is A45 T2's .mlstage, byte for byte\n");
  {
    static char buf[1 << 20];
    const long n = slurp(p0, buf, (long)sizeof buf);
    want(n > 0, "custom0.mlstage exists and is non-empty");
    if (n > 0) {
      want(n >= 9 && memcmp(buf, "MLSTAGE1\n", 9) == 0,
           "...line 1 is the MLSTAGE1 header");
      want(buf[n - 1] == '\n', "...the file ends with LF, no trailing byte");
      const long sumAt = n - 69;
      want(sumAt > 9 && memcmp(buf + sumAt, "SUM ", 4) == 0,
           "...the last line is `SUM <64 hex>`");
      want(memchr(buf + 9, '\n', (size_t)(sumAt - 9 - 1)) == 0,
           "...and the share code is exactly ONE line");
    }
    // The temp file must NOT survive a successful publish.
    char tmp[1024];
    snprintf(tmp, sizeof tmp, "%s/custom0.mlstage.tmp", dir);
    want(slurp(tmp, buf, 16) < 0, "...and no .tmp is left behind");
  }

  // --- [9] D43: one slot does not disturb another --------------------------
  printf("== [9] D43 — saving slot 3 leaves slot 0 exactly as it was\n");
  {
    static char before[1 << 20], after[1 << 20], other[1 << 20];
    const long nb = slurp(p0, before, (long)sizeof before);
    // THE DOCUMENT MUST DIFFER BETWEEN THE TWO SAVES, or a clobber would
    // write byte-identical bytes and this leg would agree with a broken
    // build. So delete one target first: slot 3 is now a DIFFERENT stage
    // from slot 0, and "slot 0 unchanged" has something to say.
    tool_to(&s, FOH_TB_TOOL_DELETE);
    {
      static FohTbView v;
      foh_tbuild_ops->view(&s, &v);
      crosshair_to(&s, v.tx[0], v.ty[0]);
      PRESS(&s, a);
    }
    want(tb_targets(&s) == 9, "one target is removed before the second save");
    PRESS(&s, start);
    pause_row(&s, FOH_TB_PAUSE_SAVE);
    PRESS(&s, a);
    pane_row(&s, 3);
    PRESS(&s, a);
    want(s.tbMsg && strcmp(s.tbMsg, "saved") == 0, "slot 3 saves");
    const long no = slurp(p3, other, (long)sizeof other);
    want(no > 0, "custom3.mlstage exists");
    want(no != nb || memcmp(before, other, (size_t)no) != 0,
         "...and holds a DIFFERENT stage from slot 0 (so the next test bites)");
    const long na = slurp(p0, after, (long)sizeof after);
    want(na == nb && na > 0 && memcmp(before, after, (size_t)na) == 0,
         "...and custom0.mlstage is BYTE-IDENTICAL (no clobber, no shift)");
  }

  // --- [8b] an unplayable stage refuses to save, VISIBLY -------------------
  printf("== [8b] saving a stage with no targets refuses ON SCREEN\n");
  {
    // Delete all ten through the real DELETE tool, then try to save.
    tool_to(&s, FOH_TB_TOOL_DELETE);
    want(s.tbTool == FOH_TB_TOOL_DELETE, "the DELETE tool is selected");
    for (int guard = 0; guard < 40 && tb_targets(&s) > 0; guard++) {
      static FohTbView v;
      foh_tbuild_ops->view(&s, &v);
      crosshair_to(&s, v.tx[0], v.ty[0]);
      if (s.tbHoverKind == FOH_TB_H_NONE) break;
      PRESS(&s, a);
    }
    want(tb_targets(&s) == 0, "every target is deleted");
    PRESS(&s, start);
    pause_row(&s, FOH_TB_PAUSE_SAVE);
    PRESS(&s, a);
    pane_row(&s, 5);
    PRESS(&s, a);
    want(s.tbMsg && strcmp(s.tbMsg, "place at least 1 target") == 0,
         "...SAVE names the rule on screen");
    want(s.tbPane == FOH_TB_PANE_SAVE, "...and stays in the slot list");
    char p5[1024];
    static char junk[64];
    snprintf(p5, sizeof p5, "%s/custom5.mlstage", dir);
    want(slurp(p5, junk, 16) < 0, "...and NOTHING was written to slot 5");
  }

  // --- [5b] LOAD brings a saved stage back ---------------------------------
  printf("== [5b] LOAD reads slot 0 back into the editor\n");
  PRESS(&s, b); // out of the slot list
  pause_row(&s, FOH_TB_PAUSE_LOAD);
  PRESS(&s, a);
  pane_row(&s, 0);
  PRESS(&s, a);
  want(s.tbMsg && strcmp(s.tbMsg, "loaded") == 0, "slot 0 reports `loaded`");
  want(tb_targets(&s) == 10, "...with all ten targets back");
  want(s.tbSlot == 0, "...and editingStage follows the slot");

  // --- [5c] DELETE removes one slot and only that slot ---------------------
  printf("== [5c] the pause menu's DELETE removes slot 3, and only slot 3\n");
  {
    static char before[1 << 20], after[1 << 20];
    const long nb = slurp(p0, before, (long)sizeof before);
    PRESS(&s, start);
    pause_row(&s, FOH_TB_PAUSE_DELETE);
    PRESS(&s, a);
    pane_row(&s, 3);
    PRESS(&s, a);
    want(s.tbMsg && strcmp(s.tbMsg, "deleted") == 0, "slot 3 reports `deleted`");
    static char junk[64];
    want(slurp(p3, junk, 16) < 0, "...custom3.mlstage is gone");
    const long na = slurp(p0, after, (long)sizeof after);
    want(na == nb && na > 0 && memcmp(before, after, (size_t)na) == 0,
         "...and slot 0 is BYTE-IDENTICAL — no shift (D43 vs :83-97)");
    // deleting an empty slot refuses, visibly
    pane_row(&s, 7);
    PRESS(&s, a);
    want(s.tbMsg && strcmp(s.tbMsg, "empty") == 0,
         "...and deleting an empty slot says `empty` on screen");
  }

  // --- [7] the owner's payoff: play the slot from Target Test --------------
  printf("== [7] target-select's CUSTOM page plays the saved slot\n");
  PRESS(&s, b);     // close the slot list
  PRESS(&s, b);     // close the pause menu
  PRESS(&s, b);     // leave the builder
  want(s.screen == FOH_MENU_TOP, "B leaves the builder for menu-top (D50)");
  want(s.menuSelected == 2, "...landing back on the TARGET BUILDER row");
  PRESS(&s, down);  // row 2 -> 3
  PRESS(&s, up);    // back to 2
  PRESS(&s, up);    // -> 1 (TARGET TEST)
  PRESS(&s, a);
  want(s.screen == FOH_TSS, "TARGET TEST opens target-select");
  want(s.tssPage == 0, "...on the AUTHORED page");
  // walk the hand onto slot 10 and press A
  {
    FohHandRect slot[FOH_TSS_SLOTS];
    foh_tss_slots(slot);
    s.tssHandX = slot[10].x + slot[10].w / 2.0;
    s.tssHandY = slot[10].y + slot[10].h / 2.0;
    tick_neutral(&s, 1);
    want(s.tssCursor == 10, "the hand reaches the page-flip slot");
    PRESS(&s, a);
    want(s.tssPage == 1, "A flips to the CUSTOM page (was: refused addcode)");
    bool refused = false;
    for (int i = 0; i < s.nev; i++) {
      if (s.ev[i].sval && strcmp(s.ev[i].sval, "addcode") == 0) refused = true;
    }
    want(!refused, "...and no `refused addcode` event is emitted");
    want(s.tssSlotPresent[0], "...slot 0 shows as present");
    want(!s.tssSlotPresent[3], "...slot 3 (deleted) shows as absent");
    want(s.tssSlotReason[3] != 0, "...and names why it is absent");
    // an ABSENT slot refuses and launches nothing
    s.tssHandX = slot[3].x + slot[3].w / 2.0;
    s.tssHandY = slot[3].y + slot[3].h / 2.0;
    tick_neutral(&s, 1);
    PRESS(&s, a);
    want(!s.launched && s.screen == FOH_TSS,
         "A on an EMPTY custom slot launches nothing");
    // the present one launches
    s.tssHandX = slot[0].x + slot[0].w / 2.0;
    s.tssHandY = slot[0].y + slot[0].h / 2.0;
    tick_neutral(&s, 1);
    want(s.tssCursor == 0, "the hand reaches custom slot 0");
    PRESS(&s, a);
    want(s.screen == FOH_TMATCH, "A LAUNCHES it");
    want(s.launched && s.targetMode, "...as a target match");
    want(s.tssStage == FOH_TB_SLOT_CACHE + 0,
         "...carrying tstage 10 == MLK_PLAYING_BASE + slot (A45 T2)");
  }

  // --- [1b] the authored page is untouched --------------------------------
  printf("== [1b] the AUTHORED page still launches authored stages\n");
  {
    foh_init(&s);
    tick_neutral(&s, 380);
    PRESS(&s, start);
    PRESS(&s, down);
    PRESS(&s, a);
    want(s.screen == FOH_TSS, "target-select opens");
    PRESS(&s, a); // hand is homed on slot 0
    want(s.screen == FOH_TMATCH && s.tssStage == 0,
         "A on authored slot 0 still launches tstage 0");
  }

  // --- [9] A45 T5-T8: the seven tools T4 did not ship ---------------------
  //
  // Driven through the real foh_tick like every other leg. A tool that is
  // built but never exercised is a claim, not evidence — and this file is
  // the only thing standing between "the tool cycle has ten entries" and
  // "seven of them do something".
  printf("== [9] T5-T8: PLATFORM, WALL, LEDGE, DAMAGE, POLYGON, SCALE, DRAW\n");
  {
    foh_init(&s);
    to_builder(&s);
    for (int i = 0; i < 4; i++) PRESS(&s, y); // grid -> FREE, so aim is exact
    want(s.tbGrid == 4, "[9] the grid is free");

    // --- T5 PLATFORM (:412-458) --------------------------------------------
    tool_to(&s, FOH_TB_TOOL_PLATFORM);
    want(s.tbTool == FOH_TB_TOOL_PLATFORM, "the PLATFORM tool is reachable");
    const int plat0 = tb_lines(&s, FOH_TB_H_PLATFORM);
    drag(&s, -60.0, 40.0, -20.0, 40.0);
    want(tb_lines(&s, FOH_TB_H_PLATFORM) == plat0 + 1,
         "a wide shallow drag builds a platform (:441 |angle| <= PI/6)");
    want(sound_fired(&s, "blunthit"), "...playing blunthit on release (:453)");
    // :429 — the WIDTH test is in CANVAS px and needs >= 10, i.e. >= 3.33
    // world units at scale 3. One world unit is 3 canvas px: too small.
    const int plat1 = tb_lines(&s, FOH_TB_H_PLATFORM);
    drag(&s, 20.0, 40.0, 21.0, 40.0);
    want(tb_lines(&s, FOH_TB_H_PLATFORM) == plat1,
         "a 1-unit drag builds nothing (:429 width < 10 canvas px)");
    want(s.tbMsgTimer > 0 && s.tbMsg && strcmp(s.tbMsg, "too small") == 0,
         "...and says `too small` ON SCREEN, not just a sound");
    // a steep drag: wide enough, but past PI/6
    drag(&s, 40.0, 10.0, 70.0, 70.0);
    want(tb_lines(&s, FOH_TB_H_PLATFORM) == plat1,
         "a steep drag builds nothing (:441 the angle guard)");
    want(s.tbMsg && strcmp(s.tbMsg, "bad angle") == 0,
         "...and says `bad angle` ON SCREEN");

    // --- T5 WALL (:459-512) + DEVIATION D54 --------------------------------
    tool_to(&s, FOH_TB_TOOL_WALL);
    want(s.tbTool == FOH_TB_TOOL_WALL, "the WALL tool is reachable");
    want(s.tbWallType == 0, "...starting on GROUND (:27 wallTypeIndex)");
    type_next(&s);
    want(s.tbWallType == 1, "X+R cycles the wall TYPE, not the tool (D54)");
    want(s.tbTool == FOH_TB_TOOL_WALL, "...and the tool is unchanged");
    want(sound_fired(&s, "menuSelect"), "...playing menuSelect (:233)");
    PRESS(&s, r);
    want(s.tbTool != FOH_TB_TOOL_WALL,
         "R WITHOUT X still cycles the tool (D54 adds, never replaces)");
    tool_to(&s, FOH_TB_TOOL_WALL);
    type_next(&s); // -> wallL
    type_next(&s);
    want(s.tbWallType == 2 || s.tbWallType == 3,
         "the type cycle reaches the wall types");
    // BOUNDED, always. Under tooth T5 the builder is unreachable and this
    // state never changes — an unbounded `while` there is not a failing
    // assertion, it is a hung check. (Measured: it hung.)
    for (int i = 0; i < FOH_TB_WALLTYPES && s.tbWallType != 2; i++) type_next(&s);
    const int wl0 = tb_lines(&s, FOH_TB_H_WALLL);
    drag(&s, -80.0, -20.0, -80.0, 30.0);
    want(tb_lines(&s, FOH_TB_H_WALLL) == wl0 + 1,
         "a vertical drag builds a wallL (:493 non-axis angle)");
    // :481-492 — an L wall within 2 WORLD units of an R wall refuses. Build
    // the R wall first, then try an L wall on top of it.
    for (int i = 0; i < FOH_TB_WALLTYPES && s.tbWallType != 3; i++) type_next(&s);
    drag(&s, 60.0, -20.0, 60.0, 30.0);
    want(tb_lines(&s, FOH_TB_H_WALLR) >= 1, "...and a wallR the same way");
    for (int i = 0; i < FOH_TB_WALLTYPES && s.tbWallType != 2; i++) type_next(&s);
    const int wl1 = tb_lines(&s, FOH_TB_H_WALLL);
    drag(&s, 60.0, -20.0, 60.0, 30.0);
    want(tb_lines(&s, FOH_TB_H_WALLL) == wl1,
         "an L wall ON an R wall refuses (:484 lineDistanceToLines < 2)");
    want(s.tbMsg && strcmp(s.tbMsg, "walls too close") == 0,
         "...and says `walls too close` ON SCREEN");

    // --- T6 LEDGE (:513-541) -----------------------------------------------
    tool_to(&s, FOH_TB_TOOL_LEDGE);
    want(s.tbTool == FOH_TB_TOOL_LEDGE, "the LEDGE tool is reachable");
    // D51's template floor runs -100..100 at y = 0; its right END is where
    // a ledge belongs.
    const int led0 = tb_ledges(&s);
    crosshair_to(&s, 98.0, 0.0);
    tick_neutral(&s, 1);
    want(s.tbLedgeKind != FOH_TB_H_NONE,
         "hovering a ground END arms the ledge cursor (:516-523)");
    want(s.tbLedgeSide == 1, "...on the RIGHT end, by manhattan distance");
    PRESS(&s, a);
    want(tb_ledges(&s) == led0 + 1, "A adds a ledge (:534-537)");
    PRESS(&s, a);
    want(tb_ledges(&s) == led0,
         "...and A again REMOVES it — upstream's toggle (:526-533)");
    PRESS(&s, a); // leave one behind for the DELETE renumbering leg

    // --- T6 DAMAGE (:542-559) ----------------------------------------------
    tool_to(&s, FOH_TB_TOOL_DAMAGE);
    want(s.tbTool == FOH_TB_TOOL_DAMAGE, "the DAMAGE tool is reachable");
    want(s.tbDamageType == 0, "...starting on FIRE (:31 damageTypeIndex)");
    crosshair_to(&s, 0.0, 0.0);
    tick_neutral(&s, 1);
    want(s.tbHoverKind == FOH_TB_H_GROUND, "it hovers the floor");
    PRESS(&s, a);
    want(tb_damaged(&s) == 1, "A tags the surface with a damage type");
    PRESS(&s, a);
    want(tb_damaged(&s) == 0,
         "...and A again writes {damageType: null}, which is INERT (:554)");
    type_next(&s);
    want(s.tbDamageType == 1, "X+R cycles the damage TYPE (D54)");
    PRESS(&s, a);
    want(tb_damaged(&s) == 1, "...and the new type tags");
    // The SIM still refuses a damaging stage at load (A45 T6 owes the
    // golden), so SAVE must refuse it HERE, with the reason on screen —
    // never write a file that cannot be launched.
    PRESS(&s, start);
    pause_row(&s, FOH_TB_PAUSE_SAVE);
    PRESS(&s, a);
    pane_row(&s, 5);
    PRESS(&s, a);
    want(s.tbMsgTimer > 0 && s.tbMsg &&
             strstr(s.tbMsg, "damaging") != NULL,
         "SAVE refuses a damaging stage, naming the rule (R1 is still open)");
    want(s.screen == FOH_TBUILD, "...and stays in the builder");
    PRESS(&s, b);
    PRESS(&s, b); // out of the pane, out of the pause menu
    tool_to(&s, FOH_TB_TOOL_DAMAGE);
    crosshair_to(&s, 0.0, 0.0);
    tick_neutral(&s, 1);
    for (int i = 0; i < FOH_TB_DAMAGETYPES && s.tbDamageType != 1; i++) type_next(&s);
    PRESS(&s, a); // toggle the damage back off
    want(tb_damaged(&s) == 0, "the damage is cleared again");

    // --- T7 POLYGON (:277-411) ---------------------------------------------
    tool_to(&s, FOH_TB_TOOL_POLYGON);
    want(s.tbTool == FOH_TB_TOOL_POLYGON, "the POLYGON tool is reachable");
    const int poly0 = tb_polys(&s);
    const int g0 = tb_collision(&s);
    crosshair_to(&s, -40.0, -60.0);
    PRESS(&s, a);
    want(s.tbDrawingPoly, "A starts a polygon (:279-287)");
    crosshair_to(&s, 40.0, -60.0);
    PRESS(&s, a);
    crosshair_to(&s, 0.0, -20.0);
    PRESS(&s, a);
    want(tb_polys(&s) == poly0, "...three vertices, still open");
    // B pops one — DEVIATION D56: while drawing, B is upstream's own
    // vertex pop (:396-408), NOT the screen's back edge.
    PRESS(&s, b);
    want(s.screen == FOH_TBUILD, "B while drawing does NOT leave (D56)");
    want(s.tbDrawingPoly, "...it pops a vertex and keeps drawing");
    crosshair_to(&s, 0.0, -20.0);
    PRESS(&s, a);
    crosshair_to(&s, -40.0, -60.0); // back to the origin: close
    PRESS(&s, a);
    want(!s.tbDrawingPoly, "returning to the origin CLOSES it (:292, :305)");
    want(tb_polys(&s) == poly0 + 1, "...and the polygon is in the document");
    want(tb_collision(&s) >= g0 + 3,
         "...having classified its three edges into collision surfaces (:351-367)");
    // A polygon's edges are ground/ceiling/wall by ANGLE, so a closed loop
    // must produce more than one KIND — one kind would mean the classifier
    // collapsed.
    {
      int kinds = 0;
      if (tb_lines(&s, FOH_TB_H_GROUND) > 1) kinds++;   // + D51's floor
      if (tb_lines(&s, FOH_TB_H_CEILING) > 0) kinds++;
      if (tb_lines(&s, FOH_TB_H_WALLL) > wl1) kinds++;
      if (tb_lines(&s, FOH_TB_H_WALLR) > 1) kinds++;
      want(kinds >= 2, "...into MORE THAN ONE kind (the angle classifier)");
    }
    // DELETE removes the polygon AND everything polygonMap says it owns.
    const int gBefore = tb_collision(&s);
    tool_to(&s, FOH_TB_TOOL_DELETE);
    crosshair_to(&s, 0.0, -50.0); // inside the triangle
    tick_neutral(&s, 1);
    want(s.tbHoverKind == FOH_TB_H_POLYGON, "DELETE hovers the polygon");
    PRESS(&s, a);
    want(tb_polys(&s) == poly0, "A deletes it");
    want(tb_collision(&s) == gBefore - 3,
         "...taking its THREE owned surfaces with it (:696-731 polygonMap)");
    want(tb_ledges(&s) >= 1,
         "...and the ledge on the UNRELATED template floor survives");

    // --- T8 SCALE (:739-764) -----------------------------------------------
    tool_to(&s, FOH_TB_TOOL_SCALE);
    want(s.tbTool == FOH_TB_TOOL_SCALE, "the SCALE tool is reachable");
    {
      static FohTbView v;
      foh_tbuild_ops->view(&s, &v);
      const double sc0 = v.scale;
      const double cx = s.tbX, cy = s.tbY;
      HOLD(&s, up, 6); // :741-750 fires on the SIXTH frame
      foh_tbuild_ops->view(&s, &v);
      want(v.scale > sc0, "six d-pad frames zoom in by one step (:747)");
      want(s.tbX == cx && s.tbY == cy,
           "...and the crosshair is FROZEN while SCALE is active (:172-174)");
      HOLD(&s, up, 300); // far past the clamp
      foh_tbuild_ops->view(&s, &v);
      want(v.scale <= 6.0, "...clamped at 6 (:749-751)");
      HOLD(&s, down, 400);
      foh_tbuild_ops->view(&s, &v);
      want(v.scale >= 2.0, "...and at 2 going the other way (:759-761)");
      for (int i = 0; i < 64 && v.scale < 3.0; i++) {
        HOLD(&s, up, 6);
        foh_tbuild_ops->view(&s, &v);
      }
    }

    // --- T8 DRAW MODE (:765-770) -------------------------------------------
    tool_to(&s, FOH_TB_TOOL_DRAWMODE);
    want(s.tbTool == FOH_TB_TOOL_DRAWMODE, "the DRAW MODE tool is reachable");
    want(s.tbDrawMode == 0, "the collision plane is the default");
    PRESS(&s, a);
    want(s.tbDrawMode == 1, "A switches to the background plane");
    want(s.tbMsg && strstr(s.tbMsg, "background") != NULL,
         "...and SAYS SO — an invisible mode switch is a trap at 240px");
    // :226-227 and :269-273 — while drawMode is on, WALL/LEDGE/DAMAGE are
    // unreachable: cycling FORWARD out of PLATFORM skips straight to MOVE,
    // and the backward arm is caught by the second coercion. So tool_to()
    // can never land on WALL, and it exhausts its bound instead — assert
    // the reachable property directly rather than through the helper.
    tool_to(&s, FOH_TB_TOOL_PLATFORM);
    want(s.tbTool == FOH_TB_TOOL_PLATFORM, "PLATFORM is reachable in drawMode");
    PRESS(&s, r);
    want(s.tbTool == FOH_TB_TOOL_MOVE,
         "R from PLATFORM skips WALL/LEDGE/DAMAGE to MOVE (:226-227)");
    PRESS(&s, l);
    want(s.tbTool != FOH_TB_TOOL_DAMAGE && s.tbTool != FOH_TB_TOOL_LEDGE &&
             s.tbTool != FOH_TB_TOOL_WALL,
         "...and L back out of MOVE cannot land on one either (:269-273)");
    tool_to(&s, FOH_TB_TOOL_PLATFORM);
    // and a background LINE is a real thing the plane can hold
    {
      const int bl0 = tb_lines(&s, FOH_TB_H_LINE);
      drag(&s, -60.0, 70.0, -20.0, 70.0);
      want(tb_lines(&s, FOH_TB_H_LINE) == bl0 + 1,
           "the PLATFORM tool draws a background LINE while drawMode (:436)");
      want(tb_lines(&s, FOH_TB_H_PLATFORM) == plat1,
           "...and adds NO collision platform");
    }
    tool_to(&s, FOH_TB_TOOL_DRAWMODE);
    PRESS(&s, a);
    want(s.tbDrawMode == 0, "A switches back to the collision plane");
  }

  if (g_fails) {
    printf("TBUILD FAIL: %d assertion(s) failed\n", g_fails);
    return 1;
  }
  printf("TBUILD OK\n");
  return 0;
}
