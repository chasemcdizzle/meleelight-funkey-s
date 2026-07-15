// CLIFFATTACKQUICK.c <- src/characters/puff/moves/CLIFFATTACKQUICK.js (M2 task 12)
// ledgegetupquick hitboxes; pos t<15, setVelocities t<27, grounding t===15, active t===19 / off t===24.
#include "../moves.h"

static AsTri pf_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex);
static AsTri pf_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex);

static AsTri pf_init(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "CLIFFATTACKQUICK");
  pl->timer = 0;
  pl->phys.intangibleTimer = 15;
  mv_turnOffHitboxes(S, p);
  pf_assign_hitbox_id(S, p, "ledgegetupquick", 0, 0);
  pf_assign_hitbox_id(S, p, "ledgegetupquick", 1, 1);
  pf_main(S, p, in, 0);
  return AS_UNDEF;
}

static AsTri pf_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  pl->timer += 1;
  if (pf_interrupt(S, p, in, 0) != AS_TRUE) {
    const double onLedge = pl->phys.onLedge;
    if (onLedge == -1) {
      mv_out_of_domain("CLIFFATTACKQUICK: canGrabLedge table write arm");
      return AS_UNDEF;
    }
    const Vec2D lp = mv_ledge_point(S, onLedge, "CLIFFATTACKQUICK: ledge deref");
    const MlLedge *l = &S->stage.ledge[(int)onLedge];
    if (pl->timer < 15) {
      Vec2D off;
      if (!mv_puff_pair("CLIFFATTACKQUICK", "offset", pl->timer - 1, &off)) {
        mv_out_of_domain("CLIFFATTACKQUICK: offset index out of range");
      }
      mv_pos_reassign(S, p, vec2d(lp.x + (off.x + 68.4) * pl->phys.face,
                                  lp.y + off.y));
    } else if (pl->timer < 27) {
      pl->phys.cVel.x = mv_puff_arr("CLIFFATTACKQUICK", "setVelocities",
                                    pl->timer - 15) *
                        pl->phys.face;
    }

    if (pl->timer == 15) {
      pl->phys.grounded = true;
      pl->phys.onSurface[0] = l->list == 'g' ? 0 : 1;
      pl->phys.onSurface[1] = l->index;
      pl->phys.airborneTimer = 0;
      mv_pos_set_y(S, p, lp.y);
    }

    if (pl->timer == 19) {
      pl->hitboxes.active[0] = true;
      pl->hitboxes.active[1] = true;
      pl->hitboxes.active[2] = false;
      pl->hitboxes.active[3] = false;
      S->aliasHbActive[(int)p] = false; // fresh array upstream
      pl->hitboxes.frame = 0;
      ml_sound_play("normalswing2");
    } else if (pl->timer > 19 && pl->timer < 24) {
      pl->hitboxes.frame += 1;
    } else if (pl->timer == 24) {
      mv_turnOffHitboxes(S, p);
    }
  }
  return AS_UNDEF;
}

static AsTri pf_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  if (pl->timer > 55) {
    pl->phys.onLedge = -1;
    pl->phys.ledgeRegrabCount = false;
    mv_WAIT.init(S, p, in, 0);
    return AS_TRUE;
  } else {
    return AS_FALSE;
  }
}

const MlMoveDef puff_CLIFFATTACKQUICK = {"CLIFFATTACKQUICK", pf_init, pf_main, pf_interrupt, 0};
