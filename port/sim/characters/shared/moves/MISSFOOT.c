// MISSFOOT.c <- src/characters/shared/moves/MISSFOOT.js (M2 task 7)
#include "../moves.h"

static AsTri mv_init(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "MISSFOOT");
  pl->timer = 0;
  pl->hit.hitstun = 0;
  mv_turnOffHitboxes(S, p);
  mv_dispatch(S, MV_CS(S, p), "MISSFOOT", "main", p, in, 0);
  return AS_UNDEF;
}

static AsTri mv_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  pl->timer += 1;
  if (mv_dispatch(S, MV_CS(S, p), "MISSFOOT", "interrupt", p, in, 0) !=
      AS_TRUE) {
    as_fastfall((int)MV_CS(S, p), &pl->phys.cVel.y, &pl->phys.fastfalled,
                MV_IN(in, p));
    as_airDrift((int)MV_CS(S, p), &pl->phys.cVel.x, MV_IN(in, p));
  }
  return AS_UNDEF;
}

static AsTri mv_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  if (pl->timer > 26) {
    mv_dispatch(S, MV_CS(S, p), "DAMAGEFALL", "init", p, in, 0);
    return AS_TRUE;
  } else {
    return AS_FALSE;
  }
}

const MlMoveDef mv_MISSFOOT = {"MISSFOOT", mv_init, mv_main, mv_interrupt, 0};
