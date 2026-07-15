// UPSPECIAL.c <- src/characters/fox/moves/UPSPECIAL.js (M2 task 8)
// The whole move object is {name, init} — init only.
#include "../moves.h"

static AsTri fx_init(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  fox_UPSPECIALCHARGE.init(S, p, in, 0);
  return AS_UNDEF;
}

const MlMoveDef fox_UPSPECIAL = {"UPSPECIAL", fx_init, 0, 0, 0};
