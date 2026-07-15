// JAB2.c <- src/characters/falcon/moves/JAB2.js (M2 task 10)
// Like JAB1: interrupt is jabCombo/WAIT only (dead checkFor imports).
#include "../moves.h"

static AsTri fc4_main(MlSim *S, double p, const MlInputBuffer in[4],
                      const MvX *ex);
static AsTri fc4_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                           const MvX *ex);

static AsTri fc4_init(MlSim *S, double p, const MlInputBuffer in[4],
                      const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "JAB2");
  pl->timer = 0;
  pl->phys.jabCombo = false;
  mv_turnOffHitboxes(S, p);
  mv_assign_hitbox_id(S, p, "jab2", 0, 0);
  mv_assign_hitbox_id(S, p, "jab2", 1, 1);
  mv_assign_hitbox_id(S, p, "jab2", 2, 2);
  fc4_main(S, p, in, 0);
  return AS_UNDEF;
}

static AsTri fc4_main(MlSim *S, double p, const MlInputBuffer in[4],
                      const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  const MlInput *i0 = &MV_IN(in, p)[0];
  const MlInput *i1 = &MV_IN(in, p)[1];
  pl->timer += 1;
  if (fc4_interrupt(S, p, in, 0) != AS_TRUE) {
    as_reduceByTraction(true, (int)MV_CS(S, p), &pl->phys.cVel.x);
    if (pl->timer > 1 && pl->timer < 26 && i0->a && !i1->a) {
      pl->phys.jabCombo = true;
    }
    if (pl->timer == 5) {
      pl->hitboxes.active[0] = true;
      pl->hitboxes.active[1] = true;
      pl->hitboxes.active[2] = true;
      pl->hitboxes.active[3] = false;
      S->aliasHbActive[(int)p] = false; // fresh array upstream
      pl->hitboxes.frame = 0;
      ml_sound_play("normalswing2");
    }
    if (pl->timer > 5 && pl->timer < 8) {
      pl->hitboxes.frame += 1;
    }
    if (pl->timer == 8) {
      mv_turnOffHitboxes(S, p);
    }
  }
  return AS_UNDEF;
}

static AsTri fc4_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                           const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  if (pl->timer > 7 && pl->phys.jabCombo) {
    falcon_JAB3.init(S, p, in, 0);
    return AS_TRUE;
  } else if (pl->timer > 20) {
    mv_WAIT.init(S, p, in, 0);
    return AS_TRUE;
  } else {
    return AS_FALSE;
  }
}

const MlMoveDef falcon_JAB2 = {"JAB2", fc4_init, fc4_main, fc4_interrupt, 0};
