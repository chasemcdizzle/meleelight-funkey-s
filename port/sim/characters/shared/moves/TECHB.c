// TECHB.c <- src/characters/shared/moves/TECHB.js (M2 task 7)
#include "../moves.h"

static AsTri mv_init(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "TECHB");
  pl->timer = 0;
  mv_drawVfx("tech");
  ml_sound_play("tech");
  mv_dispatch(S, MV_CS(S, p), "TECHB", "main", p, in, 0);
  return AS_UNDEF;
}

static AsTri mv_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  pl->timer += 1;
  mv_playSounds(S, "TECH", p);
  if (mv_dispatch(S, MV_CS(S, p), "TECHB", "interrupt", p, in, 0) !=
      AS_TRUE) {
    as_executeIntangibility("TECHB", (int)MV_CS(S, p), pl->timer,
                            &pl->phys.intangibleTimer, &pl->phys.hurtBoxState);
    pl->phys.cVel.x = mv_setVelocity(MV_CS(S, p), "TECHB", pl->timer - 1) *
                      pl->phys.face;
  }
  return AS_UNDEF;
}

static AsTri mv_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  if (pl->timer > mv_frames(MV_CS(S, p), "TECHB")) {
    mv_dispatch(S, MV_CS(S, p), "WAIT", "init", p, in, 0);
    return AS_TRUE;
  } else {
    return AS_FALSE;
  }
}

const MlMoveDef mv_TECHB = {"TECHB", mv_init, mv_main, mv_interrupt, 0};
