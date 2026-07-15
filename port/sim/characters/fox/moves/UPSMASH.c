// UPSMASH.c <- src/characters/fox/moves/UPSMASH.js (M2 task 8)
#include "../moves.h"

static AsTri fx_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex);
static AsTri fx_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex);

static AsTri fx_init(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "UPSMASH");
  pl->timer = 0;
  pl->phys.charging = false;
  pl->phys.chargeFrames = 0;
  mv_turnOffHitboxes(S, p);
  mv_assign_hitbox_id(S, p, "upsmash1", 0, 0);
  mv_assign_hitbox_id(S, p, "upsmash1", 1, 1);
  fx_main(S, p, in, 0);
  return AS_UNDEF;
}

static AsTri fx_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  const MlInput *i0 = &MV_IN(in, p)[0];
  if (pl->timer == 2) {
    if (i0->a || i0->z) {
      pl->phys.charging = true;
      pl->phys.chargeFrames += 1;
      if (pl->phys.chargeFrames == 5) {
        ml_sound_play("smashcharge");
      }
      if (pl->phys.chargeFrames == 60) {
        pl->timer += 1;
        pl->phys.charging = false;
      }
    } else {
      pl->timer += 1;
      pl->phys.charging = false;
    }
  } else {
    pl->timer += 1;
    pl->phys.charging = false;
  }
  if (fx_interrupt(S, p, in, 0) != AS_TRUE) {
    as_reduceByTraction(true, (int)MV_CS(S, p), &pl->phys.cVel.x);

    if (pl->timer == 7) {
      pl->hitboxes.active[0] = true;
      pl->hitboxes.active[1] = true;
      pl->hitboxes.active[2] = false;
      pl->hitboxes.active[3] = false;
      S->aliasHbActive[(int)p] = false;
      pl->hitboxes.frame = 0;
      as_randomShout(MV_CS(S, p));
      ml_sound_play("normalswing1");
    }
    if (pl->timer > 7 && pl->timer < 18) {
      pl->hitboxes.frame += 1;
    }
    if (pl->timer == 10) {
      mv_assign_hitbox_id(S, p, "upsmash2", 0, 0);
      mv_assign_hitbox_id(S, p, "upsmash2", 1, 1);
      pl->hitboxes.frame = 0;
    }
    if (pl->timer == 18) {
      mv_turnOffHitboxes(S, p);
    }
  }
  return AS_UNDEF;
}

static AsTri fx_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  if (pl->timer > 41) {
    mv_WAIT.init(S, p, in, 0);
    return AS_TRUE;
  } else {
    return AS_FALSE;
  }
}

const MlMoveDef fox_UPSMASH = {"UPSMASH", fx_init, fx_main, fx_interrupt, 0};
