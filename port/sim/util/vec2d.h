// vec2d.h <- src/main/util/Vec2D.js (structure-parallel translation)
// The Vec2D class collapses to a value struct; XOrY ("x"|"y") is a char.
// The .dot method is unused by the environmentalCollision slice (dotProd
// in lin_alg.h is what the module calls).
#ifndef ML_VEC2D_H
#define ML_VEC2D_H

#include "../ml_js.h"

typedef struct { double x, y; } Vec2D;

typedef char XOrY; // 'x' | 'y'

static inline Vec2D vec2d(double x, double y) {
  Vec2D v; v.x = x; v.y = y; return v;
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
