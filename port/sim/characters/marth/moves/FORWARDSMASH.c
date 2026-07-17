// FORWARDSMASH.c <- src/characters/marth/moves/FORWARDSMASH.js (M2 task 11)
#include "../moves.h"

static AsTri mr_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex);
static AsTri mr_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex);

static AsTri mr_init(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "FORWARDSMASH");
  pl->timer = 0;
  pl->phys.charging = false;
  pl->phys.chargeFrames = 0;
  mv_turnOffHitboxes(S, p);
  mv_assign_hitbox_id(S, p, "fsmash", 0, 0);
  mv_assign_hitbox_id(S, p, "fsmash", 1, 1);
  mv_assign_hitbox_id(S, p, "fsmash", 2, 2);
  mv_assign_hitbox_id(S, p, "fsmash", 3, 3);
  mr_main(S, p, in, 0);
  return AS_UNDEF;
}

static AsTri mr_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  const MlInput *i0 = &MV_IN(in, p)[0];
  if (pl->timer == 3) {
    if (i0->a || i0->z) {
      pl->phys.charging = true;
      pl->phys.chargeFrames += 1;
      if (pl->phys.chargeFrames == 5) {
        ml_sound_play("smashcharge");
      }
      if (pl->phys.chargeFrames == 60) {
        pl->timer += 1;
        pl->phys.charging = false;
      }
    } else {
      pl->timer += 1;
      pl->phys.charging = false;
    }
  } else {
    pl->timer += 1;
    pl->phys.charging = false;
  }
  if (mr_interrupt(S, p, in, 0) != AS_TRUE) {
    as_reduceByTraction(true, (int)MV_CS(S, p), &pl->phys.cVel.x);
    if (pl->timer == 5) {
      ml_sound_play("sword3");
    }
    if (pl->timer > 5 && pl->timer < 14) {
      ml_drawVfx_swing("swing", 0, 0, pl->phys.face, p, "FORWARDSMASH", pl->timer - 6);
    }
    if (pl->timer == 10) {
      pl->hitboxes.active[0] = true;
      pl->hitboxes.active[1] = true;
      pl->hitboxes.active[2] = true;
      pl->hitboxes.active[3] = true;
      S->aliasHbActive[(int)p] = false;
      pl->hitboxes.frame = 0;
      as_randomShout(MV_CS(S, p));
    }
    if (pl->timer > 10 && pl->timer < 14) {
      pl->hitboxes.frame += 1;
    }
    if (pl->timer == 14) {
      mv_turnOffHitboxes(S, p);
    }
  }
  return AS_UNDEF;
}

static AsTri mr_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  const MlInput *i0 = &MV_IN(in, p)[0];
  if (pl->timer > 49) {
    mv_WAIT.init(S, p, in, 0);
    return AS_TRUE;
  } else if (pl->timer > 47 && !pl->inCSS) {
    const AsPair b = as_checkForSpecials(&pl->phys.face,
                                         pl->phys.bTurnaroundTimer,
                                         pl->phys.bTurnaroundDirection,
                                         pl->phys.grounded, MV_IN(in, p));
    const AsPair t = as_checkForTilts(pl->phys.face, MV_IN(in, p), false, 0);
    const AsPair s = as_checkForSmashes(&pl->phys.face, MV_IN(in, p));
    const AsPair j = as_checkForJump(S->tapJumpOff[(int)p], MV_IN(in, p));
    if (j.flag) {
      MvX x = mvx_pair_payload(&j);
      mv_KNEEBEND.init(S, p, in, &x);
      return AS_TRUE;
    } else if (b.flag) {
      marth_moves_init(S, mv_pair_str(&b), p, in);
      return AS_TRUE;
    } else if (s.flag) {
      marth_moves_init(S, mv_pair_str(&s), p, in);
      return AS_TRUE;
    } else if (t.flag) {
      marth_moves_init(S, mv_pair_str(&t), p, in);
      return AS_TRUE;
    } else if (as_checkForDash(pl->phys.face, MV_IN(in, p))) {
      mv_DASH.init(S, p, in, 0);
      return AS_TRUE;
    } else if (as_checkForSmashTurn(pl->phys.face, MV_IN(in, p))) {
      mv_SMASHTURN.init(S, p, in, 0);
      return AS_TRUE;
    } else if (as_checkForTiltTurn(pl->phys.face, MV_IN(in, p))) {
      pl->phys.dashbuffer = as_tiltTurnDashBuffer(pl->phys.face,
                                                  MV_IN(in, p));
      mv_TILTTURN.init(S, p, in, 0);
      return AS_TRUE;
    } else if (js_abs(i0->lsX) > 0.3) {
      MvX x = mvx_bool(true);
      mv_WALK.init(S, p, in, &x);
      return AS_TRUE;
    } else {
      return AS_FALSE;
    }
  } else {
    return AS_FALSE;
  }
}

const MlMoveDef marth_FORWARDSMASH = {"FORWARDSMASH", mr_init, mr_main,
                                      mr_interrupt, 0};
