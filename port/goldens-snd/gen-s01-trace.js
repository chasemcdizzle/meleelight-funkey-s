#!/usr/bin/env node
// port/goldens-snd/gen-s01-trace.js — the s01 stop-path scenario trace
// (M4 task 6). Deterministic, hand-authored (no RNG): regenerating this
// file reproduces the committed trace byte-identically. All frame
// numbers are TRACE indices (the sim consumes trace[f] on sim frame
// f+1); the boundary constants marked MEASURED were tuned host-side on
// the bit-exact C sim (AGENT-LOG iter 82) and then frozen.
//
// Scenario (battlefield, P1 marth spawn x=-50, P2 fox spawn x=+50, both
// on the side platforms), designed to fire EVERY in-match Howl .stop
// path (the iter-57 registered coverage hole) plus the M0 quality
// contract (KO + DAMAGE + both players alive at 3600):
//   phase 1  marth partial NEUTRALSPECIAL charge + B release
//              -> the RELEASE-arm shieldbreakercharge.stop
//   phase 2  both dash/walk to mid-stage (fox must leave its platform —
//              walking teeters at the platform edge, dashing crosses it)
//   phase 3  fox holds full-analog shield; shieldDepletion breaks it
//              (60 HP / 0.28 per frame) -> SHIELDBREAKFALL -> land ->
//              FURAFURA + furaloop.play. The stick releases ON the
//              measured break frame: holding shield input at 0 HP
//              re-enters the upstream break arm every frame.
//   phase 4  marth full-charges to 122 -> the AUTO-arm
//              shieldbreakercharge.stop (the swing whiffs; marth is
//              deliberately out of range this episode)
//   phase 5  fox mashes A -> FURAFURA's stuckTimer wake arm ->
//              the FURAFURA-interrupt furaloop.stop, fox back to WAIT
//   phase 6  fox re-shields on regenerated HP (0.07/frame) -> second
//              depletion break -> second FURAFURA + furaloop.play;
//              marth walks in and full-charges again -> the released
//              swing HITS the dizzy fox -> hitDetection's furaloop.stop
//              + the DAMAGE state
//   phase 7  fox gets up and dashes off the LEFT edge holding
//              down+away (the g01 recipe) -> the guaranteed KO; the
//              tail is neutral (fox respawns; match live at 3600).
//
// Usage: node gen-s01-trace.js [outfile]
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

const outfile = process.argv[2] || "s01-marth-fox-stops-battlefield.trace.json";
const FRAMES = 3800;

// MEASURED break frames (host C sim; see header):
const BREAK1 = 575;   // release boundary: held 360..574 -> break on sim frame 575
const SHIELD2_ON = 1000;  // fox re-shield start
const BREAK2 = 1163;  // release boundary: held from 1000 -> break on sim frame 1163 (measured)

const trace = [];
for (let f = 0; f < FRAMES; f++) {
  const p1 = neutral(); // marth
  const p2 = neutral(); // fox

  // phase 1: early partial charge + B release (marth on its platform)
  if (f >= 160 && f < 190) p1.b = true;

  // phase 2: fox dashes left off its platform, settles mid-stage (~x 0);
  // marth dashes right off its platform, stopping LEFT of fox facing it
  if (f >= 200 && f < 235) withStick(p2, -1, 0);
  if (f >= 230 && f < 252) withStick(p1, 1, 0);

  // phase 3: fox holds full-analog shield; release ON the break frame
  if (f >= 360 && f < BREAK1) p2.rA = 1.0;

  // phase 4: marth turns around (short left walk — the dash overshot
  // past fox, which is the measured, deterministic approach), then full
  // charge -> AUTO-arm stop at charge 122; the released swing HITS the
  // adjacent dizzy fox -> hitDetection's furaloop.stop + DAMAGE
  if (f >= 540 && f < 556) withStick(p1, -0.35, 0);
  if (f >= 560 && f < 700) p1.b = true;

  // phase 5: fox gets up from the knockdown (stick-up flick)
  if (f >= 810 && f < 820) withStick(p2, 0, 1);

  // phase 6: fox re-shields on regenerated HP -> second depletion
  // break -> second FURAFURA + furaloop.play (no marth this time)
  if (f >= SHIELD2_ON && f < BREAK2) p2.rA = 1.0;

  // phase 6b: fox mashes A while dizzy -> the FURAFURA stuckTimer wake
  // arm -> the FURAFURA-interrupt furaloop.stop
  if (f >= 1250 && f < 1520 && f % 2 === 0) p2.a = true;

  // phase 7: fresh-edge dash off the LEFT edge holding down+away (a
  // fresh stick edge is required — a stick already held at wake time
  // only walks, and walking teeters at the edge)
  if (f >= 1540 && f < 1660) withStick(p2, -1, f >= 1620 ? -1 : 0);

  trace.push([p1, p2, null, null]);
}

require("fs").writeFileSync(outfile, JSON.stringify(trace));
console.log("wrote " + outfile + ": " + FRAMES + " frames (s01 scenario)");
