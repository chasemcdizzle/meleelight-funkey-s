// ATTACKAIRD.c <- src/characters/falcon/moves/ATTACKAIRD.js (M2 task 10)
// falconshout5 in init; the 17-20 arm increments hitboxes.FRAMES (the
// runtime-added rule-3 field, ATTACKAIRN's NaN quirk family).
#include "../moves.h"

static AsTri fc4_main(MlSim *S, double p, const MlInputBuffer in[4],
                      const MvX *ex);
static AsTri fc4_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                           const MvX *ex);

static AsTri fc4_init(MlSim *S, double p, const MlInputBuffer in[4],
                      const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "ATTACKAIRD");
  pl->timer = 0;
  pl->phys.autoCancel = true;
  pl->hasInAerial = true;
  pl->inAerial = true;
  pl->hasIASATimer = true;
  pl->IASATimer = 38;
  mv_turnOffHitboxes(S, p);
  mv_assign_hitbox_id(S, p, "dair", 0, 0);
  mv_assign_hitbox_id(S, p, "dair", 1, 1);
  mv_assign_hitbox_id(S, p, "dair", 2, 2);
  ml_sound_play("falconshout5");
  fc4_main(S, p, in, 0);
  return AS_UNDEF;
}

static AsTri fc4_main(MlSim *S, double p, const MlInputBuffer in[4],
                      const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  pl->timer += 1;
  if (fc4_interrupt(S, p, in, 0) != AS_TRUE) {
    as_fastfall((int)MV_CS(S, p), &pl->phys.cVel.y, &pl->phys.fastfalled,
                MV_IN(in, p));
    as_airDrift((int)MV_CS(S, p), &pl->phys.cVel.x, MV_IN(in, p));
    if (pl->timer == 3) {
      pl->phys.autoCancel = false;
    }

    if (pl->timer == 16) {
      pl->hitboxes.active[0] = true;
      pl->hitboxes.active[1] = true;
      pl->hitboxes.active[2] = true;
      pl->hitboxes.active[3] = false;
      S->aliasHbActive[(int)p] = false; // fresh array upstream
      pl->hitboxes.frame = 0;
      pl->phys.autoCancel = false;
      ml_sound_play("normalswing1");
    }
    if (pl->timer > 16 && pl->timer < 21) {
      // upstream: player[p].hitboxes.frames++ (the runtime-added field)
      pl->hitboxes.frames =
          pl->hitboxes.hasFrames ? pl->hitboxes.frames + 1 : js_nan();
      pl->hitboxes.hasFrames = true;
    }
    if (pl->timer == 21) {
      mv_turnOffHitboxes(S, p);
    }

    if (pl->timer == 36) {
      pl->phys.autoCancel = true;
    }
  }
  return AS_UNDEF;
}

static AsTri fc4_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                           const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  if (pl->timer > 44) {
    mv_FALL.init(S, p, in, 0);
    return AS_TRUE;
  } else if (mv_checkForIASA(S, p, in, true) == AS_TRUE) {
    return AS_TRUE;
  } else {
    return AS_FALSE;
  }
}

static AsTri fc4_land(MlSim *S, double p, const MlInputBuffer in[4],
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

const MlMoveDef falcon_ATTACKAIRD = {"ATTACKAIRD", fc4_init, fc4_main,
                                     fc4_interrupt, fc4_land};
