// interpolated_collision.c <- src/physics/interpolatedCollision.js
// (structure-parallel translation, M2 task 5). Expression shapes copied
// verbatim (prevention rule 6); Math.pow -> fd_pow, Math.min -> js_min
// (rules 1/4); solveQuadraticEquation's upstream signature has a DEFAULT
// parameter `sign = 1` (babel materializes it), so the 3-arg calls here
// pass sign = 1 explicitly.
#include "interpolated_collision.h"

#include "../fdlibm/fdlibm.h"
#include "environmental_collision.h" // vLineThrough/hLineThrough/coordinateInterceptParameter
#include "ml_js.h"
#include "util/detect_intersections.h" // distanceToPolygon
#include "util/find_smallest_within.h" // pickSmallestSweep
#include "util/lin_alg.h"              // euclideanDist, ml_add
#include "util/solve_quadratic_equation.h"

static MaybeVec2D maybe_vec(Vec2D v) {
  MaybeVec2D m; m.present = true; m.v = v; return m;
}
static MaybeVec2D maybe_vec_null(void) {
  MaybeVec2D m; m.present = false; m.v = vec2d(0, 0); return m;
}

// computes the first point of intersection between two sweeping circles
// circle 1 sweeps from p1 to p2 with radius going from r1 to r2
// circle 2 sweeps from q1 to q2 with radius going from s1 to s2
MaybeVec2D sweepCircleVsSweepCircle(Vec2D p1, double r1, Vec2D p2, double r2,
                                    Vec2D q1, double s1, Vec2D q2, double s2) {
  if (euclideanDist(p1, q1) < (r1 + s1)) {
    return maybe_vec(vec2d(0.5 * p1.x + 0.5 * q1.x, 0.5 * p1.y + 0.5 * q1.y));
  } else {
    const double u = p1.x + q2.x - p2.x - q1.x;
    const double v = p1.y + q2.y - p2.y - q1.y;
    const double w = r1 + s1 - r2 - s2;
    const double a0 = fd_pow(p1.x - q1.x, 2) + fd_pow(p1.y - q1.y, 2) -
                      fd_pow(r1 + s1, 2);
    const double a1 =
        -2 * ((p1.x - q1.x) * u + (p1.y - q1.y) * v - (r1 + s1) * w);
    const double a2 = fd_pow(u, 2) + fd_pow(v, 2) - fd_pow(w, 2);
    const MaybeNum t1 = solveQuadraticEquation(a0, a1, a2, 1);

    // let t = null; (tPresent models it)
    bool tPresent = false;
    double t = 0;
    if (t1.present && !isnan(t1.v)) {
      const double t2 = a0 / (a2 * t1.v);
      if (t1.v < 0 || t1.v > 1) {
        if (t2 < 0 || t2 > 1 || isnan(t2)) {
          tPresent = false;
        } else {
          tPresent = true;
          t = t2;
        }
      } else {
        if (t2 < 0 || t2 > 1 || isnan(t2)) {
          tPresent = true;
          t = t1.v;
        } else {
          tPresent = true;
          t = js_min(t1.v, t2);
        }
      }
    }

    if (!tPresent) {
      return maybe_vec_null();
    } else {
      const double r = (1 - t) * r1 + t * r2;
      const double s = (1 - t) * s1 + t * s2;
      const Vec2D p = vec2d((1 - t) * p1.x + t * p2.x, (1 - t) * p1.y + t * p2.y);
      const Vec2D q = vec2d((1 - t) * q1.x + t * q2.x, (1 - t) * q1.y + t * q2.y);
      const double wp = s / (r + s);
      const double wq = r / (r + s);
      return maybe_vec(vec2d(wp * p.x + wq * q.x, wp * p.y + wq * q.y));
    }
  }
}

// AABBCheck = { case:"corner", corner, p1, p2, r1, r2 }
//           | { case:"line", line1, line2 }
typedef struct {
  bool isCorner;
  // corner case:
  Vec2D corner;
  Vec2D p1, p2;
  double r1, r2;
  // line case:
  Line2 line1;
  Line2 line2;
} AABBCheck;

typedef struct {
  bool present; // null result
  double sweep;
  Vec2D point;
} AABBCheckResult;

// this function computes the sweeping parameter and collision location
// between a sweeping circle and an AABB (corner case / line case)
static AABBCheckResult aabbChecker(const AABBCheck *check) {
  AABBCheckResult res;
  res.present = false;
  res.sweep = 0;
  res.point = vec2d(0, 0);
  if (check->isCorner) {
    const Vec2D c = check->corner;
    const Vec2D p1 = check->p1;
    const Vec2D p2 = check->p2;
    const double r1 = check->r1;
    const double r2 = check->r2;
    if (euclideanDist(c, p1) < r1) {
      res.present = true;
      res.sweep = 0;
      res.point = c;
      return res;
    } else {
      const double a0 =
          fd_pow(p1.x - c.x, 2) + fd_pow(p1.y - c.y, 2) - fd_pow(r1, 2);
      const double a1 = -2 * ((p1.x - c.x) * (p1.x - p2.x) +
                              (p1.y - c.y) * (p1.y - p2.y) - r1 * (r1 - r2));
      const double a2 = fd_pow(p1.x - p2.x, 2) + fd_pow(p1.y - p2.y, 2) -
                        fd_pow(r1 - r2, 2);
      const MaybeNum t1 = solveQuadraticEquation(a0, a1, a2, 1);

      bool tPresent = false;
      double t = 0;
      if (t1.present && !isnan(t1.v)) {
        const double t2 = a0 / (a2 * t1.v);
        if (t1.v < 0 || t1.v > 1) {
          if (t2 < 0 || t2 > 1 || isnan(t2)) {
            tPresent = false;
          } else {
            tPresent = true;
            t = t2;
          }
        } else {
          if (t2 < 0 || t2 > 1 || isnan(t2)) {
            tPresent = true;
            t = t1.v;
          } else {
            tPresent = true;
            t = js_min(t1.v, t2);
          }
        }
      }

      if (!tPresent) {
        return res;
      } else {
        res.present = true;
        res.sweep = t;
        res.point = c;
        return res;
      }
    }
  } else {
    const Vec2D p = check->line1.a; // check.line1[0]
    const Vec2D q = check->line1.b; // check.line1[1]
    const double s = coordinateInterceptParameter(check->line2, check->line1);
    if (s < 0 || s > 1 || isnan(s) || s == INFINITY) {
      return res;
    } else {
      const Vec2D pt = vec2d((1 - s) * p.x + s * q.x, (1 - s) * p.y + s * q.y);
      if (pt.x < js_min(check->line2.a.x, check->line2.b.x) ||
          pt.x > js_max(check->line2.a.x, check->line2.b.x) ||
          pt.y < js_min(check->line2.a.y, check->line2.b.y) ||
          pt.y > js_max(check->line2.a.y, check->line2.b.y)) {
        return res;
      } else {
        res.present = true;
        res.sweep = s;
        res.point = pt;
        return res;
      }
    }
  }
}

static AABBCheck mk_corner(Vec2D corner, Vec2D p1, Vec2D p2, double r1,
                           double r2) {
  AABBCheck c;
  c.isCorner = true;
  c.corner = corner;
  c.p1 = p1;
  c.p2 = p2;
  c.r1 = r1;
  c.r2 = r2;
  c.line1 = line2(vec2d(0, 0), vec2d(0, 0));
  c.line2 = c.line1;
  return c;
}

static AABBCheck mk_line(Line2 l1, Line2 l2) {
  AABBCheck c;
  c.isCorner = false;
  c.corner = vec2d(0, 0);
  c.p1 = c.corner;
  c.p2 = c.corner;
  c.r1 = 0;
  c.r2 = 0;
  c.line1 = l1;
  c.line2 = l2;
  return c;
}

// computes the first point of collision between:
//  - sweeping circle, going from p1 with radius r1 to p2 with radius r2
//  - fixed AABB with bottom left point bl and top right point tr
MaybeVec2D sweepCircleVsAABB(Vec2D p1, double r1, Vec2D p2, double r2,
                             Vec2D bl, Vec2D tr) {
  const Vec2D br = vec2d(tr.x, bl.y);
  const Vec2D tl = vec2d(bl.x, tr.y);
  PolygonPts poly;
  poly.count = 4;
  poly.items[0] = bl;
  poly.items[1] = br;
  poly.items[2] = tr;
  poly.items[3] = tl;
  if (distanceToPolygon(p1, &poly) <= r1) {
    return maybe_vec(p1);
  } else if ((p1.x + r1 < bl.x && p2.x + r2 < bl.x) ||
             (p1.x - r1 > tr.x && p2.x - r2 > tr.x) ||
             (p1.y + r1 < bl.y && p2.y + r2 < bl.y) ||
             (p1.y - r1 > tr.y && p2.y - r2 > tr.y)) {
    return maybe_vec_null();
  } else {
    AABBCheck checks[5];
    int nchecks = 0;
    if (p1.x <= bl.x) {
      if (p1.y <= bl.y) { // bottom left corner
        checks[0] = mk_corner(bl, p1, p2, r1, r2);
        checks[1] = mk_corner(tl, p1, p2, r1, r2);
        checks[2] = mk_corner(br, p1, p2, r1, r2);
        checks[3] = mk_line(vLineThrough(bl),
                            line2(ml_add(p1, vec2d(r1, 0)), ml_add(p2, vec2d(r2, 0))));
        checks[4] = mk_line(hLineThrough(bl),
                            line2(ml_add(p1, vec2d(0, r1)), ml_add(p2, vec2d(0, r2))));
        nchecks = 5;
      } else if (p1.y >= tr.y) { // top left corner
        checks[0] = mk_corner(tl, p1, p2, r1, r2);
        checks[1] = mk_corner(bl, p1, p2, r1, r2);
        checks[2] = mk_corner(tr, p1, p2, r1, r2);
        checks[3] = mk_line(vLineThrough(bl),
                            line2(ml_add(p1, vec2d(r1, 0)), ml_add(p2, vec2d(r2, 0))));
        checks[4] = mk_line(hLineThrough(tr),
                            line2(ml_add(p1, vec2d(0, -r1)), ml_add(p2, vec2d(0, -r2))));
        nchecks = 5;
      } else { // left side
        checks[0] = mk_corner(bl, p1, p2, r1, r2);
        checks[1] = mk_corner(tl, p1, p2, r1, r2);
        checks[2] = mk_line(vLineThrough(bl),
                            line2(ml_add(p1, vec2d(r1, 0)), ml_add(p2, vec2d(r2, 0))));
        nchecks = 3;
      }
    } else if (p1.x >= tr.x) {
      if (p1.y <= bl.y) { // bottom right corner
        checks[0] = mk_corner(br, p1, p2, r1, r2);
        checks[1] = mk_corner(bl, p1, p2, r1, r2);
        checks[2] = mk_corner(tr, p1, p2, r1, r2);
        checks[3] = mk_line(vLineThrough(tr),
                            line2(ml_add(p1, vec2d(-r1, 0)), ml_add(p2, vec2d(-r2, 0))));
        checks[4] = mk_line(hLineThrough(bl),
                            line2(ml_add(p1, vec2d(0, r1)), ml_add(p2, vec2d(0, r2))));
        nchecks = 5;
      } else if (p1.y >= tr.y) { // top right corner
        checks[0] = mk_corner(tr, p1, p2, r1, r2);
        checks[1] = mk_corner(tl, p1, p2, r1, r2);
        checks[2] = mk_corner(br, p1, p2, r1, r2);
        checks[3] = mk_line(vLineThrough(tr),
                            line2(ml_add(p1, vec2d(-r1, 0)), ml_add(p2, vec2d(-r2, 0))));
        checks[4] = mk_line(hLineThrough(tr),
                            line2(ml_add(p1, vec2d(0, -r1)), ml_add(p2, vec2d(0, -r2))));
        nchecks = 5;
      } else { // right side
        checks[0] = mk_corner(tr, p1, p2, r1, r2);
        checks[1] = mk_corner(br, p1, p2, r1, r2);
        checks[2] = mk_line(vLineThrough(tr),
                            line2(ml_add(p1, vec2d(-r1, 0)), ml_add(p2, vec2d(-r2, 0))));
        nchecks = 3;
      }
    } else {
      if (p1.y <= bl.y) { // bottom side
        checks[0] = mk_corner(bl, p1, p2, r1, r2);
        checks[1] = mk_corner(br, p1, p2, r1, r2);
        checks[2] = mk_line(hLineThrough(bl),
                            line2(ml_add(p1, vec2d(0, r1)), ml_add(p2, vec2d(0, r2))));
        nchecks = 3;
      } else { // top side, all other cases have been ruled out
        checks[0] = mk_corner(tl, p1, p2, r1, r2);
        checks[1] = mk_corner(tr, p1, p2, r1, r2);
        checks[2] = mk_line(hLineThrough(tr),
                            line2(ml_add(p1, vec2d(0, -r1)), ml_add(p2, vec2d(0, -r2))));
        nchecks = 3;
      }
    }

    // const first = pickSmallestSweep(checks.map(aabbChecker));
    AABBCheckResult results[5];
    bool present[5];
    double sweep[5];
    for (int i = 0; i < nchecks; i++) {
      results[i] = aabbChecker(&checks[i]);
      present[i] = results[i].present;
      sweep[i] = results[i].sweep;
    }
    const int first = pickSmallestSweep(present, sweep, nchecks);
    if (first == -1) {
      return maybe_vec_null();
    } else {
      return maybe_vec(results[first].point);
    }
  }
}
