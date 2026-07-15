// FURAFURA.c <- src/characters/puff/moves/FURAFURA.js (M2 task 12)
// Puff's TABLE OVERRIDE of the shared FURAFURA: WAIT.init only — no furaloop, no furaLoopID (the shared version's Howl-id chain state never enters this cluster).
#include "../moves.h"

static AsTri pf_init(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  mv_WAIT.init(S, p, in, 0);
  //*cough*BITES*cough* (upstream comment)
  return AS_UNDEF;
}

const MlMoveDef puff_FURAFURA = {"FURAFURA", pf_init, 0, 0, 0};
