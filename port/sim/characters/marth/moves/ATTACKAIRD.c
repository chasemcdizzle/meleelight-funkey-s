// ATTACKAIRD.c <- src/characters/marth/moves/ATTACKAIRD.js (M2 task 11)
#include "../moves.h"

static AsTri mr_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex);
static AsTri mr_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex);

static AsTri mr_init(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "ATTACKAIRD");
  pl->timer = 0;
  pl->phys.autoCancel = true;
  pl->hasInAerial = true;
  pl->inAerial = true;
  mv_turnOffHitboxes(S, p);
  mv_assign_hitbox_id(S, p, "dair", 0, 0);
  mv_assign_hitbox_id(S, p, "dair", 1, 1);
  mv_assign_hitbox_id(S, p, "dair", 2, 2);
  mv_assign_hitbox_id(S, p, "dair", 3, 3);
  mr_main(S, p, in, 0);
  return AS_UNDEF;
}

static AsTri mr_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  pl->timer += 1;
  if (mr_interrupt(S, p, in, 0) != AS_TRUE) {
    as_fastfall((int)MV_CS(S, p), &pl->phys.cVel.y, &pl->phys.fastfalled,
                MV_IN(in, p));
    as_airDrift((int)MV_CS(S, p), &pl->phys.cVel.x, MV_IN(in, p));
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
      pl->phys.autoCancel = false;
      ml_sound_play("sword3");
    }
    if (pl->timer > 6 && pl->timer < 10) {
      pl->hitboxes.frame += 1;
    }
    if (pl->timer == 10) {
      mv_turnOffHitboxes(S, p);
    }
    if (pl->timer == 48) {
      pl->phys.autoCancel = true;
    }
  }
  return AS_UNDEF;
}

static AsTri mr_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  if (pl->timer > 59) {
    mv_FALL.init(S, p, in, 0);
    return AS_TRUE;
  } else {
    return AS_FALSE;
  }
}

static AsTri mr_land(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  if (pl->phys.autoCancel) {
    mv_LANDING.init(S, p, in, 0);
  } else {
    mv_LANDINGATTACKAIRD.init(S, p, in, 0);
  }
  return AS_UNDEF;
}

const MlMoveDef marth_ATTACKAIRD = {"ATTACKAIRD", mr_init, mr_main,
                                    mr_interrupt, mr_land};
