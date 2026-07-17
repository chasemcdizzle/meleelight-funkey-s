// DOWNSPECIALGROUND.c <- src/characters/falcon/moves/DOWNSPECIALGROUND.js
// (M2 task 10). Falcon kick (grounded): three-stage hitbox swap
// (Clean 14 / Mid 17 / Late 25), timer%2 firefoxtail while moving, and
// the specialWallCollide surface: onWallCollide(p,input,wallFace,wallNum)
// (physics.js:122) launches UPSPECIALTHROW when kicking INTO the wall
// (wallFace "R" with face -1 or "L" with face 1); wallNum is unread.
#include "../moves.h"

static AsTri fc4_main(MlSim *S, double p, const MlInputBuffer in[4],
                      const MvX *ex);
static AsTri fc4_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                           const MvX *ex);

static AsTri fc4_init(MlSim *S, double p, const MlInputBuffer in[4],
                      const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "DOWNSPECIALGROUND");
  pl->timer = 0;
  pl->phys.cVel.x = 0;
  pl->phys.cVel.y = 0;
  mv_assign_hitbox_id(S, p, "falconkickgroundClean", 0, 0);
  mv_assign_hitbox_id(S, p, "falconkickgroundClean", 1, 1);
  mv_assign_hitbox_id(S, p, "falconkickgroundClean", 2, 2);
  mv_turnOffHitboxes(S, p);
  ml_sound_play("falconkickshout");
  fc4_main(S, p, in, 0);
  return AS_UNDEF;
}

static AsTri fc4_main(MlSim *S, double p, const MlInputBuffer in[4],
                      const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  pl->timer += 1;
  if (fc4_interrupt(S, p, in, 0) != AS_TRUE) {
    if (pl->timer >= 12) {
      pl->phys.cVel.x = 2.67586 * pl->phys.face;
      if (fmod(pl->timer, 2) != 0) { // if (player[p].timer%2) truthiness
        ml_drawVfx("firefoxtail", pl->phys.pos.x + 12 * pl->phys.face,
                   pl->phys.pos.y + 3, pl->phys.face);
      }
    }
    if (pl->timer == 14) {
      pl->hitboxes.active[0] = true;
      pl->hitboxes.active[1] = true;
      pl->hitboxes.active[2] = true;
      pl->hitboxes.active[3] = false;
      S->aliasHbActive[(int)p] = false; // fresh array upstream
      pl->hitboxes.frame = 0;
      ml_sound_play("falconkick");
    }
    if (pl->timer > 14 && pl->timer < 33) {
      pl->hitboxes.frame += 1;
    }
    if (pl->timer == 17) {
      pl->hitboxes.active[0] = true;
      pl->hitboxes.active[1] = true;
      pl->hitboxes.active[2] = true;
      pl->hitboxes.active[3] = false;
      S->aliasHbActive[(int)p] = false; // fresh array upstream
      pl->hitboxes.frame = 0;
      mv_assign_hitbox_id(S, p, "falconkickgroundMid", 0, 0);
      mv_assign_hitbox_id(S, p, "falconkickgroundMid", 1, 1);
      mv_assign_hitbox_id(S, p, "falconkickgroundMid", 2, 2);
    }
    if (pl->timer == 25) {
      pl->hitboxes.active[0] = true;
      pl->hitboxes.active[1] = true;
      pl->hitboxes.active[2] = true;
      pl->hitboxes.active[3] = false;
      S->aliasHbActive[(int)p] = false; // fresh array upstream
      pl->hitboxes.frame = 0;
      mv_assign_hitbox_id(S, p, "falconkickgroundLate", 0, 0);
      mv_assign_hitbox_id(S, p, "falconkickgroundLate", 1, 1);
      mv_assign_hitbox_id(S, p, "falconkickgroundLate", 2, 2);
    }
    if (pl->timer == 33) {
      mv_turnOffHitboxes(S, p);
    }
  }
  return AS_UNDEF;
}

static AsTri fc4_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                           const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  if (pl->timer > 39) {
    if (pl->phys.grounded) {
      falcon_DOWNSPECIALGROUNDENDGROUND.init(S, p, in, 0);
    } else {
      falcon_DOWNSPECIALGROUNDENDAIR.init(S, p, in, 0);
    }
    return AS_TRUE;
  } else {
    return AS_FALSE;
  }
}

// special phase surface (moves.h: registered via falcon_special_phase)
AsTri falcon_DOWNSPECIALGROUND_onWallCollide(MlSim *S, double p,
                                             const MlInputBuffer in[4],
                                             const MvX *ex) {
  MlPlayer *pl = mv_player(S, p);
  if (ex == 0 || ex->count != 2 || ex->x[0].kind != DX_STR ||
      ex->x[0].str == 0 || ex->x[1].kind != DX_NUM) {
    mv_out_of_domain("DOWNSPECIALGROUND.onWallCollide extras");
  }
  const char *wallFace = ex->x[0].str; // wallNum (ex->x[1]) is unread
  if ((strcmp(wallFace, "R") == 0 && pl->phys.face == -1) ||
      (strcmp(wallFace, "L") == 0 && pl->phys.face == 1)) {
    pl->phys.grounded = false;
    falcon_UPSPECIALTHROW.init(S, p, in, 0);
  }
  return AS_UNDEF;
}

const MlMoveDef falcon_DOWNSPECIALGROUND = {
    "DOWNSPECIALGROUND", fc4_init, fc4_main, fc4_interrupt, 0};
