// STOPCEIL.c <- src/characters/shared/moves/STOPCEIL.js (M2 task 7)
#include "../moves.h"

#include <math.h>

static AsTri mv_init(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  // init(p, input, normal = null)
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "STOPCEIL");
  pl->timer = 0;
  pl->phys.cVel.y = 0;
  const bool hasNormal = ex != 0 && ex->count >= 1 &&
                         ex->x[0].kind == DX_VEC;
  if (ex != 0 && ex->count >= 1 && ex->x[0].kind != DX_VEC) {
    mv_out_of_domain("STOPCEIL: non-Vec2D normal arg");
  }
  if (hasNormal) {
    // knockback bounce
    const Vec2D normal = ex->x[0].vec;
    pl->phys.hurtBoxState = 1;
    pl->phys.intangibleTimer = js_max(pl->phys.intangibleTimer, 15);
    const Vec2D tangent = vec2d(-normal.y, normal.x);
    const Vec2D reflectedDec = dotProd(pl->phys.kVel, normal) < 0
                                   ? reflect(pl->phys.kDec, tangent)
                                   : pl->phys.kDec;
    const Vec2D reflectedVel = dotProd(pl->phys.kVel, normal) < 0
                                   ? reflect(pl->phys.kVel, tangent)
                                   : pl->phys.kVel;
    pl->phys.kVel.x = reflectedVel.x * 0.8;
    pl->phys.kVel.y = reflectedVel.y * 0.8;
    pl->phys.kDec.x = reflectedDec.x;
    pl->phys.kDec.y = reflectedDec.y;
  }
  mv_turnOffHitboxes(S, p);
  mv_dispatch(S, MV_CS(S, p), "STOPCEIL", "main", p, in, 0);
  return AS_UNDEF;
}

static AsTri mv_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  const ml_attributes_t *at = mv_attr(MV_CS(S, p));
  pl->timer += 1;
  if (mv_dispatch(S, MV_CS(S, p), "STOPCEIL", "interrupt", p, in, 0) !=
      AS_TRUE) {
    if (pl->hit.hitstun > 0) {
      if (fmod(pl->hit.hitstun, 10) == 0) {
        ml_drawVfx_p("flyingDust", pl->phys.pos.x, pl->phys.pos.y);
      }
      pl->hit.hitstun -= 1;
      pl->phys.cVel.y -= ml_f64(at->gravity);
      if (pl->phys.cVel.y < -ml_f64(at->terminalV)) {
        pl->phys.cVel.y = -ml_f64(at->terminalV);
      }
    } else {
      as_airDrift((int)MV_CS(S, p), &pl->phys.cVel.x, MV_IN(in, p));
    }
  }
  return AS_UNDEF;
}

static AsTri mv_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  if (pl->timer > 5 && pl->hit.hitstun <= 0) {
    // upstream arm has NO return: falls past the chain -> undefined
    mv_dispatch(S, MV_CS(S, p), "FALL", "init", p, in, 0);
    return AS_UNDEF;
  } else if (pl->timer > mv_frames(MV_CS(S, p), "STOPCEIL")) {
    if (pl->hit.hitstun <= 0) {
      mv_dispatch(S, MV_CS(S, p), "DAMAGEFALL", "init", p, in, 0);
      return AS_TRUE;
    } else {
      pl->timer = mv_frames(MV_CS(S, p), "STOPCEIL");
      return AS_FALSE;
    }
  } else {
    return AS_FALSE;
  }
}

static AsTri mv_land(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  const MlInput *i0 = &MV_IN(in, p)[0];
  if (pl->hit.hitstun > 0) {
    if (pl->phys.techTimer > 0) {
      if (i0->lsX * pl->phys.face > 0.5) {
        mv_dispatch(S, MV_CS(S, p), "TECHF", "init", p, in, 0);
      } else if (i0->lsX * pl->phys.face < -0.5) {
        mv_dispatch(S, MV_CS(S, p), "TECHB", "init", p, in, 0);
      } else {
        mv_dispatch(S, MV_CS(S, p), "TECHN", "init", p, in, 0);
      }
    } else {
      mv_dispatch(S, MV_CS(S, p), "DOWNBOUND", "init", p, in, 0);
    }
  } else {
    mv_dispatch(S, MV_CS(S, p), "LANDING", "init", p, in, 0);
  }
  return AS_UNDEF;
}

const MlMoveDef mv_STOPCEIL = {"STOPCEIL", mv_init, mv_main, mv_interrupt,
                               mv_land};
