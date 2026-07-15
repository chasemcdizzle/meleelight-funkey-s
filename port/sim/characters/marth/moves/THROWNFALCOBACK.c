// THROWNFALCOBACK.c <- src/characters/marth/moves/THROWNFALCOBACK.js
// (M2 task 11; fox's translation plus MARTH'S ADDED `face *= -1` in init
// — the ONE body delta in the fox-family THROWN set, measured by diff;
// the main's x-offset multiplies plain face, unlike THROWNFALCONBACK's
// face * -1. Offsets from the mvData marth dump.) Unguarded family.
#include "../moves.h"

static AsTri fx_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex);
static AsTri fx_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex);

static AsTri fx_init(MlSim *S, double p, const MlInputBuffer in[4],
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
  pl->phys.face *= -1; // marth's added flip (measured delta vs fox)
  mv_pos_reassign(S, p, mv_player(S, pl->phys.grabbedBy)->phys.pos);
  fx_main(S, p, in, 0);
  return AS_UNDEF;
}

static AsTri fx_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  pl->timer += 1;
  if (fx_interrupt(S, p, in, 0) != AS_TRUE) {
    if (pl->timer > 0) {
      Vec2D off;
      if (!mv_marth_pair("THROWNFALCOBACK", "offset", pl->timer - 1, &off)) {
        mv_out_of_domain("THROWNFALCOBACK: offset index out of range");
      }
      const Vec2D gpos = mv_player(S, pl->phys.grabbedBy)->phys.pos;
      mv_pos_reassign(S, p, vec2d(gpos.x + off.x * pl->phys.face,
                                  gpos.y + off.y));
    }
  }
  return AS_UNDEF;
}

static AsTri fx_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex) {
  (void)S; (void)p; (void)in; (void)ex;
  return AS_FALSE;
}

const MlMoveDef marth_THROWNFALCOBACK = {"THROWNFALCOBACK", fx_init, fx_main,
                                       fx_interrupt, 0};
