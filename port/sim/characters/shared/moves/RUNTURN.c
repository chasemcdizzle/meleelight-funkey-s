// RUNTURN.c <- src/characters/shared/moves/RUNTURN.js (M2 task 7)
#include "../moves.h"

static AsTri mv_init(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "RUNTURN");
  pl->timer = 0;
  mv_dispatch(S, MV_CS(S, p), "RUNTURN", "main", p, in, 0);
  return AS_UNDEF;
}

static AsTri mv_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  const ml_attributes_t *at = mv_attr(MV_CS(S, p));
  const MlInput *i0 = &MV_IN(in, p)[0];
  double tempAcc;
  pl->timer += 1;
  if (mv_dispatch(S, MV_CS(S, p), "RUNTURN", "interrupt", p, in, 0) !=
      AS_TRUE) {
    if (pl->timer == (double)at->runTurnBreakPoint + 1) {
      pl->phys.face *= -1;
    }

    if (pl->timer <= (double)at->runTurnBreakPoint &&
        i0->lsX * pl->phys.face < -0.3) {
      tempAcc = (ml_f64(at->dAccA) -
                 (1 - js_abs(i0->lsX)) * (ml_f64(at->dAccA))) * pl->phys.face;
      pl->phys.cVel.x -= tempAcc;
    } else if (pl->timer > (double)at->runTurnBreakPoint &&
               i0->lsX * pl->phys.face > 0.3) {
      tempAcc = (ml_f64(at->dAccA) -
                 (1 - js_abs(i0->lsX)) * (ml_f64(at->dAccA))) * pl->phys.face;
      pl->phys.cVel.x += tempAcc;
    } else {
      as_reduceByTraction(true, (int)MV_CS(S, p), &pl->phys.cVel.x);
    }

    if (pl->timer == (double)at->runTurnBreakPoint) {
      if (pl->phys.cVel.x * pl->phys.face > 0) {
        pl->timer -= 1;
      }
    }
  }
  return AS_UNDEF;
}

static AsTri mv_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  const MlInput *i0 = &MV_IN(in, p)[0];
  const AsPair j = as_checkForJump(S->tapJumpOff[(int)p], MV_IN(in, p));
  if (j.flag) {
    MvX x = mvx_pair_payload(&j);
    mv_dispatch(S, MV_CS(S, p), "KNEEBEND", "init", p, in, &x);
    return AS_TRUE;
  } else if (pl->timer > mv_frames(MV_CS(S, p), "RUNTURN")) {
    if (i0->lsX * pl->phys.face > 0.6) {
      mv_dispatch(S, MV_CS(S, p), "RUN", "init", p, in, 0);
    } else {
      mv_dispatch(S, MV_CS(S, p), "WAIT", "init", p, in, 0);
    }
    return AS_TRUE;
  } else {
    return AS_FALSE;
  }
}

const MlMoveDef mv_RUNTURN = {"RUNTURN", mv_init, mv_main, mv_interrupt, 0};
