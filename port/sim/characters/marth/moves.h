// characters/marth/moves.h <- src/characters/marth/moves/ (M2 task 11).
// The marth per-char move set: 75 move objects, structure-parallel one .c
// per upstream file under characters/marth/moves/ plus the two helper
// modules at characters/marth/ level (dancingBladeCombo.js /
// dancingBladeAirMobility.js -> dancing_blade_combo.c /
// dancing_blade_air_mobility.c). Fourth per-char cluster — task 8's fox
// recipe; marth has no fox/falco sibling for most files (17 dancing-blade
// states + the DOWNSPECIAL*2 counter pair are FRESH shapes).
//
// DISPATCH SHAPES (all verbatim to upstream):
// - direct marth-module imports (marth.JAB2.init, ...): marth_<NAME> defs.
// - `marth[b[1]].init(p,input)` — the marth module index (imported by
//   nearly EVERY marth move as `import marth from "./index"`):
//   marth_move_def() via marth_moves_init.
// - `actionStates[characterSelections[grabbing]].THROWNMARTH*.init(
//   grabbing, input)` — TABLE dispatch on the victim's char (2-arg,
//   unlike fox's 1-arg THROWBACK/THROWDOWN sites): mv_dispatch.
// - checkForIASA's characterSelections==0 arm (actionStateShortcuts.js:
//   400-401 `MARTHMOVES[a[1]].init(p,input)`) — the marth module IS
//   registered via mv_register_char_module(0, marth_move_def). NOTE the
//   arm is dead-by-construction upstream (only fox/falco/falcon aerials
//   call checkForIASA, and those run only when cs[p] is that char);
//   marth's own ATTACKAIRF/B interrupts INLINE the same logic
//   (checkForDoubleJump -> shared JUMPAERIALB/F modules; checkForAerials
//   payload -> marth[a[1]].init) — translated verbatim in their files.
// - onClank(p, input) — marth's NON-phase move fn on
//   DOWNSPECIAL{GROUND,AIR} (hitDetection.js:71-72's specialClank arm),
//   routed through mv_register_special_phases (the task-10 hook; marth
//   has NO onPlayerHit/onWallCollide surfaces).
//
// DATA PLANES: charHitboxes assigns via mv_assign_hitbox_id (CTAB1;
// SIDESPECIAL{GROUND,AIR}4DOWN compute the key "db{ground,air}4down" +
// floor((timer-7)/6) at runtime); framesData via mv_frames; move-object
// data arrays (ATTACKDASH/SSG3*/SSG4*/CLIFF* setVelocities, UPSPECIAL's
// PAIR setVelocities, CLIFF*/THROWN*.offset) come from the capture's
// frame-0 mvData marth dump (rule 15) through mv_marth_* — never retyped.
//
// ARTICLES: marth has NONE — zero `articles` imports anywhere under
// characters/marth/ (stronger than falcon's dead imports; measured).
// The capture still wraps LASER/ILLUSION as the zero-pin tripwire.
//
// STRUCTURE NOTES (measured per-file diffs vs fox/falco/falcon,
// 2026-07-14):
// - NEUTRALSPECIAL{GROUND,AIR} (shield breaker): phys.shieldBreaker*
//   trio + player.shieldBreakerID (Howl play id) modeled since task 9;
//   `player[p].shieldBreakerID = sounds.shieldbreakercharge.play()`
//   CONSUMES the Howl id — an ORACLE-FED SEAM in the replay
//   (ml_howl_play_id / ml_howl_id_oracle, ml_events.h — M4 task 6;
//   the id is a global howler counter, unrecoverable chain state in
//   the capture domain — the task-5 launch-getter discipline);
//   `sounds.shieldbreakercharge.stop(id)` is a stop token
//   ("shieldbreakercharge.stop"). The timer-46 arm writes
//   hitboxes.id[i].dmg — upstream this MUTATES the GLOBAL charHitboxes
//   objects (player.js:132 aliases, no copy): a runtime write to the
//   M1-owned char-data plane (falcon canEdgeCancel's class). newDmg ==
//   the authored 7 whenever shieldBreakerCharge < 30 (measured live
//   domain {0,1}); a live CHARGED punch followed by another nsg/nsa
//   hitbox assign would surface as an init-record divergence (loud).
//   The sweep exercises the >=120 arm and RESTORES the 8 dmg fields
//   (rule 12). Charge tint: blendColours over palettes[pPal[p]][0]
//   toward [117,50,227] (marth_blend_overlay below).
// - SIDESPECIAL* (dancing blade, 18 states + 2 helpers):
//   phys.dancingBlade / phys.dancingBladeDisable are runtime-added
//   (presence-modeled, ml_player.h task 11 — rule 16); chains dispatch
//   through the marth module index on lsY thresholds (+-0.56).
// - DOWNSPECIAL{GROUND,AIR} (counter) carry specialClank + onClank ->
//   DOWNSPECIAL{GROUND,AIR}2; colourOverlay cycles literal strings.
// - UPSPECIAL (dolphin slash): PAIR setVelocities rotated by
//   phys.upbAngleMultiplier (fdlibm sin/cos); land = falcon's 3-disjunct
//   guard shape.
// - THROW*: all four dispatch the victim 2-arg; THROWUP's interrupt
//   grabbing===-1 arm returns FALSE where the other three fall through
//   (AsTri undef) — per-file verbatim.
// - THROWN*: THROWN{PUFF,MARTH,FOX}* are guarded (grabbedBy===-1 init
//   guard — EXCEPT THROWNMARTHBACK, which has none — plus a -1 main
//   guard and a `timer > len -> len-1` clamp whose ORDER vs the guard
//   varies per file; THROWNPUFFUP wraps its body in a vacuous
//   `if(player[p].phys)`); THROWN{FALCO,FALCON}* are fox's unguarded
//   family (renamed fox translations, offsets from the marth dump).
// - CLIFF*: all 8 keep fox's onLedge===-1 `this.canGrabLedge = false`
//   table-write arm (C traps; mvData finalCheck guards);
//   CLIFFGETUPQUICK sets ledgeRegrabCount = TRUE (others false) —
//   authored quirk carried verbatim.
#ifndef ML_MARTH_MOVES_H
#define ML_MARTH_MOVES_H

#include "../shared/moves.h"

// --- driver-provided marth data seams (mvData marth dump, rule 15) ----------
// this.<key>[idx] scalar read — undefined (out of range) -> NaN.
extern double mv_marth_arr(const char *state, const char *key, double idx);
// this.<key>[idx] pair read -> [x,y]; false = out of range (upstream
// `[0]` on undefined THROWS — the caller traps).
extern bool mv_marth_pair(const char *state, const char *key, double idx,
                          Vec2D *out);
// this.<key>.length — missing key traps.
extern double mv_marth_arr_len(const char *state, const char *key);

// --- Howl play-id seam (M4 task 6 form) --------------------------------------
// `player[p].shieldBreakerID = sounds.shieldbreakercharge.play()` — the
// returned id is howler's GLOBAL play counter (advanced by every sound in
// the match, most outside this cluster's records). The call sites now use
// ml_howl_play_id (ml_events.h): in the integrated sim the id derives
// from the sim's own play-event count (howler-parallel, off the checksum
// surface); the moves-marth replay driver still injects the RECORDED
// browser ids (the capture's post "sbid" list) via the ml_howl_id_oracle
// hook — the oracle-fed seam semantics are unchanged, only the seam's
// plumbing moved to ml_events.c (task-6 class fix: one id plane, not one
// counter per binary).

// --- hitboxes.id[idx].dmg write (rule-10 element-field mirror) ---------------
static inline void mv_hb_set_dmg(MlSim *S, double p, int idx, double v) {
  MlPlayer *pl = mv_player(S, p);
  pl->hitboxes.id[idx].dmg = v;
  // field write through the SAME object: visible via the id-array alias
  if (S->aliasHbId[(int)p]) pl->phys.prevFrameHitboxes.id[idx].dmg = v;
  // rule-17: upstream this write mutates the GLOBAL charHitboxes object
  // (weak no-op outside the task-17 sim host — shared moves.h):
  mv_chd_write_dmg(S, p, idx, v);
}

// --- the shield-breaker charge tint ------------------------------------------
// palettes[pPal[p]][0] "rgb(R, G, B)" -> blendColours(split, [117,50,227],
// min(1, charge/120)) -> "rgb(R,G,B)" (blendColours floors each channel;
// marth's overlay string has NO spaces, unlike the palette's).
void marth_blend_overlay(MlSim *S, double p);

// --- the dancing blade helper modules (characters/marth/*.js) ----------------
void marth_dancingBladeCombo(MlSim *S, double p, double min, double max,
                             const MlInputBuffer in[4]);
void marth_dancingBladeAirMobility(MlSim *S, double p);

// --- the marth module index (characters/marth/moves/index.js) ----------------
const MlMoveDef *marth_move_def(const char *name);
// marth[<name>].init(p, input) — the checkFor* payload dispatch sites.
void marth_moves_init(MlSim *S, const char *name, double p,
                      const MlInputBuffer in[4]);

// --- special phase surfaces (shared moves.h mv_register_special_phases) -----
// marth's non-phase move fns: onClank on DOWNSPECIAL{GROUND,AIR}
// (hitDetection.js:71-72, 2-arg (p, input)).
AsTri marth_DOWNSPECIALGROUND_onClank(MlSim *S, double p,
                                      const MlInputBuffer in[4],
                                      const MvX *ex);
AsTri marth_DOWNSPECIALAIR_onClank(MlSim *S, double p,
                                   const MlInputBuffer in[4], const MvX *ex);
MvFn marth_special_phase(const char *state, const char *phase);

// --- the 75 marth move definitions (one per upstream file) -------------------
extern const MlMoveDef marth_ATTACKAIRB, marth_ATTACKAIRD, marth_ATTACKAIRF,
    marth_ATTACKAIRN, marth_ATTACKAIRU, marth_ATTACKDASH, marth_CATCHATTACK,
    marth_CLIFFATTACKQUICK, marth_CLIFFATTACKSLOW, marth_CLIFFESCAPEQUICK,
    marth_CLIFFESCAPESLOW, marth_CLIFFGETUPQUICK, marth_CLIFFGETUPSLOW,
    marth_CLIFFJUMPQUICK, marth_CLIFFJUMPSLOW, marth_DOWNATTACK,
    marth_DOWNSMASH, marth_DOWNSPECIALAIR, marth_DOWNSPECIALAIR2,
    marth_DOWNSPECIALGROUND, marth_DOWNSPECIALGROUND2, marth_DOWNTILT,
    marth_FORWARDSMASH, marth_FORWARDTILT, marth_GRAB, marth_JAB1,
    marth_JAB2, marth_NEUTRALSPECIALAIR, marth_NEUTRALSPECIALGROUND,
    marth_SIDESPECIALAIR, marth_SIDESPECIALAIR2FORWARD,
    marth_SIDESPECIALAIR2UP, marth_SIDESPECIALAIR3DOWN,
    marth_SIDESPECIALAIR3FORWARD, marth_SIDESPECIALAIR3UP,
    marth_SIDESPECIALAIR4DOWN, marth_SIDESPECIALAIR4FORWARD,
    marth_SIDESPECIALAIR4UP, marth_SIDESPECIALGROUND,
    marth_SIDESPECIALGROUND2FORWARD, marth_SIDESPECIALGROUND2UP,
    marth_SIDESPECIALGROUND3DOWN, marth_SIDESPECIALGROUND3FORWARD,
    marth_SIDESPECIALGROUND3UP, marth_SIDESPECIALGROUND4DOWN,
    marth_SIDESPECIALGROUND4FORWARD, marth_SIDESPECIALGROUND4UP,
    marth_THROWBACK, marth_THROWDOWN, marth_THROWFORWARD,
    marth_THROWNFOXBACK, marth_THROWNFOXDOWN, marth_THROWNFOXFORWARD,
    marth_THROWNFOXUP, marth_THROWNMARTHBACK, marth_THROWNMARTHDOWN,
    marth_THROWNMARTHFORWARD, marth_THROWNMARTHUP, marth_THROWNPUFFBACK,
    marth_THROWNPUFFDOWN, marth_THROWNPUFFFORWARD, marth_THROWNPUFFUP,
    marth_THROWUP, marth_UPSMASH, marth_UPSPECIAL, marth_UPTILT,
    marth_THROWNFALCOUP, marth_THROWNFALCODOWN, marth_THROWNFALCOBACK,
    marth_THROWNFALCOFORWARD, marth_THROWNFALCONUP, marth_THROWNFALCONDOWN,
    marth_THROWNFALCONBACK, marth_THROWNFALCONFORWARD, marth_APPEAL;

#endif // ML_MARTH_MOVES_H
