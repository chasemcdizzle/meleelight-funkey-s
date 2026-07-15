// ATTACKAIRD.c <- src/characters/fox/moves/ATTACKAIRD.js (M2 task 8)
#include "../moves.h"

static AsTri fx_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex);
static AsTri fx_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex);

static AsTri fx_init(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "ATTACKAIRD");
  pl->timer = 0;
  pl->phys.autoCancel = true;
  pl->hasInAerial = true;
  pl->inAerial = true;
  pl->hasIASATimer = true;
  pl->IASATimer = 60;
  mv_turnOffHitboxes(S, p);
  mv_assign_hitbox_id(S, p, "dair", 0, 0);
  mv_assign_hitbox_id(S, p, "dair", 1, 1);
  fx_main(S, p, in, 0);
  return AS_UNDEF;
}

static AsTri fx_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  pl->timer += 1;
  if (fx_interrupt(S, p, in, 0) != AS_TRUE) {
    as_fastfall((int)MV_CS(S, p), &pl->phys.cVel.y, &pl->phys.fastfalled,
                MV_IN(in, p));
    as_airDrift((int)MV_CS(S, p), &pl->phys.cVel.x, MV_IN(in, p));
    if (pl->timer == 4) {
      pl->phys.autoCancel = false;
    }

    if (pl->timer > 4 && pl->timer < 26) {
      const double m = fmod(pl->timer, 3);
      if (m == 2) {
        pl->hitboxes.active[0] = true;
        pl->hitboxes.active[1] = true;
        pl->hitboxes.active[2] = false;
        pl->hitboxes.active[3] = false;
        S->aliasHbActive[(int)p] = false;
        pl->hitboxes.frame = 0;
        ml_sound_play("normalswing2");
      } else if (m == 0) {
        pl->hitboxes.frame += 1;
      } else if (m == 1) {
        mv_turnOffHitboxes(S, p);
      }
    }

    if (pl->timer == 32) {
      pl->phys.autoCancel = true;
    }
  }
  return AS_UNDEF;
}

static AsTri fx_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  if (pl->timer > 49) {
    mv_FALL.init(S, p, in, 0);
    return AS_TRUE;
  } else if (mv_checkForIASA(S, p, in, true) == AS_TRUE) {
    return AS_TRUE;
  } else {
    return AS_FALSE;
  }
}

static AsTri fx_land(MlSim *S, double p, const MlInputBuffer in[4],
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

const MlMoveDef fox_ATTACKAIRD = {"ATTACKAIRD", fx_init, fx_main,
                                  fx_interrupt, fx_land};
