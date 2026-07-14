// CAPTURECUT.c <- src/characters/shared/moves/CAPTURECUT.js (M2 task 7)
#include "../moves.h"

static AsTri mv_init(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "CAPTURECUT");
  pl->timer = 0;
  pl->phys.grabbedBy = -1;
  pl->phys.cVel.x = -1 * pl->phys.face;
  mv_dispatch(S, MV_CS(S, p), "CAPTURECUT", "main", p, in, 0);
  return AS_UNDEF;
}

static AsTri mv_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  pl->timer += 1;
  if (mv_dispatch(S, MV_CS(S, p), "CAPTURECUT", "interrupt", p, in, 0) !=
      AS_TRUE) {
    if (pl->timer == 2) {
      pl->phys.hasGrabTech = true;
      pl->phys.grabTech = false;
    }
    as_reduceByTraction(true, (int)MV_CS(S, p), &pl->phys.cVel.x);
  }
  return AS_UNDEF;
}

static AsTri mv_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  if (pl->timer > mv_frames(MV_CS(S, p), "CAPTURECUT")) {
    mv_dispatch(S, MV_CS(S, p), "WAIT", "init", p, in, 0);
    return AS_TRUE;
  } else {
    return AS_FALSE;
  }
}

const MlMoveDef mv_CAPTURECUT = {"CAPTURECUT", mv_init, mv_main, mv_interrupt,
                                 0};
