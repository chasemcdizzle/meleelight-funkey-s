// SHIELDBREAKDOWNBOUND.c <-
// src/characters/shared/moves/SHIELDBREAKDOWNBOUND.js (M2 task 7)
#include "../moves.h"

static AsTri mv_init(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  // init(p, normal, input) — normal feeds the groundBounce vfx `f` only
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "SHIELDBREAKDOWNBOUND");
  pl->timer = 0;
  pl->phys.cVel.y = 0;
  pl->phys.kVel.y = 0;
  mv_drawVfx("groundBounce");
  ml_sound_play("bounce");
  mv_dispatch(S, MV_CS(S, p), "SHIELDBREAKDOWNBOUND", "main", p, in, 0);
  return AS_UNDEF;
}

static AsTri mv_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  pl->timer += 1;
  if (mv_dispatch(S, MV_CS(S, p), "SHIELDBREAKDOWNBOUND", "interrupt", p, in,
                  0) != AS_TRUE) {
    pl->phys.intangibleTimer = 1;
    if (pl->timer == 1) {
      as_reduceByTraction(true, (int)MV_CS(S, p), &pl->phys.cVel.x);
    } else {
      pl->phys.cVel.x = 0;
    }
  }
  return AS_UNDEF;
}

static AsTri mv_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  if (pl->timer > mv_frames(MV_CS(S, p), "SHIELDBREAKDOWNBOUND")) {
    mv_dispatch(S, MV_CS(S, p), "SHIELDBREAKSTAND", "init", p, in, 0);
    return AS_TRUE;
  } else {
    return AS_FALSE;
  }
}

const MlMoveDef mv_SHIELDBREAKDOWNBOUND = {"SHIELDBREAKDOWNBOUND", mv_init,
                                           mv_main, mv_interrupt, 0};
