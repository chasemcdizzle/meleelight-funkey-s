#!/usr/bin/env node
// survey-shapes.js — capture-FIRST instrument (prevention rule 7): read the
// REAL value shapes out of a capture JSONL before finalizing a C value
// model. Walks every record's canon fields and reports, per structural
// path (array indices collapsed to []):
//   - value types seen (with counts), undef occurrences
//   - for objects: every distinct sorted key-set signature (with counts)
//   - for arrays: length min/max
//   - for strings: the value set (when small)
// Usage: node survey-shapes.js <capture.jsonl>... [--field args|ret|post]
//        (default field: post if present, else ret)
"use strict";
const fs = require("fs");

const files = [];
let fieldSel = null;
const argv = process.argv.slice(2);
for (let i = 0; i < argv.length; i++) {
  if (argv[i] === "--field") fieldSel = argv[++i];
  else files.push(argv[i]);
}
if (files.length === 0) {
  console.error("usage: node survey-shapes.js <capture.jsonl>... [--field args|ret|post]");
  process.exit(1);
}

// --- canon parser (grammar: port/sim/calib/FORMAT.md "canon v1.1") --------
function parseCanon(s) {
  let i = 0;
  function fail(msg) { throw new Error(`canon parse: ${msg} at ${i}: ...${s.slice(i, i + 40)}`); }
  function lit(tok, val) { i += tok.length; return val; }
  function value() {
    const c = s[i];
    if (c === "n" && s.startsWith("null", i)) return lit("null", null);
    if (c === "u" && s.startsWith("undef", i)) return lit("undef", { $undef: true });
    if (c === "f" && s.startsWith("fn", i)) return lit("fn", { $fn: true });
    if (c === "c" && s.startsWith("cyc", i)) return lit("cyc", { $cyc: true });
    if (c === "T") return lit("T", true);
    if (c === "F") return lit("F", false);
    if (c === "d" && s[i + 1] === ":") {
      const hex = s.slice(i + 2, i + 18);
      if (!/^[0-9a-f]{16}$/.test(hex)) fail("bad number");
      i += 18;
      return { $num: hex };
    }
    if (c === '"') {
      let j = i + 1;
      while (j < s.length && s[j] !== '"') {
        if (s[j] === "\\") fail("escape out of domain");
        j++;
      }
      if (j >= s.length) fail("unterminated string");
      const str = s.slice(i + 1, j);
      i = j + 1;
      return { $str: str };
    }
    if (c === "[") {
      i++;
      const arr = [];
      if (s[i] === "]") { i++; return arr; }
      for (;;) {
        arr.push(value());
        if (s[i] === ",") { i++; continue; }
        if (s[i] === "]") { i++; return arr; }
        fail("bad array");
      }
    }
    if (c === "{") {
      i++;
      const obj = { $obj: true, keys: [], vals: [] };
      if (s[i] === "}") { i++; return obj; }
      for (;;) {
        if (s[i] !== '"') fail("bad key");
        const k = value();
        if (s[i] !== ":") fail("missing colon");
        i++;
        obj.keys.push(k.$str);
        obj.vals.push(value());
        if (s[i] === ",") { i++; continue; }
        if (s[i] === "}") { i++; return obj; }
        fail("bad object");
      }
    }
    fail("bad value");
  }
  const v = value();
  if (i !== s.length) fail("trailing bytes");
  return v;
}

// --- walk + aggregate -------------------------------------------------------
const paths = new Map(); // path -> {types:Map, keysets:Map, lenMin, lenMax, strs:Set, strOverflow}
function stat(path) {
  let st = paths.get(path);
  if (!st) {
    st = { types: new Map(), keysets: new Map(), lenMin: Infinity, lenMax: -1,
           strs: new Set(), strOverflow: false };
    paths.set(path, st);
  }
  return st;
}
function bump(map, key) { map.set(key, (map.get(key) || 0) + 1); }

function walk(v, path) {
  const st = stat(path);
  if (v === null) { bump(st.types, "null"); return; }
  if (v === true || v === false) { bump(st.types, "bool"); return; }
  if (Array.isArray(v)) {
    bump(st.types, "array");
    if (v.length < st.lenMin) st.lenMin = v.length;
    if (v.length > st.lenMax) st.lenMax = v.length;
    for (const it of v) walk(it, path + "[]");
    return;
  }
  if (v.$undef) { bump(st.types, "undef"); return; }
  if (v.$fn) { bump(st.types, "fn"); return; }
  if (v.$cyc) { bump(st.types, "cyc"); return; }
  if (v.$num !== undefined) { bump(st.types, "number"); return; }
  if (v.$str !== undefined) {
    bump(st.types, "string");
    if (st.strs.size < 40) st.strs.add(v.$str);
    else st.strOverflow = true;
    return;
  }
  bump(st.types, "object");
  bump(st.keysets, v.keys.join(","));
  for (let k = 0; k < v.keys.length; k++) walk(v.vals[k], path + "." + v.keys[k]);
}

let records = 0;
for (const file of files) {
  const lines = fs.readFileSync(file, "utf8").split("\n").filter((l) => l.length > 0);
  for (const line of lines) {
    const parts = line.split("\t");
    const field = fieldSel || (parts.length >= 5 ? "post" : "ret");
    const idx = { args: 2, ret: 3, post: 4 }[field];
    if (idx === undefined || parts[idx] === undefined) continue;
    walk(parseCanon(parts[idx]), "$");
    records++;
  }
}

console.log(`surveyed ${records} records from ${files.length} file(s)\n`);
const sorted = [...paths.keys()].sort();
for (const p of sorted) {
  const st = paths.get(p);
  const types = [...st.types.entries()].map(([t, n]) => `${t}:${n}`).join(" ");
  let extra = "";
  if (st.types.has("array")) extra += `  len ${st.lenMin}..${st.lenMax}`;
  if (st.types.has("string")) {
    extra += st.strOverflow ? `  strs>40` : `  strs {${[...st.strs].sort().join("|")}}`;
  }
  console.log(`${p}\n    ${types}${extra}`);
  if (st.keysets.size > 0) {
    for (const [ks, n] of st.keysets.entries()) {
      console.log(`    keys(${n}): ${ks}`);
    }
  }
}
