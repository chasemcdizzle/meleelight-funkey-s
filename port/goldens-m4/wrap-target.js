#!/usr/bin/env node
// wrap-target.js — wrap the C target sim's stdout (sim_host_target) into
// TWO verify-compatible run JSONs (fix_plan §M4 task 11; the wrap-run.js
// twin for gameMode 5): the PLAYER run JSON (judged by the UNCHANGED
// oracle/harness/verify-stream.js) and the TARGET-plane run JSON (judged
// by verify-target-stream.js). Params come from
// port/goldens-m4/manifest-target.json (single param source).
//
// Usage: node wrap-target.js <goldenId> <sim-output.txt> \
//          <out-player.json> <out-target.json>
//
// Sim output contract (whitelist grammar, PROCESS §3; HARD-FAILED exit 3
// on any drift — anchored full-line patterns only, binary outcome, no
// partial parses, no silent skips):
//   - lines "F <n> <64-lowercase-hex>": the PLAYER checksum stream;
//   - lines "T <n> <64-lowercase-hex>": the TARGET-plane stream;
//     F and T for a given frame must INTERLEAVE F<n> then T<n>, both
//     contiguous 1..N with N == the golden's frame count;
//   - exactly one line "RNG <rngCalls> <rngCallsOutsideStep>" (integers);
//   - a final line "SIM OK" (nothing after it);
//   - ANY other line is an error (resembles-but-doesn't-match =
//     corruption = fail closed).
//
// The target run JSON reuses the player meta shape + carries
// coverage.target finals from the FROZEN target sha (the C sim does not
// emit finals on its stdout — they are derivable from the target stream's
// last frame, so wrap-target reads them off the frozen file it will be
// judged against, and verify-target-stream re-checks stream + finals; a
// mismatch there is still caught). To avoid a self-referential finals
// pass, the finals are recomputed from the manifest's committed
// finalTargetsDestroyed only via verify — here we DERIVE them from the
// sim's own last target frame is impossible (hash only), so the finals
// come from re-running the target-plane envelope? No: the C sim already
// emitted the plane; the finals live in coverage. We instead read the
// COUNT the C sim reached from a REQUIRED trailing "TFIN" line.
"use strict";

const fs = require("fs");
const path = require("path");

const GOLDENS_DIR = __dirname;

function die(msg) { console.error("wrap-target: " + msg); process.exit(3); }

const [goldenId, simOutPath, outPlayerPath, outTargetPath] =
  process.argv.slice(2);
if (!goldenId || !simOutPath || !outPlayerPath || !outTargetPath) {
  console.error("usage: node wrap-target.js <goldenId> <sim-output.txt> " +
    "<out-player.json> <out-target.json>");
  process.exit(1);
}

const manifest = JSON.parse(
  fs.readFileSync(path.join(GOLDENS_DIR, "manifest-target.json"), "utf8"));
const g = manifest.goldens.find((x) => x.id === goldenId);
if (!g) {
  console.error("wrap-target: unknown golden id " + goldenId + " (have: " +
    manifest.goldens.map((x) => x.id).join(", ") + ")");
  process.exit(1);
}

const rawLines = fs.readFileSync(simOutPath, "utf8").split("\n");
if (rawLines.length && rawLines[rawLines.length - 1] === "") rawLines.pop();

const F_RE = /^F (\d+) ([0-9a-f]{64})$/;
const T_RE = /^T (\d+) ([0-9a-f]{64})$/;
const RNG_RE = /^RNG (\d+) (\d+)$/;
const TFIN_RE = /^TFIN (\d+) (T|F)$/; // targetsDestroyed + endTargetGame
const playerFrames = [];
const targetFrames = [];
let rng = null;
let tfin = null;
let simOk = false;
for (let i = 0; i < rawLines.length; i++) {
  const line = rawLines[i];
  if (simOk) die("output continues after SIM OK at line " + (i + 1) +
                 ": " + JSON.stringify(line));
  let m;
  if ((m = F_RE.exec(line)) !== null) {
    const n = parseInt(m[1], 10);
    if (n !== playerFrames.length + 1) {
      die("F lines not contiguous 1..N: got F " + n + " after " +
          playerFrames.length + " (line " + (i + 1) + ")");
    }
    if (n !== targetFrames.length + 1) {
      die("F/T interleave broken: F " + n + " but " + targetFrames.length +
          " T lines so far (line " + (i + 1) + ")");
    }
    playerFrames.push({ f: n, h: m[2] });
  } else if ((m = T_RE.exec(line)) !== null) {
    const n = parseInt(m[1], 10);
    if (n !== targetFrames.length + 1) {
      die("T lines not contiguous 1..N: got T " + n + " after " +
          targetFrames.length + " (line " + (i + 1) + ")");
    }
    if (n !== playerFrames.length) {
      die("F/T interleave broken: T " + n + " must follow its F " + n +
          " (line " + (i + 1) + ")");
    }
    targetFrames.push({ f: n, h: m[2] });
  } else if ((m = RNG_RE.exec(line)) !== null) {
    if (rng !== null) die("duplicate RNG line at line " + (i + 1));
    rng = { rngCalls: parseInt(m[1], 10),
            rngCallsOutsideStep: parseInt(m[2], 10) };
  } else if ((m = TFIN_RE.exec(line)) !== null) {
    if (tfin !== null) die("duplicate TFIN line at line " + (i + 1));
    tfin = { targetsDestroyed: parseInt(m[1], 10), endTargetGame: m[2] === "T" };
  } else if (line === "SIM OK") {
    simOk = true;
  } else {
    die("unrecognized sim output at line " + (i + 1) + ": " +
        JSON.stringify(line));
  }
}
if (!simOk) die("missing final SIM OK line");
if (rng === null) die("missing RNG line");
if (tfin === null) die("missing TFIN line");
if (playerFrames.length !== g.frames) {
  die("sim produced " + playerFrames.length + " player frames, golden " +
      g.id + " requires " + g.frames);
}
if (targetFrames.length !== g.frames) {
  die("sim produced " + targetFrames.length + " target frames, golden " +
      g.id + " requires " + g.frames);
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
