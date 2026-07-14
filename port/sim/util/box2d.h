// box2d.h <- src/main/util/Box2D.js (structure-parallel translation).
// Upstream: class Box2D { min: Vec2D; max: Vec2D } constructed from two
// [number,number] pairs. Pure record type (consumed by stage data — M1
// tables — and hitDetection AABB checks).
#ifndef ML_BOX2D_H
#define ML_BOX2D_H

#include "vec2d.h"

typedef struct { Vec2D min, max; } Box2D;

// constructor(min: [number,number], max: [number,number])
static inline Box2D box2d(double min0, double min1, double max0, double max1) {
  Box2D b;
  b.min = vec2d(min0, min1);
  b.max = vec2d(max0, max1);
  return b;
}

#endif // ML_BOX2D_H
