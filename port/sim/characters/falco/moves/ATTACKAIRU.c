// ATTACKAIRU.c <- src/characters/falco/moves/ATTACKAIRU.js (M2 task 9)
// NOTE upstream's main is an else-if CHAIN (timer arms are exclusive).
#include "../moves.h"

static AsTri fc_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex);
static AsTri fc_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex);

static AsTri fc_init(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "ATTACKAIRU");
  pl->timer = 0;
  pl->phys.autoCancel = true;
  pl->hasInAerial = true;
  pl->inAerial = true;
  pl->hasIASATimer = true;
  pl->IASATimer = 35;
  mv_turnOffHitboxes(S, p);
  mv_assign_hitbox_id(S, p, "upair1", 0, 0);
  mv_assign_hitbox_id(S, p, "upair1", 1, 1);
  mv_assign_hitbox_id(S, p, "upair1", 2, 2);
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
    if (pl->timer == 7) {
      pl->phys.autoCancel = false;
    } else if (pl->timer == 8) {
      pl->hitboxes.active[0] = true;
      pl->hitboxes.active[1] = true;
      pl->hitboxes.active[2] = true;
      pl->hitboxes.active[3] = false;
      S->aliasHbActive[(int)p] = false;
      pl->hitboxes.frame = 0;
      ml_sound_play("sword1");
    } else if (pl->timer == 9) {
      pl->hitboxes.frame += 1;
    } else if (pl->timer == 10) {
      mv_turnOffHitboxes(S, p);
    } else if (pl->timer == 11) {
      mv_assign_hitbox_id(S, p, "upair2", 0, 0);
      mv_assign_hitbox_id(S, p, "upair2", 1, 1);
      mv_assign_hitbox_id(S, p, "upair2", 2, 2);
      pl->hitboxes.active[0] = true;
      pl->hitboxes.active[1] = true;
      pl->hitboxes.active[2] = true;
      pl->hitboxes.active[3] = false;
      S->aliasHbActive[(int)p] = false;
      pl->hitboxes.frame = 0;
      ml_sound_play("normalswing2");
    } else if (pl->timer > 11 && pl->timer < 15) {
      pl->hitboxes.frame += 1;
    } else if (pl->timer == 15) {
      mv_turnOffHitboxes(S, p);
    } else if (pl->timer == 27) {
      pl->phys.autoCancel = true;
    }
  }
  return AS_UNDEF;
}

static AsTri fc_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  if (pl->timer > 39) {
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
    mv_LANDINGATTACKAIRU.init(S, p, in, 0);
  }
  return AS_UNDEF;
}

const MlMoveDef falco_ATTACKAIRU = {"ATTACKAIRU", fc_init, fc_main,
                                  fc_interrupt, fc_land};
