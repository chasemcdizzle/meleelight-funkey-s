// UPSPECIAL.c <- src/characters/falco/moves/UPSPECIAL.js (M2 task 9)
// The whole move object is {name, init} — init only.
#include "../moves.h"

static AsTri fc_init(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  falco_UPSPECIALCHARGE.init(S, p, in, 0);
  return AS_UNDEF;
}

const MlMoveDef falco_UPSPECIAL = {"UPSPECIAL", fc_init, 0, 0, 0};
