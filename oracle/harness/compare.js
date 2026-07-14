#!/usr/bin/env node
// Oracle harness comparator (maintained copy; spike original frozen at
// spikes/determinism/harness/compare.js).
// Compare two run.js outputs: report first divergent frame (if any) and,
// when both runs captured serialized state for that frame, a field-level
// diff of the serialized state strings.
// Usage: node compare.js a.json b.json
"use strict";
const fs = require("fs");

const a = JSON.parse(fs.readFileSync(process.argv[2], "utf8"));
const b = JSON.parse(fs.readFileSync(process.argv[3], "utf8"));

const n = Math.min(a.frames.length, b.frames.length);
let firstDiff = -1;
for (let i = 0; i < n; i++) {
  if (a.frames[i].h !== b.frames[i].h) { firstDiff = a.frames[i].f; break; }
}

console.log(`frames compared: ${n}`);
console.log(`rngCalls: ${a.coverage.rngCalls} vs ${b.coverage.rngCalls}` +
  (a.coverage.rngCalls === b.coverage.rngCalls ? " (equal)" : " (DIFFER)"));
if (firstDiff === -1) {
  console.log("IDENTICAL checksum streams");
  process.exit(0);
}
console.log(`FIRST DIVERGENCE at frame ${firstDiff}`);

const sa = a.captured && a.captured[firstDiff];
const sb = b.captured && b.captured[firstDiff];
if (sa && sb) {
  // find first differing character, print context
  let i = 0;
  while (i < sa.length && sa[i] === sb[i]) i++;
  const lo = Math.max(0, i - 160);
  console.log("--- state A around divergence ---");
  console.log(sa.slice(lo, i + 120));
  console.log("--- state B around divergence ---");
  console.log(sb.slice(lo, i + 120));
} else {
  console.log("(no captured state for that frame; re-run both with " +
    `--capture-frames ${firstDiff} to see the diverging field)`);
}
process.exit(2);
