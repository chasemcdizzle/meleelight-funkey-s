// action_state_shortcuts.h — structure-parallel C translation of
// src/physics/actionStateShortcuts.js (M2 task 4) + the actionStates
// dispatch-table scaffolding (setupActionStates/actionStates).
//
// Value modeling (capture-FIRST, measured over the g01/g04/g06 asshort
// captures — port/sim/calib/FORMAT.md "asshort"):
// - functions take their upstream READ SET explicitly (the projected
//   record args): small per-function state slices, `input` as the first
//   4 entries of the slot's 8-deep buffer (the module reads history
//   depth <= 3), char id for every charAttributes/intangibility read —
//   those come from the M1 generated tables (ml_tables.h, CTAB1), never
//   from snapshots.
// - truthiness-only JS parameters (shieldstun bool|null, applyDouble
//   bool|undefined, lock true|null, inCSS bool) are C bools: only their
//   truthiness is consumed upstream (measured domains in FORMAT.md).
// - `gameSettings["tapJumpOffp"+(p+1)] == false` ("== is on purpose"):
//   the captured domain is the number 0, so the loose-eq is (v == 0) —
//   the marshaller rejects non-number values (rule 7).
// - side effects (sound plays, seeded-RNG draws, actionStates/moves-table
//   dispatches) go through the ml_events.h seams and are compared as part
//   of the boundary.
#ifndef ACTION_STATE_SHORTCUTS_H
#define ACTION_STATE_SHORTCUTS_H

#include <stdbool.h>

#include "ml_events.h"
#include "ml_player.h" // MlHitboxes, Vec2D
#include "ml_input.h"

// --- JS-shaped results -------------------------------------------------------

// checkForIASA can fall off the end (non-aerial arm / timer <= IASATimer):
// JS returns undefined — a tri-state, not a bool (rule 8's undef-echo class).
typedef enum { AS_FALSE = 0, AS_TRUE = 1, AS_UNDEF = -1 } AsTri;

// the [flag, payload] pairs the checkFor* family returns:
// [true,"NAME"] | [false,false] | [true,0|1] | [false,0] (checkForAerials).
typedef enum { AS_P_FALSE, AS_P_STR, AS_P_NUM } AsPayloadKind;
typedef struct {
  bool flag;
  AsPayloadKind kind;
  const char *str;
  double num;
} AsPair;

// --- actionStates dispatch-table scaffolding ---------------------------------
// Upstream: `export const actionStates = []` +
// `setupActionStates(index, val)` deep-copies each character's move table
// at boot (actionStateShortcuts.js:612-615). The C table maps
// (charId, stateName) -> a move definition registered by the move
// clusters (tasks 7-12; MlMoveDef stays opaque until then). as_dispatch
// notes "<phase>:<NAME>" on the event seam — the verified observable of
// this cluster — and returns the registered def (NULL until registration)
// for the integration task to call through.
typedef struct MlMoveDef MlMoveDef;

typedef struct {
  const char *name;
  const MlMoveDef *def;
} AsMoveEntry;

#define AS_MAX_STATES 192
#define AS_CHARS 5

typedef struct {
  int count;
  AsMoveEntry entries[AS_MAX_STATES];
} AsMoveTable;

extern AsMoveTable as_action_states[AS_CHARS];

void as_setupActionStates(int charId, const AsMoveEntry *list, int count);
const MlMoveDef *as_lookup(int charId, const char *state);
const MlMoveDef *as_dispatch(int charId, const char *state, const char *phase);

// --- per-function state slices ------------------------------------------------

typedef struct {
  double face;
  bool inCSS;
  Vec2D pos;
  Vec2D shieldPosition;     // in/out
  Vec2D shieldPositionReal; // out
} AsShieldTiltState;

typedef struct {
  bool grounded;   // in/out
  Vec2D kDec;      // in/out
  Vec2D kVel;      // in/out
  double shieldHP; // in/out
  bool shielding;  // in/out
} AsShieldDepState;

typedef struct {
  double shieldAnalog;
  double shieldSize;
} AsShieldSizeOut;

typedef struct {
  bool hasIASATimer; // runtime-added player field (rule 3): absent -> the
  double IASATimer;  // `timer > IASATimer` comparison is false (undefined)
  double timer;
  double face;
  bool doubleJumped;
  double jumpsUsed;
} AsIasaState;

typedef struct {
  double gameMode;
  double versusMode;      // truthiness consumed
  double playerType[4];
  bool stocksPresent[4];  // inactive slots are not read upstream
  double stocks[4];
} AsFinalDeathState;

typedef struct {
  double frame;
  const char *name;
} AsSoundRow;

typedef struct {
  const char *actionState;
  double *face;      // checkForSpecials (computed up front) may mutate it
  bool doubleJumped;
  double jumpsUsed;
  double bTurnaroundDirection; // read by the internal checkForSpecials
  double bTurnaroundTimer;
  bool grounded;
  MlHitboxes *hitboxes;
} AsTurboAirState;

typedef struct {
  const char *actionState;
  double bTurnaroundDirection;
  double bTurnaroundTimer;
  double *face;
  bool grounded;
  MlHitboxes *hitboxes;
  bool *dashbuffer;     // runtime-added (rule 3): set only on TILTTURN arm
  bool *hasDashbuffer;
} AsTurboGroundState;

// --- the boundary -------------------------------------------------------------

void as_randomShout(double charId); // KO-shout seeded-RNG + sound sites
void as_executeIntangibility(const char *actionStateName, int charId,
                             double timer, double *intangibleTimer,
                             double *hurtBoxState);
void as_playSounds(const AsSoundRow *rows, int count, double timer);
bool as_isFinalDeath(const AsFinalDeathState *st);
double as_getAngle(double x, double y);
void as_turnOffHitboxes(MlHitboxes *hb);
void as_shieldTilt(bool shieldstun, int charId, AsShieldTiltState *st,
                   const MlInput in[4]);
void as_reduceByTraction(bool applyDouble, int charId, double *cVelX);
void as_airDrift(int charId, double *cVelX, const MlInput in[4]);
void as_fastfall(int charId, double *cVelY, bool *fastfalled,
                 const MlInput in[4]);
void as_shieldDepletion(int charId, AsShieldDepState *st, const MlInput in[4]);
void as_shieldSize(bool lock, int charId, double shieldHP,
                   AsShieldSizeOut *out, const MlInput in[4]);
bool as_mashOut(const MlInput in[4]);
AsPair as_checkForSmashes(double *face, const MlInput in[4]);
AsPair as_checkForTilts(double face, const MlInput in[4],
                        bool reversePresent, double reverseArg);
AsTri as_checkForIASA(const AsIasaState *st, double charId, double tapJumpOff,
                      const MlInput in[4], bool isAerial);
AsPair as_checkForSpecials(double *face, double bTurnaroundTimer,
                           double bTurnaroundDirection, bool grounded,
                           const MlInput in[4]);
AsPair as_checkForAerials(double face, const MlInput in[4]);
bool as_checkForDash(double face, const MlInput in[4]);
bool as_checkForSmashTurn(double face, const MlInput in[4]);
bool as_tiltTurnDashBuffer(double face, const MlInput in[4]);
bool as_checkForTiltTurn(double face, const MlInput in[4]);
AsPair as_checkForJump(double tapJumpOff, const MlInput in[4]);
bool as_checkForDoubleJump(double tapJumpOff, const MlInput in[4]);
bool as_checkForMultiJump(double tapJumpOff, const MlInput in[4]);
bool as_checkForSquat(const MlInput in[4]);
bool as_turboAirborneInterrupt(AsTurboAirState *st, double charId,
                               double tapJumpOff, const MlInput in[4]);
bool as_turboGroundedInterrupt(AsTurboGroundState *st, double charId,
                               double tapJumpOff, const MlInput in[4]);

// Provided by the host (replay driver / future sim): fatal domain trap —
// reached only where upstream would throw (e.g. unknown intangibility
// state) or where the captured domain is exceeded.
void ml_asshort_out_of_domain(const char *what);

#endif // ACTION_STATE_SHORTCUTS_H
