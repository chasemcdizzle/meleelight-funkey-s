#!/usr/bin/env node
// judge-s1-coverage.js — S1 live-session recorded-trace judge (M3 task 5;
// hardened iter 53, review-51 M2/M4/L1).
//
//   node judge-s1-coverage.js <recorded-trace.json> <frames> <keys-sidecar>
//
// Judges the PULLED recorded input trace + raw-keysym sidecar of the
// scripted uinput live session (host-side; the device never
// self-reports):
//   1. RAW WHITELIST GRAMMAR (before any JSON.parse — the duplicate-key
//      class dies here, review-51 M2): the trace file must be EXACTLY
//      line 0 `[`, lines 1..N one frame each matching the recorder's
//      anchored full-line grammar (two 22-key rows in gfx_app.c's fixed
//      rec_input key order, `,null,null]`, trailing comma except the
//      last frame), final line `]`, trailing newline. Numbers must
//      match the String(x) grammar. Duplicate keys, reordered keys,
//      whitespace, ANY resemblance-but-not-match = corruption death.
//   2. STRICT SHAPE (post-parse): exactly <frames> frames, each
//      [row0, row1, null, null]; rows carry exactly the 22 golden-trace
//      Input keys with boolean/number types.
//   3. SLOT-1 rows are all-neutral (the live app injects a neutral
//      human row for P2 every frame).
//   4. S1 INVARIANTS on every slot-0 row: y/z/l/du/dl/dr/dd never true,
//      lA === 0 unless physical X (D34), rA === 1 exactly when r else 0,
//      ls*/cs* === their raw
//      twins (every S1 table value clears the 0.28 deadzone).
//   5. RAW-KEY SIDECAR (the SOCD live witness, review-51 M4): exactly
//      <frames> lines each EXACTLY 4 lowercase hex digits (gfx_app.c's
//      --record-keys producer — bit layout paired there: bit0 up,
//      bit1 down, bit2 left, bit3 right, bit4 a, bit5 b, bit6 x,
//      bit7 y, bit8 start, bit9 l, bit10 r, bit11 menu, bit12 quit).
//      Pairing fidelity: the a/b/x/start bits must equal the trace
//      bools frame-by-frame (same pin, same loop iteration — a
//      mismatch means the sidecar is not this session's). UNIVERSAL
//      SOCD invariants: raw left+right both held => lsX===0 AND
//      csX===0 (an SDL last-key-wins regression emits ±1 there —
//      death); raw up+down => lsY===0 AND csY===0. Coverage: both SOCD
//      pairs must be observed >= MIN_FRAMES frames (the committed
//      script holds each 300 ms).
//   6. COVERAGE: every pre-registered chord signature from the
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

const [tracePath, framesArg, keysPath] = process.argv.slice(2);
if (!tracePath || !framesArg || !/^[0-9]+$/.test(framesArg) || !keysPath) {
  console.error("usage: node judge-s1-coverage.js <trace.json> <frames> <keys-sidecar>");
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

// --- 1. RAW whitelist grammar (review-51 M2; PROCESS §3) ---------------------
// Built from the SAME key arrays the shape checks use — one source of
// truth for key set AND order (the recorder's rec_input order is
// BOOLS then NUMS exactly). Number grammar = ECMAScript String(x)
// output (ml_sb_num): -?(0|[1-9]\d*)(\.\d+)?(e[+-]\d+)?
const NUM_RE = "-?(?:0|[1-9][0-9]*)(?:\\.[0-9]+)?(?:e[+-][0-9]+)?";
const ROW_RE =
  "\\{" +
  BOOLS.map((k) => '"' + k + '":(?:true|false)').join(",") +
  "," +
  NUMS.map((k) => '"' + k + '":' + NUM_RE).join(",") +
  "\\}";
const FRAME_LINE_RE = new RegExp("^\\[" + ROW_RE + "," + ROW_RE +
                                 ",null,null\\]$");

let raw;
try {
  raw = fs.readFileSync(tracePath, "utf8");
} catch (e) {
  die("cannot read " + tracePath + ": " + e.message);
}
if (raw.length === 0 || raw[raw.length - 1] !== "\n") {
  die("trace file does not end with a newline (truncated write?)");
}
const rawLines = raw.split("\n");
rawLines.pop(); // the trailing-newline empty tail
if (rawLines.length !== FRAMES + 2) {
  die("trace has " + rawLines.length + " lines, expected exactly " +
      (FRAMES + 2) + " ('[' + " + FRAMES + " frame lines + ']')");
}
if (rawLines[0] !== "[") die("trace line 1 is not exactly '['");
if (rawLines[FRAMES + 1] !== "]") {
  die("trace last line is not exactly ']'");
}
for (let f = 0; f < FRAMES; f++) {
  let line = rawLines[f + 1];
  const wantComma = f < FRAMES - 1;
  if (wantComma) {
    if (line[line.length - 1] !== ",") {
      die("frame " + f + " line missing its trailing comma");
    }
    line = line.slice(0, -1);
  }
  if (!FRAME_LINE_RE.test(line)) {
    die("frame " + f + " line does not match the recorder grammar " +
        "(duplicate/reordered keys or corruption): " +
        JSON.stringify(line.slice(0, 120)));
  }
}

let trace;
try {
  trace = JSON.parse(raw);
} catch (e) {
  die("cannot parse " + tracePath + ": " + e.message);
}
if (!Array.isArray(trace)) die("trace root is not an array");
if (trace.length !== FRAMES) {
  die("trace has " + trace.length + " frames, expected exactly " + FRAMES);
}

// --- raw-key sidecar (review-51 M4) ------------------------------------------
let keysRaw;
try {
  keysRaw = fs.readFileSync(keysPath, "utf8");
} catch (e) {
  die("cannot read keys sidecar " + keysPath + ": " + e.message);
}
if (keysRaw.length === 0 || keysRaw[keysRaw.length - 1] !== "\n") {
  die("keys sidecar does not end with a newline (truncated write?)");
}
const keyLines = keysRaw.split("\n");
keyLines.pop();
if (keyLines.length !== FRAMES) {
  die("keys sidecar has " + keyLines.length + " lines, expected exactly " +
      FRAMES);
}
const KEY_RE = /^[0-9a-f]{4}$/;
const rawKeys = new Array(FRAMES);
for (let f = 0; f < FRAMES; f++) {
  if (!KEY_RE.test(keyLines[f])) {
    die("keys sidecar line " + (f + 1) +
        " is not exactly 4 lowercase hex digits: " +
        JSON.stringify(keyLines[f]));
  }
  rawKeys[f] = parseInt(keyLines[f], 16);
}
// bit layout — PAIRED with gfx_app.c's rawKeys producer
const K_UP = 1, K_DOWN = 2, K_LEFT = 4, K_RIGHT = 8;
const K_A = 16, K_B = 32, K_X = 64, K_Y = 128, K_START = 256;

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
// review-51 L1: a cs signature ALSO requires the left stick neutral (a
// glue regression emitting correct cs alongside stale nonzero ls must
// not satisfy it).
//
// RE-PINNED 2026-08-24 for the owner's control re-ratification. The
// producer (gfx_app.c) is BOX-pinned by design, and DEVIATION D32 took
// the C-layer OFF the BOX style — six gameplay buttons cannot carry
// seven roles once grab is real — so the five cs signatures below are
// GONE FROM THIS RIG, not weakened away: the cs plane is pinned
// bit-exactly, on the style that still has one, by s1_sweep.c checks
// 11-13 (which this same check runs, host-side, in step [3/9]). What
// the session's ex-C-layer chords now produce is PLAIN stick, and two
// of those were never pinned before, so they are pinned here; and the
// BUTTON plane gains its fourth role, GRAB, which is the whole point of
// the re-ratification. The script's PRESSES are byte-unchanged.
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
  ["plain-down lsY=-1 no shield", (i) => i.lsX === 0 && i.lsY === -1 && !i.r],
  ["plain-diag (-0.7,-0.7)",      (i) => i.lsX === -0.7 && i.lsY === -0.7 && !i.r],
  // the D33 button plane, one signature per face role
  ["jump (physical A) -> i.x",    (i) => i.x === true],
  ["attack (physical B) -> i.a",  (i) => i.a === true],
  ["special (physical Y) -> i.b", (i) => i.b === true],
  // Grab in this engine is not a button (see /CONTEXT.md "Grab (how it
  // is reached)"): physical X emits the A + LIGHT SHIELD chord that
  // Melee's Z button is, and that chord is what the grab arms read.
  // DEVIATION D34 — it replaces `i.z === true`, which was a bit that
  // reached no grab arm anywhere in the sim.
  ["grab (physical X) -> i.a + light shield",
                                  (i) => i.a === true && i.lA > 0 && i.lA < 1],
];
const counts = new Array(SIGS.length).fill(0);

let neutralSlot1 = 0;
let invBad = 0;
let socdH = 0; // frames with raw left+right both held (axis proven neutral)
let socdV = 0; // frames with raw up+down both held
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
  // S1 invariants. i.lA left the flat-zero list with D34 — it is now the
  // light-shield half of the X grab chord, asserted against its physical
  // source in the sidecar pairing below — and the cs plane is dead on
  // BOX since D32, so it is pinned to zero. i.z is back on the list and
  // is now dead on EVERY style: it is an alternate attack button in this
  // engine, never a grab, and nothing emits it.
  if (i.y || i.l || i.z || i.du || i.dl || i.dr || i.dd) invBad++;
  if (i.csX !== 0 || i.csY !== 0) invBad++;
  if (i.lA !== 0 && !(i.lA > 0 && i.lA < 1)) invBad++;
  if (i.r ? i.rA !== 1 : i.rA !== 0) invBad++;
  if (i.lsX !== i.rawX || i.lsY !== i.rawY ||
      i.csX !== i.rawcsX || i.csY !== i.rawcsY) invBad++;
  // sidecar pairing fidelity: the four face buttons and START reach the
  // S1 row through the D33/D34 mapping — A jump, B attack, Y special,
  // X grab (as `a` + the light-shield analog) — and the bits and bools
  // must agree frame-by-frame. This is the
  // rig's PHYSICAL witness for the re-ratified plane: the signatures
  // above prove each role fires, this proves it fires from the right
  // button on every single frame.
  const k = rawKeys[f];
  if (((k & K_A) !== 0) !== i.x ||
      ((k & K_B) !== 0 || (k & K_X) !== 0) !== i.a ||
      ((k & K_Y) !== 0) !== i.b || ((k & K_X) !== 0) !== (i.lA > 0) ||
      ((k & K_START) !== 0) !== i.s) {
    die("frame " + f + " raw-key sidecar a/b/x/start bits disagree with " +
        "the recorded row — sidecar is not this session's recording");
  }
  // SOCD live witness (universal S1 invariants over the raw d-pad):
  // opposed cardinals MUST have resolved the axis to neutral on
  // whichever stick S1 drove (ls, or cs under the Y C-layer).
  if ((k & K_LEFT) && (k & K_RIGHT)) {
    if (i.lsX !== 0 || i.csX !== 0) {
      die("frame " + f + " SOCD VIOLATION: raw left+right both held but " +
          "x-axis not neutral (lsX=" + i.lsX + " csX=" + i.csX +
          ") — last-key-wins regression in the uinput->SDL->S1 path");
    }
    socdH++;
  }
  if ((k & K_UP) && (k & K_DOWN)) {
    if (i.lsY !== 0 || i.csY !== 0) {
      die("frame " + f + " SOCD VIOLATION: raw up+down both held but " +
          "y-axis not neutral (lsY=" + i.lsY + " csY=" + i.csY + ")");
    }
    socdV++;
  }
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
// SOCD witness coverage (review-51 M4): the committed script holds both
// opposed pairs 300 ms — the RAW sidecar must show them, proving the
// live SOCD path was actually exercised (a trace-only judge cannot:
// an SOCD frame is byte-identical to a neutral frame in the trace).
const socdSigs = [["socd-horizontal raw L+R held, x neutral", socdH],
                  ["socd-vertical raw U+D held, y neutral", socdV]];
for (const [name, n] of socdSigs) {
  const ok = n >= MIN_FRAMES;
  if (!ok) fail = 1;
  console.log((ok ? "ok  " : "FAIL") + " " + name + ": " + n + " frames" +
              (ok ? "" : " (< " + MIN_FRAMES + ")"));
}
if (fail) {
  console.error("S1 COVERAGE FAIL: missing/short chord signatures — " +
                "dropped injector events or a dead SDL input path");
  process.exit(2);
}
console.log("S1 COVERAGE OK (" + (SIGS.length + socdSigs.length) + "/" +
            (SIGS.length + socdSigs.length) +
            " signatures >= " + MIN_FRAMES + " frames each, " +
            trace.length + " frames, slot-1 neutral " + neutralSlot1 +
            "/" + trace.length + ", socd H/V " + socdH + "/" + socdV +
            " frames)");
