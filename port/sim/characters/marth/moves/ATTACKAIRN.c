// ATTACKAIRN.c <- src/characters/marth/moves/ATTACKAIRN.js (M2 task 11)
#include "../moves.h"

static AsTri mr_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex);
static AsTri mr_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex);

static AsTri mr_init(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "ATTACKAIRN");
  pl->timer = 0;
  pl->phys.autoCancel = true;
  pl->hasInAerial = true;
  pl->inAerial = true;
  mv_turnOffHitboxes(S, p);
  mv_assign_hitbox_id(S, p, "nair1", 0, 0);
  mv_assign_hitbox_id(S, p, "nair1", 1, 1);
  mv_assign_hitbox_id(S, p, "nair1", 2, 2);
  mv_assign_hitbox_id(S, p, "nair1", 3, 3);
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
    if (pl->timer > 4 && pl->timer < 9) {
      ml_drawVfx_swing("swing", 0, 0, pl->phys.face, p, "NAIR1", pl->timer - 5); // NAIR1
    }
    if (pl->timer > 13 && pl->timer < 21) {
      ml_drawVfx_swing("swing", 0, 0, pl->phys.face, p, "NAIR2", pl->timer - 14); // NAIR2
    }
    if (pl->timer == 6) {
      pl->hitboxes.active[0] = true;
      pl->hitboxes.active[1] = true;
      pl->hitboxes.active[2] = true;
      pl->hitboxes.active[3] = true;
      S->aliasHbActive[(int)p] = false;
      pl->hitboxes.frame = 0;
      pl->phys.autoCancel = false;
      ml_sound_play("sword1");
    }
    if (pl->timer == 7) {
      pl->hitboxes.frame += 1;
    }
    if (pl->timer == 8) {
      mv_turnOffHitboxes(S, p);
    }
    if (pl->timer == 15) {
      pl->hitboxes.active[0] = true;
      pl->hitboxes.active[1] = true;
      pl->hitboxes.active[2] = true;
      pl->hitboxes.active[3] = true;
      S->aliasHbActive[(int)p] = false;
      mv_assign_hitbox_id(S, p, "nair2", 0, 0);
      mv_assign_hitbox_id(S, p, "nair2", 1, 1);
      mv_assign_hitbox_id(S, p, "nair2", 2, 2);
      mv_assign_hitbox_id(S, p, "nair2", 3, 3);
      pl->hitboxes.frame = 0;
      ml_sound_play("sword2");
    }
    if (pl->timer > 15 && pl->timer < 22) {
      pl->hitboxes.frame += 1;
    }
    if (pl->timer == 22) {
      mv_turnOffHitboxes(S, p);
    }
    if (pl->timer == 25) {
      pl->phys.autoCancel = true;
    }
  }
  return AS_UNDEF;
}

static AsTri mr_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  if (pl->timer > 49) {
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
    mv_LANDINGATTACKAIRN.init(S, p, in, 0);
  }
  return AS_UNDEF;
}

const MlMoveDef marth_ATTACKAIRN = {"ATTACKAIRN", mr_init, mr_main,
                                    mr_interrupt, mr_land};
