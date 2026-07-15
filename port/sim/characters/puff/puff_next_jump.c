// puff_next_jump.c <- src/characters/puff/puffNextJump.js (M2 task 12).
// Dispatches puff["AERIALTURN" | "JUMPAERIAL" + (1 + jumpsUsed)].init —
// the COMPUTED module-index key (rule 15's dispatch-graph lesson): the
// away-stick branch turns, the rest jumps. A missing key (jumpsUsed > 4,
// or a non-integral jumpsUsed producing "AERIALTURN1.5") is the upstream
// TypeError -> mv_out_of_domain via puff_moves_init.
#include "moves.h"

#include <stdio.h>

void puff_next_jump(MlSim *S, double p, const MlInputBuffer in[4]) {
  MlPlayer *pl = mv_player(S, p);
  const MlInput *i0 = &MV_IN(in, p)[0];
  const double n = 1 + pl->phys.jumpsUsed;
  if (n != (double)(int)n) {
    mv_out_of_domain("puffNextJump: non-integral jumpsUsed key");
  }
  char name[24];
  if (js_abs(i0->lsX) > 0.3 && js_sign(i0->lsX) != pl->phys.face) {
    snprintf(name, sizeof name, "AERIALTURN%d", (int)n);
  } else {
    snprintf(name, sizeof name, "JUMPAERIAL%d", (int)n);
  }
  puff_moves_init(S, name, p, in);
}
