// port/foh/foh_controls_witness.c — punch-list A24 + A4: the Controls menu's
// LABELS and, inseparably, its ROUTING.
//
// THE TICKET, in the owner's words (2026-08-23)
// ---------------------------------------------
//   "controls option in the menu says 'Keyboard Controls'. Clicking it takes
//    you to a menu that says 'Controller' and 'Keyboard', so it shouldn't say
//    'Keyboard controls' — just Controls. Then, in the submenu, 'Keyboard'
//    actually isn't a keyboard, it's the funkey s controls. So we need to
//    change the name there. Also make funkey controls the first option
//    (controller is first currently, and it shouldn't be, because we can't
//    even use a controller on the funkey s)."
//
// Ratified: the options row becomes `CONTROLS`; the submenu entry becomes
// `HANDHELD` (a CATEGORY name, parallel to `CONTROLLER`, and shorter than it
// so no width pin moves); `HANDHELD` goes FIRST. The control STYLE named
// `NORMAL` reads `CLASSIC` (C31 shipped that string already — asserted here so
// the owner-visible half of A4 has a tooth, not a memory).
//
// WHY THIS IS NOT A LABEL CHECK
// -----------------------------
// The submenu is INDEX-selected: `foh.c`'s step_menu maps menuSelected 0/1 to
// FOH_CTRL_KEY / FOH_CTRL_PAD. Moving the two strings in `foh_render.c` without
// moving that ternary leaves the first row PAINTED "HANDHELD" and WIRED to the
// controller screen — a defect no assertion about strings can see. So every
// row claim here is BOUND to its destination: the witness reads the row's
// label off the frame, presses A on that row through the real foh_tick, and
// reads the HEADER off the frame the machine actually lands on. A half-swap
// fails on the pairing even though both halves are individually "present".
//
// WHAT IT ASSERTS
//   [D25-1] the Options page's row 2 reads "CONTROLS", and NOT the old
//           "KEYBOARD CONTROLS";
//   [D25-2] the Controls page reads "HANDHELD" on row 0 and "CONTROLLER" on
//           row 1, and row 0 is NOT "CONTROLLER" (the pre-A24 order);
//   [D25-3] A on row 0 lands on the screen headed "HANDHELD" and A on row 1 on
//           the screen headed "CONTROLLER" — the label/routing binding;
//   [D25-4] IDENTITY DID NOT MOVE: those two screens are still FOH_CTRL_KEY
//           and FOH_CTRL_PAD and their tokens are still "controls-keyboard" /
//           "controls-controller" (the judge grammar and every frozen flow
//           expect key on those, and upstream gameMode 12 / 14 with them);
//   [A4]    the STYLE row on the HANDHELD screen reads CLASSIC / BOX / NATURAL
//           across the real L/R cycle, and never the old ambiguous "NORMAL";
//   [D25-5] the explanation-bar width pin still names the widest blurb, so the
//           new HANDHELD blurb did not silently outgrow the panel;
//   [D27-1] THE ROUTE, at whichever value of foh.h's FOH_CTL_CHOOSER this
//           file is compiled with. At 0 (the shipped build, owner ruling
//           2026-08-23 "collapse now - make easily revertable though
//           please") A on the Options CONTROLS row opens the HANDHELD screen
//           DIRECTLY and its B returns to Options ON THE CONTROLS ROW; at 1
//           the pre-D27 chooser sits between them and B walks back through
//           it. ONE `#define` decides which, and check-controls-labels.sh
//           builds this witness BOTH ways, so the restorable path cannot rot;
//   [D27-2] the chooser page is WHOLE at either value — every claim in
//           [D25-2/3/4] is asserted at 0 too, with the page seated directly
//           because navigation no longer reaches it. That is what makes
//           "nothing was deleted" a measurement instead of a promise.
//
// THE INSTRUMENT is foh_legibility_witness.c's, verbatim in method: a rendered
// string is asserted by OVERDRAWING the claimed string, at the claimed place,
// in the claimed colour, onto the real frame and requiring ZERO pixels to
// change. Text composites opaquely (foh_font.c), so drawing what is already
// there is a no-op and drawing anything else is not. Every gesture goes
// through the real foh_tick as button levels.
//
// Usage: foh_controls_witness. Prints one line per assertion and `CONTROLS OK`
// on success, exit 0. Any failure prints `CONTROLS FAIL:` and exits 1. Its
// negative tests live in port/foh/check-controls-labels.sh.
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../gfx/ctl_style.h"
#include "../gfx/raster.h"
#include "foh.h"

// gfx_fatal is host-provided (raster.h), the foh_legibility_witness.c pattern.
void gfx_fatal(const char *what) {
  fprintf(stderr, "CONTROLS FAIL: gfx_fatal: %s\n", what);
  exit(3);
}

// --- the numbers this witness HAND COPIES from foh_render.c -----------------
// Every one is pinned line-for-line by check-controls-labels.sh, exactly as
// check-legibility.sh pins the lines its witness copies. If the renderer moves
// a bar or repaints a palette entry this dies loudly instead of quietly
// asserting against a screen that no longer exists.
static const RastCol kAccent = {255, 200, 60, 256};  // foh_render.c kAccent
static const RastCol kBarSel = {254, 238, 27, 256};  // `sel`, unselected label
static const RastCol kBarSelTx = {0, 0, 0, 256};     // `selTx`, selected label
#define FOH_BAR_STEP 11   // #define FOH_BAR_STEP
#define FOH_BAR_TOP 58    // #define FOH_BAR_TOP
#define FOH_BAR_PITCH 31  // #define FOH_BAR_PITCH
#define HDR_Y 6           // header(): text_center(rz, 6, 2, title, kAccent)
#define HDR_SCALE 2
#define CTL_STYLE_ROW_X 16   // render_ctrl_key's `foh_text(rz, 16, yStyle, ...`
#define CTL_STYLE_ROW_Y 176  // `const int yStyle = 176, yReset = 190;`
// A31 made the nine ACTION rows cursor rows, so the style row is no longer
// row 0: the cursor opens on the d-pad row and the screen is walked down to
// the style row exactly as a player would. foh.h owns the row layout; this
// is the only number this file needs from it.
#define CTL_STYLE_ROW FOH_CTL_ROW_STYLE
// The explanation bar's pinned width claim (foh_render.c's comment at the bar):
// the longest blurb is 230 px at face-2 scale 1, so the bar runs 4..236.
#define BLURB_MAX_PX 230
#define BLURB_WIDEST "CUSTOMIZE & CALIBRATE CONTROLLER."

static int g_fails;

static void ok(const char *what) { printf("  ok  %s\n", what); }

static void bad(const char *what) {
  printf("CONTROLS FAIL: %s\n", what);
  g_fails++;
}

static void want(int cond, const char *what) {
  if (cond) ok(what);
  else bad(what);
}

// --- rendering ---------------------------------------------------------------
// One frame as a judged shot would see it: a pure function of MACHINE state
// with the LOOK plane pinned exactly as every shot path pins it.
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

// Pixels that change when `text` is drawn over `live` at (x, y) in `col`.
// Zero means the screen already carries exactly that string, in that colour,
// at that spot. Face 2 (the menu bars' proportional face), italic as drawn.
static int overdraw2(const Raster *live, int x, int y, const char *text,
                     RastCol col) {
  g_probe = *live;
  foh_text2(&g_probe, x, y, 1, 1, text, col);
  return fb_diff(live, &g_probe);
}

// Face 1 (the 5x7 fixed face), used by header() and the ctl rows.
static int overdraw1(const Raster *live, int x, int y, int scale,
                     const char *text, RastCol col) {
  g_probe = *live;
  foh_text(&g_probe, x, y, scale, text, col);
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

static void boot_to_menu(FohState *s) {
  foh_init(s);
  int guard = 0;
  while (s->screen != FOH_TITLE && guard++ < 100000) neutral(s);
  want(s->screen == FOH_TITLE, "machine reaches the title screen");
  PRESS(s, start);
  want(s->screen == FOH_MENU_TOP, "START leaves the title for the menu top");
}

// f04-nav.flow's own path: three DOWNs to OPTIONS, A.
static void goto_options(FohState *s) {
  boot_to_menu(s);
  PRESS_N(s, down, 3);
  PRESS(s, a);
  want(s->screen == FOH_MENU_OPTIONS, "menu top row 3 (OPTIONS) opens options");
}

// --- the menu-bar label reader ----------------------------------------------
// The bars are laid out by foh_render's own expressions: the label is centred
// on x = 123 - FOH_BAR_STEP*k and sits FOH_BAR_TOP + FOH_BAR_PITCH*k + 6 down.
// The SELECTED row's label is black (`selTx`); every other row's is yellow
// (`sel`), which is why the colour is a parameter here rather than a constant.
//
// MEASURED, so the next reader is not surprised: the selected row's PULSE
// RINGS are drawn AFTER its label, so a label long enough to reach the cap
// gets a few pixels painted over and can no longer overdraw to zero. Every
// label this witness asserts measures exactly 0 today; if a future rename
// grows one into the ring this fails LOUDLY (the row "does not read" what it
// says) rather than passing quietly, which is the right direction.
static void assert_row_label(const FohState *s, int k, bool selected,
                             const char *wantStr, const char *notStr,
                             const char *ctx) {
  render_shot(s, &g_a);
  const RastCol col = selected ? kBarSelTx : kBarSel;
  const int y = FOH_BAR_TOP + FOH_BAR_PITCH * k + 6;
  char buf[240];

  const int wx = 123 - FOH_BAR_STEP * k - foh_text2_width(wantStr, 1) / 2;
  const int same = overdraw2(&g_a, wx, y, wantStr, col);
  snprintf(buf, sizeof buf,
           "%s: row %d on screen reads \"%s\" (%d px differ from that string "
           "drawn over it)", ctx, k, wantStr, same);
  want(same == 0, buf);

  const int nx = 123 - FOH_BAR_STEP * k - foh_text2_width(notStr, 1) / 2;
  const int diff = overdraw2(&g_a, nx, y, notStr, col);
  snprintf(buf, sizeof buf,
           "TOOTH: %s: row %d does NOT read \"%s\" (%d px would change)", ctx,
           k, notStr, diff);
  want(diff > 0, buf);
}

// The screen header, read off the frame the machine is actually on.
static void assert_header(const FohState *s, const char *wantStr,
                          const char *notStr, const char *ctx) {
  render_shot(s, &g_a);
  char buf[240];
  const int wx = (RAST_W - foh_text_width(wantStr, HDR_SCALE)) / 2;
  const int same = overdraw1(&g_a, wx, HDR_Y, HDR_SCALE, wantStr, kAccent);
  snprintf(buf, sizeof buf, "%s: the screen HEADER reads \"%s\" (%d px differ)",
           ctx, wantStr, same);
  want(same == 0, buf);

  const int nx = (RAST_W - foh_text_width(notStr, HDR_SCALE)) / 2;
  const int diff = overdraw1(&g_a, nx, HDR_Y, HDR_SCALE, notStr, kAccent);
  snprintf(buf, sizeof buf,
           "TOOTH: %s: the header is NOT \"%s\" (%d px would change)", ctx,
           notStr, diff);
  want(diff > 0, buf);
}

// --- [D25-1] the Options row ------------------------------------------------
static void d25_options_row(void) {
  FohState s;
  goto_options(&s);
  PRESS_N(&s, down, 2);
  want(s.menuSelected == 2, "two DOWNs land on the Controls row (index 2)");
  assert_row_label(&s, 2, true, "CONTROLS", "KEYBOARD CONTROLS",
                   "options page");
}

// Put the machine ON the chooser page with the cursor on `row`.
//
// At FOH_CTL_CHOOSER 1 that is a NAVIGATION — A on the Options CONTROLS row,
// through the real foh_tick, exactly as a player reaches it.
// At 0 (DEVIATION D27) the page is UNREACHABLE, so it is SEATED directly.
// That is deliberate and it is the foh_snd_witness.c `battle-B` idiom: the
// chooser's arms are compiled unconditionally at either flag value, and the
// owner asked for a collapse that is EASILY REVERTIBLE — a restorable page
// nobody exercises is restorable in name only. Everything below this seat is
// therefore identical in both builds, T2 (the routing half-swap trap) included.
static void seat_chooser(FohState *s, int row) {
  goto_options(s);
  PRESS_N(s, down, 2);
#if FOH_CTL_CHOOSER
  PRESS(s, a);
  want(s->screen == FOH_MENU_CONTROLS, "A on the Controls row opens the chooser");
  want(s->menuSelected == 0, "the chooser opens on row 0 (menu.js:139-141)");
  if (row != 0) {
    PRESS(s, down);
    want(s->menuSelected == row, "DOWN moves the chooser cursor to row 1");
  }
#else
  want(s->screen == FOH_MENU_OPTIONS,
       "D27: the Options page is as far as NAVIGATION reaches the chooser");
  s->screen = FOH_MENU_CONTROLS;
  s->menuSelected = row;
#endif
}

// --- [D25-2/3/4] the chooser: labels BOUND to destinations ------------------
// Each row is proved twice over: what it SAYS, and where pressing A on it
// GOES. The second half is the one a label-only check would miss, and it is
// where the index-selection trap lives.
static void d25_chooser(void) {
  FohState s;
  seat_chooser(&s, 0);

  // Both rows in ONE frame: row 0 is the selected one (black), row 1 is not.
  assert_row_label(&s, 0, true, "HANDHELD", "CONTROLLER", "controls chooser");
  assert_row_label(&s, 1, false, "CONTROLLER", "HANDHELD", "controls chooser");

  // ROW 0 -> the HANDHELD screen. The whole A24 ticket is this pairing.
  PRESS(&s, a);
  want(s.screen == FOH_CTRL_KEY,
       "A on row 0 lands on FOH_CTRL_KEY (upstream gameMode 12 — IDENTITY "
       "unchanged, only the row order moved)");
  want(strcmp(foh_screen_token(s.screen), "controls-keyboard") == 0,
       "IDENTITY: row 0's screen token is still \"controls-keyboard\" (the "
       "judge grammar and the frozen flow expects key on it)");
  assert_header(&s, "HANDHELD", "CONTROLLER", "row 0's destination");

  // ROW 1 -> the CONTROLLER screen. It is UNREACHABLE by navigation at
  // FOH_CTL_CHOOSER 0 (D27 — A33 measured that the shipped OS image compiles
  // no USB host mode), and it is still WHOLE: this is what proves the collapse
  // deleted nothing.
  seat_chooser(&s, 1);
  PRESS(&s, a);
  want(s.screen == FOH_CTRL_PAD,
       "A on row 1 lands on FOH_CTRL_PAD (upstream gameMode 14 — the arm is "
       "compiled at either flag value)");
  want(strcmp(foh_screen_token(s.screen), "controls-controller") == 0,
       "IDENTITY: row 1's screen token is still \"controls-controller\"");
  assert_header(&s, "CONTROLLER", "HANDHELD", "row 1's destination");
}

// --- [D27] the route the OWNER actually walks -------------------------------
// The collapse is a ROUTING claim in both directions, so both are pressed
// through the real foh_tick: what A on the Options CONTROLS row opens, and
// where B from that screen comes back to (and onto which row).
static void d27_route(void) {
  FohState s;
  goto_options(&s);
  PRESS_N(&s, down, 2);
  want(s.menuSelected == 2, "two DOWNs land on the CONTROLS row (index 2)");
  PRESS(&s, a);
#if FOH_CTL_CHOOSER
  want(s.screen == FOH_MENU_CONTROLS,
       "FOH_CTL_CHOOSER 1: the CONTROLS row opens the CHOOSER (the pre-D27 "
       "route, restored whole by the flag)");
  PRESS(&s, a);
  want(s.screen == FOH_CTRL_KEY, "and row 0 opens the HANDHELD screen");
  PRESS(&s, b);
  want(s.screen == FOH_MENU_CONTROLS, "B returns to the chooser");
  want(s.menuSelected == 0, "and lands back on the row it left from");
  PRESS(&s, b);
  want(s.screen == FOH_MENU_OPTIONS, "a second B returns to the Options page");
  want(s.menuSelected == 0,
       "with upstream's cursor reset to AUDIOOPTIONS (menu.js:170)");
#else
  want(s.screen == FOH_CTRL_KEY,
       "D27: the CONTROLS row opens the HANDHELD screen DIRECTLY — no chooser "
       "step (owner ruling 2026-08-23; changeGamemode(12), menu.js:159-161)");
  want(strcmp(foh_screen_token(s.screen), "controls-keyboard") == 0,
       "IDENTITY: the collapsed route's screen token is still "
       "\"controls-keyboard\" — routing moved, identity did not");
  assert_header(&s, "HANDHELD", "CONTROLLER",
                "the collapsed CONTROLS row's destination");
  PRESS(&s, b);
  want(s.screen == FOH_MENU_OPTIONS,
       "B from the HANDHELD screen returns to the Options page in ONE press");
  want(s.menuSelected == 2,
       "and lands on the CONTROLS row that opened it (nothing on the way in "
       "touched menuSelected, because there is no chooser cursor to seat)");
#endif
}

// --- [A4] the control-style row ---------------------------------------------
// Reached by the REAL gesture: enter row 0's screen and cycle the style row
// with RIGHT, asserting the rendered string at each stop. The fresh-install
// style is NATURAL (CTL_STYLE_DEFAULT), so RIGHT walks NATURAL -> CLASSIC ->
// BOX -> NATURAL across the three-value wrap.
static void assert_style_row(const FohState *s, const char *wantStr,
                             const char *notStr) {
  render_shot(s, &g_a);
  char buf[240];
  char w[64], n[64];
  snprintf(w, sizeof w, "STYLE: %s", wantStr);
  snprintf(n, sizeof n, "STYLE: %s", notStr);
  const int same =
      overdraw1(&g_a, CTL_STYLE_ROW_X, CTL_STYLE_ROW_Y, 1, w, kAccent);
  snprintf(buf, sizeof buf, "the style row on screen reads \"%s\" (%d px differ)",
           w, same);
  want(same == 0, buf);
  const int diff =
      overdraw1(&g_a, CTL_STYLE_ROW_X, CTL_STYLE_ROW_Y, 1, n, kAccent);
  snprintf(buf, sizeof buf, "TOOTH: and it is NOT \"%s\" (%d px would change)",
           n, diff);
  want(diff > 0, buf);
}

static void a4_style_names(void) {
  FohState s;
  goto_options(&s);
  PRESS_N(&s, down, 2);
  PRESS(&s, a);
#if FOH_CTL_CHOOSER
  PRESS(&s, a);  // D27 off: the chooser sits between the row and the screen
#endif
  want(s.screen == FOH_CTRL_KEY, "the HANDHELD screen is where the styles live");
  want(s.ctlRow == 0, "its cursor opens on the FIRST row (A31 row layout)");
  PRESS_N(&s, down, CTL_STYLE_ROW);
  want(s.ctlRow == CTL_STYLE_ROW,
       "nine DOWNs walk the cursor from the d-pad row to the STYLE row");

  want(ctl_style_get() == CTL_STYLE_DEFAULT &&
           CTL_STYLE_DEFAULT == CTL_STYLE_NATURAL,
       "a fresh process is on the ratified default style (NATURAL)");
  assert_style_row(&s, "NATURAL", "CLASSIC");

  PRESS(&s, right);
  want(ctl_style_get() == CTL_STYLE_NORMAL,
       "RIGHT cycles to CTL_STYLE_NORMAL — the ENUM VALUE is a frozen wire "
       "format (FohPersist.ctlStyle) and is untouched by the rename");
  assert_style_row(&s, "CLASSIC", "NORMAL");

  PRESS(&s, right);
  want(ctl_style_get() == CTL_STYLE_BOX, "RIGHT again reaches BOX");
  assert_style_row(&s, "BOX", "CLASSIC");

  PRESS(&s, right);
  want(ctl_style_get() == CTL_STYLE_NATURAL, "and RIGHT wraps back to NATURAL");
  assert_style_row(&s, "NATURAL", "BOX");
}

// --- [D25-5] the explanation-bar width pin ----------------------------------
// foh_render.c sizes the bar off "the longest blurb". The HANDHELD blurb is
// new prose, so that claim is MEASURED here rather than counted by hand: every
// blurb the four pages can draw must fit, and the named one must still be the
// widest of them.
static void d25_blurb_width(void) {
  static const char *const kAll[] = {
      "MULTIPLAYER BATTLES!", "SMASH TEN TARGETS!", "BUILD TARGET TEST STAGES!",
      "GAME SETUP.", "SELECT AUDIO LEVELS.", "CHANGE GAMEPLAY SETTINGS.",
      "CUSTOMIZE & CALIBRATE CONTROLS.", "WHO DID THIS?",
      "ONE BOX THIS SCREEN.", "RANKED MODE", "HOSTLESS MULIPLAYER",
      "HOSTED MULTIPLAYER", "CUSTOMIZE THE HANDHELD CONTROLS.",
      "CUSTOMIZE & CALIBRATE CONTROLLER."};
  const int n = (int)(sizeof kAll / sizeof kAll[0]);
  int widest = 0;
  const char *who = "";
  for (int i = 0; i < n; i++) {
    const int w = foh_text2_width(kAll[i], 1);
    if (w > widest) { widest = w; who = kAll[i]; }
  }
  char buf[240];
  snprintf(buf, sizeof buf,
           "the widest blurb is still \"%s\" at %d px (the pin names \"%s\")",
           who, widest, BLURB_WIDEST);
  want(strcmp(who, BLURB_WIDEST) == 0, buf);
  snprintf(buf, sizeof buf,
           "and it still fits the pinned %d px bar (%d px) — the panel "
           "geometry did not move", BLURB_MAX_PX, widest);
  want(widest <= BLURB_MAX_PX, buf);
}

int main(void) {
  d25_options_row();
  d25_chooser();
  d27_route();
  a4_style_names();
  d25_blurb_width();
  if (g_fails) {
    printf("CONTROLS: %d assertion(s) failed\n", g_fails);
    return 1;
  }
  printf("CONTROLS OK\n");
  return 0;
}
