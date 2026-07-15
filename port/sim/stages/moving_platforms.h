// port/sim/stages/moving_platforms.h — the per-tick movingPlatforms stage
// logic (M2 task 14): the upstream src/stages/vs-stages/*.js function
// bodies. STAB1 (ml_stages, M1 task 3) owns the stage DATA; this module
// owns the per-tick LOGIC the M1 extractor externals-stubbed (the
// god-module boundary: ystory/fountain import main/activeStage/envcoll
// but only dereference them inside these bodies). gameTick calls
// getActiveStage().movingPlatforms() FIRST thing in the mode-3 tick
// (main.js:1058) — before articles and player updates.
//
// Value model (MpSim): the exact read/write slice the two non-empty
// bodies dereference —
// - ystory: activeStage.platform[0] (Randall) + ALL FOUR
//   player[j].phys.{onSurface,grounded,pos} (the rider loop is unguarded
//   by playerType; inactive slots hold page-start CSS-era playerObjects);
// - fountain: platform[1..2].y, the module-PRIVATE platformStates pair,
//   main's exported `starting` (the reset arm), and the same player
//   slice (transfer arms).
// The four other VS stages have EMPTY movingPlatforms bodies upstream
// (measured) — the dispatcher runs them as no-ops.
//
// platformStates.state is the string "moving" | "static" upstream —
// modeled as isStatic (two-value domain; the canon bridge restores the
// exact strings).
#ifndef MP_MOVING_PLATFORMS_H
#define MP_MOVING_PLATFORMS_H

#include <stdbool.h>

#include "../util/vec2d.h"

// stageMapping order (src/stages/activeStage.js; oracle --stage ids)
typedef enum {
  MP_BATTLEFIELD = 0,
  MP_YSTORY = 1,
  MP_PSTADIUM = 2,
  MP_DREAMLAND = 3,
  MP_FDEST = 4,
  MP_FOUNTAIN = 5
} MpStageKind;

typedef struct {
  bool isStatic; // state: "moving" (false) | "static" (true)
  double timer;
  double destination;
} MpPlatformState;

typedef struct {
  bool grounded;
  double onSurface[2];
  Vec2D pos;
} MpPlayerSlice;

#define MP_MAX_PLATFORMS 4 // ystory has 4; every other VS stage fewer

typedef struct {
  int nPlat; // upstream platform list length (marshaled; bodies use literals)
  Vec2D platform[MP_MAX_PLATFORMS][2];
  MpPlatformState ps[2]; // fountain's module-private platformStates
  MpPlayerSlice player[4];
  bool starting; // main.js exported let (fountain's reset arm reads it)
} MpSim;

void mp_ystory_movingPlatforms(MpSim *S);
void mp_fountain_movingPlatforms(MpSim *S);
// the getActiveStage().movingPlatforms() dispatch (vs-stages aggregator)
void mp_movingPlatforms(MpStageKind kind, MpSim *S);

#endif // MP_MOVING_PLATFORMS_H
