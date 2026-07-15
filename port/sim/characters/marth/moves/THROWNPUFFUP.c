// THROWNPUFFUP.c <- src/characters/marth/moves/THROWNPUFFUP.js (M2 task 11).
// Guarded marth THROWN family (vacuous phys wrapper + nested guard); offsets from the mvData marth dump.
#include "../moves.h"

static AsTri mr_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex);
static AsTri mr_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex);

static AsTri mr_init(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "THROWNPUFFUP");
  const double grabbedBy = pl->phys.grabbedBy;
  if (grabbedBy == -1) {
    return AS_UNDEF;
  }
  if (grabbedBy < p) {
    pl->timer = -1;
  } else {
    pl->timer = 0;
  }
  pl->phys.grounded = false;
  mr_main(S, p, in, 0);
  return AS_UNDEF;
}

static AsTri mr_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  pl->timer += 1;
  if (mr_interrupt(S, p, in, 0) != AS_TRUE) {
    double timer = pl->timer;
    if (timer > 0) {
      // upstream wraps this in a vacuous `if (player[p].phys)` (always
      // truthy) and nests the != -1 guard:
      const double grabbedBy = pl->phys.grabbedBy;
      if (grabbedBy != -1) {
        if (timer > mv_marth_arr_len("THROWNPUFFUP", "offset")) {
          timer = mv_marth_arr_len("THROWNPUFFUP", "offset") - 1;
        }
        Vec2D off;
        if (!mv_marth_pair("THROWNPUFFUP", "offset", timer - 1, &off)) {
          mv_out_of_domain("THROWNPUFFUP: offset index out of range");
        }
        const Vec2D gpos = mv_player(S, grabbedBy)->phys.pos;
        mv_pos_reassign(S, p, vec2d(gpos.x + off.x * pl->phys.face,
                                            gpos.y + off.y));
      }
    }
  }
  return AS_UNDEF;
}

static AsTri mr_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex) {
  (void)S; (void)p; (void)in; (void)ex;
  return AS_FALSE;
}

const MlMoveDef marth_THROWNPUFFUP = {"THROWNPUFFUP", mr_init, mr_main, mr_interrupt,
                                0};
