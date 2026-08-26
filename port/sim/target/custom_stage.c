// port/sim/target/custom_stage.c — A45 T2 (D42/D43). See custom_stage.h
// for the contract, the transport, the damage refusal and the D43 ruling.
#include "custom_stage.h"

#include <stdio.h>
#include <string.h>

#include "sha256.h" // oracle/qjs/sha256.c (-Ioracle/qjs), the foh_persist
                    // SUM idiom reused rather than re-invented
#include "../util/get_connected.h" // encode.js:237 — see the note at
                                   // tp_stage_from_custom

// --- the ONE validator ------------------------------------------------------

// Local so a caller cannot skip it by calling the helper directly.
#define REFUSE(msg)             \
  do {                          \
    if (reason) *reason = (msg); \
    return false;               \
  } while (0)

static bool list_has_damage(const SurfaceList *l) {
  for (int k = 0; k < l->count; k++) {
    const Surface *s = &l->items[k];
    // physics reads `wall[2] !== undefined ? wall[2].damageType : null`
    // and then tests it for TRUTHINESS. Props with a null damageType are
    // therefore inert — and they are not exotic: upstream BUG 1
    // (encode.js:39) emits exactly that for the 6th surface of every
    // type, so refusing them would reject codes upstream plays fine.
    // The live case, and the only one refused, is a real type string.
    if (s->hasProps && s->propsDamageType.tag == DT_STR) return true;
  }
  return false;
}

bool mlk_stage_playable(const MlkStage *st, const char **reason) {
  if (reason) *reason = NULL;
  // (1) targets: the codec's 20 vs the sim's 10 — design risk R2. Loud
  //     refusal, never truncation (the T1 cap discipline).
  if (st->targetCount < 1) REFUSE("stage has no targets");
  if (st->targetCount > ML_MAX_TARGETS) {
    REFUSE("too many targets for this build (max 10; the share code allows "
           "20 — raising the cap is an owner ruling, R2)");
  }
  // (2) the concatenation cap: four 64-surface lists each pass their own
  //     cap and together overflow runCollisionRoutine's single list.
  {
    const long total = (long)st->s.wallL.count + st->s.wallR.count +
                       st->s.ground.count + st->s.ceiling.count +
                       st->s.platform.count;
    if (total > ML_MAX_LABELLED_SURFACES) {
      REFUSE("too many collision surfaces in total (max 96 across all five "
             "lists — the collision routine concatenates them)");
    }
  }
  // (3) the damage plane, which no golden covers (header note (3)).
  if (list_has_damage(&st->s.ground) || list_has_damage(&st->s.ceiling) ||
      list_has_damage(&st->s.wallL) || list_has_damage(&st->s.wallR) ||
      list_has_damage(&st->s.platform)) {
    REFUSE("stage carries a damaging surface: that plane has never executed "
           "and no golden covers it (A45 T6 owes one) — refusing rather "
           "than running untested collision code");
  }
  // startingPoint[0] is what startTargetGame reads (targetplay.js:196);
  // mlk_parse already refuses a code without one, so this is a belt on
  // the direct-caller path rather than a second parser rule.
  if (st->startingPointCount < 1) REFUSE("stage has no starting point");
  return true;
}

#undef REFUSE

// --- the sim side -----------------------------------------------------------

void tp_stage_from_custom(const MlkStage *cs, MlStageX *out) {
  const char *why = NULL;
  if (!mlk_stage_playable(cs, &why)) sim_fatal(why);
  memset(out, 0, sizeof *out);
  // The five collision lists ARE the physics read set and T1 already
  // modelled them as the sim's own Stage — so this is an assignment, not
  // a conversion. That is the whole reason the spike's "one new filler"
  // estimate held.
  out->s = cs->s;
  // encode.js:237 — parseStageCode ENDS with `stage.connected =
  // getConnected(stage)`, so every custom stage upstream plays carries a
  // derived connected plane. This used to write `false` with the comment
  // "no `connected` field in the code grammar", which is true about the
  // GRAMMAR and wrong about the BEHAVIOUR: upstream does not read connected
  // out of the code, it derives it from the surfaces. The full argument,
  // and the measurement that shows why no existing check could see the
  // difference, is at the top of util/get_connected.h.
  out->hasConnected = true;
  getConnected(&out->s, out->connGround, &out->connGroundCount,
               out->connPlatform, &out->connPlatformCount);
  out->ledgeCount = cs->ledgeCount;
  for (int k = 0; k < cs->ledgeCount; k++) out->ledge[k] = cs->ledge[k];
  out->blastzone = cs->blastzone;
  out->respawnCount = 0; // target mode: isFinalDeath() is unconditional
}

void tp_setup_target_custom(GameState *g, int charId, int slot,
                            const MlkStage *cs) {
  if (slot < 0 || slot >= MLK_MAX_SLOTS) sim_fatal("custom slot out of range");
  MlStageX stage;
  tp_stage_from_custom(cs, &stage); // validates, or dies naming the rule
  tp_setup_target_core(g, charId, (double)(MLK_PLAYING_BASE + slot), &stage,
                       cs->target, cs->targetCount, cs->startingPoint[0]);
}

// --- the .mlstage file plane ------------------------------------------------

#define MLK_MAGIC "MLSTAGE1"

bool mlk_slot_path(const char *dir, int slot, char *buf, size_t cap) {
  if (slot < 0 || slot >= MLK_MAX_SLOTS) return false;
  const int n = snprintf(buf, cap, "%s/custom%d.mlstage", dir, slot);
  return n > 0 && (size_t)n < cap;
}

static void hex32(const uint8_t d[32], char out[65]) {
  static const char *H = "0123456789abcdef";
  for (int k = 0; k < 32; k++) {
    out[2 * k] = H[d[k] >> 4];
    out[2 * k + 1] = H[d[k] & 15];
  }
  out[64] = 0;
}

// Read the whole file, bounded. Refuses (rather than truncating) anything
// larger than MLK_FILE_MAX — a file that does not fit is exactly the
// power-loss / wrong-file case the read path exists to catch.
static bool slurp(const char *path, char *buf, size_t cap, size_t *n,
                  const char **reason) {
  FILE *f = fopen(path, "rb");
  if (!f) {
    if (reason) *reason = "no such slot file";
    return false;
  }
  const size_t got = fread(buf, 1, cap, f);
  // A short read is only trustworthy at EOF; ferror must be consulted
  // because on this device /mnt is mounted errors=remount-ro and can go
  // read-only (and start erroring) underneath a running session.
  const bool err = ferror(f) != 0;
  const bool more = feof(f) == 0;
  fclose(f);
  if (err) {
    if (reason) *reason = "read error";
    return false;
  }
  if (more) {
    if (reason) *reason = "file too large for a stage code";
    return false;
  }
  *n = got;
  return true;
}

bool mlk_slot_load(const char *dir, int slot, MlkStage *out,
                   const char **reason) {
  if (reason) *reason = NULL;
  char path[512];
  if (!mlk_slot_path(dir, slot, path, sizeof path)) {
    if (reason) *reason = "slot path too long";
    return false;
  }
  // MLK_FILE_MAX + 1 so a file of exactly the cap still hits EOF and a
  // file one byte over is caught by `more` instead of being truncated.
  static char buf[MLK_FILE_MAX + 1];
  static char code[MLK_CODE_MAX + 1];
  size_t n = 0;
  if (!slurp(path, buf, sizeof buf, &n, reason)) return false;

  // Grammar, checked BEFORE anything is trusted: exactly
  //   "MLSTAGE1\n" <code> "\n" "SUM " <64 hex> "\n"
  // and not one byte more. Strict and anchored, the foh_persist.c:218-222
  // posture — a lenient grammar here is how a corrupt file becomes a
  // plausible-looking stage.
  const size_t magic = sizeof MLK_MAGIC - 1; // no NUL
  const size_t sumLine = 4 + 64 + 1;         // "SUM " + hex + "\n"
  if (n < magic + 1 + 1 + sumLine ||
      memcmp(buf, MLK_MAGIC "\n", magic + 1) != 0) {
    if (reason) *reason = "not a .mlstage file (bad magic line)";
    return false;
  }
  if (buf[n - 1] != '\n') {
    if (reason) *reason = "truncated file (no final newline)";
    return false;
  }
  const size_t sumStart = n - sumLine;
  if (memcmp(buf + sumStart, "SUM ", 4) != 0) {
    if (reason) *reason = "missing SUM line";
    return false;
  }
  // The code is everything between the magic line and the SUM line, and
  // must be exactly ONE line: an embedded newline means extra lines the
  // grammar does not have.
  const size_t codeStart = magic + 1;
  if (sumStart == codeStart || buf[sumStart - 1] != '\n') {
    if (reason) *reason = "missing code line";
    return false;
  }
  const size_t codeLen = sumStart - codeStart - 1;
  if (memchr(buf + codeStart, '\n', codeLen) != NULL) {
    if (reason) *reason = "unexpected extra line";
    return false;
  }
  if (codeLen >= sizeof code) {
    if (reason) *reason = "code too long";
    return false;
  }

  // SUM over every preceding byte (foh_persist.c:154's own definition),
  // verified BEFORE the code is parsed: integrity first, then meaning.
  {
    uint8_t dig[32];
    char hex[65];
    sha256((const uint8_t *)buf, sumStart, dig);
    hex32(dig, hex);
    for (int k = 0; k < 64; k++) {
      const char c = buf[sumStart + 4 + k];
      if (c != hex[k]) {
        if (reason) {
          *reason = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f')
                        ? "SUM mismatch (file corrupt or edited)"
                        : "malformed SUM (not 64 lowercase hex)";
        }
        return false;
      }
    }
  }

  memcpy(code, buf + codeStart, codeLen);
  code[codeLen] = 0;
  if (!mlk_parse(code, out, reason)) return false;
  return mlk_stage_playable(out, reason);
}

// --- the tp_custom_setup seam (target_play.h) -------------------------------

static bool custom_setup(GameState *g, int charId, const char *dir, int slot,
                         const char **why) {
  // One resident stage: the sim copies what it needs out of it at setup.
  static MlkStage cs;
  if (!mlk_slot_load(dir, slot, &cs, why)) return false;
  tp_setup_target_custom(g, charId, slot, &cs);
  return true;
}

__attribute__((constructor)) static void install_custom_setup(void) {
  tp_custom_setup = custom_setup;
}

void mlk_slots_scan(const char *dir, MlkSlots *out) {
  // One MlkStage of scratch, reused: presence is what the menu needs, and
  // ten resident ~45 KB stages are ~450 KB this device would rather keep.
  static MlkStage scratch;
  for (int i = 0; i < MLK_MAX_SLOTS; i++) {
    const char *why = NULL;
    out->present[i] = mlk_slot_load(dir, i, &scratch, &why);
    out->reason[i] = out->present[i] ? NULL : why;
  }
}
