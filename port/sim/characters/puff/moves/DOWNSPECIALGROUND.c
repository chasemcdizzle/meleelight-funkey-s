// DOWNSPECIALGROUND.c <- src/characters/puff/moves/DOWNSPECIALGROUND.js (M2 task 12)
// Rest: 1-frame hitbox + intangibleTimer 26; the GROUND and AIR twins are byte-identical bodies; land is a comment-only body.
#include "../moves.h"

static AsTri pf_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex);
static AsTri pf_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex);

static AsTri pf_land(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex);

static AsTri pf_init(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "DOWNSPECIALGROUND");
  pl->timer = 0;
  if (pl->phys.grounded) {
    if (pl->phys.cVel.x > 0) {
      pl->phys.cVel.x -= 0.1;
    }
    if (pl->phys.cVel.x < 0) {
      pl->phys.cVel.x += 0.1;
    }
  } else {
    pl->phys.fastfalled = false;
    if (pl->phys.cVel.y < -ml_f64(mv_attr(MV_CS(S, p))->terminalV)) {
      pl->phys.cVel.y = -ml_f64(mv_attr(MV_CS(S, p))->terminalV);
    }
  }
  mv_turnOffHitboxes(S, p);
  pf_assign_hitbox_id(S, p, "downspecial", 0, 0);
  pf_main(S, p, in, 0);
  return AS_UNDEF;
}

static AsTri pf_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  pl->timer += 1;
  if (pf_interrupt(S, p, in, 0) != AS_TRUE) {
    if (pl->phys.grounded) {
      as_reduceByTraction(false, (int)MV_CS(S, p), &pl->phys.cVel.x);
    } else {
      const double airFriction = ml_f64(mv_attr(MV_CS(S, p))->airFriction);
      if (pl->phys.cVel.x > 0) {
        pl->phys.cVel.x -= airFriction;
        if (pl->phys.cVel.x < 0) {
          pl->phys.cVel.x = 0;
        }
      } else if (pl->phys.cVel.x < 0) {
        pl->phys.cVel.x += airFriction;
        if (pl->phys.cVel.x > 0) {
          pl->phys.cVel.x = 0;
        }
      }
      pl->phys.cVel.y -= ml_f64(mv_attr(MV_CS(S, p))->gravity);
      if (pl->phys.cVel.y < -ml_f64(mv_attr(MV_CS(S, p))->terminalV)) {
        pl->phys.cVel.y = -ml_f64(mv_attr(MV_CS(S, p))->terminalV);
      }
    }
    if (pl->timer == 1) {
      pl->hitboxes.active[0] = true;
      pl->hitboxes.active[1] = false;
      pl->hitboxes.active[2] = false;
      pl->hitboxes.active[3] = false;
      S->aliasHbActive[(int)p] = false; // fresh array upstream
      pl->hitboxes.frame = 0;
      pl->phys.intangibleTimer = 26;
    }
    if (pl->timer == 2) {
      mv_turnOffHitboxes(S, p);
    }
    if (pl->timer == 10) {
      ml_sound_play("rest1");
      ml_sound_play("restbubbles");
    }
    if (pl->timer == 210) {
      ml_sound_play("rest2");
    }
  }
  return AS_UNDEF;
}

static AsTri pf_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  if (pl->timer > 249) {
    if (pl->phys.grounded) {
      mv_WAIT.init(S, p, in, 0);
    } else {
      mv_FALL.init(S, p, in, 0);
    }
    return AS_TRUE;
  } else {
    return AS_FALSE;
  }
}

static AsTri pf_land(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)S; (void)p; (void)in; (void)ex;
  //player[p].actionState = 109; (upstream comment-only body)
  return AS_UNDEF;
}

const MlMoveDef puff_DOWNSPECIALGROUND = {"DOWNSPECIALGROUND", pf_init, pf_main, pf_interrupt,
                             pf_land};
