// CAPTUREPULLED.c <- src/characters/shared/moves/CAPTUREPULLED.js
// (M2 task 7)
#include "../moves.h"

static AsTri mv_init(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "CAPTUREPULLED");
  pl->timer = 0;
  pl->phys.grounded = true;
  const double grabbedBy = pl->phys.grabbedBy;
  if (grabbedBy == -1) {
    return AS_UNDEF; // upstream bare `return;`
  }
  MlPlayer *g = mv_player(S, grabbedBy);
  pl->phys.face = -1 * g->phys.face;
  pl->phys.onSurface[0] = g->phys.onSurface[0];
  pl->phys.onSurface[1] = g->phys.onSurface[1];
  pl->phys.stuckTimer = 100 + (2 * pl->percent);
  ml_sound_play("grabbed");
  mv_dispatch(S, MV_CS(S, p), "CAPTUREPULLED", "main", p, in, 0);
  return AS_UNDEF;
}

static AsTri mv_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  pl->timer += 1;
  if (mv_dispatch(S, MV_CS(S, p), "CAPTUREPULLED", "interrupt", p, in, 0) !=
      AS_TRUE) {
    if (pl->timer == 2) {
      const double grabbedBy = pl->phys.grabbedBy;
      if (grabbedBy == -1) {
        return AS_UNDEF;
      }
      MlPlayer *g = mv_player(S, grabbedBy);
      mv_pos_reassign(S, p,
                      vec2d(g->phys.pos.x + (-16.41205 * pl->phys.face),
                            g->phys.pos.y));
    }
  }
  return AS_UNDEF;
}

static AsTri mv_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  if (pl->timer > 2) {
    mv_dispatch(S, MV_CS(S, p), "CAPTUREWAIT", "init", p, in, 0);
    const double grabbedBy = pl->phys.grabbedBy;
    if (grabbedBy == -1) {
      return AS_UNDEF; // upstream bare `return;`
    }
    mv_dispatch(S, MV_CS(S, p), "CATCHWAIT", "init", grabbedBy, in, 0);
    ml_drawVfx_p("tech", pl->phys.pos.x, pl->phys.pos.y + 10);
    return AS_TRUE;
  } else {
    return AS_FALSE;
  }
}

const MlMoveDef mv_CAPTUREPULLED = {"CAPTUREPULLED", mv_init, mv_main,
                                    mv_interrupt, 0};
