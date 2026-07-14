// CAPTUREWAIT.c <- src/characters/shared/moves/CAPTUREWAIT.js (M2 task 7).
// The seeded-RNG mash-wiggle poster child (fix_plan §M2 rule 14 context):
// main's mash arm draws ONE seeded Math.random per firing frame.
#include "../moves.h"

static AsTri mv_init(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "CAPTUREWAIT");
  pl->timer = 0;
  const double grabbedBy = pl->phys.grabbedBy;
  if (grabbedBy == -1) {
    return AS_UNDEF; // upstream bare `return;`
  }
  MlPlayer *g = mv_player(S, grabbedBy);
  mv_pos_reassign(S, p, vec2d(g->phys.pos.x + (-9.04298 * pl->phys.face),
                              g->phys.pos.y));
  mv_dispatch(S, MV_CS(S, p), "CAPTUREWAIT", "main", p, in, 0);
  return AS_UNDEF;
}

static AsTri mv_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  pl->timer += 1;
  if (mv_dispatch(S, MV_CS(S, p), "CAPTUREWAIT", "interrupt", p, in, 0) !=
      AS_TRUE) {
    pl->phys.stuckTimer -= 1;
    if (as_mashOut(MV_IN(in, p))) {
      pl->phys.stuckTimer -= 3;
      // pos.x += ...: a COMPONENT write through the pos-ECB1 alias
      mv_pos_set_x(S, p,
                   pl->phys.pos.x + (0.5 * js_sign(ml_random() - 0.5)));
    } else {
      const double grabbedBy = pl->phys.grabbedBy;
      if (grabbedBy == -1) {
        return AS_UNDEF;
      }
      MlPlayer *g = mv_player(S, grabbedBy);
      mv_pos_set_x(S, p, g->phys.pos.x + (-9.04298 * pl->phys.face));
    }
  }
  return AS_UNDEF;
}

static AsTri mv_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  if (pl->phys.stuckTimer < 0) {
    const double grabbedBy = pl->phys.grabbedBy;
    if (grabbedBy == -1) {
      return AS_UNDEF; // upstream bare `return;`
    }
    // NOTE upstream dispatches BOTH through p's char table:
    mv_dispatch(S, MV_CS(S, p), "CATCHCUT", "init", grabbedBy, in, 0);
    mv_dispatch(S, MV_CS(S, p), "CAPTURECUT", "init", p, in, 0);
    return AS_TRUE;
  } else if (pl->timer > mv_frames(MV_CS(S, p), "CAPTUREWAIT")) {
    mv_dispatch(S, MV_CS(S, p), "CAPTUREWAIT", "init", p, in, 0);
    return AS_TRUE;
  } else {
    return AS_FALSE;
  }
}

const MlMoveDef mv_CAPTUREWAIT = {"CAPTUREWAIT", mv_init, mv_main,
                                  mv_interrupt, 0};
