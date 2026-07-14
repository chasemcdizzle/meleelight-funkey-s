// RUN.c <- src/characters/shared/moves/RUN.js (M2 task 7)
#include "../moves.h"

static AsTri mv_init(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "RUN");
  pl->timer = 1;
  mv_dispatch(S, MV_CS(S, p), "RUN", "main", p, in, 0);
  return AS_UNDEF;
}

static AsTri mv_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  const ml_attributes_t *at = mv_attr(MV_CS(S, p));
  const MlInput *i0 = &MV_IN(in, p)[0];
  if (pl->timer > mv_frames(MV_CS(S, p), "RUN")) {
    pl->timer = 1;
  }
  if (mv_dispatch(S, MV_CS(S, p), "RUN", "interrupt", p, in, 0) != AS_TRUE) {
    bool footstep0 = false;
    bool footstep1 = false;
    if (pl->timer < 2) footstep0 = true;
    if (pl->timer < 10) footstep1 = true;
    const double tempMax = i0->lsX * ml_f64(at->dMaxV);

    pl->phys.cVel.x += ((ml_f64(at->dMaxV) * i0->lsX) - pl->phys.cVel.x) *
                       (1 / (ml_f64(at->dMaxV) * 2.5)) *
                       (ml_f64(at->dAccA) +
                        (ml_f64(at->dAccB) / js_abs(i0->lsX)));
    if (pl->phys.cVel.x * pl->phys.face > tempMax * pl->phys.face) {
      pl->phys.cVel.x = tempMax;
    }

    const double time = ((pl->phys.cVel.x * pl->phys.face) /
                         ml_f64(at->dMaxV)) * ml_f64(at->runAnimSpeed);
    if (time > 0) {
      pl->timer += time;
    }
    if (pl->timer > mv_frames(MV_CS(S, p), "RUN")) {
      pl->timer = 1;
    }
    if ((footstep0 && pl->timer >= 2) || (footstep1 && pl->timer >= 10)) {
      ml_sound_play("footstep");
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
  const AsPair j = as_checkForJump(S->tapJumpOff[(int)p], MV_IN(in, p));
  if (i0->a && !i1->a) {
    if (i0->lA > 0 || i0->rA > 0) {
      mv_dispatch(S, MV_CS(S, p), "GRAB", "init", p, in, 0);
    } else {
      mv_dispatch(S, MV_CS(S, p), "ATTACKDASH", "init", p, in, 0);
    }
    return AS_TRUE;
  } else if (j.flag) {
    MvX x = mvx_pair_payload(&j);
    mv_dispatch(S, MV_CS(S, p), "KNEEBEND", "init", p, in, &x);
    return AS_TRUE;
  } else if (i0->b && !i1->b && js_abs(i0->lsX) > 0.6) {
    pl->phys.face = js_sign(i0->lsX);
    if (pl->phys.grounded) {
      mv_dispatch(S, MV_CS(S, p), "SIDESPECIALGROUND", "init", p, in, 0);
    } else {
      mv_dispatch(S, MV_CS(S, p), "SIDESPECIALAIR", "init", p, in, 0);
    }
    return AS_TRUE;
  } else if (i0->b && !i1->b && i0->lsY < -0.58) {
    mv_dispatch(S, MV_CS(S, p), "DOWNSPECIALGROUND", "init", p, in, 0);
    return AS_TRUE;
  } else if (i0->l || i0->r) {
    mv_dispatch(S, MV_CS(S, p), "GUARDON", "init", p, in, 0);
    return AS_TRUE;
  } else if (i0->lA > 0 || i0->rA > 0) {
    mv_dispatch(S, MV_CS(S, p), "GUARDON", "init", p, in, 0);
    return AS_TRUE;
  } else if (i0->du) {
    mv_dispatch(S, MV_CS(S, p), "APPEAL", "init", p, in, 0);
    return AS_TRUE;
  } else if (js_abs(i0->lsX) < 0.62) {
    mv_dispatch(S, MV_CS(S, p), "RUNBRAKE", "init", p, in, 0);
    return AS_TRUE;
  } else if (i0->lsX * pl->phys.face < -0.3) {
    mv_dispatch(S, MV_CS(S, p), "RUNTURN", "init", p, in, 0);
    return AS_TRUE;
  }
  // upstream falls off the end of the chain: returns undefined
  return AS_UNDEF;
}

const MlMoveDef mv_RUN = {"RUN", mv_init, mv_main, mv_interrupt, 0};
