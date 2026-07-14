// CATCHWAIT.c <- src/characters/shared/moves/CATCHWAIT.js (M2 task 7)
#include "../moves.h"

static AsTri mv_init(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "CATCHWAIT");
  pl->timer = 0;
  mv_turnOffHitboxes(S, p);
  mv_dispatch(S, MV_CS(S, p), "CATCHWAIT", "main", p, in, 0);
  return AS_UNDEF;
}

static AsTri mv_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  pl->timer += 1;
  if (mv_dispatch(S, MV_CS(S, p), "CATCHWAIT", "interrupt", p, in, 0) !=
      AS_TRUE) {
    // empty body upstream
  }
  return AS_UNDEF;
}

static AsTri mv_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  const MlInput *i0 = &MV_IN(in, p)[0];
  const MlInput *i1 = &MV_IN(in, p)[1];
  if (i0->a && !i1->a) {
    mv_dispatch(S, MV_CS(S, p), "CATCHATTACK", "init", p, in, 0);
    return AS_TRUE;
  } else if ((i0->lsY > 0.7 && i1->lsY <= 0.7) ||
             (i0->csY > 0.7 && i1->csY <= 0.7)) {
    mv_dispatch(S, MV_CS(S, p), "THROWUP", "init", p, in, 0);
    return AS_TRUE;
  } else if ((i0->lsY < -0.7 && i1->lsY >= -0.7) || i0->csY < -0.7) {
    mv_dispatch(S, MV_CS(S, p), "THROWDOWN", "init", p, in, 0);
    return AS_TRUE;
  } else if ((i0->lsX * pl->phys.face < -0.7 &&
              i1->lsX * pl->phys.face >= -0.7) ||
             (i0->csX * pl->phys.face < -0.7 &&
              i1->csX * pl->phys.face >= -0.7)) {
    mv_dispatch(S, MV_CS(S, p), "THROWBACK", "init", p, in, 0);
    return AS_TRUE;
  } else if ((i0->lsX * pl->phys.face > 0.7 &&
              i1->lsX * pl->phys.face <= 0.7) ||
             (i0->csX * pl->phys.face > 0.7 &&
              i1->csX * pl->phys.face <= 0.7)) {
    mv_dispatch(S, MV_CS(S, p), "THROWFORWARD", "init", p, in, 0);
    return AS_TRUE;
  } else if (pl->timer > mv_frames(MV_CS(S, p), "CATCHWAIT")) {
    mv_dispatch(S, MV_CS(S, p), "CATCHWAIT", "init", p, in, 0);
    return AS_TRUE;
  } else {
    return AS_FALSE;
  }
}

const MlMoveDef mv_CATCHWAIT = {"CATCHWAIT", mv_init, mv_main, mv_interrupt,
                                0};
