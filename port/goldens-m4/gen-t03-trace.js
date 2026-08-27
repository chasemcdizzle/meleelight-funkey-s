#!/usr/bin/env node
// port/goldens-m4/gen-t03-trace.js — the t03 target-test scenario trace
// (A45 T6). Deterministic, hand-authored (no RNG): regenerating this file
// reproduces the committed trace byte-identically. Frame numbers are TRACE
// indices (the sim consumes trace[f] on sim frame f+1); every constant
// marked MEASURED was tuned host-side on the bit-exact C target sim and
// then frozen, exactly as gen-t02-trace.js's were.
//
// WHY THIS GOLDEN EXISTS. Two planes had no coverage at all:
//
//   1. THE DAMAGE PLANE. dealWithDamagingStageCollision has five
//      translated call sites (physics.c:415/502/720/748/783) and had
//      NEVER EXECUTED: `damageType` is absent from every authored stage,
//      VS or target (measured, pipeline/lib/targets-schema.js). A45 T6's
//      DAMAGE tool makes it reachable, and `mlk_stage_playable` refused a
//      damaging stage outright until this golden existed.
//
//   2. THE `connected` PLANE ON A CUSTOM STAGE. parseStageCode DERIVES
//      `connected` (encode.js:237); the port did not, from A45 T2 until
//      2026-08-26. No differential could see it because the authored
//      corpus yields zero links (measured, all ten stages) — so the fix
//      needs a stage that HAS them, played through the browser's own
//      parse.
//
// THE STAGE (manifest-target.json's `customStage`, a fixed point of
// upstream's createStageCode — run-target.js proves that in-page before
// playing it):
//
//     ground[0]  (-100,0)..(0,0)     no damage
//     ground[1]  (0,0)..(100,0)      damageType "fire"   <- the plane
//     ceiling    (-100,120)..(100,120)
//     wallR      (-100,120)..(-100,0)   bounds ground[0] on its LEFT
//     wallL      (100,120)..(100,0)     bounds ground[1] on its RIGHT
//     targets    (-50,20) (-20,20)
//     spawn      (-50,40)             blastzone +/-250, scale 3
//
// connected is `g:r0|g1,g0|l0` (measured): the two grounds link to each
// other AND each is bounded by its wall. So walking right off ground[0]
// does not fall — it CONTINUES onto the fire, which is the getConnected
// arm and the damage arm in one motion.
//
// Scenario, with the MEASURED outcomes that make it non-vacuous:
//   phase 1  wait out the starting window
//   phase 2  UP TILT breaks the target at (-50,20) on the SAFE half
//            -> MEASURED targetsDestroyed 1
//   phase 3  WALK right across the x=0 seam. The walk does NOT fall off
//            ground[0] — `connected` carries it onto ground[1] — and
//            ground[1] is fire:
//            MEASURED sim frame 263 WALK, percent 0
//                     sim frame 264 DAMAGEFLYN, percent 10
//            That transition IS the first execution of
//            dealWithDamagingStageCollision in this project's history.
//   phase 4  neutral tail: alive, playing, START never pressed, and
//            MEASURED no /^DEAD/ state anywhere in the trace.
//
// Frozen: 3600 player + 3600 target frames, rngCalls 193,
// rngCallsOutsideStep 1, targetsDestroyed 1, endTargetGame false.
//
// Usage: node gen-t03-trace.js [outfile]
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

const outfile = process.argv[2] || "t03-fox-fireground-custom.trace.json";
const FRAMES = 3800;

// --- the phase table. Tuned by measurement; see the header. --------------
// starting window: the sim ignores inputs before ~frame 91 (CLAUDE.md).
// Kept as a named constant because the phase offsets below are chosen
// relative to it, not because anything reads it.
const START_END = 95;

const rows = [];
for (let f = 0; f < FRAMES; f++) rows.push(neutral());

// phase 2 — fox spawns at (-50,40) and falls to ground[0]; target (-50,20)
// sits 20 above the floor directly overhead, so the reaching move is an UP
// TILT (a + a small up stick, below the tap-jump band), not a jab.
const UPTILT_AT = 120;
for (let f = UPTILT_AT; f < UPTILT_AT + 3; f++) {
  rows[f].a = true;
  withStick(rows[f], 0, 0.35);
}

// phase 3 — walk RIGHT, across the seam at x=0, onto the fire ground. A WALK
// and not a dash on purpose: the crossing is slow, so the frame the damage
// fires does not sit on a knife edge. It also makes the `connected` arm
// load-bearing — a walk that reaches the right end of ground[0] either
// continues onto ground[1] (upstream, and now here) or falls off it.
const WALK_FROM = 200;
const WALK_TO = 560;
for (let f = WALK_FROM; f < WALK_TO; f++) withStick(rows[f], 0.55, 0);

// phase 4 — neutral tail: alive, playing, START never pressed.

// The wire shape is FOUR SLOTS per frame with slots 1-3 null (target mode
// is single-player); trace-to-txt.js refuses anything else.
const out = rows.map((r) => [r, null, null, null]);
require("fs").writeFileSync(outfile, JSON.stringify(out));
process.stdout.write("gen-t03-trace.js: wrote " + outfile + " (" +
  rows.length + " frames)\n");
