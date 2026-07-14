// ESCAPEAIR.c <- src/characters/shared/moves/ESCAPEAIR.js (M2 task 7)
#include "../moves.h"

static AsTri mv_init(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  const MlInput *i0 = &MV_IN(in, p)[0];
  strcpy(pl->actionState, "ESCAPEAIR");
  pl->timer = 0;
  if (js_abs(i0->lsX) > 0 || js_abs(i0->lsY) > 0) {
    const double ang = as_getAngle(i0->lsX, i0->lsY);
    pl->phys.cVel.x = 3.1 * fd_cos(ang);
    pl->phys.cVel.y = 3.1 * fd_sin(ang);
  } else {
    pl->phys.cVel.x = 0;
    pl->phys.cVel.y = 0;
  }
  pl->phys.fastfalled = false;
  pl->phys.landingMultiplier = 3;
  mv_dispatch(S, MV_CS(S, p), "ESCAPEAIR", "main", p, in, 0);
  return AS_UNDEF;
}

static AsTri mv_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  pl->timer += 1;
  if (mv_dispatch(S, MV_CS(S, p), "ESCAPEAIR", "interrupt", p, in, 0) !=
      AS_TRUE) {
    if (pl->timer < 30) {
      pl->phys.cVel.x *= 0.9;
      pl->phys.cVel.y *= 0.9;
    } else {
      as_airDrift((int)MV_CS(S, p), &pl->phys.cVel.x, MV_IN(in, p));
      as_fastfall((int)MV_CS(S, p), &pl->phys.cVel.y, &pl->phys.fastfalled,
                  MV_IN(in, p));
    }
    as_executeIntangibility("ESCAPEAIR", (int)MV_CS(S, p), pl->timer,
                            &pl->phys.intangibleTimer, &pl->phys.hurtBoxState);
    mv_playSounds(S, "ESCAPEAIR", p);
  }
  return AS_UNDEF;
}

static AsTri mv_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  if (pl->timer > 49) {
    mv_dispatch(S, MV_CS(S, p), "FALLSPECIAL", "init", p, in, 0);
    return AS_TRUE;
  } else {
    return AS_FALSE;
  }
}

static AsTri mv_land(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  pl->phys.intangibleTimer = 0;
  pl->phys.hurtBoxState = 0;
  mv_dispatch(S, MV_CS(S, p), "LANDINGFALLSPECIAL", "init", p, in, 0);
  return AS_UNDEF;
}

const MlMoveDef mv_ESCAPEAIR = {"ESCAPEAIR", mv_init, mv_main, mv_interrupt,
                                mv_land};
