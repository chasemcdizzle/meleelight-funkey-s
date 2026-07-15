// CLIFFATTACKSLOW.c <- src/characters/fox/moves/CLIFFATTACKSLOW.js
// (M2 task 8). NOTE upstream assigns id[2] = ledgegetupslow.id1 (id1
// twice — authored quirk, carried verbatim).
#include "../moves.h"

static AsTri fx_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex);
static AsTri fx_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex);

static AsTri fx_init(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "CLIFFATTACKSLOW");
  pl->timer = 0;
  pl->phys.intangibleTimer = 53;
  mv_turnOffHitboxes(S, p);
  mv_assign_hitbox_id(S, p, "ledgegetupslow", 0, 0);
  mv_assign_hitbox_id(S, p, "ledgegetupslow", 1, 1);
  mv_assign_hitbox_id(S, p, "ledgegetupslow", 1, 2); // id1 again (quirk)
  fx_main(S, p, in, 0);
  return AS_UNDEF;
}

static AsTri fx_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  pl->timer += 1;
  if (fx_interrupt(S, p, in, 0) != AS_TRUE) {
    const double onLedge = pl->phys.onLedge;
    if (onLedge == -1) {
      mv_out_of_domain("CLIFFATTACKSLOW: canGrabLedge table write arm");
      return AS_UNDEF;
    }
    const Vec2D lp = mv_ledge_point(S, onLedge,
                                    "CLIFFATTACKSLOW: ledge deref");
    const MlLedge *l = &S->stage.ledge[(int)onLedge];
    if (pl->timer < 54) {
      Vec2D off;
      if (!mv_fox_pair("CLIFFATTACKSLOW", "offset", pl->timer - 1, &off)) {
        mv_out_of_domain("CLIFFATTACKSLOW: offset index out of range");
      }
      mv_pos_reassign(S, p, vec2d(lp.x + (off.x + 68.4) * pl->phys.face,
                                  lp.y + off.y));
    } else {
      pl->phys.cVel.x = mv_fox_arr("CLIFFATTACKSLOW", "setVelocities",
                                   pl->timer - 54) * pl->phys.face;
    }
    if (pl->timer == 54) {
      pl->phys.grounded = true;
      pl->phys.onSurface[0] = l->list == 'g' ? 0 : 1;
      pl->phys.onSurface[1] = l->index;
      pl->phys.airborneTimer = 0;
      mv_pos_set_y(S, p, lp.y);
    }

    if (pl->timer == 57) {
      pl->hitboxes.active[0] = true;
      pl->hitboxes.active[1] = true;
      pl->hitboxes.active[2] = true;
      pl->hitboxes.active[3] = false;
      S->aliasHbActive[(int)p] = false;
      pl->hitboxes.frame = 0;
      ml_sound_play("normalswing2");
      as_randomShout(MV_CS(S, p));
    } else if (pl->timer > 57 && pl->timer < 60) {
      pl->hitboxes.frame += 1;
    } else if (pl->timer == 60) {
      mv_turnOffHitboxes(S, p);
    }
  }
  return AS_UNDEF;
}

static AsTri fx_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  if (pl->timer > 69) {
    pl->phys.onLedge = -1;
    pl->phys.ledgeRegrabCount = false;
    mv_WAIT.init(S, p, in, 0);
    return AS_TRUE;
  } else {
    return AS_FALSE;
  }
}

const MlMoveDef fox_CLIFFATTACKSLOW = {"CLIFFATTACKSLOW", fx_init, fx_main,
                                       fx_interrupt, 0};
