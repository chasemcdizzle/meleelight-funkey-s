// CLIFFATTACKSLOW.c <- src/characters/marth/moves/CLIFFATTACKSLOW.js
// (M2 task 11). offset/setVelocities from the mvData marth dump.
// The onLedge === -1 arm writes `this.canGrabLedge = false` into
// the MOVE TABLE upstream — the C traps at the site (fox
// precedent; the finalCheck-guarded mvData dump would drift).
#include "../moves.h"

static AsTri mr_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex);
static AsTri mr_interrupt(MlSim *S, double p,
                          const MlInputBuffer in[4], const MvX *ex);

static AsTri mr_init(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "CLIFFATTACKSLOW");
  pl->timer = 0;
  pl->phys.intangibleTimer = 34;
  mv_turnOffHitboxes(S, p);
  mv_assign_hitbox_id(S, p, "ledgegetupslow", 0, 0);
  mv_assign_hitbox_id(S, p, "ledgegetupslow", 1, 1);
  mv_assign_hitbox_id(S, p, "ledgegetupslow", 2, 2);
  mr_main(S, p, in, 0);
  return AS_UNDEF;
}

static AsTri mr_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  pl->timer += 1;
  if (mr_interrupt(S, p, in, 0) != AS_TRUE) {
    const double onLedge = pl->phys.onLedge;
    if (onLedge == -1) {
      mv_out_of_domain("CLIFFATTACKSLOW: canGrabLedge table write arm");
      return AS_UNDEF;
    }
    const Vec2D lp = mv_ledge_point(S, onLedge,
                                    "CLIFFATTACKSLOW: ledge deref");
    const MlLedge *l = &S->stage.ledge[(int)onLedge];
    if (pl->timer < 33) {
      if (pl->timer > 9) {
        Vec2D off;
        if (!mv_marth_pair("CLIFFATTACKSLOW", "offset", pl->timer - 10,
                           &off)) {
          mv_out_of_domain("CLIFFATTACKSLOW: offset index out of range");
        }
        mv_pos_reassign(S, p,
                        vec2d(lp.x + (off.x + 68.4) * pl->phys.face,
                              lp.y + off.y));
      } else {
        mv_pos_reassign(S, p, vec2d(lp.x + (-71.31 + 68.4) * pl->phys.face,
                                    lp.y - 23.71));
      }
    } else {
      pl->phys.cVel.x = mv_marth_arr("CLIFFATTACKSLOW",
                                     "setVelocities",
                                     pl->timer - 33) *
                        pl->phys.face;
    }
    if (pl->timer == 32) {
      pl->phys.grounded = true;
      pl->phys.onSurface[0] = l->list == 'g' ? 0 : 1;
      pl->phys.onSurface[1] = l->index;
      pl->phys.airborneTimer = 0;
      mv_pos_set_y(S, p, lp.y);
    }

    if (pl->timer == 38) {
      pl->hitboxes.active[0] = true;
      pl->hitboxes.active[1] = true;
      pl->hitboxes.active[2] = true;
      pl->hitboxes.active[3] = false;
      S->aliasHbActive[(int)p] = false;
      pl->hitboxes.frame = 0;
      ml_sound_play("normalswing2");
      as_randomShout(MV_CS(S, p));
    } else if (pl->timer > 38 && pl->timer < 42) {
      pl->hitboxes.frame += 1;
    } else if (pl->timer == 42) {
      mv_turnOffHitboxes(S, p);
    }
  }
  return AS_UNDEF;
}

static AsTri mr_interrupt(MlSim *S, double p,
                          const MlInputBuffer in[4], const MvX *ex) {
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

const MlMoveDef marth_CLIFFATTACKSLOW = {"CLIFFATTACKSLOW", mr_init, mr_main, mr_interrupt,
                           0};
