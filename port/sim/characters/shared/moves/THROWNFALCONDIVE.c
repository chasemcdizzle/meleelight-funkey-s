// THROWNFALCONDIVE.c <- src/characters/shared/moves/THROWNFALCONDIVE.js
// (M2 task 7)
#include "../moves.h"

static AsTri mv_init(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "THROWNFALCONDIVE");
  pl->phys.cVel.x = 0;
  pl->phys.cVel.y = 0;
  pl->phys.kVel.x = 0;
  pl->phys.kVel.y = 0;
  pl->phys.grounded = false;
  pl->timer = 0;
  mv_drawVfx("tech");
  mv_dispatch(S, MV_CS(S, p), "THROWNFALCONDIVE", "main", p, in, 0);
  return AS_UNDEF;
}

static AsTri mv_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  pl->timer += 1;
  pl->phys.kVel = vec2d(0, 0); // fresh Vec2D upstream
  if (mv_dispatch(S, MV_CS(S, p), "THROWNFALCONDIVE", "interrupt", p, in,
                  0) != AS_TRUE) {
    // empty body upstream
  }
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

const MlMoveDef mv_THROWNFALCONDIVE = {"THROWNFALCONDIVE", mv_init, mv_main,
                                       mv_interrupt, 0};
