#!/usr/bin/env node
// Generic capture pin check for M2 module-cluster specs (the envcoll
// original stays frozen as check-capture-pins.js): JSONL well-formedness
// + measured-then-frozen per-function counts (expected-capture-<spec>.json)
// + the no-undef-in-returns invariant the C replay parsers rely on.
// Exit 0 only if all pins hold.
//
// Reads the JSONL as a STREAM (M2 task 6: hitdet captures exceed node's
// ~512 MB single-string cap — ERR_STRING_TOO_LONG on readFileSync). The
// checks are unchanged, line-for-line identical to the previous
// whole-file implementation.
"use strict";
const fs = require("fs");
const path = require("path");
const readline = require("readline");

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

async function main() {
  // 1./2. well-formedness: 4/5 tab fields, integer frame, nondecreasing
  //    frames, counts recomputed from the JSONL itself (not trusted from
  //    the run meta); 3. no-undef-ret invariant outside the allowlist
  const counts = {};
  let prevF = 0;
  let lineCount = 0;
  let broke = false;
  const rl = readline.createInterface({
    input: fs.createReadStream(jsonlPath),
    crlfDelay: Infinity,
  });
  for await (const l of rl) {
    if (l.length === 0) continue;
    lineCount++;
    if (broke) continue;
    const i = lineCount - 1;
    const parts = l.split("\t");
    const want = postStateFns.has(parts[1]) ? 5 : 4;
    if (parts.length !== want) { die(`line ${i + 1}: ${parts.length} fields (want ${want} for ${parts[1]})`); broke = true; continue; }
    if (want === 5 && parts[4].length === 0) { die(`line ${i + 1}: empty post-state field`); broke = true; continue; }
    const f = Number(parts[0]);
    if (!Number.isInteger(f) || f < 0) { die(`line ${i + 1}: bad frame ${parts[0]}`); broke = true; continue; }
    if (f < prevF) { die(`line ${i + 1}: frame ${f} after ${prevF} (order broken)`); broke = true; continue; }
    prevF = f;
    counts[parts[1]] = (counts[parts[1]] || 0) + 1;
    if (parts[3].indexOf("undef") !== -1 && !undefRetAllowed.has(parts[1])) {
      die(`line ${i + 1}: return carries undef (${parts[1]} frame ${f}) — ` +
          "breaks the parser's undef->NaN arg mapping soundness");
    }
  }

  // record count pin
  if (lineCount !== pins.records) {
    die(`${id}: ${lineCount} records, pinned ${pins.records}`);
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
  console.log(`${id}: ${spec} pins OK (${lineCount} records, counts + no-undef-ret invariant)`);
}

main().catch((e) => { console.error(e); process.exit(1); });
