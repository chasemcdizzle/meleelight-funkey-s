// environmental_collision.h <- src/physics/environmentalCollision.js
// (structure-parallel translation, M2-CAL calibration slice).
//
// Value-model conventions (see also stage_types.h, ml_js.h):
// - every JS number is a C double (indices, ECB point ids, angular
//   parameters included) — canon-v1 serializes bit patterns of doubles;
// - `null | X` becomes a `present`/`IsNull` flag next to the value;
// - result-object KEY PRESENCE is part of the contract (e.g. the
//   optional `damageType` key on touching data) and is modeled with
//   DamageTag DT_ABSENT — the replay serializer emits exactly the keys
//   the JS construction sites create.
#ifndef ML_ENVIRONMENTAL_COLLISION_H
#define ML_ENVIRONMENTAL_COLLISION_H

#include "ml_js.h"
#include "stage_types.h"
#include "util/vec2d.h"
#include "util/ecb_transform.h"
#include "util/find_smallest_within.h"

// export const additionalOffset / smallestECBWidth / smallestECBHeight
#define ML_ADDITIONAL_OFFSET 0.00001
#define ML_SMALLEST_ECB_WIDTH 1.95
#define ML_SMALLEST_ECB_HEIGHT 1.95

// --- result types --------------------------------------------------------

// PointSweepResult = { sweep, kind:"surface", surface, type, index, pt }
typedef struct {
  bool present;
  double sweep;
  Surface surface; // echoes the input wall (shape-faithful)
  char type;
  double index;
  double pt;
} PointSweepResult;

// EdgeSweepResult = { kind:"corner", corner, sweep, angular, damageType }
// (the damageType KEY is always present; its value may be null/undef/str)
typedef struct {
  bool present;
  Vec2D corner;
  double sweep;
  double angular;
  DamageType damageType;
} EdgeSweepResult;

// CollisionDatum = null | PointSweepResult | EdgeSweepResult
typedef enum { CD_NULL = 0, CD_SURFACE, CD_CORNER } CollisionKind;
typedef struct {
  CollisionKind kind;
  PointSweepResult ps; // when CD_SURFACE
  EdgeSweepResult es;  // when CD_CORNER
} CollisionDatum;

// SimpleTouchingDatum = { kind:"surface", type, index, pt [, damageType] }
//                     | { kind:"corner", angular [, damageType] }
// damageType.tag == DT_ABSENT <=> the key is absent.
typedef struct {
  bool present;
  CollisionKind kind;
  char type;
  double index;
  double pt;
  double angular;
  DamageType damageType;
} SimpleTouchingDatum;

// PlayerStatusInfo = { grounded, ignoringPlatforms, immune }
typedef struct {
  bool grounded;
  bool ignoringPlatforms;
  bool immune;
} PlayerStatusInfo;

// CollisionRoutineResult = { position, touching, squashDatum, ecb }
typedef struct {
  Vec2D position;
  SimpleTouchingDatum touching; // .present == false => null
  SquashDatum squashDatum;
  ECB ecb;
} CollisionRoutineResult;

// getSameAndOther returns [same, other]
typedef struct { double same, other; } SameOther;

// --- exported functions (the module boundary) ----------------------------

Line2 hLineThrough(Vec2D point);
Line2 hLineAt(double y);
Line2 vLineThrough(Vec2D point);
Line2 vLineAt(double x);
Line2 lineThrough(Vec2D point, XOrY xOrY);
Vec2D outwardsWallNormal(Vec2D wallBottomOrLeft, Vec2D wallTopOrRight,
                         char wallType);
double coordinateInterceptParameter(Line2 line1, Line2 line2);
Vec2D coordinateIntercept(Line2 line1, Line2 line2);
CollisionDatum findCollision(ECB ecb1, ECB ecbp,
                             LabelledSurface labelledSurface);
SameOther getSameAndOther(double a);
MaybeNum moveAlongGround(Vec2D pos, Vec2D posNext, double ecbHeight,
                         Surface ground, const SurfaceList *ceilings);
MaybeNum groundedECBSquashFactor(Vec2D ecbTop, Vec2D ecbBottom,
                                 const SurfaceList *ceilings);
CollisionRoutineResult runCollisionRoutine(ECB ecb1, ECB ecbp, Vec2D position,
                                           SquashDatum ecbSquashDatum,
                                           PlayerStatusInfo playerStatusInfo,
                                           const Stage *stage);

#endif // ML_ENVIRONMENTAL_COLLISION_H
