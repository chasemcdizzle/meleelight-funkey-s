// DOWNDAMAGE.c <- src/characters/shared/moves/DOWNDAMAGE.js (M2 task 7)
#include "../moves.h"

static AsTri mv_init(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "DOWNDAMAGE");
  pl->timer = 0;
  pl->phys.jabReset = true;
  pl->phys.grounded = false;
  mv_dispatch(S, MV_CS(S, p), "DOWNDAMAGE", "main", p, in, 0);
  return AS_UNDEF;
}

static AsTri mv_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  const ml_attributes_t *at = mv_attr(MV_CS(S, p));
  pl->timer += 1;
  if (mv_dispatch(S, MV_CS(S, p), "DOWNDAMAGE", "interrupt", p, in, 0) !=
      AS_TRUE) {
    if (!pl->phys.grounded) {
      pl->phys.cVel.y -= ml_f64(at->gravity);
    } else {
      as_reduceByTraction(true, (int)MV_CS(S, p), &pl->phys.cVel.x);
    }
    if (pl->timer > 1) {
      pl->hit.hitstun -= 1;
    }
  }
  return AS_UNDEF;
}

static AsTri mv_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  if (pl->timer > 13) {
    if (pl->phys.grounded) {
      if (pl->hit.hitstun <= 0) {
        mv_dispatch(S, MV_CS(S, p), "DOWNSTANDN", "init", p, in, 0);
      } else {
        mv_dispatch(S, MV_CS(S, p), "DOWNWAIT", "init", p, in, 0);
      }
    } else {
      mv_dispatch(S, MV_CS(S, p), "FALL", "init", p, in, 0);
    }
    return AS_TRUE;
  } else {
    return AS_FALSE;
  }
}

static AsTri mv_land(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  // upstream land(p, input) has an empty body
  (void)S;
  (void)p;
  (void)in;
  (void)ex;
  return AS_UNDEF;
}

const MlMoveDef mv_DOWNDAMAGE = {"DOWNDAMAGE", mv_init, mv_main, mv_interrupt,
                                 mv_land};
