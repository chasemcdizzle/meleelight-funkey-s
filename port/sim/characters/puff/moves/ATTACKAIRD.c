// ATTACKAIRD.c <- src/characters/puff/moves/ATTACKAIRD.js (M2 task 12)
// Multi-hit drill on a timer%3 switch (4<t<29) — fmod on the float timer, verbatim.
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
  strcpy(pl->actionState, "ATTACKAIRD");
  pl->timer = 0;
  pl->phys.autoCancel = true;
  pl->inAerial = true;
  pl->hasInAerial = true;
  mv_turnOffHitboxes(S, p);
  pf_assign_hitbox_id(S, p, "dair", 0, 0);
  pf_assign_hitbox_id(S, p, "dair", 1, 1);
  pf_assign_hitbox_id(S, p, "dair", 2, 2);
  pf_assign_hitbox_id(S, p, "dair", 3, 3);
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
    if (pl->timer == 4) {
      pl->phys.autoCancel = false;
    }
    if (pl->timer > 4 && pl->timer < 29) {
      const double m = fmod(pl->timer, 3);
      if (m == 2) {
        pl->hitboxes.active[0] = true;
        pl->hitboxes.active[1] = true;
        pl->hitboxes.active[2] = true;
        pl->hitboxes.active[3] = true;
        S->aliasHbActive[(int)p] = false; // fresh array upstream
        pl->hitboxes.frame = 0;
        ml_sound_play("normalswing2");
      } else if (m == 0) {
        pl->hitboxes.frame += 1;
      } else if (m == 1) {
        mv_turnOffHitboxes(S, p);
      }
    }
    if (pl->timer == 40) {
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
    mv_LANDINGATTACKAIRD.init(S, p, in, 0);
  }
  return AS_UNDEF;
}

const MlMoveDef puff_ATTACKAIRD = {"ATTACKAIRD", pf_init, pf_main,
                                   pf_interrupt, pf_land};
