// UPSPECIAL.c <- src/characters/puff/moves/UPSPECIAL.js (M2 task 12)
// Sing: three hitbox windows via id[0].size writes — runtime writes to
// the GLOBAL charHitboxes plane upstream (pf_hb_set_size; the chd
// pre-projection measures the plane per record). land is an EMPTY
// function upstream (present, does nothing).
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
  strcpy(pl->actionState, "UPSPECIAL");
  pl->timer = 0;
  //23 / 71 / 122 (upstream comment)
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
  pf_assign_hitbox_id(S, p, "upb", 0, 0);
  pf_main(S, p, in, 0);
  return AS_UNDEF;
}

static AsTri pf_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  pl->timer += 1;
  if (pf_interrupt(S, p, in, 0) != AS_TRUE) {
    if (pl->timer == 23) {
      ml_drawVfx("sing", 0, 0, p); // pos: new Vec2D(0, 0), face: p
    } else if (pl->timer == 71) {
      ml_drawVfx("sing2", 0, 0, p);
    } else if (pl->timer == 122) {
      ml_drawVfx("sing3", 0, 0, p);
    }
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
    if (pl->timer == 18) {
      ml_sound_play("sing1");
    }
    if (pl->timer == 69) {
      ml_sound_play("sing2");
    }
    if (pl->timer == 28) {
      pl->hitboxes.active[0] = true;
      pl->hitboxes.active[1] = false;
      pl->hitboxes.active[2] = false;
      pl->hitboxes.active[3] = false;
      S->aliasHbActive[(int)p] = false; // fresh array upstream
      pl->hitboxes.frame = 0;
      pf_hb_set_size(S, p, 0, 10.937);
    } else if (pl->timer == 36) {
      pf_hb_set_size(S, p, 0, 1);
    } else if (pl->timer == 69) {
      pf_hb_set_size(S, p, 0, 10.937);
    } else if (pl->timer == 77) {
      pf_hb_set_size(S, p, 0, 1);
    } else if (pl->timer == 113) {
      pf_hb_set_size(S, p, 0, 12.890);
    } else if (pl->timer == 126) {
      mv_turnOffHitboxes(S, p);
    }
  }
  return AS_UNDEF;
}

static AsTri pf_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  if (pl->timer > 179) {
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

static AsTri pf_land(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)S; (void)p; (void)in; (void)ex;
  // upstream: an empty function body
  return AS_UNDEF;
}

const MlMoveDef puff_UPSPECIAL = {"UPSPECIAL", pf_init, pf_main,
                                  pf_interrupt, pf_land};
