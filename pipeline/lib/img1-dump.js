#!/usr/bin/env node
"use strict";
// Canonical IMG1 dump from the JS reader — the differential partner of
// `port/gfx/img1_check --dump` (FORMATS.md §7.4).
// Usage: node lib/img1-dump.js <file.img1>
const fs = require("fs");
const { dumpImg1 } = require("./img1");
const file = process.argv[2];
if (!file) { console.error("usage: img1-dump.js <file.img1>"); process.exit(2); }
process.stdout.write(dumpImg1(fs.readFileSync(file), file));
