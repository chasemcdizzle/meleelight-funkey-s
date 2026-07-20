#!/usr/bin/env node
// verify-target-stream.js — judge a TARGET-plane run against its FROZEN
// target-plane stream (fix_plan §M4 task 11; the iter-63 separate-stream
// convention; Tier A+ — it is a judge path). The spec-v1 PLAYER stream is
// judged SEPARATELY by the UNCHANGED oracle/harness/verify-stream.js; this
// verifier owns ONLY the target plane + the target-specific param pins the
// player verifier has no field for (mode/tstage/char).
//
// Usage: node verify-target-stream.js <run.json> <frozen.target.sha256.json>
//
// PASS ("TARGET STREAM MATCH …", exit 0) requires ALL of:
//   1. frozen-file integrity: streamSha256 seal matches its frames, frame
//      numbers exactly 1..N, N == params.frames, stream == "target-plane",
//      basename == golden + ".target.sha256.json";
//   2. spec pin: frozen specVersion == oracle/CHECKSUM.md's current version;
//   3. trace pin: the golden trace (next to the frozen file) hashes to
//      params.traceSha256, and the run consumed that trace;
//   4. param pin: run.meta {frames, seed, mode:"target", tstage} and the
//      target CHAR (run.meta.p1) match the frozen params exactly;
//   5. the target-plane stream: EXACT per-frame {f,h} equality, FULL
//      length (never epsilon/prefix);
//   6. the target-plane FINALS: run.coverage.target.targetsDestroyed ==
//      params.finalTargetsDestroyed and endTargetGame == finalEndTargetGame
//      (the plane's end-state, pinned bit-exactly alongside the stream).
// Any failure prints the reason and exits nonzero (divergence exits 2).
"use strict";
const fs = require("fs");
const path = require("path");
const { streamDigest, sha256File, specVersion } =
  require("../../oracle/harness/streamlib");

function die(msg, code) {
  console.error("TARGET STREAM MISMATCH: " + msg);
  process.exit(code || 1);
}

const [runPath, frozenPath] = process.argv.slice(2);
if (!runPath || !frozenPath) {
  console.error("usage: node verify-target-stream.js <run.json> <frozen.target.sha256.json>");
  process.exit(1);
}
const run = JSON.parse(fs.readFileSync(runPath, "utf8"));
const frozen = JSON.parse(fs.readFileSync(frozenPath, "utf8"));

// 1. frozen-file integrity
if (frozen.stream !== "target-plane") {
  die(`frozen file stream is ${JSON.stringify(frozen.stream)}, not "target-plane"`);
}
if (path.basename(frozenPath) !== frozen.golden + ".target.sha256.json") {
  die(`frozen file ${path.basename(frozenPath)} does not match its embedded ` +
      `golden name ${frozen.golden}`);
}
if (!Array.isArray(frozen.frames) || frozen.frames.length !== frozen.params.frames) {
  die(`frozen stream has ${frozen.frames && frozen.frames.length} frames but ` +
      `params.frames=${frozen.params.frames}`);
}
for (let i = 0; i < frozen.frames.length; i++) {
  if (frozen.frames[i].f !== i + 1) die(`frozen frame numbering broken at index ${i}`);
}
if (streamDigest(frozen.frames) !== frozen.streamSha256) {
  die("frozen file streamSha256 seal does not match its frames (corrupt/edited)");
}

// 2. spec version pin
const cur = specVersion();
if (frozen.specVersion !== cur) {
  die(`frozen target stream is spec v${frozen.specVersion} but oracle/CHECKSUM.md ` +
      `is v${cur} — re-freeze required (CHECKSUM.md §8)`);
}

// 3. trace pin
const traceFile = path.join(path.dirname(frozenPath), frozen.params.trace);
if (!fs.existsSync(traceFile)) die("golden trace missing: " + traceFile);
const th = sha256File(traceFile);
if (th !== frozen.params.traceSha256) {
  die(`trace ${frozen.params.trace} sha256 ${th} != frozen ${frozen.params.traceSha256}`);
}
if (path.basename(String(run.meta.trace)) !== frozen.params.trace) {
  die(`run consumed trace ${run.meta.trace}, frozen stream is for ${frozen.params.trace}`);
}

// 4. param pin (target-specific: char is run.meta.p1; mode + tstage)
if (run.meta.frames !== frozen.params.frames) {
  die(`run meta.frames = ${run.meta.frames}, frozen ${frozen.params.frames}`);
}
if (run.meta.seed !== frozen.params.seed) {
  die(`run meta.seed = ${run.meta.seed}, frozen ${frozen.params.seed}`);
}
if (run.meta.mode !== "target" || frozen.params.mode !== "target") {
  die(`mode pin failed (run ${run.meta.mode}, frozen ${frozen.params.mode})`);
}
if (run.meta.tstage !== frozen.params.tstage) {
  die(`run meta.tstage = ${run.meta.tstage}, frozen ${frozen.params.tstage}`);
}
if (run.meta.p1 !== frozen.params.char) {
  die(`run char (meta.p1) = ${run.meta.p1}, frozen ${frozen.params.char}`);
}

// 5. the target-plane stream: exact equality, every frame, full length
const rf = run.target && run.target.frames;
if (!Array.isArray(rf)) die("run JSON has no target.frames");
if (rf.length !== frozen.frames.length) {
  die(`run has ${rf.length} target frames, frozen ${frozen.frames.length} ` +
      "(full-length match required)", 2);
}
for (let i = 0; i < frozen.frames.length; i++) {
  if (rf[i].f !== frozen.frames[i].f || rf[i].h !== frozen.frames[i].h) {
    die(`first target-plane divergence at frame ${frozen.frames[i].f} of ` +
        `${frozen.frames.length}\n  frozen: ${frozen.frames[i].h}\n  run:    ` +
        rf[i].h, 2);
  }
}

// 6. target-plane finals
const tf = run.coverage && run.coverage.target;
if (!tf) die("run coverage has no target finals");
if (tf.targetsDestroyed !== frozen.params.finalTargetsDestroyed) {
  die(`final targetsDestroyed ${tf.targetsDestroyed} != frozen ` +
      `${frozen.params.finalTargetsDestroyed}`, 2);
}
if (tf.endTargetGame !== frozen.params.finalEndTargetGame) {
  die(`final endTargetGame ${tf.endTargetGame} != frozen ` +
      `${frozen.params.finalEndTargetGame}`, 2);
}

console.log(`TARGET STREAM MATCH ${frozen.golden}: ${frozen.frames.length}/` +
  `${frozen.frames.length} target frames exact, targetsDestroyed=` +
  `${tf.targetsDestroyed}, endTargetGame=${tf.endTargetGame}, specVersion=${cur}`);
process.exit(0);
