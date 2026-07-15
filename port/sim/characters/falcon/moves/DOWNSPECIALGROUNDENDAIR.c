// DOWNSPECIALGROUNDENDAIR.c <- src/characters/falcon/moves/
// DOWNSPECIALGROUNDENDAIR.js (M2 task 10). GOTCHA carried verbatim: the
// two cVel.x arms read `player.timer` — the player ARRAY's (undefined)
// timer, not player[p].timer — so `undefined < 7` and `undefined === 7`
// are both FALSE: both arms are dead. Only the (player[p].timer > 7)
// gravity arm and its else (setVelocities read) execute. land writes
// actionState directly (no init dispatch).
#include "../moves.h"

static AsTri fc4_main(MlSim *S, double p, const MlInputBuffer in[4],
                      const MvX *ex);
static AsTri fc4_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                           const MvX *ex);

static AsTri fc4_init(MlSim *S, double p, const MlInputBuffer in[4],
                      const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "DOWNSPECIALGROUNDENDAIR");
  pl->timer = 0;
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
    if (pl->timer > 1) {
      // upstream: if (player.timer < 7) / if (player.timer === 7) — the
      // ARRAY's timer is undefined, both comparisons false: dead arms.
      if (pl->timer > 7) {
        pl->phys.cVel.y = js_max(pl->phys.cVel.y - ml_f64(at->gravity),
                                 -ml_f64(at->terminalV));
        pl->phys.cVel.x =
            js_sign(pl->phys.cVel.x) *
            js_max(js_abs(pl->phys.cVel.x) - ml_f64(at->airFriction), 0);
      } else {
        pl->phys.cVel.y = mv_falcon_arr("DOWNSPECIALGROUNDENDAIR",
                                        "setVelocities", pl->timer - 1);
      }
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

static AsTri fc4_land(MlSim *S, double p, const MlInputBuffer in[4],
                      const MvX *ex) {
  (void)in; (void)ex;
  strcpy(mv_player(S, p)->actionState, "DOWNSPECIALGROUNDENDGROUND");
  return AS_UNDEF;
}

const MlMoveDef falcon_DOWNSPECIALGROUNDENDAIR = {
    "DOWNSPECIALGROUNDENDAIR", fc4_init, fc4_main, fc4_interrupt, fc4_land};
