// ATTACKAIRF.c <- src/characters/puff/moves/ATTACKAIRF.js (M2 task 12)
// hitbox assigns come BEFORE turnOffHitboxes in init (verbatim order); interrupt carries the multijump/aerial tail (>34).
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
  strcpy(pl->actionState, "ATTACKAIRF");
  pl->timer = 0;
  pl->phys.autoCancel = true;
  pl->inAerial = true;
  pl->hasInAerial = true;
  pf_assign_hitbox_id(S, p, "fair1", 0, 0);
  pf_assign_hitbox_id(S, p, "fair1", 1, 1);
  mv_turnOffHitboxes(S, p);
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
    if (pl->timer == 7) {
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
    if (pl->timer == 8) {
      pl->hitboxes.frame += 1;
    }
    if (pl->timer == 9) {
      pl->hitboxes.frame = 0;
      pf_assign_hitbox_id(S, p, "fair2", 0, 0);
      pf_assign_hitbox_id(S, p, "fair2", 1, 1);
    }
    if (pl->timer > 9 && pl->timer < 23) {
      pl->hitboxes.frame += 1;
    }
    if (pl->timer == 23) {
      mv_turnOffHitboxes(S, p);
    }
    if (pl->timer == 35) {
      pl->phys.autoCancel = true;
    }
  }
  return AS_UNDEF;
}

static AsTri pf_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  if (pl->timer > 39) {
    mv_FALL.init(S, p, in, 0);
    return AS_TRUE;
  } else if (pl->timer > 34) {
    const AsPair a = as_checkForAerials(pl->phys.face, MV_IN(in, p));
    if (as_checkForMultiJump(S->tapJumpOff[(int)p], MV_IN(in, p)) &&
        pl->phys.jumpsUsed < 5) {
      puff_JUMPAERIALF.init(S, p, in, 0);
      return AS_TRUE;
    } else if (a.flag) {
      puff_moves_init(S, mv_pair_str(&a), p, in);
      return AS_TRUE;
    } else {
      return AS_FALSE;
    }
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
    mv_LANDINGATTACKAIRF.init(S, p, in, 0);
  }
  return AS_UNDEF;
}

const MlMoveDef puff_ATTACKAIRF = {"ATTACKAIRF", pf_init, pf_main,
                                   pf_interrupt, pf_land};
