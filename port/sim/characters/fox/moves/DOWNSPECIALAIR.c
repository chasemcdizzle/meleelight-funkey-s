// DOWNSPECIALAIR.c <- src/characters/fox/moves/DOWNSPECIALAIR.js
// (M2 task 8)
#include "../moves.h"

static AsTri fx_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex);
static AsTri fx_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex);

static AsTri fx_init(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "DOWNSPECIALAIR");
  pl->timer = 0;
  pl->phys.fastfalled = false;
  pl->phys.cVel.y = 0;
  pl->phys.cVel.x *= 0.5;
  pl->shineLoop = 6;
  pl->phys.inShine = 0;
  ml_sound_play("foxshine");
  mv_drawVfx("impactLand");
  mv_drawVfx("shine");
  mv_turnOffHitboxes(S, p);
  mv_assign_hitbox_id(S, p, "downspecial", 0, 0);
  fx_main(S, p, in, 0);
  return AS_UNDEF;
}

static AsTri fx_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  const ml_attributes_t *at = mv_attr(MV_CS(S, p));
  const MlInput *i0 = &MV_IN(in, p)[0];
  pl->timer += 1;
  pl->phys.inShine += 1;
  if (fx_interrupt(S, p, in, 0) != AS_TRUE) {
    if (pl->phys.grounded) {
      strcpy(pl->actionState, "DOWNSPECIALGROUND");
      pl->timer -= 1;
      fox_DOWNSPECIALGROUND.main_(S, p, in, 0);
    } else {
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
      if (pl->timer >= 5) {
        pl->phys.cVel.y -= 0.02667;
        if (pl->phys.cVel.y < -ml_f64(at->terminalV)) {
          pl->phys.cVel.y = -ml_f64(at->terminalV);
        }
      }

      if (pl->timer >= 4 && pl->timer <= 32) {
        if (pl->shineLoop == 6) {
          pl->shineLoop = 0;
        }
        pl->shineLoop += 1;
        mv_drawVfx("shineloop");
      }

      if (pl->timer == 35) {
        pl->phys.face *= -1;
        pl->timer = 4;
      }
      if (pl->timer >= 4 && pl->timer <= 32) {
        if (i0->lsX * pl->phys.face < 0) {
          pl->timer = 32;
        } else if (pl->phys.inShine >= 22) {
          if (!i0->b) {
            pl->timer = 36;
          } else if (pl->timer == 32) {
            pl->timer = 4;
          }
        }
      }

      if (pl->timer == 1) {
        pl->hitboxes.active[0] = true;
        pl->hitboxes.active[1] = false;
        pl->hitboxes.active[2] = false;
        pl->hitboxes.active[3] = false;
        S->aliasHbActive[(int)p] = false;
        pl->hitboxes.frame = 0;
        pl->phys.intangibleTimer = js_max(pl->phys.intangibleTimer, 1);
      }
      if (pl->timer == 2) {
        mv_turnOffHitboxes(S, p);
        mv_assign_hitbox_id(S, p, "reflector", 0, 0);
      }
      if (pl->timer == 4) {
        pl->hitboxes.active[0] = true;
        pl->hitboxes.active[1] = false;
        pl->hitboxes.active[2] = false;
        pl->hitboxes.active[3] = false;
        S->aliasHbActive[(int)p] = false;
        pl->hitboxes.frame = 0;
      }
      if (pl->timer == 36) {
        mv_turnOffHitboxes(S, p);
      }
    }
  }
  return AS_UNDEF;
}

static AsTri fx_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  const MlInput *i0 = &MV_IN(in, p)[0];
  const MlInput *i1 = &MV_IN(in, p)[1];
  const MlInput *i3 = &MV_IN(in, p)[3];
  if (pl->timer >= 4 && pl->timer <= 32) {
    if (!pl->phys.doubleJumped) {
      // gameSettings["tapJumpOffp"+(p+1)] == false — captured domain is
      // the number 0 (the established loose-eq model)
      if ((i0->x && !i1->x) || (i0->y && !i1->y) ||
          (S->tapJumpOff[(int)p] == 0 && i0->lsY >= 0.7 && i3->lsY < 0.7)) {
        if (i0->lsX * pl->phys.face < -0.3) {
          mv_JUMPAERIALB.init(S, p, in, 0);
        } else {
          mv_JUMPAERIALF.init(S, p, in, 0);
        }
        mv_turnOffHitboxes(S, p);
        return AS_TRUE;
      } else {
        return AS_FALSE;
      }
    } else {
      return AS_FALSE;
    }
  } else if (pl->timer > 49) {
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

static AsTri fx_land(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  (void)in;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "DOWNSPECIALGROUND");
  if (pl->timer >= 4 && pl->timer <= 35) {
    mv_assign_hitbox_id(S, p, "reflector", 0, 0);
    pl->hitboxes.active[0] = true;
    pl->hitboxes.active[1] = false;
    pl->hitboxes.active[2] = false;
    pl->hitboxes.active[3] = false;
    S->aliasHbActive[(int)p] = false;
    pl->hitboxes.frame = 0;
  }
  return AS_UNDEF;
}

const MlMoveDef fox_DOWNSPECIALAIR = {"DOWNSPECIALAIR", fx_init, fx_main,
                                      fx_interrupt, fx_land};
