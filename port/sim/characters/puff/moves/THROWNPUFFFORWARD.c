// THROWNPUFFFORWARD.c <- src/characters/puff/moves/THROWNPUFFFORWARD.js (M2 task 12)
// Guarded family (guard-then-clamp).
#include "../moves.h"

static AsTri pf_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex);
static AsTri pf_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex);

static AsTri pf_init(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "THROWNPUFFFORWARD");
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
      const double grabbedBy = pl->phys.grabbedBy;
      if (grabbedBy == -1) {
        return AS_UNDEF;
      }
      if (timer > mv_puff_arr_len("THROWNPUFFFORWARD", "offset")) {
        timer = mv_puff_arr_len("THROWNPUFFFORWARD", "offset") - 1;
      }
        Vec2D off;
        if (!mv_puff_pair("THROWNPUFFFORWARD", "offset", timer - 1, &off)) {
          mv_out_of_domain("THROWNPUFFFORWARD: offset index out of range");
        }
        const Vec2D gpos = mv_player(S, grabbedBy)->phys.pos;
        mv_pos_reassign(S, p,
                        vec2d(gpos.x + off.x * pl->phys.face,
                              gpos.y + off.y));
    }
  }
  return AS_UNDEF;
}

static AsTri pf_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex) {
  (void)S; (void)p; (void)in; (void)ex;
  return AS_FALSE;
}

const MlMoveDef puff_THROWNPUFFFORWARD = {"THROWNPUFFFORWARD", pf_init, pf_main, pf_interrupt, 0};
