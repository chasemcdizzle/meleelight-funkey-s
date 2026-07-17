#!/usr/bin/env node
// port/gfx/judge-render-timing.js — HOST-side judge of gfx_app's
// --timing artifact (M3 task 4). The device only RECORDS (gfx_app.c
// buffers per-frame CLOCK_MONOTONIC ns in RAM and writes them post-run
// to tmpfs); all judgment happens here on the host, per the M3
// convention. percentiles.js's strict-grammar / nearest-rank precedent,
// extended to the 4-column render rows.
//
// Usage: node judge-render-timing.js <timing.txt> <expectedFrames>
//
// Grammar (HARD-FAILED, exit 3, on any violation — a truncated pull, a
// partial device write, or garbage bytes must never read as numbers):
//   exactly <expectedFrames> lines, each
//     ^<sim> <render> <present> <skipped>$
//   with sim/render/present decimal ns and skipped 0|1; a skipped row
//   MUST carry render == 0 and present == 0 (the app skips BOTH), and a
//   rendered row MUST carry render > 0 (a zero-cost render is a recording
//   bug, not a measurement).
//
// Populations (pre-registered, AGENT-LOG iter 50):
//   full   = sim+render+present over ALL frames (work time; pacing sleep
//            is excluded at the recorder);
//   render = render ns over RENDERED frames only (a skipped frame has no
//            render cost to measure; the skip COUNT is reported so skips
//            can never hide);
//   sim / present likewise reported for the attribution split.
// Percentiles: NEAREST-RANK on the ascending sort (idx = ceil(q*n)-1).
// Output: strict key=value lines for the no-eval shell parser.
"use strict";

const fs = require("fs");

function die(msg) {
  console.error("judge-render-timing: " + msg);
  process.exit(3);
}

const [file, framesArg] = process.argv.slice(2);
if (!file || !framesArg) {
  console.error("usage: node judge-render-timing.js <timing.txt> <expectedFrames>");
  process.exit(1);
}
if (!/^[0-9]+$/.test(framesArg)) die("expectedFrames not a decimal integer: " + framesArg);
const expected = parseInt(framesArg, 10);
if (expected <= 0) die("expectedFrames must be positive");

const raw = fs.readFileSync(file, "utf8");
if (raw.length === 0) die("timing file is empty");
if (raw[raw.length - 1] !== "\n") die("timing file does not end with a newline");
const lines = raw.split("\n");
lines.pop();
if (lines.length !== expected) {
  die("timing file has " + lines.length + " lines, expected " + expected);
}

const full = new Array(expected);
const sim = new Array(expected);
const present = [];
const render = [];
let skips = 0;
const ROW = /^([0-9]+) ([0-9]+) ([0-9]+) ([01])$/;
for (let i = 0; i < expected; i++) {
  const m = ROW.exec(lines[i]);
  if (!m) die("line " + (i + 1) + " malformed: " + JSON.stringify(lines[i]));
  const s = parseInt(m[1], 10), r = parseInt(m[2], 10), p = parseInt(m[3], 10);
  if (!Number.isSafeInteger(s) || !Number.isSafeInteger(r) || !Number.isSafeInteger(p)) {
    die("line " + (i + 1) + " exceeds the safe integer range");
  }
  const skipped = m[4] === "1";
  if (skipped) {
    if (r !== 0 || p !== 0) die("line " + (i + 1) + " skipped but render/present nonzero");
    skips++;
  } else {
    if (r === 0) die("line " + (i + 1) + " rendered but render == 0 (recording bug)");
    render.push(r);
    present.push(p);
  }
  sim[i] = s;
  full[i] = s + r + p;
}
if (render.length + skips !== expected) die("population bookkeeping error");
if (render.length === 0) die("every frame was skipped — nothing was rendered (a live render run cannot pass this way)");

function pcts(arr) {
  const a = arr.slice().sort((x, y) => x - y);
  const nr = (q) => a[Math.ceil(q * a.length) - 1];
  return { p50: nr(0.5), p99: nr(0.99), max: a[a.length - 1] };
}
const F = pcts(full), S = pcts(sim), R = pcts(render), P = pcts(present);

function emit(tag, v) {
  console.log(tag + "_ns=" + String(v));
  console.log(tag + "_ms=" + (v / 1e6).toFixed(3));
}
emit("full_p50", F.p50);
emit("full_p99", F.p99);
emit("full_max", F.max);
emit("sim_p50", S.p50);
emit("sim_p99", S.p99);
emit("render_p50", R.p50);
emit("render_p99", R.p99);
emit("render_max", R.max);
emit("present_p50", P.p50);
emit("present_p99", P.p99);
console.log("skips=" + String(skips));
console.log("rendered=" + String(render.length));
// judge_complete integrity terminator (iter 62 — the iter-61
// audio-judge pattern, class completion): emitted ONLY after every
// verdict line above, so truncated/partial judge output can never end
// with it; the shell consumers assert it on the output FILE's BYTES
// (must END with exactly 'judge_complete=1\n' — trailing blank lines
// violate the grammar as written).
console.log("judge_complete=1");
