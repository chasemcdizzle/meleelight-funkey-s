// SIDESPECIALGROUND3DOWN.c <-
// src/characters/marth/moves/SIDESPECIALGROUND3DOWN.js (M2 task 11)
#include "../moves.h"

static AsTri mr_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex);
static AsTri mr_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex);

static AsTri mr_init(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "SIDESPECIALGROUND3DOWN");
  pl->timer = 0;
  pl->phys.hasDancingBlade = true;
  pl->phys.dancingBlade = false;
  pl->phys.hasDancingBladeDisable = true;
  pl->phys.dancingBladeDisable = false;
  mv_turnOffHitboxes(S, p);
  mv_assign_hitbox_id(S, p, "dbground3down", 0, 0);
  mv_assign_hitbox_id(S, p, "dbground3down", 1, 1);
  mv_assign_hitbox_id(S, p, "dbground3down", 2, 2);
  mv_assign_hitbox_id(S, p, "dbground3down", 3, 3);
  ml_sound_play("shout8");
  mr_main(S, p, in, 0);
  return AS_UNDEF;
}

static AsTri mr_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  pl->timer += 1;
  marth_dancingBladeCombo(S, p, 19, 35, in);
  if (mr_interrupt(S, p, in, 0) != AS_TRUE) {
    pl->phys.cVel.x = mv_marth_arr("SIDESPECIALGROUND3DOWN", "setVelocities",
                                   pl->timer - 1) * pl->phys.face;
    if (pl->timer > 13 && pl->timer < 18) {
      mv_drawVfx("swing");
    }
    if (pl->timer == 15) {
      pl->hitboxes.active[0] = true;
      pl->hitboxes.active[1] = true;
      pl->hitboxes.active[2] = true;
      pl->hitboxes.active[3] = true;
      S->aliasHbActive[(int)p] = false;
      pl->hitboxes.frame = 0;
      ml_sound_play("dancingBlade");
    }
    if (pl->timer > 15 && pl->timer < 19) {
      pl->hitboxes.frame += 1;
    }
    if (pl->timer == 19) {
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

const MlMoveDef marth_SIDESPECIALGROUND3DOWN = {"SIDESPECIALGROUND3DOWN",
                                                mr_init, mr_main,
                                                mr_interrupt, 0};
