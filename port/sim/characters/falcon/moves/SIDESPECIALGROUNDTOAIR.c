// SIDESPECIALGROUNDTOAIR.c <- src/characters/falcon/moves/
// SIDESPECIALGROUNDTOAIR.js (M2 task 10). The whole move object is
// {name, init, main}: init clamps to aerialHmaxV and delegates to
// FALLSPECIAL.init; main re-enters this.init (verbatim).
#include "../moves.h"

static AsTri fc4_init(MlSim *S, double p, const MlInputBuffer in[4],
                      const MvX *ex);

static AsTri fc4_main(MlSim *S, double p, const MlInputBuffer in[4],
                      const MvX *ex) {
  return fc4_init(S, p, in, ex); // upstream: this.init(p, input)
}

static AsTri fc4_init(MlSim *S, double p, const MlInputBuffer in[4],
                      const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  const ml_attributes_t *at = mv_attr(MV_CS(S, p));
  if (js_abs(pl->phys.cVel.x) > ml_f64(at->aerialHmaxV)) {
    pl->phys.cVel.x = js_sign(pl->phys.cVel.x) * ml_f64(at->aerialHmaxV);
  }
  mv_FALLSPECIAL.init(S, p, in, 0);
  return AS_UNDEF;
}

const MlMoveDef falcon_SIDESPECIALGROUNDTOAIR = {
    "SIDESPECIALGROUNDTOAIR", fc4_init, fc4_main, 0, 0};
