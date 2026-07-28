"use strict";
// IMG1 — pre-scaled RGB565 + A8 menu images (fix_plan §M4 task A9;
// spec: pipeline/FORMATS.md §7). Writer, reader and canonical dump.
//
// Everything here is INTEGER arithmetic end to end (decode -> premultiply
// -> exact-coverage box resample -> unpremultiply -> 565 truncation), so
// byte-stability across runs is a property of the code, not of a pinned
// external tool. See FORMATS.md §7.2.

const NAME_MAX = 15; // 16-byte field, NUL-terminated
const DIR_ENTRY = 24;
const HDR = 12;

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
// Alpha correctness: RGB is averaged PREMULTIPLIED and then divided back
// out by the summed alpha. Averaging straight RGB would drag the colour of
// fully transparent pixels into visible edges — and it is not hypothetical
// here: the five upstream portraits store WHITE (255,255,255) under their
// transparent pixels (measured), so straight averaging paints a white halo
// around every character.
function axisWeights(srcN, dstN) {
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

// round-half-up of num/den for non-negative integers
const divRound = (num, den) => Math.floor((2 * num + den) / (2 * den));

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
          sr += rgba[p] * wa; sg += rgba[p + 1] * wa; sb += rgba[p + 2] * wa;
          sa += wa;
        }
      }
      const o = (y * dw + x) * 4;
      const oa = divRound(sa, total);
      out[o + 3] = oa;
      if (sa > 0) {
        out[o] = divRound(sr, sa);
        out[o + 1] = divRound(sg, sa);
        out[o + 2] = divRound(sb, sa);
      }
    }
  }
  return out;
}

// --- encode ---------------------------------------------------------------
// 565 quantization is raster.c pack565()'s TRUNCATION, byte-for-byte:
// a pixel emitted here is exactly what the renderer would have produced for
// the same RGB888, so images and vector fills never disagree by one step.
const pack565 = (r, g, b) => (((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)) & 0xffff;

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
      buf.writeUInt16LE(pack565(im.rgba[k * 4], im.rgba[k * 4 + 1], im.rgba[k * 4 + 2]),
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

module.exports = { resizeRgba, encodeImg1, decodeImg1, dumpImg1, pack565, axisWeights };
