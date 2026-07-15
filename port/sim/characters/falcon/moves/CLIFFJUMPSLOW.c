// CLIFFJUMPSLOW.c <- src/characters/falcon/moves/CLIFFJUMPSLOW.js
// (M2 task 10). offset comes from the mvData falcon dump. The
// onLedge === -1 canGrabLedge table-write arm traps (fox precedent).
#include "../moves.h"

static AsTri fc4_main(MlSim *S, double p, const MlInputBuffer in[4],
                      const MvX *ex);
static AsTri fc4_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                           const MvX *ex);

static AsTri fc4_init(MlSim *S, double p, const MlInputBuffer in[4],
                      const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "CLIFFJUMPSLOW");
  pl->timer = 0;
  pl->phys.intangibleTimer = 18;
  fc4_main(S, p, in, 0);
  return AS_UNDEF;
}

static AsTri fc4_main(MlSim *S, double p, const MlInputBuffer in[4],
                      const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  pl->timer += 1;
  if (fc4_interrupt(S, p, in, 0) != AS_TRUE) {
    const double onLedge = pl->phys.onLedge;
    if (onLedge == -1) {
      mv_out_of_domain("CLIFFJUMPSLOW: canGrabLedge table write arm");
      return AS_UNDEF;
    }
    const Vec2D lp = mv_ledge_point(S, onLedge, "CLIFFJUMPSLOW: ledge deref");
    if (pl->timer < 19) {
      Vec2D off;
      if (!mv_falcon_pair("CLIFFJUMPSLOW", "offset", pl->timer - 1, &off)) {
        mv_out_of_domain("CLIFFJUMPSLOW: offset index out of range");
      }
      mv_pos_reassign(S, p, vec2d(lp.x + (off.x + 68.4) * pl->phys.face,
                                  lp.y + off.y));
    }
    if (pl->timer == 19) {
      // cVel = new Vec2D(1*face, 3.3)
      pl->phys.cVel = vec2d(1 * pl->phys.face, 3.3);
    }
    if (pl->timer > 19) {
      as_airDrift((int)MV_CS(S, p), &pl->phys.cVel.x, MV_IN(in, p));
      as_fastfall((int)MV_CS(S, p), &pl->phys.cVel.y, &pl->phys.fastfalled,
                  MV_IN(in, p));
    }
  }
  return AS_UNDEF;
}

static AsTri fc4_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                           const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  if (pl->timer > 53) {
    pl->phys.onLedge = -1;
    pl->phys.ledgeRegrabCount = false;
    mv_FALL.init(S, p, in, 0);
    return AS_TRUE;
  } else {
    return AS_FALSE;
  }
}

const MlMoveDef falcon_CLIFFJUMPSLOW = {"CLIFFJUMPSLOW", fc4_init, fc4_main,
                                fc4_interrupt, 0};
