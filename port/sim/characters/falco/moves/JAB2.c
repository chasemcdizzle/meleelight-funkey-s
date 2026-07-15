// JAB2.c <- src/characters/falco/moves/JAB2.js (M2 task 9)
#include "../moves.h"

static AsTri fc_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex);
static AsTri fc_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex);

static AsTri fc_init(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "JAB2");
  pl->timer = 0;
  pl->phys.jabCombo = false;
  mv_turnOffHitboxes(S, p);
  mv_assign_hitbox_id(S, p, "jab2", 0, 0);
  mv_assign_hitbox_id(S, p, "jab2", 1, 1);
  fc_main(S, p, in, 0);
  return AS_UNDEF;
}

static AsTri fc_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  const MlInput *i0 = &MV_IN(in, p)[0];
  const MlInput *i1 = &MV_IN(in, p)[1];
  pl->timer += 1;
  if (fc_interrupt(S, p, in, 0) != AS_TRUE) {
    if (pl->timer == 1) {
      pl->phys.cVel.x = 0;
    } else if (pl->timer == 2) {
      pl->phys.cVel.x = 3.85 * pl->phys.face;
    } else if (pl->timer == 4) {
      pl->phys.cVel.x = 0;
    }
    if (pl->timer > 0 && pl->timer < 21 && i0->a && !i1->a) {
      pl->phys.jabCombo = true;
    }
    if (pl->timer == 3) {
      pl->hitboxes.active[0] = true;
      pl->hitboxes.active[1] = true;
      pl->hitboxes.active[2] = false;
      pl->hitboxes.active[3] = false;
      S->aliasHbActive[(int)p] = false;
      pl->hitboxes.frame = 0;
      ml_sound_play("normalswing2");
    }
    if (pl->timer > 3 && pl->timer < 5) {
      pl->hitboxes.frame += 1;
    }
    if (pl->timer == 5) {
      mv_turnOffHitboxes(S, p);
    }
  }
  return AS_UNDEF;
}

static AsTri fc_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  const MlInput *i0 = &MV_IN(in, p)[0];
  if (pl->timer > 6 && pl->phys.jabCombo) {
    falco_JAB3.init(S, p, in, 0);
    return AS_TRUE;
  } else if (pl->timer > 20) {
    mv_WAIT.init(S, p, in, 0);
    return AS_TRUE;
  } else if (pl->timer > 18) {
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

const MlMoveDef falco_JAB2 = {"JAB2", fc_init, fc_main, fc_interrupt, 0};
