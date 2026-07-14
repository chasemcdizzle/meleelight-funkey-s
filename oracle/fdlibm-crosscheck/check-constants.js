#!/usr/bin/env node
// check-constants.js — transcription-typo guard for the vendored fdlibm.
//
// The fdlibm sources carry every load-bearing constant as a decimal
// literal followed by a comment with its intended IEEE-754 bit pattern:
//     6.36619772367581382433e-01, /* 0x3FE45F30, 0x6DC9C883 */
// A typo in EITHER the C or the JS copy that both sides happen to share
// would pass the C<->JS bit-exact crosscheck while being silently wrong.
// This script re-parses every such (literal, bit-pattern) pair in BOTH
// vendored files and verifies the decimal literal round-trips to exactly
// the commented bits — killing the shared-transcription-typo class.
//
// Usage: node check-constants.js <file.c-or-js> [...more files]
"use strict";
const fs = require("fs");

const buf = new ArrayBuffer(8);
const f64 = new Float64Array(buf);
const u32 = new Uint32Array(buf);
f64[0] = 1.0;
const HI = u32[1] === 0x3ff00000 ? 1 : 0;
const LO = 1 - HI;

// literal, then a comment containing exactly two 8-hex-digit words
// (with or without 0x), e.g.  /* 0x3FE45F30, 0x6DC9C883 */
const re =
  /(-?\d+\.\d+[eE][+-]?\d+|-?\d+\.\d+)(?:,|;)?\s*(?:\/\*|\/\/)\s*(?:0x)?([0-9A-Fa-f]{8}),\s*(?:0x)?([0-9A-Fa-f]{8})/g;

let checked = 0;
let bad = 0;
for (const file of process.argv.slice(2)) {
  const src = fs.readFileSync(file, "utf8");
  let m;
  while ((m = re.exec(src)) !== null) {
    const lit = Number(m[1]);
    const hi = parseInt(m[2], 16) >>> 0;
    const lo = parseInt(m[3], 16) >>> 0;
    f64[0] = lit;
    checked++;
    if (u32[HI] !== hi || u32[LO] !== lo) {
      bad++;
      const line = src.slice(0, m.index).split("\n").length;
      console.error(
        `${file}:${line}: literal ${m[1]} has bits ` +
          `0x${u32[HI].toString(16).padStart(8, "0")},0x${u32[LO]
            .toString(16)
            .padStart(8, "0")} but comment says 0x${m[2]},0x${m[3]}`
      );
    }
  }
}
if (checked < 80) {
  console.error(`CONSTANTS FAIL: only ${checked} constants matched the ` +
    `pattern — extraction regex broken?`);
  process.exit(1);
}
if (bad > 0) {
  console.error(`CONSTANTS FAIL: ${bad}/${checked} mismatched`);
  process.exit(1);
}
console.log(`CONSTANTS OK: ${checked} literal/bit-pattern pairs verified`);
