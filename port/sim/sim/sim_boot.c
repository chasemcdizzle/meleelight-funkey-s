// sim_boot.c — match boot for the integrated sim (M2 task 17):
// upstream player.js constructors + main.js start()/harnessSetupMatch/
// startGame, translated verbatim (each block cites its source), plus the
// STAB1 -> MlStageX stage builder.
//
// The oracle boot sequence this reproduces (oracle/harness/run.js +
// oracle/meleelight-harness.patch):
//   page boot: mulberry32 seeded, 465 boot draws (menu plane; the qjs
//     boot pin, CLAUDE.md M0 task 6), main.js start() builds 4 page-boot
//     players (characterSelections [0,0,0,0]) with face=1 / "WAIT"
//     (main.js:1546-1550), one gameMode-20 gameTick runs (findPlayers —
//     no sim effect; its `input` local = 4x nullInputs becomes
//     oldInputBuffers for frame 1);
//   counters reset (run.js:150) -> harnessSetupMatch(cfg) -> startGame().
//
// gameSettings: the browser oracle runs with NO stored cookies —
// getCookie returns localStorage.getItem == null for every key, so
// getGameplayCookies (menus/gameplaymenu.js:14-21) keeps the settings.js
// DEFAULTS: turbo 0, lCancelType 0, phantomThreshold 0.01, tapJumpOff* 0.
// (The qjs shim gotcha — Number("") zeroing phantomThreshold — was a
// SHIM parity bug, not oracle behavior; CLAUDE.md M0 task 6 note.)
#include <math.h>
#include <string.h>

#include "sim.h"
#include "../ml_events.h"

GameState G;

// main.js:168-169 — module CODE literals (not stage data; the platform-
// rail-constant class, task 14): CSS-era spawn points + faces.
static const double kStartingPoint[4][2] = {
    {-50, 50}, {50, 50}, {-25, 5}, {25, 5}};
static const double kStartingFace[4] = {1, -1, 1, -1};

// --- STAB1 -> the physics stage read set (MlStageX) ---------------------------

static Surface stab_surface(const ml_stage_surface_t *s) {
  Surface out;
  out.p0.x = ml_stage_f64(s->x1);
  out.p0.y = ml_stage_f64(s->y1);
  out.p1.x = ml_stage_f64(s->x2);
  out.p1.y = ml_stage_f64(s->y2);
  // "the optional SurfaceProperties third element does not exist on any
  // VS stage at the pin" (ml_stages.h) — no props, no damageType key.
  out.hasProps = false;
  out.propsHasDamageTypeKey = false;
  out.propsDamageType = damage_absent();
  return out;
}

static void stab_list(SurfaceList *out, const ml_stage_surface_t *items,
                      int32_t count) {
  if (count > ML_MAX_SURFACES) sim_fatal("STAB1 surface list over cap");
  out->count = count;
  for (int32_t k = 0; k < count; k++) out->items[k] = stab_surface(&items[k]);
}

// STAB1 conn label type enum (ml_stages.h): 0 g, 1 p, 2 c, 3 wallL,
// 4 wallR -> the upstream single-char label strings physics branches on.
static char stab_conn_char(int32_t t) {
  switch (t) {
    case 0: return 'g';
    case 1: return 'p';
    case 2: return 'c';
    case 3: return 'l';
    case 4: return 'r';
    default: sim_fatal("STAB1 conn label type out of domain");
  }
}

static MlConnHalf stab_conn_half(const ml_stage_conn_label_t *l) {
  MlConnHalf h;
  h.present = l->present != 0;
  h.type = h.present ? stab_conn_char(l->type) : 'g';
  h.index = h.present ? (double)l->index : 0;
  return h;
}

void sim_stage_from_stab1(int stageId, MlStageX *out) {
  if (stageId < 0 || stageId >= ML_STAGE_COUNT) sim_fatal("stage id");
  const ml_stage_t *st = &ml_stages[stageId];
  memset(out, 0, sizeof *out);
  stab_list(&out->s.ground, st->ground, st->groundCount);
  stab_list(&out->s.ceiling, st->ceiling, st->ceilingCount);
  stab_list(&out->s.platform, st->platform, st->platformCount);
  stab_list(&out->s.wallL, st->wallL, st->wallLCount);
  stab_list(&out->s.wallR, st->wallR, st->wallRCount);
  out->hasConnected = st->hasConnected != 0;
  if (out->hasConnected) {
    if (st->groundCount > ML_MAX_SURFACES ||
        st->platformCount > ML_MAX_SURFACES) {
      sim_fatal("STAB1 connected list over cap");
    }
    out->connGroundCount = st->groundCount;
    for (int32_t k = 0; k < st->groundCount; k++) {
      out->connGround[k].l = stab_conn_half(&st->connectedGround[k].l);
      out->connGround[k].r = stab_conn_half(&st->connectedGround[k].r);
    }
    out->connPlatformCount = st->platformCount;
    for (int32_t k = 0; k < st->platformCount; k++) {
      out->connPlatform[k].l = stab_conn_half(&st->connectedPlatform[k].l);
      out->connPlatform[k].r = stab_conn_half(&st->connectedPlatform[k].r);
    }
  }
  if (st->ledgeCount > ML_MAX_LEDGES) sim_fatal("STAB1 ledge list over cap");
  out->ledgeCount = st->ledgeCount;
  for (int32_t k = 0; k < st->ledgeCount; k++) {
    // upstream ledge[j] = [list-string, index, point]; list is "ground" |
    // "platform" (MlLedge.list keeps the first char, physics.h note)
    out->ledge[k].list = st->ledge[k].type == 0 ? 'g' : 'p';
    out->ledge[k].index = (double)st->ledge[k].index;
    out->ledge[k].point = (double)st->ledge[k].side;
  }
  out->blastzone.min.x = ml_stage_f64(st->blastzone[0]);
  out->blastzone.min.y = ml_stage_f64(st->blastzone[1]);
  out->blastzone.max.x = ml_stage_f64(st->blastzone[2]);
  out->blastzone.max.y = ml_stage_f64(st->blastzone[3]);
  out->respawnCount = ML_STAGE_PLAYERS;
  for (int k = 0; k < ML_STAGE_PLAYERS; k++) {
    out->respawnPoints[k].x = ml_stage_f64(st->respawnPoints[k].x);
    out->respawnPoints[k].y = ml_stage_f64(st->respawnPoints[k].y);
    out->respawnFace[k] = (double)st->respawnFace[k];
  }
}

// --- player.js constructors (verbatim translation) -----------------------------

// player.js:7-16 ActiveHitbox(size, offset, dmg, angle, kg, bk, sk, type)
static MlHitboxSpec active_hitbox_zero(void) {
  MlHitboxSpec hb;
  memset(&hb, 0, sizeof hb);
  hb.shape = ML_HB_CONSTRUCTOR;
  hb.offset.x = 0;
  hb.offset.y = 0;
  return hb;
}

// player.js:17-24 createHitboxes()
static MlHitboxes create_hitboxes(void) {
  MlHitboxes h;
  memset(&h, 0, sizeof h);
  for (int j = 0; j < 4; j++) h.active[j] = false;
  h.frame = 0;
  h.hasFrames = false; // `frames` is runtime-added (ml_player.h)
  h.hitListLen = 0;
  for (int j = 0; j < 4; j++) h.id[j] = active_hitbox_zero();
  return h;
}

// player.js:25-102 physicsObject(pos, face); pos is the [x,y] ARRAY.
static void physics_object(MlPhysics *ph, double posX, double posY,
                           double face) {
  memset(ph, 0, sizeof *ph);
  ph->cVel = vec2d(0, 0);
  ph->kVel = vec2d(0, 0);
  ph->kDec = vec2d(0, 0);
  ph->pos = vec2d(posX, posY); // new Vec2D(pos[0], pos[1])
  ph->posPrev = vec2d(0, 0);
  ph->posDelta = vec2d(0, 0);
  ph->grounded = false;
  ph->airborneTimer = 0;
  ph->fastfalled = false;
  ph->face = face;
  const Vec2D ecb[4] = {vec2d(0, 0), vec2d(3, 7), vec2d(0, 14), vec2d(-3, 7)};
  for (int k = 0; k < 4; k++) {
    ph->ECBp[k] = ecb[k];
    ph->ECB1[k] = ecb[k];
    ph->ECB2[k] = ecb[k];
  }
  ph->ecbpUndef = 0;
  ph->ecb1Undef = 0;
  ph->onSurface[0] = 0;
  ph->onSurface[1] = 0;
  ph->doubleJumped = false;
  ph->shieldHP = 60;
  ph->shieldSize = 0;
  ph->shieldAnalog = 0;
  ph->shielding = false;
  ph->shieldPosition = vec2d(0, 0);
  ph->shieldPositionReal = vec2d(0, 0);
  ph->shieldStun = 0;
  ph->powerShieldActive = false;
  ph->powerShieldReflectActive = false;
  ph->powerShielded = false;
  ph->onLedge = -1;
  ph->ledgeSnapBoxF = box2d(0, 5, 8, 10);
  ph->ledgeSnapBoxB = box2d(0, 5, -8, 10);
  ph->ledgeRegrabTimeout = 0;
  ph->ledgeRegrabCount = false;
  ph->hurtbox = box2d(-4, 18, 4, 0);
  ph->hurtBoxState = 0;
  ph->intangibleTimer = 0;
  ph->invincibleTimer = 0;
  ph->lCancel = false;
  ph->lCancelTimer = 0;
  ph->autoCancel = false;
  ph->landingLagScaling = 1;
  ph->passFastfall = false;
  ph->jabCombo = false;
  ph->sideBJumpFlag = true;
  ph->charging = false;
  ph->chargeFrames = 0;
  ph->stuckTimer = 0;
  ph->techTimer = 0;
  ph->grabbedBy = -1;
  ph->grabbing = -1;
  ph->dashbuffer = false;
  ph->jumpType = 0;
  ph->jumpSquatType = 0;
  ph->wallJumpTimer = 254;
  ph->canWallJump = js_bool(false);
  ph->upbAngleMultiplier = 0;
  ph->thrownHitbox = false;
  ph->thrownHitboxOwner = -1;
  ph->landingMultiplier = 15;
  ph->wallJumpCount = 0;
  ph->prevFrameHitboxes = create_hitboxes();
  ph->interPolatedHitboxLen = 0;         // this.interPolatedHitbox = []
  ph->interPolatedHitboxPhantomLen = 0;  // = []
  ph->isInterpolated = false;
  ph->facePrev = 1;
  ph->jumpsUsed = 0;
  ph->releaseFrame = 0;
  ph->vCancelTimer = 0;
  ph->shoulderLockout = 0;
  ph->inShine = 0;
  ph->jabReset = false;
  ph->outOfCameraTimer = 0;
  ph->rollOutDistance = 0;
  ph->bTurnaroundTimer = 0;
  ph->bTurnaroundDirection = 1;
  ph->groundAngle = M_PI / 2; // Math.PI/2 (same IEEE-754 constant)
  ph->raptorBoost = false;
  ph->hasPassing = false; // runtime-added by physics.js:1067
  // every other runtime-added presence flag stays false (memset)
}

// player.js:126-167 playerObject(character, pos, face); charAttributes/
// charHitboxes are the M1 CTAB1 tables (not modeled — ml_player.h note);
// percentShake is the CHECKSUM.md §7 exclusion.
static void player_object(MlPlayer *p, double posX, double posY, double face) {
  memset(p, 0, sizeof *p);
  physics_object(&p->phys, posX, posY, face);
  strcpy(p->actionState, "ENTRANCE");
  p->prevActionState[0] = 0; // ""
  p->timer = 0;
  p->showLedgeGrabBox = false;
  p->showECB = false;
  p->showHitbox = false;
  p->spawnWaitTime = 0;
  p->hitboxes = create_hitboxes();
  p->hit.knockback = 0;
  p->hit.hitlag = 0;
  p->hit.hitstun = 0;
  p->hit.angle = 0;
  p->hit.hitPoint = vec2d(0, 0);
  p->hit.powershield = false;
  p->hit.shieldstun = 0;
  p->hit.hasReverse = false; // runtime-added
  p->percent = 0;
  p->stocks = 4;
  p->miniView = false;
  p->miniViewPoint = vec2d(0, 0);
  p->inCSS = true;
  p->furaLoopID = 0;
  p->hasShieldBreakerID = false; // runtime-added
  p->shineLoop = 0;
  p->laserCombo = false;
  p->rotation = 0;
  p->rotationPoint = vec2d(0, 0);
  p->colourOverlay[0] = 0; // ""
  p->colourOverlayBool = false;
  strcpy(p->currentAction, "NONE");
  strcpy(p->currentSubaction, "NONE");
  p->difficulty = 4;
  p->lastMash = 0;
  p->hasHit = false;
  p->shocked = 0;
  p->burning = 0;
  p->hasIASATimer = false;
  p->hasInAerial = false;
}

// main.js:1296-1301 buildPlayerObject(i). NOTE the upstream quirk: the
// ECB1/ECBp overwrite reads `.x`/`.y` OFF THE [x,y] ARRAY startingPoint[i]
// -> Vec2D(undefined, undefined) x4 (the measured frame-1 ECB undef
// domain, ml_player.h rule-8 masks). Carried verbatim.
void sim_build_player(GameState *g, int i) {
  player_object(&g->sim.player[i], kStartingPoint[i][0], kStartingPoint[i][1],
                kStartingFace[i]);
  MlPhysics *ph = &g->sim.player[i].phys;
  const double undef = ml_stage_f64(UINT64_C(0x7ff8000000000000));
  for (int k = 0; k < 4; k++) {
    ph->ECB1[k] = vec2d(undef, undef);
    ph->ECBp[k] = vec2d(undef, undef);
  }
  ph->ecb1Undef = 0xFF; // all 8 components hold undefined at rest
  ph->ecbpUndef = 0xFF;
  g->sim.player[i].difficulty = g->cpuDifficulty[i];
  // whole-array reassignment: no pos/ECB1 alias, no hitbox aliases
  g->sim.aliasPosEcb1[i] = false;
  g->sim.aliasHbActive[i] = false;
  g->sim.aliasHbHitList[i] = false;
  g->sim.aliasHbId[i] = false;
}

// --- page boot (main.js start(), the sim-relevant slice) ----------------------

void sim_boot_page(GameState *g) {
  memset(g, 0, sizeof *g);
  ml_input_sim_state_init(&g->inp); // gameMode 20, playing false, ...
  for (int i = 0; i < 4; i++) {
    g->sim.playerType[i] = -1; // main.js:107
    g->sim.characterSelections[i] = 0; // main.js:59 [0,0,0,0]
    g->sim.playerPresent[i] = false;
    g->cpuDifficulty[i] = 3; // main.js:109
    g->inp.currentPlayers[i] = -1;
  }
  // start() (main.js:1546-1550): buildPlayerObject(i) for ALL 4 slots,
  // then face = 1 and actionState = "WAIT".
  for (int i = 0; i < 4; i++) {
    sim_build_player(g, i);
    g->sim.player[i].phys.face = 1;
    strcpy(g->sim.player[i].actionState, "WAIT");
  }
  // physics module state: ecbSquashData[i] = nullSquashDatum
  // ({location: null, factor: 1}; physics.h note 3)
  for (int i = 0; i < 4; i++) {
    g->sim.ecbSquashData[i].factor = 1;
    g->sim.ecbSquashData[i].locationIsNull = true;
    g->sim.ecbSquashData[i].location = 0;
  }
  // input chains: the boot gameTick's `input` local (4x nullInputs)
  // becomes frame 1's oldInputBuffers (main.js:1575-1576 + patch:49-53)
  for (int i = 0; i < 4; i++) {
    nullInputs(&g->prevBuf[i]);
    ai_null_inputs(&g->prevBufAi[i]);
    // aiInputBank[i] = 8x new inputData() (input.js:95-118): every row's
    // value plane == ai_null_input(); rows 1..7 are never written (sim.h)
    for (int k = 0; k < 8; k++) g->bank[i][k] = ai_null_input();
    g->slotIsAi[i] = false;
  }
  // fountain's module-private platformStates (page-boot literals,
  // fountain.js:22 — the task-14 __wpCache-injected declaration)
  g->ps[0].isStatic = false;
  g->ps[0].timer = 0;
  g->ps[0].destination = 22.125;
  g->ps[1].isStatic = false;
  g->ps[1].timer = 0;
  g->ps[1].destination = 16.125;
  // page-start CSS-era slices for slots the match leaves INACTIVE (the
  // ystory rider loop is unguarded by playerType — task 14): the boot
  // players built above.
  for (int i = 0; i < 4; i++) {
    g->inactiveMp[i].grounded = false;
    g->inactiveMp[i].onSurface[0] = 0;
    g->inactiveMp[i].onSurface[1] = 0;
    g->inactiveMp[i].pos = vec2d(kStartingPoint[i][0], kStartingPoint[i][1]);
  }
  // match lifecycle module lets (main.js:114/195/199/207)
  g->starting = true;
  g->startTimer = 1.5;
  g->matchTimer = 480;
  g->frame = 0;
  // versusMode module let (main.js:140): PAGE state, initialised 0 and
  // written ONLY by setVersusMode (main.js:237-239, called from the CSS
  // ribbon at menus/css.js:393 as `setVersusMode(1 - versusMode)` — a
  // BINARY toggle). startGame does NOT reset it, so it is set here and
  // never again by the sim: a caller that wants the endless mode writes
  // g->sim.versusMode = 1 BETWEEN sim_boot_page and sim_setup_match,
  // because startGame's stocks arm below READS it. Redundant after the
  // memset, stated for the same reason everyCharWallJump is (sim_setup
  // _match's tail): every frozen golden was recorded at 0, so the default
  // is declared, not inherited.
  g->sim.versusMode = 0;
}

// --- harnessSetupMatch + startGame ---------------------------------------------

// The 2-port wrapper (A46). Pre-A46 callers are byte-unchanged: this
// builds exactly the config the old hardcoded body wrote — port 0 human
// on p1 with difficulty undefined, port 1 p2type on p2 carrying the
// difficulty ONLY when it is the CPU (run.js:154-156), ports 2/3 absent.
void sim_setup_match(GameState *g, int p1, int p2, int p2type, int difficulty,
                     int stageId) {
  const SimPortCfg ports[4] = {
      {0, p1, -1},
      {p2type, p2, p2type == 1 ? difficulty : -1},
      {-1, 0, -1},
      {-1, 0, -1},
  };
  sim_setup_match_ports(g, ports, stageId);
}

void sim_setup_match_ports(GameState *g, const SimPortCfg ports[4],
                           int stageId) {
  // harnessSetupMatch (oracle/meleelight-harness.patch:76-92) VERBATIM:
  // one `for (var i = 0; i < 4; i++)` over cfg.players, `if (pc)` writing
  // the per-port plane and `else` pinning playerType/currentPlayers to
  // -1. mType is pinned "keyboard" for every present port; the else arm
  // deliberately does NOT touch mType/cpuDifficulty/slotIsAi — upstream
  // leaves those at their page-boot values, and so did the pre-A46 body.
  for (int i = 0; i < 4; i++) {
    if (ports[i].type > -1) {
      g->sim.playerType[i] = ports[i].type;
      g->sim.playerPresent[i] = true;
      g->inp.mType[i] = ML_MTYPE_KEYBOARD;
      g->inp.currentPlayers[i] = i;
      g->sim.characterSelections[i] = ports[i].character;
      // pc.difficulty === undefined -> 3 (patch:84)
      g->cpuDifficulty[i] = ports[i].difficulty < 0 ? 3 : ports[i].difficulty;
      g->slotIsAi[i] = ports[i].type == 1;
    } else {
      g->sim.playerType[i] = -1;
      g->sim.playerPresent[i] = false;
      g->inp.currentPlayers[i] = -1;
    }
  }
  g->stageSelect = stageId; // setStageSelect (main.js:189-191)

  // ---- startGame() (main.js:1320-1368) ----
  // setVsStage(stageSelect): activeStage points at the stage object
  g->stageKind = (MpStageKind)stageId;
  sim_stage_from_stab1(stageId, &g->sim.stage);
  // setBackgroundType(Math.round(Math.random())) — the ONE legitimate
  // off-step seeded draw (CHECKSUM.md §6); value is render-only.
  (void)ml_random();
  // holiday == 0: no createSnow. changeGamemode(3): render init + gameMode.
  g->sim.gameMode = 3;
  g->inp.gameMode = 3;
  // resetVfxQueue(): render plane (ml_events vfx queue is reset per tick)
  for (int n = 0; n < 4; n++) {
    if (g->sim.playerType[n] > -1) {
      // initializePlayers(n, false): buildPlayerObject + drawVfx
      // {name:"entrance", pos:new Vec2D(startingPoint[i][0],
      //  startingPoint[i][1])} (main.js:1313-1316; M4 task 1 — full
      // config; not circleDust, no draws). Boot-time: no capture spec
      // covers main.js — structure-verified only (AGENT-LOG iter 64
      // honest-coverage note); consumed via ml_vfx_sink by the renderer.
      sim_build_player(g, n);
      ml_drawVfx_p("entrance", kStartingPoint[n][0], kStartingPoint[n][1]);
      // renderPlayer(n): render plane. Its outOfCameraTimer/miniView
      // writes are =0/=false no-ops at spawn (on-screen); the oracle's
      // own renderTick is OFF (__harnessNoRender) — see AGENT-LOG task-17
      // note on the outOfCameraTimer surface.
      g->sim.player[n].inCSS = false;
    }
    // main.js:1334-1336. NOTE the arm sits OUTSIDE the playerType guard
    // above, in the SAME loop — so in the endless mode upstream writes
    // stocks = 1 on ALL FOUR slots, including the inactive ones whose
    // page-boot player objects initializePlayers never touched. Carried
    // verbatim (HARD RULE 5); the inactive slots' stocks are unread, but
    // the write is upstream's.
    if (g->sim.versusMode != 0) { // `if (versusMode)`; domain is 0|1
      g->sim.player[n].stocks = 1;
    }
  }
  g->matchTimer = 480;
  g->startTimer = 1.5;
  g->starting = true;
  // MusicManager: audio plane, no seeded draws. drawVfx({name:"start",
  // pos:new Vec2D(0, 0)}) (main.js:1364-1367; M4 task 1 — same
  // boot-time coverage note as "entrance" above).
  ml_drawVfx_p("start", 0, 0);
  g->inp.playing = true; // playing = true (findingPlayers = false)
  // gameSettings (settings.js:44-56 defaults; header note)
  g->sim.lCancelType = 0;
  g->sim.turbo = false;
  g->sim.phantomThreshold = 0.01;
  // everyCharWallJump: upstream default 0 (settings.js:51). Written
  // EXPLICITLY rather than left to the caller's memset because MENU-SPEC
  // DEVIATION D20 gives this flag real mechanical effect — every frozen
  // golden depends on it being false here, so the default is stated, not
  // inherited.
  g->sim.everyCharWallJump = false;
  for (int i = 0; i < 4; i++) g->sim.tapJumpOff[i] = 0;
  // module queues start empty
  hd_resetHitQueue(&g->hq);
  g->hq.phqCount = 0;
  memset(&g->arts, 0, sizeof g->arts);
  // rule-17 live charHitboxes plane: pristine at match start
  sim_chd_reset();
  g->frame = 0;
}
