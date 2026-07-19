// GUARD.c <- src/characters/shared/moves/GUARD.js (M2 task 7)
#include "../moves.h"

static AsTri mv_init(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "GUARD");
  pl->timer = 0;
  pl->phys.powerShieldActive = false;
  pl->phys.powerShieldReflectActive = false;
  mv_dispatch(S, MV_CS(S, p), "GUARD", "main", p, in, 0);
  return AS_UNDEF;
}

static void shield_tilt(MlSim *S, double p, bool shieldstun,
                        const MlInputBuffer in[4]) {
  MlPlayer *pl = mv_player(S, p);
  AsShieldTiltState st;
  st.face = pl->phys.face;
  st.inCSS = pl->inCSS;
  st.pos = pl->phys.pos;
  st.shieldPosition = pl->phys.shieldPosition;
  st.shieldPositionReal = pl->phys.shieldPositionReal;
  as_shieldTilt(shieldstun, (int)MV_CS(S, p), &st, MV_IN(in, p));
  pl->phys.shieldPosition = st.shieldPosition;
  pl->phys.shieldPositionReal = st.shieldPositionReal;
}

static void shield_depletion(MlSim *S, double p, const MlInputBuffer in[4]) {
  MlPlayer *pl = mv_player(S, p);
  AsShieldDepState st;
  st.grounded = pl->phys.grounded;
  st.kDec = pl->phys.kDec;
  st.kVel = pl->phys.kVel;
  st.shieldHP = pl->phys.shieldHP;
  st.shielding = pl->phys.shielding;
  st.pos = pl->phys.pos;   // M4 task 1: breakShield vfx read set
  st.face = pl->phys.face;
  const bool broke = as_shieldDepletion((int)MV_CS(S, p), &st, MV_IN(in, p));
  pl->phys.grounded = st.grounded;
  pl->phys.kDec = st.kDec;
  pl->phys.kVel = st.kVel;
  pl->phys.shieldHP = st.shieldHP;
  pl->phys.shielding = st.shielding;
  if (broke) {
    // upstream runs SHIELDBREAKFALL.init(p, input) INSIDE shieldDepletion
    // (actionStateShortcuts.js:297) — the slice API returns the arm flag
    // and the real dispatch happens here, after the slice write-back
    // (M4 task 6; as_shieldDepletion header note).
    mv_dispatch(S, MV_CS(S, p), "SHIELDBREAKFALL", "init", p, in, 0);
  }
}

static AsTri mv_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  if (pl->hit.shieldstun > 0) {
    as_reduceByTraction(false, (int)MV_CS(S, p), &pl->phys.cVel.x);
    shield_tilt(S, p, true, in);
  } else {
    pl->timer += 1;
    if (mv_dispatch(S, MV_CS(S, p), "GUARD", "interrupt", p, in, 0) !=
        AS_TRUE) {
      if (!pl->inCSS) {
        as_reduceByTraction(false, (int)MV_CS(S, p), &pl->phys.cVel.x);
        shield_depletion(S, p, in);
      }
      shield_tilt(S, p, false, in);
      {
        AsShieldSizeOut out;
        as_shieldSize(false, (int)MV_CS(S, p), pl->phys.shieldHP, &out,
                      MV_IN(in, p));
        pl->phys.shieldAnalog = out.shieldAnalog;
        pl->phys.shieldSize = out.shieldSize;
      }
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
  if (!pl->inCSS) {
    const AsPair j = as_checkForJump(S->tapJumpOff[(int)p], MV_IN(in, p));
    if (j.flag || i0->csY > 0.66) {
      pl->phys.shielding = false;
      MvX x = mvx_pair_payload(&j);
      mv_dispatch(S, MV_CS(S, p), "KNEEBEND", "init", p, in, &x);
      return AS_TRUE;
    } else if (i0->a && !i1->a) {
      pl->phys.shielding = false;
      mv_dispatch(S, MV_CS(S, p), "GRAB", "init", p, in, 0);
      return AS_TRUE;
    } else if ((i0->lsY < -0.7 && MV_IN(in, p)[4].lsY > -0.3) ||
               i0->csY < -0.7) {
      pl->phys.shielding = false;
      mv_dispatch(S, MV_CS(S, p), "ESCAPEN", "init", p, in, 0);
      return AS_TRUE;
    } else if ((i0->lsX * pl->phys.face > 0.7 &&
                MV_IN(in, p)[4].lsX * pl->phys.face < 0.3) ||
               i0->csX * pl->phys.face > 0.7) {
      pl->phys.shielding = false;
      mv_dispatch(S, MV_CS(S, p), "ESCAPEF", "init", p, in, 0);
      return AS_TRUE;
    } else if ((i0->lsX * pl->phys.face < -0.7 &&
                MV_IN(in, p)[4].lsX * pl->phys.face > -0.3) ||
               i0->csX * pl->phys.face < -0.7) {
      pl->phys.shielding = false;
      mv_dispatch(S, MV_CS(S, p), "ESCAPEB", "init", p, in, 0);
      return AS_TRUE;
    } else if (i0->lsY < -0.65 && MV_IN(in, p)[6].lsY > -0.3 &&
               pl->phys.onSurface[0] == 1) {
      pl->phys.shielding = false;
      mv_dispatch(S, MV_CS(S, p), "PASS", "init", p, in, 0);
      return AS_TRUE;
    } else if (i0->lA < 0.3 && i0->rA < 0.3) {
      pl->phys.shielding = false;
      mv_dispatch(S, MV_CS(S, p), "GUARDOFF", "init", p, in, 0);
      return AS_TRUE;
    } else if (pl->timer > 1) {
      mv_dispatch(S, MV_CS(S, p), "GUARD", "init", p, in, 0);
      return AS_TRUE;
    } else {
      return AS_FALSE;
    }
  } else {
    if (i0->lA < 0.3 && i0->rA < 0.3) {
      pl->phys.shielding = false;
      mv_dispatch(S, MV_CS(S, p), "GUARDOFF", "init", p, in, 0);
      return AS_TRUE;
    } else if (pl->timer > 1) {
      mv_dispatch(S, MV_CS(S, p), "GUARD", "init", p, in, 0);
      return AS_TRUE;
    } else {
      return AS_FALSE;
    }
  }
}

const MlMoveDef mv_GUARD = {"GUARD", mv_init, mv_main, mv_interrupt, 0};
