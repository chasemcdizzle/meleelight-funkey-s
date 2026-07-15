// port/sim/stages/moving_platforms.c <- src/stages/vs-stages/vs-stages.js
// + src/stages/activeStage.js stageMapping (upstream pin 27af171): the
// per-stage movingPlatforms aggregator. battlefield/dreamland/pstadium/
// fdest have EMPTY movingPlatforms bodies upstream (measured; carried as
// literal no-ops); ystory/fountain live in their own files.
#include "moving_platforms.h"

// src/stages/vs-stages/battlefield.js:31 — empty body
static void mp_battlefield_movingPlatforms(MpSim *S) { (void)S; }
// src/stages/vs-stages/dreamland.js:27 — empty body
static void mp_dreamland_movingPlatforms(MpSim *S) { (void)S; }
// src/stages/vs-stages/pstadium.js:30 — empty body
static void mp_pstadium_movingPlatforms(MpSim *S) { (void)S; }
// src/stages/vs-stages/fdest.js:25 — empty body
static void mp_fdest_movingPlatforms(MpSim *S) { (void)S; }

void mp_movingPlatforms(MpStageKind kind, MpSim *S) {
  switch (kind) {
    case MP_BATTLEFIELD: mp_battlefield_movingPlatforms(S); break;
    case MP_YSTORY: mp_ystory_movingPlatforms(S); break;
    case MP_PSTADIUM: mp_pstadium_movingPlatforms(S); break;
    case MP_DREAMLAND: mp_dreamland_movingPlatforms(S); break;
    case MP_FDEST: mp_fdest_movingPlatforms(S); break;
    case MP_FOUNTAIN: mp_fountain_movingPlatforms(S); break;
  }
}
