// ml_player.h <- src/main/player.js (structure-parallel value model,
// M2 task 2). C model of upstream's playerObject/physicsObject/
// createHitboxes/ActiveHitbox — every field sim code reads or writes,
// including the 7 checksummed fields (oracle/CHECKSUM.md §2) with the
// ENTIRE phys object. Field order follows upstream construction order;
// canon serialization order (sorted keys) is the serializer's job
// (port/sim/calib/player_canon.c).
//
// Finalized capture-FIRST (prevention rule 7) from the M2 task 2 player
// captures over g01/g04/g06 (21,600 post-update(i) snapshots; shape
// survey: port/sim/calib/survey-shapes.js). The captured domain pins:
//
// - hitboxes.id[j] entries take exactly TWO shapes (survey-measured):
//   * constructor shape (player.js:7-16 ActiveHitbox): keys
//     {angle,bk,dmg,kg,offset,size,sk,type}, offset a single Vec2D;
//   * chars-data shape (move code aliases player.hitboxes.id[j] to
//     chars[c].hitboxes entries, e.g. fox ATTACKAIRF.js:26): keys
//     {angle,bk,clank,dmg,hitAirborne,hitGrounded,kg,offset,size,sk,
//     throwextra,type}, offset a PER-FRAME ARRAY of Vec2D (len 1..24
//     captured). Modeled as a tagged shape (rule 3: key presence is
//     modeled explicitly, never guessed).
// - Runtime-ADDED fields (absent until first written; presence modeled
//   per rule 3): player.IASATimer, player.inAerial, hit.reverse,
//   hitboxes.frames, phys.{grabTech,laserCombo,ledgeHangTimer,autocancel,
//   rollOutCharge,rollOutChargeAttempt,rollOutCharging,rollOutPlayerHit,
//   rollOutPlayerHitTimer,rollOutVel,rollOutWallHit,shieldBreakerCharge,
//   shieldBreakerChargeAttempt,shieldBreakerCharging}. Note phys.autocancel
//   (lowercase, runtime-added by move code) COEXISTS with constructor
//   autoCancel — two distinct upstream fields, carried verbatim.
// - phys.canWallJump holds undefined AT REST (survey: undef in 18,837 of
//   21,600 snapshots — constructor sets false, sim code assigns undefined
//   back). Rule 8 extended to bools: JsBool models undef explicitly.
// - phys.passing is runtime-added by physics.js:1067 as the FIRST
//   statement of every physics() call, so it is present in every
//   post-update snapshot — modeled as always-present.
//
// NOT modeled here (documented projections, FORMAT.md "player spec"):
// - charAttributes / charHitboxes: immutable per-character table data —
//   the C sim reads the M1-generated tables (ml_tables.h, CTAB1), never a
//   snapshot copy. Zero in-match writes upstream (only menu-time
//   css.js:150/171 assign them).
// - percentShake: written off-tick by wall-clock setTimeout callbacks from
//   the NATIVE RNG — the checksum spec's one player-field exclusion
//   (oracle/CHECKSUM.md §7).
#ifndef ML_PLAYER_H
#define ML_PLAYER_H

#include "util/box2d.h"

// Rule-8 extension: a JS boolean field that can hold undefined at rest.
typedef struct { bool isUndef; bool v; } JsBool;

static inline JsBool js_bool(bool v) {
  JsBool b; b.isUndef = false; b.v = v; return b;
}
static inline JsBool js_bool_undef(void) {
  JsBool b; b.isUndef = true; b.v = false; return b;
}

// --- ActiveHitbox / chars-data hitbox spec (hitboxes.id[j]) ----------------
// Captured offset arrays reach 24 entries (falcon); cap with headroom —
// the strict marshaller hard-fails past it (rule 7), never truncates.
#define ML_HB_OFFSET_CAP 64

typedef enum {
  ML_HB_CONSTRUCTOR, // player.js ActiveHitbox: offset is one Vec2D
  ML_HB_CHARDATA,    // chars[c].hitboxes entry: offset is Vec2D[frames]
} MlHitboxShape;

typedef struct {
  MlHitboxShape shape;
  double size;
  Vec2D offset;                     // ML_HB_CONSTRUCTOR / offsetSingle
  Vec2D offsetArr[ML_HB_OFFSET_CAP]; // ML_HB_CHARDATA (per-frame array)
  int offsetLen;                     // ML_HB_CHARDATA (per-frame array)
  // ML_HB_CHARDATA with a SINGLE Vec2D offset: upstream createHitbox is
  // called with a bare Vec2D for the throw hitboxes (e.g. fox
  // attributes.js:749 throwup) — the 12-key chars-data key set with an
  // OBJECT offset (M2 task 8; measured on the fox THROW* sweep records).
  bool offsetSingle;
  double dmg;
  double angle;
  double kg;
  double bk;
  double sk;
  double type;
  // ML_HB_CHARDATA only:
  double clank;
  double hitAirborne;
  double hitGrounded;
  bool throwextra;
} MlHitboxSpec;

// --- createHitboxes (player.js:17-24) ---------------------------------------
// hitList: captured max length 1; cap = 4 (one entry per opponent slot).
#define ML_HITLIST_CAP 4

typedef struct {
  bool active[4];
  double frame;
  bool hasFrames;   // runtime-added by move code (rule 3)
  double frames;
  double hitList[ML_HITLIST_CAP];
  int hitListLen;
  MlHitboxSpec id[4];
} MlHitboxes;

// --- hit (player.js:138-146) -------------------------------------------------
typedef struct {
  double knockback;
  double hitlag;
  double hitstun;
  double angle;
  Vec2D hitPoint;
  bool powershield;
  double shieldstun;
  bool hasReverse;  // runtime-added (rule 3)
  bool reverse;
} MlHit;

// --- physicsObject (player.js:25-102) ----------------------------------------
#define ML_INTERP_CAP 4

typedef struct {
  Vec2D cVel;
  Vec2D kVel;
  Vec2D kDec;
  Vec2D pos;
  Vec2D posPrev;
  Vec2D posDelta;
  bool grounded;
  double airborneTimer;
  bool fastfalled;
  double face;
  Vec2D ECBp[4];
  Vec2D ECB1[4];
  Vec2D ECB2[4];
  // Rule-8 undef-at-rest masks for ECBp/ECB1 COMPONENTS (bit 2k = point
  // k's x, bit 2k+1 = its y). Measured domain (M2 task 5 physics capture,
  // survey-shapes): frame-1 pre-physics states carry Vec2D(undefined,
  // undefined) in exactly these two arrays. The VALUE slots hold the
  // canonical NaN (ToNumber(undefined) — every in-sim consumer is
  // arithmetic/comparison), the masks exist so serialization echoes
  // `undef` verbatim. Every whole-array assignment clears the mask.
  uint8_t ecbpUndef;
  uint8_t ecb1Undef;
  double onSurface[2];
  bool doubleJumped;
  double shieldHP;
  double shieldSize;
  double shieldAnalog;
  bool shielding;
  Vec2D shieldPosition;
  Vec2D shieldPositionReal;
  double shieldStun;
  bool powerShieldActive;
  bool powerShieldReflectActive;
  bool powerShielded;
  double onLedge;
  Box2D ledgeSnapBoxF;
  Box2D ledgeSnapBoxB;
  double ledgeRegrabTimeout;
  bool ledgeRegrabCount;
  Box2D hurtbox;
  double hurtBoxState;
  double intangibleTimer;
  double invincibleTimer;
  bool lCancel;
  double lCancelTimer;
  bool autoCancel;
  double landingLagScaling;
  bool passFastfall;
  bool jabCombo;
  bool sideBJumpFlag;
  bool charging;
  double chargeFrames;
  double stuckTimer;
  double techTimer;
  double grabbedBy;
  double grabbing;
  bool dashbuffer;
  double jumpType;
  double jumpSquatType;
  double wallJumpTimer;
  JsBool canWallJump;  // undef-at-rest (rule 8; survey-measured)
  double upbAngleMultiplier;
  bool thrownHitbox;
  double thrownHitboxOwner;
  double landingMultiplier;
  double wallJumpCount;
  MlHitboxes prevFrameHitboxes;
  Vec2D interPolatedHitbox[ML_INTERP_CAP][4];
  int interPolatedHitboxLen;
  Vec2D interPolatedHitboxPhantom[ML_INTERP_CAP][4];
  int interPolatedHitboxPhantomLen;
  bool isInterpolated;
  double facePrev;
  double jumpsUsed;
  double releaseFrame;
  double vCancelTimer;
  double shoulderLockout;
  double inShine;
  bool jabReset;
  double outOfCameraTimer;
  double rollOutDistance;
  double bTurnaroundTimer;
  double bTurnaroundDirection;
  double groundAngle;
  bool raptorBoost;
  // runtime-added by physics.js:1067 (1st stmt of every physics() call):
  // always present POST-update; a frame-1 PRE-physics state (M2 task 5's
  // capture marshals those too) legitimately lacks it — presence modeled.
  bool hasPassing;
  bool passing;
  // runtime-added, presence modeled (rule 3):
  bool hasGrabTech;             bool grabTech;
  bool hasPhantomDamage;        double phantomDamage;       // hitDetection phantom arm (M2 task 6; zero-live over the goldens)
  bool hasStageDamageImmunity;  double stageDamageImmunity; // hitDetection stage-damage arm (M4 target stages)
  bool hasLaserCombo;           bool laserCombo;            // phys-level (falco laser code); distinct from player.laserCombo
  bool hasLedgeHangTimer;       double ledgeHangTimer;
  bool hasAutocancelLower;      bool autocancelLower;       // upstream key "autocancel" (≠ constructor autoCancel)
  bool hasRollOutCharge;        double rollOutCharge;       // puff ROLLOUT family
  bool hasRollOutChargeAttempt; bool rollOutChargeAttempt;
  bool hasRollOutCharging;      bool rollOutCharging;
  bool hasRollOutPlayerHit;     bool rollOutPlayerHit;
  bool hasRollOutPlayerHitTimer;double rollOutPlayerHitTimer;
  bool hasRollOutVel;           double rollOutVel;
  bool hasRollOutWallHit;       bool rollOutWallHit;
  bool hasShieldBreakerCharge;        double shieldBreakerCharge;  // marth NEUTRALSPECIAL family (M2 task 9)
  bool hasShieldBreakerChargeAttempt; bool shieldBreakerChargeAttempt;
  bool hasShieldBreakerCharging;      bool shieldBreakerCharging;
} MlPhysics;

// --- playerObject (player.js:126-167) ----------------------------------------
// String caps: longest captured actionState is well under 40; the strict
// marshaller hard-fails on overflow (rule 7).
#define ML_STR_CAP 48

typedef struct {
  MlPhysics phys;
  char actionState[ML_STR_CAP];
  char prevActionState[ML_STR_CAP];
  double timer;
  // charAttributes / charHitboxes: via M1 ml_tables, not modeled (header note)
  bool showLedgeGrabBox;
  bool showECB;
  bool showHitbox;
  double spawnWaitTime;
  MlHitboxes hitboxes;
  MlHit hit;
  double percent;
  double stocks;
  bool miniView;
  Vec2D miniViewPoint;
  bool inCSS;
  double furaLoopID;
  // runtime-added by marth NEUTRALSPECIAL* (Howl play id, furaLoopID's
  // cousin; first live appearance g05 moves-falco, M2 task 9):
  bool hasShieldBreakerID;
  double shieldBreakerID;
  // percentShake: excluded (header note; CHECKSUM.md §7)
  double shineLoop;
  bool laserCombo;
  double rotation;
  Vec2D rotationPoint;
  char colourOverlay[ML_STR_CAP];
  bool colourOverlayBool;
  char currentAction[ML_STR_CAP];
  char currentSubaction[ML_STR_CAP];
  double difficulty;
  double lastMash;
  bool hasHit;
  double shocked;
  double burning;
  // runtime-added, presence modeled (rule 3):
  bool hasIASATimer; double IASATimer;
  bool hasInAerial;  bool inAerial;
} MlPlayer;

// --- type-specialized deepCopy / deepCopyObject (M2 task 2) -----------------
// Upstream copies player-shaped values with the generic reflective
// deepCopyObject (src/main/util/deepCopy.js). The C value model is
// BY-VALUE throughout (fixed caps, no pointers), so struct assignment IS
// the type-specialized deep copy — kept as named functions so call sites
// stay structure-parallel and a future pointer field cannot silently turn
// them shallow (replay_player.c probes copy independence every record).
static inline void ml_player_copy(MlPlayer *dst, const MlPlayer *src) {
  *dst = *src;
}
static inline void ml_hitboxes_copy(MlHitboxes *dst, const MlHitboxes *src) {
  *dst = *src;
}

// physics.js:1070 `deepObjectMerge(true, prevFrameHitboxes, hitboxes)` —
// the 3-arg call leaves exclusionList undefined, which FALSIFIES the
// recursion guard (`typeof obj[key]==='object' && exclusionList && …`) in
// src/main/util/deepCopyObject.js:23, so the "deep merge" executes as a
// SHALLOW per-key reference assignment: prevFrameHitboxes.{active,hitList,
// id} become ALIASES of hitboxes' arrays (M2 rule 10 — verify upstream
// "copy" helpers' executed semantics; they may alias). As a VALUE
// operation at the merge point this equals a field-wise copy of source
// keys with target-only keys retained (hasFrames survives when the source
// lacks frames); the LIVE aliasing consequences inside a frame belong to
// the physics cluster translation (fix_plan §M2 task 5), which owns the
// only call site.
static inline void ml_hitboxes_merge_from(MlHitboxes *target,
                                          const MlHitboxes *source) {
  const bool targetHasFrames = target->hasFrames;
  const double targetFrames = target->frames;
  *target = *source;
  if (!source->hasFrames && targetHasFrames) {
    // `for key in obj` never visits a key the source lacks: the target's
    // own `frames` property persists through the merge.
    target->hasFrames = true;
    target->frames = targetFrames;
  }
}

#endif // ML_PLAYER_H
