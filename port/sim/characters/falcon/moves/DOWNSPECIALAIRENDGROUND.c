// DOWNSPECIALAIRENDGROUND.c <- src/characters/falcon/moves/
// DOWNSPECIALAIRENDGROUND.js (M2 task 10). Landing skid off the aerial
// falcon kick: groundBounce vfx (f = groundAngle) + falconkickland
// hitboxes flashed at 1..3, literal 0.24 decel.
#include "../moves.h"

static AsTri fc4_main(MlSim *S, double p, const MlInputBuffer in[4],
                      const MvX *ex);
static AsTri fc4_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                           const MvX *ex);

static AsTri fc4_init(MlSim *S, double p, const MlInputBuffer in[4],
                      const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "DOWNSPECIALAIRENDGROUND");
  pl->timer = 0;
  pl->phys.fastfalled = false;
  pl->phys.cVel.y = 0;
  pl->phys.cVel.x = 0.98542 * pl->phys.face;
  mv_assign_hitbox_id(S, p, "falconkickland", 0, 0);
  mv_assign_hitbox_id(S, p, "falconkickland", 1, 1);
  mv_assign_hitbox_id(S, p, "falconkickland", 2, 2);
  ml_sound_play("land");
  mv_drawVfx("groundBounce");
  mv_turnOffHitboxes(S, p);
  fc4_main(S, p, in, 0);
  return AS_UNDEF;
}

static AsTri fc4_main(MlSim *S, double p, const MlInputBuffer in[4],
                      const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  pl->timer += 1;
  if (fc4_interrupt(S, p, in, 0) != AS_TRUE) {
    pl->phys.cVel.x = js_sign(pl->phys.cVel.x) *
                      js_max(js_abs(pl->phys.cVel.x) - 0.24, 0);
    if (pl->timer == 1) {
      pl->hitboxes.active[0] = true;
      pl->hitboxes.active[1] = true;
      pl->hitboxes.active[2] = true;
      pl->hitboxes.active[3] = false;
      S->aliasHbActive[(int)p] = false; // fresh array upstream
      pl->hitboxes.frame = 0;
    }
    if (pl->timer > 1 && pl->timer < 3) {
      pl->hitboxes.frame += 1;
    }
    if (pl->timer == 3) {
      mv_turnOffHitboxes(S, p);
    }
  }
  return AS_UNDEF;
}

static AsTri fc4_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                           const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  if (pl->timer > 45) {
    mv_WAIT.init(S, p, in, 0);
    return AS_TRUE;
  } else {
    return AS_FALSE;
  }
}

const MlMoveDef falcon_DOWNSPECIALAIRENDGROUND = {
    "DOWNSPECIALAIRENDGROUND", fc4_init, fc4_main, fc4_interrupt, 0};
