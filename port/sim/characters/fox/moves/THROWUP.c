// THROWUP.c <- src/characters/fox/moves/THROWUP.js (M2 task 8)
// Victim dispatch crosses the actionStates TABLE (mv_dispatch: fox victim
// = the registered fox body, other chars = the driver's mdispatch seam).
// hitQueue.push crosses mv_hq_push6; articles.LASER.init the article seam.
#include "../moves.h"

static AsTri fx_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex);
static AsTri fx_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex);

static AsTri fx_init(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "THROWUP");
  pl->timer = 0;
  const double grabbing = pl->phys.grabbing;
  if (grabbing == -1) {
    return AS_UNDEF;
  }
  mv_dispatch(S, MV_CS(S, grabbing), "THROWNFOXUP", "init", grabbing, in, 0);
  mv_turnOffHitboxes(S, p);
  const double frame = mv_frames(MV_CS(S, grabbing), "THROWNFOXUP");
  pl->phys.releaseFrame = frame + 1;
  mv_assign_hitbox_id(S, p, "throwup", 0, 0);
  fx_main(S, p, in, 0);
  return AS_UNDEF;
}

static AsTri fx_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  const double prevFrame = pl->timer;
  pl->timer += 7 / pl->phys.releaseFrame;
  if (fx_interrupt(S, p, in, 0) != AS_TRUE) {
    if (prevFrame < 13 && pl->timer >= 13) {
      ml_sound_play("foxlasercock");
    } else if (prevFrame < 16 && pl->timer >= 16) {
      mv_article_laser(S, p, 1.6, 18, js_pi() * 85 / 180);
      // rotate 85
      ml_sound_play("foxlaserfire");
      mv_drawVfx("laser");
    } else if (prevFrame < 18 && pl->timer >= 18) {
      mv_article_laser(S, p, 0.5, 18, js_pi() / 2);
      // rotate 90
      ml_sound_play("foxlaserfire");
      mv_drawVfx("laser");
    } else if (prevFrame < 21 && pl->timer >= 21) {
      mv_article_laser(S, p, 0, 18, js_pi() * 87 / 180);
      // rotate 87
      ml_sound_play("foxlaserfire");
      mv_drawVfx("laser");
    } else if (prevFrame < 33 && pl->timer >= 33) {
      ml_sound_play("foxlaserholster");
    }
    if (floor(pl->timer + 0.01) >= 7 && floor(prevFrame + 0.01) < 7) {
      mv_hq_push6(S, pl->phys.grabbing, p, 0, false, true, false);
      mv_turnOffHitboxes(S, p);
    }
  }
  return AS_UNDEF;
}

static AsTri fx_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  if (pl->timer > 33) {
    pl->phys.grabbing = -1;
    mv_WAIT.init(S, p, in, 0);
    return AS_TRUE;
  } else {
    const double grabbing = pl->phys.grabbing;
    if (grabbing == -1) {
      return AS_UNDEF; // upstream: bare `return;`
    }
    if (pl->timer < pl->phys.releaseFrame &&
        mv_player(S, grabbing)->phys.grabbedBy != p) {
      mv_CATCHCUT.init(S, p, in, 0);
      return AS_TRUE;
    } else {
      return AS_FALSE;
    }
  }
}

const MlMoveDef fox_THROWUP = {"THROWUP", fx_init, fx_main, fx_interrupt, 0};
