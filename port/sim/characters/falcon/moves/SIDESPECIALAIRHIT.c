// SIDESPECIALAIRHIT.c <- src/characters/falcon/moves/SIDESPECIALAIRHIT.js
// (M2 task 10). cVel.y decays 0.05 every un-interrupted tick; firefoxtail
// window 4..8 reads id[0].offset[frame]. Dead `articles` import upstream.
#include "../moves.h"

static AsTri fc4_main(MlSim *S, double p, const MlInputBuffer in[4],
                      const MvX *ex);
static AsTri fc4_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                           const MvX *ex);

static AsTri fc4_init(MlSim *S, double p, const MlInputBuffer in[4],
                      const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "SIDESPECIALAIRHIT");
  pl->timer = 0;
  pl->phys.cVel.x = 0;
  pl->phys.cVel.y = 0;
  mv_assign_hitbox_id(S, p, "raptorboostairhit", 0, 0);
  mv_turnOffHitboxes(S, p);
  fc4_main(S, p, in, 0);
  return AS_UNDEF;
}

static AsTri fc4_main(MlSim *S, double p, const MlInputBuffer in[4],
                      const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  pl->timer += 1;
  if (fc4_interrupt(S, p, in, 0) != AS_TRUE) {
    pl->phys.cVel.y -= 0.05;
    if (pl->timer == 4) {
      pl->hitboxes.active[0] = true;
      pl->hitboxes.active[1] = false;
      pl->hitboxes.active[2] = false;
      pl->hitboxes.active[3] = false;
      S->aliasHbActive[(int)p] = false; // fresh array upstream
      pl->hitboxes.frame = 0;
    }
    if (pl->timer > 4 && pl->timer < 9) {
      pl->hitboxes.frame += 1;
    }
    if (pl->timer == 9) {
      mv_turnOffHitboxes(S, p);
    }
    if (pl->timer >= 4 && pl->timer < 9) {
      const Vec2D off =
          mv_falcon_hb0_off(pl, "SIDESPECIALAIRHIT: vfx offset");
      ml_drawVfx("firefoxtail",
                 pl->phys.pos.x + off.x * pl->phys.face,
                 pl->phys.pos.y + off.y, pl->phys.face);
    }
  }
  return AS_UNDEF;
}

static AsTri fc4_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                           const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  if (pl->timer > 45) {
    mv_FALLSPECIAL.init(S, p, in, 0);
    return AS_TRUE;
  } else {
    return AS_FALSE;
  }
}

static AsTri fc4_land(MlSim *S, double p, const MlInputBuffer in[4],
                      const MvX *ex) {
  (void)ex;
  mv_LANDINGFALLSPECIAL.init(S, p, in, 0);
  return AS_UNDEF;
}

const MlMoveDef falcon_SIDESPECIALAIRHIT = {
    "SIDESPECIALAIRHIT", fc4_init, fc4_main, fc4_interrupt, fc4_land};
