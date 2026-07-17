// UPSPECIAL.c <- src/characters/marth/moves/UPSPECIAL.js (M2 task 11 —
// dolphin slash). setVelocities is a PAIR array (mvData marth dump);
// the 6..22 window rotates it by phys.upbAngleMultiplier via fdlibm
// sin/cos (rule 4). land is the falcon 3-disjunct guard shape.
#include "../moves.h"

static AsTri mr_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex);
static AsTri mr_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex);

static AsTri mr_init(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "UPSPECIAL");
  pl->timer = 0;
  pl->phys.cVel = vec2d(0, 0);
  pl->phys.fastfalled = false;
  pl->phys.upbAngleMultiplier = 0;
  mv_turnOffHitboxes(S, p);
  mv_assign_hitbox_id(S, p, "upb1", 0, 0);
  mv_assign_hitbox_id(S, p, "upb1", 1, 1);
  mv_assign_hitbox_id(S, p, "upb1", 2, 2);
  pl->phys.landingMultiplier = 30.0 / 34;
  ml_sound_play("dolphinSlash");
  ml_sound_play("dolphinSlash2");
  mr_main(S, p, in, 0);
  return AS_UNDEF;
}

static AsTri mr_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  const MlInput *i0 = &MV_IN(in, p)[0];
  pl->timer += 1;
  if (mr_interrupt(S, p, in, 0) != AS_TRUE) {
    if (pl->phys.cVel.y <= 0) {
      pl->phys.canWallJump.isUndef = false;
      pl->phys.canWallJump.v = true;
    }
    if (pl->timer < 6) {
      if (js_abs(i0->lsX) > 0.7) {
        pl->phys.upbAngleMultiplier = -i0->lsX * js_pi() / 16;
      }
    }
    if (pl->timer == 6) {
      pl->phys.grounded = false;
      if (i0->lsX * pl->phys.face < -0.28) {
        pl->phys.face *= -1;
      }
    }
    if (pl->timer > 5 && pl->timer < 23) {
      Vec2D sv;
      if (!mv_marth_pair("UPSPECIAL", "setVelocities", pl->timer - 6, &sv)) {
        mv_out_of_domain("UPSPECIAL: setVelocities index out of range");
      }
      pl->phys.cVel =
          vec2d(sv.x * pl->phys.face *
                        fd_cos(pl->phys.upbAngleMultiplier) -
                    sv.y * fd_sin(pl->phys.upbAngleMultiplier),
                sv.x * pl->phys.face *
                        fd_sin(pl->phys.upbAngleMultiplier) +
                    sv.y * fd_cos(pl->phys.upbAngleMultiplier));
    } else if (pl->timer > 22) {
      as_fastfall((int)MV_CS(S, p), &pl->phys.cVel.y, &pl->phys.fastfalled,
                  MV_IN(in, p));
      as_airDrift((int)MV_CS(S, p), &pl->phys.cVel.x, MV_IN(in, p));
      if (js_abs(pl->phys.cVel.x) > 0.36) {
        pl->phys.cVel.x = 0.36 * js_sign(pl->phys.cVel.x);
      }
    }

    if (pl->timer == 5) {
      pl->hitboxes.active[0] = true;
      pl->hitboxes.active[1] = true;
      pl->hitboxes.active[2] = true;
      pl->hitboxes.active[3] = false;
      S->aliasHbActive[(int)p] = false;
      pl->hitboxes.frame = 0;
    }
    if (pl->timer == 6) {
      mv_assign_hitbox_id(S, p, "upb2", 0, 0);
      mv_assign_hitbox_id(S, p, "upb2", 1, 1);
      mv_assign_hitbox_id(S, p, "upb2", 2, 2);
    }
    if (pl->timer > 6 && pl->timer < 11) {
      pl->hitboxes.frame += 1;
    }
    if (pl->timer == 11) {
      mv_turnOffHitboxes(S, p);
    }
    if (pl->timer > 2 && pl->timer < 12) {
      ml_drawVfx_swing("swing", 0, 0, pl->phys.face, p, "UPSPECIAL", pl->timer - 3);
    }
  }
  return AS_UNDEF;
}

static AsTri mr_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  if (pl->timer > 39) {
    mv_FALLSPECIAL.init(S, p, in, 0);
    return AS_TRUE;
  } else {
    return AS_FALSE;
  }
}

static AsTri mr_land(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  if (pl->phys.cVel.y + pl->phys.kVel.y <= 0 ||
      pl->phys.ECBp[0].y <= pl->phys.ECB1[0].y ||
      pl->phys.pos.y <= pl->phys.posPrev.y) {
    mv_LANDINGFALLSPECIAL.init(S, p, in, 0);
  }
  return AS_UNDEF;
}

const MlMoveDef marth_UPSPECIAL = {"UPSPECIAL", mr_init, mr_main,
                                   mr_interrupt, mr_land};
