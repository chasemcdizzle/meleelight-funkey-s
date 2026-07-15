// CLIFFJUMPSLOW.c <- src/characters/puff/moves/CLIFFJUMPSLOW.js (M2 task 12)
// pos t<18, cVel at t===18, drift+fastfall after; exits to FALL.
#include "../moves.h"

static AsTri pf_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex);
static AsTri pf_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex);

static AsTri pf_init(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "CLIFFJUMPSLOW");
  pl->timer = 0;
  pl->phys.intangibleTimer = 17;
  pf_main(S, p, in, 0);
  return AS_UNDEF;
}

static AsTri pf_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  pl->timer += 1;
  if (pf_interrupt(S, p, in, 0) != AS_TRUE) {
    const double onLedge = pl->phys.onLedge;
    if (onLedge == -1) {
      mv_out_of_domain("CLIFFJUMPSLOW: canGrabLedge table write arm");
      return AS_UNDEF;
    }
    const Vec2D lp = mv_ledge_point(S, onLedge, "CLIFFJUMPSLOW: ledge deref");
    if (pl->timer < 18) {
      Vec2D off;
      if (!mv_puff_pair("CLIFFJUMPSLOW", "offset", pl->timer - 1, &off)) {
        mv_out_of_domain("CLIFFJUMPSLOW: offset index out of range");
      }
      mv_pos_reassign(S, p, vec2d(lp.x + (off.x + 68.4) * pl->phys.face,
                                  lp.y + off.y));
    }

    if (pl->timer == 18) {
      pl->phys.cVel = vec2d(1.1 * pl->phys.face, 1.8);
    }
    if (pl->timer > 18) {
      as_airDrift((int)MV_CS(S, p), &pl->phys.cVel.x, MV_IN(in, p));
      as_fastfall((int)MV_CS(S, p), &pl->phys.cVel.y, &pl->phys.fastfalled,
                  MV_IN(in, p));
    }
  }
  return AS_UNDEF;
}

static AsTri pf_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  if (pl->timer > 38) {
    pl->phys.onLedge = -1;
    pl->phys.ledgeRegrabCount = false;
    mv_FALL.init(S, p, in, 0);
    return AS_TRUE;
  } else {
    return AS_FALSE;
  }
}

const MlMoveDef puff_CLIFFJUMPSLOW = {"CLIFFJUMPSLOW", pf_init, pf_main, pf_interrupt, 0};
