#!/usr/bin/env node
// M2-CAL capture pin check: JSONL well-formedness + measured-then-frozen
// per-function counts (expected-capture.json) + the no-undef-in-returns
// invariant the C replay parser relies on. Exit 0 only if all pins hold.
"use strict";
const fs = require("fs");
const path = require("path");

const [id, jsonlPath, runPath] = process.argv.slice(2);
if (!id || !jsonlPath || !runPath) {
  console.error("usage: node check-capture-pins.js <golden-id> <envcoll.jsonl> <capture-run.json>");
  process.exit(1);
}
const expected = JSON.parse(fs.readFileSync(
  path.join(__dirname, "expected-capture.json"), "utf8"));
const pins = expected.goldens[id];
if (!pins) { console.error(`no pins for golden ${id}`); process.exit(1); }

const run = JSON.parse(fs.readFileSync(runPath, "utf8"));
const lines = fs.readFileSync(jsonlPath, "utf8").split("\n").filter((l) => l.length > 0);

let fail = false;
const die = (m) => { console.error("CAPTURE PIN FAIL: " + m); fail = true; };

// 1. record count pin
if (lines.length !== pins.records) {
  die(`${id}: ${lines.length} records, pinned ${pins.records}`);
}

// 2. well-formedness: 4 tab fields, integer frame, nondecreasing frames,
//    counts recomputed from the JSONL itself (not trusted from the run meta)
const counts = {};
let prevF = 0;
for (let i = 0; i < lines.length; i++) {
  const parts = lines[i].split("\t");
  if (parts.length !== 4) { die(`line ${i + 1}: ${parts.length} fields`); break; }
  const f = Number(parts[0]);
  if (!Number.isInteger(f) || f < 0) { die(`line ${i + 1}: bad frame ${parts[0]}`); break; }
  if (f < prevF) { die(`line ${i + 1}: frame ${f} after ${prevF} (order broken)`); break; }
  prevF = f;
  counts[parts[1]] = (counts[parts[1]] || 0) + 1;
  // 3. invariant: no undef token in any return serialization
  if (parts[3].indexOf("undef") !== -1) {
    die(`line ${i + 1}: return carries undef (${parts[1]} frame ${f}) — ` +
        "breaks the parser's undef->NaN arg mapping soundness");
  }
}

// 4. per-function count pins (JSONL-derived AND run-meta must both match)
const metaCounts = run.meta.envcollCapture.counts;
for (const fn of Object.keys(pins.counts)) {
  if ((counts[fn] || 0) !== pins.counts[fn]) {
    die(`${id}: ${fn} count ${counts[fn] || 0} (jsonl), pinned ${pins.counts[fn]}`);
  }
  if ((metaCounts[fn] || 0) !== pins.counts[fn]) {
    die(`${id}: ${fn} count ${metaCounts[fn] || 0} (run meta), pinned ${pins.counts[fn]}`);
  }
}
for (const fn of Object.keys(counts)) {
  if (!(fn in pins.counts)) die(`${id}: unpinned function in capture: ${fn}`);
}

if (fail) process.exit(1);
console.log(`${id}: pins OK (${lines.length} records, counts + no-undef-ret invariant)`);
