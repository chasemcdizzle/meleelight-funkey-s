// THROWUP.c <- src/characters/falco/moves/THROWUP.js (M2 task 9)
// Victim dispatch crosses the actionStates TABLE (mv_dispatch: falco
// victim = the registered falco body, other chars = the driver's
// mdispatch seam). hitQueue.push crosses mv_hq_push6; articles.LASER.init
// the article seam (isFox:false). NOTE falco's init has NO grabbing===-1
// guard (upstream would throw on actionStates[undefined] — the mv_dispatch
// charId domain traps); the guard lives in the interrupt only.
#include "../moves.h"

static AsTri fc_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex);
static AsTri fc_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex);

static AsTri fc_init(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "THROWUP");
  pl->timer = 0;
  const double grabbing = pl->phys.grabbing;
  mv_dispatch(S, MV_CS(S, grabbing), "THROWNFALCOUP", "init", grabbing, in,
              0);
  mv_turnOffHitboxes(S, p);
  const double frame = mv_frames(MV_CS(S, grabbing), "THROWNFALCOUP");
  pl->phys.releaseFrame = frame + 1;
  mv_assign_hitbox_id(S, p, "throwup", 0, 0);
  fc_main(S, p, in, 0);
  return AS_UNDEF;
}

static AsTri fc_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  const double prevFrame = pl->timer;
  pl->timer += 7 / pl->phys.releaseFrame;
  if (fc_interrupt(S, p, in, 0) != AS_TRUE) {
    if (prevFrame < 14 && pl->timer >= 14) {
      ml_sound_play("foxlasercock");
    }
    if (prevFrame < 18 && pl->timer >= 18) {
      mv_article_laser_falco(S, p, 0, 18, js_pi() / 2, false);
      ml_sound_play("foxlaserfire");
      ml_drawVfx_laser("laser", pl->phys.pos.x + (0 * pl->phys.face),
                       pl->phys.pos.y + 18, pl->phys.face, js_pi() / 2, 137,
                       255, 255, 157, 255, 255);
    } else if (prevFrame < 20 && pl->timer >= 20) {
      mv_article_laser_falco(S, p, 0, 18, js_pi() / 2, false);
      // rotate 90
      ml_sound_play("foxlaserfire");
      ml_drawVfx_laser("laser", pl->phys.pos.x + (0 * pl->phys.face),
                       pl->phys.pos.y + 18, pl->phys.face, js_pi() / 2, 137,
                       255, 255, 157, 255, 255);
    } else if (prevFrame < 24 && pl->timer >= 24) {
      mv_article_laser_falco(S, p, 0, 18, js_pi() / 2, false);
      ml_sound_play("foxlaserfire");
      ml_drawVfx_laser("laser", pl->phys.pos.x + (0 * pl->phys.face),
                       pl->phys.pos.y + 18, pl->phys.face, js_pi() / 2, 137,
                       255, 255, 157, 255, 255);
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

static AsTri fc_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  if (pl->timer > 38) {
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

const MlMoveDef falco_THROWUP = {"THROWUP", fc_init, fc_main, fc_interrupt,
                                 0};
