// THROWBACK.c <- src/characters/puff/moves/THROWBACK.js (M2 task 12)
// setVelocities window carries upstream's floor-over-comparison typo
// (`Math.floor(timer + 0.01 < 37)` — floor of a BOOLEAN, truthy iff the
// comparison holds) — the C keeps the desugared shape (rule 13 family).
#include "../moves.h"

static AsTri pf_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex);
static AsTri pf_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex);

static AsTri pf_init(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "THROWBACK");
  pl->timer = 0;
  const double grabbing = pl->phys.grabbing;
  if (grabbing == -1) {
    return AS_UNDEF;
  }
  mv_dispatch(S, MV_CS(S, grabbing), "THROWNPUFFBACK", "init", grabbing, in,
              0);
  const double frame = mv_frames(MV_CS(S, grabbing), "THROWNPUFFBACK");
  pl->phys.releaseFrame = frame + 1;
  mv_turnOffHitboxes(S, p);
  pf_assign_hitbox_id(S, p, "throwback", 0, 0);
  as_randomShout(MV_CS(S, p));
  pf_main(S, p, in, 0);
  return AS_UNDEF;
}

static AsTri pf_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  const double prevFrame = pl->timer;
  pl->timer += (22 / pl->phys.releaseFrame);
  if (pf_interrupt(S, p, in, 0) != AS_TRUE) {
    // Math.floor(timer + 0.01) > 13 && Math.floor(timer + 0.01 < 37):
    // the second floor's argument is the COMPARISON (upstream typo) —
    // floor(true)=1 / floor(false)=0, truthiness preserved verbatim.
    if (floor(pl->timer + 0.01) > 13 &&
        floor((pl->timer + 0.01 < 37) ? 1.0 : 0.0) != 0.0) {
      pl->phys.cVel.x = mv_puff_arr("THROWBACK", "setVelocities",
                                    floor(pl->timer + 0.01) - 14) *
                        pl->phys.face;
    }
    if (floor(pl->timer + 0.01) >= 22 && prevFrame < 22) {
      if (pl->phys.grabbing == -1) return AS_UNDEF;
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
  if (pl->timer > 43) {
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

const MlMoveDef puff_THROWBACK = {"THROWBACK", pf_init, pf_main,
                                  pf_interrupt, 0};
