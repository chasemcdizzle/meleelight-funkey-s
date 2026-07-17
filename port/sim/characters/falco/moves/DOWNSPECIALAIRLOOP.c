// DOWNSPECIALAIRLOOP.c <- src/characters/falco/moves/DOWNSPECIALAIRLOOP.js
// (M2 task 9). NOTE the timer>28 arm re-enters THIS module's init
// (`this.init(p,input)`) — a self-loop, carried verbatim.
#include "../moves.h"

static AsTri fc_init(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex);
static AsTri fc_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex);
static AsTri fc_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex);

static AsTri fc_init(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "DOWNSPECIALAIRLOOP");
  pl->timer = 0;
  mv_assign_hitbox_id(S, p, "reflector", 0, 0);
  mv_turnOffHitboxes(S, p);
  pl->hitboxes.active[0] = true;
  pl->hitboxes.active[1] = false;
  pl->hitboxes.active[2] = false;
  pl->hitboxes.active[3] = false;
  S->aliasHbActive[(int)p] = false;
  pl->hitboxes.frame = 0;
  fc_main(S, p, in, 0);
  return AS_UNDEF;
}

static AsTri fc_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  const ml_attributes_t *at = mv_attr(MV_CS(S, p));
  pl->timer += 1;
  pl->phys.inShine += 1;
  if (fc_interrupt(S, p, in, 0) != AS_TRUE) {
    if (pl->phys.cVel.x > 0) {
      if (pl->phys.cVel.x > 0.85) {
        pl->phys.cVel.x -= 0.03;
      } else {
        pl->phys.cVel.x -= 0.02;
      }
      if (pl->phys.cVel.x < 0) {
        pl->phys.cVel.x = 0;
      }
    } else if (pl->phys.cVel.x < 0) {
      if (pl->phys.cVel.x < -0.85) {
        pl->phys.cVel.x += 0.03;
      } else {
        pl->phys.cVel.x += 0.02;
      }
      if (pl->phys.cVel.x > 0) {
        pl->phys.cVel.x = 0;
      }
    }

    if (pl->timer >= 1) {
      pl->phys.cVel.y -= 0.02667;
      if (pl->phys.cVel.y < -ml_f64(at->terminalV)) {
        pl->phys.cVel.y = -ml_f64(at->terminalV);
      }
    }

    if (pl->shineLoop == 6) {
      pl->shineLoop = 0;
    }
    pl->shineLoop += 1;
    ml_drawVfx("shineloop", 0, 0, p);
  }
  return AS_UNDEF;
}

static AsTri fc_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  const ml_attributes_t *at = mv_attr(MV_CS(S, p));
  const MlInput *i0 = &MV_IN(in, p)[0];
  if (i0->lsX * pl->phys.face < 0) {
    falco_DOWNSPECIALAIRTURN.init(S, p, in, 0);
    return AS_TRUE;
  } else if (pl->phys.inShine >= 22 && !i0->b) {
    falco_DOWNSPECIALAIREND.init(S, p, in, 0);
    return AS_TRUE;
  } else if (as_checkForDoubleJump(S->tapJumpOff[(int)p], MV_IN(in, p)) &&
             (!pl->phys.doubleJumped ||
              (pl->phys.jumpsUsed < 5 && at->multiJump != 0))) {
    mv_turnOffHitboxes(S, p);
    if (i0->lsX * pl->phys.face < -0.3) {
      mv_JUMPAERIALB.init(S, p, in, 0);
    } else {
      mv_JUMPAERIALF.init(S, p, in, 0);
    }
    return AS_TRUE;
  } else if (pl->timer > 28) {
    fc_init(S, p, in, 0);
    return AS_TRUE;
  } else {
    return AS_FALSE;
  }
}

static AsTri fc_land(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)in; (void)ex;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "DOWNSPECIALGROUNDLOOP");
  return AS_UNDEF;
}

const MlMoveDef falco_DOWNSPECIALAIRLOOP = {"DOWNSPECIALAIRLOOP", fc_init,
                                            fc_main, fc_interrupt, fc_land};
