// OTTOTTO.c <- src/characters/shared/moves/OTTOTTO.js (M2 task 7)
#include "../moves.h"

static AsTri mv_init(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "OTTOTTO");
  pl->timer = 1;
  pl->phys.cVel.x = 0;
  mv_dispatch(S, MV_CS(S, p), "OTTOTTO", "main", p, in, 0);
  return AS_UNDEF;
}

static AsTri mv_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  pl->timer += 1;
  if (mv_dispatch(S, MV_CS(S, p), "OTTOTTO", "interrupt", p, in, 0) !=
      AS_TRUE) {
    // empty body upstream
  }
  return AS_UNDEF;
}

static AsTri mv_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  const MlInput *i0 = &MV_IN(in, p)[0];
  const AsPair b = as_checkForSpecials(&pl->phys.face,
                                       pl->phys.bTurnaroundTimer,
                                       pl->phys.bTurnaroundDirection,
                                       pl->phys.grounded, MV_IN(in, p));
  const AsPair t = as_checkForTilts(pl->phys.face, MV_IN(in, p), false, 0);
  const AsPair s = as_checkForSmashes(&pl->phys.face, MV_IN(in, p));
  const AsPair j = as_checkForJump(S->tapJumpOff[(int)p], MV_IN(in, p));
  if (pl->timer > mv_frames(MV_CS(S, p), "OTTOTTO")) {
    mv_dispatch(S, MV_CS(S, p), "OTTOTTOWAIT", "init", p, in, 0);
    return AS_TRUE;
  } else if (j.flag) {
    MvX x = mvx_pair_payload(&j);
    mv_dispatch(S, MV_CS(S, p), "KNEEBEND", "init", p, in, &x);
    return AS_TRUE;
  } else if (i0->l || i0->r) {
    mv_dispatch(S, MV_CS(S, p), "GUARDON", "init", p, in, 0);
    return AS_TRUE;
  } else if (i0->lA > 0 || i0->rA > 0) {
    // upstream arm has NO return: falls out of the chain -> undefined
    mv_dispatch(S, MV_CS(S, p), "GUARDON", "init", p, in, 0);
    return AS_UNDEF;
  } else if (b.flag) {
    mv_dispatch(S, MV_CS(S, p), mv_pair_str(&b), "init", p, in, 0);
    return AS_TRUE;
  } else if (s.flag) {
    mv_dispatch(S, MV_CS(S, p), mv_pair_str(&s), "init", p, in, 0);
    return AS_TRUE;
  } else if (t.flag) {
    mv_dispatch(S, MV_CS(S, p), mv_pair_str(&t), "init", p, in, 0);
    return AS_TRUE;
  } else if (i0->du) {
    mv_dispatch(S, MV_CS(S, p), "APPEAL", "init", p, in, 0);
    return AS_TRUE;
  } else if (as_checkForSquat(MV_IN(in, p))) {
    mv_dispatch(S, MV_CS(S, p), "SQUAT", "init", p, in, 0);
    return AS_TRUE;
  } else if (as_checkForDash(pl->phys.face, MV_IN(in, p))) {
    mv_dispatch(S, MV_CS(S, p), "DASH", "init", p, in, 0);
    return AS_TRUE;
  } else if (as_checkForSmashTurn(pl->phys.face, MV_IN(in, p))) {
    mv_dispatch(S, MV_CS(S, p), "SMASHTURN", "init", p, in, 0);
    return AS_TRUE;
  } else if (as_checkForTiltTurn(pl->phys.face, MV_IN(in, p))) {
    pl->phys.dashbuffer = as_tiltTurnDashBuffer(pl->phys.face, MV_IN(in, p));
    mv_dispatch(S, MV_CS(S, p), "TILTTURN", "init", p, in, 0);
    return AS_TRUE;
  } else if (js_abs(i0->lsX) > 0.6) {
    MvX x = mvx_bool(true);
    mv_dispatch(S, MV_CS(S, p), "WALK", "init", p, in, &x);
    return AS_TRUE;
  } else {
    return AS_FALSE;
  }
}

const MlMoveDef mv_OTTOTTO = {"OTTOTTO", mv_init, mv_main, mv_interrupt, 0};
