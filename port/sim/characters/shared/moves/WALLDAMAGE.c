// WALLDAMAGE.c <- src/characters/shared/moves/WALLDAMAGE.js (M2 task 7)
// (imports getHorizontal/VerticalDecay upstream but never calls them —
// dead imports, carried as this note)
#include "../moves.h"

#include <math.h>

static AsTri mv_init(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  // init(p, input, normal)
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "WALLDAMAGE");
  pl->timer = 0;
  ml_sound_play("bounce");
  pl->phys.hurtBoxState = 1;
  pl->phys.intangibleTimer = js_max(pl->phys.intangibleTimer, 15);
  pl->phys.cVel.x = 0;
  pl->phys.cVel.y = 0;
  if (ex == 0 || ex->count < 1 || ex->x[0].kind != DX_VEC) {
    mv_out_of_domain("WALLDAMAGE: missing normal arg");
  }
  const Vec2D normal = ex->x[0].vec;
  const Vec2D tangent = vec2d(-normal.y, normal.x);
  const Vec2D totalVel = vec2d(pl->phys.kVel.x + pl->phys.cVel.x,
                               pl->phys.kVel.y + pl->phys.cVel.y);
  const Vec2D reflectedDec = dotProd(totalVel, normal) < 0
                                 ? reflect(pl->phys.kDec, tangent)
                                 : pl->phys.kDec;
  const Vec2D reflectedVel = dotProd(totalVel, normal) < 0
                                 ? reflect(totalVel, tangent)
                                 : pl->phys.kVel;
  pl->phys.kVel.x = reflectedVel.x * 0.8;
  pl->phys.kVel.y = reflectedVel.y * 0.8;
  pl->phys.kDec.x = reflectedDec.x;
  pl->phys.kDec.y = reflectedDec.y;

  mv_dispatch(S, MV_CS(S, p), "WALLDAMAGE", "main", p, in, 0);
  return AS_UNDEF;
}

static AsTri mv_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  const ml_attributes_t *at = mv_attr(MV_CS(S, p));
  pl->timer += 1;
  if (fmod(pl->hit.hitstun, 10) == 0) {
    ml_drawVfx_p("flyingDust", pl->phys.pos.x, pl->phys.pos.y);
  }
  if (mv_dispatch(S, MV_CS(S, p), "WALLDAMAGE", "interrupt", p, in, 0) !=
      AS_TRUE) {
    pl->hit.hitstun -= 1;
    pl->phys.cVel.y -= ml_f64(at->gravity);
    if (pl->phys.cVel.y < -ml_f64(at->terminalV)) {
      pl->phys.cVel.y = -ml_f64(at->terminalV);
    }
  }
  return AS_UNDEF;
}

static AsTri mv_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  if (pl->timer > mv_frames(MV_CS(S, p), "WALLDAMAGE")) {
    mv_dispatch(S, MV_CS(S, p), "DAMAGEFALL", "init", p, in, 0);
    return AS_TRUE;
  } else {
    return AS_FALSE;
  }
}

const MlMoveDef mv_WALLDAMAGE = {"WALLDAMAGE", mv_init, mv_main, mv_interrupt,
                                 0};
