#!/usr/bin/env node
"use strict";
// Fresh executed-JS walk of the extractor bundle's target-stage registry
// -> canonical leaf dump (pipeline/FORMATS.md §6) on stdout.
// check-targets.sh compares this byte-for-byte against the compiled C
// tables' dump (targets_check.c): every emitted C value must be bit-equal
// to a FRESH execution of the real upstream target stage modules — the
// fix_plan §M4 task 11 round-trip gate.
// Usage: node lib/targets-dump.js <upstream-clone-root>

const path = require("path");
const {
  loadTargetStages, buildTargetStageModel, dumpTargetStages,
} = require("./targets-schema");

const distRoot = path.resolve(process.argv[2] || "");
if (!process.argv[2]) {
  console.error("usage: targets-dump.js <upstream-clone-root>");
  process.exit(1);
}

const { tstages } = loadTargetStages(distRoot);
const model = buildTargetStageModel(tstages);
const lines = [];
dumpTargetStages(model, (line) => lines.push(line));
process.stdout.write(lines.join("\n") + "\n");
