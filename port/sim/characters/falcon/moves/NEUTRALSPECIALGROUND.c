// NEUTRALSPECIALGROUND.c <- src/characters/falcon/moves/
// NEUTRALSPECIALGROUND.js (M2 task 10). The falcon punch (grounded):
// falconpunchshout1 in init, the punch fires at 52 with three sounds,
// setVelocities kicks in at >=54, "falconpunch" vfx at 50, firefoxtail
// window 52..56 (id[0].offset[frame] crash-fidelity read). NO articles.
#include "../moves.h"

static AsTri fc4_main(MlSim *S, double p, const MlInputBuffer in[4],
                      const MvX *ex);
static AsTri fc4_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                           const MvX *ex);

static AsTri fc4_init(MlSim *S, double p, const MlInputBuffer in[4],
                      const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "NEUTRALSPECIALGROUND");
  pl->timer = 0;
  pl->phys.cVel.x = 0;
  mv_assign_hitbox_id(S, p, "falconpunchair", 0, 0);
  mv_assign_hitbox_id(S, p, "falconpunchair", 1, 1);
  mv_assign_hitbox_id(S, p, "falconpunchair", 2, 2);
  ml_sound_play("falconpunchshout1");
  fc4_main(S, p, in, 0);
  return AS_UNDEF;
}

static AsTri fc4_main(MlSim *S, double p, const MlInputBuffer in[4],
                      const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  pl->timer += 1;
  if (fc4_interrupt(S, p, in, 0) != AS_TRUE) {
    if (pl->timer >= 54) {
      pl->phys.cVel.x = mv_falcon_arr("NEUTRALSPECIALGROUND",
                                      "setVelocities", pl->timer - 54) *
                        pl->phys.face;
    }
    if (pl->timer == 52) {
      pl->hitboxes.active[0] = true;
      pl->hitboxes.active[1] = true;
      pl->hitboxes.active[2] = true;
      pl->hitboxes.active[3] = false;
      S->aliasHbActive[(int)p] = false; // fresh array upstream
      pl->hitboxes.frame = 0;
      ml_sound_play("falconpunchshout2");
      ml_sound_play("falconpunchbird");
      ml_sound_play("firemediumhit");
    }
    if (pl->timer > 52 && pl->timer < 57) {
      pl->hitboxes.frame += 1;
    }
    if (pl->timer == 57) {
      mv_turnOffHitboxes(S, p);
    }
    if (pl->timer >= 52 && pl->timer < 57) {
      const Vec2D off =
          mv_falcon_hb0_off(pl, "NEUTRALSPECIALGROUND: vfx offset");
      (void)off; // position is render-only; the vfx queue keeps names
      mv_drawVfx("firefoxtail");
    }
    if (pl->timer == 50) {
      mv_drawVfx("falconpunch");
    }
  }
  return AS_UNDEF;
}

static AsTri fc4_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                           const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  if (pl->timer > 99) {
    mv_WAIT.init(S, p, in, 0);
    return AS_TRUE;
  } else {
    return AS_FALSE;
  }
}

const MlMoveDef falcon_NEUTRALSPECIALGROUND = {
    "NEUTRALSPECIALGROUND", fc4_init, fc4_main, fc4_interrupt, 0};
