#!/usr/bin/env node
// verify-target-stream.js — judge a TARGET-plane run against its FROZEN
// target-plane stream (fix_plan §M4 task 11; the iter-63 separate-stream
// convention; Tier A+ — it is a judge path). The spec-v1 PLAYER stream is
// judged SEPARATELY by the UNCHANGED oracle/harness/verify-stream.js; this
// verifier owns the target plane + the full target metadata binding
// (review-94 H2/M1 hardened, iter 96; review-96 C-M2/G-M4/G-L7
// hardened, iter 98).
//
// Usage: node verify-target-stream.js <run.json> <frozen.target.sha256.json>
//
// PASS ("TARGET STREAM MATCH …", exit 0) requires ALL of:
//   0. DUP-KEY SCAN (review-96 C-M2): the SHARED string-aware
//      duplicate-JSON-key scanner (json-dup-key-scan.js) over the RAW
//      bytes of ALL THREE parsed files (run, frozen, sibling) BEFORE
//      any JSON.parse — a duplicate key at ANY scope (e.g. a frozen
//      row {"f":999,"f":1,...}) is last-wins corruption, death.
//   1. frozen-file EXACT SCHEMA (whitelist, fail closed — review-94 M1):
//      top-level key ORDER {golden,stream,specVersion,playerStream,
//      params,streamSha256,frames}; params key ORDER {trace,traceSha256,
//      frames,seed,char,tstage,mode,minTargets,wantArticles,
//      finalTargetsDestroyed,finalEndTargetGame}; every field typed +
//      domain-checked; frames = rows of exactly {f,h}, f == i+1,
//      h 64-lowercase-hex; frames length == params.frames >= 1 (a
//      "0/0 MATCH" is structurally unreachable); basename ==
//      golden + ".target.sha256.json"; streamSha256 integrity seal.
//   2. MECHANICAL QUALITY from the frozen metadata (review-94 H2):
//      finalTargetsDestroyed >= minTargets; finalEndTargetGame == false.
//      HONEST COVERAGE: wantArticles has NO stream-derivable witness
//      (the target envelope carries no article field; player frames are
//      hashes) — it binds frozen<->manifest below; article presence was
//      proven live at record time (check-target-quality.js) and is baked
//      into the frozen player hashes any conforming replay reproduces.
//   3. MANIFEST BINDING (review-94 H2): the COMMITTED manifest-target
//      passes the SHARED strict validator and carries a row named
//      frozen.golden whose {trace,frames,seed,char,tstage,minTargets,
//      wantArticles} equal the frozen params exactly. FINALS BINDING
//      (review-96 G-L7, assertion form): finalTargetsDestroyed >= the
//      VALIDATED manifest row's minTargets directly (the finals are NOT
//      re-derivable from sealed content — frozen frames are SHA-256
//      hashes of an envelope that is not stored; pre-registered
//      determination, iter 98 — so the binding asserts what IS frozen:
//      quality vs the manifest row here + run-finals equality in 9).
//   4. SIBLING BINDING (review-94 H2; review-96 C-M2 exact schema):
//      frozen.playerStream == golden + ".sha256.json"; the sibling next
//      to the frozen target file exists and gets the SAME whitelist
//      treatment as the frozen target file — top-level key ORDER
//      {golden,specVersion,params,rngCalls,rngCallsOutsideStep,
//      streamSha256,frames}, params key ORDER {trace,traceSha256,
//      frames,seed,p1,p2,stage,cpu,difficulty,fdlibm,seedRandom,mode,
//      tstage}, frame rows exactly {f,h} — its OWN streamSha256 seal
//      verifies over its frames (numbering 1..N), specVersion matches,
//      its params cross-pin the target params (trace/traceSha256/
//      frames/seed/p1==char/mode/tstage) AND carry the target-mode
//      value pins (fdlibm/seedRandom true, p2/stage/difficulty null,
//      cpu false); its rngCallsOutsideStep == 1.
//   5. spec pin: frozen specVersion == oracle/CHECKSUM.md's current.
//   6. trace pin: the golden trace (next to the frozen file) hashes to
//      params.traceSha256, and the run consumed that trace.
//   7. run pins (required fields, TYPED — undefined never compares
//      equal): meta {frames,seed,tstage,p1 ints; mode "target"} + the
//      M0-discipline pins (review-96 G-M4): fdlibm === true, seedRandom
//      === true, and the target-mode null/false EXPLICIT expectations
//      p2 === null, stage === null, cpu === false, difficulty === null
//      (an absent field is undefined and never compares equal);
//      coverage {rngCalls,rngCallsOutsideStep} == the SIBLING's frozen
//      values; coverage.target {targetsDestroyed int, endTargetGame
//      bool}.
//   8. the target-plane stream: EXACT per-frame {f,h} equality, FULL
//      length (never epsilon/prefix); run rows exactly {f,h} (review-96
//      C-M2 — unknown keys = death), f == i+1, 64-lowercase-hex.
//   9. finals: run targetsDestroyed/endTargetGame == the frozen
//      finalTargetsDestroyed/finalEndTargetGame.
// Any failure prints the reason and exits nonzero (divergence exits 2).
"use strict";
const fs = require("fs");
const path = require("path");
const { streamDigest, sha256File, specVersion } =
  require("../../oracle/harness/streamlib");
const { loadValidatedManifest } = require("./validate-target-manifest");
const { assertNoDuplicateKeys } = require("./json-dup-key-scan");

function die(msg, code) {
  console.error("TARGET STREAM MISMATCH: " + msg);
  process.exit(code || 1);
}

const [runPath, frozenPath] = process.argv.slice(2);
if (!runPath || !frozenPath) {
  console.error("usage: node verify-target-stream.js <run.json> <frozen.target.sha256.json>");
  process.exit(1);
}
let run, frozen;
try {
  const rawRun = fs.readFileSync(runPath, "utf8");
  assertNoDuplicateKeys(rawRun, "run JSON"); // review-96 C-M2
  run = JSON.parse(rawRun);
} catch (e) {
  die("run JSON unreadable/unparseable: " + e.message);
}
try {
  const rawFrozen = fs.readFileSync(frozenPath, "utf8");
  assertNoDuplicateKeys(rawFrozen, "frozen file"); // review-96 C-M2
  frozen = JSON.parse(rawFrozen);
} catch (e) {
  die("frozen file unreadable/unparseable: " + e.message);
}

const isInt = Number.isInteger;
const HEX64 = /^[0-9a-f]{64}$/;
function keysExact(obj, want, what) {
  if (typeof obj !== "object" || obj === null || Array.isArray(obj)) {
    die(what + " is not an object");
  }
  const ks = Object.keys(obj);
  if (ks.length !== want.length || want.some((k, i) => ks[i] !== k)) {
    die(what + " key set/order {" + ks.join(",") + "} != {" +
        want.join(",") + "} (exact schema, fail closed)");
  }
}

// 1. frozen-file exact schema + integrity ------------------------------------
keysExact(frozen, ["golden", "stream", "specVersion", "playerStream",
  "params", "streamSha256", "frames"], "frozen top level");
if (typeof frozen.golden !== "string" ||
    !/^t[0-9]{2}(-[a-z0-9]+)+$/.test(frozen.golden)) {
  die("frozen golden " + JSON.stringify(frozen.golden) +
      " fails ^t[0-9]{2}(-[a-z0-9]+)+$");
}
if (frozen.stream !== "target-plane") {
  die(`frozen file stream is ${JSON.stringify(frozen.stream)}, not "target-plane"`);
}
if (!isInt(frozen.specVersion) || frozen.specVersion < 1) {
  die("frozen specVersion " + JSON.stringify(frozen.specVersion) +
      " is not a strict positive integer");
}
if (frozen.playerStream !== frozen.golden + ".sha256.json") {
  die("frozen playerStream " + JSON.stringify(frozen.playerStream) +
      " != name-derived " + frozen.golden + ".sha256.json (sibling binding)");
}
if (typeof frozen.streamSha256 !== "string" || !HEX64.test(frozen.streamSha256)) {
  die("frozen streamSha256 is not 64 lowercase hex");
}
if (path.basename(frozenPath) !== frozen.golden + ".target.sha256.json") {
  die(`frozen file ${path.basename(frozenPath)} does not match its embedded ` +
      `golden name ${frozen.golden}`);
}
const P = frozen.params;
keysExact(P, ["trace", "traceSha256", "frames", "seed", "char", "tstage",
  "mode", "minTargets", "wantArticles", "finalTargetsDestroyed",
  "finalEndTargetGame"], "frozen params");
if (P.trace !== frozen.golden + ".trace.json") {
  die("frozen params.trace " + JSON.stringify(P.trace) +
      " != name-derived " + frozen.golden + ".trace.json");
}
if (typeof P.traceSha256 !== "string" || !HEX64.test(P.traceSha256)) {
  die("frozen params.traceSha256 is not 64 lowercase hex");
}
if (!isInt(P.frames) || P.frames < 1 || P.frames > 999999) {
  die("frozen params.frames " + JSON.stringify(P.frames) +
      " is not an integer in 1..999999 (a 0-frame stream is never judgeable)");
}
if (!isInt(P.seed) || P.seed < 0 || P.seed > 4294967295) {
  die("frozen params.seed outside 0..2^32-1");
}
if (!isInt(P.char) || P.char < 0 || P.char > 4) {
  die("frozen params.char outside the char domain 0-4");
}
if (!isInt(P.tstage) || P.tstage < 0 || P.tstage > 9) {
  die("frozen params.tstage outside the target-stage domain 0-9");
}
if (P.mode !== "target") die("frozen params.mode is not \"target\"");
if (!isInt(P.minTargets) || P.minTargets < 1 || P.minTargets > 10) {
  die("frozen params.minTargets outside 1..10");
}
if (typeof P.wantArticles !== "boolean") {
  die("frozen params.wantArticles is not a boolean");
}
// 0..20: the double-destroy quirk can count a target twice, so the count
// may exceed the 10-target authored cap but never 2x it.
if (!isInt(P.finalTargetsDestroyed) || P.finalTargetsDestroyed < 0 ||
    P.finalTargetsDestroyed > 20) {
  die("frozen params.finalTargetsDestroyed outside 0..20");
}
if (typeof P.finalEndTargetGame !== "boolean") {
  die("frozen params.finalEndTargetGame is not a boolean");
}
if (!Array.isArray(frozen.frames) || frozen.frames.length !== P.frames) {
  die(`frozen stream has ${frozen.frames && frozen.frames.length} frames but ` +
      `params.frames=${P.frames}`);
}
for (let i = 0; i < frozen.frames.length; i++) {
  const row = frozen.frames[i];
  keysExact(row, ["f", "h"], "frozen frames[" + i + "]");
  if (row.f !== i + 1) die(`frozen frame numbering broken at index ${i}`);
  if (typeof row.h !== "string" || !HEX64.test(row.h)) {
    die(`frozen frames[${i}].h is not 64 lowercase hex`);
  }
}
if (streamDigest(frozen.frames) !== frozen.streamSha256) {
  die("frozen file streamSha256 seal does not match its frames (corrupt/edited)");
}

// 2. mechanical quality from the frozen metadata (review-94 H2) --------------
if (P.finalTargetsDestroyed < P.minTargets) {
  die(`mechanical quality — frozen finalTargetsDestroyed ` +
      `${P.finalTargetsDestroyed} < minTargets ${P.minTargets}`);
}
if (P.finalEndTargetGame !== false) {
  die("mechanical quality — frozen finalEndTargetGame must be false " +
      "(all-broken is never reached in the golden domain)");
}

// 3. manifest binding (review-94 H2; the SHARED strict validator) ------------
let manifest;
try { manifest = loadValidatedManifest(); } catch (e) {
  die("committed manifest-target.json failed the shared validator: " + e.message);
}
const row = manifest.goldens.find((x) => x.name === frozen.golden);
if (!row) {
  die("frozen golden " + frozen.golden +
      " has NO row in the committed manifest-target.json");
}
for (const k of ["trace", "frames", "seed", "char", "tstage",
                 "minTargets", "wantArticles"]) {
  if (P[k] !== row[k]) {
    die(`frozen params.${k} = ${JSON.stringify(P[k])} != committed manifest ` +
        `row's ${JSON.stringify(row[k])} (metadata binding)`);
  }
}
// FINALS BINDING (review-96 G-L7, assertion form — the finals are not
// re-derivable from the sealed hashes, pre-registered iter 98): the
// frozen finalTargetsDestroyed must satisfy the VALIDATED manifest
// row's quality floor DIRECTLY (belt over the frozen-params path; the
// run-side equality lives in step 9).
if (P.finalTargetsDestroyed < row.minTargets) {
  die(`frozen finalTargetsDestroyed ${P.finalTargetsDestroyed} < the ` +
      `committed manifest row's minTargets ${row.minTargets} ` +
      "(manifest-anchored finals binding)");
}

// 4. player-stream sibling binding (review-94 H2) ----------------------------
const sibPath = path.join(path.dirname(frozenPath), frozen.playerStream);
if (!fs.existsSync(sibPath)) die("player-stream sibling missing: " + sibPath);
let sib;
try {
  const rawSib = fs.readFileSync(sibPath, "utf8");
  assertNoDuplicateKeys(rawSib, "player-stream sibling"); // review-96 C-M2
  sib = JSON.parse(rawSib);
} catch (e) {
  die("player-stream sibling unparseable: " + e.message);
}
// Sibling EXACT SCHEMA (review-96 C-M2 — the same whitelist treatment
// the frozen target file got in iter-96; key ORDER measured from the
// frozen siblings == freeze-target.js's literal emission order).
keysExact(sib, ["golden", "specVersion", "params", "rngCalls",
  "rngCallsOutsideStep", "streamSha256", "frames"], "sibling top level");
keysExact(sib.params, ["trace", "traceSha256", "frames", "seed", "p1", "p2",
  "stage", "cpu", "difficulty", "fdlibm", "seedRandom", "mode", "tstage"],
  "sibling params");
if (sib.golden !== frozen.golden) {
  die(`sibling golden ${JSON.stringify(sib.golden)} != ${frozen.golden}`);
}
if (sib.specVersion !== frozen.specVersion) {
  die(`sibling specVersion ${sib.specVersion} != frozen ${frozen.specVersion}`);
}
for (const [sk, fv, what] of [
  ["trace", P.trace, "trace"],
  ["traceSha256", P.traceSha256, "traceSha256"],
  ["frames", P.frames, "frames"],
  ["seed", P.seed, "seed"],
  ["p1", P.char, "p1==char"],
  ["mode", "target", "mode"],
  ["tstage", P.tstage, "tstage"],
  // target-mode value pins (review-96 G-M4 — explicit, typed; an
  // absent field is undefined and never compares equal):
  ["fdlibm", true, "fdlibm"],
  ["seedRandom", true, "seedRandom"],
  ["p2", null, "target-mode p2"],
  ["stage", null, "target-mode stage"],
  ["cpu", false, "target-mode cpu"],
  ["difficulty", null, "target-mode difficulty"],
]) {
  if (sib.params[sk] !== fv) {
    die(`sibling params.${sk} = ${JSON.stringify(sib.params[sk])} != target ` +
        `params' ${JSON.stringify(fv)} (${what} cross-pin)`);
  }
}
if (!Array.isArray(sib.frames) || sib.frames.length !== P.frames) {
  die(`sibling stream has ${sib.frames && sib.frames.length} frames, ` +
      `want ${P.frames}`);
}
for (let i = 0; i < sib.frames.length; i++) {
  keysExact(sib.frames[i], ["f", "h"], "sibling frames[" + i + "]");
  if (sib.frames[i].f !== i + 1 ||
      typeof sib.frames[i].h !== "string" || !HEX64.test(sib.frames[i].h)) {
    die(`sibling frame row ${i} malformed/misnumbered`);
  }
}
if (typeof sib.streamSha256 !== "string" ||
    streamDigest(sib.frames) !== sib.streamSha256) {
  die("player-stream sibling streamSha256 seal does not match its frames " +
      "(corrupt/edited sibling)");
}
if (!isInt(sib.rngCalls) || sib.rngCalls < 0) {
  die("sibling rngCalls is not a nonnegative integer");
}
if (sib.rngCallsOutsideStep !== 1) {
  die(`sibling rngCallsOutsideStep ${JSON.stringify(sib.rngCallsOutsideStep)} ` +
      "!= 1 (the ONE startTargetGame background draw)");
}

// 5. spec version pin --------------------------------------------------------
const cur = specVersion();
if (frozen.specVersion !== cur) {
  die(`frozen target stream is spec v${frozen.specVersion} but oracle/CHECKSUM.md ` +
      `is v${cur} — re-freeze required (CHECKSUM.md §8)`);
}

// 6. trace pin ---------------------------------------------------------------
const traceFile = path.join(path.dirname(frozenPath), P.trace);
if (!fs.existsSync(traceFile)) die("golden trace missing: " + traceFile);
const th = sha256File(traceFile);
if (th !== P.traceSha256) {
  die(`trace ${P.trace} sha256 ${th} != frozen ${P.traceSha256}`);
}
if (typeof run !== "object" || run === null || Array.isArray(run)) {
  die("run JSON is not an object");
}
if (typeof run.meta !== "object" || run.meta === null) {
  die("run JSON has no meta object");
}
if (typeof run.meta.trace !== "string" ||
    path.basename(run.meta.trace) !== P.trace) {
  die(`run consumed trace ${run.meta.trace}, frozen stream is for ${P.trace}`);
}

// 7. run pins (typed; undefined never compares equal) ------------------------
if (!isInt(run.meta.frames) || run.meta.frames !== P.frames) {
  die(`run meta.frames = ${JSON.stringify(run.meta.frames)}, frozen ${P.frames}`);
}
if (!isInt(run.meta.seed) || run.meta.seed !== P.seed) {
  die(`run meta.seed = ${JSON.stringify(run.meta.seed)}, frozen ${P.seed}`);
}
if (run.meta.mode !== "target") {
  die(`run meta.mode = ${JSON.stringify(run.meta.mode)}, want "target"`);
}
if (!isInt(run.meta.tstage) || run.meta.tstage !== P.tstage) {
  die(`run meta.tstage = ${JSON.stringify(run.meta.tstage)}, frozen ${P.tstage}`);
}
if (!isInt(run.meta.p1) || run.meta.p1 !== P.char) {
  die(`run char (meta.p1) = ${JSON.stringify(run.meta.p1)}, frozen ${P.char}`);
}
// M0-discipline run identity pins (review-96 G-M4): fdlibm/seedRandom
// must be EXPLICIT true, and the target-mode null/false expectations
// are asserted EXPLICITLY — an absent field is undefined and undefined
// never equals null/true/false, so undefined-equals-undefined is
// impossible by construction.
if (run.meta.fdlibm !== true) {
  die(`run meta.fdlibm = ${JSON.stringify(run.meta.fdlibm)}, want true ` +
      "(a native-libm run can never be judged against a frozen golden)");
}
if (run.meta.seedRandom !== true) {
  die(`run meta.seedRandom = ${JSON.stringify(run.meta.seedRandom)}, want ` +
      "true (an unseeded run can never be judged against a frozen golden)");
}
if (run.meta.p2 !== null) {
  die(`run meta.p2 = ${JSON.stringify(run.meta.p2)}, target mode requires ` +
      "explicit null (absent/undefined never passes)");
}
if (run.meta.stage !== null) {
  die(`run meta.stage = ${JSON.stringify(run.meta.stage)}, target mode ` +
      "requires explicit null (absent/undefined never passes)");
}
if (run.meta.cpu !== false) {
  die(`run meta.cpu = ${JSON.stringify(run.meta.cpu)}, target mode ` +
      "requires explicit false (absent/undefined never passes)");
}
if (run.meta.difficulty !== null) {
  die(`run meta.difficulty = ${JSON.stringify(run.meta.difficulty)}, target ` +
      "mode requires explicit null (absent/undefined never passes)");
}
if (typeof run.coverage !== "object" || run.coverage === null) {
  die("run JSON has no coverage object");
}
if (!isInt(run.coverage.rngCalls) || run.coverage.rngCalls !== sib.rngCalls) {
  die(`run coverage.rngCalls = ${JSON.stringify(run.coverage.rngCalls)} != ` +
      `the frozen player sibling's ${sib.rngCalls}`);
}
if (run.coverage.rngCallsOutsideStep !== sib.rngCallsOutsideStep) {
  die(`run coverage.rngCallsOutsideStep = ` +
      `${JSON.stringify(run.coverage.rngCallsOutsideStep)} != the frozen ` +
      `player sibling's ${sib.rngCallsOutsideStep}`);
}
const tf = run.coverage.target;
if (typeof tf !== "object" || tf === null) {
  die("run coverage has no target finals object");
}
if (!isInt(tf.targetsDestroyed)) {
  die("run coverage.target.targetsDestroyed is not an integer: " +
      JSON.stringify(tf.targetsDestroyed));
}
if (typeof tf.endTargetGame !== "boolean") {
  die("run coverage.target.endTargetGame is not a boolean: " +
      JSON.stringify(tf.endTargetGame));
}

// 8. the target-plane stream: exact equality, every frame, full length -------
const rf = run.target && run.target.frames;
if (!Array.isArray(rf)) die("run JSON has no target.frames");
if (rf.length !== frozen.frames.length) {
  die(`run has ${rf.length} target frames, frozen ${frozen.frames.length} ` +
      "(full-length match required)", 2);
}
for (let i = 0; i < frozen.frames.length; i++) {
  const rrow = rf[i];
  // exactly {f,h} (review-96 C-M2): unknown keys on a run row = death.
  keysExact(rrow, ["f", "h"], "run target frames[" + i + "]");
  if (rrow.f !== i + 1 ||
      typeof rrow.h !== "string" || !HEX64.test(rrow.h)) {
    die(`run target frame row ${i} malformed/misnumbered (want f=${i + 1}, ` +
        "64-lowercase-hex h)");
  }
  if (rrow.h !== frozen.frames[i].h) {
    die(`first target-plane divergence at frame ${frozen.frames[i].f} of ` +
        `${frozen.frames.length}\n  frozen: ${frozen.frames[i].h}\n  run:    ` +
        rrow.h, 2);
  }
}

// 9. target-plane finals -----------------------------------------------------
if (tf.targetsDestroyed !== P.finalTargetsDestroyed) {
  die(`final targetsDestroyed ${tf.targetsDestroyed} != frozen ` +
      `${P.finalTargetsDestroyed}`, 2);
}
if (tf.endTargetGame !== P.finalEndTargetGame) {
  die(`final endTargetGame ${tf.endTargetGame} != frozen ` +
      `${P.finalEndTargetGame}`, 2);
}

console.log(`TARGET STREAM MATCH ${frozen.golden}: ${frozen.frames.length}/` +
  `${frozen.frames.length} target frames exact, targetsDestroyed=` +
  `${tf.targetsDestroyed} (>= minTargets ${P.minTargets}), endTargetGame=` +
  `${tf.endTargetGame}, sibling seal OK, manifest bound, specVersion=${cur}`);
process.exit(0);
