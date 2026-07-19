#!/usr/bin/env node
// port/gfx/judge-shot.js — HOST-side structural judge of the in-app
// device screenshot (M3 task 4). The screenshot is the app's OWN
// framebuffer (never the kernel fb — CLAUDE.md gotcha), pulled as
// PPM (565->888 by the gfx_dump_ppm shifts) + ink PGM. This judge
// asserts it is a REAL composited frame, not a blank/garbage surface:
//
//   1. PPM is exactly P6 240x240/255; PGM exactly P5 240x240/255 with
//      only 0/255 bytes.
//   2. Letterbox: rows [0,45) and [195,240) are UNIFORM (the background
//      colour — the compositor clips all drawing to the [45,195) band).
//   3. The band contains >= 8 distinct colours (a stage + two players
//      frame; a blank/cleared band has exactly 1).
//   4. Ink coverage inside the band is within [0.5%, 90%] of band
//      pixels (0 = nothing composited; ~100% = garbage flood), and
//      ZERO ink outside the band (the rasterizer clips there).
//
// RETIRED criterion 5 (M4 task 3 — a REVIEWED pin change, pre-registered
// in the iter-73 brief/AGENT-LOG): "every band pixel differing from the
// background colour must be inked". TRUE for the M3 visual surface; made
// structurally FALSE by M4 task 2's background-art plane, which is drawn
// ink-SUPPRESSED by design (rast_ink_enable(0) — bg is not part of the
// silhouette mask on either side), so gradient/starfield/mountain pixels
// legitimately differ from the clear colour without ink (measured f0900:
// 32,153 such pixels). The count is still REPORTED (changed_not_inked=)
// for observability, no longer gated. EXPOSURE (PROCESS §8): PPM/PGM
// cross-corruption on the pull path is no longer caught HERE — it is
// fully covered downstream by check-device-render.sh's GATING bit-compare
// of BOTH planes against the host render, and every pull is already
// sha-verified by pullv; for host-side shots there is no transport to
// corrupt.
//
// Thresholds are STRUCTURAL floors (measured host render f0900: M3
// surface ~30 distinct colours, ~8% band ink; M4 surface with bg art +
// vfx + overlay: ~767 colours, ~10.7% band ink — both inside the frozen
// floors, which are unchanged), not perceptual tuning; they exist to
// make "blank/black/flooded" loud, never to grade image quality (IoU vs
// the browser is the host-side check).
//
// Usage: node judge-shot.js <shot.ppm> <shot.pgm>
// Prints strict key=value lines (distinct colours, ink fraction) then
// SHOT STRUCTURE OK; any violation dies loudly, exit 3.
"use strict";

const fs = require("fs");

function die(msg) {
  console.error("judge-shot: " + msg);
  process.exit(3);
}

const W = 240, H = 240, BAND0 = 45, BAND1 = 195;

const [ppmPath, pgmPath] = process.argv.slice(2);
if (!ppmPath || !pgmPath) {
  console.error("usage: node judge-shot.js <shot.ppm> <shot.pgm>");
  process.exit(1);
}

function parsePnm(path, magic, chans) {
  const buf = fs.readFileSync(path);
  const head = magic + "\n" + W + " " + H + "\n255\n";
  const hb = Buffer.from(head, "ascii");
  if (buf.length !== hb.length + W * H * chans) {
    die(path + ": wrong size (" + buf.length + " bytes; want header+"
        + W * H * chans + ")");
  }
  if (!buf.subarray(0, hb.length).equals(hb)) {
    die(path + ": header is not exactly " + JSON.stringify(head));
  }
  return buf.subarray(hb.length);
}

const rgb = parsePnm(ppmPath, "P6", 3);
const ink = parsePnm(pgmPath, "P5", 1);
for (let i = 0; i < W * H; i++) {
  if (ink[i] !== 0 && ink[i] !== 255) die("pgm byte " + i + " is " + ink[i] + ", want 0/255");
}

const px = (x, y) => {
  const o = (y * W + x) * 3;
  return (rgb[o] << 16) | (rgb[o + 1] << 8) | rgb[o + 2];
};

// 2. letterbox uniformity (both bars must be the same single colour)
const bg = px(0, 0);
for (let y = 0; y < H; y++) {
  if (y >= BAND0 && y < BAND1) continue;
  for (let x = 0; x < W; x++) {
    if (px(x, y) !== bg) {
      die("letterbox row " + y + " col " + x + " is not the background colour");
    }
  }
}

// 3-5. band statistics
const colours = new Set();
let inked = 0, changedNotInked = 0;
for (let y = BAND0; y < BAND1; y++) {
  for (let x = 0; x < W; x++) {
    const c = px(x, y);
    colours.add(c);
    if (ink[y * W + x]) inked++;
    else if (c !== bg) changedNotInked++;
  }
}
let inkOutside = 0;
for (let y = 0; y < H; y++) {
  if (y >= BAND0 && y < BAND1) continue;
  for (let x = 0; x < W; x++) if (ink[y * W + x]) inkOutside++;
}

const bandPixels = (BAND1 - BAND0) * W;
const inkFrac = inked / bandPixels;
console.log("distinct_colours=" + String(colours.size));
console.log("ink_fraction=" + inkFrac.toFixed(4));
console.log("ink_outside_band=" + String(inkOutside));
// reported only — no longer gated (retired criterion 5, header note)
console.log("changed_not_inked=" + String(changedNotInked));

if (colours.size < 8) die("band has only " + colours.size + " distinct colours (blank/flat frame)");
if (inkFrac < 0.005) die("band ink fraction " + inkFrac.toFixed(4) + " < 0.005 (nothing composited)");
if (inkFrac > 0.90) die("band ink fraction " + inkFrac.toFixed(4) + " > 0.90 (flooded frame)");
if (inkOutside !== 0) die(inkOutside + " inked pixels OUTSIDE the letterbox band (clip breach)");
console.log("SHOT STRUCTURE OK");
