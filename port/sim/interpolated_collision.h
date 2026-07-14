// interpolated_collision.h <- src/physics/interpolatedCollision.js
// (structure-parallel translation, M2 task 5). Pure geometry: sweeping
// circle vs sweeping circle / fixed AABB. Consumers upstream are
// hitDetection.js (interpolated hit checks) and article.js — both call
// through the babel-CJS namespace, so the physics-spec capture records
// every live call and the replay drives these translations directly.
//
// Value model: `null | Vec2D` results become MaybeVec2D (present flag),
// matching the envcoll conventions (environmental_collision.h).
#ifndef ML_INTERPOLATED_COLLISION_H
#define ML_INTERPOLATED_COLLISION_H

#include "util/vec2d.h"

typedef struct {
  bool present; // false <=> JS null
  Vec2D v;
} MaybeVec2D;

// sweepCircleVsSweepCircle(p1, r1, p2, r2, q1, s1, q2, s2)
MaybeVec2D sweepCircleVsSweepCircle(Vec2D p1, double r1, Vec2D p2, double r2,
                                    Vec2D q1, double s1, Vec2D q2, double s2);

// sweepCircleVsAABB(p1, r1, p2, r2, bl, tr)
MaybeVec2D sweepCircleVsAABB(Vec2D p1, double r1, Vec2D p2, double r2,
                             Vec2D bl, Vec2D tr);

#endif // ML_INTERPOLATED_COLLISION_H
