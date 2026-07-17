// SIDESPECIALGROUNDHIT.c <- src/characters/falcon/moves/
// SIDESPECIALGROUNDHIT.js (M2 task 10). GOTCHA carried verbatim: main
// reads player[p].PHYS.timer — physicsObject has NO timer field, so
// `undefined < 18` is false and the 0.30313 arm is DEAD; the else (cVel
// 0) always runs. The firefoxtail window 4..8 reads id[0].offset[frame].
#include "../moves.h"

static AsTri fc4_main(MlSim *S, double p, const MlInputBuffer in[4],
                      const MvX *ex);
static AsTri fc4_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                           const MvX *ex);

static AsTri fc4_init(MlSim *S, double p, const MlInputBuffer in[4],
                      const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "SIDESPECIALGROUNDHIT");
  pl->timer = 0;
  mv_assign_hitbox_id(S, p, "raptorboostgroundhit", 0, 0);
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
    // upstream: if (player[p].phys.timer < 18) — phys.timer is undefined
    // (no such constructor field anywhere): the comparison is always
    // false, the else always runs. Dead arm carried as a comment.
    pl->phys.cVel.x = 0;
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
          mv_falcon_hb0_off(pl, "SIDESPECIALGROUNDHIT: vfx offset");
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
  if (pl->timer > 25) {
    mv_WAIT.init(S, p, in, 0);
    return AS_TRUE;
  } else {
    return AS_FALSE;
  }
}

const MlMoveDef falcon_SIDESPECIALGROUNDHIT = {
    "SIDESPECIALGROUNDHIT", fc4_init, fc4_main, fc4_interrupt, 0};
