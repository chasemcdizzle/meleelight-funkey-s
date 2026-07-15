// characters/fox/moves.h <- src/characters/fox/moves/ (M2 task 8).
// The fox per-char move set: 61 move objects, structure-parallel one .c
// per upstream file under characters/fox/moves/. First per-char cluster —
// the shared set (../shared/moves.h, task 7) is the nested C tree fox
// bodies call into.
//
// DISPATCH SHAPES (all verbatim to upstream):
// - `this.main/interrupt(p,input)` — module-internal: direct static call.
// - direct shared-module imports (WAIT.init, KNEEBEND.init, ...):
//   mv_<NAME>.init(...) — the shared module objects, NOT table dispatch.
// - direct fox-module imports (JAB2.init, SIDESPECIALAIR.main, ...):
//   fox_<NAME>.<phase>(...).
// - `MOVES[b[1]].init(p,input)` — the fox module index: fox_move_def().
// - `actionStates[characterSelections[grabbing]].THROWNFOX*.init(...)` —
//   TABLE dispatch on the victim's char: mv_dispatch (registered body or
//   the driver's mv_seam). THROWBACK/THROWDOWN dispatch 1-arg (no input) —
//   the bodies never read it.
// - checkForIASA(p,input,true) — mv_checkForIASA (shared moves.h): real
//   dispatch through the shared JUMPAERIALB/F modules + the registered
//   fox module index.
//
// DATA PLANES: charHitboxes assigns via mv_assign_hitbox_id (CTAB1);
// framesData via mv_frames; the move-object data arrays (ATTACKDASH/
// APPEAL/FIREFOXBOUNCE/THROWFORWARD setVelocities*, THROWN*.offset,
// CLIFF*.offset/setVelocities) come from the capture's frame-0 mvData fox
// dump (rule 15) through the mv_fox_* seams below — never retyped.
//
// ARTICLE SEAM (task-13 boundary): articles.{LASER,ILLUSION}.init(options)
// only READ player state and mutate the JS-side article queues (no RNG,
// no player writes) — the C body crosses mv_article_* and the replay
// verifies name+options bit-exactly in call order (FIFO).
//
// CLIFF* `this.canGrabLedge = false` (onLedge === -1 arm): a runtime WRITE
// into the move table's data plane — outside the sim value domain here
// (it would also drift the finalCheck-guarded mvData dump). The C bodies
// trap at that exact site (rule 13's lazy-trap discipline).
#ifndef ML_FOX_MOVES_H
#define ML_FOX_MOVES_H

#include "../shared/moves.h"

// --- driver-provided fox data seams (mvData fox dump, rule 15) --------------
// this.<key>[idx] scalar read — undefined (out of range) -> NaN (every
// consumer multiplies/assigns into arithmetic).
extern double mv_fox_arr(const char *state, const char *key, double idx);
// this.<key>[idx] pair read -> [x,y]; false = out of range (upstream
// `[0]` on undefined THROWS — the caller traps).
extern bool mv_fox_pair(const char *state, const char *key, double idx,
                        Vec2D *out);
// this.<key>.length (offset-length clamps) — missing key traps.
extern double mv_fox_arr_len(const char *state, const char *key);

// --- driver-provided article seam (task-13 boundary) -------------------------
extern void mv_article_laser(MlSim *S, double p, double x, double y,
                             double rotate);
extern void mv_article_illusion(MlSim *S, double p, double type);

// --- the fox module index (characters/fox/moves/index.js) -------------------
const MlMoveDef *fox_move_def(const char *name);
// MOVES[<name>].init(p, input) — the checkFor* payload dispatch sites.
void fox_moves_init(MlSim *S, const char *name, double p,
                    const MlInputBuffer in[4]);

// --- the 61 fox move definitions (one per upstream file) --------------------
extern const MlMoveDef fox_JAB1, fox_JAB2, fox_JAB3, fox_DOWNTILT,
    fox_UPTILT, fox_FORWARDTILT, fox_FORWARDSMASH, fox_UPSMASH,
    fox_DOWNSMASH, fox_ATTACKAIRF, fox_ATTACKAIRB, fox_ATTACKAIRU,
    fox_ATTACKAIRD, fox_ATTACKAIRN, fox_ATTACKDASH, fox_UPSPECIAL,
    fox_UPSPECIALCHARGE, fox_UPSPECIALLAUNCH, fox_FIREFOXBOUNCE,
    fox_NEUTRALSPECIALAIR, fox_NEUTRALSPECIALGROUND, fox_SIDESPECIALAIR,
    fox_SIDESPECIALGROUND, fox_DOWNSPECIALAIR, fox_DOWNSPECIALGROUND,
    fox_THROWBACK, fox_THROWDOWN, fox_THROWUP, fox_THROWFORWARD,
    fox_THROWNPUFFFORWARD, fox_THROWNPUFFDOWN, fox_THROWNPUFFBACK,
    fox_THROWNPUFFUP, fox_THROWNMARTHUP, fox_THROWNMARTHDOWN,
    fox_THROWNMARTHBACK, fox_THROWNMARTHFORWARD, fox_THROWNFOXUP,
    fox_THROWNFOXDOWN, fox_THROWNFOXBACK, fox_THROWNFOXFORWARD,
    fox_CLIFFGETUPQUICK, fox_CLIFFGETUPSLOW, fox_CLIFFESCAPEQUICK,
    fox_CLIFFESCAPESLOW, fox_CLIFFJUMPQUICK, fox_CLIFFJUMPSLOW,
    fox_CLIFFATTACKSLOW, fox_CLIFFATTACKQUICK, fox_DOWNATTACK, fox_GRAB,
    fox_CATCHATTACK, fox_THROWNFALCOUP, fox_THROWNFALCODOWN,
    fox_THROWNFALCOBACK, fox_THROWNFALCOFORWARD, fox_THROWNFALCONUP,
    fox_THROWNFALCONDOWN, fox_THROWNFALCONBACK, fox_THROWNFALCONFORWARD,
    fox_APPEAL;

#endif // ML_FOX_MOVES_H
