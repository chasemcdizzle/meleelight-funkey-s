#!/usr/bin/env node
// jssweep.js — JS side of the fdlibm crosscheck.
//
// Reads lines "<fn> <hex16> [<hex16>]", applies port/fdlibm/fdlibm.js
// (the exact file the oracle browser harness injects), writes
// "<fn> <hex16> [<hex16>] -> <hex16>" — byte-comparable with csweep.
//
// Usage: node jssweep.js <inputs.txt> > outputs.txt
"use strict";
const fs = require("fs");
const path = require("path");
const fdlibm = require(path.join(__dirname, "..", "..", "port", "fdlibm", "fdlibm.js"));

const file = process.argv[2];
if (!file) { console.error("usage: jssweep.js <inputs.txt>"); process.exit(2); }

const buf = new ArrayBuffer(8);
const f64 = new Float64Array(buf);
const u32 = new Uint32Array(buf);
f64[0] = 1.0;
const HI = u32[1] === 0x3ff00000 ? 1 : 0;
const LO = 1 - HI;
function fromHex(h) {
  u32[HI] = parseInt(h.slice(0, 8), 16) >>> 0;
  u32[LO] = parseInt(h.slice(8, 16), 16) >>> 0;
  return f64[0];
}
function toHex(x) {
  f64[0] = x;
  return ((u32[HI] >>> 0).toString(16).padStart(8, "0") +
          (u32[LO] >>> 0).toString(16).padStart(8, "0"));
}

const out = [];
let n = 0;
for (const line of fs.readFileSync(file, "utf8").split("\n")) {
  if (!line.trim()) continue;
  const parts = line.trim().split(/\s+/);
  const fn = parts[0];
  const impl = fdlibm[fn];
  if (typeof impl !== "function") {
    console.error("unknown fn: " + line);
    process.exit(2);
  }
  let r;
  if (parts.length === 3) {
    r = impl(fromHex(parts[1]), fromHex(parts[2]));
    out.push(`${fn} ${parts[1]} ${parts[2]} -> ${toHex(r)}`);
  } else {
    r = impl(fromHex(parts[1]));
    out.push(`${fn} ${parts[1]} -> ${toHex(r)}`);
  }
  n++;
}
process.stdout.write(out.join("\n") + "\n");
console.error(`jssweep: ${n} evaluations`);
