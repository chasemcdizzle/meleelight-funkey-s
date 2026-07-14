// FURASLEEPLOOP.c <- src/characters/shared/moves/FURASLEEPLOOP.js
// (M2 task 7)
#include "../moves.h"

// shared colour-blend helper (FURASLEEPSTART.c; identical upstream code)
extern void mv_fura_colour(MlSim *S, double p);

static AsTri mv_init(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "FURASLEEPLOOP");
  pl->timer = 0;
  mv_dispatch(S, MV_CS(S, p), "FURASLEEPLOOP", "main", p, in, 0);
  return AS_UNDEF;
}

static AsTri mv_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  pl->timer += 1;
  if (mv_dispatch(S, MV_CS(S, p), "FURASLEEPLOOP", "interrupt", p, in, 0) !=
      AS_TRUE) {
    pl->phys.stuckTimer -= 1;
    mv_fura_colour(S, p); // NOTE: no reduceByTraction in the LOOP variant
  }
  return AS_UNDEF;
}

static AsTri mv_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  if (pl->phys.stuckTimer <= 0) {
    pl->colourOverlayBool = false;
    mv_dispatch(S, MV_CS(S, p), "FURASLEEPEND", "init", p, in, 0);
    return AS_TRUE;
  } else if (pl->timer > mv_frames(MV_CS(S, p), "FURASLEEPLOOP")) {
    pl->timer = 1;
    pl->colourOverlayBool = false;
    return AS_FALSE;
  } else {
    return AS_FALSE;
  }
}

const MlMoveDef mv_FURASLEEPLOOP = {"FURASLEEPLOOP", mv_init, mv_main,
                                    mv_interrupt, 0};
