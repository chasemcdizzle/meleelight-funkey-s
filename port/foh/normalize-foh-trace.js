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
// <frozen.expect> <device-trace> <flow> <end-max> [input-free]`
// Elision alone erases UNBOUNDED latency (review-93 M1): a multi-second
// mid-run uinput/SDL stall shifted every event and still normalized
// clean inside the leg's 600-tick total allowance. This mode judges the
// device ticks against the flow script's expected injection cadence
// (flow-to-fkscript.js's pinned model): an event caused by flow frame F
// is injected at LEAD_MS + round((F-370)*1000/60) wall ms, i.e. expected
// device tick T^(F) = F + 122 at 60 Hz. It was `3F - 618` until the CSS
// free cursor made a direction's DURATION semantic (css.js:195-196) and
// the injector's scale went from 3 device frames per flow frame to 1:1.
// Events BEFORE the flow's first non-neutral input row are tick-indexed
// (T == F).
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
// ANCHOR-NULL POSTURE (iter 97, review-95 L-a): a run whose events all
// precede the flow's first input receives NO anchored cadence
// judgment — that absence is FATAL (rc 3) unless the invocation is
// EXPLICITLY declared input-free: either the trailing literal arg
// `input-free` (the OPK evidence leg — no injector runs there, a
// structural fact of that leg) or the flow id sitting on the frozen
// INPUT_FREE_FLOWS whitelist below (EMPTY today — all 5 committed
// flows have measured anchors 76-91). The declaration binds BOTH
// directions: declared input-free + an anchored event = rc 2 (stale
// declaration). Never inferred.
//
// Whitelist posture (PROCESS §3): every line must match one of the
// exact FOHTRACE1 forms (the judge-foh-trace.js patterns); anything
// that merely resembles one — or any unknown line — is corruption:
// loud death, exit 2, no partial output. Numerals are CANONICAL
// decimals ((0|[1-9][0-9]*) — iter 97, review-95 M-e): a leading-zero
// tick like `007` is corruption, matching the C producer's %ld
// emission exactly.
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

// canonical decimal (iter 97, review-95 M-e): 0 or no-leading-zero
const NUM = "(0|[1-9][0-9]*)";
const RE_HDR = /^FOHTRACE1 flow=([a-z0-9-]+)$/;
const RE_T = new RegExp("^T " + NUM + " ([a-z-]+ [a-z-]+ (?:timer|start|a|b|bhold|launch))$");
// MENU-SPEC items 2/3/4 (CSS mechanics): the type fields gained N/A (-1,
// DEVIATION D5's 3-cycle), P1 gained its own type + CPU level (upstream has
// no port-0 special case, main.js:504-520), and `carry` is
// whichTokenGrabbed[0] (css.js:68) — the token-gesture state, -1 or a port.
// MENU-FIDELITY ARC: the completed gameplay row list adds flashlcancel /
// walljump (0/1, gameplaymenu.js:50/:53) and the audio screen adds the two
// volume fields in TENTHS (0..10, audiomenu.js:103/:109). This grammar is
// the DEVICE side's independent copy of judge-foh-trace.js's RE_S_NUM —
// review-r2 BLOCKER: it was missed, and it rejected the very trace the
// host gate had just frozen. Domains are spelled out per field here too,
// so the two copies can only disagree loudly. review-r3 MINOR: the shared
// `[0-9]` this list used to end in was a LIE against that claim — it took
// `turbo 9` and `difficulty 0`. Every field now carries its real domain,
// so this copy rejects exactly what judge-foh-trace.js's SVAL_DOM does.
const RE_S = new RegExp("^S " + NUM + " (" +
    "(?:p1char|p2char) [0-4]" +
    "|(?:p1difficulty|difficulty) [1-4]" +
    "|(?:turbo|flashlcancel|walljump|tapjump[1-4]) [01]" +
    "|lcancel [0-2]" +
    "|(?:p1type|p2type|carry) (?:-1|[01])" +
    "|(?:soundsvol|musicvol) (?:10|[0-9])" +
    "|refused [a-z0-9]+)$");
const RE_SHOT = new RegExp("^SHOT " + NUM + " ([a-z0-9-]{1,32})$");
// UNCHANGED by the CSS mechanics arc: the launch plane only supports a human
// port 0, so foh.c refuses any other port configuration and the record never
// needs p1type/p1difficulty columns (see judge-foh-trace.js's note).
// A27 widened `versus` from the literal 0 to [01] — the CSS mode ribbon now
// writes it (css.js:393). judge-foh-trace.js carries the full argument; the
// `rest` group is still the whole tail, so the normalized form is unchanged
// for every trace that does not touch the ribbon.
const RE_LAUNCH = new RegExp("^LAUNCH " + NUM + " (p1=[0-4] p2=[0-4] p2type=[01] difficulty=[1-4] stage=[0-5] turbo=[01] lcancel=[012] flashlcancel=[01] walljump=[01] tapjump=[01],[01],[01],[01] versus=[01])$");
// iter 99 (M4 task 12): the target-mode launch record — same
// END==launch-tick semantics as LAUNCH in bounded mode.
const RE_TLAUNCH = new RegExp("^TLAUNCH " + NUM + " (char=[0-4] tstage=[0-9])$");
const RE_END = new RegExp("^END " + NUM + " (transitions=" + NUM + ")$");

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
    if ((m = RE_TLAUNCH.exec(ln)) !== null) {
      out.lines.push({ kind: "TLAUNCH", tick: Number(m[1]), rest: m[2] });
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

// frozen per-flow input-free whitelist (iter 97, review-95 L-a):
// flows declared to have NO post-input observable event. EMPTY today —
// all 5 committed flows have measured anchors (76-91). Additions are
// reviewed changes, never inferred.
const INPUT_FREE_FLOWS = new Set([]);

if (process.argv[2] === "--bounded") {
  // ---- MODE 2: bounded-delta judgment ------------------------------------
  if (process.argv.length !== 7 &&
      !(process.argv.length === 8 && process.argv[7] === "input-free")) {
    console.error("usage: node normalize-foh-trace.js --bounded " +
                  "<frozen.expect> <device-trace> <flow> <end-max> " +
                  "[input-free]");
    process.exit(1);
  }
  const [, , , expPath, devPath, flowPath, endMaxArg] = process.argv;
  const declaredInputFree = process.argv.length === 8;
  if (!/^(0|[1-9][0-9]{0,6})$/.test(endMaxArg)) {
    die("end-max grammar: '" + endMaxArg + "'");
  }
  const endMax = Number(endMaxArg);

  // frozen bounds (measured iter 95 — header block; never loosen)
  const ID_SLACK = 2;
  const OFS_LO = 40;
  const OFS_HI = 240;
  const DEV_NEG = 90;
  const DEV_POS = 30;
  const LEAD_MS = 8200;
  // flow-to-fkscript.js's timing model, restated. It changed with the CSS
  // mechanics arc: the CSS cursor is LEVEL-driven (the hand integrates the
  // d-pad every frame it is held, css.js:195-196), so one flow frame is now
  // one DEVICE frame, not three. The old restatement here was `3F - 618`,
  // i.e. STEP_MS = 50; leaving it would have made this judge model a cadence
  // the injector no longer produces.
  //
  // The BOUNDS above (DEV_NEG/DEV_POS, measured on hardware at iter 95) are
  // deliberately NOT touched. They were measured under the 3x cadence and
  // this arc has no device to re-measure them on, so they stay exactly as
  // frozen: if the 1:1 cadence needs a different envelope, the device leg
  // FAILS LOUDLY and gets re-measured, which is the honest outcome.
  // Loosening them here to make a device run pass would be the defect this
  // whole judge exists to prevent.
  const FRAME_MS = 1000 / 60;
  const model = (F) =>
    Math.round(((LEAD_MS + Math.round((F - 370) * FRAME_MS)) * 60) / 1000);

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
      if ((m = new RegExp("^I " + NUM + " (-|[A-Z]+)$").exec(ln)) !== null) {
        if (m[2] !== "-" && firstInput === 0) firstInput = Number(m[1]);
        continue;
      }
      if (new RegExp("^SHOT " + NUM + " ([a-z0-9-]{1,32})$").test(ln)) continue;
      if (new RegExp("^END " + NUM + "$").test(ln)) continue;
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
    if (d.kind === "LAUNCH" || d.kind === "TLAUNCH") launchTick = d.tick;
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
  // anchor-null posture (iter 97, review-95 L-a): both directions bind
  const flowIdM = RE_HDR.exec(dev.hdr);
  const flowId = flowIdM ? flowIdM[1] : "";
  const inputFree = declaredInputFree || INPUT_FREE_FLOWS.has(flowId);
  if (anchor === null && !inputFree) {
    boundDie("no post-input observable event — anchored cadence " +
             "judgment is impossible and flow '" + flowId + "' is not " +
             "declared input-free (explicit whitelist/arg required)");
  }
  if (anchor !== null && inputFree) {
    die("declared input-free but event(s) anchored the cadence " +
        "(anchor=" + anchor + ") — stale declaration for flow '" +
        flowId + "'");
  }
  console.log("bounded OK " + devPath + " (events=" + exp.lines.length +
              ", anchor=" + (anchor === null ? "none(declared)" : anchor) +
              ")");
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
