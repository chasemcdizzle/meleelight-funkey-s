// CLIFFATTACKQUICK.c <- src/characters/falco/moves/CLIFFATTACKQUICK.js
// (M2 task 9). NOTE falco's CLIFF* have NO onLedge===-1 canGrabLedge
// table-write arm (fox's quirk) — ledge[-1] throws upstream and
// mv_ledge_point traps.
#include "../moves.h"

static AsTri fc_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex);
static AsTri fc_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex);

static AsTri fc_init(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "CLIFFATTACKQUICK");
  pl->timer = 0;
  pl->phys.intangibleTimer = 15;
  mv_turnOffHitboxes(S, p);
  mv_assign_hitbox_id(S, p, "ledgegetupquick", 0, 0);
  mv_assign_hitbox_id(S, p, "ledgegetupquick", 1, 1);
  mv_assign_hitbox_id(S, p, "ledgegetupquick", 2, 2);
  fc_main(S, p, in, 0);
  return AS_UNDEF;
}

static AsTri fc_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  pl->timer += 1;
  if (fc_interrupt(S, p, in, 0) != AS_TRUE) {
    const double onLedge = pl->phys.onLedge;
    const Vec2D lp = mv_ledge_point(S, onLedge,
                                    "CLIFFATTACKQUICK: ledge deref");
    const MlLedge *l = &S->stage.ledge[(int)onLedge];
    if (pl->timer < 24) {
      if (pl->timer >= 14) {
        Vec2D off;
        if (!mv_falco_pair("CLIFFATTACKQUICK", "offset", pl->timer - 14,
                         &off)) {
          mv_out_of_domain("CLIFFATTACKQUICK: offset index out of range");
        }
        mv_pos_reassign(S, p, vec2d(lp.x + (off.x + 68.4) * pl->phys.face,
                                    lp.y + off.y));
      }
    } else {
      pl->phys.cVel.x = mv_falco_arr("CLIFFATTACKQUICK", "setVelocities",
                                   pl->timer - 24) * pl->phys.face;
    }
    if (pl->timer == 24) {
      pl->phys.grounded = true;
      pl->phys.onSurface[0] = l->list == 'g' ? 0 : 1;
      pl->phys.onSurface[1] = l->index;
      pl->phys.airborneTimer = 0;
      mv_pos_set_y(S, p, lp.y);
    }

    if (pl->timer == 25) {
      pl->hitboxes.active[0] = true;
      pl->hitboxes.active[1] = true;
      pl->hitboxes.active[2] = true;
      pl->hitboxes.active[3] = false;
      S->aliasHbActive[(int)p] = false;
      pl->hitboxes.frame = 0;
      ml_sound_play("normalswing2");
      as_randomShout(MV_CS(S, p));
    } else if (pl->timer > 25 && pl->timer < 35) {
      pl->hitboxes.frame += 1;
    } else if (pl->timer == 35) {
      mv_turnOffHitboxes(S, p);
    }
  }
  return AS_UNDEF;
}

static AsTri fc_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  if (pl->timer > 54) {
    pl->phys.onLedge = -1;
    pl->phys.ledgeRegrabCount = false;
    mv_WAIT.init(S, p, in, 0);
    return AS_TRUE;
  } else {
    return AS_FALSE;
  }
}

const MlMoveDef falco_CLIFFATTACKQUICK = {"CLIFFATTACKQUICK", fc_init, fc_main,
                                        fc_interrupt, 0};
