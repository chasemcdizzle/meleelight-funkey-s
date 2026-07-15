// FIREFOXBOUNCE.c <- src/characters/falco/moves/FIREFOXBOUNCE.js (M2 task 9)
// this.setVelocities comes from the mvData falco dump (rule 15).
#include "../moves.h"

static AsTri fc_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex);
static AsTri fc_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex);

static AsTri fc_init(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "FIREFOXBOUNCE");
  pl->timer = 0;
  pl->phys.grounded = false;
  fc_main(S, p, in, 0);
  return AS_UNDEF;
}

static AsTri fc_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  pl->timer += 1;
  if (fc_interrupt(S, p, in, 0) != AS_TRUE) {
    if (pl->phys.cVel.x != 0) {
      pl->phys.cVel.x -= 0.03 * pl->phys.face;
      if (pl->phys.cVel.x * pl->phys.face < 0) {
        pl->phys.cVel.x = 0;
      }
    }
    pl->phys.cVel.y = mv_falco_arr("FIREFOXBOUNCE", "setVelocities",
                                 pl->timer - 1);
  }
  return AS_UNDEF;
}

static AsTri fc_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  if (pl->timer > 14) {
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
  mv_LANDING.init(S, p, in, 0);
  return AS_UNDEF;
}

const MlMoveDef falco_FIREFOXBOUNCE = {"FIREFOXBOUNCE", fc_init, fc_main,
                                     fc_interrupt, fc_land};
