// UPSPECIALLAUNCH.c <- src/characters/falco/moves/UPSPECIALLAUNCH.js
// (M2 task 9)
#include "../moves.h"

static AsTri fc_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex);
static AsTri fc_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex);

static AsTri fc_init(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "UPSPECIALLAUNCH");
  pl->timer = 0;
  ml_sound_play("firebirdlaunch");
  ml_sound_play("falcofirebird");
  mv_assign_hitbox_id(S, p, "upspecial", 0, 0);
  pl->hitboxes.active[0] = true;
  pl->hitboxes.active[1] = false;
  pl->hitboxes.active[2] = false;
  pl->hitboxes.active[3] = false;
  S->aliasHbActive[(int)p] = false;
  pl->hitboxes.frame = 0;
  pl->rotation = js_pi() / 2 - pl->phys.upbAngleMultiplier;
  if (pl->phys.upbAngleMultiplier != js_pi() / 2) {
    if (js_abs(pl->phys.upbAngleMultiplier) > js_pi() / 2) {
      pl->phys.face = -1;
    } else {
      pl->phys.face = 1;
    }
  }
  pl->rotationPoint = vec2d(0, 40);
  fc_main(S, p, in, 0);
  return AS_UNDEF;
}

static AsTri fc_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  const ml_attributes_t *at = mv_attr(MV_CS(S, p));
  pl->timer += 1;
  if (fc_interrupt(S, p, in, 0) != AS_TRUE) {
    if (pl->timer < 23) {
      if (fmod(pl->timer, 2) != 0) { // JS truthiness of timer%2
        ml_drawVfx("firefoxtail", pl->phys.posPrev.x, pl->phys.posPrev.y,
                   pl->phys.face);
      }
      ml_drawVfx_f("firefoxlaunch", pl->phys.pos.x, pl->phys.pos.y,
                   pl->phys.face, p);
    }
    if (pl->phys.grounded) {
      as_reduceByTraction(false, (int)MV_CS(S, p), &pl->phys.cVel.x);
    } else {
      if (pl->phys.cVel.x > 0) {
        pl->phys.cVel.x -= ml_f64(at->airFriction);
        if (pl->phys.cVel.x < 0) {
          pl->phys.cVel.x = 0;
        }
      } else if (pl->phys.cVel.x < 0) {
        pl->phys.cVel.x += ml_f64(at->airFriction);
        if (pl->phys.cVel.x > 0) {
          pl->phys.cVel.x = 0;
        }
      }
    }
    if (pl->timer >= 23) {
      if (pl->phys.grounded) {
        as_reduceByTraction(false, (int)MV_CS(S, p), &pl->phys.cVel.x);
      } else {
        as_fastfall((int)MV_CS(S, p), &pl->phys.cVel.y, &pl->phys.fastfalled,
                    MV_IN(in, p));
        as_airDrift((int)MV_CS(S, p), &pl->phys.cVel.x, MV_IN(in, p));
      }
    } else if (pl->timer >= 4) {
      pl->phys.cVel.y -= 0.17 * fd_sin(pl->phys.upbAngleMultiplier);
      pl->phys.cVel.x -= 0.17 * fd_cos(pl->phys.upbAngleMultiplier);
    } else if (pl->timer >= 1) {
      pl->phys.grounded = false;
      pl->phys.cVel.y = 4.2 * fd_sin(pl->phys.upbAngleMultiplier);
      pl->phys.cVel.x = 4.2 * fd_cos(pl->phys.upbAngleMultiplier);
    }
    if (pl->timer > 1 && pl->timer < 23) {
      pl->hitboxes.frame += 1;
    } else if (pl->timer == 23) {
      mv_turnOffHitboxes(S, p);
      pl->rotation = 0;
      pl->rotationPoint = vec2d(0, 0);
    }
  }
  return AS_UNDEF;
}

static AsTri fc_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  if (pl->timer > 42) {
    if (pl->phys.grounded) {
      mv_WAIT.init(S, p, in, 0);
    } else {
      mv_FALLSPECIAL.init(S, p, in, 0);
    }
    return AS_TRUE;
  } else {
    return AS_FALSE;
  }
}

static AsTri fc_land(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  if (pl->timer < 23) {
    // BOUNCE
    ml_drawVfx("groundBounce", pl->phys.pos.x, pl->phys.pos.y,
               pl->phys.face);
    falco_FIREFOXBOUNCE.init(S, p, in, 0);
  } else {
    ml_drawVfx("impactLand", pl->phys.pos.x, pl->phys.pos.y,
               pl->phys.face);
  }
  return AS_UNDEF;
}

const MlMoveDef falco_UPSPECIALLAUNCH = {"UPSPECIALLAUNCH", fc_init, fc_main,
                                       fc_interrupt, fc_land};
