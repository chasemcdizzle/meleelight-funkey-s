// target_finish_probe.c — the standing mechanical probe for the
// endTargetGame -> finishGame seam (fix_plan §M4 task 12; the
// target_hq_probe precedent).
//
// WHY A PROBE (measured refutation, AGENT-LOG iter 99): targetstage9 —
// the only authored 1-target stage — authors its target inside a
// TOPOLOGICALLY SEALED region (the g7/g18/room system is bounded by
// solid boxes on every side; every candidate entry measured blocked on
// the bit-exact sim: box4 wall, g12 pillar (106 units, unjumpable),
// ceiling7 underside caps the hole column from below, box19||box14 seal
// it from above). The single target is UNREACHABLE in the authored
// game, so NO recorded browser trace can reach all-broken there, and a
// 10-target completion golden elsewhere is outside the pre-registered
// budget. The finish seam therefore gets MECHANICAL live coverage here
// (both arms + the double-destroy quirk), with browser-parity of the
// finish frames structurally evidenced by per-line citation
// (target_play.c tp_finish_game / tp_game_tick_target) — registered
// honest-coverage note in AGENT-LOG iter 99.
//
// Scenario A — the COMPLETE arm through FULL REAL TICKS on tstage 8:
//   spawn a REAL laser (art_laser_init — the upstream init incl. the
//   spawn-frame main) aimed so the mode-5 tick pipeline
//   (destroyArticles -> executeArticles -> ... -> targetHitDetection)
//   destroys the single target via the ARTICLE arm: targetsDestroyed
//   == targetCount (targetplay.js:219 strict ==) -> endTargetGame.
//   The NEXT tick runs the REAL finish (main.js:988-990 -> 1420-1423):
//   endTargetGame false / gameEnd true / playing false, hook fired
//   ONCE with complete=true; the target envelope reflects T -> F; a
//   POST-FINISH tick is the :1041-1044 skip arm (state frozen).
// Scenario B — the FAILURE arm + the DOUBLE-DESTROY QUIRK live:
//   fresh tp_setup_target (gameEnd persists TRUE across relaunch —
//   startTargetGame never resets it, measured; carried faithfully),
//   then one tp_target_hit_detection call with BOTH a fabricated
//   active player hitbox over the target AND a laser over the target:
//   the hitbox loop destroys target 0 (endTargetGame fires at == 1),
//   the article loop destroys it AGAIN (the targetplay.js:226
//   top-of-iteration-check quirk, carried verbatim) ->
//   targetsDestroyed == 2 past the count. The finish tick then takes
//   the STRICT-EQUALITY overshoot arm (main.js:1431: 2 != 1) ->
//   hook complete=false (upstream "Failure").
// Drop tooth (check-target-sim.sh): --drop skips scenario A's laser —
// the break never happens and every assertion fails loudly (exit 1).
//
// Usage: target_finish_probe --simdata <s.txt> [--drop]
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../ml_events.h"
#include "target_play.h"

static int g_fail;
static void expect(bool ok, const char *what) {
  if (!ok) {
    fprintf(stderr, "PROBE FAIL: %s\n", what);
    g_fail = 1;
  }
}

// hook recorder (installed before any tick; the seam's only consumer here)
static int g_hook_fired;
static bool g_hook_complete;
static double g_hook_timer_bits_set;
static void rec_hook(GameState *g, bool complete) {
  g_hook_fired++;
  g_hook_complete = complete;
  g_hook_timer_bits_set = g->matchTimer;
}

static MlInput g_neutral;

static void run_tick(void) { tp_game_tick_target(&G, &g_neutral); }

int main(int argc, char **argv) {
  const char *simdataPath = 0;
  bool drop = false;
  for (int i = 1; i < argc; i++) {
    const char *a = argv[i];
    if (strcmp(a, "--simdata") == 0 && i + 1 < argc) simdataPath = argv[++i];
    else if (strcmp(a, "--drop") == 0) drop = true;
    else {
      fprintf(stderr, "target_finish_probe: bad argument %s\n", a);
      return 1;
    }
  }
  if (!simdataPath) {
    fprintf(stderr, "usage: target_finish_probe --simdata s.txt [--drop]\n");
    return 1;
  }

  sim_boot_page(&G);
  sim_data_load(simdataPath);
  sim_data_register();
  ml_active_rng = &G.rng;
  ml_rng_seed(&G.rng, 4899u);
  g_neutral = nullInput();
  tp_finish_hook = rec_hook;

  // ---- scenario A: COMPLETE arm through full real ticks ---------------------
  tp_setup_target(&G, 2, 8); // fox on targetstage9 (the 1-target stage)
  expect(TP.targetCount == 1, "A: tstage 8 authors exactly 1 target");
  // run out the 1.5 s starting window (91 ticks; inputs ignored)
  for (int t = 0; t < 92; t++) {
    G.frame = t + 1;
    run_tick();
  }
  expect(!G.starting, "A: starting window elapsed");
  expect(G.inp.playing, "A: playing before the break");
  const double timer0 = G.matchTimer;
  expect(timer0 > 0, "A: matchTimer ticked during play");

  // In-flight laser aimed at the target: the NEXT tick's REAL pipeline
  // (art_destroyArticles -> art_executeArticles moves it +7 with the
  // wall sweep -> tp_target_hit_detection's article arm) does the
  // break. Direct fill, not art_laser_init: the init's wallDetection
  // sweeps from posPrev = (player.x, y) — a cross-map sweep from the
  // spawn column dies at the sealed-region walls (measured this
  // session), and the seam under test is the tick pipeline, not init.
  if (!drop) {
    memset(&G.arts, 0, sizeof G.arts);
    G.arts.count = 1;
    MlArticle *la = &G.arts.a[0];
    la->kind = ART_LASER;
    la->player = 0;
    la->destroyOnHit = true;
    la->timer = 2;
    la->pos = vec2d(141.6 - 7.0, -4.0); // one tick main from the target
    la->posPrev = la->pos;
    la->posPrev1 = la->pos;
    la->posPrev2 = la->pos;
    la->posPrev3 = la->pos;
    la->vel = vec2d(7, 0);
    la->hb.size = 1.172;
    for (int k = 0; k < 4; k++) la->ecb[k] = la->pos;
    la->ecb[1].x += 10;
    la->ecb[3].x -= 10;
    la->ecb[0].y -= 0.01;
    la->ecb[2].y += 0.01;
    expect(G.arts.count == 1, "A: laser staged");
  }
  G.frame += 1;
  run_tick(); // executeArticles moves the laser onto the target; the
              // ARTICLE arm of tp_target_hit_detection destroys it
  expect(TP.targetDestroyed[0], "A: target 0 destroyed (article arm)");
  expect(TP.targetsDestroyed == 1, "A: targetsDestroyed == 1");
  expect(TP.endTargetGame, "A: endTargetGame fired (strict == arm)");
  expect(G.inp.playing, "A: still playing on the break tick");

  // envelope reflects the armed state (T) before the finish tick
  char hexBefore[65], hexAfter[65];
  tp_target_frame_hash(&G, hexBefore);

  const double timerAtBreak = G.matchTimer;
  G.frame += 1;
  run_tick(); // THE FINISH TICK (main.js:988-990 -> finishGame)
  expect(g_hook_fired == 1, "A: finish hook fired exactly once");
  expect(g_hook_complete, "A: hook complete=true (1 == 1 strict equality)");
  expect(!TP.endTargetGame, "A: endTargetGame reset by finishGame (:1421)");
  expect(TP.gameEnd, "A: gameEnd set (:1422)");
  expect(!G.inp.playing, "A: playing false (:1423)");
  expect(G.matchTimer == timerAtBreak,
         "A: matchTimer frozen on the finish tick (no timer tick)");
  expect(g_hook_timer_bits_set == timerAtBreak,
         "A: hook observed the final matchTimer");
  tp_target_frame_hash(&G, hexAfter);
  expect(strcmp(hexBefore, hexAfter) != 0,
         "A: target envelope hash changed across the finish (T -> F)");

  // POST-FINISH tick: the :1041-1044 skip arm — state frozen
  const Vec2D posF = G.sim.player[0].phys.pos;
  G.frame += 1;
  run_tick();
  expect(g_hook_fired == 1, "A: hook did NOT re-fire post-finish");
  expect(G.matchTimer == timerAtBreak, "A: matchTimer frozen post-finish");
  expect(G.sim.player[0].phys.pos.x == posF.x &&
             G.sim.player[0].phys.pos.y == posF.y,
         "A: player frozen post-finish");
  char hexPost[65];
  tp_target_frame_hash(&G, hexPost);
  expect(strcmp(hexAfter, hexPost) == 0,
         "A: target envelope stable post-finish");

  // ---- scenario B: FAILURE arm + the double-destroy quirk -------------------
  g_hook_fired = 0;
  tp_setup_target(&G, 2, 8);
  expect(TP.gameEnd, "B: gameEnd persists across relaunch (startTargetGame "
                     "never resets it — measured upstream)");
  expect(G.inp.playing, "B: playing again after relaunch");
  expect(TP.targetsDestroyed == 0 && !TP.targetDestroyed[0],
         "B: target plane reset by startTargetGame");
  MlPlayer *p0 = &G.sim.player[0];
  // fabricated ACTIVE hitbox over the target (the hb_offset_req domain:
  // CHARDATA + in-range frame index)
  p0->phys.pos = vec2d(141.6, -4.0);
  memset(&p0->hitboxes, 0, sizeof p0->hitboxes);
  p0->hitboxes.active[0] = true;
  p0->hitboxes.frame = 0;
  p0->hitboxes.id[0].shape = ML_HB_CHARDATA;
  p0->hitboxes.id[0].offsetLen = 1;
  p0->hitboxes.id[0].offsetArr[0] = vec2d(0, 0);
  p0->hitboxes.id[0].size = 7;
  // a laser resting on the target for the article loop (direct fill —
  // this scenario pins the DETECTION-call quirk, not the tick pipeline)
  memset(&G.arts, 0, sizeof G.arts);
  G.arts.count = 1;
  G.arts.a[0].kind = ART_LASER;
  G.arts.a[0].player = 0;
  G.arts.a[0].pos = vec2d(141.6, -4.0);
  G.arts.a[0].posPrev = vec2d(141.6, -4.0);
  G.arts.a[0].hb.size = 1.172;
  G.arts.a[0].timer = 1;
  tp_target_hit_detection(&G, 0);
  expect(TP.targetsDestroyed == 2,
         "B: DOUBLE-DESTROY quirk live — hitbox arm + article arm both "
         "destroyed target 0 in one call (targetplay.js:226 top-check)");
  expect(TP.endTargetGame,
         "B: endTargetGame fired at the == 1 moment and stays set");
  G.frame += 1;
  run_tick(); // finish tick: strict == overshoot arm (2 != 1)
  expect(g_hook_fired == 1, "B: finish hook fired exactly once");
  expect(!g_hook_complete,
         "B: hook complete=false (main.js:1431 strict equality — the "
         "overshot count takes the upstream Failure arm)");
  expect(!TP.endTargetGame && TP.gameEnd && !G.inp.playing,
         "B: finish state (endTargetGame F / gameEnd T / playing F)");

  if (g_fail) {
    fprintf(stderr, "TARGET FINISH PROBE FAIL%s\n", drop ? " (drop arm)" : "");
    return 1;
  }
  printf("TARGET FINISH PROBE OK\n");
  return 0;
}
