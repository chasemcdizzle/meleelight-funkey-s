// detect_intersections.h <- src/stages/util/detectIntersections.js
// (structure-parallel translation, M2 task 1). Exports intersectsAny,
// distanceToLine, distanceToPolygon, lineDistanceToLines; the private
// helpers (intersects, isInside, evenNumberOfTrue, distanceToLines,
// minimum, linesOfPolygon, distanceBetweenLines) keep a di_ prefix.
//
// Shape notes (rule 6 — expression shapes verbatim):
// - `t1 === Infinity` tests POSITIVE infinity only (C `t1 == INFINITY`).
// - `minimum` is a RIGHT fold (head < next ? head : next over the
//   recursion): the C loop runs from the tail so NaN/tie behavior is
//   byte-identical.
// - `evenNumberOfTrue` flips parity per true head; [] is true.
// - JS `.map(...)` materializes ALL distances before the fold — the C
//   code fills an array first, then folds (same evaluation structure).
#ifndef ML_DETECT_INTERSECTIONS_H
#define ML_DETECT_INTERSECTIONS_H

#include "vec2d.h"
#include "lin_alg.h"
#include "extreme_point.h"
#include "../environmental_collision.h" // coordinateInterceptParameter

#define ML_MAX_LINES 128

typedef struct { Line2 items[ML_MAX_LINES]; int count; } LineList;
typedef struct { Vec2D items[ML_MAX_LINES]; int count; } PolygonPts;

// (private upstream: intersects)
static inline bool di_intersects(Line2 line1, Line2 line2) {
  const double t1 = coordinateInterceptParameter(line1, line2);
  const double t2 = coordinateInterceptParameter(line2, line1);
  if (isnan(t1) || isnan(t2) || t1 == INFINITY || t2 == INFINITY ||
      t1 < 0 || t2 < 0 || t1 > 1 || t2 > 1) {
    return false;
  } else {
    return true;
  }
}

static inline bool intersectsAny(Line2 newLine, const LineList *lines) {
  for (int i = 0; i < lines->count; i++) {
    if (di_intersects(newLine, lines->items[i])) {
      return true;
    }
  }
  return false;
}

// (private upstream: evenNumberOfTrue — parity fold, [] === true)
static inline bool di_evenNumberOfTrue(const bool *list, int n) {
  bool even = true;
  for (int i = 0; i < n; i++) {
    if (list[i]) even = !even;
  }
  return even;
}

// (private upstream: isInside)
static inline bool di_isInside(Vec2D point, const LineList *lines) {
  const Vec2D pt = vec2d(point.x + 0.001, point.y);
  const Vec2D atInfinity = vec2d(point.x + 0.001, point.y + 100000);
  bool mapped[ML_MAX_LINES];
  for (int i = 0; i < lines->count; i++) {
    mapped[i] = di_intersects(lines->items[i], line2(pt, atInfinity));
  }
  return !di_evenNumberOfTrue(mapped, lines->count);
}

// (private upstream: minimum — right fold, minimum([]) === Infinity)
static inline double di_minimum(const double *numbers, int n) {
  double next = INFINITY;
  for (int i = n - 1; i >= 0; i--) {
    const double head = numbers[i];
    if (head < next) {
      next = head;
    }
    // else: keep next
  }
  return next;
}

static inline double distanceToLine(Vec2D point, Line2 line) {
  if (euclideanDist(line.a, line.b) < 0.001) {
    return euclideanDist(point, line.a);
  } else {
    const Vec2D projectedPoint = orthogonalProjection(point, line);
    SurfaceGeom sg;
    sg.p0 = line.a;
    sg.p1 = line.b;
    const Vec2D lineRight = extremePoint(sg, 'r');
    const Vec2D lineLeft = extremePoint(sg, 'l');
    const Vec2D lineTop = extremePoint(sg, 't');
    const Vec2D lineBot = extremePoint(sg, 'b');
    if (projectedPoint.x > lineRight.x) {
      return euclideanDist(point, lineRight);
    } else if (projectedPoint.x < lineLeft.x) {
      return euclideanDist(point, lineLeft);
    } else if (projectedPoint.y > lineTop.y) {
      return euclideanDist(point, lineTop);
    } else if (projectedPoint.y < lineBot.y) {
      return euclideanDist(point, lineBot);
    } else {
      return euclideanDist(point, projectedPoint);
    }
  }
}

// (private upstream: distanceToLines)
static inline double di_distanceToLines(Vec2D point, const LineList *lines) {
  if (di_isInside(point, lines)) {
    return -1;
  } else {
    double mapped[ML_MAX_LINES];
    for (int i = 0; i < lines->count; i++) {
      mapped[i] = distanceToLine(point, lines->items[i]);
    }
    return di_minimum(mapped, lines->count);
  }
}

// (private upstream: linesOfPolygon — polygon must be non-empty; the
// marshaller enforces count >= 1, matching the captured domain)
static inline void di_linesOfPolygon(const PolygonPts *polygon, LineList *out) {
  const int lg = polygon->count;
  Vec2D pt = polygon->items[lg - 1];
  out->count = 0;
  for (int i = 0; i < polygon->count; i++) {
    out->items[out->count++] = line2(pt, polygon->items[i]);
    pt = polygon->items[i];
  }
}

static inline double distanceToPolygon(Vec2D point, const PolygonPts *polygon) {
  LineList lines;
  di_linesOfPolygon(polygon, &lines);
  return di_distanceToLines(point, &lines);
}

// (private upstream: distanceBetweenLines)
static inline double di_distanceBetweenLines(Line2 line1, Line2 line2v) {
  if (di_intersects(line1, line2v)) {
    return 0;
  } else {
    const double candidates[4] = {
      distanceToLine(line1.a, line2v),
      distanceToLine(line1.b, line2v),
      distanceToLine(line2v.a, line1),
      distanceToLine(line2v.b, line1),
    };
    return di_minimum(candidates, 4);
  }
}

static inline double lineDistanceToLines(Line2 thisLine,
                                         const LineList *otherLines) {
  double mapped[ML_MAX_LINES];
  for (int i = 0; i < otherLines->count; i++) {
    mapped[i] = di_distanceBetweenLines(thisLine, otherLines->items[i]);
  }
  return di_minimum(mapped, otherLines->count);
}

#endif // ML_DETECT_INTERSECTIONS_H
