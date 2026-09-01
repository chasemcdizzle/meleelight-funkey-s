// port/foh/foh_target_snap.c — ticket #30: the lid-close seam for a TARGET RUN.
//
// The header states the interface, the four rules and the custom-stage
// decision. This file is where the two writes are ORDERED and where the read
// path refuses.
//
// WRITE ORDER, AND THE TORN PAIR — foh_match_snap.c's argument, inherited
// whole because it is the same argument:
//
//   (1) unlink the header      -> the pair is now DISARMED
//   (2) ss_save the snapshot   -> fsync + atomic rename
//   (3) publish the header     -> the SAME atomic publish the settings record,
//                                 the builder document and the match pair use
//   ...and only then does the caller write the settings record, which is what
//   actually arms the resume (`resume` row == FOH_TMATCH).
//
// Unlinking FIRST is the whole torn-pair argument. The dangerous state is a
// NEW snapshot beside an OLD header: the header would name a stage, a
// character and a frame belonging to a run that is gone, and the resume would
// set a run up wrong and then load a snapshot into it. Removing the header
// before writing anything means every interruption lands on "no header", which
// is the disarmed state. The reverse tear (a header with no snapshot) is
// refused by the load, by name.
//
// Belt and braces on top of that, because unlink() on vfat is not a
// transaction: `restore` compares the restored G.frame, the restored
// characterSelections[0] and the REBUILT TP.targetStagePlaying against what the
// header promised, and compares the header's SRC against the stage the launch
// actually loaded. A pair that is mismatched in any of those ways is refused
// instead of played.
//
// VALIDATE ON READ, ALWAYS (the .mlstage contract, A45 T2, inherited whole):
// bounded read, strict anchored grammar, SUM verified BEFORE anything is
// parsed, every refusal names its rule.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../../oracle/qjs/sha256.h"
#include "../sim/sim/sim_snapshot.h"
#include "../sim/target/custom_stage.h" // MLK_PLAYING_BASE, MLK_MAX_SLOTS
#include "../sim/target/target_play.h"  // TP (the rebuilt derived rows)
#include "foh_persist.h"
#include "foh_target_snap.h"

#define TS_MAGIC "MLTMATCH1\n"
// The header is a fixed shape, so its size is known and a read that is not
// exactly this long is refused before it is looked at.
//   "MLTMATCH1\n"                                 10
//   "TSTAGE " <2 digits> "\n"                     10
//   "CHAR " <1 digit> "\n"                         7
//   "FRAME " <12 zero-padded digits> "\n"         19
//   "SRC " <64 hex> "\n"                          69
//   "BUILD " <64 hex> "\n"                        71
//   "SUM " <64 hex> "\n"                          69
#define TS_HDR_BODY ((size_t)(10 + 10 + 7 + 19 + 69 + 71))
#define TS_HDR_SUM ((size_t)(4 + 64 + 1))
#define TS_HDR_BYTES (TS_HDR_BODY + TS_HDR_SUM)

// Field offsets, written once so the writer's format string and the reader's
// anchors cannot drift apart silently.
#define TS_OFF_TSTAGE ((size_t)10)
#define TS_OFF_CHAR (TS_OFF_TSTAGE + 10)
#define TS_OFF_FRAME (TS_OFF_CHAR + 7)
#define TS_OFF_SRC (TS_OFF_FRAME + 19)
#define TS_OFF_BUILD (TS_OFF_SRC + 69)
_Static_assert(TS_OFF_BUILD + 71 == TS_HDR_BODY,
               "the MLTMATCH1 field offsets do not add up to the body length");

// The FRAME field is 12 zero-padded digits, so the FORMAT's ceiling is
// 999999999999 — about 2.3 million years at 60 fps.
//
// The comment that stood here claimed that "still fits a 32-bit long". It does
// not, and nobody found out because this TU had never been compiled for the
// device at all: it was missing from riglib.sh's foh_device recipe, so the
// whole resume feature was absent from the shipped app. MEASURED 2026-08-28,
// armv7 gcc 10.2 — `frame > TS_FRAME_MAX` is a comparison the compiler can
// PROVE vacuous (-Werror=type-limits), a 32-bit long topping out at 10 digits.
//
// Both bounds are real and neither is dropped. Where `long` is the wider type
// the format ceiling is a live refusal; where it is the narrower one the
// ceiling is unreachable and the comparison is compiled out rather than
// written and ignored.
#define TS_FRAME_MAX 999999999999LL

// The tstage domain, restated from the ONE place that owns it rather than
// retyped: 0..MLK_MAX_SLOTS-1 are the authored stages and
// MLK_PLAYING_BASE + 0..MLK_MAX_SLOTS-1 are the custom slots
// (custom_stage.h; MLK_PLAYING_BASE == MLK_MAX_SLOTS).
#define TS_TSTAGE_MAX (MLK_PLAYING_BASE + MLK_MAX_SLOTS - 1)
_Static_assert(TS_TSTAGE_MAX <= 99,
               "the header's TSTAGE column is two digits wide");

static void ts_hex32(const uint8_t d[32], char out[65]) {
  static const char *H = "0123456789abcdef";
  for (int k = 0; k < 32; k++) {
    out[2 * k] = H[d[k] >> 4];
    out[2 * k + 1] = H[d[k] & 15];
  }
  out[64] = 0;
}

// --- SRC: the stage's source identity (foh_target_snap.h) --------------------
// ONE definition, called by the launch (write side) and by the resume (read
// side). For a custom slot it digests the stage's canonical share code, so it
// describes the STAGE rather than the file's incidental bytes; for an authored
// stage it digests a deterministic descriptor, because that geometry is the
// compiled TTAB1 table and the header's BUILD line is what pins it.
static void ts_src(int tstage, const MlkStage *custom, char out[65]) {
  uint8_t dig[32];
  if (tstage >= MLK_PLAYING_BASE) {
    if (!custom) {
      // Unreachable from foh_dev.c, which loads the slot before it asks — and
      // a silent digest of nothing would be a resume that accepted any stage.
      gfx_fatal("foh_target_snap: a custom slot with no loaded stage");
    }
    // MLK_CODE_MAX is 128 KB, which is far too large for a stack frame on this
    // device; the buffer is static for the same reason mlk_slot_load's is, and
    // this is called twice per process at most.
    static char code[MLK_CODE_MAX + 1];
    const int n = mlk_encode(custom, code, sizeof code);
    if (n <= 0) gfx_fatal("foh_target_snap: the loaded stage does not encode");
    sha256((const uint8_t *)code, (size_t)n, dig);
  } else {
    char desc[32];
    const int n = snprintf(desc, sizeof desc, "authored:%d", tstage);
    if (n <= 0 || (size_t)n >= sizeof desc) {
      gfx_fatal("foh_target_snap: authored descriptor overflow");
    }
    sha256((const uint8_t *)desc, (size_t)n, dig);
  }
  ts_hex32(dig, out);
}

static bool ts_path(char *dst, size_t cap, const char *name) {
  const int w = snprintf(dst, cap, "%s/%s", foh_persist_dir(), name);
  return w > 0 && (size_t)w < cap;
}

static void ts_disarm(void) {
  char p[512];
  // The HEADER first, for the same reason it is unlinked first on the write
  // path: while it is gone the pair is disarmed, whatever happens next.
  if (ts_path(p, sizeof p, FOH_TMATCH_SNAP_HDR)) (void)remove(p);
  if (ts_path(p, sizeof p, FOH_TMATCH_SNAP_FILE)) (void)remove(p);
}

static bool ts_hexrun(const char *s, int n) {
  for (int k = 0; k < n; k++) {
    const char c = s[k];
    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
  }
  return true;
}

static bool ts_arm(const GameState *g, int tstage, int charId,
                   const char *srcHex, const char **why) {
  if (why) *why = 0;
  if (!g) {
    if (why) *why = "no run state";
    return false;
  }
  // Everything that rides in the header is range-checked on the way OUT as
  // well as on the way in: a header that could not have been written by a
  // legal launch is a header nobody has to reason about later.
  if (tstage < 0 || tstage > TS_TSTAGE_MAX) {
    if (why) *why = "target stage outside the header's domain";
    return false;
  }
  if (charId < 0 || charId > 9) {
    if (why) *why = "character outside the header's domain";
    return false;
  }
#if LONG_MAX > TS_FRAME_MAX
  if (g->frame < 0 || g->frame > TS_FRAME_MAX) {
#else
  if (g->frame < 0) { // the ceiling is unreachable by the type on this target
#endif
    if (why) *why = "frame outside the header's domain";
    return false;
  }
  if (!srcHex || !ts_hexrun(srcHex, 64) || srcHex[64] != 0) {
    if (why) *why = "stage source identity is not a digest";
    return false;
  }
  // THIS TICKET'S OWN RULE (foh_target_snap.h): a FINISHED run is not armed.
  // Its record has already been written through the improve-or-first
  // chokepoint and the settings save this hibernate is about to do carries it;
  // re-entering the finish path on a resume is the one way this feature could
  // double-count one. There is nothing to continue, so the honest answer is to
  // say so and let the boot land on target select.
  if (TP.gameEnd) {
    if (why) *why = "the target run has already finished";
    return false;
  }

  char hdrPath[512], simPath[512];
  if (!ts_path(hdrPath, sizeof hdrPath, FOH_TMATCH_SNAP_HDR) ||
      !ts_path(simPath, sizeof simPath, FOH_TMATCH_SNAP_FILE)) {
    if (why) *why = "persist path too long";
    return false;
  }

  // (1) DISARM FIRST. Everything after this point is safe to be interrupted.
  (void)remove(hdrPath);

  // (2) the snapshot — #28's writer, #28's atomic publish. The target plane
  // rides it as the `mod:targets` row (#30 step 1).
  const char *ssWhy = 0;
  if (!ss_save(g, simPath, &ssWhy)) {
    if (why) *why = ssWhy ? ssWhy : "snapshot write failed";
    (void)remove(simPath);
    return false;
  }

  // (3) the header. Built into a fixed-size buffer with its own SUM, then
  // handed to the ONE publish.
  char buf[TS_HDR_BYTES + 1];
  char id[65];
  ss_build_identity(id);
  const int w =
      snprintf(buf, TS_HDR_BODY + 1,
               TS_MAGIC "TSTAGE %02d\nCHAR %d\nFRAME %012ld\nSRC %s\nBUILD %s\n",
               tstage, charId, g->frame, srcHex, id);
  if (w < 0 || (size_t)w != TS_HDR_BODY) {
    if (why) *why = "header format";
    (void)remove(simPath);
    return false;
  }
  uint8_t dig[32];
  char hex[65];
  sha256((const uint8_t *)buf, TS_HDR_BODY, dig);
  ts_hex32(dig, hex);
  const int w2 = snprintf(buf + TS_HDR_BODY, TS_HDR_SUM + 1, "SUM %s\n", hex);
  if (w2 < 0 || (size_t)w2 != TS_HDR_SUM) {
    if (why) *why = "header SUM format";
    (void)remove(simPath);
    return false;
  }
  const char *pubWhy = 0;
  if (!foh_persist_publish(FOH_TMATCH_SNAP_HDR, buf, TS_HDR_BYTES, &pubWhy)) {
    if (why) *why = pubWhy ? pubWhy : "header publish failed";
    // The snapshot is orphaned; remove it so the card does not carry bytes
    // nothing will ever read.
    (void)remove(simPath);
    return false;
  }
  // FOH_PUBLISH_NODIRSYNC is a SUCCESS with a caveat (foh_persist.h says so).
  // The settings record accepts it, the builder document accepts it, the match
  // pair accepts it, and so does this: the rename landed.
  return true;
}

// --- the read path ----------------------------------------------------------

#define TS_REFUSE(msg)                                                         \
  do {                                                                         \
    if (why) *why = (msg);                                                     \
    return false;                                                              \
  } while (0)

// Exactly `n` decimal digits at `s`, value out. Anchored: no sign, no space,
// no leading-plus, nothing clever.
static bool ts_digits(const char *s, int n, long *out) {
  long v = 0;
  for (int k = 0; k < n; k++) {
    if (s[k] < '0' || s[k] > '9') return false;
    v = v * 10 + (s[k] - '0');
  }
  *out = v;
  return true;
}

static bool ts_peek(FohTmatchHdr *out, const char **why) {
  if (why) *why = 0;
  char hdrPath[512];
  if (!ts_path(hdrPath, sizeof hdrPath, FOH_TMATCH_SNAP_HDR)) {
    TS_REFUSE("persist path too long");
  }
  FILE *f = fopen(hdrPath, "rb");
  if (!f) TS_REFUSE("no armed target run (no header)");

  // BOUNDED READ: one byte more than the grammar allows, so "too long" is
  // detectable rather than silently truncated.
  char buf[TS_HDR_BYTES + 1];
  const size_t n = fread(buf, 1, sizeof buf, f);
  const bool shortRead = feof(f) != 0;
  fclose(f);
  if (n != TS_HDR_BYTES || !shortRead) {
    TS_REFUSE("target header is the wrong length");
  }

  // INTEGRITY BEFORE MEANING (the .mlstage rule): the SUM is checked before a
  // single field is parsed, so a corrupt header is refused as corrupt and
  // never as "from a different build".
  if (memcmp(buf + TS_HDR_BODY, "SUM ", 4) != 0 ||
      !ts_hexrun(buf + TS_HDR_BODY + 4, 64) ||
      buf[TS_HDR_BODY + 4 + 64] != '\n') {
    TS_REFUSE("target header has no SUM line");
  }
  uint8_t dig[32];
  char hex[65];
  sha256((const uint8_t *)buf, TS_HDR_BODY, dig);
  ts_hex32(dig, hex);
  if (memcmp(hex, buf + TS_HDR_BODY + 4, 64) != 0) {
    TS_REFUSE("target header SUM mismatch (corrupt or edited)");
  }

  // ...then the grammar, anchored at fixed offsets.
  if (memcmp(buf, TS_MAGIC, TS_OFF_TSTAGE) != 0) {
    TS_REFUSE("not a target header (bad magic)");
  }
  if (memcmp(buf + TS_OFF_TSTAGE, "TSTAGE ", 7) != 0 ||
      buf[TS_OFF_TSTAGE + 9] != '\n') {
    TS_REFUSE("target header has no TSTAGE line");
  }
  long tstage = 0;
  if (!ts_digits(buf + TS_OFF_TSTAGE + 7, 2, &tstage)) {
    TS_REFUSE("target header TSTAGE is not two digits");
  }
  if (tstage > TS_TSTAGE_MAX) {
    TS_REFUSE("target header names a stage that cannot be played");
  }
  if (memcmp(buf + TS_OFF_CHAR, "CHAR ", 5) != 0 || buf[TS_OFF_CHAR + 6] != '\n') {
    TS_REFUSE("target header has no CHAR line");
  }
  long charId = 0;
  if (!ts_digits(buf + TS_OFF_CHAR + 5, 1, &charId)) {
    TS_REFUSE("target header CHAR is not a digit");
  }
  if (charId > 4) TS_REFUSE("target header names a character that does not exist");
  if (memcmp(buf + TS_OFF_FRAME, "FRAME ", 6) != 0 ||
      buf[TS_OFF_FRAME + 18] != '\n') {
    TS_REFUSE("target header has no FRAME line");
  }
  long fr = 0;
  if (!ts_digits(buf + TS_OFF_FRAME + 6, 12, &fr)) {
    TS_REFUSE("target header FRAME is not twelve digits");
  }
  if (memcmp(buf + TS_OFF_SRC, "SRC ", 4) != 0 ||
      !ts_hexrun(buf + TS_OFF_SRC + 4, 64) || buf[TS_OFF_SRC + 68] != '\n') {
    TS_REFUSE("target header has no SRC line");
  }
  if (memcmp(buf + TS_OFF_BUILD, "BUILD ", 6) != 0 ||
      !ts_hexrun(buf + TS_OFF_BUILD + 6, 64) || buf[TS_OFF_BUILD + 70] != '\n') {
    TS_REFUSE("target header has no BUILD line");
  }

  // The cheap identity refusal, taken here rather than a whole snapshot later.
  // It is the SAME identity ss_load enforces, so this can only ever refuse
  // earlier, never instead.
  char id[65];
  ss_build_identity(id);
  if (memcmp(id, buf + TS_OFF_BUILD + 6, 64) != 0) {
    TS_REFUSE("target snapshot is from a different build");
  }

  // A frame of 0 is not a resumable run: it is a run that had not ticked, and
  // resuming it would be a restart wearing a resume's name.
  if (fr <= 0) TS_REFUSE("armed target run has no played frames");

  if (out) {
    out->tstage = (int)tstage;
    out->charId = (int)charId;
    out->frame = fr;
    memcpy(out->src, buf + TS_OFF_SRC + 4, 64);
    out->src[64] = 0;
  }
  return true;
}

static bool ts_restore(GameState *g, const FohTmatchHdr *want,
                       const char *liveSrc, const char **why) {
  if (why) *why = 0;
  if (!g || !want) TS_REFUSE("no run state");

  // THE STAGE COMES FIRST, and it is checked BEFORE the snapshot is opened,
  // because it is the one thing that could have changed while the machine was
  // off. `liveSrc` is `src` recomputed from the stage the launch
  // actually loaded off the card; if it disagrees with the header, the run the
  // player left is not the run this card would now play, and there is no
  // honest way to continue it.
  if (!liveSrc || !ts_hexrun(liveSrc, 64) || liveSrc[64] != 0) {
    ts_disarm();
    TS_REFUSE("the stage this run would resume on has no identity");
  }
  if (memcmp(liveSrc, want->src, 64) != 0) {
    ts_disarm();
    TS_REFUSE("the custom stage on the card is not the one this run was played "
              "on");
  }
  // ...and the DERIVED row that names it. tp_setup_target_core rebuilt
  // targetStagePlaying from the header's TSTAGE (target_play.c's TS_DERIVED
  // list says it is rebuilt and never restored), so this asserts the setup
  // really used the stage the header named — the derived half of the plane
  // checked against the persisted half's provenance.
  if ((int)TP.targetStagePlaying != want->tstage) {
    ts_disarm();
    TS_REFUSE("the run was set up on a different target stage than the header "
              "names");
  }

  char simPath[512];
  if (!ts_path(simPath, sizeof simPath, FOH_TMATCH_SNAP_FILE)) {
    TS_REFUSE("persist path too long");
  }
  // A REFUSAL MUST LEAVE THE FRESH RUN INTACT. Raised in code review, and
  // NOT DEMONSTRATED REACHABLE: an attempt to make it bite — refusing after
  // ss_load vs before it, same card, comparing the played streams — produced
  // BYTE-IDENTICAL runs with this rollback deleted, so something downstream
  // re-establishes the state and the player is not currently reaching a
  // clobbered run. That attempt was removed rather than committed, because a
  // check that passes with the fix deleted proves nothing.
  //
  // The rollback is kept anyway, as a cheap invariant rather than as a fix
  // for a live bug: what it costs is one struct copy on a path that runs at
  // most once per boot, and what it buys is that the function's contract —
  // "false means nothing was changed" — is true by construction instead of by
  // a downstream accident nobody wrote down. If that accident is ever
  // refactored away, this is what stops it becoming a defect.
  //
  // The original reasoning follows, and it is still why the shape is wrong
  // without this: `ss_load` overwrites the WHOLE GameState and the target
  // module's own rows, and the pair-agreement checks below run AFTER it — so a
  // header that disagreed with its snapshot returned false with `g` and `TP`
  // already carrying the loaded state. The caller (foh_dev.c) then does what
  // its comment says it does: plays "the legal, freshly started run sitting in
  // G and TP". It was neither legal nor fresh; it was half of somebody else's
  // run, and the player would have been dropped into it.
  //
  // There is no peek in the snapshot API to validate ahead of the load, so the
  // load is made UNDOABLE instead: both planes are copied first and put back
  // on any refusal below. GameState is ~160 KB, which is why the scratch is
  // static — this is a boot-time path that runs at most once, never a loop.
  static GameState ssBefore;
  MlTargets tpBefore;
  ssBefore = *g;
  tpBefore = TP;
#define TS_REFUSE_LOADED(m)                                                    \
  do {                                                                         \
    *g = ssBefore;                                                             \
    TP = tpBefore;                                                             \
    ts_disarm();                                                               \
    TS_REFUSE(m);                                                              \
  } while (0)
  const char *ssWhy = 0;
  if (!ss_load(g, simPath, &ssWhy)) {
    // The half-torn pair (header present, snapshot gone or corrupt) lands here
    // and is refused BY THE SNAPSHOT'S OWN RULE NAME. ss_load is not
    // documented to leave `g` untouched when it fails part way, so this rolls
    // back too rather than assuming.
    TS_REFUSE_LOADED(ssWhy ? ssWhy : "snapshot load failed");
  }
  // ...and the pair has to agree. A header that survived from an earlier
  // hibernate beside a newer snapshot would set the run up wrong and then load
  // a state that does not belong to it.
  if (g->frame != want->frame) {
    TS_REFUSE_LOADED("target header and snapshot disagree about the frame");
  }
  if ((int)g->sim.characterSelections[0] != want->charId) {
    TS_REFUSE_LOADED("target header and snapshot disagree about the character");
  }
  // CONSUMED. A resumed run must not be resumable a second time: the player is
  // playing it now, and a crash an hour later must not hand back the state he
  // left behind at the lid.
  ts_disarm();
  return true;
}

#undef TS_REFUSE

static const FohTargetSnapOps ts_ops = {
    .src = ts_src,
    .arm = ts_arm,
    .peek = ts_peek,
    .restore = ts_restore,
    .disarm = ts_disarm,
};

// The NULL definition lives in foh_dev.c, the app that owns the seam — the
// foh_match_snap_ops arrangement, followed exactly. A build of foh_dev.c
// WITHOUT this TU links and behaves exactly as the port did before ticket #30.
__attribute__((constructor)) static void ts_install(void) {
  foh_target_snap_ops = &ts_ops;
}
