// DOWNSPECIALAIR.c <- src/characters/falco/moves/DOWNSPECIALAIR.js
// (M2 task 9). Falco's aerial shine entry: an init-only delegate into the
// 4-sub-state machine (the module has no main/interrupt/land).
#include "../moves.h"

static AsTri fc_init(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  falco_DOWNSPECIALAIRSTART.init(S, p, in, 0);
  return AS_UNDEF;
}

const MlMoveDef falco_DOWNSPECIALAIR = {"DOWNSPECIALAIR", fc_init, 0, 0, 0};
