// NEUTRALSPECIALAIR.c <- src/characters/puff/moves/NEUTRALSPECIALAIR.js (M2 task 12)
// Rollout (airborne): overlay check BEFORE the timer advance (NSG's is
// after — verbatim per file); carries land + the two special-phase
// surfaces onWallCollide/onPlayerHit. The interrupt's FALLSPECIAL arm
// returns FALSE (verbatim quirk). onWallCollide's wall-coordinate read
// feeds the widened "wallBounce" vfx config (M4 task 1; wallNum is valid
// by physics construction).
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
  strcpy(pl->actionState, "NEUTRALSPECIALAIR");
  pl->timer = 0;
  pf_set_rollOutCharging(pl, false);
  pf_set_rollOutCharge(pl, 0);
  pl->phys.rollOutDistance = 0;
  pf_set_rollOutChargeAttempt(pl, true);
  pf_set_rollOutVel(pl, 0.5);
  pf_set_rollOutPlayerHit(pl, false);
  pf_set_rollOutWallHit(pl, false);
  pf_set_rollOutPlayerHitTimer(pl, 0);
  strcpy(pl->colourOverlay, "rgba(255, 248, 88, 0.83)");
  pl->phys.cVel.y = js_max(-1.3, pl->phys.cVel.y);
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
    ml_drawVfx("dashDust", pl->phys.pos.x, pl->phys.pos.y, pl->phys.face);
  }
  if (pl->timer >= 16 && pl->timer <= 45 && pf_rollOutChargeAttempt(pl)) {
    if (i0->b) {
      pf_set_rollOutCharging(pl, true);
      pf_set_rollOutCharge(pl, pf_rollOutCharge(pl) + 1);
      if (pf_rollOutCharge(pl) > 44) {
        pf_set_rollOutCharge(pl, 44);
      }
      if (pf_rollOutCharge(pl) >= 21) {
        if (pl->timer == 16) {
          ml_drawVfx("dashDust", pl->phys.pos.x, pl->phys.pos.y,
                     pl->phys.face);
        }
      }
    } else {
      pl->timer += 1;
      pf_set_rollOutCharging(pl, false);
      pf_set_rollOutChargeAttempt(pl, false);
      pf_set_rollOutVel(pl,
          js_max(0.5, js_min(4.1, (0.2 + (0.09 * pf_rollOutCharge(pl))))));
      pl->phys.cVel.x = pf_rollOutVel(pl) * pl->phys.face;
      ml_sound_play("rolloutlaunch");
      ml_sound_play("rollouttickair");
      if (pf_rollOutCharge(pl) >= 21) {
        pl->hitboxes.frame = 0;
        pf_assign_hitbox_id(S, p, "neutralspecialair", 0, 0);
        pl->hitboxes.active[0] = true;
        pl->hitboxes.active[1] = false;
        pl->hitboxes.active[2] = false;
        pl->hitboxes.active[3] = false;
        S->aliasHbActive[(int)p] = false; // fresh array upstream
      }
    }
  }
  if (pf_rollOutCharging(pl) || pl->phys.rollOutDistance < 100 ||
      pf_rollOutPlayerHit(pl)) {
    pl->colourOverlayBool = false;
    if (pl->timer >= 24 && pl->timer <= 28 && pf_rollOutCharge(pl) >= 21 &&
        !pf_rollOutPlayerHit(pl)) {
      pl->colourOverlayBool = true;
    }
    pl->timer += 1 + (2 * (pf_rollOutCharge(pl) / 44));
    if (pl->timer > 39) {
      pl->timer = 16;
      ml_sound_play("rollouttickair");
    }
  } else {
    pl->timer += 1;
  }
  if (pf_interrupt(S, p, in, 0) != AS_TRUE) {
    pl->phys.cVel.y -= 0.07;
    if (pl->phys.cVel.y < -1.3) {
      pl->phys.cVel.y = -1.3;
    }
    if (pl->timer > 15 && pl->timer < 39 && !pf_rollOutCharging(pl) &&
        !pf_rollOutChargeAttempt(pl)) {
      pl->phys.rollOutDistance += 1;
      if (!pf_rollOutPlayerHit(pl)) {
        const double newDmg = 12 + js_round((pf_rollOutCharge(pl) - 19) / 4);
        pf_hb_set_dmg(S, p, 0, newDmg);
        pf_hb_set_dmg(S, p, 1, newDmg);
        pf_hb_set_dmg(S, p, 2, newDmg);
        if (pf_rollOutCharge(pl) >= 21) {
          if (fmod(pl->phys.rollOutDistance, 10) == 0) {
            ml_drawVfx("dashDust", pl->phys.pos.x, pl->phys.pos.y,
                       pl->phys.face);
          }
        }
      }
      if (pl->phys.rollOutDistance > 100 && !pf_rollOutPlayerHit(pl)) {
        pl->timer = 39;
        pl->phys.cVel.x *= 0.6;
        pl->colourOverlayBool = false;
        mv_turnOffHitboxes(S, p);
      }
    }
    if (pf_rollOutPlayerHit(pl)) {
      pf_set_rollOutPlayerHitTimer(pl, pf_rollOutPlayerHitTimer(pl) + 1);
      if (pf_rollOutPlayerHitTimer(pl) > 42) {
        as_airDrift((int)MV_CS(S, p), &pl->phys.cVel.x, MV_IN(in, p));
      }
    }
  }
  return AS_UNDEF;
}

static AsTri pf_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  if (pl->timer > 70) {
    mv_FALLSPECIAL.init(S, p, in, 0);
    return AS_FALSE; // verbatim: the FALLSPECIAL arm returns false
  } else {
    return AS_FALSE;
  }
}

static AsTri pf_land(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  if (pf_rollOutPlayerHit(pl)) {
    mv_LANDINGFALLSPECIAL.init(S, p, in, 0);
  } else {
    strcpy(pl->actionState, "NEUTRALSPECIALGROUND");
    if (pf_rollOutCharge(pl) >= 21) {
      pl->hitboxes.frame = 0;
      pf_assign_hitbox_id(S, p, "neutralspecialair", 0, 0);
      pl->hitboxes.active[0] = true;
      pl->hitboxes.active[1] = false;
      pl->hitboxes.active[2] = false;
      pl->hitboxes.active[3] = false;
      S->aliasHbActive[(int)p] = false; // fresh array upstream
    }
  }
  return AS_UNDEF;
}

// special phase surfaces (physics.js:122 specialWallCollide /
// hitDetection.js:493 specialOnHit)
AsTri puff_NEUTRALSPECIALAIR_onWallCollide(MlSim *S, double p,
                                           const MlInputBuffer in[4],
                                           const MvX *ex) {
  (void)in;
  MlPlayer *pl = mv_player(S, p);
  if (ex == 0 || ex->count != 2 || ex->x[0].kind != DX_STR ||
      ex->x[0].str == 0 || ex->x[1].kind != DX_NUM) {
    mv_out_of_domain("NEUTRALSPECIALAIR.onWallCollide extras");
  }
  const char *wallFace = ex->x[0].str;
  // upstream reads activeStage.wall{R,L}[wallNum][1].x for the vfx
  // position (wallNum is a valid index by physics construction).
  const double wallNum = ex->x[1].num;
  if (!pf_rollOutCharging(pl) && !pf_rollOutChargeAttempt(pl) &&
      !pf_rollOutPlayerHit(pl)) {
    pl->phys.cVel.x *= -0.75;
    pf_set_rollOutVel(pl, pf_rollOutVel(pl) * 0.75);
    pl->timer = 16;
    pl->phys.face *= -1;
    ml_sound_play("rollouthit");
    if (strcmp(wallFace, "R") == 0) {
      ml_drawVfx_f("wallBounce",
                   S->stage.s.wallR.items[(int)wallNum].p1.x,
                   pl->phys.ECBp[3].y, 1, 1);
    } else {
      ml_drawVfx_f("wallBounce",
                   S->stage.s.wallL.items[(int)wallNum].p1.x,
                   pl->phys.ECBp[1].y, -1, 0);
    }
  }
  return AS_UNDEF;
}

AsTri puff_NEUTRALSPECIALAIR_onPlayerHit(MlSim *S, double p,
                                         const MlInputBuffer in[4],
                                         const MvX *ex) {
  (void)in; (void)ex;
  MlPlayer *pl = mv_player(S, p);
  pf_set_rollOutPlayerHit(pl, true);
  pf_set_rollOutPlayerHitTimer(pl, 0);
  pl->phys.cVel.x *= -0.13;
  pl->phys.cVel.y = 1.6;
  pl->phys.grounded = false;
  ml_sound_play("rollouthit");
  pl->colourOverlayBool = false;
  mv_turnOffHitboxes(S, p);
  return AS_UNDEF;
}

const MlMoveDef puff_NEUTRALSPECIALAIR = {
    "NEUTRALSPECIALAIR", pf_init, pf_main, pf_interrupt, pf_land};
