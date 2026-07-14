// DOWNSTANDN.c <- src/characters/shared/moves/DOWNSTANDN.js (M2 task 7)
#include "../moves.h"

static AsTri mv_init(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "DOWNSTANDN");
  pl->timer = 0;
  mv_dispatch(S, MV_CS(S, p), "DOWNSTANDN", "main", p, in, 0);
  return AS_UNDEF;
}

static AsTri mv_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  pl->timer += 1;
  if (mv_dispatch(S, MV_CS(S, p), "DOWNSTANDN", "interrupt", p, in, 0) !=
      AS_TRUE) {
    as_reduceByTraction(true, (int)MV_CS(S, p), &pl->phys.cVel.x);
    as_executeIntangibility("DOWNSTANDN", (int)MV_CS(S, p), pl->timer,
                            &pl->phys.intangibleTimer, &pl->phys.hurtBoxState);
  }
  return AS_UNDEF;
}

static AsTri mv_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  if (pl->timer > mv_frames(MV_CS(S, p), "DOWNSTANDN")) {
    mv_dispatch(S, MV_CS(S, p), "WAIT", "init", p, in, 0);
    return AS_TRUE;
  } else {
    return AS_FALSE;
  }
}

const MlMoveDef mv_DOWNSTANDN = {"DOWNSTANDN", mv_init, mv_main, mv_interrupt,
                                 0};
