#!/usr/bin/env node
// wrap-target.js — wrap the C target sim's stdout (sim_host_target) into
// TWO verify-compatible run JSONs (fix_plan §M4 task 11; the wrap-run.js
// twin for gameMode 5): the PLAYER run JSON (judged by the UNCHANGED
// oracle/harness/verify-stream.js) and the TARGET-plane run JSON (judged
// by verify-target-stream.js). Params come from
// port/goldens-m4/manifest-target.json (single param source), validated
// by the SHARED strict validator before any row is trusted (review-94
// H1, iter 96).
//
// Usage: node wrap-target.js <goldenId> <sim-output.txt> \
//          <out-player.json> <out-target.json>
//
// Sim output contract — the EXACT-TOKEN POSITIONAL grammar (review-94
// M2, iter 96; PROCESS §3): the producer is target_main.c's printf
// discipline (measured — :217-218 "F %ld %s"/"T %ld %s" per frame,
// :246 "RNG %u %u", :250 "TFIN %d %s", :251 "SIM OK"), so the parser
// accepts NOTHING the producer cannot emit. Exactly 2N+3 lines
// (N = the golden's manifest frame count):
//   for f = 1..N, in order:  "F <f> <64-lowercase-hex>"
//                      then  "T <f> <64-lowercase-hex>"
//     — the frame token must be the CANONICAL integer text String(f)
//       (no leading zeros, exact single spacing); any reordering,
//       interleave break, or zero-padded token is corruption;
//   then exactly:  "RNG <int> <int>"    (canonical integer text)
//   then exactly:  "TFIN <int> <T|F>"   (targetsDestroyed + endTargetGame)
//   then exactly:  "SIM OK"             (the final line; nothing after)
// ANY deviation = corruption = HARD FAIL exit 3 naming the line
// (resembles-but-doesn't-match = death; no partial parses, no silent
// skips, no normalization).
//
// The target run JSON reuses the player meta shape and carries
// coverage.target finals from the REQUIRED TFIN line (the C sim's own
// reported finals); verify-target-stream.js re-checks stream + finals
// against the frozen artifacts.
"use strict";

const fs = require("fs");
const path = require("path");
const { loadValidatedManifest } = require("./validate-target-manifest");

function die(msg) { console.error("wrap-target: " + msg); process.exit(3); }

const [goldenId, simOutPath, outPlayerPath, outTargetPath] =
  process.argv.slice(2);
if (!goldenId || !simOutPath || !outPlayerPath || !outTargetPath) {
  console.error("usage: node wrap-target.js <goldenId> <sim-output.txt> " +
    "<out-player.json> <out-target.json>");
  process.exit(1);
}

let manifest;
try { manifest = loadValidatedManifest(); } catch (e) {
  console.error("wrap-target: " + e.message);
  process.exit(1);
}
const g = manifest.goldens.find((x) => x.id === goldenId);
if (!g) {
  console.error("wrap-target: unknown golden id " + goldenId + " (have: " +
    manifest.goldens.map((x) => x.id).join(", ") + ")");
  process.exit(1);
}

const rawLines = fs.readFileSync(simOutPath, "utf8").split("\n");
if (rawLines.length && rawLines[rawLines.length - 1] === "") rawLines.pop();

// Exact-token patterns (canonical integer text: 0 | [1-9][0-9]*).
const F_RE = /^F (0|[1-9][0-9]*) ([0-9a-f]{64})$/;
const T_RE = /^T (0|[1-9][0-9]*) ([0-9a-f]{64})$/;
const RNG_RE = /^RNG (0|[1-9][0-9]*) (0|[1-9][0-9]*)$/;
const TFIN_RE = /^TFIN (0|[1-9][0-9]*) (T|F)$/;

const wantLines = 2 * g.frames + 3;
if (rawLines.length !== wantLines) {
  die("sim output has " + rawLines.length + " lines; the producer grammar " +
      "requires exactly " + wantLines + " (2x" + g.frames +
      " frame lines + RNG + TFIN + SIM OK)");
}

const playerFrames = [];
const targetFrames = [];
let li = 0;
for (let f = 1; f <= g.frames; f++) {
  const fLine = rawLines[li];
  let m = F_RE.exec(fLine);
  if (m === null || m[1] !== String(f)) {
    die("line " + (li + 1) + ": expected exactly 'F " + f +
        " <64-lowercase-hex>' (canonical integer text, strict F/T " +
        "interleave); got " + JSON.stringify(fLine));
  }
  playerFrames.push({ f: f, h: m[2] });
  li++;
  const tLine = rawLines[li];
  m = T_RE.exec(tLine);
  if (m === null || m[1] !== String(f)) {
    die("line " + (li + 1) + ": expected exactly 'T " + f +
        " <64-lowercase-hex>' (canonical integer text, strict F/T " +
        "interleave); got " + JSON.stringify(tLine));
  }
  targetFrames.push({ f: f, h: m[2] });
  li++;
}
const rngLine = rawLines[li];
const rngM = RNG_RE.exec(rngLine);
if (rngM === null) {
  die("line " + (li + 1) + ": expected exactly 'RNG <int> <int>' " +
      "(canonical integer text, after all " + g.frames + " frame pairs); " +
      "got " + JSON.stringify(rngLine));
}
const rng = { rngCalls: Number(rngM[1]),
              rngCallsOutsideStep: Number(rngM[2]) };
li++;
const tfinLine = rawLines[li];
const tfinM = TFIN_RE.exec(tfinLine);
if (tfinM === null) {
  die("line " + (li + 1) + ": expected exactly 'TFIN <int> <T|F>' " +
      "(after the RNG line); got " + JSON.stringify(tfinLine));
}
const tfin = { targetsDestroyed: Number(tfinM[1]),
               endTargetGame: tfinM[2] === "T" };
li++;
if (rawLines[li] !== "SIM OK") {
  die("line " + (li + 1) + ": expected exactly 'SIM OK' as the final " +
      "line; got " + JSON.stringify(rawLines[li]));
}

const baseMeta = {
  dist: "(c-sim-target)",
  trace: g.trace,
  frames: g.frames,
  seed: g.seed,
  p1: g.char,
  p2: null,
  stage: null,
  seedRandom: true,
  fdlibm: true,
  cpu: false,
  difficulty: null,
  mode: "target",
  tstage: g.tstage,
};

// PLAYER run JSON (verify-stream.js shape)
fs.mkdirSync(path.dirname(path.resolve(outPlayerPath)), { recursive: true });
fs.writeFileSync(outPlayerPath, JSON.stringify({
  meta: baseMeta,
  coverage: {
    rngCalls: rng.rngCalls,
    rngCallsOutsideStep: rng.rngCallsOutsideStep,
  },
  frames: playerFrames,
}));

// TARGET run JSON (verify-target-stream.js shape): the target plane +
// the finals the C sim reported (verify-target-stream re-checks them
// against the frozen params — a wrong count fails there, not silently).
fs.writeFileSync(outTargetPath, JSON.stringify({
  meta: baseMeta,
  coverage: {
    rngCalls: rng.rngCalls,
    rngCallsOutsideStep: rng.rngCallsOutsideStep,
    target: {
      targetsDestroyed: tfin.targetsDestroyed,
      endTargetGame: tfin.endTargetGame,
    },
  },
  frames: playerFrames, // present for shape; verify-target reads target.frames
  target: { frames: targetFrames },
}));

console.log("wrap-target: " + g.id + " " + playerFrames.length +
  " player + " + targetFrames.length + " target frames, rngCalls=" +
  rng.rngCalls + ", targetsDestroyed=" + tfin.targetsDestroyed);
