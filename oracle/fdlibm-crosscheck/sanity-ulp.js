#!/usr/bin/env node
// sanity-ulp.js — gross-error guard for the vendored fdlibm.
//
// The bit-exact C<->JS crosscheck proves the two ports are IDENTICAL, but
// not that they are CORRECT: a transcription error shared by both sides
// would sail through it. This check re-evaluates every sweep line with
// node's native Math (an independent implementation: V8's ieee754 for
// atan/atan2/pow, platform libm trig on most builds) and requires the
// fdlibm result to be within MAX_ULP. Algorithm-variant differences are
// 1-4 ulp; transcription typos are thousands — this catches the class
// without ever becoming an equality gate (the bit-exact compares stay
// the only gates).
//
// Usage: node sanity-ulp.js <c-sweep.txt>
"use strict";
const fs = require("fs");

const MAX_ULP = 16n;

const buf = new ArrayBuffer(8);
const f64 = new Float64Array(buf);
const u32 = new Uint32Array(buf);
const i64 = new BigInt64Array(buf);
f64[0] = 1.0;
const HI = u32[1] === 0x3ff00000 ? 1 : 0;
const LO = 1 - HI;
function fromHex(h) {
  u32[HI] = parseInt(h.slice(0, 8), 16) >>> 0;
  u32[LO] = parseInt(h.slice(8, 16), 16) >>> 0;
  return f64[0];
}
function ulpDiff(a, b) {
  if (Number.isNaN(a) && Number.isNaN(b)) return 0n;
  f64[0] = a;
  let ia = i64[0];
  f64[0] = b;
  let ib = i64[0];
  if (ia < 0n) ia = -9223372036854775808n - ia;
  if (ib < 0n) ib = -9223372036854775808n - ib;
  return ia > ib ? ia - ib : ib - ia;
}

const file = process.argv[2];
if (!file) { console.error("usage: sanity-ulp.js <c-sweep.txt>"); process.exit(2); }

let checked = 0;
let bad = 0;
let worst = 0n;
for (const line of fs.readFileSync(file, "utf8").split("\n")) {
  if (!line.trim()) continue;
  // "<fn> <hex> [<hex>] -> <hex>"
  const m = line.trim().split(/\s+/);
  const fn = m[0];
  const twoArg = m.length === 5;
  const fdOut = fromHex(m[m.length - 1]);
  const native = twoArg
    ? Math[fn](fromHex(m[1]), fromHex(m[2]))
    : Math[fn](fromHex(m[1]));
  const d = ulpDiff(fdOut, native);
  if (d > worst) worst = d;
  checked++;
  if (d > MAX_ULP) {
    bad++;
    if (bad <= 10) console.error(`ULP ${d}: ${line}`);
  }
}
if (bad > 0) {
  console.error(`SANITY FAIL: ${bad}/${checked} beyond ${MAX_ULP} ulp of native Math`);
  process.exit(1);
}
console.log(`SANITY OK: ${checked} results within ${MAX_ULP} ulp of native Math (worst ${worst})`);
