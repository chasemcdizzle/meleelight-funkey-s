#!/usr/bin/env node
// fmt-composite.js — M2 task 15: the CHECKSUM.md `ser` composite
// differential. Two subcommands:
//
//   gen <player.jsonl> <article.jsonl> <out.txt>
//     Builds the shared case file BOTH sides serialize:
//       V TAB <canon>   — every player post-state snapshot and every
//                         article args/post envelope, verbatim from the
//                         captures (canon v1.1 text)
//       E TAB pt0,pt1,pt2,pt3 TAB p0 TAB p1 TAB p2 TAB p3 TAB articles
//                       — full CHECKSUM.md §2/§3.1 frame envelopes: per
//                         frame, the captured slot snapshots + that
//                         frame's post aArticles queue (extracted
//                         textually from the article capture; aArt IS
//                         CHECKSUM.md §2's `articles` key — FORMAT.md
//                         "The article spec"); every 100th frame also as
//                         a synthetic 4-slot envelope (exercises the
//                         p2/p3 emission arm; "-" = absent slot)
//
//   ref <in.txt> <out.txt>
//     The JS reference: parses each case's canon text back to live JS
//     values and serializes with THE ORACLE'S OWN CODE — V cases through
//     `ser` extracted from oracle/harness/pagelib.js source bytes
//     (pagelib.js:10-37), E cases through the actual
//     window.__serializeState (pagelib.js:41-64) under a shimmed
//     __harness, hashed by the actual window.__sha256 (pagelib.js:66-73,
//     WebCrypto). Emits "sha256hex TAB bytelen LF" per case — the same
//     shape fmt_diff --composite emits from the C side. Zero
//     transcription: any C/JS ser divergence, down to a single byte
//     anywhere in a ~10 KB frame envelope, flips the hash.
"use strict";
const fs = require("fs");
const path = require("path");
const readline = require("readline");

// --- canon v1.1 parser (FORMAT.md "canon v1.1") ------------------------------

const FN_VALUE = function () {}; // any function serializes as "fn"

function parseCanon(text) {
  let i = 0;
  const buf8 = Buffer.alloc(8);
  function fail(msg) {
    throw new Error(`canon parse: ${msg} at ${i}: ...${text.slice(Math.max(0, i - 20), i + 20)}...`);
  }
  function value() {
    const c = text[i];
    if (c === "d" && text[i + 1] === ":") {
      const hex = text.slice(i + 2, i + 18);
      if (!/^[0-9a-f]{16}$/.test(hex)) fail("bad number");
      i += 18;
      buf8.write(hex, 0, "hex");
      return buf8.readDoubleBE(0);
    }
    if (c === '"') {
      const j = text.indexOf('"', i + 1);
      if (j < 0) fail("unterminated string");
      const s = text.slice(i + 1, j);
      if (s.includes("\\")) fail("escape in string (outside canon domain)");
      i = j + 1;
      return s;
    }
    if (c === "T") { i++; return true; }
    if (c === "F") { i++; return false; }
    if (text.startsWith("null", i)) { i += 4; return null; }
    if (text.startsWith("undef", i)) { i += 5; return undefined; }
    if (text.startsWith("fn", i)) { i += 2; return FN_VALUE; }
    if (text.startsWith("cyc", i)) fail("cyc token (trees only in the capture domain)");
    if (c === "[") {
      i++;
      const arr = [];
      if (text[i] === "]") { i++; return arr; }
      for (;;) {
        arr.push(value());
        if (text[i] === ",") { i++; continue; }
        if (text[i] === "]") { i++; return arr; }
        fail("bad array");
      }
    }
    if (c === "{") {
      i++;
      const obj = Object.create(null); // safe for any key (e.g. __proto__)
      if (text[i] === "}") { i++; return obj; }
      for (;;) {
        if (text[i] !== '"') fail("bad key");
        const j = text.indexOf('"', i + 1);
        const k = text.slice(i + 1, j);
        if (k.includes("\\")) fail("escape in key");
        i = j + 1;
        if (text[i] !== ":") fail("missing colon");
        i++;
        Object.defineProperty(obj, k, {
          value: value(), enumerable: true, writable: true, configurable: true,
        });
        if (text[i] === ",") { i++; continue; }
        if (text[i] === "}") { i++; return obj; }
        fail("bad object");
      }
    }
    fail("bad value");
  }
  const v = value();
  if (i !== text.length) fail("trailing garbage");
  return v;
}

// --- textual subtree extraction (string-aware bracket matcher) ---------------

// Returns the canon text of obj[key] inside a canon OBJECT text, e.g. the
// `aArt` queue out of an article post envelope. Canon strings contain no
// escapes (parser above enforces the same), so a simple in-string flag is
// exact.
function extractField(text, key) {
  const marker = `"${key}":`;
  let idx = -1, inStr = false;
  for (let j = 0; j < text.length; j++) {
    const c = text[j];
    if (inStr) { if (c === '"') inStr = false; continue; }
    if (c === '"') {
      if (text.startsWith(marker, j)) { idx = j + marker.length; break; }
      inStr = true;
    }
  }
  if (idx < 0) return null;
  let depth = 0;
  inStr = false;
  for (let j = idx; j < text.length; j++) {
    const c = text[j];
    if (inStr) { if (c === '"') inStr = false; continue; }
    if (c === '"') inStr = true;
    else if (c === "[" || c === "{") depth++;
    else if (c === "]" || c === "}") {
      depth--;
      if (depth === 0) return text.slice(idx, j + 1);
    } else if (depth === 0 && (c === "," )) {
      return text.slice(idx, j); // scalar field
    }
  }
  throw new Error(`extractField(${key}): unbalanced`);
}

// --- the oracle's own serializer -------------------------------------------

function loadOracle() {
  const pagelibPath = path.join(__dirname, "..", "..", "..", "oracle", "harness", "pagelib.js");
  const src = fs.readFileSync(pagelibPath, "utf8");
  // The whole IIFE, executed with window === the global (the browser-parity
  // shim class, CLAUDE.md M1-task-4 note): attaches the REAL
  // __serializeState and __sha256.
  global.window = globalThis;
  new Function(src)();
  // Plus direct access to the internal ser/numStr (pagelib.js:10-37), for
  // value-level cases: the oracle's source bytes, not a transcription.
  const a = src.indexOf("function numStr");
  const b = src.indexOf("// Checksum surface");
  if (a < 0 || b <= a) throw new Error("pagelib.js markers not found");
  const { ser } = new Function(src.slice(a, b) + "\nreturn { numStr: numStr, ser: ser };")();
  return { ser };
}

// --- gen ---------------------------------------------------------------------

async function gen(playerPath, articlePath, outPath) {
  const out = fs.createWriteStream(outPath);
  const write = (s) => new Promise((res) => (out.write(s) ? res() : out.once("drain", res)));

  // player capture: <frame> TAB physics TAB [d:slot] TAB undef TAB <post>
  const bySlotFrame = new Map(); // frame -> {slot: postText}
  let nV = 0;
  {
    const rl = readline.createInterface({
      input: fs.createReadStream(playerPath), crlfDelay: Infinity });
    for await (const line of rl) {
      if (!line) continue;
      const f = line.split("\t");
      if (f.length !== 5) throw new Error(`player record with ${f.length} fields`);
      const frame = Number(f[0]);
      const slot = parseCanon(f[2])[0]; // args = [slot]
      await write(`V\t${f[4]}\n`);
      nV++;
      if (frame > 0) {
        let m = bySlotFrame.get(frame);
        if (!m) bySlotFrame.set(frame, (m = {}));
        m[slot] = f[4];
      }
    }
  }

  // article capture: V cases for args + post; per-frame LAST post aArt for
  // the envelope's articles key.
  const artByFrame = new Map(); // frame -> aArt canon text (last record wins)
  {
    const rl = readline.createInterface({
      input: fs.createReadStream(articlePath), crlfDelay: Infinity });
    for await (const line of rl) {
      if (!line) continue;
      const f = line.split("\t");
      const frame = Number(f[0]);
      await write(`V\t${f[2]}\n`);
      nV++;
      if (f.length >= 5) {
        await write(`V\t${f[4]}\n`);
        nV++;
        const aArt = extractField(f[4], "aArt");
        if (aArt && frame > 0) artByFrame.set(frame, aArt);
      }
    }
  }

  // frame envelopes: captured slot snapshots + that frame's aArt queue.
  let nE = 0, nE4 = 0, nEart = 0;
  let prev = null;
  const frames = [...bySlotFrame.keys()].sort((a, b) => a - b);
  for (const frame of frames) {
    const m = bySlotFrame.get(frame);
    const slots = Object.keys(m).map(Number).sort();
    const ptype = [-1, -1, -1, -1];
    const ps = ["-", "-", "-", "-"];
    for (const s of slots) { ptype[s] = 0; ps[s] = m[s]; }
    const articles = artByFrame.get(frame) || "[]";
    if (articles !== "[]") nEart++;
    await write(`E\t${ptype.join(",")}\t${ps.join("\t")}\t${articles}\n`);
    nE++;
    if (frame % 100 === 0 && prev && slots.length >= 2) {
      // synthetic 4-slot envelope: exercises the p2/p3 emission arm
      const ptype4 = [0, 0, 0, 0];
      const ps4 = [ps[slots[0]], ps[slots[1]], prev[0], prev[1]];
      await write(`E\t${ptype4.join(",")}\t${ps4.join("\t")}\t${articles}\n`);
      nE4++;
      if (articles !== "[]") nEart++;
    }
    if (slots.length >= 2) prev = [m[slots[0]], m[slots[1]]];
  }
  await new Promise((res) => out.end(res));
  console.log(
    `composite gen: ${nV} V + ${nE} E (${nEart} with live articles) + ${nE4} E4 -> ${outPath}`);
}

// --- ref ---------------------------------------------------------------------

async function ref(inPath, outPath) {
  const { ser } = loadOracle();
  const out = fs.createWriteStream(outPath);
  const write = (s) => new Promise((res) => (out.write(s) ? res() : out.once("drain", res)));
  const rl = readline.createInterface({
    input: fs.createReadStream(inPath), crlfDelay: Infinity });
  let n = 0;
  for await (const line of rl) {
    if (!line) continue;
    let s;
    if (line.startsWith("V\t")) {
      s = ser(parseCanon(line.slice(2)), new Set());
    } else if (line.startsWith("E\t")) {
      const f = line.split("\t");
      if (f.length !== 7) throw new Error(`bad E record (${f.length} fields)`);
      const ptype = f[1].split(",").map(Number);
      const players = [];
      for (let i = 0; i < 4; i++) players.push(f[2 + i] === "-" ? undefined : parseCanon(f[2 + i]));
      const articles = parseCanon(f[6]);
      window.__harness = {
        getPlayers: () => players,
        getPlayerType: () => ptype,
        getArticles: () => articles,
      };
      s = window.__serializeState(); // pagelib.js:41-64, the oracle itself
    } else {
      throw new Error(`bad case line: ${line.slice(0, 40)}`);
    }
    const h = await window.__sha256(s); // pagelib.js:66-73 (WebCrypto)
    await write(`${h}\t${Buffer.byteLength(s)}\n`);
    n++;
  }
  await new Promise((res) => out.end(res));
  console.log(`composite ref: ${n} cases -> ${outPath}`);
}

// --- main ----------------------------------------------------------------------

async function main() {
  const [cmd, ...args] = process.argv.slice(2);
  if (cmd === "gen" && args.length === 3) return gen(args[0], args[1], args[2]);
  if (cmd === "ref" && args.length === 2) return ref(args[0], args[1]);
  console.error(
    "usage: node fmt-composite.js gen <player.jsonl> <article.jsonl> <out.txt>\n" +
    "       node fmt-composite.js ref <in.txt> <out.txt>");
  process.exit(2);
}

main().catch((e) => { console.error(e); process.exit(1); });
