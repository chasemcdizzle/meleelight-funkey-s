// DEADDOWN.c <- src/characters/shared/moves/DEADDOWN.js (M2 task 7)
#include "../moves.h"

static AsTri mv_init(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "DEADDOWN");
  pl->timer = 0;
  pl->phys.cVel.x = 0;
  pl->phys.cVel.y = 0;
  pl->phys.kVel.x = 0;
  pl->phys.kVel.y = 0;
  pl->percent = 0;
  ml_drawVfx("blastzoneExplosion", pl->phys.pos.x, pl->phys.pos.y, 0);
  if (!mv_isFinalDeath(S)) {
    mv_screenShake();
    // percentShake: native-RNG HUD shake (CHECKSUM.md §7) — no-op
  }
  ml_sound_play("kill");
  mv_dispatch(S, MV_CS(S, p), "DEADDOWN", "main", p, in, 0);
  return AS_UNDEF;
}

static AsTri mv_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  pl->timer += 1;
  mv_playSounds(S, "DEAD", p);
  if (mv_dispatch(S, MV_CS(S, p), "DEADDOWN", "interrupt", p, in, 0) !=
      AS_TRUE) {
    pl->phys.outOfCameraTimer = 0;
    pl->phys.intangibleTimer = 2;
    pl->phys.hurtBoxState = 1;
    if (pl->timer == 4) {
      if (mv_isFinalDeath(S)) {
        // finishGame(input): match end — task 17's lifecycle surface
        mv_out_of_domain("DEADDOWN: finishGame (final death)");
      } else {
        mv_screenShake();
        // percentShake: no-op (native RNG)
      }
    }
  }
  return AS_UNDEF;
}

static AsTri mv_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  if (pl->timer > 60) {
    if (pl->stocks > 0) {
      mv_dispatch(S, MV_CS(S, p), "REBIRTH", "init", p, in, 0);
    } else {
      mv_dispatch(S, MV_CS(S, p), "SLEEP", "init", p, in, 0);
    }
    return AS_TRUE;
  } else {
    return AS_FALSE;
  }
}

const MlMoveDef mv_DEADDOWN = {"DEADDOWN", mv_init, mv_main, mv_interrupt, 0};
