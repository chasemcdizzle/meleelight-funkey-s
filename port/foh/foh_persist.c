// port/foh/foh_persist.c — the ONE persistence chokepoint (fix_plan §M4
// task 13). Contract, format, defaults provenance, and the stderr
// event grammar live in foh_persist.h. Every driver (foh_app.c,
// foh_dev.c) consumes persistence ONLY through this TU — no scattered
// fopen calls (the task's chokepoint law).
#include "foh_persist.h"

#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "../gfx/ctl_style.h" // CTL_STYLE_COUNT (enum only — no link dep)
#include "../sim/ml_ser.h" // ml_sha256_hex (oracle/qjs/sha256.c by path)

// The CURRENT format version. One number: the header literal is built from
// it and every block gate below compares against it.
#define FP_VERSION 6
#define FP_FILE "mlfk-persist.dat"
#define FP_TMP "mlfk-persist.tmp"
#define FP_DEFAULT_DIR "/mnt/mlfk-data"
// 64 lines, ~1.6 KB canonical — anything larger is not ours.
#define FP_CAP 4096
// MLFKPERSIST2's ctlstyle domain was {0 normal, 1 box} — CTL_STYLE_NATURAL
// did not exist yet. FROZEN: never re-point this at CTL_STYLE_COUNT.
#define FP_V2_STYLES 2
#define FP_NEG1_BITS UINT64_C(0xbff0000000000000)
// matchTimer cap (targetplay.js:282: capped < 6000 seconds)
#define FP_TIME_CAP 6000.0

const char *foh_persist_dir(void) {
  static const char *dir = 0;
  if (!dir) {
    const char *env = getenv("MLFK_PERSIST_DIR");
    dir = (env && env[0]) ? env : FP_DEFAULT_DIR;
  }
  return dir;
}

void foh_persist_defaults(FohPersist *p) {
  memset(p, 0, sizeof *p); // settings.js:44-56 — all zero, EXCEPT:
  // Owner ruling 2026-07-29: Natural is the FRESH-INSTALL default. It is
  // assigned explicitly because CTL_STYLE_NORMAL (not NATURAL) is the
  // zero value — the enum numbers are frozen so MLFKPERSIST2 saves keep
  // their scheme across the v3 bump (ctl_style.h).
  p->ctlStyle = (int)CTL_STYLE_DEFAULT;
  // D30 (A30(a), owner 2026-08-23): "box is good but L should be shield and
  // R should be mod / tilt". The Mod shoulder is an ORTHOGONAL cell, not a
  // scheme (ctl_style.h:69-77), and swapping is a pure RELABELING that leaves
  // the ratified BOX table untouched. THIS line is the fresh-install default
  // the player actually gets — ctl_style.c's initializer is overwritten by
  // foh_persist_load(), so without this D30 is inert.
  p->modOnR = 1; // D30: shield on L, Mod on R
  // v4 (MENU-SPEC §3/§4): the three options defaults that are NOT zero.
  // phantomThreshold is ON THE CHECKSUM SURFACE — zeroing it silently
  // flips physics, which is exactly the qjs getCookie defect (CLAUDE.md
  // M0 task 6) — so it is assigned explicitly, never left to the memset.
  // The other four v4 keys ARE zero upstream (settings.js).
  p->phantomThreshold = 0.01; // settings.js:50
  p->masterVolume[0] = 0.5;   // audiomenu.js:13 (sounds)
  p->masterVolume[1] = 0.3;   // audiomenu.js:13 (music)
  for (int c = 0; c < FOH_PERSIST_CHARS; c++) {
    for (int s = 0; s < FOH_PERSIST_TSTAGES; s++) {
      p->targetRecords[c][s] = -1.0; // targetplay.js:40
    }
  }
  // v5 (fix_plan A31): the IDENTITY binding on every port. Not left to the
  // memset for the same reason ctlStyle is not — zero is a legal SLOT
  // value, so a memset would put action 0 on all eight buttons, which is
  // not a permutation at all.
  for (int k = 0; k < CTL_BIND_PORTS; k++) {
    for (int i = 0; i < (int)CTL_BTN_COUNT; i++) p->bind[k][i] = i;
  }
  // v6 (fix_plan A49; DEVIATION D45): the CSS selection. Marth (0) on every
  // port, which is BOTH upstream's fresh state (characterSelections is
  // `[0,0,0,0]`, main.js:59) and foh_init's — so this line is what the
  // memset already gives and is written out anyway, because a default that
  // is only true by accident of being zero is the one that breaks silently
  // when the roster order changes. Same argument ctlStyle carries above.
  for (int k = 0; k < FOH_CSS_PORTS; k++) p->selChar[k] = 0;
}

// --- canonical serialization (deterministic bytes; twin-cmp'd) --------------

static uint64_t fp_bits(double d) {
  uint64_t b;
  memcpy(&b, &d, 8);
  return b;
}

// Domain guard for the v4 block's three doubles: finite, non-negative
// (signbit rejects -0.0 too) and <= hi. The qjs getCookie defect class
// (CLAUDE.md M0 task 6) is exactly a silently out-of-domain settings
// value, so these are checked, never trusted.
static bool fp_in_range(double d, double hi) {
  return isfinite(d) && !signbit(d) && d >= 0.0 && d <= hi;
}

static double fp_double(uint64_t b) {
  double d;
  memcpy(&d, &b, 8);
  return d;
}

// Emits the full canonical file (SUM line included) into buf; returns
// the byte length. Loud death on overflow (structurally impossible for
// the fixed shape — belt for the buffer contract).
static size_t fp_serialize(const FohPersist *p, char *buf, size_t cap) {
  size_t n = 0;
  int w = snprintf(buf + n, cap - n,
                   "MLFKPERSIST6\nturbo %d\nlcancel %d\n"
                   "tapjump %d %d %d %d\nctlstyle %d\nmodonr %d\n",
                   p->turbo, p->lCancelType, p->tapJumpOff[0],
                   p->tapJumpOff[1], p->tapJumpOff[2], p->tapJumpOff[3],
                   p->ctlStyle, p->modOnR);
  if (w < 0 || (size_t)w >= cap - n) gfx_fatal("foh_persist: serialize overflow");
  n += (size_t)w;
  for (int c = 0; c < FOH_PERSIST_CHARS; c++) {
    for (int s = 0; s < FOH_PERSIST_TSTAGES; s++) {
      w = snprintf(buf + n, cap - n, "rec %d %d %016llx\n", c, s,
                   (unsigned long long)fp_bits(p->targetRecords[c][s]));
      if (w < 0 || (size_t)w >= cap - n) {
        gfx_fatal("foh_persist: serialize overflow");
      }
      n += (size_t)w;
    }
  }
  // v4 BLOCK — APPENDED after the 50 rec rows (foh_persist.h): every older
  // version stays a strict PREFIX through the rec block, so their rec-row
  // line indices are unchanged and one parser still serves all of them.
  // Doubles as hex16 bit patterns, never decimal — no strtod on any path
  // (the iter-38 device-musl strtod class is structurally out).
  w = snprintf(buf + n, cap - n,
               "flash %d\nwalljump %d\nblastzone %d\ndustless %d\n"
               "phantom %016llx\nsoundslevel %016llx\nmusiclevel %016llx\n",
               p->flashOnLCancel, p->everyCharWallJump, p->blastzoneWrapping,
               p->dustLessPerfectWavedash,
               (unsigned long long)fp_bits(p->phantomThreshold),
               (unsigned long long)fp_bits(p->masterVolume[0]),
               (unsigned long long)fp_bits(p->masterVolume[1]));
  if (w < 0 || (size_t)w >= cap - n) gfx_fatal("foh_persist: serialize overflow");
  n += (size_t)w;
  // v5 BLOCK (fix_plan A31) — appended after the v4 block for the same
  // prefix reason. One row per port, port-major, eight single-digit slots.
  for (int k = 0; k < CTL_BIND_PORTS; k++) {
    w = snprintf(buf + n, cap - n, "bind %d %d %d %d %d %d %d %d %d\n", k,
                 p->bind[k][0], p->bind[k][1], p->bind[k][2], p->bind[k][3],
                 p->bind[k][4], p->bind[k][5], p->bind[k][6], p->bind[k][7]);
    if (w < 0 || (size_t)w >= cap - n) {
      gfx_fatal("foh_persist: serialize overflow");
    }
    n += (size_t)w;
  }
  // v6 BLOCK (fix_plan A49; DEVIATION D45) — appended after the v5 block for
  // the same prefix reason: v1..v5 stay strict prefixes, so one parser still
  // serves every one of them and no older line index moves.
  //
  // The SELECTION plane only — never the token plane, and never the port
  // types or the CPU levels. The argument is at foh_persist.h's format note.
  w = snprintf(buf + n, cap - n, "sel %d %d %d %d\n", p->selChar[0],
               p->selChar[1], p->selChar[2], p->selChar[3]);
  if (w < 0 || (size_t)w >= cap - n) gfx_fatal("foh_persist: serialize overflow");
  n += (size_t)w;
  char hex[65];
  ml_sha256_hex(buf, n, hex);
  w = snprintf(buf + n, cap - n, "SUM %s\n", hex);
  if (w < 0 || (size_t)w >= cap - n) gfx_fatal("foh_persist: serialize overflow");
  n += (size_t)w;
  return n;
}

// --- strict load ------------------------------------------------------------

static FohPersistStatus fp_reset(FohPersist *p, FohPersistStatus st,
                                 const char *detail) {
  foh_persist_defaults(p);
  switch (st) {
    case FOH_PERSIST_RESET_MISSING:
      fprintf(stderr, "foh_persist: reset cause=missing\n");
      break;
    case FOH_PERSIST_RESET_VERSION:
      fprintf(stderr, "foh_persist: reset cause=version\n");
      break;
    case FOH_PERSIST_RESET_CORRUPT:
      fprintf(stderr, "foh_persist: reset cause=corrupt detail=%s\n", detail);
      break;
    default: gfx_fatal("foh_persist: bad reset status");
  }
  return st;
}

static bool fp_is_hex16(const char *s) {
  for (int k = 0; k < 16; k++) {
    const char c = s[k];
    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
  }
  return true;
}

static uint64_t fp_parse_hex16(const char *s) {
  uint64_t v = 0;
  for (int k = 0; k < 16; k++) {
    const char c = s[k];
    v <<= 4;
    v |= (uint64_t)(c <= '9' ? c - '0' : c - 'a' + 10);
  }
  return v;
}

FohPersistStatus foh_persist_load(FohPersist *p) {
  char path[512];
  if (snprintf(path, sizeof path, "%s/%s", foh_persist_dir(), FP_FILE) >=
      (int)sizeof path) {
    gfx_fatal("foh_persist: dir path overflow");
  }
  FILE *f = fopen(path, "rb");
  if (!f) {
    if (errno == ENOENT) return fp_reset(p, FOH_PERSIST_RESET_MISSING, 0);
    return fp_reset(p, FOH_PERSIST_RESET_CORRUPT, "open");
  }
  static char buf[FP_CAP + 1];
  const size_t got = fread(buf, 1, sizeof buf, f);
  const bool readOk = ferror(f) == 0;
  fclose(f);
  if (!readOk) return fp_reset(p, FOH_PERSIST_RESET_CORRUPT, "open");
  if (got > FP_CAP) return fp_reset(p, FOH_PERSIST_RESET_CORRUPT, "oversize");
  if (got == 0 || buf[got - 1] != '\n') {
    return fp_reset(p, FOH_PERSIST_RESET_CORRUPT, "truncated");
  }
  // isolate the last line: must be exactly "SUM <64 lowercase hex>\n"
  size_t sumStart = got - 1;
  while (sumStart > 0 && buf[sumStart - 1] != '\n') sumStart--;
  const size_t sumLen = got - sumStart;
  if (sumLen != 4 + 64 + 1 || memcmp(buf + sumStart, "SUM ", 4) != 0 ||
      !fp_is_hex16(buf + sumStart + 4) || !fp_is_hex16(buf + sumStart + 20) ||
      !fp_is_hex16(buf + sumStart + 36) || !fp_is_hex16(buf + sumStart + 52)) {
    return fp_reset(p, FOH_PERSIST_RESET_CORRUPT, "truncated");
  }
  char want[65];
  ml_sha256_hex(buf, sumStart, want);
  if (memcmp(want, buf + sumStart + 4, 64) != 0) {
    return fp_reset(p, FOH_PERSIST_RESET_CORRUPT, "sum");
  }
  // strict line-by-line over [0, sumStart)
  FohPersist v;
  foh_persist_defaults(&v);
  size_t pos = 0;
  // line 1: header. ^MLFKPERSIST[0-9]+$ — version 4 is current; versions
  // 3, 2 and 1 MIGRATE (see below), any OTHER version -> RESET_VERSION;
  // anything else -> header corruption.
  // Migration source version: 0 = current (v4), else the version we are
  // upgrading FROM. Every older format is a strict PREFIX of v4 through
  // the rec block, so the shared parse below just skips the lines they
  // lack and fills the appended v4 block with the fresh-install defaults.
  int fromVer = 0;
  {
    size_t e = pos;
    while (e < sumStart && buf[e] != '\n') e++;
    if (e >= sumStart) return fp_reset(p, FOH_PERSIST_RESET_CORRUPT, "header");
    const size_t len = e - pos;
    if (len == 12 && memcmp(buf + pos, "MLFKPERSIST6", 12) == 0) {
      // current version
    } else if (len == 12 && memcmp(buf + pos, "MLFKPERSIST5", 12) == 0) {
      // MIGRATION. v6 is v5 plus the one appended `sel` row (fix_plan A49).
      // A v5 file was written by a build that persisted no character at all,
      // so it HAS no opinion to carry forward and the fresh-install marth
      // foh_persist_defaults() already put in *p is exactly the selection
      // that device booted with. Nothing a player set is lost: every
      // setting, both control stamps, all four bindings and all 50 target
      // records parse with the SAME code below.
      fromVer = 5;
    } else if (len == 12 && memcmp(buf + pos, "MLFKPERSIST4", 12) == 0) {
      // MIGRATION. v4 is v5 minus the four appended `bind` rows (fix_plan
      // A31). A v4 file was written by a build that had no rebinder, so the
      // identity binding foh_persist_defaults() already put in *p IS the
      // mapping that device had — carrying it forward changes nothing the
      // player can feel, which is the whole migration rule.
      fromVer = 4;
    } else if (len == 12 && memcmp(buf + pos, "MLFKPERSIST3", 12) == 0) {
      // MIGRATION. v3 is v4 minus the seven appended options lines
      // (MENU-SPEC §3/§4). Nothing older ever carried an opinion about
      // them, so they take exactly the fresh-install defaults that
      // foh_persist_defaults() already put in *p before this parse ran.
      fromVer = 3;
    } else if (len == 12 && memcmp(buf + pos, "MLFKPERSIST2", 12) == 0) {
      // MIGRATION. v2 is v3 minus the `modonr` line; its ctlstyle values
      // are the SAME numbers v3 uses (the enum is frozen for exactly
      // this reason), so the scheme carries over UNCHANGED and only the
      // Mod shoulder takes its ratified default.
      fromVer = 2;
    } else if (len == 12 && memcmp(buf + pos, "MLFKPERSIST1", 12) == 0) {
      // MIGRATION, not a reset. v1 is identical to v2 except that it
      // has no `ctlstyle` line, and its SUM has already been verified
      // above by the same seal — so every setting and all 50 target
      // records parse with the SAME code below; only the ctlstyle line
      // is skipped. Resetting here would have silently destroyed every
      // target-test personal best on an upgrading device.
      //
      // The migrated style is BOX, not the fresh-install default
      // (review-ctl r2): a v1 file can only have been written by a build
      // whose one and only mapping was the Chase-ratified S1 == BOX, so
      // carrying it forward preserves the controls that device already
      // had. NATURAL is the default for a FRESH or reset install only
      // (owner ruling 2026-07-29) — an upgrade must not silently re-map a
      // ratified control scheme underneath him.
      fromVer = 1;
    } else if (len > 11 && memcmp(buf + pos, "MLFKPERSIST", 11) == 0) {
      bool digits = len > 11;
      for (size_t k = pos + 11; k < e; k++) {
        if (buf[k] < '0' || buf[k] > '9') { digits = false; break; }
      }
      if (!digits) return fp_reset(p, FOH_PERSIST_RESET_CORRUPT, "header");
      return fp_reset(p, FOH_PERSIST_RESET_VERSION, 0);
    } else {
      return fp_reset(p, FOH_PERSIST_RESET_CORRUPT, "header");
    }
    pos = e + 1;
  }
  // The EFFECTIVE version of the file being parsed: `fromVer` is 0 for the
  // current format and the source version for a migration, which is awkward
  // to gate on. A31 measured why it matters: every block below used to name
  // its versions by ENUMERATION (`fromVer == 0 || fromVer == 3`), so the v5
  // bump silently dropped the `modonr` line for a v4 file — the parser fell a
  // line out of step and a perfectly good save was rejected as corrupt. Every
  // gate is now a >= comparison on this one number, which is total over any
  // future bump instead of needing one more disjunct each time.
  const int ver = fromVer ? fromVer : FP_VERSION;
  // line 2: "turbo [01]"
  if (sumStart - pos < 8 || memcmp(buf + pos, "turbo ", 6) != 0 ||
      (buf[pos + 6] != '0' && buf[pos + 6] != '1') || buf[pos + 7] != '\n') {
    return fp_reset(p, FOH_PERSIST_RESET_CORRUPT, "grammar");
  }
  v.turbo = buf[pos + 6] - '0';
  pos += 8;
  // line 3: "lcancel [0-2]"
  if (sumStart - pos < 10 || memcmp(buf + pos, "lcancel ", 8) != 0 ||
      buf[pos + 8] < '0' || buf[pos + 8] > '2' || buf[pos + 9] != '\n') {
    return fp_reset(p, FOH_PERSIST_RESET_CORRUPT, "grammar");
  }
  v.lCancelType = buf[pos + 8] - '0';
  pos += 10;
  // line 4: "tapjump [01] [01] [01] [01]"
  if (sumStart - pos < 16 || memcmp(buf + pos, "tapjump ", 8) != 0) {
    return fp_reset(p, FOH_PERSIST_RESET_CORRUPT, "grammar");
  }
  for (int k = 0; k < 4; k++) {
    const char c = buf[pos + 8 + 2 * (size_t)k];
    const char sep = buf[pos + 9 + 2 * (size_t)k];
    if ((c != '0' && c != '1') || sep != (k == 3 ? '\n' : ' ')) {
      return fp_reset(p, FOH_PERSIST_RESET_CORRUPT, "grammar");
    }
    v.tapJumpOff[k] = c - '0';
  }
  pos += 16;
  // line 5: "ctlstyle [0-9]" (fix_plan A4). ABSENT in a v1 file — the
  // migration arm assigns CTL_STYLE_BOX in the else branch below.
  // The digit is parsed as a DECIMAL DIGIT first and only then range-
  // checked against CTL_STYLE_COUNT: comparing the raw byte against
  // '0' + COUNT would accept ':' and friends the moment the enum grows
  // past 10 (review-ctl r1). The static assert keeps the single-digit
  // encoding honest if CtlStyle ever widens.
  if (ver >= 2) {
    _Static_assert((int)CTL_STYLE_COUNT <= 10,
                   "ctlstyle is a single decimal digit; widen the line "
                   "format (and bump MLFKPERSIST) before growing CtlStyle");
    // Each version is validated against ITS OWN grammar (review-ctl n1).
    // MLFKPERSIST2 predates CTL_STYLE_NATURAL, so its domain was {0,1};
    // accepting a resealed v2 file that says `ctlstyle 2` would install a
    // state no v2 writer could ever have produced. FP_V2_STYLES is a
    // FROZEN historical constant — it does not track CTL_STYLE_COUNT.
    const int styleMax = (ver == 2) ? FP_V2_STYLES : (int)CTL_STYLE_COUNT;
    const char d = (sumStart - pos >= 11) ? buf[pos + 9] : 0;
    if (sumStart - pos < 11 || memcmp(buf + pos, "ctlstyle ", 9) != 0 ||
        d < '0' || d > '9' || (d - '0') >= styleMax ||
        buf[pos + 10] != '\n') {
      return fp_reset(p, FOH_PERSIST_RESET_CORRUPT, "grammar");
    }
    v.ctlStyle = d - '0';
    pos += 11;
  } else {
    v.ctlStyle = (int)CTL_STYLE_BOX; // see the v1 migration note above
  }
  // line 6: "modonr [01]" (owner ruling 2026-07-29). ABSENT in v1 and
  // v2 — both migrate to the M3-RATIFIED arrangement (Mod on L), never
  // to a swapped one, so an upgrade cannot silently move a binding.
  if (ver >= 3) {
    if (sumStart - pos < 9 || memcmp(buf + pos, "modonr ", 7) != 0 ||
        (buf[pos + 7] != '0' && buf[pos + 7] != '1') ||
        buf[pos + 8] != '\n') {
      return fp_reset(p, FOH_PERSIST_RESET_CORRUPT, "grammar");
    }
    v.modOnR = buf[pos + 7] - '0';
    pos += 9;
  } else {
    v.modOnR = 1; // D29: a v2/v3 file predates the cell; adopt the new default
  }
  // 50 rec rows, exact canonical order: "rec <c> <s> <hex16>"
  for (int c = 0; c < FOH_PERSIST_CHARS; c++) {
    for (int s = 0; s < FOH_PERSIST_TSTAGES; s++) {
      // fixed width: "rec c s " (8) + 16 hex + '\n' = 25 bytes
      if (sumStart - pos < 25 || memcmp(buf + pos, "rec ", 4) != 0 ||
          buf[pos + 5] != ' ' || buf[pos + 7] != ' ' ||
          buf[pos + 24] != '\n') {
        return fp_reset(p, FOH_PERSIST_RESET_CORRUPT, "grammar");
      }
      const char cc = buf[pos + 4], sc = buf[pos + 6];
      if (cc < '0' || cc > '9' || sc < '0' || sc > '9') {
        return fp_reset(p, FOH_PERSIST_RESET_CORRUPT, "grammar");
      }
      if (cc - '0' != c || sc - '0' != s) {
        return fp_reset(p, FOH_PERSIST_RESET_CORRUPT, "order");
      }
      if (!fp_is_hex16(buf + pos + 8)) {
        return fp_reset(p, FOH_PERSIST_RESET_CORRUPT, "grammar");
      }
      const uint64_t bits = fp_parse_hex16(buf + pos + 8);
      const double d = fp_double(bits);
      if (bits != FP_NEG1_BITS &&
          !(isfinite(d) && d >= 0.0 && d < FP_TIME_CAP)) {
        return fp_reset(p, FOH_PERSIST_RESET_CORRUPT, "domain");
      }
      v.targetRecords[c][s] = d;
      pos += 25;
    }
  }
  // v4 BLOCK (MENU-SPEC §3/§4) — present in v4 AND v5 files. Every version
  // older than 4 stops after the rec rows, and none of them ever carried an
  // opinion about these seven keys, so a MIGRATED file simply keeps what
  // foh_persist_defaults(&v) above already put there: exactly the
  // fresh-install value. No older save loses anything.
  if (ver >= 4) {

  // --- the v4 block, same anchored discipline as the header lines --------
  {
    // four 0/1 flags, in the fixed order flash, walljump, blastzone,
    // dustless
    static const char *const kFlagKey[4] = {"flash ", "walljump ",
                                            "blastzone ", "dustless "};
    int *const dst[4] = {&v.flashOnLCancel, &v.everyCharWallJump,
                         &v.blastzoneWrapping, &v.dustLessPerfectWavedash};
    for (int k = 0; k < 4; k++) {
      const size_t kl = strlen(kFlagKey[k]);
      if (sumStart - pos < kl + 2 || memcmp(buf + pos, kFlagKey[k], kl) != 0 ||
          (buf[pos + kl] != '0' && buf[pos + kl] != '1') ||
          buf[pos + kl + 1] != '\n') {
        return fp_reset(p, FOH_PERSIST_RESET_CORRUPT, "grammar");
      }
      *dst[k] = buf[pos + kl] - '0';
      pos += kl + 2;
    }
    // three hex16 doubles: phantom, soundslevel, musiclevel
    static const char *const kDblKey[3] = {"phantom ", "soundslevel ",
                                           "musiclevel "};
    double *const dd[3] = {&v.phantomThreshold, &v.masterVolume[0],
                           &v.masterVolume[1]};
    for (int k = 0; k < 3; k++) {
      const size_t kl = strlen(kDblKey[k]);
      if (sumStart - pos < kl + 17 || memcmp(buf + pos, kDblKey[k], kl) != 0 ||
          !fp_is_hex16(buf + pos + kl) || buf[pos + kl + 16] != '\n') {
        return fp_reset(p, FOH_PERSIST_RESET_CORRUPT, "grammar");
      }
      const double d = fp_double(fp_parse_hex16(buf + pos + kl));
      // DOMAIN, per key: phantomThreshold is on the checksum surface
      // (hitDetection.js:335) and the two levels are audiomenu's clamped
      // [0,1] (audiomenu.js:104-112). An out-of-domain value is corruption,
      // never something to clamp silently — that is how the qjs
      // Number("")-zeroing defect got to flip physics unseen.
      const double hi = (k == 0) ? 1000.0 : 1.0;
      if (!fp_in_range(d, hi)) {
        return fp_reset(p, FOH_PERSIST_RESET_CORRUPT, "domain");
      }
      *dd[k] = d;
      pos += kl + 17;
    }
  }
  }
  // v5 BLOCK (fix_plan A31) — present only in v5 files. A v4-or-older file
  // keeps the identity binding foh_persist_defaults(&v) installed.
  if (ver >= 5) {
    for (int k = 0; k < CTL_BIND_PORTS; k++) {
      // "bind <port> <8 digits>" — fixed width, anchored, same discipline
      // as every other line: 5 + 1 + 8*2 + 1 = 23 bytes.
      if (sumStart - pos < 23 || memcmp(buf + pos, "bind ", 5) != 0 ||
          buf[pos + 5] < '0' || buf[pos + 5] > '3' || buf[pos + 22] != '\n') {
        return fp_reset(p, FOH_PERSIST_RESET_CORRUPT, "grammar");
      }
      // port-major progression, asserted BY POSITION like the rec rows
      if (buf[pos + 5] != (char)('0' + k)) {
        return fp_reset(p, FOH_PERSIST_RESET_CORRUPT, "order");
      }
      int slots[CTL_BTN_COUNT];
      for (int i = 0; i < (int)CTL_BTN_COUNT; i++) {
        const char sep = buf[pos + 6 + 2 * (size_t)i];
        const char d = buf[pos + 7 + 2 * (size_t)i];
        if (sep != ' ' || d < '0' || d > '7') {
          return fp_reset(p, FOH_PERSIST_RESET_CORRUPT, "grammar");
        }
        slots[i] = d - '0';
      }
      // DOMAIN: the row must be a PERMUTATION. A duplicate slot would
      // silently delete an action from the player's controller — the same
      // class as the qjs Number("")-zeroing defect, so it is corruption,
      // never something to repair quietly.
      bool seen[CTL_BTN_COUNT] = {false};
      for (int i = 0; i < (int)CTL_BTN_COUNT; i++) {
        if (seen[slots[i]]) {
          return fp_reset(p, FOH_PERSIST_RESET_CORRUPT, "domain");
        }
        seen[slots[i]] = true;
        v.bind[k][i] = slots[i];
      }
      pos += 23;
    }
  }
  // v6 BLOCK (fix_plan A49) — present only in v6 files. A v5-or-older file
  // keeps the fresh-install marth selection foh_persist_defaults(&v) put in.
  if (ver >= 6) {
    // "sel <c> <c> <c> <c>" — fixed width, anchored: 4 + 4*2 = 12 bytes.
    if (sumStart - pos < 12 || memcmp(buf + pos, "sel ", 4) != 0 ||
        buf[pos + 11] != '\n') {
      return fp_reset(p, FOH_PERSIST_RESET_CORRUPT, "grammar");
    }
    for (int k = 0; k < FOH_CSS_PORTS; k++) {
      const char d = buf[pos + 4 + 2 * (size_t)k];
      const char sep = buf[pos + 5 + 2 * (size_t)k];
      if (sep != (k == FOH_CSS_PORTS - 1 ? '\n' : ' ')) {
        return fp_reset(p, FOH_PERSIST_RESET_CORRUPT, "grammar");
      }
      // DOMAIN: a roster id, 0..4 (CHARIDS, five characters — the same five
      // pipeline/expected.json pins). A sixth id would index charAttributes
      // out of bounds at launch, so it is corruption and resets loudly; it
      // is never clamped, which is the qjs Number("")-zeroing lesson.
      if (d < '0' || d > '4') {
        return fp_reset(p, FOH_PERSIST_RESET_CORRUPT, "domain");
      }
      v.selChar[k] = d - '0';
    }
    pos += 12;
  }
  // nothing may sit between the last content line and the SUM line
  if (pos != sumStart) return fp_reset(p, FOH_PERSIST_RESET_CORRUPT, "grammar");
  *p = v;
  // A migrated older file is LOADED, not reset — but it says so loudly,
  // so an upgrade is never silent. The next save republishes it as v6.
  if (fromVer) fprintf(stderr, "foh_persist: migrated from=%d\n", fromVer);
  fprintf(stderr, "foh_persist: loaded\n");
  return FOH_PERSIST_LOADED;
}

// --- atomic save ------------------------------------------------------------

void foh_persist_save(const FohPersist *p) {
  static char buf[FP_CAP];
  const size_t n = fp_serialize(p, buf, sizeof buf);
  const char *dir = foh_persist_dir();
  if (mkdir(dir, 0755) != 0 && errno != EEXIST) {
    gfx_fatal("foh_persist: save failed — cannot create the persist dir");
  }
  char tmp[512], fin[512];
  if (snprintf(tmp, sizeof tmp, "%s/%s", dir, FP_TMP) >= (int)sizeof tmp ||
      snprintf(fin, sizeof fin, "%s/%s", dir, FP_FILE) >= (int)sizeof fin) {
    gfx_fatal("foh_persist: dir path overflow");
  }
  FILE *f = fopen(tmp, "wb");
  if (!f) gfx_fatal("foh_persist: save failed — cannot open mlfk-persist.tmp");
  if (fwrite(buf, 1, n, f) != n) {
    gfx_fatal("foh_persist: save failed — tmp write");
  }
  if (fflush(f) != 0) gfx_fatal("foh_persist: save failed — tmp flush");
  if (fsync(fileno(f)) != 0) gfx_fatal("foh_persist: save failed — tmp fsync");
  if (fclose(f) != 0) gfx_fatal("foh_persist: save failed — tmp close");
  // the ONLY publish: atomic rename over the real file
  if (rename(tmp, fin) != 0) {
    gfx_fatal("foh_persist: save failed — rename publish");
  }
  // directory durability, best-effort for the FAT class (EINVAL/
  // ENOTSUP tolerated; a real I/O error is still loud). review-100 M3:
  // open(dir) FAILURE is no longer a silent skip — the rename is
  // published but its directory entry was not proven durable, so the
  // save is reported through a DISTINCT loud token (saved-nodirsync),
  // NEVER the plain `saved`. The fsync EINVAL/ENOTSUP tolerance (the
  // reviewed FAT class) is unchanged and keeps the plain `saved`; real
  // durability is proven end-to-end by the reboot round-trip leg, not
  // by an fsync rc.
  bool dirDurable = true;
  const int dfd = open(dir, O_RDONLY);
  if (dfd >= 0) {
    if (fsync(dfd) != 0 && errno != EINVAL && errno != ENOTSUP) {
      gfx_fatal("foh_persist: save failed — dir fsync");
    }
    close(dfd);
  } else {
    dirDurable = false;
  }
  fprintf(stderr, dirDurable ? "foh_persist: saved\n"
                             : "foh_persist: saved-nodirsync\n");
}

// --- machine glue (single definition site) ----------------------------------

// The bound render-state, captured at apply (review-100 M1: the
// same-process stale-PB product bug). foh_persist_record_update
// refreshes bound->targetRecords at the SAME improved-write so a
// same-process return-to-target-select renders the NEW record without
// a restart. ONE mechanism at the ONE write site — no scattered syncs,
// no render read-through. Lifetime: the bound FohState is the driver's
// live main()-scope state; record_update only fires (through the
// finishGame hook) during that scope, so the pointer is always valid.
// NULL until apply — record_update's refresh is a guarded no-op then
// (e.g. the standalone --tooth-persist-finish arm, which loads but
// never applies).
static FohState *g_bound = 0;

void foh_persist_apply(const FohPersist *p, FohState *s) {
  s->turbo = p->turbo;
  s->lCancelType = p->lCancelType;
  s->flashOnLCancel = p->flashOnLCancel;
  s->everyCharWallJump = p->everyCharWallJump;
  s->phantomThreshold = p->phantomThreshold;
  s->blastzoneWrapping = p->blastzoneWrapping;
  s->dustLessPerfectWavedash = p->dustLessPerfectWavedash;
  s->masterVolume[0] = p->masterVolume[0];
  s->masterVolume[1] = p->masterVolume[1];
  for (int k = 0; k < 4; k++) s->tapJumpOff[k] = p->tapJumpOff[k];
  memcpy(s->targetRecords, p->targetRecords, sizeof s->targetRecords);
  // A49/D45: the SELECTION plane, and the TOKEN plane RE-HOMED FROM IT.
  //
  // Writing both here is the whole of observable (b) at boot, and it is
  // D21/D35/D46's rule stated once more: a token is re-homed from the
  // SELECTION, never from anything else. Only `selChar` is on disk — if the
  // token plane were persisted separately the two could come back
  // disagreeing, and a player would boot looking at a character he did not
  // pick, which is CONTEXT.md's costliest defect on this exact screen.
  //
  // cssTokenRest is deliberately NOT touched: foh_init's memset leaves it at
  // 0 (the A-drop slot) and, since D46, every slot draws on the selection
  // anyway. The bound is FOH_CSS_PORTS because the plane is four ports wide.
  for (int k = 0; k < FOH_CSS_PORTS; k++) {
    s->selChar[k] = p->selChar[k];
    s->cssChar[k] = p->selChar[k];
  }
  g_bound = s; // review-100 M1: bind for the record-time refresh
}

void foh_persist_collect(FohPersist *p, const FohState *s) {
  p->turbo = s->turbo;
  p->lCancelType = s->lCancelType;
  p->flashOnLCancel = s->flashOnLCancel;
  p->everyCharWallJump = s->everyCharWallJump;
  p->phantomThreshold = s->phantomThreshold;
  p->blastzoneWrapping = s->blastzoneWrapping;
  p->dustLessPerfectWavedash = s->dustLessPerfectWavedash;
  p->masterVolume[0] = s->masterVolume[0];
  p->masterVolume[1] = s->masterVolume[1];
  for (int k = 0; k < 4; k++) p->tapJumpOff[k] = s->tapJumpOff[k];
  // A49/D45: the SELECTION plane. `cssChar` is NOT collected — it is a view
  // of this one (foh.h), and storing a view is how two representations drift.
  for (int k = 0; k < FOH_CSS_PORTS; k++) p->selChar[k] = s->selChar[k];
  // records are chokepoint-owned: they change ONLY through
  // foh_persist_record_update (the finishGame arm), never collected
  // back from the display copy.
}

bool foh_persist_record_update(FohPersist *p, int ch, int tstage,
                               double matchTimer) {
  if (ch < 0 || ch >= FOH_PERSIST_CHARS || tstage < 0 ||
      tstage >= FOH_PERSIST_TSTAGES ||
      !(isfinite(matchTimer) && matchTimer >= 0.0 &&
        matchTimer < FP_TIME_CAP)) {
    gfx_fatal("foh_persist: record update out of domain");
  }
  const double rec = p->targetRecords[ch][tstage];
  // main.js:1442: matchTimer < rec || rec == -1
  const bool improved = (matchTimer < rec) || (rec == -1.0);
  if (improved) {
    p->targetRecords[ch][tstage] = matchTimer;
    // review-100 M1: refresh the bound render copy at the SAME write so
    // a same-process return-to-target-select renders the new record
    // (the chokepoint owns the sync; no driver-side plumbing).
    if (g_bound) g_bound->targetRecords[ch][tstage] = matchTimer;
  }
  fprintf(stderr, "foh_persist: record char=%d tstage=%d improved=%d\n", ch,
          tstage, improved ? 1 : 0);
  return improved;
}
