// JUMPAERIALF.c <- src/characters/puff/moves/JUMPAERIALF.js (M2 task 12)
// Puff's TABLE OVERRIDE of the shared JUMPAERIALF (rule 15's origin map): an init-only puffNextJump delegate.
#include "../moves.h"

static AsTri pf_init(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  puff_next_jump(S, p, in);
  return AS_UNDEF;
}

const MlMoveDef puff_JUMPAERIALF = {"JUMPAERIALF", pf_init, 0, 0, 0};
