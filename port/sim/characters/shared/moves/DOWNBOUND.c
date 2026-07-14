// DOWNBOUND.c <- src/characters/shared/moves/DOWNBOUND.js (M2 task 7)
#include "../moves.h"

static AsTri mv_init(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "DOWNBOUND");
  pl->timer = 0;
  pl->phys.kVel.y = 0;
  pl->phys.jabReset = false;
  mv_drawVfx("groundBounce");
  ml_sound_play("bounce");
  mv_dispatch(S, MV_CS(S, p), "DOWNBOUND", "main", p, in, 0);
  return AS_UNDEF;
}

static AsTri mv_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  pl->timer += 1;
  if (mv_dispatch(S, MV_CS(S, p), "DOWNBOUND", "interrupt", p, in, 0) !=
      AS_TRUE) {
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
  if (pl->timer > mv_frames(MV_CS(S, p), "DOWNBOUND")) {
    mv_dispatch(S, MV_CS(S, p), "DOWNWAIT", "init", p, in, 0);
    return AS_TRUE;
  } else {
    return AS_FALSE;
  }
}

const MlMoveDef mv_DOWNBOUND = {"DOWNBOUND", mv_init, mv_main, mv_interrupt,
                                0};
