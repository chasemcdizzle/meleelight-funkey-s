#!/usr/bin/env node
// port/goldens-m4/gen-s02-trace.js — the s02 GUARDON depletion-break
// scenario trace (iter 85). Deterministic, hand-authored (no RNG):
// regenerating this file reproduces the committed trace byte-identically.
// All frame numbers are TRACE indices (the sim consumes trace[f] on sim
// frame f+1); the boundary constants marked MEASURED were tuned
// host-side on the bit-exact C sim (AGENT-LOG iter 85) and then frozen.
//
// Scenario (battlefield, P1 marth spawn x=-50, P2 fox spawn x=+50, both
// on the side platforms), designed to make a shield deplete to break
// DURING GUARDON — the shield-RAISE window — exercising upstream
// shieldDepletion's break arm from GUARDON.main (the review-82 High:
// GUARDON.c dropped as_shieldDepletion's return; the break dispatch to
// SHIELDBREAKFALL only existed in GUARD.c). Plus the M0 quality
// contract (KO + DAMAGE + both players alive at 3600):
//   phase 1  both dash/walk to mid-stage (the s01 approach recipe:
//              fox leaves its platform by dashing, marth follows)
//   phase 2  fox holds full-analog shield in GUARD until shieldHP is a
//              sliver (0.28/frame off 60 HP), then RELEASES: GUARD ->
//              GUARDOFF (fox GUARDOFF = 15 frames) -> WAIT, regenerating
//              0.07 HP/frame while shielding === false
//   phase 3  fox RE-PRESSES shield: WAIT -> GUARDON (fox GUARDON = 8
//              frames, depletion budget 8 x 0.28 = 2.24 HP) -> the
//              depletion crosses 0 ON a GUARDON frame (MEASURED break:
//              sim frame 596, GUARDON timer 6) -> SHIELDBREAKFALL ->
//              land -> FURAFURA. The stick releases ON the measured
//              break frame (the s01 lesson: holding shield input at
//              0 HP would re-enter the break arm).
//   phase 4  marth (short left-walk turn after the overshooting dash,
//              then hold B) full-charges to 122 -> the released swing
//              HITS the dizzy fox -> the DAMAGE state
//   phase 5  fox gets up from the knockdown (stick-up flick)
//   phase 6  fox dashes off the LEFT edge holding down+away from a
//              fresh stick edge (the g01/s01 recipe) -> the guaranteed
//              KO; the tail is neutral (fox respawns; match live at
//              3600).
//
// Usage: node gen-s02-trace.js [outfile]
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

const outfile = process.argv[2] ||
  "s02-marth-fox-guardon-break-battlefield.trace.json";
const FRAMES = 3800;

// MEASURED boundary constants (host C sim; see header):
const SHIELD1_ON = 360;   // fox first shield hold (GUARD depletion)
const SHIELD1_OFF = 573;  // release boundary: held 360..572 (213 frames)
const REPRESS = 590;      // fox re-press: WAIT -> GUARDON on sim frame 591
const BREAK2 = 596;       // held 590..595; break on sim frame 596 =
                          // GUARDON timer 6 (MEASURED: GUARDON timer 5
                          // on sim 595, SHIELDBREAKFALL timer 1 on 596)

const trace = [];
for (let f = 0; f < FRAMES; f++) {
  const p1 = neutral(); // marth
  const p2 = neutral(); // fox

  // phase 1: fox dashes left off its platform, settles mid-stage (~x 0);
  // marth dashes right off its platform, stopping LEFT of fox facing it
  // (the s01 approach, frames verbatim)
  if (f >= 200 && f < 235) withStick(p2, -1, 0);
  if (f >= 230 && f < 252) withStick(p1, 1, 0);

  // phase 2: fox holds full-analog shield, releasing with a sliver of HP
  if (f >= SHIELD1_ON && f < SHIELD1_OFF) p2.rA = 1.0;

  // phase 3: fox re-presses; the depletion break lands DURING GUARDON
  if (f >= REPRESS && f < BREAK2) p2.rA = 1.0;

  // phase 4: marth turns around (short left walk — the dash overshot
  // past fox; the s01 recipe at s02's break-relative offsets), then
  // full-charges -> the released swing HITS the dizzy fox -> DAMAGE
  if (f >= 561 && f < 577) withStick(p1, -0.35, 0);
  if (f >= 581 && f < 721) p1.b = true;

  // phase 5: fox gets up from the knockdown (stick-up flick)
  if (f >= 831 && f < 841) withStick(p2, 0, 1);

  // phase 6: fresh-edge dash off the LEFT edge holding down+away
  if (f >= 880 && f < 1000) withStick(p2, -1, f >= 960 ? -1 : 0);

  trace.push([p1, p2, null, null]);
}

require("fs").writeFileSync(outfile, JSON.stringify(trace));
console.log("wrote " + outfile + ": " + FRAMES + " frames (s02 scenario)");
