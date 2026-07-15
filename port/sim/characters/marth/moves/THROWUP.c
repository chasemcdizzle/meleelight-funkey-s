// THROWUP.c <- src/characters/marth/moves/THROWUP.js (M2 task 11).
// Victim dispatch crosses the actionStates TABLE 2-ARG (marth passes
// input, unlike fox's 1-arg THROWBACK/THROWDOWN sites); hitQueue.push
// crosses mv_hq_push6. NOTE the interrupt's grabbing===-1 arm returns
// FALSE here (the other three marth throws fall through undefined).
#include "../moves.h"

static AsTri mr_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex);
static AsTri mr_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex);

static AsTri mr_init(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "THROWUP");
  pl->timer = 0;
  const double grabbing = pl->phys.grabbing;
  if (grabbing == -1) {
    return AS_UNDEF;
  }
  mv_dispatch(S, MV_CS(S, grabbing), "THROWNMARTHUP", "init", grabbing, in,
              0);
  mv_turnOffHitboxes(S, p);
  mv_assign_hitbox_id(S, p, "throwup", 0, 0);
  const double frame = mv_frames(MV_CS(S, grabbing), "THROWNMARTHUP");
  pl->phys.releaseFrame = frame + 1;
  as_randomShout(MV_CS(S, p));
  mr_main(S, p, in, 0);
  return AS_UNDEF;
}

static AsTri mr_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  const double prevFrame = pl->timer;
  pl->timer += 12 / pl->phys.releaseFrame;
  if (mr_interrupt(S, p, in, 0) != AS_TRUE) {
    if (floor(pl->timer + 0.01) >= 12 && floor(prevFrame + 0.01) < 12) {
      mv_hq_push6(S, pl->phys.grabbing, p, 0, false, true, false);
      mv_turnOffHitboxes(S, p);
    }
  }
  return AS_UNDEF;
}

static AsTri mr_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  if (pl->timer > 39) {
    pl->phys.grabbing = -1;
    mv_WAIT.init(S, p, in, 0);
    return AS_TRUE;
  } else {
    const double grabbing = pl->phys.grabbing;
    if (grabbing == -1) {
      return AS_FALSE;
    }
    if (pl->timer < 11 &&
        mv_player(S, grabbing)->phys.grabbedBy != p) {
      mv_CATCHCUT.init(S, p, in, 0);
      return AS_TRUE;
    } else {
      return AS_FALSE;
    }
  }
}

const MlMoveDef marth_THROWUP = {"THROWUP", mr_init, mr_main, mr_interrupt,
                                 0};
