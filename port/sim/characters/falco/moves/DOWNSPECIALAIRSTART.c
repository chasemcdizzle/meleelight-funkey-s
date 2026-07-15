// DOWNSPECIALAIRSTART.c <- src/characters/falco/moves/DOWNSPECIALAIRSTART.js
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
  strcpy(pl->actionState, "DOWNSPECIALAIRSTART");
  pl->timer = 0;
  pl->phys.fastfalled = false;
  pl->phys.inShine = 0;
  pl->shineLoop = 6;
  pl->phys.cVel.y = 0;
  pl->phys.cVel.x *= 0.5;
  ml_sound_play("foxshine");
  mv_drawVfx("impactLand");
  mv_drawVfx("shine");
  mv_turnOffHitboxes(S, p);
  mv_assign_hitbox_id(S, p, "downspecial", 0, 0);
  fc_main(S, p, in, 0);
  return AS_UNDEF;
}

static AsTri fc_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  pl->timer += 1;
  pl->phys.inShine += 1;
  if (fc_interrupt(S, p, in, 0) != AS_TRUE) {
    if (pl->phys.cVel.x > 0) {
      if (pl->phys.cVel.x > 0.85) {
        pl->phys.cVel.x -= 0.03;
      } else {
        pl->phys.cVel.x -= 0.02;
      }
      if (pl->phys.cVel.x < 0) {
        pl->phys.cVel.x = 0;
      }
    } else if (pl->phys.cVel.x < 0) {
      if (pl->phys.cVel.x < -0.85) {
        pl->phys.cVel.x += 0.03;
      } else {
        pl->phys.cVel.x += 0.02;
      }
      if (pl->phys.cVel.x > 0) {
        pl->phys.cVel.x = 0;
      }
    }

    if (pl->timer == 1) {
      pl->hitboxes.active[0] = true;
      pl->hitboxes.active[1] = false;
      pl->hitboxes.active[2] = false;
      pl->hitboxes.active[3] = false;
      S->aliasHbActive[(int)p] = false;
      pl->hitboxes.frame = 0;
      pl->phys.intangibleTimer = js_max(pl->phys.intangibleTimer, 1);
    }
    if (pl->timer == 2) {
      mv_turnOffHitboxes(S, p);
    }
  }
  return AS_UNDEF;
}

static AsTri fc_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  if (pl->timer > 3) {
    falco_DOWNSPECIALAIRLOOP.init(S, p, in, 0);
    return AS_TRUE;
  } else {
    return AS_FALSE;
  }
}

static AsTri fc_land(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)in; (void)ex;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "DOWNSPECIALGROUNDSTART");
  return AS_UNDEF;
}

const MlMoveDef falco_DOWNSPECIALAIRSTART = {"DOWNSPECIALAIRSTART", fc_init,
                                             fc_main, fc_interrupt, fc_land};
