#!/usr/bin/env node
// check-target-quality.js — the TARGET-mode gameplay-quality contract,
// checked MECHANICALLY from a run-target.js run JSON before freezing
// (fix_plan §M4 task 11; the M0 quality contract's target analog):
//   1. gameMode === 5 (the run really entered target test);
//   2. coverage.playing === true at the final frame (match still live);
//   3. exactly one active slot (target mode is single-player) with
//      stocks == 1 (startTargetGame forces it, targetplay.js:208);
//   4. NO /^DEAD/ action state seen — the player never SDs in-trace (a
//      DEADDOWN etc. means the trace fell into a blastzone: target-mode
//      isFinalDeath is unconditionally true, so a KO would end the run);
//   5. coverage.target.targetsDestroyed >= the golden's minTargets;
//   6. coverage.target.endTargetGame === false throughout (all-broken is
//      never reached in-trace: finishGame is FOH plane, trapped in C);
//   7. wantArticles => coverage.maxArticles > 0 (live projectile flight).
// STRICT (whitelist rule, PROCESS §3): a missing/odd-shaped field is
// corruption -> loud death, never a partial pass.
//
// Usage: node check-target-quality.js <run.json> <minTargets> <wantArticles 0|1>
"use strict";
const fs = require("fs");

function die(msg) { console.error("TARGET QUALITY FAIL: " + msg); process.exit(1); }

const runPath = process.argv[2];
const minTargets = parseInt(process.argv[3], 10);
const wantArticles = process.argv[4] === "1";
// canonical integer text only (the M2 exact-token class, iter 96):
// "2x"/zero-padded argv is corruption, never parseInt-normalized.
if (!runPath || !Number.isInteger(minTargets) ||
    String(minTargets) !== process.argv[3] ||
    !(process.argv[4] === "0" || process.argv[4] === "1")) {
  console.error("usage: node check-target-quality.js <run.json> <minTargets> <wantArticles 0|1>");
  process.exit(1);
}
const run = JSON.parse(fs.readFileSync(runPath, "utf8"));

const cov = run.coverage;
if (!cov || typeof cov !== "object") die("run JSON has no coverage object");
if (cov.gameMode !== 5) die("coverage.gameMode !== 5: " + cov.gameMode);
if (cov.playing !== true) die("coverage.playing !== true (match not live)");

if (!Array.isArray(cov.stocks) || cov.stocks.length !== 1) {
  die("coverage.stocks is not the 1-active-slot array: " + JSON.stringify(cov.stocks));
}
if (!Number.isInteger(cov.stocks[0]) || cov.stocks[0] !== 1) {
  die("target player stocks != 1: " + cov.stocks[0]);
}

const seen = cov.actionStatesSeen;
if (!seen || typeof seen !== "object" || Array.isArray(seen)) {
  die("coverage.actionStatesSeen is not an object");
}
const dead = Object.keys(seen).filter((k) => /^DEAD/.test(k));
if (dead.length > 0) {
  die("a /^DEAD/ action state was seen (player SD'd in-trace): " + dead.join(","));
}

const t = cov.target;
if (!t || typeof t !== "object") die("coverage.target is missing");
if (!Number.isInteger(t.targetsDestroyed)) {
  die("coverage.target.targetsDestroyed is not an integer: " + t.targetsDestroyed);
}
if (t.targetsDestroyed < minTargets) {
  die(`targetsDestroyed ${t.targetsDestroyed} < minTargets ${minTargets}`);
}
if (t.endTargetGame !== false) {
  die("coverage.target.endTargetGame !== false (all targets broke in-trace)");
}
if (wantArticles) {
  if (!Number.isInteger(cov.maxArticles) || cov.maxArticles < 1) {
    die("wantArticles but coverage.maxArticles < 1 (no live projectile): " +
        cov.maxArticles);
  }
}

console.log("TARGET QUALITY OK: gameMode=5 stocks=[1] targetsDestroyed=" +
  t.targetsDestroyed + " endTargetGame=false maxArticles=" + cov.maxArticles +
  " KO=none");
