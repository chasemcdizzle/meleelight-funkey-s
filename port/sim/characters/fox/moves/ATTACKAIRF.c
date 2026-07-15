// ATTACKAIRF.c <- src/characters/fox/moves/ATTACKAIRF.js (M2 task 8)
#include "../moves.h"

static AsTri fx_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex);
static AsTri fx_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex);

static void active_tt_ff(MlSim *S, MlPlayer *pl, double p) {
  pl->hitboxes.active[0] = true;
  pl->hitboxes.active[1] = true;
  pl->hitboxes.active[2] = false;
  pl->hitboxes.active[3] = false;
  S->aliasHbActive[(int)p] = false;
}

static AsTri fx_init(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "ATTACKAIRF");
  pl->timer = 0;
  pl->phys.autoCancel = true;
  pl->hasInAerial = true;
  pl->inAerial = true;
  pl->hasIASATimer = true;
  pl->IASATimer = 52;
  // upstream assigns fair1 BEFORE turnOffHitboxes (order carried verbatim)
  mv_assign_hitbox_id(S, p, "fair1", 0, 0);
  mv_assign_hitbox_id(S, p, "fair1", 1, 1);
  mv_turnOffHitboxes(S, p);
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
    if (pl->timer == 5) {
      pl->phys.autoCancel = false;
    }
    if (pl->timer == 6) {
      active_tt_ff(S, pl, p);
      pl->hitboxes.frame = 0;
      ml_sound_play("normalswing2");
    }
    if (pl->timer == 7 || pl->timer == 8) {
      pl->hitboxes.frame += 1;
    }
    if (pl->timer == 9) {
      mv_turnOffHitboxes(S, p);
    }
    if (pl->timer == 16) {
      pl->hitboxes.frame = 0;
      mv_assign_hitbox_id(S, p, "fair2", 0, 0);
      mv_assign_hitbox_id(S, p, "fair2", 1, 1);
      active_tt_ff(S, pl, p);
      ml_sound_play("normalswing2");
    }
    if (pl->timer > 16 && pl->timer < 19) {
      pl->hitboxes.frame += 1;
    }
    if (pl->timer == 19) {
      mv_turnOffHitboxes(S, p);
    }
    if (pl->timer == 24) {
      pl->hitboxes.frame = 0;
      mv_assign_hitbox_id(S, p, "fair3", 0, 0);
      mv_assign_hitbox_id(S, p, "fair3", 1, 1);
      active_tt_ff(S, pl, p);
      ml_sound_play("normalswing2");
    }
    if (pl->timer > 24 && pl->timer < 27) {
      pl->hitboxes.frame += 1;
    }
    if (pl->timer == 27) {
      mv_turnOffHitboxes(S, p);
    }
    if (pl->timer == 33) {
      pl->hitboxes.frame = 0;
      mv_assign_hitbox_id(S, p, "fair4", 0, 0);
      mv_assign_hitbox_id(S, p, "fair4", 1, 1);
      active_tt_ff(S, pl, p);
      ml_sound_play("normalswing2");
    }
    if (pl->timer > 33 && pl->timer < 36) {
      pl->hitboxes.frame += 1;
    }
    if (pl->timer == 36) {
      mv_turnOffHitboxes(S, p);
    }
    if (pl->timer == 43) {
      pl->hitboxes.frame = 0;
      mv_assign_hitbox_id(S, p, "fair5", 0, 0);
      mv_assign_hitbox_id(S, p, "fair5", 1, 1);
      active_tt_ff(S, pl, p);
      ml_sound_play("normalswing2");
    }
    if (pl->timer > 43 && pl->timer < 46) {
      pl->hitboxes.frame += 1;
    }
    if (pl->timer == 46) {
      mv_turnOffHitboxes(S, p);
    }
    if (pl->timer == 50) {
      pl->phys.autoCancel = true;
    }
  }
  return AS_UNDEF;
}

static AsTri fx_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  if (pl->timer > 59) {
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
    mv_LANDINGATTACKAIRF.init(S, p, in, 0);
  }
  return AS_UNDEF;
}

const MlMoveDef fox_ATTACKAIRF = {"ATTACKAIRF", fx_init, fx_main,
                                  fx_interrupt, fx_land};
