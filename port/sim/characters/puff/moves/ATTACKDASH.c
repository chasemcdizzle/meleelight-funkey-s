// ATTACKDASH.c <- src/characters/puff/moves/ATTACKDASH.js (M2 task 12)
// setVelocities from the mvData puff dump; interrupt carries the lA/rA GRAB arm with the dMaxV clamp.
#include "../moves.h"

static AsTri pf_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex);
static AsTri pf_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex);

static AsTri pf_init(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "ATTACKDASH");
  pl->timer = 0;
  mv_turnOffHitboxes(S, p);
  pf_assign_hitbox_id(S, p, "dashattack1", 0, 0);
  pf_main(S, p, in, 0);
  return AS_UNDEF;
}

static AsTri pf_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  pl->timer += 1;
  if (pf_interrupt(S, p, in, 0) != AS_TRUE) {
    pl->phys.cVel.x = mv_puff_arr("ATTACKDASH", "setVelocities",
                                  pl->timer - 1) * pl->phys.face;
    if (pl->timer == 4) {
      pl->hitboxes.active[0] = true;
      pl->hitboxes.active[1] = false;
      pl->hitboxes.active[2] = false;
      pl->hitboxes.active[3] = false;
      S->aliasHbActive[(int)p] = false; // fresh array upstream
      pl->hitboxes.frame = 0;
      ml_sound_play("normalswing1");
    }
    if (pl->timer > 4 && pl->timer < 15) {
      pl->hitboxes.frame += 1;
    }
    if (pl->timer == 9) {
      pf_assign_hitbox_id(S, p, "dashattack2", 0, 0);
      pl->hitboxes.frame = 0;
    }
    if (pl->timer == 15) {
      mv_turnOffHitboxes(S, p);
    }
  }
  return AS_UNDEF;
}

static AsTri pf_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  const MlInput *i0 = &MV_IN(in, p)[0];
  if (pl->timer > 39) {
    mv_WAIT.init(S, p, in, 0);
    return AS_TRUE;
  } else if (pl->timer < 5 && (i0->lA > 0 || i0->rA > 0)) {
    const double dMaxV = ml_f64(mv_attr(MV_CS(S, p))->dMaxV);
    if (pl->phys.cVel.x * pl->phys.face > dMaxV) {
      pl->phys.cVel.x = dMaxV * pl->phys.face;
    }
    puff_GRAB.init(S, p, in, 0);
    return AS_TRUE;
  } else if (pl->timer > 38) {
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
      puff_moves_init(S, mv_pair_str(&b), p, in);
      return AS_TRUE;
    } else if (s.flag) {
      puff_moves_init(S, mv_pair_str(&s), p, in);
      return AS_TRUE;
    } else if (t.flag) {
      puff_moves_init(S, mv_pair_str(&t), p, in);
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

const MlMoveDef puff_ATTACKDASH = {"ATTACKDASH", pf_init, pf_main, pf_interrupt, 0};
