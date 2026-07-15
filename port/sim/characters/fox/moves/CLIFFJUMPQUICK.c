// CLIFFJUMPQUICK.c <- src/characters/fox/moves/CLIFFJUMPQUICK.js
// (M2 task 8). See CLIFFGETUPQUICK.c for the onLedge===-1 trap note.
// NOTE the >15 arm runs airDrift THEN fastfall (reversed vs the aerials).
#include "../moves.h"

static AsTri fx_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex);
static AsTri fx_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex);

static AsTri fx_init(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "CLIFFJUMPQUICK");
  pl->timer = 0;
  pl->phys.intangibleTimer = 14;
  fx_main(S, p, in, 0);
  return AS_UNDEF;
}

static AsTri fx_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  pl->timer += 1;
  if (fx_interrupt(S, p, in, 0) != AS_TRUE) {
    const double onLedge = pl->phys.onLedge;
    if (onLedge == -1) {
      mv_out_of_domain("CLIFFJUMPQUICK: canGrabLedge table write arm");
      return AS_UNDEF;
    }
    const Vec2D lp = mv_ledge_point(S, onLedge,
                                    "CLIFFJUMPQUICK: ledge deref");
    if (pl->timer < 15) {
      Vec2D off;
      if (!mv_fox_pair("CLIFFJUMPQUICK", "offset", pl->timer - 1, &off)) {
        mv_out_of_domain("CLIFFJUMPQUICK: offset index out of range");
      }
      mv_pos_reassign(S, p, vec2d(lp.x + (off.x + 68.4) * pl->phys.face,
                                  lp.y + off.y));
    }
    if (pl->timer == 15) {
      // cVel = new Vec2D(1.1*face, 4)
      pl->phys.cVel = vec2d(1.1 * pl->phys.face, 4);
    }
    if (pl->timer > 15) {
      as_airDrift((int)MV_CS(S, p), &pl->phys.cVel.x, MV_IN(in, p));
      as_fastfall((int)MV_CS(S, p), &pl->phys.cVel.y, &pl->phys.fastfalled,
                  MV_IN(in, p));
    }
  }
  return AS_UNDEF;
}

static AsTri fx_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  if (pl->timer > 51) {
    pl->phys.onLedge = -1;
    pl->phys.ledgeRegrabCount = false;
    mv_FALL.init(S, p, in, 0);
    return AS_TRUE;
  } else {
    return AS_FALSE;
  }
}

const MlMoveDef fox_CLIFFJUMPQUICK = {"CLIFFJUMPQUICK", fx_init, fx_main,
                                      fx_interrupt, 0};
