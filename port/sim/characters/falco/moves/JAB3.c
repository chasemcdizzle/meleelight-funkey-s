// JAB3.c <- src/characters/falco/moves/JAB3.js (M2 task 9)
#include "../moves.h"

static AsTri fc_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex);
static AsTri fc_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex);

static AsTri fc_init(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "JAB3");
  pl->timer = 0;
  pl->phys.jabCombo = false;
  mv_turnOffHitboxes(S, p);
  fc_main(S, p, in, 0);
  return AS_UNDEF;
}

static AsTri fc_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  const MlInput *i0 = &MV_IN(in, p)[0];
  const MlInput *i1 = &MV_IN(in, p)[1];
  pl->timer += 1;
  if (fc_interrupt(S, p, in, 0) != AS_TRUE) {
    as_reduceByTraction(true, (int)MV_CS(S, p), &pl->phys.cVel.x);
    if (pl->timer > 6 && pl->timer < 43 && i0->a && !i1->a) {
      pl->phys.jabCombo = true;
    }

    if (pl->timer == 9) {
      mv_assign_hitbox_id(S, p, "jab3_1", 0, 0);
      mv_assign_hitbox_id(S, p, "jab3_1", 1, 1);
      mv_assign_hitbox_id(S, p, "jab3_1", 2, 2);
    } else if (pl->timer == 16) {
      mv_assign_hitbox_id(S, p, "jab3_2", 0, 0);
      mv_assign_hitbox_id(S, p, "jab3_2", 1, 1);
      mv_assign_hitbox_id(S, p, "jab3_2", 2, 2);
    } else if (pl->timer == 23) {
      mv_assign_hitbox_id(S, p, "jab3_3", 0, 0);
      mv_assign_hitbox_id(S, p, "jab3_3", 1, 1);
      mv_assign_hitbox_id(S, p, "jab3_3", 2, 2);
    } else if (pl->timer == 30) {
      mv_assign_hitbox_id(S, p, "jab3_4", 0, 0);
      mv_assign_hitbox_id(S, p, "jab3_4", 1, 1);
      mv_assign_hitbox_id(S, p, "jab3_4", 2, 2);
    } else if (pl->timer == 37) {
      mv_assign_hitbox_id(S, p, "jab3_5", 0, 0);
      mv_assign_hitbox_id(S, p, "jab3_5", 1, 1);
      mv_assign_hitbox_id(S, p, "jab3_5", 2, 2);
    }

    if (pl->timer > 8 && pl->timer < 40) {
      const double m = fmod(pl->timer, 7);
      if (m == 2) {
        pl->hitboxes.active[0] = true;
        pl->hitboxes.active[1] = true;
        pl->hitboxes.active[2] = true;
        pl->hitboxes.active[3] = false;
        S->aliasHbActive[(int)p] = false;
        pl->hitboxes.frame = 0;
        ml_sound_play("normalswing2");
      } else if (m == 3) {
        pl->hitboxes.frame += 1;
      } else if (m == 4) {
        mv_turnOffHitboxes(S, p);
      }
    }
    if (pl->timer == 43 && pl->phys.jabCombo) {
      pl->phys.jabCombo = false;
      pl->timer = 7;
    }
  }
  return AS_UNDEF;
}

static AsTri fc_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  if (pl->timer > 51) {
    mv_WAIT.init(S, p, in, 0);
    return AS_TRUE;
  } else {
    return AS_FALSE;
  }
}

const MlMoveDef falco_JAB3 = {"JAB3", fc_init, fc_main, fc_interrupt, 0};
