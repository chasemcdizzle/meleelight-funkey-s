#!/usr/bin/env node
// trace-to-txt.js — M2 task 17: convert a golden input trace
// (oracle/goldens/*.trace.json) to the integrated C sim's line-oriented
// input format. Pure text transform, no game knowledge.
//
// Usage: node trace-to-txt.js <trace.json> <out.txt>
//
// Input contract (HARD-FAILED, exit 3, on any violation): the trace is an
// array of frames; each frame is an array of exactly 4 entries; each entry
// is null OR an Input object with EXACTLY these 22 keys —
//   booleans: a b x y z r l s du dr dd dl
//   numbers:  lsX lsY csX csY lA rA rawX rawY rawcsX rawcsY
// No other keys, no other types, no missing keys.
//
// Output: one line per frame; the 4 slots joined by '|'; a null slot is
// the empty string; a present slot is 22 space-separated tokens — the 12
// booleans as 0/1 in the exact order a b x y z r l s du dr dd dl, then
// the 10 numbers as 16-lowercase-hex-digit big-endian IEEE-754 double bit
// patterns in the exact order lsX lsY csX csY lA rA rawX rawY rawcsX
// rawcsY. Bit patterns, not decimals — the C side must read back the
// EXACT doubles the oracle consumed (-0, denormals and all).
"use strict";

const fs = require("fs");

const BOOLS = ["a", "b", "x", "y", "z", "r", "l", "s",
               "du", "dr", "dd", "dl"];
const NUMS = ["lsX", "lsY", "csX", "csY", "lA", "rA",
              "rawX", "rawY", "rawcsX", "rawcsY"];

function die(msg) {
  console.error("trace-to-txt: " + msg);
  process.exit(3);
}

const [tracePath, outPath] = process.argv.slice(2);
if (!tracePath || !outPath) {
  console.error("usage: node trace-to-txt.js <trace.json> <out.txt>");
  process.exit(1);
}

let trace;
try {
  trace = JSON.parse(fs.readFileSync(tracePath, "utf8"));
} catch (e) {
  die("cannot read/parse " + tracePath + ": " + e.message);
}
if (!Array.isArray(trace)) die("trace root is not an array");

// big-endian IEEE-754 double bit pattern, 16 lowercase hex digits
const dbuf = Buffer.alloc(8);
function dhex(x) {
  dbuf.writeDoubleBE(x, 0);
  return dbuf.toString("hex");
}

const lines = [];
for (let f = 0; f < trace.length; f++) {
  const frame = trace[f];
  if (!Array.isArray(frame) || frame.length !== 4) {
    die("frame " + f + " is not an array of exactly 4 entries");
  }
  const slots = [];
  for (let s = 0; s < 4; s++) {
    const e = frame[s];
    if (e === null) {
      slots.push("");
      continue;
    }
    if (typeof e !== "object" || Array.isArray(e)) {
      die("frame " + f + " slot " + s + " is neither null nor an object");
    }
    const keys = Object.keys(e);
    if (keys.length !== 22) {
      die("frame " + f + " slot " + s + " has " + keys.length +
          " keys, expected exactly 22");
    }
    for (const k of BOOLS) {
      if (!Object.prototype.hasOwnProperty.call(e, k)) {
        die("frame " + f + " slot " + s + " missing key " + k);
      }
      if (typeof e[k] !== "boolean") {
        die("frame " + f + " slot " + s + " key " + k +
            " is not a boolean (got " + typeof e[k] + ")");
      }
    }
    for (const k of NUMS) {
      if (!Object.prototype.hasOwnProperty.call(e, k)) {
        die("frame " + f + " slot " + s + " missing key " + k);
      }
      if (typeof e[k] !== "number") {
        die("frame " + f + " slot " + s + " key " + k +
            " is not a number (got " + typeof e[k] + ")");
      }
    }
    // 22 own keys + all 22 expected keys present => the key SETS are equal
    const toks = [];
    for (const k of BOOLS) toks.push(e[k] ? "1" : "0");
    for (const k of NUMS) toks.push(dhex(e[k]));
    slots.push(toks.join(" "));
  }
  lines.push(slots.join("|"));
}

fs.writeFileSync(outPath, lines.join("\n") + "\n");
console.log("trace-to-txt: " + trace.length + " frames -> " + outPath);
