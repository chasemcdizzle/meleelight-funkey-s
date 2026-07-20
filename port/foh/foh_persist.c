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

#include "../sim/ml_ser.h" // ml_sha256_hex (oracle/qjs/sha256.c by path)

#define FP_FILE "mlfk-persist.dat"
#define FP_TMP "mlfk-persist.tmp"
#define FP_DEFAULT_DIR "/mnt/mlfk-data"
// 55 lines, ~1.5 KB canonical — anything larger is not ours.
#define FP_CAP 4096
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
  memset(p, 0, sizeof *p); // settings.js:44-56 subset — all zero
  for (int c = 0; c < FOH_PERSIST_CHARS; c++) {
    for (int s = 0; s < FOH_PERSIST_TSTAGES; s++) {
      p->targetRecords[c][s] = -1.0; // targetplay.js:40
    }
  }
}

// --- canonical serialization (deterministic bytes; twin-cmp'd) --------------

static uint64_t fp_bits(double d) {
  uint64_t b;
  memcpy(&b, &d, 8);
  return b;
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
                   "MLFKPERSIST1\nturbo %d\nlcancel %d\n"
                   "tapjump %d %d %d %d\n",
                   p->turbo, p->lCancelType, p->tapJumpOff[0],
                   p->tapJumpOff[1], p->tapJumpOff[2], p->tapJumpOff[3]);
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
  // line 1: header. ^MLFKPERSIST[0-9]+$ with version != 1 -> version;
  // anything else -> header corruption.
  {
    size_t e = pos;
    while (e < sumStart && buf[e] != '\n') e++;
    if (e >= sumStart) return fp_reset(p, FOH_PERSIST_RESET_CORRUPT, "header");
    const size_t len = e - pos;
    if (len == 12 && memcmp(buf + pos, "MLFKPERSIST1", 12) == 0) {
      // current version
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
  // nothing may sit between the last rec row and the SUM line
  if (pos != sumStart) return fp_reset(p, FOH_PERSIST_RESET_CORRUPT, "grammar");
  *p = v;
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
  // ENOTSUP tolerated; a real I/O error is still loud)
  const int dfd = open(dir, O_RDONLY);
  if (dfd >= 0) {
    if (fsync(dfd) != 0 && errno != EINVAL && errno != ENOTSUP) {
      gfx_fatal("foh_persist: save failed — dir fsync");
    }
    close(dfd);
  }
  fprintf(stderr, "foh_persist: saved\n");
}

// --- machine glue (single definition site) ----------------------------------

void foh_persist_apply(const FohPersist *p, FohState *s) {
  s->turbo = p->turbo;
  s->lCancelType = p->lCancelType;
  for (int k = 0; k < 4; k++) s->tapJumpOff[k] = p->tapJumpOff[k];
  memcpy(s->targetRecords, p->targetRecords, sizeof s->targetRecords);
}

void foh_persist_collect(FohPersist *p, const FohState *s) {
  p->turbo = s->turbo;
  p->lCancelType = s->lCancelType;
  for (int k = 0; k < 4; k++) p->tapJumpOff[k] = s->tapJumpOff[k];
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
  if (improved) p->targetRecords[ch][tstage] = matchTimer;
  fprintf(stderr, "foh_persist: record char=%d tstage=%d improved=%d\n", ch,
          tstage, improved ? 1 : 0);
  return improved;
}
