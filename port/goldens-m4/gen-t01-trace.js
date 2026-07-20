#!/usr/bin/env node
// port/goldens-m4/gen-t01-trace.js — the t01 target-test scenario trace
// (M4 task 11). Deterministic, hand-authored (no RNG): regenerating this
// file reproduces the committed trace byte-identically. Frame numbers
// are TRACE indices (the sim consumes trace[f] on sim frame f+1); the
// constants marked MEASURED were tuned host-side on the bit-exact C
// target sim (AGENT-LOG iter 94) and then frozen.
//
// Scenario: FOX on targetstage1 (tstage 0; spawn (0,0), center-block
// floor y=-14). Both breaks are ARTICLE breaks — the
// articleTargetCollision arm (current + interpolated faces) with live
// laser flight through the mode-5 destroyArticles/executeArticles
// pipeline (maxArticles > 0):
//   phase 1  wait out the 1.5 s starting window (~90 frames; inputs
//            ignored — the target arm's !starting interpretInputs gate)
//   phase 2  tap left (TILTTURN — face -1), fire NEUTRALSPECIALGROUND:
//            laser spawns at feet+7 = -7 (fox laser offset y:7,
//            NEUTRALSPECIALGROUND.js:55), travels left 7/frame, passes
//            target (-48.6, 0.7) — |dy| 7.7 < 8.172 (laser hb size
//            1.172 + the targetplay radius 7) -> BREAK 1 (measured sim
//            frame 157)
//   phase 3  tap right (face 1), fire again: the laser passes target
//            (48.4, 0.7), same margin -> BREAK 2 (measured sim frame
//            307)
//   phase 4  neutral tail on the center block (alive, playing, no
//            START, 8/10 targets remain — endTargetGame never fires)
//
// GEOMETRY AMENDMENT (measured, AGENT-LOG iter 94): the pre-registered
// t01 stage was tstage 9 (targetstage10); its measured target set sits
// >= 5.5 units off every laser line reachable without multi-jump
// platforming across SD gaps (two blind attempts SD'd through the
// -135..-112.5 gap). targetstage1's paired targets at y 0.7 lie 7.7
// units off the center-floor laser line — inside the 8.172 radius —
// so t01 moved to tstage 0 with the measurement on record.
//
// Usage: node gen-t01-trace.js [outfile]
"use strict";

function neutral() {
  return {
    a: false, b: false, x: false, y: false, z: false, r: false, l: false,
    s: false, du: false, dr: false, dd: false, dl: false,
    lsX: 0, lsY: 0, csX: 0, csY: 0, lA: 0, rA: 0,
    rawX: 0, rawY: 0, rawcsX: 0, rawcsY: 0,
  };
}
function withStick(inp, x, y) {
  inp.lsX = x; inp.lsY = y; inp.rawX = x; inp.rawY = y;
  return inp;
}

const outfile = process.argv[2] || "t01-fox-lasers-tstage1.trace.json";
const FRAMES = 3800;

// MEASURED phase boundaries (trace indices; frozen — see header):
const TURNL_ON = 110, TURNL_OFF = 118;   // face left
const LASER1_ON = 140, LASER1_OFF = 144; // B -> laser left -> (-48.6, 0.7)
const TURNR_ON = 260, TURNR_OFF = 268;   // face right
const LASER2_ON = 290, LASER2_OFF = 294; // B -> laser right -> (48.4, 0.7)

const trace = [];
for (let f = 0; f < FRAMES; f++) {
  const p1 = neutral(); // fox (slot 0 — the only target-mode player)

  if (f >= TURNL_ON && f < TURNL_OFF) withStick(p1, -0.35, 0);
  if (f >= LASER1_ON && f < LASER1_OFF) p1.b = true;
  if (f >= TURNR_ON && f < TURNR_OFF) withStick(p1, 0.35, 0);
  if (f >= LASER2_ON && f < LASER2_OFF) p1.b = true;

  trace.push([p1, null, null, null]);
}

require("fs").writeFileSync(outfile, JSON.stringify(trace));
console.log("wrote " + outfile + ": " + FRAMES + " frames (t01 scenario)");
