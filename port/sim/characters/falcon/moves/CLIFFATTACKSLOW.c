// CLIFFATTACKSLOW.c <- src/characters/falcon/moves/CLIFFATTACKSLOW.js
// (M2 task 10). offset/setVelocities come from the mvData falcon dump.
// falcondoublejump plays in INIT; hitbox arms are an else-if chain at
// 37/41. The onLedge === -1 canGrabLedge table-write arm traps.
#include "../moves.h"

static AsTri fc4_main(MlSim *S, double p, const MlInputBuffer in[4],
                      const MvX *ex);
static AsTri fc4_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                           const MvX *ex);

static AsTri fc4_init(MlSim *S, double p, const MlInputBuffer in[4],
                      const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "CLIFFATTACKSLOW");
  pl->timer = 0;
  pl->phys.intangibleTimer = 33;
  mv_turnOffHitboxes(S, p);
  mv_assign_hitbox_id(S, p, "ledgegetupslow", 0, 0);
  mv_assign_hitbox_id(S, p, "ledgegetupslow", 1, 1);
  ml_sound_play("falcondoublejump");
  fc4_main(S, p, in, 0);
  return AS_UNDEF;
}

static AsTri fc4_main(MlSim *S, double p, const MlInputBuffer in[4],
                      const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  pl->timer += 1;
  if (fc4_interrupt(S, p, in, 0) != AS_TRUE) {
    const double onLedge = pl->phys.onLedge;
    if (onLedge == -1) {
      mv_out_of_domain("CLIFFATTACKSLOW: canGrabLedge table write arm");
      return AS_UNDEF;
    }
    const Vec2D lp = mv_ledge_point(S, onLedge,
                                    "CLIFFATTACKSLOW: ledge deref");
    const MlLedge *l = &S->stage.ledge[(int)onLedge];
    if (pl->timer < 29) {
      Vec2D off;
      if (!mv_falcon_pair("CLIFFATTACKSLOW", "offset", pl->timer - 1,
                          &off)) {
        mv_out_of_domain("CLIFFATTACKSLOW: offset index out of range");
      }
      mv_pos_reassign(S, p, vec2d(lp.x + (off.x + 68.4) * pl->phys.face,
                                  lp.y + off.y));
    } else {
      pl->phys.cVel.x = mv_falcon_arr("CLIFFATTACKSLOW", "setVelocities",
                                      pl->timer - 29) * pl->phys.face;
    }
    if (pl->timer == 29) {
      pl->phys.grounded = true;
      pl->phys.onSurface[0] = l->list == 'g' ? 0 : 1;
      pl->phys.onSurface[1] = l->index;
      pl->phys.airborneTimer = 0;
      mv_pos_set_y(S, p, lp.y);
    }

    if (pl->timer == 37) {
      pl->hitboxes.active[0] = true;
      pl->hitboxes.active[1] = true;
      pl->hitboxes.active[2] = false;
      pl->hitboxes.active[3] = false;
      S->aliasHbActive[(int)p] = false; // fresh array upstream
      pl->hitboxes.frame = 0;
    } else if (pl->timer > 37 && pl->timer < 41) {
      pl->hitboxes.frame += 1;
    } else if (pl->timer == 41) {
      mv_turnOffHitboxes(S, p);
    }
  }
  return AS_UNDEF;
}

static AsTri fc4_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                           const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  if (pl->timer > 68) {
    pl->phys.onLedge = -1;
    pl->phys.ledgeRegrabCount = false;
    mv_WAIT.init(S, p, in, 0);
    return AS_TRUE;
  } else {
    return AS_FALSE;
  }
}

const MlMoveDef falcon_CLIFFATTACKSLOW = {"CLIFFATTACKSLOW", fc4_init,
                                          fc4_main, fc4_interrupt, 0};
