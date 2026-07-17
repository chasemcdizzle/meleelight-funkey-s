// SIDESPECIALGROUND3FORWARD.c <-
// src/characters/marth/moves/SIDESPECIALGROUND3FORWARD.js (M2 task 11).
// setVelocities from the mvData marth dump (rule 15).
#include "../moves.h"

static AsTri mr_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex);
static AsTri mr_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex);

static AsTri mr_init(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "SIDESPECIALGROUND3FORWARD");
  pl->timer = 0;
  pl->phys.hasDancingBlade = true;
  pl->phys.dancingBlade = false;
  pl->phys.hasDancingBladeDisable = true;
  pl->phys.dancingBladeDisable = false;
  mv_turnOffHitboxes(S, p);
  mv_assign_hitbox_id(S, p, "dbground3forward", 0, 0);
  mv_assign_hitbox_id(S, p, "dbground3forward", 1, 1);
  mv_assign_hitbox_id(S, p, "dbground3forward", 2, 2);
  mv_assign_hitbox_id(S, p, "dbground3forward", 3, 3);
  ml_sound_play("shout5");
  mr_main(S, p, in, 0);
  return AS_UNDEF;
}

static AsTri mr_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  pl->timer += 1;
  marth_dancingBladeCombo(S, p, 16, 37, in);
  if (mr_interrupt(S, p, in, 0) != AS_TRUE) {
    pl->phys.cVel.x = mv_marth_arr("SIDESPECIALGROUND3FORWARD",
                                   "setVelocities", pl->timer - 1) *
                      pl->phys.face;
    if (pl->timer > 10 && pl->timer < 18) {
      ml_drawVfx_swing("swing", 0, 0, pl->phys.face, p, "SIDESPECIALGROUND3FORWARD", pl->timer - 11);
    }
    if (pl->timer == 11) {
      pl->hitboxes.active[0] = true;
      pl->hitboxes.active[1] = true;
      pl->hitboxes.active[2] = true;
      pl->hitboxes.active[3] = true;
      S->aliasHbActive[(int)p] = false;
      pl->hitboxes.frame = 0;
      ml_sound_play("dancingBlade");
    }
    if (pl->timer > 11 && pl->timer < 15) {
      pl->hitboxes.frame += 1;
    }
    if (pl->timer == 15) {
      mv_turnOffHitboxes(S, p);
    }
  }
  return AS_UNDEF;
}

static AsTri mr_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  const MlInput *i0 = &MV_IN(in, p)[0];
  if (pl->timer > 46) {
    if (pl->phys.grounded) {
      mv_WAIT.init(S, p, in, 0);
    } else {
      mv_FALL.init(S, p, in, 0);
    }
    return AS_TRUE;
  } else if (pl->phys.hasDancingBlade && pl->phys.dancingBlade) {
    if (i0->lsY > 0.56) {
      marth_SIDESPECIALGROUND4UP.init(S, p, in, 0);
    } else if (i0->lsY < -0.56) {
      marth_SIDESPECIALGROUND4DOWN.init(S, p, in, 0);
    } else {
      marth_SIDESPECIALGROUND4FORWARD.init(S, p, in, 0);
    }
    return AS_TRUE;
  } else {
    return AS_FALSE;
  }
}

const MlMoveDef marth_SIDESPECIALGROUND3FORWARD = {"SIDESPECIALGROUND3FORWARD",
                                                   mr_init, mr_main,
                                                   mr_interrupt, 0};
