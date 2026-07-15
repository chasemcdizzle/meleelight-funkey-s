// THROWNFOXFORWARD.c <- src/characters/falcon/moves/THROWNFOXFORWARD.js
// (M2 task 10; falcon-vs-fox diff is DATA-ONLY, structure verbatim from the task-8 translation). NOTE upstream's main clamps TWICE: the first clamp writes
// player[p].timer (the LOCAL `timer` keeps its pre-clamp value), the
// second clamps the local — both carried verbatim.
#include "../moves.h"

static AsTri fc4_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex);
static AsTri fc4_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex);

static AsTri fc4_init(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "THROWNFOXFORWARD");
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
      if (timer > mv_falcon_arr_len("THROWNFOXFORWARD", "offset")) {
        pl->timer = mv_falcon_arr_len("THROWNFOXFORWARD", "offset") - 1;
      }
      const double grabbedBy = pl->phys.grabbedBy;
      if (grabbedBy == -1) {
        return AS_UNDEF;
      }
      if (timer > mv_falcon_arr_len("THROWNFOXFORWARD", "offset")) {
        timer = mv_falcon_arr_len("THROWNFOXFORWARD", "offset") - 1;
      }
      Vec2D off;
      if (!mv_falcon_pair("THROWNFOXFORWARD", "offset", timer - 1, &off)) {
        mv_out_of_domain("THROWNFOXFORWARD: offset index out of range");
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

const MlMoveDef falcon_THROWNFOXFORWARD = {"THROWNFOXFORWARD", fc4_init, fc4_main,
                                        fc4_interrupt, 0};
