#!/usr/bin/env node
// fmt-js-ref.js — M2 task 15: the JS reference side of the formatter
// differential. Reads 16-hex-digit IEEE-754 double bit patterns (one per
// line, big-endian — the canon d: encoding) and writes, per pattern:
//
//   String(x) TAB numStr(x) LF
//
// String(x) is V8's ECMA-262 Number::toString — THE reference the C
// formatter (port/sim/ml_fmt.c) must match byte-for-byte. numStr is the
// oracle's ser number rule (CHECKSUM.md §3.4): String(x) plus the
// explicit "-0" token. numStr is NOT transcribed here — it is extracted
// from oracle/harness/pagelib.js's own source bytes (pagelib.js:10-13)
// so the reference is the oracle's code, not a copy of it.
//
// Usage: node fmt-js-ref.js <in.hex> <out.txt>
"use strict";
const fs = require("fs");
const path = require("path");

function extractOracleSer() {
  const src = fs.readFileSync(
    path.join(__dirname, "..", "..", "..", "oracle", "harness", "pagelib.js"),
    "utf8");
  const a = src.indexOf("function numStr");
  const b = src.indexOf("// Checksum surface");
  if (a < 0 || b <= a) {
    throw new Error("pagelib.js markers not found — cannot extract oracle ser");
  }
  // Evaluate the oracle's own numStr+ser source (pagelib.js:10-37).
  return new Function(src.slice(a, b) + "\nreturn { numStr: numStr, ser: ser };")();
}

function main() {
  const [inPath, outPath] = process.argv.slice(2);
  if (!inPath || !outPath) {
    console.error("usage: node fmt-js-ref.js <in.hex> <out.txt>");
    process.exit(2);
  }
  const { numStr } = extractOracleSer();
  const buf8 = Buffer.alloc(8);
  const inFd = fs.openSync(inPath, "r");
  const outFd = fs.openSync(outPath, "w");
  const CHUNK = 1 << 22;
  const rbuf = Buffer.alloc(CHUNK);
  let carry = "";
  let outParts = [];
  let outBytes = 0;
  let n = 0;
  for (;;) {
    const got = fs.readSync(inFd, rbuf, 0, CHUNK);
    if (got === 0) break;
    const text = carry + rbuf.toString("latin1", 0, got);
    const lines = text.split("\n");
    carry = lines.pop(); // last element: partial line (or "")
    for (const line of lines) {
      if (line.length === 0) continue;
      if (!/^[0-9a-f]{16}$/.test(line)) {
        console.error(`fmt-js-ref: bad hex line ${n + 1}: ${line}`);
        process.exit(2);
      }
      buf8.write(line, 0, "hex");
      const x = buf8.readDoubleBE(0);
      outParts.push(String(x) + "\t" + numStr(x) + "\n");
      ++n;
      if (outParts.length >= 65536) {
        const s = outParts.join("");
        fs.writeSync(outFd, s);
        outBytes += Buffer.byteLength(s);
        outParts = [];
      }
    }
  }
  if (carry.length) {
    console.error("fmt-js-ref: input does not end in a newline");
    process.exit(2);
  }
  const s = outParts.join("");
  fs.writeSync(outFd, s);
  outBytes += Buffer.byteLength(s);
  fs.closeSync(inFd);
  fs.closeSync(outFd);
  console.log(`js-ref formatted ${n} patterns -> ${outPath} (${outBytes} bytes)`);
}

main();
