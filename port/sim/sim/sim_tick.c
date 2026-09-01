// sim_tick.c — the integrated mode-3 gameTick (M2 task 17) + the host
// implementations of every cross-module seam the clusters declare extern.
//
// Tick body: upstream main.js gameTick's `playing || frameByFrame` branch
// (main.js:1050-1092), translated verbatim in call order:
//   resetHitQueue -> getActiveStage().movingPlatforms() ->
//   destroyArticles -> executeArticles ->
//   for i (playerType > -1): [!starting: interpretInputs] + update(i) ->
//   checkPhantoms -> hitDetect x4 -> executeHits ->
//   articlesHitDetection -> executeArticleHits ->
//   (!starting && !versusMode ? matchTimerTick : startTimer countdown) ->
//   frameByFrame bookkeeping (ml_input_end_of_tick).
// update(i) (main.js:894-908): !starting && currentPlayers[i] != -1 &&
// playerType[i]==1 && actionState != "SLEEP" -> runAI(i), then
// physics(i, input). runAI has TWO arms (M4 task 5): the LIVE C ai.c
// (default when sim_ai_live.c is linked and no --ai-bridge is given;
// draws live off the seeded chain) and the M2 task-16 AIBRIDGE1 archival
// path (--ai-bridge; retained as the frozen M2 contract — check-sim.sh's
// build has ONLY this arm).
//
// SEAM WIRING (replaces the replay drivers' oracle-fed seams with the
// REAL upstream import graph):
// - mlp_dispatch / hd_dispatch -> mv_dispatch (tasks 7-12 real bodies);
// - mlp_hd_* -> hit_detection.c getters (task 6 real bodies);
// - mv_article_* -> article.c inits (the task-13 documented swap);
// - mv_hq_push6 -> the live hitQueue;
// - runAI -> ml_runAI via the ml_sim_runai_live pointer seam (sim.h), or
//   ml_ai_bridge_apply under --ai-bridge; + the pollInputs bank-row alias
//   re-copy in both arms;
// - every *_out_of_domain / *_fail -> sim_fatal (loud, HARD RULE 2).
//
// EVENT QUEUES: ml_events' sound/vfx/dispatch-note/rng-log queues are
// fixed-cap observability channels sized for per-boundary replay records;
// nothing consumes them in the integrated headless sim yet (the M4 mixer
// will own a real per-frame drain), so the host resets them at each tick
// stage to keep a heavy frame from overflowing the caps.
#include <string.h>

#include "sim.h"
#include "sim_modstate.h" // ticket #29: the live-AI snapshot seam
#include "../characters/shared/moves.h"
#include "../characters/fox/moves.h"
#include "../characters/falco/moves.h"
#include "../characters/marth/moves.h"

// --- host seam: fatal ----------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>

void sim_fatal(const char *what) {
  fprintf(stderr, "SIM FATAL frame %ld: %s\n", G.frame, what);
  exit(3);
}

// live-AI pointer seam (sim.h; M4 task 5): NULL unless sim_ai_live.c is
// linked — its constructor installs the real ai.c driver. The frozen
// M2-gate build (check-sim.sh's TU list) never links it.
void (*ml_sim_runai_live)(GameState *g, int i) = 0;
void (*ml_sim_ai_cov_dump)(void) = 0;

// finishGame pointer seam (sim.h; punch-list C18): NULL unless the live PLAY
// driver installs it. All five finishGame sites keep their verbatim
// out-of-domain trap on the NULL arm, so every trace-, flow- and golden-fed
// run behaves exactly as before.
void (*ml_sim_finish_hook)(void) = 0;

// snapshot pointer seam (sim.h; ticket #28): NULL unless sim_snapshot.c is
// linked — its constructor installs the real hooks. The frozen M2-gate build
// never links it, so both stay NULL and sim_main.c's frame loop is the loop
// it always was.
long (*ml_sim_snap_boot)(GameState *g) = 0;
void (*ml_sim_snap_frame)(GameState *g, long frame) = 0;

// live-AI SNAPSHOT seam (sim_modstate.h; ticket #29): NULL unless
// sim_ai_live.c is linked, and defined HERE for the same reason the two
// pointers above are — sim_snapshot.c is built by rigs that never link ai.c,
// so it cannot name a symbol that TU owns, while sim_tick.c is on every list.
// NULL is not "skip the row": it is a row of ZERO bytes, which changes the
// payload total and therefore the build identity, so a snapshot written by a
// build WITH the live AI is refused by name by a build without it.
size_t (*ml_ai_live_snap_bytes)(void) = 0;
void (*ml_ai_live_snap_save)(void *dst) = 0;
void (*ml_ai_live_snap_load)(const void *src) = 0;

// TARGET-PLANE SNAPSHOT seam (sim_modstate.h; ticket #30): NULL unless
// port/sim/target/target_play.c is linked, and defined HERE for exactly the
// reason the live-AI trio above is — sim_snapshot.c is built by rigs that
// never link the target plane (check-sim-snapshot.sh derives its TU list from
// the M2 gate, which has no target TU), so it cannot name a target_play.c
// symbol, while sim_tick.c is on every list there is. NULL is a row of ZERO
// bytes, so a VS-only build and a build that can play target stages have
// different payload totals and therefore different build identities: neither
// can load the other's snapshot by accident.
size_t (*ml_targets_snap_bytes)(void) = 0;
void (*ml_targets_snap_save)(void *dst) = 0;
void (*ml_targets_snap_load)(const void *src) = 0;

void mv_out_of_domain(const char *what) { sim_fatal(what); }
void ml_phys_out_of_domain(const char *what) { sim_fatal(what); }
void ml_hd_out_of_domain(const char *what) { sim_fatal(what); }
void ml_art_out_of_domain(const char *what) { sim_fatal(what); }
void ml_asshort_out_of_domain(const char *what) { sim_fatal(what); }
void ml_input_out_of_domain(const char *what) { sim_fatal(what); }
void ml_events_fail(const char *what) { sim_fatal(what); }
void pc_fail(const char *msg) { sim_fatal(msg); }
void ml_canon_fail(const char *msg) { sim_fatal(msg); }

// mv_seam: every actionStates entry of every char is registered
// (sim_data_register) — an unregistered dispatch is a composition bug.
AsTri mv_seam(MlSim *S, double charId, const char *state, const char *phase,
              double slot, const MvX *ex) {
  (void)S; (void)charId; (void)phase; (void)slot; (void)ex;
  fprintf(stderr, "mv_seam: unregistered state %s\n", state);
  sim_fatal("mv_seam crossed — registry incomplete");
}

// --- host seam: move dispatches from physics / hitDetection ---------------------

static MvX mvx_from_extras(const MlDispExtra *extras, int count) {
  MvX x;
  memset(&x, 0, sizeof x);
  x.count = count;
  for (int k = 0; k < count && k < 2; k++) x.x[k] = extras[k];
  return x;
}

void mlp_dispatch(MlSim *sim, const MlDispCall *call) {
  const MvX ex = mvx_from_extras(call->extras, call->extraCount);
  (void)mv_dispatch(sim, call->charId, call->state, call->phase, call->slot,
                    G.curBuf, call->extraCount ? &ex : 0);
}

void hd_dispatch(MlSim *sim, HdQueues *q, const HdDispCall *call) {
  (void)q; // rows enter via the live mv_hq_push6 below
  const MvX ex = mvx_from_extras(call->extras, call->extraCount);
  (void)mv_dispatch(sim, call->charId, call->state, call->phase, call->slot,
                    G.curBuf, call->extraCount ? &ex : 0);
}

// --- host seam: the task-6 launch getters (real bodies) -------------------------

double mlp_hd_getLaunchAngle(MlSim *sim, double trajectory, double knockback,
                             bool hasReverse, bool reverse, double x, double y,
                             double v) {
  // getLaunchAngle's lazy player[v].phys.grounded read (hit_detection.h)
  return hd_getLaunchAngle(trajectory, knockback, hasReverse, reverse, x, y,
                           sim->player[(int)v].phys.grounded);
}
double mlp_hd_getHorizontalVelocity(MlSim *sim, double knockback,
                                    double angle) {
  (void)sim;
  return hd_getHorizontalVelocity(knockback, angle);
}
double mlp_hd_getVerticalVelocity(MlSim *sim, double knockback, double angle,
                                  bool grounded, double trajectory) {
  (void)sim;
  return hd_getVerticalVelocity(knockback, angle, grounded, trajectory);
}
double mlp_hd_getHorizontalDecay(MlSim *sim, double angle) {
  (void)sim;
  return hd_getHorizontalDecay(angle);
}
double mlp_hd_getVerticalDecay(MlSim *sim, double angle) {
  (void)sim;
  return hd_getVerticalDecay(angle);
}

// --- host seam: move-code hitQueue pushes (THROW rows etc.) ---------------------

void mv_hq_push6(MlSim *S, double a, double b, double c, bool d, bool e,
                 bool f) {
  (void)S;
  if (G.hq.hqCount >= HD_HQ_CAP) sim_fatal("hitQueue over cap");
  HdRow *r = &G.hq.hq[G.hq.hqCount++];
  memset(r, 0, sizeof *r);
  r->v = a;
  r->aIsObj = false;
  r->a = b;
  r->h = c;
  r->shieldHit = d;
  r->isThrow = e;
  r->drawBounce = f;
  r->hasPhantom = false;
}

// --- host seam: article spawns (the task-13 seam-to-body swap) -------------------

static ArtOptBool art_opt_absent(void) {
  ArtOptBool o; o.has = false; o.v = false; return o;
}
static ArtOptBool art_opt(bool v) {
  ArtOptBool o; o.has = true; o.v = v; return o;
}

// fox: options {p, x, y, rotate} — no isFox/partOfThrow keys (task 8)
void mv_article_laser(MlSim *S, double p, double x, double y, double rotate) {
  art_laser_init(S, &G.arts, p, x, y, rotate, art_opt_absent(),
                 art_opt_absent());
}
void mv_article_illusion(MlSim *S, double p, double type) {
  art_illusion_init(S, &G.arts, p, type, art_opt_absent());
}
// falco: isFox:false always; THROWDOWN lasers add partOfThrow:true (task 9)
void mv_article_laser_falco(MlSim *S, double p, double x, double y,
                            double rotate, bool partOfThrow) {
  art_laser_init(S, &G.arts, p, x, y, rotate, art_opt(false),
                 partOfThrow ? art_opt(true) : art_opt_absent());
}
void mv_article_illusion_falco(MlSim *S, double p, double type) {
  art_illusion_init(S, &G.arts, p, type, art_opt(false));
}

// --- host seam: marth's Howl play-id consumption (task 11) ----------------------
// The upstream value is howler's GLOBAL play-id counter — audio-plane
// state consumed into player.shieldBreakerID, which is NOT on the
// checksum surface (only .stop(id) reads it back). A monotone counter is
// value-faithful in every checksummed respect; the M4 mixer owns real ids.
static double g_howl_counter = 1000; // howler ids start above 1000
double mv_howl_play_id(const char *name) {
  (void)name;
  return ++g_howl_counter;
}

// ticket #28: this counter is MODULE state that a snapshot has to carry —
// the ids it mints are stored in players (marth's shieldBreakerID) and read
// back by `.stop(id)`, so a resumed match that restarted it would re-issue
// an id a live voice already holds. Declared in sim_modstate.h.
double sim_tick_howl_counter_get(void) { return g_howl_counter; }
void sim_tick_howl_counter_set(double v) { g_howl_counter = v; }

// --- the moving-platforms bridge (task 14 -> the live plane) --------------------

static void tick_moving_platforms(GameState *g) {
  MpSim M;
  memset(&M, 0, sizeof M);
  const int nPlat = g->sim.stage.s.platform.count;
  if (nPlat > MP_MAX_PLATFORMS) sim_fatal("platform count over MpSim cap");
  M.nPlat = nPlat;
  for (int k = 0; k < nPlat; k++) {
    M.platform[k][0] = g->sim.stage.s.platform.items[k].p0;
    M.platform[k][1] = g->sim.stage.s.platform.items[k].p1;
  }
  M.ps[0] = g->ps[0];
  M.ps[1] = g->ps[1];
  M.starting = g->starting;
  for (int j = 0; j < 4; j++) {
    if (g->sim.playerPresent[j]) {
      const MlPhysics *ph = &g->sim.player[j].phys;
      M.player[j].grounded = ph->grounded;
      M.player[j].onSurface[0] = ph->onSurface[0];
      M.player[j].onSurface[1] = ph->onSurface[1];
      M.player[j].pos = ph->pos;
    } else {
      M.player[j] = g->inactiveMp[j];
    }
  }

  mp_movingPlatforms(g->stageKind, &M);

  for (int k = 0; k < nPlat; k++) {
    g->sim.stage.s.platform.items[k].p0 = M.platform[k][0];
    g->sim.stage.s.platform.items[k].p1 = M.platform[k][1];
  }
  g->ps[0] = M.ps[0];
  g->ps[1] = M.ps[1];
  for (int j = 0; j < 4; j++) {
    if (g->sim.playerPresent[j]) {
      MlPhysics *ph = &g->sim.player[j].phys;
      ph->grounded = M.player[j].grounded;
      ph->onSurface[0] = M.player[j].onSurface[0];
      ph->onSurface[1] = M.player[j].onSurface[1];
      // upstream rider/transfer arms write pos COMPONENTS through the
      // live pos object (ystory.js:75-77, fountain.js:131) — mirror
      // through the pos-ECB1[0] alias exactly like move code does.
      // Bit-compare is alias-sound: an aliased pos/ECB1[0] pair is equal
      // by construction, so a bit-equal write is a no-op on both.
      uint64_t oldBits, newBits;
      memcpy(&oldBits, &ph->pos.x, 8);
      memcpy(&newBits, &M.player[j].pos.x, 8);
      if (oldBits != newBits) mv_pos_set_x(&g->sim, j, M.player[j].pos.x);
      memcpy(&oldBits, &ph->pos.y, 8);
      memcpy(&newBits, &M.player[j].pos.y, 8);
      if (oldBits != newBits) mv_pos_set_y(&g->sim, j, M.player[j].pos.y);
    } else {
      g->inactiveMp[j] = M.player[j];
    }
  }
}

// --- AI-slot buffer projection (rule 16: truthiness buttons) ---------------------

static MlInput ml_from_ai_truthy(const MlAiInput *t) {
  MlInput o = nullInput();
  // buttons: consumed by JS truthiness everywhere (rule 16, measured —
  // ai.js writes NUMBER buttons; no raw button value propagates)
  o.a = aiv_truthy(t->a); o.b = aiv_truthy(t->b);
  o.x = aiv_truthy(t->x); o.y = aiv_truthy(t->y);
  o.z = aiv_truthy(t->z); o.l = aiv_truthy(t->l);
  o.r = aiv_truthy(t->r); o.s = aiv_truthy(t->s);
  o.du = aiv_truthy(t->du); o.dl = aiv_truthy(t->dl);
  o.dr = aiv_truthy(t->dr); o.dd = aiv_truthy(t->dd);
  // analogs: arithmetic consumers — must be number-tagged (undefined is
  // zero-live on the CPU goldens; a first appearance must be LOUD)
  if (!aiv_as_num(t->lA, &o.lA) || !aiv_as_num(t->rA, &o.rA) ||
      !aiv_as_num(t->lsX, &o.lsX) || !aiv_as_num(t->lsY, &o.lsY) ||
      !aiv_as_num(t->csX, &o.csX) || !aiv_as_num(t->csY, &o.csY) ||
      !aiv_as_num(t->rawX, &o.rawX) || !aiv_as_num(t->rawY, &o.rawY) ||
      !aiv_as_num(t->rawcsX, &o.rawcsX) || !aiv_as_num(t->rawcsY, &o.rawcsY)) {
    sim_fatal("AI buffer analog outside the number domain");
  }
  return o;
}

static void project_ai_buffer(const MlAiInputBuffer *src, MlInputBuffer *dst) {
  for (int k = 0; k < 8; k++) dst->slot[k] = ml_from_ai_truthy(&src->slot[k]);
}

// --- the tick --------------------------------------------------------------------

void sim_game_tick(GameState *g, const MlInput *traceRow[4]) {
  ml_ev_reset();
  // let input = [nullInputs() x4] (main.js:919)
  for (int i = 0; i < 4; i++) {
    nullInputs(&g->curBuf[i]);
    ai_null_inputs(&g->curBufAi[i]);
  }

  hd_resetHitQueue(&g->hq);                 // main.js:1057
  tick_moving_platforms(g);                 // main.js:1058
  art_destroyArticles(&g->arts);            // main.js:1059
  art_executeArticles(&g->sim, &g->arts);   // main.js:1060
  ml_ev_reset();

  for (int i = 0; i < 4; i++) {             // main.js:1062-1069
    if (g->sim.playerType[i] <= -1) continue;
    if (!g->starting) {
      // input[i] = interpretInputs(i, true, playerType[i], old[i])
      if (g->slotIsAi[i]) {
        // pollInputs returns the aiInputBank ROW (alias; ai_bridge.h)
        const MlAiInput polled = g->bank[i][0];
        ml_ai_interpret_inputs(&g->inp, i, true, 1, &g->prevBufAi[i], &polled,
                               &g->curBufAi[i]);
      } else {
        if (traceRow[i] == 0) sim_fatal("trace row missing for a human slot");
        const MlInput polled = ml_poll_inputs(traceRow[i]);
        ml_interpret_inputs(&g->inp, i, true, g->sim.playerType[i],
                            &g->prevBuf[i], &polled, &g->curBuf[i]);
      }
    }
    // update(i, input) (main.js:894-908)
    if (!g->starting && g->inp.currentPlayers[i] != -1 &&
        g->sim.playerType[i] == 1 &&
        strcmp(g->sim.player[i].actionState, "SLEEP") != 0) {
      // runAI(i) (ai.js:874) — two arms, M4 task 5:
      if (g->hasBridge) {
        // ARCHIVAL arm: the task-16 recorded-input bridge (--ai-bridge)
        const MlAiBridgeEntry *e = ml_ai_bridge_peek(&g->bridge);
        if (e == 0) sim_fatal("AI bridge exhausted at a runAI site");
        if (e->frame != g->frame || e->slot != i) {
          fprintf(stderr, "bridge entry (frame %ld slot %d) at runAI site "
                          "(frame %ld slot %d)\n",
                  e->frame, e->slot, g->frame, i);
          sim_fatal("AI bridge (frame,slot) desync");
        }
        const MlAiBridgeApplyResult r =
            ml_ai_bridge_apply(e, &g->rng, &g->bank[i][0]);
        if (r.bad_draw != -1) {
          uint64_t want, got;
          memcpy(&want, &e->draws[r.bad_draw], 8);
          memcpy(&got, &r.bad_draw_got, 8);
          fprintf(stderr,
                  "runAI draw %d/%d: recorded %016llx, chain %016llx\n",
                  r.bad_draw, e->ndraws, (unsigned long long)want,
                  (unsigned long long)got);
          sim_fatal("AI bridge seeded-draw chain mismatch");
        }
        if (r.bad_field != -1) {
          sim_fatal("AI bridge never-AI-written field diverged from the chain");
        }
        ml_ai_bridge_advance(&g->bridge);
      } else if (ml_sim_runai_live) {
        // LIVE arm: the real C runAI (port/sim/ai.c via sim_ai_live.c) —
        // draws live off the seeded chain (logged ml_random), writes the
        // bank row + player bookkeeping itself. Never touches AIBRIDGE1.
        ml_sim_runai_live(g, i);
      } else {
        sim_fatal("CPU slot without an AI bridge artifact or live-AI TU");
      }
      // the pollInputs alias: buffer slot 0 IS the bank row post-runAI
      // (both arms — the caller's job, ai_bridge.h contract)
      g->curBufAi[i].slot[0] = g->bank[i][0];
    }
    // physics consumes the plain projection (built AFTER runAI so the
    // alias write-through is visible; rule-16 truthiness buttons)
    if (g->slotIsAi[i]) project_ai_buffer(&g->curBufAi[i], &g->curBuf[i]);
    MlInput in4[4];
    for (int k = 0; k < 4; k++) in4[k] = g->curBuf[i].slot[k];
    ml_physics(&g->sim, (double)i, in4);
    if (g->sim.hqCount != 0) {
      // dealWithDamagingStageCollision rows: zero-live on VS stages
      // (no damageType surfaces — physics.h note); M4 target stages own
      // the real routing. Loud, never silent.
      sim_fatal("physics pushed stage-damage hitQueue rows on a VS stage");
    }
    ml_ev_reset();
  }

  hd_checkPhantoms(&g->sim, &g->hq);        // main.js:1070
  for (int i = 0; i < 4; i++) {             // main.js:1071-1075
    if (g->sim.playerType[i] > -1) hd_hitDetect(&g->sim, &g->hq, (double)i);
  }
  hd_executeHits(&g->sim, &g->hq);          // main.js:1076
  ml_ev_reset();
  art_articlesHitDetection(&g->sim, &g->arts);        // main.js:1077
  art_executeArticleHits(&g->sim, &g->arts, g->curBuf); // main.js:1078
  ml_ev_reset();

  // main.js:1079. In the ENDLESS mode (versusMode != 0) upstream takes the
  // ELSE arm every frame for the rest of the match: matchTimer never ticks
  // (so it never expires and the finishGame trap below is unreachable), and
  // startTimer just keeps counting down past 0 with `starting` already
  // false — a harmless free-running decrement. Carried verbatim.
  if (!g->starting && g->sim.versusMode == 0) { // `!starting && !versusMode`
    // matchTimerTick(input) (main.js:338-350): HUD writes are render
    // ML_MATCH_TIMER_TICK is sim.h's ONE definition of upstream's literal
    // (main.js:339); the HUD guard in gfx_overlay.c reads the same symbol so
    // the two cannot drift. Textually identical double, so the emitted
    // arithmetic is unchanged — `bash port/sim/check-sim.sh` is the proof.
    g->matchTimer -= ML_MATCH_TIMER_TICK;
    if (g->matchTimer <= 0) {
      // finishGame(input) (main.js:349). NULL hook = every evidence and
      // golden run: the trap below is unchanged and still loud.
      if (ml_sim_finish_hook == 0) {
        sim_fatal("matchTimer expired (finishGame) — outside the golden "
                  "domain (trace quality contract)");
      }
      ml_sim_finish_hook();
    }
  } else {
    g->startTimer -= 0.01666667;            // main.js:1082-1085
    if (g->startTimer < 0) g->starting = false;
  }

  // frameByFrame bookkeeping (main.js:1087-1091) — the input cluster's
  // end-of-tick contract (interpret_inputs.h)
  ml_input_end_of_tick(&g->inp);

  // window.__nextInputBuffers = input (patch:49-53): this tick's buffers
  // are next tick's oldInputBuffers — including the starting window's
  // fresh null buffers.
  for (int i = 0; i < 4; i++) {
    g->prevBuf[i] = g->curBuf[i];
    g->prevBufAi[i] = g->curBufAi[i];
  }
}
