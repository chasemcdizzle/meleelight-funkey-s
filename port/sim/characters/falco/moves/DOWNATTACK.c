// DOWNATTACK.c <- src/characters/falco/moves/DOWNATTACK.js (M2 task 9)
#include "../moves.h"

static AsTri fc_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex);
static AsTri fc_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex);

static AsTri fc_init(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "DOWNATTACK");
  pl->timer = 0;
  mv_turnOffHitboxes(S, p);
  mv_assign_hitbox_id(S, p, "downattack1", 0, 0);
  mv_assign_hitbox_id(S, p, "downattack1", 1, 1);
  mv_assign_hitbox_id(S, p, "downattack1", 2, 2);
  fc_main(S, p, in, 0);
  return AS_UNDEF;
}

static AsTri fc_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  pl->timer += 1;
  if (fc_interrupt(S, p, in, 0) != AS_TRUE) {
    as_reduceByTraction(true, (int)MV_CS(S, p), &pl->phys.cVel.x);
    if (pl->timer == 1) {
      pl->phys.intangibleTimer = 26;
    }
    if (pl->timer == 17) {
      pl->hitboxes.active[0] = true;
      pl->hitboxes.active[1] = true;
      pl->hitboxes.active[2] = true;
      pl->hitboxes.active[3] = false;
      S->aliasHbActive[(int)p] = false;
      pl->hitboxes.frame = 0;
      ml_sound_play("sword2");
    }
    if (pl->timer > 17 && pl->timer < 20) {
      pl->hitboxes.frame += 1;
    }
    if (pl->timer == 20) {
      mv_turnOffHitboxes(S, p);
    }
    if (pl->timer == 24) {
      mv_assign_hitbox_id(S, p, "downattack2", 0, 0);
      mv_assign_hitbox_id(S, p, "downattack2", 1, 1);
      mv_assign_hitbox_id(S, p, "downattack2", 2, 2);
      pl->hitboxes.active[0] = true;
      pl->hitboxes.active[1] = true;
      pl->hitboxes.active[2] = true;
      pl->hitboxes.active[3] = false;
      S->aliasHbActive[(int)p] = false;
      pl->hitboxes.frame = 0;
      ml_sound_play("sword2");
    }
    if (pl->timer > 24 && pl->timer < 27) {
      pl->hitboxes.frame += 1;
    }
    if (pl->timer == 27) {
      mv_turnOffHitboxes(S, p);
    }
  }
  return AS_UNDEF;
}

static AsTri fc_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
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

const MlMoveDef falco_DOWNATTACK = {"DOWNATTACK", fc_init, fc_main,
                                  fc_interrupt, 0};
