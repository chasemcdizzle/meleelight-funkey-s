// SIDESPECIALGROUND.c <- src/characters/fox/moves/SIDESPECIALGROUND.js
// (M2 task 8). articles.ILLUSION.init crosses the task-13 seam.
#include "../moves.h"

static AsTri fx_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex);
static AsTri fx_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex);

static AsTri fx_init(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "SIDESPECIALGROUND");
  pl->timer = 0;
  pl->phys.cVel.x = 0;
  pl->phys.landingMultiplier = 1.5;
  ml_drawVfx("dashDust", pl->phys.pos.x, pl->phys.pos.y, pl->phys.face);
  mv_turnOffHitboxes(S, p);
  ml_sound_play("star");
  fx_main(S, p, in, 0);
  return AS_UNDEF;
}

static AsTri fx_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  const MlInput *i0 = &MV_IN(in, p)[0];
  const MlInput *i1 = &MV_IN(in, p)[1];
  const MlInput *i2 = &MV_IN(in, p)[2];
  pl->timer += 1;
  if (fx_interrupt(S, p, in, 0) != AS_TRUE) {
    if (pl->phys.grounded) {
      if (pl->timer == 21) {
        mv_article_illusion(S, p, 1);
        pl->phys.cVel.x = 18.72 * pl->phys.face;
        if ((i0->b || i1->b) && !i2->b) {
          pl->timer = 24;
        }
      } else if (pl->timer == 22 || pl->timer == 23) {
        if (i0->b && !i1->b) {
          pl->timer = 24;
        }
      }
      if (pl->timer == 24) {
        pl->phys.cVel.x = 2.1 * pl->phys.face;
      }
      if (pl->timer > 24) {
        pl->phys.cVel.x -= 0.1 * pl->phys.face;
        if (pl->phys.cVel.x * pl->phys.face < 0) {
          pl->phys.cVel.x = 0;
        }
      }

      if (pl->timer == 20) {
        ml_sound_play("foxillusion1");
        ml_sound_play("foxillusion2");
      }
    } else {
      strcpy(pl->actionState, "SIDESPECIALAIR");
      pl->timer -= 1;
      fox_SIDESPECIALAIR.main_(S, p, in, 0);
    }

    if (pl->timer >= 21 && pl->timer <= 24) {
      ml_drawVfx("illusion", pl->phys.posPrev.x, pl->phys.posPrev.y,
                 pl->phys.face);
    }
  }
  return AS_UNDEF;
}

static AsTri fx_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  if (pl->timer > 63) {
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

static AsTri fx_land(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  // upstream land is an empty function
  (void)S; (void)p; (void)in; (void)ex;
  return AS_UNDEF;
}

const MlMoveDef fox_SIDESPECIALGROUND = {"SIDESPECIALGROUND", fx_init,
                                         fx_main, fx_interrupt, fx_land};
