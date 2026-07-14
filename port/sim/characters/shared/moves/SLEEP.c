// SLEEP.c <- src/characters/shared/moves/SLEEP.js (M2 task 7)
#include "../moves.h"

static AsTri mv_init(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "SLEEP");
  pl->timer = 0;
  pl->hit.hitstun = 0;
  pl->phys.kVel.y = 0;
  pl->phys.kVel.x = 0;
  pl->phys.cVel.x = 0;
  pl->phys.cVel.y = 0;
  mv_pos_set_x(S, p, 300); // component write through the pos-ECB1 alias
  mv_dispatch(S, MV_CS(S, p), "SLEEP", "main", p, in, 0);
  return AS_UNDEF;
}

static AsTri mv_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  (void)in;
  MlPlayer *pl = mv_player(S, p);
  pl->phys.outOfCameraTimer = 0;
  return AS_UNDEF;
}

static AsTri mv_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex) {
  (void)S;
  (void)p;
  (void)in;
  (void)ex;
  return AS_FALSE;
}

const MlMoveDef mv_SLEEP = {"SLEEP", mv_init, mv_main, mv_interrupt, 0};
