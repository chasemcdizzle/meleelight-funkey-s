#!/usr/bin/env node
// decode-pb-glyphs.js — review-102 L-b: connect the independently-derived
// PERSONAL BEST display string to the SHOT PIXELS. This is a small
// check-owned glyph decoder that reads the FOH 5x7 font tables from
// port/foh/foh_font.c AS DATA (parses the kGlyphs table) and decodes a
// region of a rendered PPM shot back into a string — NOT via the C
// renderer (foh_render.c). The check asserts the decoded string equals
// the string derived independently from the record bits (derive_pb), so
// the display pin is bound to actual pixels, not a renderer-vs-renderer
// echo.
//
// The layout it decodes is the FOH text_center spec (foh_render.c /
// foh_font.c): scale-1 glyphs, 6 px advance (5 px glyph + 1 px gap), 7
// rows, drawn in kAccent (255,200,60) fully opaque over kBg (12,12,28);
// text_center places the line at x = floor((RAST_W - width)/2),
// width = n*6 - 1, RAST_W = 240. On-pixels are solid accent (no AA), so a
// simple luminance threshold separates on from off exactly.
//
// Usage: decode-pb-glyphs.js <foh_font.c> <shot.ppm> <y> <scale> <nglyphs>
// Prints the decoded string on stdout (exactly <nglyphs> characters). A
// glyph whose sampled 7-row bitmap matches NO font entry is a loud
// failure (exit 3) — a silent '?' would defeat the purpose.

'use strict';
const fs = require('fs');

function die(msg, code) { process.stderr.write('decode-pb-glyphs: ' + msg + '\n'); process.exit(code || 1); }

const [, , fontPath, ppmPath, yArg, scaleArg, nArg] = process.argv;
if (!fontPath || !ppmPath || yArg === undefined || scaleArg === undefined || nArg === undefined) {
  die('usage: decode-pb-glyphs.js <foh_font.c> <shot.ppm> <y> <scale> <nglyphs>', 2);
}
const y = Number(yArg), scale = Number(scaleArg), n = Number(nArg);
if (!Number.isInteger(y) || !Number.isInteger(scale) || !Number.isInteger(n) ||
    scale < 1 || n < 1 || y < 0) {
  die('bad numeric args (y=' + yArg + ' scale=' + scaleArg + ' nglyphs=' + nArg + ')', 2);
}

// --- parse the font table from foh_font.c AS DATA ---------------------------
// review-104 M-4: EXACT full-initializer reconciliation, not a `>= 30`
// floor. Isolate the `static const FohGlyph kGlyphs[] = { … };` initializer
// BODY, count its DECLARED entries structurally, parse every entry, and
// require declared == parsed with NO unparsed residue. A silently
// dropped/added/malformed entry (the permissive-parse hole) is now a hard
// failure, and the glyph count is bound to the measured table size exactly.
const fontSrc = fs.readFileSync(fontPath, 'utf8');
const arrM = fontSrc.match(/\bkGlyphs\s*\[\]\s*=\s*\{([\s\S]*?)\n\};/);
if (!arrM) die('could not locate the kGlyphs[] initializer in ' + fontPath, 3);
const body = arrM[1];
// Each entry: {'X', {0x.., ×7}}. The char group captures either an escaped
// char (\' or \\) or one non-quote char.
const ENTRY_SRC = "\\{'(\\\\.|[^'\\\\])',\\s*\\{\\s*([^}]*?)\\s*\\}\\s*\\}";
const declared = (body.match(/\{'/g) || []).length; // every top-level entry opener
const glyphs = []; // {ch, rows:[7]}
let m;
const re = new RegExp(ENTRY_SRC, 'g');
while ((m = re.exec(body)) !== null) {
  let ch = m[1];
  if (ch === "\\'") ch = "'";
  else if (ch === '\\\\') ch = '\\';
  else if (ch.length === 2 && ch[0] === '\\') ch = ch[1]; // any other escape -> literal
  const rows = m[2].split(',').map((s) => s.trim()).filter((s) => s.length);
  if (rows.length !== 7) die('font glyph for ' + JSON.stringify(ch) + ' has ' + rows.length + ' rows (want 7)', 3);
  const bytes = rows.map((h) => {
    if (!/^0x[0-9a-fA-F]{1,2}$/.test(h)) die('font glyph ' + JSON.stringify(ch) + ' bad row token ' + JSON.stringify(h), 3);
    return parseInt(h, 16) & 0x1f; // 5 significant bits per row
  });
  glyphs.push({ ch, rows: bytes });
}
if (glyphs.length !== declared) {
  die('font initializer reconciliation: parsed ' + glyphs.length + ' glyphs != ' +
      declared + ' declared kGlyphs entries (a dropped/malformed entry)', 3);
}
if (glyphs.length < 1) die('parsed no font glyphs from ' + fontPath, 3);
// trailing bytes = death (analog): after removing every matched entry the
// initializer body must be ONLY whitespace, commas, and line comments.
let residue = body.replace(new RegExp(ENTRY_SRC, 'g'), '');
residue = residue.replace(/\/\/[^\n]*/g, '').replace(/[\s,]/g, '');
if (residue.length !== 0) {
  die('font initializer has unparsed content (not entry/comma/comment): ' +
      JSON.stringify(residue.slice(0, 40)), 3);
}

// signature (7 bytes joined) -> char; collisions are a hard error
const sig2ch = new Map();
for (const g of glyphs) {
  const key = g.rows.join(',');
  if (sig2ch.has(key) && sig2ch.get(key) !== g.ch) {
    die('font signature collision: ' + JSON.stringify(sig2ch.get(key)) + ' and ' + JSON.stringify(g.ch), 3);
  }
  sig2ch.set(key, g.ch);
}

// --- parse the PPM (P6 binary) ----------------------------------------------
const raw = fs.readFileSync(ppmPath);
function readPPM(buf) {
  if (buf.slice(0, 2).toString('latin1') !== 'P6') die('shot is not a P6 PPM', 3);
  let i = 2;
  const tok = () => {
    // skip whitespace and #comments
    while (i < buf.length) {
      const c = buf[i];
      if (c === 0x23) { while (i < buf.length && buf[i] !== 0x0a) i++; }
      else if (c === 0x20 || c === 0x09 || c === 0x0a || c === 0x0d) i++;
      else break;
    }
    let s = '';
    while (i < buf.length) {
      const c = buf[i];
      if (c === 0x20 || c === 0x09 || c === 0x0a || c === 0x0d) break;
      s += String.fromCharCode(c); i++;
    }
    return s;
  };
  // review-104 M-4: canonical full-token integers (reject '240junk',
  // leading zeros, signs, empty) — a permissive parseInt prefix is a hole.
  const intTok = (label) => {
    const t = tok();
    if (!/^(0|[1-9][0-9]*)$/.test(t)) die('PPM ' + label + ' not a canonical integer token: ' + JSON.stringify(t), 3);
    return parseInt(t, 10);
  };
  const w = intTok('width'), h = intTok('height'), mx = intTok('maxval');
  if (!(w > 0) || !(h > 0)) die('bad PPM dimensions ' + w + 'x' + h, 3);
  if (mx !== 255) die('PPM maxval ' + mx + ' != 255 (unsupported)', 3);
  i++; // exactly one whitespace byte after maxval, then pixel data
  const need = w * h * 3;
  const avail = buf.length - i;
  // review-104 M-4: EXACT byte count — trailing bytes are corruption death,
  // not tolerated slack.
  if (avail !== need) die('PPM pixel data byte count ' + avail + ' != w*h*3 (' + need + ') — short/trailing bytes', 3);
  return { w, h, data: buf.slice(i, i + need) };
}
const img = readPPM(raw);
const RAST_W = 240;
const px = (x, yy) => {
  if (x < 0 || yy < 0 || x >= img.w || yy >= img.h) return [0, 0, 0];
  const o = (yy * img.w + x) * 3;
  return [img.data[o], img.data[o + 1], img.data[o + 2]];
};
// on = solid accent (high red, high green, low blue) vs kBg (12,12,28).
// A generous luminance/red threshold separates the two exactly (no AA).
const isOn = (x, yy) => { const p = px(x, yy); return p[0] >= 128 && p[1] >= 96 && p[2] < 128; };

// --- decode the centered line of <n> glyphs ---------------------------------
const width = n * 6 - 1;                 // foh_text_width, scale 1 units
const xStart = Math.floor((RAST_W - width * scale) / 2); // text_center
let out = '';
for (let gi = 0; gi < n; gi++) {
  const gx = xStart + gi * 6 * scale;
  const rows = [];
  for (let r = 0; r < 7; r++) {
    let b = 0;
    for (let c = 0; c < 5; c++) {
      // sample the center of the scale x scale cell
      const sx = gx + c * scale + Math.floor(scale / 2);
      const sy = y + r * scale + Math.floor(scale / 2);
      if (isOn(sx, sy)) b |= (0x10 >> c);
    }
    rows.push(b);
  }
  const key = rows.join(',');
  const ch = sig2ch.get(key);
  if (ch === undefined) {
    die('glyph ' + gi + ' at x=' + gx + ' y=' + y + ' decoded to bitmap [' +
        rows.map((v) => '0x' + v.toString(16).padStart(2, '0')).join(',') +
        '] which matches NO font entry', 3);
  }
  out += ch;
}
process.stdout.write(out);
