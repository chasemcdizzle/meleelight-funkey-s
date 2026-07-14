// line_angle.h <- src/main/util/lineAngle.js (structure-parallel
// translation). Math.atan2 is the vendored fdlibm fd_atan2 (PLAN §2 —
// same bits as the oracle's shimmed Math.atan2).
#ifndef ML_LINE_ANGLE_H
#define ML_LINE_ANGLE_H

#include "../ml_js.h"
#include "vec2d.h"
#include "../../fdlibm/fdlibm.h"

// returns angle of line from the positive x axis, in radians, from 0 to pi
static inline double lineAngle(Line2 line) {
  const Vec2D v1 = line.a;
  const Vec2D v2 = line.b;
  const double theta = fd_atan2(v2.y - v1.y, v2.x - v1.x);
  if (theta < 0) {
    return theta + js_pi();
  } else {
    return theta;
  }
}

#endif // ML_LINE_ANGLE_H
