#!/usr/bin/env node
// check-quality.js — the M0 gameplay-quality contract, checked
// MECHANICALLY from a harness run JSON (M4 task 5; the oracle manifest's
// record-time contract, applied to every M4 golden before freezing):
//   1. >=1 KO: some /^DEAD/ key in coverage.actionStatesSeen;
//   2. >=1 real hit: some /^DAMAGE/ or ^CAPTUREDAMAGE key;
//   3. EVERY active port ends >=1 stock: coverage.stocks has exactly one
//      entry per ACTIVE port (pagelib pushes for playerType[i] > -1),
//      each an integer >= 1. The expected count is --ports N, default 2
//      (A46: a four-port golden passes --ports 4; every pre-A46 call
//      site omits the flag and is byte-unchanged);
//   4. the match is still live: coverage.playing === true and
//      coverage.gameMode === 3 (a 0-stock/post-match endpoint means
//      post-match frames polluted the stream — the seed-7314 lesson).
// STRICT (whitelist rule, PROCESS §3): a missing/odd-shaped field is
// corruption -> loud death, never a partial pass.
//
// Usage: node check-quality.js <run.json> [--ports N]
"use strict";
const fs = require("fs");

function die(msg) { console.error("QUALITY FAIL: " + msg); process.exit(1); }

const runPath = process.argv[2];
if (!runPath) {
  console.error("usage: node check-quality.js <run.json> [--ports N]");
  process.exit(1);
}
let NPORTS = 2;
{
  const i = process.argv.indexOf("--ports");
  if (i !== -1) {
    const raw = process.argv[i + 1];
    if (!/^[2-4]$/.test(String(raw))) die("--ports must be 2, 3 or 4; got: " + raw);
    NPORTS = parseInt(raw, 10);
  }
}
const run = JSON.parse(fs.readFileSync(runPath, "utf8"));

const cov = run.coverage;
if (!cov || typeof cov !== "object") die("run JSON has no coverage object");
const seen = cov.actionStatesSeen;
if (!seen || typeof seen !== "object" || Array.isArray(seen)) {
  die("coverage.actionStatesSeen is not an object");
}
const keys = Object.keys(seen);
if (keys.length === 0) die("actionStatesSeen is empty");

const dead = keys.filter((k) => /^DEAD/.test(k));
const dmg = keys.filter((k) => /^DAMAGE/.test(k) || /^CAPTUREDAMAGE/.test(k));
if (dead.length < 1) die("no /^DEAD/ action state seen — no KO in the trace");
if (dmg.length < 1) {
  die("no /^DAMAGE|^CAPTUREDAMAGE/ action state seen — no real hit landed");
}

if (!Array.isArray(cov.stocks) || cov.stocks.length !== NPORTS) {
  die("coverage.stocks is not the " + NPORTS + "-active-port array: " +
      JSON.stringify(cov.stocks));
}
for (let i = 0; i < NPORTS; i++) {
  const s = cov.stocks[i];
  if (!Number.isInteger(s) || s < 1) {
    die(`player ${i} ends with stocks=${s} (< 1: the match ended in-trace)`);
  }
}
if (cov.playing !== true) die("coverage.playing !== true (match not live)");
if (cov.gameMode !== 3) die("coverage.gameMode !== 3: " + cov.gameMode);

console.log("QUALITY OK: KO=[" + dead.join(",") + "] hit=[" + dmg.join(",") +
  "] stocks=[" + cov.stocks.join(",") + "] playing=true gameMode=3");
