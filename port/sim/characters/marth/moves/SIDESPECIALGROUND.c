// SIDESPECIALGROUND.c <- src/characters/marth/moves/SIDESPECIALGROUND.js
// (M2 task 11 — dancing blade 1, ground). phys.dancingBlade /
// phys.dancingBladeDisable are runtime-added (presence-modeled, rule 16).
#include "../moves.h"

static AsTri mr_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex);
static AsTri mr_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex);

static AsTri mr_init(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "SIDESPECIALGROUND");
  pl->timer = 0;
  pl->phys.hasDancingBlade = true;
  pl->phys.dancingBlade = false;
  pl->phys.hasDancingBladeDisable = true;
  pl->phys.dancingBladeDisable = false;
  pl->phys.cVel.x *= 0.2;
  mv_turnOffHitboxes(S, p);
  mv_assign_hitbox_id(S, p, "dbground", 0, 0);
  mv_assign_hitbox_id(S, p, "dbground", 1, 1);
  mv_assign_hitbox_id(S, p, "dbground", 2, 2);
  mv_assign_hitbox_id(S, p, "dbground", 3, 3);
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
    as_reduceByTraction(true, (int)MV_CS(S, p), &pl->phys.cVel.x);
    if (pl->timer > 4 && pl->timer < 12) {
      mv_drawVfx("swing");
    }
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
      marth_SIDESPECIALGROUND2UP.init(S, p, in, 0);
    } else {
      marth_SIDESPECIALGROUND2FORWARD.init(S, p, in, 0);
    }
    return AS_TRUE;
  } else {
    return AS_FALSE;
  }
}

const MlMoveDef marth_SIDESPECIALGROUND = {"SIDESPECIALGROUND", mr_init,
                                           mr_main, mr_interrupt, 0};
