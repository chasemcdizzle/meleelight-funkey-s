// CLIFFWAIT.c <- src/characters/shared/moves/CLIFFWAIT.js (M2 task 7)
#include "../moves.h"

static AsTri mv_init(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "CLIFFWAIT");
  pl->timer = 0;
  mv_dispatch(S, MV_CS(S, p), "CLIFFWAIT", "main", p, in, 0);
  return AS_UNDEF;
}

static AsTri mv_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  pl->timer += 1;
  if (mv_dispatch(S, MV_CS(S, p), "CLIFFWAIT", "interrupt", p, in, 0) !=
      AS_TRUE) {
    // ledgeHangTimer++ (runtime-added; undefined++ -> NaN, then present)
    const double v = pl->phys.hasLedgeHangTimer ? pl->phys.ledgeHangTimer
                                                : js_nan();
    pl->phys.hasLedgeHangTimer = true;
    pl->phys.ledgeHangTimer = v + 1;
  }
  return AS_UNDEF;
}

static AsTri mv_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  const MlInput *i0 = &MV_IN(in, p)[0];
  const MlInput *i1 = &MV_IN(in, p)[1];
  if ((i0->lsX * pl->phys.face < -0.2 &&
       i1->lsX * pl->phys.face >= -0.2) ||
      (i0->lsY < -0.2 && i1->lsY >= -0.2) ||
      (i0->csX * pl->phys.face < -0.2 &&
       i1->csX * pl->phys.face >= -0.2) ||
      (i0->csY < -0.2 && i1->csY >= -0.2)) {
    pl->phys.onLedge = -1;
    pl->phys.ledgeRegrabCount = true;
    MvX x = mvx_bool(true);
    mv_dispatch(S, MV_CS(S, p), "FALL", "init", p, in, &x);
    return AS_TRUE;
  } else if ((i0->x && !i1->x) || (i0->y && !i1->y) ||
             (i0->lsY > 0.65 && i1->lsY <= 0.65)) {
    if (pl->percent < 100) {
      mv_dispatch(S, MV_CS(S, p), "CLIFFJUMPQUICK", "init", p, in, 0);
    } else {
      mv_dispatch(S, MV_CS(S, p), "CLIFFJUMPSLOW", "init", p, in, 0);
    }
    return AS_TRUE;
  } else if ((i0->lsX * pl->phys.face > 0.2 &&
              i1->lsX * pl->phys.face <= 0.2) ||
             (i0->lsY > 0.2 && i1->lsY <= 0.2)) {
    if (pl->percent < 100) {
      mv_dispatch(S, MV_CS(S, p), "CLIFFGETUPQUICK", "init", p, in, 0);
    } else {
      mv_dispatch(S, MV_CS(S, p), "CLIFFGETUPSLOW", "init", p, in, 0);
    }
    return AS_TRUE;
  } else if ((i0->a && !i1->a) || (i0->b && !i1->b) ||
             (i0->csY > 0.65 && i1->csY <= 0.65)) {
    if (pl->percent < 100) {
      mv_dispatch(S, MV_CS(S, p), "CLIFFATTACKQUICK", "init", p, in, 0);
    } else {
      mv_dispatch(S, MV_CS(S, p), "CLIFFATTACKSLOW", "init", p, in, 0);
    }
    return AS_TRUE;
  } else if ((i0->lA > 0.3 && i1->lA <= 0.3) ||
             (i0->rA > 0.3 && i1->rA <= 0.3) ||
             (i0->csX * pl->phys.face > 0.8 &&
              i1->csX * pl->phys.face <= 0.8)) {
    if (pl->percent < 100) {
      mv_dispatch(S, MV_CS(S, p), "CLIFFESCAPEQUICK", "init", p, in, 0);
    } else {
      mv_dispatch(S, MV_CS(S, p), "CLIFFESCAPESLOW", "init", p, in, 0);
    }
    return AS_TRUE;
  } else if ((pl->phys.hasLedgeHangTimer ? pl->phys.ledgeHangTimer
                                         : js_nan()) > 600) {
    pl->phys.onLedge = -1;
    pl->phys.ledgeRegrabCount = true;
    mv_dispatch(S, MV_CS(S, p), "DAMAGEFALL", "init", p, in, 0);
    return AS_TRUE;
  } else if (pl->timer > mv_frames(MV_CS(S, p), "CLIFFWAIT")) {
    mv_dispatch(S, MV_CS(S, p), "CLIFFWAIT", "init", p, in, 0);
    return AS_TRUE;
  } else {
    return AS_FALSE;
  }
}

const MlMoveDef mv_CLIFFWAIT = {"CLIFFWAIT", mv_init, mv_main, mv_interrupt,
                                0};
