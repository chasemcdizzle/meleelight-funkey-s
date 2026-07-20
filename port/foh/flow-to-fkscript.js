#!/usr/bin/env node
// flow-to-fkscript.js — mechanical FLOW1 -> fk_input script derivation
// (fix_plan §M4 task 10; pre-registration AGENT-LOG iter 93).
//
// Timing model (pre-registered): t(F) = LEAD_MS + (F - 370) * STEP_MS
// for every flow frame F >= 371 (all committed flows' first inputs sit
// at 375+ — asserted). LEAD_MS = 8200 (the FunKey title screen appears
// at tick 370 = ~6.2 s; the fk_input launch handshake is strictly
// ADDITIVE latency, so inputs can only land LATER than nominal — safe
// for the tick-indexed title shot at 373). STEP_MS = 50 (SCALE 3: one
// flow frame = ~3 device frames — presses span >= 3 polled frames,
// gaps >= 200 ms, the f04 30-frame B-hold becomes 1.5 s which the
// bhold counter reads as >= 30 held frames; the FOH machine is
// edge-driven, so longer presses never double-step).
//
// Keys map 1:1 onto the FunKey letter keysyms (CLAUDE.md "Device
// access"): U/D/L/R/A/B/X/Y/S/K/N/Q -> u/d/l/r/a/b/x/y/s/k/n/q.
//
// SHOT rows at/after the first non-neutral input row become Q-MARKER
// injections (`d q` / 40 ms / `u q`) at the SHOT row's time — the
// device app captures the settled state on the q edge (q = MENU is
// consumed by no FOH arm). SHOT rows BEFORE the first input are
// tick-indexed on the app side and are NOT injected (asserted < 375).
//
// Whitelist posture (PROCESS §3): the flow is re-parsed with the exact
// FLOW1 grammar (foh_app.c semantics); anything malformed, a first
// input row below frame 375, non-monotone rows, or an event schedule
// that would violate the fk_input grammar (sleep > 60000 ms per row is
// split) dies loudly BEFORE emitting a single line.
//
// Usage: node flow-to-fkscript.js <flow> <out.fks>
"use strict";

const fs = require("fs");

function die(msg) {
  console.error("flow-to-fkscript: " + msg);
  process.exit(2);
}

if (process.argv.length !== 4) {
  console.error("usage: node flow-to-fkscript.js <flow> <out.fks>");
  process.exit(1);
}
const [, , flowPath, outPath] = process.argv;

const LEAD_MS = 8200;
const STEP_MS = 50;
const Q_PRESS_MS = 40;

const LETTER = {
  U: "u", D: "d", L: "l", R: "r", A: "a", B: "b", X: "x", Y: "y",
  S: "s", K: "k", N: "n", Q: "q",
};

const raw = fs.readFileSync(flowPath, "utf8");
if (!raw.endsWith("\n")) die("flow missing trailing newline (torn write)");
const lines = raw.slice(0, -1).split("\n");
if (lines[0] !== "FLOW1") die("first line must be exactly FLOW1");

const rows = []; // {frame, held:Set}
const shots = []; // {frame, name}
let endFrame = 0;
let sawEnd = false;
for (let k = 1; k < lines.length; k++) {
  const ln = lines[k];
  if (sawEnd) die("content after END at line " + (k + 1));
  if (ln.length === 0) die("empty line at " + (k + 1));
  if (ln[0] === "#") continue;
  let m;
  if ((m = /^I ([0-9]+) (-|[UDLRABXYSKNQ]+)$/.exec(ln)) !== null) {
    const fr = Number(m[1]);
    if (fr <= 0 || (rows.length > 0 && fr <= rows[rows.length - 1].frame)) {
      die("I frames must be positive and strictly increasing (line " +
          (k + 1) + ")");
    }
    if (rows.length === 0 && fr !== 1) die("first I row must be frame 1");
    const held = new Set();
    if (m[2] !== "-") {
      for (const c of m[2]) {
        if (held.has(c)) die("duplicate button letter at line " + (k + 1));
        held.add(c);
      }
    }
    rows.push({ frame: fr, held });
    continue;
  }
  if ((m = /^SHOT ([0-9]+) ([a-z0-9-]{1,32})$/.exec(ln)) !== null) {
    const fr = Number(m[1]);
    if (shots.length > 0 && fr <= shots[shots.length - 1].frame) {
      die("SHOT frames must be strictly increasing (line " + (k + 1) + ")");
    }
    shots.push({ frame: fr, name: m[2] });
    continue;
  }
  if ((m = /^END ([0-9]+)$/.exec(ln)) !== null) {
    endFrame = Number(m[1]);
    sawEnd = true;
    continue;
  }
  die("line " + (k + 1) + " matches no FLOW1 form: '" + ln + "'");
}
if (!sawEnd || rows.length === 0) die("missing END or I rows");
for (const s of shots) {
  if (s.frame > endFrame) die("SHOT past END");
}

// first non-neutral input row (the tick/marker split rule)
let firstInput = 0;
for (const r of rows) {
  if (r.held.size > 0) { firstInput = r.frame; break; }
}
if (firstInput === 0) die("flow has no non-neutral input row (nothing to inject)");
if (firstInput < 375) {
  die("first input row at frame " + firstInput +
      " < 375 (the pre-registered LEAD model requires the title lead)");
}

// event schedule: key transitions at each input row + q markers at
// marker-shot rows, merged in time order (ties: input row first —
// committed flows never collide input and SHOT frames; assert anyway).
const events = []; // {ms, ops:[..]}
function tOf(frame) {
  if (frame <= 370) die("input/marker event at frame " + frame + " <= 370");
  return LEAD_MS + (frame - 370) * STEP_MS;
}
let held = new Set();
for (const r of rows) {
  if (r.held.size === 0 && held.size === 0) continue; // neutral no-op row
  const ops = [];
  for (const c of held) {
    if (!r.held.has(c)) ops.push("u " + LETTER[c]);
  }
  for (const c of r.held) {
    if (!held.has(c)) ops.push("d " + LETTER[c]);
  }
  if (ops.length > 0) events.push({ ms: tOf(r.frame), ops });
  held = r.held;
}
if (held.size > 0) {
  // release anything still held at END (fk_input also releases all
  // defensively; the explicit release keeps the schedule total)
  const ops = [];
  for (const c of held) ops.push("u " + LETTER[c]);
  events.push({ ms: tOf(endFrame), ops });
}
for (const s of shots) {
  if (s.frame >= firstInput) {
    events.push({ ms: tOf(s.frame), ops: ["d q"], qup: tOf(s.frame) + Q_PRESS_MS });
  } else if (s.frame >= 375) {
    die("tick-indexed shot at frame " + s.frame + " >= 375 (split rule violated)");
  }
}
// expand q releases as their own events
const expanded = [];
for (const e of events) {
  expanded.push({ ms: e.ms, ops: e.ops });
  if (e.qup) expanded.push({ ms: e.qup, ops: ["u q"] });
}
expanded.sort((a, b) => a.ms - b.ms);
for (let i = 1; i < expanded.length; i++) {
  if (expanded[i].ms === expanded[i - 1].ms) {
    die("two events collide at t=" + expanded[i].ms +
        " ms (input row and SHOT share a frame — not a committed-flow shape)");
  }
}

const out = [];
out.push("# generated by flow-to-fkscript.js from " + flowPath);
out.push("# LEAD_MS=" + LEAD_MS + " STEP_MS=" + STEP_MS + " (AGENT-LOG iter 93)");
let cur = 0;
for (const e of expanded) {
  let gap = e.ms - cur;
  if (gap < 0) die("negative gap (schedule bug)");
  while (gap > 60000) { // fk_input sleep cap per row
    out.push("s 60000");
    gap -= 60000;
  }
  if (gap > 0) out.push("s " + gap);
  cur = e.ms;
  for (const op of e.ops) out.push(op);
}
fs.writeFileSync(outPath, out.join("\n") + "\n");
console.log("fkscript OK " + outPath + " (events=" + expanded.length +
            ", span=" + cur + " ms)");
