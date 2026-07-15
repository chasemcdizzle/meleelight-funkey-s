// CLIFFJUMPSLOW.c <- src/characters/marth/moves/CLIFFJUMPSLOW.js
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
  strcpy(pl->actionState, "CLIFFJUMPSLOW");
  pl->timer = 0;
  pl->phys.intangibleTimer = 18;
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
      mv_out_of_domain("CLIFFJUMPSLOW: canGrabLedge table write arm");
      return AS_UNDEF;
    }
    const Vec2D lp = mv_ledge_point(S, onLedge,
                                    "CLIFFJUMPSLOW: ledge deref");
    const MlLedge *l = &S->stage.ledge[(int)onLedge];
    (void)l; // jumps never ground (no onSurface write)
    (void)l; // jumps never ground (no onSurface write)
    if (pl->timer < 19) {
      Vec2D off;
      if (!mv_marth_pair("CLIFFJUMPSLOW", "offset", pl->timer - 1,
                         &off)) {
        mv_out_of_domain("CLIFFJUMPSLOW: offset index out of range");
      }
      mv_pos_reassign(S, p,
                      vec2d(lp.x + (off.x + 68.4) * pl->phys.face,
                            lp.y + off.y));
    }
    if (pl->timer == 19) {
      pl->phys.cVel = vec2d(1 * pl->phys.face, 2.4);
    }
    if (pl->timer > 19) {
      as_airDrift((int)MV_CS(S, p), &pl->phys.cVel.x,
                  MV_IN(in, p));
      as_fastfall((int)MV_CS(S, p), &pl->phys.cVel.y,
                  &pl->phys.fastfalled, MV_IN(in, p));
    }
  }
  return AS_UNDEF;
}

static AsTri mr_interrupt(MlSim *S, double p,
                          const MlInputBuffer in[4], const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  if (pl->timer > 57) {
    pl->phys.onLedge = -1;
    pl->phys.ledgeRegrabCount = false;
    mv_FALL.init(S, p, in, 0);
    return AS_TRUE;
  } else {
    return AS_FALSE;
  }
}

const MlMoveDef marth_CLIFFJUMPSLOW = {"CLIFFJUMPSLOW", mr_init, mr_main, mr_interrupt,
                           0};
