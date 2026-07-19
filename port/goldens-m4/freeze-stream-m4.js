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
//
// MANIFEST GRAMMAR (iter 83 — review-81 round-1 closure, PROCESS §3
// whitelist rule): the freezer is the artifact-minting authority, so it
// validates the WHOLE manifest before trusting any row — exact
// top-level and per-golden key set/order, types, ranges, id/name/trace
// grammar, duplicate id/name/trace rejection, a raw duplicate-JSON-key
// token guard (JSON.parse is silently last-wins), name-derived
// basename-only trace, and resolved-path containment in the golden
// home (no ../ escape). Resembles-but-doesn't-match = corruption =
// refusal, never a partial parse.
"use strict";
const fs = require("fs");
const path = require("path");
const { streamDigest, sha256File, specVersion } =
  require("../../oracle/harness/streamlib");

const GOLDENS_DIR = __dirname; // port/goldens-m4

function die(msg) { console.error("freeze-stream-m4: " + msg); process.exit(1); }
function vdie(msg) { die("manifest grammar — " + msg); }

// The exact per-golden schema, key ORDER included (measured from the
// committed manifest; a reordered or widened row is a reviewed change).
const GOLDEN_KEYS = ["id", "name", "trace", "frames", "seed",
  "p1", "p2", "stage", "cpu", "difficulty"];

function loadManifest() {
  const mPath = path.join(GOLDENS_DIR, "manifest.json");
  const raw = fs.readFileSync(mPath, "utf8");
  let m;
  try { m = JSON.parse(raw); } catch (e) {
    vdie("manifest.json is not valid JSON: " + e.message);
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
  // raw duplicate-JSON-key guard: each schema key token must occur in
  // the raw bytes EXACTLY once per golden (JSON.parse keeps the LAST
  // duplicate silently; a doctored row with two "trace" keys parses
  // clean — refuse it at the byte level).
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
    if (typeof g.id !== "string" || !/^m[0-9]{2}$/.test(g.id)) {
      vdie(where + " id '" + g.id + "' fails ^m[0-9]{2}$");
    }
    if (typeof g.name !== "string" || !/^m[0-9]{2}(-[a-z0-9]+)+$/.test(g.name)) {
      vdie(where + " name '" + g.name + "' fails ^m[0-9]{2}(-[a-z0-9]+)+$");
    }
    if (g.name.slice(0, 3) !== g.id) {
      vdie(where + " name '" + g.name + "' does not begin with its id '" + g.id + "'");
    }
    if (g.trace !== g.name + ".trace.json") {
      vdie(where + " trace '" + g.trace + "' != name-derived '" + g.name +
        ".trace.json' (basename-only by construction; no path escape " +
        "from the golden home)");
    }
    if (path.basename(g.trace) !== g.trace) {
      vdie(where + " trace '" + g.trace + "' is not a bare basename");
    }
    if (path.dirname(path.resolve(dir, g.trace)) !== dir) {
      vdie(where + " trace resolves outside the golden home " + dir);
    }
    if (path.dirname(path.resolve(dir, g.name + ".sha256.json")) !== dir) {
      vdie(where + " frozen-stream path resolves outside the golden home " + dir);
    }
    if (!Number.isInteger(g.frames) || g.frames < 1 || g.frames > 999999) {
      vdie(where + " frames " + g.frames + " is not an integer in 1..999999");
    }
    if (!Number.isInteger(g.seed) || g.seed < 0 || g.seed > 4294967295) {
      vdie(where + " seed " + g.seed + " is not an integer in 0..2^32-1");
    }
    if (!Number.isInteger(g.p1) || g.p1 < 0 || g.p1 > 4) {
      vdie(where + " p1 " + g.p1 + " outside the char domain 0-4");
    }
    if (!Number.isInteger(g.p2) || g.p2 < 0 || g.p2 > 4) {
      vdie(where + " p2 " + g.p2 + " outside the char domain 0-4");
    }
    if (!Number.isInteger(g.stage) || g.stage < 0 || g.stage > 5) {
      vdie(where + " stage " + g.stage + " outside the stage domain 0-5");
    }
    if (typeof g.cpu !== "boolean") vdie(where + " cpu is not a boolean");
    if (g.cpu === true) {
      if (!Number.isInteger(g.difficulty) || g.difficulty < 1 || g.difficulty > 9) {
        vdie(where + " cpu golden difficulty " + g.difficulty +
          " is not an integer in 1..9");
      }
    } else if (g.difficulty !== null) {
      vdie(where + " non-cpu golden difficulty must be null, got " + g.difficulty);
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
  if (!g) die("golden not in port/goldens-m4/manifest.json: " + id);
  return g;
}

// STRICT argv (no silent extra-arg tolerance): exactly
// <id> <runA> <runB> or <id> <runA> <runB> --refreeze.
const argv = process.argv.slice(2);
if (!(argv.length === 3 || (argv.length === 4 && argv[3] === "--refreeze"))) {
  die("usage: node freeze-stream-m4.js <golden-id> <runA.json> <runB.json> [--refreeze]");
}
const [id, fileA, fileB] = argv;
const refreeze = argv.length === 4;
if (!/^[a-z0-9-]+$/.test(id)) {
  die("golden id '" + id + "' fails the whitelist [a-z0-9-]");
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
