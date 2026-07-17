// TECHU.c <- src/characters/shared/moves/TECHU.js (M2 task 7)
#include "../moves.h"

static AsTri mv_init(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "TECHU");
  pl->timer = 0;
  pl->phys.cVel.y = 0;
  pl->phys.cVel.x = 0;
  pl->phys.kVel.y = 0;
  pl->phys.kVel.x = 0;
  pl->phys.fastfalled = false;
  pl->hit.knockback = 0;
  pl->hit.hitstun = 0;
  pl->phys.intangibleTimer = js_max(pl->phys.intangibleTimer, 14);
  ml_drawVfx_p("tech", pl->phys.ECBp[2].x, pl->phys.ECBp[2].y);
  ml_sound_play("tech");
  mv_turnOffHitboxes(S, p);
  mv_dispatch(S, MV_CS(S, p), "TECHU", "main", p, in, 0);
  return AS_UNDEF;
}

static AsTri mv_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  pl->timer += 1;
  mv_playSounds(S, "TECH", p);
  if (mv_dispatch(S, MV_CS(S, p), "TECHU", "interrupt", p, in, 0) !=
      AS_TRUE) {
    as_fastfall((int)MV_CS(S, p), &pl->phys.cVel.y, &pl->phys.fastfalled,
                MV_IN(in, p));
    as_airDrift((int)MV_CS(S, p), &pl->phys.cVel.x, MV_IN(in, p));
  }
  return AS_UNDEF;
}

static AsTri mv_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  if (pl->timer > mv_frames(MV_CS(S, p), "TECHU")) {
    mv_dispatch(S, MV_CS(S, p), "FALL", "init", p, in, 0);
    return AS_TRUE;
  } else {
    return AS_FALSE;
  }
}

const MlMoveDef mv_TECHU = {"TECHU", mv_init, mv_main, mv_interrupt, 0};
