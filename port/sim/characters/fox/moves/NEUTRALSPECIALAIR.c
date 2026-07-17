// NEUTRALSPECIALAIR.c <- src/characters/fox/moves/NEUTRALSPECIALAIR.js
// (M2 task 8)
#include "../moves.h"

static AsTri fx_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex);
static AsTri fx_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex);

static AsTri fx_init(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "NEUTRALSPECIALAIR");
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
    as_fastfall((int)MV_CS(S, p), &pl->phys.cVel.y, &pl->phys.fastfalled,
                MV_IN(in, p));
    as_airDrift((int)MV_CS(S, p), &pl->phys.cVel.x, MV_IN(in, p));
    if (pl->timer >= 4 && pl->timer <= 14) {
      if (i0->b && !i1->b) {
        pl->phys.hasLaserCombo = true;
        pl->phys.laserCombo = true;
      }
    }
    if (pl->timer == 15) {
      if (pl->phys.hasLaserCombo && pl->phys.laserCombo) {
        pl->timer = 5;
        pl->phys.laserCombo = false;
      }
    }
    if (pl->timer == 7) {
      ml_sound_play("foxlasercock");
    }
    if (pl->timer == 10) {
      ml_sound_play("foxlaserfire");
      ml_drawVfx_laser("laser", pl->phys.pos.x + (8 * pl->phys.face),
                       pl->phys.pos.y + 9, pl->phys.face, 0, 255, 59, 59, 255,
                       57, 87);
      mv_article_laser(S, p, 8, 9, 0);
    }
    if (pl->timer == 30) {
      ml_sound_play("foxlaserholster");
    }
  }
  return AS_UNDEF;
}

static AsTri fx_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  if (pl->timer > 36) {
    mv_FALL.init(S, p, in, 0);
    return AS_TRUE;
  } else {
    return AS_FALSE;
  }
}

const MlMoveDef fox_NEUTRALSPECIALAIR = {"NEUTRALSPECIALAIR", fx_init,
                                         fx_main, fx_interrupt, 0};
