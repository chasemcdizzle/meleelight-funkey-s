// SIDESPECIALAIR.c <- src/characters/marth/moves/SIDESPECIALAIR.js
// (M2 task 11 — dancing blade 1, air; reads phys.sideBJumpFlag).
#include "../moves.h"

static AsTri mr_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex);
static AsTri mr_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex);

static AsTri mr_init(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "SIDESPECIALAIR");
  pl->timer = 0;
  pl->phys.hasDancingBlade = true;
  pl->phys.dancingBlade = false;
  pl->phys.hasDancingBladeDisable = true;
  pl->phys.dancingBladeDisable = false;
  if (!pl->phys.grounded) {
    if (pl->phys.sideBJumpFlag) {
      pl->phys.cVel.y = 1;
      pl->phys.sideBJumpFlag = false;
    } else {
      pl->phys.cVel.y = 0;
    }
    pl->phys.fastfalled = false;
    pl->phys.cVel.x *= 0.8;
  } else {
    pl->phys.cVel.x *= 0.2;
  }
  mv_turnOffHitboxes(S, p);
  mv_assign_hitbox_id(S, p, "dbair", 0, 0);
  mv_assign_hitbox_id(S, p, "dbair", 1, 1);
  mv_assign_hitbox_id(S, p, "dbair", 2, 2);
  mv_assign_hitbox_id(S, p, "dbair", 3, 3);
  ml_sound_play("shout6");
  mr_main(S, p, in, 0);
  return AS_UNDEF;
}

static AsTri mr_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  pl->timer += 1;
  marth_dancingBladeCombo(S, p, 8, 26, in);
  if (mr_interrupt(S, p, in, 0) != AS_TRUE) {
    if (pl->timer == 6) {
      ml_sound_play("dancingBlade");
    }
    if (pl->timer > 4 && pl->timer < 12) {
      mv_drawVfx("swing");
    }
    marth_dancingBladeAirMobility(S, p);
    // upstream carries an empty `if (timer > 4 && timer < 12) {}` here
    if (pl->timer == 6) {
      pl->hitboxes.active[0] = true;
      pl->hitboxes.active[1] = true;
      pl->hitboxes.active[2] = true;
      pl->hitboxes.active[3] = true;
      S->aliasHbActive[(int)p] = false;
      pl->hitboxes.frame = 0;
    }
    if (pl->timer > 6 && pl->timer < 9) {
      pl->hitboxes.frame += 1;
    }
    if (pl->timer == 9) {
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
  if (pl->timer > 29) {
    if (pl->phys.grounded) {
      mv_WAIT.init(S, p, in, 0);
    } else {
      mv_FALL.init(S, p, in, 0);
    }
    return AS_TRUE;
  } else if (pl->phys.hasDancingBlade && pl->phys.dancingBlade) {
    if (i0->lsY > 0.56) {
      marth_SIDESPECIALAIR2UP.init(S, p, in, 0);
    } else {
      marth_SIDESPECIALAIR2FORWARD.init(S, p, in, 0);
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
  strcpy(pl->actionState, "SIDESPECIALGROUND");
  return AS_UNDEF;
}

const MlMoveDef marth_SIDESPECIALAIR = {"SIDESPECIALAIR", mr_init, mr_main,
                                        mr_interrupt, mr_land};
