// CLIFFESCAPEQUICK.c <- src/characters/falco/moves/CLIFFESCAPEQUICK.js
// (M2 task 9). NOTE falco's CLIFF* have NO onLedge===-1 canGrabLedge
// table-write arm (fox's quirk) — ledge[-1] throws upstream and
// mv_ledge_point traps.
#include "../moves.h"

static AsTri fc_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex);
static AsTri fc_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex);

static AsTri fc_init(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "CLIFFESCAPEQUICK");
  pl->timer = 0;
  pl->phys.intangibleTimer = 34;
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
                                    "CLIFFESCAPEQUICK: ledge deref");
    const MlLedge *l = &S->stage.ledge[(int)onLedge];
    if (pl->timer < 22) {
      if (pl->timer >= 13) {
        Vec2D off;
        if (!mv_falco_pair("CLIFFESCAPEQUICK", "offset", pl->timer - 13,
                         &off)) {
          mv_out_of_domain("CLIFFESCAPEQUICK: offset index out of range");
        }
        mv_pos_reassign(S, p, vec2d(lp.x + (off.x + 68.4) * pl->phys.face,
                                    lp.y + off.y));
      }
    } else if (pl->timer < 50) {
      pl->phys.cVel.x = mv_falco_arr("CLIFFESCAPEQUICK", "setVelocities",
                                   pl->timer - 22) * pl->phys.face;
    }
    if (pl->timer == 22) {
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
  if (pl->timer > 49) {
    pl->phys.onLedge = -1;
    pl->phys.ledgeRegrabCount = false;
    mv_WAIT.init(S, p, in, 0);
    return AS_TRUE;
  } else {
    return AS_FALSE;
  }
}

const MlMoveDef falco_CLIFFESCAPEQUICK = {"CLIFFESCAPEQUICK", fc_init, fc_main,
                                        fc_interrupt, 0};
