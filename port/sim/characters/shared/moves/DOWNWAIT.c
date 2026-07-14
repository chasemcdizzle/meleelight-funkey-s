// DOWNWAIT.c <- src/characters/shared/moves/DOWNWAIT.js (M2 task 7)
#include "../moves.h"

static AsTri mv_init(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "DOWNWAIT");
  pl->timer = 0;
  mv_dispatch(S, MV_CS(S, p), "DOWNWAIT", "main", p, in, 0);
  return AS_UNDEF;
}

static AsTri mv_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  pl->timer += 1;
  if (mv_dispatch(S, MV_CS(S, p), "DOWNWAIT", "interrupt", p, in, 0) !=
      AS_TRUE) {
    as_reduceByTraction(true, (int)MV_CS(S, p), &pl->phys.cVel.x);
    if (pl->timer > 1) {
      pl->hit.hitstun -= 1;
    }
  }
  return AS_UNDEF;
}

static AsTri mv_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  const MlInput *i0 = &MV_IN(in, p)[0];
  const MlInput *i1 = &MV_IN(in, p)[1];
  if (pl->timer > mv_frames(MV_CS(S, p), "DOWNWAIT")) {
    mv_dispatch(S, MV_CS(S, p), "DOWNWAIT", "init", p, in, 0);
    return AS_TRUE;
  } else if (pl->phys.jabReset) {
    if (pl->hit.hitstun <= 0) {
      if (i0->lsX * pl->phys.face < -0.7) {
        mv_dispatch(S, MV_CS(S, p), "DOWNSTANDB", "init", p, in, 0);
        return AS_TRUE;
      } else if (i0->lsX * pl->phys.face > 0.7) {
        mv_dispatch(S, MV_CS(S, p), "DOWNSTANDF", "init", p, in, 0);
        return AS_TRUE;
      } else if ((i0->a && !i1->a) || (i0->b && !i1->b)) {
        mv_dispatch(S, MV_CS(S, p), "DOWNATTACK", "init", p, in, 0);
        return AS_TRUE;
      } else {
        mv_dispatch(S, MV_CS(S, p), "DOWNSTANDN", "init", p, in, 0);
        return AS_TRUE;
      }
    } else {
      return AS_FALSE;
    }
  } else if (i0->lsX * pl->phys.face < -0.7) {
    mv_dispatch(S, MV_CS(S, p), "DOWNSTANDB", "init", p, in, 0);
    return AS_TRUE;
  } else if (i0->lsX * pl->phys.face > 0.7) {
    mv_dispatch(S, MV_CS(S, p), "DOWNSTANDF", "init", p, in, 0);
    return AS_TRUE;
  } else if (i0->lsY > 0.7) {
    mv_dispatch(S, MV_CS(S, p), "DOWNSTANDN", "init", p, in, 0);
    return AS_TRUE;
  } else if ((i0->a && !i1->a) || (i0->b && !i1->b)) {
    mv_dispatch(S, MV_CS(S, p), "DOWNATTACK", "init", p, in, 0);
    return AS_TRUE;
  } else {
    return AS_FALSE;
  }
}

const MlMoveDef mv_DOWNWAIT = {"DOWNWAIT", mv_init, mv_main, mv_interrupt, 0};
