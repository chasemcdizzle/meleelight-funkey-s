// THROWNMARTHFORWARD.c <- src/characters/falcon/moves/THROWNMARTHFORWARD.js
// (M2 task 10; falcon-vs-fox diff is DATA-ONLY, structure verbatim from the task-8 translation). NOTE the clamp precedes the grabbedBy guard upstream.
#include "../moves.h"

static AsTri fc4_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex);
static AsTri fc4_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex);

static AsTri fc4_init(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "THROWNMARTHFORWARD");
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
  mv_pos_reassign(S, p, mv_player(S, grabbedBy)->phys.pos);
  fc4_main(S, p, in, 0);
  return AS_UNDEF;
}

static AsTri fc4_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  pl->timer += 1;
  if (fc4_interrupt(S, p, in, 0) != AS_TRUE) {
    double timer = pl->timer;
    if (timer > 0) {
      if (timer > mv_falcon_arr_len("THROWNMARTHFORWARD", "offset")) {
        timer = mv_falcon_arr_len("THROWNMARTHFORWARD", "offset") - 1;
      }
      const double grabbedBy = pl->phys.grabbedBy;
      if (grabbedBy == -1) {
        return AS_UNDEF;
      }
      Vec2D off;
      if (!mv_falcon_pair("THROWNMARTHFORWARD", "offset", timer - 1, &off)) {
        mv_out_of_domain("THROWNMARTHFORWARD: offset index out of range");
      }
      const Vec2D gpos = mv_player(S, grabbedBy)->phys.pos;
      mv_pos_reassign(S, p, vec2d(gpos.x + off.x * pl->phys.face,
                                  gpos.y + off.y));
    }
  }
  return AS_UNDEF;
}

static AsTri fc4_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex) {
  (void)S; (void)p; (void)in; (void)ex;
  return AS_FALSE;
}

const MlMoveDef falcon_THROWNMARTHFORWARD = {"THROWNMARTHFORWARD", fc4_init,
                                          fc4_main, fc4_interrupt, 0};
