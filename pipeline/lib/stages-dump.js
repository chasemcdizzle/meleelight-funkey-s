#!/usr/bin/env node
"use strict";
// Fresh executed-JS walk of the extractor bundle's VS-stage registry ->
// canonical leaf dump (pipeline/FORMATS.md §4.5) on stdout.
// check-stages.sh compares this byte-for-byte against the compiled C
// tables' dump (stages_check.c): every emitted C value must be bit-equal
// to a FRESH execution of the real upstream stage modules — the fix_plan
// §M1 task 3 round-trip gate.
// Usage: node lib/stages-dump.js <upstream-clone-root>

const path = require("path");
const { loadStages, buildStageModel, dumpStages } = require("./stages-schema");

const distRoot = path.resolve(process.argv[2] || "");
if (!process.argv[2]) {
  console.error("usage: stages-dump.js <upstream-clone-root>");
  process.exit(1);
}

const { stages } = loadStages(distRoot);
const model = buildStageModel(stages);
const lines = [];
dumpStages(model, (line) => lines.push(line));
process.stdout.write(lines.join("\n") + "\n");
