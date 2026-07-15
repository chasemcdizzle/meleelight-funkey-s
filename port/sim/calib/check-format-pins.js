#!/usr/bin/env node
// check-format-pins.js — M2 task 15: asserts the measured-then-frozen
// pins in expected-format.json against the artifacts check-format.sh
// just produced. Modes:
//   adversarial <corpus.hex>   exact line count + sha256 (deterministic
//                              seeded generator — byte-stable)
//   captured <corpus.hex>      unique-double count >= the g01 cold-run
//                              baseline (more capture files only add)
//   composite <cases.txt>      exact case counts (V / E / 4-slot E /
//                              live-article envelopes) for the
//                              g01-derived composite file
"use strict";
const crypto = require("crypto");
const fs = require("fs");
const path = require("path");

const pins = JSON.parse(
  fs.readFileSync(path.join(__dirname, "expected-format.json"), "utf8"));

function die(msg) {
  console.error(`FORMAT PINS FAIL: ${msg}`);
  process.exit(1);
}

function countLines(p) {
  let n = 0;
  const fd = fs.openSync(p, "r");
  const buf = Buffer.alloc(1 << 22);
  for (;;) {
    const got = fs.readSync(fd, buf, 0, buf.length);
    if (got === 0) break;
    for (let i = 0; i < got; i++) if (buf[i] === 10) n++;
  }
  fs.closeSync(fd);
  return n;
}

const [mode, file] = process.argv.slice(2);
if (!mode || !file) die("usage: check-format-pins.js <mode> <file>");

if (mode === "adversarial") {
  const n = countLines(file);
  if (n !== pins.adversarial.count)
    die(`adversarial corpus count ${n} != pinned ${pins.adversarial.count}`);
  const h = crypto.createHash("sha256").update(fs.readFileSync(file)).digest("hex");
  if (h !== pins.adversarial.sha256)
    die(`adversarial corpus sha256 ${h} != pinned ${pins.adversarial.sha256}`);
  console.log(`adversarial pins OK: ${n} patterns, sha256 ${h.slice(0, 12)}…`);
} else if (mode === "captured") {
  const n = countLines(file);
  if (n < pins.capturedUniqueMin)
    die(`captured unique doubles ${n} < pinned minimum ${pins.capturedUniqueMin}`);
  console.log(`captured pins OK: ${n} unique doubles (>= ${pins.capturedUniqueMin})`);
} else if (mode === "composite") {
  let v = 0, e = 0, e4 = 0, live = 0;
  const rl = require("readline").createInterface({
    input: fs.createReadStream(file), crlfDelay: Infinity });
  rl.on("line", (line) => {
    if (line.startsWith("V\t")) v++;
    else if (line.startsWith("E\t")) {
      e++;
      const f = line.split("\t");
      if (f[1] === "0,0,0,0") e4++;
      if (f[6] !== "[]") live++;
    }
  });
  rl.on("close", () => {
    const c = pins.composite;
    if (v !== c.vCases) die(`composite V cases ${v} != pinned ${c.vCases}`);
    if (e !== c.eCases) die(`composite E cases ${e} != pinned ${c.eCases}`);
    if (e4 !== c.e4Cases) die(`composite 4-slot E cases ${e4} != pinned ${c.e4Cases}`);
    if (live !== c.liveArticleEnvelopes)
      die(`live-article envelopes ${live} != pinned ${c.liveArticleEnvelopes}`);
    console.log(`composite pins OK: ${v} V + ${e} E (${e4} four-slot, ${live} live-article)`);
  });
} else {
  die(`unknown mode ${mode}`);
}
