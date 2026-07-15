#!/usr/bin/env node
// build-ai-bridge.js — distill an ai-spec capture (<id>.ai.jsonl) into the
// replayable AIBRIDGE1 artifact the C bridge consumes (M2 task 16; format
// spec in port/sim/ai_bridge.h). Pure deterministic stream transform: the
// artifact is a function of the capture bytes alone (byte-stability is
// checked by running it twice and cmp-ing).
//
// Usage: node build-ai-bridge.js <golden-id> <capture.ai.jsonl> <out.txt>
"use strict";
const fs = require("fs");
const readline = require("readline");

const [id, jsonlPath, outPath] = process.argv.slice(2);
if (!id || !jsonlPath || !outPath) {
  console.error("usage: node build-ai-bridge.js <golden-id> <capture.ai.jsonl> <out.txt>");
  process.exit(1);
}

// canon key order (ai_bridge.h) — the artifact's field order
const FIELDS = ["a", "b", "csX", "csY", "dd", "dl", "dr", "du", "l", "lA",
                "lsX", "lsY", "r", "rA", "rawX", "rawY", "rawcsX", "rawcsY",
                "s", "x", "y", "z"];

const die = (m) => { console.error("BUILD-AI-BRIDGE FAIL: " + m); process.exit(1); };

const dv = new DataView(new ArrayBuffer(8));
function hexToDouble(hex) {
  dv.setBigUint64(0, BigInt("0x" + hex));
  return dv.getFloat64(0);
}

// canon scalar token -> artifact token (bool|number|undefined domain only)
function tok(line, v) {
  if (v === "T") return "B1";
  if (v === "F") return "B0";
  if (v === "undef") return "U";
  const m = /^d:([0-9a-f]{16})$/.exec(v);
  if (m) return "N" + m[1];
  die(`line ${line}: bank value outside the bool|number|undefined domain: ${v}`);
  return ""; // unreachable
}

async function main() {
  let seed = null, boot = null;
  const entries = [];
  const rl = readline.createInterface({
    input: fs.createReadStream(jsonlPath),
    crlfDelay: Infinity,
  });
  let lineNo = 0;
  for await (const l of rl) {
    lineNo++;
    if (l.length === 0) continue;
    const parts = l.split("\t");
    const fn = parts[1];
    if (fn === "rngBoot") {
      if (seed !== null) die("two rngBoot records");
      const m = /^\[d:([0-9a-f]{16}),d:([0-9a-f]{16})\]$/.exec(parts[2]);
      if (!m) die(`line ${lineNo}: bad rngBoot args`);
      seed = hexToDouble(m[1]);
      boot = hexToDouble(m[2]);
      if (seed < 0 || seed !== (seed >>> 0)) die("bad rngBoot seed");
      if (boot < 0 || !Number.isInteger(boot)) die("bad rngBoot count");
      continue;
    }
    if (fn !== "runAI") continue;
    if (parts.length !== 5) die(`line ${lineNo}: runAI record needs 5 fields`);
    const frame = Number(parts[0]);
    if (!Number.isInteger(frame) || frame < 1) die(`line ${lineNo}: bad runAI frame`);
    const am = /^\[d:([0-9a-f]{16})\]$/.exec(parts[2]);
    if (!am) die(`line ${lineNo}: bad runAI args`);
    const slot = hexToDouble(am[1]);
    if (!Number.isInteger(slot) || slot < 0 || slot > 3) die(`line ${lineNo}: bad runAI slot`);

    // post envelope: {"bank":{...},"bk":{...},"rng":[...]} — bank is FLAT
    // (scalar values only: the measured input-plane domain), rng is a flat
    // d:hex list; linear extraction, no general canon parser needed
    const post = parts[4];
    if (!post.startsWith('{"bank":{')) die(`line ${lineNo}: post envelope missing bank`);
    const bankEnd = post.indexOf("}", 9);
    if (bankEnd === -1) die(`line ${lineNo}: unterminated bank`);
    const bankBody = post.slice(9, bankEnd);
    const rngKey = ',"rng":[';
    const rngAt = post.indexOf(rngKey, bankEnd);
    if (rngAt === -1 || !post.endsWith("]}")) die(`line ${lineNo}: post envelope missing rng`);
    const rngBody = post.slice(rngAt + rngKey.length, post.length - 2);

    const fields = {};
    for (const kv of bankBody.split(",")) {
      const c = kv.indexOf(":");
      if (c === -1) die(`line ${lineNo}: bad bank field ${kv}`);
      const key = kv.slice(0, c);
      if (!/^"[A-Za-z]+"$/.test(key)) die(`line ${lineNo}: bad bank key ${key}`);
      fields[key.slice(1, -1)] = kv.slice(c + 1);
    }
    const keys = Object.keys(fields);
    if (keys.length !== 22 || FIELDS.some((k) => !(k in fields))) {
      die(`line ${lineNo}: bank key set is not the 22-key Input shape`);
    }

    const draws = [];
    if (rngBody.length > 0) {
      for (const d of rngBody.split(",")) {
        const m = /^d:([0-9a-f]{16})$/.exec(d);
        if (!m) die(`line ${lineNo}: bad rng entry ${d}`);
        draws.push(m[1]);
      }
    }

    entries.push(
      frame + " " + slot + " " + draws.length +
      draws.map((d) => " " + d).join("") +
      FIELDS.map((k) => " " + tok(lineNo, fields[k])).join(""));
  }
  if (seed === null) die("capture carries no rngBoot record");

  const outText =
    `AIBRIDGE1 ${id} seed=${seed} boot=${boot} entries=${entries.length}\n` +
    entries.map((e) => e + "\n").join("");
  fs.writeFileSync(outPath, outText);
  console.log(`${id}: ${entries.length} bridge entries -> ${outPath}`);
}

main().catch((e) => { console.error(e); process.exit(1); });
