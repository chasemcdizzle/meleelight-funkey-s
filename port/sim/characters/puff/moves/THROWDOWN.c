// THROWDOWN.c <- src/characters/puff/moves/THROWDOWN.js (M2 task 12)
// Multi-hit on timer%13 (fractional fmod, t<51); the crossing arm (>=61)
// swaps to throwdown.id0 with NO grabbing===-1 guard (verbatim — unlike
// THROWFORWARD/THROWBACK).
#include "../moves.h"

static AsTri pf_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex);
static AsTri pf_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex);

static AsTri pf_init(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "THROWDOWN");
  pl->timer = 0;
  const double grabbing = pl->phys.grabbing;
  if (grabbing == -1) {
    return AS_UNDEF;
  }
  mv_dispatch(S, MV_CS(S, grabbing), "THROWNPUFFDOWN", "init", grabbing, in,
              0);
  const double frame = mv_frames(MV_CS(S, grabbing), "THROWNPUFFDOWN");
  pl->phys.releaseFrame = frame + 1;
  mv_turnOffHitboxes(S, p);
  pf_assign_hitbox_id(S, p, "throwdownextra", 0, 0);
  as_randomShout(MV_CS(S, p));
  pf_main(S, p, in, 0);
  return AS_UNDEF;
}

static AsTri pf_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  const double prevFrame = pl->timer;
  pl->timer += 61 / pl->phys.releaseFrame;
  if (pf_interrupt(S, p, in, 0) != AS_TRUE) {
    //10,23,36,49 (upstream comment)
    if (pl->timer < 51) {
      if (fmod(pl->timer, 13) == 10) {
        pl->hitboxes.active[0] = true;
        pl->hitboxes.active[1] = false;
        pl->hitboxes.active[2] = false;
        pl->hitboxes.active[3] = false;
        S->aliasHbActive[(int)p] = false; // fresh array upstream
        pl->hitboxes.frame = 0;
      }
      if (fmod(pl->timer, 13) == 11) {
        mv_turnOffHitboxes(S, p);
      }
    }
    if (floor(pl->timer + 0.01) >= 61 && prevFrame < 61) {
      pf_assign_hitbox_id(S, p, "throwdown", 0, 0);
      mv_hq_push6(S, pl->phys.grabbing, p, 0, false, true, true);
      mv_turnOffHitboxes(S, p);
    }
  }
  return AS_UNDEF;
}

static AsTri pf_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  if (pl->timer > 84) {
    pl->phys.grabbing = -1;
    mv_WAIT.init(S, p, in, 0);
    return AS_TRUE;
  } else {
    const double grabbing = pl->phys.grabbing;
    if (grabbing == -1) {
      return AS_UNDEF; // upstream bare-return arm (rule 13)
    }
    if (pl->timer < pl->phys.releaseFrame &&
        mv_player(S, grabbing)->phys.grabbedBy != p) {
      mv_CATCHCUT.init(S, p, in, 0);
      return AS_TRUE;
    } else {
      return AS_FALSE;
    }
  }
}

const MlMoveDef puff_THROWDOWN = {"THROWDOWN", pf_init, pf_main,
                                  pf_interrupt, 0};
