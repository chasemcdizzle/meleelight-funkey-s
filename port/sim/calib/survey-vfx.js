// survey-vfx.js — standing capture-FIRST instrument (M4 task 1, rule 7:
// measure the real vfx config shapes from a fresh capture BEFORE
// finalizing the C value model). Streams a capture JSONL, extracts every
// vfx list from post envelopes ("vfx":[<canon>,...]), and prints the
// distinct config KEY SETS with per-key token classes + counts.
//
// Usage: node survey-vfx.js <capture.jsonl> [more.jsonl ...]
//
// Canon tokens (canon v1.1, capturelib.js): numbers "d:<hex16>", strings
// JSON, T/F, undef, fn, cyc, null, arrays [..], objects {"k":v,..} with
// sorted keys. This parser is a survey tool, NOT a judge — it still
// hard-fails on any token it does not recognize (fail loud, PROCESS §3).
"use strict";
const fs = require("fs");
const readline = require("readline");

function parseCanon(s, pos) {
  // returns [value, nextPos]; value: {t:"num"|"str"|"bool"|"undef"|"fn"|
  // "cyc"|"null"|"arr"|"obj", v?, items?, keys?}
  if (s.startsWith("d:", pos)) return [{ t: "num" }, pos + 18];
  const c = s[pos];
  if (c === '"') {
    let j = pos + 1;
    while (j < s.length && s[j] !== '"') {
      if (s[j] === "\\") j++;
      j++;
    }
    return [{ t: "str", v: s.slice(pos + 1, j) }, j + 1];
  }
  if (c === "T" || c === "F") return [{ t: "bool" }, pos + 1];
  if (s.startsWith("undef", pos)) return [{ t: "undef" }, pos + 5];
  if (s.startsWith("null", pos)) return [{ t: "null" }, pos + 4];
  if (s.startsWith("fn", pos)) return [{ t: "fn" }, pos + 2];
  if (s.startsWith("cyc", pos)) return [{ t: "cyc" }, pos + 3];
  if (c === "[") {
    const items = [];
    let j = pos + 1;
    if (s[j] === "]") return [{ t: "arr", items }, j + 1];
    for (;;) {
      const [v, nj] = parseCanon(s, j);
      items.push(v);
      j = nj;
      if (s[j] === ",") { j++; continue; }
      if (s[j] === "]") return [{ t: "arr", items }, j + 1];
      throw new Error("survey-vfx: bad array at " + j + ": " + s.slice(j, j + 40));
    }
  }
  if (c === "{") {
    const keys = [], vals = [];
    let j = pos + 1;
    if (s[j] === "}") return [{ t: "obj", keys, vals }, j + 1];
    for (;;) {
      if (s[j] !== '"') throw new Error("survey-vfx: bad key at " + j);
      let k = j + 1;
      while (s[k] !== '"') k++;
      keys.push(s.slice(j + 1, k));
      if (s[k + 1] !== ":") throw new Error("survey-vfx: missing colon at " + k);
      const [v, nj] = parseCanon(s, k + 2);
      vals.push(v);
      j = nj;
      if (s[j] === ",") { j++; continue; }
      if (s[j] === "}") return [{ t: "obj", keys, vals }, j + 1];
      throw new Error("survey-vfx: bad object at " + j + ": " + s.slice(j, j + 40));
    }
  }
  throw new Error("survey-vfx: unknown token at " + pos + ": " + s.slice(pos, pos + 40));
}

function classify(v) {
  if (v.t === "obj") {
    return "{" + v.keys.map((k, i) => k + ":" + classify(v.vals[i])).join(",") + "}";
  }
  if (v.t === "arr") return "[" + v.items.map(classify).join(",") + "]";
  if (v.t === "str") return "str";
  return v.t;
}

async function main() {
  const files = process.argv.slice(2);
  if (!files.length) {
    console.error("usage: node survey-vfx.js <capture.jsonl> [...]");
    process.exit(2);
  }
  const shapes = new Map(); // shape -> {count, names:Set}
  let vfxTotal = 0, recordsWithVfx = 0, lines = 0;
  for (const file of files) {
    const rl = readline.createInterface({
      input: fs.createReadStream(file), crlfDelay: Infinity,
    });
    for await (const line of rl) {
      lines++;
      // vfx lists live only in tab field 5 (post) — find the marker there.
      const tab4 = line.split("\t");
      if (tab4.length < 5) continue;
      const post = tab4[4];
      const m = post.indexOf('"vfx":[');
      if (m === -1) continue;
      const [arr, end] = parseCanon(post, m + 6);
      if (arr.t !== "arr") throw new Error("vfx field not an array");
      void end;
      if (arr.items.length) recordsWithVfx++;
      for (const cfg of arr.items) {
        vfxTotal++;
        const shape = classify(cfg);
        const name = cfg.t === "obj" && cfg.keys.indexOf("name") !== -1
          ? cfg.vals[cfg.keys.indexOf("name")].v : "<non-obj>";
        if (!shapes.has(shape)) shapes.set(shape, { count: 0, names: new Set() });
        const e = shapes.get(shape);
        e.count++;
        e.names.add(String(name));
      }
    }
  }
  console.log("lines scanned: " + lines);
  console.log("records with non-empty vfx: " + recordsWithVfx);
  console.log("vfx configs total: " + vfxTotal);
  console.log("distinct shapes: " + shapes.size);
  const sorted = [...shapes.entries()].sort((a, b) => b[1].count - a[1].count);
  for (const [shape, e] of sorted) {
    console.log(String(e.count).padStart(7) + "  " + shape);
    console.log("         names: " + [...e.names].sort().join(", "));
  }
}
main().catch((e) => { console.error(e.stack || String(e)); process.exit(1); });
