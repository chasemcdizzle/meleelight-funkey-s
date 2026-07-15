// DOWNSPECIALGROUND2.c <- src/characters/marth/moves/DOWNSPECIALGROUND2.js
// (M2 task 11 — counter retaliation, ground). NOTE init runs
// reduceByTraction FIRST (before the actionState write) — verbatim.
#include "../moves.h"

static AsTri mr_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex);
static AsTri mr_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex);

static AsTri mr_init(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  as_reduceByTraction(true, (int)MV_CS(S, p), &pl->phys.cVel.x);
  strcpy(pl->actionState, "DOWNSPECIALGROUND2");
  pl->timer = 0;
  pl->phys.intangibleTimer = 16;
  mv_turnOffHitboxes(S, p);
  mv_assign_hitbox_id(S, p, "downspecialground2", 0, 0);
  mv_assign_hitbox_id(S, p, "downspecialground2", 1, 1);
  mv_assign_hitbox_id(S, p, "downspecialground2", 2, 2);
  mv_assign_hitbox_id(S, p, "downspecialground2", 3, 3);
  ml_sound_play("powershield");
  ml_sound_play("marthcounterclank");
  ml_sound_play("marthcountershout");
  mv_drawVfx("impactLand");
  mv_drawVfx("powershield");
  mr_main(S, p, in, 0);
  return AS_UNDEF;
}

static AsTri mr_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  pl->timer += 1;
  if (mr_interrupt(S, p, in, 0) != AS_TRUE) {
    if (pl->timer == 4) {
      pl->hitboxes.active[0] = true;
      pl->hitboxes.active[1] = true;
      pl->hitboxes.active[2] = true;
      pl->hitboxes.active[3] = true;
      S->aliasHbActive[(int)p] = false;
      pl->hitboxes.frame = 0;
    } else if (pl->timer > 4 && pl->timer < 11) {
      pl->hitboxes.frame += 1;
    } else if (pl->timer == 11) {
      mv_turnOffHitboxes(S, p);
    }
  }
  return AS_UNDEF;
}

static AsTri mr_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  if (pl->timer > 36) {
    if (pl->phys.grounded) {
      mv_WAIT.init(S, p, in, 0);
    } else {
      mv_FALL.init(S, p, in, 0);
    }
    return AS_TRUE;
  } else {
    return AS_FALSE;
  }
}

const MlMoveDef marth_DOWNSPECIALGROUND2 = {"DOWNSPECIALGROUND2", mr_init,
                                            mr_main, mr_interrupt, 0};
