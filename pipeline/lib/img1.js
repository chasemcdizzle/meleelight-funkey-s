"use strict";
// IMG1 — pre-scaled RGB565 + A8 menu images (fix_plan §M4 task A9;
// spec: pipeline/FORMATS.md §7). Writer, reader and canonical dump.
//
// Everything here is INTEGER arithmetic end to end (decode -> premultiply
// -> exact-coverage box resample in LINEAR LIGHT -> unpremultiply -> nearest
// 565), so byte-stability across runs is a property of the code, not of a
// pinned external tool. The sRGB transfer tables below are built with BigInt
// exact-rational arithmetic — no Math.pow, no float anywhere on the pixel
// path. See FORMATS.md §7.2.

const NAME_MAX = 15; // 16-byte field, NUL-terminated
const DIR_ENTRY = 24;
const HDR = 12;

// --- sRGB transfer, as INTEGER tables -------------------------------------
// Box-averaging 8-bit sRGB codes averages a GAMMA-ENCODED signal, which is
// not the average of the light: it is systematically darker, and the error
// grows with the contrast inside the box. Measured on the shipped stage
// previews (.loop/c4-dim/REPORT.md hop 2): -5%..-16% of the source's linear
// light, and -42% on battlefield's p95 — its thin bright platform rims are
// exactly the high-contrast case. So the resampler averages LINEAR light.
//
// LIN[c] = round(4095 * EOTF(c/255)) for the sRGB EOTF (IEC 61966-2-1):
//   s <= 0.04045 : s / 12.92          (the linear toe)
//   else         : ((s+0.055)/1.055)^2.4
// Both branches are solved EXACTLY in BigInt rather than with Math.pow:
// with s = c/255, (s+0.055)/1.055 = (1000c+14025)/269025 =: N/D, and the
// exponent 2.4 = 12/5, so for x = 4095*(N/D)^(12/5) the rounded value L is
// the unique integer with (2L-1)^5 * D^12 <= 32 * 4095^5 * N^12 < (2L+1)^5 *
// D^12 — a comparison of exact integers. Determinism is therefore a property
// of the arithmetic, not of a libm version.
const LIN_MAX = 4095;

// Round-half-up of num/den for non-negative integers, EXACT for every safe
// integer pair. The obvious `floor((2*num + den) / (2*den))` is not: it
// doubles the operands first, so a num inside the accepted domain but above
// 2^52 rounds during the doubling (review-c4-4 [M] gave the counterexample
// num=9003899914072156, den=2199560257499 — that form returns 4094, the true
// value is 4093). Quotient-and-remainder never leaves the safe range: `%` is
// exact, `num - r` is exact, and `r >= den - r` compares two values that are
// both <= den, so it tests 2r >= den without forming 2r.
// The domain is GUARDED like every other entry point in this module
// (review-c4-6o [L]): JS `%` truncates toward zero, so a negative num would
// round the wrong way silently instead of failing.
const divRound = (num, den) => {
  if (!Number.isSafeInteger(num) || num < 0 || !Number.isSafeInteger(den) || den <= 0) {
    throw new Error(`divRound(${num}, ${den}): out of domain (num >= 0, den > 0, ` +
      `safe integers)`);
  }
  const r = num % den;
  return (num - r) / den + (r >= den - r ? 1 : 0);
};
const divRoundBig = (num, den) => (2n * num + den) / (2n * den);

function buildSrgbToLinear() {
  const D = 269025n, K = 32n * BigInt(LIN_MAX) ** 5n;
  const t = new Uint16Array(256);
  for (let c = 0; c < 256; c++) {
    if (c <= 10) { // 10/255 = 0.0392.. <= 0.04045 < 11/255; the toe
      t[c] = Number(divRoundBig(BigInt(LIN_MAX) * BigInt(c) * 1000n, 3294600n));
      continue;
    }
    const rhs = K * BigInt(1000 * c + 14025) ** 12n;
    let lo = 0, hi = LIN_MAX; // smallest L with 32*4095^5*N^12 < (2L+1)^5*D^12
    while (lo < hi) {
      const mid = (lo + hi) >> 1;
      if (rhs < BigInt(2 * mid + 1) ** 5n * D ** 12n) hi = mid; else lo = mid + 1;
    }
    t[c] = lo;
  }
  return t;
}

// Inverse: the 8-bit code whose linear value is NEAREST (ties -> the brighter
// code, matching divRound's half-up). Derived from the forward table itself,
// so LIN_TO_SRGB[SRGB_TO_LIN[c]] === c holds by construction for every c —
// which is what keeps an unscaled image (the 58 px portraits: one source
// pixel per destination pixel) bit-identical through the round trip.
function buildLinearToSrgb(lin) {
  const t = new Uint8Array(LIN_MAX + 1);
  let c = 0;
  for (let L = 0; L <= LIN_MAX; L++) {
    // advance while the next code is at least as close (>= keeps ties bright)
    while (c + 1 < 256 && (lin[c + 1] - L) * 2 <= lin[c + 1] - lin[c]) c++;
    t[L] = c;
  }
  return t;
}

const SRGB_TO_LIN = buildSrgbToLinear();
const LIN_TO_SRGB = buildLinearToSrgb(SRGB_TO_LIN);
for (let c = 1; c < 256; c++) {
  if (SRGB_TO_LIN[c] <= SRGB_TO_LIN[c - 1]) {
    throw new Error(`IMG1: sRGB->linear table is not strictly increasing at ${c}`);
  }
}
for (let c = 0; c < 256; c++) {
  if (LIN_TO_SRGB[SRGB_TO_LIN[c]] !== c) {
    throw new Error(`IMG1: sRGB<->linear round trip broken at code ${c}`);
  }
}

// --- resample -------------------------------------------------------------
// Exact-coverage box filter ("area average"), integer weights.
//
// Destination pixel i along an axis covers source interval
// [i*src/dst, (i+1)*src/dst). Multiplying through by dst makes every
// interval endpoint an integer, so each source pixel j contributes the
// integer overlap  max(0, min((i+1)*src, (j+1)*dst) - max(i*src, j*dst))
// and the per-axis weights sum to exactly `src`. Weights are therefore
// exact — no floats, no accumulated rounding, identical on every host.
//
// Colour correctness: RGB is averaged in LINEAR LIGHT (SRGB_TO_LIN above),
// PREMULTIPLIED, and then divided back out by the summed alpha before the
// inverse transfer. Averaging the 8-bit sRGB codes directly averages a
// gamma-encoded signal and loses light on every mixed cell (measured:
// -5%..-16% of source light on the stage previews, -42% on bf's p95).
// Averaging straight (non-premultiplied) RGB would drag the colour of
// fully transparent pixels into visible edges — and it is not hypothetical
// here: the five upstream portraits store WHITE (255,255,255) under their
// transparent pixels (measured), so straight averaging paints a white halo
// around every character.
function axisWeights(srcN, dstN) {
  // Every interval endpoint below is a product of an index with srcN or dstN,
  // so srcN*dstN bounds them all: requiring it to be a SAFE integer is what
  // makes "the weights are exact" true rather than merely usual
  // (review-c4-4 [M]). Guarded here, in the shared function, so the exported
  // entry point and every caller inherit the same closed domain.
  if (!Number.isSafeInteger(srcN) || !Number.isSafeInteger(dstN) ||
      srcN <= 0 || dstN <= 0 || !Number.isSafeInteger(srcN * dstN)) {
    throw new Error(`axisWeights(${srcN}, ${dstN}): out of domain (positive ` +
      `safe integers whose product is a safe integer)`);
  }
  const rows = [];
  for (let i = 0; i < dstN; i++) {
    const lo = i * srcN, hi = (i + 1) * srcN;
    const j0 = Math.floor(lo / dstN);
    const j1 = Math.min(srcN - 1, Math.floor((hi - 1) / dstN));
    const w = [];
    for (let j = j0; j <= j1; j++) {
      const ov = Math.min(hi, (j + 1) * dstN) - Math.max(lo, j * dstN);
      if (ov > 0) w.push([j, ov]);
    }
    rows.push(w);
  }
  return rows;
}

function resizeRgba(src, dw, dh) {
  const { w: sw, h: sh, rgba } = src;
  // NO identity fast path, deliberately. `dw===sw && dh===sh` through the
  // general path is exact (each destination cell covers exactly one source
  // pixel at full weight), and it is not the same as copying: copying
  // preserves whatever RGB hides under fully transparent pixels, while the
  // general path emits (0,0,0,0) there because a zero alpha sum contributes
  // no colour. Keeping the shortcut meant portraits carried hidden WHITE
  // under their transparent pixels while every other image carried black —
  // an inconsistency found by the independent oracle differential in
  // lib/assets-selftest.js. One path, one behaviour.
  // DOMAIN (review-c4-2 [M]): the accepted set is stated, not assumed. A
  // fractional dw silently produced a garbage buffer, and the accumulators
  // below are exact only while they stay safe integers — the largest one is
  // sr <= LIN_MAX * 255 * sw * sh (per-axis weights sum to sw and sh), so
  // that product IS the bound, and it is asserted rather than argued.
  for (const [n, v] of [["sw", sw], ["sh", sh], ["dw", dw], ["dh", dh]]) {
    if (!Number.isSafeInteger(v) || v <= 0) {
      throw new Error(`resize ${sw}x${sh} -> ${dw}x${dh}: ${n}=${v} is out of ` +
        `domain (positive safe integers only)`);
    }
  }
  if (rgba.length !== sw * sh * 4) {
    throw new Error(`resize ${sw}x${sh}: ${rgba.length} bytes != ${sw}*${sh}*4`);
  }
  if (LIN_MAX * 255 * sw * sh > Number.MAX_SAFE_INTEGER) {
    throw new Error(`resize ${sw}x${sh}: source is out of domain (accumulator ` +
      `${LIN_MAX} * 255 * ${sw} * ${sh} would exceed exact integer range)`);
  }
  if (dw > sw || dh > sh) {
    throw new Error(`resize ${sw}x${sh} -> ${dw}x${dh}: upscaling is out of ` +
      `domain (box filter is an area average; upscale would alias)`);
  }
  const wx = axisWeights(sw, dw), wy = axisWeights(sh, dh);
  const total = sw * sh; // == sum over all (wx*wy)
  const out = Buffer.alloc(dw * dh * 4);
  for (let y = 0; y < dh; y++) {
    for (let x = 0; x < dw; x++) {
      let sr = 0, sg = 0, sb = 0, sa = 0;
      for (const [sy, ry] of wy[y]) {
        const row = sy * sw * 4;
        for (const [sx, rx] of wx[x]) {
          const p = row + sx * 4, wgt = rx * ry, a = rgba[p + 3], wa = wgt * a;
          sr += SRGB_TO_LIN[rgba[p]] * wa; sg += SRGB_TO_LIN[rgba[p + 1]] * wa;
          sb += SRGB_TO_LIN[rgba[p + 2]] * wa;
          sa += wa;
        }
      }
      const o = (y * dw + x) * 4;
      const oa = divRound(sa, total);
      out[o + 3] = oa;
      if (sa > 0) {
        out[o] = LIN_TO_SRGB[divRound(sr, sa)];
        out[o + 1] = LIN_TO_SRGB[divRound(sg, sa)];
        out[o + 2] = LIN_TO_SRGB[divRound(sb, sa)];
      }
    }
  }
  return out;
}

// --- per-class tone map (a DEVIATION hook, not part of the format) --------
// gammaTable([p, q]) is out(v) = round(255 * (v/255)^(p/q)), as an integer
// table. Same discipline as the transfer tables: exact-rational BigInt at
// build time, never Math.pow — with x = 255*(v/255)^(p/q) we have
// x^q = 255^(q-p) * v^p, so writing M(v) = 2^q * 255^(q-p) * v^p the rounded
// value L is the unique integer with
//   (2L-1)^q  <=  M(v)  <  (2L+1)^q     for L >= 1,
//   M(v) < 1                            for L == 0
// (the L=0 arm is stated separately because for EVEN q the first form's
// (2L-1)^q = 1 would be a false lower bound at L=0 — review-c4-3 [L]).
// Callers name the rational (see stages/assets.js STAGE_PREVIEW_GAMMA); a
// value < 1 LIFTS, which is a deliberate departure from upstream's pixels
// and is documented as such in FORMATS.md §7.2.
function gammaTable([p, q]) {
  // Safe integers, not merely integers, and a bounded exponent: the exactness
  // claim is about a SMALL rational (0.75 = 3/4, 0.65 = 13/20). Without the
  // cap the domain would extend to exponents where BigInt `**` hits an
  // implementation limit — a limit is not a specification (review-c4-4 [L]).
  const Q_MAX = 64;
  if (!(Number.isSafeInteger(p) && Number.isSafeInteger(q) &&
        p > 0 && q >= p && q <= Q_MAX)) {
    throw new Error(`IMG1: gamma ${p}/${q} is out of domain ` +
      `(0 < p <= q <= ${Q_MAX}, safe integers)`);
  }
  const t = new Uint8Array(256);
  let L = 0;
  for (let v = 0; v <= 255; v++) {
    const lhs = 2n ** BigInt(q) * 255n ** BigInt(q - p) * BigInt(v) ** BigInt(p);
    while (L < 255 && BigInt(2 * L + 1) ** BigInt(q) <= lhs) L++; // monotone in v
    t[v] = L;
  }
  if (t[0] !== 0 || t[255] !== 255) {
    throw new Error(`IMG1: gamma ${p}/${q} does not fix the endpoints`);
  }
  return t;
}

// Apply a tone table to RGB, leaving ALPHA untouched (a tone map must not
// move a pixel between alpha classes — those are pinned in expected-assets).
function mapRgb(rgba, lut) {
  const out = Buffer.from(rgba);
  for (let i = 0; i < out.length; i += 4) {
    out[i] = lut[out[i]]; out[i + 1] = lut[out[i + 1]]; out[i + 2] = lut[out[i + 2]];
  }
  return out;
}

// --- encode ---------------------------------------------------------------
// raster.c pack565()'s TRUNCATION — what the RENDERER does to a vector fill.
// Kept verbatim (and still the reader's contract: it is the exact inverse of
// img1_blit's bit replication), but it is NOT what this encoder quantizes
// with; see quant565 below.
const pack565 = (r, g, b) => (((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)) & 0xffff;

// Nearest representable 565, not truncation. img1_blit expands a code k back
// to 8 bits by BIT REPLICATION, so the values a stored pixel can actually
// take are rep(k) = (k<<3)|(k>>2) / (k<<2)|(k>>4) — the encoder must pick the
// k whose rep(k) is CLOSEST to the source byte, ties to the brighter code.
// Truncation instead always rounds DOWN, ~-3.5/255 per 5-bit channel: a
// constant that is invisible on mid-tones and enormous on the near-black
// stage art (bf's mean Y is 9.18/255, so truncation alone ate 20% of it —
// .loop/c4-dim/REPORT.md hop 3). Because rep(k) round-trips through pack565
// exactly (asserted below), the stored code is still one a vector fill can
// reach; what changes is only WHICH code a given RGB888 lands on.
function nearestTable(bits) {
  const n = 1 << bits, sh = 8 - bits;
  const rep = (k) => (k << sh) | (k >> (bits - sh));
  const t = new Uint8Array(256);
  for (let v = 0; v < 256; v++) {
    let best = 0, bestErr = 1 << 30;
    for (let k = 0; k < n; k++) {
      const e = Math.abs(rep(k) - v);
      if (e <= bestErr) { best = k; bestErr = e; } // <= : ties -> brighter
    }
    t[v] = best;
  }
  return t;
}
const Q5 = nearestTable(5), Q6 = nearestTable(6);
const quant565 = (r, g, b) => ((Q5[r] << 11) | (Q6[g] << 5) | Q5[b]) & 0xffff;

// The stored code must be exactly what pack565 would produce for the value
// img1_blit unpacks it to — i.e. quantization moves the pixel, it never
// creates a code outside the renderer's own 565 lattice.
for (let v = 0; v < 256; v++) {
  const r8 = (Q5[v] << 3) | (Q5[v] >> 2), g8 = (Q6[v] << 2) | (Q6[v] >> 4);
  if (pack565(r8, g8, r8) !== quant565(v, v, v)) {
    throw new Error(`IMG1: nearest-565 code for ${v} is not a pack565 fixed point`);
  }
}

const pad4 = (n) => (n + 3) & ~3;

// images: [{ name, w, h, rgba }] in the pinned directory order.
function encodeImg1(images) {
  const seen = new Set();
  for (const im of images) {
    if (!/^[a-z0-9_]+$/.test(im.name) || im.name.length > NAME_MAX) {
      throw new Error(`IMG1: bad image name ${JSON.stringify(im.name)}`);
    }
    if (seen.has(im.name)) throw new Error(`IMG1: duplicate image name ${im.name}`);
    seen.add(im.name);
    if (!(im.w > 0 && im.h > 0 && im.w <= 0xffff && im.h <= 0xffff)) {
      throw new Error(`IMG1: ${im.name}: bad size ${im.w}x${im.h}`);
    }
    if (im.rgba.length !== im.w * im.h * 4) {
      throw new Error(`IMG1: ${im.name}: ${im.rgba.length} bytes != ${im.w}*${im.h}*4`);
    }
  }
  let off = pad4(HDR + images.length * DIR_ENTRY);
  const dirOffsets = [];
  for (const im of images) {
    dirOffsets.push(off);
    off = pad4(off + im.w * im.h * 3);
  }
  const buf = Buffer.alloc(off);
  buf.write("IMG1", 0, "latin1");
  buf.writeUInt32LE(images.length, 4);
  buf.writeUInt32LE(off, 8);
  images.forEach((im, i) => {
    const d = HDR + i * DIR_ENTRY;
    buf.write(im.name, d, NAME_MAX + 1, "latin1"); // rest already zero
    buf.writeUInt16LE(im.w, d + 16);
    buf.writeUInt16LE(im.h, d + 18);
    buf.writeUInt32LE(dirOffsets[i], d + 20);
    const px = dirOffsets[i], ap = px + im.w * im.h * 2;
    for (let k = 0; k < im.w * im.h; k++) {
      buf.writeUInt16LE(quant565(im.rgba[k * 4], im.rgba[k * 4 + 1], im.rgba[k * 4 + 2]),
        px + k * 2);
      buf[ap + k] = im.rgba[k * 4 + 3];
    }
  });
  return buf;
}

// --- decode + canonical dump (the C loader's differential partner) --------
function decodeImg1(buf, what) {
  const bad = (m) => { throw new Error(`${what}: ${m}`); };
  if (buf.length < HDR || buf.toString("latin1", 0, 4) !== "IMG1") bad("bad magic");
  const count = buf.readUInt32LE(4), total = buf.readUInt32LE(8);
  if (total !== buf.length) bad(`header bytes ${total} != file ${buf.length}`);
  if (HDR + count * DIR_ENTRY > total) bad("directory overruns file");
  const out = [];
  for (let i = 0; i < count; i++) {
    const d = HDR + i * DIR_ENTRY;
    const nb = buf.slice(d, d + 16);
    const z = nb.indexOf(0);
    if (z < 0) bad(`entry ${i}: name not NUL-terminated`);
    const name = nb.toString("latin1", 0, z);
    const w = buf.readUInt16LE(d + 16), h = buf.readUInt16LE(d + 18);
    const off = buf.readUInt32LE(d + 20);
    if (off % 4 !== 0) bad(`entry ${i}: dataOff ${off} not 4-aligned`);
    if (off + w * h * 3 > total) bad(`entry ${i}: data overruns file`);
    out.push({ name, w, h, off,
      rgb565: buf.slice(off, off + w * h * 2),
      a8: buf.slice(off + w * h * 2, off + w * h * 3) });
  }
  return out;
}

// One line per image row; byte-identical text is produced by
// port/gfx/img1_check.c (the round-trip contract, FORMATS.md §7.4).
function dumpImg1(buf, what) {
  const imgs = decodeImg1(buf, what);
  const lines = [`IMG1 count=${imgs.length}`];
  imgs.forEach((im, i) => {
    lines.push(`img ${i} ${im.name} ${im.w} ${im.h}`);
    for (let y = 0; y < im.h; y++) {
      let p = "", a = "";
      for (let x = 0; x < im.w; x++) {
        const k = y * im.w + x;
        p += im.rgb565.readUInt16LE(k * 2).toString(16).padStart(4, "0");
        a += im.a8[k].toString(16).padStart(2, "0");
      }
      lines.push(`row ${i} ${y} ${p} ${a}`);
    }
  });
  return lines.join("\n") + "\n";
}

module.exports = { resizeRgba, encodeImg1, decodeImg1, dumpImg1, pack565, quant565,
  axisWeights, divRound, gammaTable, mapRgb, SRGB_TO_LIN, LIN_TO_SRGB, LIN_MAX };
