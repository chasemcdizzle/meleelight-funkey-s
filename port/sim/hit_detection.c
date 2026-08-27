// hit_detection.c <- src/physics/hitDetection.js @27af171 (structure-
// parallel translation, M2 task 6). Expression shapes verbatim (fix_plan
// §M2 rules 6/13: parenthesization, operator desugaring, evaluation order,
// lazy dereference points); fd_* for the Math.* the harness shims
// (cos/sin/atan/atan2/pow — Math.sqrt/floor are unshimmed IEEE-exact C);
// js_round for Math.round (ml_js.h). Compile every TU with
// -ffp-contract=off (PLAN §2).
//
// Line references are to the upstream source. See hit_detection.h for the
// value model / alias / seam notes.
#include "hit_detection.h"

#include <string.h>

#include "../fdlibm/fdlibm.h"
#include "action_state_shortcuts.h" // as_turnOffHitboxes
#include "environmental_collision.h" // getSameAndOther (stage-damage arm)
#include "interpolated_collision.h"
#include "ml_events.h"
#include "ml_js.h"
#include "ml_tables.h" // charAttributes.weight (M1 CTAB1 data path)
#include "util/lin_alg.h" // euclideanDist

// const angleConversion = Math.PI / 180 (:22)
static const double angleConversion = 3.141592653589793 / 180.0;

// canonical NaN for ToNumber(undefined) results (prevention rule 2)
static inline double hd_nan(void) {
  return js_nan();
}

// --- small state helpers ------------------------------------------------------

static inline int slot(double i) { return (int)i; }

static MlPlayer *P(MlSim *S, double i) {
  const int k = slot(i);
  if (k < 0 || k > 3 || !S->playerPresent[k]) {
    // upstream: player[<absent>] is null/undefined — the field deref throws
    ml_hd_out_of_domain("player deref on absent slot");
  }
  return &S->player[k];
}

static const ml_attributes_t *attr(double charId) {
  const int c = (int)charId;
  if (c < 0 || c >= ML_CHARS) ml_hd_out_of_domain("charAttributes char id");
  return &ml_attributes[c];
}

static const HdFlags *FLAGS(MlSim *S, double who) {
  return hd_flags(S->characterSelections[slot(who)], P(S, who)->actionState);
}

// --- MlHitboxSpec read helpers (the two measured shapes; header note) ----------
// CONSTRUCTOR-shape entries LACK clank/hitGrounded/hitAirborne/throwextra:
// upstream reads give undefined — falsy in truthiness, never loose-equal to
// a number, undefined != 6 is TRUE.

static inline bool hb_clank_eq(const MlHitboxSpec *hb, double v) {
  return hb->shape == ML_HB_CHARDATA && hb->clank == v;
}
static inline bool hb_clank_neq6(const MlHitboxSpec *hb) {
  return hb->shape == ML_HB_CHARDATA ? hb->clank != 6 : true; // undef != 6
}
static inline bool hb_hitGrounded(const MlHitboxSpec *hb) {
  // truthiness of a number | undefined
  return hb->shape == ML_HB_CHARDATA &&
         hb->hitGrounded != 0 && hb->hitGrounded == hb->hitGrounded;
}
static inline bool hb_hitAirborne(const MlHitboxSpec *hb) {
  return hb->shape == ML_HB_CHARDATA &&
         hb->hitAirborne != 0 && hb->hitAirborne == hb->hitAirborne;
}
static inline bool hb_throwextra(const MlHitboxSpec *hb) {
  return hb->shape == ML_HB_CHARDATA && hb->throwextra;
}
// offset[idx]: present only on CHARDATA entries with idx < offsetLen —
// otherwise upstream reads undefined (the caller decides guard vs throw).
static inline bool hb_offset_at(const MlHitboxSpec *hb, double idx,
                                Vec2D *out) {
  if (hb->shape != ML_HB_CHARDATA) return false; // Vec2D[0|1] is undefined
  const int k = (int)idx;
  if (idx != (double)k || k < 0 || k >= hb->offsetLen) return false;
  *out = hb->offsetArr[k];
  return true;
}
static Vec2D hb_offset_req(const MlHitboxSpec *hb, double idx,
                           const char *site) {
  Vec2D v;
  if (!hb_offset_at(hb, idx, &v)) {
    // upstream: undefined.x throws — trap at the exact dereference
    ml_hd_out_of_domain(site);
  }
  return v;
}

// --- hitQueue / phantomQueue ops ------------------------------------------------

static HdRow *hq_push(HdQueues *q) {
  if (q->hqCount >= HD_HQ_CAP) ml_hd_out_of_domain("hitQueue overflow");
  HdRow *r = &q->hq[q->hqCount++];
  memset(r, 0, sizeof *r);
  return r;
}

static void phq_push(HdQueues *q, double a, double v) {
  if (q->phqCount >= HD_PHQ_CAP) ml_hd_out_of_domain("phantomQueue overflow");
  q->phq[q->phqCount][0] = a;
  q->phq[q->phqCount][1] = v;
  q->phqCount++;
}

// --- hitList writes (alias site: prevFrameHitboxes.hitList, rule 10) ------------

static void hitlist_push(MlSim *S, double p, double val) {
  MlPlayer *pl = P(S, p);
  MlHitboxes *hb = &pl->hitboxes;
  if (hb->hitListLen >= ML_HITLIST_CAP) ml_hd_out_of_domain("hitList overflow");
  hb->hitList[hb->hitListLen++] = val;
  if (S->aliasHbHitList[slot(p)]) {
    // the push writes THROUGH the live alias: prevFrameHitboxes.hitList
    // is the same array object upstream
    MlHitboxes *ph = &pl->phys.prevFrameHitboxes;
    if (ph->hitListLen >= ML_HITLIST_CAP) {
      ml_hd_out_of_domain("aliased hitList overflow");
    }
    ph->hitList[ph->hitListLen++] = val;
  }
}

static void hitlist_splice1(MlSim *S, double a, int j) {
  MlPlayer *pl = P(S, a);
  MlHitboxes *hb = &pl->hitboxes;
  for (int k = j; k + 1 < hb->hitListLen; k++) {
    hb->hitList[k] = hb->hitList[k + 1];
  }
  hb->hitListLen--;
  if (S->aliasHbHitList[slot(a)]) {
    MlHitboxes *ph = &pl->phys.prevFrameHitboxes;
    for (int k = j; k + 1 < ph->hitListLen; k++) {
      ph->hitList[k] = ph->hitList[k + 1];
    }
    ph->hitListLen--;
  }
}

// turnOffHitboxes assigns FRESH active + hitList arrays (actionState-
// Shortcuts.js) — breaks both aliases (physics.c models the same way).
static void turnoff(MlSim *S, double i) {
  as_turnOffHitboxes(&P(S, i)->hitboxes);
  S->aliasHbActive[slot(i)] = false;
  S->aliasHbHitList[slot(i)] = false;
}

// --- dispatch conveniences ------------------------------------------------------

static void dsp0(MlSim *S, HdQueues *q, const char *phase, const char *state,
                 double charId, double slotArg) {
  HdDispCall c;
  c.phase = phase; c.state = state; c.charId = charId; c.slot = slotArg;
  c.extraCount = 0;
  hd_dispatch(S, q, &c);
}
static void dsp_bool(MlSim *S, HdQueues *q, const char *phase,
                     const char *state, double charId, double slotArg,
                     bool b) {
  HdDispCall c;
  c.phase = phase; c.state = state; c.charId = charId; c.slot = slotArg;
  c.extraCount = 1;
  c.extras[0].kind = DX_BOOL; c.extras[0].b = b;
  hd_dispatch(S, q, &c);
}

// --- module state setters (:15-21) ------------------------------------------------

// A45 T6. See hit_detection.h for why this exists and why it is here.
void hd_route_stage_damage(MlSim *S, HdQueues *q) {
  for (int k = 0; k < S->hqCount; k++) {
    const MlHqRow *src = &S->hq[k];
    if (q->hqCount >= HD_HQ_CAP) ml_hd_out_of_domain("hitQueue overflow (stage damage)");
    HdRow *r = &q->hq[q->hqCount++];
    memset(r, 0, sizeof *r);
    r->v = src->i;          // row[0]
    r->aIsObj = true;       // row[1] is {normal, angular, corner}
    r->normal = src->normal;
    r->angular = src->angular;
    r->corner = src->corner;
    r->h = src->damageTypeIndex; // row[2]
    r->shieldHit = false;        // row[3]
    r->isThrow = false;          // row[4]
    r->drawBounce = true;        // row[5]
    // six elements: no row[6], so hasPhantom/phantom stay false
  }
  S->hqCount = 0; // physics' queue is a HANDOFF buffer, never a history
}

void hd_resetHitQueue(HdQueues *q) {
  q->hqCount = 0; // hitQueue = []
}

void hd_setPhantonQueue(HdQueues *q, double rows[][2], int count) {
  if (count > HD_PHQ_CAP) ml_hd_out_of_domain("setPhantonQueue overflow");
  for (int i = 0; i < count; i++) {
    q->phq[i][0] = rows[i][0];
    q->phq[i][1] = rows[i][1];
  }
  q->phqCount = count;
}

// --- the JS-value `type` parameter (strict switch + loose ==) ---------------------
// executeRegularHit's phantom arm calls hitEffectsAndSound with SWAPPED
// arguments (:524 — upstream jank, carried verbatim): the `type` slot
// receives a BOOLEAN. JS switch matches strictly (a bool matches no
// numeric case); `type == 4` is loose (true->1, false->0; neither is 4).

typedef struct { bool isBool; bool b; double num; } HdTypeVal;

static inline HdTypeVal tv_num(double n) {
  HdTypeVal t; t.isBool = false; t.b = false; t.num = n; return t;
}
static inline HdTypeVal tv_bool(bool b) {
  HdTypeVal t; t.isBool = true; t.b = b; t.num = 0; return t;
}
static inline bool tv_strict_eq(HdTypeVal t, double n) {
  return !t.isBool && t.num == n;
}
static inline bool tv_loose_eq(HdTypeVal t, double n) {
  return (t.isBool ? (t.b ? 1.0 : 0.0) : t.num) == n;
}

// --- forward decls (upstream declaration order kept for the rest) -----------------

static void setHasHit(MlSim *S, double p, double j);
typedef struct { bool hit; bool hasPt; Vec2D pt; } HdHitPair;
static HdHitPair hitHitCollision(MlSim *S, double i, double p, double j,
                                 double k);
static HdHitPair interpolatedHitHitCollision(MlSim *S, double i, double p,
                                             double j, double k);
static bool hitShieldCollision(MlSim *S, double i, double p, double j,
                               bool previous);
// non-static since M4 task 11: targetplay.js imports this export
// (targetplay.js:27) — visibility-only change, body untouched; prototype
// in hit_detection.h.
static bool interpolatedHitHurtCollision(MlSim *S, double i, double p,
                                         double j, bool phantom);
static bool hitHurtCollision(MlSim *S, double i, double p, double j,
                             bool previous, bool phantom);
static void cssHits(MlSim *S, HdQueues *q);
static void executeShieldHit(MlSim *S, HdQueues *q, double v, double a,
                             double h, double damage);
static void bluntHit(MlSim *S, double a, double h);
static void executeRegularHit(MlSim *S, HdQueues *q, double v, const HdRow *row,
                              double h, bool shieldHit, bool isThrow,
                              bool drawBounce, bool phantom, bool stageDamage,
                              const MlHitboxSpec *hitbox);
static void hitEffectsAndSound(MlSim *S, double v, bool isThrow,
                               HdTypeVal type);
static void hitEffect(MlSim *S, HdTypeVal type, double v);
static void executeGrabHits(MlSim *S, HdQueues *q,
                            double grabQueue[][3], int grabQueueLen,
                            bool ignoreGrabs[4]);
static void executeGrabTech(MlSim *S, HdQueues *q, double a, double v);
static void knockbackSoundsInternal(HdTypeVal type, double knockback,
                                    double charId);

// --- hitDetect (:24) ---------------------------------------------------------------

void hd_hitDetect(MlSim *S, HdQueues *q, double p) {
  bool attackerClank = false; // function-scoped `var` (:25)
  for (int i = 0; i < 4; i++) {
    if (S->playerType[i] > -1) {
      if (i != p) {
        MlPlayer *pp = P(S, p);
        // check if victim is already in hitList (:30-36)
        bool inHitList = false;
        for (int k = 0; k < pp->hitboxes.hitListLen; k++) {
          if ((double)i == pp->hitboxes.hitList[k]) {
            inHitList = true;
            break;
          }
        }
        if (!inHitList) {
          double storedPhantom = -1;
          for (int j = 0; j < 4; j++) {
            bool interpolate;
            if (pp->hitboxes.active[j] &&
                pp->phys.prevFrameHitboxes.active[j]) {
              interpolate = true;
            } else {
              interpolate = false;
            }
            const MlHitboxSpec *hbj = &pp->hitboxes.id[j];
            if (pp->hitboxes.active[j] &&
                !(pp->phys.thrownHitbox &&
                  pp->phys.thrownHitboxOwner == (double)i) &&
                hbj->type != 7) {
              // clank == 6 means special clank (:49)
              if (hb_clank_eq(hbj, 1) ||
                  (hb_clank_eq(hbj, 2) && pp->phys.grounded) ||
                  hb_clank_eq(hbj, 6)) {
                MlPlayer *pi = P(S, (double)i);
                for (int k = 0; k < 4; k++) {
                  const MlHitboxSpec *hbk = &pi->hitboxes.id[k];
                  if (pi->hitboxes.active[k] &&
                      (hb_clank_eq(hbk, 1) ||
                       (hb_clank_eq(hbk, 2) && pi->phys.grounded) ||
                       (hb_clank_eq(hbj, 6) && hb_clank_neq6(hbk)))) {
                    const HdHitPair clankHit =
                        interpolate && pi->phys.prevFrameHitboxes.active[k]
                            ? interpolatedHitHitCollision(S, i, p, j, k)
                            : hitHitCollision(S, i, p, j, k);
                    if (clankHit.hit) {
                      const double diff = hbj->dmg - hbk->dmg;
                      if (hb_clank_eq(hbj, 6)) {
                        attackerClank = true;
                        // drawVfx({name:"clank", pos:clankHit[1]})
                        // (hitDetection.js:64-67; M4 task 1)
                        ml_drawVfx_p("clank", clankHit.pt.x, clankHit.pt.y);
                        pp->phys.hurtBoxState = 1;
                        pp->phys.intangibleTimer = 1;
                        // double check still in action state (:71)
                        if (FLAGS(S, p)->specialClank) {
                          dsp0(S, q, "onClank", pp->actionState,
                               S->characterSelections[slot(p)], p);
                          pp = P(S, p); // resync invalidates nothing (by-value)
                        }
                      } else {
                        if (diff >= 9) {
                          // victim clank; attacker cut through (:76-80)
                          pi->hit.hitlag =
                              floor(hbj->dmg * (1.0 / 3.0) + 3);
                          turnoff(S, (double)i);
                          dsp0(S, q, "init", "CATCHCUT",
                               S->characterSelections[i], (double)i);
                        } else if (diff <= -9) {
                          // attacker clank; victim cut through (:81-87)
                          pp->hit.hitlag =
                              floor(hbk->dmg * (1.0 / 3.0) + 3);
                          attackerClank = true;
                          turnoff(S, p);
                          dsp0(S, q, "init", "CATCHCUT",
                               S->characterSelections[slot(p)], p);
                        } else {
                          // both clank (:88-97)
                          pi->hit.hitlag =
                              floor(hbj->dmg * (1.0 / 3.0) + 3);
                          pp->hit.hitlag =
                              floor(hbk->dmg * (1.0 / 3.0) + 3);
                          attackerClank = true;
                          turnoff(S, (double)i);
                          dsp0(S, q, "init", "CATCHCUT",
                               S->characterSelections[i], (double)i);
                          turnoff(S, p);
                          dsp0(S, q, "init", "CATCHCUT",
                               S->characterSelections[slot(p)], p);
                        }
                        ml_sound_play("clank");
                        // drawVfx({name:"clank", pos:clankHit[1]})
                        // (hitDetection.js:99-102; M4 task 1)
                        ml_drawVfx_p("clank", clankHit.pt.x, clankHit.pt.y);
                        hitlist_push(S, p, (double)i);
                        pp->hasHit = true;
                      }
                      break;
                    }
                  }
                }
              }
              if (!attackerClank) {
                MlPlayer *pi = P(S, (double)i);
                if (pi->phys.shielding && hb_hitGrounded(hbj) &&
                    (hitShieldCollision(S, i, p, j, false) ||
                     (interpolate &&
                      (hitShieldCollision(S, i, p, j, true) ||
                       interpolatedHitCircleCollision(
                           S, pi->phys.shieldPositionReal,
                           pi->phys.shieldSize, p, j))))) {
                  // hitQueue.push([i, p, j, true, false, false]) (:116)
                  HdRow *r = hq_push(q);
                  r->v = (double)i; r->a = p; r->h = (double)j;
                  r->shieldHit = true; r->isThrow = false;
                  r->drawBounce = false; r->hasPhantom = false;
                  hitlist_push(S, p, (double)i);
                  setHasHit(S, p, (double)j);
                  break;
                } else if (pi->phys.hurtBoxState != 1) {
                  if ((hb_hitGrounded(hbj) && pi->phys.grounded) ||
                      (hb_hitAirborne(hbj) && !pi->phys.grounded))
                    if (hitHurtCollision(S, i, p, j, false, false) ||
                        (interpolate &&
                         (interpolatedHitHurtCollision(S, i, p, j, false) ||
                          hitHurtCollision(S, i, p, j, true, false)))) {
                      if (!hitHurtCollision(S, i, p, j, false, true) &&
                          (interpolate
                               ? !interpolatedHitHurtCollision(S, i, p, j, true)
                               : true)) {
                        storedPhantom = (double)j;
                      } else {
                        // hitQueue.push([i,p,j,false,false,false,false])
                        HdRow *r = hq_push(q);
                        r->v = (double)i; r->a = p; r->h = (double)j;
                        r->shieldHit = false; r->isThrow = false;
                        r->drawBounce = false;
                        r->hasPhantom = true; r->phantom = false;
                        // upstream's triple existence guard (:131-133) is
                        // always-true in the domain: hitboxes.hitList is an
                        // Array on every player
                        hitlist_push(S, p, (double)i);
                        setHasHit(S, p, (double)j);
                        break;
                      }
                    }
                }
              }
            }
            if (storedPhantom > -1) {
              // NOTE upstream never resets storedPhantom: every remaining
              // j iteration re-pushes the phantom row (:142-146, verbatim)
              HdRow *r = hq_push(q);
              r->v = (double)i; r->a = p; r->h = storedPhantom;
              r->shieldHit = false; r->isThrow = false; r->drawBounce = false;
              r->hasPhantom = true; r->phantom = true;
              hitlist_push(S, p, (double)i);
              setHasHit(S, p, storedPhantom);
            }
          }
        }
      }
    }
  }
}

// --- setHasHit (:155) ----------------------------------------------------------------

static void setHasHit(MlSim *S, double p, double j) {
  MlPlayer *pp = P(S, p);
  const MlHitboxSpec *hb = &pp->hitboxes.id[slot(j)];
  // for turbo mode. if not a grab and not counter and not a midthrow hitbox.
  if (hb->type != 2 && hb->type != 6 &&
      strncmp(pp->actionState, "THROW", 5) != 0) {
    pp->hasHit = true;
  }
}

// --- hitHitCollision (:163) ------------------------------------------------------------

static HdHitPair hitHitCollision(MlSim *S, double i, double p, double j,
                                 double k) {
  MlPlayer *pp = P(S, p);
  MlPlayer *pi = P(S, i);
  double framePos1 = pp->hitboxes.frame;
  if (framePos1 > 1) {
    framePos1 = 1;
  }
  double framePos2 = pi->hitboxes.frame;
  if (framePos2 > 1) {
    framePos2 = 1;
  }
  const MlHitboxSpec *hbj = &pp->hitboxes.id[slot(j)];
  const MlHitboxSpec *hbk = &pi->hitboxes.id[slot(k)];
  const Vec2D oj = hb_offset_req(hbj, framePos1, "hitHitCollision offset j");
  const Vec2D ok = hb_offset_req(hbk, framePos2, "hitHitCollision offset k");
  const Vec2D hbpos = vec2d(pp->phys.pos.x + (oj.x * pp->phys.face),
                            pp->phys.pos.y + oj.y);
  const Vec2D hbpos2 = vec2d(pi->phys.pos.x + (ok.x * pi->phys.face),
                             pi->phys.pos.y + ok.y);
  const Vec2D hitPoint =
      vec2d((hbpos.x + hbpos2.x) / 2, (hbpos.y + hbpos2.y) / 2);
  HdHitPair out;
  out.hit = fd_pow(hbpos2.x - hbpos.x, 2) + fd_pow(hbpos.y - hbpos2.y, 2) <=
            fd_pow(hbj->size + hbk->size, 2);
  out.hasPt = true;
  out.pt = hitPoint;
  return out;
}

// --- interpolatedHitHitCollision (:186) --------------------------------------------------

static HdHitPair interpolatedHitHitCollision(MlSim *S, double i, double p,
                                             double j, double k) {
  MlPlayer *pp = P(S, p);
  MlPlayer *pi = P(S, i);
  // NOTE: frame indices are NOT clamped here (unlike hitHitCollision), and
  // h2i mixes player[p].phys.face into player[i]'s x — upstream verbatim.
  const Vec2D pj_prev =
      hb_offset_req(&pp->phys.prevFrameHitboxes.id[slot(j)],
                    pp->phys.prevFrameHitboxes.frame, "iHHC offset pj prev");
  const Vec2D pj_cur = hb_offset_req(&pp->hitboxes.id[slot(j)],
                                     pp->hitboxes.frame, "iHHC offset pj cur");
  const Vec2D ik_prev =
      hb_offset_req(&pi->phys.prevFrameHitboxes.id[slot(k)],
                    pi->phys.prevFrameHitboxes.frame, "iHHC offset ik prev");
  const Vec2D ik_cur = hb_offset_req(&pi->hitboxes.id[slot(k)],
                                     pi->hitboxes.frame, "iHHC offset ik cur");
  const Vec2D h1p = vec2d(pp->phys.posPrev.x + (pj_prev.x * pp->phys.facePrev),
                          pp->phys.posPrev.y + pj_prev.y);
  const Vec2D h2p = vec2d(pp->phys.pos.x + (pj_cur.x * pp->phys.face),
                          pp->phys.pos.y + pj_cur.y);
  const Vec2D h1i = vec2d(pi->phys.posPrev.x + (ik_prev.x * pi->phys.facePrev),
                          pi->phys.posPrev.y + ik_prev.y);
  const Vec2D h2i = vec2d(pi->phys.pos.x + (ik_cur.x * pp->phys.face),
                          pi->phys.pos.y + ik_cur.y);
  const double r = pp->hitboxes.id[slot(j)].size;
  const double s = pi->hitboxes.id[slot(k)].size;

  const MaybeVec2D collision =
      sweepCircleVsSweepCircle(h1p, r, h2p, r, h1i, s, h2i, s);

  HdHitPair out;
  if (!collision.present) {
    out.hit = false;
    out.hasPt = false;
    out.pt = vec2d(0, 0);
  } else {
    out.hit = true;
    out.hasPt = true;
    out.pt = collision.v;
  }
  return out;
}

// --- hitShieldCollision (:209) ------------------------------------------------------------

static bool hitShieldCollision(MlSim *S, double i, double p, double j,
                               bool previous) {
  MlPlayer *pp = P(S, p);
  MlPlayer *pi = P(S, i);
  Vec2D hbpos;
  if (previous) {
    double checkPreviousFrame = pp->phys.prevFrameHitboxes.frame;
    if (checkPreviousFrame > 1) {
      checkPreviousFrame = 1;
    }
    const Vec2D o = hb_offset_req(&pp->phys.prevFrameHitboxes.id[slot(j)],
                                  checkPreviousFrame, "hSC offset prev");
    hbpos = vec2d(pp->phys.posPrev.x + (o.x * pp->phys.facePrev),
                  pp->phys.posPrev.y + o.y);
  } else {
    double checkFrame = pp->hitboxes.frame;
    if (checkFrame > 1) {
      checkFrame = 1;
    }
    const Vec2D o = hb_offset_req(&pp->hitboxes.id[slot(j)], checkFrame,
                                  "hSC offset cur");
    hbpos = vec2d(pp->phys.pos.x + (o.x * pp->phys.face),
                  pp->phys.pos.y + o.y);
  }
  const Vec2D shieldpos = pi->phys.shieldPositionReal;

  return fd_pow(shieldpos.x - hbpos.x, 2) + fd_pow(hbpos.y - shieldpos.y, 2) <=
         fd_pow(pp->hitboxes.id[slot(j)].size + pi->phys.shieldSize, 2);
}

// --- interpolatedHitCircleCollision (:230) ---------------------------------------------------
// (exported since M4 task 11 — targetplay.js:27 imports it; body untouched)

bool interpolatedHitCircleCollision(MlSim *S, Vec2D circlePos, double r,
                                    double p, double j) {
  MlPlayer *pp = P(S, p);
  double prevPosFrame = pp->phys.prevFrameHitboxes.frame;
  if (prevPosFrame > 1) {
    prevPosFrame = 1;
  }
  double posFrame = pp->hitboxes.frame;
  if (posFrame > 1) {
    posFrame = 1;
  }
  const Vec2D op = hb_offset_req(&pp->phys.prevFrameHitboxes.id[slot(j)],
                                 prevPosFrame, "iHCC offset prev");
  const Vec2D oc =
      hb_offset_req(&pp->hitboxes.id[slot(j)], posFrame, "iHCC offset cur");
  const Vec2D h1 = vec2d(pp->phys.posPrev.x + (op.x * pp->phys.facePrev),
                         pp->phys.posPrev.y + op.y);
  const Vec2D h2 = vec2d(pp->phys.pos.x + (oc.x * pp->phys.face),
                         pp->phys.pos.y + oc.y);
  const double s = pp->hitboxes.id[slot(j)].size;
  const MaybeVec2D collision =
      sweepCircleVsSweepCircle(h1, s, h2, s, circlePos, r, circlePos, r);

  if (!collision.present) {
    return false;
  } else {
    return true;
  }
}

// --- segmentSegmentCollision (:255, exported; live caller article.js) -------------------------

bool hd_segmentSegmentCollision(Vec2D a1, Vec2D a2, Vec2D b1, Vec2D b2) {
  const Vec2D b = vec2d(a2.x - a1.x, a2.y - a1.y);
  const Vec2D d = vec2d(b2.x - b1.x, b2.y - b1.y);
  const double bDotDPerp = b.x * d.y - b.y * d.x;
  // parallel lines: infinite intersection points (:261)
  if (bDotDPerp == 0) {
    return false;
  }
  const Vec2D c = vec2d(b1.x - a1.x, b1.y - a1.y);
  const double t = (c.x * d.y - c.y * d.x) / bDotDPerp;
  if (t < 0 || t > 1) {
    return false;
  }
  const double u = (c.x * b.y - c.y * b.x) / bDotDPerp;
  if (u < 0 || u > 1) {
    return false;
  }
  // `intersection` (:273) is computed upstream but unused/discarded
  return true;
}

// --- interpolatedHitHurtCollision (:277) --------------------------------------------------------

static bool interpolatedHitHurtCollision(MlSim *S, double i, double p,
                                         double j, bool phantom) {
  MlPlayer *pp = P(S, p);
  MlPlayer *pi = P(S, i);
  const Box2D hurt = pi->phys.hurtbox;
  // NOTE upstream INVERSION carried verbatim (:281-285): phantom reads
  // interPolatedHitbox, non-phantom reads interPolatedHitboxPhantom.
  const Vec2D *hb;
  if (phantom) {
    if (slot(j) >= pp->phys.interPolatedHitboxLen) {
      ml_hd_out_of_domain("interPolatedHitbox[j] missing"); // undefined[0]
    }
    hb = pp->phys.interPolatedHitbox[slot(j)];
  } else {
    if (slot(j) >= pp->phys.interPolatedHitboxPhantomLen) {
      ml_hd_out_of_domain("interPolatedHitboxPhantom[j] missing");
    }
    hb = pp->phys.interPolatedHitboxPhantom[slot(j)];
  }

  const Vec2D h1 =
      vec2d(0.5 * hb[0].x + 0.5 * hb[3].x, 0.5 * hb[0].y + 0.5 * hb[3].y);
  const Vec2D h2 =
      vec2d(0.5 * hb[1].x + 0.5 * hb[2].x, 0.5 * hb[1].y + 0.5 * hb[2].y);
  const double r = 0.5 * euclideanDist(hb[0], hb[3]);

  const MaybeVec2D collision =
      sweepCircleVsAABB(h1, r, h2, r, hurt.min, hurt.max);

  if (!collision.present) {
    return false;
  } else {
    return true;
  }
}

// --- hitHurtCollision (:301) --------------------------------------------------------------------

static bool hitHurtCollision(MlSim *S, double i, double p, double j,
                             bool previous, bool phantom) {
  MlPlayer *pp = P(S, p);
  MlPlayer *pi = P(S, i);
  double playerframe = pp->hitboxes.frame;
  if (playerframe > 1) {
    playerframe = 1;
  }
  Vec2D offset;
  if (!hb_offset_at(&pp->hitboxes.id[slot(j)], playerframe, &offset)) {
    return false; // offset === undefined (:308)
  }
  if (strcmp(pp->actionState, "DAMAGEFLYN") == 0) {
    offset = hb_offset_req(&pp->hitboxes.id[slot(j)], 0,
                           "hHC DAMAGEFLYN offset[0]");
  }
  Vec2D hbpos;
  if (previous) {
    double prevframe = pp->phys.prevFrameHitboxes.frame;
    if (prevframe > 1) {
      prevframe = 1;
    }
    Vec2D prevoffset;
    if (!hb_offset_at(&pp->phys.prevFrameHitboxes.id[slot(j)], prevframe,
                      &prevoffset)) {
      return false; // prevoffset === undefined (:320)
    }
    hbpos = vec2d(pp->phys.posPrev.x + (prevoffset.x * pp->phys.facePrev),
                  pp->phys.posPrev.y + prevoffset.y);
  } else {
    hbpos = vec2d(pp->phys.pos.x + (offset.x * pp->phys.face),
                  pp->phys.pos.y + offset.y);
  }
  const Vec2D hurtCenter =
      vec2d((pi->phys.hurtbox.min.x + pi->phys.hurtbox.max.x) / 2,
            (pi->phys.hurtbox.min.y + pi->phys.hurtbox.max.y) / 2);

  const Vec2D distance =
      vec2d(js_abs(hbpos.x - hurtCenter.x), js_abs(hbpos.y - hurtCenter.y));

  const double hurtWidth = 8;
  const double hurtHeight = 18;
  const double size = pp->hitboxes.id[slot(j)].size;
  const double phantomThreshold = S->phantomThreshold;

  if (distance.x > (hurtWidth / 2 + size - (phantom ? phantomThreshold : 0))) {
    return false;
  }
  if (distance.y > (hurtHeight / 2 + size - (phantom ? phantomThreshold : 0))) {
    return false;
  }

  if (distance.x <= (hurtWidth / 2)) {
    return true;
  }
  if (distance.y <= (hurtHeight / 2)) {
    return true;
  }

  const double cornerDistance_sq = fd_pow(distance.x - hurtWidth / 2, 2) +
                                   fd_pow(distance.y - hurtHeight / 2, 2);

  return cornerDistance_sq <=
         fd_pow(size - (phantom ? phantomThreshold : 0), 2);
}

// --- cssHits (:352; gameMode 2 — ZERO-LIVE in-match, translated verbatim) -----------------------

static void cssHits(MlSim *S, HdQueues *q) {
  for (int i = 0; i < q->hqCount; i++) {
    const HdRow *row = &q->hq[i];
    const double v = row->v;
    if (v == -1) { // `v === -1` (:356)
      continue;
    }
    if (row->aIsObj) ml_hd_out_of_domain("cssHits stage row");
    const double a = row->a;
    const double h = row->h;
    const bool shieldHit = row->shieldHit;
    // upstream reads player[i] with the QUEUE index (:365 — jank kept):
    double frame = P(S, (double)i)->hitboxes.frame;
    if (frame > 1) {
      frame = 1;
    }
    (void)frame; // read again only on the trapped rpsPoints arm below
    const MlHitboxSpec *hb = &P(S, a)->hitboxes.id[slot(h)];
    const double damage = hb->dmg;

    if (shieldHit) {
      ml_sound_play("blunthit");
      P(S, v)->hit.hitlag = floor(damage * (1.0 / 3.0) + 3);
      if (P(S, v)->phys.powerShieldActive) {
        P(S, v)->phys.powerShielded = true;
        P(S, v)->hit.powershield = true;
        // drawVfx impactLand + powershield (hitDetection.js:377-386;
        // M4 task 1)
        ml_drawVfx("impactLand", P(S, v)->phys.pos.x, P(S, v)->phys.pos.y,
                   P(S, v)->phys.face);
        ml_drawVfx("powershield", P(S, v)->phys.shieldPositionReal.x,
                   P(S, v)->phys.shieldPositionReal.y, P(S, v)->phys.face);
        ml_sound_play("powershield");
      }
      P(S, v)->hit.shieldstun =
          ((floor(damage) *
            ((0.65 * (1 - ((P(S, v)->phys.shieldAnalog - 0.3) / 0.7))) +
             0.3)) *
           1.5) +
          2;
    } else {
      // player[a].rpsPoints++ (:392): a runtime-created NaN field outside
      // the player value model — the M4 css cluster owns this surface.
      ml_hd_out_of_domain("cssHits rpsPoints");
    }
  }
}

// --- executeShieldHit (:411) ----------------------------------------------------------------------

static void executeShieldHit(MlSim *S, HdQueues *q, double v, double a,
                             double h, double damage) {
  MlPlayer *pv = P(S, v);
  if (!pv->phys.powerShieldActive) {
    pv->phys.shieldHP -= damage;
    if (pv->phys.shieldHP < 0) {
      pv->phys.shielding = false;
      pv->phys.cVel.y = 2.5;
      pv->phys.grounded = false;
      pv->phys.shieldHP = 0;
      // drawVfx({name:"breakShield", pos:phys.pos, face:phys.face})
      // (hitDetection.js:419-423; M4 task 1)
      ml_drawVfx("breakShield", pv->phys.pos.x, pv->phys.pos.y,
                 pv->phys.face);
      dsp0(S, q, "init", "SHIELDBREAKFALL", S->characterSelections[slot(v)],
           v);
      ml_sound_play("shieldbreak");
      return;
    }
  }
  pv->hit.hitlag = floor(damage * (1.0 / 3.0) + 3);

  double vPushMultiplier = 0.6;
  if (pv->phys.powerShieldActive) {
    vPushMultiplier = 1;
    pv->phys.powerShielded = true;
    pv->hit.powershield = true;
    // drawVfx impactLand + powershield (hitDetection.js:436-445;
    // M4 task 1)
    ml_drawVfx("impactLand", pv->phys.pos.x, pv->phys.pos.y, pv->phys.face);
    ml_drawVfx("powershield", pv->phys.shieldPositionReal.x,
               pv->phys.shieldPositionReal.y, pv->phys.face);
    ml_sound_play("powershield");
  } else {
    // `let frame` (:448-451) computed upstream but unused; the clank vfx
    // (:452-455) dereferences offset[player[a].hitboxes.frame] UNCLAMPED —
    // trap exactly where undefined.x would throw (render-only otherwise).
    Vec2D o;
    if (!hb_offset_at(&P(S, a)->hitboxes.id[slot(h)],
                      P(S, a)->hitboxes.frame, &o)) {
      ml_hd_out_of_domain("executeShieldHit clank vfx offset");
    }
    // drawVfx({name:"clank", pos:new Vec2D(pos.x + offset.x*face,
    //          pos.y + offset.y)}) (hitDetection.js:452-455; M4 task 1)
    ml_drawVfx_p("clank",
                 P(S, a)->phys.pos.x + (o.x * P(S, a)->phys.face),
                 P(S, a)->phys.pos.y + o.y);
  }
  pv->hit.shieldstun =
      ((floor(damage) *
        ((0.65 * (1 - ((pv->phys.shieldAnalog - 0.3) / 0.7))) + 0.3)) *
       1.5) +
      2;
  double victimPush =
      ((floor(damage) *
        ((0.195 * (1 - ((pv->phys.shieldAnalog - 0.3) / 0.7))) + 0.09)) +
       0.4) *
      vPushMultiplier;
  if (victimPush > 2) {
    victimPush = 2;
  }
  const double attackerPush =
      (floor(damage) * ((pv->phys.shieldAnalog - 0.3) * 0.1)) + 0.02;

  if (P(S, a)->phys.pos.x < pv->phys.pos.x) {
    pv->phys.cVel.x = victimPush;
    P(S, a)->phys.cVel.x -= attackerPush;
  } else {
    pv->phys.cVel.x = -victimPush;
    P(S, a)->phys.cVel.x += attackerPush;
  }

  dsp0(S, q, "init", "GUARD", S->characterSelections[slot(v)], v);
}

// --- bluntHit (:476; unreachable — executeRegularHit's caller arm reads the
// hurtboxState TYPO key; translated for structure parallelism) ----------------------------------

static void bluntHit(MlSim *S, double a, double h) {
  (void)S; (void)a; (void)h;
  ml_sound_play("blunthit");
  // frame clamp (:478-481) then drawVfx("clank", Vec2D) — the STRING-arg
  // call (:482): drawVfx reads "clank".pos.x -> undefined.x THROWS
  // upstream. Trap mirrors the throw.
  ml_hd_out_of_domain("bluntHit drawVfx string-arg throw");
}

// --- executeRegularHit (:485) -----------------------------------------------------------------------

static void executeRegularHit(MlSim *S, HdQueues *q, double v, const HdRow *row,
                              double h, bool shieldHit, bool isThrow,
                              bool drawBounce, bool phantom, bool stageDamage,
                              const MlHitboxSpec *hitbox) {
  MlPlayer *pv = P(S, v);
  double damage = hitbox->dmg;
  pv->phys.hasGrabTech = true;
  pv->phys.grabTech = false;
  if (!stageDamage) {
    const double a = row->a;
    MlPlayer *pa = P(S, a);
    if (pa->phys.chargeFrames > 0) {
      damage *= 1 + (pa->phys.chargeFrames * (0.3671 / 60));
    }
    if (FLAGS(S, a)->specialOnHit) {
      dsp0(S, q, "onPlayerHit", P(S, a)->actionState,
           S->characterSelections[slot(a)], a);
      pv = P(S, v);
      pa = P(S, a);
      if (hitbox->type == 8) return; // strict === on the number
    }
    if (phantom) {
      phq_push(q, a, v);
      pv->phys.hasPhantomDamage = true;
      pv->phys.phantomDamage = 0.5 * damage;
    } else {
      pa->hit.hitlag = floor(damage * (1.0 / 3.0) + 3);
    }
  }
  // TODO upstream: STALING + KNOCKBACK STACKING (:503)

  if (shieldHit) {
    executeShieldHit(S, q, v, row->a, h, damage);
    return;
  }
  // if invincible (:510): upstream reads phys.hurtboxState — a TYPO for
  // hurtBoxState; the lowercase key never exists, undefined > 0 is false:
  // dead arm (bluntHit above stays unreachable from here).
  if (false && !isThrow) {
    if (!stageDamage) {
      bluntHit(S, row->a, h);
    }
    return;
  }
  if (phantom) {
    MlPlayer *pa = P(S, row->a);
    pv->hit.hitlag = floor(damage * (1.0 / 3.0) + 3);
    pv->hit.knockback = 0;
    double frame = pa->hitboxes.frame;
    if (frame > 1) {
      frame = 1;
    }
    const Vec2D o = hb_offset_req(hitbox, frame, "phantom hitPoint offset");
    pv->hit.hitPoint = vec2d(pa->phys.pos.x + (o.x * pa->phys.face),
                             pa->phys.pos.y + o.y);
    // upstream jank (:524, verbatim): hitEffectsAndSound(a, v, h, isThrow)
    // — the args land as (v=a, h=v[unused], isThrow=h, type=isThrow):
    hitEffectsAndSound(S, row->a, h != 0 && h == h, tv_bool(isThrow));
    return;
  }

  const bool crouching = FLAGS(S, v)->crouch;
  bool vCancel = false;
  if (pv->phys.vCancelTimer > 0) {
    if (FLAGS(S, v)->vCancel) {
      vCancel = true;
      ml_sound_play("vcancel");
    }
  }
  bool jabReset = false;
  if (FLAGS(S, v)->downed && damage < 7) {
    jabReset = true;
  }
  {
    HdKbSpec kb;
    kb.kg = hitbox->kg;
    kb.bk = hitbox->bk;
    kb.sk = hitbox->sk;
    pv->hit.knockback =
        hd_getKnockback(kb, damage, damage, pv->percent,
                        (double)attr(S->characterSelections[slot(v)])->weight,
                        crouching, vCancel);
  }
  pv->hit.angle = hitbox->angle;
  if (pv->hit.angle == 361) {
    if (pv->hit.knockback < 32.1) {
      pv->hit.angle = 0;
    } else if (pv->hit.knockback >= 32.1) {
      pv->hit.angle = 44;
    }
  }

  pv->hit.hitlag = floor(damage * (1.0 / 3.0) + 3);

  if (!isThrow) {
    if (stageDamage) {
      // stage-damage collision point from the row's collisionData
      // (ZERO-LIVE on VS stages — M4 target stages; physics.c note).
      const double angularParameter = row->angular;
      Vec2D collisionPoint;
      if (row->corner) {
        const SameOther so = getSameAndOther(angularParameter);
        const double same = so.same, other = so.other;
        const double t = angularParameter - floor(angularParameter);
        // ECBp components can be rule-8 undef-at-rest — NaN carries.
        if ((same == 1 && other == 2) || (same == 3 && other == 0)) {
          collisionPoint =
              vec2d((1 - t) * pv->phys.ECBp[(int)same].x +
                        t * pv->phys.ECBp[(int)other].x,
                    (1 - t) * pv->phys.ECBp[(int)same].y +
                        t * pv->phys.ECBp[(int)other].y);
        } else {
          collisionPoint =
              vec2d((1 - t) * pv->phys.ECBp[(int)other].x +
                        t * pv->phys.ECBp[(int)same].x,
                    (1 - t) * pv->phys.ECBp[(int)other].y +
                        t * pv->phys.ECBp[(int)same].y);
        }
      } else {
        const int ap = (int)angularParameter;
        if (angularParameter != (double)ap || ap < 0 || ap > 3) {
          ml_hd_out_of_domain("stageDamage ECBp[angularParameter]");
        }
        collisionPoint = pv->phys.ECBp[ap];
      }
      pv->hit.hitPoint = collisionPoint;
      pv->hit.hasReverse = true;
      pv->hit.reverse = false;
      pv->phys.hasStageDamageImmunity = true;
      pv->phys.stageDamageImmunity = 20;
    } else {
      MlPlayer *pa = P(S, row->a);
      double frame = pa->hitboxes.frame;
      if (frame > 1) {
        frame = 1;
      }
      const Vec2D o = hb_offset_req(hitbox, frame, "hit hitPoint offset");
      pv->hit.hitPoint = vec2d(pa->phys.pos.x + (o.x * pa->phys.face),
                               pa->phys.pos.y + o.y);
      pv->hit.hasReverse = true;
      if (pa->phys.pos.x < pv->phys.pos.x) {
        pv->hit.reverse = false;
      } else {
        pv->hit.reverse = true;
      }
    }
    if (!jabReset && pv->phys.grabbedBy == -1) {
      pv->phys.face = pv->hit.reverse ? 1 : -1;
    }
  } else {
    MlPlayer *pa = P(S, row->a);
    pa->hasHit = true;
    pa->phys.grabbing = -1;
    pv->phys.thrownHitbox = true;
    pv->phys.thrownHitboxOwner = row->a;
    // `pos = new Vec2D(...)` — fresh object: breaks the pos-ECB1[0] alias.
    // hitbox.offset here is the SINGLE-Vec2D read (:594): thrown hitboxes
    // are CHARDATA entries with a SINGLE Vec2D offset (offsetSingle — the
    // M2 task 8 sub-shape, discovered AFTER this module's translation; the
    // arm was zero-live over g01/g04/g06 and the first LIVE throw — g08
    // frame 371, task 17 ledger — exposed the miss). Only a CHARDATA
    // per-frame ARRAY gives offset.x === undefined -> NaN components.
    {
      double ox, oy;
      if (hitbox->shape == ML_HB_CONSTRUCTOR ||
          (hitbox->shape == ML_HB_CHARDATA && hitbox->offsetSingle)) {
        ox = hitbox->offset.x;
        oy = hitbox->offset.y;
      } else {
        ox = hd_nan();
        oy = hd_nan();
      }
      pv->phys.pos = vec2d(pa->phys.pos.x + (ox * pa->phys.face),
                           pa->phys.pos.y + oy);
      S->aliasPosEcb1[slot(v)] = false;
    }
    pv->phys.grabbedBy = -1;
    pv->hit.hitlag = 1;
    pa->hit.hitlag = 1;
    pv->hit.hasReverse = true;
    if (pa->phys.face == 1) {
      pv->hit.reverse = false;
    } else {
      pv->hit.reverse = true;
    }
    if (drawBounce) {
      ml_sound_play("bounce");
      // drawVfx({name:"groundBounce", pos:phys.pos, face:phys.face,
      //          f:Math.PI/2}) (hitDetection.js:605-610 / :646-651;
      //          M4 task 1)
      ml_drawVfx_f("groundBounce", pv->phys.pos.x, pv->phys.pos.y,
                   pv->phys.face, js_pi() / 2);
    }
  }

  pv->percent += damage;

  // if victim is grabbing someone, put their grab victim into grab release
  if (pv->phys.grabbing > -1) {
    P(S, pv->phys.grabbing)->phys.grabbedBy = -1;
    dsp0(S, q, "init", "CAPTURECUT",
         S->characterSelections[slot(pv->phys.grabbing)], pv->phys.grabbing);
    pv = P(S, v);
  }

  if (pv->phys.grabbedBy == -1 ||
      (pv->phys.grabbedBy > -1 && pv->hit.knockback > 50 &&
       !hb_throwextra(hitbox))) {
    if (pv->phys.grabbedBy > -1) {
      P(S, pv->phys.grabbedBy)->phys.grabbing = -1;
      dsp0(S, q, "init", "WAIT",
           S->characterSelections[slot(pv->phys.grabbedBy)],
           pv->phys.grabbedBy);
      pv = P(S, v);
    }
    pv->hit.hitstun = hd_getHitstun(pv->hit.knockback);

    if (jabReset) {
      dsp0(S, q, "init", "DOWNDAMAGE", S->characterSelections[slot(v)], v);
    } else if (pv->hit.knockback >= 80 || isThrow) {
      dsp_bool(S, q, "init", "DAMAGEFLYN", S->characterSelections[slot(v)], v,
               !isThrow);
    } else {
      dsp0(S, q, "init", "DAMAGEN2", S->characterSelections[slot(v)], v);
    }
    pv = P(S, v);
  } else {
    if (!hb_throwextra(hitbox)) {
      dsp0(S, q, "init", "CAPTUREDAMAGE", S->characterSelections[slot(v)], v);
      pv = P(S, v);
    }
  }

  if (pv->phys.grounded && pv->hit.angle > 180) {
    if (pv->hit.knockback >= 80) {
      ml_sound_play("bounce");
      // drawVfx({name:"groundBounce", pos:phys.pos, face:phys.face,
      //          f:Math.PI/2}) (hitDetection.js:605-610 / :646-651;
      //          M4 task 1)
      ml_drawVfx_f("groundBounce", pv->phys.pos.x, pv->phys.pos.y,
                   pv->phys.face, js_pi() / 2);
      pv->hit.angle = 360 - pv->hit.angle;
      pv->hit.knockback *= 0.8;
    }
  }
  // screenShake(knockback) (:656): 4 seeded draws for the render shake —
  // the values are discarded, the STREAM position is sim state.
  (void)ml_random();
  (void)ml_random();
  (void)ml_random();
  (void)ml_random();
  // percentShake: native-RNG off-tick HUD effect (CHECKSUM.md section 7) — no-op
  hitEffectsAndSound(S, v, isThrow, tv_num(hitbox->type));
}

// --- hitEffectsAndSound (:661) — `h` param is unused upstream ------------------------------------

static void hitEffectsAndSound(MlSim *S, double v, bool isThrow,
                               HdTypeVal type) {
  if (!isThrow) {
    hitEffect(S, type, v);
    knockbackSoundsInternal(type, P(S, v)->hit.knockback,
                            S->characterSelections[slot(v)]);
  } else {
    ml_sound_play("stronghit");
  }
}

// --- hitEffect (:670) — strict switch; vfx are render-only -----------------------------------------

static void hitEffect(MlSim *S, HdTypeVal type, double v) {
  // M4 task 1: vfx configs emitted verbatim (hitDetection.js:670-718);
  // pos is the live hit.hitPoint reference — snapshot at call, like
  // upstream drawVfx's instance.newPos.
  MlPlayer *pv = P(S, v);
  if (tv_strict_eq(type, 0)) {
    // normal (:674-678)
    ml_drawVfx("normalhit", pv->hit.hitPoint.x, pv->hit.hitPoint.y,
               pv->phys.face);
  } else if (tv_strict_eq(type, 1)) {
    // slash (:682-697)
    ml_drawVfx("hitSparks", pv->hit.hitPoint.x, pv->hit.hitPoint.y,
               pv->phys.face);
    ml_drawVfx("hitFlair", pv->hit.hitPoint.x, pv->hit.hitPoint.y,
               pv->phys.face);
    ml_drawVfx_f("hitCurve", pv->hit.hitPoint.x, pv->hit.hitPoint.y,
                 pv->phys.face, pv->hit.angle);
  } else if (tv_strict_eq(type, 3)) {
    // fire (:702-706)
    pv->burning = 20;
    ml_drawVfx("firehit", pv->hit.hitPoint.x, pv->hit.hitPoint.y,
               pv->phys.face);
  } else if (tv_strict_eq(type, 4)) {
    // electric (:711-715)
    pv->shocked = 20;
    ml_drawVfx("electrichit", pv->hit.hitPoint.x, pv->hit.hitPoint.y,
               pv->phys.face);
  }
  // default: break
}

// --- executeHits (:722) -----------------------------------------------------------------------------

void hd_executeHits(MlSim *S, HdQueues *q) {
  if (S->gameMode == 2) { // strict === on the number
    cssHits(S, q);
    return;
  }
  double grabQueue[HD_GRABQ_CAP][3];
  int grabQueueLen = 0;
  bool ignoreGrabs[4] = {false, false, false, false};
  for (int i = 0; i < q->hqCount; i++) {
    const HdRow row = q->hq[i]; // row VALUES read up front (consts :731-738)
    const double v = row.v;
    const double h = row.h;
    const bool shieldHit = row.shieldHit;
    const bool isThrow = row.isThrow;
    const bool drawBounce = row.drawBounce;
    const bool phantom = row.hasPhantom ? row.phantom : false; // [6] || false
    // if a is a string(sic: object), it is stage damage: (a >= 0) is false
    const bool stageDamage = row.aIsObj ? true : !(row.a >= 0);
    MlHitboxSpec hbVal; // stage-damage synthetic spec OR held-object copy
    const MlHitboxSpec *hitbox;
    if (stageDamage) {
      if (!row.aIsObj) ml_hd_out_of_domain("stage row without collisionData");
      double normalAngle = fd_atan2(row.normal.y, row.normal.x);
      if (normalAngle < 0) {
        normalAngle += 2 * 3.141592653589793; // 2*Math.PI
      }
      memset(&hbVal, 0, sizeof hbVal);
      // constructor-LIKE key set {offset,dmg,angle,kg,bk,sk,type}: the
      // clank/hitGrounded/hitAirborne/throwextra reads are undefined-falsy,
      // exactly the ML_HB_CONSTRUCTOR read model.
      hbVal.shape = ML_HB_CONSTRUCTOR;
      hbVal.offset = vec2d(0, 0);
      hbVal.dmg = 10;
      hbVal.angle = normalAngle * 180 / 3.141592653589793;
      hbVal.kg = 100;
      hbVal.bk = 0;
      hbVal.sk = 150;
      hbVal.type = h;
      hitbox = &hbVal;
    } else {
      const int hh = (int)h;
      if (h != (double)hh || hh < 0 || hh > 3) {
        ml_hd_out_of_domain("hitQueue hitbox index");
      }
      // upstream HOLDS the object reference `player[a].hitboxes.id[h]`:
      // a dispatch that REASSIGNS the id slot leaves the held object
      // untouched — the by-value copy models that (hitbox contents are
      // immutable chars-data / reassigned-not-mutated, task-2 measured).
      hbVal = P(S, row.a)->hitboxes.id[hh];
      hitbox = &hbVal;
    }

    // if in furafura, make sure sfx stops (:761)
    if (strcmp(P(S, v)->actionState, "FURAFURA") == 0) {
      // sounds.furaloop.stop(player[v].furaLoopID) — id-routed (M4 task 6)
      ml_sound_stop_id("furaloop.stop", 1, P(S, v)->furaLoopID);
    }
    // switch (hitbox.type) — strict matching
    if (hitbox->type == 2) {
      // grab
      if (FLAGS(S, v)->canBeGrabbed) {
        if (row.aIsObj) ml_hd_out_of_domain("grab row with stage attacker");
        if (grabQueueLen >= HD_GRABQ_CAP) {
          ml_hd_out_of_domain("grabQueue overflow");
        }
        grabQueue[grabQueueLen][0] = row.a;
        grabQueue[grabQueueLen][1] = v;
        grabQueue[grabQueueLen][2] = 0; // false
        grabQueueLen++;
      }
    } else if (hitbox->type == 5) {
      // sleep
      dsp0(S, q, "init", "FURASLEEPSTART", S->characterSelections[slot(v)], v);
    } else {
      const int vi = slot(v);
      if (vi < 0 || vi > 3) ml_hd_out_of_domain("ignoreGrabs index");
      ignoreGrabs[vi] = true;
      executeRegularHit(S, q, v, &row, h, shieldHit, isThrow, drawBounce,
                        phantom, stageDamage, hitbox);
    }
  }
  executeGrabHits(S, q, grabQueue, grabQueueLen, ignoreGrabs);
}

// --- executeGrabHits (:784) --------------------------------------------------------------------------

static void executeGrabHits(MlSim *S, HdQueues *q,
                            double grabQueue[][3], int grabQueueLen,
                            bool ignoreGrabs[4]) {
  for (int j = 0; j < grabQueueLen; j++) {
    const int aj = slot(grabQueue[j][0]);
    if (aj < 0 || aj > 3) ml_hd_out_of_domain("ignoreGrabs[grabQueue[j][0]]");
    if (!ignoreGrabs[aj]) {
      if (!(grabQueue[j][2] != 0)) {
        MlPlayer *gv = P(S, grabQueue[j][1]);
        if (strcmp(gv->actionState, "GRAB") == 0 && gv->timer > 0 &&
            gv->timer < 14 &&
            gv->phys.face != P(S, grabQueue[j][0])->phys.face) {
          executeGrabTech(S, q, grabQueue[j][0], grabQueue[j][1]);
          grabQueue[j][2] = 1; // true
          const int vj = slot(grabQueue[j][1]);
          if (vj < 0 || vj > 3) ml_hd_out_of_domain("ignoreGrabs[v]");
          ignoreGrabs[vj] = true;
        } else {
          for (int k = 0; k < grabQueueLen; k++) {
            if (k != j) {
              if (grabQueue[j][0] == grabQueue[k][1]) {
                executeGrabTech(S, q, grabQueue[j][0], grabQueue[k][0]);
                grabQueue[j][2] = 1;
                grabQueue[k][2] = 1;
                break;
              }
            }
          }
        }
      }
      if (!(grabQueue[j][2] != 0)) {
        const double a = grabQueue[j][0];
        const double v = grabQueue[j][1];
        MlPlayer *pv = P(S, v);
        MlPlayer *pa = P(S, a);
        const bool grabTechV = pv->phys.hasGrabTech ? pv->phys.grabTech
                                                    : false; // undef falsy
        if (pv->phys.grabbedBy == -1 && pa->phys.grabbing == -1 &&
            pv->phys.hurtBoxState == 0 && !grabTechV) {
          pv->phys.cVel = vec2d(0, 0);
          pv->phys.kVel = vec2d(0, 0);
          pa->phys.cVel = vec2d(0, 0);
          pa->phys.kVel = vec2d(0, 0);
          pv->phys.grabbedBy = a;
          pv->phys.shielding = false;
          pa->phys.grabbing = v;
          turnoff(S, a);
          turnoff(S, v);
          if (strcmp(pa->actionState, "UPSPECIAL") == 0) {
            pv->phys.face = pa->phys.face * -1;
            dsp0(S, q, "init", "THROWNFALCONDIVE",
                 S->characterSelections[slot(v)], v);
          } else {
            dsp0(S, q, "init", "CAPTUREPULLED",
                 S->characterSelections[slot(v)], v);
          }
        }
      }
    }
  }
}

// --- executeGrabTech (:833) --------------------------------------------------------------------------

static void executeGrabTech(MlSim *S, HdQueues *q, double a, double v) {
  MlPlayer *pa = P(S, a);
  MlPlayer *pv = P(S, v);
  if (pa->phys.pos.x < pv->phys.pos.x) {
    pa->phys.face = 1;
    pv->phys.face = -1;
  } else {
    pa->phys.face = -1;
    pv->phys.face = 1;
  }
  pa->phys.hasGrabTech = true;
  pa->phys.grabTech = true;
  pv->phys.hasGrabTech = true;
  pv->phys.grabTech = true;
  turnoff(S, a);
  turnoff(S, v);
  dsp0(S, q, "init", "CAPTURECUT", S->characterSelections[slot(a)], a);
  dsp0(S, q, "init", "CAPTURECUT", S->characterSelections[slot(v)], v);
  ml_sound_play("parry");
  // drawVfx({name:"shieldup", pos:new Vec2D((a.pos.x+v.pos.x)/2,
  //          a.pos.y+12), face:v.face, f:3}) (hitDetection.js:848-853;
  //          M4 task 1)
  ml_drawVfx_f("shieldup", (pa->phys.pos.x + pv->phys.pos.x) / 2,
               pa->phys.pos.y + 12, pv->phys.face, 3);
}

// --- getKnockback (:856) -----------------------------------------------------------------------------

double hd_getKnockback(HdKbSpec hb, double damagestaled, double damageunstaled,
                       double percent, double weight, bool crouching,
                       bool vCancel) {
  double kb;
  if (hb.sk == 0) {
    kb = ((0.01 * hb.kg) *
              ((1.4 * (((0.05 * (damageunstaled *
                                 (damagestaled + floor(percent)))) +
                        (damagestaled + floor(percent)) * 0.1) *
                       (2.0 - (2.0 * (weight * 0.01)) /
                                  (1.0 + (weight * 0.01))))) +
               18) +
          hb.bk);
  } else {
    kb = ((((hb.sk * 10 / 20) + 1) * 1.4 * (200 / (weight + 100)) + 18) *
          (hb.kg / 100)) +
         hb.bk;
  }
  if (kb > 2500) {
    kb = 2500;
  }
  if (crouching) {
    kb *= 0.67;
  }
  if (vCancel) {
    kb *= 0.95;
  }

  return kb;
}

// --- getLaunchAngle (:878) ---------------------------------------------------------------------------

double hd_getLaunchAngle(double trajectory, double knockback, bool hasReverse,
                         bool reverse, double x, double y, bool grounded) {
  (void)hasReverse; // truthiness-only consumption (undefined -> falsy)
  bool deadzone = false;
  double diAngle = hd_nan(); // read only on the !deadzone path
  if (knockback < 80 && grounded && (trajectory == 0 || trajectory == 180)) {
    deadzone = true;
  }
  if (x < 0.2875 && x > -0.2875) {
    x = 0;
  }
  if (y < 0.2875 && y > -0.2875) {
    y = 0;
  }
  if (x == 0 && y < 0) {
    diAngle = 270;
  } else if (x == 0 && y > 0) {
    diAngle = 90;
  } else if (x == 0 && y == 0) {
    deadzone = true;
  } else {
    diAngle = fd_atan(y / x) * (180 / 3.141592653589793) * 1;
    if (x < 0) {
      diAngle += 180;
    } else if (y < 0) {
      diAngle += 360;
    }
  }

  if (trajectory == 361) {
    if (knockback < 32.1) {
      if (reverse) {
        trajectory = 180;
      } else {
        trajectory = 0;
      }
    } else if (knockback >= 32.1) {
      if (reverse) {
        trajectory = 136;
      } else {
        trajectory = 44;
      }
    } else {
      // upstream: prompt("Why would this ever get called?") — a NaN
      // knockback is outside the captured domain
      ml_hd_out_of_domain("getLaunchAngle prompt arm");
    }
  } else {
    if (reverse) {
      trajectory = 180 - trajectory;
      if (trajectory < 0) {
        trajectory = 360 + trajectory;
      }
    }
  }

  double angleOffset;
  if (!deadzone) {
    double rAngle = trajectory - diAngle;
    if (rAngle > 180) {
      rAngle -= 360;
    }

    const double pDistance = fd_sin(rAngle * angleConversion) * sqrt(x * x + y * y);

    angleOffset = pDistance * pDistance * 18;
    if (angleOffset > 18) {
      angleOffset = 18;
    }

    if (rAngle < 0 && rAngle > -180) {
      angleOffset *= -1;
    }
  } else {
    angleOffset = 0;
  }
  double newtraj = trajectory - angleOffset;
  if (newtraj < 0.01) {
    newtraj = 0;
  }
  return newtraj;
}

// --- launch velocities / decays (:967-996) -----------------------------------------------------------

double hd_getHorizontalVelocity(double knockback, double angle) {
  const double initialVelocity = knockback * 0.03;
  const double horizontalAngle = fd_cos(angle * angleConversion);
  double horizontalVelocity = initialVelocity * horizontalAngle;
  horizontalVelocity = js_round(horizontalVelocity * 100000) / 100000;
  return horizontalVelocity;
}

double hd_getVerticalVelocity(double knockback, double angle, bool grounded,
                              double trajectory) {
  const double initialVelocity = knockback * 0.03;
  const double verticalAngle = fd_sin(angle * angleConversion);
  double verticalVelocity = initialVelocity * verticalAngle;
  verticalVelocity = js_round(verticalVelocity * 100000) / 100000;
  if (knockback < 80 && grounded && (trajectory == 0 || trajectory == 180)) {
    verticalVelocity = 0;
  }
  return verticalVelocity;
}

double hd_getHorizontalDecay(double angle) {
  double decay = 0.051 * fd_cos(angle * angleConversion);
  decay = js_round(decay * 100000) / 100000;
  return decay;
}

double hd_getVerticalDecay(double angle) {
  double decay = 0.051 * fd_sin(angle * angleConversion);
  decay = js_round(decay * 100000) / 100000;
  return decay;
}

// --- getHitstun (:998) --------------------------------------------------------------------------------

double hd_getHitstun(double knockback) {
  return floor(knockback * .4);
}

// --- knockbackSounds (:1005; live external caller article.js) -----------------------------------------

static void knockbackSoundsInternal(HdTypeVal type, double knockback,
                                    double charId) {
  if (tv_loose_eq(type, 4)) {
    ml_sound_play("firestronghit");
  }
  if (knockback < 50) {
    if (tv_strict_eq(type, 0)) ml_sound_play("normalweakhit");
    else if (tv_strict_eq(type, 1)) ml_sound_play("swordweakhit");
    else if (tv_strict_eq(type, 3)) ml_sound_play("fireweakhit");
  } else if (knockback < 100) {
    if (tv_strict_eq(type, 0)) ml_sound_play("normalmediumhit");
    else if (tv_strict_eq(type, 1)) ml_sound_play("swordmediumhit");
    else if (tv_strict_eq(type, 3)) ml_sound_play("firemediumhit");
  } else if (knockback < 140) {
    if (tv_strict_eq(type, 0)) ml_sound_play("normalstronghit");
    else if (tv_strict_eq(type, 1)) ml_sound_play("swordstronghit");
    else if (tv_strict_eq(type, 3)) ml_sound_play("firestronghit");
  } else {
    if (tv_strict_eq(type, 0)) ml_sound_play("normalstronghit");
    else if (tv_strict_eq(type, 1)) ml_sound_play("swordreallystronghit");
    else if (tv_strict_eq(type, 3)) {
      ml_sound_play("bathit");
      ml_sound_play("firestronghit");
    }
    ml_sound_play("cheer");
    if (knockback < 280) {
      ml_sound_play("stronghit");
      // switch (characterSelections[v]) — strict
      if (charId == 0) ml_sound_play("weakhurt");
      else if (charId == 2) ml_sound_play("foxweakhurt");
      else if (charId == 3) ml_sound_play("falcohurt1");
    } else {
      ml_sound_play("strongerhit");
      if (charId == 0) ml_sound_play("stronghurt");
      else if (charId == 1) ml_sound_play("puffhurt");
      else if (charId == 2) ml_sound_play("foxstronghurt");
      else if (charId == 3) ml_sound_play("falcohurt2");
    }
  }
}

void hd_knockbackSounds(double type, double knockback, double charId) {
  knockbackSoundsInternal(tv_num(type), knockback, charId);
}

// --- checkPhantoms (:1104) ------------------------------------------------------------------------------

void hd_checkPhantoms(MlSim *S, HdQueues *q) {
  // upstream splices while iterating WITHOUT decrementing i (the entry
  // after a settled one is skipped this call) — verbatim: the loop bound
  // re-reads the live length.
  for (int i = 0; i < q->phqCount; i++) {
    const double v = q->phq[i][1];
    MlPlayer *pv = P(S, v);
    if (pv->hit.hitlag == 0 && pv->phys.hurtBoxState == 0) {
      // percent += phys.phantomDamage: an absent (never-written) field
      // reads undefined -> NaN percent; the captured domain always has it
      // written by executeRegularHit first.
      pv->percent += pv->phys.hasPhantomDamage ? pv->phys.phantomDamage
                                               : hd_nan();
      pv->phys.hasPhantomDamage = true;
      pv->phys.phantomDamage = 0;
      const double a = q->phq[i][0];
      MlPlayer *pa = P(S, a);
      for (int j = 0; j < pa->hitboxes.hitListLen; j++) {
        if (pa->hitboxes.hitList[j] == v) {
          hitlist_splice1(S, a, j);
          break;
        }
      }
      // phantomQueue.splice(i, 1)
      for (int k = i; k + 1 < q->phqCount; k++) {
        q->phq[k][0] = q->phq[k + 1][0];
        q->phq[k][1] = q->phq[k + 1][1];
      }
      q->phqCount--;
    }
  }
}
