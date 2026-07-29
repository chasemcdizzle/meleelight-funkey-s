#!/usr/bin/env node
// port/gfx/parse-bg-stderr.js — WHOLE-FILE grammar for gfx_replay's stderr
// (review-u1 r4 M3).
//
// The background type the renderer selected is decision evidence, so it is
// parsed fail-closed: not "find a line that matches" (which ignores
// malformed neighbours, accepts an unterminated final line, and lets
// unrelated warnings ride along), but "every line must be one of the forms
// this producer is allowed to emit, in order, exactly once each".
//
// Usage: node parse-bg-stderr.js <stderr file> <g01|g04>
// Prints the selected type (0 or 1) on stdout; exit 0 only if the whole
// file matches the expected grammar.
"use strict";

const fs = require("fs");

const file = process.argv[2];
const which = process.argv[3];
if (!file || (which !== "g01" && which !== "g04")) {
  console.error("usage: parse-bg-stderr.js <stderr file> <g01|g04>");
  process.exit(1);
}

// Canonical decimals only (review-u1 r5 L1): `00` and `007` are not
// forms this producer can emit, so accepting them would accept evidence
// no run could have produced.
const N = "(?:0|[1-9][0-9]*)";
const TIMING = new RegExp(`^render-only ns: avg=${N} p50=${N} p99=${N} max=${N} \\(n=${N}, host\\)$`);
const TYPE = /^bg selected backgroundType ([01])$/;
const TUNNEL = new RegExp(`^bg tunnel leg: ${N} frames, ${N} sampled$`);

// exact ordered producer grammar per invocation
const GRAMMAR = which === "g01"
  ? [TIMING, TYPE, TUNNEL]
  : [TIMING, TYPE];

const txt = fs.readFileSync(file, "utf8");
if (txt === "" || !txt.endsWith("\n")) {
  console.error(`parse-bg-stderr: ${file}: empty, or final line is unterminated ` +
    "(truncated evidence is not valid evidence)");
  process.exit(1);
}
const lines = txt.slice(0, -1).split("\n");
if (lines.length !== GRAMMAR.length) {
  console.error(`parse-bg-stderr: ${file}: ${lines.length} line(s), the ${which} ` +
    `producer emits exactly ${GRAMMAR.length}`);
  process.exit(1);
}
let type = null;
for (let i = 0; i < GRAMMAR.length; i++) {
  const m = GRAMMAR[i].exec(lines[i]);
  if (!m) {
    console.error(`parse-bg-stderr: ${file}: line ${i + 1} does not match the ` +
      `${which} grammar: '${lines[i]}'`);
    process.exit(1);
  }
  if (GRAMMAR[i] === TYPE) type = m[1];
}
if (type === null) {
  console.error(`parse-bg-stderr: ${file}: no backgroundType line`);
  process.exit(1);
}
process.stdout.write(type);
