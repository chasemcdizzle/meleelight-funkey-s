#!/usr/bin/env node
// port/gfx/iou.js — silhouette IoU judge (M3 task 3).
//
// Compares, per sampled frame, the browser reference ink mask
// (capture-canvas.js: 1200x750 bytes, 1 = fg1|fg2 alpha>0) against the C
// renderer's ink plane (gfx_replay fNNNN.pgm, 240x240) under the frozen
// methodology (iter-44 pre-registration):
//   - browser mask -> 240x150 by 5x5 box: cell inked iff ANY of its 25
//     source pixels is inked;
//   - C mask -> the 240x150 letterbox band rows [45,195); ANY C ink
//     outside the band is a hard failure (the retarget must not leak);
//   - IoU = |A&B| / |A|B| per frame, compared against the frozen
//     threshold in expected-render.json (never loosened).
//
// Per-effect injection assertions (review-65 M2, iter 67): at the pinned
// injection frame, every inkNames effect must contribute ink on BOTH
// sides — (i) browser mask ink > 0 in the effect's derived region,
// (ii) C with-inject ink > 0 in the region, (iii) C LEAVE-ONE-OUT
// DIFFERENTIAL ink > 0 in the region: the full render diffed against a
// baseline whose INJECT1 dropped exactly that effect. (iii) is the
// sharp per-effect tripwire — full and leave-out-X are identical
// renders except for X itself, so a stubbed X draw arm makes the diff
// empty by construction and cannot hide inside the aggregate IoU (which
// stays the frame-level judge). Attribution argument: the injected
// effects draw in queue spawn order and only firefoxtail/shine consume
// the render-local RNG, so removing X can only ripple into LATER
// RNG-consuming effects' samples — measured to land inside the mover's
// own region (shine's region is disjoint from firefoxtail's), never in
// X's. (A shared no-inject baseline was the round-1 design; the T-M2
// tooth REFUTED it — overlapping regions let another effect's ink flip
// pixels inside a stubbed effect's box. See .loop/m4-task2r67-teeth.log.)
// A region-soundness guard additionally requires EVERY full-vs-noinject
// differential pixel in the whole frame to fall inside the union of
// derived regions (an effect drawing outside its derived bounds, or the
// gated no-draw pair drawing anything, dies loudly). Region derivation
// is documented at injectRegions() below.
//
// Usage: node iou.js --canvas DIR --render DIR --render-noinject DIR
//        --render-loo PREFIX (PREFIX-<name>/ per inkNames effect)
//        --vfxdata vfxdata-frozen.txt --stages stages.json --stage N
//        --expected expected-render.json [--report out.json]
// Prints one line per frame + INJ lines + a MIN line; exit 0 iff every
// frame passes and every injection assertion holds.
"use strict";

const fs = require("fs");
const path = require("path");

function arg(name, dflt) {
  const i = process.argv.indexOf("--" + name);
  if (i === -1) return dflt;
  const v = process.argv[i + 1];
  return v === undefined || v.startsWith("--") ? true : v;
}

const CANVAS = arg("canvas", "");
const RENDER = arg("render", "");
const RENDER_NOINJ = arg("render-noinject", "");
const RENDER_LOO = arg("render-loo", "");
const VFXDATA = arg("vfxdata", "");
const STAGES = arg("stages", "");
const STAGE_IDX = arg("stage", "");
const EXPECTED = arg("expected", "");
const REPORT = arg("report", "");
if (!CANVAS || !RENDER || !EXPECTED || CANVAS === true || RENDER === true || EXPECTED === true ||
    !RENDER_NOINJ || RENDER_NOINJ === true || !RENDER_LOO || RENDER_LOO === true ||
    !VFXDATA || VFXDATA === true ||
    !STAGES || STAGES === true || STAGE_IDX === "" || STAGE_IDX === true ||
    !/^[0-9]+$/.test(String(STAGE_IDX))) {
  console.error("iou: --canvas, --render, --render-noinject, --render-loo, --vfxdata, --stages, --stage, --expected are required");
  process.exit(1);
}

const exp = JSON.parse(fs.readFileSync(EXPECTED, "utf8"));
// Corpus pin (review-44 fix 1; EXTENDED 16 -> 24 by M4 task 2's reviewed
// corpus change — vfx/overlay-bearing frames + the injection frame): the
// frozen 24-frame corpus, exactly — count pinned both here and in
// expected-render.json (sampledFrameCount), every frame a unique positive
// integer. Changing the corpus is a reviewed repo change to BOTH files,
// never a runtime degradation.
if (!Array.isArray(exp.sampledFrames) || !Number.isInteger(exp.sampledFrameCount) ||
    exp.sampledFrameCount !== 24 || exp.sampledFrames.length !== exp.sampledFrameCount) {
  console.error("iou: corpus pin violated — expected-render.json must pin exactly 24 sampled frames (sampledFrames + sampledFrameCount)");
  process.exit(1);
}
{
  const seen = new Set();
  for (const v of exp.sampledFrames) {
    if (!Number.isInteger(v) || v < 1 || seen.has(v)) {
      console.error(`iou: corpus pin violated — bad or duplicate sampled frame ${v}`);
      process.exit(1);
    }
    seen.add(v);
  }
}
if (typeof exp.iouThreshold !== "number" || exp.iouThreshold <= 0 || exp.iouThreshold > 1) {
  console.error("iou: expected-render.json iouThreshold malformed");
  process.exit(1);
}

// Injection-set pin, JUDGE SIDE (review-65 M1, iter 67): iou.js is the
// third independent consumer (check-render.sh + capture-canvas.js carry
// their own copies). The runtime inject table must equal the frozen
// reviewed injectPin; the injection frame must be a sampled frame (the
// per-effect assertions below judge exactly that frame's masks).
const PIN = exp.injectPin;
const INJ = exp.inject;
{
  if (!PIN || !Array.isArray(PIN.names) || !Array.isArray(PIN.inkNames) ||
      !Number.isInteger(PIN.frame) || !Number.isInteger(PIN.count)) {
    console.error("iou: injectPin missing/malformed in expected-render.json");
    process.exit(1);
  }
  if (!INJ || !Number.isInteger(INJ.frame) || !Array.isArray(INJ.configs)) {
    console.error("iou: inject table missing/malformed in expected-render.json");
    process.exit(1);
  }
  const names = INJ.configs.map((c) => c && c.name);
  if (names.some((n) => typeof n !== "string" || n.length === 0) ||
      new Set(names).size !== names.length) {
    console.error("iou: inject pin violated (missing/duplicate config name)");
    process.exit(1);
  }
  if (PIN.count !== PIN.names.length || names.length !== PIN.count ||
      names.join(" ") !== PIN.names.join(" ")) {
    console.error("iou: inject pin violated — runtime configs [" + names.join(", ") +
      "] != pinned reviewed set [" + PIN.names.join(", ") + "]");
    process.exit(1);
  }
  if (INJ.frame !== PIN.frame || !exp.sampledFrames.includes(INJ.frame)) {
    console.error("iou: inject pin violated (inject.frame must equal the pinned frame and be a sampled frame)");
    process.exit(1);
  }
  if (PIN.inkNames.length === 0 || new Set(PIN.inkNames).size !== PIN.inkNames.length ||
      !PIN.inkNames.every((n) => PIN.names.includes(n))) {
    console.error("iou: inject pin violated (inkNames must be a nonempty unique subset of names)");
    process.exit(1);
  }
  if (PIN.names.some((n) => !/^[A-Za-z][A-Za-z0-9]*$/.test(n))) {
    console.error("iou: inject pin violated (names must match the identifier grammar — they double as artifact path tokens)");
    process.exit(1);
  }
}

const W = 240, BAND_Y0 = 45, BAND_H = 150;
const SRC_W = 1200, SRC_H = 750;

// ---------------------------------------------------------------------------
// Injection-region derivation (review-65 M2, iter 67).
//
// Center: the injected config's pos through the EXECUTED stage transform
// (scale/offset read from the pipeline stages.json artifact — bit-pattern
// doubles, never hand-retyped engine values): canvas x = pos.x*S + OX,
// canvas y = -pos.y*S + OY (the ubiquitous newPos map, render.js).
// Extents: the frozen VFXDATA1 template bounds (nested-array walk over
// the COMMITTED vfxdata-frozen.txt; bezier control points bound their
// curves — convex hull) times the upstream dVfx CODE-LITERAL draw scales
// (cited per arm below; these are code constants like the stage rail
// literals, not data-plane values). Margin: +-3 canvas px, +-1 device px
// on top, clipped to the mask/letterbox band. The region-soundness guard
// (every differential pixel inside the union of regions) mechanically
// validates these bounds every run.
//   - dashDust: dVfx/general.js via dashDust.js — path frame idx 0 at
//     the injection draw (timer 1), scale 0.2*(S/4.5); x symmetric
//     (face flip), y true template range.
//   - groundBounce: general.js path frame 0, scale 0.2*(S/4.5), rotated
//     pi/2 - facing -> rotation-safe circle of maxAbs(template).
//   - firefoxcharge: fireFox.js charge — draws path[facing] and
//     path[(facing+4)%10] (config facing 3 -> frames 3 and 7), scales
//     0.35*(S/4.5) x / 0.5*(S/4.5) y about the mapped center.
//   - firefoxtail: fireFox.js tail — two arcs about (pos.x, pos.y+4)
//     mapped; centers jittered within +-2S, radius 4S -> radius 6S.
//   - shine: shine.js t=1 — hexagon circumradius up to 6S + star spawns
//     (stars.js) radial <= 5.5S with drift ~0.94 and star extent 1.1S
//     -> radius 7S covers both.
// firefoxlaunch/shineloop are PLAYER-STATE-GATED (draw nothing on a
// plain frame): no region; the soundness guard proves they contributed
// zero differential ink.
// ---------------------------------------------------------------------------

function parseVfxTplKey(txt, tpl, key) {
  // Minimal VFXDATA1 walk: find "TPL <tpl>" block, then its "KEY <key> ..."
  // line; tokenize "[", "]" and String(x) numbers into nested arrays.
  const lines = txt.split("\n");
  let inTpl = false;
  for (const line of lines) {
    if (line.startsWith("TPL ")) { inTpl = (line === "TPL " + tpl); continue; }
    if (!inTpl || !line.startsWith("KEY " + key + " ")) continue;
    const toks = line.slice(("KEY " + key + " ").length).trim().split(/\s+/);
    const stack = [[]];
    for (const t of toks) {
      if (t === "[") { const a = []; stack[stack.length - 1].push(a); stack.push(a); }
      else if (t === "]") {
        if (stack.length < 2) { console.error(`iou: vfxdata ${tpl}.${key}: unbalanced ]`); process.exit(1); }
        stack.pop();
      } else {
        const v = Number(t);
        if (!Number.isFinite(v)) { console.error(`iou: vfxdata ${tpl}.${key}: bad token '${t}'`); process.exit(1); }
        stack[stack.length - 1].push(v);
      }
    }
    if (stack.length !== 1 || stack[0].length !== 1) {
      console.error(`iou: vfxdata ${tpl}.${key}: malformed nesting`);
      process.exit(1);
    }
    return stack[0][0];
  }
  console.error(`iou: vfxdata: TPL ${tpl} KEY ${key} not found`);
  process.exit(1);
}

function tplBounds(node, what) {
  // Walk nested arrays; innermost numeric arrays are flat even-length
  // (x, y) coordinate lists (rows of arity 2 or 6 — beziers included).
  const b = { minX: Infinity, maxX: -Infinity, minY: Infinity, maxY: -Infinity };
  (function walk(n) {
    if (!Array.isArray(n)) {
      console.error(`iou: ${what}: non-array node in template walk`);
      process.exit(1);
    }
    if (n.length && typeof n[0] === "number") {
      if (n.some((v) => typeof v !== "number") || n.length % 2 !== 0) {
        console.error(`iou: ${what}: innermost row is not an even numeric list`);
        process.exit(1);
      }
      for (let i = 0; i < n.length; i += 2) {
        if (n[i] < b.minX) b.minX = n[i];
        if (n[i] > b.maxX) b.maxX = n[i];
        if (n[i + 1] < b.minY) b.minY = n[i + 1];
        if (n[i + 1] > b.maxY) b.maxY = n[i + 1];
      }
      return;
    }
    n.forEach(walk);
  })(node);
  if (!Number.isFinite(b.minX)) {
    console.error(`iou: ${what}: empty template bounds`);
    process.exit(1);
  }
  return b;
}

function injectRegions() {
  const stages = JSON.parse(fs.readFileSync(STAGES, "utf8"));
  const st = stages.stages && stages.stages[Number(STAGE_IDX)];
  if (!st || !st.scale || typeof st.scale.bits !== "string" ||
      !Array.isArray(st.offset) || st.offset.length !== 2 ||
      typeof st.offset[0] !== "number" || typeof st.offset[1] !== "number") {
    console.error("iou: stages.json: stage " + STAGE_IDX + " transform missing/malformed");
    process.exit(1);
  }
  const S = Buffer.from(st.scale.bits, "hex").readDoubleBE(0);
  if (!Number.isFinite(S) || S <= 0) {
    console.error("iou: stages.json: bad stage scale");
    process.exit(1);
  }
  const OX = st.offset[0], OY = st.offset[1];
  const s45 = S / 4.5;
  const vfxTxt = fs.readFileSync(VFXDATA, "utf8");
  const M = 3; // canvas-px margin (raster/AA boundary)

  const regions = [];
  for (const cfg of INJ.configs) {
    if (!PIN.inkNames.includes(cfg.name)) continue; // gated no-draw pair
    const cX = cfg.pos.x * S + OX;
    const cY = -cfg.pos.y * S + OY;
    let x0, x1, y0, y1;
    if (cfg.name === "dashDust") {
      const b = tplBounds(parseVfxTplKey(vfxTxt, "dashDust", "path")[0], "dashDust path[0]");
      const k = 0.2 * s45;
      const ax = Math.max(Math.abs(b.minX), Math.abs(b.maxX)) * k;
      x0 = cX - ax; x1 = cX + ax;
      y0 = cY + b.minY * k; y1 = cY + b.maxY * k;
    } else if (cfg.name === "groundBounce") {
      const b = tplBounds(parseVfxTplKey(vfxTxt, "groundBounce", "path")[0], "groundBounce path[0]");
      const r = Math.max(Math.abs(b.minX), Math.abs(b.maxX),
                         Math.abs(b.minY), Math.abs(b.maxY)) * 0.2 * s45;
      x0 = cX - r; x1 = cX + r; y0 = cY - r; y1 = cY + r;
    } else if (cfg.name === "firefoxcharge") {
      const paths = parseVfxTplKey(vfxTxt, "firefoxcharge", "path");
      const facing = cfg.face;
      if (!Number.isInteger(facing) || facing < 0 || facing >= paths.length) {
        console.error("iou: firefoxcharge inject config facing out of template range");
        process.exit(1);
      }
      const second = (facing + 4) % 10;
      if (second >= paths.length) {
        console.error("iou: firefoxcharge second frame out of template range");
        process.exit(1);
      }
      const b1 = tplBounds(paths[facing], "firefoxcharge path[facing]");
      const b2 = tplBounds(paths[second], "firefoxcharge path[second]");
      const b = { minX: Math.min(b1.minX, b2.minX), maxX: Math.max(b1.maxX, b2.maxX),
                  minY: Math.min(b1.minY, b2.minY), maxY: Math.max(b1.maxY, b2.maxY) };
      const ax = Math.max(Math.abs(b.minX), Math.abs(b.maxX)) * 0.35 * s45;
      x0 = cX - ax; x1 = cX + ax;
      y0 = cY + b.minY * 0.5 * s45; y1 = cY + b.maxY * 0.5 * s45;
    } else if (cfg.name === "firefoxtail") {
      const tY = -(cfg.pos.y + 4) * S + OY;
      const r = 6 * S;
      x0 = cX - r; x1 = cX + r; y0 = tY - r; y1 = tY + r;
    } else if (cfg.name === "shine") {
      const r = 7 * S;
      x0 = cX - r; x1 = cX + r; y0 = cY - r; y1 = cY + r;
    } else {
      console.error("iou: no region derivation for inkNames effect '" + cfg.name +
        "' — extend injectRegions() with a documented derivation");
      process.exit(1);
    }
    regions.push({ name: cfg.name, x0: x0 - M, x1: x1 + M, y0: y0 - M, y1: y1 + M });
  }
  if (regions.length !== PIN.inkNames.length) {
    console.error("iou: region derivation did not cover every inkNames effect");
    process.exit(1);
  }
  return regions;
}

// device-space (240x240 C pgm) box for a canvas-space region: the C
// retarget is k = 0.2 (= /5, the same factor as the browser 5x5
// downscale) plus the dy = 45 letterbox shift; +-1 device px margin.
function deviceBox(r) {
  return {
    x0: Math.max(0, Math.floor(r.x0 / 5) - 1),
    x1: Math.min(W - 1, Math.ceil(r.x1 / 5) + 1),
    y0: Math.max(BAND_Y0, Math.floor(r.y0 / 5) + BAND_Y0 - 1),
    y1: Math.min(BAND_Y0 + BAND_H - 1, Math.ceil(r.y1 / 5) + BAND_Y0 + 1),
  };
}

function loadPgm(fp) {
  const b = fs.readFileSync(fp);
  const head = `P5\n240 240\n255\n`;
  const hb = Buffer.from(head, "ascii");
  if (b.length !== hb.length + 240 * 240 || !b.subarray(0, hb.length).equals(hb)) {
    console.error(`iou: ${fp}: not the expected 240x240 P5 PGM`);
    process.exit(1);
  }
  return b.subarray(hb.length);
}

const results = [];
let minIou = Infinity;
let fail = 0;
for (const f of exp.sampledFrames) {
  const tag = String(f).padStart(4, "0");
  const maskFp = path.join(CANVAS, `f${tag}.mask.bin`);
  const pgmFp = path.join(RENDER, `f${tag}.pgm`);
  if (!fs.existsSync(maskFp) || fs.statSync(maskFp).size === 0) {
    console.error(`iou: ${maskFp}: missing or empty mask (corpus pin: every pinned frame needs a non-empty mask)`);
    process.exit(1);
  }
  const src = fs.readFileSync(maskFp);
  if (src.length !== SRC_W * SRC_H) {
    console.error(`iou: ${maskFp}: ${src.length} bytes (want ${SRC_W * SRC_H})`);
    process.exit(1);
  }
  const cink = loadPgm(pgmFp);

  // C ink must stay inside the letterbox band
  for (let y = 0; y < 240; y++) {
    if (y >= BAND_Y0 && y < BAND_Y0 + BAND_H) continue;
    for (let x = 0; x < W; x++) {
      if (cink[y * W + x]) {
        console.error(`iou: frame ${f}: C ink outside the letterbox band at (${x},${y})`);
        process.exit(1);
      }
    }
  }

  // browser 5x5 any-ink downscale + IoU over the band
  let inter = 0, union = 0;
  for (let Y = 0; Y < BAND_H; Y++) {
    for (let X = 0; X < W; X++) {
      let a = 0;
      for (let dy = 0; dy < 5 && !a; dy++) {
        const row = (Y * 5 + dy) * SRC_W + X * 5;
        for (let dx = 0; dx < 5; dx++) {
          if (src[row + dx]) { a = 1; break; }
        }
      }
      const b = cink[(Y + BAND_Y0) * W + X] ? 1 : 0;
      if (a & b) inter++;
      if (a | b) union++;
    }
  }
  if (union === 0) {
    console.error(`iou: frame ${f}: both masks empty — degenerate sample`);
    process.exit(1);
  }
  const iou = inter / union;
  if (iou < minIou) minIou = iou;
  const pass = iou >= exp.iouThreshold;
  if (!pass) fail++;
  results.push({ frame: f, iou: iou, intersection: inter, union: union, pass: pass });
  console.log(`IOU f${tag} ${iou.toFixed(4)} (${inter}/${union}) ${pass ? "PASS" : "FAIL"}`);
}

// --- per-effect injection assertions (review-65 M2, iter 67) ---------------
// Runs AFTER the frame loop so the injection frame's aggregate IoU line
// is already on record: a missing effect small enough to pass the
// aggregate is exactly what these tripwires exist to catch.
const injResults = [];
{
  const tag = String(INJ.frame).padStart(4, "0");
  const src = fs.readFileSync(path.join(CANVAS, `f${tag}.mask.bin`));
  if (src.length !== SRC_W * SRC_H) {
    console.error(`iou: injection-frame browser mask has ${src.length} bytes (want ${SRC_W * SRC_H})`);
    process.exit(1);
  }
  const withInk = loadPgm(path.join(RENDER, `f${tag}.pgm`));
  const noInk = loadPgm(path.join(RENDER_NOINJ, `f${tag}.pgm`));
  const regions = injectRegions();

  for (const r of regions) {
    // (i) browser mask ink in the canvas-space region
    let browser = 0;
    const bx0 = Math.max(0, Math.floor(r.x0)), bx1 = Math.min(SRC_W - 1, Math.ceil(r.x1));
    const by0 = Math.max(0, Math.floor(r.y0)), by1 = Math.min(SRC_H - 1, Math.ceil(r.y1));
    for (let y = by0; y <= by1; y++) {
      for (let x = bx0; x <= bx1; x++) {
        if (src[y * SRC_W + x]) browser++;
      }
    }
    // (ii) C with-inject ink in the device-space box, plus the bg
    // honesty report (no-inject baseline ink sharing the region — the
    // amount by which (i)/(ii) are weakened; (iii) is unaffected).
    // (iii) LEAVE-ONE-OUT differential: full render vs the baseline that
    // dropped exactly this effect — attributing by construction.
    const looInk = loadPgm(path.join(`${RENDER_LOO}-${r.name}`, `f${tag}.pgm`));
    const d = deviceBox(r);
    let cink = 0, diff = 0, bg = 0;
    for (let y = d.y0; y <= d.y1; y++) {
      for (let x = d.x0; x <= d.x1; x++) {
        const a = withInk[y * W + x] ? 1 : 0;
        const n = noInk[y * W + x] ? 1 : 0;
        const l = looInk[y * W + x] ? 1 : 0;
        if (a) cink++;
        if (n) bg++;
        if (a !== l) diff++;
      }
    }
    const ok = browser > 0 && cink > 0 && diff > 0;
    console.log(`INJ ${r.name} browser=${browser} c=${cink} diff=${diff} bg=${bg} ${ok ? "OK" : "FAIL"}`);
    injResults.push({ name: r.name, browser, c: cink, diff, bg, pass: ok });
    if (!ok) {
      console.error(`iou: injected effect '${r.name}' failed its per-effect ink assertion at frame ${INJ.frame} ` +
        "(browser/c/leave-one-out diff must all be nonzero in the derived region — a side is not drawing this effect)");
      process.exit(1);
    }
  }

  // region-soundness guard: every differential pixel anywhere in the C
  // frame must fall inside the union of derived (device-space) regions —
  // an effect drawing outside its documented bounds, or the gated
  // firefoxlaunch/shineloop pair drawing anything, is a loud death.
  const boxes = regions.map(deviceBox);
  for (let y = 0; y < 240; y++) {
    for (let x = 0; x < W; x++) {
      const a = withInk[y * W + x] ? 1 : 0;
      const n = noInk[y * W + x] ? 1 : 0;
      if (a === n) continue;
      let inside = false;
      for (const d of boxes) {
        if (x >= d.x0 && x <= d.x1 && y >= d.y0 && y <= d.y1) { inside = true; break; }
      }
      if (!inside) {
        console.error(`iou: differential ink OUTSIDE every derived injection region at device (${x},${y}) ` +
          "— region derivation unsound or an unexpected effect drew (fail closed)");
        process.exit(1);
      }
    }
  }
  console.log(`INJ REGIONS SOUND (all differential ink inside ${boxes.length} derived regions)`);
}

console.log(`IOU MIN ${minIou.toFixed(4)} threshold ${exp.iouThreshold} frames ${exp.sampledFrames.length}`);
if (REPORT && REPORT !== true) {
  fs.writeFileSync(REPORT, JSON.stringify({
    threshold: exp.iouThreshold,
    minIou: minIou,
    inject: { frame: INJ.frame, effects: injResults },
    frames: results,
  }, null, 2) + "\n");
}
if (fail > 0) {
  console.error(`iou: ${fail} frame(s) below threshold`);
  process.exit(1);
}
console.log("IOU OK");
