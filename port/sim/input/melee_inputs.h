// melee_inputs.h <- src/input/meleeInputs.js (structure-parallel
// translation, FULL module — M2 task 3). GC-controller axis simulation:
// raw stick -> 0..255 GC values -> melee-unit axes with 1/80 quantization,
// the 0.28 deadzone, and the analog-trigger rescale.
//
// Zero LIVE records over the human-trace goldens (the harness input path
// bypasses polling), so these are verified by the synthetic-domain sweep
// records (spec-input.js sweep(), FORMAT.md) — real executed-upstream
// calls spanning every engine threshold and both branch families
// (cardinals null/object, GC/simulated, degenerate-cardinals fallback).
//
// Math.round -> js_round (ml_js.h; ECMAScript tie-toward-+Inf semantics,
// NOT C round()). Math.sqrt -> C sqrt (IEEE-754 correctly rounded,
// bit-identical to JS; not part of the fdlibm transcendental surface).
// StickCardinals | null | undefined collapses to MaybeCardinals
// (key-presence rule: absence explicit, never a sentinel).
#ifndef ML_MELEE_INPUTS_H
#define ML_MELEE_INPUTS_H

#include "../util/lin_alg.h" // inverseMatrix, multMatVect, Mat2, NumPair, Vec2D

// type StickCardinals (src/input/gamepad/gamepadInfo.js shape read here:
// center{x,y} / left / right / down / up).
typedef struct {
  Vec2D center;
  double left, right, down, up;
} StickCardinals;

typedef struct {
  bool present; // false <=> upstream null | undefined
  StickCardinals c;
} MaybeCardinals;

// function output of stickExtremePoints/fromCardinals:
// [[origx, origy], [lx, ly], [rx, ry], [dx, dy], [ux, uy]]
typedef struct {
  Vec2D orig, l, r, d, u;
} MlExtremePoints;

// --- module constants (meleeInputs.js:109-115) ------------------------------
static inline double ml_steps(void) { return 80; }
static inline double ml_deadzoneConst(void) { return 0.28; }
static inline double ml_leniency(void) { return 10; }
static inline double ml_meleeOrig(void) { return 128; }
// lowest / highest 0 -- 255 input the controller will give
static inline double ml_meleeMin(void) {
  return ml_meleeOrig() - (ml_steps() + ml_leniency());
}
static inline double ml_meleeMax(void) {
  return ml_meleeOrig() + (ml_steps() + ml_leniency());
}

// --- private helpers (module-internal upstream) ------------------------------

// meleeInputs.js:14 fromCardinals([origx,origy], l, r, d, u)
static inline MlExtremePoints fromCardinals(Vec2D orig, double l, double r,
                                            double d, double u) {
  MlExtremePoints out;
  out.orig = orig;
  out.l = vec2d(l, orig.y);
  out.r = vec2d(r, orig.y);
  out.d = vec2d(orig.x, d);
  out.u = vec2d(orig.x, u);
  return out;
}

// meleeInputs.js:22 stickExtremePoints(stickCardinals)
static inline MlExtremePoints stickExtremePoints(MaybeCardinals sc) {
  if (!sc.present) { // stickCardinals === null || undefined
    return fromCardinals(vec2d(0, 0), -1, 1, 1, -1);
  } else {
    return fromCardinals(sc.c.center, sc.c.left, sc.c.right, sc.c.down,
                         sc.c.up);
  }
}

// meleeInputs.js:40 renormaliseAxisInput([lx,ly],[rx,ry],[dx,dy],[ux,uy],[x,y])
// — maps the calibrated cardinal corners onto the unit square; quadrant
// selection by cross products, expression shapes verbatim (rule 6).
static inline NumPair renormaliseAxisInput(Vec2D l, Vec2D r, Vec2D d, Vec2D u,
                                           double x, double y) {
  const double lx = l.x, ly = l.y, rx = r.x, ry = r.y;
  const double dx = d.x, dy = d.y, ux = u.x, uy = u.y;
  MaybeMat2 invMat;
  Mat2 m;
  if ((x * ry - y * rx <= 0) && (x * uy - y * ux >= 0)) { // quadrant 1
    m.x1 = rx; m.x2 = ux; m.y1 = ry; m.y2 = uy;
    invMat = inverseMatrix(m);
  } else if ((x * uy - y * ux <= 0) && (x * ly - y * lx >= 0)) { // quadrant 2
    m.x1 = -lx; m.x2 = ux; m.y1 = -ly; m.y2 = uy;
    invMat = inverseMatrix(m);
  } else if ((x * ly - y * lx <= 0) && (x * dy - y * dx >= 0)) { // quadrant 3
    m.x1 = -lx; m.x2 = -dx; m.y1 = -ly; m.y2 = -dy;
    invMat = inverseMatrix(m);
  } else { // quadrant 4
    m.x1 = rx; m.x2 = -dx; m.y1 = ry; m.y2 = -dy;
    invMat = inverseMatrix(m);
  }

  if (!invMat.present) { // invMat === null || undefined
    NumPair out; out.a = x; out.b = y; return out;
  } else {
    NumPair v; v.a = x; v.b = y;
    return multMatVect(invMat.m, v);
  }
}

// meleeInputs.js:76 toInterval — clamps a value between -1 and 1
// (NaN falls through both comparisons and is returned unchanged).
static inline double toInterval(double x) {
  if (x < -1) {
    return -1;
  } else if (x > 1) {
    return 1;
  } else {
    return x;
  }
}

// meleeInputs.js:118 discretise — rescales -1 -- 0 -- 1 to
// min -- orig -- max and rounds to nearest integer (JS Math.round
// semantics; NaN and both zeros take the `else` branch -> orig).
static inline double discretise(double x, double min, double orig,
                                double max) {
  if (x < 0) {
    return js_round(x * (orig - min) + orig);
  } else if (x > 0) {
    return js_round(x * (max - orig) + orig);
  } else {
    return orig;
  }
}

// meleeInputs.js:163 axisRescale — basic mapping from 0 -- 255 back to
// -1 -- 1 done by Melee (both upstream call sites pass orig = meleeOrig).
static inline double axisRescale(double x, double orig) {
  return (x - orig) / ml_steps();
}

// meleeInputs.js:167 unitRetract — radial clamp onto the closed unit disc.
static inline NumPair unitRetract(double x, double y) {
  const double norm_ = sqrt(x * x + y * y); // Math.sqrt: C sqrt bit-identical
  NumPair out;
  if (norm_ < 1) {
    out.a = x; out.b = y;
  } else {
    out.a = x / norm_; out.b = y / norm_;
  }
  return out;
}

// --- exports ---------------------------------------------------------------

// meleeInputs.js:177 meleeRound — quantize to the 1/80 melee grid.
static inline double meleeRound(double x) {
  return js_round(ml_steps() * x) / ml_steps();
}

// meleeInputs.js:181 meleeAxesRescale([x,y]) (private upstream; the
// [..].map(meleeRound) applies meleeRound elementwise).
static inline NumPair meleeAxesRescale(double x, double y) {
  const double xnew = axisRescale(x, ml_meleeOrig());
  const double ynew = axisRescale(y, ml_meleeOrig());
  const NumPair r = unitRetract(xnew, ynew);
  NumPair out;
  out.a = meleeRound(r.a);
  out.b = meleeRound(r.b);
  return out;
}

// meleeInputs.js:91 scaleToGCTrigger(t, offset, scale)
static inline double scaleToGCTrigger(double t, double offset, double scale) {
  const double tnew = js_abs(scale) < 0.001 ? 0 : (t + offset) / scale;
  if (tnew > 1) {
    return 1;
  } else if (tnew < 0.3) {
    return 0;
  } else {
    return tnew;
  }
}

// meleeInputs.js:132 scaleToUnitAxes(x, y, stickCardinals, ccx, ccy) —
// rescales controller input to -1 -- 0 -- 1 in both axes.
static inline NumPair scaleToUnitAxes(double x, double y, MaybeCardinals sc,
                                      double customCenterX,
                                      double customCenterY) {
  const MlExtremePoints ep = stickExtremePoints(sc);
  double origx = ep.orig.x, origy = ep.orig.y;
  origx += customCenterX;
  origy += customCenterY;
  const NumPair p = renormaliseAxisInput(
      vec2d(ep.l.x - origx, ep.l.y - origy),
      vec2d(ep.r.x - origx, ep.r.y - origy),
      vec2d(ep.d.x - origx, ep.d.y - origy),
      vec2d(ep.u.x - origx, ep.u.y - origy),
      x - origx, y - origy);
  NumPair out;
  out.a = toInterval(p.a);
  out.b = toInterval(p.b);
  return out;
}

// meleeInputs.js:143 scaleUnitToGCAxes (private) — -1 -- 1 to 0 -- 255.
static inline NumPair scaleUnitToGCAxes(double x, double y) {
  NumPair out;
  out.a = discretise(x, ml_meleeMin(), ml_meleeOrig(), ml_meleeMax());
  out.b = discretise(y, ml_meleeMin(), ml_meleeOrig(), ml_meleeMax());
  return out;
}

// meleeInputs.js:150 scaleToGCAxes (private) — raw input to 0 -- 255 by
// GC controller simulation.
static inline NumPair scaleToGCAxes(double x, double y, MaybeCardinals sc,
                                    double customCenterX,
                                    double customCenterY) {
  const NumPair p = scaleToUnitAxes(x, y, sc, customCenterX, customCenterY);
  return scaleUnitToGCAxes(p.a, p.b);
}

// meleeInputs.js:192 scaleToMeleeAxes(x, y, isGC, stickCardinals, ccx, ccy)
// — the main input rescaling function (defaults ccx = ccy = 0 at the JS
// signature; C callers pass them explicitly).
static inline NumPair scaleToMeleeAxes(double x, double y, bool isGC,
                                       MaybeCardinals sc,
                                       double customCenterX,
                                       double customCenterY) {
  double xnew = x;
  double ynew = y;
  if (isGC) { // gamecube controllers, don't mess up the raw data
    xnew = (x - customCenterX + 1) * 255 / 2; // raw -> 0 -- 255 in obvious way
    ynew = (-y + customCenterY + 1) * 255 / 2; // y incurs a sign flip
  } else { // convert raw input to 0 -- 255 by GC controller simulation
    const NumPair p = scaleToGCAxes(x, y, sc, customCenterX, customCenterY);
    xnew = p.a;
    ynew = p.b;
  }
  return meleeAxesRescale(xnew, ynew);
}

// meleeInputs.js:210 deaden(x, dead = deadzoneConst)
static inline double deaden(double x, double dead) {
  return js_abs(x) < dead ? 0 : x;
}

// meleeInputs.js:215 tasRescale(x, y, isDeadzoned = false) — scales
// -1 -- 1 TAS data to melee-unit axes. NOTE (carried verbatim): upstream
// forwards isDeadzoned as a second argument to the UNARY meleeAxesRescale,
// which ignores it — the parameter has no effect and is dropped here.
static inline NumPair tasRescale(double x, double y) {
  const double xnew = (x + 1) * 255 / 2;
  const double ynew = (y + 1) * 255 / 2;
  return meleeAxesRescale(xnew, ynew);
}

#endif // ML_MELEE_INPUTS_H
