// ATTACKAIRF.c <- src/characters/marth/moves/ATTACKAIRF.js (M2 task 11)
// NOTE init assigns the fair hitboxes BEFORE turnOffHitboxes (verbatim
// upstream order), and the interrupt INLINES the aerial-IASA logic
// (checkForDoubleJump -> shared JUMPAERIALB/F; checkForAerials payload ->
// marth[a[1]].init) — marth aerials do NOT call checkForIASA.
#include "../moves.h"

static AsTri mr_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex);
static AsTri mr_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex);

static AsTri mr_init(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "ATTACKAIRF");
  pl->timer = 0;
  pl->phys.autoCancel = false;
  pl->hasInAerial = true;
  pl->inAerial = true;
  mv_assign_hitbox_id(S, p, "fair", 0, 0);
  mv_assign_hitbox_id(S, p, "fair", 1, 1);
  mv_assign_hitbox_id(S, p, "fair", 2, 2);
  mv_assign_hitbox_id(S, p, "fair", 3, 3);
  mv_turnOffHitboxes(S, p);
  mr_main(S, p, in, 0);
  return AS_UNDEF;
}

static AsTri mr_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  pl->timer += 1;
  if (mr_interrupt(S, p, in, 0) != AS_TRUE) {
    as_fastfall((int)MV_CS(S, p), &pl->phys.cVel.y, &pl->phys.fastfalled,
                MV_IN(in, p));
    as_airDrift((int)MV_CS(S, p), &pl->phys.cVel.x, MV_IN(in, p));
    if (pl->timer > 2 && pl->timer < 11) {
      ml_drawVfx_swing("swing", 0, 0, pl->phys.face, p, "FAIR", pl->timer - 3);
    }
    if (pl->timer == 4) {
      pl->hitboxes.active[0] = true;
      pl->hitboxes.active[1] = true;
      pl->hitboxes.active[2] = true;
      pl->hitboxes.active[3] = true;
      S->aliasHbActive[(int)p] = false;
      pl->hitboxes.frame = 0;
      ml_sound_play("sword3");
    }
    if (pl->timer > 4 && pl->timer < 8) {
      pl->hitboxes.frame += 1;
    }
    if (pl->timer == 8) {
      mv_turnOffHitboxes(S, p);
    }
    if (pl->timer == 27) {
      pl->phys.autoCancel = true;
    }
  }
  return AS_UNDEF;
}

static AsTri mr_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  const MlInput *i0 = &MV_IN(in, p)[0];
  if (pl->timer > 33) {
    mv_FALL.init(S, p, in, 0);
    return AS_TRUE;
  } else if (pl->timer > 29) {
    const AsPair a = as_checkForAerials(pl->phys.face, MV_IN(in, p));
    if (as_checkForDoubleJump(S->tapJumpOff[(int)p], MV_IN(in, p)) &&
        !pl->phys.doubleJumped) {
      if (i0->lsX * pl->phys.face < -0.3) {
        mv_JUMPAERIALB.init(S, p, in, 0);
      } else {
        mv_JUMPAERIALF.init(S, p, in, 0);
      }
      return AS_TRUE;
    } else if (a.flag) {
      marth_moves_init(S, mv_pair_str(&a), p, in);
      return AS_TRUE;
    } else {
      return AS_FALSE;
    }
  } else {
    return AS_FALSE;
  }
}

static AsTri mr_land(MlSim *S, double p, const MlInputBuffer in[4],
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

const MlMoveDef marth_ATTACKAIRF = {"ATTACKAIRF", mr_init, mr_main,
                                    mr_interrupt, mr_land};
