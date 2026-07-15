// THROWUP.c <- src/characters/falcon/moves/THROWUP.js (M2 task 10)
// Victim dispatch crosses the actionStates TABLE (mv_dispatch: falcon
// victim = the registered falcon body, other chars = the driver's
// mdispatch seam); 2-arg (with input). hitQueue.push crosses mv_hq_push6.
// NO lasers (fox's THROWUP fires three; falcon's fires none). Arm ORDER
// carried verbatim: the floor>=15 hq arm precedes the >=11 swap arm.
#include "../moves.h"

static AsTri fc4_main(MlSim *S, double p, const MlInputBuffer in[4],
                      const MvX *ex);
static AsTri fc4_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                           const MvX *ex);

static AsTri fc4_init(MlSim *S, double p, const MlInputBuffer in[4],
                      const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "THROWUP");
  pl->timer = 0;
  const double grabbing = pl->phys.grabbing;
  if (grabbing == -1) {
    return AS_UNDEF;
  }
  mv_dispatch(S, MV_CS(S, grabbing), "THROWNFALCONUP", "init", grabbing, in,
              0);
  mv_turnOffHitboxes(S, p);
  const double frame = mv_frames(MV_CS(S, grabbing), "THROWNFALCONUP");
  pl->phys.releaseFrame = frame + 1;
  mv_assign_hitbox_id(S, p, "throwup", 0, 0);
  fc4_main(S, p, in, 0);
  return AS_UNDEF;
}

static AsTri fc4_main(MlSim *S, double p, const MlInputBuffer in[4],
                      const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  const double prevFrame = pl->timer;
  pl->timer += 15 / pl->phys.releaseFrame;
  if (fc4_interrupt(S, p, in, 0) != AS_TRUE) {
    if (floor(pl->timer + 0.01) >= 15 && floor(prevFrame + 0.01) < 15) {
      mv_assign_hitbox_id(S, p, "throwup", 0, 0);
      pl->hitboxes.active[0] = true;
      pl->hitboxes.active[1] = false;
      pl->hitboxes.active[2] = false;
      pl->hitboxes.active[3] = false;
      S->aliasHbActive[(int)p] = false; // fresh array upstream
      mv_hq_push6(S, pl->phys.grabbing, p, 0, false, true, false);
      mv_turnOffHitboxes(S, p);
    }
    if (pl->timer >= 11 && prevFrame < 11) {
      mv_assign_hitbox_id(S, p, "throwupextra", 0, 0);
      mv_assign_hitbox_id(S, p, "throwupextra", 1, 1);
      mv_assign_hitbox_id(S, p, "throwupextra", 2, 2);
      pl->hitboxes.active[0] = true;
      pl->hitboxes.active[1] = true;
      pl->hitboxes.active[2] = true;
      pl->hitboxes.active[3] = false;
      S->aliasHbActive[(int)p] = false; // fresh array upstream
      pl->hitboxes.frame = 0;
    }
  }
  return AS_UNDEF;
}

static AsTri fc4_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                           const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  if (pl->timer > 43) {
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

const MlMoveDef falcon_THROWUP = {"THROWUP", fc4_init, fc4_main,
                                  fc4_interrupt, 0};
