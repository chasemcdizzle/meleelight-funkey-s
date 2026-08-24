// port/foh/foh_legibility_witness.c — the two READABILITY defects the owner
// filed on playthrough #3: A32 (gameplay options row) and A25a (target-select
// highlight). One witness, because both are claims about what is ON THE
// SCREEN and both are proved with the same instrument.
//
// THE DEFECTS THIS GUARDS
// -----------------------
// A32. The owner read the gameplay screen's per-player row as "L-cancel is
// off for P2, P3 and P4" and filed a defaults bug. Measured, it is neither:
// L-CANCEL is row 1 and is a single GLOBAL value (foh_render.c's
// `vals[1] = kLCancelNames[...]`), and the only per-player row is row 4,
// which upstream names "Tapjump off" and gives the value "On" when tap jump
// is DISABLED (gameplaymenu.js:242). A row whose label and value are both
// negatives cannot be read, and it was not read. DEVIATION D23 relabels it
// "TAP JUMP" and inverts the value ON DISPLAY ONLY.
//
// A25a. "The highlighting around test 1 or + ADD CODE you selected is not
// really visible to the eye." Measured: the whole selection signal was a
// ONE-PIXEL border, pink instead of grey, around a 100x19 black body — 242
// changed pixels on a 240x240 screen. DEVIATION D24 draws the selected
// slot's border twice and lifts its body off black.
//
// WHAT THIS ASSERTS, in the owner's terms rather than in ours
// -----------------------------------------------------------
//   [A32-1] the row 4 LABEL on screen is the string "TAP JUMP", and is NOT
//           the old double negative;
//   [A32-2] with tap jump ENABLED for P1 the screen says "ON", and with it
//           DISABLED — reached by pressing A on the cell, the real gesture,
//           never by poking the field — the screen says "OFF". That pairing
//           IS the ticket: the rendered word now tracks whether the feature
//           works, so the row cannot be misread the way it was;
//   [A32-3] the STATE plane keeps upstream's polarity (`tapJumpOff` is still
//           "off"), because foh_persist and the sim consume that bit — the
//           deviation must be in the pixels and nowhere else;
//   [A25a-1] every one of the ELEVEN slots (ten targets + "+ ADD CODE")
//           changes at least LEGIBILITY_MIN_PX pixels inside its own rect
//           when it becomes the selected one. The threshold is set above
//           what a one-pixel border can physically reach (see below), so
//           this fails on the shipped-before geometry rather than passing on
//           it — the A29 lesson, whose stated done-check passed before the
//           fix because it tested the wrong plane;
//   [A25a-2] a slot's pixels are IDENTICAL under two different elsewhere
//           cursors, so [A25a-1] measures selection and not screen noise.
//
// THE INSTRUMENT. A rendered string is asserted by OVERDRAWING the claimed
// string, at the claimed place, in the claimed colour, onto the real frame
// and requiring ZERO pixels to change: text composites opaquely
// (foh_font.c:185, `col.a256` == 256 for every colour used here), so drawing
// what is already there is a no-op and drawing anything else is not. That
// makes "the screen says OFF" a pixel claim instead of a claim about a
// variable the renderer might not be using.
//
// Every gesture is fed to the REAL foh_tick as button levels, and the cursor
// is moved by asserting where it landed rather than by counting frames — the
// foh_cssback_witness.c discipline, carried.
//
// Usage: foh_legibility_witness. Prints one line per assertion and
// `LEGIBILITY OK` on success, exit 0. Any failure prints `LEGIBILITY FAIL:`
// and exits 1. Its negative tests live in port/foh/check-legibility.sh.
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../gfx/raster.h"
#include "foh.h"

// gfx_fatal is host-provided (raster.h), the foh_cssback_witness.c pattern.
void gfx_fatal(const char *what) {
  fprintf(stderr, "LEGIBILITY FAIL: gfx_fatal: %s\n", what);
  exit(3);
}

// --- the numbers this witness HAND COPIES from foh_render.c -----------------
// Every one of these is pinned line-for-line by check-legibility.sh, exactly
// as check-css-token-rest.sh pins the foh_dev.c lines its witness copies. If
// the renderer moves a row or repaints a palette entry, this dies loudly
// instead of quietly asserting against a screen that no longer exists.
static const RastCol kText = {220, 220, 230, 256};    // foh_render.c:85
static const RastCol kDim = {120, 120, 140, 256};     // foh_render.c:86
static const RastCol kAccent = {255, 200, 60, 256};   // foh_render.c:87
#define OPT_ROW4_Y 144    // ys[4] in render_opt_gameplay
#define OPT_LABEL_X 24    // row_label's foh_text x
#define OPT_CELL_X0 22    // `x = 22 + k * 52`
#define OPT_CELL_DX 52
#define OPT_CELL_Y (OPT_ROW4_Y + 16)
#define OPT_VAL_DX 16     // the value sits at x + 16

// The selection signal a ONE-PIXEL border can physically produce is the ring
// between the 102x21 border rect and the 100x19 body: 102*21 - 100*19 == 242
// pixels for a target slot, and 102*19 - 100*17 == 238 for "+ ADD CODE". A
// threshold of 600 is therefore unreachable by ANY one-pixel border on these
// rects, whatever colour it is painted — which is what makes this tooth bite
// on the geometry the owner complained about.
#define LEGIBILITY_MIN_PX 600

static int g_fails;

static void ok(const char *what) { printf("  ok  %s\n", what); }

static void bad(const char *what) {
  printf("LEGIBILITY FAIL: %s\n", what);
  g_fails++;
}

static void want(int cond, const char *what) {
  if (cond) ok(what);
  else bad(what);
}

// --- rendering ---------------------------------------------------------------
// One frame as a judged shot would see it: a pure function of MACHINE state
// with the LOOK plane pinned exactly as every shot path pins it. For the TSS
// that pinning is load-bearing — foh_look_canonical zeroes `tssTimer`, so the
// hovered slot's flash is cold and the two frames being compared differ by
// the SELECTION and not by which phase of an 8-frame cycle they caught.
static Raster g_a, g_b, g_probe;

static void render_shot(const FohState *s, Raster *rz) {
  FohState look = *s;
  foh_look_canonical(&look);
  memset(rz, 0, sizeof *rz);
  foh_render(&look, rz);
}

static int fb_diff_in(const Raster *p, const Raster *q, int x0, int y0, int x1,
                      int y1) {
  int n = 0;
  if (x0 < 0) x0 = 0;
  if (y0 < 0) y0 = 0;
  if (x1 > RAST_W) x1 = RAST_W;
  if (y1 > RAST_H) y1 = RAST_H;
  for (int y = y0; y < y1; y++)
    for (int x = x0; x < x1; x++)
      if (p->fb[y * RAST_W + x] != q->fb[y * RAST_W + x]) n++;
  return n;
}

static int fb_diff(const Raster *p, const Raster *q) {
  return fb_diff_in(p, q, 0, 0, RAST_W, RAST_H);
}

// Pixels that change when `text` is drawn over `live` at (x, y) in `col`.
// Zero means the screen already carries exactly that string, in that colour,
// at that spot.
static int overdraw(const Raster *live, int x, int y, const char *text,
                    RastCol col) {
  g_probe = *live;
  foh_text(&g_probe, x, y, 1, text, col);
  return fb_diff(live, &g_probe);
}

// --- gestures ----------------------------------------------------------------
static void tick_with(FohState *s, size_t off) {
  PlatformInput in;
  memset(&in, 0, sizeof in);
  if (off != (size_t)-1) *(bool *)((char *)&in + off) = true;
  foh_tick(s, &in);
}

static void neutral(FohState *s) { tick_with(s, (size_t)-1); }

// One tick with a single field held, then one neutral tick — exactly one
// rising edge, which is what every edge-driven FOH arm reads.
static void press(FohState *s, size_t off) {
  tick_with(s, off);
  neutral(s);
}

#define PRESS(s, field) press((s), offsetof(PlatformInput, field))

static void press_n(FohState *s, size_t off, int n) {
  for (int i = 0; i < n; i++) press(s, off);
}

#define PRESS_N(s, field, n) press_n((s), offsetof(PlatformInput, field), (n))

// STARTUP -> TITLE is the timer transition; TITLE -> MENU_TOP is START.
static void boot_to_menu(FohState *s) {
  foh_init(s);
  int guard = 0;
  while (s->screen != FOH_TITLE && guard++ < 100000) neutral(s);
  want(s->screen == FOH_TITLE, "machine reaches the title screen");
  PRESS(s, start);
  want(s->screen == FOH_MENU_TOP, "START leaves the title for the menu top");
}

// --- [A32] the gameplay options row -----------------------------------------
// The path is f03-options.flow's own: three DOWNs to OPTIONS, A, one DOWN to
// GAMEPLAY, A. Then four DOWNs to row 4, the only multi-column row.
static void goto_gameplay_row4(FohState *s) {
  boot_to_menu(s);
  PRESS_N(s, down, 3);
  PRESS(s, a);
  want(s->screen == FOH_MENU_OPTIONS, "menu top row 3 (OPTIONS) opens options");
  PRESS(s, down);
  PRESS(s, a);
  want(s->screen == FOH_OPT_GAMEPLAY, "options row 1 opens GAMEPLAY");
  PRESS_N(s, down, 4);
  want(s->optRow == 4 && s->optCol == 0,
       "four DOWNs land on the per-player row, port 1");
}

static void assert_row4_value(FohState *s, const char *wantStr,
                              RastCol wantCol, const char *otherStr,
                              RastCol otherCol, const char *ctx) {
  const int vx = OPT_CELL_X0 + 0 * OPT_CELL_DX + OPT_VAL_DX;
  render_shot(s, &g_a);
  char buf[220];
  const int same = overdraw(&g_a, vx, OPT_CELL_Y, wantStr, wantCol);
  const int diff = overdraw(&g_a, vx, OPT_CELL_Y, otherStr, otherCol);
  snprintf(buf, sizeof buf,
           "%s: P1's cell on screen reads \"%s\" (%d px differ from that "
           "string drawn over it)", ctx, wantStr, same);
  want(same == 0, buf);
  snprintf(buf, sizeof buf,
           "%s: and it does NOT read \"%s\" (%d px would change)", ctx,
           otherStr, diff);
  want(diff > 0, buf);
}

static void a32(void) {
  FohState s;
  goto_gameplay_row4(&s);

  // [A32-1] the label.
  render_shot(&s, &g_a);
  {
    const int same = overdraw(&g_a, OPT_LABEL_X, OPT_ROW4_Y, "TAP JUMP", kText);
    const int old = overdraw(&g_a, OPT_LABEL_X, OPT_ROW4_Y, "TAPJUMP OFF",
                             kText);
    char buf[200];
    snprintf(buf, sizeof buf,
             "the per-player row's LABEL on screen is \"TAP JUMP\" (%d px "
             "differ)", same);
    want(same == 0, buf);
    snprintf(buf, sizeof buf,
             "TOOTH: it is NOT the double negative \"TAPJUMP OFF\" the owner "
             "misread (%d px would change)", old);
    want(old > 0, buf);
  }

  // [A32-2]/[A32-3] the value, in both states, reached by the real gesture.
  want(s.tapJumpOff[0] == 0,
       "a fresh machine has tap jump ENABLED for P1 (tapJumpOff == 0)");
  assert_row4_value(&s, "ON", kAccent, "OFF", kDim, "tap jump ENABLED");

  PRESS(&s, a);
  want(s.tapJumpOff[0] == 1,
       "A on the cell DISABLES tap jump for P1 (state plane keeps upstream's "
       "polarity: tapJumpOff == 1)");
  assert_row4_value(&s, "OFF", kDim, "ON", kAccent, "tap jump DISABLED");

  PRESS(&s, a);
  want(s.tapJumpOff[0] == 0, "a second A re-enables it (the toggle is a toggle)");
  assert_row4_value(&s, "ON", kAccent, "OFF", kDim, "tap jump ENABLED again");
}

// --- [A25a] the target-select highlight -------------------------------------
// Slot k's OUTER rect. A25(c): READ from foh_tss_slots() — the table render_tss
// draws from and foh.c hit-tests against — instead of the hand copy that stood
// here. Two of check-legibility.sh's grammar pins existed only to keep that
// copy honest and are retired with it; sharing the table gives the same
// guarantee structurally, which is what D4 wanted all along.
static void slot_rect(int k, int *x0, int *y0, int *x1, int *y1) {
  FohHandRect r[FOH_TSS_SLOTS];
  foh_tss_slots(r);
  *x0 = r[k].x - 2;
  *y0 = r[k].y - 2;
  *x1 = r[k].x + r[k].w + 2;
  // D24's measured asymmetry: the "+ ADD CODE" ring grows 1 px on the bottom,
  // not 2, because the info panel's 50%-black face starts on the row below it.
  *y1 = r[k].y + r[k].h + (k == 10 ? 1 : 2);
}

// Drive the REAL cursor to slot k from wherever it is.
//
// DEVIATION D29 (A25c) replaced the d-pad index cursor with the free hand, so
// this is a WALK now, not a press count: hold a direction until the machine
// reports the hand has arrived, which is the same feedback discipline the old
// version used for its arrival assertion.
//
// Then the hand is PARKED. That is not tidiness — it is what keeps the "quiet
// unless selected" assertions below measurable: the hand is DRAWN now, and a
// 24x32 sprite sitting on the slot it selected would put pixels inside a
// neighbouring slot's rect and read as noise. Parking puts the sprite at the
// SAME pixel in every frame this witness compares, and D29's STICKY selection
// is what allows it — the park hovers no slot, so the selection stays where the
// walk left it.
//
// The ROUTE matters as much as the spot, and this cost a measured failure:
// sticky selection means any slot the hand CROSSES on the way out re-selects,
// so a straight run to a screen corner walks the selection off the slot under
// test. The clean exit is the vertical gutter between the two columns (x 120:
// column 0 ends at 108, column 1 starts at 132, and "+ ADD CODE" — the only
// slot that spans it — sits BELOW every grid row), then UP to the clamp. Park
// = (120, 0); the sprite's hot spot is (10,7), so it covers x 110..133,
// y -7..24, and the topmost slot rect starts at y 28.
static void hold(FohState *s, size_t off, int n) {
  PlatformInput in;
  memset(&in, 0, sizeof in);
  *(bool *)((char *)&in + off) = true;
  for (int i = 0; i < n; i++) foh_tick(s, &in);
  memset(&in, 0, sizeof in);
  foh_tick(s, &in);
}
#define HOLD(s, field, n) hold((s), offsetof(PlatformInput, field), (n))

static void walk_axis_to(FohState *s, int axis, double aim) {
  for (int i = 0; i < 4000; i++) {
    const double cur = axis == 0 ? s->tssHandX : s->tssHandY;
    const double d = aim - cur;
    if (d > -2.0 && d < 2.0) break;
    PlatformInput in;
    memset(&in, 0, sizeof in);
    if (axis == 0) { if (d > 0) in.right = true; else in.left = true; }
    else { if (d > 0) in.down = true; else in.up = true; }
    foh_tick(s, &in);
  }
  PlatformInput idle;
  memset(&idle, 0, sizeof idle);
  foh_tick(s, &idle);
}

static bool goto_slot(FohState *s, int k) {
  FohHandRect r[FOH_TSS_SLOTS];
  foh_tss_slots(r);
  walk_axis_to(s, 0, r[k].x + r[k].w / 2.0);
  walk_axis_to(s, 1, r[k].y + r[k].h / 2.0);
  if (s->tssCursor != k) return false;
  // out through the inter-column gutter, then up to the clamp
  walk_axis_to(s, 0, (r[0].x + r[0].w + r[5].x) / 2.0);
  HOLD(s, up, 80);
  return s->tssCursor == k && s->tssHandY == 0.0;
}

static void a25a(void) {
  FohState s;
  boot_to_menu(&s);
  PRESS(&s, down);
  PRESS(&s, a);
  want(s.screen == FOH_TSS, "menu top row 1 (TARGET TEST) opens target select");

  for (int k = 0; k < 11; k++) {
    char buf[220];
    int x0, y0, x1, y1;
    slot_rect(k, &x0, &y0, &x1, &y1);
    const char *name = k == 10 ? "+ ADD CODE" : "TARGET";

    // Two DIFFERENT elsewhere cursors, so "this slot is quiet unless it is
    // the selected one" is measured and not assumed. They are +3/+7 rather
    // than the neighbours, because MEASURED: the grid pitch is 22 and a
    // selected slot's outer rect is 23 tall, so a slot and the one directly
    // below it share exactly one boundary ROW of rect — 104 px that belong
    // to whichever of the two is selected. Nothing is drawn twice (only one
    // slot is ever selected), but comparing a slot's rect against a frame
    // whose cursor sits on its own neighbour would measure that shared row
    // rather than the slot. +3 and +7 are never same-column neighbours for
    // any k in 0..10.
    const int away1 = (k + 3) % 11, away2 = (k + 7) % 11;
    if (!goto_slot(&s, away1)) { bad("cursor walk did not reach a slot"); return; }
    render_shot(&s, &g_a);
    if (!goto_slot(&s, away2)) { bad("cursor walk did not reach a slot"); return; }
    render_shot(&s, &g_b);
    const int noise = fb_diff_in(&g_a, &g_b, x0, y0, x1, y1);
    snprintf(buf, sizeof buf,
             "slot %d (%s) is byte-identical under two different elsewhere "
             "cursors (%d px differ)", k, name, noise);
    want(noise == 0, buf);

    if (!goto_slot(&s, k)) { bad("cursor walk did not reach the slot"); return; }
    render_shot(&s, &g_a);
    const int n = fb_diff_in(&g_a, &g_b, x0, y0, x1, y1);
    snprintf(buf, sizeof buf,
             "slot %d (%s) SELECTED changes %d px in its own rect x[%d,%d) "
             "y[%d,%d) — a 1 px border can reach at most 242, the floor here "
             "is %d", k, name, n, x0, x1, y0, y1, LEGIBILITY_MIN_PX);
    want(n >= LEGIBILITY_MIN_PX, buf);
  }
}

int main(void) {
  a32();
  a25a();
  if (g_fails) {
    printf("LEGIBILITY: %d assertion(s) failed\n", g_fails);
    return 1;
  }
  printf("LEGIBILITY OK\n");
  return 0;
}
