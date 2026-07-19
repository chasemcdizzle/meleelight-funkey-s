// NEUTRALSPECIALGROUND.c <- src/characters/marth/moves/
// NEUTRALSPECIALGROUND.js (M2 task 11 — the shield breaker, ground).
// - `player[p].shieldBreakerID = sounds.shieldbreakercharge.play()` — the
//   Howl play id is GLOBAL howler chain state: oracle-fed via
//   mv_howl_play_id (moves.h; the capture records it in post "sbid").
// - `sounds.shieldbreakercharge.stop(id)` — stop token.
// - the timer-46 dmg write MUTATES the char-data hitbox objects upstream
//   (player.js:132 aliases charHitboxes to the global chars data — no
//   copy). newDmg == the authored 7 whenever shieldBreakerCharge < 30
//   (measured live domain {0,1}); a live CHARGED punch would leave the
//   global plane mutated and a LATER nsg/nsa hitbox assign would surface
//   it as an init-record divergence (loud, honest-coverage hole; the
//   sweep restores the dmg fields per rule 12).
#include "../moves.h"

#include <math.h>

static AsTri mr_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex);
static AsTri mr_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex);

static AsTri mr_init(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "NEUTRALSPECIALGROUND");
  pl->timer = 0;
  pl->phys.hasShieldBreakerCharge = true;
  pl->phys.shieldBreakerCharge = 0;
  pl->phys.hasShieldBreakerChargeAttempt = true;
  pl->phys.shieldBreakerChargeAttempt = true;
  pl->phys.hasShieldBreakerCharging = true;
  pl->phys.shieldBreakerCharging = false;
  pl->colourOverlayBool = false;
  pl->phys.cVel.x *= 0.8;
  mv_turnOffHitboxes(S, p);
  mv_assign_hitbox_id(S, p, "neutralspecialground", 0, 0);
  mv_assign_hitbox_id(S, p, "neutralspecialground", 1, 1);
  mv_assign_hitbox_id(S, p, "neutralspecialground", 2, 2);
  mv_assign_hitbox_id(S, p, "neutralspecialground", 3, 3);
  ml_sound_play("jump");
  mr_main(S, p, in, 0);
  return AS_UNDEF;
}

static AsTri mr_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  const MlInput *i0 = &MV_IN(in, p)[0];
  pl->timer += 1;
  if (pl->timer >= 12 && pl->timer <= 41 &&
      pl->phys.shieldBreakerChargeAttempt) {
    if (i0->b) {
      pl->phys.shieldBreakerCharging = true;
      pl->phys.shieldBreakerCharge += 1;
      marth_blend_overlay(S, p); // colourOverlay + colourOverlayBool=true
      if (fmod(pl->phys.shieldBreakerCharge, 6) == 0) {
        ml_drawVfx("dashDust", pl->phys.pos.x, pl->phys.pos.y, pl->phys.face);
      }
    } else {
      pl->phys.shieldBreakerCharging = false;
      pl->phys.shieldBreakerChargeAttempt = false;
      pl->colourOverlayBool = false;
      pl->timer = 42;
      ml_sound_stop_id("shieldbreakercharge.stop",
                       pl->hasShieldBreakerID ? 1 : 0, pl->shieldBreakerID);
    }
  }
  if (pl->phys.shieldBreakerCharging) {
    if (pl->timer > 41) {
      pl->timer = 12;
    }
    if (pl->phys.shieldBreakerCharge == 122) {
      pl->timer = 42;
      pl->phys.shieldBreakerCharging = false;
      pl->phys.shieldBreakerChargeAttempt = false;
      pl->colourOverlayBool = false;
      ml_sound_stop_id("shieldbreakercharge.stop",
                       pl->hasShieldBreakerID ? 1 : 0, pl->shieldBreakerID);
    }
  }

  if (mr_interrupt(S, p, in, 0) != AS_TRUE) {
    if (pl->timer < 12) {
      const double sign = js_sign(pl->phys.cVel.x);
      pl->phys.cVel.x -= 0.02 * sign;
      if (pl->phys.cVel.x * sign < 0) {
        pl->phys.cVel.x = 0;
      }
    } else {
      as_reduceByTraction(false, (int)MV_CS(S, p), &pl->phys.cVel.x);
    }

    if (pl->timer == 7) {
      ml_sound_play("shieldbreaker1");
    } else if (pl->timer == 11) {
      ml_sound_play("shieldbreakercharge");
      pl->hasShieldBreakerID = true;
      pl->shieldBreakerID = ml_howl_play_id("shieldbreakercharge");
    } else if (pl->timer == 43) {
      ml_sound_play("shieldbreakershout");
      ml_sound_play("shieldbreaker2");
    } else if (pl->timer == 46) {
      pl->hitboxes.active[0] = true;
      pl->hitboxes.active[1] = true;
      pl->hitboxes.active[2] = true;
      pl->hitboxes.active[3] = true;
      S->aliasHbActive[(int)p] = false;
      pl->hitboxes.frame = 0;
      const double newDmg =
          7 + (5 * floor(pl->phys.shieldBreakerCharge / 30)) +
          1 * (floor(pl->phys.shieldBreakerCharge / 120));
      mv_hb_set_dmg(S, p, 0, newDmg);
      mv_hb_set_dmg(S, p, 1, newDmg);
      mv_hb_set_dmg(S, p, 2, newDmg);
      mv_hb_set_dmg(S, p, 3, newDmg);
      if (pl->phys.shieldBreakerCharge >= 120) {
        ml_sound_play("firestronghit");
      } else {
        ml_sound_play("sword3");
      }
    } else if (pl->timer > 46 && pl->timer < 52) {
      pl->hitboxes.frame += 1;
    } else if (pl->timer == 52) {
      mv_turnOffHitboxes(S, p);
    }

    if (pl->timer == 50) {
      if (pl->phys.shieldBreakerCharge >= 120) {
        ml_drawVfx("groundBounce", pl->phys.pos.x + 18 * pl->phys.face, pl->phys.pos.y, pl->phys.face);
      }
    }
  }
  return AS_UNDEF;
}

static AsTri mr_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  if (pl->timer > 74) {
    mv_WAIT.init(S, p, in, 0);
    return AS_TRUE;
  } else {
    return AS_FALSE;
  }
}

const MlMoveDef marth_NEUTRALSPECIALGROUND = {"NEUTRALSPECIALGROUND",
                                              mr_init, mr_main, mr_interrupt,
                                              0};
