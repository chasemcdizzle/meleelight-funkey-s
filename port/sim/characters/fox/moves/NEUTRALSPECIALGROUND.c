// NEUTRALSPECIALGROUND.c <- src/characters/fox/moves/NEUTRALSPECIALGROUND.js
// (M2 task 8). articles.LASER.init crosses the task-13 seam.
#include "../moves.h"

static AsTri fx_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex);
static AsTri fx_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex);

static AsTri fx_init(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "NEUTRALSPECIALGROUND");
  pl->timer = 0;
  pl->phys.hasLaserCombo = true;
  pl->phys.laserCombo = false;
  fx_main(S, p, in, 0);
  return AS_UNDEF;
}

static AsTri fx_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  const MlInput *i0 = &MV_IN(in, p)[0];
  const MlInput *i1 = &MV_IN(in, p)[1];
  pl->timer += 1;
  if (fx_interrupt(S, p, in, 0) != AS_TRUE) {
    as_reduceByTraction(false, (int)MV_CS(S, p), &pl->phys.cVel.x);
    if (pl->timer >= 4 && pl->timer <= 16) {
      if (i0->b && !i1->b) {
        pl->phys.hasLaserCombo = true;
        pl->phys.laserCombo = true;
      }
    }
    if (pl->timer == 17) {
      if (pl->phys.hasLaserCombo && pl->phys.laserCombo) {
        pl->timer = 7;
        pl->phys.laserCombo = false;
      }
    }
    if (pl->timer == 9) {
      ml_sound_play("foxlasercock");
    }
    if (pl->timer == 12) {
      ml_sound_play("foxlaserfire");
      mv_drawVfx("laser");
      mv_article_laser(S, p, 8, 7, 0);
    }
    if (pl->timer == 37) {
      ml_sound_play("foxlaserholster");
    }
  }
  return AS_UNDEF;
}

static AsTri fx_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  if (pl->timer > 40) {
    mv_WAIT.init(S, p, in, 0);
    return AS_TRUE;
  } else {
    return AS_FALSE;
  }
}

const MlMoveDef fox_NEUTRALSPECIALGROUND = {"NEUTRALSPECIALGROUND", fx_init,
                                            fx_main, fx_interrupt, 0};
