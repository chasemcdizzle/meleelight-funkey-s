// GUARDOFF.c <- src/characters/shared/moves/GUARDOFF.js (M2 task 7)
#include "../moves.h"

static AsTri mv_init(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "GUARDOFF");
  pl->timer = 0;
  ml_sound_play("shieldoff");
  mv_dispatch(S, MV_CS(S, p), "GUARDOFF", "main", p, in, 0);
  return AS_UNDEF;
}

static AsTri mv_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  pl->timer += 1;
  mv_playSounds(S, "GUARDOFF", p);
  if (mv_dispatch(S, MV_CS(S, p), "GUARDOFF", "interrupt", p, in, 0) !=
      AS_TRUE) {
    as_reduceByTraction(false, (int)MV_CS(S, p), &pl->phys.cVel.x);
  }
  return AS_UNDEF;
}

static AsTri mv_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  const MlInput *i0 = &MV_IN(in, p)[0];
  AsPair s;
  const AsPair j = as_checkForJump(S->tapJumpOff[(int)p], MV_IN(in, p));
  if (j.flag && !pl->inCSS) {
    MvX x = mvx_pair_payload(&j);
    mv_dispatch(S, MV_CS(S, p), "KNEEBEND", "init", p, in, &x);
    return AS_TRUE;
  } else if (pl->timer > mv_frames(MV_CS(S, p), "GUARDOFF")) {
    mv_dispatch(S, MV_CS(S, p), "WAIT", "init", p, in, 0);
    return AS_TRUE;
  } else if (pl->phys.powerShielded) {
    if (!pl->inCSS) {
      const AsPair t = as_checkForTilts(pl->phys.face, MV_IN(in, p), false,
                                        0);
      s = as_checkForSmashes(&pl->phys.face, MV_IN(in, p));
      if (s.flag) {
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
      s = as_checkForSmashes(&pl->phys.face, MV_IN(in, p));
      if (s.flag) {
        mv_dispatch(S, MV_CS(S, p), mv_pair_str(&s), "init", p, in, 0);
        return AS_TRUE;
      } else {
        return AS_FALSE;
      }
    }
  } else {
    return AS_FALSE;
  }
}

const MlMoveDef mv_GUARDOFF = {"GUARDOFF", mv_init, mv_main, mv_interrupt, 0};
