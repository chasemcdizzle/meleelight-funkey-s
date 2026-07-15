// ATTACKAIRN.c <- src/characters/fox/moves/ATTACKAIRN.js (M2 task 8)
// NOTE upstream's timer 5-7 arm increments hitboxes.FRAMES (not frame) —
// the runtime-added rule-3 field: absent -> ToNumber(undefined)+1 = NaN.
#include "../moves.h"

static AsTri fx_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex);
static AsTri fx_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex);

static AsTri fx_init(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "ATTACKAIRN");
  pl->timer = 0;
  pl->phys.autoCancel = true;
  pl->hasInAerial = true;
  pl->inAerial = true;
  pl->hasIASATimer = true;
  pl->IASATimer = 41;
  mv_turnOffHitboxes(S, p);
  mv_assign_hitbox_id(S, p, "nair1", 0, 0);
  mv_assign_hitbox_id(S, p, "nair1", 1, 1);
  mv_assign_hitbox_id(S, p, "nair1", 2, 2);
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
    if (pl->timer == 3) {
      pl->phys.autoCancel = false;
    }

    if (pl->timer == 4) {
      pl->hitboxes.active[0] = true;
      pl->hitboxes.active[1] = true;
      pl->hitboxes.active[2] = true;
      pl->hitboxes.active[3] = false;
      S->aliasHbActive[(int)p] = false;
      pl->hitboxes.frame = 0;
      pl->phys.autoCancel = false;
      ml_sound_play("normalswing2");
    }
    if (pl->timer > 4 && pl->timer < 8) {
      // upstream: player[p].hitboxes.frames++ (the runtime-added field)
      pl->hitboxes.frames =
          pl->hitboxes.hasFrames ? pl->hitboxes.frames + 1 : js_nan();
      pl->hitboxes.hasFrames = true;
    }
    if (pl->timer == 8) {
      mv_assign_hitbox_id(S, p, "nair2", 0, 0);
      mv_assign_hitbox_id(S, p, "nair2", 1, 1);
      mv_assign_hitbox_id(S, p, "nair2", 2, 2);
      pl->hitboxes.frame = 0;
    }
    if (pl->timer > 8 && pl->timer < 32) {
      pl->hitboxes.frame += 1;
    }
    if (pl->timer == 32) {
      mv_turnOffHitboxes(S, p);
    }

    if (pl->timer == 38) {
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
    mv_LANDINGATTACKAIRN.init(S, p, in, 0);
  }
  return AS_UNDEF;
}

const MlMoveDef fox_ATTACKAIRN = {"ATTACKAIRN", fx_init, fx_main,
                                  fx_interrupt, fx_land};
