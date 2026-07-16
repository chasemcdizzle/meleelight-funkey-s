// environmental_collision.c <- src/physics/environmentalCollision.js
// @ upstream pin 27af171 (structure-parallel translation, M2-CAL).
// Section order, function order, names, and expression SHAPES mirror the
// JS source line-for-line; comments quote the original where they carry
// meaning. Compile every TU with -ffp-contract=off (PLAN §2).

#include "environmental_collision.h"
#include "util/lin_alg.h"
#include "util/find_smallest_within.h"
#include "util/solve_quadratic_equation.h"
#include "util/line_angle.h"
#include "util/extreme_point.h"
#include "util/ecb_transform.h"
#include "util/zip_labels.h"
#include "util/draw_ecb.h"

// const additionalOffset / smallestECBWidth / smallestECBHeight are the
// #defines in the header; const maxRecursion = 6:
#define maxRecursion 6
#define additionalOffset ML_ADDITIONAL_OFFSET
#define smallestECBWidth ML_SMALLEST_ECB_WIDTH
#define smallestECBHeight ML_SMALLEST_ECB_HEIGHT

// -----------------------------------------------------
// various utility functions

// horizontal line through a point
Line2 hLineThrough(Vec2D point) {
  return line2(point, vec2d(point.x + 1, point.y));
}

Line2 hLineAt(double y) {
  return hLineThrough(vec2d(0, y));
}

// vertical line through a point
Line2 vLineThrough(Vec2D point) {
  return line2(point, vec2d(point.x, point.y + 1));
}

Line2 vLineAt(double x) {
  return vLineThrough(vec2d(x, 0));
}

// either horizontal or vertical line through a point
Line2 lineThrough(Vec2D point, XOrY xOrY) {
  if (xOrY == 'x') {
    return hLineThrough(point);
  } else {
    return vLineThrough(point);
  }
}

// next ECB point index, counterclockwise or clockwise (w.r.t. the ECB)
static double turn(double number, bool counterclockwise) {
  if (counterclockwise) {
    if (number == 3) {
      return 0;
    } else {
      return number + 1;
    }
  } else {
    if (number == 0) {
      return 3;
    } else {
      return number - 1;
    }
  }
}

Vec2D outwardsWallNormal(Vec2D wallBottomOrLeft, Vec2D wallTopOrRight,
                         char wallType) {
  double sign = 1;
  switch (wallType) {
    case 'l': // left wall
    case 'g': // ground
    case 'b':
    case 'd':
    case 'p': // platform
      sign = -1;
      break;
    default: // right wall, ceiling
      break;
  }
  return vec2d(sign * (wallTopOrRight.y - wallBottomOrLeft.y),
               sign * (wallBottomOrLeft.x - wallTopOrRight.x));
}

// returns true if the vector is moving into the wall, false otherwise
static bool movingInto(Vec2D vec, Vec2D wallTopOrRight, Vec2D wallBottomOrLeft,
                       char wallType) {
  return dotProd(vec, outwardsWallNormal(wallBottomOrLeft, wallTopOrRight,
                                         wallType)) < 0;
}

// returns true if point is to the right of a "left" wall, or to the left of
// a "right" wall, and false otherwise
static bool isOutside(Vec2D point, Vec2D wallTopOrRight, Vec2D wallBottomOrLeft,
                      char wallType) {
  return !movingInto(vec2d(point.x - wallBottomOrLeft.x,
                           point.y - wallBottomOrLeft.y),
                     wallTopOrRight, wallBottomOrLeft, wallType);
}

// returns the parameter t, such that p3 + t*(p4-p3) is the intersection
// point of the two lines
double coordinateInterceptParameter(Line2 line1, Line2 line2) {
  return ((line1.a.x - line2.a.x) * (line1.b.y - line1.a.y)
        + (line1.a.x - line1.b.x) * (line1.a.y - line2.a.y))
       / ((line2.b.x - line2.a.x) * (line1.b.y - line1.a.y)
        + (line1.b.x - line1.a.x) * (line2.a.y - line2.b.y));
}

// find the intersection of two lines
Vec2D coordinateIntercept(Line2 line1, Line2 line2) {
  const double t = coordinateInterceptParameter(line1, line2);
  return vec2d(line2.a.x + t * (line2.b.x - line2.a.x),
               line2.a.y + t * (line2.b.y - line2.a.y));
}

// ---------------------------------------------------------------------------
// basic collision detection functions

// first: point sweeping functions

static PointSweepResult psr_null(void) {
  PointSweepResult r; memset(&r, 0, sizeof r); r.present = false; return r;
}
static EdgeSweepResult esr_null(void) {
  EdgeSweepResult r; memset(&r, 0, sizeof r); r.present = false; return r;
}

static PointSweepResult pointSweepingCheck(ECB ecb1, ECB ecbp, double pt,
                                           Surface wall, char wallType,
                                           double wallIndex,
                                           Vec2D wallTopOrRight,
                                           Vec2D wallBottomOrLeft, XOrY xOrY);

// finds whether the ECB impacted a surface on one of its vertices
static PointSweepResult runPointSweep(ECB ecb1, ECB ecbp, double same,
                                      Surface wall, char wallType,
                                      double wallIndex,
                                      Vec2D wallBottomOrLeft,
                                      Vec2D wallTopOrRight, XOrY xOrY) {
  PointSweepResult result = psr_null();

  const double wallAngle =
      lineAngle(line2(wallBottomOrLeft, wallTopOrRight));

  if (wallType == 'l' || wallType == 'r') {
    // left or right wall, need to check top or bottom ECB vertex too
    const PointSweepResult sameResult =
        pointSweepingCheck(ecb1, ecbp, same, wall, wallType, wallIndex,
                           wallTopOrRight, wallBottomOrLeft, xOrY);
    const double other =
        (wallType == 'l' && wallAngle < js_pi() / 2) ||
        (wallType == 'r' && wallAngle > js_pi() / 2) ? 0 : 2;
    const PointSweepResult otherResult =
        pointSweepingCheck(ecb1, ecbp, other, wall, wallType, wallIndex,
                           wallTopOrRight, wallBottomOrLeft, xOrY);
    // pickSmallestSweep([sameResult, otherResult])
    {
      const bool present[2] = { sameResult.present, otherResult.present };
      const double sweep[2] = { sameResult.sweep, otherResult.sweep };
      const int pick = pickSmallestSweep(present, sweep, 2);
      result = pick == 0 ? sameResult : pick == 1 ? otherResult : psr_null();
    }
  } else if (wallType == 'c') { // for ceilings, need to check side ECB vertex too
    const PointSweepResult topResult =
        pointSweepingCheck(ecb1, ecbp, 2, wall, wallType, wallIndex,
                           wallTopOrRight, wallBottomOrLeft, xOrY);
    const double side = wallAngle < js_pi() / 2 ? 3 : 1;
    const PointSweepResult sideResult =
        pointSweepingCheck(ecb1, ecbp, side, wall, wallType, wallIndex,
                           wallTopOrRight, wallBottomOrLeft, xOrY);
    {
      const bool present[2] = { topResult.present, sideResult.present };
      const double sweep[2] = { topResult.sweep, sideResult.sweep };
      const int pick = pickSmallestSweep(present, sweep, 2);
      result = pick == 0 ? topResult : pick == 1 ? sideResult : psr_null();
    }
  } else { // can only collide grounds on the bottom ECB vertex
    result = pointSweepingCheck(ecb1, ecbp, same, wall, wallType, wallIndex,
                                wallTopOrRight, wallBottomOrLeft, xOrY);
  }

  return result;
}

static PointSweepResult pointSweepingCheck(ECB ecb1, ECB ecbp, double pt,
                                           Surface wall, char wallType,
                                           double wallIndex,
                                           Vec2D wallTopOrRight,
                                           Vec2D wallBottomOrLeft, XOrY xOrY) {
  PointSweepResult result = psr_null();
  const int ipt = (int)pt;

  if (isOutside(ecb1.pt[ipt], wallTopOrRight, wallBottomOrLeft, wallType) &&
      !isOutside(ecbp.pt[ipt], wallTopOrRight, wallBottomOrLeft, wallType)) {
    // need to put wall first
    const double s = coordinateInterceptParameter(
        line2(wall.p0, wall.p1), line2(ecb1.pt[ipt], ecbp.pt[ipt]));
    if (!(isnan(s) || s == INFINITY || s > 1 || s < 0)) {
      const Vec2D intersection =
          vec2d((1 - s) * ecb1.pt[ipt].x + s * ecbp.pt[ipt].x,
                (1 - s) * ecb1.pt[ipt].y + s * ecbp.pt[ipt].y);
      if (getXOrYCoord(intersection, xOrY) <= getXOrYCoord(wallTopOrRight, xOrY) &&
          getXOrYCoord(intersection, xOrY) >= getXOrYCoord(wallBottomOrLeft, xOrY)) {
        result.present = true;
        result.sweep = s;
        result.surface = wall;
        result.type = wallType;
        result.index = wallIndex;
        result.pt = pt;
      }
    }
  }

  return result;
}

// second: edge sweeping functions

// returns [t, s] | null
typedef struct { bool present; double t, s; } LineSweepResult;

static LineSweepResult lineSweepParameters(Line2 line1, Line2 line2v,
                                           bool flip) {
  LineSweepResult out; out.present = false; out.t = 0; out.s = 0;
  double sign = 1;
  if (flip) {
    sign = -1;
  }
  const double x1 = line1.a.x;
  const double x2 = line1.b.x;
  const double x3 = line2v.a.x;
  const double x4 = line2v.b.x;
  const double y1 = line1.a.y;
  const double y2 = line1.b.y;
  const double y3 = line2v.a.y;
  const double y4 = line2v.b.y;

  const double a0 = x2 * y1 - x1 * y2;
  const double a1 = x4 * y1 - 2 * x2 * y1 + 2 * x1 * y2 - x3 * y2 + x2 * y3 - x1 * y4;
  const double a2 = x2 * y1 - x4 * y1 - x1 * y2 + x3 * y2 - x2 * y3 + x4 * y3 + x1 * y4 - x3 * y4;

  // s satisfies the equation:   a0 + a1*s + a2*s^2 = 0
  const MaybeNum sM = solveQuadraticEquation(a0, a1, a2, sign);

  if (!sM.present || isnan(sM.v) || sM.v == INFINITY || sM.v < 0 || sM.v > 1) {
    return out; // no real solution
  } else {
    const double s = sM.v;
    const double t = (s * (x1 - x3) - x1) / (x2 - x1 + s * (x1 - x2 - x3 + x4));

    if (isnan(t) || t == INFINITY || t < 0 || t > 1) {
      return out;
    } else {
      out.present = true;
      out.t = t;
      out.s = s;
      return out;
    }
  }
}

static double getAngularParameter(double t, double same, double other);
static EdgeSweepResult edgeSweepingCheck(ECB ecb1, ECB ecbp, double same,
                                         double other, bool counterclockwise,
                                         Vec2D corner, DamageType damageType);

// finds whether the ECB impacted a surface on one of its edges
static EdgeSweepResult runEdgeSweep(ECB ecb1, ECB ecbp, double same,
                                    char wallType, Vec2D wallLeft,
                                    Vec2D wallRight, Vec2D wallBottomOrLeft,
                                    Vec2D wallTopOrRight, XOrY xOrY,
                                    DamageType damageType) {
  double other = 0;            // other ECB point
  bool counterclockwise = true; // whether (same -> other) is counterclockwise

  bool cornerPresent = false;
  Vec2D corner = vec2d(0, 0);
  bool otherCornerPresent = false;
  Vec2D otherCorner = vec2d(0, 0);

  EdgeSweepResult edgeSweepResult = esr_null();
  EdgeSweepResult otherEdgeSweepResult = esr_null();

  const bool flip = (wallType == 'r' || wallType == 'c') ? false : true;

  const int isame = (int)same;

  // case 1
  if (getXOrYCoord(ecb1.pt[isame], xOrY) > getXOrYCoord(wallTopOrRight, xOrY)) {
    counterclockwise = !flip;
    other = turn(same, counterclockwise);
    if (getXOrYCoord(ecbp.pt[(int)other], xOrY) < getXOrYCoord(wallTopOrRight, xOrY)) {
      cornerPresent = true;
      corner = wallTopOrRight;
    }
  }
  // case 2
  else if (getXOrYCoord(ecb1.pt[isame], xOrY) < getXOrYCoord(wallBottomOrLeft, xOrY)) {
    counterclockwise = flip;
    other = turn(same, counterclockwise);
    if (getXOrYCoord(ecbp.pt[(int)other], xOrY) > getXOrYCoord(wallBottomOrLeft, xOrY)) {
      cornerPresent = true;
      corner = wallBottomOrLeft;
    }
  }

  if (cornerPresent) {
    // the relevant ECB edge, that might collide with the corner, is the edge
    // between ECB points 'same' and 'other'
    char interiorECBside = 'l';
    if (counterclockwise == false) {
      interiorECBside = 'r';
    }

    if (!isOutside(corner, ecbp.pt[isame], ecbp.pt[(int)other], interiorECBside) &&
        isOutside(corner, ecb1.pt[isame], ecb1.pt[(int)other], interiorECBside)) {
      edgeSweepResult = edgeSweepingCheck(ecb1, ecbp, same, other,
                                          counterclockwise, corner, damageType);
    }
  }

  if ((wallType == 'l' || wallType == 'r') && (other == 0)) {
    // if dealing with a wall, we might also want to check the top ECB point
    // for collision if we aren't already doing so
    bool otherCounterclockwise = false; // whether (same -> top) is counterclockwise
    otherCorner = wallRight;
    otherCornerPresent = true;
    if (wallType == 'l') {
      otherCounterclockwise = true;
      otherCorner = wallLeft;
    }

    char otherInteriorECBside = 'l';
    if (otherCounterclockwise == false) {
      otherInteriorECBside = 'r';
    }

    if (!isOutside(otherCorner, ecbp.pt[isame], ecbp.pt[2], otherInteriorECBside) &&
        isOutside(otherCorner, ecb1.pt[isame], ecb1.pt[2], otherInteriorECBside)) {
      otherEdgeSweepResult = edgeSweepingCheck(ecb1, ecbp, same, 2,
                                               otherCounterclockwise,
                                               otherCorner, damageType);
    }
  }
  (void)otherCornerPresent;

  // return pickSmallestSweep([edgeSweepResult, otherEdgeSweepResult]);
  {
    const bool present[2] = { edgeSweepResult.present, otherEdgeSweepResult.present };
    const double sweep[2] = { edgeSweepResult.sweep, otherEdgeSweepResult.sweep };
    const int pick = pickSmallestSweep(present, sweep, 2);
    return pick == 0 ? edgeSweepResult
         : pick == 1 ? otherEdgeSweepResult : esr_null();
  }
}

// determines whether the given ECB edge (same--other) has collided with the
// corner, using the lineSweepParameters function
static EdgeSweepResult edgeSweepingCheck(ECB ecb1, ECB ecbp, double same,
                                         double other, bool counterclockwise,
                                         Vec2D corner, DamageType damageType) {
  EdgeSweepResult output = esr_null();
  const int isame = (int)same;
  const int iother = (int)other;

  // the relevant ECB edge, that might collide with the corner, is the edge
  // between ECB points 'same' and 'other'
  char interiorECBside = 'l';
  if (counterclockwise == false) {
    interiorECBside = 'r';
  }

  if (!isOutside(corner, ecbp.pt[isame], ecbp.pt[iother], interiorECBside) &&
      isOutside(corner, ecb1.pt[isame], ecb1.pt[iother], interiorECBside)) {

    // we sweep a line, starting from the relevant ECB1 edge, and ending at
    // the relevant ECBp edge, and figure out where this would intersect the
    // corner; first we recenter everything around the corner

    const Line2 recenteredECB1Edge =
        line2(vec2d(ecb1.pt[isame].x - corner.x, ecb1.pt[isame].y - corner.y),
              vec2d(ecb1.pt[iother].x - corner.x, ecb1.pt[iother].y - corner.y));
    const Line2 recenteredECBpEdge =
        line2(vec2d(ecbp.pt[isame].x - corner.x, ecbp.pt[isame].y - corner.y),
              vec2d(ecbp.pt[iother].x - corner.x, ecbp.pt[iother].y - corner.y));

    // a minus sign is required precisely in the counterclockwise case
    const LineSweepResult lineSweepResult =
        lineSweepParameters(recenteredECB1Edge, recenteredECBpEdge,
                            counterclockwise);

    if (lineSweepResult.present) {
      const double t = lineSweepResult.t;
      const double s = lineSweepResult.s;
      const double angularParameter = getAngularParameter(t, same, other);
      output.present = true;
      output.corner = corner;
      output.sweep = s;
      output.angular = angularParameter;
      output.damageType = damageType;
    }
  }

  return output;
}

// ---------------------------------------------------------------------------
// main collision detection routine

// this function finds the first collision that happens as the old ECB moves
// to the projected ECB
CollisionDatum findCollision(ECB ecb1, ECB ecbp,
                             LabelledSurface labelledSurface) {
  // STANDING ASSUMPTIONS: see the JS source (grounds collide bottom point,
  // ceilings top/side, walls push out horizontally, etc.)

  const Surface wall = labelledSurface.surface;
  const char wallType = labelledSurface.type;
  const double wallIndex = labelledSurface.index;
  // const damageType = wall[2] !== undefined ? wall[2].damageType : null;
  DamageType damageType;
  if (wall.hasProps) {
    damageType = wall.propsHasDamageTypeKey ? wall.propsDamageType
                                            : damage_undef();
  } else {
    damageType = damage_null();
  }

  // start defining useful constants/variables
  const SurfaceGeom wallGeom = surface_geom(&wall);
  const Vec2D wallTop = extremePoint(wallGeom, 't');
  const Vec2D wallBottom = extremePoint(wallGeom, 'b');
  const Vec2D wallLeft = extremePoint(wallGeom, 'l');
  const Vec2D wallRight = extremePoint(wallGeom, 'r');

  // right wall by default
  Vec2D wallTopOrRight = wallTop;
  Vec2D wallBottomOrLeft = wallBottom;
  double same = 3;
  XOrY xOrY = 'y';
  bool isPlatform = false;

  switch (wallType) {
    case 'l': // left wall
      same = 1;
      break;
    case 'p': // platform
      isPlatform = true;
      // JS case fallthrough is intentional upstream
      __attribute__((fallthrough));
    case 'g': // ground
      same = 0;
      wallTopOrRight = wallRight;
      wallBottomOrLeft = wallLeft;
      xOrY = 'x';
      break;
    case 'c': // ceiling
      same = 2;
      wallTopOrRight = wallRight;
      wallBottomOrLeft = wallLeft;
      xOrY = 'x';
      break;
    default: // right wall by default
      break;
  }

  CollisionDatum out;
  out.kind = CD_NULL;
  out.ps = psr_null();
  out.es = esr_null();

  // first check if player ECB was even near the wall
  if ((ecbp.pt[0].y > wallTop.y && ecb1.pt[0].y > wallTop.y)      // stayed above
      || (ecbp.pt[2].y < wallBottom.y && ecb1.pt[2].y < wallBottom.y) // stayed below
      || (ecbp.pt[3].x > wallRight.x && ecb1.pt[3].x > wallRight.x)   // stayed right
      || (ecbp.pt[1].x < wallLeft.x && ecb1.pt[1].x < wallLeft.x)     // stayed left
  ) {
    return out; // null
  } else {

    // if the surface is a platform, and the bottom ECB point is below the
    // platform, we shouldn't do anything
    if (isPlatform) {
      if (!isOutside(ecb1.pt[(int)same], wallTopOrRight, wallBottomOrLeft,
                     wallType)) {
        return out; // null
      }
    }

    const EdgeSweepResult closestEdgeCollision =
        runEdgeSweep(ecb1, ecbp, same, wallType, wallLeft, wallRight,
                     wallBottomOrLeft, wallTopOrRight, xOrY, damageType);
    const PointSweepResult closestPointCollision =
        runPointSweep(ecb1, ecbp, same, wall, wallType, wallIndex,
                      wallBottomOrLeft, wallTopOrRight, xOrY);

    // if we have only one collision type (point/edge), take that one;
    // otherwise choose the collision with smallest sweeping parameter
    if (!closestEdgeCollision.present) {
      if (closestPointCollision.present) {
        out.kind = CD_SURFACE;
        out.ps = closestPointCollision;
      }
    } else if (!closestPointCollision.present) {
      out.kind = CD_CORNER;
      out.es = closestEdgeCollision;
    } else if (closestEdgeCollision.sweep > closestPointCollision.sweep) {
      out.kind = CD_SURFACE;
      out.ps = closestPointCollision;
    } else {
      out.kind = CD_CORNER;
      out.es = closestEdgeCollision;
    }

    return out;
  }
}

// ---------------------------------------------------------------------------
// some helper functions to return the closest collision (collision with
// smallest sweeping parameter)

// TouchingDatum = null | { sweep, object : {surface...} | {corner...} }
typedef struct {
  bool present;
  double sweep;
  CollisionKind kind; // CD_SURFACE | CD_CORNER
  // object, surface variant: { kind, surface, type, index, pt }
  Surface surface;
  char type;
  double index;
  double pt;
  // object, corner variant: { kind, corner, angular, damageType }
  Vec2D corner;
  double angular;
  DamageType damageType;
} TouchingDatum;

static TouchingDatum td_null(void) {
  TouchingDatum t; memset(&t, 0, sizeof t); t.present = false; return t;
}

// this function finds the first (non-ignored) collision as the ECB1 moves
// to the ECBp
static TouchingDatum findClosestCollision(ECB ecb1, ECB ecbp,
                                          const LabelledSurfaceList *labelledSurfaces) {
  // touchingData = [null]; then push one entry per non-null collision, in
  // surface order; pickSmallestSweep keeps the FIRST smallest (strict <).
  TouchingDatum best = td_null();
  for (int i = 0; i < labelledSurfaces->count; i++) {
    const CollisionDatum collisionDatum =
        findCollision(ecb1, ecbp, labelledSurfaces->items[i]);
    TouchingDatum cand = td_null();
    if (collisionDatum.kind == CD_SURFACE) {
      cand.present = true;
      cand.sweep = collisionDatum.ps.sweep;
      cand.kind = CD_SURFACE;
      cand.surface = collisionDatum.ps.surface;
      cand.type = collisionDatum.ps.type;
      cand.index = collisionDatum.ps.index;
      cand.pt = collisionDatum.ps.pt;
    } else if (collisionDatum.kind == CD_CORNER) {
      cand.present = true;
      cand.sweep = collisionDatum.es.sweep;
      cand.kind = CD_CORNER;
      cand.corner = collisionDatum.es.corner;
      cand.angular = collisionDatum.es.angular;
      cand.damageType = collisionDatum.es.damageType;
    }
    if (cand.present && (!best.present || cand.sweep < best.sweep)) {
      best = cand;
    }
  }
  return best;
}

// ---------------------------------------------------------------------------
// ECB sliding

// CollisionObject (internal): surface | corner variant; damageType is never
// read off slide objects by the routine, so it is not carried.
typedef struct {
  CollisionKind kind;
  Surface surface;
  char type;
  double pt;
  double index;
  Vec2D corner;
  double angular;
} CollisionObject;

// Sliding = { type : null | 'l' | 'r' | 'c', angular : null | number }
typedef struct {
  char type; // 0 = null
  bool angularIsNull;
  double angular;
} Sliding;

// ECBDatum = { ecb, touching, squashed }
typedef struct {
  ECB ecb;
  SimpleTouchingDatum touching;
  bool squashed;
} ECBDatum;

// SlideDatum: end | transfer | squash | continue
typedef enum { SL_END, SL_TRANSFER, SL_SQUASH, SL_CONTINUE } SlideEvent;
typedef struct {
  SlideEvent event;
  ECB finalECB;                 // end
  SimpleTouchingDatum touching; // end
  ECB midECB;                   // transfer / squash
  ECB tgtECB;                   // squash
  CollisionObject object;       // transfer / squash
} SlideDatum;

static SimpleTouchingDatum std_null(void) {
  SimpleTouchingDatum s; memset(&s, 0, sizeof s); s.present = false;
  s.damageType = damage_absent(); return s;
}

static SlideDatum slideECB(ECB srcECB, ECB tgtECB,
                           const LabelledSurfaceList *labelledSurfaces,
                           Sliding slidingAgainst,
                           PlayerStatusInfo playerStatusInfo);
static ECB updateECBp(ECB startECB, ECB endECB, ECB ecbp, char slidingType,
                      double pt);
static void findNextTargetFromSurface(ECB srcECB, ECB ecbp, Surface wall,
                                      char wallType, double pt,
                                      ECB *outECB, bool *outFinal);
static void findNextTargetFromCorner(ECB srcECB, ECB ecbp, Vec2D corner,
                                     double angularParameter,
                                     ECB *outECB, bool *outFinal);
static void agreeOnTargetECB(ECB srcECB, ECB fstTgtECB, ECB sndTgtECB,
                             ECB ecbp, double pt, bool grounded,
                             ECB *outECB, bool *outAbort);

static ECBDatum runSlideRoutine(ECB srcECB, ECB tgtECB, ECB ecbp,
                                PlayerStatusInfo playerStatusInfo,
                                const LabelledSurfaceList *labelledSurfaces,
                                SimpleTouchingDatum oldTouchingDatum,
                                Sliding slidingAgainst, bool squashed,
                                bool final, double recursionCounter);

static ECBDatum resolveECB(ECB ecb1, ECB ecbp,
                           PlayerStatusInfo playerStatusInfo,
                           const LabelledSurfaceList *labelledSurfaces) {
  Sliding sliding; sliding.type = 0; sliding.angularIsNull = true;
  sliding.angular = 0;
  return runSlideRoutine(ecb1, ecbp, ecbp, playerStatusInfo, labelledSurfaces,
                         std_null(), sliding, false, true, 0);
}

static ECBDatum runSlideRoutine(ECB srcECB, ECB tgtECB, ECB ecbp,
                                PlayerStatusInfo playerStatusInfo,
                                const LabelledSurfaceList *labelledSurfaces,
                                SimpleTouchingDatum oldTouchingDatum,
                                Sliding slidingAgainst, bool squashed,
                                bool final, double recursionCounter) {
  ECBDatum output;
  if (recursionCounter > maxRecursion) {
    // console.log("'runSlideRoutine': excessive recursion, aborting.");
    drawECB(srcECB, "#286ee0");
    drawECB(tgtECB, "#f49930");
    drawECB(ecbp, "#fff9ad");
    output.ecb = srcECB;
    output.touching = std_null();
    output.squashed = squashed;
  } else {
    const SlideDatum slideDatum =
        slideECB(srcECB, tgtECB, labelledSurfaces, slidingAgainst,
                 playerStatusInfo);
    ECB newECBp = ecbp;

    if (slideDatum.event == SL_END) {
      output.ecb = slideDatum.finalECB;
      output.touching = slideDatum.touching;
      output.squashed = squashed;
    } else if (slideDatum.event == SL_CONTINUE) {
      if (final) {
        output.ecb = tgtECB;
        output.touching = oldTouchingDatum;
        output.squashed = squashed;
      } else {
        newECBp = updateECBp(srcECB, tgtECB, ecbp, slidingAgainst.type, 0);
        output = runSlideRoutine(tgtECB, newECBp, newECBp, playerStatusInfo,
                                 labelledSurfaces, oldTouchingDatum,
                                 slidingAgainst, squashed, true,
                                 recursionCounter + 1);
      }
    } else { // transfer || squash
      const ECB newSrcECB = slideDatum.midECB;
      const CollisionObject slideObject = slideDatum.object;

      SimpleTouchingDatum newTouchingDatum = std_null();
      double angular = 0;
      bool newFinal = false;
      ECB newTgtECB;
      char newSlidingType = 0; // null
      double same = 0;
      double other = 0;
      (void)other;

      if (slideObject.kind == CD_SURFACE) {
        const Surface surface = slideObject.surface;
        const char surfaceType = slideObject.type;
        if (surfaceType == 'l' || surfaceType == 'r' || surfaceType == 'c') {
          newSlidingType = surfaceType;
        }
        same = surfaceType == 'l' ? 1 : 3;
        angular = slideObject.pt;
        newECBp = updateECBp(srcECB, slideDatum.midECB, ecbp, newSlidingType,
                             same);
        newTouchingDatum.present = true;
        newTouchingDatum.kind = CD_SURFACE;
        newTouchingDatum.type = surfaceType;
        newTouchingDatum.index = slideObject.index;
        newTouchingDatum.pt = angular;
        newTouchingDatum.damageType = damage_absent();
        findNextTargetFromSurface(newSrcECB, newECBp, surface, surfaceType,
                                  angular, &newTgtECB, &newFinal);
      } else {
        const Vec2D corner = slideObject.corner;
        angular = slideObject.angular;
        if (angular < 2 && angular > 0) {
          newSlidingType = 'l';
        } else if (angular > 2) {
          newSlidingType = 'r';
        }
        const SameOther so = getSameAndOther(angular);
        same = so.same;
        other = so.other;
        newECBp = updateECBp(srcECB, slideDatum.midECB, ecbp, newSlidingType,
                             same);
        findNextTargetFromCorner(newSrcECB, newECBp, corner, angular,
                                 &newTgtECB, &newFinal);
        newTouchingDatum.present = true;
        newTouchingDatum.kind = CD_CORNER;
        newTouchingDatum.angular = angular;
        newTouchingDatum.damageType = damage_absent();
      }

      if (slideDatum.event == SL_TRANSFER) {
        Sliding newSliding;
        newSliding.type = newSlidingType;
        newSliding.angularIsNull = false;
        newSliding.angular = angular;
        output = runSlideRoutine(newSrcECB, newTgtECB, newECBp,
                                 playerStatusInfo, labelledSurfaces,
                                 newTouchingDatum, newSliding, squashed,
                                 newFinal, recursionCounter + 1);
      } else {
        const ECB otherTgtECB = slideDatum.tgtECB;
        ECB squashTgtECB;
        bool abort = false;
        agreeOnTargetECB(newSrcECB, otherTgtECB, newTgtECB, newECBp, same,
                         playerStatusInfo.grounded, &squashTgtECB, &abort);
        if (abort) {
          output.ecb = srcECB;
          output.touching = oldTouchingDatum;
          output.squashed = squashed;
        } else {
          Sliding newSliding;
          newSliding.type = newSlidingType;
          newSliding.angularIsNull = false;
          newSliding.angular = angular;
          output = runSlideRoutine(newSrcECB, squashTgtECB, newECBp,
                                   playerStatusInfo, labelledSurfaces,
                                   newTouchingDatum, newSliding, true,
                                   newFinal && final, recursionCounter + 1);
        }
      }
    }
  }
  return output;
}

// this function figures out if we can move the ECB, from the source ECB to
// the target ECB
static SlideDatum slideECB(ECB srcECB, ECB tgtECB,
                           const LabelledSurfaceList *labelledSurfaces,
                           Sliding slidingAgainst,
                           PlayerStatusInfo playerStatusInfo) {
  SlideDatum output;
  memset(&output, 0, sizeof output);

  // figure out whether a collision occurred while moving srcECB -> tgtECB
  const TouchingDatum touchingDatum =
      findClosestCollision(srcECB, tgtECB, labelledSurfaces);

  if (!touchingDatum.present) {
    output.event = SL_CONTINUE;
  } else {
    const double s = touchingDatum.sweep;
    // to account for floating point errors
    const double r = js_max(0, s - additionalOffset / 10);
    const ECB midECB = interpolateECB(srcECB, tgtECB, r);

    // collisionObject = touchingDatum.object
    // ------------------------------------------------------------------
    // damaging objects cause premature end to sliding

    DamageType damageType = damage_null();
    if (!playerStatusInfo.immune) {
      if (touchingDatum.kind == CD_SURFACE) {
        // surfaceProperties = collisionObject.surface[2]
        if (touchingDatum.surface.hasProps) {
          // (surfaceProperties !== null && !== undefined) — hasProps means
          // wall[2] !== undefined; a literal null third element does not
          // occur in the domain (marshaller enforces)
          damageType = touchingDatum.surface.propsHasDamageTypeKey
                           ? touchingDatum.surface.propsDamageType
                           : damage_undef();
        }
      } else if (touchingDatum.kind == CD_CORNER) {
        damageType = touchingDatum.damageType;
      }
    }

    if (damageType.tag == DT_STR) { // damageType !== null && !== undefined
      if (touchingDatum.kind == CD_SURFACE) {
        output.event = SL_END;
        output.finalECB = midECB;
        output.touching.present = true;
        output.touching.kind = CD_SURFACE;
        output.touching.type = touchingDatum.type;
        output.touching.index = touchingDatum.index;
        output.touching.pt = touchingDatum.pt;
        output.touching.damageType = damageType;
      } else {
        output.event = SL_END;
        output.finalECB = midECB;
        output.touching.present = true;
        output.touching.kind = CD_CORNER;
        output.touching.angular = touchingDatum.angular;
        output.touching.damageType = damageType;
      }
    }
    // ------------------------------------------------------------------
    else if (slidingAgainst.type == 0) { // slidingAgainst.type === null
      if (touchingDatum.kind == CD_SURFACE) {
        if (touchingDatum.type == 'g' || touchingDatum.type == 'p') {
          // sliding interrupted by landing
          output.event = SL_END;
          output.finalECB = midECB;
          output.touching.present = true;
          output.touching.kind = CD_SURFACE;
          output.touching.type = touchingDatum.type;
          output.touching.index = touchingDatum.index;
          output.touching.pt = touchingDatum.pt;
          output.touching.damageType = damage_absent();
        } else {
          // beginning slide on surface
          output.event = SL_TRANSFER;
          output.midECB = midECB;
          output.object.kind = CD_SURFACE;
          output.object.surface = touchingDatum.surface;
          output.object.type = touchingDatum.type;
          output.object.pt = touchingDatum.pt;
          output.object.index = touchingDatum.index;
        }
      } else {
        // beginning slide on corner
        output.event = SL_TRANSFER;
        output.midECB = midECB;
        output.object.kind = CD_CORNER;
        output.object.corner = touchingDatum.corner;
        output.object.angular = touchingDatum.angular;
      }
    } else {
      const char slidingType = slidingAgainst.type;
      if (touchingDatum.kind == CD_SURFACE) {
        const char surfaceType = touchingDatum.type;
        if (surfaceType == slidingType) {
          // transferring slide to new surface
          output.event = SL_TRANSFER;
          output.midECB = midECB;
          output.object.kind = CD_SURFACE;
          output.object.surface = touchingDatum.surface;
          output.object.type = touchingDatum.type;
          output.object.pt = touchingDatum.pt;
          output.object.index = touchingDatum.index;
        } else if (slidingType == 'c' || surfaceType == 'c' ||
                   surfaceType == 'g' || surfaceType == 'p') {
          // no way to continue when one of the involved surfaces is a
          // ceiling or a ground
          output.event = SL_END;
          output.finalECB = midECB;
          output.touching.present = true;
          output.touching.kind = CD_SURFACE;
          output.touching.type = touchingDatum.type;
          output.touching.index = touchingDatum.index;
          output.touching.pt = touchingDatum.pt;
          output.touching.damageType = damage_absent();
        } else {
          // beginning ECB squashing (conflicting horizontal surface pushout)
          output.event = SL_SQUASH;
          output.midECB = midECB;
          output.tgtECB = tgtECB;
          output.object.kind = CD_SURFACE;
          output.object.surface = touchingDatum.surface;
          output.object.type = touchingDatum.type;
          output.object.pt = touchingDatum.pt;
          output.object.index = touchingDatum.index;
          // (JS also sets a `pt` key on the slide datum here; it is never
          // read anywhere — not carried)
        }
      } else {
        const double angularParameter = touchingDatum.angular;
        const double side = getSameAndOther(angularParameter).same;
        if (slidingType == 'c') {
          // interrupting sliding because of conflicting corner collision
          output.event = SL_END;
          output.finalECB = midECB;
          output.touching.present = true;
          output.touching.kind = CD_CORNER;
          output.touching.angular = angularParameter;
          output.touching.damageType = damage_absent();
        } else if (slidingType == 0
                   || (side == 3 && slidingType == 'r')
                   || (side == 1 && slidingType == 'l')) {
          // transferring slide to new corner
          output.event = SL_TRANSFER;
          output.midECB = midECB;
          output.object.kind = CD_CORNER;
          output.object.corner = touchingDatum.corner;
          output.object.angular = angularParameter;
        } else {
          // beginning ECB squashing (conflicting horizontal corner pushout)
          output.event = SL_SQUASH;
          output.midECB = midECB;
          output.tgtECB = tgtECB;
          output.object.kind = CD_CORNER;
          output.object.corner = touchingDatum.corner;
          output.object.angular = angularParameter;
          // (JS also sets a `side` key here; never read — not carried)
        }
      }
    }
  }
  return output;
}

static void findNextTargetFromSurface(ECB srcECB, ECB ecbp, Surface wall,
                                      char wallType, double pt,
                                      ECB *outECB, bool *outFinal) {
  Vec2D wallForward;
  double s = 1;
  ECB tgtECB = ecbp;
  double pushout = 0;
  bool final = true;
  const int ipt = (int)pt;

  const double sign = (wallType == 'l' || wallType == 'c') ? -1 : 1;
  const double additionalPushout = sign * additionalOffset;
  const XOrY xOrY = (wallType == 'l' || wallType == 'r') ? 'x' : 'y';

  const SurfaceGeom wallGeom = surface_geom(&wall);

  if (wallType == 'c') {
    const Vec2D wallLeft = extremePoint(wallGeom, 'l');
    const Vec2D wallRight = extremePoint(wallGeom, 'r');
    if (ecbp.pt[ipt].x <= wallRight.x && ecbp.pt[ipt].x >= wallLeft.x) {
      const Vec2D intercept = coordinateIntercept(
          vLineThrough(ecbp.pt[ipt]), line2(wall.p0, wall.p1));
      pushout = intercept.y - ecbp.pt[ipt].y;
    } else {
      wallForward = ecbp.pt[ipt].x < srcECB.pt[ipt].x ? wallLeft : wallRight;
      s = (wallForward.x - srcECB.pt[ipt].x) / (ecbp.pt[ipt].x - srcECB.pt[ipt].x);
      s = js_min(js_max(s, 0), 1);
      tgtECB = interpolateECB(srcECB, ecbp, s);
      pushout = wallForward.y - tgtECB.pt[ipt].y;
    }
  } else {
    const Vec2D wallBottom = extremePoint(wallGeom, 'b');
    const Vec2D wallTop = extremePoint(wallGeom, 't');
    if (ecbp.pt[ipt].y <= wallTop.y && ecbp.pt[ipt].y >= wallBottom.y) {
      const Vec2D intercept = coordinateIntercept(
          hLineThrough(ecbp.pt[ipt]), line2(wall.p0, wall.p1));
      pushout = intercept.x - ecbp.pt[ipt].x;
    } else {
      wallForward = ecbp.pt[ipt].y < srcECB.pt[ipt].y ? wallBottom : wallTop;
      s = (wallForward.y - srcECB.pt[ipt].y) / (ecbp.pt[ipt].y - srcECB.pt[ipt].y);
      s = js_min(js_max(s, 0), 1);
      tgtECB = interpolateECB(srcECB, ecbp, s);
      pushout = wallForward.x - tgtECB.pt[ipt].x;
    }
  }

  if (s < 1 || sign * pushout < 0) {
    final = false;
  }

  tgtECB = moveECB(tgtECB, putXOrYCoord(pushout + additionalPushout, xOrY));

  drawECB(ecbp, "#8f54ff");
  drawECB(tgtECB, "#35f4ab");

  *outECB = tgtECB;
  *outFinal = final;
}

static void findNextTargetFromCorner(ECB srcECB, ECB ecbp, Vec2D corner,
                                     double angularParameter,
                                     ECB *outECB, bool *outFinal) {
  const SameOther so = getSameAndOther(angularParameter);
  const double same = so.same;
  const double other = so.other;
  const int isame = (int)same;
  const int iother = (int)other;
  const double LRSign = (same == 1) ? -1 : 1;
  const double UDSign = (other == 2) ? -1 : 1;
  const double additionalPushout = LRSign * additionalOffset;

  ECB tgtECB = ecbp;
  double s = 1;
  double pushout = 0;
  bool final = true;

  if (UDSign * ecbp.pt[isame].y < UDSign * corner.y) {
    s = (corner.y - srcECB.pt[isame].y) / (ecbp.pt[isame].y - srcECB.pt[isame].y);
    s = js_min(js_max(s, 0), 1);
    tgtECB = interpolateECB(srcECB, ecbp, s);
    pushout = corner.x - tgtECB.pt[isame].x;
  } else if (UDSign * ecbp.pt[iother].y < UDSign * corner.y) {
    const Vec2D intercept = coordinateIntercept(
        hLineThrough(corner), line2(ecbp.pt[isame], ecbp.pt[iother]));
    pushout = corner.x - intercept.x + additionalPushout;
  } else {
    s = (corner.y - srcECB.pt[iother].y) / (ecbp.pt[iother].y - srcECB.pt[iother].y);
    s = js_min(js_max(s, 0), 1);
    tgtECB = interpolateECB(srcECB, ecbp, s);
    pushout = corner.x - tgtECB.pt[iother].x;
  }

  if (s < 1 || LRSign * pushout < 0) {
    final = false;
  }

  tgtECB = moveECB(tgtECB, putXOrYCoord(pushout + additionalPushout, 'x'));

  drawECB(ecbp, "#1098c9");
  drawECB(tgtECB, "#5cbc12");

  *outECB = tgtECB;
  *outFinal = final;
}

static ECB updateECBp(ECB startECB, ECB endECB, ECB ecbp, char slidingType,
                      double pt) {
  if (slidingType == 0) { // slidingType === null
    return ecbp;
  } else {
    XOrY xOrY = (slidingType == 'l' || slidingType == 'r') ? 'y' : 'x';
    double t;
    const int ipt = (int)pt;
    if (getXOrYCoord(ecbp.pt[ipt], xOrY) - getXOrYCoord(startECB.pt[ipt], xOrY) == 0) {
      xOrY = xOrY == 'x' ? 'y' : 'x';
      if (getXOrYCoord(ecbp.pt[ipt], xOrY) - getXOrYCoord(startECB.pt[ipt], xOrY) == 0) {
        t = 1;
      } else {
        t = (getXOrYCoord(endECB.pt[ipt], xOrY) - getXOrYCoord(startECB.pt[ipt], xOrY))
          / (getXOrYCoord(ecbp.pt[ipt], xOrY) - getXOrYCoord(startECB.pt[ipt], xOrY));
      }
    } else {
      t = (getXOrYCoord(endECB.pt[ipt], xOrY) - getXOrYCoord(startECB.pt[ipt], xOrY))
        / (getXOrYCoord(ecbp.pt[ipt], xOrY) - getXOrYCoord(startECB.pt[ipt], xOrY));
    }

    ECB midECB;
    if (t <= 0) {
      midECB = startECB;
    } else if (t >= 1) {
      midECB = ecbp;
    } else {
      midECB = interpolateECB(startECB, ecbp, t);
    }
    ECB out;
    out.pt[0] = ml_add(ecbp.pt[0], ml_subtract(endECB.pt[0], midECB.pt[0]));
    out.pt[1] = ml_add(ecbp.pt[1], ml_subtract(endECB.pt[1], midECB.pt[1]));
    out.pt[2] = ml_add(ecbp.pt[2], ml_subtract(endECB.pt[2], midECB.pt[2]));
    out.pt[3] = ml_add(ecbp.pt[3], ml_subtract(endECB.pt[3], midECB.pt[3]));
    return out;
  }
}

// this function gets called when two walls (or corners) are trying to push
// horizontally in opposite directions; it computes a squashed ECB that will
// fit in between the two objects that are squeezing it
static void agreeOnTargetECB(ECB srcECB, ECB fstTgtECB, ECB sndTgtECB,
                             ECB ecbp, double pt, bool grounded,
                             ECB *outECB, bool *outAbort) {
  (void)ecbp; // JS parameter, never read in the body

  const double flipPt = pt == 1 ? 3 : 1;
  const int ipt = (int)pt;
  const int iflipPt = (int)flipPt;

  ECB closestTgtECB, furthestTgtECB;
  double same;
  if (js_abs(fstTgtECB.pt[ipt].y - srcECB.pt[ipt].y) <
      js_abs(sndTgtECB.pt[iflipPt].y - srcECB.pt[iflipPt].y)) {
    closestTgtECB = fstTgtECB;
    furthestTgtECB = sndTgtECB;
    same = pt;
  } else {
    closestTgtECB = sndTgtECB;
    furthestTgtECB = fstTgtECB;
    same = flipPt;
  }
  const double diff = same == 1 ? 3 : 1;
  const int isame = (int)same;
  const int idiff = (int)diff;

  ECB otherTgtECB;
  if (furthestTgtECB.pt[idiff].y == srcECB.pt[idiff].y) {
    otherTgtECB = furthestTgtECB;
  } else {
    const double t = (closestTgtECB.pt[isame].y - srcECB.pt[isame].y)
                   / (furthestTgtECB.pt[idiff].y - srcECB.pt[idiff].y);
    if (t <= 0) {
      otherTgtECB = srcECB;
    } else if (t >= 1) {
      otherTgtECB = furthestTgtECB;
    } else {
      otherTgtECB = interpolateECB(srcECB, furthestTgtECB, t);
    }
  }

  // const tgtECB = [Vec2D(0,0) x4]; // initialising
  ECB tgtECB;
  tgtECB.pt[0] = vec2d(0, 0);
  tgtECB.pt[1] = vec2d(0, 0);
  tgtECB.pt[2] = vec2d(0, 0);
  tgtECB.pt[3] = vec2d(0, 0);
  bool abort;
  double squashFactor = 1;

  const double sign = js_sign(closestTgtECB.pt[isame].x - closestTgtECB.pt[idiff].x);

  // ideally we would squash so the side points are otherTgtECB[same] and
  // closestTgtECB[diff]; not possible if too close together / moved past
  // each other
  if (js_abs(otherTgtECB.pt[isame].x - closestTgtECB.pt[idiff].x) > smallestECBWidth
      && js_sign(otherTgtECB.pt[isame].x - closestTgtECB.pt[idiff].x) == sign) {
    if (js_abs(otherTgtECB.pt[isame].x - closestTgtECB.pt[idiff].x) >
        js_abs(closestTgtECB.pt[isame].x - closestTgtECB.pt[idiff].x)) {
      abort = false;
      // console.log("'agreeOnTargetECB' warning: no squashing was required.");
      *outECB = closestTgtECB;
      *outAbort = abort;
      return;
    } else {
      abort = false;
      squashFactor = (otherTgtECB.pt[isame].x - closestTgtECB.pt[idiff].x)
                   / (closestTgtECB.pt[isame].x - closestTgtECB.pt[idiff].x);
      tgtECB.pt[isame] = vec2d(otherTgtECB.pt[isame].x - sign * additionalOffset,
                               otherTgtECB.pt[isame].y);
      tgtECB.pt[idiff] = vec2d(closestTgtECB.pt[idiff].x + sign * additionalOffset,
                               closestTgtECB.pt[idiff].y);
      tgtECB.pt[2].y = tgtECB.pt[isame].y
          + squashFactor * (closestTgtECB.pt[2].y - closestTgtECB.pt[isame].y);
      tgtECB.pt[0].y = grounded ? srcECB.pt[0].y
          : tgtECB.pt[isame].y
            + squashFactor * (closestTgtECB.pt[0].y - closestTgtECB.pt[isame].y);
      tgtECB.pt[2].x = (tgtECB.pt[1].x + tgtECB.pt[3].x) / 2;
      tgtECB.pt[0].x = (tgtECB.pt[1].x + tgtECB.pt[3].x) / 2;
      *outECB = tgtECB;
      *outAbort = abort;
      return;
    }
  } else {
    // can't directly squash, so we need to find the closest allowable height
    const Line2 sameLine = line2(srcECB.pt[isame], otherTgtECB.pt[isame]);
    const Line2 diffLine = line2(srcECB.pt[idiff], closestTgtECB.pt[idiff]);
    const Line2 offsetDiffLine =
        line2(ml_add(diffLine.a, vec2d(sign * smallestECBWidth, 0)),
              ml_add(diffLine.b, vec2d(sign * smallestECBWidth, 0)));
    const Vec2D intercept = coordinateIntercept(sameLine, offsetDiffLine);
    if (js_abs(closestTgtECB.pt[isame].y - srcECB.pt[isame].y) >=
        js_abs(intercept.y - srcECB.pt[isame].y)) {
      abort = true;
      tgtECB.pt[isame] = vec2d(intercept.x + sign * additionalOffset, intercept.y);
      tgtECB.pt[idiff] = vec2d(intercept.x - sign * smallestECBWidth
                               - sign * additionalOffset, intercept.y);
      squashFactor = (tgtECB.pt[isame].x - tgtECB.pt[idiff].x)
                   / (closestTgtECB.pt[isame].x - closestTgtECB.pt[idiff].x);
      tgtECB.pt[2].y = tgtECB.pt[isame].y
          + squashFactor * (closestTgtECB.pt[2].y - closestTgtECB.pt[isame].y);
      tgtECB.pt[0].y = grounded ? srcECB.pt[0].y
          : tgtECB.pt[isame].y
            + squashFactor * (closestTgtECB.pt[0].y - closestTgtECB.pt[isame].y);
      tgtECB.pt[2].x = (tgtECB.pt[1].x + tgtECB.pt[3].x) / 2;
      tgtECB.pt[0].x = (tgtECB.pt[1].x + tgtECB.pt[3].x) / 2;
      *outECB = tgtECB;
      *outAbort = abort;
      drawECB(tgtECB, "#f9482c");
      return;
    } else {
      abort = false;
      squashFactor = (otherTgtECB.pt[isame].x - closestTgtECB.pt[idiff].x
                      - 2 * sign * additionalOffset)
                   / (closestTgtECB.pt[isame].x - closestTgtECB.pt[idiff].x);
      if (squashFactor >= 1) {
        *outECB = closestTgtECB;
        *outAbort = abort;
        return;
      } else {
        tgtECB.pt[isame] = vec2d(otherTgtECB.pt[isame].x - sign * additionalOffset,
                                 otherTgtECB.pt[isame].y);
        tgtECB.pt[idiff] = vec2d(closestTgtECB.pt[idiff].x + sign * additionalOffset,
                                 closestTgtECB.pt[idiff].y);
        tgtECB.pt[2].y = tgtECB.pt[isame].y
            + squashFactor * (closestTgtECB.pt[2].y - closestTgtECB.pt[isame].y);
        tgtECB.pt[0].y = grounded ? srcECB.pt[0].y
            : tgtECB.pt[isame].y
              + squashFactor * (closestTgtECB.pt[0].y - closestTgtECB.pt[isame].y);
        tgtECB.pt[2].x = (tgtECB.pt[1].x + tgtECB.pt[3].x) / 2;
        tgtECB.pt[0].x = (tgtECB.pt[1].x + tgtECB.pt[3].x) / 2;
        *outECB = tgtECB;
        *outAbort = abort;
        return;
      }
    }
  }
}

// ---------------------------------------------------------------------------
// convert between angular parameters and "same/other" data

static double getAngularParameter(double t, double same, double other) {
  if (same == 3 && other == 0) {
    return ((1 - t) * 3 + t * 4);
  } else if (same == 0 && other == 3) {
    return ((1 - t) * 4 + t * 3);
  } else {
    return ((1 - t) * same + t * other);
  }
}

SameOther getSameAndOther(double a) {
  SameOther out;
  if (a < 1) {
    out.same = 1; out.other = 0;
  } else if (a < 2) {
    out.same = 1; out.other = 2;
  } else if (a < 3) {
    out.same = 3; out.other = 2;
  } else {
    out.same = 3; out.other = 0;
  }
  return out;
}

// ---------------------------------------------------------------------------
// function to check whether grounded movement is permissible (no low ceilings)

MaybeNum moveAlongGround(Vec2D pos, Vec2D posNext, double ecbHeight,
                         Surface ground, const SurfaceList *ceilings) {
  if (pos.x == posNext.x) {
    return maybe_null();
  } else {
    const char dir = posNext.x < pos.x ? 'l' : 'r';
    const SurfaceGeom groundGeom = surface_geom(&ground);
    const Vec2D groundLeft = extremePoint(groundGeom, 'l');
    const Vec2D groundRight = extremePoint(groundGeom, 'r');
    if ((dir == 'l' && pos.x < groundLeft.x)
        || (dir == 'r' && pos.x > groundRight.x)) {
      return maybe_null();
    } else {
      const double start = dir == 'l' ? js_min(pos.x, groundRight.x)
                                      : js_max(pos.x, groundLeft.x);
      const double end = dir == 'l' ? js_max(posNext.x, groundLeft.x)
                                    : js_min(posNext.x, groundRight.x);

      const Line2 groundLine = line2(ground.p0, ground.p1);
      const Vec2D groundStart = coordinateIntercept(groundLine, vLineAt(start));
      const Vec2D groundEnd = coordinateIntercept(groundLine, vLineAt(end));

      ECB startECB = makeECB(groundStart, additionalOffset, smallestECBHeight);
      ECB endECB = makeECB(groundEnd, additionalOffset, smallestECBHeight);

      // should not recalculate this every time...
      LabelledSurfaceList labelledCeilings;
      labelledCeilings.count = 0;
      zipLabelsInto(&labelledCeilings, ceilings, 'c');
      TouchingDatum firstCeilingCollision =
          findClosestCollision(startECB, endECB, &labelledCeilings);
      if (!firstCeilingCollision.present) {
        if (ecbHeight > smallestECBHeight) {
          return maybe_null();
        } else {
          // do a second collision check, in case the player squeezed
          // themselves into a location they should not have
          startECB = makeECB(groundStart, additionalOffset / 10, ecbHeight);
          endECB = makeECB(groundEnd, additionalOffset / 10, ecbHeight);
          firstCeilingCollision =
              findClosestCollision(startECB, endECB, &labelledCeilings);
          if (!firstCeilingCollision.present ||
              firstCeilingCollision.kind == CD_CORNER) {
            return maybe_null();
          } else {
            const Surface ceiling = firstCeilingCollision.surface;
            // find where to reposition the player by intersecting the
            // offset ground with the ceiling
            const Vec2D intercept = coordinateIntercept(
                line2(ceiling.p0, ceiling.p1),
                line2(ml_add(groundStart, vec2d(0, smallestECBHeight)),
                      ml_add(groundEnd, vec2d(0, smallestECBHeight))));
            return maybe_num(intercept.x + (dir == 'l' ? additionalOffset
                                                       : -additionalOffset));
          }
        }
      } else {
        const double s = firstCeilingCollision.sweep;
        return maybe_num((1 - s) * pos.x + s * posNext.x
                         + (dir == 'l' ? additionalOffset : -additionalOffset));
      }
    }
  }
}

// ---------------------------------------------------------------------------
// ECB squashing and re-inflating

// finds the ECB squash factor for a grounded ECB
MaybeNum groundedECBSquashFactor(Vec2D ecbTop, Vec2D ecbBottom,
                                 const SurfaceList *ceilings) {
  MaybeNum ceilingYValues[ML_MAX_SURFACES];
  for (int i = 0; i < ceilings->count; i++) {
    const SurfaceGeom ceil = surface_geom(&ceilings->items[i]);
    if (ecbTop.x < extremePoint(ceil, 'l').x ||
        ecbTop.x > extremePoint(ceil, 'r').x) {
      ceilingYValues[i] = maybe_null();
    } else {
      ceilingYValues[i] = maybe_num(
          coordinateIntercept(line2(ecbBottom, ecbTop),
                              line2(ceil.p0, ceil.p1)).y);
    }
  }
  const MaybeNum lowestCeilingYValue =
      findSmallestWithin(ceilingYValues, ceilings->count, ecbBottom.y, ecbTop.y);
  const double offset = additionalOffset / 10;
  if (!lowestCeilingYValue.present) {
    return maybe_null();
  } else {
    return maybe_num(js_max(offset,
        (lowestCeilingYValue.v - ecbBottom.y) / (ecbTop.y - ecbBottom.y)
        - offset));
  }
}

// finds the ECB squash factor by inflating the ECB from the point on the ECB
// given by the angular parameter t (null = inflate from center)
static SquashDatum inflateECB(ECB ecb, bool tIsNull, double t, Vec2D focus,
                              const LabelledSurfaceList *relevantSurfaces) {
  const double offset = additionalOffset / 10;
  ECB pointlikeECB;
  pointlikeECB.pt[0] = vec2d(focus.x, focus.y - offset);
  pointlikeECB.pt[1] = vec2d(focus.x + offset, focus.y);
  pointlikeECB.pt[2] = vec2d(focus.x, focus.y + offset);
  pointlikeECB.pt[3] = vec2d(focus.x - offset, focus.y);

  const TouchingDatum closestCollision =
      findClosestCollision(pointlikeECB, ecb, relevantSurfaces);

  if (!closestCollision.present) {
    return squash_datum(1, tIsNull, t);
  } else {
    // newLocation = t === null ? (surface ? pt : angular) : t
    double newLocation;
    if (tIsNull) {
      newLocation = closestCollision.kind == CD_SURFACE
                        ? closestCollision.pt
                        : closestCollision.angular;
    } else {
      newLocation = t;
    }
    // ECB angular parameter, sweeping parameter
    return squash_datum(js_max(additionalOffset,
                               closestCollision.sweep - additionalOffset),
                        false, newLocation);
  }
}

typedef struct {
  Vec2D position;
  SquashDatum squashDatum;
  ECB ecb;
} ReinflateResult;

static ReinflateResult reinflateECB(ECB ecb, Vec2D position,
                                    const LabelledSurfaceList *relevantSurfaces,
                                    SquashDatum oldecbSquashDatum,
                                    bool grounded) {
  ReinflateResult out;
  double q = 1;
  const bool angularIsNull = oldecbSquashDatum.locationIsNull;
  const double angularParameter = oldecbSquashDatum.location;
  if (oldecbSquashDatum.factor < 1) {
    q = 1 / oldecbSquashDatum.factor + additionalOffset / 20;
    const Vec2D focus =
        ecbFocusFromAngularParameter(ecb, angularIsNull, angularParameter);
    ECB fullsizeecb;
    fullsizeecb.pt[0] = vec2d(q * ecb.pt[0].x + (1 - q) * focus.x,
                              q * ecb.pt[0].y + (1 - q) * focus.y);
    fullsizeecb.pt[1] = vec2d(q * ecb.pt[1].x + (1 - q) * focus.x,
                              q * ecb.pt[1].y + (1 - q) * focus.y);
    fullsizeecb.pt[2] = vec2d(q * ecb.pt[2].x + (1 - q) * focus.x,
                              q * ecb.pt[2].y + (1 - q) * focus.y);
    fullsizeecb.pt[3] = vec2d(q * ecb.pt[3].x + (1 - q) * focus.x,
                              q * ecb.pt[3].y + (1 - q) * focus.y);
    const SquashDatum ecbSquashDatum =
        inflateECB(fullsizeecb, angularIsNull, angularParameter, focus,
                   relevantSurfaces);
    const ECB squashedecb = squashECBAt(fullsizeecb,
        squash_datum(ecbSquashDatum.factor, angularIsNull, angularParameter));
    const Vec2D newPosition =
        vec2d(position.x + squashedecb.pt[0].x - ecb.pt[0].x,
              grounded ? position.y
                       : position.y + squashedecb.pt[0].y - ecb.pt[0].y);
    drawECB(squashedecb, "#ffff00");
    out.position = newPosition;
    out.squashDatum = ecbSquashDatum; // { location: newAngular, factor }
    out.ecb = squashedecb;
    return out;
  } else {
    out.position = position;
    out.squashDatum = squash_datum(1, angularIsNull, angularParameter);
    out.ecb = ecb;
    return out;
  }
}

// ---------------------------------------------------------------------------
// main collision routine

// this function initialises necessary data and then calls the main collision
// routine loop
CollisionRoutineResult runCollisionRoutine(ECB ecb1, ECB ecbp, Vec2D position,
                                           SquashDatum ecbSquashDatum,
                                           PlayerStatusInfo playerStatusInfo,
                                           const Stage *stage) {
  // BELOW: this is recomputed every frame and should be avoided (JS note)
  LabelledSurfaceList stageWalls;
  stageWalls.count = 0;
  zipLabelsInto(&stageWalls, &stage->wallL, 'l');
  zipLabelsInto(&stageWalls, &stage->wallR, 'r'); // .concat(...)
  LabelledSurfaceList stageGrounds;
  stageGrounds.count = 0;
  zipLabelsInto(&stageGrounds, &stage->ground, 'g');
  LabelledSurfaceList stageCeilings;
  stageCeilings.count = 0;
  zipLabelsInto(&stageCeilings, &stage->ceiling, 'c');
  LabelledSurfaceList stagePlatforms;
  stagePlatforms.count = 0;
  zipLabelsInto(&stagePlatforms, &stage->platform, 'p');
  // ABOVE: this is recomputed every frame and should be avoided

  const bool grounded = playerStatusInfo.grounded;

  // horizIgnore: "none" | "all" | "platforms"
  char horizIgnore = 'n'; // ignore no horizontal surfaces by default
  if (grounded) {
    horizIgnore = 'a'; // ignore all horizontal surfaces when grounded
  } else {
    horizIgnore = playerStatusInfo.ignoringPlatforms ? 'p' : 'n';
  }

  // allSurfacesMinusPlatforms = walls ++ grounds ++ ceilings
  LabelledSurfaceList allSurfacesMinusPlatforms = stageWalls;
  for (int i = 0; i < stageGrounds.count; i++) {
    allSurfacesMinusPlatforms.items[allSurfacesMinusPlatforms.count++] =
        stageGrounds.items[i];
  }
  for (int i = 0; i < stageCeilings.count; i++) {
    allSurfacesMinusPlatforms.items[allSurfacesMinusPlatforms.count++] =
        stageCeilings.items[i];
  }

  LabelledSurfaceList relevantSurfaces;
  relevantSurfaces.count = 0;
  switch (horizIgnore) {
    case 'p': // "platforms": walls ++ grounds ++ ceilings
      relevantSurfaces = allSurfacesMinusPlatforms;
      break;
    case 'n': // "none" (and default): walls ++ grounds ++ ceilings ++ platforms
    default:
      relevantSurfaces = allSurfacesMinusPlatforms;
      for (int i = 0; i < stagePlatforms.count; i++) {
        relevantSurfaces.items[relevantSurfaces.count++] =
            stagePlatforms.items[i];
      }
      break;
    case 'a': // "all": walls only
      relevantSurfaces = stageWalls;
      break;
  }

  const ECBDatum resolution =
      resolveECB(ecb1, ecbp, playerStatusInfo, &relevantSurfaces);
  const SimpleTouchingDatum newTouching = resolution.touching;
  ECB newECBp = resolution.ecb;
  const double newSquashFactor = resolution.squashed
      ? js_min(1, (newECBp.pt[1].x - newECBp.pt[3].x)
                  / (ecbp.pt[1].x - ecbp.pt[3].x))
      : 1;
  bool newSquashLocationIsNull = true;
  double newSquashLocation = 0;
  if (newTouching.present) {
    if (newTouching.kind == CD_SURFACE) {
      newSquashLocationIsNull = false;
      newSquashLocation = newTouching.pt;
    } else {
      newSquashLocationIsNull = false;
      newSquashLocation = newTouching.angular;
    }
  }
  SquashDatum newSquashDatum =
      squash_datum(newSquashFactor, newSquashLocationIsNull, newSquashLocation);
  newSquashDatum.factor *= ecbSquashDatum.factor;
  Vec2D newPosition = ml_subtract(ml_add(position, newECBp.pt[0]), ecbp.pt[0]);

  if (newSquashDatum.factor < 1) {
    bool squashingLocationIsNull = true;
    double squashingLocation = 0;
    if (grounded) {
      squashingLocationIsNull = false;
      squashingLocation = 0;
    }
    ReinflateResult r = reinflateECB(newECBp, newPosition,
        &allSurfacesMinusPlatforms,
        squash_datum(newSquashDatum.factor, squashingLocationIsNull,
                     squashingLocation),
        grounded);
    newPosition = r.position;
    newSquashDatum = r.squashDatum;
    newECBp = r.ecb;
    if (!grounded && newSquashDatum.factor < 1) {
      // reinflate a second time if it might help
      r = reinflateECB(newECBp, newPosition, &allSurfacesMinusPlatforms,
                       newSquashDatum, false);
      newPosition = r.position;
      newSquashDatum = r.squashDatum;
      newECBp = r.ecb;
    }
  }

  CollisionRoutineResult out;
  out.position = newPosition;
  out.touching = newTouching;
  out.squashDatum = newSquashDatum;
  out.ecb = newECBp;
  return out;
}
