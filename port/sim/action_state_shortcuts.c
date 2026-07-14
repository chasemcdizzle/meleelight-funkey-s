// action_state_shortcuts.c — structure-parallel C translation of
// src/physics/actionStateShortcuts.js @ upstream pin 27af171 (M2 task 4).
// Statement order, expression shapes and branch chains are copied verbatim
// (fix_plan M2 prevention rule 6); js_* helpers give ECMAScript semantics
// (rule 1); transcendentals are the vendored fdlibm (rule 4); every
// charAttributes / intangibility read comes from the M1 generated CTAB1
// tables (ml_tables.h) by char id. Compare against the upstream source
// line-by-line — line refs in comments.
#include "action_state_shortcuts.h"

#include <math.h>
#include <string.h>

#include "../fdlibm/fdlibm.h"
#include "ml_js.h"
#include "ml_tables.h" // generated: pipeline stage `tables` (CTAB1)

// JS truthiness of a number: false for +0/-0/NaN (domain here is NaN-free,
// handled for correctness).
static inline bool js_truthy_num(double x) { return x == x && x != 0.0; }

static const ml_attributes_t *attr(int charId) {
  if (charId < 0 || charId >= ML_CHARS) {
    ml_asshort_out_of_domain("charId outside 0..4");
  }
  return &ml_attributes[charId];
}

// --- actionStates scaffolding (:612-615) --------------------------------------

AsMoveTable as_action_states[AS_CHARS];

// setupActionStates(index, val): upstream deep-copies the character's move
// table into actionStates[index]; the C table registration copies the
// (name -> def) list into the per-char slot (the value-level analogue —
// move defs are immutable registrations, deep-copy aliasing concerns are
// the move clusters' surface).
void as_setupActionStates(int charId, const AsMoveEntry *list, int count) {
  if (charId < 0 || charId >= AS_CHARS) {
    ml_asshort_out_of_domain("setupActionStates: charId outside 0..4");
  }
  if (count < 0 || count > AS_MAX_STATES) {
    ml_asshort_out_of_domain("setupActionStates: table too large");
  }
  AsMoveTable *t = &as_action_states[charId];
  t->count = count;
  for (int i = 0; i < count; i++) t->entries[i] = list[i];
}

const MlMoveDef *as_lookup(int charId, const char *state) {
  if (charId < 0 || charId >= AS_CHARS) {
    ml_asshort_out_of_domain("as_lookup: charId outside 0..4");
  }
  const AsMoveTable *t = &as_action_states[charId];
  for (int i = 0; i < t->count; i++) {
    if (strcmp(t->entries[i].name, state) == 0) return t->entries[i].def;
  }
  return 0;
}

const MlMoveDef *as_dispatch(int charId, const char *state, const char *phase) {
  ml_dispatch_note(phase, state);
  return as_lookup(charId, state);
}

// --- randomShout (:14-135) — the KO-shout seeded-RNG + sound sites -----------
void as_randomShout(double charId) {
  // upstream: switch (char) with strict number equality; case 4 has no
  // break but falls into an EMPTY default — no behavioral difference.
  if (charId == 0) {
    double shout = js_round(0.5 + ml_random() * 5.99);
    if (shout == 1) ml_sound_play("shout1");
    else if (shout == 2) ml_sound_play("shout2");
    else if (shout == 3) ml_sound_play("shout3");
    else if (shout == 4) ml_sound_play("shout4");
    else if (shout == 5) ml_sound_play("shout5");
    else if (shout == 6) ml_sound_play("shout6");
  } else if (charId == 1) {
    double shout = js_round(0.5 + ml_random() * 4.99);
    if (shout == 1) ml_sound_play("puffshout1");
    else if (shout == 2) ml_sound_play("puffshout2");
    else if (shout == 3) ml_sound_play("puffshout3");
    else if (shout == 4) ml_sound_play("puffshout4");
    else if (shout == 5) ml_sound_play("puffshout5");
  } else if (charId == 2) {
    double shout = js_round(0.5 + ml_random() * 4.99);
    if (shout == 1) ml_sound_play("foxshout1");
    else if (shout == 2) ml_sound_play("foxshout2");
    else if (shout == 3) ml_sound_play("foxshout3");
    else if (shout == 4) ml_sound_play("foxshout4");
    else if (shout == 5) ml_sound_play("foxshout5");
  } else if (charId == 3) {
    double shout = js_round(0.5 + ml_random() * 4.99);
    if (shout == 1) ml_sound_play("falcoshout1");
    else if (shout == 2) ml_sound_play("falcoshout2");
    else if (shout == 3) ml_sound_play("falcoshout3");
    else if (shout == 4) ml_sound_play("falcoshout4");
    else if (shout == 5) ml_sound_play("falcoshout5");
  } else if (charId == 4) {
    double shout = js_round(0.5 + ml_random() * 5.99);
    if (shout == 1) ml_sound_play("falconshout1");
    else if (shout == 2) ml_sound_play("falconshout2");
    else if (shout == 3) ml_sound_play("falconshout3");
    else if (shout == 4) ml_sound_play("falconshout4");
    else if (shout == 5) ml_sound_play("falconshout5");
    else if (shout == 6) ml_sound_play("falconshout6");
  }
}

// --- executeIntangibility (:137-142) ------------------------------------------
void as_executeIntangibility(const char *actionStateName, int charId,
                             double timer, double *intangibleTimer,
                             double *hurtBoxState) {
  if (charId < 0 || charId >= ML_CHARS) {
    ml_asshort_out_of_domain("charId outside 0..4");
  }
  const ml_intang_entry_t *tab = ml_intang_data[charId];
  const ml_intang_entry_t *e = 0;
  for (int i = 0; i < ml_intang_count[charId]; i++) {
    if (strcmp(tab[i].name, actionStateName) == 0) { e = &tab[i]; break; }
  }
  // upstream dereferences intangibility[char][name][0] unconditionally: an
  // unknown state name would throw — never observed; hard trap.
  if (!e) ml_asshort_out_of_domain("executeIntangibility: unknown state name");
  if (timer == (double)e->start) {
    *intangibleTimer = (double)e->length;
    *hurtBoxState = 1;
  }
}

// --- playSounds (:144-150) ----------------------------------------------------
// The actionSounds[char][state] schedule rows are SND1 data-plane
// (pipeline sounds.json); their C emission is the M4 mixer task
// (FORMATS.md section 5.4) — the schedule is a parameter until then.
void as_playSounds(const AsSoundRow *rows, int count, double timer) {
  for (int i = 0; i < count; i++) {
    if (timer == rows[i].frame) {
      ml_sound_play(rows[i].name);
    }
  }
}

// --- isFinalDeath (:152-170) ---------------------------------------------------
bool as_isFinalDeath(const AsFinalDeathState *st) {
  if (st->gameMode == 5) {
    return true;
  } else if (js_truthy_num(st->versusMode)) {
    return false;
  } else {
    double finalDeaths = 0;
    double totalPlayers = 0;
    for (int j = 0; j < 4; j++) {
      if (st->playerType[j] > -1) {
        totalPlayers++;
        if (!st->stocksPresent[j]) {
          ml_asshort_out_of_domain("isFinalDeath: active slot without stocks");
        }
        if (st->stocks[j] == 0) {
          finalDeaths++;
        }
      }
    }
    return finalDeaths >= js_max(1, totalPlayers - 1);
  }
}

// --- getAngle (:172-178) --------------------------------------------------------
double as_getAngle(double x, double y) {
  double angle = 0;
  if (x != 0 || y != 0) {
    angle = fd_atan2(y, x);
  }
  return angle;
}

// --- turnOffHitboxes (:180-183) --------------------------------------------------
void as_turnOffHitboxes(MlHitboxes *hb) {
  hb->active[0] = false;
  hb->active[1] = false;
  hb->active[2] = false;
  hb->active[3] = false;
  hb->hitListLen = 0; // hitList = []
}

// --- shieldTilt (:185-199) --------------------------------------------------------
void as_shieldTilt(bool shieldstun, int charId, AsShieldTiltState *st,
                   const MlInput in[4]) {
  const ml_attributes_t *at = attr(charId);
  if (!shieldstun && !st->inCSS) {
    double x = in[0].lsX;
    double y = in[0].lsY;
    double targetOffset = sqrt(x * x + y * y) * 3;
    double targetAngle = as_getAngle(x, y);
    Vec2D targetPosition = {fd_cos(targetAngle) * targetOffset,
                            fd_sin(targetAngle) * targetOffset};
    Vec2D sp = st->shieldPosition;
    st->shieldPosition.x = sp.x + ((targetPosition.x - sp.x) / 5 + 0.01);
    st->shieldPosition.y = sp.y + ((targetPosition.y - sp.y) / 5 + 0.01);
  }
  st->shieldPositionReal.x =
      st->pos.x + st->shieldPosition.x +
      ((double)at->shieldOffset[0] * st->face / 4.5);
  st->shieldPositionReal.y =
      st->pos.y + st->shieldPosition.y + ((double)at->shieldOffset[1] / 4.5);
}

// --- reduceByTraction (:201-222) ----------------------------------------------------
void as_reduceByTraction(bool applyDouble, int charId, double *cVelX) {
  const ml_attributes_t *at = attr(charId);
  const double traction = ml_f64(at->traction);
  const double maxWalk = ml_f64(at->maxWalk);
  if (*cVelX > 0) {
    if (applyDouble && *cVelX > maxWalk) {
      *cVelX -= traction * 2;
    } else {
      *cVelX -= traction;
    }
    if (*cVelX < 0) {
      *cVelX = 0;
    }
  } else {
    if (applyDouble && *cVelX < -maxWalk) {
      *cVelX += traction * 2;
    } else {
      *cVelX += traction;
    }
    if (*cVelX > 0) {
      *cVelX = 0;
    }
  }
}

// --- airDrift (:224-263) --------------------------------------------------------------
void as_airDrift(int charId, double *cVelX, const MlInput in[4]) {
  const ml_attributes_t *at = attr(charId);
  const double airFriction = ml_f64(at->airFriction);
  double tempMax;
  if (js_abs(in[0].lsX) < 0.3) {
    tempMax = 0;
  } else {
    tempMax = ml_f64(at->aerialHmaxV) * in[0].lsX;
  }

  if ((tempMax < 0 && *cVelX < tempMax) || (tempMax > 0 && *cVelX > tempMax)) {
    if (*cVelX > 0) {
      *cVelX -= airFriction;
      if (*cVelX < 0) {
        *cVelX = 0;
      }
    } else {
      *cVelX += airFriction;
      if (*cVelX > 0) {
        *cVelX = 0;
      }
    }
  } else if (js_abs(in[0].lsX) > 0.3 &&
             ((tempMax < 0 && *cVelX > tempMax) ||
              (tempMax > 0 && *cVelX < tempMax))) {
    *cVelX += (ml_f64(at->airMobA) * in[0].lsX) +
              (js_sign(in[0].lsX) * ml_f64(at->airMobB));
  }

  if (js_abs(in[0].lsX) < 0.3) {
    if (*cVelX > 0) {
      *cVelX -= airFriction;
      if (*cVelX < 0) {
        *cVelX = 0;
      }
    } else {
      *cVelX += airFriction;
      if (*cVelX > 0) {
        *cVelX = 0;
      }
    }
  }
}

// --- fastfall (:265-278) ----------------------------------------------------------------
void as_fastfall(int charId, double *cVelY, bool *fastfalled,
                 const MlInput in[4]) {
  const ml_attributes_t *at = attr(charId);
  if (!*fastfalled) {
    *cVelY -= ml_f64(at->gravity);
    if (*cVelY < -ml_f64(at->terminalV)) {
      *cVelY = -ml_f64(at->terminalV);
    }
    if (in[0].lsY < -0.65 && in[3].lsY > -0.1 && *cVelY < 0) {
      ml_sound_play("fastfall");
      *fastfalled = true;
      *cVelY = -ml_f64(at->fastFallV);
    }
  }
}

// --- shieldDepletion (:280-299) ---------------------------------------------------------
void as_shieldDepletion(int charId, AsShieldDepState *st, const MlInput in[4]) {
  const ml_attributes_t *at = attr(charId);
  // upstream shadows the parameter: var input = max(lA, rA)
  double input = js_max(in[0].lA, in[0].rA);
  st->shieldHP -= 0.28 * input - ((1 - input) / 10);
  if (st->shieldHP <= 0) {
    st->shielding = false;
    st->kVel.y = ml_f64(at->shieldBreakVel);
    st->kDec.y = 0.051;
    st->kDec.x = 0;
    st->grounded = false;
    st->shieldHP = 0;
    // drawVfx({name:"breakShield",...}): render-only vfx queue — no sim
    // effect (drawECB no-op precedent, fix_plan M2-CAL conventions).
    ml_sound_play("shieldbreak");
    as_dispatch(charId, "SHIELDBREAKFALL", "init");
  }
}

// --- shieldSize (:301-314) ---------------------------------------------------------------
void as_shieldSize(bool lock, int charId, double shieldHP, AsShieldSizeOut *out,
                   const MlInput in[4]) {
  const ml_attributes_t *at = attr(charId);
  const double shieldScale = ml_f64(at->shieldScale);
  out->shieldAnalog = js_max(in[0].lA, in[0].rA);
  if (out->shieldAnalog == 0) { // === 0 (matches -0 too)
    out->shieldAnalog = 1;
  }
  if (lock && out->shieldAnalog == 0) { // dead after the branch above — verbatim
    out->shieldAnalog = 1;
  }
  out->shieldSize =
      (shieldScale * 0.575 * ml_f64(at->modelScale) * (shieldHP / 60)) +
      ((1 - out->shieldAnalog) * 0.6 * shieldScale) + ((60 - shieldHP) / 60 * 2);
}

// --- mashOut (:316-344) -------------------------------------------------------------------
// Upstream's `!input[p][1].lsX < 0.7` parses as (!lsX) < 0.7: boolean
// coerced to 0/1 and compared — the shapes below keep the coercion
// verbatim (two of the twelve arms are dead by construction, two others
// reduce to their first conjunct; NOT simplified, rule 6).
bool as_mashOut(const MlInput in[4]) {
  if (in[0].a && !in[1].a) {
    return true;
  } else if (in[0].b && !in[1].b) {
    return true;
  } else if (in[0].x && !in[1].x) {
    return true;
  } else if (in[0].y && !in[1].y) {
    return true;
  } else if (in[0].lsX > 0.8 && (js_truthy_num(in[1].lsX) ? 0.0 : 1.0) < 0.7) {
    return true;
  } else if (in[0].lsX < -0.8 && (js_truthy_num(in[1].lsX) ? 0.0 : 1.0) < -0.7) {
    return true;
  } else if (in[0].lsY > 0.8 && (js_truthy_num(in[1].lsY) ? 0.0 : 1.0) < 0.7) {
    return true;
  } else if (in[0].lsY < -0.8 && (js_truthy_num(in[1].lsY) ? 0.0 : 1.0) > -0.7) {
    return true;
  } else if (in[0].csX > 0.8 && (js_truthy_num(in[1].csX) ? 0.0 : 1.0) < 0.7) {
    return true;
  } else if (in[0].csX < -0.8 && (js_truthy_num(in[1].csX) ? 0.0 : 1.0) < -0.7) {
    return true;
  } else if (in[0].csY > 0.8 && (js_truthy_num(in[1].csY) ? 0.0 : 1.0) < 0.7) {
    return true;
  } else if (in[0].csY < -0.8 && (js_truthy_num(in[1].csY) ? 0.0 : 1.0) > -0.7) {
    return true;
  } else {
    return false;
  }
}

// --- checkForSmashes (:347-369) -------------------------------------------------------------
static AsPair pair_false(void) {
  AsPair r = {false, AS_P_FALSE, 0, 0};
  return r;
}
static AsPair pair_str(const char *s) {
  AsPair r = {true, AS_P_STR, s, 0};
  return r;
}
static AsPair pair_num(bool flag, double n) {
  AsPair r = {flag, AS_P_NUM, 0, n};
  return r;
}

AsPair as_checkForSmashes(double *face, const MlInput in[4]) {
  if (in[0].a && !in[1].a) {
    if (js_abs(in[0].lsX) >= 0.79 && in[2].lsX * js_sign(in[0].lsX) < 0.3) {
      *face = js_sign(in[0].lsX);
      return pair_str("FORWARDSMASH");
    } else if (in[0].lsY >= 0.66 && in[2].lsY < 0.3) {
      return pair_str("UPSMASH");
    } else if (in[0].lsY <= -0.66 && in[2].lsY > -0.3) {
      return pair_str("DOWNSMASH");
    } else {
      return pair_false();
    }
  } else if (js_abs(in[0].csX) >= 0.79 && js_abs(in[1].csX) < 0.79) {
    *face = js_sign(in[0].csX);
    return pair_str("FORWARDSMASH");
  } else if (in[0].csY >= 0.66 && in[1].csY < 0.66) {
    return pair_str("UPSMASH");
  } else if (in[0].csY <= -0.66 && in[1].csY > -0.66) {
    return pair_str("DOWNSMASH");
  } else {
    return pair_false();
  }
}

// --- checkForTilts (:371-386) ----------------------------------------------------------------
AsPair as_checkForTilts(double face, const MlInput in[4], bool reversePresent,
                        double reverseArg) {
  // var reverse = reverse || 1 (undefined -> 1; 0 -> 1; any truthy number
  // kept — captured domain is 2-arg calls only)
  double reverse =
      (reversePresent && js_truthy_num(reverseArg)) ? reverseArg : 1;
  if (in[0].a && !in[1].a) {
    if (in[0].lsX * face * reverse > 0.3 &&
        js_abs(in[0].lsX) - (js_abs(in[0].lsY)) > -0.05) {
      return pair_str("FORWARDTILT");
    } else if (in[0].lsY < -0.3) {
      return pair_str("DOWNTILT");
    } else if (in[0].lsY > 0.3) {
      return pair_str("UPTILT");
    } else {
      return pair_str("JAB1");
    }
  } else {
    return pair_false();
  }
}

// --- checkForIASA (:388-416) --------------------------------------------------------------------
AsTri as_checkForIASA(const AsIasaState *st, double charId, double tapJumpOff,
                      const MlInput in[4], bool isAerial) {
  // absent IASATimer: `timer > undefined` is false (NaN comparison)
  if (st->hasIASATimer ? (st->timer > st->IASATimer) : false) {
    if (isAerial) {
      AsPair a = as_checkForAerials(st->face, in);
      if ((as_checkForDoubleJump(tapJumpOff, in) && (!st->doubleJumped)) ||
          (as_checkForMultiJump(tapJumpOff, in) && st->jumpsUsed < 5 &&
           attr((int)charId)->multiJump)) {
        if (in[0].lsX * st->face < -0.3) {
          ml_dispatch_note("init", "JUMPAERIALB"); // shared module dispatch
        } else {
          ml_dispatch_note("init", "JUMPAERIALF");
        }
        return AS_TRUE;
      } else if (a.flag) {
        if (charId == 0) {
          ml_dispatch_note("init", a.str); // MARTHMOVES[a[1]]
        } else if (charId == 1) {
          ml_dispatch_note("init", a.str); // PUFFMOVES[a[1]]
        } else if (charId == 2) {
          ml_dispatch_note("init", a.str); // FOXMOVES[a[1]]
        }
        // chars 3/4 dispatch nothing upstream (no branch) — verbatim
        return AS_TRUE;
      } else {
        return AS_FALSE;
      }
    } else {
      // upstream's non-aerial arm is empty
    }
  }
  return AS_UNDEF; // falls off the end -> undefined
}

// --- checkForSpecials (:418-454) ------------------------------------------------------------------
AsPair as_checkForSpecials(double *face, double bTurnaroundTimer,
                           double bTurnaroundDirection, bool grounded,
                           const MlInput in[4]) {
  if (in[0].b && !in[1].b) {
    if (grounded) {
      if (js_abs(in[0].lsX) > 0.59 ||
          (in[0].lsY > 0.54 && js_abs(in[0].lsX) > in[0].lsY - 0.2)) {
        *face = js_sign(in[0].lsX);
        return pair_str("SIDESPECIALGROUND");
      } else if (in[0].lsY > 0.54) {
        return pair_str("UPSPECIAL");
      } else if (in[0].lsY < -0.54) {
        return pair_str("DOWNSPECIALGROUND");
      } else {
        return pair_str("NEUTRALSPECIALGROUND");
      }
    } else {
      if (in[0].lsY > 0.54 ||
          (js_abs(in[0].lsX) > 0.59 && in[0].lsY > js_abs(in[0].lsX) - 0.2)) {
        return pair_str("UPSPECIAL");
      } else if (in[0].lsY < -0.54 ||
                 (js_abs(in[0].lsX) > 0.59 &&
                  -in[0].lsY > js_abs(in[0].lsX) - 0.2)) {
        return pair_str("DOWNSPECIALAIR");
      } else if (js_abs(in[0].lsX) > 0.59) {
        *face = js_sign(in[0].lsX);
        return pair_str("SIDESPECIALAIR");
      } else {
        if (in[0].lsX * *face < -0.25) {
          *face *= -1;
        } else if (bTurnaroundTimer > 0) {
          *face = bTurnaroundDirection;
        }
        return pair_str("NEUTRALSPECIALAIR");
      }
    }
  } else {
    return pair_false();
  }
}

// --- checkForAerials (:456-485) -----------------------------------------------------------------
AsPair as_checkForAerials(double face, const MlInput in[4]) {
  if (in[0].csX * face >= 0.3 && in[1].csX * face < 0.3 &&
      js_abs(in[0].csX) > js_abs(in[0].csY) - 0.1) {
    return pair_str("ATTACKAIRF");
  } else if (in[0].csX * face <= -0.3 && in[1].csX * face > -0.3 &&
             js_abs(in[0].csX) > js_abs(in[0].csY) - 0.1) {
    return pair_str("ATTACKAIRB");
  } else if (in[0].csY >= 0.3 && in[1].csY < 0.3) {
    return pair_str("ATTACKAIRU");
  } else if (in[0].csY < -0.3 && in[1].csY > -0.3) {
    return pair_str("ATTACKAIRD");
  } else if ((in[0].a && !in[1].a) || (in[0].z && !in[1].z)) {
    if (in[0].lsX * face > 0.3 && js_abs(in[0].lsX) > js_abs(in[0].lsY) - 0.1) {
      return pair_str("ATTACKAIRF");
    } else if (in[0].lsX * face < -0.3 &&
               js_abs(in[0].lsX) > js_abs(in[0].lsY) - 0.1) {
      return pair_str("ATTACKAIRB");
    } else if (in[0].lsY > 0.3) {
      return pair_str("ATTACKAIRU");
    } else if (in[0].lsY < -0.3) {
      return pair_str("ATTACKAIRD");
    } else {
      return pair_str("ATTACKAIRN");
    }
  }
  return pair_num(false, 0); // [false, 0]
}

// --- simple checks (:488-525) --------------------------------------------------------------------
bool as_checkForDash(double face, const MlInput in[4]) {
  return in[0].lsX * face > 0.79 && in[2].lsX * face < 0.3;
}

bool as_checkForSmashTurn(double face, const MlInput in[4]) {
  return in[0].lsX * face < -0.79 && in[2].lsX * face > -0.3;
}

bool as_tiltTurnDashBuffer(double face, const MlInput in[4]) {
  return in[1].lsX * face > -0.3;
}

bool as_checkForTiltTurn(double face, const MlInput in[4]) {
  return in[0].lsX * face < -0.3;
}

// gameSettings["tapJumpOffp"+(p+1)] == false — "== is on purpose": the
// captured domain is the number 0/1-class value, where JS loose equality
// to false is (v == 0). Non-number settings values are outside the
// captured domain (the marshaller rejects them).
static inline bool tap_jump_eq_false(double v) { return v == 0; }

AsPair as_checkForJump(double tapJumpOff, const MlInput in[4]) {
  if ((in[0].x && !in[1].x) || (in[0].y && !in[1].y)) {
    return pair_num(true, 0);
  } else if (tap_jump_eq_false(tapJumpOff) &&
             (in[0].lsY > 0.66 && in[3].lsY < 0.2)) {
    return pair_num(true, 1);
  } else {
    return pair_false();
  }
}

bool as_checkForDoubleJump(double tapJumpOff, const MlInput in[4]) {
  return ((in[0].x && !in[1].x) || (in[0].y && !in[1].y) ||
          (tap_jump_eq_false(tapJumpOff) &&
           (in[0].lsY > 0.69 && in[1].lsY <= 0.69)));
}

bool as_checkForMultiJump(double tapJumpOff, const MlInput in[4]) {
  return !!(in[0].x || in[0].y ||
            (tap_jump_eq_false(tapJumpOff) && in[0].lsY > 0.7));
}

bool as_checkForSquat(const MlInput in[4]) { return in[0].lsY < -0.69; }

// --- turboAirborneInterrupt (:527-555) ----------------------------------------------------------
bool as_turboAirborneInterrupt(AsTurboAirState *st, double charId,
                               double tapJumpOff, const MlInput in[4]) {
  const int c = (int)charId;
  AsPair a = as_checkForAerials(*st->face, in);
  AsPair b = as_checkForSpecials(st->face, st->bTurnaroundTimer,
                                 st->bTurnaroundDirection, st->grounded, in);
  if (a.flag && strcmp(a.str, st->actionState) != 0) {
    as_turnOffHitboxes(st->hitboxes);
    as_dispatch(c, a.str, "init");
    return true;
  } else if ((in[0].l && !in[1].l) || (in[0].r && !in[1].r)) {
    as_turnOffHitboxes(st->hitboxes);
    as_dispatch(c, "ESCAPEAIR", "init");
    return true;
  } else if (((in[0].x && !in[1].x) || (in[0].y && !in[1].y) ||
              (in[0].lsY > 0.7 && in[1].lsY <= 0.7)) &&
             (!st->doubleJumped ||
              (st->jumpsUsed < 5 && attr(c)->multiJump))) {
    as_turnOffHitboxes(st->hitboxes);
    if (in[0].lsX * *st->face < -0.3) {
      as_dispatch(c, "JUMPAERIALB", "init");
    } else {
      as_dispatch(c, "JUMPAERIALF", "init");
    }
    return true;
  } else if (b.flag && strcmp(b.str, st->actionState) != 0) {
    as_turnOffHitboxes(st->hitboxes);
    as_dispatch(c, b.str, "init");
    return true;
  } else {
    (void)tapJumpOff;
    return false;
  }
}

// --- turboGroundedInterrupt (:557-610) ----------------------------------------------------------
bool as_turboGroundedInterrupt(AsTurboGroundState *st, double charId,
                               double tapJumpOff, const MlInput in[4]) {
  const int c = (int)charId;
  // upstream evaluates b, t, s, j UP FRONT — checkForSpecials/Smashes may
  // mutate face before the later checks read it (order preserved).
  AsPair b = as_checkForSpecials(st->face, st->bTurnaroundTimer,
                                 st->bTurnaroundDirection, st->grounded, in);
  AsPair t = as_checkForTilts(*st->face, in, false, 0);
  AsPair s = as_checkForSmashes(st->face, in);
  AsPair j = as_checkForJump(tapJumpOff, in);
  if (j.flag) {
    as_turnOffHitboxes(st->hitboxes);
    as_dispatch(c, "KNEEBEND", "init"); // init(p, j[1], input)
    return true;
  } else if (in[0].l || in[0].r) {
    as_turnOffHitboxes(st->hitboxes);
    as_dispatch(c, "GUARDON", "init");
    return true;
  } else if (in[0].lA > 0 || in[0].rA > 0) {
    as_turnOffHitboxes(st->hitboxes);
    as_dispatch(c, "GUARDON", "init");
    return true;
  } else if (b.flag && strcmp(b.str, st->actionState) != 0) {
    as_turnOffHitboxes(st->hitboxes);
    as_dispatch(c, b.str, "init");
    return true;
  } else if (s.flag && strcmp(s.str, st->actionState) != 0) {
    as_turnOffHitboxes(st->hitboxes);
    as_dispatch(c, s.str, "init");
    return true;
  } else if (t.flag && strcmp(t.str, st->actionState) != 0) {
    as_turnOffHitboxes(st->hitboxes);
    as_dispatch(c, t.str, "init");
    return true;
  } else if (as_checkForSquat(in)) {
    as_turnOffHitboxes(st->hitboxes);
    as_dispatch(c, "SQUAT", "init");
    return true;
  } else if (as_checkForDash(*st->face, in)) {
    as_turnOffHitboxes(st->hitboxes);
    as_dispatch(c, "DASH", "init");
    return true;
  } else if (as_checkForSmashTurn(*st->face, in)) {
    as_turnOffHitboxes(st->hitboxes);
    as_dispatch(c, "SMASHTURN", "init");
    return true;
  } else if (as_checkForTiltTurn(*st->face, in)) {
    as_turnOffHitboxes(st->hitboxes);
    *st->dashbuffer = as_tiltTurnDashBuffer(*st->face, in);
    *st->hasDashbuffer = true;
    as_dispatch(c, "TILTTURN", "init");
    return true;
  } else if (js_abs(in[0].lsX) > 0.3) {
    as_turnOffHitboxes(st->hitboxes);
    as_dispatch(c, "WALK", "init"); // init(p, true, input)
    return true;
  } else {
    return false;
  }
}
