// sim_ser.c — per-frame CHECKSUM.md §2/§3/§4 emission for the integrated
// sim (sim.h sim_frame_hash; fix_plan §M2 task 17).
//
// COMPOSITION, no new serialization logic: every emitter here is either a
// direct call into the verified task-2/-15 TUs or a verbatim lift of an
// already-differentially-verified static function (cited per block):
//
//   1. each present player  -> ser_player (port/sim/calib/player_canon.c,
//      task 2's canon v1.1 serializer) into a reused CanonBuf;
//   2. the aArticles queue  -> ser_vec / ser_art_hb / ser_article /
//      ser_aArt, LIFTED VERBATIM from port/sim/calib/replay_article.c
//      (task 13's capture-verified article canon);
//   3. canon_arena_reset + canon_parse (port/sim/calib/canon.c) turn the
//      canon bytes back into CanonVal trees;
//   4. ser_canon + ser_envelope, LIFTED from port/sim/calib/fmt_diff.c
//      (task 15's composite mode, proven byte-identical to the oracle's
//      __serializeState — pagelib.js:41-64), build the §3.1 fixed-literal-
//      order frame envelope (active players p0..p3, each with the SEVEN
//      allow-listed fields actionState/timer/percent/stocks/hit/hitboxes/
//      phys, then "articles") into an MlSb;
//   5. ml_sha256_hex (port/sim/ml_ser.c §4) -> lowercase hex digest.
//
// The only adaptation to the lifted code: fmt_diff.c's error paths
// (fprintf + exit(2), line-number context) become sim_fatal (sim.h) with
// the current frame as context — any impossible state (article queue over
// cap, canon parse error, a player tree missing one of the 7 envelope
// fields) aborts loudly, HARD RULE 2.
//
// Buffers are process-static and reused across frames (one match per
// process): no per-frame mallocs after warm-up beyond canon-arena growth.
//
// NOTE: player_canon.c declares `extern void pc_fail(const char *)`; the
// HOST driver defines it (this TU deliberately does not).

#include "sim.h"

#include "../calib/canon.h"
#include "../calib/player_canon.h"
#include "../ml_ser.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

// --- fatal-with-context helper ------------------------------------------------

static void ser_fatal(const char *what, uint64_t frame) {
  static char msg[192];
  snprintf(msg, sizeof msg, "sim_ser (frame %" PRIu64 "): %s", frame, what);
  sim_fatal(msg);
}

// --- article-queue canon -------------------------------------------------------
// LIFTED VERBATIM from port/sim/calib/replay_article.c (ser_vec:728,
// ser_art_hb:736, ser_article:828, ser_aArt:873) — the task-13
// capture-verified serializers for CHECKSUM.md §2's `articles` key
// (aArticles entries {instance,name,player}, sorted-key canon v1.1).

static void ser_vec(CanonBuf *b, Vec2D v) {
  cb_puts(b, "{\"x\":");
  cb_num(b, v.x);
  cb_puts(b, ",\"y\":");
  cb_num(b, v.y);
  cb_putc(b, '}');
}

static void ser_art_hb(CanonBuf *b, const MlHitboxSpec *hb) {
  cb_puts(b, "{\"angle\":");
  cb_num(b, hb->angle);
  cb_puts(b, ",\"bk\":");
  cb_num(b, hb->bk);
  cb_puts(b, ",\"clank\":");
  cb_num(b, hb->clank);
  cb_puts(b, ",\"dmg\":");
  cb_num(b, hb->dmg);
  cb_puts(b, ",\"hitAirborne\":");
  cb_num(b, hb->hitAirborne);
  cb_puts(b, ",\"hitGrounded\":");
  cb_num(b, hb->hitGrounded);
  cb_puts(b, ",\"kg\":");
  cb_num(b, hb->kg);
  cb_puts(b, ",\"offset\":");
  ser_vec(b, hb->offset);
  cb_puts(b, ",\"size\":");
  cb_num(b, hb->size);
  cb_puts(b, ",\"sk\":");
  cb_num(b, hb->sk);
  cb_puts(b, ",\"throwextra\":");
  cb_puts(b, hb->throwextra ? "T" : "F");
  cb_puts(b, ",\"type\":");
  cb_num(b, hb->type);
  cb_putc(b, '}');
}

static void ser_article(CanonBuf *b, const MlArticle *it) {
  const bool isLaser = it->kind == ART_LASER;
  cb_puts(b, "{\"instance\":{\"clank\":");
  cb_puts(b, it->clank ? "T" : "F");
  cb_puts(b, ",\"destroyOnHit\":");
  cb_puts(b, it->destroyOnHit ? "T" : "F");
  cb_puts(b, ",\"ecb\":[");
  for (int i = 0; i < 4; i++) {
    if (i) cb_putc(b, ',');
    ser_vec(b, it->ecb[i]);
  }
  cb_puts(b, "],\"hb\":");
  ser_art_hb(b, &it->hb);
  cb_puts(b, ",\"hitList\":[");
  for (int i = 0; i < it->hitListLen; i++) {
    if (i) cb_putc(b, ',');
    cb_num(b, it->hitList[i]);
  }
  cb_puts(b, "],\"pos\":");
  ser_vec(b, it->pos);
  cb_puts(b, ",\"posPrev\":");
  ser_vec(b, it->posPrev);
  if (isLaser) {
    cb_puts(b, ",\"posPrev1\":");
    ser_vec(b, it->posPrev1);
    cb_puts(b, ",\"posPrev2\":");
    ser_vec(b, it->posPrev2);
    cb_puts(b, ",\"posPrev3\":");
    ser_vec(b, it->posPrev3);
    cb_puts(b, ",\"rotate\":");
    cb_num(b, it->rotate);
  }
  cb_puts(b, ",\"timer\":");
  cb_num(b, it->timer);
  if (isLaser) {
    cb_puts(b, ",\"vel\":");
    ser_vec(b, it->vel);
  }
  cb_puts(b, "},\"name\":");
  cb_qstr(b, isLaser ? "LASER" : "ILLUSION");
  cb_puts(b, ",\"player\":");
  cb_num(b, it->player);
  cb_putc(b, '}');
}

static void ser_aArt(CanonBuf *b, const MlArticles *A) {
  cb_putc(b, '[');
  for (int i = 0; i < A->count; i++) {
    if (i) cb_putc(b, ',');
    ser_article(b, &A->a[i]);
  }
  cb_putc(b, ']');
}

// --- CHECKSUM.md ser over parsed canon -------------------------------------------
// LIFTED from port/sim/calib/fmt_diff.c (ser_canon:444, obj_get:480,
// ser_envelope:501) — task 15's composite mode, differentially proven
// byte-identical to the oracle's __serializeState/__sha256
// (pagelib.js:41-73) over every g01 snapshot. Only change: error paths
// route to sim_fatal (via ser_fatal) instead of fprintf+exit(2), and the
// diagnostic context is the FRAME number rather than a case-file line.

// The generic ser walk: CHECKSUM.md §3 over a parsed canon tree. The
// parsed domain is trees (canon.c enforces), so the seen-set/cycle rule
// (§3.9) reduces to emitting the pre-collapsed "cyc" token verbatim.
static void ser_canon(MlSb *sb, const CanonVal *v) {
  switch (v->type) {
    case CV_NULL: ml_sb_puts(sb, "null"); return;   // §3.7 (pagelib.js:15)
    case CV_UNDEF: ml_sb_puts(sb, "undef"); return; // §3.7 (pagelib.js:20)
    case CV_FN: ml_sb_puts(sb, "fn"); return;       // §3.8 (pagelib.js:21)
    case CV_CYC: ml_sb_puts(sb, "cyc"); return;     // §3.9 (pagelib.js:22)
    case CV_BOOL: ml_sb_bool(sb, v->b); return;     // §3.6 (pagelib.js:19)
    case CV_NUM: ml_sb_num(sb, v->num); return;     // §3.4 (pagelib.js:10-13,17)
    case CV_STR: ml_sb_jsonstr(sb, v->str); return; // §3.5 (pagelib.js:18)
    case CV_ARR:                                    // §3.3 (pagelib.js:25-28)
      ml_sb_putc(sb, '[');
      for (int i = 0; i < v->count; ++i) {
        if (i) ml_sb_putc(sb, ',');
        ser_canon(sb, v->items[i]);
      }
      ml_sb_putc(sb, ']');
      return;
    case CV_OBJ:                                    // §3.2 (pagelib.js:30-33)
      // canon keys are serialized in sorted order (canon.h) == the order
      // pagelib's ser emits.
      ml_sb_putc(sb, '{');
      for (int i = 0; i < v->nkeys; ++i) {
        if (i) ml_sb_putc(sb, ',');
        ml_sb_jsonstr(sb, v->keys[i]);
        ml_sb_putc(sb, ':');
        ser_canon(sb, v->vals[i]);
      }
      ml_sb_putc(sb, '}');
      return;
  }
  sim_fatal("sim_ser: bad canon type");
}

static const CanonVal *obj_get(const CanonVal *obj, const char *key) {
  if (obj->type != CV_OBJ) return NULL;
  for (int i = 0; i < obj->nkeys; ++i)
    if (strcmp(obj->keys[i], key) == 0) return obj->vals[i];
  return NULL;
}

// The §2/§3.1 frame envelope, byte-for-byte pagelib.js:41-64
// (__serializeState): active players (ptype[i] > -1) in slot order, each
// with the SEVEN allow-listed fields in FIXED literal order, then
// "articles".
static void ser_envelope(MlSb *sb, const int *ptype, const CanonVal **players,
                         const CanonVal *articles, uint64_t frame) {
  static const char *kFields[] = {"actionState", "timer",    "percent", "stocks",
                                  "hit",         "hitboxes", "phys"};
  ml_sb_putc(sb, '{');
  bool first = true;
  for (int i = 0; i < 4; ++i) {
    if (ptype[i] <= -1) continue; // inactive slots are omitted entirely (§2)
    if (!players[i])
      ser_fatal("active slot has no player snapshot", frame);
    if (!first) ml_sb_putc(sb, ',');
    first = false;
    char pk[8];
    snprintf(pk, sizeof pk, "\"p%d\":", i);
    ml_sb_puts(sb, pk);
    ml_sb_putc(sb, '{');
    for (size_t k = 0; k < sizeof kFields / sizeof kFields[0]; ++k) {
      const CanonVal *fv = obj_get(players[i], kFields[k]);
      if (!fv)
        ser_fatal("player tree missing an envelope field", frame);
      if (k) ml_sb_putc(sb, ',');
      ml_sb_putc(sb, '"');
      ml_sb_puts(sb, kFields[k]);
      ml_sb_puts(sb, "\":");
      ser_canon(sb, fv);
    }
    ml_sb_putc(sb, '}');
  }
  if (!first) ml_sb_putc(sb, ',');
  ml_sb_puts(sb, "\"articles\":");
  ser_canon(sb, articles);
  ml_sb_putc(sb, '}');
}

// --- the per-frame hash ------------------------------------------------------------

// Process-static reused buffers (one match per process; sim.h perf note).
static CanonBuf g_pcanon[4]; // per-slot player canon bytes
static CanonBuf g_acanon;    // article-queue canon bytes
static MlSb g_env;           // the §3.1 envelope bytes (the hashed string)
static bool g_bufs_ready = false;

static void cb_reset_keep(CanonBuf *b) {
  b->len = 0;
  b->buf[0] = 0;
}

void sim_frame_hash(const GameState *g, char out_hex[65]) {
  const uint64_t frame = (uint64_t) g->frame;
  if (!g_bufs_ready) {
    for (int i = 0; i < 4; ++i) cb_init(&g_pcanon[i]);
    cb_init(&g_acanon);
    ml_sb_init(&g_env);
    g_bufs_ready = true;
  }

  // impossible-state guards (rule 7: abort loudly, never truncate)
  if (g->arts.count < 0 || g->arts.count > ART_CAP)
    ser_fatal("article queue count outside [0, ART_CAP]", frame);
  for (int i = 0; i < g->arts.count; ++i) {
    if (g->arts.a[i].hitListLen < 0 ||
        g->arts.a[i].hitListLen > ART_HITLIST_CAP)
      ser_fatal("article hitList length outside cap", frame);
  }

  // 1+2. live state -> canon v1.1 bytes (task-2 / task-13 serializers)
  for (int i = 0; i < 4; ++i) {
    if (!g->sim.playerPresent[i]) continue;
    cb_reset_keep(&g_pcanon[i]);
    ser_player(&g_pcanon[i], &g->sim.player[i]);
  }
  cb_reset_keep(&g_acanon);
  ser_aArt(&g_acanon, &g->arts);

  // 3. canon bytes -> parsed trees (per-frame arena)
  canon_arena_reset();
  const CanonVal *players[4] = {NULL, NULL, NULL, NULL};
  int ptype[4];
  for (int i = 0; i < 4; ++i) {
    ptype[i] = g->sim.playerPresent[i] ? 0 : -1; // >-1 == active (§2)
    if (!g->sim.playerPresent[i]) continue;
    const char *err = NULL;
    players[i] = canon_parse(g_pcanon[i].buf, &err);
    if (!players[i]) ser_fatal("player canon parse error", frame);
  }
  const char *err = NULL;
  const CanonVal *articles = canon_parse(g_acanon.buf, &err);
  if (!articles) ser_fatal("articles canon parse error", frame);

  // 4. the §3.1 fixed-literal-order frame envelope
  ml_sb_reset(&g_env);
  ser_envelope(&g_env, ptype, players, articles, frame);

  // 5. §4: SHA-256 lowercase hex over the envelope bytes
  ml_sha256_hex(g_env.buf, g_env.len, out_hex);
}

// Diagnostic accessor (sim.h): the envelope bytes of the LAST
// sim_frame_hash call — the M2CAL divergence-localization instrument
// (byte-diff against the oracle's --capture-frames dump of the same
// frame; sim_main --dump-frames). Also serves the SIM_SER_TEST self-test.
const char *sim_frame_envelope(size_t *len) {
  if (len) *len = g_env.len;
  return g_env.buf;
}
#ifdef SIM_SER_TEST
const char *sim_ser_test_envelope(size_t *len) { return sim_frame_envelope(len); }
#endif
