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
// The layout it decodes is the FOH text-placement spec (foh_render.c /
// foh_font.c): 7 glyph rows, 5 px wide, 6*scale px advance (5 px glyph +
// 1 px gap at scale 1), composited FULLY OPAQUE — foh_text passes
// col.a256 == 256, and rast_blend_px_run stores the packed colour
// outright on that path, so there is no AA and no blend. Placement is ONE
// rule, foh_render.c's text_in:
//   x = win_x + (win_w - foh_text_width(s, scale)) / 2   [C trunc div]
//   foh_text_width(s, scale) = (n*6 - 1) * scale
// text_center IS that rule over the whole raster (win_x = 0, win_w =
// RAST_W = 240), which is why the default window below reproduces the
// original text_center decode exactly.
//
// COLOUR, and why it is now a parameter (R5, 2026-07-31): the
// target-select PERSONAL BEST row stopped being one kAccent text_center
// line and became a PANEL carrying TWO lines in TWO colours — the kDim
// label at scale 1 and the kAccent time at scale 2 — and the old fixed
// accent threshold (r>=128) could not see kDim at all. The on-test is now
// an EXACT match against the requested SOURCE colour after the RGB565
// quantization the pipeline performs: raster.c's pack565 keeps r>>3 /
// g>>2 / b>>3, and foh_dev.c's write_shot_ppm expands back by plain
// left-shift, so a source (r,g,b) lands in the shot as
// (r & 0xF8, g & 0xFC, b & 0xF8). Exact equality is strictly SHARPER than
// the threshold it replaces: an opaque glyph pixel can only ever BE that
// value, while a threshold also admits neighbouring UI ink.
//
// FOREIGN INK, and why it is opt-in (review-r6-r6/r7 [HIGH]). Exact colour
// equality answers "is this pixel this line's ink"; it does NOT notice ink of
// some OTHER colour landing in a cell that should be background. The decoder
// this replaced noticed one shape of that by accident: its accent threshold
// (r>=128 && g>=96 && b<128) read any bright warm pixel as ON, so foreign ink
// broke the bitmap and killed the decode. That was only ever sound because
// the old PB row sat on a FLAT DARK PANEL. It cannot be kept unconditionally:
// the VS-finish banner is drawn OVER A LIVE GAME FRAME, and pixel (74,116) of
// a real finish-banner shot is (200,100,112) — legitimate gameplay art that
// satisfies the predicate inside an OFF cell. So the guard is a per-call
// declaration about the BACKGROUND, spelled `flat-panel`: on a flat panel any
// bright warm pixel that is not this line's ink is corruption and dies; over
// gameplay there is no such statement to make and the flag is omitted.
//
// Usage: decode-pb-glyphs.js <foh_font.c> <shot.ppm> <y> <scale> <nglyphs>
//                            [<win_x>,<win_w>] [<r>,<g>,<b>] [flat-panel]
// Defaults: window 0,240 (i.e. text_center) and colour 255,200,60
// (kAccent) — omitting both decodes exactly what this tool decoded before.
// Prints the decoded string on stdout (exactly <nglyphs> characters). A
// glyph whose sampled 7-row bitmap matches NO font entry is a loud
// failure (exit 3) — a silent '?' would defeat the purpose.

'use strict';
const fs = require('fs');

function die(msg, code) { process.stderr.write('decode-pb-glyphs: ' + msg + '\n'); process.exit(code || 1); }

const [, , fontPath, ppmPath, yArg, scaleArg, nArg, winArg, colArg, flatArg] = process.argv;
if (!fontPath || !ppmPath || yArg === undefined || scaleArg === undefined || nArg === undefined) {
  die('usage: decode-pb-glyphs.js <foh_font.c> <shot.ppm> <y> <scale> <nglyphs> ' +
      '[<win_x>,<win_w>] [<r>,<g>,<b>] [flat-panel]', 2);
}
// Exact arity: 5 required + at most 3 optional. A positional typo'd into the
// wrong slot would otherwise be silently ignored and the decode would run
// against a window, colour or background claim the caller did not ask for.
if (process.argv.length > 10) {
  die('too many arguments (' + (process.argv.length - 2) + '); 5 required + 3 optional', 2);
}
// The background declaration is a LITERAL, not a truthy string: `--flat`,
// `1` or a stray path must be a usage death, never a silently-disabled guard.
if (flatArg !== undefined && flatArg !== 'flat-panel') {
  die('the 8th argument must be the literal token "flat-panel" (or be omitted), got ' +
      JSON.stringify(flatArg), 2);
}
const flatPanel = flatArg === 'flat-panel';
const y = Number(yArg), scale = Number(scaleArg), n = Number(nArg);
if (!Number.isInteger(y) || !Number.isInteger(scale) || !Number.isInteger(n) ||
    scale < 1 || n < 1 || y < 0) {
  die('bad numeric args (y=' + yArg + ' scale=' + scaleArg + ' nglyphs=' + nArg + ')', 2);
}

// The two optional args get the SAME whitelist discipline as the PPM
// header tokens below (review-104 M-4): canonical decimals, exact field
// count, in-domain. A permissive Number() would take '0x5c', ' 92' or ''
// as a silent 0 and then decode a SHIFTED window without ever failing —
// and a shifted window that lands on background decodes to a loud
// no-matching-glyph death only by luck.
const RAST_W = 240;
const csvInts = (s, k, label) => {
  const parts = s.split(',');
  if (parts.length !== k) {
    die(label + ' wants ' + k + ' comma-separated fields, got ' + JSON.stringify(s), 2);
  }
  return parts.map((t) => {
    if (!/^(0|[1-9][0-9]*)$/.test(t)) {
      die(label + ' field ' + JSON.stringify(t) + ' is not a canonical decimal', 2);
    }
    return parseInt(t, 10);
  });
};
// Window = text_in's [x, x+w). Default = text_center over the raster.
const [winX, winW] = winArg === undefined ? [0, RAST_W] : csvInts(winArg, 2, 'window');
if (winW < 1 || winX + winW > RAST_W) {
  die('window ' + winX + ',' + winW + ' is not inside [0,' + RAST_W + ']', 2);
}
// Colour = the SOURCE RastCol; quantized here exactly as the pipeline does.
const [srcR, srcG, srcB] = colArg === undefined ? [255, 200, 60] : csvInts(colArg, 3, 'colour');
if (srcR > 255 || srcG > 255 || srcB > 255) {
  die('colour ' + srcR + ',' + srcG + ',' + srcB + ' has a component > 255', 2);
}
const onR = srcR & 0xf8, onG = srcG & 0xfc, onB = srcB & 0xf8; // pack565 -> write_shot_ppm

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
const px = (x, yy) => {
  if (x < 0 || yy < 0 || x >= img.w || yy >= img.h) return [0, 0, 0];
  const o = (yy * img.w + x) * 3;
  return [img.data[o], img.data[o + 1], img.data[o + 2]];
};
// on = EXACTLY the requested colour, post-565. Glyph pixels are opaque, so
// an on-pixel is bit-for-bit this value; everything else in the cell is
// whatever the panel fill / background left there. The two classes this
// check uses cannot alias each other (kDim -> 120,120,136 and kAccent ->
// 248,200,56), and neither aliases the panel face (16,4,0).
const isOn = (x, yy) => { const p = px(x, yy); return p[0] === onR && p[1] === onG && p[2] === onB; };
// The `flat-panel` foreign-ink predicate: the decoder this replaced read ANY
// bright warm pixel as ink. On a flat panel that is exactly the corruption
// test worth keeping — kBg (12,12,28), the info panel's face (16,4,0) and the
// kDim label (120,120,136) all fail it, so only genuine bright ink can trip
// it, and this line's own ink is excluded by isOn before the test is asked.
const isForeignInk = (x, yy) => {
  const p = px(x, yy);
  return p[0] >= 128 && p[1] >= 96 && p[2] < 128;
};

// --- decode the centered line of <n> glyphs ---------------------------------
const width = (n * 6 - 1) * scale;       // foh_text_width(s, scale)
// text_in: win_x + (win_w - foh_text_width) / 2 in C truncating division.
// Math.trunc, not floor, so a caller-supplied window narrower than the
// line is decoded where the C would have placed it rather than one pixel
// off (both agree whenever the line fits, which is the committed case).
const xStart = winX + Math.trunc((winW - width) / 2);
// IN-FRAME OR DEATH (review-r6-r1 [LOW]). px() answers [0,0,0] outside the
// image, so any sample that leaves the shot reads as "off" — a decode whose
// rectangle hangs off an edge is PARTLY blind, and the ink guard below
// cannot see that because the visible part still carries ink. The line's
// full 7*scale rows and n*6*scale-1 columns must be inside the image, or
// the decode is refused rather than silently trimmed.
if (xStart < 0 || xStart + width > img.w || y + 7 * scale > img.h) {
  die('the sampled line rectangle x=[' + xStart + ',' + (xStart + width) +
      ') y=[' + y + ',' + (y + 7 * scale) + ') leaves the ' + img.w + 'x' +
      img.h + ' shot — a decode that samples outside the image reads those ' +
      'pixels as OFF and is only partly observing', 3);
}
let out = '';
let ink = 0; // OR of every sampled row across the whole line
for (let gi = 0; gi < n; gi++) {
  const gx = xStart + gi * 6 * scale;
  const rows = [];
  for (let r = 0; r < 7; r++) {
    let b = 0;
    for (let c = 0; c < 5; c++) {
      // EVERY pixel of the scale x scale cell, not its centre
      // (review-r6-r3 [MEDIUM]). The original tool only ever ran at scale 1,
      // where cell == pixel and the sample was exhaustive by construction;
      // the panel layout put a scale-2 line and (in check-device-foh.sh) a
      // scale-4 banner on this decoder, and a centre sample looks at 1 of 4
      // and 1 of 16 pixels respectively — most of a scaled glyph's ink could
      // be missing or wrong and the decode would still come back clean.
      // foh_font.c fills a set column as `scale` rows x `scale` columns of
      // one solid colour, so a genuine cell is UNIFORM; a mixed cell is
      // corruption and is refused rather than resolved by majority.
      let on = 0;
      for (let dy = 0; dy < scale; dy++) {
        for (let dx = 0; dx < scale; dx++) {
          const sx = gx + c * scale + dx, sy = y + r * scale + dy;
          if (isOn(sx, sy)) { on++; continue; }
          // not this line's ink — on a flat panel, is it ink at all?
          if (flatPanel && isForeignInk(sx, sy)) {
            const p = px(sx, sy);
            die('FOREIGN INK at (' + sx + ',' + sy + ') = ' + p.join(',') +
                ' inside glyph ' + gi + ' cell (row ' + r + ', col ' + c +
                '): a bright pixel that is NOT this line\'s colour (' + onR +
                ',' + onG + ',' + onB + '). This decode was declared ' +
                'flat-panel, where the only ink in the line rectangle is the ' +
                'line — so this is damage, not background', 3);
          }
        }
      }
      if (on === scale * scale) b |= (0x10 >> c);
      else if (on !== 0) {
        die('glyph ' + gi + ' cell (row ' + r + ', col ' + c + ') at x=' +
            (gx + c * scale) + ' y=' + (y + r * scale) + ' is MIXED: ' + on +
            ' of ' + (scale * scale) + ' pixels carry the colour. foh_text ' +
            'fills a set column solid at every scale, so a partly-on cell is ' +
            'not a glyph — it is damage', 3);
      }
    }
    rows.push(b);
    ink |= b;
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
// UNSOUND-NEGATIVE GUARD (the C35 class). The font has a blank glyph, so a
// window or colour that lands entirely on background decodes to a run of
// SPACES and reports a clean, wrong answer instead of failing — an
// instrument that cannot see its subject saying "nothing is there". No
// caller asks this tool to decode a blank line, so zero ink is death.
if (ink === 0) {
  die('decoded ' + n + ' blank glyphs at y=' + y + ' scale=' + scale +
      ' window=' + winX + ',' + winW + ' colour=' + srcR + ',' + srcG + ',' + srcB +
      ' — NO pixel of that colour was sampled anywhere on the line (wrong ' +
      'row, wrong window or wrong colour class), so this decode observed nothing', 3);
}
process.stdout.write(out);
