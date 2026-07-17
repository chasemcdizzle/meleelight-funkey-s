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
// Usage: node iou.js --canvas DIR --render DIR --expected expected-render.json
//        [--report out.json]
// Prints one line per frame + a MIN line; exit 0 iff every frame passes.
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
const EXPECTED = arg("expected", "");
const REPORT = arg("report", "");
if (!CANVAS || !RENDER || !EXPECTED || CANVAS === true || RENDER === true || EXPECTED === true) {
  console.error("iou: --canvas, --render, --expected are required");
  process.exit(1);
}

const exp = JSON.parse(fs.readFileSync(EXPECTED, "utf8"));
// Corpus pin (review-44 fix 1): the frozen 16-frame corpus, exactly —
// count pinned both here and in expected-render.json (sampledFrameCount),
// every frame a unique positive integer. Changing the corpus is a
// reviewed repo change to BOTH files, never a runtime degradation.
if (!Array.isArray(exp.sampledFrames) || !Number.isInteger(exp.sampledFrameCount) ||
    exp.sampledFrameCount !== 16 || exp.sampledFrames.length !== exp.sampledFrameCount) {
  console.error("iou: corpus pin violated — expected-render.json must pin exactly 16 sampled frames (sampledFrames + sampledFrameCount)");
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
const W = 240, BAND_Y0 = 45, BAND_H = 150;
const SRC_W = 1200, SRC_H = 750;

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

console.log(`IOU MIN ${minIou.toFixed(4)} threshold ${exp.iouThreshold} frames ${exp.sampledFrames.length}`);
if (REPORT && REPORT !== true) {
  fs.writeFileSync(REPORT, JSON.stringify({
    threshold: exp.iouThreshold,
    minIou: minIou,
    frames: results,
  }, null, 2) + "\n");
}
if (fail > 0) {
  console.error(`iou: ${fail} frame(s) below threshold`);
  process.exit(1);
}
console.log("IOU OK");
