// THROWFORWARD.c <- src/characters/falco/moves/THROWFORWARD.js (M2 task 9)
// See THROWUP.c. The victim dispatch is 2-arg (with input); NOTE falco's
// init has NO grabbing===-1 guard and main's setVelocities index is
// Math.max(0, floor(timer+0.01)-1)-clamped (fox's is unclamped).
// this.setVelocities comes from the mvData falco dump.
#include "../moves.h"

static AsTri fc_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex);
static AsTri fc_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex);

static AsTri fc_init(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "THROWFORWARD");
  pl->timer = 0;
  const double grabbing = pl->phys.grabbing;
  mv_dispatch(S, MV_CS(S, grabbing), "THROWNFALCOFORWARD", "init", grabbing,
              in, 0);
  const double frame = mv_frames(MV_CS(S, grabbing), "THROWNFALCOFORWARD");
  pl->phys.releaseFrame = frame + 1;
  mv_turnOffHitboxes(S, p);
  mv_assign_hitbox_id(S, p, "throwforward", 0, 0);
  as_randomShout(MV_CS(S, p));
  fc_main(S, p, in, 0);
  return AS_UNDEF;
}

static AsTri fc_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  const double prevFrame = pl->timer;
  pl->timer += 11 / pl->phys.releaseFrame;
  if (fc_interrupt(S, p, in, 0) != AS_TRUE) {
    pl->phys.cVel.x =
        mv_falco_arr("THROWFORWARD", "setVelocities",
                     js_max(0, floor(pl->timer + 0.01) - 1)) *
        pl->phys.face;
    if (floor(pl->timer + 0.01) >= 11 && floor(prevFrame + 0.01) < 11) {
      mv_hq_push6(S, pl->phys.grabbing, p, 0, false, true, false);
      mv_turnOffHitboxes(S, p);
    }
  }
  return AS_UNDEF;
}

static AsTri fc_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
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
      return AS_UNDEF; // upstream: bare `return;`
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

const MlMoveDef falco_THROWFORWARD = {"THROWFORWARD", fc_init, fc_main,
                                      fc_interrupt, 0};
