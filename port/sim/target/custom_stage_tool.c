// port/sim/target/custom_stage_tool.c — CHECK-SIDE driver for A45 T2
// (port/sim/target/check-custom-stage.sh). Not linked into any app.
//
// Its whole job is to make the T2 done-check a real DIFFERENTIAL rather
// than a self-consistency test. The strongest available oracle for "does
// a custom stage play correctly" is an AUTHORED stage that already has a
// frozen golden: re-express one as a share code, load it back through the
// custom path, replay the SAME trace, and require byte-identical streams.
// Any error in the codec, the filler, the loader or the setup entry shows
// up as a stream divergence against evidence nobody can quietly adjust.
//
// That differential is only sound because the round trip is EXACT here.
// MEASURED this session over all 10 authored target stages (targets.json,
// executed walk): 210 numbers, ZERO of them lossy at toFixed(2). So the
// emitted code names every authored coordinate exactly and the two paths
// must agree bit for bit — if they ever disagree it is the port, never
// the quantisation.
//
//   --emit <tstageId> <out.mlstage>   TTAB1 -> MlkStage -> code -> file
//   --emit-damage <id> <out>          ... with a fire damageType forced
//                                     onto ground surface 0 (a tooth:
//                                     the plane no golden covers)
//   --emit-targets <id> <n> <out>     ... with the target list padded to
//                                     n entries (a tooth: the R2 cap)
//   --resum <in> <out>                recompute SUM (used to build teeth
//                                     that are VALID files with hostile
//                                     CONTENT, so the content check is
//                                     what bites and not the integrity
//                                     check standing in for it)
//   --load <dir> <slot>               print "OK <targets> <surfaces>" or
//                                     "REFUSED <reason>", exit 0/3
//   --self-test                       the frozen anchors
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "custom_stage.h"
#include "ml_targets.h"
#include "sha256.h"

static void die(const char *m) {
  fprintf(stderr, "custom_stage_tool: %s\n", m);
  exit(1);
}

// --- TTAB1 -> MlkStage (check-side only: the port never needs it) -----------

static void cp_list(SurfaceList *dst, const ml_tstage_surface_t *src,
                    int32_t n) {
  if (n > ML_MAX_SURFACES) die("TTAB1 list over cap");
  dst->count = n;
  for (int32_t k = 0; k < n; k++) {
    memset(&dst->items[k], 0, sizeof dst->items[k]);
    dst->items[k].p0 = vec2d(ml_target_f64(src[k].x1), ml_target_f64(src[k].y1));
    dst->items[k].p1 = vec2d(ml_target_f64(src[k].x2), ml_target_f64(src[k].y2));
  }
}

static void from_ttab1(int id, MlkStage *out) {
  if (id < 0 || id >= ML_TSTAGE_COUNT) die("bad tstage id");
  const ml_tstage_t *st = &ml_tstages[id];
  memset(out, 0, sizeof *out);
  out->startingPointCount = 1;
  out->startingPoint[0] = vec2d(ml_target_f64(st->startingPoint.x),
                                ml_target_f64(st->startingPoint.y));
  // hasStartingFace false: authored target stages carry no startingFace
  // and neither does the builder's stageTemp — encode.js:20-22 then emits
  // "1,1,1,1", which is exactly what upstream would emit for this stage.
  cp_list(&out->s.ground, st->ground, st->groundCount);
  cp_list(&out->s.ceiling, st->ceiling, st->ceilingCount);
  cp_list(&out->s.wallL, st->wallL, st->wallLCount);
  cp_list(&out->s.wallR, st->wallR, st->wallRCount);
  cp_list(&out->s.platform, st->platform, st->platformCount);
  if (st->ledgeCount > ML_MAX_LEDGES) die("TTAB1 ledge list over cap");
  out->ledgeCount = st->ledgeCount;
  for (int32_t k = 0; k < st->ledgeCount; k++) {
    out->ledge[k].list = st->ledge[k].type == 0 ? 'g' : 'p';
    out->ledge[k].index = (double)st->ledge[k].index;
    out->ledge[k].point = (double)st->ledge[k].side;
  }
  if (st->targetCount > MLK_MAX_TARGETS) die("TTAB1 target list over cap");
  out->targetCount = st->targetCount;
  for (int32_t k = 0; k < st->targetCount; k++) {
    out->target[k] = vec2d(ml_target_f64(st->target[k].x),
                           ml_target_f64(st->target[k].y));
  }
  out->blastzone.min = vec2d(ml_target_f64(st->blastzone[0]),
                             ml_target_f64(st->blastzone[1]));
  out->blastzone.max = vec2d(ml_target_f64(st->blastzone[2]),
                             ml_target_f64(st->blastzone[3]));
  out->scale = ml_target_f64(st->scale);
}

// --- .mlstage writing (CHECK-SIDE. The app has no writer — see
// custom_stage.h: when T3/T4 need one it must reuse foh_persist's
// existing atomic publish, not a second file-writing path.) -----------------

static void write_mlstage(const char *path, const char *code) {
  static char buf[MLK_FILE_MAX];
  int n = snprintf(buf, sizeof buf, "MLSTAGE1\n%s\n", code);
  if (n <= 0 || (size_t)n >= sizeof buf) die("code too long to publish");
  uint8_t dig[32];
  sha256((const uint8_t *)buf, (size_t)n, dig);
  static const char *H = "0123456789abcdef";
  char hex[65];
  for (int k = 0; k < 32; k++) {
    hex[2 * k] = H[dig[k] >> 4];
    hex[2 * k + 1] = H[dig[k] & 15];
  }
  hex[64] = 0;
  const int m = snprintf(buf + n, sizeof buf - (size_t)n, "SUM %s\n", hex);
  if (m <= 0 || (size_t)(n + m) >= sizeof buf) die("no room for SUM");
  FILE *f = fopen(path, "wb");
  if (!f) die("cannot open output");
  if (fwrite(buf, 1, (size_t)(n + m), f) != (size_t)(n + m)) die("write");
  if (fclose(f) != 0) die("close");
}

static void emit(const MlkStage *st, const char *path) {
  static char code[MLK_CODE_MAX];
  if (mlk_encode(st, code, sizeof code) < 0) die("encode overflow");
  write_mlstage(path, code);
}

int main(int argc, char **argv) {
  if (argc >= 2 && strcmp(argv[1], "--self-test") == 0) {
    // Anchors, so a broken build fails here and legibly rather than as a
    // puzzling stream divergence five steps later.
    MlkStage st;
    const char *why = "?";
    from_ttab1(0, &st);
    if (!mlk_stage_playable(&st, &why)) die(why);
    MlStageX x;
    tp_stage_from_custom(&st, &x);
    // encode.js:237 — parseStageCode DERIVES `connected` for every custom
    // stage, so the plane is PRESENT (corrected 2026-08-26; this anchor
    // used to assert the opposite, which is why nothing caught the gap —
    // util/get_connected.h carries the argument).
    if (!x.hasConnected) die("custom stage must carry a connected plane");
    if (x.connGroundCount != x.s.ground.count) die("connGround count");
    if (x.connPlatformCount != x.s.platform.count) die("connPlatform count");
    // ...and on the AUTHORED corpus every entry is absent. MEASURED with
    // upstream's own getConnected over all ten target stages: zero links.
    // That is exactly why this leg's byte-identical TTAB1-vs-custom stream
    // comparison stays sound (physics.js:265-300 takes the same arm for an
    // all-null plane as for an absent one) — and it is a MEASUREMENT, not
    // an assumption, so it is pinned here rather than trusted.
    for (int k = 0; k < x.connGroundCount; k++) {
      if (x.connGround[k].l.present || x.connGround[k].r.present) {
        die("authored target stage 1 grew a ground link");
      }
    }
    for (int k = 0; k < x.connPlatformCount; k++) {
      if (x.connPlatform[k].l.present || x.connPlatform[k].r.present) {
        die("authored target stage 1 grew a platform link");
      }
    }
    if (x.respawnCount != 0) die("custom stage must have no respawn points");
    if (x.s.ground.count != ml_tstages[0].groundCount) die("ground count");
    // the damage refusal, exercised directly
    st.s.ground.items[0].hasProps = true;
    st.s.ground.items[0].propsHasDamageTypeKey = true;
    st.s.ground.items[0].propsDamageType.tag = DT_STR;
    snprintf(st.s.ground.items[0].propsDamageType.str,
             sizeof st.s.ground.items[0].propsDamageType.str, "fire");
    if (mlk_stage_playable(&st, &why)) die("a damaging surface must refuse");
    // props with a NULL damageType are inert (upstream BUG 1 emits them)
    st.s.ground.items[0].propsDamageType = damage_null();
    if (!mlk_stage_playable(&st, &why)) die("null damageType must be inert");
    // the R2 cap
    from_ttab1(0, &st);
    st.targetCount = ML_MAX_TARGETS + 1;
    if (mlk_stage_playable(&st, &why)) die("over-cap targets must refuse");
    // the concatenation cap
    from_ttab1(0, &st);
    st.s.ground.count = ML_MAX_SURFACES;
    st.s.ceiling.count = ML_MAX_SURFACES;
    if (mlk_stage_playable(&st, &why)) die("over-concat surfaces must refuse");
    printf("CUSTOM STAGE SELF-TEST OK\n");
    return 0;
  }
  if (argc == 4 && strcmp(argv[1], "--emit") == 0) {
    MlkStage st;
    from_ttab1(atoi(argv[2]), &st);
    emit(&st, argv[3]);
    return 0;
  }
  if (argc == 4 && strcmp(argv[1], "--emit-damage") == 0) {
    MlkStage st;
    from_ttab1(atoi(argv[2]), &st);
    if (st.s.ground.count < 1) die("stage has no ground surface");
    st.s.ground.items[0].hasProps = true;
    st.s.ground.items[0].propsHasDamageTypeKey = true;
    st.s.ground.items[0].propsDamageType.tag = DT_STR;
    snprintf(st.s.ground.items[0].propsDamageType.str,
             sizeof st.s.ground.items[0].propsDamageType.str, "fire");
    emit(&st, argv[3]);
    return 0;
  }
  if (argc == 5 && strcmp(argv[1], "--emit-targets") == 0) {
    MlkStage st;
    from_ttab1(atoi(argv[2]), &st);
    const int n = atoi(argv[3]);
    if (n < 1 || n > MLK_MAX_TARGETS) die("target count out of the codec cap");
    for (int k = st.targetCount; k < n; k++) st.target[k] = st.target[0];
    st.targetCount = n;
    emit(&st, argv[4]);
    return 0;
  }
  if (argc == 4 && strcmp(argv[1], "--resum") == 0) {
    // Re-publish a hand-edited file with a CORRECT SUM, so a content
    // tooth is judged by the content rule and not by the integrity rule.
    static char buf[MLK_FILE_MAX];
    FILE *f = fopen(argv[2], "rb");
    if (!f) die("cannot open input");
    const size_t n = fread(buf, 1, sizeof buf - 1, f);
    fclose(f);
    buf[n] = 0;
    const char *nl = memchr(buf, '\n', n);
    if (!nl) die("no magic line");
    const char *nl2 = memchr(nl + 1, '\n', n - (size_t)(nl + 1 - buf));
    if (!nl2) die("no code line");
    static char code[MLK_CODE_MAX];
    const size_t cl = (size_t)(nl2 - nl - 1);
    if (cl >= sizeof code) die("code too long");
    memcpy(code, nl + 1, cl);
    code[cl] = 0;
    write_mlstage(argv[3], code);
    return 0;
  }
  if (argc == 4 && strcmp(argv[1], "--load") == 0) {
    MlkStage st;
    const char *why = "unknown";
    if (!mlk_slot_load(argv[2], atoi(argv[3]), &st, &why)) {
      printf("REFUSED %s\n", why);
      return 3;
    }
    printf("OK %d %d\n", st.targetCount,
           st.s.ground.count + st.s.ceiling.count + st.s.wallL.count +
               st.s.wallR.count + st.s.platform.count);
    return 0;
  }
  fprintf(stderr, "usage: custom_stage_tool --self-test | --emit <id> <out> | "
                  "--emit-damage <id> <out> | --emit-targets <id> <n> <out> | "
                  "--resum <in> <out> | --load <dir> <slot>\n");
  return 1;
}
