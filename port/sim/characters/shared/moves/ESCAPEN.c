// ESCAPEN.c <- src/characters/shared/moves/ESCAPEN.js (M2 task 7)
#include "../moves.h"

static AsTri mv_init(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "ESCAPEN");
  pl->timer = 0;
  pl->phys.shielding = false;
  ml_drawVfx("circleDust", pl->phys.pos.x, pl->phys.pos.y,
             pl->phys.face); // 4 seeded draws
  mv_dispatch(S, MV_CS(S, p), "ESCAPEN", "main", p, in, 0);
  return AS_UNDEF;
}

static AsTri mv_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  pl->timer += 1;
  mv_playSounds(S, "ESCAPEN", p);
  if (mv_dispatch(S, MV_CS(S, p), "ESCAPEN", "interrupt", p, in, 0) !=
      AS_TRUE) {
    if (pl->timer == 1) {
      ml_sound_play("spotdodge");
    }
    as_reduceByTraction(true, (int)MV_CS(S, p), &pl->phys.cVel.x);
    as_executeIntangibility("ESCAPEN", (int)MV_CS(S, p), pl->timer,
                            &pl->phys.intangibleTimer, &pl->phys.hurtBoxState);
  }
  return AS_UNDEF;
}

static AsTri mv_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  if (pl->timer > mv_frames(MV_CS(S, p), "ESCAPEN")) {
    mv_dispatch(S, MV_CS(S, p), "WAIT", "init", p, in, 0);
    return AS_TRUE;
  } else {
    return AS_FALSE;
  }
}

const MlMoveDef mv_ESCAPEN = {"ESCAPEN", mv_init, mv_main, mv_interrupt, 0};
