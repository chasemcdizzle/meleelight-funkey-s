// port/foh/foh_cssback_witness.c — the CSS BACK WEDGE witness
// (punch-list A23, owner playthrough #3).
//
// THE DEFECT THIS GUARDS. The owner reported: "clicking the back button does
// nothing in the top right of the vs. screen. I want it to behave like how
// melee does — holding the cursor OR holding the back button starts
// progressing a little red bar that fills below the back button, and when it
// fills it actually backs out." The wedge was DRAWN (foh_render.c's
// css_header) but no hit test reached it — a registered D4 exception in
// foh.h. There was also no bar: upstream draws one (css.js:735-746) and this
// port had not carried it.
//
// WHAT THIS ASSERTS, in the owner's own terms rather than in ours. Each
// assertion below is something the owner could have SEEN on the device; none
// of them reads an internal we merely suspect underlies the symptom.
//   [1] the hand can reach the wedge, and holding A there fills a red bar
//       that GROWS — measured as pixels that changed against the cold frame,
//       inside the bar's own rect and nowhere else;
//   [2] a 29-frame hold does NOT back out (the ticket's own tooth: the
//       counter's `== 30` equality is upstream's, css.js:188, and a `>=`
//       would pass [3] while breaking this);
//   [3] the 30th frame DOES back out, to menu-top;
//   [4] holding B does the same thing and moves the SAME counter — one
//       counter, two input paths, so the bar reports both;
//   [5] holding A anywhere OTHER than the wedge never backs out, however
//       long it is held (the region is a region, not "A anywhere");
//   [6] the COLD frame — bHold == 0, the state every judged CSS shot is
//       taken in — has a bar of exactly zero pixels. This driver writes that
//       frame to disk; port/foh/check-css-back.sh cmp's it against the same
//       frame rendered by a COPY of foh_render.c with the bar block removed,
//       which is the only honest way to prove "unchanged" from inside the
//       tree that changed it.
//
// The state machine here is the REAL one. Every gesture is driven by feeding
// button levels to foh_tick(), and the cursor is walked by FEEDBACK (hold the
// direction until the machine reports the hand is where we want it), never by
// a frame count — the foh_cssrest_witness.c discipline, carried.
//
// Usage: foh_cssback_witness <outdir>. Prints one line per assertion and
// `CSS BACK OK` on success, exit 0. Any failure prints `CSS BACK FAIL: ...`
// and exits 1. Its negative tests live in port/foh/check-css-back.sh.
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../gfx/raster.h"
#include "foh.h"

// gfx_fatal is host-provided (raster.h), the foh_cssrest_witness.c pattern.
void gfx_fatal(const char *what) {
  fprintf(stderr, "CSS BACK FAIL: gfx_fatal: %s\n", what);
  exit(3);
}

static int g_fails;

static void ok(const char *what) { printf("  ok  %s\n", what); }

static void bad(const char *what) {
  printf("CSS BACK FAIL: %s\n", what);
  g_fails++;
}

static void want(int cond, const char *what) {
  if (cond) ok(what);
  else bad(what);
}

// --- rendering ---------------------------------------------------------------
// One CSS frame as a judged shot would see it: a pure function of MACHINE
// state, with the LOOK plane pinned exactly as every shot path pins it. That
// pinning is what makes "the bar changed these pixels and no others" a claim
// about the bar rather than about whichever tick we happened to stop on.
static void render_shot(const FohState *s, Raster *rz) {
  FohState look = *s;
  foh_look_canonical(&look);
  memset(rz, 0, sizeof *rz);
  foh_render(&look, rz);
}

// Pixels that differ from `cold`, and the bounding box they fall in. The
// owner's "a little red bar fills below the back button" is exactly this
// measurement: how much of the screen changed, and where.
typedef struct {
  int n;
  int x0, y0, x1, y1; // half-open; x0 > x1 when n == 0
} Diff;

static Diff diff_vs(const Raster *cold, const Raster *hot) {
  Diff d = {0, RAST_W, RAST_H, 0, 0};
  for (int y = 0; y < RAST_H; y++) {
    for (int x = 0; x < RAST_W; x++) {
      if (cold->fb[y * RAST_W + x] == hot->fb[y * RAST_W + x]) continue;
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

// Walk the hand until `pred` holds, holding `off` the whole way.
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
static bool at_right(const FohState *s) { return s->cssHandX >= (double)RAST_W; }
static bool at_left(const FohState *s) { return s->cssHandX <= 0.0; }
// The wedge, stated the way foh.c hit-tests it — from the SAME foh.h
// constants foh_render.c draws it with (D4).
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

// The bar's own rect, from foh.h: rows [BAR_TOP, 26) and columns
// [BAR_LEAN, 240). Nothing the bar draws may fall outside it.
static bool in_bar_rect(const Diff *d) {
  return d->x0 >= (int)FOH_CSS_BACK_BAR_LEAN && d->x1 <= RAST_W &&
         d->y0 >= (int)FOH_CSS_BACK_BAR_TOP && d->y1 <= 26;
}

// Boot a fresh machine to the CSS. STARTUP -> TITLE is the timer transition;
// TITLE -> MENU_TOP is START; MENU_TOP row 0 is `VS. Melee`, whose A runs
// menu.js:105 (owner ruling C5).
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

// Park the hand on the BACK wedge, anchoring on the clamps rather than on
// frame counts (the f01 flow's own discipline).
static void park_on_wedge(FohState *s) {
  want(WALK(s, up, at_top), "hand walks up to the y clamp");
  want(WALK(s, right, at_right), "hand walks right to the x clamp");
  want(on_wedge(s), "the clamped corner IS the drawn BACK wedge (D4)");
  want(s->bHold == 0, "walking to the wedge does not itself arm the counter");
}

int main(int argc, char **argv) {
  if (argc != 2) {
    fprintf(stderr, "usage: %s <outdir>\n", argv[0]);
    return 2;
  }
  const char *outdir = argv[1];
  char path[512];
  FohState s;

  // --- [6] the COLD frame ----------------------------------------------------
  // Rendered FIRST, from a CSS that has never held anything, and written to
  // disk for check-css-back.sh's byte comparison against the bar-less build.
  boot_to_css(&s);
  Raster cold;
  render_shot(&s, &cold);
  want(s.bHold == 0, "a freshly opened CSS has bHold == 0 (the shot state)");
  snprintf(path, sizeof path, "%s/cold.fb", outdir);
  want(write_fb(path, &cold), "the cold CSS frame is written for comparison");

  // --- [1][2][3] the cursor path --------------------------------------------
  park_on_wedge(&s);
  {
    // The baseline is the wedge frame with the counter still at 0, NOT the
    // cold frame: the hand has MOVED to get here, and the hand is drawn. A
    // diff against `cold` would be mostly cursor. Against `parked`, the only
    // thing that can differ is what the hold itself puts on screen — which
    // makes "the bar disturbs nothing else" a real claim instead of a
    // tolerance.
    Raster parked, at10, at20, at29;
    render_shot(&s, &parked);
    // Frames 1..29 of the hold. The bar is sampled DURING the real gesture —
    // no field is poked to produce a bar to look at.
    for (int i = 1; i <= 29; i++) {
      HOLD(&s, a);
      if (s.bHold == 10) render_shot(&s, &at10);
      if (s.bHold == 20) render_shot(&s, &at20);
    }
    want(s.bHold == 29, "29 frames of A on the wedge counted 29 (one path)");
    want(s.screen == FOH_CSS,
         "TOOTH: a 29-frame hold does NOT back out (css.js:188 equality)");
    render_shot(&s, &at29);

    const Diff d10 = diff_vs(&parked, &at10);
    const Diff d20 = diff_vs(&parked, &at20);
    const Diff d29 = diff_vs(&parked, &at29);
    char buf[200];
    snprintf(buf, sizeof buf,
             "holding the cursor on the wedge FILLS a bar (%d px changed at "
             "frame 10)", d10.n);
    want(d10.n > 0, buf);
    snprintf(buf, sizeof buf,
             "the bar GROWS with the hold (%d px at 10 -> %d at 20 -> %d at "
             "29)", d10.n, d20.n, d29.n);
    want(d10.n < d20.n && d20.n < d29.n, buf);
    snprintf(buf, sizeof buf,
             "every changed pixel lies in the bar's rect x[%d,%d) y[%d,%d) — "
             "the bar disturbs nothing else on the screen",
             d29.x0, d29.x1, d29.y0, d29.y1);
    want(in_bar_rect(&d29), buf);
    snprintf(buf, sizeof buf, "%s/hold29.fb", outdir);
    want(write_fb(buf, &at29), "the 29-frame frame is written for comparison");

    // The 30th frame is the owner's "it actually backs out".
    HOLD(&s, a);
    want(s.bHold == 30, "the 30th held frame reaches the counter's edge");
    want(s.screen == FOH_MENU_TOP,
         "holding the cursor on the wedge 30 frames BACKS OUT to the menu");
  }

  // --- [4] the B path moves the SAME counter ---------------------------------
  // A fresh machine, so the counter starts from 0 (upstream never resets
  // bHold on a gamemode change either — css.js:186-194 is its only reset).
  boot_to_css(&s);
  {
    Raster at15;
    for (int i = 1; i <= 29; i++) {
      HOLD(&s, b);
      if (s.bHold == 15) render_shot(&s, &at15);
    }
    want(s.bHold == 29 && s.screen == FOH_CSS,
         "TOOTH: 29 frames of B does not back out either");
    const Diff d15 = diff_vs(&cold, &at15);
    char buf[200];
    snprintf(buf, sizeof buf,
             "the B hold fills the SAME bar (%d px changed at frame 15, hand "
             "nowhere near the wedge)", d15.n);
    want(d15.n > 0 && in_bar_rect(&d15), buf);
    HOLD(&s, b);
    want(s.screen == FOH_MENU_TOP, "30 frames of B still backs out (upstream)");
  }

  // --- [5] A off the wedge is inert ------------------------------------------
  // The region has to BE a region. Parked in the roster band — where A is a
  // grab/drop, not a back — a hold twice as long as the counter needs must
  // leave the CSS exactly where it was.
  boot_to_css(&s);
  {
    want(WALK(&s, up, at_top), "hand walks up to the y clamp");
    want(WALK(&s, left, at_left), "hand walks left to the x clamp");
    want(WALK(&s, down, in_cells), "hand walks down into the roster cell row");
    want(!on_wedge(&s), "the roster cell row is NOT the wedge");
    Raster parked, off;
    render_shot(&s, &parked);
    for (int i = 0; i < 60; i++) HOLD(&s, a);
    want(s.screen == FOH_CSS,
         "TOOTH: 60 frames of A in the roster band never backs out");
    want(s.bHold == 0, "and it never armed the counter at all");
    render_shot(&s, &off);
    const Diff d = diff_vs(&parked, &off);
    char buf[200];
    snprintf(buf, sizeof buf,
             "60 frames of A off the wedge change NOTHING on screen — no bar "
             "anywhere (%d px differ)", d.n);
    want(d.n == 0, buf);
  }

  if (g_fails) {
    printf("CSS BACK: %d assertion(s) failed\n", g_fails);
    return 1;
  }
  printf("CSS BACK OK\n");
  return 0;
}
