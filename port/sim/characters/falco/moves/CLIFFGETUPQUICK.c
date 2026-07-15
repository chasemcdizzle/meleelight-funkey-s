// CLIFFGETUPQUICK.c <- src/characters/falco/moves/CLIFFGETUPQUICK.js
// (M2 task 9). offset/setVelocities come from the mvData falco dump.
// The onLedge === -1 arm writes `this.canGrabLedge = false` into the MOVE
// TABLE upstream — outside this cluster's value domain (and it would
// drift the finalCheck-guarded mvData dump): the C traps at that site.
#include "../moves.h"

static AsTri fc_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex);
static AsTri fc_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex);

static AsTri fc_init(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "CLIFFGETUPQUICK");
  pl->timer = 0;
  pl->phys.intangibleTimer = 30;
  fc_main(S, p, in, 0);
  return AS_UNDEF;
}

static AsTri fc_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  pl->timer += 1;
  if (fc_interrupt(S, p, in, 0) != AS_TRUE) {
    const Vec2D lp = mv_ledge_point(S, pl->phys.onLedge,
                                    "CLIFFGETUPQUICK: ledge deref");
    const MlLedge *l = &S->stage.ledge[(int)pl->phys.onLedge];
    if (pl->timer < 24) {
      if (pl->timer >= 14) {
        Vec2D off;
        if (!mv_falco_pair("CLIFFGETUPQUICK", "offset", pl->timer - 14,
                         &off)) {
          mv_out_of_domain("CLIFFGETUPQUICK: offset index out of range");
        }
        mv_pos_reassign(S, p, vec2d(lp.x + (off.x + 68.4) * pl->phys.face,
                                    lp.y + off.y));
      }
    } else {
      pl->phys.cVel.x = mv_falco_arr("CLIFFGETUPQUICK", "setVelocities",
                                   pl->timer - 24) * pl->phys.face;
    }
    if (pl->timer == 24) {
      pl->phys.grounded = true;
      pl->phys.onSurface[0] = l->list == 'g' ? 0 : 1;
      pl->phys.onSurface[1] = l->index;
      pl->phys.airborneTimer = 0;
      mv_pos_set_y(S, p, lp.y);
    }
  }
  return AS_UNDEF;
}

static AsTri fc_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  if (pl->timer > 33) {
    pl->phys.onLedge = -1;
    pl->phys.ledgeRegrabCount = true;
    mv_WAIT.init(S, p, in, 0);
    return AS_TRUE;
  } else {
    return AS_FALSE;
  }
}

const MlMoveDef falco_CLIFFGETUPQUICK = {"CLIFFGETUPQUICK", fc_init, fc_main,
                                       fc_interrupt, 0};
