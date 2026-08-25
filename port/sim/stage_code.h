// stage_code.h <- src/stages/encode.js (A45 T1: the share-code codec and
// the custom-stage value model every later A45 ticket builds on).
//
// Upstream's LIVE pair is `createStageCode` / `parseStageCode`.
// `createTargetCode` (targetbuilder.js:83-115) is DEAD — zero callers, and
// it dereferences `stageTemp.box` / `stageTemp.startingPoint.x`, neither of
// which that object has (docs/research/target-builder-design.md §0/P2).
// It is deliberately NOT ported.
//
// The wire format is a WHOLE STAGE, not a target layout: 14 `&`-separated
// fields, `~` between records, `,` between numbers. Targets are field 11.
//
//   0  startingPoint    x,y
//   1  startingFace     1|-1, comma separated ("1,1,1,1" when the stage
//                       object has no startingFace key — encode.js:20-22)
//   2  ground           x1,y1,x2,y2,d    d in 0..4 = none/fire/electric/
//   3  ceiling                            slash/darkness
//   4  wallL
//   5  wallR
//   6  platform
//   7  background.line
//   8  polygon          x,y,x,y,...  (flat)
//   9  background.polygon
//  10  ledge            <g|p>,index,side
//  11  target           x,y
//  12  blastzone        minx,miny,maxx,maxy
//  13  scale            one number
//
// THE CODE IS THE CANONICAL FORM. The first emission is lossy (every
// coordinate goes through toFixed(2)); every emission after that is exact,
// so `mlk_encode(mlk_parse(c)) == c` for any c this codec emitted. That
// idempotence is what port/sim/check-stage-code.sh judges, differentially,
// against upstream's own executed encode.js.
//
// Two upstream defects are CARRIED VERBATIM (HARD RULE 5) — see the named
// comments at their sites in stage_code.c:
//   BUG 1  encode.js:39  `if (i !== 5)` tests the SURFACE index where it
//          meant the TYPE index: the 6th surface of EVERY type silently
//          loses its damage digit.
//   BUG 2  encode.js:244 a decoded stage gets polygonMap = [null, ...].
//
// D39 (registered deviation): mlk_parse accepts a STRICT SUBSET of
// parseStageCode's input language. Upstream parses with parseFloat /
// parseInt, which coerce arbitrary junk into a NaN-riddled stage; the port
// requires every coordinate token to match the emitted alphabet
// `-?\d+(\.\d{1,2})?` and rejects otherwise, because `strtod` is BANNED on
// this device (iter 38: the SDK's static musl strtod mis-rounds subnormals
// and drops -0). Integer hundredths / 100.0 is correctly rounded and needs
// no libc. Rejecting IS upstream's own "Invalid code" path — parseStageCode
// already returns null on several inputs. On every code createStageCode can
// emit, the two agree byte for byte; that is the differential's claim.
#ifndef ML_STAGE_CODE_H
#define ML_STAGE_CODE_H

#include <stddef.h>

#include "ml_js.h"
#include "physics.h"      // MlLedge, ML_MAX_LEDGES
#include "stage_types.h"  // Surface, SurfaceList, ML_MAX_SURFACES
#include "util/box2d.h"
#include "util/vec2d.h"

// --- toFixed ---------------------------------------------------------------

// "-1152921504606846976.00" + NUL: the widest toFixed(2) below 1e21, plus
// slack for the >= 1e21 String(x) fallback ("-1.7976931348623157e+308").
#define ML_TO_FIXED2_MAX 32

// ECMA-262 §21.1.3.3 Number.prototype.toFixed with fractionDigits = 2.
// Writes a NUL-terminated result into buf and returns its length.
// Only f == 2 is implemented — it is the only width encode.js uses, and a
// general f would need 10^f in a uint64 (ponytail: generalise when a second
// caller exists). The rounding is EXACT integer work, never `round(x*100)`:
// step 7's "n minimising |n/10^f - x|, ties to the larger n" is a property
// of the real number x, and the double product x*100.0 crosses .5 in the
// wrong direction for values such as 2^60 (measured: V8 emits
// "1152921504606846976.00", which String(x) cannot even name).
int ml_to_fixed2(double x, char *buf);

// --- the custom-stage value model -----------------------------------------
//
// Reuses the sim's existing stage vocabulary wherever one exists: Surface /
// SurfaceList / Stage (stage_types.h), MlLedge (physics.h), Box2D, Vec2D.
// Only the planes envcoll never needed are new here: starting points and
// faces, polygons, the background plane, targets and scale.
//
// CAPS ARE LOUD, NEVER SILENT. A code that exceeds one is REJECTED with a
// reason; nothing is truncated. Upstream's JS arrays grow without bound and
// its builder allows 120 polygons / 20 targets, so a hand-authored code CAN
// exceed these — that asymmetry is design risk R2 and its resolution
// (refuse at load, or raise ML_MAX_TARGETS and friends) is an owner ruling
// owed before A45 T2. Refusing loudly is the safe half of it.

#define MLK_MAX_STARTING_POINTS 8  // upstream's builder authors 4
#define MLK_MAX_POLYGONS 16
#define MLK_MAX_POLY_POINTS 32
#define MLK_MAX_TARGETS 20  // targetbuilder.js:563's own cap

typedef struct {
  Vec2D pts[MLK_MAX_POLY_POINTS];
  int count;  // 0 is legal: parsePolygon returns [] on an odd token count
} MlkPolygon;

typedef struct {
  MlkPolygon items[MLK_MAX_POLYGONS];
  int count;
} MlkPolygonList;

typedef struct {
  Vec2D startingPoint[MLK_MAX_STARTING_POINTS];
  int startingPointCount;

  // `hasStartingFace` is load-bearing, not decoration: an ABSENT key emits
  // "1,1,1,1" while an EMPTY array emits nothing (encode.js:20-30), and the
  // builder's stageTemp genuinely has no startingFace key.
  bool hasStartingFace;
  double startingFace[MLK_MAX_STARTING_POINTS];  // +1 | -1 (parseSign)
  int startingFaceCount;

  Stage s;            // ground, ceiling, wallL, wallR, platform
  SurfaceList bgLine; // background.line

  MlkPolygonList polygon, bgPolygon;

  // BUG 2 (encode.js:244) is carried in the value model, not just in the
  // code path: a parsed stage's polygonMap is all-null, so every polygon <->
  // surface link an imported stage once had is gone. A45 T7's MOVE/DELETE
  // arms must see that null and behave as upstream does (drag the outline
  // away from its collision surfaces) rather than silently re-deriving it.
  int polygonMapCount;
  bool polygonMapIsNull[MLK_MAX_POLYGONS];

  MlLedge ledge[ML_MAX_LEDGES];
  int ledgeCount;

  Vec2D target[MLK_MAX_TARGETS];
  int targetCount;

  Box2D blastzone;
  double scale;
} MlkStage;

// Worst-case emission for the caps above, rounded up: 6 surface lists x 64
// x 134 bytes + 2 polygon planes x 16 x 32 points x 65 bytes + the small
// fields ~= 121 KB. mlk_encode is bounds-checked regardless of this value.
#define MLK_CODE_MAX 131072

// createStageCode. Writes a NUL-terminated code into buf and returns its
// length, or -1 if it does not fit in cap bytes (nothing partial is
// promised on failure).
int mlk_encode(const MlkStage *st, char *buf, size_t cap);

// parseStageCode. Returns true and fills *out on success. On failure returns
// false, leaves *out indeterminate, and — when reason is non-NULL — points
// *reason at a static string naming the rejecting rule (this is the string
// behind upstream's "Invalid code", targetselect.js:158-162).
bool mlk_parse(const char *code, MlkStage *out, const char **reason);

#endif  // ML_STAGE_CODE_H
