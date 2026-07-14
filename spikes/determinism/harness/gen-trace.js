#!/usr/bin/env node
// Deterministic input-trace generator for the determinism spike.
//
// Emits trace-p1p2.json: array indexed by frame, each entry
// [inputP0, inputP1, null, null] of full 22-field meleelight Input objects
// (src/input/input.js inputData shape).
//
// Scenario (Battlefield, P1 Fox @ x=-50 facing right, P2 Marth @ x=+50
// facing left), designed to cover: fox lasers (articles), hits, a grab ->
// CAPTUREWAIT with mashing (the gameplay Math.random site), and a
// guaranteed KO (P2 walks off the left edge holding down+away), then
// seeded pseudo-random mashing for the remainder.
//
// Usage: node gen-trace.js [outfile] [frames] [seed]

"use strict";

function mulberry32(a) {
  return function () {
    a |= 0; a = (a + 0x6d2b79f5) | 0;
    let t = Math.imul(a ^ (a >>> 15), 1 | a);
    t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t;
    return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
  };
}

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

const outfile = process.argv[2] || "trace-p1p2.json";
const FRAMES = parseInt(process.argv[3] || "3800", 10);
const seed = parseInt(process.argv[4] || "1337", 10);
const rng = mulberry32(seed);

const AXES = [-1, -0.7, -0.35, 0, 0.35, 0.7, 1];
function randAxis() { return AXES[Math.floor(rng() * AXES.length)]; }

// Geometry facts (observed via --capture-frames probes): players spawn on
// the side platforms (P1 x=-50, P2 x=+50, y=27.2). Walking cannot carry a
// player offstage (edge teeter catches at x=+-68.4); only dashing can.
// Beat-based chaos: hold one stick value per 16-frame beat so players
// actually move between actions instead of jab-locking in place.
const BEAT = 16;
let beat1 = { x: 0, y: 0, act: 0 };
let beat2 = { x: 0, y: 0, act: 0 };
function rollBeat() {
  const sticks = [-0.7, -0.35, 0, 0.35, 0.7];
  return {
    x: sticks[Math.floor(rng() * sticks.length)],
    y: rng() < 0.1 ? -0.35 : 0,
    act: Math.floor(rng() * 10), // 0-2 jab, 3 grab, 4 jump, 5 shield, else none
  };
}

const trace = [];
for (let f = 0; f < FRAMES; f++) {
  const p1 = neutral();
  const p2 = neutral();

  if (f < 160) {
    // entrance + settle: neutral
  } else if (f < 280) {
    // P1 fires lasers from its platform (articles); P2 walks in and stops
    // near centre-stage (~x=0)
    if (f % 10 < 2) p1.b = true;
    withStick(p2, -0.35, 0);
  } else if (f < 400) {
    // P1 walks right: off its platform, down, up to P2 near centre
    withStick(p1, 0.35, 0);
  } else if (f < 560) {
    // close-range scrap: P1 jabs, P2 jabs back -> hits/percent
    if (f % 40 < 2) p1.a = true;
    if (f % 40 >= 8 && f % 40 < 20) withStick(p1, 0.35, 0);
    if (f % 60 < 2) p2.a = true;
  } else if (f < 940) {
    // grab attempts: P1 walks forward and taps Z; P2 stands nearly still
    // (easy target) but taps A periodically — while free that's a jab,
    // while grabbed it's a mash-out edge, which is exactly what fires the
    // CAPTUREWAIT Math.random position wiggle.
    if (f % 35 < 20) withStick(p1, 0.35, 0);
    if (f % 35 >= 25 && f % 35 < 27) p1.z = true;
    if (f % 55 < 4) p2.a = true;
  } else if (f < 1100) {
    // P1 backs off and lasers again; P2 shields briefly then drops
    withStick(p1, f < 1010 ? -0.35 : 0, 0);
    if (f >= 1010 && f % 12 < 2) p1.b = true;
    if (f < 1030) p2.rA = 0.6;
  } else if (f < 1500) {
    // guaranteed KO: P2 dashes off the LEFT edge (x=-68.4) holding
    // down+away so it can't ledge-snap; falls to the bottom blastzone.
    withStick(p2, -1, f > 1200 ? -1 : 0);
    // P1 stays safely mid-stage, occasional laser
    if (f % 20 < 2) p1.b = true;
    if (f % 50 < 6) withStick(p1, -0.35, 0);
  } else if (f < 1650) {
    // P2 respawning; P1 walks back toward centre-right to meet it
    withStick(p1, 0.35, 0);
  } else {
    // seeded chaos: per-16-frame beats. Sticks are walk-strength (teeter
    // protects walkers from falling offstage, so random drift can't end
    // the match early); one action rolled per beat. Covers tilts, jabs,
    // grabs/throws, jumps/aerials, shields for the remaining ~2100 frames.
    if ((f - 1650) % BEAT === 0) { beat1 = rollBeat(); beat2 = rollBeat(); }
    const k = (f - 1650) % BEAT;
    withStick(p1, beat1.x, beat1.y);
    withStick(p2, beat2.x, beat2.y);
    if (k < 2) {
      if (beat1.act <= 2) p1.a = true;
      else if (beat1.act === 3) p1.z = true;
      else if (beat1.act === 4) p1.x = true;
      if (beat2.act <= 2) p2.a = true;
      else if (beat2.act === 3) p2.z = true;
      else if (beat2.act === 4) p2.x = true;
    }
    if (beat1.act === 5) p1.lA = 0.8;
    if (beat2.act === 5) p2.rA = 0.8;
  }
  trace.push([p1, p2, null, null]);
}

require("fs").writeFileSync(outfile, JSON.stringify(trace));
console.log("wrote " + outfile + ": " + FRAMES + " frames, seed " + seed);
