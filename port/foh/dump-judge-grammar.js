#!/usr/bin/env node
// dump-judge-grammar.js — canonical dump of the JUDGE'S DECISION TABLES.
//
// WHY (Tier A+ second reviewer, round 2): per-rule negative fixtures catch the
// loosenings someone thought to write a fixture for. They do NOT catch a rule
// nobody guarded — the reviewer proved this by stacking five simultaneous
// widenings (a dropped REFUSED screen-binding, a widened SFIELD_SCREENS entry,
// a widened LAUNCH field domain, a spurious EDGES entry, and loosened
// normalizer domains) and watching every printed leg stay green.
//
// The class fix is to freeze the TABLES THEMSELVES, so ANY widening of ANY of
// them is caught whether or not a fixture exists for it. This extracts the
// exact SOURCE SPAN of each decision table and prints `<name> <sha256>`.
//
// SOURCE-TEXT, not semantics — deliberately, and the trade is worth naming: a
// pure reformat of a table trips this and needs a re-freeze. That is the
// correct bias for a frozen decision table (the same bias every other frozen
// artifact in this repo has); a semantic dump would need the judge to grow a
// mode, and the judge is precisely the file this arc is trying to keep still.
//
// Usage: node dump-judge-grammar.js <judge.js> [<normalizer.js>]
"use strict";
const fs = require("fs");
const crypto = require("crypto");

// The decision tables + the line-form regexes. A rule that lives outside this
// list is NOT frozen, so adding a new table means adding it here too.
const NAMES = [
  "EDGES", "REFUSED", "SVAL_DOM", "SFIELD_SCREENS",
  "RE_S_NUM", "RE_S_REF", "RE_SHOT", "RE_LAUNCH", "RE_TLAUNCH",
  // normalize-foh-trace.js names its line forms differently, and the Tier A+
  // reviewer loosened exactly these in its stacked proof.
  "NUM", "RE_HDR", "RE_T", "RE_S", "RE_END", "INPUT_FREE_FLOWS",
];

// Span = from `const NAME` to the `;` that closes it at bracket depth 0.
// Strings and comments are skipped so a `;` inside either cannot end it early.
function span(src, name) {
  const start = src.search(new RegExp("^const " + name + "\\b", "m"));
  if (start < 0) return null;
  let i = start, depth = 0, inS = null, inC = null;
  for (; i < src.length; i++) {
    const c = src[i], n = src[i + 1];
    if (inC) {
      if (inC === "//" && c === "\n") inC = null;
      else if (inC === "/*" && c === "*" && n === "/") { inC = null; i++; }
      continue;
    }
    if (inS) {
      if (c === "\\") { i++; continue; }
      if (c === inS) inS = null;
      continue;
    }
    if (c === "/" && n === "/") { inC = "//"; i++; continue; }
    if (c === "/" && n === "*") { inC = "/*"; i++; continue; }
    if (c === '"' || c === "'" || c === "`") { inS = c; continue; }
    if (c === "(" || c === "[" || c === "{") depth++;
    else if (c === ")" || c === "]" || c === "}") depth--;
    else if (c === ";" && depth === 0) return src.slice(start, i + 1);
  }
  return null;
}

// PER-NAME SPANS MISS POST-DECLARATION MUTATION (review-r13 BLOCKER):
// judge-foh-trace.js mutates EDGES and REFUSED AFTER their `const` ends, inside
// the FOH_NETPLAY profile blocks (`EDGES.add(e)`, `REFUSED.set(...)`). A
// spurious edge added there changes no per-name hash. So the WHOLE decision
// region is hashed as well, from the first table declaration to the start of
// the judging loop — which contains the profile blocks by construction and
// cannot be bypassed by adding a new mutation site inside it.
function region(src, fromRe, toRe) {
  const a = src.search(fromRe);
  if (a < 0) return null;
  const rest = src.slice(a);
  const b = rest.search(toRe);
  if (b < 0) return null;
  return rest.slice(0, b);
}

const files = process.argv.slice(2);
if (files.length < 1) {
  console.error("usage: node dump-judge-grammar.js <judge.js> [<normalizer.js>]");
  process.exit(1);
}
let found = 0;
for (const f of files) {
  const src = fs.readFileSync(f, "utf8");
  const base = require("path").basename(f);
  for (const nm of NAMES) {
    const sp = span(src, nm);
    if (sp === null) continue;
    const h = crypto.createHash("sha256").update(sp, "utf8").digest("hex");
    console.log(base + " " + nm + " " + h);
    found++;
  }
  // the decision REGION (declarations + any profile mutation of them)
  const reg = region(src, /^const (EDGES|NUM|RE_HDR)\b/m,
                     /^for \(let k = 1;|^const \[, , inPath/m);
  if (reg !== null) {
    const rh = crypto.createHash("sha256").update(reg, "utf8").digest("hex");
    console.log(base + " DECISION_REGION " + rh);
    found++;
  }
  // THE ENFORCEMENT REGION (Tier A+ second reviewer, round 3 BLOCKER):
  // freezing the tables does NOT freeze the LOOP that consults them. The
  // reviewer constructed four loosenings that left every table byte-identical
  // and still printed OK — each one a bypass planted inside the judging loop
  // (`if (m[2] === "tapjump2") continue;` before the domain check; a
  // `m[2] !== "turbo" &&` guard on the per-screen check; the same for a
  // refusal token; `if (false && ...)` on LAUNCH adjacency). "The table still
  // says X, the loop no longer enforces X for field Y" is a whole CLASS that
  // no finite set of per-rule fixtures can cover, so the loop body is hashed
  // too: from the judging loop / the normalizer's driver to EOF.
  const encAt = src.search(/^for \(let k = 1;|^const \[, , inPath/m);
  if (encAt >= 0) {
    const eh = crypto.createHash("sha256")
      .update(src.slice(encAt), "utf8").digest("hex");
    console.log(base + " ENFORCE_REGION " + eh);
    found++;
  }
  // ...and the WHOLE FILE, which is the only thing that covers what is
  // OUTSIDE both regions — the prologue holds `die()` itself, and a `die`
  // that stopped exiting would loosen every rule at once while every region
  // hash above stayed green. Per-region hashes are kept for locality: they
  // say WHICH surface moved, this one says THAT something moved.
  const fh = crypto.createHash("sha256").update(src, "utf8").digest("hex");
  console.log(base + " FILE " + fh);
  found++;
}
// A dump that found nothing would freeze nothing and pass forever.
if (found === 0) {
  console.error("dump-judge-grammar: no decision tables found — the extractor "
                + "is broken or the judge was restructured");
  process.exit(2);
}
console.log("TABLES " + found);
