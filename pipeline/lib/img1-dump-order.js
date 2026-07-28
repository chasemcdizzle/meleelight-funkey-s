#!/usr/bin/env node
"use strict";
// Assert that the IMG1 directory ORDER a dump exhibits equals the frozen pin
// expected-assets.json assets.directory.
// Usage: node lib/img1-dump-order.js <dump-file> [--expect-mismatch]
//
// WHITELIST GRAMMAR (docs/PROCESS.md §3; review-a9-4 [M]): the first version
// filtered lines with startsWith("img ") and split off one whitespace token,
// which silently ignores malformed records, extra tokens and unrelated
// trailing data. Every line of the dump is validated here against an
// anchored grammar — `IMG1 count=<n>`, `img <i> <name> <w> <h>` with <i>
// equal to its position, or `row <i> <y> <hex> <hex>` — and anything else is
// a hard failure. The comparison is then over the complete parsed sequence.

const fs = require("fs");
const path = require("path");

const dumpFile = process.argv[2];
const expectMismatch = process.argv.includes("--expect-mismatch");
if (!dumpFile) {
  console.error("usage: img1-dump-order.js <dump-file> [--expect-mismatch]");
  process.exit(2);
}
const die = (msg) => { console.error("IMG1-ORDER-FAIL: " + msg); process.exit(2); };

const exp = JSON.parse(fs.readFileSync(
  path.join(__dirname, "..", "expected-assets.json"), "utf8"));
const want = exp.assets.directory;
if (!Array.isArray(want) || want.length === 0) die("pinned directory is empty");

const text = fs.readFileSync(dumpFile, "utf8");
if (!text.endsWith("\n")) die("dump does not end with a newline");
const lines = text.slice(0, -1).split("\n");
if (lines.length === 0) die("empty dump");

// Canonical decimals only — no leading zeros — so one value has exactly one
// spelling (review-a9-5 [L]).
const NUM = "(0|[1-9][0-9]*)";
const mHdr = new RegExp(`^IMG1 count=${NUM}$`).exec(lines[0]);
if (!mHdr) die(`line 1 is not the IMG1 header: ${JSON.stringify(lines[0])}`);
const count = Number(mHdr[1]);

const got = [];
const dims = [];
let expectRows = 0, rowY = 0, rowImg = 0;
for (let n = 1; n < lines.length; n++) {
  const line = lines[n];
  const mImg = new RegExp(`^img ${NUM} ([a-z0-9_]+) ${NUM} ${NUM}$`).exec(line);
  if (mImg) {
    // Dimensions are positive u16 — img1_open rejects zero, so a dump
    // claiming 0x0 (and therefore no rows at all) cannot be genuine.
    const iw = Number(mImg[3]), ih = Number(mImg[4]);
    if (iw < 1 || iw > 65535 || ih < 1 || ih > 65535) {
      die(`line ${n + 1}: image dimensions ${iw}x${ih} outside 1..65535`);
    }
    if (expectRows !== 0) {
      die(`line ${n + 1}: new image record while ${expectRows} row(s) of the ` +
        `previous image are still outstanding`);
    }
    if (Number(mImg[1]) !== got.length) {
      die(`line ${n + 1}: image index ${mImg[1]} != its position ${got.length}`);
    }
    got.push(mImg[2]);
    dims.push([Number(mImg[3]), Number(mImg[4])]);
    rowImg = Number(mImg[1]);
    expectRows = Number(mImg[4]);
    rowY = 0;
    continue;
  }
  const mRow = new RegExp(`^row ${NUM} ${NUM} ([0-9a-f]+) ([0-9a-f]+)$`).exec(line);
  if (mRow) {
    if (expectRows <= 0) die(`line ${n + 1}: row record outside any image`);
    const [w] = dims[dims.length - 1];
    if (Number(mRow[1]) !== rowImg) {
      die(`line ${n + 1}: row image ${mRow[1]} != current image ${rowImg}`);
    }
    if (Number(mRow[2]) !== rowY) {
      die(`line ${n + 1}: row y ${mRow[2]} != expected ${rowY}`);
    }
    if (mRow[3].length !== w * 4) {
      die(`line ${n + 1}: pixel field is ${mRow[3].length} hex chars, expected ${w * 4}`);
    }
    if (mRow[4].length !== w * 2) {
      die(`line ${n + 1}: alpha field is ${mRow[4].length} hex chars, expected ${w * 2}`);
    }
    rowY++;
    expectRows--;
    continue;
  }
  die(`line ${n + 1} matches no permitted record: ${JSON.stringify(line)}`);
}
if (expectRows !== 0) die(`dump ends with ${expectRows} row(s) outstanding`);
if (got.length !== count) {
  die(`header says count=${count} but ${got.length} image record(s) follow`);
}

const same = JSON.stringify(got) === JSON.stringify(want);
if (expectMismatch) {
  // A PERMUTATION, specifically (review-a9-6 [L]): accepting "any differing
  // sequence" would let a structurally impossible dump — an empty one, or
  // one missing images — stand in for the reordering the tooth is meant to
  // demonstrate. Same length, same name set, different order.
  if (got.length !== want.length) {
    die(`tooth: perturbed dump has ${got.length} image(s), expected ${want.length}`);
  }
  if (new Set(got).size !== got.length) die("tooth: perturbed dump has duplicate names");
  if (JSON.stringify([...got].sort()) !== JSON.stringify([...want].sort())) {
    die("tooth: perturbed dump is not a PERMUTATION of the pinned names " +
      `(${JSON.stringify([...got].sort())} vs ${JSON.stringify([...want].sort())})`);
  }
  if (same) {
    console.error("IMG1-ORDER-FAIL: expected a directory-order MISMATCH, but " +
      "the dump matches the frozen pin");
    process.exit(1);
  }
  console.log(`directory-order tooth: a PERMUTATION of the pinned names ` +
    `(${JSON.stringify(got.slice(0, 2))}...) does NOT satisfy the frozen index space`);
  process.exit(0);
}
if (!same) {
  console.error("IMG1-ORDER-FAIL: directory order " + JSON.stringify(got) +
    " != pinned " + JSON.stringify(want));
  process.exit(1);
}
console.log(`directory order: ${got.length} names match the frozen index space ` +
  `(every dump line validated against the anchored grammar)`);
