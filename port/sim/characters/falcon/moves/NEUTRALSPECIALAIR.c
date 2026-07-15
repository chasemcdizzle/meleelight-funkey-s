// NEUTRALSPECIALAIR.c <- src/characters/falcon/moves/NEUTRALSPECIALAIR.js
// (M2 task 10). The falcon punch (aerial): friction/gravity before 50,
// setVelocities 50..64 with cVel.y pinned 0, fastfall/airDrift from 65.
// land writes actionState/cVel directly (no init dispatch). NO articles.
#include "../moves.h"

static AsTri fc4_main(MlSim *S, double p, const MlInputBuffer in[4],
                      const MvX *ex);
static AsTri fc4_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                           const MvX *ex);

static AsTri fc4_init(MlSim *S, double p, const MlInputBuffer in[4],
                      const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "NEUTRALSPECIALAIR");
  pl->timer = 0;
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
  const ml_attributes_t *attr = mv_attr(MV_CS(S, p));
  pl->timer += 1;
  if (fc4_interrupt(S, p, in, 0) != AS_TRUE) {
    if (pl->timer >= 65) {
      as_fastfall((int)MV_CS(S, p), &pl->phys.cVel.y, &pl->phys.fastfalled,
                  MV_IN(in, p));
      as_airDrift((int)MV_CS(S, p), &pl->phys.cVel.x, MV_IN(in, p));
    } else if (pl->timer >= 50) {
      pl->phys.cVel.x = mv_falcon_arr("NEUTRALSPECIALAIR", "setVelocities",
                                      pl->timer - 50) * pl->phys.face;
      pl->phys.cVel.y = 0;
    } else {
      pl->phys.cVel.x =
          js_sign(pl->phys.cVel.x) *
          js_max(js_abs(pl->phys.cVel.x) - ml_f64(attr->airFriction), 0);
      pl->phys.cVel.y = js_max(pl->phys.cVel.y - ml_f64(attr->gravity),
                               -ml_f64(attr->terminalV));
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
          mv_falcon_hb0_off(pl, "NEUTRALSPECIALAIR: vfx offset");
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
    mv_FALL.init(S, p, in, 0);
    return AS_TRUE;
  } else {
    return AS_FALSE;
  }
}

static AsTri fc4_land(MlSim *S, double p, const MlInputBuffer in[4],
                      const MvX *ex) {
  (void)in; (void)ex;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "NEUTRALSPECIALGROUND");
  pl->phys.cVel.x = 0;
  return AS_UNDEF;
}

const MlMoveDef falcon_NEUTRALSPECIALAIR = {
    "NEUTRALSPECIALAIR", fc4_init, fc4_main, fc4_interrupt, fc4_land};
