// puff_multi_jump_drift.c <- src/characters/puff/puffMultiJumpDrift.js
// (M2 task 12). Puff's multijump horizontal drift: a 1.08*lsX cap with
// airFriction pull-back, an 0.072*lsX accelerator, and a SECOND friction
// pass for the neutral stick (both passes can run — verbatim shape).
#include "moves.h"

void puff_multi_jump_drift(MlSim *S, double p, const MlInputBuffer in[4]) {
  MlPlayer *pl = mv_player(S, p);
  const MlInput *i0 = &MV_IN(in, p)[0];
  const double airFriction = ml_f64(mv_attr(MV_CS(S, p))->airFriction);
  double tempMax;
  if (js_abs(i0->lsX) < 0.3) {
    tempMax = 0;
  } else {
    tempMax = 1.08 * i0->lsX;
  }

  if ((tempMax < 0 && pl->phys.cVel.x < tempMax) ||
      (tempMax > 0 && pl->phys.cVel.x > tempMax)) {
    if (pl->phys.cVel.x > 0) {
      pl->phys.cVel.x -= airFriction;
      if (pl->phys.cVel.x < 0) {
        pl->phys.cVel.x = 0;
      }
    } else {
      pl->phys.cVel.x += airFriction;
      if (pl->phys.cVel.x > 0) {
        pl->phys.cVel.x = 0;
      }
    }
  } else if (js_abs(i0->lsX) > 0.3 &&
             ((tempMax < 0 && pl->phys.cVel.x > tempMax) ||
              (tempMax > 0 && pl->phys.cVel.x < tempMax))) {
    pl->phys.cVel.x += (0.072 * i0->lsX);
  }

  if (js_abs(i0->lsX) < 0.3) {
    if (pl->phys.cVel.x > 0) {
      pl->phys.cVel.x -= airFriction;
      if (pl->phys.cVel.x < 0) {
        pl->phys.cVel.x = 0;
      }
    } else {
      pl->phys.cVel.x += airFriction;
      if (pl->phys.cVel.x > 0) {
        pl->phys.cVel.x = 0;
      }
    }
  }
}
