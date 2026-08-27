// port/foh/foh_rebind_witness.c — punch-list A31 (DEVIATION D26): the
// Controls > HANDHELD screen's REAL rebinder.
//
// THE TICKET, in the owner's own words (2026-08-23)
// -------------------------------------------------
//   "controls in general (for funkey-s controls) you should be able to
//    rebind any of the 'active mappings'. currently you can't even go to
//    any of those rows, you can only change between 'style:' and 'mod'.
//    changing 'mod' changes the controls. we don't want that. get rid of
//    'mod' altogether as an option here. we should just be able to select
//    what we want and just be able to set any of the active mappings. on
//    that note, what is 'rebind: N/A'? why do we even have that section?
//    also, can we have a 'reset to defaults' button please."
//
// WHY THIS IS NOT A SCREEN CHECK
// ------------------------------
// The worst outcome available here is a rebinding UI that renders a new
// label and does not rebind anything — a screen that lies. So every claim
// below is BOUND to the play path: the witness navigates with real
// gestures through the real foh_tick, reads the new label off the rendered
// frame, and then pushes a PlatformInput with that PHYSICAL button held
// through THE PRODUCT CHAIN — ctl_bind_apply() then s1_input_row_style(),
// which is exactly what foh_dev.c's poll_bound() and its two match loops
// do (that pairing is pinned textually by check-rebind.sh leg [1]) — and
// asserts the resulting Melee-unit input row carries the NEW action.
// Nothing here pokes a binding cell directly.
//
// WHAT IT ASSERTS
//   [A31-1] every one of the eleven rows is REACHABLE with up/down, the
//           cursor wraps both ways, and the caret is DRAWN on the row the
//           state says it is on (screen and state cannot disagree);
//   [A31-2] L/R on an action row rebinds it: the two rows' labels SWAP on
//           screen AND the two physical buttons swap what they drive in
//           the product chain, in BOTH directions;
//   [A31-3] a rebind of a NON-sim action (START, the pause button
//           foh_dev.c's overlay edge reads) moves on the same plane;
//   [A31-4] the binding stays a PERMUTATION under arbitrary editing — no
//           action is ever lost, which is what makes "protected
//           primaries" unnecessary;
//   [A31-5] the d-pad row REFUSES (deny, no binding change): it drives the
//           control stick, not one of the eight buttons;
//   [A31-6] RESET TO DEFAULTS restores the EXACT default table — identity
//           binding, default style, ratified Mod arrangement — and the
//           screen redraws the identity labels;
//   [A31-7] the `mod` row is GONE from the screen while its CELL is
//           untouched (a UI removal, not a model removal);
//   [A31-8] `REBIND: N/A` is gone and the caption names a live control;
//   [A31-9] bindings SURVIVE A SAVE/LOAD ROUND TRIP through the one
//           persistence chokepoint (MLFKPERSIST5), a non-permutation row
//           is REFUSED as corruption, and a v4 file MIGRATES to the
//           identity rather than being discarded.
//
// THE INSTRUMENT is foh_controls_witness.c's, verbatim in method: a
// rendered string is asserted by OVERDRAWING the claimed string, at the
// claimed place, in the claimed colour, onto the real frame and requiring
// ZERO pixels to change. Text composites opaquely (foh_font.c), so drawing
// what is already there is a no-op and drawing anything else is not.
//
// Usage: MLFK_PERSIST_DIR=<fresh dir> foh_rebind_witness. Prints one line
// per assertion and `REBIND OK` on success, exit 0. Any failure prints
// `REBIND FAIL:` and exits 1. Its negative tests live in
// port/foh/check-rebind.sh.
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../gfx/ctl_style.h"
#include "../gfx/raster.h"
#include "../gfx/s1_input.h" // THE product chord resolver
#include "../sim/ml_ser.h" // ml_sha256_hex — to RESEAL a synthetic v4 file
#include "foh.h"
#include "foh_ctl_labels.h"
#include "foh_persist.h"

void gfx_fatal(const char *what) {
  fprintf(stderr, "REBIND FAIL: gfx_fatal: %s\n", what);
  exit(3);
}

// HAND-COPIED out of foh_render.c, line for line, and pinned by
// check-rebind.sh leg [1] — if a coordinate or a palette entry moves in the
// renderer without moving here, the pin fails before any assertion runs.
static const RastCol kText = {220, 220, 230, 256};
static const RastCol kDim = {120, 120, 140, 256};
static const RastCol kAccent = {255, 200, 60, 256};
#define ROW_BTN_X 16  // foh_text(rz, 16, y, 1, kBtn[i], ...)
#define ROW_ACT_X 96  // foh_text(rz, 96, y, 1, kAct[act], ...)
#define ROW_Y0 44     // const int y = 44 + i * 14;
#define ROW_PITCH 14
#define CARET_X 6     // foh_text(rz, 6, yCur, 1, ">", kAccent)
#define Y_STYLE 176   // const int yStyle = 176, yReset = 190;
#define Y_RESET 190
#define CAPTION_Y 204 // text_center(rz, 204, 1, ..., kDim)
#define RESET_LABEL "RESET TO DEFAULTS"
#define CAPTION_NOW "L/R: CHANGE   A: RESET"
#define CAPTION_OLD "L/R: CHANGE   REBIND: N/A"

static const char *const kBtn[FOH_CTL_ACTION_ROWS] = {
    "D-PAD", "A", "B", "X", "Y", "L", "R", "START", "MENU"};

static int g_fails;
static void ok(const char *what) { printf("  ok  %s\n", what); }
static void bad(const char *what) {
  printf("REBIND FAIL: %s\n", what);
  g_fails++;
}
static void want(int cond, const char *what) {
  if (cond) ok(what); else bad(what);
}

// --- rendering ---------------------------------------------------------------
static Raster g_a, g_probe;

static void render_shot(const FohState *s, Raster *rz) {
  FohState look = *s;
  foh_look_canonical(&look);
  memset(rz, 0, sizeof *rz);
  foh_render(&look, rz);
}

static int fb_diff(const Raster *p, const Raster *q) {
  int n = 0;
  for (int i = 0; i < RAST_W * RAST_H; i++)
    if (p->fb[i] != q->fb[i]) n++;
  return n;
}

static int overdraw(const Raster *live, int x, int y, const char *text,
                    RastCol col) {
  g_probe = *live;
  foh_text(&g_probe, x, y, 1, text, col);
  return fb_diff(live, &g_probe);
}

// --- gestures (foh_controls_witness.c's, verbatim in method) -----------------
static void tick_with(FohState *s, size_t off) {
  PlatformInput in;
  memset(&in, 0, sizeof in);
  if (off != (size_t)-1) *(bool *)((char *)&in + off) = true;
  foh_tick(s, &in);
}
static void neutral(FohState *s) { tick_with(s, (size_t)-1); }
static void press(FohState *s, size_t off) {
  tick_with(s, off);
  neutral(s);
}
#define PRESS(s, field) press((s), offsetof(PlatformInput, field))
static void press_n(FohState *s, size_t off, int n) {
  for (int i = 0; i < n; i++) press(s, off);
}
#define PRESS_N(s, field, n) press_n((s), offsetof(PlatformInput, field), (n))

// f04-nav.flow's own path: three DOWNs to OPTIONS, A; two DOWNs to CONTROLS,
// A; then A on chooser row 0 (HANDHELD, DEVIATION D25).
// The style/Mod/binding cells are PROCESS-WIDE (ctl_style.c is a TU, not a
// header, precisely so the FOH and the input path cannot desync), so foh_init
// does not touch them. Each leg starts from the fresh-install record the way a
// cold boot would — this is harness setup, never a shortcut around the thing
// under test: every claim below still gets there through foh_tick.
static void reset_process(void) {
  for (int k = 0; k < CTL_BIND_PORTS; k++) ctl_bind_reset(k);
  ctl_style_set((int)CTL_STYLE_DEFAULT);
  ctl_mod_on_r_set(false);
}

static void goto_handheld(FohState *s) {
  reset_process();
  foh_init(s);
  int guard = 0;
  while (s->screen != FOH_TITLE && guard++ < 100000) neutral(s);
  PRESS(s, start);
  PRESS_N(s, down, 3);
  PRESS(s, a);
  PRESS_N(s, down, 2);
  PRESS(s, a);
#if FOH_CTL_CHOOSER
  // The chooser sits between the CONTROLS row and this screen only while
  // DEVIATION D27's switch is on (foh.h). At its shipped 0 the row opens the
  // HANDHELD screen DIRECTLY, so a second A here would land on the d-pad row
  // instead of navigating.
  PRESS(s, a);
#endif
  want(s->screen == FOH_CTRL_KEY,
       "real gestures reach the HANDHELD screen (Options > CONTROLS)");
}

// --- screen readers ----------------------------------------------------------
static int row_y(int row) {
  if (row < FOH_CTL_ACTION_ROWS) return ROW_Y0 + row * ROW_PITCH;
  return row == FOH_CTL_ROW_STYLE ? Y_STYLE : Y_RESET;
}

// Row `row` (0..8) claims to name button `btn` doing action `act`.
static void assert_action_row(const FohState *s, int row, const char *btn,
                              const char *act, const char *ctx) {
  render_shot(s, &g_a);
  const bool cur = s->ctlRow == row;
  const int y = row_y(row);
  char buf[240];
  int d = overdraw(&g_a, ROW_BTN_X, y, btn, cur ? kAccent : kText);
  snprintf(buf, sizeof buf, "%s: row %d's BUTTON reads \"%s\" (%d px differ)",
           ctx, row, btn, d);
  want(d == 0, buf);
  d = overdraw(&g_a, ROW_ACT_X, y, act, cur ? kAccent : kDim);
  snprintf(buf, sizeof buf, "%s: row %d's ACTION reads \"%s\" (%d px differ)",
           ctx, row, act, d);
  want(d == 0, buf);
}

// The caret is the only thing that tells the player which row he is on, so
// SCREEN and STATE are bound: it must be drawn at the state's row and NOWHERE
// else. A cursor that renders one row behind the machine is exactly the class
// of defect a state-only assertion cannot see.
static void assert_caret(const FohState *s, const char *ctx) {
  render_shot(s, &g_a);
  char buf[240];
  for (int r = 0; r < FOH_CTL_ROWS; r++) {
    const int d = overdraw(&g_a, CARET_X, row_y(r), ">", kAccent);
    if (r == s->ctlRow) {
      snprintf(buf, sizeof buf, "%s: the caret IS drawn on row %d (%d px)",
               ctx, r, d);
      want(d == 0, buf);
    } else if (d == 0) {
      snprintf(buf, sizeof buf,
               "%s: a caret is ALSO drawn on row %d while the state says row "
               "%d — screen and state disagree", ctx, r, s->ctlRow);
      bad(buf);
    }
  }
}

// --- THE PRODUCT CHAIN -------------------------------------------------------
// foh_dev.c's poll_bound() does ctl_bind_apply(0, in, in) on every match-loop
// poll, and the match loops hand that struct to s1_input_row_style with the
// live style cells. This is that pairing and nothing else: no field is poked
// and no binding is read back to build the expectation.
static MlInput drive(size_t physOff) {
  PlatformInput raw;
  memset(&raw, 0, sizeof raw);
  *(bool *)((char *)&raw + physOff) = true;
  PlatformInput bound = raw;
  ctl_bind_apply(0, &bound, &bound); // in place, exactly like poll_bound
  return s1_input_row_style(&bound, ctl_style_get(), ctl_mod_on_r_get());
}
#define DRIVE(field) drive(offsetof(PlatformInput, field))

// The same seam at the PlatformInput level, for the two actions the sim never
// sees: START (foh_dev.c's pause-overlay edge) and MENU (the FunKey system
// menu edge). Both read the struct poll_bound produced, so a rebind of them
// lands on the same plane a rebind of ATTACK does.
static PlatformInput drive_raw(size_t physOff) {
  PlatformInput raw;
  memset(&raw, 0, sizeof raw);
  *(bool *)((char *)&raw + physOff) = true;
  PlatformInput bound = raw;
  ctl_bind_apply(0, &bound, &bound);
  return bound;
}
#define DRIVE_RAW(field) drive_raw(offsetof(PlatformInput, field))

static bool is_identity(void) {
  for (int k = 0; k < CTL_BIND_PORTS; k++)
    for (int i = 0; i < (int)CTL_BTN_COUNT; i++)
      if (ctl_bind_get(k, i) != i) return false;
  return true;
}

static bool is_permutation(int port) {
  bool seen[CTL_BTN_COUNT] = {false};
  for (int i = 0; i < (int)CTL_BTN_COUNT; i++) {
    const int v = ctl_bind_get(port, i);
    if (v < 0 || v >= (int)CTL_BTN_COUNT || seen[v]) return false;
    seen[v] = true;
  }
  return true;
}

// One tick with one field held, reporting the sound tokens it emitted. Used
// for the refusal arms, where the ABSENCE of a change is only half the claim
// — the screen must say no out loud.
static bool tick_emits(FohState *s, size_t off, const char *tok) {
  PlatformInput in;
  memset(&in, 0, sizeof in);
  *(bool *)((char *)&in + off) = true;
  foh_tick(s, &in);
  const bool got = s->nsnd == 1 && strcmp(s->snd[0], tok) == 0;
  neutral(s);
  return got;
}
#define TICK_EMITS(s, field, tok) \
  tick_emits((s), offsetof(PlatformInput, field), (tok))

// --- [A31-1] every row is reachable, and the caret says so -------------------
static void a31_rows_reachable(void) {
  FohState s;
  goto_handheld(&s);
  want(s.ctlRow == 0, "the screen opens on row 0 (the d-pad row)");
  assert_caret(&s, "fresh screen");
  for (int r = 1; r < FOH_CTL_ROWS; r++) {
    PRESS(&s, down);
    char buf[120];
    snprintf(buf, sizeof buf, "DOWN reaches row %d of %d", r,
             FOH_CTL_ROWS - 1);
    want(s.ctlRow == r, buf);
  }
  assert_caret(&s, "last row");
  PRESS(&s, down);
  want(s.ctlRow == 0, "DOWN off the last row wraps to the first");
  PRESS(&s, up);
  want(s.ctlRow == FOH_CTL_ROWS - 1, "UP off the first row wraps to the last");
  assert_caret(&s, "after wrapping");
  PRESS_N(&s, down, 2); // last -> 0 -> 1
  want(s.ctlRow == 1,
       "the first BUTTON row (A) is reachable — the ticket's whole symptom");
  assert_caret(&s, "on the A row");
  // the two non-action rows are on screen where the reader claims they are
  render_shot(&s, &g_a);
  {
    char buf[200];
    const int d = overdraw(&g_a, ROW_BTN_X, Y_RESET, RESET_LABEL, kDim);
    snprintf(buf, sizeof buf,
             "the RESET row is drawn at y=%d reading \"%s\" (%d px differ)",
             Y_RESET, RESET_LABEL, d);
    want(d == 0, buf);
  }
}

// --- [A31-2/3/4] the rebind, bound to what the buttons actually do -----------
static void a31_rebind_drives(void) {
  FohState s;
  goto_handheld(&s);
  // Fresh install is NATURAL (ctl_style.h). Since the 2026-08-24 owner
  // re-ratification (DEVIATION D33) the FACE column is the same in every
  // style: row 1 = A = JUMP (in.x), row 2 = B = ATTACK (in.a).
  want(ctl_style_get() == CTL_STYLE_NATURAL,
       "the screen opens on the ratified default style (NATURAL)");
  want(is_identity(), "the screen opens on the IDENTITY binding");
  assert_action_row(&s, 1, "A", "JUMP", "before the rebind");
  assert_action_row(&s, 2, "B", "ATTACK", "before the rebind");
  {
    const MlInput ia = DRIVE(a), ib = DRIVE(b);
    want(ia.x && !ia.a, "before the rebind, physical A drives JUMP");
    want(ib.a && !ib.x, "before the rebind, physical B drives ATTACK");
  }
  // ONE press of RIGHT on the A row — the owner's gesture.
  PRESS(&s, down);
  want(s.ctlRow == 1, "the cursor is on the A row");
  PRESS(&s, right);
  assert_action_row(&s, 1, "A", "ATTACK", "after one RIGHT on the A row");
  assert_action_row(&s, 2, "B", "JUMP", "the swap partner moved with it");
  {
    const MlInput ia = DRIVE(a), ib = DRIVE(b);
    want(ia.a && !ia.x,
         "AND THE BUTTON FOLLOWS: physical A now drives ATTACK through the "
         "product chain (ctl_bind_apply -> s1_input_row_style)");
    want(ib.x && !ib.a, "physical B now drives JUMP");
  }
  want(is_permutation(0), "the table is still a permutation after the rebind");
  // LEFT is the exact inverse.
  PRESS(&s, left);
  assert_action_row(&s, 1, "A", "JUMP", "LEFT undoes it");
  {
    const MlInput ia = DRIVE(a);
    want(ia.x && !ia.a, "and physical A drives JUMP again");
  }
  // [A31-3] a NON-sim action: put PAUSE (the START slot) on the R shoulder.
  // CtlBtn order is A B X Y L R START MENU, so ONE RIGHT on the R row (5) is
  // the clean pairwise swap R <-> START.
  PRESS_N(&s, down, 5);
  want(s.ctlRow == 6, "the cursor is on the R row");
  PRESS(&s, right);
  want(ctl_bind_get(0, CTL_BTN_R) == CTL_BTN_START &&
           ctl_bind_get(0, CTL_BTN_START) == CTL_BTN_R,
       "one RIGHT swaps the R and START slots");
  assert_action_row(&s, 6, "R", "PAUSE", "the R row now says PAUSE");
  assert_action_row(&s, 7, "START", "C-STICK (HOLD)",
                    "and START took R's C-layer (D32)");
  {
    const PlatformInput br = DRIVE_RAW(r);
    want(br.start && !br.r,
         "AND THE BUTTON FOLLOWS: physical R now raises the START bit that "
         "foh_dev.c's pause-overlay edge reads");
    const PlatformInput bs = DRIVE_RAW(start);
    want(bs.r && !bs.start, "and physical START now raises the R bit");
  }
  // [A31-4] permutation under arbitrary editing: hammer every button row.
  for (int r = 1; r < FOH_CTL_ACTION_ROWS; r++) {
    while (s.ctlRow != r) PRESS(&s, down);
    PRESS_N(&s, right, r);
    PRESS_N(&s, left, (r * 3) % 5);
  }
  want(is_permutation(0),
       "after 8 rows x mixed L/R editing the table is STILL a permutation — "
       "no action can be lost, so no primary needs protecting");
  // and every one of the eight logical actions is still on exactly one button
  {
    int found[CTL_BTN_COUNT] = {0};
    for (int i = 0; i < (int)CTL_BTN_COUNT; i++) found[ctl_bind_get(0, i)]++;
    int missing = -1;
    for (int i = 0; i < (int)CTL_BTN_COUNT; i++)
      if (found[i] != 1) missing = i;
    char buf[200];
    snprintf(buf, sizeof buf,
             "every action is still reachable on exactly one button (first "
             "offender: %d)", missing);
    want(missing < 0, buf);
  }
}

// --- [A31-5] the d-pad row refuses -------------------------------------------
static void a31_dpad_refuses(void) {
  FohState s;
  goto_handheld(&s);
  want(s.ctlRow == 0, "the cursor is on the d-pad row");
  want(TICK_EMITS(&s, right, "deny"),
       "RIGHT on the d-pad row emits exactly one `deny` (not silence)");
  want(is_identity(), "and it changed no binding");
  want(TICK_EMITS(&s, left, "deny"), "LEFT on the d-pad row denies too");
  want(is_identity(), "and it changed no binding either");
  assert_action_row(&s, 0, "D-PAD", "CONTROL STICK", "the d-pad row");
  // A is the RESET row's activator and nothing else's
  want(TICK_EMITS(&s, a, "deny"),
       "A on a non-RESET row emits exactly one `deny`");
}

// --- [A31-6/7/8] reset, the vanished mod row, the vanished N/A ---------------
static void a31_reset_and_removals(void) {
  FohState s;
  goto_handheld(&s);
  // make a mess through the real screen: two rebinds and a style change
  PRESS(&s, down);
  PRESS_N(&s, right, 3);
  PRESS_N(&s, down, 4);
  PRESS_N(&s, left, 2);
  while (s.ctlRow != FOH_CTL_ROW_STYLE) PRESS(&s, down);
  PRESS(&s, right);
  // the Mod cell: nothing on this screen writes it any more, so the witness
  // writes it directly to prove RESET is what clears it (and only RESET).
  ctl_mod_on_r_set(true);
  want(!is_identity() && ctl_style_get() != CTL_STYLE_DEFAULT,
       "the screen is now genuinely off-default (binding AND style)");
  // [A31-7] the mod row is GONE: its old label is nowhere on the screen.
  render_shot(&s, &g_a);
  {
    char buf[240];
    int found = -1;
    for (int r = 0; r < FOH_CTL_ROWS; r++) {
      if (overdraw(&g_a, ROW_BTN_X, row_y(r), "MOD: R", kDim) == 0) found = r;
      if (overdraw(&g_a, ROW_BTN_X, row_y(r), "MOD: L", kDim) == 0) found = r;
      if (overdraw(&g_a, ROW_BTN_X, row_y(r), "MOD: R", kAccent) == 0)
        found = r;
      if (overdraw(&g_a, ROW_BTN_X, row_y(r), "MOD: L", kAccent) == 0)
        found = r;
    }
    snprintf(buf, sizeof buf,
             "the Mod-shoulder row is GONE from the screen (found at row %d)",
             found);
    want(found < 0, buf);
  }
  // ...but the CELL is not. This is a UI removal, not a model removal: A30(a)
  // wants the cell and the BOX label table still reads it.
  want(ctl_mod_on_r_get(),
       "and the modOnR CELL still holds what was written to it (UI removal, "
       "not model removal)");
  // [A31-8] `REBIND: N/A` is gone and the caption names a live control.
  {
    const int dOld =
        overdraw(&g_a, (RAST_W - foh_text_width(CAPTION_OLD, 1)) / 2,
                 CAPTION_Y, CAPTION_OLD, kDim);
    want(dOld > 0, "`REBIND: N/A` is gone from the caption");
    const int dNow =
        overdraw(&g_a, (RAST_W - foh_text_width(CAPTION_NOW, 1)) / 2,
                 CAPTION_Y, CAPTION_NOW, kDim);
    want(dNow == 0, "and the caption names the controls that are now live");
  }
  // [A31-6] RESET, through the real gesture.
  PRESS(&s, down);
  want(s.ctlRow == FOH_CTL_ROW_RESET, "the cursor is on the RESET row");
  PRESS(&s, a);
  want(is_identity(), "A on RESET restores the EXACT identity binding");
  want(ctl_style_get() == CTL_STYLE_DEFAULT,
       "A on RESET restores the default style");
  want(ctl_mod_on_r_get(), // D29: the CURRENT default is Mod on R, shield on L
       "A on RESET restores the ratified Mod arrangement");
  // and the screen agrees, row by row, with the fresh label table
  {
    const char *lab[FOH_CTL_LABEL_ROWS];
    foh_ctl_labels(ctl_style_get(), ctl_mod_on_r_get(), lab);
    for (int r = 0; r < FOH_CTL_ACTION_ROWS; r++) {
      assert_action_row(&s, r, kBtn[r], lab[r], "after RESET");
    }
  }
  // and the play path agrees too
  {
    const MlInput ia = DRIVE(a);
    want(ia.x && !ia.a, "after RESET physical A drives JUMP again");
  }
}

// --- [A31-9] the MLFKPERSIST5 round trip -------------------------------------
// A binding that does not survive a restart is the same defect as a slider
// that makes no sound: it appears to take and does not. The whole point of
// the format bump is this leg.
static void a31_persist_round_trip(void) {
  FohState s;
  goto_handheld(&s);
  // rebind through the screen, then STAMP exactly as the drivers do
  // (foh_persist.h: the cells live in another TU, so the driver moves them).
  PRESS(&s, down);
  PRESS_N(&s, right, 5);
  PRESS_N(&s, down, 3);
  PRESS_N(&s, right, 2);
  int wantRow[CTL_BTN_COUNT];
  for (int i = 0; i < (int)CTL_BTN_COUNT; i++) wantRow[i] = ctl_bind_get(0, i);
  want(!is_identity(), "the screen produced a non-identity binding to persist");

  FohPersist p;
  foh_persist_defaults(&p);
  for (int k = 0; k < CTL_BIND_PORTS; k++)
    for (int i = 0; i < (int)CTL_BTN_COUNT; i++) p.bind[k][i] = ctl_bind_get(k, i);
  p.ctlStyle = (int)ctl_style_get();
  p.modOnR = ctl_mod_on_r_get() ? 1 : 0;
  foh_persist_save(&p);

  // a fresh process: defaults everywhere, then the load chokepoint
  ctl_bind_reset(0);
  want(is_identity(), "a fresh process starts from the identity again");
  FohPersist q;
  const FohPersistStatus st = foh_persist_load(&q);
  want(st == FOH_PERSIST_LOADED, "the saved record loads (not a reset)");
  for (int k = 0; k < CTL_BIND_PORTS; k++)
    want(ctl_bind_set_row(k, q.bind[k]), "every persisted row installs");
  {
    int bad_i = -1;
    for (int i = 0; i < (int)CTL_BTN_COUNT; i++)
      if (ctl_bind_get(0, i) != wantRow[i]) bad_i = i;
    char buf[200];
    snprintf(buf, sizeof buf,
             "the binding SURVIVED the save/load round trip byte for byte "
             "(first mismatch: %d)", bad_i);
    want(bad_i < 0, buf);
  }
  // the play path, after the reload — the claim that actually matters. The
  // expectation comes from the SAVED table, not from a re-read of the cells,
  // so a load that quietly installed something else cannot satisfy it.
  {
    const PlatformInput b = DRIVE_RAW(a);
    const bool bits[CTL_BTN_COUNT] = {b.a, b.b, b.x, b.y,
                                      b.l, b.r, b.start, b.menu};
    char buf[200];
    snprintf(buf, sizeof buf,
             "and the RELOADED binding drives the play path: physical A "
             "raises the logical %s bit the saved table named",
             ctl_btn_name(wantRow[CTL_BTN_A]));
    want(bits[wantRow[CTL_BTN_A]], buf);
  }
  // a NON-PERMUTATION row is corruption, never something to repair quietly
  {
    int dup[CTL_BTN_COUNT] = {0, 0, 2, 3, 4, 5, 6, 7};
    want(!ctl_bind_set_row(0, dup),
         "a duplicated slot is REFUSED (an action would be unreachable)");
    int oob[CTL_BTN_COUNT] = {0, 1, 2, 3, 4, 5, 6, 9};
    want(!ctl_bind_set_row(0, oob), "an out-of-domain slot is REFUSED");
  }
  ctl_bind_reset(0);
}

// --- [A31-9b] the v4 -> v5 MIGRATION -----------------------------------------
// The owner has a real save on his device. A format bump that RESET it would
// destroy every target-test personal best, so v4 must MIGRATE — and its
// bindings must land on the identity, which is exactly the mapping a pre-A31
// build had. This leg builds a genuine v4 file by DOWNGRADING a real v5 one
// (strip the bind rows, restore the v4 header, reseal) so the fixture cannot
// drift away from the format the serializer actually writes.
static void a31_v4_migrates(void) {
  reset_process();
  FohPersist p;
  foh_persist_defaults(&p);
  p.turbo = 1;
  p.ctlStyle = (int)CTL_STYLE_BOX;
  p.modOnR = 1;
  p.targetRecords[2][3] = 14.5;
  p.bind[0][0] = 3; p.bind[0][3] = 0; // a binding v4 could never carry
  foh_persist_save(&p);

  char path[512];
  snprintf(path, sizeof path, "%s/mlfk-persist.dat", foh_persist_dir());
  static char buf[8192];
  size_t n = 0;
  {
    FILE *f = fopen(path, "rb");
    want(f != NULL, "the v5 record was written where the loader looks");
    if (!f) return;
    n = fread(buf, 1, sizeof buf - 1, f);
    fclose(f);
  }
  // strip every row v4 never had (`bind ` from v5, `sel ` from v6) plus the
  // SUM line, restore the v4 header, reseal.
  // A49 (MLFKPERSIST6) added `sel` and A26 (MLFKPERSIST7) `resume`; this
  // constructor synthesises an OLD file
  // out of a CURRENT one, so EVERY row a later version appends must be
  // stripped here or the fixture is a v4 header over v6 content and the
  // loader rightly refuses it. That is what broke all four v4 assertions
  // after A49 — a STALE FIXTURE, not a migration defect: a real v4 file from
  // a real old device never had either row and still migrates.
  // ticket #25 appended eight more v7 rows (the CSS machine plane), stripped
  // by the same rule and counted by the same kind of assertion: this
  // constructor's whole failure mode is going STALE, so every row is named
  // and every name has to be found.
  static const char *const kCssRows[] = {
      "ptype ", "cpudiff ",  "vsmode ",   "hand ",
      "slider ", "carry ",   "cpucarry ", "handtype "};
  const int kNCssRows = (int)(sizeof kCssRows / sizeof *kCssRows);
  static char out[8192];
  size_t m = 0;
  int stripped = 0, strippedSel = 0, strippedResume = 0, strippedCss = 0;
  for (size_t i = 0; i < n;) {
    size_t e = i;
    while (e < n && buf[e] != '\n') e++;
    const size_t len = e - i + 1;
    bool isCss = false;
    for (int c = 0; c < kNCssRows; c++) {
      const size_t kl = strlen(kCssRows[c]);
      if (len > kl && memcmp(buf + i, kCssRows[c], kl) == 0) {
        isCss = true;
        break;
      }
    }
    if (isCss) {
      strippedCss++; /* ticket #25's CSS rows (v7) — v4 never had them */
    } else if (len > 5 && memcmp(buf + i, "bind ", 5) == 0) {
      stripped++;
    } else if (len > 4 && memcmp(buf + i, "sel ", 4) == 0) {
      strippedSel++; /* v6's selection row — v4 never had it */
    } else if (len > 7 && memcmp(buf + i, "resume ", 7) == 0) {
      strippedResume++; /* v7's hibernate row (A26) — v4 never had it */
    } else if (len > 4 && memcmp(buf + i, "SUM ", 4) == 0) {
      /* dropped; resealed below */
    } else if (i == 0) {
      memcpy(out + m, "MLFKPERSIST4\n", 13);
      m += 13;
    } else {
      memcpy(out + m, buf + i, len);
      m += len;
    }
    i = e + 1;
  }
  want(stripped == CTL_BIND_PORTS,
       "the synthetic v4 fixture dropped exactly one bind row per port");
  want(strippedSel == 1,
       "the synthetic v4 fixture dropped v6's sel row (a fixture carrying a "
       "later version's rows under an older header is not that older version)");
  want(strippedResume == 1,
       "the synthetic v4 fixture dropped v7's resume row (same rule: every "
       "format bump has to be stripped here or the fixture stops being v4)");
  want(strippedCss == kNCssRows,
       "the synthetic v4 fixture dropped all eight of ticket #25's CSS rows "
       "(same rule again, and now it is APPENDED ROWS rather than version "
       "bumps that go stale here)");
  {
    char hex[65];
    ml_sha256_hex(out, m, hex);
    m += (size_t)snprintf(out + m, sizeof out - m, "SUM %s\n", hex);
    FILE *f = fopen(path, "wb");
    if (f) { fwrite(out, 1, m, f); fclose(f); }
  }
  reset_process();
  FohPersist q;
  const FohPersistStatus st = foh_persist_load(&q);
  want(st == FOH_PERSIST_LOADED,
       "a genuine v4 file MIGRATES rather than resetting (the owner's save "
       "survives the bump)");
  {
    bool ident = true;
    for (int k = 0; k < CTL_BIND_PORTS; k++)
      for (int i = 0; i < (int)CTL_BTN_COUNT; i++)
        if (q.bind[k][i] != i) ident = false;
    want(ident,
         "and its bindings land on the IDENTITY — the mapping a pre-A31 "
         "build actually had, not a silent re-map");
  }
  want(q.turbo == 1 && q.ctlStyle == (int)CTL_STYLE_BOX && q.modOnR == 1,
       "every v4 setting carried forward unchanged");
  want(q.targetRecords[2][3] == 14.5,
       "and the target-test record carried forward (the PB data-loss class)");
  // and the migrated record REPUBLISHES as v5, so the upgrade happens once
  // (and the check's header/bind-row inspection reads a current file).
  foh_persist_save(&q);
  {
    FILE *f = fopen(path, "rb");
    char hdr[16] = {0};
    if (f) { (void)!fread(hdr, 1, 13, f); fclose(f); }
    want(memcmp(hdr, "MLFKPERSIST7\n", 13) == 0,
         "and the next save republishes it as MLFKPERSIST7 (upgrade once)");
  }
  reset_process();
}

int main(void) {
  // Before ANY leg runs and before any reset: the fresh-install record is
  // what a cold boot really has.
  want(is_identity() && ctl_style_get() == CTL_STYLE_DEFAULT &&
           ctl_mod_on_r_get(), // D29: shield on L, Mod on R
       "a FRESH PROCESS starts on the identity binding, the default style and "
       "the ratified Mod arrangement");
  a31_rows_reachable();
  a31_rebind_drives();
  a31_dpad_refuses();
  a31_reset_and_removals();
  a31_persist_round_trip();
  a31_v4_migrates();
  if (g_fails) {
    printf("REBIND: %d assertion(s) failed\n", g_fails);
    return 1;
  }
  printf("REBIND OK\n");
  return 0;
}
