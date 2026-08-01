#!/usr/bin/env node
// make-jitter-flow.js — derive a ±1-device-frame VARIANT of a committed FLOW1
// script (B11, owner-ratified 2026-08-01; docs/STATE.md §rulings decision 2).
//
// WHY THIS EXISTS
// ---------------
// The host twin is TICK-INDEXED: `I 708 U` is applied when the tick counter
// equals 708, so a 36-frame hold is exactly 36 frames, every run. The DEVICE
// has no such channel — it is a real machine with real buttons, so
// flow-to-fkscript.js converts frames to wall-clock times and a separate
// process injects key events at those times. Two independent clocks then
// decide how many polls observe the key held, and that generator's own header
// states the consequence:
//
//     "A held direction therefore lasts N device frames +/- 1 for an N-frame
//      flow hold; the committed flows are authored against that tolerance."
//
// The flows ARE authored against it: every cursor stop is approached from a
// screen CLAMP (which absorbs overshoot entirely) and then held a counted
// number of frames to a target with more than one frame of slack on each
// side. That protects the LOGICAL outcome, and it works — the device's event
// trace comes back byte-identical to the twin's.
//
// It cannot protect the RESTING PIXEL POSITION. The hand moves 2.40 px/frame
// in x and 3.84 in y (DEVIATION D3), so one frame of hold difference
// relocates it, and the screenshot judge is `cmp`. MEASURED 2026-08-01: a
// device css shot was byte-identical to a twin whose UP hold ran 37 frames
// instead of 36 (51e0d8db…c00217 both sides), while the committed 36-frame
// twin was 3abd60ff…5c7ec91. The judge was strictly stronger than the
// determinism the injector provides, and passed only when the jitter happened
// to land on the authored count.
//
// WHAT THIS DOES, AND WHAT IT DELIBERATELY DOES NOT DO
// ----------------------------------------------------
// It moves ONE OR MORE release rows by exactly ±1 frame, so the check can
// build the small set of host twins the declared tolerance admits and demand
// a BYTE-EXACT match against one of them. Byte-exactness is retained. A
// position-tolerant comparison was offered to the owner and REFUSED on HARD
// RULE 3 grounds; this is the alternative that keeps `cmp`.
//
// It refuses anything it cannot justify:
//   * a named frame that is not a release row (`I <frame> -`)      -> throw
//   * a release not preceded by a DIRECTION hold                   -> throw
//   * a shift that would collide with, or reorder past, a          -> throw
//     neighbouring row
//   * a delta outside {-1,0,+1}                                    -> throw
// so a flow edit that moves a hold makes this tool fail loudly instead of
// silently widening what the check accepts.
//
// USAGE
//   node port/foh/make-jitter-flow.js <in.flow> <out.flow> <spec>
//   spec := <frame>:<delta>[,<frame>:<delta>...]      delta in -1 | 0 | +1
// e.g.
//   node port/foh/make-jitter-flow.js port/foh/flows/f01-vs-g01.flow \
//        /tmp/v.flow 703:+1,744:-1

"use strict";
const fs = require("fs");

const DIRS = new Set(["U", "D", "L", "R"]);

function die(msg) {
  console.error("make-jitter-flow: " + msg);
  process.exit(1);
}

const [, , inPath, outPath, spec] = process.argv;
if (!inPath || !outPath || !spec) {
  die("usage: make-jitter-flow.js <in.flow> <out.flow> <frame>:<delta>[,...]");
}

const src = fs.readFileSync(inPath, "utf8");
const lines = src.split("\n");

// Parse every `I <frame> <token>` row, keeping its line index. Row ORDER in
// the file is the authority; frames are asserted strictly increasing below,
// because a variant is only meaningful against a monotonic script.
const rows = [];
lines.forEach((ln, i) => {
  const m = /^I ([0-9]+) ([-A-Z]+)$/.exec(ln);
  if (m) rows.push({ i, frame: Number(m[1]), tok: m[2] });
});
if (rows.length === 0) die("no `I <frame> <token>` rows in " + inPath);
for (let k = 1; k < rows.length; k++) {
  if (rows[k].frame <= rows[k - 1].frame) {
    die(
      "input rows are not strictly increasing in frame (" +
        rows[k - 1].frame + " then " + rows[k].frame + ") — refusing to derive a variant"
    );
  }
}

// Parse the spec.
const wanted = new Map();
for (const part of spec.split(",")) {
  const m = /^([0-9]+):([+-]?[01])$/.exec(part.trim());
  if (!m) die("bad spec element '" + part + "' (want <frame>:<-1|0|+1>)");
  const frame = Number(m[1]);
  const delta = Number(m[2]);
  if (delta !== -1 && delta !== 0 && delta !== 1) {
    die("delta for frame " + frame + " is " + delta + ", want -1, 0 or +1");
  }
  if (wanted.has(frame)) die("frame " + frame + " named twice in the spec");
  wanted.set(frame, delta);
}

// Validate each named frame and compute its new value.
const moves = new Map(); // line index -> new frame
for (const [frame, delta] of wanted) {
  const k = rows.findIndex((r) => r.frame === frame);
  if (k < 0) die("no `I " + frame + " ...` row in " + inPath);
  const row = rows[k];
  if (row.tok !== "-") {
    die("row `I " + frame + " " + row.tok + "` is not a RELEASE row — only a release may be shifted");
  }
  if (k === 0) die("release at frame " + frame + " has no preceding row");
  const prev = rows[k - 1];
  const heldDir = [...prev.tok].some((c) => DIRS.has(c));
  if (!heldDir) {
    die(
      "the row before release " + frame + " is `I " + prev.frame + " " + prev.tok +
        "`, which holds no direction — this release does not end a cursor leg, so shifting it would not model injector jitter"
    );
  }
  if (delta === 0) continue;
  const next = rows[k + 1];
  const nf = frame + delta;
  if (nf <= prev.frame) {
    die("shifting release " + frame + " by " + delta + " would reach its own press at " + prev.frame);
  }
  if (next && nf >= next.frame) {
    die("shifting release " + frame + " by " + delta + " would collide with the next row at " + next.frame);
  }
  moves.set(row.i, nf);
}

if (moves.size === 0) {
  // A zero-delta variant is the committed flow itself. Emitting it VERBATIM is
  // correct and is what makes the "0,0" member of an acceptance set exactly
  // the existing reference rather than a re-derived lookalike.
  fs.writeFileSync(outPath, src);
  process.exit(0);
}

const out = lines.map((ln, i) => {
  if (!moves.has(i)) return ln;
  return "I " + moves.get(i) + " -";
});
const text = out.join("\n");
if (text === src) die("the derived variant is byte-identical to the input (nothing was shifted)");
fs.writeFileSync(outPath, text);
