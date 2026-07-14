#!/usr/bin/env node
// Generic capture pin check for M2 module-cluster specs (the envcoll
// original stays frozen as check-capture-pins.js): JSONL well-formedness
// + measured-then-frozen per-function counts (expected-capture-<spec>.json)
// + the no-undef-in-returns invariant the C replay parsers rely on.
// Exit 0 only if all pins hold.
"use strict";
const fs = require("fs");
const path = require("path");

const [spec, id, jsonlPath, runPath] = process.argv.slice(2);
if (!spec || !id || !jsonlPath || !runPath) {
  console.error("usage: node check-spec-pins.js <spec> <golden-id> <capture.jsonl> <run.json>");
  process.exit(1);
}
const expected = JSON.parse(fs.readFileSync(
  path.join(__dirname, `expected-capture-${spec}.json`), "utf8"));
const pins = expected.goldens[id];
if (!pins) { console.error(`no pins for golden ${id}`); process.exit(1); }

const run = JSON.parse(fs.readFileSync(runPath, "utf8"));
const lines = fs.readFileSync(jsonlPath, "utf8").split("\n").filter((l) => l.length > 0);

// Accessor-class functions (M2 rule 8) echo undefined VERBATIM, and
// void MUTATORS (mutation-capture class, M2 task 2: the value is in the
// post-state field) return undefined by construction — the no-undef-ret
// invariant applies to every OTHER function. The allowlist is part of the
// frozen expectations, never inferred.
const undefRetAllowed = new Set(expected.undefRetAllowed || []);
// Mutation-captured functions (M2 task 2, FORMAT.md "post-state field"):
// their records carry exactly 5 tab fields (the 5th = post-state canon,
// which MAY contain undef — undef-at-rest values are modeled, rule 8);
// every other function's records carry exactly 4. Frozen per spec.
const postStateFns = new Set(expected.postStateFns || []);

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
  const want = postStateFns.has(parts[1]) ? 5 : 4;
  if (parts.length !== want) { die(`line ${i + 1}: ${parts.length} fields (want ${want} for ${parts[1]})`); break; }
  if (want === 5 && parts[4].length === 0) { die(`line ${i + 1}: empty post-state field`); break; }
  const f = Number(parts[0]);
  if (!Number.isInteger(f) || f < 0) { die(`line ${i + 1}: bad frame ${parts[0]}`); break; }
  if (f < prevF) { die(`line ${i + 1}: frame ${f} after ${prevF} (order broken)`); break; }
  prevF = f;
  counts[parts[1]] = (counts[parts[1]] || 0) + 1;
  // 3. invariant: no undef token in any return serialization, except the
  //    frozen accessor-class allowlist (undefined echoes, rule 8)
  if (parts[3].indexOf("undef") !== -1 && !undefRetAllowed.has(parts[1])) {
    die(`line ${i + 1}: return carries undef (${parts[1]} frame ${f}) — ` +
        "breaks the parser's undef->NaN arg mapping soundness");
  }
}

// 4. per-function count pins (JSONL-derived AND run-meta must both match)
if (!run.meta.capture || run.meta.capture.spec !== spec) {
  die(`${id}: run meta capture spec is not '${spec}'`);
}
const metaCounts = run.meta.capture ? run.meta.capture.counts : {};
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
console.log(`${id}: ${spec} pins OK (${lines.length} records, counts + no-undef-ret invariant)`);
