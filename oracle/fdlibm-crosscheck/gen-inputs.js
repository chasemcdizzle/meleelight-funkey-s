#!/usr/bin/env node
// gen-inputs.js — deterministic input sweep for the fdlibm C<->JS crosscheck.
//
// Emits lines "<fn> <hex16> [<hex16>]" (IEEE-754 bit patterns of the
// argument(s)). The set is (a) every special value / algorithm-branch
// threshold in the vendored fdlibm sources, densely sampled (bits +-1/+-2,
// both signs, varied low words), and (b) seeded-PRNG sweeps over the
// ranges the sim actually exercises plus full-exponent-range patterns.
// Deterministic: same file every run (seeded mulberry32, no Date/random).
//
// Usage: node gen-inputs.js <out.txt>
"use strict";
const fs = require("fs");

const out = process.argv[2];
if (!out) { console.error("usage: gen-inputs.js <out.txt>"); process.exit(1); }

// --- deterministic PRNG (mulberry32, same as the oracle harness) --------
function mulberry32(a) {
  return function () {
    a |= 0;
    a = (a + 0x6d2b79f5) | 0;
    let t = Math.imul(a ^ (a >>> 15), 1 | a);
    t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t;
    return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
  };
}
const rng = mulberry32(0xf00dcafe);
const r32 = () => (rng() * 4294967296) >>> 0;

// --- bit helpers ---------------------------------------------------------
const buf = new ArrayBuffer(8);
const f64 = new Float64Array(buf);
const u32 = new Uint32Array(buf);
f64[0] = 1.0;
const HI = u32[1] === 0x3ff00000 ? 1 : 0;
const LO = 1 - HI;
function bitsOf(x) {
  f64[0] = x;
  return ((u32[HI] >>> 0).toString(16).padStart(8, "0") +
          (u32[LO] >>> 0).toString(16).padStart(8, "0"));
}
function fromWords(hi, lo) {
  u32[HI] = hi; u32[LO] = lo;
  return f64[0];
}
function hex(hi, lo) {
  return ((hi >>> 0).toString(16).padStart(8, "0") +
          (lo >>> 0).toString(16).padStart(8, "0"));
}
function nudge(x, n) { // x with its bit pattern shifted by n ulps (int64-ish)
  f64[0] = x;
  let lo = u32[LO] >>> 0, hi = u32[HI] >>> 0;
  for (let i = 0; i < Math.abs(n); i++) {
    if (n > 0) { lo = (lo + 1) >>> 0; if (lo === 0) hi = (hi + 1) >>> 0; }
    else { if (lo === 0) hi = (hi - 1) >>> 0; lo = (lo - 1) >>> 0; }
  }
  u32[LO] = lo; u32[HI] = hi;
  return f64[0];
}

// --- assemble the value pools -------------------------------------------
const lines = [];
function emit1(fn, x) { lines.push(fn + " " + bitsOf(x)); }
function emit2(fn, a, b) { lines.push(fn + " " + bitsOf(a) + " " + bitsOf(b)); }

// specials
const specials = [
  fromWords(0x00000000, 0), fromWords(0x80000000, 0),           // +-0
  fromWords(0x00000000, 1), fromWords(0x80000000, 1),           // min denormal
  fromWords(0x000fffff, 0xffffffff),                            // max denormal
  fromWords(0x00100000, 0),                                     // min normal
  fromWords(0x7fefffff, 0xffffffff),                            // max double
  fromWords(0x7ff00000, 0), fromWords(0xfff00000, 0),           // +-Inf
  fromWords(0x7ff80000, 0),                                     // canonical qNaN
  fromWords(0x7ff4dead, 0xbeef0001),                            // payload sNaN
  fromWords(0xfffdead0, 0xcafe0002),                            // payload -qNaN
  1.0, -1.0, 0.5, -0.5, 1.5, -1.5, 2.0, -2.0, 3.0, -3.0,
  Math.PI / 4, Math.PI / 2, Math.PI, (3 * Math.PI) / 4, 2 * Math.PI,
  -Math.PI / 4, -Math.PI / 2, -Math.PI, -2 * Math.PI,
];

// fdlibm branch-threshold HIGH words (from the vendored sources)
const trigHW = [
  0x3e300000, 0x3e400000,             // tiny cutoffs (tan / sin+cos+atan)
  0x3fd33333, 0x3fdc0000, 0x3fe59428, // kernel_cos 0.3 / atan 0.4375 / kernel_tan 0.6744
  0x3fe60000, 0x3fe90000, 0x3fe921fb, // atan 11/16 / kernel_cos 0.78125 / pi/4
  0x3ff30000, 0x3ff921fb,             // atan 19/16 / pi/2
  0x40038000, 0x4002d97c,             // atan 39/16 / 3pi/4
  0x413921fb,                         // rem_pio2 medium/large cutoff 2^19*pi/2
  0x44100000,                         // atan 2^66
  0x7fe00000, 0x7ff00000,             // huge / inf boundary
];
// npio2_hw: high words of k*pi/2 (worst cancellation in medium reduction)
const npio2 = [
  0x3ff921fb, 0x400921fb, 0x4012d97c, 0x401921fb, 0x401f6a7a, 0x4022d97c,
  0x4025fdbb, 0x402921fb, 0x402c463a, 0x402f6a7a, 0x4031475c, 0x4032d97c,
  0x40346b9c, 0x4035fdbb, 0x40378fdb, 0x403921fb, 0x403ab41b, 0x403c463a,
  0x403dd85a, 0x403f6a7a, 0x40407e4c, 0x4041475c, 0x4042106c, 0x4042d97c,
  0x4043a28c, 0x40446b9c, 0x404534ac, 0x4045fdbb, 0x4046c6cb, 0x40478fdb,
  0x404858eb, 0x404921fb,
];

const trigPool = [...specials];
for (const hw of [...trigHW, ...npio2]) {
  for (const lo of [0, 1, 0x54442d18, 0xffffffff, r32()]) {
    for (const sign of [0, 0x80000000]) {
      trigPool.push(fromWords((hw | sign) >>> 0, lo >>> 0));
      trigPool.push(nudge(fromWords((hw | sign) >>> 0, lo >>> 0), 1));
      trigPool.push(nudge(fromWords((hw | sign) >>> 0, lo >>> 0), -1));
    }
  }
}
// nearest doubles to k*pi/2 (dense cancellation near multiples), k to 400
for (let k = 1; k <= 400; k++) {
  const v = (k * Math.PI) / 2;
  for (const n of [-2, -1, 0, 1, 2]) {
    trigPool.push(nudge(v, n));
    trigPool.push(nudge(-v, n));
  }
}
// Payne-Hanek territory: full exponent spread incl. the classic worst cases
for (let e = 0x414; e <= 0x7fe; e += 7) {
  trigPool.push(fromWords(((e << 20) | (r32() & 0xfffff)) >>> 0, r32()));
}
trigPool.push(fromWords(0x41d92c4e, 0x8a496ff0)); // ~1.68e9, near k*pi/2
trigPool.push(5e9, 1e15, 1e22, 1e300, 6381956970095103 * Math.pow(2, 797));

// random sim-range sweeps
const N_TRIG_RAND = 20000;
for (let i = 0; i < N_TRIG_RAND; i++) {
  trigPool.push((rng() - 0.5) * 4 * Math.PI);          // one rotation
  trigPool.push((rng() - 0.5) * 2000);                  // game coordinates
  if (i < 4000) trigPool.push(fromWords(r32(), r32())); // raw 64-bit patterns
}

for (const v of trigPool) {
  emit1("sin", v); emit1("cos", v); emit1("tan", v); emit1("atan", v);
}

// atan2: edge x edge cross + randoms
const edge2 = [
  fromWords(0, 0), fromWords(0x80000000, 0),
  fromWords(0x7ff00000, 0), fromWords(0xfff00000, 0),
  fromWords(0x7ff80000, 0),
  fromWords(0, 1), fromWords(0x000fffff, 0xffffffff),
  fromWords(0x7fefffff, 0xffffffff),
  1.0, -1.0, 1.5, -0.5, Math.PI, -Math.PI, 1e-300, -1e-300, 1e300, -1e300,
  0.7071067811865476, 123.456, -987.654,
];
for (const y of edge2) for (const x of edge2) emit2("atan2", y, x);
emit2("atan2", 3.5, 1.0);                       // x == 1.0 fast path
emit2("atan2", nudge(1.0, 1), 1.0);
for (let i = 0; i < 20000; i++) {
  emit2("atan2", (rng() - 0.5) * 2000, (rng() - 0.5) * 2000); // game vectors
  if (i < 3000) {
    emit2("atan2", (rng() - 0.5) * 1e-280, (rng() - 0.5) * 1e60);  // k<-60
    emit2("atan2", (rng() - 0.5) * 1e60, (rng() - 0.5) * 1e-280);  // k>60
    emit2("atan2", fromWords(r32(), r32()), fromWords(r32(), r32()));
  }
}

// pow: special x special cross + boundary-targeted + randoms
const powX = [
  fromWords(0, 0), fromWords(0x80000000, 0),
  fromWords(0x7ff00000, 0), fromWords(0xfff00000, 0), fromWords(0x7ff80000, 0),
  1.0, -1.0, nudge(1.0, 1), nudge(1.0, -1),      // |1-x| tiny (huge-y path)
  1.0 + Math.pow(2, -21), 1.0 - Math.pow(2, -21),
  1.5, -1.5, 2.0, -2.0, 0.5, 9.999,
  fromWords(0, 1), fromWords(0x000fffff, 0xffffffff), // denormals
  fromWords(0x00100000, 0),
  fromWords(0x3ff3988e, 0), fromWords(0x3ff3988f, 0), // sqrt(3/2) interval j-bound
  fromWords(0x3ffbb67a, 0), fromWords(0x3ffbb679, 0), // sqrt(3) interval j-bound
  fromWords(0x7fefffff, 0xffffffff),
  Math.E, 10.0, -7.0,
];
const powY = [
  fromWords(0, 0), fromWords(0x80000000, 0),
  fromWords(0x7ff00000, 0), fromWords(0xfff00000, 0), fromWords(0x7ff80000, 0),
  1.0, -1.0, 2.0, -2.0, 0.5, -0.5, 3.0, -3.0, 4.0, 5.0, 0.75, -0.75,
  1.4, 2.5, -2.5, 100.0, -100.0, 1023.5, -1074.5,
  9007199254740991, 9007199254740992, 9007199254740994,   // 2^53-1, 2^53, 2^53+2
  4503599627370495.5, 4503599627370496,                    // 2^52-.5, 2^52
  2147483647, 2147483648, -2147483648,                     // 2^31 boundary
  1.8446744073709552e19,                                   // 2^64 boundary
  fromWords(0x41e00000, 0), nudge(fromWords(0x41e00000, 0), 1),
  fromWords(0x43f00000, 0), nudge(fromWords(0x43f00000, 0), 1),
];
for (const x of powX) for (const y of powY) emit2("pow", x, y);
// overflow/underflow z-boundary: x=2, y dense around +-1024/-1075
for (const yc of [1024, -1075, 1023.9999999, -1074.9999999]) {
  for (let n = -40; n <= 40; n++) emit2("pow", 2.0, nudge(yc, n));
}
// sim-typical: knockback-style pow(base in (0,20), frac exponents)
for (let i = 0; i < 20000; i++) {
  emit2("pow", rng() * 20, (rng() - 0.5) * 8);
  if (i < 4000) {
    emit2("pow", -(rng() * 20), Math.floor((rng() - 0.5) * 40)); // neg^int
    emit2("pow", fromWords(r32(), r32()), fromWords(r32(), r32()));
  }
}

fs.writeFileSync(out, lines.join("\n") + "\n");
console.log(`${out}: ${lines.length} sweep inputs`);
