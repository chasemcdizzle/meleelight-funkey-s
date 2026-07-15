// SIDESPECIALGROUND.c <- src/characters/falcon/moves/SIDESPECIALGROUND.js
// (M2 task 10). Raptor boost (grounded). setVelocities1/setVelocities2
// come from the mvData falcon dump. UPSTREAM WRITES THE MOVE TABLE at
// runtime: init does `this.canEdgeCancel = false` and the timer>16 arm
// `this.canEdgeCancel = true` — a SCALAR table write (outside the
// array-only mvData dump), modeled as module state below; its only sim
// READER is physics' per-state flag lookup (task 17 wires it — within
// this cluster the state is write-only). onPlayerHit (hitDetection.js:493
// specialOnHit arm) sets phys.raptorBoost, the interrupt's raptorBoost
// arm chains SIDESPECIALGROUNDHIT. `articles` import is dead upstream.
#include "../moves.h"

static bool g_ssg_canEdgeCancel = false; // this.canEdgeCancel (runtime)

void mv_falcon_ssg_set_canEdgeCancel(bool v) { g_ssg_canEdgeCancel = v; }

static AsTri fc4_main(MlSim *S, double p, const MlInputBuffer in[4],
                      const MvX *ex);
static AsTri fc4_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                           const MvX *ex);

static AsTri fc4_init(MlSim *S, double p, const MlInputBuffer in[4],
                      const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "SIDESPECIALGROUND");
  pl->timer = 0;
  pl->phys.cVel.x = 0;
  pl->phys.landingMultiplier = 1.5;
  pl->phys.raptorBoost = false;
  mv_assign_hitbox_id(S, p, "raptorboostground", 0, 0);
  mv_assign_hitbox_id(S, p, "raptorboostground", 1, 1);
  mv_assign_hitbox_id(S, p, "raptorboostground", 2, 2);
  mv_falcon_ssg_set_canEdgeCancel(false); // this.canEdgeCancel = false
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
    if (pl->timer <= 4) {
      pl->phys.cVel.x = mv_falcon_arr("SIDESPECIALGROUND", "setVelocities1",
                                      pl->timer - 1) * pl->phys.face;
    } else if (pl->timer <= 16) {
      pl->phys.cVel.x = 0;
    } else {
      mv_falcon_ssg_set_canEdgeCancel(true); // this.canEdgeCancel = true
      pl->phys.cVel.x = mv_falcon_arr("SIDESPECIALGROUND", "setVelocities2",
                                      pl->timer - 17) * pl->phys.face;
    }
    if (pl->timer == 15) {
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
    falcon_SIDESPECIALGROUNDHIT.init(S, p, in, 0);
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
  (void)S; (void)p; (void)in; (void)ex; // empty land upstream
  return AS_UNDEF;
}

// special phase surface (moves.h: registered via falcon_special_phase)
AsTri falcon_SIDESPECIALGROUND_onPlayerHit(MlSim *S, double p,
                                           const MlInputBuffer in[4],
                                           const MvX *ex) {
  (void)in; (void)ex;
  mv_player(S, p)->phys.raptorBoost = true;
  return AS_UNDEF;
}

const MlMoveDef falcon_SIDESPECIALGROUND = {
    "SIDESPECIALGROUND", fc4_init, fc4_main, fc4_interrupt, fc4_land};
