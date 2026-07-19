// ai.h — structure-parallel C port of upstream src/main/ai.js (M4 task 4).
//
// ai.js's import graph (its whole reachable read/write surface, measured
// M2 task 16 + re-measured this task): player / characterSelections (cS)
// / playerType from main, aiInputBank from input, gameSettings (.turbo is
// the only read), activeStage (aS: .ledgePos/.platform/.ground only),
// plus the seeded Math.random. That god-module slice is MlAiSim (the
// MlInputSimState pattern, M2 task 3).
//
// Value planes:
// - aiInputBank rows are TAGGED JS values (MlAiInput / MlAiVal,
//   port/sim/input/ai_input.h — fix_plan §M2 rule 16: ai.js writes
//   NUMBERS into button fields, e.g. `.a = 1.0`, `.l = 0`, `.x = 1.0`);
//   every write site here carries its upstream literal's exact tag.
// - player bookkeeping: currentAction/currentSubaction/lastMash live on
//   MlPlayer; the upstream TYPO field `curentAction` (ai.js:1254,
//   CPULedge TOURNAMENTWINNER arm) is WRITE-ONLY upstream (no reader
//   anywhere) and is modeled as MlAiSim slice state (the falcon
//   canEdgeCancel class, M2 task 10), NOT an MlPlayer widening.
// - charAttributes.multiJump is M1-table data (ml_player.h header note);
//   the replay marshals it from the record, task-5 integration wires the
//   CTAB1 lookup. An undefined read (charAttributes absent — impossible
//   in-match) traps.
// - `player[i].grounded` (ai.js:262) is a TOP-LEVEL read playerObject
//   never defines (phys.grounded is the real field) — always undefined,
//   `!(undefined) === true`; the capture records it and the replay
//   marshal hard-asserts undef (quirk q1, AGENT-LOG iter 75).
//
// Every Math.* routes per the sim rules: fdlibm for transcendentals
// (fd_pow/fd_atan/fd_cos/fd_sin/fd_tan — tan vendored since M0 task 3),
// js_min/js_max/js_sign for the JS NaN/-0 semantics, C floor/fabs/fmod
// for the exact ops. Draws via ml_random() (ml_events.h): logged, chained
// on ml_active_rng.
#ifndef ML_AI_H
#define ML_AI_H

#include "input/ai_input.h"
#include "ml_player.h"

// --- the ai.js activeStage read set ----------------------------------------
// aS.ledgePos (Vec2D list), aS.platform / aS.ground (lists of [Vec2D,
// Vec2D] pairs). Strict caps; the replay marshal hard-fails past them.
#define ML_AI_MAX_SURF 16
#define ML_AI_MAX_LEDGE 4

typedef struct {
  Vec2D ledgePos[ML_AI_MAX_LEDGE];
  int ledgePosLen;
  Vec2D ground[ML_AI_MAX_SURF][2];
  int groundLen;
  Vec2D platform[ML_AI_MAX_SURF][2];
  int platformLen;
} MlAiStage;

// --- the god-module slice ---------------------------------------------------
typedef struct {
  MlPlayer *player[4];
  double playerType[4];
  double cS[4];       // characterSelections
  bool turbo;         // gameSettings.turbo (the only gameSettings read)
  const MlAiStage *aS;
  MlAiInput (*bank)[8]; // aiInputBank[4][8] (tagged rows)
  // charAttributes.multiJump per slot (table plane; see header note):
  bool multiJump[4];
  bool multiJumpUndef[4]; // charAttributes absent -> read traps
  // player.curentAction (upstream typo field; slice state, see note):
  bool hasCurentAction[4];
  char curentAction[4][ML_STR_CAP];
} MlAiSim;

// upstream runAI(i) (ai.js:874) — generalAI + the character dispatch.
void ml_runAI(MlAiSim *sim, int i);

// Provided by the driver (mv_out_of_domain class): report + abort, never
// returns.
void ml_ai_out_of_domain(const char *what);

// --- arm-coverage instrument (Tier-B diagnostic; AGENT-LOG iter 75) --------
// Cheap counters at every measured branch arm; the replay prints the
// table under --cover so honest coverage is MEASURED, never assumed.
#define ML_AI_NCOV 64
extern long ml_ai_cov[ML_AI_NCOV];
extern const char *const ml_ai_cov_names[ML_AI_NCOV];

#endif // ML_AI_H
