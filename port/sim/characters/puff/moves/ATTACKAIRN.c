// ATTACKAIRN.c <- src/characters/puff/moves/ATTACKAIRN.js (M2 task 12)
// t===7 increments hitboxes.FRAMES (upstream typo: the runtime-added plural, NOT frame) — carried verbatim.
#include "../moves.h"

static AsTri pf_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex);
static AsTri pf_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex);

static AsTri pf_land(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex);

static AsTri pf_init(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "ATTACKAIRN");
  pl->timer = 0;
  pl->phys.autoCancel = true;
  pl->inAerial = true;
  pl->hasInAerial = true;
  mv_turnOffHitboxes(S, p);
  pf_assign_hitbox_id(S, p, "nair1", 0, 0);
  pf_assign_hitbox_id(S, p, "nair1", 1, 1);
  pf_main(S, p, in, 0);
  return AS_UNDEF;
}

static AsTri pf_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  pl->timer += 1;
  if (pf_interrupt(S, p, in, 0) != AS_TRUE) {
    as_fastfall((int)MV_CS(S, p), &pl->phys.cVel.y, &pl->phys.fastfalled,
                MV_IN(in, p));
    as_airDrift((int)MV_CS(S, p), &pl->phys.cVel.x, MV_IN(in, p));
    if (pl->timer == 5) {
      pl->phys.autoCancel = false;
    }
    if (pl->timer == 6) {
      pl->hitboxes.active[0] = true;
      pl->hitboxes.active[1] = true;
      pl->hitboxes.active[2] = false;
      pl->hitboxes.active[3] = false;
      S->aliasHbActive[(int)p] = false; // fresh array upstream
      pl->hitboxes.frame = 0;
      pl->phys.autoCancel = false;
      ml_sound_play("normalswing2");
      // needs normalswing3 (upstream comment)
    }
    if (pl->timer == 7) {
      // player[p].hitboxes.frames++ — the runtime-added PLURAL (typo):
      // undefined + 1 = NaN on first touch, rule-8 semantics.
      const double old = pl->hitboxes.hasFrames ? pl->hitboxes.frames
                                                : js_nan();
      pl->hitboxes.frames = old + 1;
      pl->hitboxes.hasFrames = true;
    }
    if (pl->timer == 8) {
      pf_assign_hitbox_id(S, p, "nair2", 0, 0);
      pf_assign_hitbox_id(S, p, "nair2", 1, 1);
      pl->hitboxes.frame = 0;
    }
    if (pl->timer > 8 && pl->timer < 29) {
      pl->hitboxes.frame += 1;
    }
    if (pl->timer == 29) {
      mv_turnOffHitboxes(S, p);
    }
    if (pl->timer == 30) {
      pl->phys.autoCancel = true;
    }
  }
  return AS_UNDEF;
}

static AsTri pf_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
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

static AsTri pf_land(MlSim *S, double p, const MlInputBuffer in[4],
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

const MlMoveDef puff_ATTACKAIRN = {"ATTACKAIRN", pf_init, pf_main,
                                   pf_interrupt, pf_land};
