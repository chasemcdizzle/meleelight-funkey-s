// ESCAPEF.c <- src/characters/shared/moves/ESCAPEF.js (M2 task 7)
#include "../moves.h"

static AsTri mv_init(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "ESCAPEF");
  pl->timer = 0;
  pl->phys.shielding = false;
  mv_dispatch(S, MV_CS(S, p), "ESCAPEF", "main", p, in, 0);
  return AS_UNDEF;
}

static AsTri mv_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  pl->timer += 1;
  mv_playSounds(S, "ESCAPEF", p);
  if (mv_dispatch(S, MV_CS(S, p), "ESCAPEF", "interrupt", p, in, 0) !=
      AS_TRUE) {
    pl->phys.cVel.x = mv_setVelocity(MV_CS(S, p), "ESCAPEF", pl->timer - 1) *
                      pl->phys.face;
    as_executeIntangibility("ESCAPEF", (int)MV_CS(S, p), pl->timer,
                            &pl->phys.intangibleTimer, &pl->phys.hurtBoxState);
    if (pl->timer == 4) {
      ml_sound_play("roll");
    }
  }
  return AS_UNDEF;
}

static AsTri mv_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  if (pl->timer > mv_frames(MV_CS(S, p), "ESCAPEF")) {
    pl->phys.cVel.x = 0;
    pl->phys.face *= -1;
    mv_dispatch(S, MV_CS(S, p), "WAIT", "init", p, in, 0);
    return AS_TRUE;
  } else {
    return AS_FALSE;
  }
}

const MlMoveDef mv_ESCAPEF = {"ESCAPEF", mv_init, mv_main, mv_interrupt, 0};
