// port/sim/target/target_play.h — structure-parallel translation of
// upstream src/target/targetplay.js's SIM slice + the main.js target-test
// plumbing it needs (fix_plan §M4 task 11).
//
// SCOPE (measured, AGENT-LOG iter 94): the AUTHORED target-test path only
// — targetselect entry (setActiveStageTarget/setTargetStagePlaying/
// startTargetGame(p, false)), the per-frame targetHitDetection /
// destroyTarget / targetTimerTick, and the mode-5 gameTick arm
// (main.js:987-1044; port/sim/target/target_tick.c). The target BUILDER
// (test=true arm, stageTemp, custom/encoded stages, records/medals/
// cookies plane) is scope-EXCLUDED per fix_plan §M4 conventions — its
// arms are loud traps, never silent skips.
//
// STATE: MlTargets mirrors targetplay.js's module-level lets
// (targetTesting/targetPlayer/targetStagePlaying/targetDestroyed/
// targetsDestroyed) plus main.js's endTargetGame let, kept as C module
// state OUTSIDE GameState (upstream module scope ≠ the god-module slice;
// the pointer-seam precedent: state the frozen sim never touches lives
// outside GameState). activeStage.target arrives TTAB1-decoded at setup.
//
// The checksum stream is untouched by this module's existence: target
// state is NOT on the CHECKSUM.md §2 surface (measured — pagelib
// serializes players + articles only). Conformance for the target plane
// is the SEPARATE per-frame target stream (fix_plan §M4 iter-63
// convention), emitted by target_main.c and judged by
// port/goldens-m4/verify-target-stream.js.
#ifndef ML_TARGET_PLAY_H
#define ML_TARGET_PLAY_H

#include "../sim/sim.h" // GameState (port/sim/target/ -> port/sim/sim/)

// ML_MAX_TARGETS — THE pinned authored target-count cap (review-94 M5,
// iter 96; ONE constant, schema twin = pipeline/lib/targets-schema.js's
// identical ML_MAX_TARGETS). MEASURED over all 10 authored stages
// (executed walk, .loop/m4-tgt96-measure-keys.log == expected.json
// targets.perStage): 8 stages author 10 targets, targetstage6 authors 9,
// targetstage9 authors 1 — max = 10 == the upstream 10-slot
// targetDestroyed literal (targetplay.js:37; the _Static_assert in
// target_play.c ties the literal to the cap). tp_setup_target dies
// LOUDLY outside 1..cap (never truncation); the old 16-slot silent
// acceptance — which let targetDestroyed[] index out of bounds for
// counts 11-16 — is dead.
#define ML_MAX_TARGETS 10

typedef struct {
  // targetplay.js module lets (:34-38)
  bool targetTesting;
  double targetPlayer;
  double targetStagePlaying;
  bool targetDestroyed[10]; // literal [false x10] (:37; reset :189) ==
                            // ML_MAX_TARGETS (static-asserted)
  double targetsDestroyed;
  // main.js let endTargetGame (setEndTargetGame; read main.js:988)
  bool endTargetGame;
  // main.js let gameEnd (:68; set true by finishGame :1422, reset ONLY
  // by endGame :1373 — startTargetGame does NOT reset it, measured)
  bool gameEnd;
  // activeStage.target for the ACTIVE target stage (TTAB1-decoded)
  Vec2D target[ML_MAX_TARGETS];
  int targetCount;
  // A45 T6. Does the ACTIVE stage carry a truthy damageType on any
  // collision surface? DERIVED in tp_setup_target_core from the MlStageX
  // the sim is about to read, so it cannot disagree with what physics
  // sees, and derived for BOTH entries rather than set by the custom one —
  // an authored stage that ever grew a damaging surface would be covered
  // by the same rule instead of falling through it.
  //
  // It gates ONE thing: whether a dealWithDamagingStageCollision row is a
  // fatal impossibility or ordinary work. On a stage with no damaging
  // surface such a row cannot exist and the trap stays (it is the VS-trap
  // twin). On one that HAS them the rows are consumed by hd_executeHits
  // on the very next line, which is upstream's own order at main.js
  // :1002-1003.
  bool stageHasDamage;
} MlTargets;

extern MlTargets TP; // one module instance per process (upstream page)

// TTAB1 -> the physics stage read set (MlStageX): the target-stage twin
// of sim_stage_from_stab1. hasConnected false, respawnCount 0 (authored
// target stages carry neither; a REBIRTH dispatch traps), ledgePos never
// enters MlStageX (AI-only upstream; target mode fields no CPU).
void tp_stage_from_ttab1(int tstageId, MlStageX *out);

// The targetselect.js:143-146 entry + startTargetGame(p, false)
// (targetplay.js:178-209) over an already-page-booted GameState:
// playerType [0,-1,-1,-1] / mType keyboard / cS[0]=charId (the
// harnessSetupMatch state writes projected to the 1-player domain), then
// setActiveStageTarget(tstageId) + setTargetStagePlaying(tstageId) +
// startTargetGame(0, false) verbatim — incl. the ONE off-step seeded
// background draw (targetplay.js:187, the startGame twin).
void tp_setup_target(GameState *g, int charId, int tstageId);

// A45 T2 (D42): the same body with the stage plane passed IN rather than
// read out of TTAB1 — the ONE place startTargetGame is translated. The
// custom-stage entry (tp_setup_target_custom, target/custom_stage.h)
// routes through it, so an authored and a custom stage differ only in
// WHERE their geometry came from and in nothing else. That is what makes
// the T2 done-check a real differential: the same trace over the same
// geometry delivered by the two paths must produce byte-identical
// streams. `playingId` is upstream's targetStagePlaying (0..9 authored;
// MLK_PLAYING_BASE + slot for a custom stage, mirroring targetselect.js's
// custom slots 10..19). Dies loudly outside 1..ML_MAX_TARGETS.
void tp_setup_target_core(GameState *g, int charId, double playingId,
                          const MlStageX *stage, const Vec2D *targets,
                          int targetCount, Vec2D startingPoint);

// A45 T2 (D42): the CUSTOM-STAGE seam, installed by a constructor in
// target/custom_stage.c and NULL in every build that does not link it —
// the ml_sim_runai_live pointer-seam precedent (port/sim/sim/sim_ai_live.c),
// reused for the same reason. target_main.c's --custom arm is behind it, so
// the TU list of every EXISTING builder of target_main.c
// (check-target-sim.sh, port/foh/check-foh-flows.sh) is symbol- and
// behaviour-identical and neither script needs a line changed. A --custom
// run against a build that did not link custom_stage.c is refused, loudly,
// rather than silently doing something else.
// Loads <dir>/custom<slot>.mlstage, validates it, and performs
// tp_setup_target_custom. False + *why on any refusal; nothing reaches the
// sim on false.
extern bool (*tp_custom_setup)(GameState *g, int charId, const char *dir,
                               int slot, const char **why);

// targetplay.js:224-255 targetHitDetection(p) — verbatim, incl. the
// double-destroy quirk (the article loop still runs for a target the
// hitbox loop just destroyed: targetDestroyed[i] was only checked at the
// top of the i-iteration — carried faithfully).
void tp_target_hit_detection(GameState *g, double p);

// targetplay.js:281-288 targetTimerTick — matchTimer stopwatch
// (+0.016667, capped < 6000; the jQuery HUD writes are render plane).
void tp_target_timer_tick(GameState *g);

// The target-mode finishGame (main.js:1420-1476, gameMode-5 arm) —
// REAL since iter 99 (M4 task 12; the trap's domain gained coverage):
// setEndTargetGame(false) (:1421), gameEnd = true (:1422), playing =
// false (:1423). The banner/gradient plane (:1425-1460) is render;
// medals/records/cookies are the task-13 persistence surface
// (REGISTERED deferral); the finish sounds (newRecord/complete/
// failure — sounds.js menu-plane Howls, zero seeded draws, measured)
// belong to the FOH driver. All of those are reached via
// tp_finish_hook (NULL default — sim-only replays perform exactly the
// sim-observable finish; the FOH apps install it at boot). `complete`
// = the :1431 STRICT equality activeStage.target.length ==
// targetsDestroyed (the double-destroy quirk can overshoot the count
// — then the upstream Failure arm is what runs, carried verbatim).
extern void (*tp_finish_hook)(GameState *g, bool complete);
void tp_finish_game(GameState *g);

// endGame's START-quit edge (main.js:1013-1015). NULL default keeps the
// loud trap in tp_game_tick_target — trace-fed replays never press
// START, so a START there is a domain break, not a quit. The FOH live
// PLAY driver installs a hook; what "leaving the match" means is the
// driver's, not the sim's (the tp_finish_hook split, verbatim). The
// hook fires INSIDE the tick: upstream does not return from gameTick
// here, so the remaining tick body still runs on the quitting frame.
extern void (*tp_endgame_hook)(GameState *g);

// main.js:987-1044 gameTick's gameMode == 5 arm under the harness step
// semantics (the sim_game_tick twin): endTargetGame -> finishGame is
// REAL (tp_finish_game above; iter 99); post-finish ticks mirror
// :991/:1041-1044 exactly (playing false + gameEnd true -> the whole
// body is skipped; playing false WITHOUT gameEnd stays a loud trap).
// START-quit endGame TRAPS for every caller that leaves
// tp_endgame_hook NULL — which is every trace-fed/evidence caller
// (target_main.c, foh_app.c, the probes): their goldens never press
// START, so a START edge there is a domain break, not a quit. ONLY the
// FOH live PLAY driver may install the hook and handle the edge; see
// the tp_endgame_hook note below. traceRow0 = the injected pollInputs
// result for slot 0 (the only active slot).
void tp_game_tick_target(GameState *g, const MlInput *traceRow0);

// The target-plane frame envelope (fix_plan §M4 iter-63 separate-stream
// convention; format doc: port/goldens-m4/verify-target-stream.js
// header): fixed-literal order
//   {"endTargetGame":<T/F>,"matchTimer":<num>,"targetDestroyed":[<T/F>
//    x10],"targetsDestroyed":<num>}
// under the CHECKSUM.md §3 value rules (ml_sb_num / ml_sb_bool), then
// SHA-256 lowercase hex into out_hex[65].
void tp_target_frame_hash(const GameState *g, char out_hex[65]);

#endif // ML_TARGET_PLAY_H
