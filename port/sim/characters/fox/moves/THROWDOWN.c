// THROWDOWN.c <- src/characters/fox/moves/THROWDOWN.js (M2 task 8)
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
  strcpy(pl->actionState, "THROWDOWN");
  pl->timer = 0;
  const double grabbing = pl->phys.grabbing;
  if (grabbing == -1) {
    return AS_UNDEF;
  }
  mv_dispatch(S, MV_CS(S, grabbing), "THROWNFOXDOWN", "init", grabbing, in,
              0);
  const double frame = mv_frames(MV_CS(S, grabbing), "THROWNFOXDOWN");
  pl->phys.releaseFrame = frame + 1;
  mv_turnOffHitboxes(S, p);
  mv_assign_hitbox_id(S, p, "throwdown", 0, 0);
  as_randomShout(MV_CS(S, p));
  fx_main(S, p, in, 0);
  return AS_UNDEF;
}

static AsTri fx_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  const double prevFrame = pl->timer;
  pl->timer += 33 / pl->phys.releaseFrame;
  if (fx_interrupt(S, p, in, 0) != AS_TRUE) {
    if (floor(pl->timer + 0.01) >= 33 && floor(prevFrame + 0.01) < 33) {
      mv_assign_hitbox_id(S, p, "throwdown", 0, 0);
      mv_hq_push6(S, pl->phys.grabbing, p, 0, false, true, true);
      mv_turnOffHitboxes(S, p);
    }

    if (prevFrame < 23 && pl->timer >= 23) {
      mv_article_laser(S, p, 1, 12, js_pi() * 275 / 180);
      ml_sound_play("foxlaserfire");
      // 275
      ml_drawVfx_laser("laser", pl->phys.pos.x + (1 * pl->phys.face),
                       pl->phys.pos.y + 12, pl->phys.face, js_pi() * 275 / 180,
                       255, 59, 59, 255, 57, 87);
    } else if (prevFrame < 25 && pl->timer >= 25) {
      mv_article_laser(S, p, 1, 16, js_pi() * 260 / 180);
      ml_sound_play("foxlaserfire");
      // 260
      ml_drawVfx_laser("laser", pl->phys.pos.x + (1 * pl->phys.face),
                       pl->phys.pos.y + 16, pl->phys.face, js_pi() * 260 / 180,
                       255, 59, 59, 255, 57, 87);
    } else if (prevFrame < 28 && pl->timer >= 28) {
      mv_article_laser(S, p, 2, 15, js_pi() * 290 / 180);
      ml_sound_play("foxlaserfire");
      // 290
      ml_drawVfx_laser("laser", pl->phys.pos.x + (2 * pl->phys.face),
                       pl->phys.pos.y + 15, pl->phys.face, js_pi() * 290 / 180,
                       255, 59, 59, 255, 57, 87);
    } else if (prevFrame < 31 && pl->timer >= 31) {
      mv_article_laser(S, p, 2, 17, js_pi() * 275 / 180);
      ml_sound_play("foxlaserfire");
      // 275
      ml_drawVfx_laser("laser", pl->phys.pos.x + (2 * pl->phys.face),
                       pl->phys.pos.y + 17, pl->phys.face, js_pi() * 275 / 180,
                       255, 59, 59, 255, 57, 87);
    }
  }
  return AS_UNDEF;
}

static AsTri fx_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  if (pl->timer > 43) {
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

const MlMoveDef fox_THROWDOWN = {"THROWDOWN", fx_init, fx_main, fx_interrupt,
                                 0};
