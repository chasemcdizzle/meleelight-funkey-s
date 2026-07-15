// ATTACKAIRB.c <- src/characters/puff/moves/ATTACKAIRB.js (M2 task 12)
// t===8 writes phys.AUTOCANCEL (lowercase upstream typo — the runtime-added field, distinct from constructor autoCancel) — carried verbatim.
#include "../moves.h"

static AsTri pf_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex);
static AsTri pf_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex);

static AsTri pf_land(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex);

static AsTri pf_init(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "ATTACKAIRB");
  pl->timer = 0;
  pl->phys.autoCancel = true;
  pl->inAerial = true;
  pl->hasInAerial = true;
  mv_turnOffHitboxes(S, p);
  pf_assign_hitbox_id(S, p, "bair", 0, 0);
  pf_assign_hitbox_id(S, p, "bair", 1, 1);
  pf_assign_hitbox_id(S, p, "bair", 2, 2);
  pf_main(S, p, in, 0);
  return AS_UNDEF;
}

static AsTri pf_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  pl->timer += 1;
  if (pf_interrupt(S, p, in, 0) != AS_TRUE) {
    as_fastfall((int)MV_CS(S, p), &pl->phys.cVel.y, &pl->phys.fastfalled,
                MV_IN(in, p));
    as_airDrift((int)MV_CS(S, p), &pl->phys.cVel.x, MV_IN(in, p));
    if (pl->timer == 8) {
      // player[p].phys.autocancel = false — the LOWERCASE typo field
      pl->phys.autocancelLower = false;
      pl->phys.hasAutocancelLower = true;
    }
    if (pl->timer == 9) {
      pl->hitboxes.active[0] = true;
      pl->hitboxes.active[1] = true;
      pl->hitboxes.active[2] = true;
      pl->hitboxes.active[3] = false;
      S->aliasHbActive[(int)p] = false; // fresh array upstream
      pl->hitboxes.frame = 0;
      ml_sound_play("normalswing1");
    }
    if (pl->timer > 9 && pl->timer < 13) {
      pl->hitboxes.frame += 1;
    }
    if (pl->timer == 13) {
      mv_turnOffHitboxes(S, p);
    }
    if (pl->timer == 26) {
      pl->phys.autoCancel = true;
    }
  }
  return AS_UNDEF;
}

static AsTri pf_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  if (pl->timer > 39) {
    mv_FALL.init(S, p, in, 0);
    return AS_TRUE;
  } else if (pl->timer > 30) {
    const AsPair a = as_checkForAerials(pl->phys.face, MV_IN(in, p));
    if (as_checkForMultiJump(S->tapJumpOff[(int)p], MV_IN(in, p)) &&
        pl->phys.jumpsUsed < 5) {
      puff_JUMPAERIALF.init(S, p, in, 0);
      return AS_TRUE;
    } else if (a.flag) {
      puff_moves_init(S, mv_pair_str(&a), p, in);
      return AS_TRUE;
    } else {
      return AS_FALSE;
    }
  } else {
    return AS_FALSE;
  }
}

static AsTri pf_land(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  if (pl->phys.autoCancel) {
    mv_LANDING.init(S, p, in, 0);
  } else {
    mv_LANDINGATTACKAIRB.init(S, p, in, 0);
  }
  return AS_UNDEF;
}

const MlMoveDef puff_ATTACKAIRB = {"ATTACKAIRB", pf_init, pf_main,
                                   pf_interrupt, pf_land};
