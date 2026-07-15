// THROWFORWARD.c <- src/characters/puff/moves/THROWFORWARD.js (M2 task 12)
// Fractional timer (12/releaseFrame); the crossing arm's grabbing===-1
// bare return SKIPS the t===11/12 hitbox arms below it (verbatim).
#include "../moves.h"

static AsTri pf_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex);
static AsTri pf_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex);

static AsTri pf_init(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "THROWFORWARD");
  pl->timer = 0;
  const double grabbing = pl->phys.grabbing;
  if (grabbing == -1) {
    return AS_UNDEF;
  }
  mv_dispatch(S, MV_CS(S, grabbing), "THROWNPUFFFORWARD", "init", grabbing,
              in, 0);
  const double frame = mv_frames(MV_CS(S, grabbing), "THROWNPUFFFORWARD");
  pl->phys.releaseFrame = frame + 1;
  mv_turnOffHitboxes(S, p);
  pf_assign_hitbox_id(S, p, "throwforward", 0, 0);
  as_randomShout(MV_CS(S, p));
  pf_main(S, p, in, 0);
  return AS_UNDEF;
}

static AsTri pf_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  const double prevFrame = pl->timer;
  pl->timer += 12 / pl->phys.releaseFrame;
  if (pf_interrupt(S, p, in, 0) != AS_TRUE) {
    if (floor(pl->timer + 0.01) >= 12 && prevFrame < 12) {
      if (pl->phys.grabbing == -1) return AS_UNDEF;
      mv_hq_push6(S, pl->phys.grabbing, p, 0, false, true, true);
      mv_turnOffHitboxes(S, p);
    }
    if (pl->timer == 11) {
      pf_assign_hitbox_id(S, p, "throwforwardextra", 0, 0);
      pl->hitboxes.active[0] = true;
      pl->hitboxes.active[1] = false;
      pl->hitboxes.active[2] = false;
      pl->hitboxes.active[3] = false;
      S->aliasHbActive[(int)p] = false; // fresh array upstream
      pl->hitboxes.frame = 0;
    }
    if (pl->timer == 12) {
      mv_turnOffHitboxes(S, p);
    }
  }
  return AS_UNDEF;
}

static AsTri pf_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  if (pl->timer > 35) {
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

const MlMoveDef puff_THROWFORWARD = {"THROWFORWARD", pf_init, pf_main,
                                     pf_interrupt, 0};
