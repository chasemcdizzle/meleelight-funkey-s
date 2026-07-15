// SIDESPECIALAIR.c <- src/characters/falcon/moves/SIDESPECIALAIR.js
// (M2 task 10). Raptor boost (aerial). setVelocities from the mvData
// falcon dump; NO init recursion (fox's grounded-arm quirk is absent —
// falcon zeroes cVel instead). onPlayerHit sets phys.raptorBoost; the
// raptorBoost interrupt arm chains SIDESPECIALAIRHIT. Dead `articles`
// import upstream.
#include "../moves.h"

static AsTri fc4_main(MlSim *S, double p, const MlInputBuffer in[4],
                      const MvX *ex);
static AsTri fc4_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                           const MvX *ex);

static AsTri fc4_init(MlSim *S, double p, const MlInputBuffer in[4],
                      const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "SIDESPECIALAIR");
  pl->timer = 0;
  pl->phys.cVel.x = 0;
  pl->phys.cVel.y = 0;
  mv_assign_hitbox_id(S, p, "raptorboostair", 0, 0);
  mv_assign_hitbox_id(S, p, "raptorboostair", 1, 1);
  mv_assign_hitbox_id(S, p, "raptorboostair", 2, 2);
  pl->phys.raptorBoost = false;
  mv_turnOffHitboxes(S, p);
  ml_sound_play("raptorboost");
  fc4_main(S, p, in, 0);
  return AS_UNDEF;
}

static AsTri fc4_main(MlSim *S, double p, const MlInputBuffer in[4],
                      const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  pl->timer += 1;
  if (fc4_interrupt(S, p, in, 0) != AS_TRUE) {
    if (pl->timer >= 16) {
      pl->phys.cVel.x = mv_falcon_arr("SIDESPECIALAIR", "setVelocities",
                                      pl->timer - 16) * pl->phys.face;
    }
    if (pl->timer >= 30) {
      pl->phys.cVel.y -= 0.05;
    }
    if (pl->timer == 17) {
      pl->hitboxes.active[0] = true;
      pl->hitboxes.active[1] = true;
      pl->hitboxes.active[2] = true;
      pl->hitboxes.active[3] = false;
      S->aliasHbActive[(int)p] = false; // fresh array upstream
      pl->hitboxes.frame = 0;
      ml_sound_play("fireweakhit");
    }
    if (pl->timer == 35) {
      mv_turnOffHitboxes(S, p);
    }
  }
  return AS_UNDEF;
}

static AsTri fc4_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                           const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  if (pl->phys.raptorBoost) {
    falcon_SIDESPECIALAIRHIT.init(S, p, in, 0);
    return AS_TRUE;
  } else if (pl->timer > 79) {
    if (pl->phys.grounded) {
      mv_WAIT.init(S, p, in, 0);
    } else {
      mv_FALLSPECIAL.init(S, p, in, 0);
    }
    return AS_TRUE;
  } else {
    return AS_FALSE;
  }
}

static AsTri fc4_land(MlSim *S, double p, const MlInputBuffer in[4],
                      const MvX *ex) {
  (void)ex;
  mv_LANDINGFALLSPECIAL.init(S, p, in, 0);
  return AS_UNDEF;
}

// special phase surface (moves.h: registered via falcon_special_phase)
AsTri falcon_SIDESPECIALAIR_onPlayerHit(MlSim *S, double p,
                                        const MlInputBuffer in[4],
                                        const MvX *ex) {
  (void)in; (void)ex;
  mv_player(S, p)->phys.raptorBoost = true;
  return AS_UNDEF;
}

const MlMoveDef falcon_SIDESPECIALAIR = {
    "SIDESPECIALAIR", fc4_init, fc4_main, fc4_interrupt, fc4_land};
