// characters/falcon/moves.h <- src/characters/falcon/moves/ (M2 task 10).
// The falcon per-char move set: 67 move objects, structure-parallel one .c
// per upstream file under characters/falcon/moves/. Third per-char cluster
// — task 8's fox recipe followed; the shared set (../shared/moves.h,
// task 7) is the nested C tree falcon bodies call into.
//
// DISPATCH SHAPES (all verbatim to upstream; the fox moves.h notes apply):
// - direct falcon-module imports (JAB2.init, UPSPECIALCATCH.init, ...):
//   falcon_<NAME>.<phase>(...).
// - `MOVES[b[1]].init(p,input)` — the falcon module index:
//   falcon_move_def() via falcon_moves_init.
// - `actionStates[characterSelections[grabbing]].THROWNFALCON*.init(...)` —
//   TABLE dispatch on the victim's char: mv_dispatch (registered body or
//   the driver's mv_seam). THROWBACK/THROWDOWN dispatch 1-arg (no input).
// - checkForIASA(p,input,true) — mv_checkForIASA (shared moves.h). NOTE
//   upstream's aerial-payload arm has NO characterSelections==4 branch:
//   a falcon IASA aerial payload dispatches NOTHING (returns true with no
//   state change) — no falcon module registration, verbatim (falco
//   precedent).
// - onPlayerHit(p) / onWallCollide(p,input,wallFace,wallNum) — falcon's
//   NON-phase move fns (MlMoveDef's task-10 fields): hitDetection.js:493's
//   specialOnHit arm (SIDESPECIAL{GROUND,AIR}) and physics.js:122's
//   specialWallCollide arm (DOWNSPECIALGROUND; extras = [wallFace(DX_STR),
//   wallNum(DX_NUM)] — wallNum is unread upstream, carried for shape).
//
// DATA PLANES: charHitboxes assigns via mv_assign_hitbox_id (CTAB1);
// framesData via mv_frames; the move-object data arrays (JAB3/FORWARDSMASH/
// NEUTRALSPECIAL*/SIDESPECIAL*/DOWNSPECIALGROUNDENDAIR setVelocities,
// the PAIR-array setVelocities of UPSPECIAL/UPSPECIALTHROW/DOWNSPECIALAIR,
// THROWN*.offset, CLIFF* offset/setVelocities) come from the capture's
// frame-0 mvData falcon dump (rule 15) through the mv_falcon_* seams below
// — never retyped.
//
// ARTICLES: falcon has NONE — it imports `articles` in 6 files but never
// dereferences it (dead imports, measured; the capture pins the article
// seam count at ZERO). No article seam functions exist for falcon.
//
// STRUCTURE NOTES vs fox/falco (measured per-file diffs, 2026-07-14):
// falcon's THROWN* family is byte-identical to FOX's shapes (guarded
// THROWN{PUFF,MARTH,FOX}* with -1 guards + offset clamps; unguarded
// THROWN{FALCO,FALCON}* where player[-1]/overrun throws upstream — traps);
// falcon's THROW* keep fox's grabbing===-1 init guard but fire NO lasers;
// falcon's CLIFF* keep fox's onLedge===-1 canGrabLedge table-write arm
// (C traps at the site; the falcon mvData dump is drift-guarded);
// SIDESPECIALGROUND writes `this.canEdgeCancel` at runtime — a SCALAR
// move-table write outside the array-only mvData dump, modeled as module
// state (mv_falcon_ssg_set_canEdgeCancel below; its only sim READER is
// physics' per-state flag lookup — task 17's integration wires it);
// UPSPECIALCATCH/UPSPECIALTHROW draw the seeded stream INLINE (2 draws per
// firefoxtail spawn x3) and UPSPECIALCATCH's interrupt pushes a hitQueue
// row; SIDESPECIALGROUNDHIT.main reads player[p].phys.timer (undefined
// upstream — physicsObject has no timer: the `< 18` arm is dead) and
// DOWNSPECIALGROUNDENDAIR.main reads player.timer (the ARRAY — both its
// arms dead); both carried verbatim as dead arms with comments.
#ifndef ML_FALCON_MOVES_H
#define ML_FALCON_MOVES_H

#include "../shared/moves.h"

// --- driver-provided falcon data seams (mvData falcon dump, rule 15) --------
// this.<key>[idx] scalar read — undefined (out of range) -> NaN (every
// consumer multiplies/assigns into arithmetic).
extern double mv_falcon_arr(const char *state, const char *key, double idx);
// this.<key>[idx] pair read -> [x,y]; false = out of range (upstream
// `[0]` on undefined THROWS — the caller traps).
extern bool mv_falcon_pair(const char *state, const char *key, double idx,
                           Vec2D *out);
// this.<key>.length — missing key traps.
extern double mv_falcon_arr_len(const char *state, const char *key);

// --- hitboxes.id[0].offset[hitboxes.frame] read ------------------------------
// Falcon's firefoxtail vfx windows compute their (render-only) position
// from the ACTIVE hitbox's per-frame offset array. The VALUE is discarded
// (the capture compares vfx NAMES only), but the READ is crash-fidelity:
// upstream throws when offset is not an array (constructor/offsetSingle
// hitbox) or the frame overruns it — the C traps at the same site.
static inline Vec2D mv_falcon_hb0_off(const MlPlayer *pl, const char *what) {
  const MlHitboxSpec *hb = &pl->hitboxes.id[0];
  const double idx = pl->hitboxes.frame;
  const int k = (int)idx;
  if (hb->shape != ML_HB_CHARDATA || hb->offsetSingle ||
      idx != (double)k || k < 0 || k >= hb->offsetLen) {
    mv_out_of_domain(what);
  }
  return hb->offsetArr[k];
}

// --- SIDESPECIALGROUND's runtime canEdgeCancel table write -------------------
// (scalar move-table state; write-only within this cluster — the reader is
// physics' flag table, task 17)
void mv_falcon_ssg_set_canEdgeCancel(bool v);

// --- the falcon module index (characters/falcon/moves/index.js) -------------
const MlMoveDef *falcon_move_def(const char *name);
// MOVES[<name>].init(p, input) — the checkFor* payload dispatch sites.
void falcon_moves_init(MlSim *S, const char *name, double p,
                       const MlInputBuffer in[4]);

// --- special phase surfaces (shared moves.h mv_register_special_phases) -----
// the three falcon non-phase move fns + the lookup the driver registers
AsTri falcon_SIDESPECIALGROUND_onPlayerHit(MlSim *S, double p,
                                           const MlInputBuffer in[4],
                                           const MvX *ex);
AsTri falcon_SIDESPECIALAIR_onPlayerHit(MlSim *S, double p,
                                        const MlInputBuffer in[4],
                                        const MvX *ex);
AsTri falcon_DOWNSPECIALGROUND_onWallCollide(MlSim *S, double p,
                                             const MlInputBuffer in[4],
                                             const MvX *ex);
MvFn falcon_special_phase(const char *state, const char *phase);

// --- the 67 falcon move definitions (one per upstream file) ------------------
extern const MlMoveDef falcon_JAB1, falcon_JAB2, falcon_JAB3, falcon_DOWNTILT,
    falcon_UPTILT, falcon_FORWARDTILT, falcon_FORWARDSMASH, falcon_UPSMASH,
    falcon_DOWNSMASH, falcon_ATTACKAIRF, falcon_ATTACKAIRB, falcon_ATTACKAIRU,
    falcon_ATTACKAIRD, falcon_ATTACKAIRN, falcon_ATTACKDASH, falcon_UPSPECIAL,
    falcon_UPSPECIALCATCH, falcon_UPSPECIALTHROW, falcon_NEUTRALSPECIALAIR,
    falcon_NEUTRALSPECIALGROUND, falcon_SIDESPECIALAIR,
    falcon_SIDESPECIALAIRHIT, falcon_SIDESPECIALGROUND,
    falcon_SIDESPECIALGROUNDTOAIR, falcon_SIDESPECIALGROUNDHIT,
    falcon_DOWNSPECIALAIR, falcon_DOWNSPECIALAIRENDAIR,
    falcon_DOWNSPECIALAIRENDGROUND, falcon_DOWNSPECIALGROUND,
    falcon_DOWNSPECIALGROUNDENDAIR, falcon_DOWNSPECIALGROUNDENDGROUND,
    falcon_THROWBACK, falcon_THROWDOWN, falcon_THROWUP, falcon_THROWFORWARD,
    falcon_THROWNPUFFFORWARD, falcon_THROWNPUFFDOWN, falcon_THROWNPUFFBACK,
    falcon_THROWNPUFFUP, falcon_THROWNMARTHUP, falcon_THROWNMARTHDOWN,
    falcon_THROWNMARTHBACK, falcon_THROWNMARTHFORWARD, falcon_THROWNFOXUP,
    falcon_THROWNFOXDOWN, falcon_THROWNFOXBACK, falcon_THROWNFOXFORWARD,
    falcon_CLIFFGETUPQUICK, falcon_CLIFFGETUPSLOW, falcon_CLIFFESCAPEQUICK,
    falcon_CLIFFESCAPESLOW, falcon_CLIFFJUMPQUICK, falcon_CLIFFJUMPSLOW,
    falcon_CLIFFATTACKSLOW, falcon_CLIFFATTACKQUICK, falcon_DOWNATTACK,
    falcon_GRAB, falcon_CATCHATTACK, falcon_THROWNFALCOUP,
    falcon_THROWNFALCODOWN, falcon_THROWNFALCOBACK, falcon_THROWNFALCOFORWARD,
    falcon_APPEAL, falcon_THROWNFALCONUP, falcon_THROWNFALCONDOWN,
    falcon_THROWNFALCONBACK, falcon_THROWNFALCONFORWARD;

#endif // ML_FALCON_MOVES_H
