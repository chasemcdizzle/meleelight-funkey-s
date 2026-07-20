#!/usr/bin/env node
// normalize-foh-trace.js — FOHTRACE1 frame-field ELISION (fix_plan §M4
// task 10; pre-registration AGENT-LOG iter 93).
//
// The device FOH legs are driven by wall-clock fk_input injection, so
// event TICK NUMBERS cannot be frame-exact against the frozen host
// traces — every STRUCTURAL fact can: edges (from/to/cause), S
// field/values, LAUNCH parameters, SHOT names, line ORDER, and the END
// transitions count. This tool rewrites a FOHTRACE1 file with the
// frame field of every event line replaced by the literal 'F', and
// NOTHING else; the check runs BOTH the frozen .expect and the device
// trace through it and requires byte equality of the outputs (frozen
// bytes are never edited).
//
// Whitelist posture (PROCESS §3): every line must match one of the
// exact FOHTRACE1 forms (the judge-foh-trace.js patterns); anything
// that merely resembles one — or any unknown line — is corruption:
// loud death, exit 2, no partial output.
//
// Usage: node normalize-foh-trace.js <trace.txt> <out.txt>
"use strict";

const fs = require("fs");

function die(msg) {
  console.error("normalize-foh-trace: CORRUPT: " + msg);
  process.exit(2);
}

if (process.argv.length !== 4) {
  console.error("usage: node normalize-foh-trace.js <trace.txt> <out.txt>");
  process.exit(1);
}
const [, , inPath, outPath] = process.argv;

const RE_HDR = /^FOHTRACE1 flow=([a-z0-9-]+)$/;
const RE_T = /^T ([0-9]+) ([a-z-]+ [a-z-]+ (?:timer|start|a|b|bhold|launch))$/;
const RE_S = /^S ([0-9]+) ((?:p1char|p2char|p2type|difficulty|turbo|lcancel|tapjump[1-4]) [0-9]|refused [a-z0-9]+)$/;
const RE_SHOT = /^SHOT ([0-9]+) ([a-z0-9-]{1,32})$/;
const RE_LAUNCH = /^LAUNCH ([0-9]+) (p1=[0-4] p2=[0-4] p2type=[01] difficulty=[1-4] stage=[0-5] turbo=[01] lcancel=[012] tapjump=[01],[01],[01],[01] versus=0)$/;
const RE_END = /^END ([0-9]+) (transitions=[0-9]+)$/;

const raw = fs.readFileSync(inPath, "utf8");
if (raw.length === 0) die("empty trace");
if (!raw.endsWith("\n")) die("missing trailing newline (torn write)");
const lines = raw.slice(0, -1).split("\n");
if (!RE_HDR.test(lines[0])) die("bad header: '" + lines[0] + "'");

const out = [lines[0]];
let sawEnd = false;
for (let k = 1; k < lines.length; k++) {
  const ln = lines[k];
  if (sawEnd) die("content after END at line " + (k + 1));
  let m;
  if ((m = RE_T.exec(ln)) !== null) { out.push("T F " + m[2]); continue; }
  if ((m = RE_S.exec(ln)) !== null) { out.push("S F " + m[2]); continue; }
  if ((m = RE_SHOT.exec(ln)) !== null) { out.push("SHOT F " + m[2]); continue; }
  if ((m = RE_LAUNCH.exec(ln)) !== null) { out.push("LAUNCH F " + m[2]); continue; }
  if ((m = RE_END.exec(ln)) !== null) {
    out.push("END F " + m[2]);
    sawEnd = true;
    continue;
  }
  die("line " + (k + 1) + " matches no FOHTRACE1 form: '" + ln + "'");
}
if (!sawEnd) die("no END line (truncated trace)");
fs.writeFileSync(outPath, out.join("\n") + "\n");
