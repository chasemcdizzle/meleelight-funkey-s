// simbench.js — meleelight-shaped sim workload for the QuickJS on-device
// instrument (ticket #8, Experiment 2 / strategy S2).
//
// WHAT THIS IS: a calibrated proxy for one frame of meleelight's sim tick
// (anatomy doc §3: [interpretInputs + update] x4 players, hitDetect x4,
// executeHits), built from REAL meleelight leaf functions copied verbatim
// (Flow annotations stripped) — Vec2D, linAlg (dotProd/scalarProd/norm/add/
// subtract/euclideanDist/orthogonalProjection), solveQuadraticEquation —
// driven over playerObject-shaped state (nested phys/hit objects, per-call
// Vec2D allocation churn, string actionState, 8-deep input buffer), stage
// collision sweeps vs 12 surfaces, 4x4 hitbox circle sweeps per player pair,
// and the Melee knockback formula (Math.pow/atan2/sin/cos).
//
// WHAT IT IS NOT: the actual meleelight bundle (whose module graph needs the
// ticket-#7 oracle harness to run headless). Volumes below are tuned so the
// same file measures ~0.3-0.5 ms/frame under desktop Node/V8-JIT — the
// survey's (porting-strategies.md §3) assumed desktop cost of one sim frame —
// so the on-device QuickJS number reads directly as "estimated ms per sim
// frame, interpreted".
//
// Runs identically under: node simbench.js  |  qjsmin simbench.js [frames]
"use strict";

// ---------- REAL meleelight code (src/main/util/Vec2D.js) ----------
class Vec2D {
  constructor(x, y) { this.x = x; this.y = y; }
  dot(vector) { return this.x * vector.x + this.y * vector.y; }
}

// ---------- REAL meleelight code (src/main/linAlg.js) ----------
function dotProd(vec1, vec2) { return (vec1.x * vec2.x + vec1.y * vec2.y); }
function scalarProd(lambda, vec) { return (new Vec2D(lambda * vec.x, lambda * vec.y)); }
function norm(vec) { return (Math.sqrt(dotProd(vec, vec))); }
function add(vec1, vec2) { return (new Vec2D(vec1.x + vec2.x, vec1.y + vec2.y)); }
function subtract(vec1, vec2) { return (new Vec2D(vec1.x - vec2.x, vec1.y - vec2.y)); }
function squaredDist(c1, c2) {
  return ((c2.x - c1.x) * (c2.x - c1.x) + (c2.y - c1.y) * (c2.y - c1.y));
}
function euclideanDist(c1, c2) {
  const sqDist = squaredDist(c1, c2);
  return sqDist <= 0 ? 0 : Math.sqrt(sqDist);
}
function orthogonalProjection(point, line) {
  const line0 = line[0];
  const [line0x, line0y] = [line0.x, line0.y];
  if (line0x === line[1].x && line0y === line[1].y) return line0;
  else {
    const pointVec = new Vec2D(point.x - line0x, point.y - line0y);
    const lineVec = new Vec2D(line[1].x - line0x, line[1].y - line0y);
    const lineNorm = norm(lineVec);
    const lineElem = scalarProd(1 / lineNorm, lineVec);
    const factor = dotProd(pointVec, lineElem);
    const projVec = scalarProd(factor, lineElem);
    return (new Vec2D(projVec.x + line0x, projVec.y + line0y));
  }
}

// ---------- REAL meleelight code (src/main/util/solveQuadraticEquation.js) ----------
function solveQuadraticEquation(a0, a1, a2, sign) {
  if (sign === undefined) sign = 1;
  if (a1 === 0 && a2 === 0) {
    if (a0 === 0) return -1;
    else return null;
  } else if (Math.abs(a0 * a0 * a2 / (a1 * a1)) < 1e-20) {
    return (-a0 / a1);
  } else {
    const disc = a1 * a1 - 4 * a0 * a2;
    if (disc < 0) return null;
    else if (Math.sign(a1) === sign) return 2 * a0 / (-a1 - sign * Math.sqrt(disc));
    else return ((-a1 + sign * Math.sqrt(disc)) / (2 * a2));
  }
}

// ---------- playerObject-shaped state (per src/main/player.js) ----------
function makeInput() {
  return { a: false, b: false, x: false, y: false, z: false, s: false,
           l: false, r: false, du: false, dr: false, dd: false, dl: false,
           lsX: 0, lsY: 0, csX: 0, csY: 0, lA: 0, rA: 0 };
}
function makePlayer(i) {
  return {
    actionState: "WAIT",
    timer: 1,
    charAttributes: { gravity: 0.23, maxFallV: 2.8, aerialH: 0.02,
      maxAerialH: 0.83, airFriction: 0.02, weight: 75, traction: 0.08 },
    phys: {
      pos: new Vec2D(10 * i, 0), posDelta: new Vec2D(0, 0),
      cVel: new Vec2D(0, 0), kVel: new Vec2D(0, 0),
      grounded: true, face: 1, shielding: false, jumps: 2,
      intangibleTimer: 0, invincibleTimer: 0, hitlag: 0,
      ECB1: [new Vec2D(0, 0), new Vec2D(2, 6), new Vec2D(0, 12), new Vec2D(-2, 6)],
      ECBp: [new Vec2D(0, 0), new Vec2D(2, 6), new Vec2D(0, 12), new Vec2D(-2, 6)],
      hurtbox: { c: new Vec2D(0, 6), r: 7 },
    },
    hit: { hitlag: 0, knockback: 0, angle: 0, percent: 0 },
    hitboxes: {
      active: [true, true, false, false],
      id: [{ dmg: 12, bkb: 30, kbg: 100, angle: 45, r: 4.5, offset: new Vec2D(5, 6) },
           { dmg: 9, bkb: 20, kbg: 80, angle: 361, r: 3.5, offset: new Vec2D(8, 6) },
           null, null],
      hitList: [],
    },
    inputBuffer: Array.from({ length: 8 }, makeInput),
  };
}
const players = [makePlayer(0), makePlayer(1), makePlayer(2), makePlayer(3)];

// stage: 12 surfaces (battlefield-ish: grounds, platforms, walls, ceilings)
const surfaces = [];
for (let i = 0; i < 12; i++) {
  surfaces.push([new Vec2D(-70 + i * 12, (i % 3) * 25), new Vec2D(-40 + i * 12, (i % 3) * 25)]);
}
// environmentalCollision.js re-sweeps after each resolved collision
// (maxRecursion = 6); 3 passes is a typical eventful frame
const SWEEP_PASSES = 3;

// saveGameState() deep-copies gameplay state EVERY tick (src/main/replay.js)
function deepCopy(o) {
  if (o === null || typeof o !== "object") return o;
  if (o instanceof Vec2D) return new Vec2D(o.x, o.y);
  if (Array.isArray(o)) { const a = new Array(o.length);
    for (let i = 0; i < o.length; i++) a[i] = deepCopy(o[i]); return a; }
  const r = {};
  for (const k in o) r[k] = deepCopy(o[k]);
  return r;
}

const STATES = ["WAIT", "DASH", "RUN", "JUMPSQUAT", "ATTACKAIRN", "FALL"];

// ---------- one sim frame, shaped like main.js:1039-1080 ----------
function interpretInputs(p, f) {
  // scan the 8-deep buffer like dash/smash detection does
  const buf = players[p].inputBuffer;
  const cur = buf[f % 8];
  cur.lsX = Math.sin(f * 0.1 + p);
  cur.lsY = Math.cos(f * 0.13 + p);
  cur.a = ((f + p) & 7) === 0;
  let smash = false;
  for (let k = 1; k < 8; k++) {
    const prev = buf[(f + 8 - k) % 8];
    if (Math.abs(cur.lsX - prev.lsX) > 0.5 && Math.abs(prev.lsX) < 0.3) smash = true;
    if (prev.a && !cur.a) smash = smash || prev.lsY > 0.7;
  }
  if (smash) players[p].actionState = STATES[(f + p) % STATES.length];
}

function updatePlayer(p, f) {
  const pl = players[p];
  const ph = pl.phys;
  pl.timer += 1;
  // physics integration (physics.js style: fresh Vec2Ds everywhere)
  ph.cVel = new Vec2D(
    Math.max(-pl.charAttributes.maxAerialH,
      Math.min(pl.charAttributes.maxAerialH, ph.cVel.x + pl.charAttributes.aerialH * Math.sin(f * 0.05))),
    Math.max(-pl.charAttributes.maxFallV, ph.cVel.y - pl.charAttributes.gravity));
  ph.kVel = scalarProd(0.95, ph.kVel);
  const vel = add(ph.cVel, ph.kVel);
  const newPos = add(ph.pos, vel);
  // ECB collision sweep vs all surfaces (environmentalCollision.js shape:
  // orthogonal projections + quadratic sweep solves + fresh ECB points,
  // re-swept SWEEP_PASSES times as collision resolution does)
  let best = null;
  for (let pass = 0; pass < SWEEP_PASSES; pass++) {
    for (let s = 0; s < surfaces.length; s++) {
      const surf = surfaces[s];
      for (let c = 0; c < 4; c++) {
        const ecb = (pass & 1) ? ph.ECBp[c] : ph.ECB1[c];
        const pt = new Vec2D(newPos.x + ecb.x, newPos.y + ecb.y + pass * 0.001);
        const proj = orthogonalProjection(pt, surf);
        const d = euclideanDist(pt, proj);
        if (d < 30) {
          const t = solveQuadraticEquation(d - 30, vel.x + 0.01, 0.5 * pl.charAttributes.gravity);
          if (t !== null && (best === null || t < best)) best = t;
        }
      }
    }
  }
  if (best !== null && best > 0 && best < 1) {
    ph.pos = add(ph.pos, scalarProd(best, vel));
    ph.grounded = true;
    ph.cVel = new Vec2D(ph.cVel.x, 0);
  } else {
    ph.pos = newPos;
    ph.grounded = false;
  }
  // keep in bounds so numbers stay sim-realistic
  if (ph.pos.y < -100) { ph.pos = new Vec2D(ph.pos.x, 50); }
  if (Math.abs(ph.pos.x) > 120) { ph.pos = new Vec2D(-ph.pos.x * 0.9, ph.pos.y); }
  // ECBp update (fresh objects, like squashECBAt)
  for (let c = 0; c < 4; c++)
    ph.ECBp[c] = new Vec2D(ph.pos.x + ph.ECB1[c].x, ph.pos.y + ph.ECB1[c].y);
}

const hitQueue = [];
function hitDetect(p, f) {
  const atk = players[p];
  for (let i = 0; i < 4; i++) {
    if (i === p) continue;
    const vic = players[i];
    for (let j = 0; j < 4; j++) {
      if (!atk.hitboxes.active[j]) continue;
      const hb = atk.hitboxes.id[j];
      const hbPos = new Vec2D(atk.phys.pos.x + hb.offset.x * atk.phys.face,
                              atk.phys.pos.y + hb.offset.y);
      const hurtPos = add(vic.phys.pos, vic.phys.hurtbox.c);
      // swept circle-vs-circle (interpolatedCollision.js shape)
      const relVel = subtract(vic.phys.cVel, atk.phys.cVel);
      const relPos = subtract(hurtPos, hbPos);
      const rsum = hb.r + vic.phys.hurtbox.r;
      const t = solveQuadraticEquation(
        dotProd(relPos, relPos) - rsum * rsum,
        2 * dotProd(relPos, relVel), dotProd(relVel, relVel), -1);
      if (t !== null && t >= 0 && t <= 1) {
        hitQueue.push([i, p, j, t]);
      } else if (euclideanDist(hbPos, hurtPos) < rsum) {
        hitQueue.push([i, p, j, 0]);
      }
    }
  }
}

function executeHits(f) {
  while (hitQueue.length > 0) {
    const [i, p, j] = hitQueue.pop();
    const vic = players[i], hb = players[p].hitboxes.id[j];
    vic.hit.percent += hb.dmg;
    // REAL Melee knockback formula shape (hitDetection.js)
    const kb = (((vic.hit.percent / 10 + vic.hit.percent * hb.dmg / 20) *
      (200 / (players[i].charAttributes.weight + 100)) * 1.4 + 18) *
      (hb.kbg / 100)) + hb.bkb;
    const ang = (hb.angle === 361 ? (kb < 32 ? 0 : 44) : hb.angle) * Math.PI / 180;
    vic.phys.kVel = new Vec2D(kb * 0.03 * Math.cos(ang) * players[p].phys.face,
                              kb * 0.03 * Math.sin(ang));
    vic.hit.hitlag = Math.floor(hb.dmg / 3 + 3);
    vic.actionState = "DAMAGEFLYN";
    vic.hit.angle = Math.atan2(vic.phys.kVel.y, vic.phys.kVel.x);
  }
}

let stateSnapshot = null;
function simFrame(f) {
  for (let p = 0; p < 4; p++) interpretInputs(p, f);
  for (let p = 0; p < 4; p++) updatePlayer(p, f);
  for (let p = 0; p < 4; p++) players[p].hitboxes.hitList.length = 0;
  for (let p = 0; p < 4; p++) hitDetect(p, f);
  executeHits(f);
  // saveGameState(): per-tick deep copy of gameplay state (replay.js:62-123)
  stateSnapshot = [deepCopy(players[0].phys), deepCopy(players[1].phys),
                   deepCopy(players[2].phys), deepCopy(players[3].phys)];
}

// ---------- driver ----------
const isNode = typeof process !== "undefined" && !!process.hrtime;
const nowUs = isNode
  ? () => Number(process.hrtime.bigint()) / 1e3
  : (typeof hrtime === "function" ? hrtime : () => Date.now() * 1000);
const argv = isNode ? process.argv.slice(2) : (typeof scriptArgs !== "undefined" ? scriptArgs.slice(0) : []);
const FRAMES = argv.length > 0 ? parseInt(argv[argv.length - 1], 10) || 2000 : 2000;

// warmup (JIT for node; touch everything for qjs)
for (let f = 0; f < 120; f++) simFrame(f);

const times = new Array(FRAMES);
const t0 = nowUs();
for (let f = 0; f < FRAMES; f++) {
  const a = nowUs();
  simFrame(f + 120);
  times[f] = nowUs() - a;
}
const t1 = nowUs();
times.sort((a, b) => a - b);
const sum = times.reduce((a, b) => a + b, 0);
const engine = isNode ? "node" : "quickjs";
print_or_log(
  `simbench engine=${engine} frames=${FRAMES} ` +
  `min=${(times[0] / 1000).toFixed(3)} avg=${(sum / FRAMES / 1000).toFixed(3)} ` +
  `p50=${(times[FRAMES >> 1] / 1000).toFixed(3)} p90=${(times[(FRAMES * 0.9) | 0] / 1000).toFixed(3)} ` +
  `p99=${(times[(FRAMES * 0.99) | 0] / 1000).toFixed(3)} max=${(times[FRAMES - 1] / 1000).toFixed(3)} ms/frame ` +
  `(wall total ${(t1 - t0) / 1000 | 0} ms)`);
// checksum so nothing dead-code-eliminates
let cs = 0;
for (let p = 0; p < 4; p++) cs += players[p].phys.pos.x + players[p].hit.percent;
print_or_log(`state checksum: ${cs}`);

function print_or_log(s) {
  if (typeof print === "function") print(s); else console.log(s);
}
