// CLIFFCATCH.c <- src/characters/shared/moves/CLIFFCATCH.js (M2 task 7)
#include "../moves.h"

// activeStage[l[0]][l[1]][l[2]] for l = activeStage.ledge[onLedge]
static Vec2D ledge_point(MlSim *S, double onLedge) {
  const int idx = (int)onLedge;
  if (onLedge != (double)idx || idx < 0 || idx >= S->stage.ledgeCount) {
    // ledge[-1] is undefined; activeStage[l[0]] then THROWS upstream
    mv_out_of_domain("CLIFFCATCH: ledge index out of range");
  }
  const MlLedge *l = &S->stage.ledge[idx];
  const SurfaceList *list =
      l->list == 'g' ? &S->stage.s.ground : &S->stage.s.platform;
  const int si = (int)l->index;
  if (si < 0 || si >= list->count) {
    mv_out_of_domain("CLIFFCATCH: ledge surface index out of range");
  }
  const Surface *sf = &list->items[si];
  return ((int)l->point == 0) ? sf->p0 : sf->p1;
}

static AsTri mv_init(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "CLIFFCATCH");
  pl->timer = 0;
  pl->phys.cVel.x = 0;
  pl->phys.cVel.y = 0;
  pl->phys.kVel.x = 0;
  pl->phys.kVel.y = 0;
  pl->phys.thrownHitbox = false;
  pl->phys.fastfalled = false;
  pl->phys.doubleJumped = false;
  pl->phys.jumpsUsed = 0;
  pl->phys.intangibleTimer = 38;
  pl->phys.hasLedgeHangTimer = true;
  pl->phys.ledgeHangTimer = 0;
  pl->rotation = 0;
  pl->rotationPoint = vec2d(0, 0);
  pl->colourOverlayBool = false;
  pl->phys.chargeFrames = 0;
  pl->phys.charging = false;
  mv_turnOffHitboxes(S, p);
  const Vec2D lp = ledge_point(S, pl->phys.onLedge); // const l = ...
  ml_drawVfx("cliffcatchspark", lp.x, lp.y, pl->phys.face);
  mv_dispatch(S, MV_CS(S, p), "CLIFFCATCH", "main", p, in, 0);
  return AS_UNDEF;
}

static AsTri mv_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  pl->timer += 1;
  mv_playSounds(S, "CLIFFCATCH", p);
  if (mv_dispatch(S, MV_CS(S, p), "CLIFFCATCH", "interrupt", p, in, 0) !=
      AS_TRUE) {
    const double onLedge = pl->phys.onLedge;
    if (onLedge == -1) {
      return AS_UNDEF;
    }
    const Vec2D lp = ledge_point(S, onLedge);
    Vec2D off;
    if (!mv_posOffsetCliffCatch(MV_CS(S, p), pl->timer - 1, &off)) {
      // posOffset[timer-1][0] on undefined THROWS upstream
      mv_out_of_domain("CLIFFCATCH: posOffset index out of range");
    }
    mv_pos_reassign(S, p, vec2d(lp.x + (off.x + 68.4) * pl->phys.face,
                                lp.y + off.y));
  }
  return AS_UNDEF;
}

static AsTri mv_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  if (pl->timer > mv_frames(MV_CS(S, p), "CLIFFCATCH")) {
    mv_dispatch(S, MV_CS(S, p), "CLIFFWAIT", "init", p, in, 0);
    return AS_TRUE;
  } else {
    return AS_FALSE;
  }
}

const MlMoveDef mv_CLIFFCATCH = {"CLIFFCATCH", mv_init, mv_main, mv_interrupt,
                                 0};
