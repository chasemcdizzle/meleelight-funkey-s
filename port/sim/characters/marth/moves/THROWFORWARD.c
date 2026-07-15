// THROWFORWARD.c <- src/characters/marth/moves/THROWFORWARD.js
// (M2 task 11). 2-arg victim dispatch; the interrupt's grabbing===-1 arm
// falls through (AsTri undef, rule 13's family).
#include "../moves.h"

static AsTri mr_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex);
static AsTri mr_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex);

static AsTri mr_init(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "THROWFORWARD");
  pl->timer = 0;
  const double grabbing = pl->phys.grabbing;
  if (grabbing == -1) {
    return AS_UNDEF;
  }
  mv_dispatch(S, MV_CS(S, grabbing), "THROWNMARTHFORWARD", "init", grabbing,
              in, 0);
  mv_turnOffHitboxes(S, p);
  mv_assign_hitbox_id(S, p, "throwforward", 0, 0);
  as_randomShout(MV_CS(S, p));
  const double frame = mv_frames(MV_CS(S, grabbing), "THROWNMARTHFORWARD");
  pl->phys.releaseFrame = frame + 1;
  mr_main(S, p, in, 0);
  return AS_UNDEF;
}

static AsTri mr_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  const double prevFrame = pl->timer;
  pl->timer += 13 / pl->phys.releaseFrame;
  if (mr_interrupt(S, p, in, 0) != AS_TRUE) {
    if (floor(pl->timer + 0.01) >= 13 && floor(prevFrame + 0.01) < 13) {
      mv_hq_push6(S, pl->phys.grabbing, p, 0, false, true, true);
      mv_turnOffHitboxes(S, p);
    }
  }
  return AS_UNDEF;
}

static AsTri mr_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  if (pl->timer > 27) {
    pl->phys.grabbing = -1;
    mv_WAIT.init(S, p, in, 0);
    return AS_TRUE;
  } else {
    const double grabbing = pl->phys.grabbing;
    if (grabbing == -1) {
      return AS_UNDEF; // upstream: bare `return;`
    }
    if (pl->timer < 13 &&
        mv_player(S, grabbing)->phys.grabbedBy != p) {
      mv_CATCHCUT.init(S, p, in, 0);
      return AS_TRUE;
    } else {
      return AS_FALSE;
    }
  }
}

const MlMoveDef marth_THROWFORWARD = {"THROWFORWARD", mr_init, mr_main,
                                      mr_interrupt, 0};
