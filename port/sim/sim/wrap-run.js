#!/usr/bin/env node
// wrap-run.js — M2 task 17: wrap the integrated C sim's stdout into a
// verify-stream.js-compatible run JSON. The C sim knows nothing about the
// harness meta shape; this script marries its checksum stream to the
// golden's manifest params (single param source, M0 convention) so
// `node oracle/harness/verify-stream.js <out-run.json> <frozen>` judges
// the C sim exactly like a browser run.
//
// Usage: node wrap-run.js <goldenId> <sim-output.txt> <out-run.json> \
//          [<manifest.json>]
//
// The optional 4th arg (M4 task 5) selects an ALTERNATE golden manifest
// (e.g. port/goldens-m4/manifest.json — same schema); omitted, the
// oracle manifest is used exactly as before (all pre-M4 invocations are
// byte-identical in behavior).
//
// Sim output contract (HARD-FAILED, exit 3, on any violation):
//   - lines "F <n> <64-lowercase-hex>": the per-frame checksum stream,
//     REQUIRED contiguous 1..N with N == the golden's frame count;
//   - exactly one line "RNG <rngCalls> <rngCallsOutsideStep>" (integers);
//   - a final line "SIM OK" (nothing after it);
//   - ANY other line is an error — the C sim's stdout is the artifact,
//     stray prints would mean an undisciplined sim build.
//
// Emitted meta mirrors oracle/harness/run.js + the frozen params block
// TYPE-exactly (verify-stream compares meta.{frames,seed,p1,p2,stage,cpu,
// difficulty,fdlibm,seedRandom} with !== and path.basename(meta.trace)
// against params.trace): cpu stays the manifest boolean, difficulty is
// null for non-cpu goldens, trace is the golden's trace FILENAME.
"use strict";

const fs = require("fs");
const path = require("path");

const REPO = path.resolve(__dirname, "..", "..", "..");

function die(msg) {
  console.error("wrap-run: " + msg);
  process.exit(3);
}

const [goldenId, simOutPath, outRunPath, manifestArg] = process.argv.slice(2);
if (!goldenId || !simOutPath || !outRunPath) {
  console.error("usage: node wrap-run.js <goldenId> <sim-output.txt> " +
    "<out-run.json> [<manifest.json>]");
  process.exit(1);
}

const manifestPath = manifestArg
  ? path.resolve(manifestArg)
  : path.join(REPO, "oracle", "goldens", "manifest.json");
const manifest = JSON.parse(fs.readFileSync(manifestPath, "utf8"));
const g = manifest.goldens.find((x) => x.id === goldenId);
if (!g) {
  console.error("wrap-run: unknown golden id " + goldenId + " (have: " +
    manifest.goldens.map((x) => x.id).join(", ") + ")");
  process.exit(1);
}

const rawLines = fs.readFileSync(simOutPath, "utf8").split("\n");
if (rawLines.length && rawLines[rawLines.length - 1] === "") rawLines.pop();

const F_RE = /^F (\d+) ([0-9a-f]{64})$/;
const RNG_RE = /^RNG (\d+) (\d+)$/;
const frames = [];
let rng = null;
let simOk = false;
for (let i = 0; i < rawLines.length; i++) {
  const line = rawLines[i];
  if (simOk) die("output continues after SIM OK at line " + (i + 1) +
                 ": " + JSON.stringify(line));
  let m;
  if ((m = F_RE.exec(line)) !== null) {
    const n = parseInt(m[1], 10);
    if (n !== frames.length + 1) {
      die("frame lines not contiguous 1..N: got F " + n + " after " +
          frames.length + " frames (line " + (i + 1) + ")");
    }
    frames.push({ f: n, h: m[2] });
  } else if ((m = RNG_RE.exec(line)) !== null) {
    if (rng !== null) die("duplicate RNG line at line " + (i + 1));
    rng = { rngCalls: parseInt(m[1], 10),
            rngCallsOutsideStep: parseInt(m[2], 10) };
  } else if (line === "SIM OK") {
    simOk = true;
  } else {
    die("unrecognized sim output at line " + (i + 1) + ": " +
        JSON.stringify(line));
  }
}
if (!simOk) die("missing final SIM OK line");
if (rng === null) die("missing RNG line");
if (frames.length !== g.frames) {
  die("sim produced " + frames.length + " frames, golden " + g.id +
      " requires " + g.frames);
}

const run = {
  meta: {
    dist: "(c-sim)",
    trace: g.trace, // filename, as in the manifest (basename-compared)
    frames: g.frames,
    seed: g.seed,
    p1: g.p1,
    p2: g.p2,
    stage: g.stage,
    seedRandom: true,
    fdlibm: true,
    cpu: g.cpu,
    difficulty: g.cpu ? (g.difficulty || 5) : null,
  },
  coverage: {
    rngCalls: rng.rngCalls,
    rngCallsOutsideStep: rng.rngCallsOutsideStep,
  },
  frames: frames,
};

fs.mkdirSync(path.dirname(path.resolve(outRunPath)), { recursive: true });
fs.writeFileSync(outRunPath, JSON.stringify(run));
console.log("wrap-run: " + g.id + " " + frames.length + " frames, rngCalls=" +
  rng.rngCalls + ", rngCallsOutsideStep=" + rng.rngCallsOutsideStep +
  " -> " + outRunPath);
