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
  char fn[16];
  long n = 0;
  while (fgets(line, sizeof line, f)) {
    uint64_t a = 0, b = 0;
    int got = sscanf(line, "%15s %" SCNx64 " %" SCNx64, fn, &a, &b);
    if (got < 2) continue;
    sweep1(a);
    if (got == 3) {
      sweep1(b);
      sweep2(a, b);
    }
    n++;
  }
  fclose(f);
  fprintf(stderr, "mathsweep: %ld input lines swept\n", n);
  return 0;
}
