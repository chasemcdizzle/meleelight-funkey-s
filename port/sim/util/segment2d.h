// segment2d.h <- src/main/util/Segment2D.js (structure-parallel
// translation). Upstream Segment2D is a constructor function whose
// instances carry {x, y, vecx, vecy} plus two per-instance closure
// methods (segLength, project); the C struct keeps the four value fields
// and the methods become functions taking the instance first.
#ifndef ML_SEGMENT2D_H
#define ML_SEGMENT2D_H

#include "vec2d.h"

typedef struct { double x, y, vecx, vecy; } Segment2D;

static inline Segment2D segment2d(double x, double y, double vecx, double vecy) {
  Segment2D s;
  s.x = x;
  s.y = y;
  s.vecx = vecx;
  s.vecy = vecy;
  return s;
}

// this.segLength = function () { ... }
static inline double segment2d_segLength(Segment2D self) {
  const double dx = self.vecx;
  const double dy = self.vecy;
  return sqrt(dx * dx + dy * dy);
}

// this.project = function (segOnto) { ... }
static inline Vec2D segment2d_project(Segment2D self, Segment2D segOnto) {
  const Vec2D vec = vec2d(self.vecx, self.vecy);
  const Vec2D onto = vec2d(segOnto.vecx, segOnto.vecy);
  const double d = vec2d_dot(onto, onto);
  if (0 < d) {
    const double dp = vec2d_dot(vec, onto);
    const double multiplier = dp / d;
    const double rx = onto.x * multiplier;
    const double ry = onto.y * multiplier;
    return vec2d(rx, ry);
  }
  return vec2d(0, 0);
}

#endif // ML_SEGMENT2D_H
