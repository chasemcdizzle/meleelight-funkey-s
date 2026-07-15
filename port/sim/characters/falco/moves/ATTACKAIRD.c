// ATTACKAIRD.c <- src/characters/falco/moves/ATTACKAIRD.js (M2 task 9)
// NOTE falco's dair swaps to the dair2 hitbox spec mid-move (===15 arm)
// — no fox-style modulo flicker.
#include "../moves.h"

static AsTri fc_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex);
static AsTri fc_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex);

static AsTri fc_init(MlSim *S, double p, const MlInputBuffer in[4],
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
  mv_assign_hitbox_id(S, p, "dair1", 0, 0);
  mv_assign_hitbox_id(S, p, "dair1", 1, 1);
  fc_main(S, p, in, 0);
  return AS_UNDEF;
}

static AsTri fc_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  pl->timer += 1;
  if (fc_interrupt(S, p, in, 0) != AS_TRUE) {
    as_fastfall((int)MV_CS(S, p), &pl->phys.cVel.y, &pl->phys.fastfalled,
                MV_IN(in, p));
    as_airDrift((int)MV_CS(S, p), &pl->phys.cVel.x, MV_IN(in, p));
    if (pl->timer == 4) {
      pl->phys.autoCancel = false;
    }

    if (pl->timer == 5) {
      pl->hitboxes.active[0] = true;
      pl->hitboxes.active[1] = true;
      pl->hitboxes.active[2] = false;
      pl->hitboxes.active[3] = false;
      S->aliasHbActive[(int)p] = false;
      pl->hitboxes.frame = 0;
      ml_sound_play("hitspin");
    } else if ((pl->timer > 5 && pl->timer < 15) ||
               (pl->timer > 15 && pl->timer < 25)) {
      pl->hitboxes.frame += 1;
    } else if (pl->timer == 15) {
      mv_assign_hitbox_id(S, p, "dair2", 0, 0);
      mv_assign_hitbox_id(S, p, "dair2", 1, 1);
      pl->hitboxes.frame = 0;
    } else if (pl->timer == 25) {
      mv_turnOffHitboxes(S, p);
    }

    if (pl->timer == 31) {
      pl->phys.autoCancel = true;
    }
  }
  return AS_UNDEF;
}

static AsTri fc_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
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

static AsTri fc_land(MlSim *S, double p, const MlInputBuffer in[4],
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

const MlMoveDef falco_ATTACKAIRD = {"ATTACKAIRD", fc_init, fc_main,
                                    fc_interrupt, fc_land};
