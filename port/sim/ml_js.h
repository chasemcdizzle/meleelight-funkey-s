// ml_js.h — JavaScript number semantics for the structure-parallel C port
// (M2-CAL). Every JS `number` is a C `double` — including indices, ECB
// point numbers, and angular parameters — so that canon-v1 bit-pattern
// serialization is trivially faithful. PLAN §2 kit: doubles only, vendored
// fdlibm for transcendentals, every TU compiled with -ffp-contract=off.
#ifndef ML_JS_H
#define ML_JS_H

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

// The canonical quiet NaN JS arithmetic produces (and ToNumber(undefined)
// yields): 0x7ff8000000000000. C arithmetic on non-NaN operands produces
// this same pattern on arm64/x86_64; helpers below return it explicitly so
// JS Math.* NaN results match bit-for-bit.
static inline double js_nan(void) {
  uint64_t bits = UINT64_C(0x7ff8000000000000);
  double d;
  memcpy(&d, &bits, 8);
  return d;
}

// Math.PI (0x400921FB54442D18).
static inline double js_pi(void) {
  uint64_t bits = UINT64_C(0x400921fb54442d18);
  double d;
  memcpy(&d, &bits, 8);
  return d;
}

// ECMAScript Math.max/Math.min (NOT C fmax/fmin!):
//   - any NaN operand -> NaN (fmax/fmin return the non-NaN operand)
//   - Math.max(+0,-0) = +0, Math.min(+0,-0) = -0
static inline double js_max(double a, double b) {
  if (isnan(a) || isnan(b)) return js_nan();
  if (a < b) return b;
  if (b < a) return a;
  return signbit(a) ? b : a; // equal (covers +0/-0): prefer +0
}

static inline double js_min(double a, double b) {
  if (isnan(a) || isnan(b)) return js_nan();
  if (a < b) return a;
  if (b < a) return b;
  return signbit(a) ? a : b; // equal (covers +0/-0): prefer -0
}

// ECMAScript Math.sign: NaN->NaN, +0->+0, -0->-0, else +/-1.
static inline double js_sign(double x) {
  if (isnan(x)) return js_nan();
  if (x > 0) return 1.0;
  if (x < 0) return -1.0;
  return x; // preserves +0 / -0
}

// Math.abs == fabs (clears the sign bit; NaN inputs here are canonical).
static inline double js_abs(double x) { return fabs(x); }

// ECMAScript Math.round (NOT C round()/rint()): ties round toward
// +Infinity and the sign of zero is preserved — Math.round(-0.5) = -0,
// Math.round(2.5) = 3, Math.round(0.49999999999999994) = 0 (naive
// floor(x+0.5) gets that last one wrong: x+0.5 rounds up to 1.0).
// Algorithm = V8's Float64Round: r = ceil(x); if (r - 0.5 > x) r -= 1.
// `r - 0.5` is exact for every integer-valued double |r| <= 2^52; for
// larger |x| ceil(x) == x and the guard stays false, so the identity is
// returned. C ceil is exact and preserves -0 for inputs in (-1, -0].
static inline double js_round(double x) {
  if (isnan(x)) return js_nan();
  double r = ceil(x);
  if (r - 0.5 > x) r -= 1.0;
  return r;
}

#endif // ML_JS_H
