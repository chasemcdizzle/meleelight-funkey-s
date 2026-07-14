// DASH.c <- src/characters/shared/moves/DASH.js (M2 task 7)
#include "../moves.h"

static AsTri mv_init(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "DASH");
  pl->timer = 0;
  ml_sound_play("dash");
  mv_dispatch(S, MV_CS(S, p), "DASH", "main", p, in, 0);
  return AS_UNDEF;
}

static AsTri mv_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  const ml_attributes_t *at = mv_attr(MV_CS(S, p));
  const MlInput *i0 = &MV_IN(in, p)[0];
  pl->timer += 1;
  if (mv_dispatch(S, MV_CS(S, p), "DASH", "interrupt", p, in, 0) != AS_TRUE) {
    if (pl->timer == 2) {
      pl->phys.cVel.x += ml_f64(at->dInitV) * pl->phys.face;
      if (js_abs(pl->phys.cVel.x) > ml_f64(at->dMaxV)) {
        pl->phys.cVel.x = ml_f64(at->dMaxV) * pl->phys.face;
      }
    }
    if (pl->timer == 4) {
      mv_drawVfx("dashDust");
    }
    if (pl->timer > 1) {
      if (js_abs(i0->lsX) < 0.3) {
        as_reduceByTraction(false, (int)MV_CS(S, p), &pl->phys.cVel.x);
      } else {
        const double tempMax = i0->lsX * ml_f64(at->dMaxV);
        const double tempAcc = i0->lsX * ml_f64(at->dAccA);

        pl->phys.cVel.x += tempAcc;
        if ((tempMax > 0 && pl->phys.cVel.x > tempMax) ||
            (tempMax < 0 && pl->phys.cVel.x < tempMax)) {
          as_reduceByTraction(false, (int)MV_CS(S, p), &pl->phys.cVel.x);
          if ((tempMax > 0 && pl->phys.cVel.x < tempMax) ||
              (tempMax < 0 && pl->phys.cVel.x > tempMax)) {
            pl->phys.cVel.x = tempMax;
          }
        } else {
          pl->phys.cVel.x += tempAcc;
          if ((tempMax > 0 && pl->phys.cVel.x > tempMax) ||
              (tempMax < 0 && pl->phys.cVel.x < tempMax)) {
            pl->phys.cVel.x = tempMax;
          }
        }
      }
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
  const AsPair j = as_checkForJump(S->tapJumpOff[(int)p], MV_IN(in, p));
  if (i0->l || i0->r) {
    pl->phys.cVel.x *= 0.25;
    mv_dispatch(S, MV_CS(S, p), "GUARDON", "init", p, in, 0);
    return AS_TRUE;
  } else if (i0->lA > 0 || i0->rA > 0) {
    pl->phys.cVel.x *= 0.25;
    mv_dispatch(S, MV_CS(S, p), "GUARDON", "init", p, in, 0);
    return AS_TRUE;
  } else if (i0->a && !i1->a) {
    if (pl->timer < 4 && i0->lsX * pl->phys.face >= 0.8) {
      pl->phys.cVel.x *= 0.25;
      mv_dispatch(S, MV_CS(S, p), "FORWARDSMASH", "init", p, in, 0);
    } else if (i0->lA > 0 || i0->rA > 0) {
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
  } else if (i0->du) {
    mv_dispatch(S, MV_CS(S, p), "APPEAL", "init", p, in, 0);
    return AS_TRUE;
  } else if (pl->timer > 4 && as_checkForSmashTurn(pl->phys.face,
                                                   MV_IN(in, p))) {
    pl->phys.cVel.x *= 0.25;
    mv_dispatch(S, MV_CS(S, p), "SMASHTURN", "init", p, in, 0);
    return AS_TRUE;
  } else if (pl->timer > (double)at->dashFrameMax &&
             i0->lsX * pl->phys.face > 0.79 &&
             MV_IN(in, p)[2].lsX * pl->phys.face < 0.3) {
    mv_dispatch(S, MV_CS(S, p), "DASH", "init", p, in, 0);
    return AS_TRUE;
  } else if (pl->timer > (double)at->dashFrameMin &&
             i0->lsX * pl->phys.face > 0.62) {
    mv_dispatch(S, MV_CS(S, p), "RUN", "init", p, in, 0);
    return AS_TRUE;
  } else if (pl->timer > mv_frames(MV_CS(S, p), "DASH")) {
    mv_dispatch(S, MV_CS(S, p), "WAIT", "init", p, in, 0);
    return AS_TRUE;
  } else {
    return AS_FALSE;
  }
}

const MlMoveDef mv_DASH = {"DASH", mv_init, mv_main, mv_interrupt, 0};
