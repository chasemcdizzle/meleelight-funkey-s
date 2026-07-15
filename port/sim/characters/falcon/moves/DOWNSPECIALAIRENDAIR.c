// DOWNSPECIALAIRENDAIR.c <- src/characters/falcon/moves/
// DOWNSPECIALAIRENDAIR.js (M2 task 10). Aerial cool-off after the falcon
// kick: gravity/airFriction decay, doubleJumped reset in init.
#include "../moves.h"

static AsTri fc4_main(MlSim *S, double p, const MlInputBuffer in[4],
                      const MvX *ex);
static AsTri fc4_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                           const MvX *ex);

static AsTri fc4_init(MlSim *S, double p, const MlInputBuffer in[4],
                      const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "DOWNSPECIALAIRENDAIR");
  pl->timer = 0;
  pl->phys.fastfalled = false;
  pl->phys.doubleJumped = false;
  mv_turnOffHitboxes(S, p);
  fc4_main(S, p, in, 0);
  return AS_UNDEF;
}

static AsTri fc4_main(MlSim *S, double p, const MlInputBuffer in[4],
                      const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  const ml_attributes_t *at = mv_attr(MV_CS(S, p));
  pl->timer += 1;
  if (fc4_interrupt(S, p, in, 0) != AS_TRUE) {
    pl->phys.cVel.y = js_max(pl->phys.cVel.y - ml_f64(at->gravity),
                             -ml_f64(at->terminalV));
    pl->phys.cVel.x =
        js_sign(pl->phys.cVel.x) *
        js_max(js_abs(pl->phys.cVel.x) - ml_f64(at->airFriction), 0);
  }
  return AS_UNDEF;
}

static AsTri fc4_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                           const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  if (pl->timer > 29) {
    mv_FALL.init(S, p, in, 0);
    return AS_TRUE;
  } else {
    return AS_FALSE;
  }
}

static AsTri fc4_land(MlSim *S, double p, const MlInputBuffer in[4],
                      const MvX *ex) {
  (void)ex;
  falcon_DOWNSPECIALAIRENDGROUND.init(S, p, in, 0);
  return AS_UNDEF;
}

const MlMoveDef falcon_DOWNSPECIALAIRENDAIR = {
    "DOWNSPECIALAIRENDAIR", fc4_init, fc4_main, fc4_interrupt, fc4_land};
