// ENTRANCE.c <- src/characters/shared/moves/ENTRANCE.js (M2 task 7)
#include "../moves.h"

static AsTri mv_init(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "ENTRANCE");
  pl->timer = 0;
  pl->phys.grounded = false;
  mv_dispatch(S, MV_CS(S, p), "ENTRANCE", "main", p, in, 0);
  return AS_UNDEF;
}

static AsTri mv_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  pl->timer += 1;
  // upstream discards the interrupt result (no `if (!...)`)
  mv_dispatch(S, MV_CS(S, p), "ENTRANCE", "interrupt", p, in, 0);
  return AS_UNDEF;
}

static AsTri mv_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  if (pl->timer > 60) {
    // upstream has NO return here: always returns undefined
    mv_dispatch(S, MV_CS(S, p), "FALL", "init", p, in, 0);
  }
  return AS_UNDEF;
}

const MlMoveDef mv_ENTRANCE = {"ENTRANCE", mv_init, mv_main, mv_interrupt, 0};
