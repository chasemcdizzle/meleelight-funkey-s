#!/usr/bin/env node
"use strict";
// Fresh executed-JS walk of the extractor bundle -> canonical leaf dump
// (pipeline/FORMATS.md §3.6) on stdout. check-tables.sh compares this
// byte-for-byte against the compiled C tables' dump (tables_check.c):
// every emitted C value must be bit-equal to a FRESH execution of the
// real upstream modules — the fix_plan §M1 task 2 round-trip gate.
// Usage: node lib/tables-dump.js <upstream-clone-root>

const path = require("path");
const { loadTables, buildModel, dumpModel } = require("./tables-schema");

const distRoot = path.resolve(process.argv[2] || "");
if (!process.argv[2]) {
  console.error("usage: tables-dump.js <upstream-clone-root>");
  process.exit(1);
}

const { tables } = loadTables(distRoot);
const model = buildModel(tables);
const lines = [];
dumpModel(model, (line) => lines.push(line));
process.stdout.write(lines.join("\n") + "\n");
