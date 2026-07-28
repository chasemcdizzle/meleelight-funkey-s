#!/usr/bin/env node
"use strict";
// PNG-decoder differential (fix_plan §M4 task A9). lib/png.js is our own
// decoder, so "it is correct" must be MEASURED against an independent one
// rather than asserted. ffmpeg is already a pipeline dependency (§5) and is
// a completely separate implementation; PNG decoding is lossless and fully
// specified, so agreement must be byte-for-byte on every source pixel — no
// version pin is involved or needed (unlike §5.2's resampler).
// Usage: node lib/assets-decode-diff.js <upstream-clone-root>

const fs = require("fs");
const path = require("path");
const { execFileSync } = require("child_process");
const { decodePng } = require("./png");

const distRoot = path.resolve(process.argv[2] || "");
const assets = path.join(distRoot, "dist", "assets");
if (!fs.existsSync(assets)) {
  console.error(`assets-decode-diff: no ${assets}`);
  process.exit(1);
}

let n = 0, bytes = 0, fail = 0;
for (const dir of ["css", "hand", "stage-icons"]) {
  for (const f of fs.readdirSync(path.join(assets, dir)).sort()) {
    if (!f.endsWith(".png")) continue;
    const abs = path.join(assets, dir, f);
    const ours = decodePng(fs.readFileSync(abs), `${dir}/${f}`);
    const ref = execFileSync("ffmpeg", ["-hide_banner", "-nostdin", "-loglevel", "error",
      "-i", abs, "-f", "rawvideo", "-pix_fmt", "rgba", "-"], { maxBuffer: 1 << 28 });
    if (!ours.rgba.equals(ref)) {
      let at = -1;
      for (let i = 0; i < Math.min(ours.rgba.length, ref.length); i++) {
        if (ours.rgba[i] !== ref[i]) { at = i; break; }
      }
      console.error(`DECODE-DIFF FAIL ${dir}/${f}: ours ${ours.rgba.length} B vs ` +
        `ffmpeg ${ref.length} B, first differing byte ${at}`);
      fail++;
      continue;
    }
    n++; bytes += ref.length;
  }
}
// The differential pins its OWN WIDTH (review-a9-1 [L]): with only an
// `n === 0` guard, a pruned directory or a renamed file silently shrinks
// the comparison and still prints a pass line.
const EXPECT_PNGS = 15;
if (fail || n !== EXPECT_PNGS) {
  console.error(`assets-decode-diff: ${fail} mismatch(es), ${n} PNG(s) compared ` +
    `(pinned ${EXPECT_PNGS})`);
  process.exit(1);
}
console.log(`decode differential: ${n} PNGs, ${bytes} RGBA bytes byte-identical ` +
  `to ffmpeg's decoder`);
