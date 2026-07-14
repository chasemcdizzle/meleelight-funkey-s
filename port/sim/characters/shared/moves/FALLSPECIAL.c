// FALLSPECIAL.c <- src/characters/shared/moves/FALLSPECIAL.js (M2 task 7)
#include "../moves.h"

static AsTri mv_init(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "FALLSPECIAL");
  pl->timer = 0;
  mv_dispatch(S, MV_CS(S, p), "FALLSPECIAL", "main", p, in, 0);
  return AS_UNDEF;
}

static AsTri mv_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  pl->timer += 1;
  if (mv_dispatch(S, MV_CS(S, p), "FALLSPECIAL", "interrupt", p, in, 0) !=
      AS_TRUE) {
    as_fastfall((int)MV_CS(S, p), &pl->phys.cVel.y, &pl->phys.fastfalled,
                MV_IN(in, p));
    as_airDrift((int)MV_CS(S, p), &pl->phys.cVel.x, MV_IN(in, p));
  }
  return AS_UNDEF;
}

static AsTri mv_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  if (pl->timer > mv_frames(MV_CS(S, p), "FALLSPECIAL")) {
    mv_dispatch(S, MV_CS(S, p), "FALLSPECIAL", "init", p, in, 0);
    return AS_TRUE;
  } else {
    return AS_FALSE;
  }
}

static AsTri mv_land(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  mv_dispatch(S, MV_CS(S, p), "LANDINGFALLSPECIAL", "init", p, in, 0);
  return AS_UNDEF;
}

const MlMoveDef mv_FALLSPECIAL = {"FALLSPECIAL", mv_init, mv_main,
                                  mv_interrupt, mv_land};
