// UPSPECIALTHROW.c <- src/characters/falcon/moves/UPSPECIALTHROW.js
// (M2 task 10). The dive throw-out: init draws the seeded stream INLINE
// (2 draws per firefoxtail x3, UPSPECIALCATCH's pattern) + falconyes;
// main drives cVel from the PAIR setVelocities array (mvData).
#include "../moves.h"

static AsTri fc4_main(MlSim *S, double p, const MlInputBuffer in[4],
                      const MvX *ex);
static AsTri fc4_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                           const MvX *ex);

static AsTri fc4_init(MlSim *S, double p, const MlInputBuffer in[4],
                      const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "UPSPECIALTHROW");
  pl->timer = 0;
  // cVel = new Vec2D(0, 0)
  pl->phys.cVel = vec2d(0, 0);
  pl->phys.fastfalled = false;
  mv_turnOffHitboxes(S, p);
  ml_sound_play("falconyes");
  for (int n = 0; n < 3; n++) {
    // (-0.5+Math.random())*17 offsets — render-only values, chain draws
    (void)ml_random();
    (void)ml_random();
    mv_drawVfx("firefoxtail");
  }
  fc4_main(S, p, in, 0);
  return AS_UNDEF;
}

static AsTri fc4_main(MlSim *S, double p, const MlInputBuffer in[4],
                      const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  pl->timer += 1;
  if (fc4_interrupt(S, p, in, 0) != AS_TRUE) {
    Vec2D sv;
    if (!mv_falcon_pair("UPSPECIALTHROW", "setVelocities", pl->timer - 1,
                        &sv)) {
      mv_out_of_domain("UPSPECIALTHROW: setVelocities out of range");
    }
    pl->phys.cVel.x = sv.x * pl->phys.face;
    pl->phys.cVel.y = sv.y;
  }
  return AS_UNDEF;
}

static AsTri fc4_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                           const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  if (pl->timer > 60) {
    mv_FALL.init(S, p, in, 0);
    return AS_TRUE;
  } else {
    return AS_FALSE;
  }
}

const MlMoveDef falcon_UPSPECIALTHROW = {
    "UPSPECIALTHROW", fc4_init, fc4_main, fc4_interrupt, 0};
