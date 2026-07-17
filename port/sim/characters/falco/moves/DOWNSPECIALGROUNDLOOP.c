// DOWNSPECIALGROUNDLOOP.c <-
// src/characters/falco/moves/DOWNSPECIALGROUNDLOOP.js (M2 task 9).
// NOTE upstream's interrupt computes `const j = checkForJump(p,input)` at
// the TOP (before every arm) and the timer>28 arm re-enters this init.
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
  strcpy(pl->actionState, "DOWNSPECIALGROUNDLOOP");
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
  const MlInput *i0 = &MV_IN(in, p)[0];
  const MlInput *i6 = &MV_IN(in, p)[6];
  pl->timer += 1;
  pl->phys.inShine += 1;
  if (fc_interrupt(S, p, in, 0) != AS_TRUE) {
    if (pl->phys.onSurface[0] == 1) {
      if (i0->lsY < -0.66 && i6->lsY >= 0) {
        pl->phys.grounded = false;
        pl->phys.hasPassing = true;
        pl->phys.passing = true;
        pl->phys.cVel.y = -0.5;
        strcpy(pl->actionState, "DOWNSPECIALAIRLOOP");
      }
    }
    as_reduceByTraction(false, (int)MV_CS(S, p), &pl->phys.cVel.x);

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
  const MlInput *i0 = &MV_IN(in, p)[0];
  const AsPair j = as_checkForJump(S->tapJumpOff[(int)p], MV_IN(in, p));
  if (i0->lsX * pl->phys.face < 0) {
    falco_DOWNSPECIALGROUNDTURN.init(S, p, in, 0);
    return AS_TRUE;
  } else if (pl->phys.inShine >= 22 && !i0->b) {
    falco_DOWNSPECIALGROUNDEND.init(S, p, in, 0);
    return AS_TRUE;
  } else if (j.flag) {
    mv_turnOffHitboxes(S, p);
    MvX x = mvx_pair_payload(&j);
    mv_KNEEBEND.init(S, p, in, &x);
    return AS_TRUE;
  } else if (pl->timer > 28) {
    fc_init(S, p, in, 0);
    return AS_TRUE;
  } else {
    return AS_FALSE;
  }
}

const MlMoveDef falco_DOWNSPECIALGROUNDLOOP = {
    "DOWNSPECIALGROUNDLOOP", fc_init, fc_main, fc_interrupt, 0};
