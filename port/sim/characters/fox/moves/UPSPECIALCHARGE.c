// UPSPECIALCHARGE.c <- src/characters/fox/moves/UPSPECIALCHARGE.js
// (M2 task 8). NOTE the interrupt's timer>42 arm dispatches
// UPSPECIALLAUNCH.init and FALLS THROUGH without a return (undefined —
// rule 13's control-flow-fallthrough family): the caller's
// `!this.interrupt(...)` is then true and main's body still runs.
#include "../moves.h"

static AsTri fx_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex);
static AsTri fx_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex);

static AsTri fx_init(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "UPSPECIALCHARGE");
  pl->timer = 0;
  pl->phys.cVel.x *= 0.8;
  pl->phys.cVel.y = 0;
  pl->phys.fastfalled = false;
  pl->phys.landingMultiplier = 10;
  ml_sound_play("foxupbburn");
  mv_turnOffHitboxes(S, p);
  mv_assign_hitbox_id(S, p, "upb1", 0, 0);
  fx_main(S, p, in, 0);
  return AS_UNDEF;
}

static AsTri fx_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  const ml_attributes_t *at = mv_attr(MV_CS(S, p));
  const MlInput *i0 = &MV_IN(in, p)[0];
  pl->timer += 1;
  if (fx_interrupt(S, p, in, 0) != AS_TRUE) {
    // const frame = (timer-1) % 10 — drawVfx's f field (render-only)
    mv_drawVfx("firefoxcharge");

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

    if (pl->timer == 42) {
      double firefoxAngle = (i0->lsX == 0 && i0->lsY == 0)
                                ? js_pi() / 2
                                : fd_atan2(i0->lsY, i0->lsX);

      if (pl->phys.grounded && pl->phys.onSurface[0] == 0) {
        if (firefoxAngle < -js_pi() / 2) {
          // need the angle to go from -pi/2 to 3pi/2
          firefoxAngle += 2 * js_pi();
        }
        // JS ||: 0/-0/NaN fall through to Math.PI/2
        const double ga = pl->phys.groundAngle;
        const double groundedAngle =
            (ga == ga && ga != 0) ? ga : js_pi() / 2;
        if (firefoxAngle > groundedAngle + js_pi() / 2) {
          firefoxAngle = groundedAngle + js_pi() / 2;
        } else if (firefoxAngle < groundedAngle - js_pi() / 2) {
          firefoxAngle = groundedAngle - js_pi() / 2;
        }
      }
      if (firefoxAngle > js_pi()) {
        // return an angle between -pi and pi
        firefoxAngle -= 2 * js_pi();
      }
      pl->phys.upbAngleMultiplier = firefoxAngle;
    } else if (pl->timer >= 16 && !pl->phys.grounded) {
      pl->phys.cVel.y -= 0.015;
    }

    if (pl->timer > 19 && pl->timer < 34) {
      const double m = fmod(pl->timer, 2);
      if (m == 0) {
        pl->hitboxes.active[0] = true;
        pl->hitboxes.active[1] = false;
        pl->hitboxes.active[2] = false;
        pl->hitboxes.active[3] = false;
        S->aliasHbActive[(int)p] = false;
        pl->hitboxes.frame = 0;
      } else if (m == 1) {
        mv_turnOffHitboxes(S, p);
      }
    }
  }
  return AS_UNDEF;
}

static AsTri fx_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  if (pl->timer > 42) {
    fox_UPSPECIALLAUNCH.init(S, p, in, 0);
    // upstream falls through WITHOUT a return here
    return AS_UNDEF;
  } else {
    return AS_FALSE;
  }
}

static AsTri fx_land(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  // upstream land: do nothing
  (void)S; (void)p; (void)in; (void)ex;
  return AS_UNDEF;
}

const MlMoveDef fox_UPSPECIALCHARGE = {"UPSPECIALCHARGE", fx_init, fx_main,
                                       fx_interrupt, fx_land};
