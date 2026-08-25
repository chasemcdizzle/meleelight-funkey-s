#!/usr/bin/env node
// port/goldens-m4/gen-s03-trace.js — the s03 FOUR-PORT scenario trace
// (A46). Deterministic (seeded mulberry32, fixed seed below): running
// this file again reproduces the committed trace byte-identically.
//
// This is oracle/harness/gen-trace.js's recipe carried onto four
// columns. gen-trace.js itself emits `[p1, p2, null, null]` and lives
// under oracle/, which HARD RULE 3 makes read-only outside M0 — so the
// four-port variant lives here, in the golden home, exactly as
// run-4p.js does for the recorder.
//
// Ports (CONTEXT.md: a port is a player slot 0-3, never a roster index)
//   0 fox   @ startingPoint[0] = (-50, 50)
//   1 falco @ startingPoint[1] = ( 50, 50)
//   2 puff  @ startingPoint[2] = (-25,  5)
//   3 marth @ startingPoint[3] = ( 25,  5)
// on battlefield (main.js:168 — the four spawn points are upstream's,
// and ports 2/3 spawn mid-air over centre stage, which is why the early
// phases let everyone land before anything is asked of them).
//
// The M0 gameplay-quality contract this trace must satisfy (checked
// mechanically at record time by check-quality.js --ports 4):
//   >=1 KO      — port 3 dashes off the LEFT edge holding down+away
//                 (the g01 recipe: walking teeters at x=±68.4, only a
//                 dash crosses; full down defeats the ledge snap).
//   >=1 hit     — the mid-stage scrap in phase 2 (four players on one
//                 stage land jabs on each other freely).
//   all 4 ports end >=1 stock, match still live at the final frame.
//
// Usage: node gen-s03-trace.js [outfile]
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

const outfile = process.argv[2] || "s03-fourport-battlefield.trace.json";
const FRAMES = 3600;      // == the manifest row's frames
const SEED = 8146;        // == the manifest row's seed (M0 convention)
const rng = mulberry32(SEED);

// Beat-based chaos, gen-trace.js's tail recipe: hold one stick value per
// 16-frame beat so players actually travel between actions instead of
// jab-locking in place. Walk-strength sticks only — the edge teeter
// protects walkers from falling offstage, so random drift can never end
// the match early and steal the "all four alive at 3600" contract.
const BEAT = 16;
function rollBeat() {
  const sticks = [-0.7, -0.35, 0, 0.35, 0.7];
  return {
    x: sticks[Math.floor(rng() * sticks.length)],
    y: rng() < 0.1 ? -0.35 : 0,
    act: Math.floor(rng() * 10), // 0-2 jab, 3 grab, 4 jump, 5 shield, else none
  };
}
function applyBeat(inp, beat, k, shoulder) {
  withStick(inp, beat.x, beat.y);
  if (k < 2) {
    if (beat.act <= 2) inp.a = true;
    else if (beat.act === 3) inp.z = true;
    else if (beat.act === 4) inp.x = true;
  }
  if (beat.act === 5) inp[shoulder] = 0.8;
}

const trace = [];
let beats = [null, null, null, null];
for (let f = 0; f < FRAMES; f++) {
  const p = [neutral(), neutral(), neutral(), neutral()];

  if (f < 160) {
    // entrance + the ports 2/3 fall from (±25, 5): everyone neutral until
    // all four are grounded and the `starting` window (frame ~91) is past.
  } else if (f < 280) {
    // port 0 fires lasers from its platform (articles on a 4-port stage);
    // ports 1/2/3 converge on centre stage.
    if (f % 10 < 2) p[0].b = true;
    withStick(p[1], -0.35, 0);
    withStick(p[2], 0.35, 0);
    withStick(p[3], -0.35, 0);
  } else if (f < 420) {
    // port 0 walks right off its platform and down toward the crowd.
    withStick(p[0], 0.35, 0);
  } else if (f < 900) {
    // the four-way scrap: overlapping jab cadences on different periods
    // so hits land between every pairing that happens to be adjacent.
    if (f % 40 < 2) p[0].a = true;
    if (f % 40 >= 8 && f % 40 < 20) withStick(p[0], 0.35, 0);
    if (f % 47 < 2) p[1].a = true;
    if (f % 47 >= 8 && f % 47 < 20) withStick(p[1], -0.35, 0);
    if (f % 53 < 2) p[2].a = true;
    if (f % 61 < 2) p[3].a = true;
    if (f % 61 >= 8 && f % 61 < 24) withStick(p[3], -0.35, 0);
  } else if (f < 1400) {
    // guaranteed KO: port 3 dashes off the LEFT edge (x = -68.4) holding
    // down+away so it cannot ledge-snap, and falls to the blastzone.
    withStick(p[3], -1, f > 1050 ? -1 : 0);
    // the other three stay mid-stage; port 0 keeps lasering.
    if (f % 20 < 2) p[0].b = true;
    if (f % 50 < 6) withStick(p[0], -0.35, 0);
    if (f % 55 < 6) withStick(p[1], 0.35, 0);
    if (f % 45 < 6) withStick(p[2], -0.35, 0);
  } else if (f < 1560) {
    // port 3 respawning; the rest reset toward centre.
    withStick(p[0], 0.35, 0);
    withStick(p[1], -0.35, 0);
  } else {
    // seeded chaos on all four ports for the remaining ~2000 frames.
    if ((f - 1560) % BEAT === 0) {
      beats = [rollBeat(), rollBeat(), rollBeat(), rollBeat()];
    }
    const k = (f - 1560) % BEAT;
    for (let s = 0; s < 4; s++) {
      applyBeat(p[s], beats[s], k, s % 2 === 0 ? "lA" : "rA");
    }
  }

  trace.push([p[0], p[1], p[2], p[3]]);
}

require("fs").writeFileSync(outfile, JSON.stringify(trace));
console.log("wrote " + outfile + ": " + FRAMES + " frames, seed " + SEED +
  " (4 ports)");
