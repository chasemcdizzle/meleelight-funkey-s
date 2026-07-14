/*
 * port/fdlibm/fdlibm.js — line-for-line JavaScript port of
 * port/fdlibm/fdlibm.c (sin, cos, tan, atan, atan2, pow).
 *
 * JS numbers ARE IEEE-754 doubles and every +,-,*,/ is correctly rounded,
 * so a faithful port of the C (same operations, same order, same
 * int32/uint32 bit manipulation) is bit-identical to the C compiled with
 * -ffp-contract=off. Verified by oracle/fdlibm-crosscheck/.
 *
 * Provenance: see fdlibm.c — V8 v8/src/base/ieee754.cc @ 12.4.254,
 * derived from Sun's fdlibm. Upstream notices, carried verbatim:
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
 * Port conventions (C -> JS), and nothing else:
 *   - int32_t arithmetic  -> |0 ; uint32_t -> >>>0 ; C's uint32 >> is >>>.
 *   - word access via a shared Float64Array/Uint32Array pair (endian
 *     detected once); SET_*_WORD(d, v) macros become d = setXxxWord(d, v).
 *   - (int32_t)double truncation -> |0 (all sites are range-bounded).
 *   - fabs/copysign are bit operations (not Math.abs), floor/sqrt are
 *     Math.floor/Math.sqrt (IEEE-exact operations).
 *   - loads/exports as a browser classic script (window.__fdlibm) or a
 *     CommonJS module (module.exports) — same single file both sides.
 *
 * Constants keep their /* 0xHHHHHHHH, 0xHHHHHHHH *​/ bit-pattern comments;
 * oracle/fdlibm-crosscheck/check-constants.js verifies every one.
 */
(function (global) {
  "use strict";

  var buf = new ArrayBuffer(8);
  var f64 = new Float64Array(buf);
  var u32 = new Uint32Array(buf);
  f64[0] = 1.0;
  var HIW = u32[1] === 0x3ff00000 ? 1 : 0;
  var LOW = 1 - HIW;

  function highWord(d) { f64[0] = d; return u32[HIW] | 0; }     /* GET_HIGH_WORD -> int32 */
  function lowWord(d) { f64[0] = d; return u32[LOW] >>> 0; }    /* GET_LOW_WORD -> uint32 */
  function insertWords(hi, lo) { u32[HIW] = hi; u32[LOW] = lo; return f64[0]; }
  function setHighWord(d, hi) { f64[0] = d; u32[HIW] = hi; return f64[0]; }
  function setLowWord(d, lo) { f64[0] = d; u32[LOW] = lo; return f64[0]; }
  function fabs(x) { f64[0] = x; u32[HIW] = u32[HIW] & 0x7fffffff; return f64[0]; }
  function copysign(x, y) {
    f64[0] = y;
    var sign = u32[HIW] & 0x80000000;
    f64[0] = x;
    u32[HIW] = (u32[HIW] & 0x7fffffff) | sign;
    return f64[0];
  }
  function qnan() { return insertWords(0x7ff80000, 0); }

  /* scalbn(x,n) — classic Sun fdlibm s_scalbn.c */
  var scalbn_two54 = 1.80143985094819840000e+16,  /* 0x43500000, 0x00000000 */
      scalbn_twom54 = 5.55111512312578270212e-17, /* 0x3C900000, 0x00000000 */
      scalbn_huge = 1.0e+300, scalbn_tiny = 1.0e-300;
  function scalbn(x, n) {
    var k, hx, lx;
    hx = highWord(x);
    lx = lowWord(x) | 0;
    k = (hx & 0x7ff00000) >> 20; /* extract exponent */
    if (k === 0) {               /* 0 or subnormal x */
      if ((lx | (hx & 0x7fffffff)) === 0) return x; /* +-0 */
      x *= scalbn_two54;
      hx = highWord(x);
      k = ((hx & 0x7ff00000) >> 20) - 54;
      if (n < -50000) return scalbn_tiny * x; /* underflow */
    }
    if (k === 0x7ff) return x + x; /* NaN or Inf */
    k = (k + n) | 0;
    if (k > 0x7fe) return scalbn_huge * copysign(scalbn_huge, x); /* overflow */
    if (k > 0) { /* normal result */
      x = setHighWord(x, (hx & 0x800fffff) | (k << 20));
      return x;
    }
    if (k <= -54) {
      if (n > 50000) { /* in case integer overflow in n+k */
        return scalbn_huge * copysign(scalbn_huge, x); /* overflow */
      } else {
        return scalbn_tiny * copysign(scalbn_tiny, x); /* underflow */
      }
    }
    k += 54; /* subnormal result */
    x = setHighWord(x, (hx & 0x800fffff) | (k << 20));
    return x * scalbn_twom54;
  }

  /* __ieee754_rem_pio2(x,y) */
  var two_over_pi = [
      0xA2F983, 0x6E4E44, 0x1529FC, 0x2757D1, 0xF534DD, 0xC0DB62, 0x95993C,
      0x439041, 0xFE5163, 0xABDEBB, 0xC561B7, 0x246E3A, 0x424DD2, 0xE00649,
      0x2EEA09, 0xD1921C, 0xFE1DEB, 0x1CB129, 0xA73EE8, 0x8235F5, 0x2EBB44,
      0x84E99C, 0x7026B4, 0x5F7E41, 0x3991D6, 0x398353, 0x39F49C, 0x845F8B,
      0xBDF928, 0x3B1FF8, 0x97FFDE, 0x05980F, 0xEF2F11, 0x8B5A0A, 0x6D1F6D,
      0x367ECF, 0x27CB09, 0xB74F46, 0x3F669E, 0x5FEA2D, 0x7527BA, 0xC7EBE5,
      0xF17B3D, 0x0739F7, 0x8A5292, 0xEA6BFB, 0x5FB11F, 0x8D5D08, 0x560330,
      0x46FC7B, 0x6BABF0, 0xCFBC20, 0x9AF436, 0x1DA9E3, 0x91615E, 0xE61B08,
      0x659985, 0x5F14A0, 0x68408D, 0xFFD880, 0x4D7327, 0x310606, 0x1556CA,
      0x73A8C9, 0x60E27B, 0xC08C6B
  ];

  var npio2_hw = [
      0x3FF921FB, 0x400921FB, 0x4012D97C, 0x401921FB, 0x401F6A7A, 0x4022D97C,
      0x4025FDBB, 0x402921FB, 0x402C463A, 0x402F6A7A, 0x4031475C, 0x4032D97C,
      0x40346B9C, 0x4035FDBB, 0x40378FDB, 0x403921FB, 0x403AB41B, 0x403C463A,
      0x403DD85A, 0x403F6A7A, 0x40407E4C, 0x4041475C, 0x4042106C, 0x4042D97C,
      0x4043A28C, 0x40446B9C, 0x404534AC, 0x4045FDBB, 0x4046C6CB, 0x40478FDB,
      0x404858EB, 0x404921FB
  ];

  var rp_zero = 0.00000000000000000000e+00,    /* 0x00000000, 0x00000000 */
      rp_half = 5.00000000000000000000e-01,    /* 0x3FE00000, 0x00000000 */
      rp_two24 = 1.67772160000000000000e+07,   /* 0x41700000, 0x00000000 */
      rp_invpio2 = 6.36619772367581382433e-01, /* 0x3FE45F30, 0x6DC9C883 */
      rp_pio2_1 = 1.57079632673412561417e+00,  /* 0x3FF921FB, 0x54400000 */
      rp_pio2_1t = 6.07710050650619224932e-11, /* 0x3DD0B461, 0x1A626331 */
      rp_pio2_2 = 6.07710050630396597660e-11,  /* 0x3DD0B461, 0x1A600000 */
      rp_pio2_2t = 2.02226624879595063154e-21, /* 0x3BA3198A, 0x2E037073 */
      rp_pio2_3 = 2.02226624871116645580e-21,  /* 0x3BA3198A, 0x2E000000 */
      rp_pio2_3t = 8.47842766036889956997e-32; /* 0x397B839A, 0x252049C1 */

  function rem_pio2(x, y) {
    var z, w, t, r, fn;
    var tx = [0.0, 0.0, 0.0];
    var e0, i, j, nx, n, ix, hx;
    var low, high;

    z = 0;
    hx = highWord(x); /* high word of x */
    ix = hx & 0x7fffffff;
    if (ix <= 0x3fe921fb) { /* |x| ~<= pi/4 , no need for reduction */
      y[0] = x;
      y[1] = 0;
      return 0;
    }
    if (ix < 0x4002d97c) { /* |x| < 3pi/4, special case with n=+-1 */
      if (hx > 0) {
        z = x - rp_pio2_1;
        if (ix !== 0x3ff921fb) { /* 33+53 bit pi is good enough */
          y[0] = z - rp_pio2_1t;
          y[1] = (z - y[0]) - rp_pio2_1t;
        } else { /* near pi/2, use 33+33+53 bit pi */
          z -= rp_pio2_2;
          y[0] = z - rp_pio2_2t;
          y[1] = (z - y[0]) - rp_pio2_2t;
        }
        return 1;
      } else { /* negative x */
        z = x + rp_pio2_1;
        if (ix !== 0x3ff921fb) { /* 33+53 bit pi is good enough */
          y[0] = z + rp_pio2_1t;
          y[1] = (z - y[0]) + rp_pio2_1t;
        } else { /* near pi/2, use 33+33+53 bit pi */
          z += rp_pio2_2;
          y[0] = z + rp_pio2_2t;
          y[1] = (z - y[0]) + rp_pio2_2t;
        }
        return -1;
      }
    }
    if (ix <= 0x413921fb) { /* |x| ~<= 2^19*(pi/2), medium size */
      t = fabs(x);
      n = (t * rp_invpio2 + rp_half) | 0;
      fn = n;
      r = t - fn * rp_pio2_1;
      w = fn * rp_pio2_1t; /* 1st round good to 85 bit */
      if (n < 32 && ix !== npio2_hw[n - 1]) {
        y[0] = r - w; /* quick check no cancellation */
      } else {
        j = ix >> 20;
        y[0] = r - w;
        high = highWord(y[0]);
        i = j - ((high >> 20) & 0x7ff);
        if (i > 16) { /* 2nd iteration needed, good to 118 */
          t = r;
          w = fn * rp_pio2_2;
          r = t - w;
          w = fn * rp_pio2_2t - ((t - r) - w);
          y[0] = r - w;
          high = highWord(y[0]);
          i = j - ((high >> 20) & 0x7ff);
          if (i > 49) { /* 3rd iteration need, 151 bits acc */
            t = r;      /* will cover all possible cases */
            w = fn * rp_pio2_3;
            r = t - w;
            w = fn * rp_pio2_3t - ((t - r) - w);
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
    if (ix >= 0x7ff00000) { /* x is inf or NaN */
      y[0] = y[1] = x - x;
      return 0;
    }
    /* set z = scalbn(|x|,ilogb(x)-23) */
    low = lowWord(x);
    z = setLowWord(z, low);
    e0 = ((ix >> 20) - 1046) | 0; /* e0 = ilogb(z)-23; */
    z = setHighWord(z, (ix - (e0 << 20)) | 0);
    for (i = 0; i < 2; i++) {
      tx[i] = z | 0;
      z = (z - tx[i]) * rp_two24;
    }
    tx[2] = z;
    nx = 3;
    while (tx[nx - 1] === rp_zero) nx--; /* skip zero term */
    n = kernel_rem_pio2(tx, y, e0, nx, 2, two_over_pi);
    if (hx < 0) {
      y[0] = -y[0];
      y[1] = -y[1];
      return -n;
    }
    return n;
  }

  /* __kernel_cos( x,  y ) */
  var kc_one = 1.00000000000000000000e+00, /* 0x3FF00000, 0x00000000 */
      kc_C1 = 4.16666666666666019037e-02,  /* 0x3FA55555, 0x5555554C */
      kc_C2 = -1.38888888888741095749e-03, /* 0xBF56C16C, 0x16C15177 */
      kc_C3 = 2.48015872894767294178e-05,  /* 0x3EFA01A0, 0x19CB1590 */
      kc_C4 = -2.75573143513906633035e-07, /* 0xBE927E4F, 0x809C52AD */
      kc_C5 = 2.08757232129817482790e-09,  /* 0x3E21EE9E, 0xBDB4B1C4 */
      kc_C6 = -1.13596475577881948265e-11; /* 0xBDA8FAE9, 0xBE8838D4 */

  function kernel_cos(x, y) {
    var a, iz, z, r, qx;
    var ix;
    ix = highWord(x);
    ix &= 0x7fffffff;                       /* ix = |x|'s high word*/
    if (ix < 0x3e400000) {                  /* if x < 2**27 */
      if ((x | 0) === 0) return kc_one;     /* generate inexact */
    }
    z = x * x;
    r = z * (kc_C1 + z * (kc_C2 + z * (kc_C3 + z * (kc_C4 + z * (kc_C5 + z * kc_C6)))));
    if (ix < 0x3fd33333) { /* if |x| < 0.3 */
      return kc_one - (0.5 * z - (z * r - x * y));
    } else {
      if (ix > 0x3fe90000) { /* x > 0.78125 */
        qx = 0.28125;
      } else {
        qx = insertWords(ix - 0x00200000, 0); /* x/4 */
      }
      iz = 0.5 * z - qx;
      a = kc_one - qx;
      return a - (iz - (z * r - x * y));
    }
  }

  /* __kernel_rem_pio2(x,y,e0,nx,prec,ipio2) */
  var init_jk = [2, 3, 4, 6]; /* initial value for jk */

  var PIo2 = [
      1.57079625129699707031e+00, /* 0x3FF921FB, 0x40000000 */
      7.54978941586159635335e-08, /* 0x3E74442D, 0x00000000 */
      5.39030252995776476554e-15, /* 0x3CF84698, 0x80000000 */
      3.28200341580791294123e-22, /* 0x3B78CC51, 0x60000000 */
      1.27065575308067607349e-29, /* 0x39F01B83, 0x80000000 */
      1.22933308981111328932e-36, /* 0x387A2520, 0x40000000 */
      2.73370053816464559624e-44, /* 0x36E38222, 0x80000000 */
      2.16741683877804819444e-51  /* 0x3569F31D, 0x00000000 */
  ];

  var kr_zero = 0.0,
      kr_one = 1.0,
      kr_two24 = 1.67772160000000000000e+07,  /* 0x41700000, 0x00000000 */
      kr_twon24 = 5.96046447753906250000e-08; /* 0x3E700000, 0x00000000 */

  function kernel_rem_pio2(x, y, e0, nx, prec, ipio2) {
    var jz, jx, jv, jp, jk, carry, n, i, j, k, m, q0, ih;
    var iq = new Array(20);
    var z, fw;
    var f = new Array(20), fq = new Array(20), q = new Array(20);

    /* initialize jk*/
    jk = init_jk[prec];
    jp = jk;

    /* determine jx,jv,q0, note that 3>q0 */
    jx = (nx - 1) | 0;
    jv = ((e0 - 3) / 24) | 0; /* C integer division truncates toward 0 */
    if (jv < 0) jv = 0;
    q0 = (e0 - 24 * (jv + 1)) | 0;

    /* set up f[0] to f[jx+jk] where f[jx+jk] = ipio2[jv+jk] */
    j = (jv - jx) | 0;
    m = (jx + jk) | 0;
    for (i = 0; i <= m; i++, j++) {
      f[i] = (j < 0) ? kr_zero : ipio2[j];
    }

    /* compute q[0],q[1],...q[jk] */
    for (i = 0; i <= jk; i++) {
      for (j = 0, fw = 0.0; j <= jx; j++) fw += x[j] * f[jx + i - j];
      q[i] = fw;
    }

    jz = jk;
    recompute: for (;;) {
      /* distill q[] into iq[] reversingly */
      for (i = 0, j = jz, z = q[jz]; j > 0; i++, j--) {
        fw = (kr_twon24 * z) | 0;
        iq[i] = (z - kr_two24 * fw) | 0;
        z = q[j - 1] + fw;
      }

      /* compute n */
      z = scalbn(z, q0);                /* actual value of z */
      z -= 8.0 * Math.floor(z * 0.125); /* trim off integer >= 8 */
      n = z | 0;
      z -= n;
      ih = 0;
      if (q0 > 0) { /* need iq[jz-1] to determine n */
        i = (iq[jz - 1] >> (24 - q0));
        n = (n + i) | 0;
        iq[jz - 1] = (iq[jz - 1] - (i << (24 - q0))) | 0;
        ih = iq[jz - 1] >> (23 - q0);
      } else if (q0 === 0) {
        ih = iq[jz - 1] >> 23;
      } else if (z >= 0.5) {
        ih = 2;
      }

      if (ih > 0) { /* q > 0.5 */
        n = (n + 1) | 0;
        carry = 0;
        for (i = 0; i < jz; i++) { /* compute 1-q */
          j = iq[i];
          if (carry === 0) {
            if (j !== 0) {
              carry = 1;
              iq[i] = (0x1000000 - j) | 0;
            }
          } else {
            iq[i] = (0xffffff - j) | 0;
          }
        }
        if (q0 > 0) { /* rare case: chance is 1 in 12 */
          switch (q0) {
            case 1:
              iq[jz - 1] &= 0x7fffff;
              break;
            case 2:
              iq[jz - 1] &= 0x3fffff;
              break;
          }
        }
        if (ih === 2) {
          z = kr_one - z;
          if (carry !== 0) z -= scalbn(kr_one, q0);
        }
      }

      /* check if recomputation is needed */
      if (z === kr_zero) {
        j = 0;
        for (i = jz - 1; i >= jk; i--) j |= iq[i];
        if (j === 0) { /* need recomputation */
          for (k = 1; jk >= k && iq[jk - k] === 0; k++) {
            /* k = no. of terms needed */
          }

          for (i = jz + 1; i <= jz + k; i++) { /* add q[jz+1] to q[jz+k] */
            f[jx + i] = ipio2[jv + i];
            for (j = 0, fw = 0.0; j <= jx; j++) fw += x[j] * f[jx + i - j];
            q[i] = fw;
          }
          jz = (jz + k) | 0;
          continue recompute;
        }
      }
      break;
    }

    /* chop off zero terms */
    if (z === 0.0) {
      jz = (jz - 1) | 0;
      q0 = (q0 - 24) | 0;
      while (iq[jz] === 0) {
        jz--;
        q0 -= 24;
      }
    } else { /* break z into 24-bit if necessary */
      z = scalbn(z, -q0);
      if (z >= kr_two24) {
        fw = (kr_twon24 * z) | 0;
        iq[jz] = (z - kr_two24 * fw) | 0;
        jz = (jz + 1) | 0;
        q0 = (q0 + 24) | 0;
        iq[jz] = fw | 0;
      } else {
        iq[jz] = z | 0;
      }
    }

    /* convert integer "bit" chunk to floating-point value */
    fw = scalbn(kr_one, q0);
    for (i = jz; i >= 0; i--) {
      q[i] = fw * iq[i];
      fw *= kr_twon24;
    }

    /* compute PIo2[0,...,jp]*q[jz,...,0] */
    for (i = jz; i >= 0; i--) {
      for (fw = 0.0, k = 0; k <= jp && k <= jz - i; k++) fw += PIo2[k] * q[i + k];
      fq[jz - i] = fw;
    }

    /* compress fq[] into y[] (prec is always 2 here) */
    switch (prec) {
      case 0:
        fw = 0.0;
        for (i = jz; i >= 0; i--) fw += fq[i];
        y[0] = (ih === 0) ? fw : -fw;
        break;
      case 1:
      case 2:
        fw = 0.0;
        for (i = jz; i >= 0; i--) fw += fq[i];
        y[0] = (ih === 0) ? fw : -fw;
        fw = fq[0] - fw;
        for (i = 1; i <= jz; i++) fw += fq[i];
        y[1] = (ih === 0) ? fw : -fw;
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
        if (ih === 0) {
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

  /* __kernel_sin( x, y, iy) */
  var ks_half = 5.00000000000000000000e-01, /* 0x3FE00000, 0x00000000 */
      ks_S1 = -1.66666666666666324348e-01,  /* 0xBFC55555, 0x55555549 */
      ks_S2 = 8.33333333332248946124e-03,   /* 0x3F811111, 0x1110F8A6 */
      ks_S3 = -1.98412698298579493134e-04,  /* 0xBF2A01A0, 0x19C161D5 */
      ks_S4 = 2.75573137070700676789e-06,   /* 0x3EC71DE3, 0x57B1FE7D */
      ks_S5 = -2.50507602534068634195e-08,  /* 0xBE5AE5E6, 0x8A2B9CEB */
      ks_S6 = 1.58969099521155010221e-10;   /* 0x3DE5D93A, 0x5ACFD57C */

  function kernel_sin(x, y, iy) {
    var z, r, v;
    var ix;
    ix = highWord(x);
    ix &= 0x7fffffff;      /* high word of x */
    if (ix < 0x3e400000) { /* |x| < 2**-27 */
      if ((x | 0) === 0) return x;
    } /* generate inexact */
    z = x * x;
    v = z * x;
    r = ks_S2 + z * (ks_S3 + z * (ks_S4 + z * (ks_S5 + z * ks_S6)));
    if (iy === 0) {
      return x + v * (ks_S1 + z * r);
    } else {
      return x - ((z * (ks_half * y - v * r) - y) - v * ks_S1);
    }
  }

  /* __kernel_tan( x, y, k ) */
  var kt_T = [
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
  ];
  var kt_one = kt_T[13], kt_pio4 = kt_T[14], kt_pio4lo = kt_T[15];

  function kernel_tan(x, y, iy) {
    var z, r, v, w, s;
    var ix, hx;
    var low;
    var a, t;

    hx = highWord(x);      /* high word of x */
    ix = hx & 0x7fffffff;  /* high word of |x| */
    if (ix < 0x3e300000) { /* x < 2**-28 */
      if ((x | 0) === 0) { /* generate inexact */
        low = lowWord(x) | 0;
        if (((ix | low) | (iy + 1)) === 0) {
          return kt_one / fabs(x);
        } else {
          if (iy === 1) {
            return x;
          } else { /* compute -1 / (x+y) carefully */
            z = w = x + y;
            z = setLowWord(z, 0);
            v = y - (z - x);
            t = a = -kt_one / w;
            t = setLowWord(t, 0);
            s = kt_one + t * z;
            return t + a * (s + t * v);
          }
        }
      }
    }
    if (ix >= 0x3fe59428) { /* |x| >= 0.6744 */
      if (hx < 0) {
        x = -x;
        y = -y;
      }
      z = kt_pio4 - x;
      w = kt_pio4lo - y;
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
    r = kt_T[1] + w * (kt_T[3] + w * (kt_T[5] + w * (kt_T[7] + w * (kt_T[9] + w * kt_T[11]))));
    v = z *
        (kt_T[2] + w * (kt_T[4] + w * (kt_T[6] + w * (kt_T[8] + w * (kt_T[10] + w * kt_T[12])))));
    s = z * x;
    r = y + z * (s * (r + v) + y);
    r += kt_T[0] * s;
    w = x + r;
    if (ix >= 0x3fe59428) {
      v = iy;
      return (1 - ((hx >> 30) & 2)) * (v - 2.0 * (x - (w * w / (w + v) - r)));
    }
    if (iy === 1) {
      return w;
    } else {
      /* compute -1.0 / (x+r) accurately */
      z = w;
      z = setLowWord(z, 0);
      v = r - (z - x);  /* z+v = r+x */
      t = a = -1.0 / w; /* a = -1.0/w */
      t = setLowWord(t, 0);
      s = 1.0 + t * z;
      return t + a * (s + t * v);
    }
  }

  /* atan(x) */
  var atanhi = [
      4.63647609000806093515e-01, /* atan(0.5)hi 0x3FDDAC67, 0x0561BB4F */
      7.85398163397448278999e-01, /* atan(1.0)hi 0x3FE921FB, 0x54442D18 */
      9.82793723247329054082e-01, /* atan(1.5)hi 0x3FEF730B, 0xD281F69B */
      1.57079632679489655800e+00  /* atan(inf)hi 0x3FF921FB, 0x54442D18 */
  ];

  var atanlo = [
      2.26987774529616870924e-17, /* atan(0.5)lo 0x3C7A2B7F, 0x222F65E2 */
      3.06161699786838301793e-17, /* atan(1.0)lo 0x3C81A626, 0x33145C07 */
      1.39033110312309984516e-17, /* atan(1.5)lo 0x3C700788, 0x7AF0CBBD */
      6.12323399573676603587e-17  /* atan(inf)lo 0x3C91A626, 0x33145C07 */
  ];

  var aT = [
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
      1.62858201153657823623e-02   /* 0x3F90AD3A, 0xE322DA11 */
  ];

  var at_one = 1.0, at_huge = 1.0e300;

  function atan(x) {
    var w, s1, s2, z;
    var ix, hx, id;
    var low;

    hx = highWord(x);
    ix = hx & 0x7fffffff;
    if (ix >= 0x44100000) { /* if |x| >= 2^66 */
      low = lowWord(x) | 0;
      if (ix > 0x7ff00000 || (ix === 0x7ff00000 && (low !== 0)))
        return x + x; /* NaN */
      if (hx > 0)
        return atanhi[3] + atanlo[3];
      else
        return -atanhi[3] - atanlo[3];
    }
    if (ix < 0x3fdc0000) {                 /* |x| < 0.4375 */
      if (ix < 0x3e400000) {               /* |x| < 2^-27 */
        if (at_huge + x > at_one) return x; /* raise inexact */
      }
      id = -1;
    } else {
      x = fabs(x);
      if (ix < 0x3ff30000) {   /* |x| < 1.1875 */
        if (ix < 0x3fe60000) { /* 7/16 <=|x|<11/16 */
          id = 0;
          x = (2.0 * x - at_one) / (2.0 + x);
        } else { /* 11/16<=|x|< 19/16 */
          id = 1;
          x = (x - at_one) / (x + at_one);
        }
      } else {
        if (ix < 0x40038000) { /* |x| < 2.4375 */
          id = 2;
          x = (x - 1.5) / (at_one + 1.5 * x);
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

  /* atan2(y,x) */
  var a2_tiny = 1.0e-300,
      a2_zero = 0.0,
      a2_pi_o_4 = 7.8539816339744827900e-01, /* 0x3FE921FB, 0x54442D18 */
      a2_pi_o_2 = 1.5707963267948965580e+00, /* 0x3FF921FB, 0x54442D18 */
      a2_pi = 3.1415926535897931160e+00,     /* 0x400921FB, 0x54442D18 */
      a2_pi_lo = 1.2246467991473531772e-16;  /* 0x3CA1A626, 0x33145C07 */

  function atan2(y, x) {
    var z;
    var k, m, hx, hy, ix, iy;
    var lx, ly;

    hx = highWord(x);
    lx = lowWord(x);
    ix = hx & 0x7fffffff;
    hy = highWord(y);
    ly = lowWord(y);
    iy = hy & 0x7fffffff;
    if (((ix | ((lx | ((-lx) | 0)) >>> 31)) > 0x7ff00000) ||
        ((iy | ((ly | ((-ly) | 0)) >>> 31)) > 0x7ff00000)) {
      return x + y; /* x or y is NaN */
    }
    if ((((hx - 0x3ff00000) | 0) | lx) === 0) {
      return atan(y); /* x=1.0 */
    }
    m = ((hy >> 31) & 1) | ((hx >> 30) & 2); /* 2*sign(x)+sign(y) */

    /* when y = 0 */
    if ((iy | ly) === 0) {
      switch (m) {
        case 0:
        case 1:
          return y; /* atan(+-0,+anything)=+-0 */
        case 2:
          return a2_pi + a2_tiny; /* atan(+0,-anything) = pi */
        case 3:
          return -a2_pi - a2_tiny; /* atan(-0,-anything) =-pi */
      }
    }
    /* when x = 0 */
    if ((ix | lx) === 0) return (hy < 0) ? -a2_pi_o_2 - a2_tiny : a2_pi_o_2 + a2_tiny;

    /* when x is INF */
    if (ix === 0x7ff00000) {
      if (iy === 0x7ff00000) {
        switch (m) {
          case 0:
            return a2_pi_o_4 + a2_tiny; /* atan(+INF,+INF) */
          case 1:
            return -a2_pi_o_4 - a2_tiny; /* atan(-INF,+INF) */
          case 2:
            return 3.0 * a2_pi_o_4 + a2_tiny; /*atan(+INF,-INF)*/
          case 3:
            return -3.0 * a2_pi_o_4 - a2_tiny; /*atan(-INF,-INF)*/
        }
      } else {
        switch (m) {
          case 0:
            return a2_zero; /* atan(+...,+INF) */
          case 1:
            return -a2_zero; /* atan(-...,+INF) */
          case 2:
            return a2_pi + a2_tiny; /* atan(+...,-INF) */
          case 3:
            return -a2_pi - a2_tiny; /* atan(-...,-INF) */
        }
      }
    }
    /* when y is INF */
    if (iy === 0x7ff00000) return (hy < 0) ? -a2_pi_o_2 - a2_tiny : a2_pi_o_2 + a2_tiny;

    /* compute y/x */
    k = (iy - ix) >> 20;
    if (k > 60) { /* |y/x| >  2**60 */
      z = a2_pi_o_2 + 0.5 * a2_pi_lo;
      m &= 1;
    } else if (hx < 0 && k < -60) {
      z = 0.0; /* 0 > |y|/x > -2**-60 */
    } else {
      z = atan(fabs(y / x)); /* safe to do y/x */
    }
    switch (m) {
      case 0:
        return z; /* atan(+,+) */
      case 1:
        return -z; /* atan(-,+) */
      case 2:
        return a2_pi - (z - a2_pi_lo); /* atan(+,-) */
      default:                         /* case 3 */
        return (z - a2_pi_lo) - a2_pi; /* atan(-,-) */
    }
  }

  /* cos(x) — V8's fdlibm_cos body */
  function cos(x) {
    var y = [0.0, 0.0], z = 0.0;
    var n, ix;

    /* High word of x. */
    ix = highWord(x);

    /* |x| ~< pi/4 */
    ix &= 0x7fffffff;
    if (ix <= 0x3fe921fb) {
      return kernel_cos(x, z);
    } else if (ix >= 0x7ff00000) {
      /* cos(Inf or NaN) is NaN */
      return x - x;
    } else {
      /* argument reduction needed */
      n = rem_pio2(x, y);
      switch (n & 3) {
        case 0:
          return kernel_cos(y[0], y[1]);
        case 1:
          return -kernel_sin(y[0], y[1], 1);
        case 2:
          return -kernel_cos(y[0], y[1]);
        default:
          return kernel_sin(y[0], y[1], 1);
      }
    }
  }

  /* sin(x) — V8's fdlibm_sin body */
  function sin(x) {
    var y = [0.0, 0.0], z = 0.0;
    var n, ix;

    /* High word of x. */
    ix = highWord(x);

    /* |x| ~< pi/4 */
    ix &= 0x7fffffff;
    if (ix <= 0x3fe921fb) {
      return kernel_sin(x, z, 0);
    } else if (ix >= 0x7ff00000) {
      /* sin(Inf or NaN) is NaN */
      return x - x;
    } else {
      /* argument reduction needed */
      n = rem_pio2(x, y);
      switch (n & 3) {
        case 0:
          return kernel_sin(y[0], y[1], 1);
        case 1:
          return kernel_cos(y[0], y[1]);
        case 2:
          return -kernel_sin(y[0], y[1], 1);
        default:
          return -kernel_cos(y[0], y[1]);
      }
    }
  }

  /* tan(x) */
  function tan(x) {
    var y = [0.0, 0.0], z = 0.0;
    var n, ix;

    /* High word of x. */
    ix = highWord(x);

    /* |x| ~< pi/4 */
    ix &= 0x7fffffff;
    if (ix <= 0x3fe921fb) {
      return kernel_tan(x, z, 1);
    } else if (ix >= 0x7ff00000) {
      /* tan(Inf or NaN) is NaN */
      return x - x; /* NaN */
    } else {
      /* argument reduction needed */
      n = rem_pio2(x, y);
      /* 1 -> n even, -1 -> n odd */
      return kernel_tan(y[0], y[1], 1 - ((n & 1) << 1));
    }
  }

  /* pow(x, y) — ES2019 Math.pow */
  var pw_bp = [1.0, 1.5],
      pw_dp_h = [0.0, 5.84962487220764160156e-01],  /* 0x3FE2B803, 0x40000000 */
      pw_dp_l = [0.0, 1.35003920212974897128e-08],  /* 0x3E4CFDEB, 0x43CFD006 */
      pw_zero = 0.0, pw_one = 1.0, pw_two = 2.0,
      pw_two53 = 9007199254740992.0,  /* 0x43400000, 0x00000000 */
      pw_huge = 1.0e300, pw_tiny = 1.0e-300,
      /* poly coefs for (3/2)*(log(x)-2s-2/3*s**3 */
      pw_L1 = 5.99999999999994648725e-01,      /* 0x3FE33333, 0x33333303 */
      pw_L2 = 4.28571428578550184252e-01,      /* 0x3FDB6DB6, 0xDB6FABFF */
      pw_L3 = 3.33333329818377432918e-01,      /* 0x3FD55555, 0x518F264D */
      pw_L4 = 2.72728123808534006489e-01,      /* 0x3FD17460, 0xA91D4101 */
      pw_L5 = 2.30660745775561754067e-01,      /* 0x3FCD864A, 0x93C9DB65 */
      pw_L6 = 2.06975017800338417784e-01,      /* 0x3FCA7E28, 0x4A454EEF */
      pw_P1 = 1.66666666666666019037e-01,      /* 0x3FC55555, 0x5555553E */
      pw_P2 = -2.77777777770155933842e-03,     /* 0xBF66C16C, 0x16BEBD93 */
      pw_P3 = 6.61375632143793436117e-05,      /* 0x3F11566A, 0xAF25DE2C */
      pw_P4 = -1.65339022054652515390e-06,     /* 0xBEBBBD41, 0xC5D26BF1 */
      pw_P5 = 4.13813679705723846039e-08,      /* 0x3E663769, 0x72BEA4D0 */
      pw_lg2 = 6.93147180559945286227e-01,     /* 0x3FE62E42, 0xFEFA39EF */
      pw_lg2_h = 6.93147182464599609375e-01,   /* 0x3FE62E43, 0x00000000 */
      pw_lg2_l = -1.90465429995776804525e-09,  /* 0xBE205C61, 0x0CA86C39 */
      pw_ovt = 8.0085662595372944372e-0017,    /* -(1024-log2(ovfl+.5ulp)) */
      pw_cp = 9.61796693925975554329e-01,      /* 0x3FEEC709, 0xDC3A03FD =2/(3ln2) */
      pw_cp_h = 9.61796700954437255859e-01,    /* 0x3FEEC709, 0xE0000000 =(float)cp */
      pw_cp_l = -7.02846165095275826516e-09,   /* 0xBE3E2FE0, 0x145B01F5 =tail cp_h */
      pw_ivln2 = 1.44269504088896338700e+00,   /* 0x3FF71547, 0x652B82FE =1/ln2 */
      pw_ivln2_h = 1.44269502162933349609e+00, /* 0x3FF71547, 0x60000000 =24b 1/ln2 */
      pw_ivln2_l = 1.92596299112661746887e-08; /* 0x3E54AE0B, 0xF85DDF44 =1/ln2 tail */

  function pow(x, y) {
    var z, ax, z_h, z_l, p_h, p_l;
    var y1, t1, t2, r, s, t, u, v, w;
    var i, j, k, yisint, n;
    var hx, hy, ix, iy;
    var lx, ly;
    var ss, s2, s_h, s_l, t_h, t_l;
    var tmp;

    hx = highWord(x);
    lx = lowWord(x);
    hy = highWord(y);
    ly = lowWord(y);
    ix = hx & 0x7fffffff;
    iy = hy & 0x7fffffff;

    /* y==zero: x**0 = 1 */
    if ((iy | ly) === 0) return pw_one;

    /* +-NaN return x+y */
    if (ix > 0x7ff00000 || ((ix === 0x7ff00000) && (lx !== 0)) || iy > 0x7ff00000 ||
        ((iy === 0x7ff00000) && (ly !== 0))) {
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
        k = ((iy >> 20) - 0x3ff) | 0; /* exponent */
        if (k > 20) {
          j = ly >>> (52 - k);
          if (((j << (52 - k)) | 0) === (ly | 0)) yisint = 2 - (j & 1);
        } else if (ly === 0) {
          j = iy >> (20 - k);
          if (((j << (20 - k)) | 0) === iy) yisint = 2 - (j & 1);
        }
      }
    }

    /* special value of y */
    if (ly === 0) {
      if (iy === 0x7ff00000) { /* y is +-inf */
        if ((((ix - 0x3ff00000) | 0) | lx) === 0) {
          return y - y;                 /* inf**+-1 is NaN */
        } else if (ix >= 0x3ff00000) {  /* (|x|>1)**+-inf = inf,0 */
          return (hy >= 0) ? y : pw_zero;
        } else { /* (|x|<1)**-,+inf = inf,0 */
          return (hy < 0) ? -y : pw_zero;
        }
      }
      if (iy === 0x3ff00000) { /* y is  +-1 */
        if (hy < 0) {
          return pw_one / x;
        } else {
          return x;
        }
      }
      if (hy === 0x40000000) return x * x; /* y is  2 */
      if (hy === 0x3fe00000) {             /* y is  0.5 */
        if (hx >= 0) {                     /* x >= +0 */
          return Math.sqrt(x);
        }
      }
    }

    ax = fabs(x);
    /* special value of x */
    if (lx === 0) {
      if (ix === 0x7ff00000 || ix === 0 || ix === 0x3ff00000) {
        z = ax;                       /*x is +-0,+-inf,+-1*/
        if (hy < 0) z = pw_one / z;   /* z = (1/|x|) */
        if (hx < 0) {
          if ((((ix - 0x3ff00000) | 0) | yisint) === 0) {
            /* (-1)**non-int is NaN */
            z = qnan();
          } else if (yisint === 1) {
            z = -z; /* (x<0)**odd = -(|x|**odd) */
          }
        }
        return z;
      }
    }

    n = ((hx >> 31) + 1) | 0;

    /* (x<0)**(non-int) is NaN */
    if ((n | yisint) === 0) {
      return qnan();
    }

    s = pw_one; /* s (sign of result -ve**odd) = -1 else = 1 */
    if ((n | (yisint - 1)) === 0) s = -pw_one; /* (-ve)**(odd int) */

    /* |y| is huge */
    if (iy > 0x41e00000) {   /* if |y| > 2**31 */
      if (iy > 0x43f00000) { /* if |y| > 2**64, must o/uflow */
        if (ix <= 0x3fefffff) return (hy < 0) ? pw_huge * pw_huge : pw_tiny * pw_tiny;
        if (ix >= 0x3ff00000) return (hy > 0) ? pw_huge * pw_huge : pw_tiny * pw_tiny;
      }
      /* over/underflow if x is not close to one */
      if (ix < 0x3fefffff) return (hy < 0) ? s * pw_huge * pw_huge : s * pw_tiny * pw_tiny;
      if (ix > 0x3ff00000) return (hy > 0) ? s * pw_huge * pw_huge : s * pw_tiny * pw_tiny;
      /* now |1-x| is tiny <= 2**-20, suffice to compute
         log(x) by x-x^2/2+x^3/3-x^4/4 */
      t = ax - pw_one; /* t has 20 trailing zeros */
      w = (t * t) * (0.5 - t * (0.3333333333333333333333 - t * 0.25));
      u = pw_ivln2_h * t; /* ivln2_h has 21 sig. bits */
      v = t * pw_ivln2_l - w * pw_ivln2;
      t1 = u + v;
      t1 = setLowWord(t1, 0);
      t2 = v - (t1 - u);
    } else {
      n = 0;
      /* take care subnormal number */
      if (ix < 0x00100000) {
        ax *= pw_two53;
        n = (n - 53) | 0;
        ix = highWord(ax);
      }
      n = (n + (((ix) >> 20) - 0x3ff)) | 0;
      j = ix & 0x000fffff;
      /* determine interval */
      ix = j | 0x3ff00000; /* normalize ix */
      if (j <= 0x3988e) {
        k = 0; /* |x|<sqrt(3/2) */
      } else if (j < 0xbb67a) {
        k = 1; /* |x|<sqrt(3)   */
      } else {
        k = 0;
        n = (n + 1) | 0;
        ix = (ix - 0x00100000) | 0;
      }
      ax = setHighWord(ax, ix);

      /* compute ss = s_h+s_l = (x-1)/(x+1) or (x-1.5)/(x+1.5) */
      u = ax - pw_bp[k]; /* bp[0]=1.0, bp[1]=1.5 */
      v = pw_one / (ax + pw_bp[k]);
      ss = u * v;
      s_h = ss;
      s_h = setLowWord(s_h, 0);
      /* t_h=ax+bp[k] High */
      t_h = pw_zero;
      t_h = setHighWord(t_h, (((ix >> 1) | 0x20000000) + 0x00080000 + (k << 18)) | 0);
      t_l = ax - (t_h - pw_bp[k]);
      s_l = v * ((u - s_h * t_h) - s_h * t_l);
      /* compute log(ax) */
      s2 = ss * ss;
      r = s2 * s2 *
          (pw_L1 + s2 * (pw_L2 + s2 * (pw_L3 + s2 * (pw_L4 + s2 * (pw_L5 + s2 * pw_L6)))));
      r += s_l * (s_h + ss);
      s2 = s_h * s_h;
      t_h = 3.0 + s2 + r;
      t_h = setLowWord(t_h, 0);
      t_l = r - ((t_h - 3.0) - s2);
      /* u+v = ss*(1+...) */
      u = s_h * t_h;
      v = s_l * t_h + t_l * ss;
      /* 2/(3log2)*(ss+...) */
      p_h = u + v;
      p_h = setLowWord(p_h, 0);
      p_l = v - (p_h - u);
      z_h = pw_cp_h * p_h; /* cp_h+cp_l = 2/(3*log2) */
      z_l = pw_cp_l * p_h + p_l * pw_cp + pw_dp_l[k];
      /* log2(ax) = (ss+..)*2/(3*log2) = n + dp_h + z_h + z_l */
      t = n;
      t1 = (((z_h + z_l) + pw_dp_h[k]) + t);
      t1 = setLowWord(t1, 0);
      t2 = z_l - (((t1 - t) - pw_dp_h[k]) - z_h);
    }

    /* split up y into y1+y2 and compute (y1+y2)*(t1+t2) */
    y1 = y;
    y1 = setLowWord(y1, 0);
    p_l = (y - y1) * t1 + y * t2;
    p_h = y1 * t1;
    z = p_l + p_h;
    j = highWord(z);
    i = lowWord(z) | 0;
    if (j >= 0x40900000) {                       /* z >= 1024 */
      if ((((j - 0x40900000) | 0) | i) !== 0) {  /* if z > 1024 */
        return s * pw_huge * pw_huge;            /* overflow */
      } else {
        if (p_l + pw_ovt > z - p_h) return s * pw_huge * pw_huge; /* overflow */
      }
    } else if ((j & 0x7fffffff) >= 0x4090cc00) { /* z <= -1075 */
      if ((((j - 0xc090cc00) | 0) | i) !== 0) {  /* z < -1075 */
        return s * pw_tiny * pw_tiny;            /* underflow */
      } else {
        if (p_l <= z - p_h) return s * pw_tiny * pw_tiny; /* underflow */
      }
    }
    /*
     * compute 2**(p_h+p_l)
     */
    i = j & 0x7fffffff;
    k = ((i >> 20) - 0x3ff) | 0;
    n = 0;
    if (i > 0x3fe00000) { /* if |z| > 0.5, set n = [z+0.5] */
      n = (j + (0x00100000 >> (k + 1))) | 0;
      k = (((n & 0x7fffffff) >> 20) - 0x3ff) | 0; /* new k for n */
      t = pw_zero;
      t = setHighWord(t, n & ~(0x000fffff >> k));
      n = ((n & 0x000fffff) | 0x00100000) >> (20 - k);
      if (j < 0) n = -n | 0;
      p_h -= t;
    }
    t = p_l + p_h;
    t = setLowWord(t, 0);
    u = t * pw_lg2_h;
    v = (p_l - (t - p_h)) * pw_lg2 + t * pw_lg2_l;
    z = u + v;
    w = v - (z - u);
    t = z * z;
    t1 = z - t * (pw_P1 + t * (pw_P2 + t * (pw_P3 + t * (pw_P4 + t * pw_P5))));
    r = (z * t1) / ((t1 - pw_two) - (w + z * w));
    z = pw_one - (r - z);
    j = highWord(z);
    j = (j + (n << 20)) | 0;
    if ((j >> 20) <= 0) {
      z = scalbn(z, n); /* subnormal output */
    } else {
      tmp = highWord(z);
      z = setHighWord(z, (tmp + (n << 20)) | 0);
    }
    return s * z;
  }

  var fdlibm = {
    sin: sin,
    cos: cos,
    tan: tan,
    atan: atan,
    atan2: atan2,
    pow: pow
  };

  if (typeof module !== "undefined" && module.exports) {
    module.exports = fdlibm;
  } else {
    global.__fdlibm = fdlibm;
  }
})(typeof globalThis !== "undefined" ? globalThis : this);
