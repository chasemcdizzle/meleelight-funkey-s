// THROWBACK.c <- src/characters/falcon/moves/THROWBACK.js (M2 task 10)
// Victim dispatch is 1-arg upstream: .init(grabbing) — no input. NO
// lasers, NO face flip (fox's THROWBACK flips at 10; falcon's does not).
#include "../moves.h"

static AsTri fc4_main(MlSim *S, double p, const MlInputBuffer in[4],
                      const MvX *ex);
static AsTri fc4_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                           const MvX *ex);

static AsTri fc4_init(MlSim *S, double p, const MlInputBuffer in[4],
                      const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "THROWBACK");
  pl->timer = 0;
  const double grabbing = pl->phys.grabbing;
  if (grabbing == -1) {
    return AS_UNDEF;
  }
  mv_dispatch(S, MV_CS(S, grabbing), "THROWNFALCONBACK", "init", grabbing,
              in, 0); // upstream passes no input; THROWN* never reads it
  const double frame = mv_frames(MV_CS(S, grabbing), "THROWNFALCONBACK");
  pl->phys.releaseFrame = frame + 1;
  mv_turnOffHitboxes(S, p);
  mv_assign_hitbox_id(S, p, "throwback", 0, 0);
  as_randomShout(MV_CS(S, p));
  fc4_main(S, p, in, 0);
  return AS_UNDEF;
}

static AsTri fc4_main(MlSim *S, double p, const MlInputBuffer in[4],
                      const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  const double prevFrame = pl->timer;
  pl->timer += 20 / pl->phys.releaseFrame;
  if (fc4_interrupt(S, p, in, 0) != AS_TRUE) {
    if (pl->timer >= 12 && prevFrame < 12) {
      mv_assign_hitbox_id(S, p, "throwbackextra", 0, 0);
      mv_assign_hitbox_id(S, p, "throwbackextra", 1, 1);
      mv_assign_hitbox_id(S, p, "throwbackextra", 2, 2);
      pl->hitboxes.active[0] = true;
      pl->hitboxes.active[1] = true;
      pl->hitboxes.active[2] = true;
      pl->hitboxes.active[3] = false;
      S->aliasHbActive[(int)p] = false; // fresh array upstream
      pl->hitboxes.frame = 0;
    }
    if (pl->timer >= 20 && prevFrame < 20) {
      mv_turnOffHitboxes(S, p);
    }
    if (floor(pl->timer + 0.01) >= 20 && floor(prevFrame + 0.01) < 20) {
      mv_assign_hitbox_id(S, p, "throwback", 0, 0);
      pl->hitboxes.active[0] = true;
      pl->hitboxes.active[1] = false;
      pl->hitboxes.active[2] = false;
      pl->hitboxes.active[3] = false;
      S->aliasHbActive[(int)p] = false; // fresh array upstream
      mv_hq_push6(S, pl->phys.grabbing, p, 0, false, true, false);
      mv_turnOffHitboxes(S, p);
    }
  }
  return AS_UNDEF;
}

static AsTri fc4_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                           const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  if (pl->timer > 49) {
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

const MlMoveDef falcon_THROWBACK = {"THROWBACK", fc4_init, fc4_main,
                                    fc4_interrupt, 0};
