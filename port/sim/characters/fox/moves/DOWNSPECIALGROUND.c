// DOWNSPECIALGROUND.c <- src/characters/fox/moves/DOWNSPECIALGROUND.js
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
  strcpy(pl->actionState, "DOWNSPECIALGROUND");
  pl->timer = 0;
  pl->phys.inShine = 0;
  ml_sound_play("foxshine");
  pl->shineLoop = 6;
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
  const MlInput *i0 = &MV_IN(in, p)[0];
  const MlInput *i6 = &MV_IN(in, p)[6];
  pl->timer += 1;
  pl->phys.inShine += 1;
  if (fx_interrupt(S, p, in, 0) != AS_TRUE) {
    if (pl->phys.onSurface[0] == 1 && pl->timer > 1) {
      if (i0->lsY < -0.66 && i6->lsY >= 0) {
        pl->phys.grounded = false;
        pl->phys.hasPassing = true;
        pl->phys.passing = true;
        pl->phys.cVel.y = -0.5;
      }
    }
    if (pl->phys.grounded) {
      as_reduceByTraction(false, (int)MV_CS(S, p), &pl->phys.cVel.x);
      if (pl->timer >= 3) {
        // shine turn: takes 3 frames, act on 4th (upstream comment only)
      }
      if (pl->timer >= 4 && pl->timer <= 35) {
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
    } else {
      strcpy(pl->actionState, "DOWNSPECIALAIR");
      pl->timer -= 1;
      fox_DOWNSPECIALAIR.main_(S, p, in, 0);
    }
  }
  return AS_UNDEF;
}

static AsTri fx_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  if (pl->timer >= 4 && pl->timer <= 32) {
    const AsPair j = as_checkForJump(S->tapJumpOff[(int)p], MV_IN(in, p));
    if (j.flag) {
      MvX x = mvx_pair_payload(&j);
      mv_KNEEBEND.init(S, p, in, &x);
      mv_turnOffHitboxes(S, p);
      return AS_TRUE;
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

const MlMoveDef fox_DOWNSPECIALGROUND = {"DOWNSPECIALGROUND", fx_init,
                                         fx_main, fx_interrupt, 0};
