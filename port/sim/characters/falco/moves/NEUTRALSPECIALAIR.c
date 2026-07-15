// NEUTRALSPECIALAIR.c <- src/characters/falco/moves/NEUTRALSPECIALAIR.js
// (M2 task 9)
#include "../moves.h"

static AsTri fc_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex);
static AsTri fc_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex);

static AsTri fc_init(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "NEUTRALSPECIALAIR");
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
    as_fastfall((int)MV_CS(S, p), &pl->phys.cVel.y, &pl->phys.fastfalled,
                MV_IN(in, p));
    as_airDrift((int)MV_CS(S, p), &pl->phys.cVel.x, MV_IN(in, p));
    if (pl->timer >= 4 && pl->timer <= 16) {
      if (i0->b && !i1->b) {
        pl->phys.hasLaserCombo = true;
        pl->phys.laserCombo = true;
      }
    }
    if (pl->timer == 21) {
      if (pl->phys.hasLaserCombo && pl->phys.laserCombo) {
        pl->timer = 5;
        pl->phys.laserCombo = false;
      }
    }
    if (pl->timer == 7) {
      ml_sound_play("foxlasercock");
    }
    if (pl->timer == 13) {
      ml_sound_play("foxlaserfire");
      mv_drawVfx("laser");
      mv_article_laser_falco(S, p, 8, 9, 0, false);
    }
  }
  return AS_UNDEF;
}

static AsTri fc_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  if (pl->timer > 42) {
    mv_FALL.init(S, p, in, 0);
    return AS_TRUE;
  } else {
    return AS_FALSE;
  }
}

const MlMoveDef falco_NEUTRALSPECIALAIR = {"NEUTRALSPECIALAIR", fc_init,
                                         fc_main, fc_interrupt, 0};
