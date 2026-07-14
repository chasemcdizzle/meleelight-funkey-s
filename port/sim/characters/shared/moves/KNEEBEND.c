// KNEEBEND.c <- src/characters/shared/moves/KNEEBEND.js (M2 task 7)
#include "../moves.h"

static AsTri mv_init(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  // init(p, type, input) — type is checkForJump's payload (0|1) or, from
  // GUARD/GUARDON's c-stick arm with j[0] false, the literal `false`
  // (zero-live over the goldens; jumpSquatType is number-modeled — rule 7).
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "KNEEBEND");
  pl->timer = 0;
  pl->phys.jumpType = 1;
  if (ex == 0 || ex->count < 1 || ex->x[0].kind != DX_NUM) {
    mv_out_of_domain("KNEEBEND: non-number jumpSquatType");
  }
  pl->phys.jumpSquatType = ex->x[0].num;
  mv_dispatch(S, MV_CS(S, p), "KNEEBEND", "main", p, in, 0);
  return AS_UNDEF;
}

static AsTri mv_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  const MlInput *i0 = &MV_IN(in, p)[0];
  pl->timer += 1;
  if (mv_dispatch(S, MV_CS(S, p), "KNEEBEND", "interrupt", p, in, 0) !=
      AS_TRUE) {
    as_reduceByTraction(true, (int)MV_CS(S, p), &pl->phys.cVel.x);
    // if jumpsquat initiated by stick
    if (pl->phys.jumpSquatType == pl->phys.jumpSquatType &&
        pl->phys.jumpSquatType != 0) {
      if (i0->lsY < 0.67) {
        pl->phys.jumpType = 0;
      }
    }
    // else if jumpsquat initiated by button
    else {
      if (!i0->x && !i0->y) {
        pl->phys.jumpType = 0;
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
  if (pl->timer == (double)at->jumpSquat) {
    // so they can be detected as above current surface instantly
    mv_pos_set_y(S, p, pl->phys.pos.y + 0.001);
  }
  if (pl->timer > (double)at->jumpSquat) {
    MvX x = mvx_num(pl->phys.jumpType);
    if (MV_IN(in, p)[2].lsX * pl->phys.face >= -0.3) {
      mv_dispatch(S, MV_CS(S, p), "JUMPF", "init", p, in, &x);
    } else {
      mv_dispatch(S, MV_CS(S, p), "JUMPB", "init", p, in, &x);
    }
    return AS_TRUE;
  } else if (i0->a && !i1->a && (i0->lA > 0 || i0->rA > 0)) {
    mv_dispatch(S, MV_CS(S, p), "GRAB", "init", p, in, 0);
    return AS_TRUE;
  } else if ((i0->a && !i1->a && i0->lsY >= 0.8 &&
              MV_IN(in, p)[3].lsY < 0.3) ||
             (i0->csY >= 0.8 && MV_IN(in, p)[3].csY < 0.3)) {
    mv_dispatch(S, MV_CS(S, p), "UPSMASH", "init", p, in, 0);
    return AS_TRUE;
  } else if (i0->b && !i1->b && i0->lsY > 0.58) {
    mv_dispatch(S, MV_CS(S, p), "UPSPECIAL", "init", p, in, 0);
    return AS_TRUE;
  } else {
    return AS_FALSE;
  }
}

const MlMoveDef mv_KNEEBEND = {"KNEEBEND", mv_init, mv_main, mv_interrupt, 0};
