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
// before trusting any row — exact top-level and per-golden key set/order,
// types, ranges, id/name/trace grammar (^t[0-9]{2}$), duplicate
// rejection, a raw duplicate-JSON-key token guard, name-derived
// basename-only trace, and resolved-path containment in the golden home.
"use strict";
const fs = require("fs");
const path = require("path");
const { streamDigest, sha256File, specVersion } =
  require("../../oracle/harness/streamlib");

const GOLDENS_DIR = __dirname; // port/goldens-m4

function die(msg) { console.error("freeze-target: " + msg); process.exit(1); }
function vdie(msg) { die("manifest grammar — " + msg); }

function checkSpec(v, what) {
  if (typeof v !== "number" || !Number.isInteger(v) || v < 1) {
    die(what + " specVersion " + JSON.stringify(v) + " is not a strict " +
      "positive integer — corruption, refusing");
  }
  return v;
}
const CUR_SPEC = checkSpec(specVersion(), "current oracle/CHECKSUM.md");

const GOLDEN_KEYS = ["id", "name", "trace", "frames", "seed",
  "char", "tstage", "minTargets", "wantArticles"];

function loadManifest() {
  const mPath = path.join(GOLDENS_DIR, "manifest-target.json");
  const raw = fs.readFileSync(mPath, "utf8");
  let m;
  try { m = JSON.parse(raw); } catch (e) {
    vdie("manifest-target.json is not valid JSON: " + e.message);
  }
  if (typeof m !== "object" || m === null || Array.isArray(m)) {
    vdie("top level is not an object");
  }
  const topKeys = Object.keys(m).sort().join(",");
  if (topKeys !== "comment,goldens") {
    vdie("top-level keys {" + topKeys + "} != {comment,goldens} (exact schema)");
  }
  if (typeof m.comment !== "string") vdie("comment is not a string");
  if (!Array.isArray(m.goldens) || m.goldens.length < 1) {
    vdie("goldens is not a nonempty array");
  }
  for (const k of GOLDEN_KEYS) {
    const tok = JSON.stringify(k) + ":";
    let cnt = 0, i = -1;
    while ((i = raw.indexOf(tok, i + 1)) !== -1) cnt++;
    if (cnt !== m.goldens.length) {
      vdie("raw token " + tok + " occurs " + cnt + " times, want exactly " +
        m.goldens.length + " (one per golden; a duplicated JSON key is " +
        "silently last-wins — corruption, refuse)");
    }
  }
  const dir = path.resolve(GOLDENS_DIR);
  const ids = new Set(), names = new Set(), traces = new Set();
  m.goldens.forEach(function (g, idx) {
    const where = "goldens[" + idx + "]";
    if (typeof g !== "object" || g === null || Array.isArray(g)) {
      vdie(where + " is not an object");
    }
    const keys = Object.keys(g);
    if (keys.length !== GOLDEN_KEYS.length ||
        GOLDEN_KEYS.some(function (k, j) { return keys[j] !== k; })) {
      vdie(where + " key set/order {" + keys.join(",") + "} != {" +
        GOLDEN_KEYS.join(",") + "} (exact schema, fail closed)");
    }
    if (typeof g.id !== "string" || !/^t[0-9]{2}$/.test(g.id)) {
      vdie(where + " id '" + g.id + "' fails ^t[0-9]{2}$");
    }
    if (typeof g.name !== "string" || !/^t[0-9]{2}(-[a-z0-9]+)+$/.test(g.name)) {
      vdie(where + " name '" + g.name + "' fails ^t[0-9]{2}(-[a-z0-9]+)+$");
    }
    if (g.name.slice(0, 3) !== g.id) {
      vdie(where + " name '" + g.name + "' does not begin with its id '" + g.id + "'");
    }
    if (g.trace !== g.name + ".trace.json") {
      vdie(where + " trace '" + g.trace + "' != name-derived '" + g.name +
        ".trace.json' (basename-only by construction)");
    }
    if (path.basename(g.trace) !== g.trace) {
      vdie(where + " trace '" + g.trace + "' is not a bare basename");
    }
    if (path.dirname(path.resolve(dir, g.trace)) !== dir) {
      vdie(where + " trace resolves outside the golden home " + dir);
    }
    for (const suffix of [".sha256.json", ".target.sha256.json"]) {
      if (path.dirname(path.resolve(dir, g.name + suffix)) !== dir) {
        vdie(where + " frozen path " + suffix + " resolves outside " + dir);
      }
    }
    if (!Number.isInteger(g.frames) || g.frames < 1 || g.frames > 999999) {
      vdie(where + " frames " + g.frames + " is not an integer in 1..999999");
    }
    if (!Number.isInteger(g.seed) || g.seed < 0 || g.seed > 4294967295) {
      vdie(where + " seed " + g.seed + " is not an integer in 0..2^32-1");
    }
    if (!Number.isInteger(g.char) || g.char < 0 || g.char > 4) {
      vdie(where + " char " + g.char + " outside the char domain 0-4");
    }
    if (!Number.isInteger(g.tstage) || g.tstage < 0 || g.tstage > 9) {
      vdie(where + " tstage " + g.tstage + " outside the target-stage domain 0-9");
    }
    if (!Number.isInteger(g.minTargets) || g.minTargets < 1 || g.minTargets > 10) {
      vdie(where + " minTargets " + g.minTargets + " outside 1..10");
    }
    if (typeof g.wantArticles !== "boolean") {
      vdie(where + " wantArticles is not a boolean");
    }
    if (ids.has(g.id)) vdie("duplicate golden id " + g.id);
    if (names.has(g.name)) vdie("duplicate golden name " + g.name);
    if (traces.has(g.trace)) vdie("duplicate golden trace " + g.trace);
    ids.add(g.id); names.add(g.name); traces.add(g.trace);
  });
  return m;
}

function goldenById(id) {
  const m = loadManifest();
  const g = m.goldens.find((x) => x.id === id || x.name === id);
  if (!g) die("golden not in port/goldens-m4/manifest-target.json: " + id);
  return g;
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
const a = JSON.parse(fs.readFileSync(fileA, "utf8"));
const b = JSON.parse(fs.readFileSync(fileB, "utf8"));

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
    try { oldParsed = JSON.parse(old); } catch (e) {
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
