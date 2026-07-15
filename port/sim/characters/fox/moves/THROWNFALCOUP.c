// THROWNFALCOUP.c <- src/characters/fox/moves/THROWNFALCOUP.js (M2 task 8)
// NOTE the FALCO/FALCON THROWN* family has NO grabbedBy guard and NO
// timer clamp upstream (player[grabbedBy] on -1 / offset[i] past the end
// would THROW there — mv_player / the mv_fox_pair trap mirror that).
#include "../moves.h"

static AsTri fx_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex);
static AsTri fx_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex);

static AsTri fx_init(MlSim *S, double p, const MlInputBuffer in[4],
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
      if (!mv_fox_pair("THROWNFALCOUP", "offset", pl->timer - 1, &off)) {
        mv_out_of_domain("THROWNFALCOUP: offset index out of range");
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

const MlMoveDef fox_THROWNFALCOUP = {"THROWNFALCOUP", fx_init, fx_main,
                                     fx_interrupt, 0};
