// NEUTRALSPECIALGROUND.c <- src/characters/puff/moves/NEUTRALSPECIALGROUND.js (M2 task 12)
// Rollout (grounded): charge window on phys.rollOut* (runtime-added,
// rule-8 read helpers), launch, roll with the per-frame dmg writes
// THROUGH the current id objects (the chd plane: pf_hb_set_dmg mirrors
// the value copy; the GLOBAL plane's evolution is measured per record).
// The interrupt's WAIT arm returns FALSE (verbatim quirk). NON-phase
// onPlayerHit delegates to NSA's (specialOnHit arm).
#include "../moves.h"

static AsTri pf_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex);
static AsTri pf_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex);

AsTri puff_NEUTRALSPECIALGROUND_onPlayerHit(MlSim *S, double p,
                                            const MlInputBuffer in[4],
                                            const MvX *ex);

static AsTri pf_init(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "NEUTRALSPECIALGROUND");
  pl->timer = 0;
  pf_set_rollOutCharging(pl, false);
  pf_set_rollOutCharge(pl, 0);
  pl->phys.rollOutDistance = 0;
  pf_set_rollOutChargeAttempt(pl, true);
  pf_set_rollOutVel(pl, 0.3);
  pf_set_rollOutPlayerHit(pl, false);
  pf_set_rollOutWallHit(pl, false);
  pf_set_rollOutPlayerHitTimer(pl, 0);
  strcpy(pl->colourOverlay, "rgba(255, 248, 88, 0.83)");
  pl->phys.cVel.x = 0.0001 * pl->phys.face;
  ml_sound_play("rolloutshout");
  mv_turnOffHitboxes(S, p);
  pf_main(S, p, in, 0);
  return AS_UNDEF;
}

static AsTri pf_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  const MlInput *i0 = &MV_IN(in, p)[0];
  if (pl->timer == 15) {
    mv_drawVfx("dashDust");
  }
  if (pl->timer >= 16 && pl->timer <= 45 && pf_rollOutChargeAttempt(pl)) {
    if (i0->b) {
      pf_set_rollOutCharging(pl, true);
      pf_set_rollOutCharge(pl, pf_rollOutCharge(pl) + 1);
      if (pf_rollOutCharge(pl) > 44) {
        pf_set_rollOutCharge(pl, 44);
      }
      if (pf_rollOutCharge(pl) >= 19) {
        if (pl->timer == 16) {
          mv_drawVfx("dashDust");
        }
      }
      pl->phys.cVel.x = 0.0001 * pl->phys.face;
    } else {
      pl->timer += 1;
      pf_set_rollOutCharging(pl, false);
      pf_set_rollOutChargeAttempt(pl, false);
      pf_set_rollOutVel(pl, js_min(4.2, (0.3 + (0.09 * pf_rollOutCharge(pl)))));
      ml_sound_play("stronghit");
      ml_sound_play("rolloutlaunch");
      ml_sound_play("rollouttickground");
      if (pf_rollOutCharge(pl) >= 19) {
        pl->hitboxes.frame = 0;
        pf_assign_hitbox_id(S, p, "neutralspecialground", 0, 0);
        pf_assign_hitbox_id(S, p, "neutralspecialground", 1, 1);
        pf_assign_hitbox_id(S, p, "neutralspecialground", 2, 2);
        pl->hitboxes.active[0] = true;
        pl->hitboxes.active[1] = true;
        pl->hitboxes.active[2] = true;
        pl->hitboxes.active[3] = false;
        S->aliasHbActive[(int)p] = false; // fresh array upstream
      }
    }
  }
  if (pf_rollOutCharging(pl) || pl->phys.rollOutDistance < 100) {
    pl->timer += 1 + (2 * (pf_rollOutCharge(pl) / 44));
    pl->colourOverlayBool = false;
    if (pl->timer >= 28 && pl->timer <= 34 && pf_rollOutCharge(pl) >= 19 &&
        !pf_rollOutPlayerHit(pl)) {
      pl->colourOverlayBool = true;
    }
    if (pl->timer > 45) {
      pl->timer = 16;
      ml_sound_play("rollouttickground");
    }
  } else {
    pl->timer += 1;
  }
  if (pf_interrupt(S, p, in, 0) != AS_TRUE) {
    if (pl->timer > 15 && pl->timer < 46 && !pf_rollOutCharging(pl) &&
        !pf_rollOutChargeAttempt(pl)) {
      pl->phys.rollOutDistance += 1;
      if (!pf_rollOutPlayerHit(pl)) {
        const double newDmg = 12 + js_round((pf_rollOutCharge(pl) - 19) / 4);
        pf_hb_set_dmg(S, p, 0, newDmg);
        pf_hb_set_dmg(S, p, 1, newDmg);
        pf_hb_set_dmg(S, p, 2, newDmg);
        if (pf_rollOutCharge(pl) >= 19) {
          if (fmod(pl->phys.rollOutDistance, 10) == 0) {
            mv_drawVfx("dashDust");
          }
        }
      }
      if (pl->phys.rollOutDistance > 100) {
        mv_turnOffHitboxes(S, p);
        pl->timer = 46;
        pl->phys.cVel.x *= 0.6;
        pl->colourOverlayBool = false;
      } else {
        pl->phys.cVel.x = pf_rollOutVel(pl) * pl->phys.face;
        if (i0->lsX * pl->phys.face < -0.49) {
          puff_NEUTRALSPECIALGROUNDTURN.init(S, p, in, 0);
          pl->colourOverlayBool = false;
        }
      }
    }
    if (pl->timer >= 46) {
      const double sign = js_sign(pl->phys.cVel.x);
      pl->phys.cVel.x -= 0.09 * sign;
      if (pl->phys.cVel.x * sign < 0) {
        pl->phys.cVel.x = 0;
      }
    }
  }
  return AS_UNDEF;
}

static AsTri pf_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  if (pl->timer > 77) {
    mv_WAIT.init(S, p, in, 0);
    return AS_FALSE; // verbatim: the WAIT arm returns false
  } else {
    return AS_FALSE;
  }
}

// special phase surface (hitDetection.js:493 specialOnHit)
AsTri puff_NEUTRALSPECIALGROUND_onPlayerHit(MlSim *S, double p,
                                            const MlInputBuffer in[4],
                                            const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "NEUTRALSPECIALAIR");
  puff_NEUTRALSPECIALAIR_onPlayerHit(S, p, in, 0);
  return AS_UNDEF;
}

const MlMoveDef puff_NEUTRALSPECIALGROUND = {
    "NEUTRALSPECIALGROUND", pf_init, pf_main, pf_interrupt, 0};
