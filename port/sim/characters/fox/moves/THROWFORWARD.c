// THROWFORWARD.c <- src/characters/fox/moves/THROWFORWARD.js (M2 task 8)
// this.setVelocities comes from the mvData fox dump (rule 15).
#include "../moves.h"

static AsTri fx_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex);
static AsTri fx_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex);

static AsTri fx_init(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "THROWFORWARD");
  pl->timer = 0;
  const double grabbing = pl->phys.grabbing;
  if (grabbing == -1) {
    return AS_UNDEF;
  }
  mv_dispatch(S, MV_CS(S, grabbing), "THROWNFOXFORWARD", "init", grabbing,
              in, 0);
  const double frame = mv_frames(MV_CS(S, grabbing), "THROWNFOXFORWARD");
  pl->phys.releaseFrame = frame + 1;
  mv_turnOffHitboxes(S, p);
  mv_assign_hitbox_id(S, p, "throwforward", 0, 0);
  as_randomShout(MV_CS(S, p));
  fx_main(S, p, in, 0);
  return AS_UNDEF;
}

static AsTri fx_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  const double prevFrame = pl->timer;
  pl->timer += 11 / pl->phys.releaseFrame;
  if (fx_interrupt(S, p, in, 0) != AS_TRUE) {
    pl->phys.cVel.x =
        mv_fox_arr("THROWFORWARD", "setVelocities",
                   floor(pl->timer + 0.01) - 1) * pl->phys.face;
    if (floor(pl->timer + 0.01) >= 11 && floor(prevFrame + 0.01) < 11) {
      mv_hq_push6(S, pl->phys.grabbing, p, 0, false, true, false);
      mv_turnOffHitboxes(S, p);
    }
  }
  return AS_UNDEF;
}

static AsTri fx_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  if (pl->timer > 33) {
    pl->phys.grabbing = -1;
    mv_WAIT.init(S, p, in, 0);
    return AS_TRUE;
  } else {
    const double grabbing = pl->phys.grabbing;
    if (grabbing == -1) {
      return AS_UNDEF;
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

const MlMoveDef fox_THROWFORWARD = {"THROWFORWARD", fx_init, fx_main,
                                    fx_interrupt, 0};
