// PASS.c <- src/characters/shared/moves/PASS.js (M2 task 7)
#include "../moves.h"

static AsTri mv_init(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "PASS");
  pl->timer = 0;
  pl->phys.grounded = false;
  pl->phys.passFastfall = false;
  pl->phys.cVel.y = -0.5;
  pl->phys.hasPassing = true;
  pl->phys.passing = true;
  mv_dispatch(S, MV_CS(S, p), "PASS", "main", p, in, 0);
  return AS_UNDEF;
}

static AsTri mv_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  const ml_attributes_t *at = mv_attr(MV_CS(S, p));
  const MlInput *i0 = &MV_IN(in, p)[0];
  pl->timer += 1;
  if (pl->timer > 1) {
    if (mv_dispatch(S, MV_CS(S, p), "PASS", "interrupt", p, in, 0) !=
        AS_TRUE) {
      if (pl->phys.passFastfall) {
        as_fastfall((int)MV_CS(S, p), &pl->phys.cVel.y, &pl->phys.fastfalled,
                    MV_IN(in, p));
      } else {
        pl->phys.cVel.y -= ml_f64(at->gravity);
        if (pl->phys.cVel.y < -ml_f64(at->terminalV)) {
          pl->phys.cVel.y = -ml_f64(at->terminalV);
        }
        if (i0->lsY > -0.3) {
          pl->phys.passFastfall = true;
        }
      }
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
  } else if (pl->timer > mv_frames(MV_CS(S, p), "PASS")) {
    mv_dispatch(S, MV_CS(S, p), "FALL", "init", p, in, 0);
    return AS_TRUE;
  } else {
    return AS_FALSE;
  }
}

const MlMoveDef mv_PASS = {"PASS", mv_init, mv_main, mv_interrupt, 0};
