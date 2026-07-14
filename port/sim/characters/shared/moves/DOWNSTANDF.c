// DOWNSTANDF.c <- src/characters/shared/moves/DOWNSTANDF.js (M2 task 7)
#include "../moves.h"

static AsTri mv_init(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "DOWNSTANDF");
  pl->timer = 0;
  mv_dispatch(S, MV_CS(S, p), "DOWNSTANDF", "main", p, in, 0);
  return AS_UNDEF;
}

static AsTri mv_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  pl->timer += 1;
  if (mv_dispatch(S, MV_CS(S, p), "DOWNSTANDF", "interrupt", p, in, 0) !=
      AS_TRUE) {
    pl->phys.cVel.x = mv_setVelocity(MV_CS(S, p), "DOWNSTANDF",
                                     pl->timer - 1) * pl->phys.face;
    as_executeIntangibility("DOWNSTANDF", (int)MV_CS(S, p), pl->timer,
                            &pl->phys.intangibleTimer, &pl->phys.hurtBoxState);
  }
  return AS_UNDEF;
}

static AsTri mv_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  if (pl->timer > mv_frames(MV_CS(S, p), "DOWNSTANDF")) {
    mv_dispatch(S, MV_CS(S, p), "WAIT", "init", p, in, 0);
    return AS_TRUE;
  } else {
    return AS_FALSE;
  }
}

const MlMoveDef mv_DOWNSTANDF = {"DOWNSTANDF", mv_init, mv_main, mv_interrupt,
                                 0};
