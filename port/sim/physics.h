// physics.h <- src/physics/physics.js (structure-parallel translation,
// M2 task 5). The per-player update pipeline: physics(i, input) mutates
// player[i] (and, via pushing/grab-release, other players) IN PLACE.
//
// SIM STATE (MlSim): the god-module globals slice physics.js reads —
// player[4] + playerType/characterSelections + gameMode/versusMode +
// the gameSettings slice + the extended stage (surfaces + connected +
// ledges + blastzone) + the module-private ecbSquashData[4] (physics.js
// module state: written ONLY inside physics, so the replay CHAINS it in C
// across records; divergence would surface in ECBp/ECB1 post-state).
//
// ALIAS SITES (M2 rule 10 — each modeled deliberately, probed by capture):
// 1. prevFrameHitboxes.{active,hitList,id} alias hitboxes' arrays after
//    the 3-arg deepObjectMerge (physics.js:1070; ml_player.h). Broken by
//    whole-array reassignment: land() (hitboxes.active = fresh) and
//    turnOffHitboxes (active + hitList fresh). Physics itself never
//    element-writes through them; moves do (dispatch records carry a
//    post-dispatch alias PROBE the replay restores from).
// 2. phys.pos aliases phys.ECB1[0] when land() ran through the ground/
//    platform touching arm: land's newPosition IS newECB[0], and
//    physics.js:847 then assigns ECB1 = newECB — so later COMPONENT writes
//    to pos (SDI, velocity integration, cross-slot pushing) also write
//    ECB1[0] in JS. All pos component writes go through mlp_pos_*
//    helpers; every pos/ECB1 whole-object reassignment breaks the alias.
// 3. ecbSquashData starts as FOUR references to ONE shared nullSquashDatum
//    ({location:null,factor:1}) — but runCollisionRoutine always returns a
//    freshly constructed datum and every mutation site executes after the
//    per-slot reassignment (physics.js:805 precedes :858; :1106 requires
//    factor<1 which the shared datum never has), so the shared object is
//    never written: per-slot VALUE semantics are exact. Documented, not
//    modeled.
//
// SEAMS (driver-provided; the capture records each crossing):
// - mlp_dispatch: actionStates[char][STATE].<phase> move dispatches
//   (moves are tasks 7-12; the recording resyncs post-dispatch state).
// - mlp_hd_*: the 5 hitDetection launch getters (task 6 owns their
//   bodies; args are verified bit-exactly, returns injected).
// - mlp_flags: the actionStates per-(char,state) flag table (move-object
//   data plane; dumped once per capture, drift-checked at capture end).
// - ml_sound_play (ml_events.h): physics' own 4 direct sound sites.
// - sim->hq: dealWithDamagingStageCollision's hitQueue pushes (zero-live
//   on VS stages — no damageType surfaces; M4 target stages are the
//   reachable future domain).
// Render/off-tick planes (documented no-ops): drawVfx, lostStockQueue,
// percentShake (CHECKSUM.md §7), the showDebug DOM block.
#ifndef ML_PHYSICS_H
#define ML_PHYSICS_H

#include "environmental_collision.h"
#include "ml_input.h"
#include "ml_player.h"

// --- extended stage (the physics.js read set of activeStage) ---------------

typedef struct {
  bool present; // false <=> null (no connected surface on this side)
  char type;    // 'g' | 'p' | 'l' | 'r'
  double index;
} MlConnHalf;

typedef struct { MlConnHalf l, r; } MlConnPair;

#define ML_MAX_LEDGES 8

typedef struct {
  char list;    // 'g' (= "ground") | 'p' (= "platform"): ledge[j][0]
  double index; // ledge[j][1]
  double point; // ledge[j][2] (0 | 1)
} MlLedge;

typedef struct {
  Stage s; // the five surface lists (stage_types.h; envcoll consumes this)
  bool hasConnected;
  MlConnPair connGround[ML_MAX_SURFACES];   // connected[0]
  int connGroundCount;
  MlConnPair connPlatform[ML_MAX_SURFACES]; // connected[1]
  int connPlatformCount;
  MlLedge ledge[ML_MAX_LEDGES];
  int ledgeCount;
  Box2D blastzone;
  // REBIRTH's read set (M2 task 7; STAB1 carries the same data — the
  // capture projects it per record like the rest of the stage argument):
  Vec2D respawnPoints[4];
  double respawnFace[4];
  int respawnCount; // 0 in captures that don't project it (physics spec)
} MlStageX;

// --- actionStates flag table (per char x state; capture-dumped) ------------

typedef struct {
  const char *name; // move object's `name` (dispatch-record verification)
  // truthiness-consumed flags (undefined counts as false):
  bool canEdgeCancel;
  bool disableTeeter;
  bool inGrab;
  bool headBonk;
  bool specialWallCollide;
  bool canPassThrough;
  bool dead;
  bool missfoot;
  bool ignoreCollision;
  // wallJumpAble is ASSIGNED to phys.canWallJump verbatim (undef-at-rest):
  JsBool wallJumpAble;
  bool hasLandType;
  double landType;
  bool hasAirborneState;
  char airborneState[ML_STR_CAP];
  bool hasCanGrabLedge;
  bool canGrabLedge[2];
} MlAsFlags;

// --- hitQueue rows physics can push (dealWithDamagingStageCollision) -------

#define ML_HQ_CAP 8

typedef struct {
  double i;
  Vec2D normal;
  double angular;
  bool corner;
  double damageTypeIndex;
} MlHqRow;

// --- the sim state slice ----------------------------------------------------

typedef struct {
  MlPlayer player[4];
  bool playerPresent[4]; // playerType[k] > -1 slots carry real players
  double playerType[4];
  double characterSelections[4];
  double gameMode;
  double versusMode; // truthiness consumed
  // gameSettings slice:
  double lCancelType;
  bool turbo;
  double phantomThreshold;
  double tapJumpOff[4]; // tapJumpOffp1..4 (turbo interrupt path only)
  MlStageX stage;
  // physics.js module state, chained across records (header note):
  SquashDatum ecbSquashData[4];
  // alias flags (rule 10; header note). Set from capture probes at record
  // entry and at every dispatch return; compared at record post.
  bool aliasPosEcb1[4];
  bool aliasHbActive[4];
  bool aliasHbHitList[4];
  bool aliasHbId[4];
  // physics' own hitQueue pushes this call (reset per boundary call):
  MlHqRow hq[ML_HQ_CAP];
  int hqCount;
  // transient (reset per findAndResolveCollisions): land() assigned pos =
  // newECB[0] — physics.js:847 then makes that the pos-ECB1[0] alias.
  bool landEcbBottom;
} MlSim;

// --- dispatch seam -----------------------------------------------------------

typedef enum { DX_NUM, DX_STR, DX_VEC, DX_BOOL } MlDispExtraKind;

typedef struct {
  MlDispExtraKind kind;
  double num;
  const char *str;
  Vec2D vec;
  bool b;
} MlDispExtra;

typedef struct {
  const char *phase; // "init" | "main" | "land" | "onWallCollide"
  const char *state; // the actionStates KEY being dispatched
  double charId;     // characterSelections[<lookup slot>]
  double slot;       // the move's first argument
  int extraCount;    // extra args after (slot, input)
  MlDispExtra extras[2];
} MlDispCall;

// Provided by the driver: verify the dispatch against the recording and
// RESYNC sim->player[] + alias flags from the recorded post-dispatch state.
extern void mlp_dispatch(MlSim *sim, const MlDispCall *call);

// Provided by the driver: hitDetection launch getters (task 6 surface) —
// args verified bit-exactly against the recording, return value injected.
extern double mlp_hd_getLaunchAngle(MlSim *sim, double trajectory,
                                    double knockback, bool hasReverse,
                                    bool reverse, double x, double y, double v);
extern double mlp_hd_getHorizontalVelocity(MlSim *sim, double knockback,
                                           double angle);
extern double mlp_hd_getVerticalVelocity(MlSim *sim, double knockback,
                                         double angle, bool grounded,
                                         double trajectory);
extern double mlp_hd_getHorizontalDecay(MlSim *sim, double angle);
extern double mlp_hd_getVerticalDecay(MlSim *sim, double angle);

// Provided by the driver: actionStates[charId][state] flag lookup (the
// capture's frame-0 asFlags dump). Missing state = out of captured domain.
extern const MlAsFlags *mlp_flags(const MlSim *sim, double charId,
                                  const char *state);

// Provided by the driver: fatal domain trap (mirrors where upstream would
// throw, or where the captured domain is exceeded — rule 7).
extern void ml_phys_out_of_domain(const char *what);

// --- the boundary -------------------------------------------------------------

// physics(i, input): input is the slot's input buffer, depth >= 4 read
// (indices 0..3). Mutates sim in place.
void ml_physics(MlSim *sim, double i, const MlInput in[4]);

// land is exported upstream (moves call it too — their calls live inside
// the dispatch seam; this entry is physics' own).
void ml_land(MlSim *sim, double i, Vec2D newPosition, bool posIsNewEcbBottom,
             double t, double j, bool normalPresent, Vec2D normal,
             const MlInput in[4]);

#endif // ML_PHYSICS_H
