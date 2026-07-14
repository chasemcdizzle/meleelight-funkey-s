// REBIRTH.c <- src/characters/shared/moves/REBIRTH.js (M2 task 7)
#include "../moves.h"

static AsTri mv_init(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  (void)in;
  MlPlayer *pl = mv_player(S, p);
  const int k = (int)p;
  strcpy(pl->actionState, "REBIRTH");
  pl->timer = 1;
  if (k < 0 || k >= S->stage.respawnCount) {
    mv_out_of_domain("REBIRTH: respawnPoints index out of range");
  }
  mv_pos_set_x(S, p, S->stage.respawnPoints[k].x);
  mv_pos_set_y(S, p, S->stage.respawnPoints[k].y + 135);
  pl->phys.cVel.x = 0;
  pl->phys.cVel.y = -1.5;
  pl->phys.face = S->stage.respawnFace[k];
  pl->phys.doubleJumped = false;
  pl->phys.fastfalled = false;
  pl->phys.jumpsUsed = 0;
  pl->phys.wallJumpCount = 0;
  pl->phys.sideBJumpFlag = true;
  pl->spawnWaitTime = 0;
  pl->percent = 0;
  pl->phys.kVel.x = 0;
  pl->phys.kVel.y = 0;
  pl->hit.hitstun = 0;
  pl->phys.shieldHP = 60;
  pl->burning = 0;
  pl->shocked = 0;
  return AS_UNDEF; // upstream REBIRTH.init does NOT call main
}

static AsTri mv_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  pl->timer += 1;
  if (mv_dispatch(S, MV_CS(S, p), "REBIRTH", "interrupt", p, in, 0) !=
      AS_TRUE) {
    pl->phys.outOfCameraTimer = 0;
  }
  return AS_UNDEF;
}

static AsTri mv_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  if (pl->timer > 90) {
    mv_dispatch(S, MV_CS(S, p), "REBIRTHWAIT", "init", p, in, 0);
    return AS_TRUE;
  } else {
    return AS_FALSE;
  }
}

const MlMoveDef mv_REBIRTH = {"REBIRTH", mv_init, mv_main, mv_interrupt, 0};
