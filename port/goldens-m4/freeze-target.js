#!/usr/bin/env node
// freeze-target.js — freeze a TARGET golden's TWO streams (fix_plan §M4
// task 11): the spec-v1 PLAYER stream into <name>.sha256.json (M0 format,
// judged by the UNCHANGED oracle/harness/verify-stream.js) and the
// SEPARATE target-plane stream into <name>.target.sha256.json (judged by
// verify-target-stream.js; iter-63 convention).
//
// HARD RULE 3: only M0 writes oracle/ — the shared digest primitives are
// REQUIRED BY PATH from the UNCHANGED oracle/harness/streamlib.js (byte
// reuse, zero transcription). The frozen PLAYER format is byte-identical
// to the M0 freezer's, so the UNCHANGED verify-stream.js judges it.
//
// Usage: node freeze-target.js <golden-id> <runA.json> <runB.json>
//          [--refreeze]
//
// Contract (per stream): refuses unless the two fresh runs are
// bit-identical on every conformance channel (player frames + target
// frames + rngCalls + rngCallsOutsideStep); pins rngCallsOutsideStep == 1;
// pins every run meta field against the manifest; output is fully
// deterministic (no timestamps/environment); an existing DIFFERING frozen
// file fails without --refreeze.
//
// MANIFEST GRAMMAR (PROCESS §3 whitelist rule): the freezer is the
// artifact-minting authority, so it validates the WHOLE manifest-target
// before trusting any row. The iter-94 grammar was EXTRACTED VERBATIM
// into the SHARED validate-target-manifest.js (review-94 H1, iter 96)
// so every done-check consumer runs the same validator.
//
// DUP-KEY SCAN (review-96 C-M1, iter 98): every JSON this freezer
// parses for a decision — runA, runB, and an existing frozen file on
// the --refreeze read path — goes through the SHARED string-aware
// duplicate-key scanner (json-dup-key-scan.js) on the RAW bytes before
// JSON.parse; a duplicate key at ANY scope is last-wins corruption and
// refuses (the manifest is scanned inside the shared validator).
//
// x2 BROWSER IDENTITY (review-94 L1, iter 96): the two fresh runs must
// record the SAME browser name + version — a channel-Chrome run A paired
// with a bundled-Chromium fallback run B is NOT same-browser
// repeatability evidence; mismatch = refusal naming both sides.
"use strict";
const fs = require("fs");
const path = require("path");
const { streamDigest, sha256File, specVersion } =
  require("../../oracle/harness/streamlib");
const { loadValidatedManifest, goldenByIdOrName } =
  require("./validate-target-manifest");
const { assertNoDuplicateKeys } = require("./json-dup-key-scan");

const GOLDENS_DIR = __dirname; // port/goldens-m4

function die(msg) { console.error("freeze-target: " + msg); process.exit(1); }

function checkSpec(v, what) {
  if (typeof v !== "number" || !Number.isInteger(v) || v < 1) {
    die(what + " specVersion " + JSON.stringify(v) + " is not a strict " +
      "positive integer — corruption, refusing");
  }
  return v;
}
const CUR_SPEC = checkSpec(specVersion(), "current oracle/CHECKSUM.md");

function goldenById(id) {
  // The SHARED strict validator (review-94 H1) — the whole manifest is
  // validated before any row is trusted; violations die here.
  let m;
  try { m = loadValidatedManifest(); } catch (e) { die(e.message); }
  try { return goldenByIdOrName(m, id); } catch (e) { die(e.message); }
}

const argv = process.argv.slice(2);
if (!(argv.length === 3 || (argv.length === 4 && argv[3] === "--refreeze"))) {
  die("usage: node freeze-target.js <golden-id> <runA.json> <runB.json> [--refreeze]");
}
const [id, fileA, fileB] = argv;
const refreeze = argv.length === 4;
if (!/^[a-z0-9-]+$/.test(id)) {
  die("golden id '" + id + "' fails the whitelist [a-z0-9-]");
}
if (path.resolve(fileA) === path.resolve(fileB)) {
  die("runA and runB are the SAME path — x2 identity requires two runs");
}
{
  const stA = fs.statSync(fileA);
  const stB = fs.statSync(fileB);
  if (stA.dev === stB.dev && stA.ino === stB.ino) {
    die("runA and runB are the same file (dev:ino alias) — x2 identity");
  }
}

const g = goldenById(id);
function loadRun(fp, label) {
  const raw = fs.readFileSync(fp, "utf8");
  try { assertNoDuplicateKeys(raw, "run " + label); } catch (e) {
    die(e.message); // review-96 C-M1: dup key at any scope = corruption
  }
  return JSON.parse(raw);
}
const a = loadRun(fileA, "A");
const b = loadRun(fileB, "B");

// --- both fresh runs must agree on EVERY conformance channel -----------------
function frameArraysEqual(fa, fb, label) {
  if (fa.length !== fb.length) {
    die(`${label} run lengths differ: ${fa.length} vs ${fb.length}`);
  }
  for (let i = 0; i < fa.length; i++) {
    if (fa[i].f !== fb[i].f || fa[i].h !== fb[i].h) {
      die(`${label} runs diverge at frame ${fa[i].f} — NOT freezing`);
    }
  }
}
frameArraysEqual(a.frames, b.frames, "player");
if (!a.target || !b.target || !Array.isArray(a.target.frames) ||
    !Array.isArray(b.target.frames)) {
  die("a run JSON is missing target.frames (run-target.js output)");
}
frameArraysEqual(a.target.frames, b.target.frames, "target-plane");
if (a.coverage.rngCalls !== b.coverage.rngCalls) {
  die(`rngCalls differ: ${a.coverage.rngCalls} vs ${b.coverage.rngCalls}`);
}
if (a.coverage.rngCallsOutsideStep !== b.coverage.rngCallsOutsideStep) {
  die("rngCallsOutsideStep differ between runs");
}
if (a.coverage.rngCallsOutsideStep !== 1) {
  die(`rngCallsOutsideStep is ${a.coverage.rngCallsOutsideStep}, expected 1 ` +
      "(CHECKSUM.md §1.3/§6 — the startTargetGame background draw)");
}

// --- both runs must carry the golden's exact params --------------------------
const tracePath = path.join(GOLDENS_DIR, g.trace);
const traceSha256 = sha256File(tracePath);
for (const [label, r] of [["A", a], ["B", b]]) {
  const m = r.meta;
  const want = { frames: g.frames, seed: g.seed, p1: g.char, p2: null,
    stage: null, cpu: false, difficulty: null, seedRandom: true, fdlibm: true,
    mode: "target", tstage: g.tstage };
  for (const k of Object.keys(want)) {
    if (m[k] !== want[k]) {
      die(`run ${label} meta.${k} = ${m[k]}, manifest wants ${want[k]}`);
    }
  }
  if (path.basename(String(m.trace)) !== g.trace) {
    die(`run ${label} trace ${m.trace} is not the golden's ${g.trace}`);
  }
  if (r.frames.length !== g.frames) {
    die(`run ${label} has ${r.frames.length} player frames, wants ${g.frames}`);
  }
  if (r.target.frames.length !== g.frames) {
    die(`run ${label} has ${r.target.frames.length} target frames, wants ${g.frames}`);
  }
}

// --- x2 browser identity (review-94 L1) --------------------------------------
if (typeof a.meta.browser !== "string" || typeof a.meta.version !== "string" ||
    typeof b.meta.browser !== "string" || typeof b.meta.version !== "string") {
  die("run meta.browser/meta.version missing or not strings — x2 browser " +
      "identity requires both runs to record their browser");
}
if (a.meta.browser !== b.meta.browser || a.meta.version !== b.meta.version) {
  die(`runs A/B used DIFFERENT browsers — A ${a.meta.browser} ` +
      `${a.meta.version} vs B ${b.meta.browser} ${b.meta.version} ` +
      "(x2 identity evidence requires one browser; the channel-Chrome vs " +
      "bundled-Chromium fallback pair is refused)");
}

// --- write a frozen file deterministically, guarding overwrite ---------------
function writeFrozen(outPath, text) {
  if (fs.existsSync(outPath)) {
    const old = fs.readFileSync(outPath, "utf8");
    if (old === text) {
      console.log(`FROZEN ${path.basename(outPath)}: unchanged (byte-identical re-freeze)`);
      return;
    }
    if (!refreeze) {
      die(`${outPath} exists and DIFFERS from this recording. Overwriting ` +
          "requires --refreeze and is only legitimate with a spec version bump.");
    }
    let oldParsed = null;
    try {
      assertNoDuplicateKeys(old, "existing frozen " + path.basename(outPath));
      oldParsed = JSON.parse(old);
    } catch (e) {
      die(`${outPath} exists but is not parseable JSON (${e.message}) — refusing`);
    }
    const oldSpec = checkSpec(oldParsed.specVersion, "existing frozen " + outPath);
    if (oldSpec === CUR_SPEC) {
      die(`--refreeze refused: ${outPath} carries specVersion ${oldSpec} and the ` +
          "current spec is ALSO the same — differing same-spec is drift.");
    }
  }
  fs.writeFileSync(outPath, text);
  console.log(`FROZEN ${path.basename(outPath)}`);
}

// PLAYER stream: M0 format (params carry mode/tstage as EXTRA keys the
// UNCHANGED verify-stream.js ignores; verify-target-stream.js pins them).
const playerParams = { trace: g.trace, traceSha256: traceSha256,
  frames: g.frames, seed: g.seed, p1: g.char, p2: null, stage: null,
  cpu: false, difficulty: null, fdlibm: true, seedRandom: true,
  mode: "target", tstage: g.tstage };
{
  const L = [];
  L.push("{");
  L.push('"golden": ' + JSON.stringify(g.name) + ",");
  L.push('"specVersion": ' + CUR_SPEC + ",");
  L.push('"params": ' + JSON.stringify(playerParams) + ",");
  L.push('"rngCalls": ' + a.coverage.rngCalls + ",");
  L.push('"rngCallsOutsideStep": ' + a.coverage.rngCallsOutsideStep + ",");
  L.push('"streamSha256": ' + JSON.stringify(streamDigest(a.frames)) + ",");
  L.push('"frames": [');
  for (let i = 0; i < a.frames.length; i++) {
    L.push(JSON.stringify({ f: a.frames[i].f, h: a.frames[i].h }) +
      (i === a.frames.length - 1 ? "" : ","));
  }
  L.push("]");
  L.push("}");
  writeFrozen(path.join(GOLDENS_DIR, g.name + ".sha256.json"), L.join("\n") + "\n");
}

// TARGET-PLANE stream: its own format (verify-target-stream.js). Pins the
// player-stream sibling's name + the mode/tstage params so the two files
// cannot be judged against a mismatched player recording.
{
  const params = { trace: g.trace, traceSha256: traceSha256, frames: g.frames,
    seed: g.seed, char: g.char, tstage: g.tstage, mode: "target",
    minTargets: g.minTargets, wantArticles: g.wantArticles,
    finalTargetsDestroyed: a.coverage.target.targetsDestroyed,
    finalEndTargetGame: a.coverage.target.endTargetGame };
  const L = [];
  L.push("{");
  L.push('"golden": ' + JSON.stringify(g.name) + ",");
  L.push('"stream": "target-plane",');
  L.push('"specVersion": ' + CUR_SPEC + ",");
  L.push('"playerStream": ' + JSON.stringify(g.name + ".sha256.json") + ",");
  L.push('"params": ' + JSON.stringify(params) + ",");
  L.push('"streamSha256": ' + JSON.stringify(streamDigest(a.target.frames)) + ",");
  L.push('"frames": [');
  for (let i = 0; i < a.target.frames.length; i++) {
    L.push(JSON.stringify({ f: a.target.frames[i].f, h: a.target.frames[i].h }) +
      (i === a.target.frames.length - 1 ? "" : ","));
  }
  L.push("]");
  L.push("}");
  writeFrozen(path.join(GOLDENS_DIR, g.name + ".target.sha256.json"),
    L.join("\n") + "\n");
}

console.log(`FROZEN ${g.name}: ${a.frames.length} player + ` +
  `${a.target.frames.length} target frames, rngCalls=${a.coverage.rngCalls}, ` +
  `targetsDestroyed=${a.coverage.target.targetsDestroyed}, specVersion=${CUR_SPEC}`);
