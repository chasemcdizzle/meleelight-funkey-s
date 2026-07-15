// SIDESPECIALGROUND.c <- src/characters/puff/moves/SIDESPECIALGROUND.js (M2 task 12)
// Pound: groundVelocities/airVelocities from the mvData puff dump (rule
// 15); the air arc rotates airVelocities by phys.upbAngleMultiplier =
// lsY * PI * (20/180) via fdlibm sin/cos.
#include "../moves.h"

static AsTri pf_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex);
static AsTri pf_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex);

static AsTri pf_init(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "SIDESPECIALGROUND");
  pl->timer = 0;
  if (pl->phys.grounded) {
    pl->phys.cVel.x = 0;
  } else {
    if (pl->phys.cVel.y < -ml_f64(mv_attr(MV_CS(S, p))->terminalV)) {
      pl->phys.cVel.y = -ml_f64(mv_attr(MV_CS(S, p))->terminalV);
    }
  }
  mv_turnOffHitboxes(S, p);
  pf_assign_hitbox_id(S, p, "sidespecial", 0, 0);
  pf_assign_hitbox_id(S, p, "sidespecial", 1, 1);
  pf_main(S, p, in, 0);
  return AS_UNDEF;
}

static AsTri pf_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  const MlInput *i0 = &MV_IN(in, p)[0];
  pl->timer += 1;
  if (pf_interrupt(S, p, in, 0) != AS_TRUE) {
    if (pl->phys.grounded) {
      if (pl->timer > 11) {
        pl->phys.cVel.x = mv_puff_arr("SIDESPECIALGROUND", "groundVelocities",
                                      pl->timer - 12) * pl->phys.face;
      }
    } else {
      if (pl->timer == 12) {
        pl->phys.fastfalled = false;
        pl->phys.upbAngleMultiplier = i0->lsY * js_pi() * (20.0 / 180.0);
        //decide angle / max 20 degrees (upstream comment)
        pl->phys.cVel.y = 0;
      }
      if (pl->timer < 12) {
        const double airFriction =
            ml_f64(mv_attr(MV_CS(S, p))->airFriction);
        if (pl->phys.cVel.x > 0) {
          pl->phys.cVel.x -= airFriction;
          if (pl->phys.cVel.x < 0) {
            pl->phys.cVel.x = 0;
          }
        } else if (pl->phys.cVel.x < 0) {
          pl->phys.cVel.x += airFriction;
          if (pl->phys.cVel.x > 0) {
            pl->phys.cVel.x = 0;
          }
        }
        pl->phys.cVel.y -= ml_f64(mv_attr(MV_CS(S, p))->gravity);
        if (pl->phys.cVel.y < -ml_f64(mv_attr(MV_CS(S, p))->terminalV)) {
          pl->phys.cVel.y = -ml_f64(mv_attr(MV_CS(S, p))->terminalV);
        }
      } else if (pl->timer > 11 && pl->timer < 40) {
        pl->phys.cVel.x = mv_puff_arr("SIDESPECIALGROUND", "airVelocities",
                                      pl->timer - 12) *
                          pl->phys.face * fd_cos(pl->phys.upbAngleMultiplier);
        pl->phys.cVel.y = mv_puff_arr("SIDESPECIALGROUND", "airVelocities",
                                      pl->timer - 12) *
                          fd_sin(pl->phys.upbAngleMultiplier);
      } else {
        as_airDrift((int)MV_CS(S, p), &pl->phys.cVel.x, MV_IN(in, p));
        as_fastfall((int)MV_CS(S, p), &pl->phys.cVel.y,
                    &pl->phys.fastfalled, MV_IN(in, p));
      }
    }
    if (pl->timer == 12) {
      pl->hitboxes.active[0] = true;
      pl->hitboxes.active[1] = true;
      pl->hitboxes.active[2] = false;
      pl->hitboxes.active[3] = false;
      S->aliasHbActive[(int)p] = false; // fresh array upstream
      pl->hitboxes.frame = 0;
      ml_sound_play("puffshout1");
    }
    if (pl->timer > 12 && pl->timer < 28) {
      pl->hitboxes.frame += 1;
    }
    if (pl->timer == 28) {
      mv_turnOffHitboxes(S, p);
    }
  }
  return AS_UNDEF;
}

static AsTri pf_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  if (pl->timer > 45) {
    if (pl->phys.grounded) {
      mv_WAIT.init(S, p, in, 0);
    } else {
      mv_FALL.init(S, p, in, 0);
    }
    return AS_TRUE;
  } else {
    return AS_FALSE;
  }
}

const MlMoveDef puff_SIDESPECIALGROUND = {
    "SIDESPECIALGROUND", pf_init, pf_main, pf_interrupt, 0};
