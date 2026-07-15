// THROWNMARTHBACK.c <- src/characters/marth/moves/THROWNMARTHBACK.js (M2 task 11).
// Guarded marth THROWN family (NO init -1 guard — this file alone); offsets from the mvData marth dump.
#include "../moves.h"

static AsTri mr_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex);
static AsTri mr_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex);

static AsTri mr_init(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "THROWNMARTHBACK");
  // NO grabbedBy === -1 init guard upstream (quirk)
  if (pl->phys.grabbedBy < p) {
    pl->timer = -1;
  } else {
    pl->timer = 0;
  }
  pl->phys.grounded = false;
  mv_pos_reassign(S, p, mv_player(S, pl->phys.grabbedBy)->phys.pos);
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
      const double grabbedBy = pl->phys.grabbedBy;
      if (grabbedBy == -1) {
        return AS_UNDEF; // upstream: bare `return;`
      }
      if (timer > mv_marth_arr_len("THROWNMARTHBACK", "offset")) {
        timer = mv_marth_arr_len("THROWNMARTHBACK", "offset") - 1;
      }
      Vec2D off;
      if (!mv_marth_pair("THROWNMARTHBACK", "offset", timer - 1, &off)) {
        mv_out_of_domain("THROWNMARTHBACK: offset index out of range");
      }
      const Vec2D gpos = mv_player(S, grabbedBy)->phys.pos;
      mv_pos_reassign(S, p, vec2d(gpos.x + off.x * pl->phys.face,
                                  gpos.y + off.y));
    }
  }
  return AS_UNDEF;
}

static AsTri mr_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex) {
  (void)S; (void)p; (void)in; (void)ex;
  return AS_FALSE;
}

const MlMoveDef marth_THROWNMARTHBACK = {"THROWNMARTHBACK", mr_init, mr_main, mr_interrupt,
                                0};
