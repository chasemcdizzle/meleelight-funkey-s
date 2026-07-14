// FALL.c <- src/characters/shared/moves/FALL.js (M2 task 7)
#include "../moves.h"

static AsTri mv_init(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  // init(p, input, disableInputs = false)
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "FALL");
  pl->timer = 0;
  mv_turnOffHitboxes(S, p);
  // upstream forwards the RESOLVED disableInputs value to main:
  MvX x = mvx_bool(mvx_truthy(ex, 0));
  mv_dispatch(S, MV_CS(S, p), "FALL", "main", p, in, &x);
  return AS_UNDEF;
}

static AsTri mv_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  // main(p, input, disableInputs = false)
  MlPlayer *pl = mv_player(S, p);
  const ml_attributes_t *at = mv_attr(MV_CS(S, p));
  pl->timer += 1;
  if (mvx_truthy(ex, 0)) {
    pl->phys.cVel.y -= ml_f64(at->gravity);
    as_airDrift((int)MV_CS(S, p), &pl->phys.cVel.x, MV_IN(in, p));
  } else {
    if (mv_dispatch(S, MV_CS(S, p), "FALL", "interrupt", p, in, 0) !=
        AS_TRUE) {
      as_fastfall((int)MV_CS(S, p), &pl->phys.cVel.y, &pl->phys.fastfalled,
                  MV_IN(in, p));
      as_airDrift((int)MV_CS(S, p), &pl->phys.cVel.x, MV_IN(in, p));
    }
  }
  return AS_UNDEF;
}

static AsTri mv_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  const ml_attributes_t *at = mv_attr(MV_CS(S, p));
  const MlInput *i0 = &MV_IN(in, p)[0];
  const MlInput *i1 = &MV_IN(in, p)[1];
  const AsPair a = as_checkForAerials(pl->phys.face, MV_IN(in, p));
  const AsPair b = as_checkForSpecials(&pl->phys.face,
                                       pl->phys.bTurnaroundTimer,
                                       pl->phys.bTurnaroundDirection,
                                       pl->phys.grounded, MV_IN(in, p));
  if (a.flag) {
    mv_dispatch(S, MV_CS(S, p), mv_pair_str(&a), "init", p, in, 0);
    return AS_TRUE;
  } else if ((i0->l && !i1->l) || (i0->r && !i1->r)) {
    mv_dispatch(S, MV_CS(S, p), "ESCAPEAIR", "init", p, in, 0);
    return AS_TRUE;
  } else if (as_checkForDoubleJump(S->tapJumpOff[(int)p], MV_IN(in, p)) &&
             (!pl->phys.doubleJumped ||
              (pl->phys.jumpsUsed < 5 && at->multiJump != 0))) {
    if (i0->lsX * pl->phys.face < -0.3) {
      mv_dispatch(S, MV_CS(S, p), "JUMPAERIALB", "init", p, in, 0);
    } else {
      mv_dispatch(S, MV_CS(S, p), "JUMPAERIALF", "init", p, in, 0);
    }
    return AS_TRUE;
  } else if (b.flag) {
    mv_dispatch(S, MV_CS(S, p), mv_pair_str(&b), "init", p, in, 0);
    return AS_TRUE;
  } else if (pl->timer > mv_frames(MV_CS(S, p), "FALL")) {
    mv_dispatch(S, MV_CS(S, p), "FALL", "init", p, in, 0);
    return AS_TRUE;
  } else {
    return AS_FALSE;
  }
}

const MlMoveDef mv_FALL = {"FALL", mv_init, mv_main, mv_interrupt, 0};
