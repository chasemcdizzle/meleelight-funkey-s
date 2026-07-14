// REBIRTHWAIT.c <- src/characters/shared/moves/REBIRTHWAIT.js (M2 task 7)
#include "../moves.h"

static AsTri mv_init(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  (void)in;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "REBIRTHWAIT");
  pl->timer = 1;
  pl->phys.cVel.y = 0;
  return AS_UNDEF; // upstream REBIRTHWAIT.init does NOT call main
}

static AsTri mv_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  pl->timer += 1;
  pl->spawnWaitTime += 1;
  if (mv_dispatch(S, MV_CS(S, p), "REBIRTHWAIT", "interrupt", p, in, 0) !=
      AS_TRUE) {
    pl->phys.outOfCameraTimer = 0;
  }
  return AS_UNDEF;
}

static AsTri mv_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  const MlInput *i0 = &MV_IN(in, p)[0];
  const MlInput *i1 = &MV_IN(in, p)[1];
  const AsPair a = as_checkForAerials(pl->phys.face, MV_IN(in, p));
  const AsPair b = as_checkForSpecials(&pl->phys.face,
                                       pl->phys.bTurnaroundTimer,
                                       pl->phys.bTurnaroundDirection,
                                       pl->phys.grounded, MV_IN(in, p));
  const bool j = as_checkForDoubleJump(S->tapJumpOff[(int)p], MV_IN(in, p));
  if (a.flag) {
    pl->phys.grounded = false;
    pl->phys.invincibleTimer = 120;
    mv_dispatch(S, MV_CS(S, p), mv_pair_str(&a), "init", p, in, 0);
    return AS_TRUE;
  } else if ((i0->l && !i1->l) || (i0->r && !i1->r)) {
    pl->phys.grounded = false;
    pl->phys.invincibleTimer = 120;
    mv_dispatch(S, MV_CS(S, p), "ESCAPEAIR", "init", p, in, 0);
    return AS_TRUE;
  } else if (j) {
    pl->phys.grounded = false;
    pl->phys.invincibleTimer = 120;
    if (i0->lsX * pl->phys.face < -0.3) {
      mv_dispatch(S, MV_CS(S, p), "JUMPAERIALB", "init", p, in, 0);
    } else {
      mv_dispatch(S, MV_CS(S, p), "JUMPAERIALF", "init", p, in, 0);
    }
    return AS_TRUE;
  } else if (b.flag) {
    pl->phys.grounded = false;
    pl->phys.invincibleTimer = 120;
    mv_dispatch(S, MV_CS(S, p), mv_pair_str(&b), "init", p, in, 0);
    return AS_TRUE;
  }
  // upstream: a NEW `if` (not else-if) — uses framesData WAIT, verbatim
  if (pl->timer > mv_frames(MV_CS(S, p), "WAIT")) {
    mv_dispatch(S, MV_CS(S, p), "REBIRTHWAIT", "init", p, in, 0);
    return AS_TRUE;
  } else if (pl->spawnWaitTime > 300) {
    pl->phys.grounded = false;
    pl->phys.invincibleTimer = 120;
    mv_dispatch(S, MV_CS(S, p), "FALL", "init", p, in, 0);
    return AS_TRUE;
  } else if (js_abs(i0->lsX) > 0.3 || js_abs(i0->lsY) > 0.3) {
    pl->phys.grounded = false;
    pl->phys.invincibleTimer = 120;
    mv_dispatch(S, MV_CS(S, p), "FALL", "init", p, in, 0);
    return AS_TRUE;
  } else {
    return AS_FALSE;
  }
}

const MlMoveDef mv_REBIRTHWAIT = {"REBIRTHWAIT", mv_init, mv_main,
                                  mv_interrupt, 0};
