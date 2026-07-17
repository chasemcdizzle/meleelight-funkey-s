// DOWNSPECIALAIR.c <- src/characters/falcon/moves/DOWNSPECIALAIR.js
// (M2 task 10). Falcon kick (aerial): pair-array setVelocities before 17,
// literal dive velocity after (with the timer%2 firefoxtail), three-stage
// hitbox swap (Clean 15 / Mid 18 / Late 26).
#include "../moves.h"

static AsTri fc4_main(MlSim *S, double p, const MlInputBuffer in[4],
                      const MvX *ex);
static AsTri fc4_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                           const MvX *ex);

static AsTri fc4_init(MlSim *S, double p, const MlInputBuffer in[4],
                      const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "DOWNSPECIALAIR");
  pl->timer = 0;
  pl->phys.fastfalled = false;
  pl->phys.cVel.y = 0;
  pl->phys.cVel.x = 0;
  mv_assign_hitbox_id(S, p, "falconkickairClean", 0, 0);
  mv_assign_hitbox_id(S, p, "falconkickairClean", 1, 1);
  ml_sound_play("falconkickshout");
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
    if (pl->timer < 17) {
      Vec2D sv;
      if (!mv_falcon_pair("DOWNSPECIALAIR", "setVelocities", pl->timer - 1,
                          &sv)) {
        mv_out_of_domain("DOWNSPECIALAIR: setVelocities out of range");
      }
      pl->phys.cVel.x = sv.x * pl->phys.face;
      pl->phys.cVel.y = sv.y;
    } else {
      pl->phys.cVel.x = 1.22542 * pl->phys.face;
      pl->phys.cVel.y = -3.81748;
      if (fmod(pl->timer, 2) != 0) { // if (player[p].timer%2) truthiness
        ml_drawVfx("firefoxtail", pl->phys.pos.x + 3 * pl->phys.face,
                   pl->phys.pos.y - 7, pl->phys.face);
      }
    }
    if (pl->timer == 15) {
      pl->hitboxes.active[0] = true;
      pl->hitboxes.active[1] = true;
      pl->hitboxes.active[2] = false;
      pl->hitboxes.active[3] = false;
      S->aliasHbActive[(int)p] = false; // fresh array upstream
      pl->hitboxes.frame = 0;
      ml_sound_play("falconkick");
    }
    if (pl->timer > 15 && pl->timer < 30) {
      pl->hitboxes.frame += 1;
    }
    if (pl->timer == 18) {
      pl->hitboxes.active[0] = true;
      pl->hitboxes.active[1] = true;
      pl->hitboxes.active[2] = false;
      pl->hitboxes.active[3] = false;
      S->aliasHbActive[(int)p] = false; // fresh array upstream
      pl->hitboxes.frame = 0;
      mv_assign_hitbox_id(S, p, "falconkickairMid", 0, 0);
      mv_assign_hitbox_id(S, p, "falconkickairMid", 1, 1);
    }
    if (pl->timer == 26) {
      pl->hitboxes.active[0] = true;
      pl->hitboxes.active[1] = true;
      pl->hitboxes.active[2] = false;
      pl->hitboxes.active[3] = false;
      S->aliasHbActive[(int)p] = false; // fresh array upstream
      pl->hitboxes.frame = 0;
      mv_assign_hitbox_id(S, p, "falconkickairLate", 0, 0);
      mv_assign_hitbox_id(S, p, "falconkickairLate", 1, 1);
    }
    if (pl->timer == 30) {
      mv_turnOffHitboxes(S, p);
    }
  }
  return AS_UNDEF;
}

static AsTri fc4_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                           const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  if (pl->timer > 29) {
    falcon_DOWNSPECIALAIRENDAIR.init(S, p, in, 0);
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

const MlMoveDef falcon_DOWNSPECIALAIR = {
    "DOWNSPECIALAIR", fc4_init, fc4_main, fc4_interrupt, fc4_land};
