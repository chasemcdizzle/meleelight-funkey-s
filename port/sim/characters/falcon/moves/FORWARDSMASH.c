// FORWARDSMASH.c <- src/characters/falcon/moves/FORWARDSMASH.js
// (M2 task 10). setVelocities comes from the mvData falcon dump; the
// non-charging arm reads it EVERY un-interrupted tick. The firefoxtail
// window (18..21) reads id[0].offset[frame] for its render-only position
// (mv_falcon_hb0_off — crash-fidelity read). Tail arms are falcon's
// explicit lsX/lsY TILTTURN/WALK forms.
#include "../moves.h"

static AsTri fc4_main(MlSim *S, double p, const MlInputBuffer in[4],
                      const MvX *ex);
static AsTri fc4_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                           const MvX *ex);

static AsTri fc4_init(MlSim *S, double p, const MlInputBuffer in[4],
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
  fc4_main(S, p, in, 0);
  return AS_UNDEF;
}

static AsTri fc4_main(MlSim *S, double p, const MlInputBuffer in[4],
                      const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  const MlInput *i0 = &MV_IN(in, p)[0];
  if (pl->timer == 10) {
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
  if (fc4_interrupt(S, p, in, 0) != AS_TRUE) {
    if (pl->phys.charging) {
      pl->phys.cVel.x = 0;
    } else {
      pl->phys.cVel.x = mv_falcon_arr("FORWARDSMASH", "setVelocities",
                                      pl->timer - 1) * pl->phys.face;
    }

    if (pl->timer == 18) {
      pl->hitboxes.active[0] = true;
      pl->hitboxes.active[1] = true;
      pl->hitboxes.active[2] = false;
      pl->hitboxes.active[3] = false;
      S->aliasHbActive[(int)p] = false; // fresh array upstream
      pl->hitboxes.frame = 0;
      as_randomShout(MV_CS(S, p));
      ml_sound_play("fireweakhit");
    }
    if (pl->timer > 18 && pl->timer < 22) {
      pl->hitboxes.frame += 1;
    }
    if (pl->timer == 22) {
      mv_turnOffHitboxes(S, p);
    }
    if (pl->timer >= 18 && pl->timer < 22) {
      const Vec2D off = mv_falcon_hb0_off(pl, "FORWARDSMASH: vfx offset");
      (void)off; // position is render-only; the vfx queue keeps names
      mv_drawVfx("firefoxtail");
    }
  }
  return AS_UNDEF;
}

static AsTri fc4_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                           const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  const MlInput *i0 = &MV_IN(in, p)[0];
  if (pl->timer > 64) {
    mv_WAIT.init(S, p, in, 0);
    return AS_TRUE;
  } else if (pl->timer > 59) {
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
      falcon_moves_init(S, mv_pair_str(&b), p, in);
      return AS_TRUE;
    } else if (s.flag) {
      falcon_moves_init(S, mv_pair_str(&s), p, in);
      return AS_TRUE;
    } else if (t.flag) {
      falcon_moves_init(S, mv_pair_str(&t), p, in);
      return AS_TRUE;
    } else if (as_checkForDash(pl->phys.face, MV_IN(in, p))) {
      mv_DASH.init(S, p, in, 0);
      return AS_TRUE;
    } else if (as_checkForSmashTurn(pl->phys.face, MV_IN(in, p))) {
      mv_SMASHTURN.init(S, p, in, 0);
      return AS_TRUE;
    } else if (i0->lsX * pl->phys.face < -0.3 &&
               js_abs(i0->lsX) > i0->lsY * -1) {
      pl->phys.dashbuffer = as_tiltTurnDashBuffer(pl->phys.face,
                                                  MV_IN(in, p));
      mv_TILTTURN.init(S, p, in, 0);
      return AS_TRUE;
    } else if (i0->lsX * pl->phys.face > 0.3 &&
               js_abs(i0->lsX) > i0->lsY * -1) {
      MvX x = mvx_bool(true);
      mv_WALK.init(S, p, in, &x);
      return AS_TRUE;
    } else {
      return AS_FALSE;
    }
    // iasa 60
  } else {
    return AS_FALSE;
  }
}

const MlMoveDef falcon_FORWARDSMASH = {"FORWARDSMASH", fc4_init, fc4_main,
                                       fc4_interrupt, 0};
