#!/usr/bin/env node
// Freeze a golden's checksum stream (M0 task 5). Called by oracle/record.sh
// after TWO fresh browser runs of the golden; refuses to freeze unless the
// two runs are bit-identical on every conformance channel.
//
// Usage: node freeze-stream.js <golden-id> <runA.json> <runB.json> [--refreeze]
//
// Writes oracle/goldens/<name>.sha256.json — a contract artifact:
//   { golden, specVersion, params{trace,traceSha256,frames,seed,p1,p2,stage,
//     cpu,difficulty,fdlibm,seedRandom}, rngCalls, rngCallsOutsideStep,
//     streamSha256, frames:[{f,h}…] }
// The file is FULLY DETERMINISTIC (no timestamps, no environment data such
// as browser versions): re-recording the same golden under the same spec
// must reproduce it byte-for-byte, so `git diff` after a re-freeze is the
// drift detector. If an existing frozen file differs, this script FAILS
// unless --refreeze is passed (a re-freeze is only legitimate alongside a
// spec version bump — oracle/CHECKSUM.md §8).
"use strict";
const fs = require("fs");
const path = require("path");
const { streamDigest, sha256File, GOLDENS_DIR, goldenById, specVersion } =
  require("./streamlib");

function die(msg) { console.error("freeze-stream: " + msg); process.exit(1); }

const [id, fileA, fileB] = process.argv.slice(2);
const refreeze = process.argv.includes("--refreeze");
if (!id || !fileA || !fileB) {
  die("usage: node freeze-stream.js <golden-id> <runA.json> <runB.json> [--refreeze]");
}

const g = goldenById(id);
const a = JSON.parse(fs.readFileSync(fileA, "utf8"));
const b = JSON.parse(fs.readFileSync(fileB, "utf8"));

// --- the two fresh runs must agree on EVERY conformance channel ---------
if (a.frames.length !== b.frames.length) {
  die(`run lengths differ: ${a.frames.length} vs ${b.frames.length}`);
}
for (let i = 0; i < a.frames.length; i++) {
  if (a.frames[i].f !== b.frames[i].f || a.frames[i].h !== b.frames[i].h) {
    die(`runs diverge at frame ${a.frames[i].f} — NOT freezing`);
  }
}
if (a.coverage.rngCalls !== b.coverage.rngCalls) {
  die(`rngCalls differ: ${a.coverage.rngCalls} vs ${b.coverage.rngCalls}`);
}
if (a.coverage.rngCallsOutsideStep !== b.coverage.rngCallsOutsideStep) {
  die("rngCallsOutsideStep differ between runs");
}
// Expected diagnostic value (CHECKSUM.md §1.3): exactly one off-step draw.
if (a.coverage.rngCallsOutsideStep !== 1) {
  die(`rngCallsOutsideStep is ${a.coverage.rngCallsOutsideStep}, expected 1 ` +
      "(CHECKSUM.md §1.3/§6) — a new off-tick RNG consumer leaked; investigate");
}

// --- both runs must have been recorded with the golden's exact params ---
const tracePath = path.join(GOLDENS_DIR, g.trace);
const traceSha256 = sha256File(tracePath);
for (const [label, r] of [["A", a], ["B", b]]) {
  const m = r.meta;
  const want = { frames: g.frames, seed: g.seed, p1: g.p1, p2: g.p2,
    stage: g.stage, cpu: g.cpu, difficulty: g.cpu ? g.difficulty : null,
    seedRandom: true, fdlibm: true };
  for (const k of Object.keys(want)) {
    if (m[k] !== want[k]) {
      die(`run ${label} meta.${k} = ${m[k]}, manifest wants ${want[k]}`);
    }
  }
  if (path.basename(String(m.trace)) !== g.trace) {
    die(`run ${label} trace ${m.trace} is not the golden's ${g.trace}`);
  }
  if (r.frames.length !== g.frames) {
    die(`run ${label} has ${r.frames.length} frames, manifest wants ${g.frames}`);
  }
}

// --- build the frozen file deterministically -----------------------------
const params = { trace: g.trace, traceSha256: traceSha256, frames: g.frames,
  seed: g.seed, p1: g.p1, p2: g.p2, stage: g.stage, cpu: g.cpu,
  difficulty: g.cpu ? g.difficulty : null, fdlibm: true, seedRandom: true };
const lines = [];
lines.push("{");
lines.push('"golden": ' + JSON.stringify(g.name) + ",");
lines.push('"specVersion": ' + specVersion() + ",");
lines.push('"params": ' + JSON.stringify(params) + ",");
lines.push('"rngCalls": ' + a.coverage.rngCalls + ",");
lines.push('"rngCallsOutsideStep": ' + a.coverage.rngCallsOutsideStep + ",");
lines.push('"streamSha256": ' + JSON.stringify(streamDigest(a.frames)) + ",");
lines.push('"frames": [');
for (let i = 0; i < a.frames.length; i++) {
  lines.push(JSON.stringify({ f: a.frames[i].f, h: a.frames[i].h }) +
    (i === a.frames.length - 1 ? "" : ","));
}
lines.push("]");
lines.push("}");
const text = lines.join("\n") + "\n";

const outPath = path.join(GOLDENS_DIR, g.name + ".sha256.json");
if (fs.existsSync(outPath)) {
  const old = fs.readFileSync(outPath, "utf8");
  if (old === text) {
    console.log(`FROZEN ${g.name}: unchanged (byte-identical re-freeze)`);
    process.exit(0);
  }
  if (!refreeze) {
    die(`${outPath} exists and DIFFERS from this recording. A frozen stream ` +
        "is a contract artifact; overwriting requires --refreeze and is only " +
        "legitimate with a spec version bump (oracle/CHECKSUM.md §8).");
  }
}
fs.writeFileSync(outPath, text);
console.log(`FROZEN ${g.name}: ${a.frames.length} frames, ` +
  `rngCalls=${a.coverage.rngCalls}, specVersion=${specVersion()} -> ${outPath}`);
