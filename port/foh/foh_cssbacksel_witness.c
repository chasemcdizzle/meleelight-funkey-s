// port/foh/foh_cssbacksel_witness.c — the CSS BACK-OUT SELECTION witness
// (punch-list A43, owner playtest 2026-08-24; DEVIATION D35).
//
// THE DEFECT THIS GUARDS, in the owner's words:
//   "if you go to the CSS back button while selected as any other character
//    except for falcon, and then come back in to the game, you are selected as
//    falcon. it should remember the last character you were. I think the
//    reason it picks falcon is because when you go to the back button it lets
//    go of the p1 pin and goes to the 'nearest' character. i'm not a big fan
//    of this behavior, because it doesn't choose that character either but
//    puts the pin on top of them."
//
// MEASURED, and the mechanism is NOT "re-homed from pixels on re-entry" —
// `foh_css_token_pos` already homes every rest slot on `cssChar[k]`, D21's own
// rule. It is a LEAK, and the falcon is the last thing it drags over:
//   1. one B press inside the roster band BOTH retrieves your token
//      (css.js:209-215) and arms the 30-frame back counter — upstream's
//      deliberate overlap, MENU-SPEC §2.11 — so "press B to go back" leaves
//      the CSS CARRYING;
//   2. nothing on the back-out path released it. Upstream's own grab is
//      module state cleared by exactly two arms (the A-drop, css.js:228-232;
//      the leave-band drop, css.js:341-347), and css.js:186-194's
//      `changeGamemode(1)` is neither. `changeGamemode` case 2 is
//      `drawCSSInit()` alone (main.js:571) and the re-entry adds only
//      `positionPlayersInCSS()` (menu.js:106), which moves SIM players and no
//      token. This port carried that verbatim;
//   3. so re-entry finds the token still glued to the hand, and the hover arm
//      re-selects LIVE from wherever the hand goes (css.js:222-226, the one
//      site that writes BOTH planes). D22's BACK wedge sits at x > 184, y < 26
//      — directly above roster cell 4 — so the next walk to BACK drags the
//      carried token across FALCON and commits it. Both planes then really do
//      read falcon. Always falcon, because cell 4 is the cell under the wedge.
// DEVIATION D35 releases every token on the back-out and re-homes it on the
// character its port chose. The SELECTION plane is never written here.
//
// WHAT THIS ASSERTS, and in which order:
//   [A] the owner's exact gesture — pick fox, B-hold out, come back, walk to
//       the BACK wedge, back out again — leaves BOTH planes reading fox at
//       every step, and the token resting on FOX's cell;
//   [B] the OTHER rest path (walk out of the band carrying, so the token
//       lands in the leave-band slot one cell right — quirk Q1, and the half
//       the owner described as "puts the pin on top of them") also comes back
//       showing the pick. Its precondition is asserted first, so the leg
//       cannot pass by never reproducing the state it exists to check.
//
// FOX (2) is the pick on purpose: it is neither foh_init's default (marth, 0),
// nor the port index (0), nor the character the bug produces (falcon, 4), nor
// the cell the leave-band quirk would shift it to (falco, 3). No assertion
// below can pass by accident on any of those.
//
// The state machine is the REAL one — every gesture is fed to foh_tick() as
// button levels, the cursor is walked by FEEDBACK (hold the direction until
// the machine reports the hand arrived), never by a copied frame count, and no
// character field is ever hand-poked. The foh_cssrest_witness.c discipline,
// carried.
//
// Prints one line per assertion and `CSS BACK SELECT OK` on success, exit 0.
// Any failure prints `CSS BACK SELECT FAIL: ...` and exits 1. Its two
// orthogonal negative tests live in port/foh/check-css-backsel.sh.
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "foh.h"

// gfx_fatal is host-provided (raster.h), the foh_cssrest_witness.c pattern.
void gfx_fatal(const char *what) {
  fprintf(stderr, "CSS BACK SELECT FAIL: gfx_fatal: %s\n", what);
  exit(3);
}

static int g_fails;

static void ok(const char *what) { printf("  ok  %s\n", what); }

static void bad(const char *what) {
  printf("CSS BACK SELECT FAIL: %s\n", what);
  g_fails++;
}

static void want(int cond, const char *what) {
  if (cond) ok(what);
  else bad(what);
}

static const char *const kName[5] = {"marth", "puff", "fox", "falco", "falcon"};

#define PICK 2 /* fox */

// The roster cell a token's DRAWN centre falls in, or -1 for none — the
// player-visible question the whole witness is about. Same cell extents foh.c
// hit-tests and foh_render.c draws (foh.h is the single source, D4).
static int token_cell(const FohState *s, int k) {
  double x, y;
  foh_css_token_pos(s, k, &x, &y);
  for (int c = 0; c < 5; c++) {
    const double x0 = (double)foh_css_cell_x(c);
    if (x > x0 && x < x0 + (double)FOH_CSS_CELL_W) return c;
  }
  return -1;
}

// --- gestures ---------------------------------------------------------------
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

// Hold one field for n consecutive frames, then release. The back counter is
// LEVEL driven and fires on its 30th frame (css.js:188), so a hold IS the
// gesture; 40 is comfortably past it and the `== 30` equality means the extra
// frames cannot fire it twice.
static void hold(FohState *s, size_t off, int n) {
  PlatformInput in;
  memset(&in, 0, sizeof in);
  *(bool *)((char *)&in + off) = true;
  for (int i = 0; i < n; i++) foh_tick(s, &in);
  memset(&in, 0, sizeof in);
  foh_tick(s, &in);
}
#define HOLD(s, field, n) hold((s), offsetof(PlatformInput, field), (n))

// Walk the hand until `pred` holds, holding `off` the whole way. Returns false
// if the guard runs out — a walk that never arrives must FAIL the witness, not
// silently continue from wherever it stopped.
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

// --- predicates -------------------------------------------------------------
static bool at_top(const FohState *s) { return s->cssHandY <= 0.0; }
static bool at_right(const FohState *s) {
  return s->cssHandX >= (double)(RAST_W - 1);
}
// Inside the roster band AND inside the cell row: the only place the hover arm
// selects a character (foh.c's band branch).
static bool in_cells(const FohState *s) {
  return s->cssHandY > (double)FOH_CSS_CELL_Y &&
         s->cssHandY < (double)(FOH_CSS_CELL_Y + FOH_CSS_CELL_H) &&
         s->cssHandY < (double)FOH_CSS_BAND_BOT &&
         s->cssHandY > (double)FOH_CSS_BAND_TOP;
}
static bool above_band(const FohState *s) {
  return s->cssHandY < (double)FOH_CSS_BAND_TOP;
}
static bool p1_on_pick(const FohState *s) { return s->cssChar[0] == PICK; }
// The BACK wedge, stated the way foh.c hit-tests it (D22).
static bool on_wedge(const FohState *s) {
  return s->cssHandY < (double)FOH_CSS_BAND_TOP &&
         s->cssHandX > (double)FOH_CSS_BACK_X0;
}

// --- the shared shapes ------------------------------------------------------
// Boot a fresh machine into the CSS. STARTUP -> TITLE is the timer transition;
// TITLE -> MENU_TOP is START; MENU_TOP row 0 is `VS. Melee`, whose A runs
// menu.js:105 (owner ruling C5).
static void boot_to_css(FohState *s, const char *leg) {
  char buf[120];
  foh_init(s);
  PlatformInput neutral;
  memset(&neutral, 0, sizeof neutral);
  int guard = 0;
  while (s->screen != FOH_TITLE && guard++ < 100000) foh_tick(s, &neutral);
  snprintf(buf, sizeof buf, "%s: machine reaches the title screen", leg);
  want(s->screen == FOH_TITLE, buf);
  PRESS(s, start);
  PRESS(s, a);
  snprintf(buf, sizeof buf, "%s: menu top row 0 (VS. Melee) opens the CSS",
           leg);
  want(s->screen == FOH_CSS, buf);
  snprintf(buf, sizeof buf, "%s: the CSS opens on foh_init's marth defaults",
           leg);
  want(s->p1Char == 0 && s->cssChar[0] == 0, buf);
}

// P1 picks fox through the REAL gestures — B retrieves the token, hovering
// right selects LIVE, A drops it in the hovered cell. Nothing is poked.
static void pick_fox(FohState *s, const char *leg) {
  char buf[160];
  want(WALK(s, up, at_top), "hand walks up to the y clamp");
  want(WALK(s, down, in_cells), "hand walks down into the roster cell row");
  PRESS(s, b);
  snprintf(buf, sizeof buf, "%s: B in the band retrieves P1's token", leg);
  want(s->cssCarry == 0, buf);
  want(WALK(s, right, p1_on_pick),
       "carrying right over the cells selects fox LIVE (css.js:222-226)");
  PRESS(s, a);
  snprintf(buf, sizeof buf, "%s: A drops the token into the hovered cell", leg);
  want(s->cssCarry == -1 && s->cssTokenRest[0] == 0, buf);
}

// The whole subject, asserted the way the owner would read the screen: the
// planes AND the pixel the token sits on.
static void assert_pick_survives(const FohState *s, const char *when) {
  char buf[200];
  snprintf(buf, sizeof buf,
           "%s: both planes still read the pick (p1Char=%s cssChar=%s, want "
           "fox/fox)",
           when, kName[s->p1Char], kName[s->cssChar[0]]);
  want(s->p1Char == PICK && s->cssChar[0] == PICK, buf);
  snprintf(buf, sizeof buf,
           "%s: P1's token rests on the cell of the character P1 picked "
           "(fox/2, got cell %d)",
           when, token_cell(s, 0));
  want(token_cell(s, 0) == PICK, buf);
}

int main(void) {
  FohState s;

  // === [A] the owner's exact gesture ========================================
  // Pick fox, press-and-hold B to go back (which is ALSO the grab), come back
  // in, then walk to the BACK wedge and out again.
  printf("=== [A] pick fox, B-hold out, re-enter, walk to BACK, out again\n");
  boot_to_css(&s, "A");
  pick_fox(&s, "A");
  assert_pick_survives(&s, "A: before backing out");

  HOLD(&s, b, 40); // the B-hold back — and the grab, in the same press
  want(s.screen == FOH_MENU_TOP,
       "A: 30 frames of B in the band backs out to menu-top");
  want(s.cssCarry == -1,
       "A: the back-out RELEASED the token (D35 — this is the root cause: "
       "upstream's back arm leaves it grabbed, css.js:186-194)");

  PRESS(&s, a); // back into the CSS
  want(s.screen == FOH_CSS, "A: VS. Melee re-opens the CSS");
  assert_pick_survives(&s, "A: on re-entry");

  // The walk that USED to commit falcon: with the token released, crossing
  // the roster on the way to the wedge cannot write either plane.
  want(WALK(&s, right, at_right), "A: hand walks right to the x clamp");
  assert_pick_survives(&s, "A: after walking right across the whole roster");
  want(WALK(&s, up, above_band), "A: hand walks up out of the band");
  want(on_wedge(&s), "A: the clamped corner IS the BACK wedge (D22)");
  assert_pick_survives(&s, "A: parked on the BACK wedge");

  HOLD(&s, a, 40); // D22: holding A on the wedge backs out too
  want(s.screen == FOH_MENU_TOP, "A: 30 frames of A on the wedge backs out");
  PRESS(&s, a);
  want(s.screen == FOH_CSS, "A: the CSS opens a second time");
  assert_pick_survives(&s, "A: on the second re-entry");

  // === [B] the leave-band rest path ========================================
  // The other way a token comes to rest: carried OUT of the band, which lands
  // it in quirk Q1's other formula — one cell RIGHT of the character it
  // selected. That is the owner's "it doesn't choose that character either but
  // puts the pin on top of them". A back-out must still bring it home.
  printf("=== [B] carry the token out of the band, then back out\n");
  boot_to_css(&s, "B");
  pick_fox(&s, "B");
  PRESS(&s, b); // retrieve it again
  want(s.cssCarry == 0, "B: B retrieves P1's token a second time");
  want(WALK(&s, up, above_band),
       "B: carrying the token up out of the band drops it there");
  want(s.cssCarry == -1 && s.cssTokenRest[0] == 1,
       "B: the leave-band drop really used quirk Q1's other rest slot");
  {
    // A49/DEVIATION D46 CHANGED WHAT THIS ARM OWES, and the change is
    // deliberate rather than absorbed. Until A49 this leg asserted the
    // DEFECT as its precondition: the leave-band slot drew the token on
    // falco (3) while fox (2) was the pick, one whole cell right, and the
    // point of the leg was that a BACK-OUT still brought it home. The owner
    // then filed the drop itself — *"whenever the pin is let go of (going
    // off) it should go back to the character you had selected"* — so the
    // token no longer leaves home in the first place.
    //
    // The leg is KEPT, not deleted, and it still bites: the LEAVE-BAND PATH
    // is still the one being driven (the rest-slot assertion above proves
    // that), and what it now owes is the same claim the rest of this file
    // makes — a token at rest is drawn on its port's SELECTION. Deleting the
    // leg because its old expectation flipped would have quietly retired a
    // path nothing else here covers.
    char buf[200];
    snprintf(buf, sizeof buf,
             "B: the leave-band drop draws the token on the character P1 "
             "picked (cell %d, want fox/2 — D46; it used to land on falco/3)",
             token_cell(&s, 0));
    want(token_cell(&s, 0) == PICK, buf);
    snprintf(buf, sizeof buf,
             "B: while the SELECTION never moved (p1Char=%s cssChar=%s)",
             kName[s.p1Char], kName[s.cssChar[0]]);
    want(s.p1Char == PICK && s.cssChar[0] == PICK, buf);
  }

  want(WALK(&s, right, at_right), "B: hand walks right to the x clamp");
  want(on_wedge(&s), "B: the hand is on the BACK wedge");
  HOLD(&s, a, 40);
  want(s.screen == FOH_MENU_TOP, "B: holding A on the wedge backs out");
  PRESS(&s, a);
  want(s.screen == FOH_CSS, "B: the CSS re-opens");
  assert_pick_survives(&s, "B: on re-entry");

  if (g_fails) {
    printf("CSS BACK SELECT FAIL: %d assertion(s) failed\n", g_fails);
    return 1;
  }
  printf("CSS BACK SELECT OK\n");
  return 0;
}
