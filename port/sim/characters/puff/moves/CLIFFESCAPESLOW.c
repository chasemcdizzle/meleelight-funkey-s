// CLIFFESCAPESLOW.c <- src/characters/puff/moves/CLIFFESCAPESLOW.js (M2 task 12)
// pos t<33, setVelocities t<74, grounding at t===32 (one BEFORE the pos arm ends — verbatim).
#include "../moves.h"

static AsTri pf_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex);
static AsTri pf_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex);

static AsTri pf_init(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "CLIFFESCAPESLOW");
  pl->timer = 0;
  pl->phys.intangibleTimer = 53;
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
      mv_out_of_domain("CLIFFESCAPESLOW: canGrabLedge table write arm");
      return AS_UNDEF;
    }
    const Vec2D lp = mv_ledge_point(S, onLedge, "CLIFFESCAPESLOW: ledge deref");
    const MlLedge *l = &S->stage.ledge[(int)onLedge];
    if (pl->timer < 33) {
      Vec2D off;
      if (!mv_puff_pair("CLIFFESCAPESLOW", "offset", pl->timer - 1, &off)) {
        mv_out_of_domain("CLIFFESCAPESLOW: offset index out of range");
      }
      mv_pos_reassign(S, p, vec2d(lp.x + (off.x + 68.4) * pl->phys.face,
                                  lp.y + off.y));
    } else if (pl->timer < 74) {
      pl->phys.cVel.x = mv_puff_arr("CLIFFESCAPESLOW", "setVelocities",
                                    pl->timer - 33) *
                        pl->phys.face;
    }

    if (pl->timer == 32) {
      pl->phys.grounded = true;
      pl->phys.onSurface[0] = l->list == 'g' ? 0 : 1;
      pl->phys.onSurface[1] = l->index;
      pl->phys.airborneTimer = 0;
      mv_pos_set_y(S, p, lp.y);
    }
  }
  return AS_UNDEF;
}

static AsTri pf_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  if (pl->timer > 79) {
    pl->phys.onLedge = -1;
    pl->phys.ledgeRegrabCount = false;
    mv_WAIT.init(S, p, in, 0);
    return AS_TRUE;
  } else {
    return AS_FALSE;
  }
}

const MlMoveDef puff_CLIFFESCAPESLOW = {"CLIFFESCAPESLOW", pf_init, pf_main, pf_interrupt, 0};
