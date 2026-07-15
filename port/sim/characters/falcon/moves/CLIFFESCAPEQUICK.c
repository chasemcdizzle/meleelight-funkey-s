// CLIFFESCAPEQUICK.c <- src/characters/falcon/moves/CLIFFESCAPEQUICK.js
// (M2 task 10). offset/setVelocities come from the mvData falcon dump.
// The onLedge === -1 arm writes `this.canGrabLedge = false` into the MOVE
// TABLE upstream — outside this cluster's value domain (and it would
// drift the finalCheck-guarded mvData dump): the C traps at that site.
#include "../moves.h"

static AsTri fc4_main(MlSim *S, double p, const MlInputBuffer in[4],
                      const MvX *ex);
static AsTri fc4_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                           const MvX *ex);

static AsTri fc4_init(MlSim *S, double p, const MlInputBuffer in[4],
                      const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "CLIFFESCAPEQUICK");
  pl->timer = 0;
  pl->phys.intangibleTimer = 24;
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
      mv_out_of_domain("CLIFFESCAPEQUICK: canGrabLedge table write arm");
      return AS_UNDEF;
    }
    const Vec2D lp = mv_ledge_point(S, onLedge, "CLIFFESCAPEQUICK: ledge deref");
    const MlLedge *l = &S->stage.ledge[(int)onLedge];
    if (pl->timer < 18) {
      Vec2D off;
      if (!mv_falcon_pair("CLIFFESCAPEQUICK", "offset", pl->timer - 1, &off)) {
        mv_out_of_domain("CLIFFESCAPEQUICK: offset index out of range");
      }
      mv_pos_reassign(S, p, vec2d(lp.x + (off.x + 68.4) * pl->phys.face,
                                  lp.y + off.y));
    } else {
      pl->phys.cVel.x = mv_falcon_arr("CLIFFESCAPEQUICK", "setVelocities",
                                      pl->timer - 18) * pl->phys.face;
    }
    if (pl->timer == 18) {
      pl->phys.grounded = true;
      pl->phys.onSurface[0] = l->list == 'g' ? 0 : 1;
      pl->phys.onSurface[1] = l->index;
      pl->phys.airborneTimer = 0;
      mv_pos_set_y(S, p, lp.y);
    }
  }
  return AS_UNDEF;
}

static AsTri fc4_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                           const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  if (pl->timer > 48) {
    pl->phys.onLedge = -1;
    pl->phys.ledgeRegrabCount = false;
    mv_WAIT.init(S, p, in, 0);
    return AS_TRUE;
  } else {
    return AS_FALSE;
  }
}

const MlMoveDef falcon_CLIFFESCAPEQUICK = {"CLIFFESCAPEQUICK", fc4_init, fc4_main,
                                fc4_interrupt, 0};
