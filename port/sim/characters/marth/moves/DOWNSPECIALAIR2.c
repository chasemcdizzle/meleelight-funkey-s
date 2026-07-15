// DOWNSPECIALAIR2.c <- src/characters/marth/moves/DOWNSPECIALAIR2.js
// (M2 task 11 — counter retaliation, air). NOTE the main's cVel.x decel
// has NO zero-crossing clamp (unlike DOWNSPECIALAIR) — verbatim.
#include "../moves.h"

static AsTri mr_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex);
static AsTri mr_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex);

static AsTri mr_init(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "DOWNSPECIALAIR2");
  pl->timer = 0;
  pl->phys.intangibleTimer = 16;
  mv_turnOffHitboxes(S, p);
  mv_assign_hitbox_id(S, p, "downspecialair2", 0, 0);
  mv_assign_hitbox_id(S, p, "downspecialair2", 1, 1);
  mv_assign_hitbox_id(S, p, "downspecialair2", 2, 2);
  mv_assign_hitbox_id(S, p, "downspecialair2", 3, 3);
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
    pl->phys.cVel.y -= 0.04;
    if (pl->phys.cVel.y < -1.2) {
      pl->phys.cVel.y = -1.2;
    }
    const double sign = js_sign(pl->phys.cVel.x);
    pl->phys.cVel.x -= sign * 0.0025;
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

const MlMoveDef marth_DOWNSPECIALAIR2 = {"DOWNSPECIALAIR2", mr_init,
                                         mr_main, mr_interrupt, 0};
