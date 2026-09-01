// port/foh/foh_cssrest_witness.c — the CSS TOKEN REST witness
// (punch-list A29, owner playthrough #3 round 2).
//
// THE DEFECT THIS GUARDS. The owner picked falco for P1 and marth for P2,
// played a match, quit, came back to the CSS — and the roster showed marth
// and puff, i.e. a constant `{0, 1}` matching neither the picks nor foh_init's
// `{0, 0}` defaults. MEASURED (this driver's own first run, before the fix):
// BOTH character planes survive the match exit intact. `p1Char`/`p2Char` and
// `cssChar[]` still read the picks. What lied was the TOKEN: foh_dev.c's
// tdev_end_game puts every port's token in rest slot 2 (the endGame snap,
// main.js:1382-1385), and that slot indexed the roster by the PORT number, so
// port 0's token landed on cell 0 (marth) and port 1's on cell 1 (puff)
// whatever those ports had chosen. On 240x240 the token is the ONLY
// roster-level indicator of who is picked — render_css draws no selected-cell
// highlight, only a hover one — so an index-identity token reads as "my pick
// was discarded". DEVIATION D21 at foh.c's slot-2 arm carries the argument.
//
// WHAT THIS ASSERTS, and in which order:
//   [1] a NON-DEFAULT pair reaches both planes through the REAL gestures
//       (grab, hover, drop) — no field is hand-poked to make a pick;
//   [2] both tokens rest INSIDE the cell of the character their port picked;
//   [3] after the A19 match-exit mutation, [1] AND [2] still hold.
//
// The picks are P1 falco (3) and P2 falcon (4) rather than the owner's exact
// falco/marth, on purpose: marth is 0, which is both foh_init's default AND
// port 0's index, so a marth pick cannot tell "preserved" from "reset" from
// "index identity". {3, 4} differs from all three.
//
// WHY A UNIT DRIVER. The subject is a state the machine can only be in AFTER a
// match exit, and the flow-fed rigs end their FOH phase at the launch. The
// state machine here is the REAL one — every CSS gesture below is driven by
// feeding button levels and edges to foh_tick(), and the cursor is walked by
// FEEDBACK (hold the direction until the machine reports the hand is where we
// want it), never by a frame count copied out of a flow file.
//
// The one hand copy is foh_dev.c's re-entry mutation, which is not a function
// and cannot be called from here — the same obligation
// foh_launchkind_witness.c carries and states. It is NOT left to hope:
// port/foh/check-css-token-rest.sh
// grammar-checks foh_dev.c for the two lines modelled below and dies loudly if
// either moves.
//
// Prints one line per assertion and `CSS TOKEN REST OK` on success, exit 0. Any
// failure prints `CSS TOKEN REST FAIL: ...` and exits 1. Its negative test
// lives in port/foh/check-css-token-rest.sh: a COPY of foh.c with the D21 arm
// put back to the port index must make this driver fail.
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "foh.h"

// gfx_fatal is host-provided (raster.h), the foh_launchkind_witness.c pattern.
// Nothing this driver reaches should trip it; if something does, that is a
// failure of the witness, not a pass.
void gfx_fatal(const char *what) {
  fprintf(stderr, "CSS TOKEN REST FAIL: gfx_fatal: %s\n", what);
  exit(3);
}

static int g_fails;

static void ok(const char *what) { printf("  ok  %s\n", what); }

static void bad(const char *what) {
  printf("CSS TOKEN REST FAIL: %s\n", what);
  g_fails++;
}

static void want(int cond, const char *what) {
  if (cond) ok(what);
  else bad(what);
}

static const char *const kName[5] = {"marth", "puff", "fox", "falco", "falcon"};

// The roster cell a token's DRAWN centre falls in, or -1 for none. This is the
// player-visible question the whole witness is about: "is my token on the guy I
// picked". Same cell extents foh.c hit-tests and foh_render.c draws (foh.h is
// the single source for both, D4).
static int token_cell(const FohState *s, int k) {
  double x, y;
  foh_css_token_pos(s, k, &x, &y);
  for (int c = 0; c < 5; c++) {
    const double x0 = (double)foh_css_cell_x(c);
    if (x > x0 && x < x0 + (double)FOH_CSS_CELL_W) return c;
  }
  return -1;
}

// One tick with a single field held, then one neutral tick — exactly one
// rising edge, which is what every edge-driven FOH arm reads.
static void press(FohState *s, size_t off) {
  PlatformInput in;
  memset(&in, 0, sizeof in);
  *(bool *)((char *)&in + off) = true;
  foh_tick(s, &in);
  memset(&in, 0, sizeof in);
  foh_tick(s, &in);
}

#define PRESS(s, field) press((s), offsetof(PlatformInput, field))

// Walk the hand until `pred` holds, holding `off` the whole way. The CSS
// cursor is LEVEL driven (MENU-SPEC §2.2), so a held direction IS the gesture.
// Returns false if the guard runs out — a walk that never arrives must FAIL
// the witness, not silently continue from wherever it stopped.
static bool walk_until(FohState *s, size_t off,
                       bool (*pred)(const FohState *)) {
  PlatformInput in;
  memset(&in, 0, sizeof in);
  *(bool *)((char *)&in + off) = true;
  for (int i = 0; i < 4000 && !pred(s); i++) foh_tick(s, &in);
  const bool got = pred(s);
  memset(&in, 0, sizeof in);
  foh_tick(s, &in);
  return got;
}

#define WALK(s, field, pred) \
  walk_until((s), offsetof(PlatformInput, field), (pred))

// --- the predicates the walks aim at ----------------------------------------
static bool at_top(const FohState *s) { return s->cssHandY <= 0.0; }
static bool at_left(const FohState *s) { return s->cssHandX <= 0.0; }
// Inside the roster band AND inside the cell row: the only place the hover arm
// selects a character (foh.c's band branch).
static bool in_cells(const FohState *s) {
  return s->cssHandY > (double)FOH_CSS_CELL_Y &&
         s->cssHandY < (double)(FOH_CSS_CELL_Y + FOH_CSS_CELL_H) &&
         s->cssHandY < (double)FOH_CSS_BAND_BOT &&
         s->cssHandY > (double)FOH_CSS_BAND_TOP;
}
// Over port 1's type tab, and its two halves — the walk approaches it the way
// the f01 flow does, one axis at a time (foh.c's out-of-band branch hit-tests
// exactly this rect).
static bool x_on_p2_tab(const FohState *s) {
  return s->cssHandX > (double)foh_css_panel_x(1) &&
         s->cssHandX < (double)(foh_css_panel_x(1) + FOH_CSS_TAB_W);
}
static bool below_p2_tab(const FohState *s) {
  return s->cssHandY > (double)(FOH_CSS_PANEL_Y + FOH_CSS_TAB_H);
}
static bool on_p2_tab(const FohState *s) {
  return x_on_p2_tab(s) && s->cssHandY > (double)FOH_CSS_PANEL_Y &&
         s->cssHandY < (double)(FOH_CSS_PANEL_Y + FOH_CSS_TAB_H);
}
// Over port 1's resting token, which is what the A-grab loop hit-tests — and
// its y half, so the approach can settle one axis at a time.
static bool y_on_p2_token(const FohState *s) {
  double tx, ty;
  foh_css_token_pos(s, 1, &tx, &ty);
  (void)tx;
  return s->cssHandY > ty - (double)FOH_CSS_TOKEN_R &&
         s->cssHandY < ty + (double)FOH_CSS_TOKEN_R;
}
static bool on_p2_token(const FohState *s) {
  double tx, ty;
  foh_css_token_pos(s, 1, &tx, &ty);
  (void)ty;
  return y_on_p2_token(s) && s->cssHandX > tx - (double)FOH_CSS_TOKEN_R &&
         s->cssHandX < tx + (double)FOH_CSS_TOKEN_R;
}
static bool p1_on_falco(const FohState *s) { return s->cssChar[0] == 3; }
static bool p2_on_falcon(const FohState *s) { return s->cssChar[1] == 4; }

// The witness's whole subject, asserted twice (before and after the exit).
static void assert_picks_visible(const FohState *s, const char *when) {
  char buf[160];
  snprintf(buf, sizeof buf,
           "%s: both planes still read the picks (p1=%s p2=%s)", when,
           kName[s->p1Char], kName[s->p2Char]);
  want(s->p1Char == 3 && s->cssChar[0] == 3 && s->p2Char == 4 &&
           s->cssChar[1] == 4,
       buf);
  snprintf(buf, sizeof buf,
           "%s: P1's token rests on the cell of the character P1 picked "
           "(falco/3, got cell %d)",
           when, token_cell(s, 0));
  want(token_cell(s, 0) == 3, buf);
  snprintf(buf, sizeof buf,
           "%s: P2's token rests on the cell of the character P2 picked "
           "(falcon/4, got cell %d)",
           when, token_cell(s, 1));
  want(token_cell(s, 1) == 4, buf);
}

int main(void) {
  FohState s;
  foh_init(&s);

  // --- boot to the CSS ------------------------------------------------------
  // STARTUP -> TITLE is the timer transition; TITLE -> MENU_TOP is START;
  // MENU_TOP row 0 is `VS. Melee`, whose A runs menu.js:105 (owner ruling C5).
  {
    PlatformInput neutral;
    memset(&neutral, 0, sizeof neutral);
    int guard = 0;
    while (s.screen != FOH_TITLE && guard++ < 100000) foh_tick(&s, &neutral);
    want(s.screen == FOH_TITLE, "machine reaches the title screen");
  }
  PRESS(&s, start);
  want(s.screen == FOH_MENU_TOP, "START leaves the title for the menu top");
  PRESS(&s, a);
  want(s.screen == FOH_CSS, "menu top row 0 (VS. Melee) opens the CSS");
  want(s.p1Char == 0 && s.p2Char == 0 && s.cssChar[0] == 0 && s.cssChar[1] == 0,
       "the CSS opens on foh_init's marth/marth defaults");

  // --- P1 picks falco, through the real gesture -----------------------------
  // Anchor on the clamps rather than on frame counts (the f01 flow's own
  // discipline), then walk by feedback.
  want(WALK(&s, up, at_top), "hand walks up to the y clamp");
  want(WALK(&s, down, in_cells), "hand walks down into the roster cell row");
  PRESS(&s, b); // B in band, P1 HMN, empty-handed: retrieve own token
  want(s.cssCarry == 0, "B in the band retrieves P1's token (css.js:209-215)");
  want(WALK(&s, right, p1_on_falco),
       "carrying right over the cells selects falco LIVE (css.js:222-226)");
  want(s.p1Char == 3 && s.cssChar[0] == 3,
       "the hover arm wrote BOTH planes at its one site (foh.c:525-526)");
  PRESS(&s, a); // A-drop, in band -> rest slot 0
  want(s.cssCarry == -1 && s.cssTokenRest[0] == 0,
       "A drops P1's token into the A-drop rest slot");
  want(token_cell(&s, 0) == 3, "P1's dropped token rests on falco's cell");

  // --- port 2 becomes a CPU so its token is grabbable ------------------------
  // Upstream's ownership rule is `playerType[j] == 1 || i == j` (css.js:300):
  // one hand may move its OWN token or any CPU's, never another human's. This
  // device has one hand (D6), so port 1 must be CPU for P2 to be picked at all.
  want(WALK(&s, left, at_left), "hand walks left to the x clamp");
  want(WALK(&s, down, below_p2_tab), "hand walks down past the panel tabs");
  want(WALK(&s, right, x_on_p2_tab), "hand walks right under port 2's tab");
  want(WALK(&s, up, on_p2_tab), "hand walks up onto port 2's type tab");
  PRESS(&s, a); // N/A -> HMN
  PRESS(&s, a); // HMN -> CPU
  want(foh_css_port_type(&s, 1) == 1,
       "two A presses on port 2's tab cycle N/A -> HMN -> CPU (D5)");

  // --- P2 picks falcon, through the real gesture ----------------------------
  want(WALK(&s, up, y_on_p2_token),
       "hand walks back up to port 2's token row");
  // D61: A PORT THAT BECOMES A CPU PICKS ITS OWN CHARACTER, so port 2's
  // token is now resting on a cell this witness does not get to choose, and
  // the direction to reach it is whichever side of the hand it landed on.
  // The predicate was always dynamic (foh_css_token_pos); only the hard-coded
  // `left` assumed the home cell. Deriving the direction keeps this witness
  // about the TOKEN REST rule and independent of which character was drawn —
  // which is what it should have been either way.
  {
    double tx, ty;
    foh_css_token_pos(&s, 1, &tx, &ty);
    (void)ty;
    const bool tokenIsLeft = tx < s.cssHandX;
    want(tokenIsLeft ? WALK(&s, left, on_p2_token)
                     : WALK(&s, right, on_p2_token),
         "hand walks onto port 2's resting token");
  }
  PRESS(&s, a); // A on a CPU port's token grabs it (css.js:297-313)
  want(s.cssCarry == 1, "A on port 2's token grabs it");
  want(WALK(&s, right, p2_on_falcon),
       "carrying right over the cells selects falcon LIVE");
  PRESS(&s, a);
  want(s.cssCarry == -1 && s.cssTokenRest[1] == 0,
       "A drops P2's token into the A-drop rest slot");

  assert_picks_visible(&s, "before the match");

  // --- the A19 in-process match exit, as foh_dev.c performs it --------------
  // HAND COPY of the two lines tdev_end_game runs on the CSS plane
  // (foh_dev.c:1339-1340) plus the four the re-entry block clears
  // (foh_dev.c's `foh_phase:` return). This driver cannot call the production
  // code — tdev_end_game wants a GameState and the re-entry is inline in
  // main() — so port/foh/check-css-token-rest.sh grammar-checks foh_dev.c for
  // the two CSS lines below and fails loudly if either moves. Nothing here
  // detects drift in the rest of that block, and that is stated rather than
  // wished away (the foh_launchkind_witness.c lesson, review-mexit-r5 Low).
  s.cssCarry = -1;
  for (int k = 0; k < 2; k++) s.cssTokenRest[k] = 2; // the endGame SNAP slot
  s.launched = false;
  s.bHold = 0;
  s.nev = 0;
  s.nsnd = 0;
  s.screen = FOH_CSS; // MEX_CSS -> changeGamemode(2) (main.js:1386-1388)

  want(s.cssTokenRest[0] == 2 && s.cssTokenRest[1] == 2,
       "the match exit really put both tokens in the endGame SNAP slot "
       "(the defect's precondition, reproduced)");
  assert_picks_visible(&s, "after the match exit");

  if (g_fails) {
    printf("CSS TOKEN REST FAIL: %d assertion(s) failed\n", g_fails);
    return 1;
  }
  printf("CSS TOKEN REST OK\n");
  return 0;
}
