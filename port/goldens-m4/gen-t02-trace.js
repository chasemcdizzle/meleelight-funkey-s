#!/usr/bin/env node
// port/goldens-m4/gen-t02-trace.js — the t02 target-test scenario trace
// (M4 task 11). Deterministic, hand-authored (no RNG): regenerating this
// file reproduces the committed trace byte-identically. Frame numbers
// are TRACE indices (the sim consumes trace[f] on sim frame f+1); the
// constants marked MEASURED were tuned host-side on the bit-exact C
// target sim (AGENT-LOG iter 94) and then frozen.
//
// Scenario: FALCON on targetstage2 (tstage 1; spawn (-159.5, -73.8),
// lower-corridor floor y=-88.3). Both breaks are MELEE breaks — the
// hitTargetCollision arm (current-frame + prev-frame + interpolated
// faces are all in the live expression) with zero articles:
//   phase 1  wait out the 1.5 s starting window
//   phase 2  walk left to ~x -172 (TILTTURN + WALK), JAB: the jab
//            hitbox reaches target (-185.1, -75.1), 13.2 above the
//            floor -> BREAK 1 (measured sim frame 163)
//   phase 3  dash right along the corridor floor (stopping short of the
//            -48.9 edge — past it is a bottom-blastzone fall), full-hop
//            right (KNEEBEND/JUMPF held 10 frames) with drift onto
//            platform[0] (-43.6..-0.3 at y=-69.7), settling at
//            ~x -33.5 (measured)
//   phase 4  UPTILT (a + lsY 0.35 — a tilt, below the tap-jump band):
//            the up-tilt arc reaches target (-25.3, -44.8), 24.9 above
//            the platform -> BREAK 2 (measured sim frame 447)
//   phase 5  neutral tail on the platform (alive, playing, no START,
//            8/10 targets remain — endTargetGame never fires)
//
// Usage: node gen-t02-trace.js [outfile]
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

const outfile = process.argv[2] || "t02-falcon-melee-tstage2.trace.json";
const FRAMES = 3800;

// MEASURED phase boundaries (trace indices; frozen — see header):
const WALKL_ON = 110, WALKL_OFF = 152;  // walk left toward the corner
const JAB_ON = 160, JAB_OFF = 164;      // A -> jab -> (-185.1, -75.1)
const DASH_ON = 260, DASH_OFF = 300;    // dash right (short of the edge)
const HOP_ON = 302, HOP_OFF = 312;      // X held (full hop), drift right
const DRIFT_OFF = 340;                  // keep drifting onto platform[0]
const UPTILT_ON = 430, UPTILT_OFF = 434; // a + up-tilt -> (-25.3, -44.8)

const trace = [];
for (let f = 0; f < FRAMES; f++) {
  const p1 = neutral(); // falcon (slot 0 — the only target-mode player)

  if (f >= WALKL_ON && f < WALKL_OFF) withStick(p1, -0.5, 0);
  if (f >= JAB_ON && f < JAB_OFF) p1.a = true;
  if (f >= DASH_ON && f < DASH_OFF) withStick(p1, 1, 0);
  if (f >= HOP_ON && f < HOP_OFF) { p1.x = true; withStick(p1, 0.5, 0); }
  if (f >= HOP_OFF && f < DRIFT_OFF) withStick(p1, 0.5, 0);
  if (f >= UPTILT_ON && f < UPTILT_OFF) { p1.a = true; withStick(p1, 0, 0.35); }

  trace.push([p1, null, null, null]);
}

require("fs").writeFileSync(outfile, JSON.stringify(trace));
console.log("wrote " + outfile + ": " + FRAMES + " frames (t02 scenario)");
