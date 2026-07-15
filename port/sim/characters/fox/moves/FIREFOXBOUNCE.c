// FIREFOXBOUNCE.c <- src/characters/fox/moves/FIREFOXBOUNCE.js (M2 task 8)
// this.setVelocities comes from the mvData fox dump (rule 15).
#include "../moves.h"

static AsTri fx_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex);
static AsTri fx_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex);

static AsTri fx_init(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "FIREFOXBOUNCE");
  pl->timer = 0;
  pl->phys.grounded = false;
  fx_main(S, p, in, 0);
  return AS_UNDEF;
}

static AsTri fx_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  pl->timer += 1;
  if (fx_interrupt(S, p, in, 0) != AS_TRUE) {
    if (pl->phys.cVel.x != 0) {
      pl->phys.cVel.x -= 0.03 * pl->phys.face;
      if (pl->phys.cVel.x * pl->phys.face < 0) {
        pl->phys.cVel.x = 0;
      }
    }
    pl->phys.cVel.y = mv_fox_arr("FIREFOXBOUNCE", "setVelocities",
                                 pl->timer - 1);
  }
  return AS_UNDEF;
}

static AsTri fx_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
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

static AsTri fx_land(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  mv_LANDING.init(S, p, in, 0);
  return AS_UNDEF;
}

const MlMoveDef fox_FIREFOXBOUNCE = {"FIREFOXBOUNCE", fx_init, fx_main,
                                     fx_interrupt, fx_land};
