// DOWNSPECIALGROUND.c <- src/characters/marth/moves/DOWNSPECIALGROUND.js
// (M2 task 11 — counter, ground). Carries specialClank + the NON-phase
// onClank(p, input) (hitDetection.js:71-72's specialClank arm), routed
// through marth_special_phase. colourOverlay cycles literal strings.
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
  strcpy(pl->actionState, "DOWNSPECIALGROUND");
  pl->timer = 0;
  mv_turnOffHitboxes(S, p);
  mv_assign_hitbox_id(S, p, "downspecialground", 0, 0);
  mr_main(S, p, in, 0);
  return AS_UNDEF;
}

static AsTri mr_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  pl->timer += 1;
  if (mr_interrupt(S, p, in, 0) != AS_TRUE) {
    as_reduceByTraction(true, (int)MV_CS(S, p), &pl->phys.cVel.x);
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

AsTri marth_DOWNSPECIALGROUND_onClank(MlSim *S, double p,
                                      const MlInputBuffer in[4],
                                      const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  pl->hit.hitlag = 11;
  pl->colourOverlayBool = false;
  marth_DOWNSPECIALGROUND2.init(S, p, in, 0);
  return AS_UNDEF;
}

const MlMoveDef marth_DOWNSPECIALGROUND = {"DOWNSPECIALGROUND", mr_init,
                                           mr_main, mr_interrupt, 0};
