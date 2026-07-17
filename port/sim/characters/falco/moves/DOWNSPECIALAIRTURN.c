// DOWNSPECIALAIRTURN.c <- src/characters/falco/moves/DOWNSPECIALAIRTURN.js
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
  strcpy(pl->actionState, "DOWNSPECIALAIRTURN");
  pl->timer = 0;
  mv_turnOffHitboxes(S, p);
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

    pl->phys.cVel.y -= 0.02667;
    if (pl->phys.cVel.y < -ml_f64(at->terminalV)) {
      pl->phys.cVel.y = -ml_f64(at->terminalV);
    }

    if (pl->shineLoop == 6) {
      pl->shineLoop = 0;
    }
    pl->shineLoop += 1;
    ml_drawVfx("shineloop", 0, 0, p);
  }
  return AS_UNDEF;
}

static AsTri fc_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  if (pl->timer > 3) {
    pl->phys.face *= -1;
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
  strcpy(pl->actionState, "DOWNSPECIALGROUNDTURN");
  return AS_UNDEF;
}

const MlMoveDef falco_DOWNSPECIALAIRTURN = {"DOWNSPECIALAIRTURN", fc_init,
                                            fc_main, fc_interrupt, fc_land};
