// SIDESPECIALAIR4UP.c <-
// src/characters/marth/moves/SIDESPECIALAIR4UP.js (M2 task 11)
#include "../moves.h"

static AsTri mr_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex);
static AsTri mr_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex);

static AsTri mr_init(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "SIDESPECIALAIR4UP");
  pl->timer = 0;
  mv_turnOffHitboxes(S, p);
  mv_assign_hitbox_id(S, p, "dbair4up", 0, 0);
  mv_assign_hitbox_id(S, p, "dbair4up", 1, 1);
  mv_assign_hitbox_id(S, p, "dbair4up", 2, 2);
  mv_assign_hitbox_id(S, p, "dbair4up", 3, 3);
  ml_sound_play("shout4");
  mr_main(S, p, in, 0);
  return AS_UNDEF;
}

static AsTri mr_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  pl->timer += 1;
  if (mr_interrupt(S, p, in, 0) != AS_TRUE) {
    if (pl->timer > 17 && pl->timer < 27) {
      ml_drawVfx_swing("swing", 0, 0, pl->phys.face, p, "SIDESPECIALAIR4UP", pl->timer - 18);
    }
    marth_dancingBladeAirMobility(S, p);
    pl->phys.cVel.x = 0;
    if (pl->timer == 20) {
      pl->hitboxes.active[0] = true;
      pl->hitboxes.active[1] = true;
      pl->hitboxes.active[2] = true;
      pl->hitboxes.active[3] = true;
      S->aliasHbActive[(int)p] = false;
      pl->hitboxes.frame = 0;
      ml_sound_play("dancingBlade");
    }
    if (pl->timer > 20 && pl->timer < 26) {
      pl->hitboxes.frame += 1;
    }
    if (pl->timer == 26) {
      mv_turnOffHitboxes(S, p);
    }
  }
  return AS_UNDEF;
}

static AsTri mr_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  if (pl->timer > 50) {
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
  strcpy(pl->actionState, "SIDESPECIALGROUND4UP");
  return AS_UNDEF;
}

const MlMoveDef marth_SIDESPECIALAIR4UP = {"SIDESPECIALAIR4UP", mr_init, mr_main, mr_interrupt,
                                mr_land};
