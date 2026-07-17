// ATTACKAIRB.c <- src/characters/marth/moves/ATTACKAIRB.js (M2 task 11)
// (inline aerial-IASA arm at >34, face flip at timer 30 — verbatim)
#include "../moves.h"

static AsTri mr_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex);
static AsTri mr_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex);

static AsTri mr_init(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "ATTACKAIRB");
  pl->timer = 0;
  pl->phys.autoCancel = false;
  pl->hasInAerial = true;
  pl->inAerial = true;
  mv_turnOffHitboxes(S, p);
  mv_assign_hitbox_id(S, p, "bair", 0, 0);
  mv_assign_hitbox_id(S, p, "bair", 1, 1);
  mv_assign_hitbox_id(S, p, "bair", 2, 2);
  mv_assign_hitbox_id(S, p, "bair", 3, 3);
  mr_main(S, p, in, 0);
  return AS_UNDEF;
}

static AsTri mr_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  pl->timer += 1;
  if (mr_interrupt(S, p, in, 0) != AS_TRUE) {
    if (pl->timer == 30) {
      pl->phys.face *= -1;
    }
    as_fastfall((int)MV_CS(S, p), &pl->phys.cVel.y, &pl->phys.fastfalled,
                MV_IN(in, p));
    as_airDrift((int)MV_CS(S, p), &pl->phys.cVel.x, MV_IN(in, p));
    if (pl->timer > 2 && pl->timer < 12) {
      ml_drawVfx_swing("swing", 0, 0, pl->phys.face, p, "BAIR", pl->timer - 3);
    }
    if (pl->timer == 7) {
      pl->hitboxes.active[0] = true;
      pl->hitboxes.active[1] = true;
      pl->hitboxes.active[2] = true;
      pl->hitboxes.active[3] = true;
      S->aliasHbActive[(int)p] = false;
      pl->hitboxes.frame = 0;
      ml_sound_play("sword3");
    }
    if (pl->timer > 7 && pl->timer < 12) {
      pl->hitboxes.frame += 1;
    }
    if (pl->timer == 12) {
      mv_turnOffHitboxes(S, p);
    }
    if (pl->timer == 32) {
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
  if (pl->timer > 39) {
    mv_FALL.init(S, p, in, 0);
    return AS_TRUE;
  } else if (pl->timer > 34) {
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
    mv_LANDINGATTACKAIRB.init(S, p, in, 0);
  }
  return AS_UNDEF;
}

const MlMoveDef marth_ATTACKAIRB = {"ATTACKAIRB", mr_init, mr_main,
                                    mr_interrupt, mr_land};
