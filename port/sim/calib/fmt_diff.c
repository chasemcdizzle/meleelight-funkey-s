// fmt_diff.c — M2 task 15 differential driver: the C ECMAScript
// formatter (port/sim/ml_fmt.c) + CHECKSUM.md `ser` (port/sim/ml_ser.c)
// vs the JS oracle (V8 String(x) / oracle/harness/pagelib.js ser),
// byte-for-byte. Modes:
//
//   --self-test
//       SHA-256 NIST self-test + a pinned anchor table (bit pattern ->
//       expected String(x) / ser bytes, generated from node and frozen
//       here) + JSON string-escaping pins.
//   --gen <out.hex>
//       Deterministic adversarial corpus: one 16-hex-digit IEEE-754
//       double bit pattern per line (big-endian, canon d:-style).
//       Specials, every exponent x mantissa template, subnormals,
//       powers of 2 and 10 (+/- ulp spreads), ECMA n<=21 / n>-6
//       threshold straddles, decimal-digit-count ladders, known-hard
//       shortest-repr cases, millions of seeded-PRNG raw/structured
//       patterns. Byte-stable across runs (seeded splitmix64).
//   --format <in.hex> <out.txt>
//       For each input pattern x: "String(x) TAB serNum(x) LF"
//       (serNum = CHECKSUM.md §3.4: String + the explicit -0 token).
//   --extract <out.hex> <capture.jsonl>...
//       Scans capture files for canon `d:<16hex>` number tokens, dumps
//       the UNIQUE bit patterns, sorted ascending, one per line.
//   --composite <in.txt> <out.txt> [--dump N]
//       Case file from fmt-composite.js: `V TAB <canon>` (serialize the
//       parsed canon value with the CHECKSUM.md ser) or `E TAB
//       pt0,pt1,pt2,pt3 TAB p0 TAB p1 TAB p2 TAB p3 TAB articles`
//       (build the FULL §3.1 fixed-literal-order frame envelope from
//       parsed player snapshots + article queue: pagelib.js:41-64).
//       Emits "sha256hex TAB bytelen LF" per case (§4 hash). --dump N
//       prints case N's full serialized bytes to stdout for diagnosis.
//
// Like every TU: cc -O2 -ffp-contract=off -Wall -Wextra -Werror.
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "canon.h"
#include "ml_fmt.h"
#include "ml_ser.h"
#include "sha256.h"

// --- helpers ---------------------------------------------------------------

static double bits2d(uint64_t b) {
  double d;
  memcpy(&d, &b, 8);
  return d;
}
static uint64_t d2bits(double d) {
  uint64_t b;
  memcpy(&b, &d, 8);
  return b;
}

static uint64_t sm_state;
static uint64_t sm64(void) { // splitmix64 (public domain, Vigna)
  uint64_t z = (sm_state += UINT64_C(0x9E3779B97F4A7C15));
  z = (z ^ (z >> 30)) * UINT64_C(0xBF58476D1CE4E5B9);
  z = (z ^ (z >> 27)) * UINT64_C(0x94D049BB133111EB);
  return z ^ (z >> 31);
}

static FILE *xopen(const char *path, const char *mode) {
  FILE *f = fopen(path, mode);
  if (!f) {
    fprintf(stderr, "fmt_diff: cannot open %s\n", path);
    exit(2);
  }
  return f;
}

// --- self-test -------------------------------------------------------------

// Anchors: expected strings are V8's String(x) (node v22), frozen here as
// regression teeth; the binding proof is the differential corpus.
static const struct { uint64_t bits; const char *str; const char *ser; } kAnchors[] = {
  {UINT64_C(0x0000000000000000), "0", "0"},
  {UINT64_C(0x8000000000000000), "0", "-0"}, // the §3.4 explicit -0 token
  {UINT64_C(0x7ff8000000000000), "NaN", "NaN"},
  {UINT64_C(0x7ff0000000000001), "NaN", "NaN"}, // payload NaNs stay "NaN"
  {UINT64_C(0xffffffffffffffff), "NaN", "NaN"},
  {UINT64_C(0x7ff0000000000000), "Infinity", "Infinity"},
  {UINT64_C(0xfff0000000000000), "-Infinity", "-Infinity"},
  {UINT64_C(0x3ff0000000000000), "1", "1"},
  {UINT64_C(0xbff0000000000000), "-1", "-1"},
  {UINT64_C(0x3fb999999999999a), "0.1", "0.1"},
  {UINT64_C(0x3fd5555555555555), "0.3333333333333333", "0.3333333333333333"},
  {UINT64_C(0x0000000000000001), "5e-324", "5e-324"},
  {UINT64_C(0x000fffffffffffff), "2.225073858507201e-308", "2.225073858507201e-308"},
  {UINT64_C(0x0010000000000000), "2.2250738585072014e-308", "2.2250738585072014e-308"},
  {UINT64_C(0x7fefffffffffffff), "1.7976931348623157e+308", "1.7976931348623157e+308"},
  {UINT64_C(0x4340000000000000), "9007199254740992", "9007199254740992"},
  {UINT64_C(0x4340000000000001), "9007199254740994", "9007199254740994"},
  {UINT64_C(0x437b69b4ba630f35), "123456789012345680", "123456789012345680"},
  {UINT64_C(0x43b0000000000000), "1152921504606847000", "1152921504606847000"},
  {UINT64_C(0x444b1ae4d6e2ef50), "1e+21", "1e+21"},   // ECMA n>21 threshold
  {UINT64_C(0x4480f0cf064dd592), "1e+22", "1e+22"},
  {UINT64_C(0x3e7ad7f29abcaf48), "1e-7", "1e-7"},     // ECMA n<=-6 threshold
  {UINT64_C(0x3eb0c6f7a0b5ed8d), "0.000001", "0.000001"},
  {UINT64_C(0x400921fb54442d18), "3.141592653589793", "3.141592653589793"},
  {UINT64_C(0x4011666666666666), "4.35", "4.35"},
  {UINT64_C(0x3ff0147ae147ae14), "1.005", "1.005"},
  {UINT64_C(0xc05119999999999a), "-68.4", "-68.4"},
  {UINT64_C(0x4045000000000000), "42", "42"},
};

static int self_test(void) {
  if (sha256_self_test() != 0) {
    fprintf(stderr, "fmt_diff: SHA-256 NIST self-test FAILED\n");
    return 1;
  }
  int bad = 0;
  char buf[ML_FMT_DTOA_MAX];
  MlSb sb;
  ml_sb_init(&sb);
  for (size_t i = 0; i < sizeof kAnchors / sizeof kAnchors[0]; ++i) {
    ml_fmt_dtoa(bits2d(kAnchors[i].bits), buf);
    if (strcmp(buf, kAnchors[i].str) != 0) {
      fprintf(stderr, "anchor String %016" PRIx64 ": got %s want %s\n",
              kAnchors[i].bits, buf, kAnchors[i].str);
      bad = 1;
    }
    ml_sb_reset(&sb);
    ml_sb_num(&sb, bits2d(kAnchors[i].bits));
    if (strcmp(sb.buf, kAnchors[i].ser) != 0) {
      fprintf(stderr, "anchor ser %016" PRIx64 ": got %s want %s\n",
              kAnchors[i].bits, sb.buf, kAnchors[i].ser);
      bad = 1;
    }
  }
  // JSON.stringify escaping pins (node: JSON.stringify("\b\t\n\f\r\"\\"))
  ml_sb_reset(&sb);
  ml_sb_jsonstr(&sb, "\x01\b\t\n\f\r\"\\\x1f");
  if (strcmp(sb.buf, "\"\\u0001\\b\\t\\n\\f\\r\\\"\\\\\\u001f\"") != 0) {
    fprintf(stderr, "jsonstr escaping: got %s\n", sb.buf);
    bad = 1;
  }
  ml_sb_reset(&sb);
  ml_sb_jsonstr(&sb, "FALLSPECIAL");
  if (strcmp(sb.buf, "\"FALLSPECIAL\"") != 0) { fprintf(stderr, "jsonstr plain\n"); bad = 1; }
  // §4 hash pin: sha256("abc") lowercase hex.
  char hex[65];
  ml_sha256_hex("abc", 3, hex);
  if (strcmp(hex, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad") != 0) {
    fprintf(stderr, "sha256 hex pin failed: %s\n", hex);
    bad = 1;
  }
  ml_sb_free(&sb);
  if (!bad) printf("fmt_diff self-test OK (%zu anchors)\n", sizeof kAnchors / sizeof kAnchors[0]);
  return bad;
}

// --- corpus generation -----------------------------------------------------

static uint64_t gen_count;
static void put_bits(FILE *f, uint64_t b) {
  fprintf(f, "%016" PRIx64 "\n", b);
  ++gen_count;
}
static void put_spread(FILE *f, uint64_t b, int ulps) {
  // b and +/-1..ulps neighbors (bit-pattern arithmetic == ulp walk within
  // finite space), both signs.
  for (int d = -ulps; d <= ulps; ++d) {
    uint64_t nb = b + (uint64_t) (int64_t) d;
    if ((nb & UINT64_C(0x7fffffffffffffff)) >> 52 == 0x7ff) continue; // skip Inf/NaN wrap
    put_bits(f, nb);
    put_bits(f, nb ^ UINT64_C(0x8000000000000000));
  }
}

static void gen_corpus(const char *path) {
  FILE *f = xopen(path, "w");
  gen_count = 0;

  // 1. specials
  static const uint64_t specials[] = {
    UINT64_C(0x0000000000000000), UINT64_C(0x8000000000000000),
    UINT64_C(0x7ff0000000000000), UINT64_C(0xfff0000000000000),
    UINT64_C(0x7ff8000000000000), UINT64_C(0xfff8000000000000),
    UINT64_C(0x7ff0000000000001), UINT64_C(0x7ff4000000000001),
    UINT64_C(0x7fffffffffffffff), UINT64_C(0xffffffffffffffff),
  };
  for (size_t i = 0; i < sizeof specials / sizeof specials[0]; ++i) put_bits(f, specials[i]);

  // 2. every exponent x mantissa templates, both signs (covers all powers
  // of two incl. subnormal boundary + max-mantissa at every scale)
  static const uint64_t mans[] = {
    0, 1, 2, UINT64_C(0x8000000000000), UINT64_C(0xfffffffffffff),
    UINT64_C(0xaaaaaaaaaaaaa), UINT64_C(0x5555555555555), UINT64_C(0x4000000000001),
  };
  for (uint64_t e = 0; e <= 2046; ++e)
    for (size_t m = 0; m < sizeof mans / sizeof mans[0]; ++m) {
      uint64_t b = (e << 52) | mans[m];
      put_bits(f, b);
      put_bits(f, b | UINT64_C(0x8000000000000000));
    }

  // 3. subnormal bit ladders
  for (int j = 0; j <= 51; ++j) {
    put_spread(f, UINT64_C(1) << j, 2);
    put_spread(f, (UINT64_C(1) << j) - 1, 2);
  }

  // 4. powers of 10 +/- 3 ulp (strtod only chooses test INPUTS here; the
  // formatter under test never parses)
  for (int k = -330; k <= 309; ++k) {
    char lit[32];
    snprintf(lit, sizeof lit, "1e%d", k);
    double v = strtod(lit, NULL);
    if (v == 0.0 || v > 1.7976931348623157e308) continue;
    put_spread(f, d2bits(v), 3);
    snprintf(lit, sizeof lit, "5e%d", k);
    v = strtod(lit, NULL);
    if (v == 0.0 || v > 1.7976931348623157e308) continue;
    put_spread(f, d2bits(v), 3);
  }

  // 5. ECMA formatting-threshold straddles (n=21/22 and n=-6/-7 with many
  // digit counts) + known-hard shortest/parse cases
  static const char *lits[] = {
    "1e21", "1e-7", "1e-6", "1e22", "1e20", "1e-8",
    "9.999999999999999e20", "1.0000000000000001e21", "999999999999999900000",
    "123456789012345680000", "1.2345678901234567e21", "1.2345678901234567e20",
    "9.999999999999997e-7", "1.0000000000000001e-7", "9.999999999999999e-8",
    "1.5e21", "1.05e21", "1.005e-6", "2.5e-7", "7.5e-7",
    "9007199254740992", "9007199254740993", "9007199254740994", "9007199254740995",
    "4503599627370495.5", "4503599627370496", "4503599627370497",
    "2.2250738585072011e-308", "2.2250738585072012e-308",
    "0.500000000000000166533453693773481063544750213623046875",
    "1.00000000000000011102230246251565404236316680908203125",
    "1e23", "8.98846567431158e307", "8.988465674311579e307",
    "4.450147717014403e-308", "2.4703282292062327e-324", "4.9e-324",
    "0.1", "0.2", "0.3", "0.4", "0.5", "0.6", "0.7", "0.8", "0.9",
    "0.01", "0.001", "0.0001", "0.00001", "0.000001", "0.0000001",
    "1.005", "4.35", "1125899906842624.125", "5.9604644775390625e-8",
    "2.98023223876953125e-8", "0.000030517578125",
    "3.518437208883201e13", "6.6335268e-316", "3.7920501898711395e-10",
    "1.7800590868057611e-307", "2.8480945388892175e-306",
    "2.446494580089078e-296", "1.2345678901234567e-59",
    "31.245270191439438", "121.44548662524972", "0.35544106882545625",
    "0.000035689", "123.456e-67", "988.5678e103",
  };
  for (size_t i = 0; i < sizeof lits / sizeof lits[0]; ++i) {
    double v = strtod(lits[i], NULL);
    put_spread(f, d2bits(v), 64); // wide ulp spread across each anchor
  }

  // 6. seeded random: raw uniform u64 bit patterns (uniform over the
  // exponent range; includes NaN/Inf/subnormals naturally)
  sm_state = UINT64_C(0x0badf00dcafe1337);
  for (int i = 0; i < 4000000; ++i) put_bits(f, sm64());

  // 7. seeded random integers (the integer/trailing-zero fast path and the
  // k<=n<=21 branch), raw + scaled by powers of 10
  static const int64_t p10i[] = {1, 10, 100, 1000, 10000, 100000, 1000000, 10000000};
  for (int i = 0; i < 500000; ++i) {
    int digits = (int) (sm64() % 17) + 1;
    uint64_t mod = 1;
    for (int d = 0; d < digits; ++d) mod *= 10;
    int64_t n = (int64_t) (sm64() % mod);
    double v = (double) n * (double) p10i[sm64() % 8];
    if (sm64() & 1) v = -v;
    put_bits(f, d2bits(v));
  }

  // 8. seeded random short-decimal literals m * 10^k (1..17 significant
  // digits, exponents straddling every formatting branch)
  for (int i = 0; i < 500000; ++i) {
    int digits = (int) (sm64() % 17) + 1;
    uint64_t mod = 1;
    for (int d = 0; d < digits; ++d) mod *= 10;
    char lit[48];
    snprintf(lit, sizeof lit, "%s%" PRIu64 "e%d", (sm64() & 1) ? "-" : "",
             sm64() % mod, (int) (sm64() % 61) - 30);
    put_bits(f, d2bits(strtod(lit, NULL)));
  }

  // 9. seeded random subnormals + near-overflow/underflow exponents
  for (int i = 0; i < 200000; ++i) {
    uint64_t b = (sm64() & UINT64_C(0xfffffffffffff)) | ((uint64_t) (sm64() & 1) << 63);
    put_bits(f, b); // exponent field 0: subnormal
  }
  for (int i = 0; i < 200000; ++i) {
    static const uint64_t exps[] = {1, 2, 3, 2044, 2045, 2046};
    uint64_t b = (exps[sm64() % 6] << 52) | (sm64() & UINT64_C(0xfffffffffffff)) |
                 ((uint64_t) (sm64() & 1) << 63);
    put_bits(f, b);
  }

  fclose(f);
  printf("corpus: %" PRIu64 " patterns -> %s\n", gen_count, path);
}

// --- format mode -----------------------------------------------------------

static int do_format(const char *inpath, const char *outpath) {
  FILE *in = xopen(inpath, "r");
  FILE *out = xopen(outpath, "w");
  char line[128];
  char a[ML_FMT_DTOA_MAX];
  uint64_t nlines = 0;
  MlSb sb;
  ml_sb_init(&sb);
  while (fgets(line, sizeof line, in)) {
    if (line[0] == '\n' || line[0] == '\0') continue;
    char *end = NULL;
    uint64_t b = strtoull(line, &end, 16);
    if (end == line || (*end != '\n' && *end != '\0')) {
      fprintf(stderr, "fmt_diff: bad hex line %" PRIu64 ": %s", nlines + 1, line);
      return 2;
    }
    double x = bits2d(b);
    ml_fmt_dtoa(x, a);
    ml_sb_reset(&sb);
    ml_sb_num(&sb, x);
    fputs(a, out);
    fputc('\t', out);
    fputs(sb.buf, out);
    fputc('\n', out);
    ++nlines;
  }
  ml_sb_free(&sb);
  fclose(in);
  fclose(out);
  printf("formatted %" PRIu64 " patterns -> %s\n", nlines, outpath);
  return 0;
}

// --- extract mode ----------------------------------------------------------

typedef struct {
  uint64_t *keys;
  uint8_t *used;
  size_t cap, count;
} Set64;

static void set_init(Set64 *s, size_t cap) {
  s->cap = cap;
  s->count = 0;
  s->keys = (uint64_t *) malloc(cap * 8);
  s->used = (uint8_t *) calloc(cap, 1);
  if (!s->keys || !s->used) { fprintf(stderr, "fmt_diff: OOM\n"); exit(2); }
}
static void set_insert(Set64 *s, uint64_t k);
static void set_grow(Set64 *s) {
  Set64 ns;
  set_init(&ns, s->cap * 4);
  for (size_t i = 0; i < s->cap; ++i)
    if (s->used[i]) set_insert(&ns, s->keys[i]);
  free(s->keys);
  free(s->used);
  *s = ns;
}
static void set_insert(Set64 *s, uint64_t k) {
  if (s->count * 2 >= s->cap) set_grow(s);
  uint64_t h = k * UINT64_C(0x9E3779B97F4A7C15);
  size_t i = (size_t) (h & (s->cap - 1));
  while (s->used[i]) {
    if (s->keys[i] == k) return;
    i = (i + 1) & (s->cap - 1);
  }
  s->used[i] = 1;
  s->keys[i] = k;
  ++s->count;
}

static int hexval(int c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  return -1;
}

static int cmp_u64(const void *a, const void *b) {
  uint64_t x = *(const uint64_t *) a, y = *(const uint64_t *) b;
  return x < y ? -1 : x > y ? 1 : 0;
}

static int do_extract(const char *outpath, int nfiles, char **files) {
  Set64 set;
  set_init(&set, (size_t) 1 << 20);
  enum { CHUNK = 1 << 22, TAIL = 32 };
  char *buf = (char *) malloc(CHUNK + TAIL);
  if (!buf) { fprintf(stderr, "fmt_diff: OOM\n"); return 2; }
  for (int fi = 0; fi < nfiles; ++fi) {
    FILE *f = xopen(files[fi], "rb");
    size_t have = 0;
    for (;;) {
      size_t got = fread(buf + have, 1, CHUNK, f);
      size_t total = have + got;
      bool final = (got == 0);
      if (total == 0) break;
      // scan for d:<16 hex> with a non-hex follower (or EOF). A token
      // whose follower byte is beyond this chunk is left for the carried
      // tail (TAIL >= 19 covers it); rescanned positions dedupe in the set.
      for (size_t i = 0; i + 18 <= total; ++i) {
        if (!final && i + 18 == total) break; // follower undecidable yet
        if (buf[i] != 'd' || buf[i + 1] != ':') continue;
        uint64_t v = 0;
        int ok = 1;
        for (int j = 0; j < 16; ++j) {
          int hv = hexval((unsigned char) buf[i + 2 + j]);
          if (hv < 0) { ok = 0; break; }
          v = (v << 4) | (uint64_t) hv;
        }
        if (ok && i + 18 < total && hexval((unsigned char) buf[i + 18]) >= 0)
          ok = 0; // longer hex run: not a canon double token
        if (ok) set_insert(&set, v);
      }
      if (final) break;
      // carry the tail across the chunk boundary
      size_t keep = total < TAIL ? total : TAIL;
      memmove(buf, buf + total - keep, keep);
      have = keep;
    }
    fclose(f);
  }
  free(buf);
  uint64_t *all = (uint64_t *) malloc(set.count * 8);
  if (!all) { fprintf(stderr, "fmt_diff: OOM\n"); return 2; }
  size_t n = 0;
  for (size_t i = 0; i < set.cap; ++i)
    if (set.used[i]) all[n++] = set.keys[i];
  qsort(all, n, 8, cmp_u64);
  FILE *out = xopen(outpath, "w");
  for (size_t i = 0; i < n; ++i) fprintf(out, "%016" PRIx64 "\n", all[i]);
  fclose(out);
  printf("extracted %zu unique double bit patterns from %d files -> %s\n",
         n, nfiles, outpath);
  free(all);
  free(set.keys);
  free(set.used);
  return 0;
}

// --- composite mode (CHECKSUM.md ser over parsed canon values) --------------

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
      // pagelib's ser emits; a non-sorted input would diff against the JS
      // reference (which re-sorts) and fail the check — a tripwire, never
      // a mask.
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
  fprintf(stderr, "fmt_diff: bad canon type %d\n", (int) v->type);
  exit(2);
}

static const CanonVal *obj_get(const CanonVal *obj, const char *key) {
  if (obj->type != CV_OBJ) return NULL;
  for (int i = 0; i < obj->nkeys; ++i)
    if (strcmp(obj->keys[i], key) == 0) return obj->vals[i];
  return NULL;
}

static const CanonVal *parse_or_die(const char *s, uint64_t lineno, const char *what) {
  const char *err = NULL;
  const CanonVal *v = canon_parse(s, &err);
  if (!v) {
    fprintf(stderr, "fmt_diff: composite line %" PRIu64 " (%s): canon parse error: %s\n",
            lineno, what, err ? err : "?");
    exit(2);
  }
  return v;
}

// The §2/§3.1 frame envelope, byte-for-byte pagelib.js:41-64
// (__serializeState): active players in slot order, each with the SEVEN
// allow-listed fields in FIXED literal order, then "articles".
static void ser_envelope(MlSb *sb, const int *ptype, const CanonVal **players,
                         const CanonVal *articles, uint64_t lineno) {
  static const char *kFields[] = {"actionState", "timer",    "percent", "stocks",
                                  "hit",         "hitboxes", "phys"};
  ml_sb_putc(sb, '{');
  bool first = true;
  for (int i = 0; i < 4; ++i) {
    if (ptype[i] <= -1) continue; // inactive slots are omitted entirely (§2)
    if (!players[i]) {
      fprintf(stderr, "fmt_diff: composite line %" PRIu64 ": active slot %d has no snapshot\n",
              lineno, i);
      exit(2);
    }
    if (!first) ml_sb_putc(sb, ',');
    first = false;
    char pk[8];
    snprintf(pk, sizeof pk, "\"p%d\":", i);
    ml_sb_puts(sb, pk);
    ml_sb_putc(sb, '{');
    for (size_t k = 0; k < sizeof kFields / sizeof kFields[0]; ++k) {
      const CanonVal *fv = obj_get(players[i], kFields[k]);
      if (!fv) {
        fprintf(stderr, "fmt_diff: composite line %" PRIu64 ": player %d missing %s\n",
                lineno, i, kFields[k]);
        exit(2);
      }
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

static int do_composite(const char *inpath, const char *outpath, long dump) {
  FILE *in = xopen(inpath, "r");
  FILE *out = xopen(outpath, "w");
  char *line = NULL;
  size_t cap = 0;
  ssize_t len;
  uint64_t lineno = 0, ncases = 0;
  MlSb sb;
  ml_sb_init(&sb);
  while ((len = getline(&line, &cap, in)) > 0) {
    ++lineno;
    if (line[len - 1] == '\n') line[--len] = '\0';
    if (len == 0) continue;
    canon_arena_reset();
    ml_sb_reset(&sb);
    if (line[0] == 'V' && line[1] == '\t') {
      ser_canon(&sb, parse_or_die(line + 2, lineno, "value"));
    } else if (line[0] == 'E' && line[1] == '\t') {
      // E \t pt0,pt1,pt2,pt3 \t p0 \t p1 \t p2 \t p3 \t articles
      char *fields[6];
      char *p = line + 2;
      for (int i = 0; i < 6; ++i) {
        fields[i] = p;
        if (i < 5) {
          char *tab = strchr(p, '\t');
          if (!tab) {
            fprintf(stderr, "fmt_diff: composite line %" PRIu64 ": bad E record\n", lineno);
            return 2;
          }
          *tab = '\0';
          p = tab + 1;
        }
      }
      int ptype[4];
      if (sscanf(fields[0], "%d,%d,%d,%d", &ptype[0], &ptype[1], &ptype[2], &ptype[3]) != 4) {
        fprintf(stderr, "fmt_diff: composite line %" PRIu64 ": bad ptype\n", lineno);
        return 2;
      }
      const CanonVal *players[4] = {NULL, NULL, NULL, NULL};
      for (int i = 0; i < 4; ++i)
        if (strcmp(fields[1 + i], "-") != 0)
          players[i] = parse_or_die(fields[1 + i], lineno, "player");
      const CanonVal *articles = parse_or_die(fields[5], lineno, "articles");
      ser_envelope(&sb, ptype, players, articles, lineno);
    } else {
      fprintf(stderr, "fmt_diff: composite line %" PRIu64 ": unknown case type\n", lineno);
      return 2;
    }
    ++ncases;
    if (dump == (long) ncases) {
      fwrite(sb.buf, 1, sb.len, stdout);
      fputc('\n', stdout);
    }
    char hex[65];
    ml_sha256_hex(sb.buf, sb.len, hex);
    fprintf(out, "%s\t%zu\n", hex, sb.len);
  }
  free(line);
  ml_sb_free(&sb);
  fclose(in);
  fclose(out);
  printf("composite: %" PRIu64 " cases -> %s\n", ncases, outpath);
  return 0;
}

// --- main --------------------------------------------------------------------

int main(int argc, char **argv) {
  if (argc >= 2 && strcmp(argv[1], "--self-test") == 0) return self_test();
  if (argc == 3 && strcmp(argv[1], "--gen") == 0) {
    gen_corpus(argv[2]);
    return 0;
  }
  if (argc == 4 && strcmp(argv[1], "--format") == 0) return do_format(argv[2], argv[3]);
  if (argc >= 4 && strcmp(argv[1], "--extract") == 0)
    return do_extract(argv[2], argc - 3, argv + 3);
  if ((argc == 4 || argc == 6) && strcmp(argv[1], "--composite") == 0) {
    long dump = 0;
    if (argc == 6) {
      if (strcmp(argv[4], "--dump") != 0) { fprintf(stderr, "usage\n"); return 2; }
      dump = atol(argv[5]);
    }
    return do_composite(argv[2], argv[3], dump);
  }
  fprintf(stderr,
          "usage: fmt_diff --self-test | --gen out.hex | --format in.hex out.txt |\n"
          "       --extract out.hex capture.jsonl... | --composite in.txt out.txt [--dump N]\n");
  return 2;
}
