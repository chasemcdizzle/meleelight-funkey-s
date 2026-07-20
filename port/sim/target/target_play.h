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
  // activeStage.target for the ACTIVE target stage (TTAB1-decoded)
  Vec2D target[ML_MAX_TARGETS];
  int targetCount;
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

// targetplay.js:224-255 targetHitDetection(p) — verbatim, incl. the
// double-destroy quirk (the article loop still runs for a target the
// hitbox loop just destroyed: targetDestroyed[i] was only checked at the
// top of the i-iteration — carried faithfully).
void tp_target_hit_detection(GameState *g, double p);

// targetplay.js:281-288 targetTimerTick — matchTimer stopwatch
// (+0.016667, capped < 6000; the jQuery HUD writes are render plane).
void tp_target_timer_tick(GameState *g);

// main.js:987-1044 gameTick's gameMode == 5 arm under the harness step
// semantics (the sim_game_tick twin): endTargetGame -> finishGame is a
// LOUD TRAP (FOH plane + outside the golden quality domain), START-quit
// endGame likewise. traceRow0 = the injected pollInputs result for slot
// 0 (the only active slot).
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
