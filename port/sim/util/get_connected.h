// get_connected.h <- src/target/util/getConnected.js (A45 T5 prerequisite;
// structure-parallel translation, doubles only).
//
// WHY THIS IS IN THE SIM PLANE AND NOT IN THE BUILDER.
//
// The A45 spike filed getConnected as a helper the POLYGON tool (T7) needs,
// because upstream's builder recomputes `stageTemp.connected` after every
// structural edit (targetbuilder.js:410, :457, :510, :613, :735). MEASURED
// 2026-08-26: that is not where it matters here.
//
//   1. `connected` is NOT a field of the share-code grammar (encode.js has
//      14 fields, none of them connected — stage_code.h), so nothing the
//      builder computes survives a save. And upstream's only other consumer
//      of a builder-computed `connected` is the *Test stage* arm, which D50
//      did not port. In THIS port every one of those five assignments is
//      dead, and they are carried as commented dead arms in foh_tbuild.c.
//
//   2. But upstream DERIVES it at PARSE time — encode.js:237,
//      `stage.connected = getConnected(stage)`, at the end of
//      parseStageCode. So EVERY custom stage upstream plays has a connected
//      plane, computed from its own surfaces. `custom_stage.c` used to write
//      `hasConnected = false` with the comment "no `connected` field in the
//      code grammar": true about the grammar, wrong about the behaviour.
//
// WHY NO CHECK CAUGHT IT (the /CONTEXT.md "seam" entry, verbatim: each
// side's own check passes while nothing asserts the crossing).
// check-custom-stage.sh compares the port's custom path against the port's
// TTAB1 path. The authored target stages carry no `connected` key
// (pipeline/lib/targets-schema.js:143 pins its absence), so target_play.c
// also writes false — both sides of the differential were false, and the
// frozen goldens were recorded through the AUTHORED path, so the browser
// never ran parseStageCode either.
//
// WHY IT WAS HARMLESS UNTIL NOW, AND WHY IT STOPS BEING HARMLESS.
// MEASURED over all ten authored target stages with upstream's own bodies:
// ZERO links — every ground and platform yields [null, null]. And an
// all-null `connected` takes the same physics arm as an absent one
// (physics.js:265-300: `maybeLeftGroundTypeAndIndex === null` ->
// fallOffGround). So the shipped corpus diverges by exactly nothing.
// A polygon, however, emits its ground edges and its wall edges sharing
// vertices BY CONSTRUCTION (targetbuilder.js:351-367 classifies each edge of
// one closed loop), and getConnected.js:36-41 turns a ground whose left
// extreme coincides with a wallR's right extreme into ["r", j], which
// physics.js:281-285 reads as `disableFall = true`. Upstream stops the
// player at the lip of a polygon floor; without this the port walks them
// off it. T5's WALL tool reaches the same state by hand.
//
// SHAPE NOTES (rule 6 — expression shapes verbatim):
// - `d` is manhattanDist (the import is aliased `as d` upstream).
// - The `if (broke[0] && broke[1]) break;` at the top of each outer body is
//   upstream's, and it is DEAD: `broke` is re-declared `[false,false]` two
//   lines above it on every iteration. Carried, never "fixed".
// - The four inner loops run in upstream's order (ground, platform, wallR,
//   wallL) and each is guarded by its own `!broke[k]`, so the FIRST match in
//   that order wins. Reordering them would change which link is recorded.
// - wallR is only ever consulted for the LEFT side and wallL only for the
//   RIGHT side; that asymmetry is upstream's and is correct (a right-facing
//   wall bounds a ground on its left).
#ifndef ML_GET_CONNECTED_H
#define ML_GET_CONNECTED_H

#include "../physics.h"      // MlConnHalf, MlConnPair, ML_MAX_SURFACES
#include "../stage_types.h"  // Stage, SurfaceList, surface_geom
#include "extreme_point.h"
#include "lin_alg.h"         // manhattanDist

static inline MlConnHalf gc_none(void) {
  MlConnHalf h;
  h.present = false;
  h.type = 0;
  h.index = 0;
  return h;
}

static inline MlConnHalf gc_link(char type, int j) {
  MlConnHalf h;
  h.present = true;
  h.type = type;
  h.index = (double)j;
  return h;
}

// d(extremePoint(a, ea), extremePoint(b, eb)) < 0.001
static inline bool gc_meets(const Surface *a, char ea, const Surface *b,
                            char eb) {
  return manhattanDist(extremePoint(surface_geom(a), ea),
                       extremePoint(surface_geom(b), eb)) < 0.001;
}

// One pass of the outer body, shared by the ground loop (i over `self` ==
// stage.ground) and the platform loop (`self` == stage.platform). Upstream
// writes the two loops out longhand; they differ ONLY in which list `self`
// is, so one body with a `self` parameter is the same expression shape.
static inline MlConnPair gc_pair(const Stage *st, const SurfaceList *self,
                                 int i) {
  MlConnPair out;
  out.l = gc_none();
  out.r = gc_none();
  bool broke0 = false, broke1 = false;
  const Surface *me = &self->items[i];
  for (int j = 0; j < st->ground.count; j++) {
    const Surface *o = &st->ground.items[j];
    if (!broke0 && gc_meets(me, 'l', o, 'r')) { out.l = gc_link('g', j); broke0 = true; }
    if (!broke1 && gc_meets(me, 'r', o, 'l')) { out.r = gc_link('g', j); broke1 = true; }
  }
  for (int j = 0; j < st->platform.count; j++) {
    const Surface *o = &st->platform.items[j];
    if (!broke0 && gc_meets(me, 'l', o, 'r')) { out.l = gc_link('p', j); broke0 = true; }
    if (!broke1 && gc_meets(me, 'r', o, 'l')) { out.r = gc_link('p', j); broke1 = true; }
  }
  for (int j = 0; j < st->wallR.count; j++) {
    const Surface *o = &st->wallR.items[j];
    if (!broke0 && gc_meets(me, 'l', o, 'r')) { out.l = gc_link('r', j); broke0 = true; }
  }
  for (int j = 0; j < st->wallL.count; j++) {
    const Surface *o = &st->wallL.items[j];
    if (!broke1 && gc_meets(me, 'r', o, 'l')) { out.r = gc_link('l', j); broke1 = true; }
  }
  return out;
}

// getConnected(stage) -> the two rows of `connected`, written into an
// MlStageX's own arrays. The caller sets hasConnected: upstream's parsed
// stage ALWAYS has the key, so a custom stage always gets true — an
// all-null result is not the same value as an absent one to the reader,
// even though physics.js:265-300 takes the same arm for both.
static inline void getConnected(const Stage *st, MlConnPair *connGround,
                                int *connGroundCount, MlConnPair *connPlatform,
                                int *connPlatformCount) {
  const int lg = st->ground.count;
  const int lp = st->platform.count;
  for (int i = 0; i < lg; i++) connGround[i] = gc_pair(st, &st->ground, i);
  *connGroundCount = lg;
  for (int i = 0; i < lp; i++) connPlatform[i] = gc_pair(st, &st->platform, i);
  *connPlatformCount = lp;
}

#endif // ML_GET_CONNECTED_H
