// hit_detection.h <- src/physics/hitDetection.js (structure-parallel
// translation, M2 task 6). The queue-ordered hit-resolution pipeline:
// hitDetect fills the hitQueue from hitbox/shield/hurtbox intersections,
// executeHits resolves rows in queue order (clank/shield/grab/sleep/
// regular arms), checkPhantoms settles deferred phantom damage; plus the
// launch-parameter getters physics.js consumes at hitlag exit (task 5's
// oracle-fed getter seams become these real bodies).
//
// MODULE STATE (HdQueues): the exported `hitQueue` + `phantomQueue`
// arrays. Rows enter from OTHER clusters too (THROW moves push during
// update; physics pushes damaging-stage rows), so the replay marshals the
// queues from each record's pre-state envelope, never chains them.
//
// VALUE MODEL notes (capture-FIRST, measured over g01/g04/g06):
// - hitQueue rows are 6- or 7-element arrays [v, a, h, shieldHit,
//   isThrow, drawBounce(, phantom)]; a is a slot number, or (stage
//   damage, zero-live on VS stages) physics' collisionData object
//   {angular, corner, normal}. HdRow models both (aIsObj + hasPhantom).
// - hitbox spec reads go through the two measured shapes (ml_player.h
//   MlHitboxSpec): CHARDATA carries clank/hitGrounded/hitAirborne/
//   throwextra + a per-frame offset array; CONSTRUCTOR (player.js
//   ActiveHitbox) LACKS those keys — upstream reads give undefined
//   (falsy / NaN at arithmetic), modeled by the hb_* helpers in the .c.
// - charAttributes.weight comes from the M1 CTAB1 tables (ml_tables),
//   never from snapshots (the asshort/task-4 data path).
// - `player[v].phys.hurtboxState` (executeRegularHit's invincibility
//   arm) is an upstream TYPO for hurtBoxState: the lowercase key never
//   exists, undefined > 0 is false — the bluntHit arm is dead code
//   upstream and is translated (with its internal throw-trap) but
//   unreachable from executeRegularHit.
//
// ALIAS SITES (M2 rule 10): hitList writes (hitDetect's push,
// checkPhantoms' splice) write THROUGH the prevFrameHitboxes.hitList
// alias when live (sim->aliasHbHitList); turnOffHitboxes assigns fresh
// active/hitList arrays and BREAKS those aliases; executeRegularHit's
// throw arm reassigns phys.pos (fresh Vec2D) and breaks pos-ECB1[0].
//
// SEAMS (driver-provided): hd_dispatch (move dispatches: CATCHCUT/
// DAMAGEN2/DAMAGEFLYN/GUARD/SHIELDBREAKFALL/FURASLEEPSTART/CAPTUREPULLED/
// THROWNFALCONDIVE/WAIT/DOWNDAMAGE/CAPTUREDAMAGE/CAPTURECUT .init +
// .onClank/.onPlayerHit — moves are tasks 7-12; the recording resyncs
// players + hq + aliases), hd_flags (the hdFlags frame-0 dump:
// canBeGrabbed/crouch/downed/specialClank/specialOnHit/vCancel/name),
// ml_sound_play / ml_sound_stop / ml_random (ml_events.h; screenShake =
// 4 seeded draws per regular hit, render effect discarded),
// ml_hd_out_of_domain (rule-7 traps at upstream throw sites / outside
// the captured domain). Render/off-tick no-ops: drawVfx (none of the
// module's vfx names is "circleDust", so no RNG), percentShake
// (CHECKSUM.md section 7), screenShake's fg1.translate.
#ifndef ML_HIT_DETECTION_H
#define ML_HIT_DETECTION_H

#include "physics.h" // MlSim, MlDispExtra (god-module slice + seam types)

// --- hitQueue / phantomQueue value model -------------------------------------

#define HD_HQ_CAP 32
#define HD_PHQ_CAP 16
#define HD_GRABQ_CAP 8

typedef struct {
  double v; // row[0]
  bool aIsObj; // stage-damage rows: row[1] is {angular, corner, normal}
  double a;      // row[1] when a slot number
  double angular;
  bool corner;
  Vec2D normal;
  double h;         // row[2]: hitbox index, or damage-type index (stage)
  bool shieldHit;   // row[3]
  bool isThrow;     // row[4]
  bool drawBounce;  // row[5]
  bool hasPhantom;  // 7-element row
  bool phantom;     // row[6]
} HdRow;

typedef struct {
  HdRow hq[HD_HQ_CAP];
  int hqCount;
  double phq[HD_PHQ_CAP][2]; // phantomQueue rows [a, v]
  int phqCount;
} HdQueues;

// --- hdFlags: the actionStates data plane hitDetection branches on -----------

typedef struct {
  const char *name; // move object's `name` (dispatch-record verification)
  // truthiness-consumed flags (undefined counts as false):
  bool canBeGrabbed;
  bool crouch;
  bool downed;
  bool specialClank;
  bool specialOnHit;
  bool vCancel;
} HdFlags;

// Provided by the driver: hdFlags lookup (frame-0 capture dump; missing
// state = out of captured domain — upstream would throw on the deref).
extern const HdFlags *hd_flags(double charId, const char *state);

// Provided by the driver: verify the dispatch against the recording and
// RESYNC sim->player[] + alias flags + q->hq from the recorded post.
typedef struct {
  const char *phase; // "init" | "onClank" | "onPlayerHit"
  const char *state; // the actionStates KEY being dispatched
  double charId;
  double slot;       // the move's first argument
  int extraCount;
  MlDispExtra extras[1];
} HdDispCall;

extern void hd_dispatch(MlSim *sim, HdQueues *q, const HdDispCall *call);

// Provided by the driver: fatal domain trap (mirrors upstream throw sites
// or captured-domain overruns — rule 7).
extern void ml_hd_out_of_domain(const char *what);

// --- the pipeline boundary (mutation captures) --------------------------------

void hd_resetHitQueue(HdQueues *q);
// setPhantonQueue(val): the captured domain is endGame's `[]` (zero-live
// over the goldens); the C form takes the new row list explicitly.
void hd_setPhantonQueue(HdQueues *q, double rows[][2], int count);
void hd_hitDetect(MlSim *sim, HdQueues *q, double p);
void hd_executeHits(MlSim *sim, HdQueues *q);
void hd_checkPhantoms(MlSim *sim, HdQueues *q);

// --- pure boundaries (live callers: physics.js hitlag exit + article.js) ------

typedef struct { double kg, bk, sk; } HdKbSpec; // getKnockback's hb read set

double hd_getKnockback(HdKbSpec hb, double damagestaled, double damageunstaled,
                       double percent, double weight, bool crouching,
                       bool vCancel);
// getLaunchAngle's ONLY player read is player[v].phys.grounded, guarded by
// knockback < 80 (JS && short-circuit): the caller supplies it (grounded
// is an always-present plain bool in MlPhysics, so the eager read is safe;
// the capture projects it lazily — null when upstream never reads it).
double hd_getLaunchAngle(double trajectory, double knockback, bool hasReverse,
                         bool reverse, double x, double y, bool grounded);
double hd_getHorizontalVelocity(double knockback, double angle);
double hd_getVerticalVelocity(double knockback, double angle, bool grounded,
                              double trajectory);
double hd_getHorizontalDecay(double angle);
double hd_getVerticalDecay(double angle);
double hd_getHitstun(double knockback);
bool hd_segmentSegmentCollision(Vec2D a1, Vec2D a2, Vec2D b1, Vec2D b2);
// knockbackSounds reads v only as characterSelections[v]: char id arg
// (read-set projection; internal call sites pass the sim's value).
void hd_knockbackSounds(double type, double knockback, double charId);

#endif // ML_HIT_DETECTION_H
