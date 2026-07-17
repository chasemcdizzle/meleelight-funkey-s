// THROWBACK.c <- src/characters/falco/moves/THROWBACK.js (M2 task 9)
// See THROWUP.c: table dispatch (1-arg — no input), hq push, laser seams
// (isFox:false), interrupt-only grabbing===-1 guard.
#include "../moves.h"

static AsTri fc_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex);
static AsTri fc_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex);

static AsTri fc_init(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "THROWBACK");
  pl->timer = 0;
  const double grabbing = pl->phys.grabbing;
  mv_dispatch(S, MV_CS(S, grabbing), "THROWNFALCOBACK", "init", grabbing, in,
              0); // upstream passes no input; THROWN* bodies never read it
  const double frame = mv_frames(MV_CS(S, grabbing), "THROWNFALCOBACK");
  pl->phys.releaseFrame = frame + 1;
  mv_turnOffHitboxes(S, p);
  mv_assign_hitbox_id(S, p, "throwback", 0, 0);
  as_randomShout(MV_CS(S, p));
  fc_main(S, p, in, 0);
  return AS_UNDEF;
}

static AsTri fc_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  const double prevFrame = pl->timer;
  pl->timer += 9 / pl->phys.releaseFrame;
  if (fc_interrupt(S, p, in, 0) != AS_TRUE) {
    if (prevFrame < 10 && pl->timer >= 10) {
      pl->phys.face *= -1;
    }
    if (prevFrame < 14 && pl->timer >= 14) {
      ml_sound_play("foxlasercock");
    }
    if (prevFrame < 15 && pl->timer >= 15) {
      mv_article_laser_falco(S, p, 5.2, 10, js_pi() * 0.22, false);
      ml_sound_play("foxlaserfire");
      // 135
      ml_drawVfx_laser("laser", pl->phys.pos.x + (5.2 * pl->phys.face),
                       pl->phys.pos.y + 10, pl->phys.face, js_pi() * 0.22,
                       137, 255, 255, 157, 255, 255);
    } else if (prevFrame < 18 && pl->timer >= 18) {
      mv_article_laser_falco(S, p, 5.4, 9.7, js_pi() * 0.20, false);
      ml_sound_play("foxlaserfire");
      // 135
      ml_drawVfx_laser("laser", pl->phys.pos.x + (5.4 * pl->phys.face),
                       pl->phys.pos.y + 9.7, pl->phys.face, js_pi() * 0.20,
                       137, 255, 255, 157, 255, 255);
    } else if (prevFrame < 21 && pl->timer >= 21) {
      mv_article_laser_falco(S, p, 5.3, 9.8, js_pi() * 0.22, false);
      ml_sound_play("foxlaserfire");
      // 135
      ml_drawVfx_laser("laser", pl->phys.pos.x + (5.3 * pl->phys.face),
                       pl->phys.pos.y + 9.8, pl->phys.face, js_pi() * 0.22,
                       137, 255, 255, 157, 255, 255);
    }
    if (floor(pl->timer + 0.01) >= 9 && floor(prevFrame + 0.01) < 9) {
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

const MlMoveDef falco_THROWBACK = {"THROWBACK", fc_init, fc_main,
                                   fc_interrupt, 0};
