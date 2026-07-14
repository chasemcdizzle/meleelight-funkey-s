// SHIELDBREAKFALL.c <- src/characters/shared/moves/SHIELDBREAKFALL.js
// (M2 task 7)
#include "../moves.h"

static AsTri mv_init(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "SHIELDBREAKFALL");
  pl->timer = 0;
  mv_dispatch(S, MV_CS(S, p), "SHIELDBREAKFALL", "main", p, in, 0);
  return AS_UNDEF;
}

static AsTri mv_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  const ml_attributes_t *at = mv_attr(MV_CS(S, p));
  pl->timer += 1;
  if (mv_dispatch(S, MV_CS(S, p), "SHIELDBREAKFALL", "interrupt", p, in, 0) !=
      AS_TRUE) {
    pl->phys.intangibleTimer = 1;
    pl->phys.cVel.y -= ml_f64(at->gravity);
  }
  return AS_UNDEF;
}

static AsTri mv_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  if (pl->timer > mv_frames(MV_CS(S, p), "SHIELDBREAKFALL")) {
    mv_dispatch(S, MV_CS(S, p), "SHIELDBREAKFALL", "init", p, in, 0);
    return AS_TRUE;
  } else {
    return AS_FALSE;
  }
}

static AsTri mv_land(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  // land(p, normal, input) — forwards normal to SHIELDBREAKDOWNBOUND.init
  MvX x;
  if (ex != 0 && ex->count >= 1) {
    x = *ex;
    x.count = 1;
  } else {
    mv_out_of_domain("SHIELDBREAKFALL.land: missing normal arg");
    return AS_UNDEF;
  }
  mv_dispatch(S, MV_CS(S, p), "SHIELDBREAKDOWNBOUND", "init", p, in, &x);
  return AS_UNDEF;
}

const MlMoveDef mv_SHIELDBREAKFALL = {"SHIELDBREAKFALL", mv_init, mv_main,
                                      mv_interrupt, mv_land};
