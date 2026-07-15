// CLIFFJUMPSLOW.c <- src/characters/falco/moves/CLIFFJUMPSLOW.js
// (M2 task 9). See CLIFFJUMPQUICK.c (airDrift-then-fastfall order).
#include "../moves.h"

static AsTri fc_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex);
static AsTri fc_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex);

static AsTri fc_init(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "CLIFFJUMPSLOW");
  pl->timer = 0;
  pl->phys.intangibleTimer = 19;
  fc_main(S, p, in, 0);
  return AS_UNDEF;
}

static AsTri fc_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  pl->timer += 1;
  if (fc_interrupt(S, p, in, 0) != AS_TRUE) {
    const double onLedge = pl->phys.onLedge;
    const Vec2D lp = mv_ledge_point(S, onLedge,
                                    "CLIFFJUMPSLOW: ledge deref");
    if (pl->timer < 20) {
      Vec2D off;
      if (!mv_falco_pair("CLIFFJUMPSLOW", "offset", pl->timer - 1, &off)) {
        mv_out_of_domain("CLIFFJUMPSLOW: offset index out of range");
      }
      mv_pos_reassign(S, p, vec2d(lp.x + (off.x + 68.4) * pl->phys.face,
                                  lp.y + off.y));
    }
    if (pl->timer == 20) {
      pl->phys.cVel = vec2d(1 * pl->phys.face, 3.9);
    }
    if (pl->timer > 20) {
      as_airDrift((int)MV_CS(S, p), &pl->phys.cVel.x, MV_IN(in, p));
      as_fastfall((int)MV_CS(S, p), &pl->phys.cVel.y, &pl->phys.fastfalled,
                  MV_IN(in, p));
    }
  }
  return AS_UNDEF;
}

static AsTri fc_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
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

const MlMoveDef falco_CLIFFJUMPSLOW = {"CLIFFJUMPSLOW", fc_init, fc_main,
                                     fc_interrupt, 0};
