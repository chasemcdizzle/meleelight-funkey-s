// mathsweep.c — M3 device-vs-host differential sweep over the sim's
// NON-transcendental libm surface: floor, ceil, sqrt, fabs, fmod, plus
// the composed js_round (ml_js.h) — i.e. every raw libm function the
// device sim TU set can reach (measured, iter 38: floor ~86 sites,
// ceil via js_round, sqrt 8 sites, fmod 30 sites, fabs via js_abs;
// no raw round/trunc/exp/log call sites exist).
//
// WHY: the FunKey SDK's static musl libc.a ships floor/ceil/round whose
// +-2^52 "toint" trick was optimized away when the SDK's libc was built
// (measured on device: floor(1.5) == 1.5). port/fdlibm/fdlibm.c now
// carries exact floor/ceil as strong symbol overrides; THIS tool proves
// those overrides — and the still-libm-resolved sqrt/fabs/fmod — equal a
// known-good libm, bit for bit, over a large deterministic corpus:
//
//   host build  (NO fdlibm.c linked)  = the macOS libm ANCHOR
//   arm build   (fdlibm.c linked, exactly like sim_device) = under test
//
// cmp(host output, device output) in check-device-g01.sh is the judge —
// the device never self-reports. NaN OUTPUTS are canonicalized to
// d:7ff8000000000000 before printing (fix_plan §M2 rule 9: NaN payloads
// are engine artifacts, canon collapses them; every non-NaN result is
// compared raw).
//
// Inputs: the fdlibm crosscheck input file (oracle/fdlibm-crosscheck/
// gen-inputs.js output, "<fn> <hex16> [<hex16>]" lines — the ~257k
// argument doubles are reused as sweep values; 2-arg lines also drive
// fmod both ways) prepended by a built-in exhaustive exponent battery:
// every exponent field x mantissa templates x both signs + specials.
//
// Usage: mathsweep <inputs.txt>   (output on stdout)
// Build: cc -O2 -ffp-contract=off -Wall -Wextra -Werror (like every TU).
#include <inttypes.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ml_js.h" // js_round (ceil-composed) — swept as a unit

static double u2d(uint64_t u) {
  double d;
  memcpy(&d, &u, 8);
  return d;
}

static uint64_t d2u(double d) {
  uint64_t u;
  memcpy(&u, &d, 8);
  return u;
}

// rule 9: collapse NaN payloads; print every other pattern raw.
static uint64_t canon_bits(double d) {
  if (isnan(d)) return UINT64_C(0x7ff8000000000000);
  return d2u(d);
}

static void sweep1(uint64_t bits) {
  double x = u2d(bits);
  printf("v %016" PRIx64 " f=%016" PRIx64 " c=%016" PRIx64 " s=%016" PRIx64
         " a=%016" PRIx64 " r=%016" PRIx64 "\n",
         bits, canon_bits(floor(x)), canon_bits(ceil(x)),
         canon_bits(sqrt(x)), canon_bits(fabs(x)), canon_bits(js_round(x)));
}

static void sweep2(uint64_t a, uint64_t b) {
  printf("m %016" PRIx64 " %016" PRIx64 " %016" PRIx64 " %016" PRIx64 "\n", a,
         b, canon_bits(fmod(u2d(a), u2d(b))), canon_bits(fmod(u2d(b), u2d(a))));
}

// --- strict corpus grammar (iter 40, review M3 round 2) --------------------
// A corpus line is EXACTLY one of (measured gen-inputs.js emission set):
//   <op> ' ' <hex16> '\n'                 op in {sin, cos, tan, atan}
//   <op> ' ' <hex16> ' ' <hex16> '\n'     op in {atan2, pow}
// where <hex16> is exactly 16 LOWERCASE hex chars. Anything else —
// unknown op, wrong arity, wrong operand width/case, trailing junk, an
// overlong or unterminated line — is corruption: exit 3 at the caller.

static int hex16(const char *s, uint64_t *out) {
  uint64_t v = 0;
  for (int i = 0; i < 16; i++) {
    char c = s[i];
    uint64_t d;
    if (c >= '0' && c <= '9')
      d = (uint64_t)(c - '0');
    else if (c >= 'a' && c <= 'f')
      d = (uint64_t)(c - 'a' + 10);
    else
      return 0;
    v = (v << 4) | d;
  }
  *out = v;
  return 1;
}

static const struct {
  const char *op;
  int arity;
} kOps[] = {
    {"sin", 1}, {"cos", 1}, {"tan", 1}, {"atan", 1}, {"atan2", 2}, {"pow", 2},
};

// Returns NULL on success (a/b/arity filled), else a reason string.
static const char *parse_line(const char *line, uint64_t *a, uint64_t *b,
                              int *arity) {
  size_t len = strlen(line);
  if (len == 0 || line[len - 1] != '\n')
    return "no terminating newline (overlong or truncated line)";
  int found = -1;
  size_t i = 0;
  for (size_t k = 0; k < sizeof kOps / sizeof *kOps; k++) {
    size_t ol = strlen(kOps[k].op);
    // the required following space disambiguates the "atan"/"atan2" prefix
    if (strncmp(line, kOps[k].op, ol) == 0 && line[ol] == ' ') {
      found = (int)k;
      i = ol + 1;
      break;
    }
  }
  if (found < 0) return "unknown op token";
  if (!hex16(line + i, a)) return "operand 1 not exactly 16 lowercase hex";
  i += 16;
  if (kOps[found].arity == 2) {
    if (line[i] != ' ') return "missing second operand";
    i++;
    if (!hex16(line + i, b)) return "operand 2 not exactly 16 lowercase hex";
    i += 16;
  }
  if (line[i] != '\n' || line[i + 1] != '\0') return "trailing junk";
  *arity = kOps[found].arity;
  return NULL;
}

int main(int argc, char **argv) {
  if (argc != 2) {
    fprintf(stderr, "usage: mathsweep <inputs.txt>\n");
    return 2;
  }

  // --- built-in battery: every exponent field x mantissa templates ------
  static const uint64_t mant[] = {
      UINT64_C(0x0000000000000),  UINT64_C(0x0000000000001),
      UINT64_C(0x8000000000000),  UINT64_C(0xFFFFFFFFFFFFF),
      UINT64_C(0x7FFFFFFFFFFFF),  UINT64_C(0x8000000000001),
      UINT64_C(0x0000000100000),
  };
  uint64_t prev = 0;
  for (uint64_t e = 0; e <= 0x7FE; e++) {
    for (size_t m = 0; m < sizeof mant / sizeof *mant; m++) {
      for (uint64_t s = 0; s <= 1; s++) {
        uint64_t bits = (s << 63) | (e << 52) | mant[m];
        sweep1(bits);
        sweep2(prev, bits); // rolling fmod pairs across the battery
        prev = bits;
      }
    }
  }
  static const uint64_t specials[] = {
      UINT64_C(0x0000000000000000), UINT64_C(0x8000000000000000),
      UINT64_C(0x7FF0000000000000), UINT64_C(0xFFF0000000000000),
      UINT64_C(0x7FF8000000000000), UINT64_C(0x3FE0000000000000),
      UINT64_C(0xBFE0000000000000), UINT64_C(0x3FDFFFFFFFFFFFFF),
      UINT64_C(0x0010000000000000), UINT64_C(0x000FFFFFFFFFFFFF),
  };
  for (size_t i = 0; i < sizeof specials / sizeof *specials; i++) {
    sweep1(specials[i]);
    for (size_t j = 0; j < sizeof specials / sizeof *specials; j++)
      sweep2(specials[i], specials[j]);
  }

  // --- the fdlibm crosscheck input corpus -------------------------------
  FILE *f = fopen(argv[1], "r");
  if (!f) {
    fprintf(stderr, "mathsweep: cannot open %s\n", argv[1]);
    return 2;
  }
  char line[256];
  long n = 0, lineno = 0;
  while (fgets(line, sizeof line, f)) {
    lineno++;
    uint64_t a = 0, b = 0;
    // STRICT grammar (iter 39 review M3; tightened iter 40, round 2):
    // the corpus is machine-generated (gen-inputs.js), so anything but
    //   <op> SP <hex16> ["\n"]          op in {sin,cos,tan,atan}
    //   <op> SP <hex16> SP <hex16> ["\n"]  op in {atan2,pow}
    // — whitelisted op token, EXACTLY 16 lowercase hex chars per
    // operand, exact arity, single spaces, no trailing junk, no
    // overlong lines — is CORRUPTION, never something to skip silently.
    int arity = 0;
    const char *why = parse_line(line, &a, &b, &arity);
    if (why) {
      fprintf(stderr, "mathsweep: malformed input line %ld (%s): %s%s",
              lineno, why, line,
              line[0] && line[strlen(line) - 1] == '\n' ? "" : "\n");
      fclose(f);
      return 3;
    }
    sweep1(a);
    if (arity == 2) {
      sweep1(b);
      sweep2(a, b);
    }
    n++;
  }
  fclose(f);
  if (n == 0) {
    fprintf(stderr, "mathsweep: zero input lines accepted — empty corpus "
                    "(review M3: an empty sweep proves nothing)\n");
    return 3;
  }
  // Accepted-line trailer INSIDE the compared stream: the host<->device
  // cmp also judges count equality, and check-device-g01.sh asserts this
  // trailer equals the host corpus `wc -l` on both legs.
  printf("n %ld\n", n);
  fprintf(stderr, "mathsweep: %ld input lines swept\n", n);
  return 0;
}
