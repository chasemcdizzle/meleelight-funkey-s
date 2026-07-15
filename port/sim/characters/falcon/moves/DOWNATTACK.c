// DOWNATTACK.c <- src/characters/falcon/moves/DOWNATTACK.js (M2 task 10)
// Falcon deltas vs fox: FOUR ids per hit (downattack1/downattack2 id3),
// no sword2 sounds, different timers, and the second hit re-assigns all
// four ids.
#include "../moves.h"

static AsTri fc4_main(MlSim *S, double p, const MlInputBuffer in[4],
                      const MvX *ex);
static AsTri fc4_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                           const MvX *ex);

static AsTri fc4_init(MlSim *S, double p, const MlInputBuffer in[4],
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
  fc4_main(S, p, in, 0);
  return AS_UNDEF;
}

static AsTri fc4_main(MlSim *S, double p, const MlInputBuffer in[4],
                      const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  pl->timer += 1;
  if (fc4_interrupt(S, p, in, 0) != AS_TRUE) {
    as_reduceByTraction(true, (int)MV_CS(S, p), &pl->phys.cVel.x);
    if (pl->timer == 1) {
      pl->phys.intangibleTimer = 26;
    }
    if (pl->timer == 19) {
      pl->hitboxes.active[0] = true;
      pl->hitboxes.active[1] = true;
      pl->hitboxes.active[2] = true;
      pl->hitboxes.active[3] = true;
      S->aliasHbActive[(int)p] = false; // fresh array upstream
      pl->hitboxes.frame = 0;
    }
    if (pl->timer > 19 && pl->timer < 21) {
      pl->hitboxes.frame += 1;
    }
    if (pl->timer == 21) {
      mv_turnOffHitboxes(S, p);
    }
    if (pl->timer == 28) {
      mv_assign_hitbox_id(S, p, "downattack2", 0, 0);
      mv_assign_hitbox_id(S, p, "downattack2", 1, 1);
      mv_assign_hitbox_id(S, p, "downattack2", 2, 2);
      mv_assign_hitbox_id(S, p, "downattack2", 3, 3);
      pl->hitboxes.active[0] = true;
      pl->hitboxes.active[1] = true;
      pl->hitboxes.active[2] = true;
      pl->hitboxes.active[3] = true;
      S->aliasHbActive[(int)p] = false; // fresh array upstream
      pl->hitboxes.frame = 0;
    }
    if (pl->timer > 28 && pl->timer < 30) {
      pl->hitboxes.frame += 1;
    }
    if (pl->timer == 30) {
      mv_turnOffHitboxes(S, p);
    }
  }
  return AS_UNDEF;
}

static AsTri fc4_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
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

const MlMoveDef falcon_DOWNATTACK = {"DOWNATTACK", fc4_init, fc4_main,
                                     fc4_interrupt, 0};
