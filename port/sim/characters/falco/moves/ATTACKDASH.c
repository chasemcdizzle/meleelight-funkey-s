// ATTACKDASH.c <- src/characters/falco/moves/ATTACKDASH.js (M2 task 9)
// this.setVelocities comes from the mvData falco dump (rule 15).
#include "../moves.h"

static AsTri fc_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex);
static AsTri fc_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex);

static AsTri fc_init(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "ATTACKDASH");
  pl->timer = 0;
  mv_turnOffHitboxes(S, p);
  mv_assign_hitbox_id(S, p, "dashattack1", 0, 0);
  fc_main(S, p, in, 0);
  return AS_UNDEF;
}

static AsTri fc_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  pl->timer += 1;
  if (fc_interrupt(S, p, in, 0) != AS_TRUE) {
    pl->phys.cVel.x =
        mv_falco_arr("ATTACKDASH", "setVelocities", pl->timer - 1) *
        pl->phys.face;

    if (pl->timer == 4) {
      pl->hitboxes.active[0] = true;
      pl->hitboxes.active[1] = false;
      pl->hitboxes.active[2] = false;
      pl->hitboxes.active[3] = false;
      S->aliasHbActive[(int)p] = false;
      pl->hitboxes.frame = 0;
      ml_sound_play("normalswing2");
    }
    if (pl->timer > 4 && pl->timer < 18) {
      pl->hitboxes.frame += 1;
    }
    if (pl->timer == 8) {
      mv_assign_hitbox_id(S, p, "dashattack2", 0, 0);
      pl->hitboxes.frame = 0;
    }
    if (pl->timer == 18) {
      mv_turnOffHitboxes(S, p);
    }
  }
  return AS_UNDEF;
}

static AsTri fc_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  const ml_attributes_t *at = mv_attr(MV_CS(S, p));
  const MlInput *i0 = &MV_IN(in, p)[0];
  if (pl->timer > 39) {
    mv_WAIT.init(S, p, in, 0);
    return AS_TRUE;
  } else if (pl->timer < 5 && (i0->lA > 0 || i0->rA > 0)) {
    if (pl->phys.cVel.x * pl->phys.face > ml_f64(at->dMaxV)) {
      pl->phys.cVel.x = ml_f64(at->dMaxV) * pl->phys.face;
    }
    falco_GRAB.init(S, p, in, 0);
    return AS_TRUE;
  } else if (pl->timer > 35) {
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
      falco_moves_init(S, mv_pair_str(&b), p, in);
      return AS_TRUE;
    } else if (s.flag) {
      falco_moves_init(S, mv_pair_str(&s), p, in);
      return AS_TRUE;
    } else if (t.flag) {
      falco_moves_init(S, mv_pair_str(&t), p, in);
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

const MlMoveDef falco_ATTACKDASH = {"ATTACKDASH", fc_init, fc_main,
                                  fc_interrupt, 0};
