// DAMAGEN2.c <- src/characters/shared/moves/DAMAGEN2.js (M2 task 7)
#include "../moves.h"

static AsTri mv_init(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "DAMAGEN2");
  pl->timer = 0;
  pl->phys.grabbing = -1;
  pl->phys.grabbedBy = -1;
  pl->phys.fastfalled = false;
  pl->rotation = 0;
  pl->rotationPoint = vec2d(0, 0);
  pl->colourOverlayBool = false;
  mv_turnOffHitboxes(S, p);
  mv_dispatch(S, MV_CS(S, p), "DAMAGEN2", "main", p, in, 0);
  return AS_UNDEF;
}

static AsTri mv_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  const ml_attributes_t *at = mv_attr(MV_CS(S, p));
  if (pl->inCSS) {
    pl->timer += 0.7;
  } else {
    pl->timer += 1;
  }
  if (mv_dispatch(S, MV_CS(S, p), "DAMAGEN2", "interrupt", p, in, 0) !=
      AS_TRUE) {
    if (pl->timer > 1) {
      pl->hit.hitstun -= 1;
      if (!pl->phys.grounded) {
        pl->phys.cVel.y -= ml_f64(at->gravity);
        if (pl->phys.cVel.y < -ml_f64(at->terminalV)) {
          pl->phys.cVel.y = -ml_f64(at->terminalV);
        }
      } else {
        as_reduceByTraction(false, (int)MV_CS(S, p), &pl->phys.cVel.x);
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
  AsPair b;
  if (pl->timer > mv_frames(MV_CS(S, p), "DAMAGEN2")) {
    if (pl->hit.hitstun > 0) {
      pl->timer -= 1;
      return AS_FALSE;
    } else {
      if (pl->phys.grounded || pl->inCSS) {
        mv_dispatch(S, MV_CS(S, p), "WAIT", "init", p, in, 0);
      } else {
        mv_dispatch(S, MV_CS(S, p), "FALL", "init", p, in, 0);
      }
      return AS_TRUE;
    }
  } else if (pl->hit.hitstun <= 0 && !pl->inCSS) {
    if (pl->phys.grounded) {
      b = as_checkForSpecials(&pl->phys.face, pl->phys.bTurnaroundTimer,
                              pl->phys.bTurnaroundDirection,
                              pl->phys.grounded, MV_IN(in, p));
      const AsPair t = as_checkForTilts(pl->phys.face, MV_IN(in, p), false,
                                        0);
      const AsPair s = as_checkForSmashes(&pl->phys.face, MV_IN(in, p));
      const AsPair j = as_checkForJump(S->tapJumpOff[(int)p], MV_IN(in, p));
      if (j.flag) {
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
        pl->phys.dashbuffer = as_tiltTurnDashBuffer(pl->phys.face,
                                                    MV_IN(in, p));
        mv_dispatch(S, MV_CS(S, p), "TILTTURN", "init", p, in, 0);
        return AS_TRUE;
      } else if (js_abs(i0->lsX) > 0.3) {
        MvX x = mvx_bool(true);
        mv_dispatch(S, MV_CS(S, p), "WALK", "init", p, in, &x);
        return AS_TRUE;
      } else {
        return AS_FALSE;
      }
    } else {
      const AsPair a = as_checkForAerials(pl->phys.face, MV_IN(in, p));
      b = as_checkForSpecials(&pl->phys.face, pl->phys.bTurnaroundTimer,
                              pl->phys.bTurnaroundDirection,
                              pl->phys.grounded, MV_IN(in, p));
      if (a.flag) {
        mv_dispatch(S, MV_CS(S, p), mv_pair_str(&a), "init", p, in, 0);
        return AS_TRUE;
      } else if ((i0->l && !i1->l) || (i0->r && !i1->r)) {
        mv_dispatch(S, MV_CS(S, p), "ESCAPEAIR", "init", p, in, 0);
        return AS_TRUE;
      } else if (as_checkForDoubleJump(S->tapJumpOff[(int)p],
                                       MV_IN(in, p)) &&
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
      } else if ((i0->lsX > 0.7 && i1->lsX < 0.7) ||
                 (i0->lsX < -0.7 && i1->lsX > -0.7) ||
                 (i0->lsY > 0.7 && i1->lsY < 0.7) ||
                 (i0->lsY < -0.7 && i1->lsY > -0.7)) {
        mv_dispatch(S, MV_CS(S, p), "FALL", "init", p, in, 0);
        return AS_TRUE;
      } else {
        return AS_FALSE;
      }
    }
  } else {
    return AS_FALSE;
  }
}

static AsTri mv_land(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  if (pl->hit.hitstun <= 0) {
    mv_dispatch(S, MV_CS(S, p), "LANDING", "init", p, in, 0);
  }
  return AS_UNDEF;
}

const MlMoveDef mv_DAMAGEN2 = {"DAMAGEN2", mv_init, mv_main, mv_interrupt,
                               mv_land};
