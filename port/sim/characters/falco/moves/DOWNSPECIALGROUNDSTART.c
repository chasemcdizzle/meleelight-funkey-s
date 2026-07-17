// DOWNSPECIALGROUNDSTART.c <-
// src/characters/falco/moves/DOWNSPECIALGROUNDSTART.js (M2 task 9)
#include "../moves.h"

static AsTri fc_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex);
static AsTri fc_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex);

static AsTri fc_init(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "DOWNSPECIALGROUNDSTART");
  pl->timer = 0;
  pl->phys.inShine = 0;
  pl->shineLoop = 6;
  ml_sound_play("foxshine");
  ml_drawVfx("impactLand", pl->phys.pos.x, pl->phys.pos.y, pl->phys.face);
  ml_drawVfx_p("shine", pl->phys.pos.x, pl->phys.pos.y + 6);
  mv_turnOffHitboxes(S, p);
  mv_assign_hitbox_id(S, p, "downspecial", 0, 0);
  fc_main(S, p, in, 0);
  return AS_UNDEF;
}

static AsTri fc_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  const MlInput *i0 = &MV_IN(in, p)[0];
  const MlInput *i6 = &MV_IN(in, p)[6];
  pl->timer += 1;
  pl->phys.inShine += 1;
  if (fc_interrupt(S, p, in, 0) != AS_TRUE) {
    if (pl->phys.onSurface[0] == 1 && pl->timer > 1) {
      if (i0->lsY < -0.66 && i6->lsY >= 0) {
        pl->phys.grounded = false;
        pl->phys.hasPassing = true;
        pl->phys.passing = true;
        pl->phys.cVel.y = -0.5;
        strcpy(pl->actionState, "DOWNSPECIALAIRSTART");
      }
    }
    as_reduceByTraction(false, (int)MV_CS(S, p), &pl->phys.cVel.x);

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
    }
  }
  return AS_UNDEF;
}

static AsTri fc_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  if (pl->timer > 3) {
    falco_DOWNSPECIALGROUNDLOOP.init(S, p, in, 0);
    return AS_TRUE;
  } else {
    return AS_FALSE;
  }
}

const MlMoveDef falco_DOWNSPECIALGROUNDSTART = {
    "DOWNSPECIALGROUNDSTART", fc_init, fc_main, fc_interrupt, 0};
