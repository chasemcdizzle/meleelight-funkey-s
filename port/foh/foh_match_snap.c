// port/foh/foh_match_snap.c — ticket #29: the lid-close seam for a match.
//
// The header states the interface and the three rules. This file is where the
// two writes are ORDERED and where the read path refuses.
//
// WRITE ORDER, AND THE TORN PAIR. A hibernate does, in this order:
//
//   (1) unlink the header      -> the pair is now DISARMED
//   (2) ss_save the snapshot   -> ~33 ms, 160 KB, fsync + atomic rename
//   (3) publish the header     -> ~150 bytes, the SAME atomic publish the
//                                 settings record and the builder document
//                                 use (foh_persist_publish)
//   ...and only then does the caller write the settings record, which is what
//   actually arms the resume (`resume` row == FOH_MATCH).
//
// Unlinking FIRST is the whole torn-pair argument, and it is cheap. The
// dangerous state is a NEW snapshot beside an OLD header — the header would
// name a stage and a frame belonging to a match that is gone, and the resume
// would set a match up wrong and then load a snapshot into it. Removing the
// header before writing anything means every interruption lands on "no
// header", which is the disarmed state, which is rule 2. The reverse tear (a
// header with no snapshot) is refused by the load, by name.
//
// Belt and braces on top of that, because unlink() on vfat is not a
// transaction: the header carries the FRAME and the snapshot's BUILD
// identity, and `restore` compares the restored G.frame and G.stageSelect
// against what the header promised. A pair that is mismatched in any of those
// three ways is refused instead of played.
//
// VALIDATE ON READ, ALWAYS (the .mlstage contract, A45 T2, inherited whole):
// bounded read, strict anchored grammar, SUM verified BEFORE anything is
// parsed, every refusal names its rule. A corrupt header cannot reach the
// sim, and `peek` runs entirely before the 160 KB snapshot is opened, so a
// stale or foreign card costs one small read.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../../oracle/qjs/sha256.h"
#include "../sim/sim/sim_snapshot.h"
#include "foh_match_snap.h"
#include "foh_persist.h"

#define MS_MAGIC "MLMATCH1\n"
// The header is a fixed shape, so its size is known and a read that is not
// exactly this long is refused before it is looked at.
//   "MLMATCH1\n"                                   9
//   "STAGE " <2 digits> "\n"                       9
//   "FRAME " <12 zero-padded digits> "\n"         19
//   "BUILD " <64 hex> "\n"                        71
//   "SUM " <64 hex> "\n"                          69
#define MS_HDR_BODY ((size_t)(9 + 9 + 19 + 71))
#define MS_HDR_SUM ((size_t)(4 + 64 + 1))
#define MS_HDR_BYTES (MS_HDR_BODY + MS_HDR_SUM)

// A match frame count is bounded by the same long the loop runs on, but the
// wire form is fixed-width so the grammar can be anchored; 12 digits is
// ~2.3 million years at 60 fps and still fits a 32-bit long.
#define MS_FRAME_MAX 999999999999L

static void ms_hex32(const uint8_t d[32], char out[65]) {
  static const char *H = "0123456789abcdef";
  for (int k = 0; k < 32; k++) {
    out[2 * k] = H[d[k] >> 4];
    out[2 * k + 1] = H[d[k] & 15];
  }
  out[64] = 0;
}

static bool ms_path(char *dst, size_t cap, const char *name) {
  const int w = snprintf(dst, cap, "%s/%s", foh_persist_dir(), name);
  return w > 0 && (size_t)w < cap;
}

static void ms_disarm(void) {
  char p[512];
  // The HEADER first, for the same reason it is unlinked first on the write
  // path: while it is gone the pair is disarmed, whatever happens next.
  if (ms_path(p, sizeof p, FOH_MATCH_SNAP_HDR)) (void)remove(p);
  if (ms_path(p, sizeof p, FOH_MATCH_SNAP_FILE)) (void)remove(p);
}

static bool ms_arm(const GameState *g, int stageSel, const char **why) {
  if (why) *why = 0;
  if (!g) {
    if (why) *why = "no match state";
    return false;
  }
  // stageSel rides in the header and comes back as an argument to
  // sim_setup_match_ports, so it is range-checked on the way OUT as well as
  // on the way in: a header that could not have been written by a legal
  // launch is a header nobody has to reason about later.
  if (stageSel < 0 || stageSel > 99) {
    if (why) *why = "stage outside the header's domain";
    return false;
  }
  if (g->frame < 0 || g->frame > MS_FRAME_MAX) {
    if (why) *why = "frame outside the header's domain";
    return false;
  }

  char hdrPath[512], simPath[512];
  if (!ms_path(hdrPath, sizeof hdrPath, FOH_MATCH_SNAP_HDR) ||
      !ms_path(simPath, sizeof simPath, FOH_MATCH_SNAP_FILE)) {
    if (why) *why = "persist path too long";
    return false;
  }

  // (1) DISARM FIRST. Everything after this point is safe to be interrupted.
  (void)remove(hdrPath);

  // (2) the snapshot — #28's writer, #28's atomic publish.
  const char *ssWhy = 0;
  if (!ss_save(g, simPath, &ssWhy)) {
    if (why) *why = ssWhy ? ssWhy : "snapshot write failed";
    (void)remove(simPath);
    return false;
  }

  // (3) the header. Built into a fixed-size buffer with its own SUM, then
  // handed to the ONE publish.
  char buf[MS_HDR_BYTES + 1];
  char id[65];
  ss_build_identity(id);
  const int w = snprintf(buf, MS_HDR_BODY + 1,
                         MS_MAGIC "STAGE %02d\nFRAME %012ld\nBUILD %s\n",
                         stageSel, g->frame, id);
  if (w < 0 || (size_t)w != MS_HDR_BODY) {
    if (why) *why = "header format";
    (void)remove(simPath);
    return false;
  }
  uint8_t dig[32];
  char hex[65];
  sha256((const uint8_t *)buf, MS_HDR_BODY, dig);
  ms_hex32(dig, hex);
  const int w2 = snprintf(buf + MS_HDR_BODY, MS_HDR_SUM + 1, "SUM %s\n", hex);
  if (w2 < 0 || (size_t)w2 != MS_HDR_SUM) {
    if (why) *why = "header SUM format";
    (void)remove(simPath);
    return false;
  }
  const char *pubWhy = 0;
  if (!foh_persist_publish(FOH_MATCH_SNAP_HDR, buf, MS_HDR_BYTES, &pubWhy)) {
    if (why) *why = pubWhy ? pubWhy : "header publish failed";
    // The snapshot is orphaned; remove it so the card does not carry 160 KB
    // nothing will ever read.
    (void)remove(simPath);
    return false;
  }
  // FOH_PUBLISH_NODIRSYNC is a SUCCESS with a caveat (compare by pointer —
  // foh_persist.h says so). The settings record accepts it, the builder
  // document accepts it, and so does this: the rename landed. Durability of
  // the directory entry is proven end to end by a reboot, never by an fsync
  // return code.
  return true;
}

// --- the read path ----------------------------------------------------------

#define MS_REFUSE(msg)                                                         \
  do {                                                                         \
    if (why) *why = (msg);                                                     \
    return false;                                                              \
  } while (0)

// Exactly `n` decimal digits at `s`, value out. Anchored: no sign, no space,
// no leading-plus, nothing clever.
static bool ms_digits(const char *s, int n, long *out) {
  long v = 0;
  for (int k = 0; k < n; k++) {
    if (s[k] < '0' || s[k] > '9') return false;
    v = v * 10 + (s[k] - '0');
  }
  *out = v;
  return true;
}

static bool ms_hexrun(const char *s, int n) {
  for (int k = 0; k < n; k++) {
    const char c = s[k];
    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
  }
  return true;
}

static bool ms_peek(int *stageSel, long *frame, const char **why) {
  if (why) *why = 0;
  char hdrPath[512];
  if (!ms_path(hdrPath, sizeof hdrPath, FOH_MATCH_SNAP_HDR)) {
    MS_REFUSE("persist path too long");
  }
  FILE *f = fopen(hdrPath, "rb");
  if (!f) MS_REFUSE("no armed match (no header)");

  // BOUNDED READ: one byte more than the grammar allows, so "too long" is
  // detectable rather than silently truncated.
  char buf[MS_HDR_BYTES + 1];
  const size_t n = fread(buf, 1, sizeof buf, f);
  const bool shortRead = feof(f) != 0;
  fclose(f);
  if (n != MS_HDR_BYTES || !shortRead) {
    MS_REFUSE("match header is the wrong length");
  }

  // INTEGRITY BEFORE MEANING (the .mlstage rule): the SUM is checked before a
  // single field is parsed, so a corrupt header is refused as corrupt and
  // never as "from a different build".
  if (memcmp(buf + MS_HDR_BODY, "SUM ", 4) != 0 ||
      !ms_hexrun(buf + MS_HDR_BODY + 4, 64) ||
      buf[MS_HDR_BODY + 4 + 64] != '\n') {
    MS_REFUSE("match header has no SUM line");
  }
  uint8_t dig[32];
  char hex[65];
  sha256((const uint8_t *)buf, MS_HDR_BODY, dig);
  ms_hex32(dig, hex);
  if (memcmp(hex, buf + MS_HDR_BODY + 4, 64) != 0) {
    MS_REFUSE("match header SUM mismatch (corrupt or edited)");
  }

  // ...then the grammar, anchored at fixed offsets.
  if (memcmp(buf, MS_MAGIC, 9) != 0) MS_REFUSE("not a match header (bad magic)");
  if (memcmp(buf + 9, "STAGE ", 6) != 0 || buf[9 + 8] != '\n') {
    MS_REFUSE("match header has no STAGE line");
  }
  long stage = 0;
  if (!ms_digits(buf + 9 + 6, 2, &stage)) {
    MS_REFUSE("match header STAGE is not two digits");
  }
  if (memcmp(buf + 18, "FRAME ", 6) != 0 || buf[18 + 18] != '\n') {
    MS_REFUSE("match header has no FRAME line");
  }
  long fr = 0;
  if (!ms_digits(buf + 18 + 6, 12, &fr)) {
    MS_REFUSE("match header FRAME is not twelve digits");
  }
  if (memcmp(buf + 37, "BUILD ", 6) != 0 || !ms_hexrun(buf + 37 + 6, 64) ||
      buf[37 + 70] != '\n') {
    MS_REFUSE("match header has no BUILD line");
  }

  // The cheap identity refusal, taken here rather than 160 KB later. It is
  // the SAME identity ss_load enforces, so this can only ever refuse earlier,
  // never instead: a header that passes here still faces the snapshot's own
  // BUILD line.
  char id[65];
  ss_build_identity(id);
  if (memcmp(id, buf + 37 + 6, 64) != 0) {
    MS_REFUSE("match snapshot is from a different build");
  }

  // A frame of 0 is not a resumable match: it is a match that had not ticked,
  // and resuming it would be a restart wearing a resume's name.
  if (fr <= 0) MS_REFUSE("armed match has no played frames");

  if (stageSel) *stageSel = (int)stage;
  if (frame) *frame = fr;
  return true;
}

static bool ms_restore(GameState *g, int stageSel, long frame,
                       const char **why) {
  if (why) *why = 0;
  if (!g) MS_REFUSE("no match state");
  char simPath[512];
  if (!ms_path(simPath, sizeof simPath, FOH_MATCH_SNAP_FILE)) {
    MS_REFUSE("persist path too long");
  }
  const char *ssWhy = 0;
  if (!ss_load(g, simPath, &ssWhy)) {
    // The half-torn pair (header present, snapshot gone or corrupt) lands
    // here and is refused BY THE SNAPSHOT'S OWN RULE NAME.
    ms_disarm();
    MS_REFUSE(ssWhy ? ssWhy : "snapshot load failed");
  }
  // ...and the pair has to agree. A header that survived from an earlier
  // hibernate beside a newer snapshot would set the match up on the wrong
  // stage and then load a state that does not belong to it; these two
  // comparisons are what makes that a named refusal instead of a wrong game.
  if ((int)g->stageSelect != stageSel) {
    ms_disarm();
    MS_REFUSE("match header and snapshot disagree about the stage");
  }
  if (g->frame != frame) {
    ms_disarm();
    MS_REFUSE("match header and snapshot disagree about the frame");
  }
  // CONSUMED. A resumed match must not be resumable a second time: the player
  // is playing it now, and a crash an hour later must not hand back the state
  // he left behind at the lid.
  ms_disarm();
  return true;
}

#undef MS_REFUSE

static const FohMatchSnapOps ms_ops = {
    .arm = ms_arm,
    .peek = ms_peek,
    .restore = ms_restore,
    .disarm = ms_disarm,
};

// The NULL definition lives in foh_dev.c, the app that owns the seam — the
// foh_tbuild_ops arrangement (its NULL is in foh.c) applied to an app-level
// pointer, because this header needs GameState and foh.c deliberately does
// not see the sim. A build of foh_dev.c WITHOUT this TU links and behaves
// exactly as the port did before ticket #29.
__attribute__((constructor)) static void ms_install(void) {
  foh_match_snap_ops = &ms_ops;
}
