// APPEAL.c <- src/characters/fox/moves/APPEAL.js (M2 task 8)
// this.setVelocities1/setVelocities2 come from the mvData fox dump.
#include "../moves.h"

static AsTri fx_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex);
static AsTri fx_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex);

static AsTri fx_init(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "APPEAL");
  pl->timer = 0;
  pl->phys.cVel.x = 0;
  fx_main(S, p, in, 0);
  return AS_UNDEF;
}

static AsTri fx_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  pl->timer += 1;
  if (fx_interrupt(S, p, in, 0) != AS_TRUE) {
    if (pl->timer > 1 && pl->timer < 18) {
      pl->phys.cVel.x = mv_fox_arr("APPEAL", "setVelocities1",
                                   pl->timer - 2) * pl->phys.face;
    } else if (pl->timer > 88) {
      pl->phys.cVel.x = mv_fox_arr("APPEAL", "setVelocities2",
                                   pl->timer - 89) * pl->phys.face;
    }
    if (pl->timer == 31) {
      ml_sound_play("foxtaunt");
    }
  }
  return AS_UNDEF;
}

static AsTri fx_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  if (pl->timer > 110) {
    mv_WAIT.init(S, p, in, 0);
    return AS_TRUE;
  } else {
    return AS_FALSE;
  }
}

const MlMoveDef fox_APPEAL = {"APPEAL", fx_init, fx_main, fx_interrupt, 0};
