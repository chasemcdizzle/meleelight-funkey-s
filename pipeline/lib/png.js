"use strict";
// Minimal PNG decoder (fix_plan §M4 task A9; format IMG1, FORMATS.md §7).
//
// WHY OURS AND NOT ffmpeg: the audio stage needs ffmpeg because resampling
// and vorbis decoding are properties of the ffmpeg BUILD (FORMATS.md §5.2)
// — hence its three-layer version pin. PNG decoding is LOSSLESS and fully
// specified (zlib inflate + a 5-filter reconstruction), and node ships zlib
// in stdlib, so decoding here costs ~80 lines and buys back a whole class
// of tool-version pins: the assets stage has NO external tool dependency
// and is byte-deterministic by construction. check-assets.sh still runs a
// DIFFERENTIAL against ffmpeg's decoder on every source PNG, so "our
// decoder is right" is measured, not asserted.
//
// DOMAIN (measured over all 15 upstream menu PNGs, iter A9): bit depth 8,
// colour type 2 (RGB) or 6 (RGBA), no interlace, no tRNS/palette. Anything
// else HARD-THROWS — an unsupported PNG must fail loudly, never decode to
// plausible-looking wrong pixels.

const zlib = require("zlib");

const SIG = Buffer.from([0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a]);

function paeth(a, b, c) {
  const p = a + b - c;
  const pa = Math.abs(p - a), pb = Math.abs(p - b), pc = Math.abs(p - c);
  if (pa <= pb && pa <= pc) return a;
  if (pb <= pc) return b;
  return c;
}

// Decode a PNG file into straight (non-premultiplied) RGBA8888.
// Returns { w, h, rgba: Buffer(w*h*4), colorType, srcHasAlpha }.
function decodePng(buf, what) {
  const bad = (msg) => { throw new Error(`${what}: ${msg}`); };
  if (buf.length < 8 || !buf.slice(0, 8).equals(SIG)) bad("not a PNG (bad signature)");

  let w = 0, h = 0, bitDepth = 0, colorType = 0, interlace = 0, sawIhdr = false;
  const idat = [];
  let off = 8;
  while (off + 8 <= buf.length) {
    const len = buf.readUInt32BE(off);
    const type = buf.toString("latin1", off + 4, off + 8);
    const data = buf.slice(off + 8, off + 8 + len);
    if (off + 12 + len > buf.length) bad(`truncated chunk ${type}`);
    if (type === "IHDR") {
      if (sawIhdr) bad("duplicate IHDR");
      sawIhdr = true;
      w = data.readUInt32BE(0); h = data.readUInt32BE(4);
      bitDepth = data[8]; colorType = data[9];
      if (data[10] !== 0) bad(`compression method ${data[10]} != 0`);
      if (data[11] !== 0) bad(`filter method ${data[11]} != 0`);
      interlace = data[12];
    } else if (type === "IDAT") {
      idat.push(data);
    } else if (type === "PLTE" || type === "tRNS") {
      // Out of the measured domain: both change pixel meaning, and silently
      // ignoring either yields wrong-but-plausible pixels.
      bad(`unsupported chunk ${type} (palette/transparency out of domain)`);
    } else if (type === "IEND") {
      break;
    }
    off += 12 + len;
  }
  if (!sawIhdr) bad("no IHDR");
  if (bitDepth !== 8) bad(`bit depth ${bitDepth} != 8 (out of domain)`);
  if (colorType !== 2 && colorType !== 6) bad(`colour type ${colorType} not in {2,6}`);
  if (interlace !== 0) bad("interlaced (Adam7) PNG is out of domain");
  if (!w || !h) bad(`degenerate size ${w}x${h}`);
  if (idat.length === 0) bad("no IDAT");

  const chans = colorType === 6 ? 4 : 3;
  const raw = zlib.inflateSync(Buffer.concat(idat));
  const stride = w * chans;
  if (raw.length !== (stride + 1) * h) {
    bad(`inflated ${raw.length} bytes != (${stride}+1)*${h}`);
  }

  // Filter reconstruction (PNG spec §9.2), in place into `lines`.
  const lines = Buffer.alloc(stride * h);
  for (let y = 0; y < h; y++) {
    const ft = raw[y * (stride + 1)];
    const src = raw.slice(y * (stride + 1) + 1, (y + 1) * (stride + 1));
    const cur = lines.slice(y * stride, (y + 1) * stride);
    const prev = y > 0 ? lines.slice((y - 1) * stride, y * stride) : null;
    for (let i = 0; i < stride; i++) {
      const a = i >= chans ? cur[i - chans] : 0;
      const b = prev ? prev[i] : 0;
      const c = prev && i >= chans ? prev[i - chans] : 0;
      let v;
      switch (ft) {
        case 0: v = src[i]; break;
        case 1: v = src[i] + a; break;
        case 2: v = src[i] + b; break;
        case 3: v = src[i] + ((a + b) >> 1); break;
        case 4: v = src[i] + paeth(a, b, c); break;
        default: bad(`row ${y}: unknown filter type ${ft}`);
      }
      cur[i] = v & 0xff;
    }
  }

  if (chans === 4) return { w, h, rgba: lines, colorType, srcHasAlpha: true };
  const rgba = Buffer.alloc(w * h * 4);
  for (let i = 0, j = 0; i < w * h; i++, j += 3) {
    rgba[i * 4] = lines[j]; rgba[i * 4 + 1] = lines[j + 1];
    rgba[i * 4 + 2] = lines[j + 2]; rgba[i * 4 + 3] = 255;
  }
  return { w, h, rgba, colorType, srcHasAlpha: false };
}

module.exports = { decodePng };
