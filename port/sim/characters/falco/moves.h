// characters/falco/moves.h <- src/characters/falco/moves/ (M2 task 9).
// The falco per-char move set: 69 move objects, structure-parallel one .c
// per upstream file under characters/falco/moves/. Second per-char cluster
// — task 8's fox recipe followed exactly; the shared set
// (../shared/moves.h, task 7) is the nested C tree falco bodies call into.
//
// DISPATCH SHAPES (all verbatim to upstream; the fox moves.h notes apply):
// - direct falco-module imports (JAB2.init, DOWNSPECIALAIRLOOP.init, ...):
//   falco_<NAME>.<phase>(...).
// - `MOVES[b[1]].init(p,input)` — the falco module index:
//   falco_move_def() via falco_moves_init.
// - `actionStates[characterSelections[grabbing]].THROWNFALCO*.init(...)` —
//   TABLE dispatch on the victim's char: mv_dispatch (registered body or
//   the driver's mv_seam). THROWBACK/THROWDOWN dispatch 1-arg (no input).
// - checkForIASA(p,input,true) — mv_checkForIASA (shared moves.h). NOTE
//   upstream's aerial-payload arm has NO characterSelections==3 branch:
//   a falco IASA aerial payload dispatches NOTHING (returns true with no
//   state change) — no falco module registration, verbatim.
//
// DATA PLANES: charHitboxes assigns via mv_assign_hitbox_id (CTAB1);
// framesData via mv_frames; the move-object data arrays (THROWFORWARD/
// ATTACKDASH/FIREFOXBOUNCE setVelocities, THROWN*.offset, CLIFF*.offset/
// setVelocities) come from the capture's frame-0 mvData falco dump
// (rule 15) through the mv_falco_* seams below — never retyped.
//
// ARTICLE SEAM (task-13 boundary): falco's articles.{LASER,ILLUSION}.init
// options carry `isFox: false` (LASER: NEUTRALSPECIAL*/THROW* sites, the
// THROWDOWN sites additionally `partOfThrow: true`; ILLUSION: always
// type 0) — the driver serializes/verifies them bit-exactly in call order.
//
// STRUCTURE NOTES vs fox (measured per-file diffs, 2026-07-14): falco's
// CLIFF* have NO onLedge===-1 canGrabLedge table-write arm (the raw
// ledge[-1] deref throws upstream — mv_ledge_point traps); falco's
// THROWN* have NO grabbedBy===-1 guards and NO offset-length clamps
// (player[-1]/offset overrun throw upstream — mv_player/mv_falco_pair
// trap); falco's shine is a 4-sub-state machine per environment
// (DOWNSPECIAL{AIR,GROUND}{START,LOOP,END,TURN}; the DOWNSPECIALAIR/
// GROUND entries are init-only delegates).
#ifndef ML_FALCO_MOVES_H
#define ML_FALCO_MOVES_H

#include "../shared/moves.h"

// --- driver-provided falco data seams (mvData falco dump, rule 15) ----------
// this.<key>[idx] scalar read — undefined (out of range) -> NaN (every
// consumer multiplies/assigns into arithmetic).
extern double mv_falco_arr(const char *state, const char *key, double idx);
// this.<key>[idx] pair read -> [x,y]; false = out of range (upstream
// `[0]` on undefined THROWS — the caller traps).
extern bool mv_falco_pair(const char *state, const char *key, double idx,
                          Vec2D *out);
// this.<key>.length — missing key traps.
extern double mv_falco_arr_len(const char *state, const char *key);

// --- driver-provided article seams (task-13 boundary; isFox:false) ----------
extern void mv_article_laser_falco(MlSim *S, double p, double x, double y,
                                   double rotate, bool partOfThrow);
extern void mv_article_illusion_falco(MlSim *S, double p, double type);

// --- the falco module index (characters/falco/moves/index.js) ---------------
const MlMoveDef *falco_move_def(const char *name);
// MOVES[<name>].init(p, input) — the checkFor* payload dispatch sites.
void falco_moves_init(MlSim *S, const char *name, double p,
                      const MlInputBuffer in[4]);

// --- the 69 falco move definitions (one per upstream file) ------------------
extern const MlMoveDef falco_JAB1, falco_JAB2, falco_JAB3, falco_DOWNTILT,
    falco_UPTILT, falco_FORWARDTILT, falco_FORWARDSMASH, falco_UPSMASH,
    falco_DOWNSMASH, falco_ATTACKAIRF, falco_ATTACKAIRB, falco_ATTACKAIRU,
    falco_ATTACKAIRD, falco_ATTACKAIRN, falco_ATTACKDASH, falco_UPSPECIAL,
    falco_UPSPECIALCHARGE, falco_UPSPECIALLAUNCH, falco_FIREFOXBOUNCE,
    falco_NEUTRALSPECIALAIR, falco_NEUTRALSPECIALGROUND,
    falco_SIDESPECIALAIR, falco_SIDESPECIALGROUND, falco_DOWNSPECIALAIR,
    falco_DOWNSPECIALAIRSTART, falco_DOWNSPECIALAIRLOOP,
    falco_DOWNSPECIALAIREND, falco_DOWNSPECIALAIRTURN,
    falco_DOWNSPECIALGROUND, falco_DOWNSPECIALGROUNDSTART,
    falco_DOWNSPECIALGROUNDLOOP, falco_DOWNSPECIALGROUNDEND,
    falco_DOWNSPECIALGROUNDTURN, falco_THROWBACK, falco_THROWDOWN,
    falco_THROWUP, falco_THROWFORWARD, falco_THROWNPUFFFORWARD,
    falco_THROWNPUFFDOWN, falco_THROWNPUFFBACK, falco_THROWNPUFFUP,
    falco_THROWNMARTHUP, falco_THROWNMARTHDOWN, falco_THROWNMARTHBACK,
    falco_THROWNMARTHFORWARD, falco_THROWNFOXUP, falco_THROWNFOXDOWN,
    falco_THROWNFOXBACK, falco_THROWNFOXFORWARD, falco_THROWNFALCOUP,
    falco_THROWNFALCODOWN, falco_THROWNFALCOBACK, falco_THROWNFALCOFORWARD,
    falco_THROWNFALCONUP, falco_THROWNFALCONDOWN, falco_THROWNFALCONBACK,
    falco_THROWNFALCONFORWARD, falco_CLIFFGETUPQUICK, falco_CLIFFGETUPSLOW,
    falco_CLIFFESCAPEQUICK, falco_CLIFFESCAPESLOW, falco_CLIFFJUMPQUICK,
    falco_CLIFFJUMPSLOW, falco_CLIFFATTACKSLOW, falco_CLIFFATTACKQUICK,
    falco_DOWNATTACK, falco_GRAB, falco_CATCHATTACK, falco_APPEAL;

#endif // ML_FALCO_MOVES_H
