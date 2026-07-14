// TILTTURN.c <- src/characters/shared/moves/TILTTURN.js (M2 task 7)
#include "../moves.h"

static AsTri mv_init(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "TILTTURN");
  pl->timer = 0;
  mv_dispatch(S, MV_CS(S, p), "TILTTURN", "main", p, in, 0);
  return AS_UNDEF;
}

static AsTri mv_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  pl->timer += 1;
  if (pl->timer == 6) {
    pl->phys.face *= -1;
  }
  if (mv_dispatch(S, MV_CS(S, p), "TILTTURN", "interrupt", p, in, 0) !=
      AS_TRUE) {
    as_reduceByTraction(true, (int)MV_CS(S, p), &pl->phys.cVel.x);
  }
  return AS_UNDEF;
}

static AsTri mv_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  const MlInput *i0 = &MV_IN(in, p)[0];
  const MlInput *i1 = &MV_IN(in, p)[1];
  const AsPair t = pl->timer < 6
      ? as_checkForTilts(pl->phys.face, MV_IN(in, p), true, -1)
      : as_checkForTilts(pl->phys.face, MV_IN(in, p), false, 0);
  const AsPair s = as_checkForSmashes(&pl->phys.face, MV_IN(in, p));
  const AsPair j = as_checkForJump(S->tapJumpOff[(int)p], MV_IN(in, p));

  if (j.flag) {
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
  } else if (i0->l || i0->r) {
    mv_dispatch(S, MV_CS(S, p), "GUARDON", "init", p, in, 0);
    return AS_TRUE;
  } else if (i0->lA > 0 || i0->rA > 0) {
    mv_dispatch(S, MV_CS(S, p), "GUARDON", "init", p, in, 0);
    return AS_TRUE;
  } else if (s.flag) {
    mv_dispatch(S, MV_CS(S, p), mv_pair_str(&s), "init", p, in, 0);
    return AS_TRUE;
  } else if (t.flag) {
    // upstream arm has NO return: falls out of the chain -> undefined
    if (pl->timer < 6) {
      pl->phys.face *= -1;
    }
    mv_dispatch(S, MV_CS(S, p), mv_pair_str(&t), "init", p, in, 0);
    return AS_UNDEF;
  } else if (pl->timer > 11) {
    mv_dispatch(S, MV_CS(S, p), "WAIT", "init", p, in, 0);
    return AS_TRUE;
  } else if (i0->du) {
    mv_dispatch(S, MV_CS(S, p), "APPEAL", "init", p, in, 0);
    return AS_TRUE;
  } else if (pl->timer == 6 && i0->lsX * pl->phys.face > 0.79 &&
             pl->phys.dashbuffer) {
    mv_dispatch(S, MV_CS(S, p), "DASH", "init", p, in, 0);
    return AS_TRUE;
  } else {
    return AS_FALSE;
  }
}

const MlMoveDef mv_TILTTURN = {"TILTTURN", mv_init, mv_main, mv_interrupt, 0};
