// DOWNATTACK.c <- src/characters/puff/moves/DOWNATTACK.js (M2 task 12)
#include "../moves.h"

static AsTri pf_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex);
static AsTri pf_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex);

static AsTri pf_init(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "DOWNATTACK");
  pl->timer = 0;
  mv_turnOffHitboxes(S, p);
  pf_assign_hitbox_id(S, p, "downattack1", 0, 0);
  pf_assign_hitbox_id(S, p, "downattack1", 1, 1);
  pf_assign_hitbox_id(S, p, "downattack1", 2, 2);
  pf_assign_hitbox_id(S, p, "downattack1", 3, 3);
  pf_main(S, p, in, 0);
  return AS_UNDEF;
}

static AsTri pf_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  pl->timer += 1;
  if (pf_interrupt(S, p, in, 0) != AS_TRUE) {
    as_reduceByTraction(true, (int)MV_CS(S, p), &pl->phys.cVel.x);
    if (pl->timer == 1) {
      pl->phys.intangibleTimer = 15;
    }
    if (pl->timer == 20) {
      pl->hitboxes.active[0] = true;
      pl->hitboxes.active[1] = true;
      pl->hitboxes.active[2] = true;
      pl->hitboxes.active[3] = true;
      S->aliasHbActive[(int)p] = false; // fresh array upstream
      pl->hitboxes.frame = 0;
      ml_sound_play("sword2");
    }
    if (pl->timer > 20 && pl->timer < 22) {
      pl->hitboxes.frame += 1;
    }
    if (pl->timer == 22) {
      mv_turnOffHitboxes(S, p);
    }
    if (pl->timer == 30) {
      pf_assign_hitbox_id(S, p, "downattack2", 0, 0);
      pf_assign_hitbox_id(S, p, "downattack2", 1, 1);
      pf_assign_hitbox_id(S, p, "downattack2", 2, 2);
      pf_assign_hitbox_id(S, p, "downattack2", 3, 3);
      pl->hitboxes.active[0] = true;
      pl->hitboxes.active[1] = true;
      pl->hitboxes.active[2] = true;
      pl->hitboxes.active[3] = true;
      S->aliasHbActive[(int)p] = false; // fresh array upstream
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

static AsTri pf_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
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

const MlMoveDef puff_DOWNATTACK = {"DOWNATTACK", pf_init, pf_main, pf_interrupt, 0};
