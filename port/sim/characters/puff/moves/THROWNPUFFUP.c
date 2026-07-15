// THROWNPUFFUP.c <- src/characters/puff/moves/THROWNPUFFUP.js (M2 task 12)
// Guarded family; the vacuous `if(player[p].phys)` wrapper nests the -1 guard (no early return) — verbatim.
#include "../moves.h"

static AsTri pf_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex);
static AsTri pf_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex);

static AsTri pf_init(MlSim *S, double p, const MlInputBuffer in[4],
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
  pf_main(S, p, in, 0);
  return AS_UNDEF;
}

static AsTri pf_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  pl->timer += 1;
  if (pf_interrupt(S, p, in, 0) != AS_TRUE) {
    double timer = pl->timer;
    if (timer > 0) {
      // upstream wraps this in a vacuous `if (player[p].phys)` (always
      // truthy) and NESTS the != -1 guard (no early return):
      const double grabbedBy = pl->phys.grabbedBy;
      if (grabbedBy != -1) {
        if (timer > mv_puff_arr_len("THROWNPUFFUP", "offset")) {
          timer = mv_puff_arr_len("THROWNPUFFUP", "offset") - 1;
        }
        Vec2D off;
        if (!mv_puff_pair("THROWNPUFFUP", "offset", timer - 1, &off)) {
          mv_out_of_domain("THROWNPUFFUP: offset index out of range");
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

const MlMoveDef puff_THROWNPUFFUP = {"THROWNPUFFUP", pf_init, pf_main, pf_interrupt, 0};
