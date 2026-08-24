// port/foh/foh_hand.h — the FREE HAND CURSOR, extracted (punch-list A25(c),
// owner-requested; MENU-SPEC DEVIATION D29).
//
// WHY THIS FILE EXISTS. The CSS has carried a free 2D hand cursor since the
// FOH's first pass — position in DOUBLES, integrated from the d-pad every
// frame (css.js:64 `handPos`, css.js:195-206), hit-testing widgets as
// point-in-rect. The owner asked for the SAME cursor on target-select, and
// asked for it DRY: *"here I want to utilize our cursor logic like we have at
// the character select screen where it's a free moving cursor (with the hand
// pointing). want to keep things DRY here"*. So the motion and the hit
// predicate move HERE, once, and both screens call them. Nothing is copied.
//
// The extraction is a PURE REFACTOR on the CSS side and that is a hard
// constraint, not an aspiration: same doubles, same integration order, same
// clamp, same strict comparisons. `port/foh/check-hand.sh` leg [2] proves it
// DIFFERENTIALLY against the pre-refactor foh.c — see that script.
//
// WHAT IS DELIBERATELY *NOT* HERE:
//   * the hand TYPE (handPoint / handOpen / handGrab). It is a CSS concept —
//     the sprite reports whether that screen's grab gesture is armed
//     (css.js:1135-1143) — and target-select has no grab. `FohState.cssHandType`
//     stays where it is; hoisting it would put a field here that one of the
//     two callers can never write, which is the opposite of DRY.
//   * the rect TABLES. Hit-testing is caller-supplied by design: the shared
//     unit answers "which of these N rects is the hand in", over a table the
//     SCREEN owns, because the rects must come from the same constants the
//     RENDERER draws (DEVIATION D4 — a hit region where nothing is drawn is
//     forbidden). `foh_css_cells()` / `foh_tss_slots()` in foh.h are those
//     tables, and foh.c and foh_render.c share each one.
//
// HEADER-ONLY, deliberately. Every check in port/foh/ and every device rig
// carries its own explicit TU list, and a new .c file would mean editing all
// of them — including the device checks, which cannot be run from here. The
// two bodies are a dozen lines between them, `static inline` is already this
// project's shape for exactly that (foh.h's own foh_css_cell_x, port/sim's
// ml_js.h / ml_rng.h), and the whole point of the extraction is that ONE
// definition exists, which a header gives just as well as a TU.
#ifndef FOH_HAND_H
#define FOH_HAND_H

// DEVIATION D3 — cursor speed as a fraction of the screen, plus the one
// calibration knob in this whole spec. Upstream moves the CSS hand 12
// px/frame on a 1200x750 canvas (css.js:195-196) = 1.00% of width and 1.60%
// of height per frame; at 240x240 that is 2.40 / 3.84 px/frame, which keeps
// upstream's feel exactly (full-width traversal stays 100 frames, full-height
// 62.5). Hardware feel cannot be judged from source, so the owner tunes ONE
// number here rather than us guessing a curve.
//
// It lives beside the cursor it scales (A25c) rather than in foh.h, because
// two screens now read it and one of them is not the CSS.
#define FOH_CURSOR_SPEED 1.0
#define FOH_CURSOR_VX (2.40 * FOH_CURSOR_SPEED)
#define FOH_CURSOR_VY (3.84 * FOH_CURSOR_SPEED)

// A widget's drawn extent, in raster pixels: the half-open box the renderer
// fills, [x, x+w) x [y, y+h). Ints because every FOH layout constant is one.
typedef struct {
  int x, y, w, h;
} FohHandRect;

// Integrate one frame of d-pad into (*x, *y) and clamp to [0,w] x [0,h].
//
// This is css.js:195-206's body, unchanged, with the port's two registered
// deviations already in it: DEVIATION D1 (a d-pad supplies full deflection
// only, so the stick axes are -1 / 0 / +1) and DEVIATION D3 (upstream's 12
// px/frame rescaled by FOH_CURSOR_VX / FOH_CURSOR_VY, the port's one
// calibration knob). Position stays in DOUBLES — rounding to a pixel happens
// only at draw time — and the clamp is to the LOGICAL canvas exactly as
// upstream clamps to its 1200x750 (its canvas dimensions, not its last pixel
// index), so the cursor is always recoverable (MENU-SPEC §2.2 property 3).
//
// The four direction arguments are plain truth values rather than a
// PlatformInput, so this unit needs no platform header and no screen state:
// its whole contract is "here is a d-pad, here is a point, move the point".
static inline void foh_hand_step(double *x, double *y, int up, int down,
                                 int left, int right, double w, double h) {
  // lsX/lsY are the analogue stick upstream reads; a d-pad gives full
  // deflection or nothing (D1). Note the SIGN: upstream's canvas y grows
  // downward and its stick y grows upward, so up SUBTRACTS.
  const double lsX = (right ? 1.0 : 0.0) - (left ? 1.0 : 0.0);
  const double lsY = (up ? 1.0 : 0.0) - (down ? 1.0 : 0.0);
  *x += lsX * FOH_CURSOR_VX;
  *y += -lsY * FOH_CURSOR_VY;
  // The clamp order (x fully, then y fully) and the `>` / `else if <` shape
  // are the pre-extraction CSS body verbatim. They are behaviour, not style:
  // check-hand.sh's differential compares the resulting doubles bit for bit.
  if (*x > w) *x = w;
  else if (*x < 0.0) *x = 0.0;
  if (*y > h) *y = h;
  else if (*y < 0.0) *y = 0.0;
}

// The index of the first rect that STRICTLY contains (x, y), else -1.
//
// Strict on all four sides, which is `css_cell_at`'s rule promoted: the CSS
// cells are drawn with a 2 px gutter between them and D4 forbids a hit region
// where nothing is drawn, so the gutter is genuinely no cell — a hand parked
// in it hovers nothing. Target-select inherits that for free, and it matters
// there too (its slot rows are 22 px apart with 19 px bodies).
static inline int foh_hand_hit(const FohHandRect *rects, int n, double x,
                               double y) {
  for (int i = 0; i < n; i++) {
    if (x > (double)rects[i].x && x < (double)(rects[i].x + rects[i].w) &&
        y > (double)rects[i].y && y < (double)(rects[i].y + rects[i].h)) {
      return i;
    }
  }
  return -1;
}

#endif // FOH_HAND_H
