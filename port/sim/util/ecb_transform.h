// ecb_transform.h <- src/main/util/ecbTransform.js (structure-parallel
// translation). ECB = [Vec2D x4] (0 bottom, 1 right, 2 top, 3 left);
// SquashDatum = { factor : number, location : null | number }.
#ifndef ML_ECB_TRANSFORM_H
#define ML_ECB_TRANSFORM_H

#include "vec2d.h"
#include "lin_alg.h"

typedef struct { Vec2D pt[4]; } ECB;

typedef struct {
  double factor;
  bool locationIsNull; // location: null | number
  double location;
} SquashDatum;

static inline SquashDatum squash_datum(double factor, bool locationIsNull,
                                       double location) {
  SquashDatum s; s.factor = factor; s.locationIsNull = locationIsNull;
  s.location = location; return s;
}

static inline ECB moveECB(ECB ecb, Vec2D vec) {
  ECB out;
  out.pt[0] = vec2d(ecb.pt[0].x + vec.x, ecb.pt[0].y + vec.y);
  out.pt[1] = vec2d(ecb.pt[1].x + vec.x, ecb.pt[1].y + vec.y);
  out.pt[2] = vec2d(ecb.pt[2].x + vec.x, ecb.pt[2].y + vec.y);
  out.pt[3] = vec2d(ecb.pt[3].x + vec.x, ecb.pt[3].y + vec.y);
  return out;
}

static inline Vec2D ecbFocusFromAngularParameter(ECB ecb, bool tIsNull,
                                                 double t) {
  Vec2D focus;
  if (tIsNull) {
    focus = vec2d(ecb.pt[0].x, (ecb.pt[0].y + ecb.pt[2].y) / 2);
  } else if (t <= 1) {
    focus = vec2d((1 - t) * ecb.pt[0].x + t * ecb.pt[1].x,
                  (1 - t) * ecb.pt[0].y + t * ecb.pt[1].y);
  } else if (t <= 2) {
    focus = vec2d((1 - (t - 1)) * ecb.pt[1].x + (t - 1) * ecb.pt[2].x,
                  (1 - (t - 1)) * ecb.pt[1].y + (t - 1) * ecb.pt[2].y);
  } else if (t <= 3) {
    focus = vec2d((1 - (t - 2)) * ecb.pt[2].x + (t - 2) * ecb.pt[3].x,
                  (1 - (t - 2)) * ecb.pt[2].y + (t - 2) * ecb.pt[3].y);
  } else {
    focus = vec2d((1 - (t - 3)) * ecb.pt[3].x + (t - 3) * ecb.pt[0].x,
                  (1 - (t - 3)) * ecb.pt[3].y + (t - 3) * ecb.pt[0].y);
  }
  return focus;
}

static inline ECB squashECBAt(ECB ecb, SquashDatum squashDatum) {
  const Vec2D pos = ecbFocusFromAngularParameter(ecb, squashDatum.locationIsNull,
                                                 squashDatum.location);
  const double t = squashDatum.factor;
  ECB out;
  out.pt[0] = vec2d(t * ecb.pt[0].x + (1 - t) * pos.x, t * ecb.pt[0].y + (1 - t) * pos.y);
  out.pt[1] = vec2d(t * ecb.pt[1].x + (1 - t) * pos.x, t * ecb.pt[1].y + (1 - t) * pos.y);
  out.pt[2] = vec2d(t * ecb.pt[2].x + (1 - t) * pos.x, t * ecb.pt[2].y + (1 - t) * pos.y);
  out.pt[3] = vec2d(t * ecb.pt[3].x + (1 - t) * pos.x, t * ecb.pt[3].y + (1 - t) * pos.y);
  return out;
}

static inline ECB interpolateECB(ECB srcECB, ECB tgtECB, double s) {
  ECB out;
  out.pt[0] = vec2d((1 - s) * srcECB.pt[0].x + s * tgtECB.pt[0].x,
                    (1 - s) * srcECB.pt[0].y + s * tgtECB.pt[0].y);
  out.pt[1] = vec2d((1 - s) * srcECB.pt[1].x + s * tgtECB.pt[1].x,
                    (1 - s) * srcECB.pt[1].y + s * tgtECB.pt[1].y);
  out.pt[2] = vec2d((1 - s) * srcECB.pt[2].x + s * tgtECB.pt[2].x,
                    (1 - s) * srcECB.pt[2].y + s * tgtECB.pt[2].y);
  out.pt[3] = vec2d((1 - s) * srcECB.pt[3].x + s * tgtECB.pt[3].x,
                    (1 - s) * srcECB.pt[3].y + s * tgtECB.pt[3].y);
  return out;
}

static inline ECB makeECB(Vec2D pos, double halfWidth, double height) {
  ECB out;
  out.pt[0] = pos;
  out.pt[1] = ml_add(pos, vec2d(halfWidth, 0));
  out.pt[2] = ml_add(pos, vec2d(0, height));
  out.pt[3] = ml_add(pos, vec2d(-halfWidth, 0));
  return out;
}

#endif // ML_ECB_TRANSFORM_H
