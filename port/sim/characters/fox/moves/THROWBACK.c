// THROWBACK.c <- src/characters/fox/moves/THROWBACK.js (M2 task 8)
// NOTE the victim dispatch is 1-arg upstream: .init(grabbing) — no input.
#include "../moves.h"

static AsTri fx_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex);
static AsTri fx_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex);

static AsTri fx_init(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "THROWBACK");
  pl->timer = 0;
  const double grabbing = pl->phys.grabbing;
  if (grabbing == -1) {
    return AS_UNDEF;
  }
  mv_dispatch(S, MV_CS(S, grabbing), "THROWNFOXBACK", "init", grabbing, in,
              0); // upstream passes no input; THROWN* bodies never read it
  const double frame = mv_frames(MV_CS(S, grabbing), "THROWNFOXBACK");
  pl->phys.releaseFrame = frame + 1;
  mv_turnOffHitboxes(S, p);
  mv_assign_hitbox_id(S, p, "throwback", 0, 0);
  as_randomShout(MV_CS(S, p));
  fx_main(S, p, in, 0);
  return AS_UNDEF;
}

static AsTri fx_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  const double prevFrame = pl->timer;
  pl->timer += 8 / pl->phys.releaseFrame;
  if (fx_interrupt(S, p, in, 0) != AS_TRUE) {
    if (prevFrame < 10 && pl->timer >= 10) {
      pl->phys.face *= -1;
    }
    if (prevFrame < 14 && pl->timer >= 14) {
      mv_article_laser(S, p, 5.2, 10, js_pi() * 0.22);
      ml_sound_play("foxlaserfire");
      // 135
      ml_drawVfx_laser("laser", pl->phys.pos.x + (5.2 * pl->phys.face),
                       pl->phys.pos.y + 10, pl->phys.face, js_pi() * 0.22, 255,
                       59, 59, 255, 57, 87);
    } else if (prevFrame < 16 && pl->timer >= 16) {
      mv_article_laser(S, p, 5.4, 9.7, js_pi() * 0.20);
      ml_sound_play("foxlaserfire");
      // 135
      ml_drawVfx_laser("laser", pl->phys.pos.x + (5.4 * pl->phys.face),
                       pl->phys.pos.y + 9.7, pl->phys.face, js_pi() * 0.20, 255,
                       59, 59, 255, 57, 87);
    } else if (prevFrame < 19 && pl->timer >= 19) {
      mv_article_laser(S, p, 5.3, 9.8, js_pi() * 0.22);
      ml_sound_play("foxlaserfire");
      // 135
      ml_drawVfx_laser("laser", pl->phys.pos.x + (5.3 * pl->phys.face),
                       pl->phys.pos.y + 9.8, pl->phys.face, js_pi() * 0.22, 255,
                       59, 59, 255, 57, 87);
    }
    if (floor(pl->timer + 0.01) >= 8 && floor(prevFrame + 0.01) < 8) {
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
  if (pl->timer > 32) {
    pl->phys.grabbing = -1;
    mv_WAIT.init(S, p, in, 0);
    return AS_TRUE;
  } else {
    const double grabbing = pl->phys.grabbing;
    if (grabbing == -1) {
      return AS_UNDEF;
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

const MlMoveDef fox_THROWBACK = {"THROWBACK", fx_init, fx_main, fx_interrupt,
                                 0};
