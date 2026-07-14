// lin_alg.h <- src/main/linAlg.js (structure-parallel translation, FULL
// module — M2 task 1; the M2-CAL slice carried only dotProd/add/subtract).
// `add`/`subtract` keep their ml_ prefix (C namespace hygiene); everything
// else keeps the upstream export name. norm/euclideanDist use C sqrt —
// IEEE-754 correctly-rounded, bit-identical to JS Math.sqrt (not part of
// the fdlibm transcendental surface).
//
// null | matrix (inverseMatrix) is modeled as MaybeMat2 (key-presence
// rule: absence is explicit, never a sentinel value).
#ifndef ML_LIN_ALG_H
#define ML_LIN_ALG_H

#include "vec2d.h"

// [[x1,x2],[y1,y2]] — a JS 2x2 row-major matrix (array of arrays).
typedef struct { double x1, x2, y1, y2; } Mat2;

typedef struct { bool present; Mat2 m; } MaybeMat2;

// A JS pair [x,y] (multMatVect operand/result — an ARRAY, not a Vec2D).
typedef struct { double a, b; } NumPair;

static inline double dotProd(Vec2D vec1, Vec2D vec2) {
  return (vec1.x * vec2.x + vec1.y * vec2.y);
}

static inline Vec2D scalarProd(double lambda, Vec2D vec) {
  return vec2d(lambda * vec.x, lambda * vec.y);
}

static inline double norm(Vec2D vec) {
  return sqrt(dotProd(vec, vec));
}

static inline Vec2D ml_add(Vec2D vec1, Vec2D vec2) {
  return vec2d(vec1.x + vec2.x, vec1.y + vec2.y);
}

static inline Vec2D ml_subtract(Vec2D vec1, Vec2D vec2) {
  return vec2d(vec1.x - vec2.x, vec1.y - vec2.y);
}

// (module-private upstream)
static inline double squaredDist(Vec2D center1, Vec2D center2) {
  return ((center2.x - center1.x) * (center2.x - center1.x) +
          (center2.y - center1.y) * (center2.y - center1.y));
}

static inline double euclideanDist(Vec2D center1, Vec2D center2) {
  const double sqDist = squaredDist(center1, center2);
  return sqDist <= 0 ? 0 : sqrt(sqDist);
}

static inline double manhattanDist(Vec2D center1, Vec2D center2) {
  return (js_abs(center2.x - center1.x) + js_abs(center2.y - center1.y));
}

// orthogonally projects a point onto a line
// line is given by two points it passes through
static inline Vec2D orthogonalProjection(Vec2D point, Line2 line) {
  const Vec2D line0 = line.a;
  const double line0x = line0.x, line0y = line0.y;
  if (line0x == line.b.x && line0y == line.b.y) {
    // console.log("error in function 'orthogonalProjection', line reduced
    // to a point.") — log dropped, value behavior identical
    return line0;
  } else {
    // turn everything into relative coordinates with respect to the point line[0]
    const Vec2D pointVec = vec2d(point.x - line0x, point.y - line0y);
    const Vec2D lineVec = vec2d(line.b.x - line0x, line.b.y - line0y);
    // renormalise line vector
    const double lineNorm = norm(lineVec);
    const Vec2D lineElem = scalarProd(1 / lineNorm, lineVec);
    // vector projection calculation
    const double factor = dotProd(pointVec, lineElem);
    const Vec2D projVec = scalarProd(factor, lineElem);
    // back to absolute coordinates by adding the coordinates of line[0]
    return vec2d(projVec.x + line0x, projVec.y + line0y);
  }
}

// Computes the inverse of a 2x2 matrix.
static inline MaybeMat2 inverseMatrix(Mat2 m) {
  MaybeMat2 out;
  const double det = m.x1 * m.y2 - m.x2 * m.y1;
  if (js_abs(det) < 0.00001) {
    // console.log("error in inverseMatrix: determinant too small")
    out.present = false;
    out.m.x1 = out.m.x2 = out.m.y1 = out.m.y2 = 0;
    return out;
  } else {
    out.present = true;
    out.m.x1 = m.y2 / det;
    out.m.x2 = -m.x2 / det;
    out.m.y1 = -m.y1 / det;
    out.m.y2 = m.x1 / det;
    return out;
  }
}

// Multiplication Av (A a 2x2 matrix, v a 2x1 column vector)
// Return type: [xnew,ynew]
static inline NumPair multMatVect(Mat2 m, NumPair v) {
  NumPair out;
  out.a = m.x1 * v.a + m.x2 * v.b;
  out.b = m.y1 * v.a + m.y2 * v.b;
  return out;
}

static inline Vec2D reflect(Vec2D reflectee, Vec2D reflector) {
  const Vec2D projVec =
      orthogonalProjection(reflectee, line2(vec2d(0, 0), reflector));
  const Vec2D moveVec = ml_subtract(projVec, reflectee);
  return ml_add(reflectee, scalarProd(2, moveVec));
}

#endif // ML_LIN_ALG_H
