// CAPTUREDAMAGE.c <- src/characters/shared/moves/CAPTUREDAMAGE.js
// (M2 task 7). setPositions is authored move data — served through the
// mvData capture dump (executed upstream data, never retyped).
#include "../moves.h"

static AsTri mv_init(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "CAPTUREDAMAGE");
  pl->timer = 0;
  mv_dispatch(S, MV_CS(S, p), "CAPTUREDAMAGE", "main", p, in, 0);
  return AS_UNDEF;
}

static AsTri mv_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  pl->timer += 1;
  if (mv_dispatch(S, MV_CS(S, p), "CAPTUREDAMAGE", "interrupt", p, in, 0) !=
      AS_TRUE) {
    const double grabbedBy = pl->phys.grabbedBy;
    if (grabbedBy == -1) {
      return AS_UNDEF;
    }
    MlPlayer *g = mv_player(S, grabbedBy);
    mv_pos_set_x(S, p,
                 g->phys.pos.x +
                     (-mv_setPosition_capturedamage(MV_CS(S, p),
                                                    pl->timer - 1) *
                      pl->phys.face));
  }
  return AS_UNDEF;
}

static AsTri mv_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  if (pl->timer > mv_frames(MV_CS(S, p), "CAPTUREDAMAGE")) {
    mv_dispatch(S, MV_CS(S, p), "CAPTUREWAIT", "init", p, in, 0);
    return AS_TRUE;
  } else {
    return AS_FALSE;
  }
}

const MlMoveDef mv_CAPTUREDAMAGE = {"CAPTUREDAMAGE", mv_init, mv_main,
                                    mv_interrupt, 0};
