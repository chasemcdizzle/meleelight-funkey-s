// THROWNFALCOBACK.c <- src/characters/puff/moves/THROWNFALCOBACK.js (M2 task 12)
// Unguarded family; face flip but PLAIN-face x (no *-1) — verbatim quirk (unlike THROWNFALCODOWN's).
#include "../moves.h"

static AsTri pf_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex);
static AsTri pf_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex);

static AsTri pf_init(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "THROWNFALCOBACK");
  if (pl->phys.grabbedBy < p) {
    pl->timer = -1;
  } else {
    pl->timer = 0;
  }
  pl->phys.grounded = false;
  pl->phys.face *= -1;
  {
    const Vec2D gpos = mv_player(S, pl->phys.grabbedBy)->phys.pos;
    mv_pos_reassign(S, p, vec2d(gpos.x, gpos.y));
  }
  pf_main(S, p, in, 0);
  return AS_UNDEF;
}

static AsTri pf_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  pl->timer += 1;
  if (pf_interrupt(S, p, in, 0) != AS_TRUE) {
    if (pl->timer > 0) {
      // old-style unguarded family: player[grabbedBy] with grabbedBy=-1
      // THROWS upstream (mv_player traps); no length clamp (an offset
      // overrun THROWS upstream — mv_puff_pair's false arm traps).
      const double grabbedBy = pl->phys.grabbedBy;
      {
        double timer = pl->timer;
        Vec2D off;
        if (!mv_puff_pair("THROWNFALCOBACK", "offset", timer - 1, &off)) {
          mv_out_of_domain("THROWNFALCOBACK: offset index out of range");
        }
        const Vec2D gpos = mv_player(S, grabbedBy)->phys.pos;
        mv_pos_reassign(S, p,
                        vec2d(gpos.x + off.x * pl->phys.face,
                              gpos.y + off.y));
      }
    }
  }
  return AS_UNDEF;
}

static AsTri pf_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex) {
  (void)S; (void)p; (void)in; (void)ex;
  return AS_FALSE;
}

const MlMoveDef puff_THROWNFALCOBACK = {"THROWNFALCOBACK", pf_init, pf_main, pf_interrupt, 0};
