// SIDESPECIALAIR.c <- src/characters/falco/moves/SIDESPECIALAIR.js
// (M2 task 9). articles.ILLUSION.init (type 0, isFox:false) crosses the
// task-13 seam. NOTE falco's phantasm has NO grounded-arm recursion (the
// fox SIDESPECIALAIR quirk does not exist here); the ground state is
// reached via land's actionState write.
#include "../moves.h"

static AsTri fc_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex);
static AsTri fc_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex);

static AsTri fc_init(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "SIDESPECIALAIR");
  pl->timer = 0;
  pl->phys.cVel.x *= 0.667;
  pl->phys.cVel.y = 0;
  pl->phys.landingMultiplier = 1.5;
  mv_drawVfx("dashDust");
  mv_turnOffHitboxes(S, p);
  ml_sound_play("star");
  fc_main(S, p, in, 0);
  return AS_UNDEF;
}

static AsTri fc_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  const MlInput *i0 = &MV_IN(in, p)[0];
  const MlInput *i1 = &MV_IN(in, p)[1];
  const MlInput *i2 = &MV_IN(in, p)[2];
  pl->timer += 1;
  if (fc_interrupt(S, p, in, 0) != AS_TRUE) {
    if (pl->timer <= 15) {
      if (pl->phys.cVel.x != 0) {
        const double dir = js_sign(pl->phys.cVel.x);
        pl->phys.cVel.x -= dir * 0.05;
        if (pl->phys.cVel.x * dir < 0) {
          pl->phys.cVel.x = 0;
        }
      }
    }
    if (pl->timer >= 25) {
      pl->phys.cVel.y -= 0.08;
    }

    if (pl->timer == 16) {
      ml_sound_play("phantasm");
      ml_sound_play("phantasmshout");
    }

    if (pl->timer == 17) {
      pl->phys.cVel.x = 16.50 * pl->phys.face;
    }

    if (pl->timer == 18) {
      mv_article_illusion_falco(S, p, 0);
      if ((i0->b || i1->b) && !i2->b) {
        pl->timer = 20;
      }
    } else if (pl->timer >= 16 && pl->timer < 20) {
      if (i0->b && !i1->b) {
        pl->timer = 20;
      }
    }
    if (pl->timer == 20) {
      pl->phys.cVel.x = 2 * pl->phys.face;
    }
    if (pl->timer > 20) {
      pl->phys.cVel.x -= 0.07 * pl->phys.face;
      if (pl->phys.cVel.x * pl->phys.face < 0) {
        pl->phys.cVel.x = 0;
      }
    }

    if (pl->timer >= 18 && pl->timer <= 21) {
      mv_drawVfx("phantasm");
    }
  }
  return AS_UNDEF;
}

static AsTri fc_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  if (pl->timer > 58) {
    mv_FALLSPECIAL.init(S, p, in, 0);
    return AS_TRUE;
  } else {
    return AS_FALSE;
  }
}

static AsTri fc_land(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  if (pl->timer >= 20) {
    mv_LANDINGFALLSPECIAL.init(S, p, in, 0);
  } else {
    strcpy(pl->actionState, "SIDESPECIALGROUND");
  }
  return AS_UNDEF;
}

const MlMoveDef falco_SIDESPECIALAIR = {"SIDESPECIALAIR", fc_init, fc_main,
                                        fc_interrupt, fc_land};
