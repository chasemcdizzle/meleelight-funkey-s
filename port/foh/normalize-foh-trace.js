#!/usr/bin/env node
// normalize-foh-trace.js — FOHTRACE1 frame-field ELISION + BOUNDED-DELTA
// judgment (fix_plan §M4 task 10; iter 95 review-93 M1 hardening).
//
// MODE 1 (elide): `node normalize-foh-trace.js <trace.txt> <out.txt>`
// The device FOH legs are driven by wall-clock fk_input injection, so
// event TICK NUMBERS cannot be frame-exact against the frozen host
// traces — every STRUCTURAL fact can: edges (from/to/cause), S
// field/values, LAUNCH parameters, SHOT names, line ORDER, and the END
// transitions count. This mode rewrites a FOHTRACE1 file with the
// frame field of every event line replaced by the literal 'F', and
// NOTHING else; the check runs BOTH the frozen .expect and the device
// trace through it and requires byte equality of the outputs (frozen
// bytes are never edited).
//
// MODE 2 (bounded): `node normalize-foh-trace.js --bounded
// <frozen.expect> <device-trace> <flow> <end-max>`
// Elision alone erases UNBOUNDED latency (review-93 M1): a multi-second
// mid-run uinput/SDL stall shifted every event and still normalized
// clean inside the leg's 600-tick total allowance. This mode judges the
// device ticks against the flow script's expected injection cadence
// (flow-to-fkscript.js's pinned model): an event caused by flow frame F
// is injected at LEAD_MS + (F-370)*STEP_MS wall ms, i.e. expected
// device tick T^(F) = 3F - 618 at 60 Hz; events BEFORE the flow's
// first non-neutral input row are tick-indexed (T == F).
// MEASURED-THEN-FROZEN bounds (iter 95, from the iter-93 archived green
// device traces — measurement recorded in AGENT-LOG iter 95; measured
// values: identity-phase |T-F| = 0 everywhere; run anchors delta1 in
// {75, 86, 87, 93, 134}; in-run deviation [-57 (the f04 30-tick bhold
// counter runs 1:1 on device ticks — systematic), +1]; END == LAUNCH
// tick on every launch leg; no-launch END == foh-max exactly):
//   ID_SLACK  = 2    pre-input events: |T - F| <= 2
//   OFS_LO/HI = 40 / 240   the run's anchor delta1 = T1 - T^(F1)
//   DEV_NEG/POS = 90 / 30  later events: delta_i - delta1 in [-90, +30]
//   END: launch trace -> END tick == LAUNCH tick (exact);
//        no-launch    -> END tick == <end-max> (the leg's foh-max).
// A violation is rc 3 (loud, names the line); the structural pairing is
// re-verified line-by-line (kind + payload byte-equal) — any mismatch
// or grammar violation is rc 2. A mid-run stall > DEV_POS ticks beyond
// the anchored cadence now DIES instead of normalizing away.
//
// Whitelist posture (PROCESS §3): every line must match one of the
// exact FOHTRACE1 forms (the judge-foh-trace.js patterns); anything
// that merely resembles one — or any unknown line — is corruption:
// loud death, exit 2, no partial output.
"use strict";

const fs = require("fs");

function die(msg) {
  console.error("normalize-foh-trace: CORRUPT: " + msg);
  process.exit(2);
}

function boundDie(msg) {
  console.error("normalize-foh-trace: BOUND VIOLATION: " + msg);
  process.exit(3);
}

const RE_HDR = /^FOHTRACE1 flow=([a-z0-9-]+)$/;
const RE_T = /^T ([0-9]+) ([a-z-]+ [a-z-]+ (?:timer|start|a|b|bhold|launch))$/;
const RE_S = /^S ([0-9]+) ((?:p1char|p2char|p2type|difficulty|turbo|lcancel|tapjump[1-4]) [0-9]|refused [a-z0-9]+)$/;
const RE_SHOT = /^SHOT ([0-9]+) ([a-z0-9-]{1,32})$/;
const RE_LAUNCH = /^LAUNCH ([0-9]+) (p1=[0-4] p2=[0-4] p2type=[01] difficulty=[1-4] stage=[0-5] turbo=[01] lcancel=[012] tapjump=[01],[01],[01],[01] versus=0)$/;
const RE_END = /^END ([0-9]+) (transitions=[0-9]+)$/;

// strict parse -> {hdr, lines:[{kind, tick, rest}]}
function parseTrace(path) {
  const raw = fs.readFileSync(path, "utf8");
  if (raw.length === 0) die("empty trace (" + path + ")");
  if (!raw.endsWith("\n")) die("missing trailing newline (torn write): " + path);
  const lines = raw.slice(0, -1).split("\n");
  if (!RE_HDR.test(lines[0])) die("bad header: '" + lines[0] + "' (" + path + ")");
  const out = { hdr: lines[0], lines: [] };
  let sawEnd = false;
  for (let k = 1; k < lines.length; k++) {
    const ln = lines[k];
    if (sawEnd) die("content after END at line " + (k + 1) + " (" + path + ")");
    let m;
    if ((m = RE_T.exec(ln)) !== null) {
      out.lines.push({ kind: "T", tick: Number(m[1]), rest: m[2] });
      continue;
    }
    if ((m = RE_S.exec(ln)) !== null) {
      out.lines.push({ kind: "S", tick: Number(m[1]), rest: m[2] });
      continue;
    }
    if ((m = RE_SHOT.exec(ln)) !== null) {
      out.lines.push({ kind: "SHOT", tick: Number(m[1]), rest: m[2] });
      continue;
    }
    if ((m = RE_LAUNCH.exec(ln)) !== null) {
      out.lines.push({ kind: "LAUNCH", tick: Number(m[1]), rest: m[2] });
      continue;
    }
    if ((m = RE_END.exec(ln)) !== null) {
      out.lines.push({ kind: "END", tick: Number(m[1]), rest: m[2] });
      sawEnd = true;
      continue;
    }
    die("line " + (k + 1) + " matches no FOHTRACE1 form: '" + ln + "' (" +
        path + ")");
  }
  if (!sawEnd) die("no END line (truncated trace): " + path);
  return out;
}

if (process.argv[2] === "--bounded") {
  // ---- MODE 2: bounded-delta judgment ------------------------------------
  if (process.argv.length !== 7) {
    console.error("usage: node normalize-foh-trace.js --bounded " +
                  "<frozen.expect> <device-trace> <flow> <end-max>");
    process.exit(1);
  }
  const [, , , expPath, devPath, flowPath, endMaxArg] = process.argv;
  if (!/^[0-9]{1,7}$/.test(endMaxArg)) die("end-max grammar: '" + endMaxArg + "'");
  const endMax = Number(endMaxArg);

  // frozen bounds (measured iter 95 — header block; never loosen)
  const ID_SLACK = 2;
  const OFS_LO = 40;
  const OFS_HI = 240;
  const DEV_NEG = 90;
  const DEV_POS = 30;
  const LEAD_MS = 8200;
  const STEP_MS = 50;
  // T^(F) = (LEAD_MS + (F-370)*STEP_MS) ms * 60/1000 = 3F - 618 at the
  // pinned LEAD/STEP (flow-to-fkscript.js's model, restated)
  const model = (F) =>
    Math.round(((LEAD_MS + (F - 370) * STEP_MS) * 60) / 1000);

  // first non-neutral input row from the flow (strict FLOW1 subset)
  const rawFlow = fs.readFileSync(flowPath, "utf8");
  if (!rawFlow.endsWith("\n")) die("flow missing trailing newline");
  let firstInput = 0;
  {
    const lines = rawFlow.slice(0, -1).split("\n");
    if (lines[0] !== "FLOW1") die("flow header must be exactly FLOW1");
    for (let k = 1; k < lines.length; k++) {
      const ln = lines[k];
      if (ln.length === 0) die("empty flow line at " + (k + 1));
      if (ln[0] === "#") continue;
      let m;
      if ((m = /^I ([0-9]+) (-|[A-Z]+)$/.exec(ln)) !== null) {
        if (m[2] !== "-" && firstInput === 0) firstInput = Number(m[1]);
        continue;
      }
      if (/^SHOT ([0-9]+) ([a-z0-9-]{1,32})$/.test(ln)) continue;
      if (/^END ([0-9]+)$/.test(ln)) continue;
      die("flow line " + (k + 1) + " matches no FLOW1 form: '" + ln + "'");
    }
    if (firstInput === 0) die("flow has no non-neutral input row");
  }

  const exp = parseTrace(expPath);
  const dev = parseTrace(devPath);
  if (exp.hdr !== dev.hdr) die("trace headers differ");
  if (exp.lines.length !== dev.lines.length) {
    die("structural line counts differ (" + exp.lines.length + " vs " +
        dev.lines.length + ")");
  }
  let anchor = null;
  let launchTick = null;
  for (let i = 0; i < exp.lines.length; i++) {
    const e = exp.lines[i];
    const d = dev.lines[i];
    if (e.kind !== d.kind || e.rest !== d.rest) {
      die("structural mismatch at event " + (i + 1) + ": '" + e.kind + " " +
          e.rest + "' vs '" + d.kind + " " + d.rest + "'");
    }
    if (d.kind === "LAUNCH") launchTick = d.tick;
    if (e.kind === "END") {
      if (launchTick !== null) {
        if (d.tick !== launchTick) {
          boundDie("END tick " + d.tick + " != LAUNCH tick " + launchTick);
        }
      } else if (d.tick !== endMax) {
        boundDie("no-launch END tick " + d.tick + " != foh-max " + endMax);
      }
      continue;
    }
    if (e.tick < firstInput) {
      // tick-indexed phase (timer/startup events + pre-input shots)
      if (Math.abs(d.tick - e.tick) > ID_SLACK) {
        boundDie("pre-input event " + (i + 1) + " (" + e.kind + " " + e.rest +
                 "): device tick " + d.tick + " vs host " + e.tick +
                 " exceeds ID_SLACK " + ID_SLACK);
      }
      continue;
    }
    const delta = d.tick - model(e.tick);
    if (anchor === null) {
      anchor = delta;
      if (delta < OFS_LO || delta > OFS_HI) {
        boundDie("anchor event " + (i + 1) + " (" + e.kind + " " + e.rest +
                 "): injection offset " + delta + " ticks outside [" + OFS_LO +
                 "," + OFS_HI + "]");
      }
      continue;
    }
    const devn = delta - anchor;
    if (devn < -DEV_NEG || devn > DEV_POS) {
      boundDie("event " + (i + 1) + " (" + e.kind + " " + e.rest +
               "): cadence deviation " + devn + " ticks outside [-" + DEV_NEG +
               ",+" + DEV_POS + "] (anchor " + anchor + ") — mid-run stall " +
               "or schedule defect");
    }
  }
  console.log("bounded OK " + devPath + " (events=" + exp.lines.length +
              ", anchor=" + (anchor === null ? "none" : anchor) + ")");
  process.exit(0);
}

// ---- MODE 1: frame-field elision -----------------------------------------
if (process.argv.length !== 4) {
  console.error("usage: node normalize-foh-trace.js <trace.txt> <out.txt>\n" +
                "       node normalize-foh-trace.js --bounded <frozen.expect>" +
                " <device-trace> <flow> <end-max>");
  process.exit(1);
}
const [, , inPath, outPath] = process.argv;
const tr = parseTrace(inPath);
const out = [tr.hdr];
for (const ln of tr.lines) out.push(ln.kind + " F " + ln.rest);
fs.writeFileSync(outPath, out.join("\n") + "\n");
