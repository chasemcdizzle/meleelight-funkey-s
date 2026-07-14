// lin_alg.h <- src/main/linAlg.js (structure-parallel translation)
// Only the functions the environmentalCollision slice actually CALLS are
// translated: dotProd, add, subtract. The module's import list also names
// scalarProd, norm, orthogonalProjection but never uses them (dead
// imports upstream) — omitted; euclideanDist/manhattanDist/inverseMatrix/
// multMatVect/reflect belong to other modules' slices (M2).
#ifndef ML_LIN_ALG_H
#define ML_LIN_ALG_H

#include "vec2d.h"

static inline double dotProd(Vec2D vec1, Vec2D vec2) {
  return (vec1.x * vec2.x + vec1.y * vec2.y);
}

static inline Vec2D ml_add(Vec2D vec1, Vec2D vec2) {
  return vec2d(vec1.x + vec2.x, vec1.y + vec2.y);
}

static inline Vec2D ml_subtract(Vec2D vec1, Vec2D vec2) {
  return vec2d(vec1.x - vec2.x, vec1.y - vec2.y);
}

#endif // ML_LIN_ALG_H
