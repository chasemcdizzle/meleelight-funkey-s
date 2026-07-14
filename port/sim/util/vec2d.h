// vec2d.h <- src/main/util/Vec2D.js (structure-parallel translation,
// FULL module — M2 task 1 completes what the M2-CAL slice started).
// The Vec2D class collapses to a value struct; XOrY ("x"|"y") is a char.
#ifndef ML_VEC2D_H
#define ML_VEC2D_H

#include "../ml_js.h"

typedef struct { double x, y; } Vec2D;

typedef char XOrY; // 'x' | 'y'

static inline Vec2D vec2d(double x, double y) {
  Vec2D v; v.x = x; v.y = y; return v;
}

// Vec2D.prototype.dot: this.x * vector.x + this.y * vector.y
static inline double vec2d_dot(Vec2D self, Vec2D vector) {
  return self.x * vector.x + self.y * vector.y;
}

// A JS line [Vec2D, Vec2D] — value pair.
typedef struct { Vec2D a, b; } Line2;

static inline Line2 line2(Vec2D a, Vec2D b) {
  Line2 l; l.a = a; l.b = b; return l;
}

static inline double getXOrYCoord(Vec2D vec, XOrY xOrY) {
  if (xOrY == 'x') {
    return vec.x;
  } else {
    return vec.y;
  }
}

// --- undef-at-rest projection (M2 rule 8) ----------------------------------
// getXOrYCoord is a property ACCESSOR: JS returns `vec.x` VERBATIM, so an
// undefined component (upstream's frame-1 uninitialized ECB1) is echoed as
// undefined, NOT converted — ToNumber(undefined)->NaN happens only at the
// consumer's arithmetic. Value models whose fields can hold undefined use
// JsNum/JsVec2D; the plain-double getXOrYCoord above is the valid
// projection for arithmetic contexts (soundness pinned per boundary by the
// no-undef-ret capture invariant).
typedef struct { bool isUndef; double v; } JsNum;

static inline JsNum js_num(double v) {
  JsNum n; n.isUndef = false; n.v = v; return n;
}
static inline JsNum js_undef(void) {
  JsNum n; n.isUndef = true; n.v = 0; return n;
}

typedef struct { JsNum x, y; } JsVec2D;

static inline JsNum getXOrYCoordJs(JsVec2D vec, XOrY xOrY) {
  if (xOrY == 'x') {
    return vec.x;
  } else {
    return vec.y;
  }
}

static inline Vec2D putXOrYCoord(double coord, XOrY xOrY) {
  if (xOrY == 'x') {
    return vec2d(coord, 0);
  } else {
    return vec2d(0, coord);
  }
}

static inline XOrY flipXOrY(XOrY xOrY) {
  return xOrY == 'x' ? 'y' : 'x';
}

#endif // ML_VEC2D_H
