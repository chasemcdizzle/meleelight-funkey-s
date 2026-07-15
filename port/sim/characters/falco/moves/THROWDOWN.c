// THROWDOWN.c <- src/characters/falco/moves/THROWDOWN.js (M2 task 9)
// See THROWUP.c. The victim dispatch is 1-arg upstream; the LASER options
// additionally carry `partOfThrow: true` (article seam serializes it).
// NOTE the hq row's 6th flag is TRUE (isThrowDown) and the push arm
// RE-ASSIGNS hitboxes.id[0] before pushing.
#include "../moves.h"

static AsTri fc_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex);
static AsTri fc_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex);

static AsTri fc_init(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "THROWDOWN");
  pl->timer = 0;
  const double grabbing = pl->phys.grabbing;
  mv_dispatch(S, MV_CS(S, grabbing), "THROWNFALCODOWN", "init", grabbing, in,
              0); // upstream passes no input; THROWN* bodies never read it
  const double frame = mv_frames(MV_CS(S, grabbing), "THROWNFALCODOWN");
  pl->phys.releaseFrame = frame + 1;
  mv_turnOffHitboxes(S, p);
  mv_assign_hitbox_id(S, p, "throwdown", 0, 0);
  as_randomShout(MV_CS(S, p));
  fc_main(S, p, in, 0);
  return AS_UNDEF;
}

static AsTri fc_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  const double prevFrame = pl->timer;
  pl->timer += 33 / pl->phys.releaseFrame;
  if (fc_interrupt(S, p, in, 0) != AS_TRUE) {
    if (floor(pl->timer + 0.01) >= 33 && floor(prevFrame + 0.01) < 33) {
      mv_assign_hitbox_id(S, p, "throwdown", 0, 0);
      mv_hq_push6(S, pl->phys.grabbing, p, 0, false, true, true);
      mv_turnOffHitboxes(S, p);
    }
    if (prevFrame < 22 && pl->timer >= 22) {
      ml_sound_play("foxlasercock");
    }
    if (prevFrame < 23 && pl->timer >= 23) {
      mv_article_laser_falco(S, p, 1, 12, js_pi() * 275 / 180, true);
      ml_sound_play("foxlaserfire");
      // 275
      mv_drawVfx("laser");
    } else if (prevFrame < 25 && pl->timer >= 25) {
      mv_article_laser_falco(S, p, 1, 16, js_pi() * 260 / 180, true);
      ml_sound_play("foxlaserfire");
      // 260
      mv_drawVfx("laser");
    } else if (prevFrame < 28 && pl->timer >= 28) {
      mv_article_laser_falco(S, p, 2, 15, js_pi() * 290 / 180, true);
      ml_sound_play("foxlaserfire");
      // 290
      mv_drawVfx("laser");
    } else if (prevFrame < 31 && pl->timer >= 31) {
      mv_article_laser_falco(S, p, 2, 17, js_pi() * 275 / 180, true);
      ml_sound_play("foxlaserfire");
      // 275
      mv_drawVfx("laser");
    }
  }
  return AS_UNDEF;
}

static AsTri fc_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
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

const MlMoveDef falco_THROWDOWN = {"THROWDOWN", fc_init, fc_main,
                                   fc_interrupt, 0};
