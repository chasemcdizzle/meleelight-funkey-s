// CLIFFATTACKSLOW.c <- src/characters/puff/moves/CLIFFATTACKSLOW.js (M2 task 12)
// ledgegetupslow hitboxes; pos t<34, setVelocities else-arm (t-34), grounding t===33, active t===43 / off t===60.
#include "../moves.h"

static AsTri pf_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex);
static AsTri pf_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex);

static AsTri pf_init(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "CLIFFATTACKSLOW");
  pl->timer = 0;
  pl->phys.intangibleTimer = 39;
  mv_turnOffHitboxes(S, p);
  pf_assign_hitbox_id(S, p, "ledgegetupslow", 0, 0);
  pf_assign_hitbox_id(S, p, "ledgegetupslow", 1, 1);
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
      mv_out_of_domain("CLIFFATTACKSLOW: canGrabLedge table write arm");
      return AS_UNDEF;
    }
    const Vec2D lp = mv_ledge_point(S, onLedge, "CLIFFATTACKSLOW: ledge deref");
    const MlLedge *l = &S->stage.ledge[(int)onLedge];
    if (pl->timer < 34) {
      Vec2D off;
      if (!mv_puff_pair("CLIFFATTACKSLOW", "offset", pl->timer - 1, &off)) {
        mv_out_of_domain("CLIFFATTACKSLOW: offset index out of range");
      }
      mv_pos_reassign(S, p, vec2d(lp.x + (off.x + 68.4) * pl->phys.face,
                                  lp.y + off.y));
    } else {
      pl->phys.cVel.x = mv_puff_arr("CLIFFATTACKSLOW", "setVelocities",
                                    pl->timer - 34) *
                        pl->phys.face;
    }

    if (pl->timer == 33) {
      pl->phys.grounded = true;
      pl->phys.onSurface[0] = l->list == 'g' ? 0 : 1;
      pl->phys.onSurface[1] = l->index;
      pl->phys.airborneTimer = 0;
      mv_pos_set_y(S, p, lp.y);
    }

    if (pl->timer == 43) {
      pl->hitboxes.active[0] = true;
      pl->hitboxes.active[1] = true;
      pl->hitboxes.active[2] = false;
      pl->hitboxes.active[3] = false;
      S->aliasHbActive[(int)p] = false; // fresh array upstream
      pl->hitboxes.frame = 0;
      ml_sound_play("normalswing2");
    } else if (pl->timer > 43 && pl->timer < 60) {
      pl->hitboxes.frame += 1;
    } else if (pl->timer == 60) {
      mv_turnOffHitboxes(S, p);
    }
  }
  return AS_UNDEF;
}

static AsTri pf_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
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

const MlMoveDef puff_CLIFFATTACKSLOW = {"CLIFFATTACKSLOW", pf_init, pf_main, pf_interrupt, 0};
