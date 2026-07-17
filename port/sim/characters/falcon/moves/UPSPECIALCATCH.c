// UPSPECIALCATCH.c <- src/characters/falcon/moves/UPSPECIALCATCH.js
// (M2 task 10). The dive grab: main's timer==2 arm draws the seeded
// stream INLINE (2 draws per firefoxtail x3 — the render-only positions
// are (-0.5+Math.random())*17 offsets; the DRAWS are chain state); the
// interrupt's >16 grabbed arm assigns falcondivethrow, pushes the
// hitQueue row [grabbing,p,0,false,true,false] (mv_hq_push6) and chains
// UPSPECIALTHROW; grabbing===-1 is a bare `return;` (AS_UNDEF); the
// grabbedBy-mismatch arm ("exiting" console.log upstream — a no-op here)
// also chains UPSPECIALTHROW. land is empty.
#include "../moves.h"

static AsTri fc4_main(MlSim *S, double p, const MlInputBuffer in[4],
                      const MvX *ex);
static AsTri fc4_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                           const MvX *ex);

static AsTri fc4_init(MlSim *S, double p, const MlInputBuffer in[4],
                      const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "UPSPECIALCATCH");
  pl->timer = 0;
  // cVel = new Vec2D(0, 0)
  pl->phys.cVel = vec2d(0, 0);
  pl->phys.fastfalled = false;
  pl->phys.upbAngleMultiplier = 0;
  mv_turnOffHitboxes(S, p);
  mv_assign_hitbox_id(S, p, "falcondivethrowextra", 0, 0);
  pl->phys.landingMultiplier = 30.0 / 34;
  fc4_main(S, p, in, 0);
  return AS_UNDEF;
}

static AsTri fc4_main(MlSim *S, double p, const MlInputBuffer in[4],
                      const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  pl->timer += 1;
  if (fc4_interrupt(S, p, in, 0) != AS_TRUE) {
    if (pl->timer == 2) {
      pl->hitboxes.active[0] = true;
      pl->hitboxes.active[1] = false;
      pl->hitboxes.active[2] = false;
      pl->hitboxes.active[3] = false;
      S->aliasHbActive[(int)p] = false; // fresh array upstream
      pl->hitboxes.frame = 0;
      for (int n = 0; n < 3; n++) {
        // pos = new Vec2D(pos.x+(-0.5+Math.random())*17,
        //                 pos.y+5+(-0.5+Math.random())*17) — JS evaluates
        // the x draw first; C args are unsequenced, so sequence explicitly
        const double vfxX = pl->phys.pos.x + (-0.5 + ml_random()) * 17;
        const double vfxY = pl->phys.pos.y + 5 + (-0.5 + ml_random()) * 17;
        ml_drawVfx("firefoxtail", vfxX, vfxY, pl->phys.face);
      }
    }
    if (pl->timer == 4) {
      mv_turnOffHitboxes(S, p);
    }
  }
  return AS_UNDEF;
}

static AsTri fc4_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                           const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  if (pl->timer > 16) {
    if (pl->phys.grabbing != -1) {
      mv_assign_hitbox_id(S, p, "falcondivethrow", 0, 0);
      pl->hitboxes.active[0] = true;
      pl->hitboxes.active[1] = false;
      pl->hitboxes.active[2] = false;
      pl->hitboxes.active[3] = false;
      S->aliasHbActive[(int)p] = false; // fresh array upstream
      mv_hq_push6(S, pl->phys.grabbing, p, 0, false, true, false);
      mv_turnOffHitboxes(S, p);
    }
    falcon_UPSPECIALTHROW.init(S, p, in, 0);
    return AS_TRUE;
  } else {
    const double grabbing = pl->phys.grabbing;
    if (grabbing == -1) {
      return AS_UNDEF; // upstream: bare `return;`
    }
    if (pl->timer <= 16 && mv_player(S, grabbing)->phys.grabbedBy != p) {
      // upstream: console.log("exiting") — render/debug-only, no seam
      falcon_UPSPECIALTHROW.init(S, p, in, 0);
      return AS_TRUE;
    } else {
      return AS_FALSE;
    }
  }
}

static AsTri fc4_land(MlSim *S, double p, const MlInputBuffer in[4],
                      const MvX *ex) {
  (void)S; (void)p; (void)in; (void)ex; // empty land upstream
  return AS_UNDEF;
}

const MlMoveDef falcon_UPSPECIALCATCH = {
    "UPSPECIALCATCH", fc4_init, fc4_main, fc4_interrupt, fc4_land};
