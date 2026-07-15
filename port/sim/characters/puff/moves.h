// characters/puff/moves.h <- src/characters/puff/moves/ (M2 task 12).
// The puff per-char move set: 71 move objects, structure-parallel one .c
// per upstream file under characters/puff/moves/ plus the two helper
// modules at characters/puff/ level (puffMultiJumpDrift.js /
// puffNextJump.js -> puff_multi_jump_drift.c / puff_next_jump.c). Fifth
// and LAST per-char cluster — task 8's recipe.
//
// TABLE OVERRIDES (rule 15's origin map): puff's index carries FURAFURA /
// JUMPAERIALB / JUMPAERIALF, which OVERRIDE the shared states on table 1
// ({...baseActionStates, ...puffMoves}). Puff's FURAFURA is TRIVIAL
// (WAIT.init only — no furaloop/furaLoopID: the shared FURAFURA's Howl-id
// chain state never enters this cluster; measured); JUMPAERIALB/F are
// init-only puffNextJump delegates. The shared versions stay task 7's on
// every other table.
//
// DISPATCH SHAPES (all verbatim to upstream):
// - direct puff-module imports (puff.JAB2.init, ...): puff_<NAME> defs.
// - `puff[b[1]].init(p,input)` — the puff module index (imported by nearly
//   every puff move as `import puff from "./index"`): puff_move_def() via
//   puff_moves_init (also serves puffNextJump's COMPUTED
//   "AERIALTURN"/"JUMPAERIAL" + (1 + jumpsUsed) keys — a missing key is
//   the upstream TypeError -> mv_out_of_domain; AERIALTURN6/JUMPAERIAL6
//   are unreachable: every multijump dispatch is jumpsUsed<5-guarded or
//   capped by JUMPAERIAL5's armless interrupt).
// - `actionStates[characterSelections[grabbing]].THROWNPUFF*.init(
//   grabbing, input)` — TABLE dispatch on the victim's char (2-arg, the
//   marth convention): mv_dispatch.
// - checkForIASA's characterSelections==1 arm (actionStateShortcuts.js
//   PUFFMOVES[a[1]].init) — the puff module IS registered via
//   mv_register_char_module(1, puff_move_def). NOTE the arm is
//   dead-by-construction upstream (only fox/falco/falcon aerials call
//   checkForIASA, each only for its own char); puff's ATTACKAIRF/B/U
//   interrupts INLINE their multijump/aerial logic — translated verbatim
//   in their files (marth precedent).
// - onPlayerHit(p) on NEUTRALSPECIAL{GROUND,AIR,GROUNDTURN}
//   (hitDetection.js:493's specialOnHit arm) and onWallCollide(p,input,
//   wallFace,wallNum) on NEUTRALSPECIALAIR (physics.js:122's
//   specialWallCollide arm) — routed through mv_register_special_phases
//   (the task-10 hook; puff_special_phase below).
//
// DATA PLANES: framesData via mv_frames; move-object data arrays
// (FORWARDSMASH/ATTACKDASH/THROWBACK setVelocities, SIDESPECIAL*
// groundVelocities/airVelocities, CLIFF* offset/setVelocities,
// THROWN*.offset incl. authored expressions, THROWNPUFFBACK.offsetVel
// (dead — its arm is commented out upstream), canGrabLedge pairs) come
// from the capture's frame-0 mvData puff dump (rule 15) through
// mv_puff_* — never retyped. charHitboxes assigns go through
// pf_assign_hitbox_id — like mv_assign_hitbox_id but with dmg/size fed
// from the record's "chd" PRE-PROJECTION (the executed char-data VALUE
// plane): puff's rollout writes id[0..2].dmg after release EVEN WHEN
// UNCHARGED — through whatever STALE id objects the previous move
// assigned (cross-move provenance; MEASURED live on g04: jab1's dmg 3->7
// at frame 1038) — and sing writes id[0].size. The plane is measured per
// record, never assumed pristine CTAB1 (the falcon-canEdgeCancel /
// marth-dmg class, resolved at CLASS level this task).
//
// STRUCTURE NOTES (measured per-file diffs, 2026-07-14):
// - ROLLOUT (NSG/NSA/NSGT): its own charge/launch/turn machine on
//   runtime-added phys.rollOut* (rollOutTurnTimer added THIS task — rule
//   16); NSG/NSA mains do NOT advance timer at the top (the advance sits
//   mid-body, charge-scaled: timer += 1 + 2*(charge/44)); NSG/NSA
//   interrupts ALWAYS return false (their WAIT/FALLSPECIAL arms fire the
//   init and still return false — verbatim); NSGT's two exit arms return
//   true.
// - SING (UPSPECIAL): id[0].size writes at t 28/36/69/77/113 (the chd
//   plane); land is an EMPTY function (present).
// - REST (DOWNSPECIAL{GROUND,AIR}): byte-identical twins; land is a
//   comment-only body.
// - POUND (SIDESPECIAL{GROUND,AIR}): twins; the air arc rotates
//   airVelocities by phys.upbAngleMultiplier = lsY*PI*(20/180) via
//   fdlibm sin/cos; AIR adds a bare-actionState-write land.
// - Multijump ladder (AERIALTURN1-5 / JUMPAERIAL1-5): cVel.y rungs
//   1.65/1.59/1.47/1.36/1.25, rungs 2-5 set doubleJumped; AERIALTURN
//   flips face at t===6 and hands off to its JUMPAERIAL at t===13;
//   JUMPAERIAL1-4 carry the t>28 checkForMultiJump -> puffNextJump arm
//   (5 does not). ATTACKAIRN's t===7 arm increments hitboxes.FRAMES and
//   ATTACKAIRB's t===8 writes phys.AUTOCANCEL (lowercase) — upstream
//   typos on the runtime-added fields, carried verbatim.
// - THROW*: 2-arg TABLE victim dispatch; fractional timers
//   (timer += K/releaseFrame) with floor(+0.01) crossings; THROWBACK's
//   window carries the floor-over-comparison typo; THROWDOWN's crossing
//   has NO grabbing===-1 guard (FORWARD/BACK do); interrupts' -1 arms
//   bare-return undefined (rule 13).
// - THROWN{PUFF,MARTH,FOX}* are GUARDED (init -1 guard + main -1 guard +
//   len clamp; THROWNPUFFUP nests its guard under a vacuous
//   `if(player[p].phys)`; THROWNMARTHFORWARD clamps BEFORE the guard;
//   THROWNMARTH* snap pos to the grabber in init);
//   THROWN{FALCO,FALCON}* are the old-style UNGUARDED family (init pos
//   snap; player[-1]/overruns throw upstream — traps); back/down x
//   formulas vary per file (THROWNMARTHBACK/THROWNFALCOBACK flip face
//   but keep PLAIN-face x; THROWNFALCONDOWN has reverseModel but NO
//   flip) — per-file verbatim.
// - CLIFF*: all 8 keep the onLedge===-1 `this.canGrabLedge = false`
//   table-write arm (C traps; mvData finalCheck guards);
//   CLIFFGETUPQUICK sets ledgeRegrabCount = TRUE (others false).
#ifndef ML_PUFF_MOVES_H
#define ML_PUFF_MOVES_H

#include "../shared/moves.h"

// --- driver-provided puff data seams (mvData puff dump, rule 15) -------------
// this.<key>[idx] scalar read — undefined (out of range) -> NaN.
extern double mv_puff_arr(const char *state, const char *key, double idx);
// this.<key>[idx] pair read -> [x,y]; false = out of range (upstream
// `[0]` on undefined THROWS — the caller traps).
extern bool mv_puff_pair(const char *state, const char *key, double idx,
                         Vec2D *out);
// this.<key>.length — missing key traps.
extern double mv_puff_arr_len(const char *state, const char *key);

// --- the chd-fed charHitboxes assign (driver-provided) ------------------------
// player[p].hitboxes.id[dst] = charHitboxes[moveKey].id<src>: like
// mv_assign_hitbox_id (CTAB1 shapes/offsets — no write sites exist for
// those fields upstream, measured by grep) but dmg/size come from the
// record's "chd" pre-projection: the GLOBAL charHitboxes plane's dmg/size
// evolve at runtime (rollout dmg through stale ids, sing size).
extern void pf_assign_hitbox_id(MlSim *S, double p, const char *moveKey,
                                int srcIdx, int dstIdx);

// --- hitboxes.id[idx].{dmg,size} writes (rule-10 element-field mirror) --------
static inline void pf_hb_set_dmg(MlSim *S, double p, int idx, double v) {
  MlPlayer *pl = mv_player(S, p);
  pl->hitboxes.id[idx].dmg = v;
  if (S->aliasHbId[(int)p]) pl->phys.prevFrameHitboxes.id[idx].dmg = v;
}
static inline void pf_hb_set_size(MlSim *S, double p, int idx, double v) {
  MlPlayer *pl = mv_player(S, p);
  pl->hitboxes.id[idx].size = v;
  if (S->aliasHbId[(int)p]) pl->phys.prevFrameHitboxes.id[idx].size = v;
}

// --- rule-8 read/write helpers for the runtime-added rollOut plane ------------
// (undefined at rest until the first NSG/NSA init: reads produce NaN in
// arithmetic / false in boolean context, writes set the presence flag)
#define PF_ROLL_NUM(field, Field) \
  static inline double pf_##field(const MlPlayer *pl) { \
    return pl->phys.has##Field ? pl->phys.field : js_nan(); \
  } \
  static inline void pf_set_##field(MlPlayer *pl, double v) { \
    pl->phys.field = v; \
    pl->phys.has##Field = true; \
  }
#define PF_ROLL_BOOL(field, Field) \
  static inline bool pf_##field(const MlPlayer *pl) { \
    return pl->phys.has##Field ? pl->phys.field : false; \
  } \
  static inline void pf_set_##field(MlPlayer *pl, bool v) { \
    pl->phys.field = v; \
    pl->phys.has##Field = true; \
  }
PF_ROLL_NUM(rollOutCharge, RollOutCharge)
PF_ROLL_BOOL(rollOutChargeAttempt, RollOutChargeAttempt)
PF_ROLL_BOOL(rollOutCharging, RollOutCharging)
PF_ROLL_BOOL(rollOutPlayerHit, RollOutPlayerHit)
PF_ROLL_NUM(rollOutPlayerHitTimer, RollOutPlayerHitTimer)
PF_ROLL_NUM(rollOutTurnTimer, RollOutTurnTimer)
PF_ROLL_NUM(rollOutVel, RollOutVel)
PF_ROLL_BOOL(rollOutWallHit, RollOutWallHit)
#undef PF_ROLL_NUM
#undef PF_ROLL_BOOL

// --- the puff helper modules (characters/puff/*.js) ---------------------------
void puff_multi_jump_drift(MlSim *S, double p, const MlInputBuffer in[4]);
void puff_next_jump(MlSim *S, double p, const MlInputBuffer in[4]);

// --- the puff module index (characters/puff/moves/index.js) -------------------
const MlMoveDef *puff_move_def(const char *name);
// puff[<name>].init(p, input) — the checkFor*/puffNextJump payload
// dispatch sites.
void puff_moves_init(MlSim *S, const char *name, double p,
                     const MlInputBuffer in[4]);

// --- special phase surfaces (shared moves.h mv_register_special_phases) -------
AsTri puff_NEUTRALSPECIALGROUND_onPlayerHit(MlSim *S, double p,
                                            const MlInputBuffer in[4],
                                            const MvX *ex);
AsTri puff_NEUTRALSPECIALAIR_onPlayerHit(MlSim *S, double p,
                                         const MlInputBuffer in[4],
                                         const MvX *ex);
AsTri puff_NEUTRALSPECIALAIR_onWallCollide(MlSim *S, double p,
                                           const MlInputBuffer in[4],
                                           const MvX *ex);
AsTri puff_NEUTRALSPECIALGROUNDTURN_onPlayerHit(MlSim *S, double p,
                                                const MlInputBuffer in[4],
                                                const MvX *ex);
MvFn puff_special_phase(const char *state, const char *phase);

// --- the 71 puff move definitions (one per upstream file) ---------------------
extern const MlMoveDef puff_AERIALTURN1, puff_AERIALTURN2, puff_AERIALTURN3,
    puff_AERIALTURN4, puff_AERIALTURN5, puff_ATTACKAIRB, puff_ATTACKAIRD,
    puff_ATTACKAIRF, puff_ATTACKAIRN, puff_ATTACKAIRU, puff_ATTACKDASH,
    puff_CATCHATTACK, puff_CLIFFATTACKQUICK, puff_CLIFFATTACKSLOW,
    puff_CLIFFESCAPEQUICK, puff_CLIFFESCAPESLOW, puff_CLIFFGETUPQUICK,
    puff_CLIFFGETUPSLOW, puff_CLIFFJUMPQUICK, puff_CLIFFJUMPSLOW,
    puff_DOWNATTACK, puff_DOWNSMASH, puff_DOWNSPECIALAIR,
    puff_DOWNSPECIALGROUND, puff_DOWNTILT, puff_FORWARDSMASH,
    puff_FORWARDTILT, puff_FURAFURA, puff_GRAB, puff_JAB1, puff_JAB2,
    puff_JUMPAERIAL1, puff_JUMPAERIAL2, puff_JUMPAERIAL3, puff_JUMPAERIAL4,
    puff_JUMPAERIAL5, puff_JUMPAERIALB, puff_JUMPAERIALF,
    puff_NEUTRALSPECIALAIR, puff_NEUTRALSPECIALGROUND,
    puff_NEUTRALSPECIALGROUNDTURN, puff_SIDESPECIALAIR,
    puff_SIDESPECIALGROUND, puff_THROWBACK, puff_THROWDOWN,
    puff_THROWFORWARD, puff_THROWNFOXBACK, puff_THROWNFOXDOWN,
    puff_THROWNFOXFORWARD, puff_THROWNFOXUP, puff_THROWNMARTHBACK,
    puff_THROWNMARTHDOWN, puff_THROWNMARTHFORWARD, puff_THROWNMARTHUP,
    puff_THROWNPUFFBACK, puff_THROWNPUFFDOWN, puff_THROWNPUFFFORWARD,
    puff_THROWNPUFFUP, puff_THROWUP, puff_UPSMASH, puff_UPSPECIAL,
    puff_UPTILT, puff_THROWNFALCOUP, puff_THROWNFALCODOWN,
    puff_THROWNFALCOBACK, puff_THROWNFALCOFORWARD, puff_THROWNFALCONUP,
    puff_THROWNFALCONDOWN, puff_THROWNFALCONBACK, puff_THROWNFALCONFORWARD,
    puff_APPEAL;

#endif // ML_PUFF_MOVES_H
