// NEUTRALSPECIALGROUNDTURN.c <- src/characters/puff/moves/NEUTRALSPECIALGROUNDTURN.js (M2 task 12)
// Rollout's turn state: timer runs in 3s with a >30 wrap; both interrupt
// exit arms return TRUE (back into NEUTRALSPECIALGROUND). NON-phase
// onPlayerHit delegates to NSA's.
#include "../moves.h"

static AsTri pf_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex);
static AsTri pf_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex);

static AsTri pf_init(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "NEUTRALSPECIALGROUNDTURN");
  pl->timer = 0;
  pf_set_rollOutTurnTimer(pl, 0);
  pl->phys.face *= -1;
  ml_sound_play("rolloutlaunch");
  mv_turnOffHitboxes(S, p);
  pf_main(S, p, in, 0);
  return AS_UNDEF;
}

static AsTri pf_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  pl->timer += 3;
  if (pl->timer > 30) {
    pl->timer = 3;
  }
  pf_set_rollOutTurnTimer(pl, pf_rollOutTurnTimer(pl) + 1);
  pl->phys.rollOutDistance += 1;
  if (pf_interrupt(S, p, in, 0) != AS_TRUE) {
    pl->phys.cVel.x =
        (pf_rollOutVel(pl) * pl->phys.face * -1) -
        (pf_rollOutVel(pl) * 0.045 * pf_rollOutTurnTimer(pl) *
         pl->phys.face * -1);
    if (fmod(pl->phys.rollOutDistance, 5) == 0) {
      mv_drawVfx("dashDust");
    }
  }
  return AS_UNDEF;
}

static AsTri pf_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex) {
  (void)in; (void)ex;
  MlPlayer *pl = mv_player(S, p);
  if (pl->phys.rollOutDistance > 100) {
    strcpy(pl->actionState, "NEUTRALSPECIALGROUND");
    pl->timer = 46;
    return AS_TRUE;
  } else if (pf_rollOutTurnTimer(pl) > 28) {
    pl->phys.cVel.x = pf_rollOutVel(pl) * pl->phys.face;
    strcpy(pl->actionState, "NEUTRALSPECIALGROUND");
    pl->timer = 15 + pl->timer;
    if (pf_rollOutCharge(pl) >= 19) {
      pl->hitboxes.frame = 0;
      pf_assign_hitbox_id(S, p, "neutralspecialground", 0, 0);
      pf_assign_hitbox_id(S, p, "neutralspecialground", 1, 1);
      pf_assign_hitbox_id(S, p, "neutralspecialground", 2, 2);
      pl->hitboxes.active[0] = true;
      pl->hitboxes.active[1] = true;
      pl->hitboxes.active[2] = true;
      pl->hitboxes.active[3] = false;
      S->aliasHbActive[(int)p] = false; // fresh array upstream
    }
    ml_sound_play("stronghit");
    return AS_TRUE;
  } else {
    return AS_FALSE;
  }
}

// special phase surface (hitDetection.js:493 specialOnHit)
AsTri puff_NEUTRALSPECIALGROUNDTURN_onPlayerHit(MlSim *S, double p,
                                                const MlInputBuffer in[4],
                                                const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "NEUTRALSPECIALAIR");
  puff_NEUTRALSPECIALAIR_onPlayerHit(S, p, in, 0);
  return AS_UNDEF;
}

const MlMoveDef puff_NEUTRALSPECIALGROUNDTURN = {
    "NEUTRALSPECIALGROUNDTURN", pf_init, pf_main, pf_interrupt, 0};
