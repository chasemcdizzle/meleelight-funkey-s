// WALLTECH.c <- src/characters/shared/moves/WALLTECH.js (M2 task 7)
#include "../moves.h"

static AsTri mv_init(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "WALLTECH");
  pl->timer = 0;
  pl->phys.fastfalled = false;
  pl->hit.knockback = 0;
  pl->hit.hitstun = 0;
  pl->phys.kVel.y = 0;
  pl->phys.kVel.x = 0;
  pl->phys.cVel.x = 0;
  pl->phys.cVel.y = 0;
  pl->phys.intangibleTimer = js_max(pl->phys.intangibleTimer, 14);
  if (pl->phys.face == 1) {
    ml_drawVfx_p("tech", pl->phys.ECBp[3].x, pl->phys.ECBp[3].y);
  } else {
    ml_drawVfx_p("tech", pl->phys.ECBp[1].x, pl->phys.ECBp[1].y);
  }
  mv_dispatch(S, MV_CS(S, p), "WALLTECH", "main", p, in, 0);
  return AS_UNDEF;
}

static AsTri mv_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  if (pl->timer < 1) {
    pl->timer += 0.15;
    if (pl->timer > 1) {
      pl->timer = 1;
    }
  } else {
    pl->timer += 1;
  }
  mv_playSounds(S, "TECH", p);
  if (mv_dispatch(S, MV_CS(S, p), "WALLTECH", "interrupt", p, in, 0) !=
      AS_TRUE) {
    if (pl->timer == 2) {
      ml_sound_play("walljump");
    }
    if (pl->timer > 0.89 && pl->timer < 0.91) {
      pl->phys.cVel.x = pl->phys.face * 0.5;
    }
    if (pl->timer >= 1) {
      as_fastfall((int)MV_CS(S, p), &pl->phys.cVel.y, &pl->phys.fastfalled,
                  MV_IN(in, p));
      as_airDrift((int)MV_CS(S, p), &pl->phys.cVel.x, MV_IN(in, p));
    }
  }
  return AS_UNDEF;
}

static AsTri mv_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  const ml_attributes_t *at = mv_attr(MV_CS(S, p));
  const MlInput *i0 = &MV_IN(in, p)[0];
  const MlInput *i1 = &MV_IN(in, p)[1];
  if (pl->timer > 1) {
    const AsPair a = as_checkForAerials(pl->phys.face, MV_IN(in, p));
    const AsPair b = as_checkForSpecials(&pl->phys.face,
                                         pl->phys.bTurnaroundTimer,
                                         pl->phys.bTurnaroundDirection,
                                         pl->phys.grounded, MV_IN(in, p));
    if (a.flag) {
      mv_dispatch(S, MV_CS(S, p), mv_pair_str(&a), "init", p, in, 0);
      return AS_TRUE;
    } else if ((i0->l && !i1->l) || (i0->r && !i1->r)) {
      mv_dispatch(S, MV_CS(S, p), "ESCAPEAIR", "init", p, in, 0);
      return AS_TRUE;
    } else if (as_checkForDoubleJump(S->tapJumpOff[(int)p], MV_IN(in, p)) &&
               (!pl->phys.doubleJumped ||
                (pl->phys.jumpsUsed < 5 && at->multiJump != 0))) {
      if (i0->lsX * pl->phys.face < -0.3) {
        mv_dispatch(S, MV_CS(S, p), "JUMPAERIALB", "init", p, in, 0);
      } else {
        mv_dispatch(S, MV_CS(S, p), "JUMPAERIALF", "init", p, in, 0);
      }
      return AS_TRUE;
    } else if (b.flag) {
      mv_dispatch(S, MV_CS(S, p), mv_pair_str(&b), "init", p, in, 0);
      return AS_TRUE;
    } else if (pl->timer > mv_frames(MV_CS(S, p), "WALLTECH")) {
      mv_dispatch(S, MV_CS(S, p), "FALL", "init", p, in, 0);
      return AS_TRUE;
    } else {
      return AS_FALSE;
    }
  } else {
    return AS_FALSE;
  }
}

const MlMoveDef mv_WALLTECH = {"WALLTECH", mv_init, mv_main, mv_interrupt, 0};
