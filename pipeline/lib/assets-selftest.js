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
const { resizeRgba, axisWeights, divRound, pack565, quant565, gammaTable, mapRgb,
  SRGB_TO_LIN, LIN_TO_SRGB, LIN_MAX } = require("./img1");

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
// The sRGB transfer tables get the same treatment: this file derives its own
// pair from the IEC 61966-2-1 definition by an independent route (a monotone
// two-pointer scan over the exact-integer characterization, not img1.js's
// per-code binary search) and asserts the production tables are identical,
// entry for entry. The characterization is complete, so agreement is proof of
// correctness and not just of mutual consistency: with s = c/255,
// (s+0.055)/1.055 = N/D for N = 1000c+14025, D = 269025, and 2.4 = 12/5, so
// L = round(4095*(N/D)^(12/5)) is the UNIQUE integer with
//     (2L-1)^5 * D^12  <=  32 * 4095^5 * N^12  <  (2L+1)^5 * D^12.
const O_LIN_MAX = 4095;
const oS2L = (() => {
  const D12 = 269025n ** 12n, K = 32n * 4095n ** 5n;
  const t = new Array(256);
  let L = 0;
  for (let c = 0; c <= 255; c++) {
    if (c <= 10) { // toe: L = round(4095 * (c/255)/12.92)
      t[c] = Number((2n * 4095n * BigInt(c) * 1000n + 3294600n) / (2n * 3294600n));
      L = t[c];
      continue;
    }
    const lhs = K * BigInt(1000 * c + 14025) ** 12n;
    while (BigInt(2 * L + 1) ** 5n * D12 <= lhs) L++; // monotone in c
    t[c] = L;
  }
  return t;
})();
// inverse: nearest code in LINEAR distance, ties to the brighter code
const oL2S = (() => {
  const t = new Array(O_LIN_MAX + 1);
  for (let L = 0; L <= O_LIN_MAX; L++) {
    let best = 0, bestErr = Infinity;
    for (let c = 0; c < 256; c++) {
      const e = Math.abs(oS2L[c] - L);
      if (e <= bestErr) { best = c; bestErr = e; }
    }
    t[L] = best;
  }
  return t;
})();
assert.strictEqual(LIN_MAX, O_LIN_MAX, "LIN_MAX drifted");
assert.deepStrictEqual([...SRGB_TO_LIN], oS2L,
  "sRGB->linear table disagrees with the independent exact-rational derivation");
assert.deepStrictEqual([...LIN_TO_SRGB], oL2S,
  "linear->sRGB table is not the nearest-code inverse (ties bright)");
for (let c = 0; c < 256; c++) {
  assert.strictEqual(oL2S[oS2L[c]], c, `transfer round trip broken at code ${c}`);
  if (c) assert(oS2L[c] > oS2L[c - 1], `transfer table not strictly increasing at ${c}`);
}
// A SECOND, differently-derived partner for the forward table (review-c4-6o
// [L]): the BigInt derivation above is a rewrite of the same algebraic
// reduction, so a shared misreading of the spec (wrong toe threshold, wrong
// exponent) would agree with itself. Float Math.pow — legal here, this is the
// validator, never the pixel path — reads the EOTF straight off the standard
// and must land within half a step of every entry.
{
  let worst = 0;
  for (let c = 0; c < 256; c++) {
    const s = c / 255;
    const f = s <= 0.04045 ? s / 12.92 : Math.pow((s + 0.055) / 1.055, 2.4);
    const d = Math.abs(SRGB_TO_LIN[c] - O_LIN_MAX * f);
    assert(d <= 0.5 + 1e-9, `SRGB_TO_LIN[${c}] = ${SRGB_TO_LIN[c]} is ${d} from the EOTF`);
    worst = Math.max(worst, d);
  }
  assert(worst > 0.4, `EOTF differential never approached a half step (worst ${worst}) ` +
    `— the comparison is too loose to detect an off-by-one`);
  ok(`sRGB transfer tables == independent exact-integer derivation AND within ` +
    `half a step of the float EOTF (worst ${worst.toFixed(4)})`);
}

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
          sr += BigInt(oS2L[rgba[p]]) * wa;
          sg += BigInt(oS2L[rgba[p + 1]]) * wa;
          sb += BigInt(oS2L[rgba[p + 2]]) * wa;
          sa += wa;
        }
      }
      const o = (y * dw + x) * 4;
      out[o + 3] = Number(half(sa, total));
      if (sa > 0n) {
        out[o] = oL2S[Number(half(sr, sa))];
        out[o + 1] = oL2S[Number(half(sg, sa))];
        out[o + 2] = oL2S[Number(half(sb, sa))];
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

// divRound must be EXACT over the whole accepted domain, not just over the
// magnitudes the real assets reach (review-c4-4 [M]). The old
// floor((2n+d)/(2d)) form doubled first and lost a unit above 2^52; this
// asserts the exact BigInt answer on that counterexample and on a
// deterministic sweep (a fixed LCG — never Math.random: this file must give
// the same verdict on every run).
{
  const exact = (n, d) => Number((2n * BigInt(n) + BigInt(d)) / (2n * BigInt(d)));
  const cases = [[9003899914072156, 2199560257499], [0, 1], [1, 2], [3, 2],
    [Number.MAX_SAFE_INTEGER, 1], [Number.MAX_SAFE_INTEGER, Number.MAX_SAFE_INTEGER],
    [Number.MAX_SAFE_INTEGER - 1, 2], [501289200000, 240000]];
  let seed = 0x5eed1234;
  const next = () => (seed = (seed * 1103515245 + 12345) >>> 0);
  for (let i = 0; i < 20000; i++) {
    const d = 1 + (next() % 1000003);
    const n = next() * 4194304 + (next() % 4194304); // up to ~2^54, clamped below
    cases.push([Math.min(n, Number.MAX_SAFE_INTEGER), d]);
  }
  for (const [n, d] of cases) {
    assert.strictEqual(divRound(n, d), exact(n, d), `divRound(${n}, ${d})`);
  }
  assert.strictEqual(divRound(9003899914072156, 2199560257499), 4093,
    "the review-c4-4 counterexample regressed");
  ok(`divRound exact vs BigInt on ${cases.length} pairs incl. the 2^52 counterexample`);
}

// axisWeights' domain is closed too — the exactness of every weight rests on
// srcN*dstN staying a safe integer (review-c4-4 [M]).
mustThrow("axisWeights rejects an unsafe axis product",
  () => axisWeights(1 << 27, 1 << 27), /out of domain/);
mustThrow("axisWeights rejects a fractional axis",
  () => axisWeights(10, 2.5), /out of domain/);
mustThrow("axisWeights rejects a zero axis",
  () => axisWeights(10, 0), /out of domain/);

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
// These two hit ties dead on: codes 10 and 11 sit at linear 12 and 14, whose
// average 13 is equidistant from both (the tie now lands on the inverse
// table's tie rule), and alpha 0 and 1 average to 0.5. Both must round UP.
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
ok("tie handling pinned: the colour case is the INVERSE TABLE's midpoint tie " +
  "(linear 12/14 -> 13 -> code 11), the alpha case is a true divRound .5 — both up");

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
  // A quarter of the LIGHT of a full-intensity pixel, re-encoded: 4095/4
  // rounds to linear 1024, which is sRGB 137 — NOT 64. sRGB-space averaging
  // (the old behaviour) would give 64 and throw away 46% of the light.
  const quarter = oL2S[Number((2n * BigInt(oS2L[255]) + 4n) / 8n)];
  assert.strictEqual(quarter, 137, "quarter-light reference value drifted");
  assert.strictEqual(out[((0 * 6) + 1) * 4], quarter,
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
  // Each destination cell averages 4 source steps IN LIGHT; the expected
  // value is derived from the independent tables, never typed in. Red must
  // depend only on x and green only on y — that is the flip/transpose tooth.
  const band = (k) => {
    let s = 0n;
    for (let j = 0; j < 4; j++) s += BigInt(oS2L[(k * 4 + j) * 16]);
    return oL2S[Number((2n * s + 4n) / 8n)];
  };
  const wantBand = [0, 1, 2, 3].map(band);
  assert.deepStrictEqual(wantBand, [29, 90, 153, 217], "gradient band reference drifted");
  for (let y = 0; y < 4; y++) for (let x = 0; x < 4; x++) {
    const i = (y * 4 + x) * 4;
    assert.strictEqual(out[i], wantBand[x], `gradient red drifted at (${x},${y})`);
    assert.strictEqual(out[i + 1], wantBand[y], `gradient green drifted at (${x},${y})`);
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
// the resampler's numeric DOMAIN is closed too (review-c4-2 [M]): fractional
// or non-integer sizes used to be accepted and produce a garbage buffer, and
// the exactness of the integer accumulators is a bounded claim, not a hope.
mustThrow("fractional destination width rejected",
  () => resizeRgba(constImg(10, 10, 1, 2, 3, 255), 1.5, 1), /dw=1.5 is out of domain/);
mustThrow("zero destination height rejected",
  () => resizeRgba(constImg(10, 10, 1, 2, 3, 255), 5, 0), /dh=0 is out of domain/);
mustThrow("NaN size rejected",
  () => resizeRgba(constImg(10, 10, 1, 2, 3, 255), NaN, 5), /dw=NaN is out of domain/);
mustThrow("rgba length mismatch rejected",
  () => resizeRgba({ w: 10, h: 10, rgba: Buffer.alloc(4) }, 5, 5), /bytes != 10\*10\*4/);
mustThrow("accumulator-overflowing source rejected", () => resizeRgba(
  { w: 1 << 24, h: 1 << 24, rgba: { length: (1 << 24) * (1 << 24) * 4 } }, 2, 2),
/would exceed exact integer range/);

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

// ---- 3b. the ENCODER stores the NEAREST representable 565 ----------------
// pack565 (above) is the RENDERER's quantizer and stays truncation. The
// encoder must instead pick the code whose bit-replicated expansion — what
// img1_blit actually puts on screen — is closest to the source byte, because
// truncation is a one-sided ~-3.5/255 loss per 5-bit channel that costs 20%
// of the mean luminance on near-black stage art (.loop/c4-dim/REPORT.md).
// Asserted as the DEFINING property (nearest over the whole lattice, ties to
// the brighter code), not by re-running the production search.
{
  const rep5 = (k) => (k << 3) | (k >> 2), rep6 = (k) => (k << 2) | (k >> 4);
  let strictlyBetter = 0;
  const darkNear = { 5: 0, 6: 0 }, darkTrunc = { 5: 0, 6: 0 };
  for (let v = 0; v < 256; v++) {
    const code = quant565(v, v, v);
    const got = { 5: [(code >> 11) & 31, code & 31], 6: [(code >> 5) & 63] };
    assert.strictEqual(got[5][0], got[5][1], `red/blue 5-bit codes disagree at ${v}`);
    for (const [bits, rep, n] of [[5, rep5, 32], [6, rep6, 64]]) {
      const k = got[bits][0], e = Math.abs(rep(k) - v);
      for (let j = 0; j < n; j++) {
        const ej = Math.abs(rep(j) - v);
        assert(ej > e || (ej === e && j <= k),
          `${bits}-bit code for ${v} is ${k} (err ${e}), but ${j} has err ${ej}`);
      }
      // never worse than the truncating renderer, and strictly better often
      const t = v >> (8 - bits), et = Math.abs(rep(t) - v);
      assert(e <= et, `nearest ${bits}-bit code for ${v} is worse than truncation`);
      assert(e <= (bits === 5 ? 4 : 2), `${bits}-bit error ${e} at ${v} exceeds half a step`);
      if (e < et) strictlyBetter++;
      if (v < 16) { darkNear[bits] += rep(k) - v; darkTrunc[bits] += rep(t) - v; }
    }
    // FIXED POINT: what is stored is still a code the renderer can produce,
    // so an image pixel and a vector fill of the SAME 565 code still agree.
    assert.strictEqual(pack565(rep5(got[5][0]), rep6(got[6][0]), rep5(got[5][1])), code,
      `stored code for ${v} is not a pack565 fixed point`);
  }
  // THE MEASUREMENT THAT MATTERS. Averaged over all 256 codes, bit-replicated
  // truncation is unbiased (the replicated low bits pay the debt back at the
  // top of each 4-code group) — which is how FORMATS.md §7.2 talked itself
  // into "invisible". Restricted to the DARK end, where the stage art
  // actually lives, the debt is never paid: truncation runs -3.5/255 per
  // 5-bit channel, exactly the constant §7.2 measured and dismissed.
  assert.strictEqual(darkTrunc[5] / 16, -3.5, "dark-end truncation bias drifted");
  assert.strictEqual(darkNear[5] / 16, 0.5, "dark-end nearest bias drifted");
  assert(Math.abs(darkNear[6] / 16) < Math.abs(darkTrunc[6] / 16),
    "nearest did not reduce the dark-end 6-bit bias");
  assert(strictlyBetter === 80, `nearest-565 differs from truncation on ` +
    `${strictlyBetter} of 512 (v,channel) pairs, expected 80`);
  ok(`encoder stores nearest representable 565 (dark-end bias -3.5 -> +0.5 per ` +
    `5-bit channel, every code a pack565 fixed point)`);
}

// ---- 4. the stagePreview tone map (a DEVIATION, held to the same bar) ---
// gammaTable is the one place where we deliberately depart from upstream's
// pixels (FORMATS.md §7.2, owner ruling 2026-07-28). It still has to be an
// exact integer table: asserted here against the defining inequality
// (2L-1)^q <= 2^q * 255^(q-p) * v^p < (2L+1)^q, computed independently.
{
  for (const [p, q] of [[3, 4], [13, 20], [1, 1]]) {
    const t = gammaTable([p, q]);
    assert.strictEqual(t.length, 256, `gammaTable(${p}/${q}) length`);
    assert.strictEqual(t[0], 0, `gammaTable(${p}/${q})[0]`);
    assert.strictEqual(t[255], 255, `gammaTable(${p}/${q})[255]`);
    for (let v = 0; v < 256; v++) {
      const L = BigInt(t[v]);
      const mid = 2n ** BigInt(q) * 255n ** BigInt(q - p) * BigInt(v) ** BigInt(p);
      // L=0 has its OWN lower arm: for even q the (2L-1)^q form evaluates to
      // 1 and would be a false bound, so it is stated as M(v) < 1, i.e. v=0
      // (review-c4-3 [L]: the masked exception was hiding a real hole).
      if (t[v] === 0) {
        assert(mid < 1n, `gammaTable(${p}/${q})[${v}] = 0 but M(${v}) = ${mid} >= 1`);
        assert.strictEqual(v, 0, `gammaTable(${p}/${q}) zeroed a non-zero input ${v}`);
      } else {
        assert((2n * L - 1n) ** BigInt(q) <= mid,
          `gammaTable(${p}/${q})[${v}] = ${t[v]} is too high`);
      }
      assert(mid < (2n * L + 1n) ** BigInt(q),
        `gammaTable(${p}/${q})[${v}] = ${t[v]} is too low`);
      if (v) assert(t[v] >= t[v - 1], `gammaTable(${p}/${q}) not monotone at ${v}`);
      assert(t[v] >= v, `gammaTable(${p}/${q})[${v}] darkens (${t[v]} < ${v})`);
      // second, independent partner: plain float Math.pow (legal HERE — this
      // is the validator, not the pixel path) must agree within half a step
      assert(Math.abs(t[v] - 255 * Math.pow(v / 255, p / q)) <= 0.5 + 1e-9,
        `gammaTable(${p}/${q})[${v}] = ${t[v]} disagrees with Math.pow`);
    }
  }
  // identity must be exactly identity, so "no gamma" and "gamma 1" agree
  assert.deepStrictEqual([...gammaTable([1, 1])], [...Array(256).keys()],
    "gammaTable(1/1) is not the identity");
  // the shipped lift, at the luminance the stage art actually lives at
  const g = gammaTable([3, 4]);
  assert.strictEqual(g[9], 21, "gamma 0.75 at the stage art's mean Y drifted");
  // alpha must survive a tone map untouched (alpha classes are pinned)
  const rgba = Buffer.from([9, 9, 9, 0, 9, 9, 9, 128, 9, 9, 9, 255]);
  const mapped = mapRgb(rgba, g);
  assert.deepStrictEqual([...mapped], [21, 21, 21, 0, 21, 21, 21, 128, 21, 21, 21, 255],
    "mapRgb touched alpha or missed a channel");
  ok("stagePreview tone map: exact integer table, identity at 1/1, alpha untouched");
}
mustThrow("darkening gamma is out of domain",
  () => gammaTable([5, 4]), /out of domain/);
mustThrow("non-integer gamma is out of domain",
  () => gammaTable([0.75, 1]), /out of domain/);

console.log(`assets self-test: ${checks} property groups OK ` +
  `(decoder rejection domain, exact resample weights, premultiplied alpha, ` +
  `linear-light transfer tables, 565 truncation + nearest-565 encoding, ` +
  `stagePreview tone map)`);
