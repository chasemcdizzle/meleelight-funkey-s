#!/usr/bin/env node
// judge-s1-coverage.js — S1 live-session recorded-trace judge (M3 task 5).
//
//   node judge-s1-coverage.js <recorded-trace.json> <frames>
//
// Judges the PULLED recorded input trace of the scripted uinput live
// session (host-side; the device never self-reports):
//   1. STRICT SHAPE: array of exactly <frames> frames, each
//      [row0, row1, null, null]; rows carry exactly the 22 golden-trace
//      Input keys with boolean/number types (trace-to-txt.js's contract,
//      asserted here BEFORE any replay is attempted).
//   2. SLOT-1 rows are all-neutral (the live app injects a neutral
//      human row for P2 every frame).
//   3. S1 INVARIANTS on every slot-0 row: y/z/l/du/dl/dr/dd never true,
//      lA === 0, rA === 1 exactly when r else 0, ls*/cs* === their raw
//      twins (every S1 table value clears the 0.28 deadzone).
//   4. COVERAGE: every pre-registered chord signature from the
//      committed s1-session.script appears in >= MIN_FRAMES slot-0
//      rows (values, never frame indices — the settling strategy; a
//      dropped injector event = a missing/short signature, LOUD).
// Exact === comparisons: JSON.parse returns the exact doubles the C
// recorder serialized via String(x); the literals below are the same
// doubles (1/80-grid values round-trip decimal->double identically).
//
// Exit 0 + "S1 COVERAGE OK ..." only if everything holds.
"use strict";

const fs = require("fs");

function die(msg) {
  console.error("judge-s1-coverage: " + msg);
  process.exit(3);
}

const [tracePath, framesArg] = process.argv.slice(2);
if (!tracePath || !framesArg || !/^[0-9]+$/.test(framesArg)) {
  console.error("usage: node judge-s1-coverage.js <trace.json> <frames>");
  process.exit(1);
}
const FRAMES = parseInt(framesArg, 10);
const MIN_FRAMES = 5; // every chord is held 15-18 frames; 5 tolerates
                      // heavy boundary jitter without ever passing on
                      // a single stray sample

const BOOLS = ["a", "b", "x", "y", "z", "r", "l", "s",
               "du", "dr", "dd", "dl"];
const NUMS = ["lsX", "lsY", "csX", "csY", "lA", "rA",
              "rawX", "rawY", "rawcsX", "rawcsY"];

let trace;
try {
  trace = JSON.parse(fs.readFileSync(tracePath, "utf8"));
} catch (e) {
  die("cannot read/parse " + tracePath + ": " + e.message);
}
if (!Array.isArray(trace)) die("trace root is not an array");
if (trace.length !== FRAMES) {
  die("trace has " + trace.length + " frames, expected exactly " + FRAMES);
}

function checkRow(f, s, e) {
  if (typeof e !== "object" || e === null || Array.isArray(e)) {
    die("frame " + f + " slot " + s + " is not an Input object");
  }
  if (Object.keys(e).length !== 22) {
    die("frame " + f + " slot " + s + " has " + Object.keys(e).length +
        " keys, expected exactly 22");
  }
  for (const k of BOOLS) {
    if (typeof e[k] !== "boolean") {
      die("frame " + f + " slot " + s + " key " + k + " not a boolean");
    }
  }
  for (const k of NUMS) {
    if (typeof e[k] !== "number" || !isFinite(e[k])) {
      die("frame " + f + " slot " + s + " key " + k + " not a finite number");
    }
  }
}

// --- coverage signatures (pre-registered; s1-session.script plan) -----------
const SIGS = [
  ["dash-right lsX=1",            (i) => i.lsX === 1 && i.lsY === 0 && !i.y],
  ["dash-left lsX=-1",            (i) => i.lsX === -1 && i.lsY === 0],
  ["plain-up lsY=1",              (i) => i.lsX === 0 && i.lsY === 1],
  ["walk-right 0.6625",           (i) => i.lsX === 0.6625 && i.lsY === 0],
  ["walk-left -0.6625",           (i) => i.lsX === -0.6625 && i.lsY === 0],
  ["u-tilt 0.5375",               (i) => i.lsX === 0 && i.lsY === 0.5375],
  ["d-tilt -0.5375",              (i) => i.lsX === 0 && i.lsY === -0.5375],
  ["plain-diag (0.7,0.7)",        (i) => i.lsX === 0.7 && i.lsY === 0.7 && !i.r],
  ["R+up-diag (0.7,0.7)+shield",  (i) => i.lsX === 0.7 && i.lsY === 0.7 && i.r],
  ["L-diag (0.7375,0.3125)",      (i) => i.lsX === 0.7375 && i.lsY === 0.3125],
  ["wavedash-left (-0.6375,-0.375)", (i) => i.lsX === -0.6375 && i.lsY === -0.375 && i.r],
  ["wavedash-right (0.6375,-0.375)", (i) => i.lsX === 0.6375 && i.lsY === -0.375 && i.r],
  ["shield-drop-right (0.7,-0.6875)", (i) => i.lsX === 0.7 && i.lsY === -0.6875 && i.r],
  ["shield-drop-left (-0.7,-0.6875)", (i) => i.lsX === -0.7 && i.lsY === -0.6875 && i.r],
  ["spotdodge (0,-1)+shield",     (i) => i.lsX === 0 && i.lsY === -1 && i.r],
  ["shield r/rA=1",               (i) => i.r === true && i.rA === 1],
  ["clayer-right csX=1 ls neutral", (i) => i.csX === 1 && i.lsX === 0 && i.lsY === 0],
  ["clayer-left csX=-1",          (i) => i.csX === -1 && i.lsX === 0 && i.lsY === 0],
  ["clayer-up csY=1",             (i) => i.csY === 1 && i.lsX === 0 && i.lsY === 0],
  ["clayer-down csY=-1",          (i) => i.csY === -1 && i.lsX === 0 && i.lsY === 0],
  ["clayer-diag (-0.7,-0.7)",     (i) => i.csX === -0.7 && i.csY === -0.7],
  ["attack a",                    (i) => i.a === true],
  ["special b",                   (i) => i.b === true],
  ["jump x",                      (i) => i.x === true],
];
const counts = new Array(SIGS.length).fill(0);

let neutralSlot1 = 0;
let invBad = 0;
for (let f = 0; f < trace.length; f++) {
  const frame = trace[f];
  if (!Array.isArray(frame) || frame.length !== 4 ||
      frame[2] !== null || frame[3] !== null) {
    die("frame " + f + " is not [row0,row1,null,null]");
  }
  checkRow(f, 0, frame[0]);
  checkRow(f, 1, frame[1]);
  const p1 = frame[1];
  const p1Neutral = BOOLS.every((k) => p1[k] === false) &&
                    NUMS.every((k) => p1[k] === 0);
  if (p1Neutral) neutralSlot1++;
  else die("frame " + f + " slot 1 is not the neutral human row");
  const i = frame[0];
  // S1 invariants
  if (i.y || i.z || i.l || i.du || i.dl || i.dr || i.dd) invBad++;
  if (i.lA !== 0) invBad++;
  if (i.r ? i.rA !== 1 : i.rA !== 0) invBad++;
  if (i.lsX !== i.rawX || i.lsY !== i.rawY ||
      i.csX !== i.rawcsX || i.csY !== i.rawcsY) invBad++;
  for (let s = 0; s < SIGS.length; s++) {
    if (SIGS[s][1](i)) counts[s]++;
  }
}
if (invBad) die("S1 invariant violations on " + invBad + " slot-0 rows");

let fail = 0;
for (let s = 0; s < SIGS.length; s++) {
  const tag = counts[s] >= MIN_FRAMES ? "ok  " : "FAIL";
  if (counts[s] < MIN_FRAMES) fail = 1;
  console.log(tag + " " + SIGS[s][0] + ": " + counts[s] + " frames" +
              (counts[s] < MIN_FRAMES ? " (< " + MIN_FRAMES + ")" : ""));
}
if (fail) {
  console.error("S1 COVERAGE FAIL: missing/short chord signatures — " +
                "dropped injector events or a dead SDL input path");
  process.exit(2);
}
console.log("S1 COVERAGE OK (" + SIGS.length + "/" + SIGS.length +
            " signatures >= " + MIN_FRAMES + " frames each, " +
            trace.length + " frames, slot-1 neutral " + neutralSlot1 +
            "/" + trace.length + ")");
