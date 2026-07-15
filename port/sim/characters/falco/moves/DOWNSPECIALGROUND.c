// DOWNSPECIALGROUND.c <- src/characters/falco/moves/DOWNSPECIALGROUND.js
// (M2 task 9). Falco's grounded shine entry: an init-only delegate.
#include "../moves.h"

static AsTri fc_init(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  falco_DOWNSPECIALGROUNDSTART.init(S, p, in, 0);
  return AS_UNDEF;
}

const MlMoveDef falco_DOWNSPECIALGROUND = {"DOWNSPECIALGROUND", fc_init, 0, 0,
                                           0};
