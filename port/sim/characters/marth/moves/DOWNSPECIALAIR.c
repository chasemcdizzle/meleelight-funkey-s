// DOWNSPECIALAIR.c <- src/characters/marth/moves/DOWNSPECIALAIR.js
// (M2 task 11 — counter, air). specialClank + onClank; land writes
// actionState directly.
#include "../moves.h"

#include <math.h>

static AsTri mr_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex);
static AsTri mr_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex);

static AsTri mr_init(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "DOWNSPECIALAIR");
  pl->timer = 0;
  pl->phys.fastfalled = false;
  pl->phys.cVel.y = 0;
  pl->phys.cVel.x *= 0.5;
  mv_turnOffHitboxes(S, p);
  mv_assign_hitbox_id(S, p, "downspecialair", 0, 0);
  mr_main(S, p, in, 0);
  return AS_UNDEF;
}

static AsTri mr_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  pl->timer += 1;
  if (mr_interrupt(S, p, in, 0) != AS_TRUE) {
    pl->phys.cVel.y -= 0.04;
    if (pl->phys.cVel.y < -1.2) {
      pl->phys.cVel.y = -1.2;
    }
    const double sign = js_sign(pl->phys.cVel.x);
    pl->phys.cVel.x -= sign * 0.0025;
    if (pl->phys.cVel.x * sign < 0) {
      pl->phys.cVel.x = 0;
    }
    if (pl->timer == 5) {
      ml_sound_play("marthcounter");
      pl->colourOverlayBool = true;
      strcpy(pl->colourOverlay, "white");
      pl->hitboxes.active[0] = true;
      pl->hitboxes.active[1] = false;
      pl->hitboxes.active[2] = false;
      pl->hitboxes.active[3] = false;
      S->aliasHbActive[(int)p] = false;
      pl->hitboxes.frame = 0;
    } else if (pl->timer == 30) {
      mv_turnOffHitboxes(S, p);
    }
    if (pl->timer >= 6 && pl->timer <= 28) {
      if (fmod(pl->timer, 6) < 2) {
        pl->colourOverlayBool = true;
        strcpy(pl->colourOverlay, "rgb(122, 122, 122)");
      } else if (fmod(pl->timer, 6) < 4) {
        pl->colourOverlayBool = true;
        strcpy(pl->colourOverlay, "rgb(200, 120, 255)");
      } else {
        pl->colourOverlayBool = false;
      }
    }
  }
  return AS_UNDEF;
}

static AsTri mr_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  if (pl->timer > 59) {
    if (pl->phys.grounded) {
      mv_WAIT.init(S, p, in, 0);
    } else {
      mv_FALL.init(S, p, in, 0);
    }
    return AS_TRUE;
  } else {
    return AS_FALSE;
  }
}

static AsTri mr_land(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)in; (void)ex;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "DOWNSPECIALGROUND");
  return AS_UNDEF;
}

AsTri marth_DOWNSPECIALAIR_onClank(MlSim *S, double p,
                                   const MlInputBuffer in[4], const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  pl->hit.hitlag = 11;
  pl->colourOverlayBool = false;
  marth_DOWNSPECIALAIR2.init(S, p, in, 0);
  return AS_UNDEF;
}

const MlMoveDef marth_DOWNSPECIALAIR = {"DOWNSPECIALAIR", mr_init, mr_main,
                                        mr_interrupt, mr_land};
