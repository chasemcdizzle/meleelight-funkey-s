// stage_types.h <- the slice of src/stages/stage.js type declarations the
// environmentalCollision module consumes (Surface, LabelledSurface, Stage),
// plus the DamageType slice of src/physics/damageTypes.js.
//
// JS value model notes (canon-v1 faithfulness):
// - A JS Surface is the array [Vec2D, Vec2D] with an OPTIONAL third
//   element (surface properties object, possibly carrying `damageType`).
//   The C Surface remembers exactly which of those shapes it had, because
//   PointSweepResult ECHOES the surface into the return value and the
//   echo must serialize identically.
// - DamageType at the findCollision level is `null | undefined | string`
//   (wall[2] !== undefined ? wall[2].damageType : null) -> tagged value.
//   DT_ABSENT means "no damageType KEY" (only meaningful for the optional
//   key on result objects / the props object).
#ifndef ML_STAGE_TYPES_H
#define ML_STAGE_TYPES_H

#include "util/vec2d.h"

typedef enum { DT_ABSENT = 0, DT_NULL, DT_UNDEF, DT_STR } DamageTag;

typedef struct {
  DamageTag tag;
  char str[32]; // value when tag == DT_STR
} DamageType;

static inline DamageType damage_absent(void) {
  DamageType d; d.tag = DT_ABSENT; d.str[0] = 0; return d;
}
static inline DamageType damage_null(void) {
  DamageType d; d.tag = DT_NULL; d.str[0] = 0; return d;
}
static inline DamageType damage_undef(void) {
  DamageType d; d.tag = DT_UNDEF; d.str[0] = 0; return d;
}

// The geometric pair (what extremePoint etc. consume).
typedef struct { Vec2D p0, p1; } SurfaceGeom;

typedef struct {
  Vec2D p0, p1;
  bool hasProps;              // JS wall[2] !== undefined (array had elem 2)
  bool propsHasDamageTypeKey; // wall[2] carries a "damageType" own key
  DamageType propsDamageType; // its value when the key is present
} Surface;

static inline SurfaceGeom surface_geom(const Surface *s) {
  SurfaceGeom g; g.p0 = s->p0; g.p1 = s->p1; return g;
}

// LabelledSurface = [Surface, [type-string, index-number]]
typedef struct {
  Surface surface;
  char type;    // 'l' | 'r' | 'g' | 'c' | 'p' (single-char label strings)
  double index; // JS number
} LabelledSurface;

#define ML_MAX_SURFACES 64

typedef struct {
  Surface items[ML_MAX_SURFACES];
  int count;
} SurfaceList;

typedef struct {
  LabelledSurface items[ML_MAX_SURFACES];
  int count;
} LabelledSurfaceList;

// The module-read projection of the upstream Stage object (FORMAT.md):
// exactly the five surface lists runCollisionRoutine dereferences.
typedef struct {
  SurfaceList wallL, wallR, ground, ceiling, platform;
} Stage;

#endif // ML_STAGE_TYPES_H
