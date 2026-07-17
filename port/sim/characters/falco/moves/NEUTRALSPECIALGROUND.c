// NEUTRALSPECIALGROUND.c <- src/characters/falco/moves/NEUTRALSPECIALGROUND.js
// (M2 task 9). articles.LASER.init crosses the task-13 seam.
#include "../moves.h"

static AsTri fc_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex);
static AsTri fc_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex);

static AsTri fc_init(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "NEUTRALSPECIALGROUND");
  pl->timer = 0;
  pl->phys.hasLaserCombo = true;
  pl->phys.laserCombo = false;
  fc_main(S, p, in, 0);
  return AS_UNDEF;
}

static AsTri fc_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  const MlInput *i0 = &MV_IN(in, p)[0];
  const MlInput *i1 = &MV_IN(in, p)[1];
  pl->timer += 1;
  if (fc_interrupt(S, p, in, 0) != AS_TRUE) {
    as_reduceByTraction(false, (int)MV_CS(S, p), &pl->phys.cVel.x);
    if (pl->timer >= 15 && pl->timer <= 28) {
      if (i0->b && !i1->b) {
        pl->phys.hasLaserCombo = true;
        pl->phys.laserCombo = true;
      }
    }
    if (pl->timer == 31) {
      if (pl->phys.hasLaserCombo && pl->phys.laserCombo) {
        pl->timer = 7;
        pl->phys.laserCombo = false;
      }
    }
    if (pl->timer == 9) {
      ml_sound_play("foxlasercock");
    }
    if (pl->timer == 23) {
      ml_sound_play("foxlaserfire");
      ml_drawVfx_laser("laser", pl->phys.pos.x + (8 * pl->phys.face),
                       pl->phys.pos.y + 7, pl->phys.face, 0, 137, 255, 255,
                       157, 255, 255);
      mv_article_laser_falco(S, p, 8, 7, 0, false);
    }
  }
  return AS_UNDEF;
}

static AsTri fc_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  if (pl->timer > 57) {
    mv_WAIT.init(S, p, in, 0);
    return AS_TRUE;
  } else {
    return AS_FALSE;
  }
}

const MlMoveDef falco_NEUTRALSPECIALGROUND = {"NEUTRALSPECIALGROUND", fc_init,
                                            fc_main, fc_interrupt, 0};
