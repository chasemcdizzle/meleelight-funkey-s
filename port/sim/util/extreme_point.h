// extreme_point.h <- src/stages/util/extremePoint.js (structure-parallel
// translation). `extreme` is one of 'u'/'t'/'d'/'b'/'l'/'r'.
#ifndef ML_EXTREME_POINT_H
#define ML_EXTREME_POINT_H

#include "vec2d.h"
#include "../stage_types.h"

static inline Vec2D extremePoint(SurfaceGeom wall, char extreme) {
  const Vec2D v1 = wall.p0;
  const Vec2D v2 = wall.p1;
  switch (extreme) {
    case 'u':
    case 't':
      if (v2.y < v1.y) {
        return v1;
      } else {
        return v2;
      }
    case 'd':
    case 'b':
      if (v2.y > v1.y) {
        return v1;
      } else {
        return v2;
      }
    case 'l':
      if (v2.x > v1.x) {
        return v1;
      } else {
        return v2;
      }
    case 'r':
      if (v2.x < v1.x) {
        return v1;
      } else {
        return v2;
      }
    default:
      // console.log("error in 'extremePoint': invalid parameter ...")
      return v1; // just to make the type checker happy
  }
}

#endif // ML_EXTREME_POINT_H
