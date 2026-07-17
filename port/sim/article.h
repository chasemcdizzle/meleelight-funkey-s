// article.h <- src/physics/article.js (structure-parallel translation,
// M2 task 13). The fox/falco projectile plane: LASER + ILLUSION article
// definitions, the three module queues, per-tick execution, article hit
// detection and article hit execution. Articles are CHECKSUMMED state:
// oracle/CHECKSUM.md §2's `articles` key is exactly the live aArticles
// queue, each entry {name, player, instance} with the whole physics
// instance.
//
// MODULE STATE (MlArticles): upstream module-level `aArticles`,
// `destroyArticleQueue`, `articleHitQueue`. Every mutation goes through
// this module's boundaries (spawns arrive from fox/falco move code via
// the init entries below — the crossings tasks 8/9 verified as seam
// FIFOs become these REAL bodies), so the queues chain across the tick:
// destroyArticles consumes the queue built by the PREVIOUS tick's
// executeArticles (wall/timeout deaths) + articlesHitDetection/
// executeArticleHits (destroyOnHit) — gameTick order main.js:1059-1078.
//
// VALUE MODEL (survey-measured, g01 article capture):
// - LASER instance: 13 keys {clank,destroyOnHit,ecb,hb,hitList,pos,
//   posPrev,posPrev1,posPrev2,posPrev3,rotate,timer,vel};
// - ILLUSION instance: 8 keys (no rotate/vel/posPrev1-3) — key presence
//   is the article KIND (rule 3);
// - hb: a per-article `new createHitbox(...)` — the 12-key chars-data
//   key set with a SINGLE Vec2D offset (ml_player.h MlHitboxSpec,
//   ML_HB_CHARDATA + offsetSingle; fox-throw-hitbox precedent). It is
//   article-OWNED (fresh per spawn — no rule-17 global-plane aliasing)
//   and MUTATES in-match: reflect dmg*=1.5, powershield dmg*=0.5,
//   ILLUSION's ground-type kg patch.
// - hitList: victim slot numbers (upstream reads hitList[0..3];
//   out-of-range reads are undefined == v, never equal).
//
// UPSTREAM QUIRKS carried verbatim (never "fixed"):
// - ILLUSION init's `const isFox = options.isFox || true` is ALWAYS true
//   (falco illusions get fox hb values);
// - LASER wall death tests `wallDetection(i) || timer > 200` — a sweep
//   parameter of exactly 0 is FALSY and does not destroy;
// - destroyArticleQueue may receive DUPLICATE indices (the hurt arm never
//   sets articleDestroyed), so `splice(queue[k] - k, 1)` can go negative:
//   JS splice(-1,1) removes from the END — art_splice1 implements JS
//   ToInteger + negative-start semantics;
// - executeArticleHits' `player[v].hit.hitPoint = instance.pos` ALIASES
//   the article's pos upstream; modeled BY VALUE — unobservable
//   (measured by reading: destroyOnHit lasers never run another main;
//   ILLUSION main REASSIGNS pos before any component write);
// - articlesHitDetection's clank block is commented out upstream ("ILL DO
//   CLANKS TOMOZ"): the guard EVALUATES (id[k].clank reads through both
//   hitbox shapes), the body is empty, attackerClank stays false;
// - `this.strokeStyle/fillStyle` writes in LASER.init are render-only
//   table writes (draw() colours) — documented no-op;
// - renderArticles / LASER.draw are render-only (fg2 canvas) — no C
//   surface (drawECB precedent).
//
// SEAMS (driver-provided): mv_dispatch (executeArticleHits dispatches
// GUARD/SHIELDBREAKFALL/DAMAGEFLYN/DAMAGEN2/CAPTUREDAMAGE .init — all
// shared-origin, task 7's real bodies; unregistered states cross
// mv_seam), hd_flags (crouch/vCancel reads), hd_getKnockback/
// hd_getHitstun/hd_knockbackSounds (task 6's real bodies),
// mv_screenShake (4 seeded draws), ml_drawVfx* / ml_sound_play
// (ml_events.h), mv_attr (CTAB1 weight), ml_art_out_of_domain (rule-7
// traps at upstream throw sites / outside the captured domain).
// percentShake is the CHECKSUM.md §7 native-RNG exclusion (no-op).
#ifndef ML_ARTICLE_H
#define ML_ARTICLE_H

#include "hit_detection.h" // MlSim + hd_* seams (includes physics.h)
#include "ml_input.h"

#define ART_CAP 32
#define ART_HITLIST_CAP 8

typedef enum { ART_LASER, ART_ILLUSION } ArtKind;

typedef struct {
  ArtKind kind;  // aArticles[i].name ("LASER" | "ILLUSION")
  double player; // aArticles[i].player (mutates: reflect/powershield)
  // --- instance ---
  double hitList[ART_HITLIST_CAP];
  int hitListLen;
  bool destroyOnHit; // LASER true, ILLUSION false
  bool clank;        // LASER false, ILLUSION true
  double timer;
  Vec2D pos, posPrev;
  MlHitboxSpec hb; // ML_HB_CHARDATA + offsetSingle (header note)
  Vec2D ecb[4];
  // --- LASER-only keys (presence == kind) ---
  double rotate;
  Vec2D vel;
  Vec2D posPrev1, posPrev2, posPrev3;
} MlArticle;

typedef struct { double a, v; bool shieldHit; } ArtHitRow; // [a, v, bool]

typedef struct {
  MlArticle a[ART_CAP];
  int count; // aArticles
  double destroyQ[ART_CAP];
  int destroyCount; // destroyArticleQueue (persists across the tick)
  ArtHitRow hitQ[ART_CAP];
  int hitCount; // articleHitQueue
} MlArticles;

// options.<key> presence (rule 3: `options.isFox !== undefined` is a
// key-presence test upstream).
typedef struct { bool has; bool v; } ArtOptBool;

// --- the boundaries -----------------------------------------------------

// resetAArticles(): endGame lifecycle upstream (main.js:1376) — zero-live
// over the goldens (matches never end: trace quality contract).
void art_resetAArticles(MlArticles *A);

// articles.LASER.init(options {p,x,y,rotate[,isFox][,partOfThrow]}) —
// spawns AND runs the spawn-frame main (movement + wall check).
void art_laser_init(MlSim *S, MlArticles *A, double p, double x, double y,
                    double rotate, ArtOptBool isFox, ArtOptBool partOfThrow);
// articles.ILLUSION.init(options {p,type[,isFox]})
void art_illusion_init(MlSim *S, MlArticles *A, double p, double type,
                       ArtOptBool isFox);

// gameTick pipeline (main.js:1059-1060, 1077-1078):
void art_executeArticles(MlSim *S, MlArticles *A);
void art_destroyArticles(MlArticles *A);
void art_articlesHitDetection(MlSim *S, MlArticles *A);
void art_executeArticleHits(MlSim *S, MlArticles *A,
                            const MlInputBuffer in[4]);

// Provided by the driver: fatal domain trap (upstream throw sites /
// captured-domain overruns — rule 7).
extern void ml_art_out_of_domain(const char *what);

#endif // ML_ARTICLE_H
