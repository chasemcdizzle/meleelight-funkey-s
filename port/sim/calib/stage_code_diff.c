// stage_code_diff.c — the C half of A45 T1's differential rig
// (port/sim/check-stage-code.sh). Same discipline as fmt_diff.c: this
// program NEVER states what the answer should be, it only produces the C
// answer in a format the JS half — running upstream's OWN transpiled
// encode.js out of the read-only clone — produces line for line.
//
//   --genhex <out>            deterministic double corpus (bit patterns)
//   --tofixed <hex> <out>     one ml_to_fixed2 line per bit pattern
//   --ref <codes> <out>       per code: "NULL" | "OK <re-encoded code>"
//                             ("NULL" is spelled the same as the JS side's
//                             null return so the two files can be cmp'd;
//                             the rejecting rule goes to stderr, where it
//                             cannot silently become part of the judgment)
//   --self-test               value-model size + frozen anchors
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "stage_code.h"

static double bits_to_d(uint64_t b) {
  double d;
  memcpy(&d, &b, sizeof d);
  return d;
}

static uint64_t d_to_bits(double d) {
  uint64_t b;
  memcpy(&b, &d, sizeof b);
  return b;
}

// mulberry32 — the project's standard deterministic generator.
static uint32_t mb_state;
static uint32_t mb_next(void) {
  mb_state += 0x6D2B79F5u;
  uint32_t z = mb_state;
  z = (z ^ (z >> 15)) * (z | 1u);
  z ^= z + (z ^ (z >> 7)) * (z | 61u);
  return z ^ (z >> 14);
}

static void emit(FILE *f, uint64_t b) { fprintf(f, "%016" PRIx64 "\n", b); }

static int gen_hex(const char *path) {
  FILE *f = fopen(path, "w");
  if (f == NULL) return 1;

  // 1. The .5 boundary of toFixed(2), which is where every naive
  //    round(x * 100.0) implementation dies: the double nearest k/100 and
  //    its two neighbours, for both signs.
  for (int k = 0; k <= 40000; ++k) {
    const double x = (double)k / 100.0;
    const uint64_t b = d_to_bits(x);
    for (int s = 0; s < 2; ++s) {
      const uint64_t sign = (uint64_t)s << 63;
      emit(f, b | sign);
      emit(f, (b + 1) | sign);
      emit(f, (b == 0 ? 0 : b - 1) | sign);
    }
  }

  // 2. Halfway points in the other direction: k/1000 and k/200, whose
  //    exact values straddle a hundredth from both sides.
  for (int k = 0; k <= 20000; ++k) {
    emit(f, d_to_bits((double)k / 1000.0));
    emit(f, d_to_bits(-(double)k / 1000.0));
    emit(f, d_to_bits((double)k / 200.0));
  }

  // 3. Named boundaries: zero and -0, the subnormal floor, the integer
  //    plateau where String(x) stops being the exact value, and the 1e21
  //    ToString fallback on both sides.
  static const double named[] = {0.0,   -0.0,  1.0,   -1.0,  0.005, -0.005,
                                 0.015, 1.005, 8.575, 1e-7,  1e20,  1e21,
                                 -1e21, 1e22,  2.5,   -2.5,  0.5,   -0.5};
  for (size_t i = 0; i < sizeof named / sizeof named[0]; ++i) {
    const uint64_t b = d_to_bits(named[i]);
    emit(f, b);
    emit(f, b + 1);
    emit(f, b - 1);
  }
  emit(f, 0x0000000000000001ull);  // 5e-324
  emit(f, 0x000fffffffffffffull);  // largest subnormal
  emit(f, 0x0010000000000000ull);  // smallest normal
  emit(f, 0x4330000000000000ull);  // 2^52
  emit(f, 0x4340000000000000ull);  // 2^53
  emit(f, 0x433fffffffffffffull);  // 2^53 - 0.5
  emit(f, 0x43b0000000000000ull);  // 2^60
  emit(f, 0x7ff0000000000000ull);  // Infinity
  emit(f, 0xfff0000000000000ull);  // -Infinity
  emit(f, 0x7ff8000000000000ull);  // NaN
  emit(f, 0x7fefffffffffffffull);  // DBL_MAX
  emit(f, 0xffefffffffffffffull);  // -DBL_MAX

  // 4. Random patterns across the whole finite range (seeded, so the
  //    corpus is a frozen artifact, not a lottery).
  mb_state = 0x5A45C0DEu;
  for (int i = 0; i < 300000; ++i) {
    uint64_t b = ((uint64_t)mb_next() << 32) | mb_next();
    if (((b >> 52) & 0x7ffu) == 0x7ffu) b &= ~(1ull << 62);  // keep it finite
    emit(f, b);
  }
  // 5. Random patterns concentrated in the magnitudes a stage can hold.
  for (int i = 0; i < 200000; ++i) {
    const double m = (double)(mb_next() % 2000001u) / 100.0 - 10000.0;
    uint64_t b = d_to_bits(m);
    b += (mb_next() % 3u) - 1u;
    emit(f, b);
  }

  return fclose(f) == 0 ? 0 : 1;
}

static int do_tofixed(const char *in, const char *out) {
  FILE *fi = fopen(in, "r");
  FILE *fo = fopen(out, "w");
  if (fi == NULL || fo == NULL) return 1;
  char line[64];
  char buf[ML_TO_FIXED2_MAX];
  while (fgets(line, sizeof line, fi) != NULL) {
    if (line[0] == '\n' || line[0] == '\0') continue;
    const uint64_t b = strtoull(line, NULL, 16);
    ml_to_fixed2(bits_to_d(b), buf);
    fputs(buf, fo);
    fputc('\n', fo);
  }
  fclose(fi);
  return fclose(fo) == 0 ? 0 : 1;
}

static int do_ref(const char *in, const char *out) {
  FILE *fi = fopen(in, "r");
  FILE *fo = fopen(out, "w");
  if (fi == NULL || fo == NULL) return 1;
  // Codes reach MLK_CODE_MAX; neither buffer belongs on the stack.
  char *line = malloc(MLK_CODE_MAX + 2);
  char *code = malloc(MLK_CODE_MAX);
  MlkStage *st = malloc(sizeof *st);
  if (line == NULL || code == NULL || st == NULL) return 1;
  long n = 0;
  while (fgets(line, MLK_CODE_MAX + 2, fi) != NULL) {
    size_t len = strlen(line);
    while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r'))
      line[--len] = '\0';
    ++n;
    const char *reason = NULL;
    if (!mlk_parse(line, st, &reason)) {
      fprintf(fo, "NULL\n");
      fprintf(stderr, "  line %ld rejected: %s\n", n,
              reason != NULL ? reason : "?");
      continue;
    }
    const int w = mlk_encode(st, code, MLK_CODE_MAX);
    if (w < 0) {
      fprintf(fo, "ENCODE-OVERFLOW\n");
      continue;
    }
    fprintf(fo, "OK %s\n", code);
  }
  fclose(fi);
  free(line);
  free(code);
  free(st);
  return fclose(fo) == 0 ? 0 : 1;
}

#define CHECK(cond)                                                       \
  do {                                                                    \
    if (!(cond)) {                                                        \
      fprintf(stderr, "SELF-TEST FAIL %s:%d: %s\n", __FILE__, __LINE__,   \
              #cond);                                                     \
      return 1;                                                           \
    }                                                                     \
  } while (0)

static int fixed_is(double x, const char *want) {
  char b[ML_TO_FIXED2_MAX];
  ml_to_fixed2(x, b);
  if (strcmp(b, want) != 0) {
    fprintf(stderr, "SELF-TEST FAIL toFixed2: got %s want %s\n", b, want);
    return 0;
  }
  return 1;
}

static int self_test(void) {
  printf("  sizeof(MlkStage) = %zu bytes; MLK_CODE_MAX = %d\n", sizeof(MlkStage),
         MLK_CODE_MAX);

  // Frozen toFixed anchors. Every one of these is also covered by the
  // corpus differential; they are here so a broken build fails fast and
  // legibly instead of producing a megabyte of diff.
  CHECK(fixed_is(0.0, "0.00"));
  CHECK(fixed_is(-0.0, "0.00"));       // step 4: -0 is not < 0
  CHECK(fixed_is(-0.001, "-0.00"));    // ... but -0.001 keeps its sign
  CHECK(fixed_is(0.005, "0.01"));
  CHECK(fixed_is(0.015, "0.01"));
  CHECK(fixed_is(1.005, "1.00"));
  CHECK(fixed_is(8.575, "8.57"));
  CHECK(fixed_is(1e-7, "0.00"));
  CHECK(fixed_is(bits_to_d(1), "0.00"));  // 5e-324
  CHECK(fixed_is(1152921504606846976.0, "1152921504606846976.00"));  // 2^60
  CHECK(fixed_is(1e20, "100000000000000000000.00"));
  CHECK(fixed_is(1e21, "1e+21"));
  CHECK(fixed_is(-1e21, "-1e+21"));
  CHECK(fixed_is(1.0 / 0.0, "Infinity"));
  CHECK(fixed_is(-1.0 / 0.0, "-Infinity"));
  CHECK(fixed_is(bits_to_d(0x7ff8000000000000ull), "NaN"));

  // The parser reaches -0 without strtod, and the sign survives.
  MlkStage *st = malloc(sizeof *st);
  CHECK(st != NULL);
  const char *why = NULL;
  CHECK(mlk_parse("-0.00,-0.00&1&&&&&&&&&&&-250.00,-250.00,250.00,250.00&3.00",
                  st, &why));
  CHECK(d_to_bits(st->startingPoint[0].x) == 0x8000000000000000ull);

  // BUG 2 is carried in the value model, not merely in a comment.
  CHECK(mlk_parse(
      "0.00,0.00&1&&&&&&&1.00,1.00,2.00,2.00,3.00,1.00&&&&-250.00,-250.00,"
      "250.00,250.00&3.00",
      st, &why));
  CHECK(st->polygon.count == 1 && st->polygonMapCount == 1);
  CHECK(st->polygonMapIsNull[0]);

  // Encode refuses rather than truncating when the buffer is short.
  char small[8];
  CHECK(mlk_encode(st, small, sizeof small) == -1);

  free(st);
  printf("  self-test OK\n");
  return 0;
}

int main(int argc, char **argv) {
  if (argc >= 2 && strcmp(argv[1], "--self-test") == 0) return self_test();
  if (argc == 3 && strcmp(argv[1], "--genhex") == 0) return gen_hex(argv[2]);
  if (argc == 4 && strcmp(argv[1], "--tofixed") == 0)
    return do_tofixed(argv[2], argv[3]);
  if (argc == 4 && strcmp(argv[1], "--ref") == 0) return do_ref(argv[2], argv[3]);
  fprintf(stderr, "usage: stage_code_diff --self-test | --genhex <out> | "
                  "--tofixed <hex> <out> | --ref <codes> <out>\n");
  return 2;
}
