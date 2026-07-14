// DAMAGEFLYN.c <- src/characters/shared/moves/DAMAGEFLYN.js (M2 task 7)
#include "../moves.h"

#include <math.h>

static AsTri mv_init(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  // init(p, input, drawStuff) — drawStuff gates commented-out vfx only
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "DAMAGEFLYN");
  pl->timer = 0;
  pl->phys.grabbing = -1;
  pl->phys.grabbedBy = -1;
  pl->phys.fastfalled = false;
  pl->rotation = 0;
  pl->rotationPoint = vec2d(0, 0);
  pl->colourOverlayBool = false;
  // (drawStuff arm is fully commented out upstream)
  mv_assign_thrown_id0(S, p); // hitboxes.id[0] = charHitboxes.thrown.id0
  mv_turnOffHitboxes(S, p);
  mv_dispatch(S, MV_CS(S, p), "DAMAGEFLYN", "main", p, in, 0);
  return AS_UNDEF;
}

static AsTri mv_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  const ml_attributes_t *at = mv_attr(MV_CS(S, p));
  if (pl->phys.thrownHitbox) {
    if (pl->timer == 1 && pl->phys.cVel.y + pl->phys.kVel.y > 0) {
      // hitboxes.active = [true,false,false,false]: fresh array upstream
      pl->hitboxes.active[0] = true;
      pl->hitboxes.active[1] = false;
      pl->hitboxes.active[2] = false;
      pl->hitboxes.active[3] = false;
      S->aliasHbActive[(int)p] = false;
      pl->hitboxes.frame = 0;
    }
    if (pl->timer > 1 && pl->phys.cVel.y + pl->phys.kVel.y > 0) {
      // (frame++ commented out upstream)
    }
    if (pl->phys.cVel.y + pl->phys.kVel.y <= 0) {
      mv_turnOffHitboxes(S, p);
    }
  }
  if (pl->timer < mv_frames(MV_CS(S, p), "DAMAGEFLYN")) {
    pl->timer += 1;
  }
  if (fmod(pl->hit.hitstun, 10) == 0) {
    mv_drawVfx("flyingDust");
  }
  if (mv_dispatch(S, MV_CS(S, p), "DAMAGEFLYN", "interrupt", p, in, 0) !=
      AS_TRUE) {
    if (pl->timer > 1) {
      pl->hit.hitstun -= 1;
      if (!pl->phys.grounded) {
        pl->phys.cVel.y -= ml_f64(at->gravity);
        if (pl->phys.cVel.y < -ml_f64(at->terminalV)) {
          pl->phys.cVel.y = -ml_f64(at->terminalV);
        }
      }
    }
  } else {
    pl->phys.thrownHitbox = false;
  }
  return AS_UNDEF;
}

static AsTri mv_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  if (pl->timer > 1 && pl->hit.hitstun == 0) {
    mv_dispatch(S, MV_CS(S, p), "DAMAGEFALL", "init", p, in, 0);
    pl->phys.thrownHitbox = false;
    return AS_TRUE;
  } else {
    return AS_FALSE;
  }
}

const MlMoveDef mv_DAMAGEFLYN = {"DAMAGEFLYN", mv_init, mv_main, mv_interrupt,
                                 0};
