// physics.c <- src/physics/physics.js (structure-parallel translation,
// M2 task 5). Expression shapes copied VERBATIM (prevention rule 6);
// js_max/js_min/js_sign for Math.max/min/sign (rule 1); fd_atan/fd_atan2/
// fd_sin/fd_cos/fd_pow for the transcendentals (rule 4); every JS number
// is a double (ml_js.h). Seams + alias sites: physics.h header note.
//
// Line references in comments are to upstream physics.js at pin 27af171.
#include "physics.h"

#include <math.h>
#include <string.h>

#include "../fdlibm/fdlibm.h"
#include "action_state_shortcuts.h" // as_turnOffHitboxes, as_turbo*Interrupt
#include "ml_events.h"              // ml_sound_play
#include "ml_tables.h"              // ml_attributes / ml_frames_data / ml_ecb_states
#include "util/ecb_transform.h"     // moveECB, squashECBAt
#include "util/extreme_point.h"
#include "util/lin_alg.h" // subtract
#include "util/to_list.h"

// main.js:181 `export const edgeOffset = [[-2.9,-23.7],[2.9,-23.7]]` —
// a main.js code literal (code plane, transcribed like any inline
// constant; NOT engine-table data).
static const double edgeOffset[2][2] = {{-2.9, -23.7}, {2.9, -23.7}};

// --- small helpers -----------------------------------------------------------

static int slot(double i) { return (int)i; }

static MlPlayer *P(MlSim *S, double i) { return &S->player[slot(i)]; }

static const ml_attributes_t *ATTR(const MlSim *S, double i) {
  const int c = (int)S->characterSelections[slot(i)];
  if (c < 0 || c >= ML_CHARS) ml_phys_out_of_domain("charId out of range");
  return &ml_attributes[c];
}

static const MlAsFlags *FLAGS(MlSim *S, double i) {
  return mlp_flags(S, S->characterSelections[slot(i)], P(S, i)->actionState);
}

static void set_state(char dst[ML_STR_CAP], const char *s) {
  if (strlen(s) >= ML_STR_CAP) ml_phys_out_of_domain("actionState too long");
  strcpy(dst, s);
}

// framesData[characterSelections[i]][state] — CTAB1 lookup. A state
// missing from the table reads as undefined upstream (`frame > undefined`
// is false -> no clamp); presence is returned explicitly.
static bool frames_data(const MlSim *S, double i, const char *state,
                        double *out) {
  const int c = (int)S->characterSelections[slot(i)];
  const ml_frames_entry_t *tab = ml_frames_data[c];
  for (int k = 0; k < ml_frames_count[c]; k++) {
    if (strcmp(tab[k].name, state) == 0) {
      *out = (double)tab[k].frames;
      return true;
    }
  }
  return false;
}

// ecb[characterSelections[i]][state] — CTAB1 lookup. Upstream would throw
// on a missing state / out-of-range frame (undefined[0]): domain trap.
static const ml_ecb_state_t *ecb_state(const MlSim *S, double i,
                                       const char *state) {
  const int c = (int)S->characterSelections[slot(i)];
  const ml_ecb_state_t *tab = ml_ecb_states[c];
  for (int k = 0; k < ml_ecb_state_count[c]; k++) {
    if (strcmp(tab[k].name, state) == 0) return &tab[k];
  }
  ml_phys_out_of_domain("ecb: unknown action state");
  return 0;
}

// ECB (envcoll value type) <-> the player model's Vec2D[4]
static ECB ecb_of(const Vec2D v[4]) {
  ECB e;
  e.pt[0] = v[0]; e.pt[1] = v[1]; e.pt[2] = v[2]; e.pt[3] = v[3];
  return e;
}
// whole-array ECB assignment: fresh real Vec2Ds — clears the rule-8
// undef-at-rest component mask (ml_player.h).
static void ecb_store(Vec2D v[4], uint8_t *undefMask, ECB e) {
  v[0] = e.pt[0]; v[1] = e.pt[1]; v[2] = e.pt[2]; v[3] = e.pt[3];
  *undefMask = 0;
}

// --- pos write helpers (alias site 2, physics.h header note) ---------------
// Component writes to phys.pos write through to phys.ECB1[0] while the
// land()-established alias holds (JS: they are the same object).

static void pos_set_x(MlSim *S, double i, double v) {
  MlPlayer *p = P(S, i);
  p->phys.pos.x = v;
  if (S->aliasPosEcb1[slot(i)]) {
    p->phys.ECB1[0].x = v;
    p->phys.ecb1Undef &= (uint8_t)~1u; // component now a number (rule 8)
  }
}
static void pos_set_y(MlSim *S, double i, double v) {
  MlPlayer *p = P(S, i);
  p->phys.pos.y = v;
  if (S->aliasPosEcb1[slot(i)]) {
    p->phys.ECB1[0].y = v;
    p->phys.ecb1Undef &= (uint8_t)~2u;
  }
}
// whole-object reassignment: `phys.pos = <fresh Vec2D>` breaks the alias.
static void pos_reassign(MlSim *S, double i, Vec2D v) {
  P(S, i)->phys.pos = v;
  S->aliasPosEcb1[slot(i)] = false;
}

// --- getSurfaceFromStage <- src/stages/stage.js:41 --------------------------
static Surface getSurfaceFromStage(const MlSim *S, char surfaceType,
                                   double surfaceIndex) {
  const int idx = (int)surfaceIndex;
  const SurfaceList *list;
  switch (surfaceType) {
    case 'l': list = &S->stage.s.wallL; break;
    case 'r': list = &S->stage.s.wallR; break;
    case 'p': list = &S->stage.s.platform; break;
    case 'c': list = &S->stage.s.ceiling; break;
    case 'g':
    default: list = &S->stage.s.ground; break;
  }
  if (idx < 0 || idx >= list->count) {
    ml_phys_out_of_domain("getSurfaceFromStage: index out of range");
  }
  return list->items[idx];
}

// wall[2] === undefined ? null : wall[2].damageType — the recurring
// surface-props damageType read (null | undefined | string).
static DamageType surface_damage(const Surface *s) {
  if (!s->hasProps) return damage_null();
  if (!s->propsHasDamageTypeKey) return damage_undef();
  return s->propsDamageType;
}
static bool damage_is_real(DamageType d) {
  // `damageType !== undefined && damageType !== null`
  return d.tag == DT_STR;
}

// --- dispatch plumbing -------------------------------------------------------

static void dispatch0(MlSim *S, const char *phase, const char *state,
                      double charId, double slotArg) {
  MlDispCall c;
  c.phase = phase; c.state = state; c.charId = charId; c.slot = slotArg;
  c.extraCount = 0;
  mlp_dispatch(S, &c);
}
static void dispatch_vec(MlSim *S, const char *phase, const char *state,
                         double charId, double slotArg, Vec2D v) {
  MlDispCall c;
  c.phase = phase; c.state = state; c.charId = charId; c.slot = slotArg;
  c.extraCount = 1;
  c.extras[0].kind = DX_VEC; c.extras[0].vec = v;
  mlp_dispatch(S, &c);
}
static void dispatch_bool(MlSim *S, const char *phase, const char *state,
                          double charId, double slotArg, bool b) {
  MlDispCall c;
  c.phase = phase; c.state = state; c.charId = charId; c.slot = slotArg;
  c.extraCount = 1;
  c.extras[0].kind = DX_BOOL; c.extras[0].b = b;
  mlp_dispatch(S, &c);
}

// convenience: dispatch on slot i's own char
static void dsp(MlSim *S, const char *phase, const char *state, double i) {
  dispatch0(S, phase, state, S->characterSelections[slot(i)], i);
}

// --- updatePosition (physics.js:49) -----------------------------------------
static void updatePosition(MlSim *S, double i, Vec2D newPosition) {
  // `player[i].phys.pos = newPosition` — fresh object from the collision
  // routine result / a fresh Vec2D: breaks the pos-ECB1[0] alias.
  pos_reassign(S, i, newPosition);
}

// --- dealWithDamagingStageCollision (physics.js:53) --------------------------
static void dealWithDamagingStageCollision(MlSim *S, double i, Vec2D normal,
                                           bool corner, double angular,
                                           DamageType damageType) {
  // ZERO-LIVE on the six VS stages (no surface carries damageType —
  // grep-verified over stages/vs-stages/); reachable future domain = M4
  // target stages (fix_plan rule 11 note: unsweepable without full sim
  // state injection — documented honest coverage debt).
  double damageTypeIndex = -1;
  if (damageType.tag == DT_STR) {
    if (strcmp(damageType.str, "fire") == 0) damageTypeIndex = 3;
    else if (strcmp(damageType.str, "electric") == 0) damageTypeIndex = 4;
    else if (strcmp(damageType.str, "slash") == 0) damageTypeIndex = 1;
    else if (strcmp(damageType.str, "darkness") == 0) damageTypeIndex = 5;
    // default: break;
  }
  if (damageTypeIndex != -1) {
    // hitQueue.push([i, {normal, angular, corner}, damageTypeIndex,
    //                false, false, true])
    if (S->hqCount >= ML_HQ_CAP) ml_phys_out_of_domain("hq overflow");
    MlHqRow *r = &S->hq[S->hqCount++];
    r->i = i;
    r->normal = normal;
    r->angular = angular;
    r->corner = corner;
    r->damageTypeIndex = damageTypeIndex;
  }
}

// --- land (physics.js:394, exported) -----------------------------------------
void ml_land(MlSim *S, double i, Vec2D newPosition, bool posIsNewEcbBottom,
             double t, double j, bool normalPresent, Vec2D normal,
             const MlInput in[4]) {
  MlPlayer *p = P(S, i);
  // `phys.pos = newPosition`: when the caller passed newECB[0] the alias
  // to ECB1[0] goes live at physics.js:847 (ECB1 = newECB); until then no
  // pos component write occurs, so the deferred transient flag is exact.
  p->phys.pos = newPosition;
  S->aliasPosEcb1[slot(i)] = false; // reassignment breaks any old alias
  if (posIsNewEcbBottom) S->landEcbBottom = true;
  p->phys.grounded = true;
  p->phys.doubleJumped = false;
  p->phys.jumpsUsed = 0;
  p->phys.airborneTimer = 0;
  p->phys.fastfalled = false;
  p->phys.chargeFrames = 0;
  p->phys.charging = false;
  p->phys.wallJumpCount = 0;
  p->phys.thrownHitbox = false;
  p->phys.sideBJumpFlag = true;
  p->phys.onSurface[0] = t;
  p->phys.onSurface[1] = j;
  p->phys.onLedge = -1;
  p->rotation = 0;
  p->rotationPoint = vec2d(0, 0);
  p->colourOverlayBool = false;
  // `hitboxes.active = [false,false,false,false]` — FRESH array: breaks
  // the prevFrameHitboxes.active alias (alias site 1).
  p->hitboxes.active[0] = false;
  p->hitboxes.active[1] = false;
  p->hitboxes.active[2] = false;
  p->hitboxes.active[3] = false;
  S->aliasHbActive[slot(i)] = false;

  // let newNormal = normal; null/undefined/zero-vector -> (0,1)
  bool nnPresent = normalPresent;
  Vec2D nn = normal;
  if (!nnPresent || (nn.x == 0 && nn.y == 0)) {
    nn = vec2d(0, 1);
  }
  p->phys.groundAngle = fd_atan2(nn.y, nn.x);

  const MlAsFlags *f = FLAGS(S, i);
  if (!f->hasLandType) {
    // switch (undefined) -> default arm
    dsp(S, "init", "LANDING", i);
  } else if (f->landType == 0) {
    // LANDING / NIL
    if (p->phys.cVel.y >= -1) {
      dsp(S, "init", "WAIT", i);
    } else {
      dsp(S, "init", "LANDING", i);
    }
  } else if (f->landType == 1) {
    // OWN FUNCTION
    dsp(S, "land", p->actionState, i);
  } else if (f->landType == 2) {
    // KNOCKDOWN / TECH
    if (p->phys.techTimer > 0) {
      if (in[0].lsX * p->phys.face > 0.5) {
        dsp(S, "init", "TECHF", i);
      } else if (in[0].lsX * p->phys.face < -0.5) {
        dsp(S, "init", "TECHB", i);
      } else {
        dsp(S, "init", "TECHN", i);
      }
    } else {
      dsp(S, "init", "DOWNBOUND", i);
    }
  } else {
    dsp(S, "init", "LANDING", i);
  }
  p = P(S, i); // (pointer is stable; kept for structure clarity)
  p->phys.cVel.y = 0;
  p->phys.kVel.y = 0;
  p->hit.hitstun = 0;
}

// --- dealWithWallCollision (physics.js:77) -----------------------------------
static void dealWithWallCollision(MlSim *S, double i, Vec2D newPosition,
                                  double pt, const char *wallType,
                                  double wallIndex, const MlInput in[4]) {
  updatePosition(S, i, newPosition);
  MlPlayer *p = P(S, i);

  const char *wallLabel = "L";
  double sign = -1;
  double isRight = 0;
  if (wallType[0] == 'r') { // wallType[0].toLowerCase() === "r"
    wallLabel = "R";
    sign = 1;
    isRight = 1;
  }
  (void)isRight; // upstream computes-but-never-reads isRight

  const Surface wall = getSurfaceFromStage(S, wallType[0], wallIndex);
  const Vec2D wallBottom = extremePoint(surface_geom(&wall), 'b');
  const Vec2D wallTop = extremePoint(surface_geom(&wall), 't');
  const Vec2D wallNormal = outwardsWallNormal(wallBottom, wallTop, wallType[0]);
  const DamageType damageType = surface_damage(&wall);

  const bool inDamageState =
      strcmp(p->actionState, "DAMAGEFLYN") == 0 ||
      strcmp(p->actionState, "WALLDAMAGE") == 0 ||
      strcmp(p->actionState, "DAMAGEFALL") == 0;

  if (inDamageState && p->phys.techTimer > 0) {
    p->phys.face = sign;
    if (in[0].x || in[0].y || in[0].lsY > 0.7) {
      dsp(S, "init", "WALLTECHJUMP", i);
    } else {
      dsp(S, "init", "WALLTECH", i);
    }
  } else if (inDamageState &&
             /* Math.sign(phys.kVel) — kVel is a Vec2D OBJECT upstream:
                Math.sign(object) is NaN, and `NaN !== sign` is ALWAYS
                true. Upstream bug carried verbatim (faithfulness >
                plausibility). */
             true &&
             p->hit.hitlag == 0 &&
             fd_pow(p->phys.kVel.x, 2) + fd_pow(p->phys.kVel.y, 2) >= 2.25) {
    p->phys.face = sign;
    // drawVfx({name:"wallBounce", pos:new Vec2D(pos.x, ECBp[1].y),
    //          face:sign, f:wallNormal}) (physics.js:107-112; M4 task 1)
    ml_drawVfx_fv("wallBounce", p->phys.pos.x, p->phys.ECBp[1].y, sign,
                  wallNormal.x, wallNormal.y);
    dispatch_vec(S, "init", "WALLDAMAGE", S->characterSelections[slot(i)], i,
                 wallNormal);
  } else if (p->hit.hitlag == 0) {
    const MlAsFlags *f = FLAGS(S, i);
    if (damage_is_real(damageType) && p->phys.hurtBoxState == 0) {
      // apply damage
      dealWithDamagingStageCollision(S, i, wallNormal, false, pt, damageType);
    } else if (f->specialWallCollide) {
      // actionStates[..][state].onWallCollide(i, input, wallLabel, wallIndex)
      MlDispCall c;
      c.phase = "onWallCollide";
      c.state = p->actionState;
      c.charId = S->characterSelections[slot(i)];
      c.slot = i;
      c.extraCount = 2;
      c.extras[0].kind = DX_STR; c.extras[0].str = wallLabel;
      c.extras[1].kind = DX_NUM; c.extras[1].num = wallIndex;
      mlp_dispatch(S, &c);
    } else if (!p->phys.canWallJump.isUndef && p->phys.canWallJump.v) {
      if (p->phys.wallJumpTimer == 254) {
        if (p->phys.posDelta.x >= 0.5) {
          p->phys.wallJumpTimer = 0;
        }
      }
    }
    if (p->phys.wallJumpTimer >= 0 && p->phys.wallJumpTimer < 120) {
      if (sign * in[0].lsX >= 0.7 && sign * in[3].lsX <= 0 &&
          ATTR(S, i)->walljump) {
        p->phys.wallJumpTimer = 254;
        p->phys.face = sign;
        dsp(S, "init", "WALLJUMP", i);
      } else {
        p->phys.wallJumpTimer++;
      }
    }
  }
}

// --- dealWithPlatformCollision (physics.js:146) --------------------------------
static void dealWithPlatformCollision(MlSim *S, double i, bool alreadyGrounded,
                                      Vec2D newPosition, Vec2D ecbpBottom,
                                      double platformIndex,
                                      const MlInput in[4]) {
  const Surface platform = getSurfaceFromStage(S, 'p', platformIndex);
  // const damageType = ... — computed upstream but never consumed here.

  const Vec2D platLeft = extremePoint(surface_geom(&platform), 'l');
  const Vec2D platRight = extremePoint(surface_geom(&platform), 'r');
  const Vec2D platNormal = outwardsWallNormal(platLeft, platRight, 'g');

  MlPlayer *p = P(S, i);
  if (p->hit.hitlag > 0 || alreadyGrounded || p->phys.grabbedBy != -1) {
    updatePosition(S, i, newPosition);
  } else {
    ml_land(S, i, ecbpBottom, true, 1, platformIndex, true, platNormal, in);
  }
}

// --- dealWithGroundCollision (physics.js:164) ----------------------------------
static void dealWithGroundCollision(MlSim *S, double i, bool alreadyGrounded,
                                    Vec2D newPosition, Vec2D ecbpBottom,
                                    double groundIndex, const MlInput in[4]) {
  const Surface ground = getSurfaceFromStage(S, 'g', groundIndex);
  const DamageType damageType = surface_damage(&ground);

  MlPlayer *p = P(S, i);
  const bool ignoreDamage =
      strcmp(p->actionState, "DAMAGEFLYN") == 0 ||
      strcmp(p->actionState, "DAMAGEFALL") == 0 ||
      strcmp(p->actionState, "WALLDAMAGE") == 0;
  const Vec2D groundLeft = extremePoint(surface_geom(&ground), 'l');
  const Vec2D groundRight = extremePoint(surface_geom(&ground), 'r');
  const Vec2D groundNormal = outwardsWallNormal(groundLeft, groundRight, 'g');

  if (!ignoreDamage && damage_is_real(damageType) &&
      p->phys.hurtBoxState == 0) {
    // apply damage
    dealWithDamagingStageCollision(S, i, groundNormal, false, 0, damageType);
  } else {
    if (p->hit.hitlag > 0 || alreadyGrounded || p->phys.grabbedBy != -1) {
      updatePosition(S, i, newPosition);
    } else {
      ml_land(S, i, ecbpBottom, true, 0, groundIndex, true, groundNormal, in);
    }
  }
}

// --- fallOffGround (physics.js:190) --------------------------------------------
typedef struct { bool stillGrounded, backward; } GroundResult;

static GroundResult fallOffGround(MlSim *S, double i, char side,
                                  Vec2D groundEdgePosition, bool disableFall,
                                  const MlInput in[4]) {
  MlPlayer *p = P(S, i);
  GroundResult r;
  r.stillGrounded = true;
  r.backward = false;
  double sign = 1;
  if (side == 'r') {
    sign = -1;
  }
  if (disableFall) {
    pos_set_y(S, i, js_max(p->phys.pos.y, groundEdgePosition.y) +
                        ML_ADDITIONAL_OFFSET);
    pos_set_x(S, i, groundEdgePosition.x + (side == 'l' ? ML_ADDITIONAL_OFFSET
                                                        : -ML_ADDITIONAL_OFFSET));
    ECB ecbp = ecb_of(p->phys.ECBp);
    ecb_store(p->phys.ECBp, &p->phys.ecbpUndef,
              moveECB(ecbp, ml_subtract(p->phys.pos, p->phys.ECBp[0])));
  } else if (FLAGS(S, i)->canEdgeCancel) {
    if (p->phys.face == sign) {
      r.stillGrounded = false;
      pos_set_y(S, i, js_max(p->phys.pos.y, groundEdgePosition.y) +
                          ML_ADDITIONAL_OFFSET);
      r.backward = true;
    } else if (js_abs(in[0].lsX) > 0.6 ||
               (p->phys.cVel.x == 0 && p->phys.kVel.x == 0) ||
               FLAGS(S, i)->disableTeeter || p->phys.shielding) {
      r.stillGrounded = false;
      pos_set_y(S, i, js_max(p->phys.pos.y, groundEdgePosition.y) +
                          ML_ADDITIONAL_OFFSET);
    } else {
      p->phys.cVel.x = 0;
      pos_set_x(S, i, groundEdgePosition.x + sign * ML_ADDITIONAL_OFFSET);
      dsp(S, "init", "OTTOTTO", i);
    }
  } else if (p->phys.cVel.x == 0 && p->phys.kVel.x == 0 &&
             !FLAGS(S, i)->inGrab) {
    r.stillGrounded = false;
    pos_set_y(S, i, js_max(p->phys.pos.y, groundEdgePosition.y) +
                        ML_ADDITIONAL_OFFSET);
  } else {
    p->phys.cVel.x = 0;
    pos_set_x(S, i, groundEdgePosition.x + sign * ML_ADDITIONAL_OFFSET);
  }
  return r;
}

// --- dealWithGround (physics.js:236) — recursive over connected surfaces ------
static const Surface *surf_at(const SurfaceList *list, double idx,
                              const char *what) {
  if ((int)idx < 0 || (int)idx >= list->count) ml_phys_out_of_domain(what);
  return &list->items[(int)idx];
}

static GroundResult dealWithGround(MlSim *S, double i, Surface ground,
                                   char gtType, double gtIndex,
                                   const MlInput in[4]) {
  MlPlayer *p = P(S, i);
  const DamageType damageType = surface_damage(&ground);

  const bool ignoreDamage =
      strcmp(p->actionState, "DAMAGEFLYN") == 0 ||
      strcmp(p->actionState, "DAMAGEFALL") == 0 ||
      strcmp(p->actionState, "WALLDAMAGE") == 0;

  const Vec2D leftmostGroundPoint = extremePoint(surface_geom(&ground), 'l');
  const Vec2D rightmostGroundPoint = extremePoint(surface_geom(&ground), 'r');
  const Vec2D groundNormal =
      outwardsWallNormal(leftmostGroundPoint, rightmostGroundPoint, 'g');
  GroundResult r;
  r.stillGrounded = true;
  r.backward = false;
  double groundOrPlatform = 0;
  if (gtType == 'p') {
    groundOrPlatform = 1;
  }
  bool disableFall = false;

  // maybeLeft/RightGroundTypeAndIndex: null | [type, index]
  bool maybeLeftPresent = false;
  char maybeLeftType = 0;
  double maybeLeftIndex = 0;
  bool maybeRightPresent = false;
  char maybeRightType = 0;
  double maybeRightIndex = 0;

  // first check the player may move along the ground (no low ceilings)
  const double ecb0Height =
      js_max(ML_ADDITIONAL_OFFSET, p->phys.ECB1[2].y - p->phys.ECB1[0].y -
                                       ML_ADDITIONAL_OFFSET);
  const MaybeNum maybeNextPosX =
      moveAlongGround(p->phys.ECB1[0], p->phys.ECBp[0], ecb0Height, ground,
                      &S->stage.s.ceiling);
  if (maybeNextPosX.present) {
    // ceiling has obstructed grounded movement
    pos_set_x(S, i, maybeNextPosX.v);
    ECB ecbp = ecb_of(p->phys.ECBp);
    ecb_store(p->phys.ECBp, &p->phys.ecbpUndef,
              moveECB(ecbp, vec2d(maybeNextPosX.v - p->phys.ECBp[0].x, 0)));
  }
  if (p->phys.ECBp[0].x < leftmostGroundPoint.x) {
    if (S->stage.hasConnected) { // connected !== null && !== undefined
      const int ci = (int)gtIndex;
      const int cn = gtType == 'g' ? S->stage.connGroundCount
                                   : S->stage.connPlatformCount;
      if (ci < 0 || ci >= cn) ml_phys_out_of_domain("connected index");
      const MlConnPair *pair = gtType == 'g' ? &S->stage.connGround[ci]
                                             : &S->stage.connPlatform[ci];
      maybeLeftPresent = pair->l.present;
      maybeLeftType = pair->l.type;
      maybeLeftIndex = pair->l.index;
    }
    if (!maybeLeftPresent) { // no other ground to the left
      r = fallOffGround(S, i, 'l', leftmostGroundPoint, disableFall, in);
    } else {
      switch (maybeLeftType) {
        case 'g':
          r = dealWithGround(
              S, i, *surf_at(&S->stage.s.ground, maybeLeftIndex, "conn g idx"),
              'g', maybeLeftIndex, in);
          break;
        case 'p':
          r = dealWithGround(S, i,
                             *surf_at(&S->stage.s.platform, maybeLeftIndex,
                                      "conn p idx"),
                             'p', maybeLeftIndex, in);
          break;
        case 'r': {
          const Surface rightWallToTheLeft =
              *surf_at(&S->stage.s.wallR, maybeLeftIndex, "conn r idx");
          if (extremePoint(surface_geom(&rightWallToTheLeft), 'l').y >
              leftmostGroundPoint.y) {
            disableFall = true;
          }
          r = fallOffGround(S, i, 'l', leftmostGroundPoint, disableFall, in);
          break;
        }
        default: // neither ground, platform or right wall
          r = fallOffGround(S, i, 'l', leftmostGroundPoint, disableFall, in);
          break;
      }
    }
  } else if (p->phys.ECBp[0].x > rightmostGroundPoint.x) {
    if (S->stage.hasConnected) {
      const int ci = (int)gtIndex;
      const int cn = gtType == 'g' ? S->stage.connGroundCount
                                   : S->stage.connPlatformCount;
      if (ci < 0 || ci >= cn) ml_phys_out_of_domain("connected index");
      const MlConnPair *pair = gtType == 'g' ? &S->stage.connGround[ci]
                                             : &S->stage.connPlatform[ci];
      maybeRightPresent = pair->r.present;
      maybeRightType = pair->r.type;
      maybeRightIndex = pair->r.index;
    }
    if (!maybeRightPresent) { // no other ground to the right
      r = fallOffGround(S, i, 'r', rightmostGroundPoint, disableFall, in);
    } else {
      switch (maybeRightType) {
        case 'g':
          r = dealWithGround(
              S, i, *surf_at(&S->stage.s.ground, maybeRightIndex, "conn g idx"),
              'g', maybeRightIndex, in);
          break;
        case 'p':
          r = dealWithGround(S, i,
                             *surf_at(&S->stage.s.platform, maybeRightIndex,
                                      "conn p idx"),
                             'p', maybeRightIndex, in);
          break;
        case 'l': {
          const Surface leftWallToTheRight =
              *surf_at(&S->stage.s.wallL, maybeRightIndex, "conn l idx");
          if (extremePoint(surface_geom(&leftWallToTheRight), 'r').y >
              rightmostGroundPoint.y) {
            disableFall = true;
          }
          r = fallOffGround(S, i, 'r', rightmostGroundPoint, disableFall, in);
          break;
        }
        default: // neither ground, platform or left wall
          r = fallOffGround(S, i, 'r', rightmostGroundPoint, disableFall, in);
          break;
      }
    }
  } else {
    const Vec2D ecbpBottom = p->phys.ECBp[0];
    const Vec2D yIntercept = coordinateIntercept(
        line2(ecbpBottom, vec2d(ecbpBottom.x, ecbpBottom.y + 1)),
        line2(ground.p0, ground.p1));
    pos_set_y(S, i, p->phys.pos.y + yIntercept.y - ecbpBottom.y +
                        ML_ADDITIONAL_OFFSET);
    ECB ecbp = ecb_of(p->phys.ECBp);
    ecb_store(p->phys.ECBp, &p->phys.ecbpUndef,
              moveECB(ecbp, vec2d(0, yIntercept.y - ecbpBottom.y +
                                         ML_ADDITIONAL_OFFSET)));
    p->phys.onSurface[0] = groundOrPlatform;
    p->phys.onSurface[1] = gtIndex;
    // Math.atan2(...) || Math.PI / 2 — JS `||`: 0/-0/NaN fall through
    const double ga = fd_atan2(groundNormal.y, groundNormal.x);
    p->phys.groundAngle = (ga == 0 || isnan(ga)) ? js_pi() / 2 : ga;
  }
  if (!ignoreDamage && damage_is_real(damageType) &&
      p->phys.hurtBoxState == 0) {
    // apply damage
    dealWithDamagingStageCollision(S, i, groundNormal, false, 0, damageType);
    r.stillGrounded = false;
  }
  return r;
}

// --- dealWithCeilingCollision (physics.js:343) ---------------------------------
static void dealWithCeilingCollision(MlSim *S, double i, Vec2D newPosition,
                                     Vec2D ecbTop, double ceilingIndex,
                                     const MlInput in[4]) {
  (void)in; // input flows only into the move-init dispatches upstream
  updatePosition(S, i, newPosition);
  MlPlayer *p = P(S, i);
  const Surface ceiling = getSurfaceFromStage(S, 'c', ceilingIndex);
  const DamageType damageType = surface_damage(&ceiling);
  const Vec2D ceilingLeft = extremePoint(surface_geom(&ceiling), 'l');
  const Vec2D ceilingRight = extremePoint(surface_geom(&ceiling), 'r');
  const Vec2D ceilingNormal =
      outwardsWallNormal(ceilingLeft, ceilingRight, 'c');

  const bool ignoreDamage =
      strcmp(p->actionState, "DAMAGEFLYN") == 0 ||
      strcmp(p->actionState, "DAMAGEFALL") == 0 ||
      strcmp(p->actionState, "WALLDAMAGE") == 0;

  if (!ignoreDamage && damage_is_real(damageType) &&
      p->phys.hurtBoxState == 0) {
    // apply damage
    dealWithDamagingStageCollision(S, i, ceilingNormal, false, 2, damageType);
  } else if (FLAGS(S, i)->headBonk &&
             p->phys.cVel.y + p->phys.kVel.y > 0) {
    if (p->hit.hitstun > 0) {
      if (p->phys.techTimer > 0) {
        dsp(S, "init", "TECHU", i);
      } else {
        // drawVfx({name:"ceilingBounce", pos:ecbTop, face:1,
        //          f:ceilingNormal}) (physics.js:366-371; M4 task 1)
        ml_drawVfx_fv("ceilingBounce", ecbTop.x, ecbTop.y, 1,
                      ceilingNormal.x, ceilingNormal.y);
        ml_sound_play("bounce");
        dispatch_vec(S, "init", "STOPCEIL", S->characterSelections[slot(i)],
                     i, ceilingNormal);
      }
    } else {
      dsp(S, "init", "STOPCEIL", i);
    }
  }
}

// --- dealWithCornerCollision (physics.js:381) ----------------------------------
static void dealWithCornerCollision(MlSim *S, double i, Vec2D newPosition,
                                    ECB ecb, double angularParameter,
                                    DamageType damageType) {
  updatePosition(S, i, newPosition);
  MlPlayer *p = P(S, i);
  const char insideECBType = angularParameter < 2 ? 'l' : 'r';
  const SameOther so = getSameAndOther(angularParameter);
  const Vec2D lowerECBPoint = so.other == 2 ? ecb.pt[(int)so.same] : ecb.pt[0];
  const Vec2D upperECBPoint = so.other == 2 ? ecb.pt[2] : ecb.pt[(int)so.same];
  const Vec2D normal =
      outwardsWallNormal(lowerECBPoint, upperECBPoint, insideECBType);
  if (p->hit.hitlag == 0 && damage_is_real(damageType) &&
      p->phys.hurtBoxState == 0) {
    dealWithDamagingStageCollision(S, i, normal, true, angularParameter,
                                   damageType);
  }
}

// --- hitlagSwitchUpdate (physics.js:458) ---------------------------------------
static void hitlagSwitchUpdate(MlSim *S, double i, const MlInput in[4]) {
  MlPlayer *p = P(S, i);
  if (p->hit.hitlag > 0) {
    p->hit.hitlag--;
    if (p->hit.hitlag == 0 && p->hit.knockback > 0) {
      if (p->phys.grabbedBy == -1 || p->hit.knockback > 50) {
        const double newAngle = mlp_hd_getLaunchAngle(
            S, p->hit.angle, p->hit.knockback, p->hit.hasReverse,
            p->hit.reverse, in[0].lsX, in[0].lsY, i);

        p->phys.cVel.x = 0;
        p->phys.cVel.y = 0;
        p->phys.kVel.x =
            mlp_hd_getHorizontalVelocity(S, p->hit.knockback, newAngle);
        p->phys.kVel.y = mlp_hd_getVerticalVelocity(
            S, p->hit.knockback, newAngle, p->phys.grounded, p->hit.angle);
        p->phys.kDec.x = mlp_hd_getHorizontalDecay(S, newAngle);
        p->phys.kDec.y = mlp_hd_getVerticalDecay(S, newAngle);

        p->phys.onLedge = -1;
        p->phys.charging = false;
        p->phys.chargeFrames = 0;
        p->phys.shielding = false;
        if (p->phys.kVel.y == 0) {
          if (p->hit.knockback >= 80) {
            p->phys.grounded = false;
            pos_set_y(S, i, p->phys.pos.y + 0.0001);
          }
        }
        if (p->phys.kVel.y > 0) {
          p->phys.grounded = false;
        }
      }
      p->hit.knockback = 0;
    }

    // SDI / ASDI
    if (strcmp(p->actionState, "DAMAGEN2") == 0 ||
        strcmp(p->actionState, "DAMAGEFLYN") == 0 ||
        strcmp(p->actionState, "GUARDON") == 0 ||
        strcmp(p->actionState, "GUARD") == 0 ||
        strcmp(p->actionState, "DOWNDAMAGE") == 0) {
      if (p->hit.hitlag > 0) {
        if ((in[0].lsX > 0.7 && in[1].lsX < 0.7) ||
            (in[0].lsX < -0.7 && in[1].lsX > -0.7) ||
            (in[0].lsY > 0.7 && in[1].lsY < 0.7) ||
            (in[0].lsY < -0.7 && in[1].lsY > -0.7)) {
          if (!((in[0].lsX * in[0].lsX) + (in[0].lsY * in[0].lsY) < (0.49))) {
            pos_set_x(S, i, p->phys.pos.x + in[0].lsX * 6);
            pos_set_y(S, i, p->phys.pos.y +
                                (p->phys.grounded ? 0 : in[0].lsY * 6));
          }
        }
      } else {
        pos_set_x(S, i, p->phys.pos.x + in[0].lsX * 3);
        pos_set_y(S, i,
                  p->phys.pos.y + (p->phys.grounded ? 0 : in[0].lsY * 3));
      }
    }
    if (p->hit.hitlag == 0) {
      // if hitlag just ended, do normal stuff as well
      hitlagSwitchUpdate(S, i, in);
    }
  } else {
    if (p->hit.shieldstun > 0) {
      p->hit.shieldstun--;
      if (p->hit.shieldstun < 0) {
        p->hit.shieldstun = 0;
      }
    }
    // canWallJump = actionStates[..][state].wallJumpAble — the VALUE is
    // assigned verbatim (undefined stays undefined: JsBool, rule 8).
    p->phys.canWallJump = FLAGS(S, i)->wallJumpAble;
    p->phys.bTurnaroundTimer--;
    if (p->phys.bTurnaroundTimer < 0) {
      p->phys.bTurnaroundTimer = 0;
    }

    if ((in[0].lsX > 0.9 && in[1].lsX < 0.9) ||
        (in[0].lsX < -0.9 && in[1].lsX > -0.9)) {
      p->phys.bTurnaroundTimer = 20;
      p->phys.bTurnaroundDirection = js_sign(in[0].lsX);
    }

    set_state(p->prevActionState, p->actionState);
    dsp(S, "main", p->actionState, i);

    if (p->shocked > 0) {
      p->shocked--;
      if (fmod(p->shocked, 5) == 0) {
        ml_sound_play("electricfizz");
      }
      // drawVfx({name:"shocked", pos:new Vec2D(pos.x, pos.y+5),
      //          face:phys.face}) (physics.js:572-576; M4 task 1)
      ml_drawVfx("shocked", p->phys.pos.x, p->phys.pos.y + 5, p->phys.face);
    }

    if (p->burning > 0) {
      p->burning--;
      if (fmod(p->burning, 6) == 0) {
        // drawVfx({name:"burning", pos:new Vec2D(pos.x, pos.y+5),
        //          face:phys.face}) (physics.js:579-587; M4 task 1)
        ml_drawVfx("burning", p->phys.pos.x, p->phys.pos.y + 5,
                   p->phys.face);
      }
    }

    // TURBO MODE — structurally off in every golden (gameSettings.turbo
    // false); translated verbatim, zero live coverage (fix_plan task 4
    // note). A live turbo dispatch would surface as an unconsumed FIFO
    // dispatch record (self-flagging).
    // if just changed action states, remove ability to cancel
    if (strcmp(p->prevActionState, p->actionState) != 0) {
      p->hasHit = false;
    }
    if (S->turbo && S->gameMode != 5) {
      if (p->hasHit) {
        if (strcmp(p->actionState, "CATCHATTACK") != 0) {
          if (p->phys.grounded) {
            AsTurboGroundState st;
            bool dashbuffer = p->phys.dashbuffer;
            bool hasDashbuffer = true;
            double face = p->phys.face;
            st.actionState = p->actionState;
            st.bTurnaroundDirection = p->phys.bTurnaroundDirection;
            st.bTurnaroundTimer = p->phys.bTurnaroundTimer;
            st.face = &face;
            st.grounded = p->phys.grounded;
            st.hitboxes = &p->hitboxes;
            st.dashbuffer = &dashbuffer;
            st.hasDashbuffer = &hasDashbuffer;
            if (as_turboGroundedInterrupt(&st,
                                          S->characterSelections[slot(i)],
                                          S->tapJumpOff[slot(i)], in)) {
              p->hasHit = false;
            }
            p->phys.face = face;
            p->phys.dashbuffer = dashbuffer;
          } else {
            AsTurboAirState st;
            double face = p->phys.face;
            st.actionState = p->actionState;
            st.face = &face;
            st.doubleJumped = p->phys.doubleJumped;
            st.jumpsUsed = p->phys.jumpsUsed;
            st.bTurnaroundDirection = p->phys.bTurnaroundDirection;
            st.bTurnaroundTimer = p->phys.bTurnaroundTimer;
            st.grounded = p->phys.grounded;
            st.hitboxes = &p->hitboxes;
            if (as_turboAirborneInterrupt(&st,
                                          S->characterSelections[slot(i)],
                                          S->tapJumpOff[slot(i)], in)) {
              p->hasHit = false;
            }
            p->phys.face = face;
          }
        }
      }
    }

    if (js_abs(p->phys.kVel.x) > 0) {
      const double oSign = js_sign(p->phys.kVel.x);
      if (p->phys.grounded) {
        p->phys.kVel.x -= oSign * ml_f64(ATTR(S, i)->traction);
      } else {
        p->phys.kVel.x -= p->phys.kDec.x;
      }
      if (oSign != js_sign(p->phys.kVel.x)) {
        p->phys.kVel.x = 0;
      }
    }
    if (js_abs(p->phys.kVel.y) > 0) {
      const double oSign = js_sign(p->phys.kVel.y);
      if (p->phys.grounded) {
        p->phys.kVel.y = 0;
      }
      p->phys.kVel.y -= p->phys.kDec.y;
      if (oSign != js_sign(p->phys.kVel.y)) {
        p->phys.kVel.y = 0;
      }
    }

    // `pos.x += cVel.x + kVel.x`: compound assignment groups the RIGHT
    // side first — pos.x + (cVel.x + kVel.x). Left-flattening this is a
    // 1-ulp mistranslation class (rule 6: expression shapes verbatim).
    pos_set_x(S, i, p->phys.pos.x + (p->phys.cVel.x + p->phys.kVel.x));
    pos_set_y(S, i, p->phys.pos.y + (p->phys.cVel.y + p->phys.kVel.y));
  }
}

// --- hurtBoxStateUpdate (physics.js:642) -----------------------------------------
static void hurtBoxStateUpdate(MlSim *S, double i) {
  MlPlayer *p = P(S, i);
  if (strcmp(p->actionState, "REBIRTH") == 0 ||
      strcmp(p->actionState, "REBIRTHWAIT") == 0) {
    p->phys.hurtBoxState = 1;
  } else {
    p->phys.hurtBoxState = 0;
  }
  if (p->phys.invincibleTimer > 0) {
    p->phys.invincibleTimer--;
    p->phys.hurtBoxState = 2;
  }
  if (p->phys.intangibleTimer > 0) {
    p->phys.intangibleTimer--;
    p->phys.hurtBoxState = 1;
  }
}

// --- outOfCameraUpdate (physics.js:659) ------------------------------------------
static void outOfCameraUpdate(MlSim *S, double i) {
  MlPlayer *p = P(S, i);
  if (p->phys.outOfCameraTimer >= 60) {
    if (p->percent < 150) {
      p->percent++;
    }
    // percentShake(40, i) — off-tick native-RNG shake machinery; the one
    // checksum-excluded player field (oracle/CHECKSUM.md section 7). No-op.
    ml_sound_play("outofcamera");
    p->phys.outOfCameraTimer = 0;
  }
}

// --- lCancelUpdate (physics.js:670) ------------------------------------------------
static void lCancelUpdate(MlSim *S, double i, const MlInput in[4]) {
  MlPlayer *p = P(S, i);

  // if smash 64 lcancel, put any landingattackair action states into landing
  if (S->lCancelType == 2 && S->gameMode != 5) {
    if (p->phys.lCancel) {
      if (strncmp(p->actionState, "LANDINGATTACKAIR", 16) == 0) {
        set_state(p->actionState, "LANDING");
        p->timer = 1;
      }
    }
  }

  if (p->phys.lCancelTimer > 0) {
    p->phys.lCancelTimer--;
    if (p->phys.lCancelTimer == 0) {
      p->phys.lCancel = false;
    }
  }
  // l CANCEL
  if (p->phys.lCancelTimer == 0 &&
      ((in[0].lA > 0 && in[1].lA == 0) || (in[0].rA > 0 && in[1].rA == 0) ||
       (in[0].z && !in[1].z))) {
    // if smash 64 lcancel, increase window to 11 frames
    if (S->lCancelType == 2 && S->gameMode != 5) {
      p->phys.lCancelTimer = 11;
    } else {
      p->phys.lCancelTimer = 7;
    }
    p->phys.lCancel = true;
  }

  // if auto lcancel is on, always lcancel
  if (S->lCancelType == 1 && S->gameMode != 5) {
    p->phys.lCancel = true;
  }

  // V Cancel
  if (p->phys.vCancelTimer > 0) {
    p->phys.vCancelTimer--;
  }

  if (p->phys.techTimer > 0) {
    p->phys.techTimer--;
  }

  if (p->phys.shoulderLockout > 0) {
    p->phys.shoulderLockout--;
  }

  if ((in[0].l && !in[1].l) || (in[0].r && !in[1].r)) {
    if (!p->phys.grounded) {
      if (p->phys.shoulderLockout == 0) {
        p->phys.vCancelTimer = 3;
        p->phys.techTimer = 20;
      }
    }
    p->phys.shoulderLockout = 40;
  }
}

// --- findAndResolveCollisions (physics.js:749) --------------------------------------
// ecbSquashData: physics.js module state, per-slot on the sim (chained by
// the replay). The shared nullSquashDatum object is never mutated
// (physics.h alias note 3), so value semantics are exact.
static void findAndResolveCollisions(MlSim *S, double i, const MlInput in[4],
                                     bool oldBackward,
                                     bool oldNotTouchingWalls[2],
                                     const double ecbOffset[4],
                                     bool *outStillGrounded,
                                     bool *outBackward,
                                     bool outNotTouchingWalls[2]) {
  MlPlayer *p = P(S, i);
  bool stillGrounded = true;
  bool backward = oldBackward;
  bool notTouchingWalls[2] = {oldNotTouchingWalls[0], oldNotTouchingWalls[1]};

  // ------------------------------------------------------------------
  // grounded state movement
  if (p->phys.grounded) {
    const double relevantGroundIndex = p->phys.onSurface[1];
    char relevantGroundType = 'g';
    Surface relevantGround;
    if (p->phys.onSurface[0] == 1) {
      relevantGroundType = 'p';
      if ((int)relevantGroundIndex < 0 ||
          (int)relevantGroundIndex >= S->stage.s.platform.count) {
        ml_phys_out_of_domain("onSurface platform index");
      }
      relevantGround = S->stage.s.platform.items[(int)relevantGroundIndex];
    } else {
      if ((int)relevantGroundIndex < 0 ||
          (int)relevantGroundIndex >= S->stage.s.ground.count) {
        ml_phys_out_of_domain("onSurface ground index");
      }
      relevantGround = S->stage.s.ground.items[(int)relevantGroundIndex];
    }

    GroundResult gr = dealWithGround(S, i, relevantGround, relevantGroundType,
                                     relevantGroundIndex, in);
    stillGrounded = gr.stillGrounded;
    backward = gr.backward;
  }
  // end of grounded state movement
  // ------------------------------------------------------------------

  // main collision detection routine
  const bool notIgnoringPlatforms =
      ((!FLAGS(S, i)->canPassThrough || (in[0].lsY > -0.56)) &&
       !p->phys.passing);
  const bool isImmune = p->phys.hurtBoxState != 0;

  PlayerStatusInfo playerStatusInfo;
  playerStatusInfo.ignoringPlatforms = !notIgnoringPlatforms;
  playerStatusInfo.grounded = p->phys.grounded;
  playerStatusInfo.immune = isImmune;

  const CollisionRoutineResult collisionData = runCollisionRoutine(
      ecb_of(p->phys.ECB1), ecb_of(p->phys.ECBp), p->phys.pos,
      S->ecbSquashData[slot(i)], playerStatusInfo, &S->stage.s);

  S->ecbSquashData[slot(i)] = collisionData.squashDatum;

  const Vec2D newPosition = collisionData.position;
  const ECB newECB = collisionData.ecb;
  const SimpleTouchingDatum touchingDatum = collisionData.touching;

  // ml_land sets S->landEcbBottom when it assigned pos = newECB[0]
  // (pos-ECB1[0] alias, physics.h note 2)
  S->landEcbBottom = false;

  if (!touchingDatum.present) {
    updatePosition(S, i, newPosition);
  } else if (touchingDatum.kind == CD_SURFACE) {
    const char c = touchingDatum.type; // surfaceLabel[0].toLowerCase()
    const double surfaceIndex = touchingDatum.index;
    const double pt = touchingDatum.pt;
    switch (c) {
      case 'l': // player touching left wall
        notTouchingWalls[0] = false;
        dealWithWallCollision(S, i, newPosition, pt, "l", surfaceIndex, in);
        break;
      case 'r': // player touching right wall
        notTouchingWalls[1] = false;
        dealWithWallCollision(S, i, newPosition, pt, "r", surfaceIndex, in);
        break;
      case 'g': // player landed on ground
        dealWithGroundCollision(S, i, p->phys.grounded, newPosition,
                                newECB.pt[0], surfaceIndex, in);
        break;
      case 'c': // player touching ceiling
        dealWithCeilingCollision(S, i, newPosition, newECB.pt[2],
                                 surfaceIndex, in);
        break;
      case 'p': // player landed on platform
        dealWithPlatformCollision(S, i, p->phys.grounded, newPosition,
                                  newECB.pt[0], surfaceIndex, in);
        break;
      default:
        // console.log("error in 'findAndResolveCollisions': ...")
        break;
    }
  } else { // touchingDatum.kind === "corner"
    const double angularParameter = touchingDatum.angular;
    // touchingDatum.damageType !== undefined ? it : null
    DamageType cornerDamageType = touchingDatum.damageType;
    if (cornerDamageType.tag == DT_ABSENT || cornerDamageType.tag == DT_UNDEF) {
      cornerDamageType = damage_null();
    }
    dealWithCornerCollision(S, i, newPosition, newECB, angularParameter,
                            cornerDamageType);
  }

  // physics.js:847 `player[i].phys.ECB1 = newECB` — if land() ran through
  // the g/p arm, phys.pos IS newECB[0] in JS: the alias becomes live here.
  // No pos component write happens between land() and this point; a move
  // init inside land that REASSIGNED pos gets its truth restored by the
  // very next capture probe (pre-args of the following record).
  ecb_store(p->phys.ECB1, &p->phys.ecb1Undef, newECB);
  S->aliasPosEcb1[slot(i)] = S->landEcbBottom;

  // finally, calculate how much squashing is required by the ground
  if (p->phys.grounded) {
    SurfaceList ceilingList = toList_surfaces(&S->stage.s.ceiling);
    const MaybeNum groundSquashFactor = groundedECBSquashFactor(
        vec2d(p->phys.pos.x, p->phys.pos.y + ecbOffset[3]),
        vec2d(p->phys.pos.x, p->phys.pos.y), &ceilingList);
    if (groundSquashFactor.present &&
        (groundSquashFactor.v < S->ecbSquashData[slot(i)].factor)) {
      S->ecbSquashData[slot(i)] =
          squash_datum(groundSquashFactor.v, false, 0);
    }
    // `if (ecbSquashData[i] !== null)` — vacuously true (never null).
    S->ecbSquashData[slot(i)].locationIsNull = false;
    S->ecbSquashData[slot(i)].location = 0;
  }

  *outStillGrounded = stillGrounded;
  *outBackward = backward;
  outNotTouchingWalls[0] = notTouchingWalls[0];
  outNotTouchingWalls[1] = notTouchingWalls[1];
}

// canGrabLedge is read LAZILY upstream (only when a snap-box test or the
// face-check short-circuit reaches it); undefined[k] would throw there —
// the trap sits at the exact read point.
static bool flag_canGrabLedge(const MlAsFlags *f, int idx) {
  if (!f->hasCanGrabLedge) ml_phys_out_of_domain("canGrabLedge read on undef");
  return f->canGrabLedge[idx];
}

// --- dealWithLedges (physics.js:866) ---------------------------------------------
static void dealWithLedges(MlSim *S, double i, const MlInput in[4]) {
  MlPlayer *p = P(S, i);
  const double playerPosX = p->phys.pos.x;
  const double playerPosY = p->phys.pos.y;
  const ml_attributes_t *attr = ATTR(S, i);
  const double ledgeSnapBoxOffset2 = (double)attr->ledgeSnapBoxOffset[2];
  const double ledgeSnapBoxOffset0 = (double)attr->ledgeSnapBoxOffset[0];
  const double ledgeSnapBoxOffset1 = (double)attr->ledgeSnapBoxOffset[1];
  p->phys.ledgeSnapBoxF =
      box2d(playerPosX, playerPosY + ledgeSnapBoxOffset2,
            playerPosX + ledgeSnapBoxOffset0, playerPosY + ledgeSnapBoxOffset1);
  p->phys.ledgeSnapBoxB =
      box2d(playerPosX - ledgeSnapBoxOffset0, playerPosY + ledgeSnapBoxOffset2,
            playerPosX, playerPosY + ledgeSnapBoxOffset1);

  if (p->phys.ledgeRegrabCount) {
    p->phys.ledgeRegrabTimeout--;
    if (p->phys.ledgeRegrabTimeout == 0) {
      p->phys.ledgeRegrabCount = false;
    }
  }

  double lsBF = -1;
  double lsBB = -1;
  if (p->phys.onLedge == -1 && !p->phys.ledgeRegrabCount) {
    for (int j = 0; j < S->stage.ledgeCount; j++) {
      bool ledgeAvailable = true;
      for (int k = 0; k < 4; k++) {
        if (S->playerType[k] > -1) {
          if (k != slot(i)) {
            if (S->player[k].phys.onLedge == (double)j) {
              ledgeAvailable = false;
            }
          }
        }
      }
      if (ledgeAvailable && !p->phys.grounded && p->hit.hitstun <= 0) {
        const MlLedge *L = &S->stage.ledge[j];
        const SurfaceList *list =
            L->list == 'g' ? &S->stage.s.ground : &S->stage.s.platform;
        if ((int)L->index < 0 || (int)L->index >= list->count) {
          ml_phys_out_of_domain("ledge surface index");
        }
        const Surface *srf = &list->items[(int)L->index];
        const Vec2D lp = L->point == 0 ? srf->p0 : srf->p1;
        const double x = lp.x;
        const double y = lp.y;

        const MlAsFlags *f = FLAGS(S, i);
        if (x > p->phys.ledgeSnapBoxF.min.x && x < p->phys.ledgeSnapBoxF.max.x &&
            y < p->phys.ledgeSnapBoxF.min.y && y > p->phys.ledgeSnapBoxF.max.y) {
          if (L->point == 0) {
            if (flag_canGrabLedge(f, 0)) {
              lsBF = j;
            }
          } else if (flag_canGrabLedge(f, 1)) {
            lsBF = j;
          }
        }
        // upstream quirk: the back-box lower bound reads ledgeSnapBoxF
        // (not B) — carried verbatim (physics.js:926).
        if (x > p->phys.ledgeSnapBoxB.min.x && x < p->phys.ledgeSnapBoxB.max.x &&
            y < p->phys.ledgeSnapBoxB.min.y && y > p->phys.ledgeSnapBoxF.max.y) {
          if (L->point == 1) {
            if (flag_canGrabLedge(f, 0)) {
              lsBB = j;
            }
          } else if (flag_canGrabLedge(f, 1)) {
            lsBB = j;
          }
        }
      }
      if (p->phys.cVel.y < 0 && in[0].lsY > -0.5) {
        if (lsBF > -1) {
          const MlLedge *FL = &S->stage.ledge[(int)lsBF];
          const MlAsFlags *f = FLAGS(S, i);
          if (FL->point * -2 + 1 == p->phys.face || flag_canGrabLedge(f, 1)) {
            p->phys.onLedge = lsBF;
            p->phys.ledgeRegrabTimeout = 30;
            p->phys.face = FL->point * -2 + 1;
            const SurfaceList *list =
                FL->list == 'g' ? &S->stage.s.ground : &S->stage.s.platform;
            const Surface *srf = &list->items[(int)FL->index];
            const Vec2D lp = FL->point == 0 ? srf->p0 : srf->p1;
            pos_reassign(S, i, vec2d(lp.x + edgeOffset[0][0],
                                     lp.y + edgeOffset[0][1]));
            dsp(S, "init", "CLIFFCATCH", i);
          }
        } else if (lsBB > -1) {
          const MlLedge *FL = &S->stage.ledge[(int)lsBB];
          const MlAsFlags *f = FLAGS(S, i);
          if (FL->point * -2 + 1 == p->phys.face || flag_canGrabLedge(f, 1)) {
            p->phys.onLedge = lsBB;
            p->phys.ledgeRegrabTimeout = 30;
            p->phys.face = FL->point * -2 + 1;
            const SurfaceList *list =
                FL->list == 'g' ? &S->stage.s.ground : &S->stage.s.platform;
            const Surface *srf = &list->items[(int)FL->index];
            const Vec2D lp = FL->point == 0 ? srf->p0 : srf->p1;
            pos_reassign(S, i, vec2d(lp.x + edgeOffset[1][0],
                                     lp.y + edgeOffset[1][1]));
            dsp(S, "init", "CLIFFCATCH", i);
          }
        }
      }
    }
  }
}

// --- dealWithDeath (physics.js:962) -------------------------------------------------
static void dealWithDeath(MlSim *S, double i, const MlInput in[4]) {
  MlPlayer *p = P(S, i);
  (void)in;
  if (!FLAGS(S, i)->dead && strcmp(p->actionState, "SLEEP") != 0) {
    const char *state = 0; // let state = 0 (number|string morph upstream)
    if (p->phys.pos.x < S->stage.blastzone.min.x) {
      state = "DEADLEFT";
    } else if (p->phys.pos.x > S->stage.blastzone.max.x) {
      state = "DEADRIGHT";
    } else if (p->phys.pos.y < S->stage.blastzone.min.y) {
      state = "DEADDOWN";
    } else if (p->phys.pos.y > S->stage.blastzone.max.y &&
               p->phys.kVel.y >= 2.4) {
      state = "DEADUP";
    }
    if (state != 0) {
      p->phys.outOfCameraTimer = 0;
      // turnOffHitboxes(i): fresh active + hitList arrays — breaks those
      // prevFrameHitboxes aliases (alias site 1).
      as_turnOffHitboxes(&p->hitboxes);
      S->aliasHbActive[slot(i)] = false;
      S->aliasHbHitList[slot(i)] = false;
      p->stocks--;
      p->colourOverlayBool = false;
      // lostStockQueue.push(...) — render plane (stock icons), no-op in C.
      if (p->stocks == 0 && S->versusMode != 0) {
        p->stocks = 1;
      }
      dsp(S, "init", state, i);
    }
  }
}

// --- updateHitboxes (physics.js:988) -------------------------------------------------
static void updateHitboxes(MlSim *S, double i) {
  MlPlayer *p = P(S, i);
  p->phys.isInterpolated = false;
  for (int j = 0; j < 4; j++) {
    if (p->hitboxes.active[j] && p->phys.prevFrameHitboxes.active[j]) {
      // id[j].offset[frame] === undefined guards: the CONSTRUCTOR shape's
      // offset is a single Vec2D object (offset[<number>] is undefined in
      // JS — always `continue`); the CHARDATA shape's per-frame array is
      // undefined past its length.
      const MlHitboxSpec *pf = &p->phys.prevFrameHitboxes.id[j];
      const MlHitboxSpec *hb = &p->hitboxes.id[j];
      const double pframe = p->phys.prevFrameHitboxes.frame;
      const double hframe = p->hitboxes.frame;
      if (pf->shape == ML_HB_CONSTRUCTOR) continue;
      if (pframe != floor(pframe) || pframe < 0 || pframe >= pf->offsetLen) {
        continue;
      }
      if (hb->shape == ML_HB_CONSTRUCTOR) continue;
      if (hframe != floor(hframe) || hframe < 0 || hframe >= hb->offsetLen) {
        continue;
      }

      const Vec2D h1 = vec2d(
          p->phys.posPrev.x +
              (pf->offsetArr[(int)pframe].x * p->phys.facePrev),
          p->phys.posPrev.y + pf->offsetArr[(int)pframe].y);
      const Vec2D h2 = vec2d(
          p->phys.pos.x + (hb->offsetArr[(int)hframe].x * p->phys.face),
          p->phys.pos.y + hb->offsetArr[(int)hframe].y);

      const double a = h2.x - h1.x;
      const double b = h2.y - h1.y;
      double x = 0;
      if (!(a == 0 || b == 0)) {
        x = fd_atan(js_abs(a) / js_abs(b));
      }
      {
        const double opp = fd_sin(x) * hb->size;
        const double adj = fd_cos(x) * hb->size;
        const double sigma0 = h1.x;
        const double sigma1 = h1.y;
        Vec2D alpha1, alpha2, beta1, beta2;
        if ((a > 0 && b > 0) || (a <= 0 && b <= 0)) {
          alpha1 = vec2d(sigma0 + adj, sigma1 - opp);
          alpha2 = vec2d(alpha1.x + a, alpha1.y + b);
          beta1 = vec2d(sigma0 - adj, sigma1 + opp);
          beta2 = vec2d(beta1.x + a, beta1.y + b);
        } else {
          alpha1 = vec2d(sigma0 - adj, sigma1 - opp);
          alpha2 = vec2d(alpha1.x + a, alpha1.y + b);
          beta1 = vec2d(sigma0 + adj, sigma1 + opp);
          beta2 = vec2d(beta1.x + a, beta1.y + b);
        }
        // interPolatedHitbox[j] = [alpha1, alpha2, beta2, beta1] — sparse
        // writes (holes) never occur in the captured domain (rule 7).
        if (j > p->phys.interPolatedHitboxLen) {
          ml_phys_out_of_domain("interPolatedHitbox hole write");
        }
        p->phys.interPolatedHitbox[j][0] = alpha1;
        p->phys.interPolatedHitbox[j][1] = alpha2;
        p->phys.interPolatedHitbox[j][2] = beta2;
        p->phys.interPolatedHitbox[j][3] = beta1;
        if (j + 1 > p->phys.interPolatedHitboxLen) {
          p->phys.interPolatedHitboxLen = j + 1;
        }
      }
      {
        const double opp = fd_sin(x) * hb->size - S->phantomThreshold;
        const double adj = fd_cos(x) * hb->size - S->phantomThreshold;
        const double sigma0 = h1.x;
        const double sigma1 = h1.y;
        Vec2D alpha1, alpha2, beta1, beta2;
        if ((a > 0 && b > 0) || (a <= 0 && b <= 0)) {
          alpha1 = vec2d(sigma0 + adj, sigma1 - opp);
          alpha2 = vec2d(alpha1.x + a, alpha1.y + b);
          beta1 = vec2d(sigma0 - adj, sigma1 + opp);
          beta2 = vec2d(beta1.x + a, beta1.y + b);
        } else {
          alpha1 = vec2d(sigma0 - adj, sigma1 - opp);
          alpha2 = vec2d(alpha1.x + a, alpha1.y + b);
          beta1 = vec2d(sigma0 + adj, sigma1 + opp);
          beta2 = vec2d(beta1.x + a, beta1.y + b);
        }
        if (j > p->phys.interPolatedHitboxPhantomLen) {
          ml_phys_out_of_domain("interPolatedHitboxPhantom hole write");
        }
        p->phys.interPolatedHitboxPhantom[j][0] = alpha1;
        p->phys.interPolatedHitboxPhantom[j][1] = alpha2;
        p->phys.interPolatedHitboxPhantom[j][2] = beta2;
        p->phys.interPolatedHitboxPhantom[j][3] = beta1;
        if (j + 1 > p->phys.interPolatedHitboxPhantomLen) {
          p->phys.interPolatedHitboxPhantomLen = j + 1;
        }
        p->phys.isInterpolated = true;
      }
    }
  }
}

// --- physics (physics.js:1066, exported) ---------------------------------------------
void ml_physics(MlSim *S, double i, const MlInput in[4]) {
  MlPlayer *p = P(S, i);

  // runtime-added field, first statement of every call (ml_player.h note)
  p->phys.hasPassing = true;
  p->phys.passing = false;
  p->phys.posPrev = vec2d(p->phys.pos.x, p->phys.pos.y);
  p->phys.facePrev = p->phys.face;
  // deepObjectMerge(true, prevFrameHitboxes, hitboxes) — 3-arg call: a
  // SHALLOW per-key reference assignment (rule 10; ml_player.h). Value
  // effect via ml_hitboxes_merge_from; the alias flags go live here.
  ml_hitboxes_merge_from(&p->phys.prevFrameHitboxes, &p->hitboxes);
  S->aliasHbActive[slot(i)] = true;
  S->aliasHbHitList[slot(i)] = true;
  S->aliasHbId[slot(i)] = true;

  hitlagSwitchUpdate(S, i, in);
  hurtBoxStateUpdate(S, i);
  outOfCameraUpdate(S, i);
  lCancelUpdate(S, i, in);

  if (!p->phys.grounded) {
    p->phys.airborneTimer++;
  }

  double frame = floor(p->timer);
  if (frame == 0) {
    frame = 1;
  }
  double fd;
  if (frames_data(S, i, p->actionState, &fd)) {
    if (frame > fd) {
      frame = fd;
    }
  } // else: `frame > undefined` is false — no clamp

  // ecbOffset: dead states skip the ecb table read entirely
  double ecbOffset[4] = {0, 0, 0, 0};
  if (!FLAGS(S, i)->dead) {
    const ml_ecb_state_t *es = ecb_state(S, i, p->actionState);
    const int fidx = (int)(frame - 1);
    if (fidx < 0 || fidx >= es->frameCount) {
      ml_phys_out_of_domain("ecb frame out of range"); // JS would throw
    }
    const double ecbScale = ml_f64(ATTR(S, i)->ecbScale);
    ecbOffset[0] = (double)es->frames[fidx].v[0] * ecbScale;
    ecbOffset[1] = (double)es->frames[fidx].v[1] * ecbScale;
    ecbOffset[2] = (double)es->frames[fidx].v[2] * ecbScale;
    ecbOffset[3] = (double)es->frames[fidx].v[3] * ecbScale;
  }

  const double playerPosX = p->phys.pos.x;
  const double playerPosY = p->phys.pos.y;
  p->phys.ecbpUndef = 0; // fresh Vec2D array (rule-8 mask)
  p->phys.ECBp[0] = vec2d(
      p->phys.pos.x,
      p->phys.pos.y + ((p->phys.grounded || p->phys.airborneTimer < 10)
                           ? 0
                           : ecbOffset[0]));
  p->phys.ECBp[1] =
      vec2d(p->phys.pos.x + js_max(1, ecbOffset[1]), p->phys.pos.y + ecbOffset[2]);
  p->phys.ECBp[2] = vec2d(p->phys.pos.x, p->phys.pos.y + ecbOffset[3]);
  p->phys.ECBp[3] =
      vec2d(p->phys.pos.x - ecbOffset[1], p->phys.pos.y + ecbOffset[2]);

  // `ecbSquashData[i] !== null && factor < 1` — never null (header note 3)
  if (S->ecbSquashData[slot(i)].factor < 1) {
    if (S->ecbSquashData[slot(i)].factor * 2 * ecbOffset[1] <
        ML_SMALLEST_ECB_WIDTH) {
      S->ecbSquashData[slot(i)].factor =
          (ML_SMALLEST_ECB_WIDTH + 2 * ML_ADDITIONAL_OFFSET) /
          (2 * ecbOffset[1]);
    }
    ECB ecbp = ecb_of(p->phys.ECBp);
    ecb_store(p->phys.ECBp, &p->phys.ecbpUndef,
              squashECBAt(ecbp, squash_datum(S->ecbSquashData[slot(i)].factor,
                                             false, 0)));
    if (!p->phys.grounded) {
      ecbp = ecb_of(p->phys.ECBp);
      ecb_store(p->phys.ECBp, &p->phys.ecbpUndef,
                moveECB(ecbp, vec2d(0, (S->ecbSquashData[slot(i)].factor - 1) *
                                           ecbOffset[0])));
    }
  }

  if (!FLAGS(S, i)->ignoreCollision) {
    bool notTouchingWalls[2] = {true, true};
    bool stillGrounded = true;
    bool backward = false;

    findAndResolveCollisions(S, i, in, backward, notTouchingWalls, ecbOffset,
                             &stillGrounded, &backward, notTouchingWalls);

    if (p->phys.grabbedBy == -1) {
      if (notTouchingWalls[0] && notTouchingWalls[1] &&
          (!p->phys.canWallJump.isUndef && p->phys.canWallJump.v)) {
        p->phys.wallJumpTimer = 254;
      }
      if (!notTouchingWalls[0] || !notTouchingWalls[1]) {
        if (p->phys.grounded) {
          const double s = p->phys.onSurface[1];
          const SurfaceList *list = p->phys.onSurface[0] != 0
                                        ? &S->stage.s.platform
                                        : &S->stage.s.ground;
          if ((int)s < 0 || (int)s >= list->count) {
            ml_phys_out_of_domain("onSurface index (walls)");
          }
          const Surface *surface = &list->items[(int)s];
          if (p->phys.pos.x < surface->p0.x - 0.1 ||
              p->phys.pos.x > surface->p1.x + 0.1) {
            stillGrounded = false;
          }
        }
      }
      if (!stillGrounded) {
        p->phys.grounded = false;
        const MlAsFlags *f = FLAGS(S, i);
        if (f->hasAirborneState) {
          set_state(p->actionState, f->airborneState);
        } else {
          if (f->missfoot && backward) {
            dsp(S, "init", "MISSFOOT", i);
          } else {
            if (p->phys.grabbing != -1) {
              // FALL.init(grabbing, input, true) on the GRABBED char's table
              dispatch_bool(S, "init", "FALL",
                            S->characterSelections[(int)p->phys.grabbing],
                            p->phys.grabbing, true);
              S->player[(int)p->phys.grabbing].phys.grabbedBy = -1;
              p->phys.grabbing = -1;
            }
            dsp(S, "init", "FALL", i);
          }
          if (js_abs(p->phys.cVel.x) > ml_f64(ATTR(S, i)->aerialHmaxV)) {
            p->phys.cVel.x =
                js_sign(p->phys.cVel.x) * ml_f64(ATTR(S, i)->aerialHmaxV);
          }
        }
        p->phys.shielding = false;
      }
      if (p->phys.grounded) {
        for (int j = 0; j < 4; j++) {
          if (S->playerType[j] > -1) {
            if (slot(i) != j) {
              MlPlayer *q = &S->player[j];
              if (q->phys.grounded &&
                  q->phys.onSurface[0] == p->phys.onSurface[0] &&
                  q->phys.onSurface[1] == p->phys.onSurface[1]) {
                if (p->phys.grabbing != j && p->phys.grabbedBy != j) {
                  const double diff = js_abs(p->phys.pos.x - q->phys.pos.x);
                  if (diff < 6.5 && diff > 0) {
                    pos_set_x(S, j,
                              q->phys.pos.x +
                                  js_sign(p->phys.pos.x - q->phys.pos.x) *
                                      -0.3);
                  } else if (diff == 0 &&
                             js_abs(p->phys.cVel.x) > js_abs(q->phys.cVel.x)) {
                    pos_set_x(S, j,
                              q->phys.pos.x + js_sign(p->phys.cVel.x) * -0.3);
                  }
                }
              }
            }
          }
        }
      }
    }
  } else { // player ignoring collisions
    p->phys.ecb1Undef = 0; // fresh Vec2D array (rule-8 mask)
    p->phys.ECB1[0] = vec2d(
        p->phys.pos.x,
        p->phys.pos.y + ((p->phys.grounded || p->phys.airborneTimer < 10)
                             ? 0
                             : ecbOffset[0]));
    p->phys.ECB1[1] =
        vec2d(p->phys.pos.x + ecbOffset[1], p->phys.pos.y + ecbOffset[2]);
    p->phys.ECB1[2] = vec2d(p->phys.pos.x, p->phys.pos.y + ecbOffset[3]);
    p->phys.ECB1[3] =
        vec2d(p->phys.pos.x - ecbOffset[1], p->phys.pos.y + ecbOffset[2]);
    // ECB1 = fresh array: any pos-ECB1[0] alias is broken.
    S->aliasPosEcb1[slot(i)] = false;
  }

  if (p->phys.shielding == false) {
    p->phys.shieldHP += 0.07;
    if (p->phys.shieldHP > 60) {
      p->phys.shieldHP = 60;
    }
  }

  dealWithLedges(S, i, in);
  dealWithDeath(S, i, in);

  p->phys.hurtbox = box2d(
      playerPosX - (double)ATTR(S, i)->hurtboxOffset[0],
      playerPosY + (double)ATTR(S, i)->hurtboxOffset[1],
      playerPosX + (double)ATTR(S, i)->hurtboxOffset[0], playerPosY);

  if (S->gameMode == 3 && p->phys.posPrev.y > -80 && playerPosY <= -80) {
    ml_sound_play("lowdown");
  }

  updateHitboxes(S, i);

  p->phys.posDelta = vec2d(js_abs(playerPosX - p->phys.posPrev.x),
                           js_abs(playerPosY - p->phys.posPrev.y));

  // if (showDebug) { document.getElementById(...) ... } — debug DOM plane,
  // no-op in C (reads only, display only).
}
