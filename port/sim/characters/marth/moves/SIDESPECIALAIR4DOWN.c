// SIDESPECIALAIR4DOWN.c <-
// src/characters/marth/moves/SIDESPECIALAIR4DOWN.js (M2 task 11 — the
// air multi-hit ender: computed key "dbair4down" + floor((timer-7)/6);
// air mobility, NO setVelocities plane).
#include "../moves.h"

#include <math.h>
#include <stdio.h>

static AsTri mr_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex);
static AsTri mr_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex);

static AsTri mr_init(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "SIDESPECIALAIR4DOWN");
  pl->timer = 0;
  mv_turnOffHitboxes(S, p);
  mr_main(S, p, in, 0);
  return AS_UNDEF;
}

static AsTri mr_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  pl->timer += 1;
  if (mr_interrupt(S, p, in, 0) != AS_TRUE) {
    if (pl->timer > 9 && pl->timer < 41) {
      mv_drawVfx("swing");
    }
    marth_dancingBladeAirMobility(S, p);
    if (pl->timer > 12 && pl->timer < 39) {
      const int rem = (int)fmod(pl->timer, 6);
      switch (rem) {
        case 1: {
          char hbName[32];
          snprintf(hbName, sizeof hbName, "dbair4down%d",
                   (int)floor((pl->timer - 7) / 6));
          mv_assign_hitbox_id(S, p, hbName, 0, 0);
          mv_assign_hitbox_id(S, p, hbName, 1, 1);
          mv_assign_hitbox_id(S, p, hbName, 2, 2);
          mv_assign_hitbox_id(S, p, hbName, 3, 3);
          pl->hitboxes.active[0] = true;
          pl->hitboxes.active[1] = true;
          pl->hitboxes.active[2] = true;
          pl->hitboxes.active[3] = true;
          S->aliasHbActive[(int)p] = false;
          pl->hitboxes.frame = 0;
          ml_sound_play("dancingBlade2");
          if (pl->timer < 37) {
            ml_sound_play("shout6");
          }
          break;
        }
        case 2:
        case 3:
          pl->hitboxes.frame += 1;
          break;
        case 4:
          mv_turnOffHitboxes(S, p);
          break;
        default:
          break;
      }
    }
    if (pl->timer == 39) {
      mv_turnOffHitboxes(S, p);
    }
  }
  return AS_UNDEF;
}

static AsTri mr_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  if (pl->timer > 60) {
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

static AsTri mr_land(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)in; (void)ex;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "SIDESPECIALGROUND4DOWN");
  return AS_UNDEF;
}

const MlMoveDef marth_SIDESPECIALAIR4DOWN = {"SIDESPECIALAIR4DOWN", mr_init,
                                             mr_main, mr_interrupt, mr_land};
