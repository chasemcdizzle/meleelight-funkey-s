// dancing_blade_combo.c <- src/characters/marth/dancingBladeCombo.js
// (M2 task 11). Helper module at characters/marth/ level, imported by the
// SIDESPECIAL* mains.
#include "moves.h"

void marth_dancingBladeCombo(MlSim *S, double p, double min, double max,
                             const MlInputBuffer in[4]) {
  MlPlayer *pl = mv_player(S, p);
  const MlInput *i0 = &MV_IN(in, p)[0];
  const MlInput *i1 = &MV_IN(in, p)[1];
  if (pl->timer > 1) {
    // upstream precedence verbatim: (a-press) OR (b-press AND !disable)
    const bool dbDisable =
        pl->phys.hasDancingBladeDisable && pl->phys.dancingBladeDisable;
    if ((i0->a && !i1->a) || ((i0->b && !i1->b) && !dbDisable)) {
      if (pl->timer < min) {
        pl->phys.hasDancingBladeDisable = true;
        pl->phys.dancingBladeDisable = true;
      } else if (pl->timer <= max) {
        pl->phys.hasDancingBlade = true;
        pl->phys.dancingBlade = true;
      }
    }
  }
}
