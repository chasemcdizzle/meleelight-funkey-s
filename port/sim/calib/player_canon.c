// player_canon.c — see player_canon.h. Marshal/serialize the MlPlayer
// value model (ml_player.h) against canon v1.1. The captured-domain
// shapes implemented here are the survey-measured ones (ml_player.h
// header comment); anything else is a hard failure, never a guess
// (prevention rule 7). -ffp-contract=off like every TU.
#include "player_canon.h"

#include <stdio.h>
#include <string.h>

// --- leaf marshal helpers ---------------------------------------------------

static double pc_num(const CanonVal *v, const char *what) {
  if (v->type != CV_NUM) {
    char msg[128];
    snprintf(msg, sizeof msg, "%s: expected number", what);
    pc_fail(msg);
  }
  return v->num;
}

static bool pc_bool(const CanonVal *v, const char *what) {
  if (v->type != CV_BOOL) {
    char msg[128];
    snprintf(msg, sizeof msg, "%s: expected boolean", what);
    pc_fail(msg);
  }
  return v->b;
}

static JsBool pc_jsbool(const CanonVal *v, const char *what) {
  if (v->type == CV_UNDEF) return js_bool_undef();
  return js_bool(pc_bool(v, what));
}

static void pc_str(const CanonVal *v, char *dst, size_t cap, const char *what) {
  char msg[128];
  if (v->type != CV_STR) {
    snprintf(msg, sizeof msg, "%s: expected string", what);
    pc_fail(msg);
  }
  if (strlen(v->str) >= cap) {
    snprintf(msg, sizeof msg, "%s: string exceeds cap", what);
    pc_fail(msg);
  }
  strcpy(dst, v->str);
}

static Vec2D pc_vec2d(const CanonVal *v, const char *what) {
  if (v->type != CV_OBJ || v->nkeys != 2 ||
      strcmp(v->keys[0], "x") != 0 || strcmp(v->keys[1], "y") != 0) {
    char msg[128];
    snprintf(msg, sizeof msg, "%s: expected Vec2D {x,y}", what);
    pc_fail(msg);
  }
  return vec2d(pc_num(v->vals[0], what), pc_num(v->vals[1], what));
}

// Box2D canon (sorted): {max:Vec2D, min:Vec2D}
static Box2D pc_box2d(const CanonVal *v, const char *what) {
  if (v->type != CV_OBJ || v->nkeys != 2 ||
      strcmp(v->keys[0], "max") != 0 || strcmp(v->keys[1], "min") != 0) {
    char msg[128];
    snprintf(msg, sizeof msg, "%s: expected Box2D {max,min}", what);
    pc_fail(msg);
  }
  Box2D b;
  b.max = pc_vec2d(v->vals[0], what);
  b.min = pc_vec2d(v->vals[1], what);
  return b;
}

// ECB-style [Vec2D x4]
static void pc_vec4(const CanonVal *v, Vec2D out[4], const char *what) {
  if (v->type != CV_ARR || v->count != 4) {
    char msg[128];
    snprintf(msg, sizeof msg, "%s: expected [Vec2D x4]", what);
    pc_fail(msg);
  }
  for (int i = 0; i < 4; i++) out[i] = pc_vec2d(v->items[i], what);
}

// --- hitbox spec (hitboxes.id[j]) --------------------------------------------

static MlHitboxSpec pc_hitbox_spec(const CanonVal *v) {
  MlHitboxSpec s;
  memset(&s, 0, sizeof s);
  if (v->type != CV_OBJ) pc_fail("hitbox spec: expected object");
  if (v->nkeys == 8) {
    // constructor ActiveHitbox: angle,bk,dmg,kg,offset,size,sk,type
    static const char *K[8] = {"angle","bk","dmg","kg","offset","size","sk","type"};
    for (int i = 0; i < 8; i++) {
      if (strcmp(v->keys[i], K[i]) != 0) pc_fail("hitbox spec (constructor): unexpected key set");
    }
    s.shape = ML_HB_CONSTRUCTOR;
    s.angle = pc_num(v->vals[0], "hitbox.angle");
    s.bk = pc_num(v->vals[1], "hitbox.bk");
    s.dmg = pc_num(v->vals[2], "hitbox.dmg");
    s.kg = pc_num(v->vals[3], "hitbox.kg");
    s.offset = pc_vec2d(v->vals[4], "hitbox.offset");
    s.size = pc_num(v->vals[5], "hitbox.size");
    s.sk = pc_num(v->vals[6], "hitbox.sk");
    s.type = pc_num(v->vals[7], "hitbox.type");
    return s;
  }
  if (v->nkeys == 12) {
    // chars-data spec: angle,bk,clank,dmg,hitAirborne,hitGrounded,kg,
    //                  offset,size,sk,throwextra,type
    static const char *K[12] = {"angle","bk","clank","dmg","hitAirborne",
      "hitGrounded","kg","offset","size","sk","throwextra","type"};
    for (int i = 0; i < 12; i++) {
      if (strcmp(v->keys[i], K[i]) != 0) pc_fail("hitbox spec (chars-data): unexpected key set");
    }
    s.shape = ML_HB_CHARDATA;
    s.angle = pc_num(v->vals[0], "hitbox.angle");
    s.bk = pc_num(v->vals[1], "hitbox.bk");
    s.clank = pc_num(v->vals[2], "hitbox.clank");
    s.dmg = pc_num(v->vals[3], "hitbox.dmg");
    s.hitAirborne = pc_num(v->vals[4], "hitbox.hitAirborne");
    s.hitGrounded = pc_num(v->vals[5], "hitbox.hitGrounded");
    s.kg = pc_num(v->vals[6], "hitbox.kg");
    const CanonVal *off = v->vals[7];
    if (off->type != CV_ARR || off->count < 1 || off->count > ML_HB_OFFSET_CAP) {
      pc_fail("hitbox.offset: expected Vec2D array within cap");
    }
    s.offsetLen = off->count;
    for (int i = 0; i < off->count; i++) {
      s.offsetArr[i] = pc_vec2d(off->items[i], "hitbox.offset[]");
    }
    s.size = pc_num(v->vals[8], "hitbox.size");
    s.sk = pc_num(v->vals[9], "hitbox.sk");
    s.throwextra = pc_bool(v->vals[10], "hitbox.throwextra");
    s.type = pc_num(v->vals[11], "hitbox.type");
    return s;
  }
  pc_fail("hitbox spec: key count outside captured domain (8 or 12)");
  return s;
}

static void ser_vec2d_pc(CanonBuf *b, Vec2D v) {
  cb_puts(b, "{\"x\":");
  cb_num(b, v.x);
  cb_puts(b, ",\"y\":");
  cb_num(b, v.y);
  cb_putc(b, '}');
}

static void ser_box2d(CanonBuf *b, Box2D box) {
  cb_puts(b, "{\"max\":");
  ser_vec2d_pc(b, box.max);
  cb_puts(b, ",\"min\":");
  ser_vec2d_pc(b, box.min);
  cb_putc(b, '}');
}

static void ser_vec4(CanonBuf *b, const Vec2D v[4]) {
  cb_putc(b, '[');
  for (int i = 0; i < 4; i++) {
    if (i) cb_putc(b, ',');
    ser_vec2d_pc(b, v[i]);
  }
  cb_putc(b, ']');
}

static void ser_bool(CanonBuf *b, bool v) { cb_puts(b, v ? "T" : "F"); }

static void ser_jsbool(CanonBuf *b, JsBool v) {
  if (v.isUndef) cb_puts(b, "undef");
  else ser_bool(b, v.v);
}

static void ser_hitbox_spec(CanonBuf *b, const MlHitboxSpec *s) {
  cb_puts(b, "{\"angle\":");
  cb_num(b, s->angle);
  cb_puts(b, ",\"bk\":");
  cb_num(b, s->bk);
  if (s->shape == ML_HB_CHARDATA) {
    cb_puts(b, ",\"clank\":");
    cb_num(b, s->clank);
  }
  cb_puts(b, ",\"dmg\":");
  cb_num(b, s->dmg);
  if (s->shape == ML_HB_CHARDATA) {
    cb_puts(b, ",\"hitAirborne\":");
    cb_num(b, s->hitAirborne);
    cb_puts(b, ",\"hitGrounded\":");
    cb_num(b, s->hitGrounded);
  }
  cb_puts(b, ",\"kg\":");
  cb_num(b, s->kg);
  cb_puts(b, ",\"offset\":");
  if (s->shape == ML_HB_CONSTRUCTOR) {
    ser_vec2d_pc(b, s->offset);
  } else {
    cb_putc(b, '[');
    for (int i = 0; i < s->offsetLen; i++) {
      if (i) cb_putc(b, ',');
      ser_vec2d_pc(b, s->offsetArr[i]);
    }
    cb_putc(b, ']');
  }
  cb_puts(b, ",\"size\":");
  cb_num(b, s->size);
  cb_puts(b, ",\"sk\":");
  cb_num(b, s->sk);
  if (s->shape == ML_HB_CHARDATA) {
    cb_puts(b, ",\"throwextra\":");
    ser_bool(b, s->throwextra);
  }
  cb_puts(b, ",\"type\":");
  cb_num(b, s->type);
  cb_putc(b, '}');
}

// --- createHitboxes -----------------------------------------------------------

void cv_hitboxes(const CanonVal *v, MlHitboxes *out) {
  memset(out, 0, sizeof *out);
  if (v->type != CV_OBJ) pc_fail("hitboxes: expected object");
  int required = 0;
  for (int k = 0; k < v->nkeys; k++) {
    const char *key = v->keys[k];
    const CanonVal *val = v->vals[k];
    if (strcmp(key, "active") == 0) {
      if (val->type != CV_ARR || val->count != 4) pc_fail("hitboxes.active: expected [bool x4]");
      for (int i = 0; i < 4; i++) out->active[i] = pc_bool(val->items[i], "hitboxes.active[]");
      required++;
    } else if (strcmp(key, "frame") == 0) {
      out->frame = pc_num(val, "hitboxes.frame");
      required++;
    } else if (strcmp(key, "frames") == 0) {
      out->hasFrames = true;
      out->frames = pc_num(val, "hitboxes.frames");
    } else if (strcmp(key, "hitList") == 0) {
      if (val->type != CV_ARR || val->count > ML_HITLIST_CAP) pc_fail("hitboxes.hitList: bad array");
      out->hitListLen = val->count;
      for (int i = 0; i < val->count; i++) {
        out->hitList[i] = pc_num(val->items[i], "hitboxes.hitList[]");
      }
      required++;
    } else if (strcmp(key, "id") == 0) {
      if (val->type != CV_ARR || val->count != 4) pc_fail("hitboxes.id: expected [spec x4]");
      for (int i = 0; i < 4; i++) out->id[i] = pc_hitbox_spec(val->items[i]);
      required++;
    } else {
      pc_fail("hitboxes: unexpected key");
    }
  }
  if (required != 4) pc_fail("hitboxes: missing required key");
}

void ser_hitboxes(CanonBuf *b, const MlHitboxes *h) {
  cb_puts(b, "{\"active\":[");
  for (int i = 0; i < 4; i++) {
    if (i) cb_putc(b, ',');
    ser_bool(b, h->active[i]);
  }
  cb_puts(b, "],\"frame\":");
  cb_num(b, h->frame);
  if (h->hasFrames) {
    cb_puts(b, ",\"frames\":");
    cb_num(b, h->frames);
  }
  cb_puts(b, ",\"hitList\":[");
  for (int i = 0; i < h->hitListLen; i++) {
    if (i) cb_putc(b, ',');
    cb_num(b, h->hitList[i]);
  }
  cb_puts(b, "],\"id\":[");
  for (int i = 0; i < 4; i++) {
    if (i) cb_putc(b, ',');
    ser_hitbox_spec(b, &h->id[i]);
  }
  cb_puts(b, "]}");
}

// --- hit ------------------------------------------------------------------------

static void cv_hit(const CanonVal *v, MlHit *out) {
  memset(out, 0, sizeof *out);
  if (v->type != CV_OBJ) pc_fail("hit: expected object");
  int required = 0;
  for (int k = 0; k < v->nkeys; k++) {
    const char *key = v->keys[k];
    const CanonVal *val = v->vals[k];
    if (strcmp(key, "angle") == 0) { out->angle = pc_num(val, "hit.angle"); required++; }
    else if (strcmp(key, "hitPoint") == 0) { out->hitPoint = pc_vec2d(val, "hit.hitPoint"); required++; }
    else if (strcmp(key, "hitlag") == 0) { out->hitlag = pc_num(val, "hit.hitlag"); required++; }
    else if (strcmp(key, "hitstun") == 0) { out->hitstun = pc_num(val, "hit.hitstun"); required++; }
    else if (strcmp(key, "knockback") == 0) { out->knockback = pc_num(val, "hit.knockback"); required++; }
    else if (strcmp(key, "powershield") == 0) { out->powershield = pc_bool(val, "hit.powershield"); required++; }
    else if (strcmp(key, "reverse") == 0) { out->hasReverse = true; out->reverse = pc_bool(val, "hit.reverse"); }
    else if (strcmp(key, "shieldstun") == 0) { out->shieldstun = pc_num(val, "hit.shieldstun"); required++; }
    else pc_fail("hit: unexpected key");
  }
  if (required != 7) pc_fail("hit: missing required key");
}

static void ser_hit(CanonBuf *b, const MlHit *h) {
  cb_puts(b, "{\"angle\":");
  cb_num(b, h->angle);
  cb_puts(b, ",\"hitPoint\":");
  ser_vec2d_pc(b, h->hitPoint);
  cb_puts(b, ",\"hitlag\":");
  cb_num(b, h->hitlag);
  cb_puts(b, ",\"hitstun\":");
  cb_num(b, h->hitstun);
  cb_puts(b, ",\"knockback\":");
  cb_num(b, h->knockback);
  cb_puts(b, ",\"powershield\":");
  ser_bool(b, h->powershield);
  if (h->hasReverse) {
    cb_puts(b, ",\"reverse\":");
    ser_bool(b, h->reverse);
  }
  cb_puts(b, ",\"shieldstun\":");
  cb_num(b, h->shieldstun);
  cb_putc(b, '}');
}

// --- interpolated hitbox lists ([ [Vec2D x4] x0..4 ]) ---------------------------

static int cv_interp(const CanonVal *v, Vec2D out[ML_INTERP_CAP][4], const char *what) {
  char msg[128];
  if (v->type != CV_ARR || v->count > ML_INTERP_CAP) {
    snprintf(msg, sizeof msg, "%s: expected array (cap %d)", what, ML_INTERP_CAP);
    pc_fail(msg);
  }
  for (int i = 0; i < v->count; i++) pc_vec4(v->items[i], out[i], what);
  return v->count;
}

static void ser_interp(CanonBuf *b, const Vec2D v[ML_INTERP_CAP][4], int n) {
  cb_putc(b, '[');
  for (int i = 0; i < n; i++) {
    if (i) cb_putc(b, ',');
    ser_vec4(b, v[i]);
  }
  cb_putc(b, ']');
}

// --- physicsObject ----------------------------------------------------------------

// 75 always-present phys keys post-update (74 distinct constructor fields
// + runtime-added passing).
#define PHYS_REQUIRED 75

static void cv_phys(const CanonVal *v, MlPhysics *out) {
  memset(out, 0, sizeof *out);
  if (v->type != CV_OBJ) pc_fail("phys: expected object");
  int required = 0;
  for (int k = 0; k < v->nkeys; k++) {
    const char *key = v->keys[k];
    const CanonVal *val = v->vals[k];
    #define REQ_NUM(name, field) \
      else if (strcmp(key, name) == 0) { out->field = pc_num(val, "phys." name); required++; }
    #define REQ_BOOL(name, field) \
      else if (strcmp(key, name) == 0) { out->field = pc_bool(val, "phys." name); required++; }
    #define REQ_VEC(name, field) \
      else if (strcmp(key, name) == 0) { out->field = pc_vec2d(val, "phys." name); required++; }
    #define REQ_BOX(name, field) \
      else if (strcmp(key, name) == 0) { out->field = pc_box2d(val, "phys." name); required++; }
    #define REQ_VEC4(name, field) \
      else if (strcmp(key, name) == 0) { pc_vec4(val, out->field, "phys." name); required++; }
    #define OPT_NUM(name, flag, field) \
      else if (strcmp(key, name) == 0) { out->flag = true; out->field = pc_num(val, "phys." name); }
    #define OPT_BOOL(name, flag, field) \
      else if (strcmp(key, name) == 0) { out->flag = true; out->field = pc_bool(val, "phys." name); }

    if (false) {}
    REQ_VEC4("ECB1", ECB1)
    REQ_VEC4("ECB2", ECB2)
    REQ_VEC4("ECBp", ECBp)
    REQ_NUM("airborneTimer", airborneTimer)
    REQ_BOOL("autoCancel", autoCancel)
    OPT_BOOL("autocancel", hasAutocancelLower, autocancelLower)
    REQ_NUM("bTurnaroundDirection", bTurnaroundDirection)
    REQ_NUM("bTurnaroundTimer", bTurnaroundTimer)
    REQ_VEC("cVel", cVel)
    else if (strcmp(key, "canWallJump") == 0) {
      out->canWallJump = pc_jsbool(val, "phys.canWallJump");
      required++;
    }
    REQ_NUM("chargeFrames", chargeFrames)
    REQ_BOOL("charging", charging)
    REQ_BOOL("dashbuffer", dashbuffer)
    REQ_BOOL("doubleJumped", doubleJumped)
    REQ_NUM("face", face)
    REQ_NUM("facePrev", facePrev)
    REQ_BOOL("fastfalled", fastfalled)
    OPT_BOOL("grabTech", hasGrabTech, grabTech)
    REQ_NUM("grabbedBy", grabbedBy)
    REQ_NUM("grabbing", grabbing)
    REQ_NUM("groundAngle", groundAngle)
    REQ_BOOL("grounded", grounded)
    REQ_NUM("hurtBoxState", hurtBoxState)
    REQ_BOX("hurtbox", hurtbox)
    REQ_NUM("inShine", inShine)
    REQ_NUM("intangibleTimer", intangibleTimer)
    else if (strcmp(key, "interPolatedHitbox") == 0) {
      out->interPolatedHitboxLen = cv_interp(val, out->interPolatedHitbox, "phys.interPolatedHitbox");
      required++;
    }
    else if (strcmp(key, "interPolatedHitboxPhantom") == 0) {
      out->interPolatedHitboxPhantomLen =
          cv_interp(val, out->interPolatedHitboxPhantom, "phys.interPolatedHitboxPhantom");
      required++;
    }
    REQ_NUM("invincibleTimer", invincibleTimer)
    REQ_BOOL("isInterpolated", isInterpolated)
    REQ_BOOL("jabCombo", jabCombo)
    REQ_BOOL("jabReset", jabReset)
    REQ_NUM("jumpSquatType", jumpSquatType)
    REQ_NUM("jumpType", jumpType)
    REQ_NUM("jumpsUsed", jumpsUsed)
    REQ_VEC("kDec", kDec)
    REQ_VEC("kVel", kVel)
    REQ_BOOL("lCancel", lCancel)
    REQ_NUM("lCancelTimer", lCancelTimer)
    REQ_NUM("landingLagScaling", landingLagScaling)
    REQ_NUM("landingMultiplier", landingMultiplier)
    OPT_BOOL("laserCombo", hasLaserCombo, laserCombo)
    OPT_NUM("ledgeHangTimer", hasLedgeHangTimer, ledgeHangTimer)
    REQ_BOOL("ledgeRegrabCount", ledgeRegrabCount)
    REQ_NUM("ledgeRegrabTimeout", ledgeRegrabTimeout)
    REQ_BOX("ledgeSnapBoxB", ledgeSnapBoxB)
    REQ_BOX("ledgeSnapBoxF", ledgeSnapBoxF)
    REQ_NUM("onLedge", onLedge)
    else if (strcmp(key, "onSurface") == 0) {
      if (val->type != CV_ARR || val->count != 2) pc_fail("phys.onSurface: expected [n,n]");
      out->onSurface[0] = pc_num(val->items[0], "phys.onSurface[0]");
      out->onSurface[1] = pc_num(val->items[1], "phys.onSurface[1]");
      required++;
    }
    REQ_NUM("outOfCameraTimer", outOfCameraTimer)
    REQ_BOOL("passFastfall", passFastfall)
    REQ_BOOL("passing", passing)
    REQ_VEC("pos", pos)
    REQ_VEC("posDelta", posDelta)
    REQ_VEC("posPrev", posPrev)
    REQ_BOOL("powerShieldActive", powerShieldActive)
    REQ_BOOL("powerShieldReflectActive", powerShieldReflectActive)
    REQ_BOOL("powerShielded", powerShielded)
    else if (strcmp(key, "prevFrameHitboxes") == 0) {
      cv_hitboxes(val, &out->prevFrameHitboxes);
      required++;
    }
    REQ_BOOL("raptorBoost", raptorBoost)
    REQ_NUM("releaseFrame", releaseFrame)
    OPT_NUM("rollOutCharge", hasRollOutCharge, rollOutCharge)
    OPT_BOOL("rollOutChargeAttempt", hasRollOutChargeAttempt, rollOutChargeAttempt)
    OPT_BOOL("rollOutCharging", hasRollOutCharging, rollOutCharging)
    REQ_NUM("rollOutDistance", rollOutDistance)
    OPT_BOOL("rollOutPlayerHit", hasRollOutPlayerHit, rollOutPlayerHit)
    OPT_NUM("rollOutPlayerHitTimer", hasRollOutPlayerHitTimer, rollOutPlayerHitTimer)
    OPT_NUM("rollOutVel", hasRollOutVel, rollOutVel)
    OPT_BOOL("rollOutWallHit", hasRollOutWallHit, rollOutWallHit)
    REQ_NUM("shieldAnalog", shieldAnalog)
    REQ_NUM("shieldHP", shieldHP)
    REQ_VEC("shieldPosition", shieldPosition)
    REQ_VEC("shieldPositionReal", shieldPositionReal)
    REQ_NUM("shieldSize", shieldSize)
    REQ_NUM("shieldStun", shieldStun)
    REQ_BOOL("shielding", shielding)
    REQ_NUM("shoulderLockout", shoulderLockout)
    REQ_BOOL("sideBJumpFlag", sideBJumpFlag)
    REQ_NUM("stuckTimer", stuckTimer)
    REQ_NUM("techTimer", techTimer)
    REQ_BOOL("thrownHitbox", thrownHitbox)
    REQ_NUM("thrownHitboxOwner", thrownHitboxOwner)
    REQ_NUM("upbAngleMultiplier", upbAngleMultiplier)
    REQ_NUM("vCancelTimer", vCancelTimer)
    REQ_NUM("wallJumpCount", wallJumpCount)
    REQ_NUM("wallJumpTimer", wallJumpTimer)
    else pc_fail("phys: unexpected key (extend the model from a fresh survey)");

    #undef REQ_NUM
    #undef REQ_BOOL
    #undef REQ_VEC
    #undef REQ_BOX
    #undef REQ_VEC4
    #undef OPT_NUM
    #undef OPT_BOOL
  }
  if (required != PHYS_REQUIRED) pc_fail("phys: missing required key");
}

static void ser_phys(CanonBuf *b, const MlPhysics *p) {
  cb_puts(b, "{\"ECB1\":");
  ser_vec4(b, p->ECB1);
  cb_puts(b, ",\"ECB2\":");
  ser_vec4(b, p->ECB2);
  cb_puts(b, ",\"ECBp\":");
  ser_vec4(b, p->ECBp);
  cb_puts(b, ",\"airborneTimer\":");
  cb_num(b, p->airborneTimer);
  cb_puts(b, ",\"autoCancel\":");
  ser_bool(b, p->autoCancel);
  if (p->hasAutocancelLower) {
    cb_puts(b, ",\"autocancel\":");
    ser_bool(b, p->autocancelLower);
  }
  cb_puts(b, ",\"bTurnaroundDirection\":");
  cb_num(b, p->bTurnaroundDirection);
  cb_puts(b, ",\"bTurnaroundTimer\":");
  cb_num(b, p->bTurnaroundTimer);
  cb_puts(b, ",\"cVel\":");
  ser_vec2d_pc(b, p->cVel);
  cb_puts(b, ",\"canWallJump\":");
  ser_jsbool(b, p->canWallJump);
  cb_puts(b, ",\"chargeFrames\":");
  cb_num(b, p->chargeFrames);
  cb_puts(b, ",\"charging\":");
  ser_bool(b, p->charging);
  cb_puts(b, ",\"dashbuffer\":");
  ser_bool(b, p->dashbuffer);
  cb_puts(b, ",\"doubleJumped\":");
  ser_bool(b, p->doubleJumped);
  cb_puts(b, ",\"face\":");
  cb_num(b, p->face);
  cb_puts(b, ",\"facePrev\":");
  cb_num(b, p->facePrev);
  cb_puts(b, ",\"fastfalled\":");
  ser_bool(b, p->fastfalled);
  if (p->hasGrabTech) {
    cb_puts(b, ",\"grabTech\":");
    ser_bool(b, p->grabTech);
  }
  cb_puts(b, ",\"grabbedBy\":");
  cb_num(b, p->grabbedBy);
  cb_puts(b, ",\"grabbing\":");
  cb_num(b, p->grabbing);
  cb_puts(b, ",\"groundAngle\":");
  cb_num(b, p->groundAngle);
  cb_puts(b, ",\"grounded\":");
  ser_bool(b, p->grounded);
  cb_puts(b, ",\"hurtBoxState\":");
  cb_num(b, p->hurtBoxState);
  cb_puts(b, ",\"hurtbox\":");
  ser_box2d(b, p->hurtbox);
  cb_puts(b, ",\"inShine\":");
  cb_num(b, p->inShine);
  cb_puts(b, ",\"intangibleTimer\":");
  cb_num(b, p->intangibleTimer);
  cb_puts(b, ",\"interPolatedHitbox\":");
  ser_interp(b, p->interPolatedHitbox, p->interPolatedHitboxLen);
  cb_puts(b, ",\"interPolatedHitboxPhantom\":");
  ser_interp(b, p->interPolatedHitboxPhantom, p->interPolatedHitboxPhantomLen);
  cb_puts(b, ",\"invincibleTimer\":");
  cb_num(b, p->invincibleTimer);
  cb_puts(b, ",\"isInterpolated\":");
  ser_bool(b, p->isInterpolated);
  cb_puts(b, ",\"jabCombo\":");
  ser_bool(b, p->jabCombo);
  cb_puts(b, ",\"jabReset\":");
  ser_bool(b, p->jabReset);
  cb_puts(b, ",\"jumpSquatType\":");
  cb_num(b, p->jumpSquatType);
  cb_puts(b, ",\"jumpType\":");
  cb_num(b, p->jumpType);
  cb_puts(b, ",\"jumpsUsed\":");
  cb_num(b, p->jumpsUsed);
  cb_puts(b, ",\"kDec\":");
  ser_vec2d_pc(b, p->kDec);
  cb_puts(b, ",\"kVel\":");
  ser_vec2d_pc(b, p->kVel);
  cb_puts(b, ",\"lCancel\":");
  ser_bool(b, p->lCancel);
  cb_puts(b, ",\"lCancelTimer\":");
  cb_num(b, p->lCancelTimer);
  cb_puts(b, ",\"landingLagScaling\":");
  cb_num(b, p->landingLagScaling);
  cb_puts(b, ",\"landingMultiplier\":");
  cb_num(b, p->landingMultiplier);
  if (p->hasLaserCombo) {
    cb_puts(b, ",\"laserCombo\":");
    ser_bool(b, p->laserCombo);
  }
  if (p->hasLedgeHangTimer) {
    cb_puts(b, ",\"ledgeHangTimer\":");
    cb_num(b, p->ledgeHangTimer);
  }
  cb_puts(b, ",\"ledgeRegrabCount\":");
  ser_bool(b, p->ledgeRegrabCount);
  cb_puts(b, ",\"ledgeRegrabTimeout\":");
  cb_num(b, p->ledgeRegrabTimeout);
  cb_puts(b, ",\"ledgeSnapBoxB\":");
  ser_box2d(b, p->ledgeSnapBoxB);
  cb_puts(b, ",\"ledgeSnapBoxF\":");
  ser_box2d(b, p->ledgeSnapBoxF);
  cb_puts(b, ",\"onLedge\":");
  cb_num(b, p->onLedge);
  cb_puts(b, ",\"onSurface\":[");
  cb_num(b, p->onSurface[0]);
  cb_putc(b, ',');
  cb_num(b, p->onSurface[1]);
  cb_puts(b, "],\"outOfCameraTimer\":");
  cb_num(b, p->outOfCameraTimer);
  cb_puts(b, ",\"passFastfall\":");
  ser_bool(b, p->passFastfall);
  cb_puts(b, ",\"passing\":");
  ser_bool(b, p->passing);
  cb_puts(b, ",\"pos\":");
  ser_vec2d_pc(b, p->pos);
  cb_puts(b, ",\"posDelta\":");
  ser_vec2d_pc(b, p->posDelta);
  cb_puts(b, ",\"posPrev\":");
  ser_vec2d_pc(b, p->posPrev);
  cb_puts(b, ",\"powerShieldActive\":");
  ser_bool(b, p->powerShieldActive);
  cb_puts(b, ",\"powerShieldReflectActive\":");
  ser_bool(b, p->powerShieldReflectActive);
  cb_puts(b, ",\"powerShielded\":");
  ser_bool(b, p->powerShielded);
  cb_puts(b, ",\"prevFrameHitboxes\":");
  ser_hitboxes(b, &p->prevFrameHitboxes);
  cb_puts(b, ",\"raptorBoost\":");
  ser_bool(b, p->raptorBoost);
  cb_puts(b, ",\"releaseFrame\":");
  cb_num(b, p->releaseFrame);
  if (p->hasRollOutCharge) {
    cb_puts(b, ",\"rollOutCharge\":");
    cb_num(b, p->rollOutCharge);
  }
  if (p->hasRollOutChargeAttempt) {
    cb_puts(b, ",\"rollOutChargeAttempt\":");
    ser_bool(b, p->rollOutChargeAttempt);
  }
  if (p->hasRollOutCharging) {
    cb_puts(b, ",\"rollOutCharging\":");
    ser_bool(b, p->rollOutCharging);
  }
  cb_puts(b, ",\"rollOutDistance\":");
  cb_num(b, p->rollOutDistance);
  if (p->hasRollOutPlayerHit) {
    cb_puts(b, ",\"rollOutPlayerHit\":");
    ser_bool(b, p->rollOutPlayerHit);
  }
  if (p->hasRollOutPlayerHitTimer) {
    cb_puts(b, ",\"rollOutPlayerHitTimer\":");
    cb_num(b, p->rollOutPlayerHitTimer);
  }
  if (p->hasRollOutVel) {
    cb_puts(b, ",\"rollOutVel\":");
    cb_num(b, p->rollOutVel);
  }
  if (p->hasRollOutWallHit) {
    cb_puts(b, ",\"rollOutWallHit\":");
    ser_bool(b, p->rollOutWallHit);
  }
  cb_puts(b, ",\"shieldAnalog\":");
  cb_num(b, p->shieldAnalog);
  cb_puts(b, ",\"shieldHP\":");
  cb_num(b, p->shieldHP);
  cb_puts(b, ",\"shieldPosition\":");
  ser_vec2d_pc(b, p->shieldPosition);
  cb_puts(b, ",\"shieldPositionReal\":");
  ser_vec2d_pc(b, p->shieldPositionReal);
  cb_puts(b, ",\"shieldSize\":");
  cb_num(b, p->shieldSize);
  cb_puts(b, ",\"shieldStun\":");
  cb_num(b, p->shieldStun);
  cb_puts(b, ",\"shielding\":");
  ser_bool(b, p->shielding);
  cb_puts(b, ",\"shoulderLockout\":");
  cb_num(b, p->shoulderLockout);
  cb_puts(b, ",\"sideBJumpFlag\":");
  ser_bool(b, p->sideBJumpFlag);
  cb_puts(b, ",\"stuckTimer\":");
  cb_num(b, p->stuckTimer);
  cb_puts(b, ",\"techTimer\":");
  cb_num(b, p->techTimer);
  cb_puts(b, ",\"thrownHitbox\":");
  ser_bool(b, p->thrownHitbox);
  cb_puts(b, ",\"thrownHitboxOwner\":");
  cb_num(b, p->thrownHitboxOwner);
  cb_puts(b, ",\"upbAngleMultiplier\":");
  cb_num(b, p->upbAngleMultiplier);
  cb_puts(b, ",\"vCancelTimer\":");
  cb_num(b, p->vCancelTimer);
  cb_puts(b, ",\"wallJumpCount\":");
  cb_num(b, p->wallJumpCount);
  cb_puts(b, ",\"wallJumpTimer\":");
  cb_num(b, p->wallJumpTimer);
  cb_putc(b, '}');
}

// --- playerObject ------------------------------------------------------------------

// 29 always-present player keys post-update (constructor minus the three
// projected fields: charAttributes, charHitboxes, percentShake).
#define PLAYER_REQUIRED 29

void cv_player(const CanonVal *v, MlPlayer *out) {
  memset(out, 0, sizeof *out);
  if (v->type != CV_OBJ) pc_fail("player: expected object");
  int required = 0;
  for (int k = 0; k < v->nkeys; k++) {
    const char *key = v->keys[k];
    const CanonVal *val = v->vals[k];
    if (strcmp(key, "IASATimer") == 0) {
      out->hasIASATimer = true;
      out->IASATimer = pc_num(val, "player.IASATimer");
    } else if (strcmp(key, "actionState") == 0) {
      pc_str(val, out->actionState, ML_STR_CAP, "player.actionState");
      required++;
    } else if (strcmp(key, "burning") == 0) {
      out->burning = pc_num(val, "player.burning");
      required++;
    } else if (strcmp(key, "colourOverlay") == 0) {
      pc_str(val, out->colourOverlay, ML_STR_CAP, "player.colourOverlay");
      required++;
    } else if (strcmp(key, "colourOverlayBool") == 0) {
      out->colourOverlayBool = pc_bool(val, "player.colourOverlayBool");
      required++;
    } else if (strcmp(key, "currentAction") == 0) {
      pc_str(val, out->currentAction, ML_STR_CAP, "player.currentAction");
      required++;
    } else if (strcmp(key, "currentSubaction") == 0) {
      pc_str(val, out->currentSubaction, ML_STR_CAP, "player.currentSubaction");
      required++;
    } else if (strcmp(key, "difficulty") == 0) {
      out->difficulty = pc_num(val, "player.difficulty");
      required++;
    } else if (strcmp(key, "furaLoopID") == 0) {
      out->furaLoopID = pc_num(val, "player.furaLoopID");
      required++;
    } else if (strcmp(key, "hasHit") == 0) {
      out->hasHit = pc_bool(val, "player.hasHit");
      required++;
    } else if (strcmp(key, "hit") == 0) {
      cv_hit(val, &out->hit);
      required++;
    } else if (strcmp(key, "hitboxes") == 0) {
      cv_hitboxes(val, &out->hitboxes);
      required++;
    } else if (strcmp(key, "inAerial") == 0) {
      out->hasInAerial = true;
      out->inAerial = pc_bool(val, "player.inAerial");
    } else if (strcmp(key, "inCSS") == 0) {
      out->inCSS = pc_bool(val, "player.inCSS");
      required++;
    } else if (strcmp(key, "laserCombo") == 0) {
      out->laserCombo = pc_bool(val, "player.laserCombo");
      required++;
    } else if (strcmp(key, "lastMash") == 0) {
      out->lastMash = pc_num(val, "player.lastMash");
      required++;
    } else if (strcmp(key, "miniView") == 0) {
      out->miniView = pc_bool(val, "player.miniView");
      required++;
    } else if (strcmp(key, "miniViewPoint") == 0) {
      out->miniViewPoint = pc_vec2d(val, "player.miniViewPoint");
      required++;
    } else if (strcmp(key, "percent") == 0) {
      out->percent = pc_num(val, "player.percent");
      required++;
    } else if (strcmp(key, "phys") == 0) {
      cv_phys(val, &out->phys);
      required++;
    } else if (strcmp(key, "prevActionState") == 0) {
      pc_str(val, out->prevActionState, ML_STR_CAP, "player.prevActionState");
      required++;
    } else if (strcmp(key, "rotation") == 0) {
      out->rotation = pc_num(val, "player.rotation");
      required++;
    } else if (strcmp(key, "rotationPoint") == 0) {
      out->rotationPoint = pc_vec2d(val, "player.rotationPoint");
      required++;
    } else if (strcmp(key, "shineLoop") == 0) {
      out->shineLoop = pc_num(val, "player.shineLoop");
      required++;
    } else if (strcmp(key, "shocked") == 0) {
      out->shocked = pc_num(val, "player.shocked");
      required++;
    } else if (strcmp(key, "showECB") == 0) {
      out->showECB = pc_bool(val, "player.showECB");
      required++;
    } else if (strcmp(key, "showHitbox") == 0) {
      out->showHitbox = pc_bool(val, "player.showHitbox");
      required++;
    } else if (strcmp(key, "showLedgeGrabBox") == 0) {
      out->showLedgeGrabBox = pc_bool(val, "player.showLedgeGrabBox");
      required++;
    } else if (strcmp(key, "spawnWaitTime") == 0) {
      out->spawnWaitTime = pc_num(val, "player.spawnWaitTime");
      required++;
    } else if (strcmp(key, "stocks") == 0) {
      out->stocks = pc_num(val, "player.stocks");
      required++;
    } else if (strcmp(key, "timer") == 0) {
      out->timer = pc_num(val, "player.timer");
      required++;
    } else {
      pc_fail("player: unexpected key (extend the model from a fresh survey)");
    }
  }
  if (required != PLAYER_REQUIRED) pc_fail("player: missing required key");
}

void ser_player(CanonBuf *b, const MlPlayer *p) {
  cb_putc(b, '{');
  if (p->hasIASATimer) {
    cb_puts(b, "\"IASATimer\":");
    cb_num(b, p->IASATimer);
    cb_putc(b, ',');
  }
  cb_puts(b, "\"actionState\":");
  cb_qstr(b, p->actionState);
  cb_puts(b, ",\"burning\":");
  cb_num(b, p->burning);
  cb_puts(b, ",\"colourOverlay\":");
  cb_qstr(b, p->colourOverlay);
  cb_puts(b, ",\"colourOverlayBool\":");
  ser_bool(b, p->colourOverlayBool);
  cb_puts(b, ",\"currentAction\":");
  cb_qstr(b, p->currentAction);
  cb_puts(b, ",\"currentSubaction\":");
  cb_qstr(b, p->currentSubaction);
  cb_puts(b, ",\"difficulty\":");
  cb_num(b, p->difficulty);
  cb_puts(b, ",\"furaLoopID\":");
  cb_num(b, p->furaLoopID);
  cb_puts(b, ",\"hasHit\":");
  ser_bool(b, p->hasHit);
  cb_puts(b, ",\"hit\":");
  ser_hit(b, &p->hit);
  cb_puts(b, ",\"hitboxes\":");
  ser_hitboxes(b, &p->hitboxes);
  if (p->hasInAerial) {
    cb_puts(b, ",\"inAerial\":");
    ser_bool(b, p->inAerial);
  }
  cb_puts(b, ",\"inCSS\":");
  ser_bool(b, p->inCSS);
  cb_puts(b, ",\"laserCombo\":");
  ser_bool(b, p->laserCombo);
  cb_puts(b, ",\"lastMash\":");
  cb_num(b, p->lastMash);
  cb_puts(b, ",\"miniView\":");
  ser_bool(b, p->miniView);
  cb_puts(b, ",\"miniViewPoint\":");
  ser_vec2d_pc(b, p->miniViewPoint);
  cb_puts(b, ",\"percent\":");
  cb_num(b, p->percent);
  cb_puts(b, ",\"phys\":");
  ser_phys(b, &p->phys);
  cb_puts(b, ",\"prevActionState\":");
  cb_qstr(b, p->prevActionState);
  cb_puts(b, ",\"rotation\":");
  cb_num(b, p->rotation);
  cb_puts(b, ",\"rotationPoint\":");
  ser_vec2d_pc(b, p->rotationPoint);
  cb_puts(b, ",\"shineLoop\":");
  cb_num(b, p->shineLoop);
  cb_puts(b, ",\"shocked\":");
  cb_num(b, p->shocked);
  cb_puts(b, ",\"showECB\":");
  ser_bool(b, p->showECB);
  cb_puts(b, ",\"showHitbox\":");
  ser_bool(b, p->showHitbox);
  cb_puts(b, ",\"showLedgeGrabBox\":");
  ser_bool(b, p->showLedgeGrabBox);
  cb_puts(b, ",\"spawnWaitTime\":");
  cb_num(b, p->spawnWaitTime);
  cb_puts(b, ",\"stocks\":");
  cb_num(b, p->stocks);
  cb_puts(b, ",\"timer\":");
  cb_num(b, p->timer);
  cb_putc(b, '}');
}
