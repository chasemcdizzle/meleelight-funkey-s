// port/sim/target/target_play.c — src/target/targetplay.js's SIM slice,
// translated verbatim (fix_plan §M4 task 11; rules 1-18; every upstream
// line cited). See target_play.h for scope + state notes.
//
// Faithfulness notes (measured iter 94):
// - Math.pow sites keep the upstream call shape via fd_pow (rule 6/13 —
//   never algebraic x*x), matching hit_detection.c's Math.pow discipline.
// - hitTargetCollision reads hitboxes.id[j].offset[hitboxes.frame] with
//   NO frame clamp and dereferences .x DIRECTLY (targetplay.js:259-263 —
//   unlike executeRegularHit's frame>1 clamp): an absent element is
//   upstream's undefined.x TypeError -> loud trap at the exact
//   dereference (the hb_offset_req class, hit_detection.c:88).
// - The DOUBLE-DESTROY quirk (targetHitDetection): targetDestroyed[i] is
//   checked once at the top of the i-iteration; when the hitbox loop
//   destroys target i, the article loop below STILL runs and can destroy
//   it AGAIN in the same frame (targetsDestroyed increments twice, two
//   vfx/sfx events, and the strict == in destroyTarget can then STEP PAST
//   activeStage.target.length so endTargetGame never fires) — carried
//   verbatim, never "fixed".
// - destroyTarget's vfx {name:"targetDestroy", pos:activeStage.target[i]}
//   consumes ZERO seeded draws (drawVfx.js: only circleDust draws).
#include "target_play.h"

#include <string.h>

#include "../../fdlibm/fdlibm.h"
#include "../ml_events.h"
#include "../ml_js.h"
#include "../ml_ser.h"
#include "ml_targets.h" // TTAB1 (generated; -I pipeline build dir)

MlTargets TP; // zero-init: page-boot values (targetplay.js:34-38 lets)

// The verbatim 10-slot targetDestroyed literal (targetplay.js:37) IS the
// authored cap (review-94 M5) — if either side ever moves, this dies at
// compile time instead of reopening the OOB window.
_Static_assert(ML_MAX_TARGETS == 10,
               "ML_MAX_TARGETS must equal the upstream 10-slot "
               "targetDestroyed literal (targetplay.js:37)");
_Static_assert(sizeof TP.targetDestroyed / sizeof TP.targetDestroyed[0] ==
                   ML_MAX_TARGETS,
               "targetDestroyed[] must hold exactly ML_MAX_TARGETS slots");

// --- slot deref (P()/slot() are hit_detection.c statics; same semantics) -----

static MlPlayer *tp_P(GameState *g, double p) {
  const int k = (int)p;
  if (p != (double)k || k < 0 || k > 3 || !g->sim.playerPresent[k]) {
    sim_fatal("target_play: player deref out of domain");
  }
  return &g->sim.player[k];
}

// --- TTAB1 -> MlStageX (the sim_stage_from_stab1 twin) ------------------------

static Surface ttab_surface(const ml_tstage_surface_t *s) {
  Surface out;
  out.p0.x = ml_target_f64(s->x1);
  out.p0.y = ml_target_f64(s->y1);
  out.p1.x = ml_target_f64(s->x2);
  out.p1.y = ml_target_f64(s->y2);
  // NO authored stage carries the optional SurfaceProperties third
  // element (ml_targets.h; measured iter 94) — no props, no damageType.
  out.hasProps = false;
  out.propsHasDamageTypeKey = false;
  out.propsDamageType = damage_absent();
  return out;
}

static void ttab_list(SurfaceList *out, const ml_tstage_surface_t *items,
                      int32_t count) {
  if (count > ML_MAX_SURFACES) sim_fatal("TTAB1 surface list over cap");
  out->count = count;
  for (int32_t k = 0; k < count; k++) out->items[k] = ttab_surface(&items[k]);
}

void tp_stage_from_ttab1(int tstageId, MlStageX *out) {
  if (tstageId < 0 || tstageId >= ML_TSTAGE_COUNT) sim_fatal("tstage id");
  const ml_tstage_t *st = &ml_tstages[tstageId];
  memset(out, 0, sizeof *out);
  ttab_list(&out->s.ground, st->ground, st->groundCount);
  ttab_list(&out->s.ceiling, st->ceiling, st->ceilingCount);
  ttab_list(&out->s.platform, st->platform, st->platformCount);
  ttab_list(&out->s.wallL, st->wallL, st->wallLCount);
  ttab_list(&out->s.wallR, st->wallR, st->wallRCount);
  // no `connected` key on any authored target stage (TTAB1 pin): the
  // physics reads fall into their absent arms exactly like fdest/ystory.
  out->hasConnected = false;
  if (st->ledgeCount > ML_MAX_LEDGES) sim_fatal("TTAB1 ledge list over cap");
  out->ledgeCount = st->ledgeCount;
  for (int32_t k = 0; k < st->ledgeCount; k++) {
    out->ledge[k].list = st->ledge[k].type == 0 ? 'g' : 'p';
    out->ledge[k].index = (double)st->ledge[k].index;
    out->ledge[k].point = (double)st->ledge[k].side;
  }
  out->blastzone.min.x = ml_target_f64(st->blastzone[0]);
  out->blastzone.min.y = ml_target_f64(st->blastzone[1]);
  out->blastzone.max.x = ml_target_f64(st->blastzone[2]);
  out->blastzone.max.y = ml_target_f64(st->blastzone[3]);
  // respawnPoints/respawnFace: ABSENT on authored target stages — a
  // REBIRTH dispatch (upstream: undefined deref) traps via respawnCount 0
  // (characters/shared/moves/REBIRTH.c bounds check). Unreachable in the
  // golden domain anyway: target-mode isFinalDeath() is unconditionally
  // true (actionStateShortcuts.js:153, gameMode == 5).
  out->respawnCount = 0;
}

// --- destroyTarget (targetplay.js:211-222) ------------------------------------

static void tp_destroy_target(int i) {
  TP.targetDestroyed[i] = true;                       // :212
  TP.targetsDestroyed = TP.targetsDestroyed + 1;      // :213 targetsDestroyed++
  // drawVfx({name:"targetDestroy", pos: activeStage.target[i]}) (:214-217)
  ml_drawVfx_p("targetDestroy", TP.target[i].x, TP.target[i].y);
  ml_sound_play("targetBreak");                       // :218
  if (TP.targetsDestroyed == (double)TP.targetCount) { // :219 strict ==
    TP.endTargetGame = true;                          // :220 setEndTargetGame
  }
}

// --- hitTargetCollision (targetplay.js:257-267) --------------------------------

// offset[frame] element read: CHARDATA per-frame array only; anything
// else is upstream's undefined.x TypeError -> trap (header note).
static Vec2D tp_hb_offset_req(const MlHitboxSpec *hb, double idx,
                              const char *site) {
  const int k = (int)idx;
  if (hb->shape != ML_HB_CHARDATA || idx != (double)k || k < 0 ||
      k >= hb->offsetLen) {
    sim_fatal(site);
  }
  return hb->offsetArr[k];
}

static bool tp_hitTargetCollision(GameState *g, double p, double j, int t,
                                  bool previous) {
  MlPlayer *pp = tp_P(g, p);
  const int jj = (int)j;
  Vec2D hbpos;
  if (previous) { // :258-260
    const Vec2D o =
        tp_hb_offset_req(&pp->phys.prevFrameHitboxes.id[jj],
                         pp->phys.prevFrameHitboxes.frame,
                         "hitTargetCollision prev offset[frame].x");
    hbpos = vec2d(pp->phys.posPrev.x + (o.x * pp->phys.facePrev),
                  pp->phys.posPrev.y + o.y);
  } else { // :261-263
    const Vec2D o = tp_hb_offset_req(&pp->hitboxes.id[jj], pp->hitboxes.frame,
                                     "hitTargetCollision offset[frame].x");
    hbpos = vec2d(pp->phys.pos.x + (o.x * pp->phys.face),
                  pp->phys.pos.y + o.y);
  }
  const Vec2D targetPos = vec2d(TP.target[t].x, TP.target[t].y); // :264
  // :266 — Math.pow shapes verbatim
  return fd_pow(targetPos.x - hbpos.x, 2) + fd_pow(hbpos.y - targetPos.y, 2) <=
         fd_pow(pp->hitboxes.id[jj].size + 7, 2);
}

// --- articleTargetCollision (targetplay.js:269-279) -----------------------------

static bool tp_articleTargetCollision(GameState *g, int a, int t,
                                      bool previous) {
  const MlArticle *ar = &g->arts.a[a];
  const Vec2D hbpos = previous ? ar->posPrev : ar->pos; // :270-274
  const Vec2D targetpos = vec2d(TP.target[t].x, TP.target[t].y); // :276
  // :278 — Math.pow shapes verbatim
  return fd_pow(targetpos.x - hbpos.x, 2) + fd_pow(hbpos.y - targetpos.y, 2) <=
         fd_pow(ar->hb.size + 7, 2);
}

// --- targetHitDetection (targetplay.js:224-255) ---------------------------------

void tp_target_hit_detection(GameState *g, double p) {
  for (int i = 0; i < TP.targetCount; i++) { // :225
    if (!TP.targetDestroyed[i]) {            // :226
      MlPlayer *pp = tp_P(g, p);
      for (int j = 0; j < 4; j++) {          // :227
        if (pp->hitboxes.active[j]) {        // :228
          // :229 — the || / && short-circuit shape verbatim
          if (tp_hitTargetCollision(g, p, (double)j, i, false) ||
              (pp->hitboxes.active[j] &&
               pp->phys.prevFrameHitboxes.active[j] &&
               (tp_hitTargetCollision(g, p, (double)j, i, true) ||
                interpolatedHitCircleCollision(
                    &g->sim, vec2d(TP.target[i].x, TP.target[i].y), 7, p,
                    (double)j)))) {
            pp->hasHit = true;               // :230
            tp_destroy_target(i);            // :231
            break;                           // :232 (the j loop)
          }
        }
      }
      for (int a = 0; a < g->arts.count; a++) { // :236
        // var articleDestroyed = false (:237) — dead upstream, carried as
        // this comment only (no observable).
        bool interpolate;                    // :238-243
        if (g->arts.a[a].timer > 1) {
          interpolate = true;
        } else {
          interpolate = false;
        }
        // :244 — shape verbatim
        if (tp_articleTargetCollision(g, a, i, false) ||
            (interpolate &&
             (tp_articleTargetCollision(g, a, i, true) ||
              interpolatedArticleCircleCollision(
                  &g->arts, a, vec2d(TP.target[i].x, TP.target[i].y), 7)))) {
          // articles[aArticles[a].name].canTurboCancel (:245): LASER
          // false, ILLUSION true (article.js:35/:124; article.c:403).
          if (g->arts.a[a].kind == ART_ILLUSION) {
            tp_P(g, g->arts.a[a].player)->hasHit = true; // :246
          }
          tp_destroy_target(i);              // :248
          // destroyArticleQueue.push(a) (:249) — duplicates allowed
          // (article.h note); cap = the article queue cap.
          if (g->arts.destroyCount >= ART_CAP) {
            sim_fatal("destroyArticleQueue over cap (target arm)");
          }
          g->arts.destroyQ[g->arts.destroyCount++] = (double)a;
          break;                             // :250 (the a loop)
        }
      }
    }
  }
}

// --- targetTimerTick (targetplay.js:281-288) -------------------------------------

void tp_target_timer_tick(GameState *g) {
  if (g->matchTimer + 0.016667 < 6000) { // :282
    // addMatchTimer(0.016667) (main.js setMatchTimer family)
    g->matchTimer += 0.016667;           // :283
  }
  // :284-287 jQuery HUD writes (#matchMinutes/#matchSeconds): render
  // plane, pure reads — no sim effect.
}

// --- setup: targetselect entry + startTargetGame(p, false) ----------------------

// A45 T2: the setup body below is IDENTICAL for an authored (TTAB1) and a
// custom (.mlstage) stage — only where the geometry, the targets and the
// starting point come FROM differs. So the body lives here once, taking
// them as arguments, and BOTH entries route through it: tp_setup_target
// decodes TTAB1 (below), tp_setup_target_custom decodes an MlkStage
// (custom_stage.c). Duplicating startTargetGame would leave two
// translations of one upstream function to keep in sync — the exact
// failure HARD RULE 5 makes expensive.
void tp_setup_target_core(GameState *g, int charId, double playingId,
                          const MlStageX *stage, const Vec2D *targets,
                          int targetCount, Vec2D startingPoint) {
  // The page state the REAL flow leaves before startTargetGame — the
  // harnessSetupMatch writes (patch:76-92) projected to the 1-player
  // target domain (slot 0 human keyboard; 1-3 off), exactly what the
  // recorder (port/goldens-m4/run-target.js) performs in-page:
  g->sim.playerType[0] = 0;
  g->sim.playerPresent[0] = true;
  g->inp.mType[0] = ML_MTYPE_KEYBOARD;
  g->inp.currentPlayers[0] = 0;
  g->sim.characterSelections[0] = charId;
  g->cpuDifficulty[0] = 3;
  g->slotIsAi[0] = false;
  for (int i = 1; i < 4; i++) {
    g->sim.playerType[i] = -1;
    g->sim.playerPresent[i] = false;
    g->inp.currentPlayers[i] = -1;
  }
  // setActiveStageTarget(tstageId) (targetselect.js:143; activeStage.js
  // :49-51 targetStageMapping) — activeStage now points at the target
  // stage; stageKind stays 0: the mode-5 arm NEVER calls movingPlatforms
  // (main.js:987-1044 — measured absence). For a custom stage the same
  // assignment is activeStage.js:83 setActiveStageCustomTarget.
  g->sim.stage = *stage;
  // A45 T6 — derived from the stage the SIM will read, never from which
  // entry point we came through. physics tests `wall[2] !== undefined ?
  // wall[2].damageType : null` for TRUTHINESS, so a props object carrying
  // a NULL damageType (upstream BUG 1's output for every sixth surface,
  // and what the builder's own toggle writes) is INERT and reads false
  // here exactly as it does there.
  {
    const SurfaceList *lists[5] = {&stage->s.ground, &stage->s.ceiling,
                                   &stage->s.wallL, &stage->s.wallR,
                                   &stage->s.platform};
    TP.stageHasDamage = false;
    for (int L = 0; L < 5 && !TP.stageHasDamage; L++) {
      for (int k = 0; k < lists[L]->count; k++) {
        if (lists[L]->items[k].hasProps &&
            lists[L]->items[k].propsDamageType.tag == DT_STR) {
          TP.stageHasDamage = true;
          break;
        }
      }
    }
  }
  // setTargetStagePlaying(tstageId) (targetselect.js:145)
  TP.targetStagePlaying = playingId;
  // activeStage.target -> the module's decoded copy
  // review-94 M5: LOUD death outside the measured authored domain
  // 1..ML_MAX_TARGETS (never truncation) — above the cap the
  // targetDestroyed plane would index out of bounds.
  if (targetCount < 1 || targetCount > ML_MAX_TARGETS) {
    sim_fatal("target count outside 1..ML_MAX_TARGETS (the measured "
              "authored cap; refusing — never truncated)");
  }
  TP.targetCount = targetCount;
  for (int k = 0; k < targetCount; k++) TP.target[k] = targets[k];
  // sounds.menuForward.play() (targetselect.js:127): menu-plane Howl, no
  // seeded draw — not part of the sim slice.

  // ---- startTargetGame(p = 0, test = false) (targetplay.js:178-209) ----
  const int p = 0; // targetselect passes the pressing slot; slot 0 here
  TP.endTargetGame = false;      // :179 setEndTargetGame(false)
  // :181 test arm: BUILDER plane (stageTemp) — scope-excluded; test is
  // hardwired false on this path.
  // :184 holiday == 0 in the harness clock domain: no createSnow (the
  // VS startGame twin — check-sim's 8/8 pins the same fact).
  TP.targetTesting = false;      // :186 targetTesting = test
  // setBackgroundType(Math.round(Math.random())) (:187) — the ONE
  // off-step seeded draw (the startGame twin; value render-only).
  (void)js_round(ml_random());
  g->sim.gameMode = 5;           // :188 changeGamemode(5)
  g->inp.gameMode = 5;
  for (int k = 0; k < 10; k++) TP.targetDestroyed[k] = false; // :189
  TP.targetsDestroyed = 0;       // :190
  // resetVfxQueue() (:191): render plane (ml_events vfx queue resets per
  // tick stage).
  art_resetAArticles(&g->arts);  // :192 resetAArticles()
  // initializePlayers(p, true) (:193; main.js:1305-1312): buildPlayerObject
  // + the target-arm entrance vfx at activeStage.startingPoint[0].
  sim_build_player(g, p);
  {
    const double spx = startingPoint.x;
    const double spy = startingPoint.y;
    ml_drawVfx_p("entrance", spx, spy);
    // renderPlayer(p) (:194): render plane.
    // player[p].phys.pos = new Vec2D(activeStage.startingPoint[0].x, .y)
    // (:196) — fresh Vec2D (no pos-ECB1 alias; sim_build_player reset it)
    g->sim.player[p].phys.pos = vec2d(spx, spy);
  }
  g->matchTimer = 0;             // :197 setMatchTimer(0)
  g->startTimer = 1.5;           // :198 setStartTimer(1.5)
  g->starting = true;            // :199 setStarting(true)
  ml_drawVfx_p("start", 0, 0);   // :200-203 drawVfx {name:"start",(0,0)}
  // setFindingPlayers(false) (:204): page plane. setPlaying(true) (:205):
  g->inp.playing = true;
  g->sim.player[p].inCSS = false; // :207
  g->sim.player[p].stocks = 1;    // :208
  // gameSettings: the harness cookie domain's defaults (the VS twin —
  // settings.js:44-56 under getGameplayCookies zeroes; qjs class note)
  g->sim.versusMode = 0;
  g->sim.lCancelType = 0;
  g->sim.turbo = false;
  g->sim.phantomThreshold = 0.01;
  for (int i = 0; i < 4; i++) g->sim.tapJumpOff[i] = 0;
  // module queues start empty (the sim_setup_match posture)
  hd_resetHitQueue(&g->hq);
  g->hq.phqCount = 0;
  memset(&g->arts, 0, sizeof g->arts);
  sim_chd_reset();
  g->frame = 0;
}

// A45 T2: NULL here (see target_play.h); custom_stage.c installs it.
bool (*tp_custom_setup)(GameState *g, int charId, const char *dir, int slot,
                        const char **why) = 0;

void tp_setup_target(GameState *g, int charId, int tstageId) {
  MlStageX stage;
  tp_stage_from_ttab1(tstageId, &stage); // dies loudly on a bad id
  const ml_tstage_t *st = &ml_tstages[tstageId];
  Vec2D targets[ML_MAX_TARGETS];
  // The cap check is the CORE's (one site for both entries); this loop
  // must not run past our own array first.
  const int n = st->targetCount;
  if (n >= 1 && n <= ML_MAX_TARGETS) {
    for (int k = 0; k < n; k++) {
      targets[k].x = ml_target_f64(st->target[k].x);
      targets[k].y = ml_target_f64(st->target[k].y);
    }
  }
  const Vec2D sp = vec2d(ml_target_f64(st->startingPoint.x),
                         ml_target_f64(st->startingPoint.y));
  tp_setup_target_core(g, charId, (double)tstageId, &stage, targets, n, sp);
}

// --- finishGame, target arm (main.js:1420-1476) — REAL since iter 99 ------------

void (*tp_finish_hook)(GameState *g, bool complete) = 0;
void (*tp_endgame_hook)(GameState *g) = 0;

void tp_finish_game(GameState *g) {
  TP.endTargetGame = false; // :1421 setEndTargetGame(false)
  TP.gameEnd = true;        // :1422 gameEnd = true
  g->inp.playing = false;   // :1423 playing = false
  // :1425-1429 fg2 banner state = render plane. :1431 STRICT equality
  // (the double-destroy quirk can step PAST the count -> Failure arm,
  // carried verbatim, never "fixed").
  const bool complete = TP.targetsDestroyed == (double)TP.targetCount;
  // :1432-1449 medals/targetRecords/cookies = task-13 persistence
  // (REGISTERED deferral); the newRecord/complete/failure sounds are
  // menu-plane Howls (sounds.js — zero seeded draws, measured); the
  // Complete!/Failure banner is render. All FOH-driver surface:
  if (tp_finish_hook) tp_finish_hook(g, complete);
}

// --- the mode-5 gameTick arm (main.js:987-1044) ----------------------------------

void tp_game_tick_target(GameState *g, const MlInput *traceRow0) {
  ml_ev_reset();
  // if (endTargetGame) finishGame(input) (:988-990) — REAL (iter 99;
  // t01/t02 never reach it: endTargetGame stays false in their domain).
  // finishGame reads nothing from `input` in the target arm (measured).
  if (TP.endTargetGame) {
    tp_finish_game(g);
  }
  // if (playing || frameByFrame) (:991): playing is true for the live
  // golden domain; false ONLY post-finish (gameEnd, :1041-1044 — the
  // else arm's `if (!gameEnd) interpretInputs(...)` does nothing when
  // gameEnd is true). playing false WITHOUT gameEnd stays a loud trap.
  if (!g->inp.playing && !TP.gameEnd) {
    sim_fatal("target tick with playing false outside gameEnd");
  }
  // let input = [nullInputs() x4] (main.js:919 — created before the arms)
  for (int i = 0; i < 4; i++) {
    nullInputs(&g->curBuf[i]);
    ai_null_inputs(&g->curBufAi[i]);
  }
  if (g->inp.playing) {
    const int tb = 0; // targetBuilder (targetbuilder.js:25) — slot 0
    hd_resetHitQueue(&g->hq);               // :996
    art_destroyArticles(&g->arts);          // :997
    art_executeArticles(&g->sim, &g->arts); // :998
    ml_ev_reset();
    if (!g->starting) {                     // :999-1001
      // input[tb] = interpretInputs(tb, true, playerType[tb], old[tb])
      if (traceRow0 == 0) sim_fatal("trace row missing for the target player");
      const MlInput polled = ml_poll_inputs(traceRow0);
      ml_interpret_inputs(&g->inp, tb, true, g->sim.playerType[tb],
                          &g->prevBuf[tb], &polled, &g->curBuf[tb]);
    }
    // update(tb, input) (:1002; main.js:894-908): playerType 0 human — the
    // runAI arm's playerType==1 condition is false; physics(tb, input).
    {
      MlInput in4[4];
      for (int k = 0; k < 4; k++) in4[k] = g->curBuf[tb].slot[k];
      ml_physics(&g->sim, (double)tb, in4);
      // A45 T6 — the routing upstream does implicitly by pushing straight
      // into `hitQueue` (physics.js:53). hd_executeHits runs on the very
      // next lines, which is main.js:1002-1003's own order.
      if (TP.stageHasDamage) hd_route_stage_damage(&g->sim, &g->hq);
      if (g->sim.hqCount != 0 && !TP.stageHasDamage) {
        // dealWithDamagingStageCollision rows: still measured IMPOSSIBLE on
        // a stage with no damaging surface — every authored target stage
        // (zero damageType surfaces, iter 94) and the VS-trap twin. The
        // trap is NARROWED by A45 T6, not removed: on a stage that HAS
        // damaging surfaces the rows are ordinary work and hd_executeHits
        // consumes them on the very next line, which is upstream's own
        // order at main.js:1002-1003. Golden: t03.
        sim_fatal("physics pushed stage-damage hq rows on a target stage "
                  "with NO damaging surface (measured impossible)");
      }
    }
    ml_ev_reset();
    hd_executeHits(&g->sim, &g->hq);        // :1003
    tp_target_hit_detection(g, (double)tb); // :1004
    if (!g->starting) {                     // :1005-1011
      tp_target_timer_tick(g);
    } else {
      g->startTimer -= 0.01666667;
      if (g->startTimer < 0) {
        g->starting = false;
      }
    }
    // if (input[tb][0].s && !input[tb][1].s) endGame(input) (:1013-1015):
    // the quit path is MENU plane, so the sim owns only the edge test —
    // what "leaving" means belongs to the driver, exactly like the
    // tp_finish_hook seam above. Default (NULL hook) is the unchanged
    // loud trap: every trace-fed replay keeps it, because those goldens
    // never press START, so a START there really is a domain break. The
    // FOH live PLAY driver installs a hook instead (punch-list A2 — the
    // acceptance surface this trap was registered against).
    if (g->curBuf[tb].slot[0].s && !g->curBuf[tb].slot[1].s) {
      if (tp_endgame_hook == 0) {
        sim_fatal("endGame — START pressed in target mode (outside the "
                  "target-golden quality domain)");
      }
      tp_endgame_hook(g);
    }
    // frameByFrame bookkeeping (:1016-1021) — the input cluster's
    // end-of-tick contract (the VS arm's twin block).
    ml_input_end_of_tick(&g->inp);
  }
  // window.__nextInputBuffers = input (patch:49-53): this tick's buffers
  // are next tick's oldInputBuffers — including the starting window's
  // fresh null buffers (slot-0 history does NOT chain through starting)
  // AND the post-finish ticks' untouched nulls (the patch runs per
  // gameTick call regardless of arm).
  for (int i = 0; i < 4; i++) {
    g->prevBuf[i] = g->curBuf[i];
    g->prevBufAi[i] = g->curBufAi[i];
  }
}

// --- the target-plane frame envelope (iter-63 separate-stream convention) --------

void tp_target_frame_hash(const GameState *g, char out_hex[65]) {
  static MlSb sb;
  static bool init = false;
  if (!init) {
    ml_sb_init(&sb);
    init = true;
  }
  ml_sb_reset(&sb);
  ml_sb_puts(&sb, "{\"endTargetGame\":");
  ml_sb_bool(&sb, TP.endTargetGame);
  ml_sb_puts(&sb, ",\"matchTimer\":");
  ml_sb_num(&sb, g->matchTimer);
  ml_sb_puts(&sb, ",\"targetDestroyed\":[");
  for (int k = 0; k < 10; k++) { // the VERBATIM 10-slot let (:37/:189)
    if (k) ml_sb_putc(&sb, ',');
    ml_sb_bool(&sb, TP.targetDestroyed[k]);
  }
  ml_sb_puts(&sb, "],\"targetsDestroyed\":");
  ml_sb_num(&sb, TP.targetsDestroyed);
  ml_sb_putc(&sb, '}');
  ml_sha256_hex(sb.buf, sb.len, out_hex);
}
