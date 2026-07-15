// CLIFFGETUPQUICK.c <- src/characters/marth/moves/CLIFFGETUPQUICK.js
// (M2 task 11). offset/setVelocities from the mvData marth dump.
// The onLedge === -1 arm writes `this.canGrabLedge = false` into
// the MOVE TABLE upstream — the C traps at the site (fox
// precedent; the finalCheck-guarded mvData dump would drift).
#include "../moves.h"

static AsTri mr_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex);
static AsTri mr_interrupt(MlSim *S, double p,
                          const MlInputBuffer in[4], const MvX *ex);

static AsTri mr_init(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "CLIFFGETUPQUICK");
  pl->timer = 0;
  pl->phys.intangibleTimer = 30;
  mr_main(S, p, in, 0);
  return AS_UNDEF;
}

static AsTri mr_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  pl->timer += 1;
  if (mr_interrupt(S, p, in, 0) != AS_TRUE) {
    const double onLedge = pl->phys.onLedge;
    if (onLedge == -1) {
      mv_out_of_domain("CLIFFGETUPQUICK: canGrabLedge table write arm");
      return AS_UNDEF;
    }
    const Vec2D lp = mv_ledge_point(S, onLedge,
                                    "CLIFFGETUPQUICK: ledge deref");
    const MlLedge *l = &S->stage.ledge[(int)onLedge];
    if (pl->timer < 16) {
      Vec2D off;
      if (!mv_marth_pair("CLIFFGETUPQUICK", "offset", pl->timer - 1,
                         &off)) {
        mv_out_of_domain("CLIFFGETUPQUICK: offset index out of range");
      }
      mv_pos_reassign(S, p,
                      vec2d(lp.x + (off.x + 68.4) * pl->phys.face,
                            lp.y + off.y));
    } else {
      Vec2D off;
      if (!mv_marth_pair("CLIFFGETUPQUICK", "offset", pl->timer - 1,
                         &off)) {
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

static AsTri mr_interrupt(MlSim *S, double p,
                          const MlInputBuffer in[4], const MvX *ex) {
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

const MlMoveDef marth_CLIFFGETUPQUICK = {"CLIFFGETUPQUICK", mr_init, mr_main, mr_interrupt,
                           0};
