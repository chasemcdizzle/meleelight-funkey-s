// THROWNFALCOUP.c <- src/characters/falco/moves/THROWNFALCOUP.js (M2 task 9)
// NOTE falco's THROWN* have NO grabbedBy===-1 guards and NO offset-length
// clamps (upstream throws on player[-1]/offset overrun — mv_player /
// mv_falco_pair trap). this.offset comes from the mvData falco dump.
#include "../moves.h"

static AsTri fc_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex);
static AsTri fc_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex);

static AsTri fc_init(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "THROWNFALCOUP");
  if (pl->phys.grabbedBy < p) {
    pl->timer = -1;
  } else {
    pl->timer = 0;
  }
  pl->phys.grounded = false;
  {
    const Vec2D gpos = mv_player(S, pl->phys.grabbedBy)->phys.pos;
    mv_pos_reassign(S, p, vec2d(gpos.x, gpos.y));
  }
  fc_main(S, p, in, 0);
  return AS_UNDEF;
}

static AsTri fc_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  pl->timer += 1;
  if (fc_interrupt(S, p, in, 0) != AS_TRUE) {
    if (pl->timer > 0) {
      Vec2D off;
      if (!mv_falco_pair("THROWNFALCOUP", "offset", pl->timer - 1, &off)) {
        mv_out_of_domain("THROWNFALCOUP: offset index out of range");
      }
      const Vec2D gpos = mv_player(S, pl->phys.grabbedBy)->phys.pos;
      mv_pos_reassign(S, p, vec2d(gpos.x + off.x * pl->phys.face,
                                  gpos.y + off.y));
    }
  }
  return AS_UNDEF;
}

static AsTri fc_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex) {
  (void)S; (void)p; (void)in; (void)ex;
  return AS_FALSE;
}

const MlMoveDef falco_THROWNFALCOUP = {"THROWNFALCOUP", fc_init, fc_main, fc_interrupt,
    0};
