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

#endif // ML_JS_H
