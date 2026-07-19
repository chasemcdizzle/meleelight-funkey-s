#!/usr/bin/env node
// freeze-stream-m4.js — freeze an M4 golden's checksum stream into
// port/goldens-m4/ (M4 task 5; fix_plan §M4 conventions).
//
// HARD RULE 3: only M0 tasks write oracle/ — so oracle/harness/
// freeze-stream.js (which writes oracle/goldens/) cannot be reused as a
// whole. This is its M4 twin: the shared primitives (streamDigest,
// sha256File, specVersion) are REQUIRED BY PATH from the UNCHANGED
// oracle/harness/streamlib.js (byte reuse, zero transcription — one
// digest implementation in the tree), the golden registry is
// port/goldens-m4/manifest.json, and the frozen file lands next to it.
// The frozen FORMAT is identical, so the UNCHANGED
// oracle/harness/verify-stream.js judges M4 goldens with zero changes.
//
// Usage: node freeze-stream-m4.js <golden-id> <runA.json> <runB.json>
//          [--refreeze]
//
// Identical contract to the M0 freezer: refuses unless the two fresh
// runs are bit-identical on every conformance channel; pins
// rngCallsOutsideStep == 1; pins every run meta field against the
// manifest; output is fully deterministic (no timestamps/environment);
// an existing DIFFERING frozen file fails without --refreeze
// (only legitimate with a spec version bump, oracle/CHECKSUM.md §8).
"use strict";
const fs = require("fs");
const path = require("path");
const { streamDigest, sha256File, specVersion } =
  require("../../oracle/harness/streamlib");

const GOLDENS_DIR = __dirname; // port/goldens-m4

function die(msg) { console.error("freeze-stream-m4: " + msg); process.exit(1); }

function goldenById(id) {
  const m = JSON.parse(
    fs.readFileSync(path.join(GOLDENS_DIR, "manifest.json"), "utf8"));
  const g = m.goldens.find((x) => x.id === id || x.name === id);
  if (!g) throw new Error("golden not in port/goldens-m4/manifest.json: " + id);
  return g;
}

const [id, fileA, fileB] = process.argv.slice(2);
const refreeze = process.argv.includes("--refreeze");
if (!id || !fileA || !fileB) {
  die("usage: node freeze-stream-m4.js <golden-id> <runA.json> <runB.json> [--refreeze]");
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

// --- build the frozen file deterministically (M0 format, verbatim) -------
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
