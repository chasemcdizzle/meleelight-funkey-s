#!/usr/bin/env node
// port/gfx/glyph-compare.js — measured-then-frozen VFXGLYPHS1 comparator
// (M4 task 2 glyph-jitter class fix, iter 72).
//
// WHY NOT cmp: the committed vfxglyphs-frozen.txt is browser-rasterized
// TEXT. Cross-session canvas font rasterization is NOT bit-deterministic
// (measured: ~1-in-8 cold captures flip a single antialiased pixel's
// 5x5-area-average across a Math.round boundary — driver find,
// .loop/driver-cold-m4t2r71-donecheck.log: sprite "ready" pixel 2576,
// r 234->235, delta 1; 5 fresh probe sessions + the frozen file were
// otherwise byte-identical, .loop/m4-task2r72-probe.log). A bit-exact
// cmp on this artifact is a FALSE oracle: it fails healthy code. This
// comparator replaces it with an HONEST measured contract:
//
//   STRUCTURAL FIELDS EXACT (any drift = death): line count, record
//     order, every GLYPH/SPRITE header line byte-for-byte (font id,
//     char code, dimensions, offsets, advance, sprite names), mask-line
//     tags and lengths, the VFXGLYPHS1/END frame.
//   PIXEL CHANNELS TOLERANCED (frozen, never loosened): any channel
//     (FMASK/SMASK alpha; RGBA r,g,b,a) may differ by at most
//     MAX_CHANNEL_DELTA, and at most MAX_DIFF_PIXELS pixels (of the
//     19,764 total) may differ AT ALL. Measured basis (iter 72
//     characterization): max observed delta 1, max observed
//     differing-pixel count 1 -> frozen at 4x + floor: delta 4,
//     count 16 (pre-registered freeze rule). A pixel outside the
//     channel tolerance, or a 17th differing pixel, is real drift ->
//     death ("re-freeze is a reviewed change").
//
// The pins are HARD-CODED here AND frozen in expected-render.json
// (glyphComparePins) — --judge asserts the twin pins agree (the
// inkNames twin-pin class: a silently edited JSON cannot loosen the
// comparator, a silently edited comparator trips the JSON).
//
// Usage:
//   node glyph-compare.js --judge <frozen> <fresh> <expected-render.json>
//     exit 0 + "GLYPHS MATCH ..." iff the contract above holds.
//   node glyph-compare.js --measure <a> <b>
//     characterization mode: same parser, reports every differing pixel
//     + per-channel deltas; exit 0 if structurally identical (pixel
//     diffs allowed and reported), exit 2 on structural drift.
"use strict";

const fs = require("fs");

const MAX_CHANNEL_DELTA = 4; // frozen iter 72 (measured max 1); never loosened
const MAX_DIFF_PIXELS = 16;  // frozen iter 72 (measured max 1); never loosened

const INT_RE = /^(0|-?[1-9][0-9]*)$/;
const NUM_RE = /^-?(0|[1-9][0-9]*)(\.[0-9]+)?$/;
const HEX_RE = /^[0-9a-f]*$/;

function die(msg) {
  console.error("glyph-compare: " + msg);
  process.exit(1);
}

// Strict VFXGLYPHS1 grammar -> ordered record list. Fails loud on any
// malformation (a malformed file is never "compared", it is rejected).
function parse(fp) {
  const raw = fs.readFileSync(fp, "utf8");
  if (!raw.endsWith("\n")) die(fp + ": missing trailing newline");
  const lines = raw.slice(0, -1).split("\n");
  if (lines[0] !== "VFXGLYPHS1") die(fp + ": line 1 != VFXGLYPHS1");
  if (lines[lines.length - 1] !== "END") die(fp + ": last line != END");
  const recs = [{ kind: "frame", text: lines[0], line: 1 }];
  let i = 1;
  const mask = (tag, w, h, pxHex, owner) => {
    const l = lines[i];
    if (l === undefined || !l.startsWith(tag + " "))
      die(fp + ":" + (i + 1) + ": expected " + tag + " after " + owner);
    const hex = l.slice(tag.length + 1);
    if (!HEX_RE.test(hex) || hex.length !== w * h * pxHex)
      die(fp + ":" + (i + 1) + ": " + tag + " hex malformed or length " +
          hex.length + " != " + w * h * pxHex + " (" + owner + ")");
    recs.push({ kind: "mask", tag, hex, w, h,
                chPerPx: pxHex / 2, owner, line: i + 1 });
    i++;
  };
  while (i < lines.length - 1) {
    const l = lines[i];
    const t = l.split(" ");
    if (t[0] === "GLYPH") {
      if (t.length !== 8 || !INT_RE.test(t[1]) || !INT_RE.test(t[2]) ||
          !INT_RE.test(t[3]) || !INT_RE.test(t[4]) || !NUM_RE.test(t[5]) ||
          !NUM_RE.test(t[6]) || !NUM_RE.test(t[7]))
        die(fp + ":" + (i + 1) + ": malformed GLYPH line");
      const w = parseInt(t[3], 10), h = parseInt(t[4], 10);
      if (w < 0 || h < 0 || (w === 0) !== (h === 0))
        die(fp + ":" + (i + 1) + ": bad GLYPH dims " + w + "x" + h);
      const owner = "GLYPH " + t[1] + " " + t[2];
      recs.push({ kind: "header", text: l, line: i + 1 });
      i++;
      if (w > 0) { mask("FMASK", w, h, 2, owner); mask("SMASK", w, h, 2, owner); }
    } else if (t[0] === "SPRITE") {
      if (t.length !== 6 || !/^[A-Za-z][A-Za-z0-9]*$/.test(t[1]) ||
          !INT_RE.test(t[2]) || !INT_RE.test(t[3]) ||
          !NUM_RE.test(t[4]) || !NUM_RE.test(t[5]))
        die(fp + ":" + (i + 1) + ": malformed SPRITE line");
      const w = parseInt(t[2], 10), h = parseInt(t[3], 10);
      if (w <= 0 || h <= 0)
        die(fp + ":" + (i + 1) + ": bad SPRITE dims " + w + "x" + h);
      recs.push({ kind: "header", text: l, line: i + 1 });
      i++;
      mask("RGBA", w, h, 8, "SPRITE " + t[1]);
    } else {
      die(fp + ":" + (i + 1) + ": unexpected line tag '" + t[0] + "'");
    }
  }
  recs.push({ kind: "frame", text: "END", line: lines.length });
  return recs;
}

// Lockstep compare. Returns { structural: [msgs], diffs: [pixel diffs] }.
function compare(fpA, fpB) {
  const A = parse(fpA), B = parse(fpB);
  const structural = [];
  const diffs = [];
  if (A.length !== B.length) {
    structural.push("record count " + A.length + " != " + B.length);
    return { structural, diffs };
  }
  for (let r = 0; r < A.length; r++) {
    const a = A[r], b = B[r];
    if (a.kind !== b.kind) {
      structural.push("record " + r + " kind " + a.kind + " != " + b.kind +
                      " (lines " + a.line + "/" + b.line + ")");
      return { structural, diffs };
    }
    if (a.kind === "frame" || a.kind === "header") {
      if (a.text !== b.text)
        structural.push("line " + a.line + ": '" + a.text + "' != '" + b.text + "'");
      continue;
    }
    // mask record
    if (a.tag !== b.tag || a.hex.length !== b.hex.length ||
        a.w !== b.w || a.h !== b.h) {
      structural.push("line " + a.line + ": mask shape drift (" + a.tag + " " +
                      a.w + "x" + a.h + " len " + a.hex.length + " vs " + b.tag +
                      " " + b.w + "x" + b.h + " len " + b.hex.length + ")");
      continue;
    }
    if (a.hex === b.hex) continue;
    const ch = a.chPerPx;
    const px = a.hex.length / (ch * 2);
    for (let p = 0; p < px; p++) {
      const pa = a.hex.slice(p * ch * 2, (p + 1) * ch * 2);
      const pb = b.hex.slice(p * ch * 2, (p + 1) * ch * 2);
      if (pa === pb) continue;
      let maxDelta = 0;
      const chans = [];
      for (let c = 0; c < ch; c++) {
        const va = parseInt(pa.slice(c * 2, c * 2 + 2), 16);
        const vb = parseInt(pb.slice(c * 2, c * 2 + 2), 16);
        if (va !== vb) {
          const d = Math.abs(va - vb);
          if (d > maxDelta) maxDelta = d;
          chans.push((ch === 4 ? "rgba"[c] : "a") + ":" + va + "->" + vb);
        }
      }
      diffs.push({ owner: a.owner, tag: a.tag, line: a.line, pixel: p,
                   x: p % a.w, y: Math.floor(p / a.w),
                   maxDelta, detail: chans.join(",") });
    }
  }
  return { structural, diffs };
}

function main() {
  const mode = process.argv[2];
  if (mode === "--measure") {
    const [a, b] = process.argv.slice(3);
    if (!a || !b) die("--measure needs two files");
    const r = compare(a, b);
    for (const s of r.structural) console.log("STRUCTURAL: " + s);
    for (const d of r.diffs)
      console.log(`PIXEL: ${d.owner} ${d.tag} line ${d.line} px ${d.pixel} ` +
                  `(${d.x},${d.y}) maxDelta ${d.maxDelta} [${d.detail}]`);
    const maxD = r.diffs.reduce((m, d) => Math.max(m, d.maxDelta), 0);
    console.log(`MEASURE: structuralDiffs=${r.structural.length} ` +
                `diffPixels=${r.diffs.length} maxChannelDelta=${maxD}`);
    process.exit(r.structural.length ? 2 : 0);
  }
  if (mode !== "--judge") die("mode must be --judge or --measure");
  const [frozen, fresh, expPath] = process.argv.slice(3);
  if (!frozen || !fresh || !expPath)
    die("--judge needs <frozen> <fresh> <expected-render.json>");
  // twin-pin assert (inkNames class): the frozen JSON pins must equal
  // the hard-coded reviewed values — neither side can drift silently.
  const pins = JSON.parse(fs.readFileSync(expPath, "utf8")).glyphComparePins;
  if (!pins || pins.maxChannelDelta !== MAX_CHANNEL_DELTA ||
      pins.maxDiffPixels !== MAX_DIFF_PIXELS)
    die("twin-pin violation: expected-render.json glyphComparePins " +
        JSON.stringify(pins) + " != hard-coded {maxChannelDelta:" +
        MAX_CHANNEL_DELTA + ",maxDiffPixels:" + MAX_DIFF_PIXELS + "}");
  const r = compare(frozen, fresh);
  if (r.structural.length) {
    for (const s of r.structural) console.error("glyph-compare: STRUCTURAL DRIFT: " + s);
    console.error("glyph-compare: captured VFXGLYPHS structure differs from " +
                  frozen + " (re-freeze is a reviewed change)");
    process.exit(1);
  }
  let bad = 0;
  for (const d of r.diffs) {
    const over = d.maxDelta > MAX_CHANNEL_DELTA;
    if (over) bad++;
    const log = over ? console.error : console.log;
    log(`glyph-compare: ${over ? "OUT-OF-TOLERANCE" : "jitter"} pixel: ` +
        `${d.owner} ${d.tag} line ${d.line} px ${d.pixel} (${d.x},${d.y}) ` +
        `maxDelta ${d.maxDelta} [${d.detail}]`);
  }
  if (bad > 0) {
    console.error("glyph-compare: " + bad + " pixel(s) exceed the frozen channel " +
                  "tolerance " + MAX_CHANNEL_DELTA + " (re-freeze is a reviewed change)");
    process.exit(1);
  }
  if (r.diffs.length > MAX_DIFF_PIXELS) {
    console.error("glyph-compare: " + r.diffs.length + " differing pixels > frozen cap " +
                  MAX_DIFF_PIXELS + " (re-freeze is a reviewed change)");
    process.exit(1);
  }
  const maxD = r.diffs.reduce((m, d) => Math.max(m, d.maxDelta), 0);
  console.log("GLYPHS MATCH (structural exact; " + r.diffs.length +
              " differing pixel(s), max channel delta " + maxD +
              "; frozen caps " + MAX_DIFF_PIXELS + "/" + MAX_CHANNEL_DELTA + ")");
}

main();
