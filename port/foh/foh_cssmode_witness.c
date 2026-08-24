// port/foh/foh_cssmode_witness.c — the CSS MODE RIBBON witness
// (punch-list A27, owner playthrough #3 round 2).
//
// THE DEFECT THIS GUARDS. The owner reported: "there's no way to change
// between stock mode and 'endless ko fest'. If you click the 'VS Melee' in
// the CSS it should change modes." The ribbon was DRAWN (foh_render.c's
// css_header) but no hit test reached it — the last registered D4 exception
// in foh.h, and it was registered rather than fixed because versusMode is
// SIM-VISIBLE state that the sim pinned to 0 until A37 made it real.
//
// WHAT THIS ASSERTS, in the owner's own terms:
//   [1] the hand can reach the ribbon, and pressing A there CHANGES THE
//       LABEL — measured as pixels that changed against the same frame with
//       the hand parked in the same place, inside the ribbon's own rect and
//       nowhere else;
//   [2] pressing A again changes it BACK, and the frame is byte-identical to
//       the one before the first press (upstream's `1 - versusMode` is a
//       binary toggle, css.js:393 — not a cycle, and not a one-way latch);
//   [3] the mode is PAGE state, not screen state: it survives leaving the
//       CSS and coming back (upstream's setVersusMode writes a module let
//       that startGame never resets, main.js:140/237-239);
//   [4] pressing A anywhere OTHER than the ribbon never changes the mode,
//       and in particular not on the BACK wedge, whose A is the D22 hold —
//       the two header widgets are adjacent and must stay disjoint;
//   [5] the COLD frame — versusMode == 0, the state every judged CSS shot is
//       taken in — is written to disk so port/foh/check-css-mode.sh can cmp
//       it against the same frame from a COPY of foh_render.c whose ribbon
//       label is unconditional. That is the only honest way to prove
//       "unchanged" from inside the tree that changed it;
//   [6] the ENDLESS frame is written too, so [5]'s comparison is proven to
//       have teeth rather than to be comparing two identical builds.
//
// The state machine here is the REAL one: every gesture feeds button levels
// to foh_tick() and the cursor is walked by FEEDBACK, never by frame counts.
// The foh_cssback_witness.c discipline, carried whole — this is its sibling
// widget in the same header.
//
// Usage: foh_cssmode_witness <outdir>. Prints one line per assertion and
// `CSS MODE OK` on success, exit 0. Any failure prints `CSS MODE FAIL: ...`
// and exits 1. Its negative tests live in port/foh/check-css-mode.sh.
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../gfx/raster.h"
#include "foh.h"

// gfx_fatal is host-provided (raster.h), the foh_cssback_witness.c pattern.
void gfx_fatal(const char *what) {
  fprintf(stderr, "CSS MODE FAIL: gfx_fatal: %s\n", what);
  exit(3);
}

static int g_fails;

static void ok(const char *what) { printf("  ok  %s\n", what); }

static void bad(const char *what) {
  printf("CSS MODE FAIL: %s\n", what);
  g_fails++;
}

static void want(int cond, const char *what) {
  if (cond) ok(what);
  else bad(what);
}

// --- rendering ---------------------------------------------------------------
// One CSS frame as a judged shot would see it: a pure function of MACHINE
// state with the LOOK plane pinned exactly as every shot path pins it.
static void render_shot(const FohState *s, Raster *rz) {
  FohState look = *s;
  foh_look_canonical(&look);
  memset(rz, 0, sizeof *rz);
  foh_render(&look, rz);
}

typedef struct {
  int n;
  int x0, y0, x1, y1; // half-open; x0 > x1 when n == 0
} Diff;

static Diff diff_vs(const Raster *a, const Raster *b) {
  Diff d = {0, RAST_W, RAST_H, 0, 0};
  for (int y = 0; y < RAST_H; y++) {
    for (int x = 0; x < RAST_W; x++) {
      if (a->fb[y * RAST_W + x] == b->fb[y * RAST_W + x]) continue;
      d.n++;
      if (x < d.x0) d.x0 = x;
      if (y < d.y0) d.y0 = y;
      if (x + 1 > d.x1) d.x1 = x + 1;
      if (y + 1 > d.y1) d.y1 = y + 1;
    }
  }
  return d;
}

static bool write_fb(const char *path, const Raster *rz) {
  FILE *f = fopen(path, "wb");
  if (!f) return false;
  const size_t n = sizeof rz->fb;
  const bool got = fwrite(rz->fb, 1, n, f) == n;
  return fclose(f) == 0 && got;
}

// The ribbon's own rect, from foh.h — the SAME constants css_header draws
// the plate with and step_css hit-tests it with (D4). Nothing the label
// draws may fall outside the plate.
static bool in_mode_rect(const Diff *d) {
  return d->x0 >= FOH_CSS_MODE_X0 && d->x1 <= FOH_CSS_MODE_X1 + 1 &&
         d->y0 >= FOH_CSS_MODE_Y0 && d->y1 <= FOH_CSS_MODE_Y1 + 1;
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
#define HOLD(s, field) tick_with((s), offsetof(PlatformInput, field))

static bool walk_until(FohState *s, size_t off,
                       bool (*pred)(const FohState *)) {
  for (int i = 0; i < 4000 && !pred(s); i++) tick_with(s, off);
  const bool got = pred(s);
  neutral(s);
  return got;
}

#define WALK(s, field, pred) \
  walk_until((s), offsetof(PlatformInput, field), (pred))

// --- predicates --------------------------------------------------------------
static bool at_top(const FohState *s) { return s->cssHandY <= 0.0; }
static bool at_left(const FohState *s) { return s->cssHandX <= 0.0; }
static bool at_right(const FohState *s) { return s->cssHandX >= (double)RAST_W; }

// The ribbon, stated the way foh.c hit-tests it.
static bool on_ribbon(const FohState *s) {
  return s->cssHandY > (double)FOH_CSS_MODE_Y0 &&
         s->cssHandY < (double)FOH_CSS_MODE_Y1 &&
         s->cssHandX > (double)FOH_CSS_MODE_X0 &&
         s->cssHandX < (double)FOH_CSS_MODE_X1;
}
// The ribbon's ROW alone — the hand has to descend into the plate's band
// before walking across it, and the descent has to stop somewhere the walk
// can see. (The d-pad step is 3.84 px/frame vertically, foh.h, so the band
// is four steps deep: this is not a knife edge.)
static bool at_ribbon_row(const FohState *s) {
  return s->cssHandY > (double)FOH_CSS_MODE_Y0 &&
         s->cssHandY < (double)FOH_CSS_MODE_Y1;
}
static bool on_wedge(const FohState *s) {
  return s->cssHandY < (double)FOH_CSS_BAND_TOP &&
         s->cssHandX > (double)FOH_CSS_BACK_X0;
}
static bool in_cells(const FohState *s) {
  return s->cssHandY > (double)FOH_CSS_CELL_Y &&
         s->cssHandY < (double)(FOH_CSS_CELL_Y + FOH_CSS_CELL_H) &&
         s->cssHandY < (double)FOH_CSS_BAND_BOT &&
         s->cssHandY > (double)FOH_CSS_BAND_TOP;
}

// Boot a fresh machine to the CSS (foh_cssback_witness.c's ladder).
static void boot_to_css(FohState *s) {
  foh_init(s);
  int guard = 0;
  while (s->screen != FOH_TITLE && guard++ < 100000) neutral(s);
  want(s->screen == FOH_TITLE, "machine reaches the title screen");
  PRESS(s, start);
  want(s->screen == FOH_MENU_TOP, "START leaves the title for the menu top");
  PRESS(s, a);
  want(s->screen == FOH_CSS, "menu top row 0 (VS. Melee) opens the CSS");
}

// Park the hand ON the ribbon. The plate sits between the VS badge and the
// BACK wedge, so it cannot be reached by clamping into a corner the way the
// wedge can: walk to the top edge, then to the LEFT clamp, then right until
// the hit test itself says we have arrived. Anchoring on the predicate is
// the point — the walk is over when the machine agrees, not after N frames.
static void park_on_ribbon(FohState *s) {
  want(WALK(s, up, at_top), "hand walks up to the y clamp");
  want(WALK(s, left, at_left), "hand walks left to the x clamp");
  // The y clamp is 0 and the plate opens at FOH_CSS_MODE_Y0 (4), so the hand
  // must come DOWN into the plate's band before walking across it.
  want(WALK(s, down, at_ribbon_row), "hand descends into the ribbon's row");
  want(WALK(s, right, on_ribbon), "hand walks right onto the drawn ribbon");
  want(on_ribbon(s), "the hand is inside the ribbon's drawn extent (D4)");
  want(!on_wedge(s), "and the ribbon is NOT the BACK wedge (disjoint widgets)");
}

int main(int argc, char **argv) {
  if (argc != 2) {
    fprintf(stderr, "usage: %s <outdir>\n", argv[0]);
    return 2;
  }
  const char *outdir = argv[1];
  char path[512];
  char buf[240];
  FohState s;

  // --- [5] the COLD frame ----------------------------------------------------
  boot_to_css(&s);
  Raster cold;
  render_shot(&s, &cold);
  want(s.versusMode == 0,
       "a freshly opened CSS is in STOCK mode (upstream's `versusMode = 0`)");
  snprintf(path, sizeof path, "%s/cold.fb", outdir);
  want(write_fb(path, &cold), "the cold CSS frame is written for comparison");

  // --- [1][2] the ribbon toggles, and toggles back --------------------------
  park_on_ribbon(&s);
  {
    Raster parked, endless, back;
    render_shot(&s, &parked);
    want(s.versusMode == 0, "parking on the ribbon does not itself toggle it");

    PRESS(&s, a);
    want(s.versusMode == 1,
         "A on the ribbon arms the ENDLESS mode (setVersusMode(1 - 0))");
    render_shot(&s, &endless);
    const Diff d = diff_vs(&parked, &endless);
    snprintf(buf, sizeof buf,
             "the LABEL changes when the mode does (%d px changed in "
             "x[%d,%d) y[%d,%d))", d.n, d.x0, d.x1, d.y0, d.y1);
    want(d.n > 0, buf);
    want(in_mode_rect(&d),
         "every changed pixel lies inside the ribbon's own plate — the label "
         "disturbs nothing else on the screen");

    PRESS(&s, a);
    want(s.versusMode == 0,
         "a second A returns to STOCK (a binary toggle, not a one-way latch)");
    render_shot(&s, &back);
    want(diff_vs(&parked, &back).n == 0,
         "and the screen is byte-identical to before the first press");

    // Written for check-css-mode.sh: the ENDLESS frame is what proves the
    // cold comparison in [5] has teeth.
    PRESS(&s, a);
    want(s.versusMode == 1, "third press arms ENDLESS again (for the shot)");
    Raster endless2;
    render_shot(&s, &endless2);
    snprintf(path, sizeof path, "%s/endless.fb", outdir);
    want(write_fb(path, &endless2), "the ENDLESS frame is written too");

    // --- [3] it is PAGE state --------------------------------------------
    // Hold B for the 30 frames the CSS back counter needs (css.js:186-194),
    // then re-enter from the menu. Upstream's versusMode is a module let
    // that nothing on this path resets.
    for (int i = 0; i < 30; i++) HOLD(&s, b);
    want(s.screen == FOH_MENU_TOP, "the B hold backs out of the CSS");
    want(s.versusMode == 1, "leaving the CSS does not clear the mode");
    neutral(&s);
    PRESS(&s, a);
    want(s.screen == FOH_CSS, "and the CSS re-opens");
    want(s.versusMode == 1,
         "the ENDLESS mode SURVIVES the round trip — it is page state "
         "(main.js:140), not screen state");
  }

  // --- [4] A off the ribbon is inert -----------------------------------------
  boot_to_css(&s);
  {
    // (a) the roster band, where A is a grab.
    want(WALK(&s, up, at_top), "hand walks up to the y clamp");
    want(WALK(&s, left, at_left), "hand walks left to the x clamp");
    want(WALK(&s, down, in_cells), "hand walks down into the roster cell row");
    want(!on_ribbon(&s), "the roster cell row is NOT the ribbon");
    for (int i = 0; i < 8; i++) PRESS(&s, a);
    want(s.versusMode == 0,
         "eight A presses in the roster band never change the mode");

    // (b) the BACK wedge, the ribbon's NEIGHBOUR in the same header. Its A
    // is the D22 back-out hold, so this also proves the two rects do not
    // overlap — the one place a mistake here would be invisible on screen.
    want(WALK(&s, up, at_top), "hand walks back up to the y clamp");
    want(WALK(&s, right, at_right), "hand walks right to the x clamp");
    want(on_wedge(&s), "the clamped corner IS the BACK wedge");
    want(!on_ribbon(&s), "and the BACK wedge is NOT the ribbon");
    for (int i = 0; i < 8; i++) PRESS(&s, a);
    want(s.versusMode == 0,
         "eight A presses on the BACK wedge never change the mode either");
  }

  if (g_fails) {
    printf("CSS MODE: %d assertion(s) failed\n", g_fails);
    return 1;
  }
  printf("CSS MODE OK\n");
  return 0;
}
