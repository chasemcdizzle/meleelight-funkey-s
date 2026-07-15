// UPSPECIAL.c <- src/characters/falcon/moves/UPSPECIAL.js (M2 task 10)
// Falcon dive. setVelocities is a PAIR array (mvData falcon dump); the
// drift block is rule-13 compound-assignment territory: `cVel.x -=
// setVel[timer-2][0]*face` groups its whole RHS, then `+= lsX*0.044`,
// clamp arms, then the [timer-1] pair lands. The grabbing!==-1 interrupt
// arm writes pos COMPONENTS (the rule-10 pos-ECB1[0] write-through) and
// chains UPSPECIALCATCH. land's guard reads cVel+kVel/ECBp/ECB1/posPrev —
// undef ECB components hold canonical NaN, so the JS undefined<=undefined
// false arm falls out of NaN comparison; when the guard is all-false NO
// state change happens (bare land).
#include "../moves.h"

static AsTri fc4_main(MlSim *S, double p, const MlInputBuffer in[4],
                      const MvX *ex);
static AsTri fc4_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                           const MvX *ex);

static AsTri fc4_init(MlSim *S, double p, const MlInputBuffer in[4],
                      const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "UPSPECIAL");
  pl->timer = 0;
  // cVel = new Vec2D(0, 0)
  pl->phys.cVel = vec2d(0, 0);
  pl->phys.fastfalled = false;
  pl->phys.upbAngleMultiplier = 0;
  mv_turnOffHitboxes(S, p);
  pl->phys.landingMultiplier = 30.0 / 34;
  mv_assign_hitbox_id(S, p, "falcondive1", 0, 0);
  mv_assign_hitbox_id(S, p, "falcondive1", 1, 1);
  mv_drawVfx("groundBounce");
  fc4_main(S, p, in, 0);
  return AS_UNDEF;
}

static AsTri fc4_main(MlSim *S, double p, const MlInputBuffer in[4],
                      const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  const MlInput *i0 = &MV_IN(in, p)[0];
  pl->timer += 1;
  if (fc4_interrupt(S, p, in, 0) != AS_TRUE) {
    if (pl->timer == 13) {
      pl->phys.grounded = false;
      if (i0->lsX * pl->phys.face < -0.28) {
        pl->phys.face *= -1;
      }
    }
    if (pl->timer > 1) {
      Vec2D sv2;
      if (!mv_falcon_pair("UPSPECIAL", "setVelocities", pl->timer - 2,
                          &sv2)) {
        mv_out_of_domain("UPSPECIAL: setVelocities[timer-2] out of range");
      }
      pl->phys.cVel.x = pl->phys.cVel.x - (sv2.x * pl->phys.face);
    }
    pl->phys.cVel.x = pl->phys.cVel.x + (i0->lsX * 0.044);
    if (js_abs(i0->lsX) < 0.28) {
      pl->phys.cVel.x = 0;
    }
    if (pl->phys.cVel.x < -0.952) {
      pl->phys.cVel.x = -0.952;
    }
    if (pl->phys.cVel.x > 0.952) {
      pl->phys.cVel.x = 0.952;
    }
    Vec2D sv1;
    if (!mv_falcon_pair("UPSPECIAL", "setVelocities", pl->timer - 1, &sv1)) {
      mv_out_of_domain("UPSPECIAL: setVelocities[timer-1] out of range");
    }
    pl->phys.cVel.x = pl->phys.cVel.x + (sv1.x * pl->phys.face);
    pl->phys.cVel.y = sv1.y;
    if (pl->timer == 13) {
      pl->hitboxes.active[0] = true;
      pl->hitboxes.active[1] = true;
      pl->hitboxes.active[2] = false;
      pl->hitboxes.active[3] = false;
      S->aliasHbActive[(int)p] = false; // fresh array upstream
      pl->hitboxes.frame = 0;
    }
    if (pl->timer > 13 && pl->timer < 34) {
      pl->hitboxes.frame += 1;
    }
    if (pl->timer == 14) {
      mv_assign_hitbox_id(S, p, "falcondive2", 0, 0);
      pl->hitboxes.frame = 0;
      pl->hitboxes.active[0] = true;
      pl->hitboxes.active[1] = false;
      pl->hitboxes.active[2] = false;
      pl->hitboxes.active[3] = false;
      S->aliasHbActive[(int)p] = false; // fresh array upstream
      ml_sound_play("falcondive");
    }
    if (pl->timer == 34) {
      mv_turnOffHitboxes(S, p);
    }
  }
  return AS_UNDEF;
}

static AsTri fc4_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                           const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  if (pl->timer > 64) {
    mv_FALLSPECIAL.init(S, p, in, 0);
    return AS_TRUE;
  } else if (pl->phys.grabbing != -1) {
    // pos COMPONENT writes (rule-10 pos-ECB1[0] write-through)
    mv_pos_set_x(S, p, mv_player(S, pl->phys.grabbing)->phys.pos.x);
    mv_pos_set_y(S, p, mv_player(S, pl->phys.grabbing)->phys.pos.y);
    falcon_UPSPECIALCATCH.init(S, p, in, 0);
    return AS_TRUE;
  } else {
    return AS_FALSE;
  }
}

static AsTri fc4_land(MlSim *S, double p, const MlInputBuffer in[4],
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

const MlMoveDef falcon_UPSPECIAL = {"UPSPECIAL", fc4_init, fc4_main,
                                    fc4_interrupt, fc4_land};
