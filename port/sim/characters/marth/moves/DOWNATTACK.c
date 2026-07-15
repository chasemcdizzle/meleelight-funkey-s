// DOWNATTACK.c <- src/characters/marth/moves/DOWNATTACK.js (M2 task 11)
#include "../moves.h"

static AsTri mr_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex);
static AsTri mr_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex);

static AsTri mr_init(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "DOWNATTACK");
  pl->timer = 0;
  mv_turnOffHitboxes(S, p);
  mv_assign_hitbox_id(S, p, "downattack1", 0, 0);
  mv_assign_hitbox_id(S, p, "downattack1", 1, 1);
  mv_assign_hitbox_id(S, p, "downattack1", 2, 2);
  mv_assign_hitbox_id(S, p, "downattack1", 3, 3);
  mr_main(S, p, in, 0);
  return AS_UNDEF;
}

static AsTri mr_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  pl->timer += 1;
  if (mr_interrupt(S, p, in, 0) != AS_TRUE) {
    as_reduceByTraction(true, (int)MV_CS(S, p), &pl->phys.cVel.x);
    if (pl->timer == 1) {
      pl->phys.intangibleTimer = 31;
    }
    if (pl->timer == 20) {
      pl->hitboxes.active[0] = true;
      pl->hitboxes.active[1] = true;
      pl->hitboxes.active[2] = true;
      pl->hitboxes.active[3] = true;
      S->aliasHbActive[(int)p] = false;
      pl->hitboxes.frame = 0;
      ml_sound_play("sword2");
    }
    if (pl->timer > 20 && pl->timer < 24) {
      pl->hitboxes.frame += 1;
    }
    if (pl->timer == 24) {
      mv_turnOffHitboxes(S, p);
    }
    if (pl->timer == 30) {
      mv_assign_hitbox_id(S, p, "downattack2", 0, 0);
      mv_assign_hitbox_id(S, p, "downattack2", 1, 1);
      mv_assign_hitbox_id(S, p, "downattack2", 2, 2);
      mv_assign_hitbox_id(S, p, "downattack2", 3, 3);
      pl->hitboxes.active[0] = true;
      pl->hitboxes.active[1] = true;
      pl->hitboxes.active[2] = true;
      pl->hitboxes.active[3] = true;
      S->aliasHbActive[(int)p] = false;
      pl->hitboxes.frame = 0;
      ml_sound_play("sword2");
    }
    if (pl->timer > 30 && pl->timer < 32) {
      pl->hitboxes.frame += 1;
    }
    if (pl->timer == 32) {
      mv_turnOffHitboxes(S, p);
    }
  }
  return AS_UNDEF;
}

static AsTri mr_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  if (pl->timer > 49) {
    mv_WAIT.init(S, p, in, 0);
    return AS_TRUE;
  } else {
    return AS_FALSE;
  }
}

const MlMoveDef marth_DOWNATTACK = {"DOWNATTACK", mr_init, mr_main,
                                    mr_interrupt, 0};
