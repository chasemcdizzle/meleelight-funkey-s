// CLIFFGETUPQUICK.c <- src/characters/puff/moves/CLIFFGETUPQUICK.js (M2 task 12)
// The lone ledgeRegrabCount=TRUE file (authored quirk); pos arm t<16 then a pos.x-only arm (see below — hand-adjusted).
#include "../moves.h"

static AsTri pf_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex);
static AsTri pf_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex);

static AsTri pf_init(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "CLIFFGETUPQUICK");
  pl->timer = 0;
  pl->phys.intangibleTimer = 30;
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
      mv_out_of_domain("CLIFFGETUPQUICK: canGrabLedge table write arm");
      return AS_UNDEF;
    }
    const Vec2D lp = mv_ledge_point(S, onLedge, "CLIFFGETUPQUICK: ledge deref");
    const MlLedge *l = &S->stage.ledge[(int)onLedge];
    if (pl->timer < 16) {
      Vec2D off;
      if (!mv_puff_pair("CLIFFGETUPQUICK", "offset", pl->timer - 1, &off)) {
        mv_out_of_domain("CLIFFGETUPQUICK: offset index out of range");
      }
      mv_pos_reassign(S, p, vec2d(lp.x + (off.x + 68.4) * pl->phys.face,
                                  lp.y + off.y));
    } else {
      // upstream: pos.x-only arm for t >= 16 (the 68.4 + offset ordering
      // is swapped vs the t<16 arm — verbatim shape)
      Vec2D off;
      if (!mv_puff_pair("CLIFFGETUPQUICK", "offset", pl->timer - 1, &off)) {
        mv_out_of_domain("CLIFFGETUPQUICK: offset index out of range");
      }
      mv_pos_set_x(S, p, lp.x + (68.4 + off.x) * pl->phys.face);
    }

    if (pl->timer == 16) {
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
  if (pl->timer > 32) {
    pl->phys.onLedge = -1;
    pl->phys.ledgeRegrabCount = true;
    mv_WAIT.init(S, p, in, 0);
    return AS_TRUE;
  } else {
    return AS_FALSE;
  }
}

const MlMoveDef puff_CLIFFGETUPQUICK = {"CLIFFGETUPQUICK", pf_init, pf_main, pf_interrupt, 0};
