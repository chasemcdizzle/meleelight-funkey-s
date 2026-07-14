// LANDINGFALLSPECIAL.c <- src/characters/shared/moves/LANDINGFALLSPECIAL.js
// (M2 task 7)
#include "../moves.h"

static AsTri mv_init(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "LANDINGFALLSPECIAL");
  pl->timer = 0;
  mv_drawVfx("circleDust"); // 4 seeded draws
  ml_sound_play("land");
  mv_dispatch(S, MV_CS(S, p), "LANDINGFALLSPECIAL", "main", p, in, 0);
  return AS_UNDEF;
}

static AsTri mv_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  pl->timer += pl->phys.landingMultiplier;
  if (mv_dispatch(S, MV_CS(S, p), "LANDINGFALLSPECIAL", "interrupt", p, in,
                  0) != AS_TRUE) {
    as_reduceByTraction(true, (int)MV_CS(S, p), &pl->phys.cVel.x);
  }
  return AS_UNDEF;
}

static AsTri mv_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  if (pl->timer > 30) {
    mv_dispatch(S, MV_CS(S, p), "WAIT", "init", p, in, 0);
    return AS_TRUE;
  } else {
    return AS_FALSE;
  }
}

const MlMoveDef mv_LANDINGFALLSPECIAL = {"LANDINGFALLSPECIAL", mv_init,
                                         mv_main, mv_interrupt, 0};
