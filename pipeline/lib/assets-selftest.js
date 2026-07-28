#!/usr/bin/env node
"use strict";
// Independent validator for the two novel algorithms in the assets stage
// (review-a9-1): the PNG decoder's REJECTION domain and the resampler's
// arithmetic. Every other novel step has a differential partner — PNG
// decode vs ffmpeg, container parse vs the C loader, blit vs
// rast_blit_rgba — but the resampled pixels were checked only by
// artifactsSha256, which freezes whatever the FIRST run produced: that
// proves stability, not correctness. These assertions turn FORMATS.md
// §7.4's argued properties into measured ones.
// Usage: node lib/assets-selftest.js

const assert = require("assert");
const { decodePng } = require("./png");
const { resizeRgba, axisWeights, pack565 } = require("./img1");

let checks = 0;
const ok = (what) => { checks++; void what; };
const mustThrow = (what, fn, re) => {
  let threw = null;
  try { fn(); } catch (e) { threw = e; }
  assert(threw, `${what}: expected a hard throw, got none`);
  assert(re.test(threw.message), `${what}: threw "${threw.message}", expected /${re.source}/`);
  ok(what);
};

// ---- 1. PNG decoder: the accepted domain is CLOSED -----------------------
// Built from a real 1x1 RGBA PNG so only the field under test differs.
const zlib = require("zlib");
const chunk = (type, data, declaredLen) => {
  const b = Buffer.alloc(12 + data.length);
  b.writeUInt32BE(declaredLen === undefined ? data.length : declaredLen, 0);
  b.write(type, 4, "latin1");
  data.copy(b, 8);
  return b; // CRC left zero: our decoder does not verify CRC (documented)
};

function makePng(mut) {
  const ihdr = Buffer.alloc(13);
  ihdr.writeUInt32BE(1, 0); ihdr.writeUInt32BE(1, 4);
  ihdr[8] = 8;   // bit depth
  ihdr[9] = 6;   // colour type RGBA
  ihdr[10] = 0; ihdr[11] = 0; ihdr[12] = 0; // compression/filter/interlace
  const idat = zlib.deflateSync(Buffer.from([0, 1, 2, 3, 4])); // filter 0 + RGBA
  const opts = { ihdr, idat, extra: [], omitIhdr: false, omitIdat: false };
  if (mut) mut(opts);
  return Buffer.concat([
    Buffer.from([0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a]),
    ...(opts.omitIhdr ? [] : [chunk("IHDR", opts.ihdr)]),
    ...opts.extra,
    ...(opts.omitIdat ? [] : [chunk("IDAT", opts.idat)]),
    chunk("IEND", Buffer.alloc(0)),
  ]);
}

// the control: an in-domain file decodes to exactly its pixel
{
  const px = decodePng(makePng(null), "control");
  assert.strictEqual(px.w, 1);
  assert.strictEqual(px.h, 1);
  assert.deepStrictEqual([...px.rgba], [1, 2, 3, 4]);
  ok("control 1x1 RGBA decodes exactly");
}
mustThrow("bit depth 16 rejected",
  () => decodePng(makePng((o) => { o.ihdr[8] = 16; }), "x"), /bit depth 16/);
mustThrow("colour type 3 (palette) rejected",
  () => decodePng(makePng((o) => { o.ihdr[9] = 3; }), "x"), /colour type 3/);
mustThrow("interlaced rejected",
  () => decodePng(makePng((o) => { o.ihdr[12] = 1; }), "x"), /interlaced/);
mustThrow("PLTE chunk rejected", () => decodePng(makePng((o) => {
  o.extra.push(chunk("PLTE", Buffer.alloc(3)));
}), "x"), /unsupported chunk PLTE/);
mustThrow("tRNS chunk rejected", () => decodePng(makePng((o) => {
  o.extra.push(chunk("tRNS", Buffer.alloc(2)));
}), "x"), /unsupported chunk tRNS/);
mustThrow("bad signature rejected",
  () => decodePng(Buffer.alloc(64), "x"), /not a PNG/);
mustThrow("short IDAT rejected", () => decodePng(makePng((o) => {
  o.idat = zlib.deflateSync(Buffer.from([0, 1, 2, 3])); // one byte short
}), "x"), /inflated \d+ bytes/);
// One tooth per remaining rejection branch in png.js (round 2 [M]: only 7
// of them were covered, so deleting e.g. the duplicate-IHDR check stayed
// green).
mustThrow("duplicate IHDR rejected", () => decodePng(makePng((o) => {
  o.extra.push(chunk("IHDR", o.ihdr));
}), "x"), /duplicate IHDR/);
mustThrow("compression method != 0 rejected",
  () => decodePng(makePng((o) => { o.ihdr[10] = 1; }), "x"), /compression method 1/);
mustThrow("filter method != 0 rejected",
  () => decodePng(makePng((o) => { o.ihdr[11] = 1; }), "x"), /filter method 1/);
mustThrow("missing IHDR rejected",
  () => decodePng(makePng((o) => { o.omitIhdr = true; }), "x"), /no IHDR/);
mustThrow("missing IDAT rejected",
  () => decodePng(makePng((o) => { o.omitIdat = true; }), "x"), /no IDAT/);
mustThrow("degenerate size rejected",
  () => decodePng(makePng((o) => { o.ihdr.writeUInt32BE(0, 0); }), "x"), /degenerate size/);
mustThrow("truncated chunk rejected", () => decodePng(makePng((o) => {
  o.extra.push(chunk("tEXt", Buffer.alloc(0), 0xffff)); // declares more than it has
}), "x"), /truncated chunk tEXt/);
mustThrow("unknown row filter rejected", () => decodePng(makePng((o) => {
  o.idat = zlib.deflateSync(Buffer.from([5, 1, 2, 3, 4])); // filter type 5
}), "x"), /unknown filter type 5/);

// ---- 2. resampler: an INDEPENDENT oracle, not just invariants -----------
// Round 2 of review broke the first version of this section: symmetric
// fixtures (constant colours, an axis-aligned halo boundary) are satisfied
// by a VERTICALLY FLIPPED resampler and by STRAIGHT-alpha averaging. Sums
// and symmetries are not correctness. So the primary check is now a
// second, independent implementation — exact rational arithmetic in BigInt
// over the source rectangle, sharing NO code with lib/img1.js (not even
// axisWeights) — run over the REAL source images at the REAL target sizes.
//
// oracleResize: for destination pixel (x,y), the source rectangle is
// [x*sw/dw, (x+1)*sw/dw) x [y*sh/dh, (y+1)*sh/dh). Coverage of source
// pixel (j,i) is the exact overlap length product, kept as an exact
// rational by scaling by dw (resp. dh) — i.e. integer overlaps, but
// derived here from the interval endpoints directly rather than from a
// precomputed weight table, and accumulated in BigInt so there is no
// shared rounding path either.
function oracleResize(src, dw, dh) {
  const { w: sw, h: sh, rgba } = src;
  const ov = (aLo, aHi, bLo, bHi) => {
    const lo = aLo > bLo ? aLo : bLo, hi = aHi < bHi ? aHi : bHi;
    return hi > lo ? hi - lo : 0;
  };
  const out = Buffer.alloc(dw * dh * 4);
  const total = BigInt(sw) * BigInt(sh);
  const half = (num, den) => (2n * num + den) / (2n * den); // round half up
  for (let y = 0; y < dh; y++) {
    const yLo = y * sh, yHi = (y + 1) * sh; // scaled by dh
    for (let x = 0; x < dw; x++) {
      const xLo = x * sw, xHi = (x + 1) * sw; // scaled by dw
      let sr = 0n, sg = 0n, sb = 0n, sa = 0n;
      for (let i = 0; i < sh; i++) {
        const wy = ov(yLo, yHi, i * dh, (i + 1) * dh);
        if (!wy) continue;
        for (let j = 0; j < sw; j++) {
          const wx = ov(xLo, xHi, j * dw, (j + 1) * dw);
          if (!wx) continue;
          const p = (i * sw + j) * 4;
          const wa = BigInt(wx) * BigInt(wy) * BigInt(rgba[p + 3]);
          sr += BigInt(rgba[p]) * wa;
          sg += BigInt(rgba[p + 1]) * wa;
          sb += BigInt(rgba[p + 2]) * wa;
          sa += wa;
        }
      }
      const o = (y * dw + x) * 4;
      out[o + 3] = Number(half(sa, total));
      if (sa > 0n) {
        out[o] = Number(half(sr, sa));
        out[o + 1] = Number(half(sg, sa));
        out[o + 2] = Number(half(sb, sa));
      }
    }
  }
  return out;
}

// Per destination cell the weights must sum to exactly `src`, and each
// source index's total contribution across destinations must be exactly
// `dst`. Both are integers; a float anywhere would show up here.
function sweepWeights(src, dst) {
  const rows = axisWeights(src, dst);
  assert.strictEqual(rows.length, dst, `axisWeights(${src},${dst}) row count`);
  const perSource = new Array(src).fill(0);
  for (let i = 0; i < dst; i++) {
    let sum = 0;
    for (const [j, w] of rows[i]) {
      assert(Number.isInteger(w) && w > 0, `axisWeights(${src},${dst})[${i}]: weight ${w}`);
      assert(j >= 0 && j < src, `axisWeights(${src},${dst})[${i}]: index ${j}`);
      sum += w;
      perSource[j] += w;
    }
    assert.strictEqual(sum, src, `axisWeights(${src},${dst})[${i}] sums to ${sum}, want ${src}`);
  }
  for (let j = 0; j < src; j++) {
    assert.strictEqual(perSource[j], dst,
      `axisWeights(${src},${dst}): source ${j} total ${perSource[j]}, want ${dst}`);
  }
}
// the pairs this stage actually uses, then a broad sweep
for (const [s, d] of [[800, 65], [300, 24], [72, 24], [95, 32], [58, 58], [45, 45]]) {
  sweepWeights(s, d);
}
for (let s = 1; s <= 120; s++) for (let d = 1; d <= s; d++) sweepWeights(s, d);
ok("axis weights exact over used pairs + all (src<=120, dst<=src)");

const constImg = (w, h, r, g, b, a) => {
  const rgba = Buffer.alloc(w * h * 4);
  for (let i = 0; i < w * h; i++) {
    rgba[i * 4] = r; rgba[i * 4 + 1] = g; rgba[i * 4 + 2] = b; rgba[i * 4 + 3] = a;
  }
  return { w, h, rgba };
};

// A constant image must resample to the SAME constant — any weight or
// rounding asymmetry shows up as a drifted channel.
for (const [a, label] of [[255, "opaque"], [128, "partial alpha"], [1, "near-transparent"]]) {
  const out = resizeRgba(constImg(800, 300, 200, 100, 50, a), 65, 24);
  for (let i = 0; i < 65 * 24; i++) {
    assert.deepStrictEqual([out[i * 4], out[i * 4 + 1], out[i * 4 + 2], out[i * 4 + 3]],
      [200, 100, 50, a], `constant ${label} resample drifted at pixel ${i}`);
  }
  ok(`constant image (${label}) resamples to itself`);
}

// Fully transparent input: alpha 0 out, and RGB must NOT be invented.
{
  const out = resizeRgba(constImg(72, 95, 255, 255, 255, 0), 24, 32);
  for (let i = 0; i < 24 * 32; i++) {
    assert.deepStrictEqual([out[i * 4], out[i * 4 + 1], out[i * 4 + 2], out[i * 4 + 3]],
      [0, 0, 0, 0], `transparent resample leaked colour at pixel ${i}`);
  }
  ok("fully transparent image stays transparent (no colour leak)");
}

// THE HALO CASE with the boundary deliberately INSIDE a destination cell
// (round 2: an axis-aligned boundary is satisfied by straight-alpha
// averaging, because no destination pixel ever mixes the two regions).
// 9 source columns -> 2 destination columns: destination column 0 covers
// source [0,4.5), straddling the transparent-white/opaque-black edge at 4.
// Premultiplied: 4 white pixels contribute NOTHING (alpha 0), so the colour
// is pure black and alpha = 0.5*255 rounded. Straight averaging would drag
// the colour towards white and fail here.
{
  const w = 9, h = 1;
  const rgba = Buffer.alloc(w * h * 4);
  for (let x = 0; x < w; x++) {
    const i = x * 4;
    if (x < 4) { rgba[i] = 255; rgba[i + 1] = 255; rgba[i + 2] = 255; rgba[i + 3] = 0; }
    else { rgba[i + 3] = 255; }
  }
  const out = resizeRgba({ w, h, rgba }, 2, 1);
  // cell 0 covers source [0,4.5): 0.5 of an opaque black pixel out of 4.5
  assert.deepStrictEqual([out[0], out[1], out[2]], [0, 0, 0],
    "white-under-transparent bled into a MIXED destination cell");
  assert.strictEqual(out[3], Math.round((255 * 1) / 9),
    "mixed-cell alpha is not the exact coverage average");
  ok("transparent-white does not bleed across a MIXED cell (premultiply)");
}

// ROUNDING MODE, pinned at an exact TIE. Real artwork essentially never
// produces a .5 quotient, so half-up vs half-down is unobservable on it —
// a rounding-mode change would slip through every other fixture here.
// These two hit ties dead on: colour 10 and 11 average to 10.5, alpha 0
// and 1 average to 0.5. Both must round UP.
{
  const rgba = Buffer.from([10, 10, 10, 255, 11, 11, 11, 255]);
  const out = resizeRgba({ w: 2, h: 1, rgba }, 1, 1);
  assert.deepStrictEqual([out[0], out[1], out[2], out[3]], [11, 11, 11, 255],
    "colour tie 10.5 did not round half UP");
}
{
  const rgba = Buffer.from([200, 200, 200, 0, 200, 200, 200, 1]);
  const out = resizeRgba({ w: 2, h: 1, rgba }, 1, 1);
  assert.strictEqual(out[3], 1, "alpha tie 0.5 did not round half UP");
}
ok("round-half-up pinned at exact ties (colour and alpha)");

// ASYMMETRIC fixtures: a single bright source pixel, and a monotone
// gradient. Both detect a flipped, transposed or offset resampler — which
// the constant-colour cases above cannot (round 2 [M]).
{
  const w = 12, h = 8;
  const rgba = Buffer.alloc(w * h * 4);
  for (let i = 0; i < w * h; i++) rgba[i * 4 + 3] = 255;
  rgba[((1 * w) + 2) * 4] = 255; // one red pixel near the top-left
  const out = resizeRgba({ w, h, rgba }, 6, 4);
  const red = [];
  for (let i = 0; i < 6 * 4; i++) if (out[i * 4] !== 0) red.push(i);
  assert.deepStrictEqual(red, [(0 * 6) + 1],
    `impulse landed at ${JSON.stringify(red)}, expected destination cell (1,0)`);
  assert.strictEqual(out[((0 * 6) + 1) * 4], Math.round(255 / 4),
    "impulse energy was not spread over exactly the covering cell");
  ok("impulse lands in the correct destination cell (detects flip/transpose)");
}
{
  const w = 16, h = 16;
  const rgba = Buffer.alloc(w * h * 4);
  for (let y = 0; y < h; y++) for (let x = 0; x < w; x++) {
    const i = (y * w + x) * 4;
    rgba[i] = x * 16; rgba[i + 1] = y * 16; rgba[i + 2] = 0; rgba[i + 3] = 255;
  }
  const out = resizeRgba({ w, h, rgba }, 4, 4);
  for (let y = 0; y < 4; y++) for (let x = 0; x < 4; x++) {
    const i = (y * 4 + x) * 4;
    assert.strictEqual(out[i], x * 64 + 24, `gradient red drifted at (${x},${y})`);
    assert.strictEqual(out[i + 1], y * 64 + 24, `gradient green drifted at (${x},${y})`);
  }
  ok("monotone gradient maps exactly (detects flip/transpose/offset)");
}

// THE PRIMARY RESAMPLER CHECK: the production resampler must agree with
// the independent BigInt oracle byte-for-byte on REAL artwork at the REAL
// target sizes. This is what a flipped, transposed or straight-alpha
// implementation cannot survive.
{
  const fs = require("fs");
  const path = require("path");
  const distRoot = process.env.MELEELIGHT_CLONE ||
    path.join(process.env.HOME, ".cache", "meleelight-funkey-s", "upstream");
  const cases = [
    ["stage-icons/bf.png", 65], ["stage-icons/fod.png", 65],
    ["stage-icons/Icon_Transparent_Question.png", 65],
    ["hand/handpoint.png", 24], ["hand/handgrab.png", 24],
    ["css/marth.png", 58],
  ];
  let px = 0;
  for (const [rel, w] of cases) {
    const abs = path.join(distRoot, "dist", "assets", ...rel.split("/"));
    if (!fs.existsSync(abs)) {
      throw new Error(`assets-selftest: missing source ${abs} — the oracle ` +
        `differential cannot be skipped silently`);
    }
    const src = decodePng(fs.readFileSync(abs), rel);
    const h = Math.round((w * src.h) / src.w);
    const got = resizeRgba(src, w, h);
    const want = oracleResize(src, w, h);
    assert(got.equals(want),
      `resampler disagrees with the independent oracle on ${rel} -> ${w}x${h}`);
    px += w * h;
  }
  ok(`resampler == independent BigInt oracle on ${cases.length} real images (${px} px)`);
}

mustThrow("upscale hard-throws",
  () => resizeRgba(constImg(10, 10, 1, 2, 3, 255), 11, 10), /upscaling is out of/);
mustThrow("upscale hard-throws (height)",
  () => resizeRgba(constImg(10, 10, 1, 2, 3, 255), 10, 11), /upscaling is out of/);

// ---- 3. 565 quantization is pack565's truncation -------------------------
{
  assert.strictEqual(pack565(255, 255, 255), 0xffff);
  assert.strictEqual(pack565(0, 0, 0), 0x0000);
  for (let v = 0; v < 256; v++) {
    assert.strictEqual(pack565(v, 0, 0), (v >> 3) << 11, `pack565 red ${v}`);
    assert.strictEqual(pack565(0, v, 0), (v >> 2) << 5, `pack565 green ${v}`);
    assert.strictEqual(pack565(0, 0, v), v >> 3, `pack565 blue ${v}`);
  }
  ok("565 quantization is truncation on all 256 channel values");
}

console.log(`assets self-test: ${checks} property groups OK ` +
  `(decoder rejection domain, exact resample weights, premultiplied alpha, 565 truncation)`);
