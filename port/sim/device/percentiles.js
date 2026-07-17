#!/usr/bin/env node
// port/sim/device/percentiles.js — HOST-side judge of the sim's --timing
// artifact (M3 task 2). The device only RECORDS (sim_main.c buffers
// per-frame CLOCK_MONOTONIC ns in RAM and writes them post-run); all
// judgment happens here on the host, per the M3 convention.
//
// Usage: node percentiles.js <timing.txt> <expectedFrames>
//
// Grammar (HARD-FAILED, exit 3, on any violation — a truncated pull, a
// partial device write, or garbage bytes must never read as a number):
//   exactly <expectedFrames> lines, each ^[0-9]+$ (decimal ns), no
//   blanks, no trailing junk after the final newline.
//
// Percentiles: NEAREST-RANK on the ascending sort — idx = ceil(q*n) - 1
// (n = 3600: p50 idx 1799, p99 idx 3563). Printed as strict key=value
// lines for the no-eval shell parser (the reviewed gparams class):
//   p50_ns / p99_ns (exact integers — the shell threshold compare is
//   integer ns) and p50_ms / p99_ms (3-decimal display strings for the
//   measured table).
"use strict";

const fs = require("fs");

function die(msg) {
  console.error("percentiles: " + msg);
  process.exit(3);
}

const [file, framesArg] = process.argv.slice(2);
if (!file || !framesArg) {
  console.error("usage: node percentiles.js <timing.txt> <expectedFrames>");
  process.exit(1);
}
if (!/^[0-9]+$/.test(framesArg)) die("expectedFrames not a decimal integer: " + framesArg);
const expected = parseInt(framesArg, 10);
if (expected <= 0) die("expectedFrames must be positive");

const raw = fs.readFileSync(file, "utf8");
if (raw.length === 0) die("timing file is empty");
if (raw[raw.length - 1] !== "\n") die("timing file does not end with a newline");
const lines = raw.split("\n");
lines.pop(); // the final newline's empty remainder
if (lines.length !== expected) {
  die("timing file has " + lines.length + " lines, expected " + expected);
}
const ns = new Array(expected);
for (let i = 0; i < expected; i++) {
  if (!/^[0-9]+$/.test(lines[i])) {
    die("line " + (i + 1) + " is not a decimal integer: " + JSON.stringify(lines[i]));
  }
  const v = parseInt(lines[i], 10);
  if (!Number.isSafeInteger(v)) die("line " + (i + 1) + " exceeds the safe integer range");
  ns[i] = v;
}
ns.sort((a, b) => a - b);

function nearestRank(q) {
  return ns[Math.ceil(q * ns.length) - 1];
}
const p50 = nearestRank(0.5);
const p99 = nearestRank(0.99);

console.log("p50_ns=" + String(p50));
console.log("p99_ns=" + String(p99));
console.log("p50_ms=" + (p50 / 1e6).toFixed(3));
console.log("p99_ms=" + (p99 / 1e6).toFixed(3));
