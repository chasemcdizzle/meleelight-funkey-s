// DOWNSPECIALGROUNDENDGROUND.c <- src/characters/falcon/moves/
// DOWNSPECIALGROUNDENDGROUND.js (M2 task 10). Skid-out after the grounded
// falcon kick: grounded decel (literal 0.128) vs airborne
// airFriction/gravity arms.
#include "../moves.h"

static AsTri fc4_main(MlSim *S, double p, const MlInputBuffer in[4],
                      const MvX *ex);
static AsTri fc4_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                           const MvX *ex);

static AsTri fc4_init(MlSim *S, double p, const MlInputBuffer in[4],
                      const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "DOWNSPECIALGROUNDENDGROUND");
  pl->timer = 0;
  pl->phys.cVel.x = 2.14 * pl->phys.face;
  pl->phys.cVel.y = 0;
  mv_turnOffHitboxes(S, p);
  ml_sound_play("land");
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
    if (pl->phys.grounded) {
      pl->phys.cVel.x = js_sign(pl->phys.cVel.x) *
                        js_max(js_abs(pl->phys.cVel.x) - 0.128, 0);
      pl->phys.cVel.y = 0;
    } else {
      pl->phys.cVel.x =
          js_sign(pl->phys.cVel.x) *
          js_max(js_abs(pl->phys.cVel.x) - ml_f64(at->airFriction), 0);
      pl->phys.cVel.y = js_max(pl->phys.cVel.y - ml_f64(at->gravity),
                               -ml_f64(at->terminalV));
    }
  }
  return AS_UNDEF;
}

static AsTri fc4_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                           const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  if (pl->timer > 30) {
    if (pl->phys.grounded) {
      mv_WAIT.init(S, p, in, 0);
    } else {
      mv_FALL.init(S, p, in, 0);
    }
    return AS_TRUE;
  } else {
    return AS_FALSE;
  }
}

const MlMoveDef falcon_DOWNSPECIALGROUNDENDGROUND = {
    "DOWNSPECIALGROUNDENDGROUND", fc4_init, fc4_main, fc4_interrupt, 0};
