// SIDESPECIALGROUND2UP.c <-
// src/characters/marth/moves/SIDESPECIALGROUND2UP.js (M2 task 11)
#include "../moves.h"

static AsTri mr_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex);
static AsTri mr_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex);

static AsTri mr_init(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "SIDESPECIALGROUND2UP");
  pl->timer = 0;
  pl->phys.hasDancingBlade = true;
  pl->phys.dancingBlade = false;
  pl->phys.hasDancingBladeDisable = true;
  pl->phys.dancingBladeDisable = false;
  mv_turnOffHitboxes(S, p);
  mv_assign_hitbox_id(S, p, "dbground2up", 0, 0);
  mv_assign_hitbox_id(S, p, "dbground2up", 1, 1);
  mv_assign_hitbox_id(S, p, "dbground2up", 2, 2);
  mv_assign_hitbox_id(S, p, "dbground2up", 3, 3);
  ml_sound_play("shout1");
  mr_main(S, p, in, 0);
  return AS_UNDEF;
}

static AsTri mr_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  pl->timer += 1;
  marth_dancingBladeCombo(S, p, 17, 32, in);
  if (mr_interrupt(S, p, in, 0) != AS_TRUE) {
    as_reduceByTraction(true, (int)MV_CS(S, p), &pl->phys.cVel.x);
    if (pl->timer > 10 && pl->timer < 20) {
      ml_drawVfx_swing("swing", 0, 0, pl->phys.face, p, "SIDESPECIALGROUND2UP", pl->timer - 11);
    }
    if (pl->timer == 12) {
      pl->hitboxes.active[0] = true;
      pl->hitboxes.active[1] = true;
      pl->hitboxes.active[2] = true;
      pl->hitboxes.active[3] = true;
      S->aliasHbActive[(int)p] = false;
      pl->hitboxes.frame = 0;
      ml_sound_play("dancingBlade2");
    }
    if (pl->timer > 12 && pl->timer < 16) {
      pl->hitboxes.frame += 1;
    }
    if (pl->timer == 16) {
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
  if (pl->timer > 40) {
    if (pl->phys.grounded) {
      mv_WAIT.init(S, p, in, 0);
    } else {
      mv_FALL.init(S, p, in, 0);
    }
    return AS_TRUE;
  } else if (pl->phys.hasDancingBlade && pl->phys.dancingBlade) {
    if (i0->lsY > 0.56) {
      marth_SIDESPECIALGROUND3UP.init(S, p, in, 0);
    } else if (i0->lsY < -0.56) {
      marth_SIDESPECIALGROUND3DOWN.init(S, p, in, 0);
    } else {
      marth_SIDESPECIALGROUND3FORWARD.init(S, p, in, 0);
    }
    return AS_TRUE;
  } else {
    return AS_FALSE;
  }
}

const MlMoveDef marth_SIDESPECIALGROUND2UP = {"SIDESPECIALGROUND2UP",
                                              mr_init, mr_main, mr_interrupt,
                                              0};
