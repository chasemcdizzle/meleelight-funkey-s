/*
 * port/fdlibm/fdlibm.c — vendored fdlibm implementations of
 * sin, cos, tan, atan, atan2, pow (the project's locked transcendental
 * surface, PLAN §2 / issue #10).
 *
 * VENDORED FROM: V8's fdlibm port, v8/src/base/ieee754.cc at tag 12.4.254
 *   https://raw.githubusercontent.com/v8/v8/12.4.254/src/base/ieee754.cc
 *   sha256 da01a54955911cfc550117988de91516c6f1aac343af28142dce619b433a67fc
 * itself adapted from fdlibm (http://www.netlib.org/fdlibm).
 * sin/cos below are V8's fdlibm_sin/fdlibm_cos bodies (pure fdlibm
 * variants). scalbn is from classic Sun fdlibm s_scalbn.c (netlib 5.3).
 *
 * Upstream notices, carried verbatim:
 *
 * ====================================================
 * Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.
 *
 * Developed at SunSoft, a Sun Microsystems, Inc. business.
 * Permission to use, copy, modify, and distribute this
 * software is freely granted, provided that this notice
 * is preserved.
 * ====================================================
 *
 * The original source code covered by the above license above has been
 * modified significantly by Google Inc.
 * Copyright 2016 the V8 project authors. All rights reserved.
 * (V8 is BSD-3-Clause; see https://github.com/v8/v8/blob/12.4.254/LICENSE)
 *
 * Mechanical transformations applied here (C++ → C99), and nothing else:
 *   - namespaces / V8_INLINE / V8_WARN_UNUSED_RESULT removed; functions
 *     renamed fd_* (public) / fd__* (internal) to avoid libm collisions.
 *   - base::bit_cast<uint64_t/double> → memcpy-based fd__d2u/fd__u2d.
 *   - static_cast<T>(x) → (T)(x); digit separators (0x0000'0000) removed.
 *   - base::Divide(a, b) → (a / b) (V8's helper is plain IEEE division).
 *   - {Negate,Sub}WithWraparound<int32_t> → explicit unsigned arithmetic
 *     cast back to int32_t (identical two's-complement wrap semantics).
 *   - std::numeric_limits<double>::signaling_NaN() → fd__qnan()
 *     (canonical quiet NaN 0x7FF8000000000000 — the JS side of the
 *     oracle can only round-trip quiet NaNs; both sides return this
 *     exact bit pattern so the crosscheck stays bit-exact).
 *   - *const_cast<volatile double*>(&atanlo[3]) → atanlo[3] (the
 *     volatile only defeated const-folding; the value is identical).
 *     static volatile double tiny/pi_lo/huge → static const double
 *     (same rationale; all arithmetic on them is correctly rounded
 *     whether folded at compile time or done at run time).
 *   - scalbn: host <math.h> scalbn replaced by fd__scalbn (classic
 *     fdlibm s_scalbn) so no libm behavior leaks in; fabs → fd__fabs
 *     (bit mask). sqrt stays <math.h> (IEEE-exact operation, and
 *     bit-verified against the host anchor by the M3 device mathsweep).
 *   - floor/ceil/fmod: vendored here as STRONG OVERRIDES of the libm
 *     symbols (classic Sun fdlibm s_floor.c / s_ceil.c / e_fmod.c
 *     algorithms restated over the 64-bit pattern — pure bit/integer
 *     manipulation). MEASURED (M3 task 1, iter 38): the FunKey SDK's
 *     static musl libc.a math objects were built with unsafe-FP
 *     optimization — every algebraic-identity code path is folded away:
 *     floor/ceil/round's ±2^52 "toint" trick collapses (on device
 *     floor(1.5) == 1.5, identity for every non-integer — this silently
 *     corrupted fd__rem_pio2's Payne-Hanek path and every sim
 *     Math.floor), fmod's `(x*y)/(x*y)` NaN arm folds to 1.0
 *     (fmod(0,0) == 1.0 on device) and its `0*x` signed-zero arm loses
 *     the sign of -0 results. sqrt/fabs resolve to VFP instructions and
 *     measured healthy. Any TU that links fdlibm.c (host + device sim,
 *     csweep, fmt tools, qjs-oracle) resolves floor/ceil/fmod to these
 *     exact implementations; equality with a known-good libm is proven
 *     differentially by port/sim/device/mathsweep.c vs the macOS-host
 *     libm anchor (NaN outputs canon-collapsed, fix_plan §M2 rule 9).
 *
 * MUST be compiled with -ffp-contract=off (no fused multiply-add).
 */

#include <math.h>    /* sqrt — IEEE-exact operation; floor/ceil overridden below */
#include <stdint.h>
#include <string.h>

#include "fdlibm.h"

static uint64_t fd__d2u(double d) {
  uint64_t u;
  memcpy(&u, &d, 8);
  return u;
}

static double fd__u2d(uint64_t u) {
  double d;
  memcpy(&d, &u, 8);
  return d;
}

/* Get two 32 bit ints from a double. */
#define EXTRACT_WORDS(ix0, ix1, d)      \
  do {                                  \
    uint64_t bits = fd__d2u(d);         \
    (ix0) = (uint32_t)(bits >> 32);     \
    (ix1) = (uint32_t)(bits & 0xFFFFFFFFu); \
  } while (0)

/* Get the more significant 32 bit int from a double. */
#define GET_HIGH_WORD(i, d)             \
  do {                                  \
    (i) = (uint32_t)(fd__d2u(d) >> 32); \
  } while (0)

/* Get the less significant 32 bit int from a double. */
#define GET_LOW_WORD(i, d)                        \
  do {                                            \
    (i) = (uint32_t)(fd__d2u(d) & 0xFFFFFFFFu);   \
  } while (0)

/* Set a double from two 32 bit ints. */
#define INSERT_WORDS(d, ix0, ix1)                                      \
  do {                                                                 \
    (d) = fd__u2d(((uint64_t)(uint32_t)(ix0) << 32) |                  \
                  (uint32_t)(ix1));                                    \
  } while (0)

/* Set the more significant 32 bits of a double from an int. */
#define SET_HIGH_WORD(d, v)                              \
  do {                                                   \
    uint64_t bits = fd__d2u(d);                          \
    bits &= 0x00000000FFFFFFFFULL;                       \
    bits |= (uint64_t)(uint32_t)(v) << 32;               \
    (d) = fd__u2d(bits);                                 \
  } while (0)

/* Set the less significant 32 bits of a double from an int. */
#define SET_LOW_WORD(d, v)                               \
  do {                                                   \
    uint64_t bits = fd__d2u(d);                          \
    bits &= 0xFFFFFFFF00000000ULL;                       \
    bits |= (uint32_t)(v);                               \
    (d) = fd__u2d(bits);                                 \
  } while (0)

static double fd__fabs(double x) {
  return fd__u2d(fd__d2u(x) & 0x7FFFFFFFFFFFFFFFULL);
}

static double fd__copysign(double x, double y) {
  return fd__u2d((fd__d2u(x) & 0x7FFFFFFFFFFFFFFFULL) |
                 (fd__d2u(y) & 0x8000000000000000ULL));
}

static double fd__qnan(void) { return fd__u2d(0x7FF8000000000000ULL); }

/* floor(x)/ceil(x) — classic Sun fdlibm s_floor.c / s_ceil.c (netlib
 * 5.3) restated over the single 64-bit pattern (the two-word original
 * splits the same masks across hi/lo words; the mantissa-increment for
 * the away-from-zero case is the identical add, 1 << (52 - e), carried
 * through the exponent field). Pure bit manipulation — exact for every
 * double under any rounding mode, no libm dependence. Deliberately NOT
 * kept: the original's `if (huge + x > 0.0)` guard, whose only effect
 * is raising FE_INEXACT (always true in the guarded range; nothing in
 * this project observes FP exception flags). STRONG OVERRIDES: see the
 * header note — the FunKey SDK's musl libc.a floor/ceil are broken
 * (identity for non-integers, measured on device iter 38). */
double floor(double x) {
  uint64_t u = fd__d2u(x);
  int e = (int)((u >> 52) & 0x7FF) - 1023; /* unbiased exponent */
  if (e >= 52) {
    if (e == 1024) return x + x; /* inf or NaN */
    return x;                    /* already integral */
  }
  if (e < 0) { /* |x| < 1 */
    if ((u & 0x7FFFFFFFFFFFFFFFULL) == 0) return x; /* +-0 preserved */
    return (u >> 63) ? -1.0 : 0.0;
  }
  uint64_t frac = 0x000FFFFFFFFFFFFFULL >> e; /* fractional mantissa bits */
  if ((u & frac) == 0) return x;              /* already integral */
  if (u >> 63) u += 0x0010000000000000ULL >> e; /* negative: away from zero */
  u &= ~frac;
  return fd__u2d(u);
}

double ceil(double x) {
  uint64_t u = fd__d2u(x);
  int e = (int)((u >> 52) & 0x7FF) - 1023; /* unbiased exponent */
  if (e >= 52) {
    if (e == 1024) return x + x; /* inf or NaN */
    return x;                    /* already integral */
  }
  if (e < 0) { /* |x| < 1 */
    if ((u & 0x7FFFFFFFFFFFFFFFULL) == 0) return x; /* +-0 preserved */
    return (u >> 63) ? -0.0 : 1.0; /* C99: ceil of (-1,0) is -0 */
  }
  uint64_t frac = 0x000FFFFFFFFFFFFFULL >> e; /* fractional mantissa bits */
  if ((u & frac) == 0) return x;              /* already integral */
  if (!(u >> 63)) u += 0x0010000000000000ULL >> e; /* positive: away from zero */
  u &= ~frac;
  return fd__u2d(u);
}

/* fmod(x,y) — classic Sun fdlibm e_fmod.c fixed-point algorithm
 * restated over the 64-bit pattern (the original's hi/lo-word shift
 * ladder is the same 53-bit-significand long division). fmod is EXACT:
 * pure integer manipulation end to end. Deliberately NOT kept from the
 * original: the `(x*y)/(x*y)` invalid-operand arm (returns the
 * canonical quiet NaN directly — NaN payloads are canon-collapsed
 * everywhere in this project, fix_plan §M2 rule 9) and the Zero[] table
 * (signed zero built by bit ops). STRONG OVERRIDE: see header note. */
double fmod(double x, double y) {
  uint64_t ux = fd__d2u(x), uy = fd__d2u(y);
  uint64_t sx = ux & 0x8000000000000000ULL;      /* sign of x */
  uint64_t ax = ux & 0x7FFFFFFFFFFFFFFFULL;      /* |x| pattern */
  uint64_t ay = uy & 0x7FFFFFFFFFFFFFFFULL;      /* |y| pattern */

  /* purge off exception values: y = 0, x inf/NaN, y NaN */
  if (ay == 0 || ax >= 0x7FF0000000000000ULL || ay > 0x7FF0000000000000ULL)
    return fd__qnan();
  if (ax < ay) return x;            /* |x| < |y|: x is the remainder */
  if (ax == ay) return fd__u2d(sx); /* |x| == |y|: signed zero */

  /* ilogb + 53-bit significand aligned at bit 52 (subnormals normalized) */
  int ix, iy;
  uint64_t mx, my;
  if (ax >= 0x0010000000000000ULL) {
    ix = (int)(ax >> 52) - 1023;
    mx = (ax & 0x000FFFFFFFFFFFFFULL) | 0x0010000000000000ULL;
  } else {
    ix = -1022;
    for (mx = ax; mx < 0x0010000000000000ULL; mx <<= 1) ix--;
  }
  if (ay >= 0x0010000000000000ULL) {
    iy = (int)(ay >> 52) - 1023;
    my = (ay & 0x000FFFFFFFFFFFFFULL) | 0x0010000000000000ULL;
  } else {
    iy = -1022;
    for (my = ay; my < 0x0010000000000000ULL; my <<= 1) iy--;
  }

  /* fix point fmod (e_fmod's shift-subtract ladder) */
  for (int n = ix - iy; n; n--) {
    if (mx >= my) {
      mx -= my;
      if (mx == 0) return fd__u2d(sx); /* exact multiple: signed zero */
    }
    mx <<= 1;
  }
  if (mx >= my) {
    mx -= my;
    if (mx == 0) return fd__u2d(sx);
  }

  /* normalize the (exact) remainder up to bit 52 */
  while (mx < 0x0010000000000000ULL) {
    mx <<= 1;
    iy--;
  }

  /* convert back to floating value and restore the sign */
  if (iy >= -1022) { /* normal result */
    return fd__u2d(sx | ((uint64_t)(iy + 1023) << 52) |
                   (mx & 0x000FFFFFFFFFFFFFULL));
  }
  /* subnormal result: fmod is exact, the shifted-out bits are zero */
  return fd__u2d(sx | (mx >> (-1022 - iy)));
}

/* scalbn(x,n) — classic Sun fdlibm s_scalbn.c: x * 2**n computed by
 * exponent manipulation. */
static double fd__scalbn(double x, int n) {
  static const double
      two54 = 1.80143985094819840000e+16,  /* 0x43500000, 0x00000000 */
      twom54 = 5.55111512312578270212e-17, /* 0x3C900000, 0x00000000 */
      huge_ = 1.0e+300, tiny = 1.0e-300;
  int32_t k, hx, lx;
  EXTRACT_WORDS(hx, lx, x);
  k = (hx & 0x7FF00000) >> 20; /* extract exponent */
  if (k == 0) {                /* 0 or subnormal x */
    if ((lx | (hx & 0x7FFFFFFF)) == 0) return x; /* +-0 */
    x *= two54;
    GET_HIGH_WORD(hx, x);
    k = ((hx & 0x7FF00000) >> 20) - 54;
    if (n < -50000) return tiny * x; /* underflow */
  }
  if (k == 0x7FF) return x + x; /* NaN or Inf */
  k = k + n;
  if (k > 0x7FE) return huge_ * fd__copysign(huge_, x); /* overflow */
  if (k > 0) { /* normal result */
    SET_HIGH_WORD(x, (hx & 0x800FFFFF) | (k << 20));
    return x;
  }
  if (k <= -54) {
    if (n > 50000) { /* in case integer overflow in n+k */
      return huge_ * fd__copysign(huge_, x); /* overflow */
    } else {
      return tiny * fd__copysign(tiny, x); /* underflow */
    }
  }
  k += 54; /* subnormal result */
  SET_HIGH_WORD(x, (hx & 0x800FFFFF) | (k << 20));
  return x * twom54;
}

static int fd__kernel_rem_pio2(double *x, double *y, int e0, int nx, int prec,
                               const int32_t *ipio2);

/* __ieee754_rem_pio2(x,y)
 *
 * return the remainder of x rem pi/2 in y[0]+y[1]
 * use __kernel_rem_pio2()
 */
static int32_t fd__rem_pio2(double x, double *y) {
  /*
   * Table of constants for 2/pi, 396 Hex digits (476 decimal) of 2/pi
   */
  static const int32_t two_over_pi[] = {
      0xA2F983, 0x6E4E44, 0x1529FC, 0x2757D1, 0xF534DD, 0xC0DB62, 0x95993C,
      0x439041, 0xFE5163, 0xABDEBB, 0xC561B7, 0x246E3A, 0x424DD2, 0xE00649,
      0x2EEA09, 0xD1921C, 0xFE1DEB, 0x1CB129, 0xA73EE8, 0x8235F5, 0x2EBB44,
      0x84E99C, 0x7026B4, 0x5F7E41, 0x3991D6, 0x398353, 0x39F49C, 0x845F8B,
      0xBDF928, 0x3B1FF8, 0x97FFDE, 0x05980F, 0xEF2F11, 0x8B5A0A, 0x6D1F6D,
      0x367ECF, 0x27CB09, 0xB74F46, 0x3F669E, 0x5FEA2D, 0x7527BA, 0xC7EBE5,
      0xF17B3D, 0x0739F7, 0x8A5292, 0xEA6BFB, 0x5FB11F, 0x8D5D08, 0x560330,
      0x46FC7B, 0x6BABF0, 0xCFBC20, 0x9AF436, 0x1DA9E3, 0x91615E, 0xE61B08,
      0x659985, 0x5F14A0, 0x68408D, 0xFFD880, 0x4D7327, 0x310606, 0x1556CA,
      0x73A8C9, 0x60E27B, 0xC08C6B,
  };

  static const int32_t npio2_hw[] = {
      0x3FF921FB, 0x400921FB, 0x4012D97C, 0x401921FB, 0x401F6A7A, 0x4022D97C,
      0x4025FDBB, 0x402921FB, 0x402C463A, 0x402F6A7A, 0x4031475C, 0x4032D97C,
      0x40346B9C, 0x4035FDBB, 0x40378FDB, 0x403921FB, 0x403AB41B, 0x403C463A,
      0x403DD85A, 0x403F6A7A, 0x40407E4C, 0x4041475C, 0x4042106C, 0x4042D97C,
      0x4043A28C, 0x40446B9C, 0x404534AC, 0x4045FDBB, 0x4046C6CB, 0x40478FDB,
      0x404858EB, 0x404921FB,
  };

  /*
   * invpio2:  53 bits of 2/pi
   * pio2_1:   first  33 bit of pi/2
   * pio2_1t:  pi/2 - pio2_1
   * pio2_2:   second 33 bit of pi/2
   * pio2_2t:  pi/2 - (pio2_1+pio2_2)
   * pio2_3:   third  33 bit of pi/2
   * pio2_3t:  pi/2 - (pio2_1+pio2_2+pio2_3)
   */

  static const double
      zero = 0.00000000000000000000e+00,    /* 0x00000000, 0x00000000 */
      half = 5.00000000000000000000e-01,    /* 0x3FE00000, 0x00000000 */
      two24 = 1.67772160000000000000e+07,   /* 0x41700000, 0x00000000 */
      invpio2 = 6.36619772367581382433e-01, /* 0x3FE45F30, 0x6DC9C883 */
      pio2_1 = 1.57079632673412561417e+00,  /* 0x3FF921FB, 0x54400000 */
      pio2_1t = 6.07710050650619224932e-11, /* 0x3DD0B461, 0x1A626331 */
      pio2_2 = 6.07710050630396597660e-11,  /* 0x3DD0B461, 0x1A600000 */
      pio2_2t = 2.02226624879595063154e-21, /* 0x3BA3198A, 0x2E037073 */
      pio2_3 = 2.02226624871116645580e-21,  /* 0x3BA3198A, 0x2E000000 */
      pio2_3t = 8.47842766036889956997e-32; /* 0x397B839A, 0x252049C1 */

  double z, w, t, r, fn;
  double tx[3];
  int32_t e0, i, j, nx, n, ix, hx;
  uint32_t low;

  z = 0;
  GET_HIGH_WORD(hx, x); /* high word of x */
  ix = hx & 0x7FFFFFFF;
  if (ix <= 0x3FE921FB) { /* |x| ~<= pi/4 , no need for reduction */
    y[0] = x;
    y[1] = 0;
    return 0;
  }
  if (ix < 0x4002D97C) { /* |x| < 3pi/4, special case with n=+-1 */
    if (hx > 0) {
      z = x - pio2_1;
      if (ix != 0x3FF921FB) { /* 33+53 bit pi is good enough */
        y[0] = z - pio2_1t;
        y[1] = (z - y[0]) - pio2_1t;
      } else { /* near pi/2, use 33+33+53 bit pi */
        z -= pio2_2;
        y[0] = z - pio2_2t;
        y[1] = (z - y[0]) - pio2_2t;
      }
      return 1;
    } else { /* negative x */
      z = x + pio2_1;
      if (ix != 0x3FF921FB) { /* 33+53 bit pi is good enough */
        y[0] = z + pio2_1t;
        y[1] = (z - y[0]) + pio2_1t;
      } else { /* near pi/2, use 33+33+53 bit pi */
        z += pio2_2;
        y[0] = z + pio2_2t;
        y[1] = (z - y[0]) + pio2_2t;
      }
      return -1;
    }
  }
  if (ix <= 0x413921FB) { /* |x| ~<= 2^19*(pi/2), medium size */
    t = fd__fabs(x);
    n = (int32_t)(t * invpio2 + half);
    fn = (double)n;
    r = t - fn * pio2_1;
    w = fn * pio2_1t; /* 1st round good to 85 bit */
    if (n < 32 && ix != npio2_hw[n - 1]) {
      y[0] = r - w; /* quick check no cancellation */
    } else {
      uint32_t high;
      j = ix >> 20;
      y[0] = r - w;
      GET_HIGH_WORD(high, y[0]);
      i = j - ((high >> 20) & 0x7FF);
      if (i > 16) { /* 2nd iteration needed, good to 118 */
        t = r;
        w = fn * pio2_2;
        r = t - w;
        w = fn * pio2_2t - ((t - r) - w);
        y[0] = r - w;
        GET_HIGH_WORD(high, y[0]);
        i = j - ((high >> 20) & 0x7FF);
        if (i > 49) { /* 3rd iteration need, 151 bits acc */
          t = r;      /* will cover all possible cases */
          w = fn * pio2_3;
          r = t - w;
          w = fn * pio2_3t - ((t - r) - w);
          y[0] = r - w;
        }
      }
    }
    y[1] = (r - y[0]) - w;
    if (hx < 0) {
      y[0] = -y[0];
      y[1] = -y[1];
      return -n;
    } else {
      return n;
    }
  }
  /*
   * all other (large) arguments
   */
  if (ix >= 0x7FF00000) { /* x is inf or NaN */
    y[0] = y[1] = x - x;
    return 0;
  }
  /* set z = scalbn(|x|,ilogb(x)-23) */
  GET_LOW_WORD(low, x);
  SET_LOW_WORD(z, low);
  e0 = (ix >> 20) - 1046; /* e0 = ilogb(z)-23; */
  SET_HIGH_WORD(z, ix - (int32_t)((uint32_t)e0 << 20));
  for (i = 0; i < 2; i++) {
    tx[i] = (double)((int32_t)z);
    z = (z - tx[i]) * two24;
  }
  tx[2] = z;
  nx = 3;
  while (tx[nx - 1] == zero) nx--; /* skip zero term */
  n = fd__kernel_rem_pio2(tx, y, e0, nx, 2, two_over_pi);
  if (hx < 0) {
    y[0] = -y[0];
    y[1] = -y[1];
    return -n;
  }
  return n;
}

/* __kernel_cos( x,  y )
 * kernel cos function on [-pi/4, pi/4], pi/4 ~ 0.785398164
 * Input x is assumed to be bounded by ~pi/4 in magnitude.
 * Input y is the tail of x.
 */
static double fd__kernel_cos(double x, double y) {
  static const double
      one = 1.00000000000000000000e+00, /* 0x3FF00000, 0x00000000 */
      C1 = 4.16666666666666019037e-02,  /* 0x3FA55555, 0x5555554C */
      C2 = -1.38888888888741095749e-03, /* 0xBF56C16C, 0x16C15177 */
      C3 = 2.48015872894767294178e-05,  /* 0x3EFA01A0, 0x19CB1590 */
      C4 = -2.75573143513906633035e-07, /* 0xBE927E4F, 0x809C52AD */
      C5 = 2.08757232129817482790e-09,  /* 0x3E21EE9E, 0xBDB4B1C4 */
      C6 = -1.13596475577881948265e-11; /* 0xBDA8FAE9, 0xBE8838D4 */

  double a, iz, z, r, qx;
  int32_t ix;
  GET_HIGH_WORD(ix, x);
  ix &= 0x7FFFFFFF;                     /* ix = |x|'s high word*/
  if (ix < 0x3E400000) {                /* if x < 2**27 */
    if ((int)x == 0) return one;        /* generate inexact */
  }
  z = x * x;
  r = z * (C1 + z * (C2 + z * (C3 + z * (C4 + z * (C5 + z * C6)))));
  if (ix < 0x3FD33333) { /* if |x| < 0.3 */
    return one - (0.5 * z - (z * r - x * y));
  } else {
    if (ix > 0x3FE90000) { /* x > 0.78125 */
      qx = 0.28125;
    } else {
      INSERT_WORDS(qx, ix - 0x00200000, 0); /* x/4 */
    }
    iz = 0.5 * z - qx;
    a = one - qx;
    return a - (iz - (z * r - x * y));
  }
}

/* __kernel_rem_pio2(x,y,e0,nx,prec,ipio2)
 * double x[],y[]; int e0,nx,prec; int ipio2[];
 *
 * __kernel_rem_pio2 return the last three digits of N with
 *              y = x - N*pi/2
 * so that |y| < pi/2.
 */
static int fd__kernel_rem_pio2(double *x, double *y, int e0, int nx, int prec,
                               const int32_t *ipio2) {
  static const int init_jk[] = {2, 3, 4, 6}; /* initial value for jk */

  static const double PIo2[] = {
      1.57079625129699707031e+00, /* 0x3FF921FB, 0x40000000 */
      7.54978941586159635335e-08, /* 0x3E74442D, 0x00000000 */
      5.39030252995776476554e-15, /* 0x3CF84698, 0x80000000 */
      3.28200341580791294123e-22, /* 0x3B78CC51, 0x60000000 */
      1.27065575308067607349e-29, /* 0x39F01B83, 0x80000000 */
      1.22933308981111328932e-36, /* 0x387A2520, 0x40000000 */
      2.73370053816464559624e-44, /* 0x36E38222, 0x80000000 */
      2.16741683877804819444e-51, /* 0x3569F31D, 0x00000000 */
  };

  static const double
      zero = 0.0,
      one = 1.0,
      two24 = 1.67772160000000000000e+07,  /* 0x41700000, 0x00000000 */
      twon24 = 5.96046447753906250000e-08; /* 0x3E700000, 0x00000000 */

  int32_t jz, jx, jv, jp, jk, carry, n, iq[20], i, j, k, m, q0, ih;
  double z, fw, f[20], fq[20], q[20];

  /* initialize jk*/
  jk = init_jk[prec];
  jp = jk;

  /* determine jx,jv,q0, note that 3>q0 */
  jx = nx - 1;
  jv = (e0 - 3) / 24;
  if (jv < 0) jv = 0;
  q0 = e0 - 24 * (jv + 1);

  /* set up f[0] to f[jx+jk] where f[jx+jk] = ipio2[jv+jk] */
  j = jv - jx;
  m = jx + jk;
  for (i = 0; i <= m; i++, j++) {
    f[i] = (j < 0) ? zero : (double)ipio2[j];
  }

  /* compute q[0],q[1],...q[jk] */
  for (i = 0; i <= jk; i++) {
    for (j = 0, fw = 0.0; j <= jx; j++) fw += x[j] * f[jx + i - j];
    q[i] = fw;
  }

  jz = jk;
recompute:
  /* distill q[] into iq[] reversingly */
  for (i = 0, j = jz, z = q[jz]; j > 0; i++, j--) {
    fw = (double)((int32_t)(twon24 * z));
    iq[i] = (int32_t)(z - two24 * fw);
    z = q[j - 1] + fw;
  }

  /* compute n */
  z = fd__scalbn(z, q0);       /* actual value of z */
  z -= 8.0 * floor(z * 0.125); /* trim off integer >= 8 */
  n = (int32_t)z;
  z -= (double)n;
  ih = 0;
  if (q0 > 0) { /* need iq[jz-1] to determine n */
    i = (iq[jz - 1] >> (24 - q0));
    n += i;
    iq[jz - 1] -= i << (24 - q0);
    ih = iq[jz - 1] >> (23 - q0);
  } else if (q0 == 0) {
    ih = iq[jz - 1] >> 23;
  } else if (z >= 0.5) {
    ih = 2;
  }

  if (ih > 0) { /* q > 0.5 */
    n += 1;
    carry = 0;
    for (i = 0; i < jz; i++) { /* compute 1-q */
      j = iq[i];
      if (carry == 0) {
        if (j != 0) {
          carry = 1;
          iq[i] = 0x1000000 - j;
        }
      } else {
        iq[i] = 0xFFFFFF - j;
      }
    }
    if (q0 > 0) { /* rare case: chance is 1 in 12 */
      switch (q0) {
        case 1:
          iq[jz - 1] &= 0x7FFFFF;
          break;
        case 2:
          iq[jz - 1] &= 0x3FFFFF;
          break;
      }
    }
    if (ih == 2) {
      z = one - z;
      if (carry != 0) z -= fd__scalbn(one, q0);
    }
  }

  /* check if recomputation is needed */
  if (z == zero) {
    j = 0;
    for (i = jz - 1; i >= jk; i--) j |= iq[i];
    if (j == 0) { /* need recomputation */
      for (k = 1; jk >= k && iq[jk - k] == 0; k++) {
        /* k = no. of terms needed */
      }

      for (i = jz + 1; i <= jz + k; i++) { /* add q[jz+1] to q[jz+k] */
        f[jx + i] = ipio2[jv + i];
        for (j = 0, fw = 0.0; j <= jx; j++) fw += x[j] * f[jx + i - j];
        q[i] = fw;
      }
      jz += k;
      goto recompute;
    }
  }

  /* chop off zero terms */
  if (z == 0.0) {
    jz -= 1;
    q0 -= 24;
    while (iq[jz] == 0) {
      jz--;
      q0 -= 24;
    }
  } else { /* break z into 24-bit if necessary */
    z = fd__scalbn(z, -q0);
    if (z >= two24) {
      fw = (double)((int32_t)(twon24 * z));
      iq[jz] = (int32_t)(z - two24 * fw);
      jz += 1;
      q0 += 24;
      iq[jz] = (int32_t)fw;
    } else {
      iq[jz] = (int32_t)z;
    }
  }

  /* convert integer "bit" chunk to floating-point value */
  fw = fd__scalbn(one, q0);
  for (i = jz; i >= 0; i--) {
    q[i] = fw * iq[i];
    fw *= twon24;
  }

  /* compute PIo2[0,...,jp]*q[jz,...,0] */
  for (i = jz; i >= 0; i--) {
    for (fw = 0.0, k = 0; k <= jp && k <= jz - i; k++) fw += PIo2[k] * q[i + k];
    fq[jz - i] = fw;
  }

  /* compress fq[] into y[] */
  switch (prec) {
    case 0:
      fw = 0.0;
      for (i = jz; i >= 0; i--) fw += fq[i];
      y[0] = (ih == 0) ? fw : -fw;
      break;
    case 1:
    case 2:
      fw = 0.0;
      for (i = jz; i >= 0; i--) fw += fq[i];
      y[0] = (ih == 0) ? fw : -fw;
      fw = fq[0] - fw;
      for (i = 1; i <= jz; i++) fw += fq[i];
      y[1] = (ih == 0) ? fw : -fw;
      break;
    case 3: /* painful */
      for (i = jz; i > 0; i--) {
        fw = fq[i - 1] + fq[i];
        fq[i] += fq[i - 1] - fw;
        fq[i - 1] = fw;
      }
      for (i = jz; i > 1; i--) {
        fw = fq[i - 1] + fq[i];
        fq[i] += fq[i - 1] - fw;
        fq[i - 1] = fw;
      }
      for (fw = 0.0, i = jz; i >= 2; i--) fw += fq[i];
      if (ih == 0) {
        y[0] = fq[0];
        y[1] = fq[1];
        y[2] = fw;
      } else {
        y[0] = -fq[0];
        y[1] = -fq[1];
        y[2] = -fw;
      }
  }
  return n & 7;
}

/* __kernel_sin( x, y, iy)
 * kernel sin function on [-pi/4, pi/4], pi/4 ~ 0.7854
 * Input x is assumed to be bounded by ~pi/4 in magnitude.
 * Input y is the tail of x.
 * Input iy indicates whether y is 0. (if iy=0, y assume to be 0).
 */
static double fd__kernel_sin(double x, double y, int iy) {
  static const double
      half = 5.00000000000000000000e-01, /* 0x3FE00000, 0x00000000 */
      S1 = -1.66666666666666324348e-01,  /* 0xBFC55555, 0x55555549 */
      S2 = 8.33333333332248946124e-03,   /* 0x3F811111, 0x1110F8A6 */
      S3 = -1.98412698298579493134e-04,  /* 0xBF2A01A0, 0x19C161D5 */
      S4 = 2.75573137070700676789e-06,   /* 0x3EC71DE3, 0x57B1FE7D */
      S5 = -2.50507602534068634195e-08,  /* 0xBE5AE5E6, 0x8A2B9CEB */
      S6 = 1.58969099521155010221e-10;   /* 0x3DE5D93A, 0x5ACFD57C */

  double z, r, v;
  int32_t ix;
  GET_HIGH_WORD(ix, x);
  ix &= 0x7FFFFFFF;      /* high word of x */
  if (ix < 0x3E400000) { /* |x| < 2**-27 */
    if ((int)x == 0) return x;
  } /* generate inexact */
  z = x * x;
  v = z * x;
  r = S2 + z * (S3 + z * (S4 + z * (S5 + z * S6)));
  if (iy == 0) {
    return x + v * (S1 + z * r);
  } else {
    return x - ((z * (half * y - v * r) - y) - v * S1);
  }
}

/* __kernel_tan( x, y, k )
 * kernel tan function on [-pi/4, pi/4], pi/4 ~ 0.7854
 * Input x is assumed to be bounded by ~pi/4 in magnitude.
 * Input y is the tail of x.
 * Input k indicates whether tan (if k=1) or
 * -1/tan (if k= -1) is returned.
 */
static double fd__kernel_tan(double x, double y, int iy) {
  static const double xxx[] = {
      3.33333333333334091986e-01,             /* 0x3FD55555, 0x55555563 */
      1.33333333333201242699e-01,             /* 0x3FC11111, 0x1110FE7A */
      5.39682539762260521377e-02,             /* 0x3FABA1BA, 0x1BB341FE */
      2.18694882948595424599e-02,             /* 0x3F9664F4, 0x8406D637 */
      8.86323982359930005737e-03,             /* 0x3F8226E3, 0xE96E8493 */
      3.59207910759131235356e-03,             /* 0x3F6D6D22, 0xC9560328 */
      1.45620945432529025516e-03,             /* 0x3F57DBC8, 0xFEE08315 */
      5.88041240820264096874e-04,             /* 0x3F4344D8, 0xF2F26501 */
      2.46463134818469906812e-04,             /* 0x3F3026F7, 0x1A8D1068 */
      7.81794442939557092300e-05,             /* 0x3F147E88, 0xA03792A6 */
      7.14072491382608190305e-05,             /* 0x3F12B80F, 0x32F0A7E9 */
      -1.85586374855275456654e-05,            /* 0xBEF375CB, 0xDB605373 */
      2.59073051863633712884e-05,             /* 0x3EFB2A70, 0x74BF7AD4 */
      /* one */ 1.00000000000000000000e+00,   /* 0x3FF00000, 0x00000000 */
      /* pio4 */ 7.85398163397448278999e-01,  /* 0x3FE921FB, 0x54442D18 */
      /* pio4lo */ 3.06161699786838301793e-17 /* 0x3C81A626, 0x33145C07 */
  };
#define one xxx[13]
#define pio4 xxx[14]
#define pio4lo xxx[15]
#define T xxx

  double z, r, v, w, s;
  int32_t ix, hx;

  GET_HIGH_WORD(hx, x);  /* high word of x */
  ix = hx & 0x7FFFFFFF;  /* high word of |x| */
  if (ix < 0x3E300000) { /* x < 2**-28 */
    if ((int)x == 0) {   /* generate inexact */
      uint32_t low;
      GET_LOW_WORD(low, x);
      if (((ix | low) | (iy + 1)) == 0) {
        return one / fd__fabs(x);
      } else {
        if (iy == 1) {
          return x;
        } else { /* compute -1 / (x+y) carefully */
          double a, t;

          z = w = x + y;
          SET_LOW_WORD(z, 0);
          v = y - (z - x);
          t = a = -one / w;
          SET_LOW_WORD(t, 0);
          s = one + t * z;
          return t + a * (s + t * v);
        }
      }
    }
  }
  if (ix >= 0x3FE59428) { /* |x| >= 0.6744 */
    if (hx < 0) {
      x = -x;
      y = -y;
    }
    z = pio4 - x;
    w = pio4lo - y;
    x = z + w;
    y = 0.0;
  }
  z = x * x;
  w = z * z;
  /*
   * Break x^5*(T[1]+x^2*T[2]+...) into
   * x^5(T[1]+x^4*T[3]+...+x^20*T[11]) +
   * x^5(x^2*(T[2]+x^4*T[4]+...+x^22*[T12]))
   */
  r = T[1] + w * (T[3] + w * (T[5] + w * (T[7] + w * (T[9] + w * T[11]))));
  v = z *
      (T[2] + w * (T[4] + w * (T[6] + w * (T[8] + w * (T[10] + w * T[12])))));
  s = z * x;
  r = y + z * (s * (r + v) + y);
  r += T[0] * s;
  w = x + r;
  if (ix >= 0x3FE59428) {
    v = iy;
    return (1 - ((hx >> 30) & 2)) * (v - 2.0 * (x - (w * w / (w + v) - r)));
  }
  if (iy == 1) {
    return w;
  } else {
    /*
     * if allow error up to 2 ulp, simply return
     * -1.0 / (x+r) here
     */
    /* compute -1.0 / (x+r) accurately */
    double a, t;
    z = w;
    SET_LOW_WORD(z, 0);
    v = r - (z - x);  /* z+v = r+x */
    t = a = -1.0 / w; /* a = -1.0/w */
    SET_LOW_WORD(t, 0);
    s = 1.0 + t * z;
    return t + a * (s + t * v);
  }

#undef one
#undef pio4
#undef pio4lo
#undef T
}

/* atan(x)
 * Method
 *   1. Reduce x to positive by atan(x) = -atan(-x).
 *   2. According to the integer k=4t+0.25 chopped, t=x, the argument
 *      is further reduced to one of the following intervals and the
 *      arctangent of t is evaluated by the corresponding formula:
 *
 *      [0,7/16]      atan(x) = t-t^3*(a1+t^2*(a2+...(a10+t^2*a11)...)
 *      [7/16,11/16]  atan(x) = atan(1/2) + atan( (t-0.5)/(1+t/2) )
 *      [11/16.19/16] atan(x) = atan( 1 ) + atan( (t-1)/(1+t) )
 *      [19/16,39/16] atan(x) = atan(3/2) + atan( (t-1.5)/(1+1.5t) )
 *      [39/16,INF]   atan(x) = atan(INF) + atan( -1/t )
 */
double fd_atan(double x) {
  static const double atanhi[] = {
      4.63647609000806093515e-01, /* atan(0.5)hi 0x3FDDAC67, 0x0561BB4F */
      7.85398163397448278999e-01, /* atan(1.0)hi 0x3FE921FB, 0x54442D18 */
      9.82793723247329054082e-01, /* atan(1.5)hi 0x3FEF730B, 0xD281F69B */
      1.57079632679489655800e+00, /* atan(inf)hi 0x3FF921FB, 0x54442D18 */
  };

  static const double atanlo[] = {
      2.26987774529616870924e-17, /* atan(0.5)lo 0x3C7A2B7F, 0x222F65E2 */
      3.06161699786838301793e-17, /* atan(1.0)lo 0x3C81A626, 0x33145C07 */
      1.39033110312309984516e-17, /* atan(1.5)lo 0x3C700788, 0x7AF0CBBD */
      6.12323399573676603587e-17, /* atan(inf)lo 0x3C91A626, 0x33145C07 */
  };

  static const double aT[] = {
      3.33333333333329318027e-01,  /* 0x3FD55555, 0x5555550D */
      -1.99999999998764832476e-01, /* 0xBFC99999, 0x9998EBC4 */
      1.42857142725034663711e-01,  /* 0x3FC24924, 0x920083FF */
      -1.11111104054623557880e-01, /* 0xBFBC71C6, 0xFE231671 */
      9.09088713343650656196e-02,  /* 0x3FB745CD, 0xC54C206E */
      -7.69187620504482999495e-02, /* 0xBFB3B0F2, 0xAF749A6D */
      6.66107313738753120669e-02,  /* 0x3FB10D66, 0xA0D03D51 */
      -5.83357013379057348645e-02, /* 0xBFADDE2D, 0x52DEFD9A */
      4.97687799461593236017e-02,  /* 0x3FA97B4B, 0x24760DEB */
      -3.65315727442169155270e-02, /* 0xBFA2B444, 0x2C6A6C2F */
      1.62858201153657823623e-02,  /* 0x3F90AD3A, 0xE322DA11 */
  };

  static const double one = 1.0, huge_ = 1.0e300;

  double w, s1, s2, z;
  int32_t ix, hx, id;

  GET_HIGH_WORD(hx, x);
  ix = hx & 0x7FFFFFFF;
  if (ix >= 0x44100000) { /* if |x| >= 2^66 */
    uint32_t low;
    GET_LOW_WORD(low, x);
    if (ix > 0x7FF00000 || (ix == 0x7FF00000 && (low != 0)))
      return x + x; /* NaN */
    if (hx > 0)
      return atanhi[3] + atanlo[3];
    else
      return -atanhi[3] - atanlo[3];
  }
  if (ix < 0x3FDC0000) {             /* |x| < 0.4375 */
    if (ix < 0x3E400000) {           /* |x| < 2^-27 */
      if (huge_ + x > one) return x; /* raise inexact */
    }
    id = -1;
  } else {
    x = fd__fabs(x);
    if (ix < 0x3FF30000) {   /* |x| < 1.1875 */
      if (ix < 0x3FE60000) { /* 7/16 <=|x|<11/16 */
        id = 0;
        x = (2.0 * x - one) / (2.0 + x);
      } else { /* 11/16<=|x|< 19/16 */
        id = 1;
        x = (x - one) / (x + one);
      }
    } else {
      if (ix < 0x40038000) { /* |x| < 2.4375 */
        id = 2;
        x = (x - 1.5) / (one + 1.5 * x);
      } else { /* 2.4375 <= |x| < 2^66 */
        id = 3;
        x = -1.0 / x;
      }
    }
  }
  /* end of argument reduction */
  z = x * x;
  w = z * z;
  /* break sum from i=0 to 10 aT[i]z**(i+1) into odd and even poly */
  s1 = z * (aT[0] +
            w * (aT[2] + w * (aT[4] + w * (aT[6] + w * (aT[8] + w * aT[10])))));
  s2 = w * (aT[1] + w * (aT[3] + w * (aT[5] + w * (aT[7] + w * aT[9]))));
  if (id < 0) {
    return x - x * (s1 + s2);
  } else {
    z = atanhi[id] - ((x * (s1 + s2) - atanlo[id]) - x);
    return (hx < 0) ? -z : z;
  }
}

/* atan2(y,x)
 * Method :
 *  1. Reduce y to positive by atan2(y,x)=-atan2(-y,x).
 *  2. Reduce x to positive by (if x and y are unexceptional):
 *    ARG (x+iy) = arctan(y/x)       ... if x > 0,
 *    ARG (x+iy) = pi - arctan[y/(-x)]   ... if x < 0,
 */
double fd_atan2(double y, double x) {
  static const double tiny = 1.0e-300;
  static const double
      zero = 0.0,
      pi_o_4 = 7.8539816339744827900E-01, /* 0x3FE921FB, 0x54442D18 */
      pi_o_2 = 1.5707963267948965580E+00, /* 0x3FF921FB, 0x54442D18 */
      pi = 3.1415926535897931160E+00;     /* 0x400921FB, 0x54442D18 */
  static const double pi_lo =
      1.2246467991473531772E-16; /* 0x3CA1A626, 0x33145C07 */

  double z;
  int32_t k, m, hx, hy, ix, iy;
  uint32_t lx, ly;

  EXTRACT_WORDS(hx, lx, x);
  ix = hx & 0x7FFFFFFF;
  EXTRACT_WORDS(hy, ly, y);
  iy = hy & 0x7FFFFFFF;
  if (((ix | ((lx | (uint32_t)(0u - lx)) >> 31)) > 0x7FF00000) ||
      ((iy | ((ly | (uint32_t)(0u - ly)) >> 31)) > 0x7FF00000)) {
    return x + y; /* x or y is NaN */
  }
  if (((int32_t)((uint32_t)hx - 0x3FF00000u) | lx) == 0) {
    return fd_atan(y); /* x=1.0 */
  }
  m = ((hy >> 31) & 1) | ((hx >> 30) & 2); /* 2*sign(x)+sign(y) */

  /* when y = 0 */
  if ((iy | ly) == 0) {
    switch (m) {
      case 0:
      case 1:
        return y; /* atan(+-0,+anything)=+-0 */
      case 2:
        return pi + tiny; /* atan(+0,-anything) = pi */
      case 3:
        return -pi - tiny; /* atan(-0,-anything) =-pi */
    }
  }
  /* when x = 0 */
  if ((ix | lx) == 0) return (hy < 0) ? -pi_o_2 - tiny : pi_o_2 + tiny;

  /* when x is INF */
  if (ix == 0x7FF00000) {
    if (iy == 0x7FF00000) {
      switch (m) {
        case 0:
          return pi_o_4 + tiny; /* atan(+INF,+INF) */
        case 1:
          return -pi_o_4 - tiny; /* atan(-INF,+INF) */
        case 2:
          return 3.0 * pi_o_4 + tiny; /*atan(+INF,-INF)*/
        case 3:
          return -3.0 * pi_o_4 - tiny; /*atan(-INF,-INF)*/
      }
    } else {
      switch (m) {
        case 0:
          return zero; /* atan(+...,+INF) */
        case 1:
          return -zero; /* atan(-...,+INF) */
        case 2:
          return pi + tiny; /* atan(+...,-INF) */
        case 3:
          return -pi - tiny; /* atan(-...,-INF) */
      }
    }
  }
  /* when y is INF */
  if (iy == 0x7FF00000) return (hy < 0) ? -pi_o_2 - tiny : pi_o_2 + tiny;

  /* compute y/x */
  k = (iy - ix) >> 20;
  if (k > 60) { /* |y/x| >  2**60 */
    z = pi_o_2 + 0.5 * pi_lo;
    m &= 1;
  } else if (hx < 0 && k < -60) {
    z = 0.0; /* 0 > |y|/x > -2**-60 */
  } else {
    z = fd_atan(fd__fabs(y / x)); /* safe to do y/x */
  }
  switch (m) {
    case 0:
      return z; /* atan(+,+) */
    case 1:
      return -z; /* atan(-,+) */
    case 2:
      return pi - (z - pi_lo); /* atan(+,-) */
    default:                   /* case 3 */
      return (z - pi_lo) - pi; /* atan(-,-) */
  }
}

/* cos(x)
 * Return cosine function of x. (V8's fdlibm_cos body.)
 */
double fd_cos(double x) {
  double y[2], z = 0.0;
  int32_t n, ix;

  /* High word of x. */
  GET_HIGH_WORD(ix, x);

  /* |x| ~< pi/4 */
  ix &= 0x7FFFFFFF;
  if (ix <= 0x3FE921FB) {
    return fd__kernel_cos(x, z);
  } else if (ix >= 0x7FF00000) {
    /* cos(Inf or NaN) is NaN */
    return x - x;
  } else {
    /* argument reduction needed */
    n = fd__rem_pio2(x, y);
    switch (n & 3) {
      case 0:
        return fd__kernel_cos(y[0], y[1]);
      case 1:
        return -fd__kernel_sin(y[0], y[1], 1);
      case 2:
        return -fd__kernel_cos(y[0], y[1]);
      default:
        return fd__kernel_sin(y[0], y[1], 1);
    }
  }
}

/* sin(x)
 * Return sine function of x. (V8's fdlibm_sin body.)
 */
double fd_sin(double x) {
  double y[2], z = 0.0;
  int32_t n, ix;

  /* High word of x. */
  GET_HIGH_WORD(ix, x);

  /* |x| ~< pi/4 */
  ix &= 0x7FFFFFFF;
  if (ix <= 0x3FE921FB) {
    return fd__kernel_sin(x, z, 0);
  } else if (ix >= 0x7FF00000) {
    /* sin(Inf or NaN) is NaN */
    return x - x;
  } else {
    /* argument reduction needed */
    n = fd__rem_pio2(x, y);
    switch (n & 3) {
      case 0:
        return fd__kernel_sin(y[0], y[1], 1);
      case 1:
        return fd__kernel_cos(y[0], y[1]);
      case 2:
        return -fd__kernel_sin(y[0], y[1], 1);
      default:
        return -fd__kernel_cos(y[0], y[1]);
    }
  }
}

/* tan(x)
 * Return tangent function of x.
 */
double fd_tan(double x) {
  double y[2], z = 0.0;
  int32_t n, ix;

  /* High word of x. */
  GET_HIGH_WORD(ix, x);

  /* |x| ~< pi/4 */
  ix &= 0x7FFFFFFF;
  if (ix <= 0x3FE921FB) {
    return fd__kernel_tan(x, z, 1);
  } else if (ix >= 0x7FF00000) {
    /* tan(Inf or NaN) is NaN */
    return x - x; /* NaN */
  } else {
    /* argument reduction needed */
    n = fd__rem_pio2(x, y);
    /* 1 -> n even, -1 -> n odd */
    return fd__kernel_tan(y[0], y[1], 1 - ((n & 1) << 1));
  }
}

/*
 * ES2019 Draft 2019-01-02 12.6.4
 * Math.pow & Exponentiation Operator
 *
 * Return X raised to the Yth power
 */
double fd_pow(double x, double y) {
  static const double
      bp[] = {1.0, 1.5},
      dp_h[] = {0.0, 5.84962487220764160156e-01},  /* 0x3FE2B803, 0x40000000 */
      dp_l[] = {0.0, 1.35003920212974897128e-08},  /* 0x3E4CFDEB, 0x43CFD006 */
      zero = 0.0, one = 1.0, two = 2.0,
      two53 = 9007199254740992.0,  /* 0x43400000, 0x00000000 */
      huge_ = 1.0e300, tiny = 1.0e-300,
      /* poly coefs for (3/2)*(log(x)-2s-2/3*s**3 */
      L1 = 5.99999999999994648725e-01,      /* 0x3FE33333, 0x33333303 */
      L2 = 4.28571428578550184252e-01,      /* 0x3FDB6DB6, 0xDB6FABFF */
      L3 = 3.33333329818377432918e-01,      /* 0x3FD55555, 0x518F264D */
      L4 = 2.72728123808534006489e-01,      /* 0x3FD17460, 0xA91D4101 */
      L5 = 2.30660745775561754067e-01,      /* 0x3FCD864A, 0x93C9DB65 */
      L6 = 2.06975017800338417784e-01,      /* 0x3FCA7E28, 0x4A454EEF */
      P1 = 1.66666666666666019037e-01,      /* 0x3FC55555, 0x5555553E */
      P2 = -2.77777777770155933842e-03,     /* 0xBF66C16C, 0x16BEBD93 */
      P3 = 6.61375632143793436117e-05,      /* 0x3F11566A, 0xAF25DE2C */
      P4 = -1.65339022054652515390e-06,     /* 0xBEBBBD41, 0xC5D26BF1 */
      P5 = 4.13813679705723846039e-08,      /* 0x3E663769, 0x72BEA4D0 */
      lg2 = 6.93147180559945286227e-01,     /* 0x3FE62E42, 0xFEFA39EF */
      lg2_h = 6.93147182464599609375e-01,   /* 0x3FE62E43, 0x00000000 */
      lg2_l = -1.90465429995776804525e-09,  /* 0xBE205C61, 0x0CA86C39 */
      ovt = 8.0085662595372944372e-0017,    /* -(1024-log2(ovfl+.5ulp)) */
      cp = 9.61796693925975554329e-01,      /* 0x3FEEC709, 0xDC3A03FD =2/(3ln2) */
      cp_h = 9.61796700954437255859e-01,    /* 0x3FEEC709, 0xE0000000 =(float)cp */
      cp_l = -7.02846165095275826516e-09,   /* 0xBE3E2FE0, 0x145B01F5 =tail cp_h */
      ivln2 = 1.44269504088896338700e+00,   /* 0x3FF71547, 0x652B82FE =1/ln2 */
      ivln2_h =
          1.44269502162933349609e+00,  /* 0x3FF71547, 0x60000000 =24b 1/ln2 */
      ivln2_l =
          1.92596299112661746887e-08;  /* 0x3E54AE0B, 0xF85DDF44 =1/ln2 tail */

  double z, ax, z_h, z_l, p_h, p_l;
  double y1, t1, t2, r, s, t, u, v, w;
  int i, j, k, yisint, n;
  int hx, hy, ix, iy;
  unsigned lx, ly;

  EXTRACT_WORDS(hx, lx, x);
  EXTRACT_WORDS(hy, ly, y);
  ix = hx & 0x7fffffff;
  iy = hy & 0x7fffffff;

  /* y==zero: x**0 = 1 */
  if ((iy | ly) == 0) return one;

  /* +-NaN return x+y */
  if (ix > 0x7ff00000 || ((ix == 0x7ff00000) && (lx != 0)) || iy > 0x7ff00000 ||
      ((iy == 0x7ff00000) && (ly != 0))) {
    return x + y;
  }

  /* determine if y is an odd int when x < 0
   * yisint = 0 ... y is not an integer
   * yisint = 1 ... y is an odd int
   * yisint = 2 ... y is an even int
   */
  yisint = 0;
  if (hx < 0) {
    if (iy >= 0x43400000) {
      yisint = 2; /* even integer y */
    } else if (iy >= 0x3ff00000) {
      k = (iy >> 20) - 0x3ff; /* exponent */
      if (k > 20) {
        j = ly >> (52 - k);
        if ((j << (52 - k)) == (int)ly) yisint = 2 - (j & 1);
      } else if (ly == 0) {
        j = iy >> (20 - k);
        if ((j << (20 - k)) == iy) yisint = 2 - (j & 1);
      }
    }
  }

  /* special value of y */
  if (ly == 0) {
    if (iy == 0x7ff00000) { /* y is +-inf */
      if (((ix - 0x3ff00000) | lx) == 0) {
        return y - y;                /* inf**+-1 is NaN */
      } else if (ix >= 0x3ff00000) { /* (|x|>1)**+-inf = inf,0 */
        return (hy >= 0) ? y : zero;
      } else { /* (|x|<1)**-,+inf = inf,0 */
        return (hy < 0) ? -y : zero;
      }
    }
    if (iy == 0x3ff00000) { /* y is  +-1 */
      if (hy < 0) {
        return one / x;
      } else {
        return x;
      }
    }
    if (hy == 0x40000000) return x * x; /* y is  2 */
    if (hy == 0x3fe00000) {             /* y is  0.5 */
      if (hx >= 0) {                    /* x >= +0 */
        return sqrt(x);
      }
    }
  }

  ax = fd__fabs(x);
  /* special value of x */
  if (lx == 0) {
    if (ix == 0x7ff00000 || ix == 0 || ix == 0x3ff00000) {
      z = ax;                    /*x is +-0,+-inf,+-1*/
      if (hy < 0) z = one / z;   /* z = (1/|x|) */
      if (hx < 0) {
        if (((ix - 0x3ff00000) | yisint) == 0) {
          /* (-1)**non-int is NaN */
          z = fd__qnan();
        } else if (yisint == 1) {
          z = -z; /* (x<0)**odd = -(|x|**odd) */
        }
      }
      return z;
    }
  }

  n = (hx >> 31) + 1;

  /* (x<0)**(non-int) is NaN */
  if ((n | yisint) == 0) {
    return fd__qnan();
  }

  s = one; /* s (sign of result -ve**odd) = -1 else = 1 */
  if ((n | (yisint - 1)) == 0) s = -one; /* (-ve)**(odd int) */

  /* |y| is huge */
  if (iy > 0x41e00000) {   /* if |y| > 2**31 */
    if (iy > 0x43f00000) { /* if |y| > 2**64, must o/uflow */
      if (ix <= 0x3fefffff) return (hy < 0) ? huge_ * huge_ : tiny * tiny;
      if (ix >= 0x3ff00000) return (hy > 0) ? huge_ * huge_ : tiny * tiny;
    }
    /* over/underflow if x is not close to one */
    if (ix < 0x3fefffff) return (hy < 0) ? s * huge_ * huge_ : s * tiny * tiny;
    if (ix > 0x3ff00000) return (hy > 0) ? s * huge_ * huge_ : s * tiny * tiny;
    /* now |1-x| is tiny <= 2**-20, suffice to compute
       log(x) by x-x^2/2+x^3/3-x^4/4 */
    t = ax - one; /* t has 20 trailing zeros */
    w = (t * t) * (0.5 - t * (0.3333333333333333333333 - t * 0.25));
    u = ivln2_h * t; /* ivln2_h has 21 sig. bits */
    v = t * ivln2_l - w * ivln2;
    t1 = u + v;
    SET_LOW_WORD(t1, 0);
    t2 = v - (t1 - u);
  } else {
    double ss, s2, s_h, s_l, t_h, t_l;
    n = 0;
    /* take care subnormal number */
    if (ix < 0x00100000) {
      ax *= two53;
      n -= 53;
      GET_HIGH_WORD(ix, ax);
    }
    n += ((ix) >> 20) - 0x3ff;
    j = ix & 0x000fffff;
    /* determine interval */
    ix = j | 0x3ff00000; /* normalize ix */
    if (j <= 0x3988E) {
      k = 0; /* |x|<sqrt(3/2) */
    } else if (j < 0xBB67A) {
      k = 1; /* |x|<sqrt(3)   */
    } else {
      k = 0;
      n += 1;
      ix -= 0x00100000;
    }
    SET_HIGH_WORD(ax, ix);

    /* compute ss = s_h+s_l = (x-1)/(x+1) or (x-1.5)/(x+1.5) */
    u = ax - bp[k]; /* bp[0]=1.0, bp[1]=1.5 */
    v = one / (ax + bp[k]);
    ss = u * v;
    s_h = ss;
    SET_LOW_WORD(s_h, 0);
    /* t_h=ax+bp[k] High */
    t_h = zero;
    SET_HIGH_WORD(t_h, ((ix >> 1) | 0x20000000) + 0x00080000 + (k << 18));
    t_l = ax - (t_h - bp[k]);
    s_l = v * ((u - s_h * t_h) - s_h * t_l);
    /* compute log(ax) */
    s2 = ss * ss;
    r = s2 * s2 *
        (L1 + s2 * (L2 + s2 * (L3 + s2 * (L4 + s2 * (L5 + s2 * L6)))));
    r += s_l * (s_h + ss);
    s2 = s_h * s_h;
    t_h = 3.0 + s2 + r;
    SET_LOW_WORD(t_h, 0);
    t_l = r - ((t_h - 3.0) - s2);
    /* u+v = ss*(1+...) */
    u = s_h * t_h;
    v = s_l * t_h + t_l * ss;
    /* 2/(3log2)*(ss+...) */
    p_h = u + v;
    SET_LOW_WORD(p_h, 0);
    p_l = v - (p_h - u);
    z_h = cp_h * p_h; /* cp_h+cp_l = 2/(3*log2) */
    z_l = cp_l * p_h + p_l * cp + dp_l[k];
    /* log2(ax) = (ss+..)*2/(3*log2) = n + dp_h + z_h + z_l */
    t = (double)n;
    t1 = (((z_h + z_l) + dp_h[k]) + t);
    SET_LOW_WORD(t1, 0);
    t2 = z_l - (((t1 - t) - dp_h[k]) - z_h);
  }

  /* split up y into y1+y2 and compute (y1+y2)*(t1+t2) */
  y1 = y;
  SET_LOW_WORD(y1, 0);
  p_l = (y - y1) * t1 + y * t2;
  p_h = y1 * t1;
  z = p_l + p_h;
  EXTRACT_WORDS(j, i, z);
  if (j >= 0x40900000) {               /* z >= 1024 */
    if (((j - 0x40900000) | i) != 0) { /* if z > 1024 */
      return s * huge_ * huge_;        /* overflow */
    } else {
      if (p_l + ovt > z - p_h) return s * huge_ * huge_; /* overflow */
    }
  } else if ((j & 0x7fffffff) >= 0x4090cc00) { /* z <= -1075 */
    if (((j - 0xc090cc00) | i) != 0) {         /* z < -1075 */
      return s * tiny * tiny;                  /* underflow */
    } else {
      if (p_l <= z - p_h) return s * tiny * tiny; /* underflow */
    }
  }
  /*
   * compute 2**(p_h+p_l)
   */
  i = j & 0x7fffffff;
  k = (i >> 20) - 0x3ff;
  n = 0;
  if (i > 0x3fe00000) { /* if |z| > 0.5, set n = [z+0.5] */
    n = j + (0x00100000 >> (k + 1));
    k = ((n & 0x7fffffff) >> 20) - 0x3ff; /* new k for n */
    t = zero;
    SET_HIGH_WORD(t, n & ~(0x000fffff >> k));
    n = ((n & 0x000fffff) | 0x00100000) >> (20 - k);
    if (j < 0) n = -n;
    p_h -= t;
  }
  t = p_l + p_h;
  SET_LOW_WORD(t, 0);
  u = t * lg2_h;
  v = (p_l - (t - p_h)) * lg2 + t * lg2_l;
  z = u + v;
  w = v - (z - u);
  t = z * z;
  t1 = z - t * (P1 + t * (P2 + t * (P3 + t * (P4 + t * P5))));
  r = (z * t1) / ((t1 - two) - (w + z * w));
  z = one - (r - z);
  GET_HIGH_WORD(j, z);
  j += (int)((uint32_t)n << 20);
  if ((j >> 20) <= 0) {
    z = fd__scalbn(z, n); /* subnormal output */
  } else {
    int tmp;
    GET_HIGH_WORD(tmp, z);
    SET_HIGH_WORD(z, tmp + (int)((uint32_t)n << 20));
  }
  return s * z;
}
