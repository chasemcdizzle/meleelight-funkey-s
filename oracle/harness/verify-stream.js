#!/usr/bin/env node
// Verify a harness run against a FROZEN golden checksum stream (M0 task 5).
//
// Usage: node verify-stream.js <run.json> <frozen.sha256.json>
//
// PASS (prints "STREAM MATCH …", exit 0) requires ALL of:
//   1. frozen file integrity: its streamSha256 seal matches its frames,
//      its frame numbers are exactly 1..N, N == params.frames, and its
//      basename matches its embedded golden name;
//   2. spec pin: frozen specVersion == the current version parsed from
//      oracle/CHECKSUM.md (a spec bump without a re-freeze fails here);
//   3. trace pin: the golden's trace file (next to the frozen file) hashes
//      to params.traceSha256, and the run consumed that trace;
//   4. param pin: the run's meta matches the frozen params exactly
//      (frames, seed, p1, p2, stage, cpu, difficulty, seedRandom, and the
//      fdlibm shim ACTIVE — frozen streams are fdlibm streams);
//   5. the checksum stream: EXACT string equality, every frame, FULL
//      length (never epsilon, never prefix — CHECKSUM.md §1.1);
//   6. RNG channels: end-of-run rngCalls equal (binding, §1.2) and
//      rngCallsOutsideStep equal (== 1, diagnostic, §1.3).
// Any failure prints the reason and exits nonzero (divergence exits 2).
"use strict";
const fs = require("fs");
const path = require("path");
const { streamDigest, sha256File, specVersion } = require("./streamlib");

function die(msg, code) {
  console.error("STREAM MISMATCH: " + msg);
  process.exit(code || 1);
}

const [runPath, frozenPath] = process.argv.slice(2);
if (!runPath || !frozenPath) {
  console.error("usage: node verify-stream.js <run.json> <frozen.sha256.json>");
  process.exit(1);
}
const run = JSON.parse(fs.readFileSync(runPath, "utf8"));
const frozen = JSON.parse(fs.readFileSync(frozenPath, "utf8"));

// 1. frozen-file integrity
if (path.basename(frozenPath) !== frozen.golden + ".sha256.json") {
  die(`frozen file ${path.basename(frozenPath)} does not match its embedded ` +
      `golden name ${frozen.golden}`);
}
if (frozen.frames.length !== frozen.params.frames) {
  die(`frozen stream has ${frozen.frames.length} frames but params.frames=` +
      frozen.params.frames);
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
  die(`frozen stream is spec v${frozen.specVersion} but oracle/CHECKSUM.md is ` +
      `v${cur} — re-freeze required (CHECKSUM.md §8)`);
}

// 3. trace pin
const traceFile = path.join(path.dirname(frozenPath), frozen.params.trace);
if (!fs.existsSync(traceFile)) die("golden trace missing: " + traceFile);
const th = sha256File(traceFile);
if (th !== frozen.params.traceSha256) {
  die(`trace ${frozen.params.trace} sha256 ${th} != frozen ` +
      frozen.params.traceSha256);
}
if (path.basename(String(run.meta.trace)) !== frozen.params.trace) {
  die(`run consumed trace ${run.meta.trace}, frozen stream is for ` +
      frozen.params.trace);
}

// 4. run params must match the frozen recording exactly
for (const k of ["frames", "seed", "p1", "p2", "stage", "cpu", "difficulty",
                 "fdlibm", "seedRandom"]) {
  if (run.meta[k] !== frozen.params[k]) {
    die(`run meta.${k} = ${run.meta[k]}, frozen params.${k} = ${frozen.params[k]}`);
  }
}

// 5. the stream: exact equality, every frame, full length
if (run.frames.length !== frozen.frames.length) {
  die(`run has ${run.frames.length} frames, frozen stream has ` +
      `${frozen.frames.length} (full-length match required)`, 2);
}
for (let i = 0; i < frozen.frames.length; i++) {
  if (run.frames[i].f !== frozen.frames[i].f ||
      run.frames[i].h !== frozen.frames[i].h) {
    die(`first divergence at frame ${frozen.frames[i].f} of ` +
        `${frozen.frames.length}\n  frozen: ${frozen.frames[i].h}\n  run:    ` +
        run.frames[i].h, 2);
  }
}

// 6. RNG channels
if (run.coverage.rngCalls !== frozen.rngCalls) {
  die(`rngCalls ${run.coverage.rngCalls} != frozen ${frozen.rngCalls} ` +
      "(binding secondary channel, CHECKSUM.md §1.2)", 2);
}
if (run.coverage.rngCallsOutsideStep !== frozen.rngCallsOutsideStep) {
  die(`rngCallsOutsideStep ${run.coverage.rngCallsOutsideStep} != frozen ` +
      `${frozen.rngCallsOutsideStep} (CHECKSUM.md §1.3)`, 2);
}

console.log(`STREAM MATCH ${frozen.golden}: ${frozen.frames.length}/` +
  `${frozen.frames.length} frames exact, rngCalls=${frozen.rngCalls}, ` +
  `rngCallsOutsideStep=${frozen.rngCallsOutsideStep}, specVersion=${cur}`);
process.exit(0);
