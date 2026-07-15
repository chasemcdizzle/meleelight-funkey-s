// ml_fmt.c — ECMAScript Number::toString(x, 10) in C (M2 task 15).
//
// Layering:
//   1. The vendored Ryu core (port/ryu/ryu/d2s.c, byte-verbatim,
//      #included below as part of THIS translation unit) computes the
//      shortest correctly-rounded decimal digits + exponent for a finite
//      nonzero double: the unique (s, n) with the fewest digits s such
//      that s * 10^(n-k) round-trips to x, ties resolved closest-then-
//      even — exactly ECMA-262 §6.1.6.1.20 steps 5's (n, k, s).
//   2. This file applies ECMA-262 §6.1.6.1.20 ("Number::toString")
//      steps 6-10 VERBATIM to Ryu's (digits, exponent):
//        step 6: k <= n <= 21  -> digits + (n-k) zeros
//        step 7: 0 < n <= 21   -> digits with '.' after n digits
//        step 8: -6 < n <= 0   -> "0." + (-n) zeros + digits
//        step 9: k == 1        -> d "e" sign |n-1|
//        step 10:              -> d "." rest "e" sign |n-1|
//      (n here = Ryu's exponent + k, the ECMA 10^n scale of 0.digits.)
//   3. The small-integer fast path + trailing-zero fold mirrors Ryu's
//      own d2s_buffered_n (port/ryu/ryu/d2s.c:475-493): for integers in
//      [1, 2^53) the exact digits ARE the shortest once trailing decimal
//      zeros are moved into the exponent.
//
// Verified differentially against V8's String(x) over every double bit
// pattern in every capture file plus a multi-million adversarial corpus
// (port/sim/calib/check-format.sh — FORMAT MATCH).
//
// PLAN §2: doubles only; this TU (like every sim TU) is compiled with
// -ffp-contract=off. No floating-point arithmetic happens here at all —
// the double is only ever decoded bitwise.

#include "ml_fmt.h"

#include <stdint.h>
#include <string.h>

// The vendored Ryu core, byte-verbatim (provenance: NOTICES +
// port/ryu/PROVENANCE.sha256). Compiled into this TU so we can reach the
// internal d2d()/d2d_small_int()/decimalLength17() (all static inline).
#include "ryu/d2s.c"

int ml_fmt_dtoa(double x, char *buf) {
  const uint64_t bits = double_to_bits(x);
  const int sign = (int) (bits >> 63);
  const uint64_t ieeeMantissa = bits & ((1ull << DOUBLE_MANTISSA_BITS) - 1);
  const uint32_t ieeeExponent =
      (uint32_t) ((bits >> DOUBLE_MANTISSA_BITS) & ((1u << DOUBLE_EXPONENT_BITS) - 1u));

  // Specials (ECMA-262 6.1.6.1.20 steps 1-4). Note String(-0) === "0".
  if (ieeeExponent == ((1u << DOUBLE_EXPONENT_BITS) - 1u)) {
    if (ieeeMantissa != 0) { memcpy(buf, "NaN", 4); return 3; } // any payload
    if (sign) { memcpy(buf, "-Infinity", 10); return 9; }
    memcpy(buf, "Infinity", 9);
    return 8;
  }
  if (ieeeExponent == 0 && ieeeMantissa == 0) { memcpy(buf, "0", 2); return 1; }

  char *p = buf;
  if (sign) *p++ = '-';

  // Shortest digits + exponent, exactly as Ryu's d2s_buffered_n does it
  // (port/ryu/ryu/d2s.c:475-493): small-int fast path with the trailing
  // (decimal) zero fold, general Ryu otherwise.
  floating_decimal_64 v;
  if (d2d_small_int(ieeeMantissa, ieeeExponent, &v)) {
    for (;;) {
      const uint64_t q = div10(v.mantissa);
      const uint32_t r = ((uint32_t) v.mantissa) - 10 * ((uint32_t) q);
      if (r != 0) break;
      v.mantissa = q;
      ++v.exponent;
    }
  } else {
    v = d2d(ieeeMantissa, ieeeExponent);
  }

  // Decimal digits of the mantissa, most significant first.
  char dig[18];
  const int32_t k = (int32_t) decimalLength17(v.mantissa); // 1..17
  {
    uint64_t m = v.mantissa;
    for (int32_t i = k - 1; i >= 0; --i) { dig[i] = (char) ('0' + (m % 10)); m /= 10; }
  }
  const int32_t n = v.exponent + k; // x = 0.<digits> * 10^n (ECMA's n)

  if (k <= n && n <= 21) {
    // step 6: the digits followed by n-k zeros.
    memcpy(p, dig, (size_t) k);
    p += k;
    for (int32_t i = 0; i < n - k; ++i) *p++ = '0';
  } else if (0 < n && n <= 21) {
    // step 7: first n digits, '.', remaining k-n digits.
    memcpy(p, dig, (size_t) n);
    p += n;
    *p++ = '.';
    memcpy(p, dig + n, (size_t) (k - n));
    p += k - n;
  } else if (-6 < n && n <= 0) {
    // step 8: "0." + (-n) zeros + digits.
    *p++ = '0';
    *p++ = '.';
    for (int32_t i = 0; i < -n; ++i) *p++ = '0';
    memcpy(p, dig, (size_t) k);
    p += k;
  } else {
    // steps 9/10: exponent form d[.ddd]e(+|-)|n-1|.
    *p++ = dig[0];
    if (k > 1) {
      *p++ = '.';
      memcpy(p, dig + 1, (size_t) (k - 1));
      p += k - 1;
    }
    *p++ = 'e';
    int32_t e = n - 1;
    if (e < 0) { *p++ = '-'; e = -e; } else { *p++ = '+'; }
    // 1..3 decimal digits (|e| <= 324), no leading zeros.
    char ed[4];
    int32_t ei = 0;
    do { ed[ei++] = (char) ('0' + (e % 10)); e /= 10; } while (e != 0);
    while (ei > 0) *p++ = ed[--ei];
  }
  *p = '\0';
  return (int) (p - buf);
}
