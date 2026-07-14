// WALK.c <- src/characters/shared/moves/WALK.js (M2 task 7)
#include "../moves.h"

static AsTri mv_init(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  // init(p, addInitV, input)
  MlPlayer *pl = mv_player(S, p);
  const ml_attributes_t *at = mv_attr(MV_CS(S, p));
  strcpy(pl->actionState, "WALK");
  pl->timer = 1;
  if (mvx_truthy(ex, 0)) {
    const double tempInit = ml_f64(at->walkInitV) * pl->phys.face;
    if ((tempInit > 0 && pl->phys.cVel.x < tempInit) ||
        (tempInit < 0 && pl->phys.cVel.x > tempInit)) {
      pl->phys.cVel.x += ml_f64(at->walkInitV) * pl->phys.face;
    }
  }
  mv_dispatch(S, MV_CS(S, p), "WALK", "main", p, in, 0);
  return AS_UNDEF;
}

static AsTri mv_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  const ml_attributes_t *at = mv_attr(MV_CS(S, p));
  const MlInput *i0 = &MV_IN(in, p)[0];
  if (mv_dispatch(S, MV_CS(S, p), "WALK", "interrupt", p, in, 0) != AS_TRUE) {
    bool footstep0 = false;
    bool footstep1 = false;
    if (pl->timer < 5) footstep0 = true;
    if (pl->timer < 15) footstep1 = true;

    const double tempMax = ml_f64(at->walkMaxV) * i0->lsX;

    if (js_abs(pl->phys.cVel.x) > js_abs(tempMax)) {
      as_reduceByTraction(true, (int)MV_CS(S, p), &pl->phys.cVel.x);
    } else {
      const double tempAcc = (tempMax - pl->phys.cVel.x) *
                             (1 / (ml_f64(at->walkMaxV) * 2)) *
                             (ml_f64(at->walkInitV) + ml_f64(at->walkAcc));

      pl->phys.cVel.x += tempAcc;
      if (pl->phys.cVel.x * pl->phys.face > tempMax * pl->phys.face) {
        pl->phys.cVel.x = tempMax;
      }
    }

    const double time = ((pl->phys.cVel.x * pl->phys.face) /
                         ml_f64(at->walkMaxV)) * ml_f64(at->walkAnimSpeed);
    if (time > 0) {
      pl->timer += time;
    }
    if ((footstep0 && pl->timer >= 5) || (footstep1 && pl->timer >= 15)) {
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
  const AsPair b = as_checkForSpecials(&pl->phys.face,
                                       pl->phys.bTurnaroundTimer,
                                       pl->phys.bTurnaroundDirection,
                                       pl->phys.grounded, MV_IN(in, p));
  const AsPair t = as_checkForTilts(pl->phys.face, MV_IN(in, p), false, 0);
  const AsPair s = as_checkForSmashes(&pl->phys.face, MV_IN(in, p));
  const AsPair j = as_checkForJump(S->tapJumpOff[(int)p], MV_IN(in, p));
  if (pl->timer > mv_frames(MV_CS(S, p), "WALK")) {
    MvX x = mvx_bool(false);
    mv_dispatch(S, MV_CS(S, p), "WALK", "init", p, in, &x);
    return AS_TRUE;
  }
  if (i0->lsX == 0) {
    mv_dispatch(S, MV_CS(S, p), "WAIT", "init", p, in, 0);
    return AS_TRUE;
  } else if (j.flag) {
    MvX x = mvx_pair_payload(&j);
    mv_dispatch(S, MV_CS(S, p), "KNEEBEND", "init", p, in, &x);
    return AS_TRUE;
  } else if (i0->l || i0->r) {
    mv_dispatch(S, MV_CS(S, p), "GUARDON", "init", p, in, 0);
    return AS_TRUE;
  } else if (i0->lA > 0 || i0->rA > 0) {
    mv_dispatch(S, MV_CS(S, p), "GUARDON", "init", p, in, 0);
    return AS_TRUE;
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
  } else {
    return AS_FALSE;
  }
}

const MlMoveDef mv_WALK = {"WALK", mv_init, mv_main, mv_interrupt, 0};
