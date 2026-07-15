// AERIALTURN2.c <- src/characters/puff/moves/AERIALTURN2.js (M2 task 12)
// The lsX-away multijump rung: flips face at t===6, hands off to JUMPAERIAL2 at t===13.
#include "../moves.h"

static AsTri pf_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex);
static AsTri pf_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex);

static AsTri pf_init(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  const MlInput *i0 = &MV_IN(in, p)[0];
  strcpy(pl->actionState, "AERIALTURN2");
  pl->timer = 0;
  pl->phys.fastfalled = false;
  pl->phys.doubleJumped = true;
  pl->phys.cVel.y = 1.59;
  pl->phys.cVel.x = (i0->lsX * 0.5);
  pl->phys.jumpsUsed += 1;
  ml_sound_play("jump2");
  pf_main(S, p, in, 0);
  return AS_UNDEF;
}

static AsTri pf_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  pl->timer += 1;
  if (pl->timer == 13) {
    pl->timer -= 1;
    strcpy(pl->actionState, "JUMPAERIAL2");
    puff_JUMPAERIAL2.main_(S, p, in, 0);
  } else {
    if (pf_interrupt(S, p, in, 0) != AS_TRUE) {
      as_fastfall((int)MV_CS(S, p), &pl->phys.cVel.y, &pl->phys.fastfalled,
                  MV_IN(in, p));
      puff_multi_jump_drift(S, p, in);
      if (pl->timer == 6) {
        pl->phys.face *= -1;
      }
    }
  }
  return AS_UNDEF;
}

static AsTri pf_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  const MlInput *i0 = &MV_IN(in, p)[0];
  const MlInput *i1 = &MV_IN(in, p)[1];
  const AsPair a = as_checkForAerials(pl->phys.face, MV_IN(in, p));
  const AsPair b = as_checkForSpecials(&pl->phys.face,
                                       pl->phys.bTurnaroundTimer,
                                       pl->phys.bTurnaroundDirection,
                                       pl->phys.grounded, MV_IN(in, p));
  if (a.flag) {
    puff_moves_init(S, mv_pair_str(&a), p, in);
    return AS_TRUE;
  } else if ((i0->l && !i1->l) || (i0->r && !i1->r)) {
    mv_ESCAPEAIR.init(S, p, in, 0);
    return AS_TRUE;
  } else if (b.flag) {
    puff_moves_init(S, mv_pair_str(&b), p, in);
    return AS_TRUE;
  } else {
    return AS_FALSE;
  }
}

const MlMoveDef puff_AERIALTURN2 = {"AERIALTURN2", pf_init, pf_main, pf_interrupt, 0};
