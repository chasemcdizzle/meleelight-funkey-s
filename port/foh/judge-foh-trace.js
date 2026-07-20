#!/usr/bin/env node
// judge-foh-trace.js — FOHTRACE1 whitelist-grammar judge (fix_plan §M4
// task 9; PROCESS §3 whitelist-grammar rule). Decision-bearing parser:
// anchored FULL-LINE patterns measured from the producer (foh_app.c's
// exact fprintf grammars), binary outcome — resembles-but-doesn't-match
// is CORRUPTION, fail closed. Validated against the full genuine corpus
// (the 4 committed flow traces) with zero false rejections before
// shipping (check-foh-flows.sh leg [3]).
//
// STRUCTURE enforced beyond per-line grammar:
//   - header `FOHTRACE1 flow=<id>` with the EXPECTED id (argv);
//   - event frames non-decreasing; END is the last line, exactly once;
//   - T-chain continuity: first T departs `startup`, every T departs
//     the previous T's destination (a screen machine cannot teleport);
//   - every T edge is in the PINNED FLOW GRAPH (the faithful edge set,
//     upstream citations in port/foh/foh.h) — an off-graph transition
//     is corruption, never new behavior;
//   - S field/value domains pinned (chars 0-4, p2type 0/1, difficulty
//     1-4 = the upstream slider domain, lcancel 0-2, tapjump/turbo 0/1,
//     refused entries = the 9 registered tokens);
//   - LAUNCH at most once, required/forbidden per argv, and IMMEDIATELY
//     preceded by its `T <f> sss match launch` line at the same frame;
//   - END transitions= count == the number of T lines.
//
// Usage: node judge-foh-trace.js <trace.txt> <flow-id> <launch: 0|1>
"use strict";

const fs = require("fs");

function die(msg) {
  console.error("judge-foh-trace: CORRUPT: " + msg);
  process.exit(2);
}

if (process.argv.length !== 5) {
  console.error(
      "usage: node judge-foh-trace.js <trace.txt> <flow-id> <launch 0|1>");
  process.exit(1);
}
const [, , path, flowId, launchArg] = process.argv;
if (!/^[a-z0-9-]+$/.test(flowId)) die("flow id argv fails [a-z0-9-]+");
if (launchArg !== "0" && launchArg !== "1") die("launch argv must be 0|1");
const wantLaunch = launchArg === "1";

// The pinned faithful edge set (foh.h flow graph; upstream citations
// there). ANY other (from,to,cause) triple is corruption.
const EDGES = new Set([
  "startup>title>timer",
  "title>menu-top>start",
  "menu-top>menu-battle>a",
  "menu-top>menu-options>a",
  "menu-battle>css>a",
  "menu-battle>menu-top>b",
  "menu-options>options-gameplay>a",
  "menu-options>menu-controls>a",
  "menu-options>menu-top>b",
  "menu-controls>menu-options>b",
  "options-gameplay>menu-options>b",
  "css>sss>start",
  "css>menu-battle>bhold",
  "sss>css>b",
  "sss>match>launch",
]);
const REFUSED = new Set([
  "targettest", "targetbuilder", "audio", "credits", "controller",
  "keyboard", "spectate", "p2p", "server",
]);

const RE_T = /^T ([0-9]+) ([a-z-]+) ([a-z-]+) (timer|start|a|b|bhold|launch)$/;
const RE_S_NUM =
    /^S ([0-9]+) (p1char|p2char|p2type|difficulty|turbo|lcancel|tapjump[1-4]) ([0-9])$/;
const RE_S_REF = /^S ([0-9]+) refused ([a-z0-9]+)$/;
const RE_SHOT = /^SHOT ([0-9]+) ([a-z0-9-]{1,32})$/;
const RE_LAUNCH =
    /^LAUNCH ([0-9]+) p1=([0-4]) p2=([0-4]) p2type=([01]) difficulty=([1-4]) stage=([0-5]) turbo=([01]) lcancel=([012]) tapjump=([01]),([01]),([01]),([01]) versus=0$/;
const RE_END = /^END ([0-9]+) transitions=([0-9]+)$/;

const raw = fs.readFileSync(path, "utf8");
if (raw.length === 0) die("empty trace");
if (!raw.endsWith("\n")) die("missing trailing newline (torn write)");
const lines = raw.slice(0, -1).split("\n");
if (lines[0] !== "FOHTRACE1 flow=" + flowId) {
  die("header line is not exactly 'FOHTRACE1 flow=" + flowId + "': '" +
      lines[0] + "'");
}

let lastFrame = 0;
let lastScreen = "startup";
let tCount = 0;
let sawEnd = false;
let launches = 0;
let prevWasLaunchT = false;
let prevLaunchFrame = -1;
const shotNames = new Set();

const SVAL_DOM = {
  p1char: [0, 4], p2char: [0, 4], p2type: [0, 1], difficulty: [1, 4],
  turbo: [0, 1], lcancel: [0, 2],
  tapjump1: [0, 1], tapjump2: [0, 1], tapjump3: [0, 1], tapjump4: [0, 1],
};

for (let k = 1; k < lines.length; k++) {
  const ln = lines[k];
  if (sawEnd) die("content after END at line " + (k + 1));
  let m;
  if ((m = RE_T.exec(ln)) !== null) {
    const f = Number(m[1]);
    if (f < lastFrame) die("T frame regressed at line " + (k + 1));
    lastFrame = f;
    if (m[2] !== lastScreen) {
      die("T-chain break at line " + (k + 1) + ": departs '" + m[2] +
          "' but the machine is on '" + lastScreen + "'");
    }
    const edge = m[2] + ">" + m[3] + ">" + m[4];
    if (!EDGES.has(edge)) die("off-graph transition '" + edge + "' at line " + (k + 1));
    lastScreen = m[3];
    tCount++;
    prevWasLaunchT = m[4] === "launch";
    prevLaunchFrame = f;
    continue;
  }
  if ((m = RE_LAUNCH.exec(ln)) !== null) {
    const f = Number(m[1]);
    if (!prevWasLaunchT || f !== prevLaunchFrame) {
      die("LAUNCH at line " + (k + 1) +
          " is not immediately preceded by its 'T <f> sss match launch'");
    }
    if (f < lastFrame) die("LAUNCH frame regressed at line " + (k + 1));
    lastFrame = f;
    launches++;
    if (launches > 1) die("multiple LAUNCH lines");
    prevWasLaunchT = false;
    continue;
  }
  prevWasLaunchT = false;
  if ((m = RE_S_NUM.exec(ln)) !== null) {
    const f = Number(m[1]);
    const v = Number(m[3]);
    if (f < lastFrame) die("S frame regressed at line " + (k + 1));
    lastFrame = f;
    const dom = SVAL_DOM[m[2]];
    if (v < dom[0] || v > dom[1]) {
      die("S value " + v + " outside the pinned domain of " + m[2] +
          " at line " + (k + 1));
    }
    continue;
  }
  if ((m = RE_S_REF.exec(ln)) !== null) {
    const f = Number(m[1]);
    if (f < lastFrame) die("S frame regressed at line " + (k + 1));
    lastFrame = f;
    if (!REFUSED.has(m[2])) {
      die("unregistered refused entry '" + m[2] + "' at line " + (k + 1));
    }
    continue;
  }
  if ((m = RE_SHOT.exec(ln)) !== null) {
    const f = Number(m[1]);
    if (f < lastFrame) die("SHOT frame regressed at line " + (k + 1));
    lastFrame = f;
    if (shotNames.has(m[2])) die("duplicate SHOT name at line " + (k + 1));
    shotNames.add(m[2]);
    continue;
  }
  if ((m = RE_END.exec(ln)) !== null) {
    const f = Number(m[1]);
    if (f < lastFrame) die("END frame below the last event frame");
    if (Number(m[2]) !== tCount) {
      die("END transitions=" + m[2] + " but " + tCount + " T lines counted");
    }
    sawEnd = true;
    continue;
  }
  // Resemblance = same leading keyword, failed full grammar: corruption
  // by construction (no partial parses, no silent skips).
  die("line " + (k + 1) + " matches no FOHTRACE1 form: '" + ln + "'");
}
if (!sawEnd) die("no END line (truncated trace)");
if (wantLaunch !== (launches === 1)) {
  die("launch expectation: want " + (wantLaunch ? 1 : 0) + ", saw " + launches);
}
console.log("FOH TRACE GRAMMAR OK " + flowId + " (transitions=" + tCount +
            ", shots=" + shotNames.size + ", launch=" + launches + ")");
