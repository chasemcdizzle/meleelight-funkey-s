#!/usr/bin/env node
// flow-to-fkscript.js — mechanical FLOW1 -> fk_input script derivation
// (fix_plan §M4 task 10; pre-registration AGENT-LOG iter 93).
//
// Timing model: t(F) = LEAD_MS + round((F - 370) * FRAME_MS) for every
// flow frame F >= 371 (all committed flows' first inputs sit at 375+ —
// asserted). LEAD_MS = 8200 (the FunKey title screen appears at tick
// 370 = ~6.2 s; the fk_input launch handshake is strictly ADDITIVE
// latency, so inputs can only land LATER than nominal — safe for the
// tick-indexed title shot at 373).
//
// SCALE IS 1:1 (changed with the CSS mechanics arc, MENU-SPEC items
// 1+2+4). The pre-registered model used STEP_MS = 50, i.e. one flow
// frame = ~3 device frames, and justified it with "the FOH machine is
// edge-driven, so longer presses never double-step". That justification
// DIED when the CSS became a free cursor: the hand integrates the d-pad
// every frame it is HELD (css.js:195-196), so a direction is now
// LEVEL-driven and a 3x scale moves the cursor three times as far. A
// 43-frame UP hold became ~129 device frames and slammed the cursor
// into the top clamp, which silently broke every gesture after it.
//
// So one flow frame is now exactly one device frame (FRAME_MS =
// 1000/60), scheduled ABSOLUTELY from the frame index so rounding error
// stays under 1 ms instead of accumulating. A held direction therefore
// lasts N device frames +/- 1 for an N-frame flow hold; the committed
// flows are authored against that tolerance (every cursor stop is
// approached from a screen CLAMP, which absorbs overshoot entirely, and
// then held a counted number of frames to a target with more than one
// frame of slack on each side).
//
// Presses of non-direction keys are WIDENED to MIN_PRESS_MS so a single
// flow-frame press still spans several polls. That is safe precisely
// where the old model's claim was true: A/B/X/Y/START/L/R are read as
// EDGES, so a longer press yields the same single rising edge. B also
// feeds the CSS/bhold hold counter, which fires on an equality
// (`bHold == 30`, css.js:188) and so is unharmed by a couple of extra
// frames; committed flows give that counter slack anyway.
//
// DIRECTIONS ARE NEVER WIDENED — a too-short one is REFUSED. Their held
// duration is semantic on the free-pointer screens, and nothing in FLOW1
// syntax distinguishes a menu edge-step from a cursor nudge, so guessing
// would silently stretch a real cursor move. Every direction press must
// therefore be AUTHORED at least MIN_PRESS_MS long.
//
// Keys map onto the FunKey letter keysyms through the FROZEN KEYMAP
// SSOT (iter 95, review-93 H2): port/foh/keymap-frozen.txt is THE
// single source of truth for logical button -> letter keysym; this
// generator consumes it at runtime (strict KEYMAP1 grammar below),
// foh_dev's --dump-keymap emits its compiled copy byte-exactly, and
// the device check cross-asserts both plus platform_sdl1.c's poll
// table. An optional third argv overrides the keymap PATH — used ONLY
// by the check's swapped-mapping device tooth on a generated COPY;
// the committed file is the default.
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
// Usage: node flow-to-fkscript.js <flow> <out.fks> [keymap]
"use strict";

const fs = require("fs");
const path = require("path");

function die(msg) {
  console.error("flow-to-fkscript: " + msg);
  process.exit(2);
}

if (process.argv.length !== 4 && process.argv.length !== 5) {
  console.error("usage: node flow-to-fkscript.js <flow> <out.fks> [keymap]");
  process.exit(1);
}
const [, , flowPath, outPath] = process.argv;
const keymapPath =
  process.argv.length === 5
    ? process.argv[4]
    : path.join(__dirname, "keymap-frozen.txt");

const LEAD_MS = 8200;
const FRAME_MS = 1000 / 60; // one device frame; the flow's own unit
const MIN_PRESS_MS = 50;    // >= 3 polls for edge-read keys
const Q_PRESS_MS = 40;
// The SHOT marker's physical keysym. ONE constant for both the overlap
// validation and the emitted ops: they were `LETTER["Q"]` and a literal "q",
// which agree under the committed keymap but would diverge under the check's
// override-keymap tooth — the validator would police one key while the script
// pressed another, and an authored Q could silently swallow a marker edge.
const MARKER_SYM = "q";
// The level-driven keys: their held DURATION is semantic (the CSS hand
// integrates them per frame), so they are never widened.
const LEVEL_KEYS = new Set(["U", "D", "L", "R"]);

// KEYMAP1 (the frozen SSOT): header + exactly 12 `map <logical>
// <FLOWLETTER> <keysym>` rows in the pinned logical order; anything
// that merely resembles a row is corruption — loud death, exit 2.
const KEYMAP_LOGICAL = [
  "up", "down", "left", "right", "a", "b", "x", "y", "start", "l", "r",
  "menu",
];
const KEYMAP_FLOW_LETTERS = "UDLRABXYSKNQ";
const LETTER = (() => {
  const raw = fs.readFileSync(keymapPath, "utf8");
  if (!raw.endsWith("\n")) die("keymap missing trailing newline (torn write)");
  const lines = raw.slice(0, -1).split("\n");
  if (lines.length !== 1 + KEYMAP_LOGICAL.length) {
    die("keymap must be exactly " + (1 + KEYMAP_LOGICAL.length) + " lines");
  }
  if (lines[0] !== "KEYMAP1") die("keymap header must be exactly KEYMAP1");
  const map = {};
  const seenSym = new Set();
  for (let k = 0; k < KEYMAP_LOGICAL.length; k++) {
    const m = /^map ([a-z]+) ([A-Z]) ([a-z])$/.exec(lines[k + 1]);
    if (m === null) die("keymap line " + (k + 2) + " matches no KEYMAP1 form");
    if (m[1] !== KEYMAP_LOGICAL[k]) {
      die("keymap line " + (k + 2) + " logical '" + m[1] +
          "' != pinned '" + KEYMAP_LOGICAL[k] + "'");
    }
    if (m[2] !== KEYMAP_FLOW_LETTERS[k]) {
      die("keymap line " + (k + 2) + " flow letter '" + m[2] +
          "' != pinned '" + KEYMAP_FLOW_LETTERS[k] + "'");
    }
    if (seenSym.has(m[3])) die("keymap keysym '" + m[3] + "' duplicated");
    seenSym.add(m[3]);
    map[m[2]] = m[3];
  }
  return map;
})();

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
  return LEAD_MS + Math.round((frame - 370) * FRAME_MS);
}
// Per-key press INTERVALS, so a press can be widened without disturbing the
// down time of anything else. Down times are exact; up times are widened for
// edge-read keys only (see the header note).
const presses = []; // {key, downMs, upMs}
const open = new Map(); // key -> index into presses
let held = new Set();
for (const r of rows) {
  if (r.held.size === 0 && held.size === 0) continue; // neutral no-op row
  for (const c of held) {
    if (!r.held.has(c)) {
      presses[open.get(c)].upMs = tOf(r.frame);
      open.delete(c);
    }
  }
  for (const c of r.held) {
    if (!held.has(c)) {
      open.set(c, presses.length);
      presses.push({ key: c, downMs: tOf(r.frame), upMs: -1 });
    }
  }
  held = r.held;
}
for (const [c, idx] of open) {
  // release anything still held at END (fk_input also releases all
  // defensively; the explicit release keeps the schedule total)
  presses[idx].upMs = tOf(endFrame);
  open.delete(c);
}
for (const p of presses) {
  if (p.upMs <= p.downMs) die("press with a non-positive span (schedule bug)");
  // A too-short press of an EDGE-read key is widened: at 1:1 it would span
  // ~17 ms, and platform_sdl1.c's poll samples FINAL key state, so it could
  // vanish between two polls entirely. Widening restores the >= 3-poll press
  // the previous 3x-scale model gave every tap for free, and is safe because
  // those keys yield the same single rising edge however long they are held.
  // A too-short DIRECTION is REFUSED instead — see below.
  const span = p.upMs - p.downMs;
  if (span < MIN_PRESS_MS) {
    // A DIRECTION is never inferred to be a tap. Duration cannot classify it
    // from FLOW1 syntax alone — the same `I n R` / `I n+1 -` pair is a menu
    // edge-step on one screen and a 2.40 px cursor nudge on another — so
    // guessing would silently stretch a free-pointer move. Instead the flow
    // must AUTHOR every direction press at least this long, and anything
    // shorter dies here. Widening then applies only to keys whose reading is
    // unambiguously an edge.
    if (LEVEL_KEYS.has(p.key)) {
      die("direction '" + p.key + "' at t=" + p.downMs + " ms is held " +
          span + " ms (" + Math.round(span / FRAME_MS) + " flow frames) — " +
          "shorter than the " + Math.ceil(MIN_PRESS_MS / FRAME_MS) +
          "-frame minimum. A direction's DURATION is semantic on the " +
          "free-pointer screens (css.js:195-196), so this translator will " +
          "not stretch it: author the press as at least " +
          Math.ceil(MIN_PRESS_MS / FRAME_MS) + " flow frames.");
    }
    p.upMs = p.downMs + MIN_PRESS_MS;
  }
}
// Collect every interval that will be EMITTED, keyed by the PHYSICAL keysym.
// Overlap is checked after that mapping, not on the flow's letters, because
// two different authored things can land on one physical key: a `Q` press in
// an `I` row and a SHOT marker are both `q`. Validating them separately left
// a hole where `I 375 Q` + `SHOT 376 ...` emitted `d q` twice with no release
// between — the marker's edge vanishes and its `u q` truncates the hold.
const intervals = []; // {sym, downMs, upMs, what}
for (const p of presses) {
  intervals.push({ sym: LETTER[p.key], downMs: p.downMs, upMs: p.upMs,
                   what: "press " + p.key });
}
{
  const byMs = new Map();
  const push = (ms, op) => {
    if (!byMs.has(ms)) byMs.set(ms, []);
    byMs.get(ms).push(op);
  };
  for (const p of presses) {
    push(p.downMs, "d " + LETTER[p.key]);
    push(p.upMs, "u " + LETTER[p.key]);
  }
  for (const [ms, ops] of byMs) events.push({ ms, ops });
}
// Markers are `q` presses Q_PRESS_MS wide — same physical key as an authored
// `Q`, so they join the same interval set and are checked together below.
for (const s of shots) {
  if (s.frame >= firstInput) {
    intervals.push({ sym: MARKER_SYM, downMs: tOf(s.frame),
                     upMs: tOf(s.frame) + Q_PRESS_MS,
                     what: "marker " + s.name });
  }
}
{
  const bySym = new Map();
  for (const iv of intervals) {
    if (!bySym.has(iv.sym)) bySym.set(iv.sym, []);
    bySym.get(iv.sym).push(iv);
  }
  for (const [sym, list] of bySym) {
    list.sort((a, b) => a.downMs - b.downMs);
    for (let i = 1; i < list.length; i++) {
      if (list[i].downMs < list[i - 1].upMs) {
        die("keysym '" + sym + "' intervals overlap: " + list[i - 1].what +
            " [" + list[i - 1].downMs + "," + list[i - 1].upMs + ") then " +
            list[i].what + " [" + list[i].downMs + "," + list[i].upMs +
            ") — the second press would emit a down edge with no release " +
            "between. Separate them by at least " +
            Math.ceil(MIN_PRESS_MS / FRAME_MS) + " flow frames.");
      }
    }
  }
}
for (const s of shots) {
  if (s.frame >= firstInput) {
    events.push({ ms: tOf(s.frame), ops: ["d " + MARKER_SYM], qup: tOf(s.frame) + Q_PRESS_MS });
  } else if (s.frame >= 375) {
    die("tick-indexed shot at frame " + s.frame + " >= 375 (split rule violated)");
  }
}
// expand q releases as their own events
const expanded = [];
for (const e of events) {
  expanded.push({ ms: e.ms, ops: e.ops });
  if (e.qup) expanded.push({ ms: e.qup, ops: ["u " + MARKER_SYM] });
}
expanded.sort((a, b) => a.ms - b.ms);
// MERGE same-instant events deterministically instead of refusing them.
// Widening a press can legitimately land its RELEASE on a marker's PRESS
// instant (f01's START release and the frame-378 SHOT marker both fall at
// 8333 ms), and those two operations do not conflict — they touch different
// keys. What WOULD be corruption is two operations on the SAME key at the
// same instant, so that is what still dies. Ordering inside an instant is
// fixed: every release first, then every press, each alphabetically, so the
// emitted script is a pure function of the flow.
{
  const merged = [];
  for (const e of expanded) {
    if (merged.length > 0 && merged[merged.length - 1].ms === e.ms) {
      merged[merged.length - 1].ops.push(...e.ops);
    } else {
      merged.push({ ms: e.ms, ops: [...e.ops] });
    }
  }
  for (const e of merged) {
    const keys = new Set();
    for (const op of e.ops) {
      const k = op.slice(2);
      if (keys.has(k)) {
        die("two operations on key '" + k + "' collide at t=" + e.ms + " ms");
      }
      keys.add(k);
    }
    e.ops.sort((a, b) =>
      a[0] !== b[0] ? (a[0] === "u" ? -1 : 1) : a.localeCompare(b));
  }
  expanded.length = 0;
  expanded.push(...merged);
}

const out = [];
out.push("# generated by flow-to-fkscript.js from " + flowPath);
out.push("# LEAD_MS=" + LEAD_MS + " FRAME_MS=1000/60 MIN_PRESS_MS=" +
         MIN_PRESS_MS + " (1:1 scale — the CSS cursor is level-driven)");
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
