// THROWNFOXBACK.c <- src/characters/fox/moves/THROWNFOXBACK.js (M2 task 8)
#include "../moves.h"

static AsTri fx_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex);
static AsTri fx_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex);

static AsTri fx_init(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "THROWNFOXBACK");
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
  pl->phys.face *= -1;
  mv_pos_reassign(S, p, mv_player(S, grabbedBy)->phys.pos);
  fx_main(S, p, in, 0);
  return AS_UNDEF;
}

static AsTri fx_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  pl->timer += 1;
  if (fx_interrupt(S, p, in, 0) != AS_TRUE) {
    double timer = pl->timer;
    if (timer > 0) {
      const double grabbedBy = pl->phys.grabbedBy;
      if (grabbedBy == -1) {
        return AS_UNDEF;
      }
      if (timer > mv_fox_arr_len("THROWNFOXBACK", "offset")) {
        timer = mv_fox_arr_len("THROWNFOXBACK", "offset") - 1;
      }
      Vec2D off;
      if (!mv_fox_pair("THROWNFOXBACK", "offset", timer - 1, &off)) {
        mv_out_of_domain("THROWNFOXBACK: offset index out of range");
      }
      const Vec2D gpos = mv_player(S, grabbedBy)->phys.pos;
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

const MlMoveDef fox_THROWNFOXBACK = {"THROWNFOXBACK", fx_init, fx_main,
                                     fx_interrupt, 0};
