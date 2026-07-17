// article.c <- src/physics/article.js (structure-parallel translation,
// M2 task 13). See article.h for the value model, queue chaining, seams
// and the verbatim upstream quirks. File order mirrors the upstream
// module; the exported collision helpers (wallDetection,
// articleHitCollision, articleShieldCollision, interpolatedArticle*,
// articleHurtCollision) have ONLY internal callers upstream (babel local
// bindings; targetplay is M4 target mode) — static here, pinned 0
// external records by the capture.
#include "article.h"

#include <math.h>   // floor / fabs (Math.floor / Math.abs — unshimmed,
                    // IEEE-exact; PLAN §2 / hit_detection.c precedent)
#include <string.h>

#include "../fdlibm/fdlibm.h" // fd_cos / fd_sin / fd_pow (Math.* shimmed)
#include "characters/shared/moves.h" // mv_dispatch/mv_player/
                                     // mv_screenShake/mv_attr, MvX
#include "environmental_collision.h" // findCollision
#include "interpolated_collision.h"  // sweepCircleVsSweepCircle / VsAABB
#include "ml_events.h"               // ml_sound_play

// --- forward decls (upstream hoisted function declarations) --------------
static MaybeNum art_wallDetection(MlSim *S, MlArticles *A, int i);
static bool articleHitCollision(MlSim *S, MlArticles *A, int a, double v,
                                int k);
static bool articleShieldCollision(MlSim *S, MlArticles *A, int a, double v,
                                   bool previous);
static bool interpolatedArticleCircleCollision(MlArticles *A, int a,
                                               Vec2D circlePos, double r);
static bool interpolatedArticleHurtCollision(MlSim *S, MlArticles *A, int a,
                                             double v);
static bool articleHurtCollision(MlSim *S, MlArticles *A, int a, double v,
                                 bool previous);
static void art_laser_main(MlSim *S, MlArticles *A, int i);
static void art_illusion_main(MlSim *S, MlArticles *A, int i);

// --- small helpers --------------------------------------------------------

static void art_destroy_push(MlArticles *A, double idx) {
  if (A->destroyCount >= ART_CAP) {
    ml_art_out_of_domain("destroyArticleQueue over cap");
  }
  A->destroyQ[A->destroyCount++] = idx;
}

static void art_hit_push(MlArticles *A, double a, double v, bool shieldHit) {
  if (A->hitCount >= ART_CAP) {
    ml_art_out_of_domain("articleHitQueue over cap");
  }
  A->hitQ[A->hitCount].a = a;
  A->hitQ[A->hitCount].v = v;
  A->hitQ[A->hitCount].shieldHit = shieldHit;
  A->hitCount++;
}

static void art_hitlist_push(MlArticle *it, double v) {
  if (it->hitListLen >= ART_HITLIST_CAP) {
    ml_art_out_of_domain("article hitList over cap");
  }
  it->hitList[it->hitListLen++] = v;
}

static MlArticle *art_at(MlArticles *A, double a, const char *what) {
  const int i = (int)a;
  if (a != (double)i || i < 0 || i >= A->count) ml_art_out_of_domain(what);
  return &A->a[i];
}

// victim-hitbox reads through BOTH measured shapes (ml_player.h): the
// CONSTRUCTOR key set lacks clank — the read is undefined, NaN in the
// comparison (rule 2).
static double vhb_clank(const MlHitboxSpec *hb) {
  if (hb->shape == ML_HB_CHARDATA) return hb->clank;
  return js_nan();
}

// offset[idx] on a victim hitbox: only a chars-data PER-FRAME ARRAY has
// indexable entries; a single-Vec2D offset (either shape) gives
// undefined, and upstream then throws on `.x` — trap (rule 13's lazy
// site: callers only reach this under the arms that dereference).
static Vec2D vhb_offset_at(const MlHitboxSpec *hb, double idx,
                           const char *what) {
  if (hb->shape == ML_HB_CHARDATA && !hb->offsetSingle) {
    const int i = (int)idx;
    if (idx != (double)i || i < 0 || i >= hb->offsetLen) {
      ml_art_out_of_domain(what);
    }
    return hb->offsetArr[i];
  }
  ml_art_out_of_domain(what);
  return vec2d(0, 0); // unreachable
}

// new createHitbox(new Vec2D(0,0), size, dmg, angle, kg, bk, sk, type,
// clank, hG, hA) — 11 args, throwex defaults false. Article hitboxes are
// article-owned (fresh per spawn).
static MlHitboxSpec art_hb(double size, double dmg, double angle, double kg,
                           double bk, double sk, double type, double clank,
                           double hG, double hA) {
  MlHitboxSpec hb;
  memset(&hb, 0, sizeof hb);
  hb.shape = ML_HB_CHARDATA; // the 12-key createHitbox key set
  hb.offsetSingle = true;    // offset is one Vec2D (fox-throw precedent)
  hb.offset = vec2d(0, 0);
  hb.offsetLen = 0;
  hb.size = size;
  hb.dmg = dmg;
  hb.angle = angle;
  hb.kg = kg;
  hb.bk = bk;
  hb.sk = sk;
  hb.type = type;
  hb.clank = clank;
  hb.hitGrounded = hG;
  hb.hitAirborne = hA;
  hb.throwextra = false;
  return hb;
}

// --- export function resetAArticles () -----------------------------------

void art_resetAArticles(MlArticles *A) { A->count = 0; }

// --- articles.LASER ---------------------------------------------------------

void art_laser_init(MlSim *S, MlArticles *A, double p, double x, double y,
                    double rotate, ArtOptBool isFoxOpt,
                    ArtOptBool partOfThrowOpt) {
  MlPlayer *pl = mv_player(S, p);
  // const isFox = (options.isFox !== undefined) ? options.isFox : true;
  const bool isFox = isFoxOpt.has ? isFoxOpt.v : true;
  // const partOfThrow = options.partOfThrow || false;
  const bool partOfThrow = partOfThrowOpt.has ? partOfThrowOpt.v : false;
  // this.strokeStyle / this.fillStyle: render-only writes on the article
  // table (draw() colours) — documented no-op (article.h).
  if (A->count >= ART_CAP) ml_art_out_of_domain("aArticles over cap");
  MlArticle *obj = &A->a[A->count];
  memset(obj, 0, sizeof *obj);
  obj->kind = ART_LASER;
  obj->player = p;
  obj->hitListLen = 0;
  obj->rotate = rotate;
  obj->destroyOnHit = true;
  obj->clank = false;
  obj->timer = 0;
  obj->vel = vec2d((isFox ? 7 : 5) * fd_cos(rotate) * pl->phys.face,
                   (isFox ? 7 : 5) * fd_sin(rotate));
  obj->pos = vec2d(pl->phys.pos.x + (x * pl->phys.face), pl->phys.pos.y + y);
  obj->posPrev1 = vec2d(pl->phys.pos.x, pl->phys.pos.y + y);
  obj->posPrev2 = vec2d(pl->phys.pos.x, pl->phys.pos.y + y);
  obj->posPrev3 = vec2d(pl->phys.pos.x, pl->phys.pos.y + y);
  obj->posPrev = vec2d(pl->phys.pos.x, pl->phys.pos.y + y);
  obj->hb = art_hb(1.172, 3, 361, isFox ? 0 : (partOfThrow ? 0 : 100), 0,
                   isFox ? 0 : (partOfThrow ? 0 : 5), 0, 0, 1, 1);
  obj->ecb[0] = vec2d(pl->phys.pos.x + (x * pl->phys.face),
                      pl->phys.pos.y + y - 0.01);
  obj->ecb[1] = vec2d(pl->phys.pos.x + (x * pl->phys.face) + 10,
                      pl->phys.pos.y + y);
  obj->ecb[2] = vec2d(pl->phys.pos.x + (x * pl->phys.face),
                      pl->phys.pos.y + y + 0.01);
  obj->ecb[3] = vec2d(pl->phys.pos.x + (x * pl->phys.face) - 10,
                      pl->phys.pos.y + y);
  A->count++; // aArticles.push({name:"LASER", player:p, instance:obj})
  art_laser_main(S, A, A->count - 1); // articles.LASER.main(len - 1)
}

static void art_laser_main(MlSim *S, MlArticles *A, int i) {
  MlArticle *it = &A->a[i];
  it->timer++;
  if (it->timer > 4) {
    it->posPrev.x = it->posPrev3.x;
    it->posPrev.y = it->posPrev3.y;
  }
  if (it->timer > 3) {
    it->posPrev3.x = it->posPrev2.x;
    it->posPrev3.y = it->posPrev2.y;
  }
  if (it->timer > 2) {
    it->posPrev2.x = it->posPrev1.x;
    it->posPrev2.y = it->posPrev1.y;
  }
  if (it->timer > 1) {
    it->posPrev1.x = it->pos.x;
    it->posPrev1.y = it->pos.y;
  }
  it->pos.x += it->vel.x;
  it->ecb[0].x += it->vel.x;
  it->ecb[1].x += it->vel.x;
  it->ecb[2].x += it->vel.x;
  it->ecb[3].x += it->vel.x;
  it->pos.y += it->vel.y;
  it->ecb[0].y += it->vel.y;
  it->ecb[1].y += it->vel.y;
  it->ecb[2].y += it->vel.y;
  it->ecb[3].y += it->vel.y;
  // `if (wallDetection(i) || timer > 200)`: the returned sweep parameter
  // is consumed by TRUTHINESS — sweep exactly 0 is falsy (quirk carried
  // verbatim, article.h).
  const MaybeNum wd = art_wallDetection(S, A, i);
  const bool wdTruthy = wd.present && wd.v == wd.v && wd.v != 0;
  if (wdTruthy || it->timer > 200) {
    art_destroy_push(A, (double)i);
  }
  // draw: render-only (article.h)
}

// --- articles.ILLUSION -------------------------------------------------------

void art_illusion_init(MlSim *S, MlArticles *A, double p, double type,
                       ArtOptBool isFoxOpt) {
  MlPlayer *pl = mv_player(S, p);
  // const isFox = options.isFox || true; — ALWAYS true upstream (a truthy
  // isFox yields itself, anything else yields the literal true). Carried
  // verbatim: falco illusions get the fox hb values.
  const bool isFox = true;
  (void)isFoxOpt;
  if (A->count >= ART_CAP) ml_art_out_of_domain("aArticles over cap");
  MlArticle *obj = &A->a[A->count];
  memset(obj, 0, sizeof *obj);
  obj->kind = ART_ILLUSION;
  obj->player = p;
  obj->hitListLen = 0;
  obj->destroyOnHit = false;
  obj->clank = true;
  obj->timer = 0;
  obj->pos = vec2d(pl->phys.posPrev.x, pl->phys.posPrev.y + 5);
  obj->posPrev = vec2d(pl->phys.posPrev.x, pl->phys.posPrev.y + 5);
  obj->hb = art_hb(4.160, 7, isFox ? 80 : 270, isFox ? 60 : 70,
                   isFox ? 68 : 70, 0, 1, 1, 1, 1);
  obj->ecb[0] = vec2d(pl->phys.posPrev.x, pl->phys.posPrev.y - 10);
  obj->ecb[1] = vec2d(pl->phys.posPrev.x + 10, pl->phys.posPrev.y);
  obj->ecb[2] = vec2d(pl->phys.posPrev.x, pl->phys.posPrev.y + 10);
  obj->ecb[3] = vec2d(pl->phys.posPrev.x - 10, pl->phys.posPrev.y);
  // if (type) — number consumed by truthiness
  if (type == type && type != 0) {
    if (isFox) {
      obj->hb.kg = 40;
    } else {
      // dead arm upstream (isFox always true) — carried verbatim
      obj->hb.angle = 65;
      obj->hb.kg = 60;
      obj->hb.bk = 74;
    }
  }
  A->count++; // aArticles.push({name:"ILLUSION", player:p, instance:obj})
  art_illusion_main(S, A, A->count - 1); // articles.ILLUSION.main(len - 1)
}

static void art_illusion_main(MlSim *S, MlArticles *A, int i) {
  MlArticle *it = &A->a[i];
  MlPlayer *pl = mv_player(S, it->player);
  it->timer++;
  // posPrev = new Vec2D(pos.x, pos.y); pos = new Vec2D(posPrev(player)...)
  // — whole-object reassignments (by-value here).
  it->posPrev = vec2d(it->pos.x, it->pos.y);
  it->pos = vec2d(pl->phys.posPrev.x, pl->phys.posPrev.y);
  it->ecb[0] = vec2d(pl->phys.posPrev.x, pl->phys.posPrev.y - 10);
  it->ecb[1] = vec2d(pl->phys.posPrev.x + 10, pl->phys.posPrev.y);
  it->ecb[2] = vec2d(pl->phys.posPrev.x, pl->phys.posPrev.y + 10);
  it->ecb[3] = vec2d(pl->phys.posPrev.x - 10, pl->phys.posPrev.y);
  if (it->timer > 5) {
    art_destroy_push(A, (double)i);
  }
}

// --- export function executeArticles () --------------------------------------

void art_executeArticles(MlSim *S, MlArticles *A) {
  A->destroyCount = 0; // destroyArticleQueue = []
  for (int i = 0; i < A->count; i++) {
    if (A->a[i].kind == ART_LASER) {
      art_laser_main(S, A, i);
    } else {
      art_illusion_main(S, A, i);
    }
  }
}

// --- export function destroyArticles () ---------------------------------------

// aArticles.splice(start, 1) with JS semantics: ToInteger start, negative
// start counts from the END (duplicate destroy pushes make `queue[k] - k`
// go negative — quirk carried verbatim, article.h).
static void art_splice1(MlArticles *A, double startD) {
  if (startD != startD) ml_art_out_of_domain("splice NaN start");
  long start = (long)startD; // queue entries are integer-valued
  if (startD != (double)start) ml_art_out_of_domain("splice fractional start");
  const long len = A->count;
  if (start < 0) {
    start = len + start;
    if (start < 0) start = 0;
  }
  if (start >= len) return; // deleteCount clips to 0
  for (long j = start; j + 1 < len; j++) A->a[j] = A->a[j + 1];
  A->count--;
}

void art_destroyArticles(MlArticles *A) {
  for (int k = 0; k < A->destroyCount; k++) {
    art_splice1(A, A->destroyQ[k] - (double)k);
  }
  // NOTE: the queue is NOT cleared here — the next executeArticles resets
  // it (upstream reassigns destroyArticleQueue = [] there).
}

// renderArticles: render-only (article.h) — no C surface.

// --- export function articlesHitDetection () -----------------------------------

void art_articlesHitDetection(MlSim *S, MlArticles *A) {
  A->hitCount = 0; // articleHitQueue = []
  for (int a = 0; a < A->count; a++) {
    MlArticle *it = &A->a[a];
    // set true only inside the commented-out clank body — stays false
    const bool articleDestroyed = false;
    bool interpolate;
    if (it->timer > 1) {
      interpolate = true;
    } else {
      interpolate = false;
    }
    for (int v = 0; v < 4; v++) {
      bool inHitList = false;
      for (int n = 0; n < 4; n++) {
        // hitList[n] beyond the length reads undefined — never equal
        if (n < it->hitListLen && (double)v == it->hitList[n]) {
          inHitList = true;
          break;
        }
      }
      // if v isnt the owner, not destroyed and no in article's hitlist
      if ((double)v != it->player && !articleDestroyed && !inHitList &&
          S->playerType[v] != -1) {
        MlPlayer *pv = mv_player(S, (double)v);
        // if article is clankable — the body is commented out upstream
        // ("ILL DO CLANKS TOMOZ"); the GUARD still evaluates (id[k].clank
        // reads through both hitbox shapes), attackerClank stays false.
        const bool attackerClank = false;
        if (it->clank) {
          for (int k = 0; k < 4; k++) {
            const double cl = pv->hitboxes.active[k]
                                  ? vhb_clank(&pv->hitboxes.id[k])
                                  : js_nan();
            if (pv->hitboxes.active[k] &&
                (cl == 1 || (cl == 2 && pv->phys.grounded))) {
              // upstream body fully commented out
            }
          }
        }
        if (!attackerClank) {
          bool reflected = false;
          for (int i = 0; i < 4; i++) {
            if (pv->hitboxes.active[i]) {
              if (pv->hitboxes.id[i].type == 7) {
                // articleHitCollision(a,v,i) || (interpolate &&
                //   (articleHitCollision(a,v,i) ||
                //    interpolatedArticleCircleCollision(...))) — the
                // double direct call and the LAZY offset[0] deref are
                // carried verbatim (rule 13).
                bool hit = articleHitCollision(S, A, a, (double)v, i);
                if (!hit && interpolate) {
                  hit = articleHitCollision(S, A, a, (double)v, i);
                  if (!hit) {
                    const Vec2D off0 = vhb_offset_at(
                        &pv->hitboxes.id[i], 0, "reflect offset[0] deref");
                    hit = interpolatedArticleCircleCollision(
                        A, a,
                        vec2d(pv->phys.pos.x + off0.x,
                              pv->phys.pos.y + off0.y),
                        pv->hitboxes.id[i].size);
                  }
                }
                if (hit) {
                  if (strncmp(pv->actionState, "DOWNSPECIAL", 11) == 0) {
                    // do shine reflect animation
                    ml_sound_play("foxshinereflect");
                  }
                  // change ownership
                  it->player = (double)v;
                  // increase damage
                  it->hb.dmg *= 1.5;
                  // reflect (vel != undefined check: key presence == kind)
                  if (it->kind == ART_LASER) {
                    it->vel.x *= -1;
                    it->vel.y *= -1;
                  }
                  reflected = true;
                  break;
                }
              }
            }
          }
          if (!reflected) {
            if (pv->phys.shielding &&
                (articleShieldCollision(S, A, a, (double)v, false) ||
                 (interpolate &&
                  (articleShieldCollision(S, A, a, (double)v, true) ||
                   interpolatedArticleCircleCollision(
                       A, a, pv->phys.shieldPositionReal,
                       pv->phys.shieldSize))))) {
              art_hit_push(A, (double)a, (double)v, true);
              art_hitlist_push(it, (double)v);
              // articles[name].canTurboCancel: LASER false, ILLUSION true
              if (it->kind == ART_ILLUSION) {
                mv_player(S, it->player)->hasHit = true;
              }
            } else if (pv->phys.hurtBoxState != 1) {
              if (articleHurtCollision(S, A, a, (double)v, false) ||
                  (interpolate &&
                   (interpolatedArticleHurtCollision(S, A, a, (double)v) ||
                    articleHurtCollision(S, A, a, (double)v, true)))) {
                art_hit_push(A, (double)a, (double)v, false);
                art_hitlist_push(it, (double)v);
                if (it->kind == ART_ILLUSION) {
                  mv_player(S, it->player)->hasHit = true;
                }
                if (it->destroyOnHit) {
                  art_destroy_push(A, (double)a);
                }
                // NOTE: articleDestroyed is NOT set here (upstream bug —
                // duplicate destroy pushes possible; splice quirk).
              }
            }
          }
        }
      }
    }
  }
}

// --- export function executeArticleHits (input) ---------------------------------

void art_executeArticleHits(MlSim *S, MlArticles *A,
                            const MlInputBuffer in[4]) {
  for (int i = 0; i < A->hitCount; i++) {
    const double a = A->hitQ[i].a;
    const double v = A->hitQ[i].v;
    const bool shieldHit = A->hitQ[i].shieldHit;
    MlArticle *it = art_at(A, a, "executeArticleHits row index");
    // var o = aArticles[a].player; — declared and never used upstream
    MlHitboxSpec *hb = &it->hb; // reference: dmg writes go through it
    MlPlayer *pv = mv_player(S, v);

    const double damage = hb->dmg;

    if (shieldHit) {
      if (pv->phys.powerShieldReflectActive) {
        ml_drawVfx("powershieldreflect", pv->phys.shieldPositionReal.x,
                   pv->phys.shieldPositionReal.y, pv->phys.face);
        ml_sound_play("powershieldreflect");
        it->player = v; // change ownership to victim
        // reflects velocity (vel != undefined: key presence == kind)
        if (it->kind == ART_LASER) {
          it->vel.x *= -1;
          it->vel.y *= -1;
        }
        // cuts damage in half
        hb->dmg *= 0.5;
      } else {
        pv->phys.shieldHP -= damage;
        if (pv->phys.shieldHP < 0) {
          pv->phys.shielding = false;
          pv->phys.cVel.y = 2.5;
          pv->phys.grounded = false;
          pv->phys.shieldHP = 0;
          ml_drawVfx("breakShield", pv->phys.pos.x, pv->phys.pos.y,
                     pv->phys.face);
          mv_dispatch(S, MV_CS(S, v), "SHIELDBREAKFALL", "init", v, in, 0);
          ml_sound_play("shieldbreak");
          break; // exits the whole row loop (GUARD.init skipped)
        }
        if (it->destroyOnHit) {
          art_destroy_push(A, a);
        }
        ml_drawVfx("clank", it->pos.x, it->pos.y, 1);
        pv->hit.hitlag = floor(damage * (1.0 / 3.0) + 3);
        pv->hit.shieldstun =
            ((floor(damage) *
              ((0.65 * (1 - ((pv->phys.shieldAnalog - 0.3) / 0.7))) + 0.3)) *
             1.5) +
            2;
        double victimPush =
            ((floor(damage) *
              ((0.195 * (1 - ((pv->phys.shieldAnalog - 0.3) / 0.7))) +
               0.09)) +
             0.4) *
            0.6;
        if (victimPush > 2) {
          victimPush = 2;
        }
        if (it->pos.x < pv->phys.pos.x) {
          pv->phys.cVel.x = victimPush;
        } else {
          pv->phys.cVel.x = -victimPush;
        }
      }

      mv_dispatch(S, MV_CS(S, v), "GUARD", "init", v, in, 0);

    } else {
      if (pv->phys.hurtBoxState == 0) {
        const HdFlags *fl = hd_flags(MV_CS(S, v), pv->actionState);
        const bool crouching = fl->crouch;
        bool vCancel = false;
        if (pv->phys.vCancelTimer > 0) {
          if (fl->vCancel) {
            vCancel = true;
            ml_sound_play("vcancel");
          }
        }
        HdKbSpec kb;
        kb.kg = hb->kg;
        kb.bk = hb->bk;
        kb.sk = hb->sk;
        pv->hit.knockback =
            hd_getKnockback(kb, damage, damage, pv->percent,
                            (double)mv_attr(MV_CS(S, v))->weight, crouching,
                            vCancel);

        // upstream ALIASES instance.pos here; by-value copy is
        // unobservable (article.h note).
        pv->hit.hitPoint = it->pos;
        pv->percent += damage;

        // switch (hb.type) — article types are authored 0 (laser) /
        // 1 (illusion); default falls through.
        if (hb->type == 0) {
          ml_drawVfx("normalhit", pv->hit.hitPoint.x, pv->hit.hitPoint.y,
                     pv->phys.face);
        } else if (hb->type == 1) {
          ml_drawVfx("hitSparks", pv->hit.hitPoint.x, pv->hit.hitPoint.y,
                     pv->phys.face);
          ml_drawVfx("hitFlair", pv->hit.hitPoint.x, pv->hit.hitPoint.y,
                     pv->phys.face);
          ml_drawVfx_f("hitCurve", pv->hit.hitPoint.x, pv->hit.hitPoint.y,
                       pv->phys.face, pv->hit.angle);
        }

        hd_knockbackSounds(hb->type, pv->hit.knockback, MV_CS(S, v));

        if (pv->hit.knockback > 0) {
          pv->hit.angle = hb->angle;
          if (pv->hit.angle == 361) {
            if (pv->hit.knockback < 32.1) {
              pv->hit.angle = 0;
            } else if (pv->hit.knockback >= 32.1) {
              pv->hit.angle = 44;
            }
          }

          pv->hit.hitlag = floor(damage * (1.0 / 3.0) + 3);

          if (it->pos.x < pv->phys.pos.x) {
            pv->hit.hasReverse = true;
            pv->hit.reverse = false;
            pv->phys.face = -1;
          } else {
            pv->hit.hasReverse = true;
            pv->hit.reverse = true;
            pv->phys.face = 1;
          }

          const bool isThrow = false;
          if (pv->phys.grabbedBy == -1 ||
              (pv->phys.grabbedBy > -1 && pv->hit.knockback > 50)) {
            pv->hit.hitstun = hd_getHitstun(pv->hit.knockback);

            if (pv->hit.knockback >= 80 || isThrow) {
              MvX x = mvx_bool(!isThrow); // DAMAGEFLYN.init(v,input,!isThrow)
              mv_dispatch(S, MV_CS(S, v), "DAMAGEFLYN", "init", v, in, &x);
            } else {
              mv_dispatch(S, MV_CS(S, v), "DAMAGEN2", "init", v, in, 0);
            }
          } else {
            if (strcmp(pv->actionState, "THROWNPUFFDOWN") != 0) {
              mv_dispatch(S, MV_CS(S, v), "CAPTUREDAMAGE", "init", v, in, 0);
            }
          }

          if (pv->phys.grounded && pv->hit.angle > 180) {
            if (pv->hit.knockback >= 80) {
              ml_sound_play("bounce");
              ml_drawVfx("groundBounce", pv->phys.pos.x, pv->phys.pos.y,
                         pv->phys.face);
              pv->hit.angle = 360 - pv->hit.angle;
              pv->hit.knockback *= 0.8;
            }
          }
          // screenShake(knockback): 4 seeded draws, render value discarded
          mv_screenShake();
          // percentShake: native-RNG off-tick HUD effect (CHECKSUM.md §7)
        }

      } else {
        ml_sound_play("blunthit");
        ml_drawVfx_p("clank", it->pos.x, it->pos.y);
      }
    }
  }
}

// --- export function wallDetection (i) --------------------------------------------

static MaybeNum art_wallDetection(MlSim *S, MlArticles *A, int i) {
  const MlArticle *article = &A->a[i];
  ECB ecbp;
  ecbp.pt[0] = article->ecb[0];
  ecbp.pt[1] = article->ecb[1];
  ecbp.pt[2] = article->ecb[2];
  ecbp.pt[3] = article->ecb[3];
  ECB ecb1;
  if (article->timer < 2) {
    const Vec2D focus = article->posPrev;
    const double offset = 0.0001;
    ecb1.pt[0] = vec2d(focus.x, focus.y - offset);
    ecb1.pt[1] = vec2d(focus.x + offset, focus.y);
    ecb1.pt[2] = vec2d(focus.x, focus.y + offset);
    ecb1.pt[3] = vec2d(focus.x - offset, focus.y);
  } else {
    ecb1 = moveECB(ecbp, ml_subtract(article->posPrev, article->pos));
  }
  bool present[5 * ML_MAX_SURFACES];
  double sweeps[5 * ML_MAX_SURFACES];
  int cn = 0;
  const SurfaceList *lists[5];
  char types[5];
  lists[0] = &S->stage.s.wallL;    types[0] = 'l';
  lists[1] = &S->stage.s.wallR;    types[1] = 'r';
  lists[2] = &S->stage.s.ceiling;  types[2] = 'c';
  lists[3] = &S->stage.s.ground;   types[3] = 'g';
  lists[4] = &S->stage.s.platform; types[4] = 'p';
  for (int l = 0; l < 5; l++) {
    for (int j = 0; j < lists[l]->count; j++) {
      LabelledSurface ls;
      ls.surface = lists[l]->items[j];
      ls.type = types[l];
      ls.index = (double)j;
      const CollisionDatum cd = findCollision(ecb1, ecbp, ls);
      if (cd.kind != CD_NULL) { // thisCollision !== null
        present[cn] = true;
        sweeps[cn] = cd.kind == CD_SURFACE ? cd.ps.sweep : cd.es.sweep;
        cn++;
      }
    }
  }
  const int pick = pickSmallestSweep(present, sweeps, cn);
  if (pick != -1) {
    return maybe_num(sweeps[pick]); // firstCollision.sweep
  } else {
    return maybe_null();
  }
}

// --- export function articleHitCollision (a,v,k) ------------------------------------

static bool articleHitCollision(MlSim *S, MlArticles *A, int a, double v,
                                int k) {
  MlPlayer *pv = mv_player(S, v);
  const Vec2D hbpos = A->a[a].pos;
  const Vec2D off = vhb_offset_at(&pv->hitboxes.id[k], pv->hitboxes.frame,
                                  "articleHitCollision offset[frame]");
  const Vec2D hbpos2 = vec2d(pv->phys.pos.x + (off.x * pv->phys.face),
                             pv->phys.pos.y + off.y);
  // var hitPoint — computed and unused upstream (dead local)
  return fd_pow(hbpos2.x - hbpos.x, 2) + fd_pow(hbpos.y - hbpos2.y, 2) <=
         fd_pow(A->a[a].hb.size + pv->hitboxes.id[k].size, 2);
}

// --- export function articleShieldCollision (a,v,previous) ---------------------------

static bool articleShieldCollision(MlSim *S, MlArticles *A, int a, double v,
                                   bool previous) {
  Vec2D hbpos;
  if (previous) {
    hbpos = A->a[a].posPrev;
  } else {
    hbpos = A->a[a].pos;
  }
  MlPlayer *pv = mv_player(S, v);
  const Vec2D shieldpos = pv->phys.shieldPositionReal;
  return fd_pow(shieldpos.x - hbpos.x, 2) + fd_pow(hbpos.y - shieldpos.y, 2) <=
         fd_pow(A->a[a].hb.size + pv->phys.shieldSize, 2);
}

// --- export function interpolatedArticleCircleCollision (a,circlePos,r) ---------------

static bool interpolatedArticleCircleCollision(MlArticles *A, int a,
                                               Vec2D circlePos, double r) {
  const Vec2D h1 = A->a[a].posPrev;
  const Vec2D h2 = A->a[a].pos;
  const double s = A->a[a].hb.size;
  const MaybeVec2D collision =
      sweepCircleVsSweepCircle(h1, s, h2, s, circlePos, r, circlePos, r);
  if (!collision.present) {
    return false;
  } else {
    return true;
  }
}

// --- export function interpolatedArticleHurtCollision (a,v) ----------------------------

static bool interpolatedArticleHurtCollision(MlSim *S, MlArticles *A, int a,
                                             double v) {
  MlPlayer *pv = mv_player(S, v);
  const Box2D hurt = pv->phys.hurtbox;
  const Vec2D h1 = A->a[a].posPrev;
  const Vec2D h2 = A->a[a].pos;
  const double r = A->a[a].hb.size;
  const MaybeVec2D collision =
      sweepCircleVsAABB(h1, r, h2, r, hurt.min, hurt.max);
  if (!collision.present) {
    return false;
  } else {
    return true;
  }
}

// --- export function articleHurtCollision (a,v,previous) -------------------------------

static bool articleHurtCollision(MlSim *S, MlArticles *A, int a, double v,
                                 bool previous) {
  Vec2D hbpos;
  if (previous) {
    hbpos = A->a[a].posPrev;
  } else {
    hbpos = A->a[a].pos;
  }
  MlPlayer *pv = mv_player(S, v);
  const Vec2D hurtCenter =
      vec2d((pv->phys.hurtbox.min.x + pv->phys.hurtbox.max.x) / 2,
            (pv->phys.hurtbox.min.y + pv->phys.hurtbox.max.y) / 2);
  const Vec2D distance = vec2d(fabs(hbpos.x - hurtCenter.x),
                               fabs(hbpos.y - hurtCenter.y));

  const double hurtWidth = 8;
  const double hurtHeight = 18;

  if (distance.x > (hurtWidth / 2 + A->a[a].hb.size)) {
    return false;
  }
  if (distance.y > (hurtHeight / 2 + A->a[a].hb.size)) {
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

  return cornerDistance_sq <= fd_pow(A->a[a].hb.size, 2);
}
