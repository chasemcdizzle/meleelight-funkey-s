// characters/shared/moves.h <- src/characters/shared/moves/ (M2 task 7).
// The shared move set: 79 move objects {name, init, main, interrupt(, land)}
// driven by player[p].timer (anatomy §2/§4), structure-parallel one .c per
// upstream file under characters/shared/moves/. This header completes the
// task-4 scaffolding's opaque MlMoveDef with the real phase-function table.
//
// DISPATCH (upstream `actionStates[characterSelections[x]][STATE].<phase>`):
// every dispatch in translated move code goes through mv_dispatch, which
// resolves the (charId, state) entry in the task-4 registry
// (as_action_states, populated by the driver from the capture's measured
// sharedOrigin map — puff's FURAFURA/JUMPAERIALB/JUMPAERIALF are per-char
// OVERRIDES, tasks 8-12) and either calls the registered shared C body or
// crosses the driver-provided mv_seam (oracle-fed seam: args verified in
// call order, post-state resynced — the physics/hitdet seam discipline).
//
// PHASE SIGNATURES: JS phases are (p, input) with per-move extra args
// (JUMPF/JUMPB/KNEEBEND init(p,type,input); WALK init(p,addInitV,input);
// FALL init/main(p,input,disableInputs=false); STOPCEIL init(p,input,
// normal=null); WALLDAMAGE init(p,input,normal); SHIELDBREAKDOWNBOUND
// init(p,normal,input); SHIELDBREAKFALL land(p,normal,input); DAMAGEFLYN
// init(p,input,drawStuff)). C uniformly: (S, p, in, ex) where `in` is the
// god input array (4 slots x 8-deep buffers — moves read input[p][0..6]
// and input[grabbedBy]) and `ex` carries the extra args as typed
// MlDispExtra values (NULL = none). All phases return AsTri: JS interrupts
// return true/false/UNDEFINED (several arms fall through without a return
// — SMASHTURN/TILTTURN's tilt arms, OTTOTTO/OTTOTTOWAIT's second GUARDON
// arm, RUN's chain end, SQUATWAIT's restart arm, ENTRANCE always,
// CAPTUREWAIT's grabbedBy===-1 guard — all carried verbatim); init/main/
// land return AS_UNDEF.
//
// SIDE-EFFECT SEAMS: sounds via ml_sound_play/_stop; seeded draws via
// ml_random (CAPTUREWAIT's mash wiggle, FURAFURA's vfx jitter,
// screenShake's 4 draws per call, drawVfx("circleDust")'s 4 draws —
// main/vfx/drawVfx.js:15-18); vfx spawns via ml_vfx (render-only except
// the circleDust draws; the capture compares the name queue).
// percentShake is the CHECKSUM.md §7 native-RNG exclusion (no-op);
// finishGame is match-end (isFinalDeath true — zero-live, task 17's
// lifecycle surface: mv_out_of_domain).
//
// DATA PLANES: framesData + charAttributes + charHitboxes ("thrown") come
// from the M1 CTAB1 generated tables (ml_tables.h) — never snapshots. The
// per-char post-setup data patches (index.js: ESCAPEB/ESCAPEF/DOWNSTANDB/
// DOWNSTANDF/TECHB/TECHF setVelocities, CLIFFCATCH posOffset) plus
// CAPTUREDAMAGE.setPositions, actionSounds schedules and the palettes/pPal
// colour strings are EXECUTED data dumped by the capture's frame-0 mvData
// record (asFlags pattern, finalCheck drift-guarded) and served by the
// driver through the mv_* data seams below.
#ifndef ML_SHARED_MOVES_H
#define ML_SHARED_MOVES_H

#include <string.h>

#include "../../../fdlibm/fdlibm.h"
#include "../../action_state_shortcuts.h"
#include "../../ml_events.h" // ml_sound_play/_stop, ml_vfx, ml_random
#include "../../ml_js.h"
#include "../../physics.h" // MlSim, MlDispExtra
#include "../../util/lin_alg.h" // reflect, dotProd (STOPCEIL/WALLDAMAGE)
#include "ml_tables.h" // CTAB1: attributes/framesData/charHitboxes (M1)

// --- extra-argument pack (the JS args after/around (p, input)) --------------
typedef struct {
  int count;
  MlDispExtra x[2];
} MvX;

// upstream `input[p]` / `characterSelections[p]`:
#define MV_IN(in, p) ((in)[(int)(p)].slot)
#define MV_CS(S, p) ((S)->characterSelections[(int)(p)])

static inline MvX mvx_bool(bool b) {
  MvX x; x.count = 1; x.x[0].kind = DX_BOOL; x.x[0].b = b;
  x.x[0].num = 0; x.x[0].str = 0; x.x[0].vec.x = 0; x.x[0].vec.y = 0;
  return x;
}
static inline MvX mvx_num(double n) {
  MvX x; x.count = 1; x.x[0].kind = DX_NUM; x.x[0].num = n;
  x.x[0].b = false; x.x[0].str = 0; x.x[0].vec.x = 0; x.x[0].vec.y = 0;
  return x;
}
static inline MvX mvx_vec(Vec2D v) {
  MvX x; x.count = 1; x.x[0].kind = DX_VEC; x.x[0].vec = v;
  x.x[0].num = 0; x.x[0].b = false; x.x[0].str = 0;
  return x;
}
// checkFor* pair payloads: [true,"NAME"] | [true, 0|1] | [false,false] —
// KNEEBEND.init(p, j[1], input) forwards the payload VALUE verbatim.
extern void mv_out_of_domain(const char *what);
static inline MvX mvx_pair_payload(const AsPair *pr) {
  if (pr->kind == AS_P_NUM) return mvx_num(pr->num);
  if (pr->kind == AS_P_FALSE) return mvx_bool(false);
  mv_out_of_domain("pair payload kind"); // STR payloads never reach here
  return mvx_bool(false);
}
static inline const char *mv_pair_str(const AsPair *pr) {
  if (pr->kind != AS_P_STR || pr->str == 0) {
    mv_out_of_domain("pair payload: expected move-name string");
  }
  return pr->str;
}
static inline AsPair mv_pair_false(void) {
  AsPair pr; pr.flag = false; pr.kind = AS_P_FALSE; pr.str = 0; pr.num = 0;
  return pr;
}
// ES6 default-param / truthiness reads of an extra arg (absent =
// undefined = falsy):
static inline bool mvx_truthy(const MvX *ex, int i) {
  if (ex == 0 || i >= ex->count) return false;
  const MlDispExtra *e = &ex->x[i];
  switch (e->kind) {
    case DX_NUM: return e->num == e->num && e->num != 0;
    case DX_BOOL: return e->b;
    case DX_STR: return e->str != 0 && e->str[0] != 0;
    case DX_VEC: return true;
  }
  return false;
}

// --- the move definition (completes task 4's opaque MlMoveDef) --------------
typedef AsTri (*MvFn)(MlSim *S, double p, const MlInputBuffer in[4],
                      const MvX *ex);
struct MlMoveDef {
  const char *name;
  MvFn init;
  MvFn main_; // `main` shadows C's main
  MvFn interrupt;
  MvFn land; // NULL where the JS object has no land property
};

// --- special phase surfaces (M2 task 10) -------------------------------------
// A few per-char move objects carry NON-phase dispatch functions:
// onPlayerHit(p) — hitDetection.js:493's specialOnHit arm (falcon
// SIDESPECIAL{GROUND,AIR}, puff NEUTRALSPECIAL*); onWallCollide(p,input,
// wallFace,wallNum) — physics.js:122's specialWallCollide arm (falcon
// DOWNSPECIALGROUND, puff NEUTRALSPECIALAIR; extras carry
// [wallFace(DX_STR), wallNum(DX_NUM)]). Rather than widening every
// MlMoveDef initializer, the owning cluster's driver registers a lookup;
// mv_dispatch routes the two phase names through it (unregistered ->
// mv_out_of_domain, the upstream missing-property TypeError).
typedef MvFn (*MvSpecialPhaseLookup)(const char *state, const char *phase);
void mv_register_special_phases(MvSpecialPhaseLookup lookup);

// --- dispatch ----------------------------------------------------------------
// actionStates[charId][state].<phase>(slot, input, ...extras): registered
// shared body, or mv_seam. Calling a phase the entry lacks mirrors the
// upstream TypeError -> mv_out_of_domain.
AsTri mv_dispatch(MlSim *S, double charId, const char *state,
                  const char *phase, double slot, const MlInputBuffer in[4],
                  const MvX *ex);

// name -> shared def (NULL if not a shared move); the driver uses this to
// build the per-char registry from the capture's sharedOrigin map.
const MlMoveDef *mv_shared_def(const char *name);

// --- driver-provided seams ----------------------------------------------------
extern AsTri mv_seam(MlSim *S, double charId, const char *state,
                     const char *phase, double slot, const MvX *ex);
extern void mv_out_of_domain(const char *what);

// mvData data seams (frame-0 capture dump; executed upstream data):
// setVelocities[idx] — undefined out of range: NaN (consumers multiply).
extern double mv_setVelocity(double charId, const char *moveKey, double idx);
// CAPTUREDAMAGE.setPositions[idx] — same undefined->NaN convention.
extern double mv_setPosition_capturedamage(double charId, double idx);
// CLIFFCATCH.posOffset[idx] -> [x,y]; false = out of range (upstream
// `posOffset[t][0]` on undefined THROWS — the caller traps).
extern bool mv_posOffsetCliffCatch(double charId, double idx, Vec2D *out);
// actionSounds[charId][state] rows; -1 = key absent (upstream `.length`
// on undefined THROWS — the caller traps).
extern int mv_actionSounds(double charId, const char *state,
                           const AsSoundRow **rows);
// palettes[pPal[slot]][0] — the FURASLEEP colour-blend base string.
extern const char *mv_palette0(double slot);

// --- shared helpers (moves_index.c) -------------------------------------------
// framesData[charId][state] from CTAB1 (M1 data path); NaN if absent
// (upstream `timer > undefined` is false — NaN compares false in C too).
double mv_frames(double charId, const char *state);
const ml_attributes_t *mv_attr(double charId);

// drawVfx(name): ml_vfx note; "circleDust" additionally consumes 4 seeded
// draws (drawVfx.js:15-18 — the values are render-only, the DRAWS are
// chain state). screenShake: 4 seeded draws (main.js:352-358).
void mv_drawVfx(const char *name);
void mv_screenShake(void);
// playSounds(state, p) — actionStateShortcuts.js:144 via the mvData rows.
void mv_playSounds(MlSim *S, const char *state, double p);
// isFinalDeath() — builds the projected globals slice from MlSim.
bool mv_isFinalDeath(MlSim *S);

// rule-10 alias helpers (the physics.c pos_set_* discipline):
void mv_pos_set_x(MlSim *S, double i, double v);
void mv_pos_set_y(MlSim *S, double i, double v);
void mv_pos_reassign(MlSim *S, double i, Vec2D v);
// turnOffHitboxes(p): fresh active/hitList arrays break both aliases.
void mv_turnOffHitboxes(MlSim *S, double i);
// player deref (absent slot = domain violation):
MlPlayer *mv_player(MlSim *S, double i);

// player[p].hitboxes.id[dst] = charHitboxes[moveKey].id<src> (CTAB1
// ml_hitbox_moves; element write mirrors through the id alias when live).
// Generalized for the per-char clusters (M2 task 8); DAMAGEFLYN's
// mv_assign_thrown_id0 is the thrown/id0 special case.
void mv_assign_hitbox_id(MlSim *S, double p, const char *moveKey, int srcIdx,
                         int dstIdx);
void mv_assign_thrown_id0(MlSim *S, double p);

// activeStage[l[0]][l[1]][l[2]] for l = activeStage.ledge[onLedge] — the
// CLIFF* coordinate read (M2 task 8; CLIFFCATCH.c carries the original
// static copy). Out-of-range mirrors upstream's throw via mv_out_of_domain.
Vec2D mv_ledge_point(MlSim *S, double onLedge, const char *what);

// hitQueue.push([a, b, c, d, e, f]) from move code (fox THROW*'s
// [grabbing, p, 0, false, true, isThrowDown] rows) — driver-provided:
// the replay appends the row's canon to its opaque hq carrier.
extern void mv_hq_push6(MlSim *S, double a, double b, double c, bool d,
                        bool e, bool f);

// checkForIASA with REAL dispatch (actionStateShortcuts.js:388-416; the
// note-based as_checkForIASA stays the task-4 asshort boundary): the
// JUMPAERIALB/F arm dispatches the shared MODULE objects (checkForIASA's
// import path — puff's table overrides do NOT apply here), the aerial-name
// arm dispatches the per-char module index registered below. Called by the
// per-char aerials' interrupts (tasks 8-12).
typedef const MlMoveDef *(*MvCharModuleLookup)(const char *name);
void mv_register_char_module(int charId, MvCharModuleLookup lookup);
AsTri mv_checkForIASA(MlSim *S, double p, const MlInputBuffer in[4],
                      bool isAerial);

// --- the 79 shared move definitions (one per upstream file) -------------------
extern const MlMoveDef mv_WAIT, mv_DASH, mv_RUN, mv_SMASHTURN, mv_TILTTURN,
    mv_RUNBRAKE, mv_RUNTURN, mv_WALK, mv_KNEEBEND, mv_JUMPF, mv_JUMPB,
    mv_LANDING, mv_ESCAPEAIR, mv_LANDINGFALLSPECIAL, mv_FALL, mv_FALLAERIAL,
    mv_FALLSPECIAL, mv_SQUAT, mv_SQUATWAIT, mv_SQUATRV, mv_JUMPAERIALF,
    mv_JUMPAERIALB, mv_PASS, mv_GUARDON, mv_GUARD, mv_GUARDOFF,
    mv_CLIFFCATCH, mv_CLIFFWAIT, mv_DEADLEFT, mv_DEADRIGHT, mv_DEADUP,
    mv_DEADDOWN, mv_REBIRTH, mv_REBIRTHWAIT, mv_DAMAGEFLYN, mv_DAMAGEFALL,
    mv_DAMAGEN2, mv_LANDINGATTACKAIRN, mv_LANDINGATTACKAIRF,
    mv_LANDINGATTACKAIRB, mv_LANDINGATTACKAIRD, mv_LANDINGATTACKAIRU,
    mv_ESCAPEB, mv_ESCAPEF, mv_ESCAPEN, mv_DOWNBOUND, mv_DOWNWAIT,
    mv_DOWNDAMAGE, mv_DOWNSTANDN, mv_DOWNSTANDB, mv_DOWNSTANDF, mv_TECHN,
    mv_TECHB, mv_TECHF, mv_SHIELDBREAKFALL, mv_SHIELDBREAKDOWNBOUND,
    mv_SHIELDBREAKSTAND, mv_FURAFURA, mv_CAPTUREPULLED, mv_CAPTUREWAIT,
    mv_CATCHWAIT, mv_CAPTURECUT, mv_CATCHCUT, mv_CAPTUREDAMAGE,
    mv_WALLDAMAGE, mv_WALLTECH, mv_WALLJUMP, mv_WALLTECHJUMP, mv_OTTOTTO,
    mv_OTTOTTOWAIT, mv_MISSFOOT, mv_FURASLEEPSTART, mv_FURASLEEPLOOP,
    mv_FURASLEEPEND, mv_STOPCEIL, mv_TECHU, mv_SLEEP, mv_ENTRANCE,
    mv_THROWNFALCONDIVE;

#endif // ML_SHARED_MOVES_H
