#!/usr/bin/env node
"use strict";
// Cross-check the generated engine tables against the ANIM1 animation
// binaries from the SAME pipeline run (fix_plan §M1 task 2 done-check:
// "framesData totals cross-check against the ANIM1 per-state frame
// counts").
//
// The relationship is NOT uniform equality upstream (measured iter 10:
// e.g. marth DOWNWAIT framesData 60 vs 70 animation frames; four puff
// framesData states have no animation at all) — so the honest instrument
// is a measured-then-frozen reconciliation: this script re-derives the
// FULL per-state comparison live from the run's decoded ANIM1 binaries +
// tables.json and asserts it matches the exact lists pinned in
// pipeline/expected.json (tables.animXref). Any upstream or generator
// drift becomes a loud diff, exactly like the golden streams.
// Usage: node lib/tables-anim-xref.js <run-dir>

const fs = require("fs");
const path = require("path");
const { decodeAnim } = require("./animbin");
const { CHAR_NAMES } = require("./tables-schema");

const runDir = path.resolve(process.argv[2] || "");
if (!process.argv[2] || !fs.existsSync(path.join(runDir, "tables.json"))) {
  console.error("usage: tables-anim-xref.js <run-dir with tables.json + anim_*.bin>");
  process.exit(1);
}

const expected = JSON.parse(
  fs.readFileSync(path.join(__dirname, "..", "expected.json"), "utf8"));
const pins = expected.tables && expected.tables.animXref;
if (!pins) {
  console.error("expected.json has no tables.animXref pins");
  process.exit(1);
}
const model = JSON.parse(fs.readFileSync(path.join(runDir, "tables.json"), "utf8"));

// Decode the run's ANIM1 binaries -> per-char Map(state -> frameCount).
const animFrames = CHAR_NAMES.map((name, charId) => {
  const bin = fs.readFileSync(path.join(runDir, `anim_${charId}_${name}.bin`));
  const dec = decodeAnim(bin);
  const m = new Map();
  for (const [state, frames] of dec.states) m.set(state, frames.length);
  return m;
});

// Live reconciliation.
const live = {
  framesEqual: 0, framesDiffer: [], framesNoAnim: [],
  ecbFramesEqual: 0, ecbFramesDiffer: [], ecbNoAnim: [],
};
for (const c of model.chars) {
  const anim = animFrames[c.charId];
  for (const [state, n] of Object.entries(c.framesData)) {
    if (!anim.has(state)) { live.framesNoAnim.push(`${c.name}/${state}:${n}`); continue; }
    const an = anim.get(state);
    if (an === n) live.framesEqual++;
    else live.framesDiffer.push(`${c.name}/${state}:${n}:${an}`);
  }
  for (const [state, frames] of Object.entries(c.ecb)) {
    if (!anim.has(state)) { live.ecbNoAnim.push(`${c.name}/${state}`); continue; }
    const an = anim.get(state);
    if (an === frames.length) live.ecbFramesEqual++;
    else live.ecbFramesDiffer.push(`${c.name}/${state}:${frames.length}:${an}`);
  }
}
for (const k of ["framesDiffer", "framesNoAnim", "ecbFramesDiffer", "ecbNoAnim"]) {
  live[k].sort();
}

let failures = 0;
const eq = (what, got, want) => {
  const g = JSON.stringify(got), w = JSON.stringify(want);
  if (g !== w) {
    console.error(`XREF-FAIL ${what}:\n  got    ${g}\n  pinned ${w}`);
    failures++;
  }
};
eq("framesEqual", live.framesEqual, pins.framesEqual);
eq("framesDiffer", live.framesDiffer, pins.framesDiffer);
eq("framesNoAnim", live.framesNoAnim, pins.framesNoAnim);
eq("ecbFramesEqual", live.ecbFramesEqual, pins.ecbFramesEqual);
eq("ecbFramesDiffer", live.ecbFramesDiffer, pins.ecbFramesDiffer);
eq("ecbNoAnim", live.ecbNoAnim, pins.ecbNoAnim);

if (failures > 0) {
  console.error(`tables-anim-xref: ${failures} mismatch(es) vs expected.json pins`);
  process.exit(1);
}
console.log(`tables-anim-xref: framesData ${live.framesEqual} equal / ` +
  `${live.framesDiffer.length} pinned-differ / ${live.framesNoAnim.length} pinned-no-anim; ` +
  `ecb ${live.ecbFramesEqual} equal / ${live.ecbFramesDiffer.length} pinned-differ / ` +
  `${live.ecbNoAnim.length} pinned-no-anim — all match pins`);
