// DOWNSPECIALGROUNDTURN.c <-
// src/characters/falco/moves/DOWNSPECIALGROUNDTURN.js (M2 task 9)
#include "../moves.h"

static AsTri fc_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex);
static AsTri fc_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex);

static AsTri fc_init(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "DOWNSPECIALGROUNDTURN");
  pl->timer = 0;
  mv_turnOffHitboxes(S, p);
  fc_main(S, p, in, 0);
  return AS_UNDEF;
}

static AsTri fc_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  pl->timer += 1;
  if (fc_interrupt(S, p, in, 0) != AS_TRUE) {
    as_reduceByTraction(false, (int)MV_CS(S, p), &pl->phys.cVel.x);

    if (pl->shineLoop == 6) {
      pl->shineLoop = 0;
    }
    pl->shineLoop += 1;
    ml_drawVfx("shineloop", 0, 0, p);
  }
  return AS_UNDEF;
}

static AsTri fc_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  if (pl->timer > 3) {
    pl->phys.face *= -1;
    falco_DOWNSPECIALGROUNDLOOP.init(S, p, in, 0);
    return AS_TRUE;
  } else {
    return AS_FALSE;
  }
}

const MlMoveDef falco_DOWNSPECIALGROUNDTURN = {
    "DOWNSPECIALGROUNDTURN", fc_init, fc_main, fc_interrupt, 0};
