#!/usr/bin/env node
// Extract real meleelight animation vector paths into a flat binary for the
// on-device rasterizer benchmark (ticket #8, Experiment 1).
//
// Usage: node extract_anim.js /path/to/meleelight/clone out/anim.bin
//
// Source format (verified against src/main/render.js:37-68 drawArrayPathCompress):
//   src/animations/<char>/<STATE>.js  =  module.exports = [frame, ...]
//   frame = [Int16Array, ...]  (one filled canvas path per Int16Array)
//   Int16Array = [startX, startY, then 6-tuples of cubic bezier control pts]
//   All subpaths of a frame are drawn in ONE beginPath()/fill() (nonzero winding).
//
// Output format (little-endian):
//   u32 magic 0x4E414C4D ('MLAN')
//   u32 nAnims
//   per anim:  u32 nFrames
//     per frame: u32 nPaths
//       per path: u32 nCoords, then nCoords x i16, padded to 4-byte boundary

const fs = require('fs');
const path = require('path');

const repo = process.argv[2];
const out = process.argv[3];
if (!repo || !out) {
  console.error('usage: node extract_anim.js <meleelight-clone> <out.bin>');
  process.exit(1);
}

// Order is hardcoded in rastbench.c — do not reorder.
const ANIMS = [
  ['fox', 'WAIT'], ['fox', 'DASH'], ['fox', 'ATTACKAIRN'],
  ['marth', 'WAIT'], ['marth', 'DASH'], ['marth', 'ATTACKAIRN'],
];

const chunks = [];
function u32(v) { const b = Buffer.alloc(4); b.writeUInt32LE(v >>> 0); chunks.push(b); }

u32(0x4E414C4D);
u32(ANIMS.length);

let totPaths = 0, totCoords = 0;
for (const [ch, st] of ANIMS) {
  const frames = require(path.join(repo, 'src', 'animations', ch, st + '.js'));
  if (!Array.isArray(frames)) throw new Error(`${ch}/${st}: not an array`);
  u32(frames.length);
  for (const frame of frames) {
    if (!Array.isArray(frame)) throw new Error(`${ch}/${st}: frame not an array`);
    u32(frame.length);
    for (const p of frame) {
      if (!(p instanceof Int16Array)) throw new Error(`${ch}/${st}: path not Int16Array`);
      if ((p.length - 2) % 6 !== 0)
        throw new Error(`${ch}/${st}: path len ${p.length} not 2+6k`);
      u32(p.length);
      const b = Buffer.alloc((p.length + (p.length & 1)) * 2); // pad to 4 bytes
      for (let i = 0; i < p.length; i++) b.writeInt16LE(p[i], i * 2);
      chunks.push(b);
      totPaths++; totCoords += p.length;
    }
  }
  const nP = frames.reduce((a, f) => a + f.length, 0);
  const nC = frames.reduce((a, f) => a + f.reduce((x, p) => x + p.length, 0), 0);
  console.log(`${ch}/${st}: ${frames.length} frames, ${nP} paths, ${nC} int16 coords`);
}

fs.writeFileSync(out, Buffer.concat(chunks));
console.log(`wrote ${out}: ${fs.statSync(out).size} bytes, ${totPaths} paths, ${totCoords} coords`);
