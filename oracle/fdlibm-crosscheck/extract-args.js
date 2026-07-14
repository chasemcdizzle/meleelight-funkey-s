#!/usr/bin/env node
// extract-args.js — pull the recorded Math call stream out of a harness
// run made with --capture-math.
//
// The harness records every shimmed Math call as a crosscheck-format line
// "<fn> <hex16> [<hex16>] -> <hex16>" (args + the value the in-browser
// shim returned). This writes:
//   <args-out>    the argument stream (lines without "-> ..."), feedable
//                 to csweep / jssweep
//   <browser-out> the full lines (browser-observed outputs), byte-
//                 comparable with the csweep/jssweep replay outputs
// and asserts the stream actually exercises the surface (golden #1 uses
// sin, cos, atan, atan2, pow; tan is legitimately 0 until the AI port).
//
// Usage: node extract-args.js <run.json> <args-out> <browser-out>
"use strict";
const fs = require("fs");

const [runFile, argsOut, browserOut] = process.argv.slice(2);
if (!runFile || !argsOut || !browserOut) {
  console.error("usage: extract-args.js <run.json> <args-out> <browser-out>");
  process.exit(2);
}
const run = JSON.parse(fs.readFileSync(runFile, "utf8"));
const cap = run.mathCapture;
if (!Array.isArray(cap) || cap.length === 0) {
  console.error("run has no mathCapture (was --capture-math passed?)");
  process.exit(1);
}
const counts = {};
const args = [];
for (const line of cap) {
  const fn = line.slice(0, line.indexOf(" "));
  counts[fn] = (counts[fn] || 0) + 1;
  args.push(line.slice(0, line.indexOf(" ->")));
}
for (const fn of ["sin", "cos", "atan", "atan2", "pow"]) {
  if (!(counts[fn] > 0)) {
    console.error(`golden stream never calls ${fn} — capture broken?`);
    process.exit(1);
  }
}
fs.writeFileSync(argsOut, args.join("\n") + "\n");
fs.writeFileSync(browserOut, cap.join("\n") + "\n");
console.log(`${cap.length} captured calls:`,
  Object.keys(counts).sort().map((k) => `${k}=${counts[k]}`).join(" "));
