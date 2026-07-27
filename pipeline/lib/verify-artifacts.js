#!/usr/bin/env node
"use strict";
// Re-hash every artifact in a pipeline run against its manifest entry and
// reject stray artifact files the manifest does not list.
// Usage: node lib/verify-artifacts.js <run-dir>

const fs = require("fs");
const path = require("path");
const { sha256 } = require("./manifest");

const runDir = path.resolve(process.argv[2] || "");
const manifestPath = path.join(runDir, "manifest.json");
if (!process.argv[2] || !fs.existsSync(manifestPath)) {
  console.error("usage: verify-artifacts.js <run-dir with manifest.json>");
  process.exit(1);
}
// The manifest must be a REGULAR FILE before it is read (a symlink or a
// fifo here would redirect the whole verification), and it must be free
// of duplicate keys — JSON.parse is silently last-wins, so a decoy stage
// or artifact list can precede the genuine one (review-117-plib-1 [M]).
// Scanner reused from the rig's reviewed one (port/goldens-m4/
// json-dup-key-scan.js, closed GO in .loop/review-98-1.log).
const { assertNoDuplicateKeys } = require(
  path.join(__dirname, "..", "..", "port", "goldens-m4", "json-dup-key-scan.js"));
{
  let mst = null;
  try { mst = fs.lstatSync(manifestPath); } catch (e) { mst = null; }
  if (!mst || !mst.isFile()) {
    console.error("verify-artifacts: manifest.json is not a regular file");
    process.exit(1);
  }
}
const manifestRaw = fs.readFileSync(manifestPath, "utf8");
try {
  assertNoDuplicateKeys(manifestRaw, "manifest.json");
} catch (e) {
  console.error("verify-artifacts: " + e.message);
  process.exit(1);
}
const manifest = JSON.parse(manifestRaw);

// FAIL CLOSED ON A VACUOUS SET (iter 117; review-116-plib-1 [H]3 and
// review-116-plib-2-grok [H], independently reproduced live: a manifest
// of `{"stages":{}}` — or one whose every stage carries `artifacts: []` —
// printed "verify-artifacts: 0 artifact(s) OK" and exited 0). Nothing
// below can fail when there is nothing to check: the bidirectional walk
// only rejects files the manifest does not list, and an empty run dir
// lists nothing. Verifying nothing is not verification, so the shape of
// the set is asserted BEFORE its contents.
const bad = (msg) => { console.error("verify-artifacts: " + msg); process.exit(1); };
if (manifest === null || typeof manifest !== "object" || Array.isArray(manifest)) {
  bad("manifest.json is not a JSON object");
}
const stages = manifest.stages;
if (stages === null || typeof stages !== "object" || Array.isArray(stages)) {
  bad("manifest.stages is not a JSON object");
}
if (Object.keys(stages).length === 0) bad("manifest lists ZERO stages — nothing to verify");
for (const [stageName, stage] of Object.entries(stages)) {
  if (stage === null || typeof stage !== "object" || Array.isArray(stage)) {
    bad(`stage ${stageName} is not a JSON object`);
  }
  if (!Array.isArray(stage.artifacts)) bad(`stage ${stageName} has no artifacts array`);
  if (stage.artifacts.length === 0) bad(`stage ${stageName} lists ZERO artifacts — nothing to verify`);
}

let checked = 0;
const listed = new Set();
const identities = new Map();
for (const [stageName, stage] of Object.entries(stages)) {
  for (const a of stage.artifacts) {
    if (a === null || typeof a !== "object" || Array.isArray(a)) {
      bad(`stage ${stageName} has a non-object artifact entry`);
    }
    // Path must be a CANONICAL run-dir-relative path. `path.join` happily
    // resolves "../outside.bin" and an absolute path REPLACES runDir, so
    // without this an entry can be "verified" against a file that is not
    // in the run at all — and then the unlisted-file walk, which only ever
    // looks inside runDir, has nothing to object to.
    if (typeof a.path !== "string" || a.path === "") bad(`stage ${stageName} artifact has no path string`);
    // Whitespace is rejected as well: the frozen per-stage aggregate joins
    // `${path} ${sha256}\n`, so a path containing a space or a newline is
    // the same separator-injection shape as the dotted contract paths
    // (review-117-plib-2o [L]2). No real manifest has one.
    if (path.posix.normalize(a.path) !== a.path || path.isAbsolute(a.path) ||
        a.path.split("/").some((seg) => seg === "" || seg === "." || seg === "..") ||
        /\s/.test(a.path) || a.path.includes("\\")) {
      bad(`artifact path is not canonical run-relative: ${JSON.stringify(a.path)}`);
    }
    // Duplicates would inflate `checked` and let one real file stand in
    // for several manifest rows.
    if (listed.has(a.path)) bad(`duplicate artifact path in manifest: ${a.path}`);
    if (!Number.isSafeInteger(a.bytes) || a.bytes < 0) {
      bad(`artifact ${a.path} has no safe-integer bytes field`);
    }
    if (typeof a.sha256 !== "string" || !/^[0-9a-f]{64}$/.test(a.sha256)) {
      bad(`artifact ${a.path} has no lowercase 64-hex sha256 field`);
    }
    listed.add(a.path);
    const fp = path.join(runDir, a.path);
    // lstat, not exists: a symlink pointing outside the run dir would be
    // read through by readFileSync and would never be walked as a stray.
    let st = null;
    try { st = fs.lstatSync(fp); } catch (e) { st = null; }
    if (st && !st.isFile()) bad(`artifact ${a.path} is not a regular file`);
    // Lexical uniqueness is not filesystem identity: on a case-insensitive
    // volume "Anim.bin" and "anim.bin" are DIFFERENT manifest rows naming
    // the SAME file, so one real artifact satisfied two rows and inflated
    // the checked count (review-117-plib-1 [M], reproduced live on this
    // host). (dev, ino) is the identity that matters.
    if (st) {
      const ident = st.dev + ":" + st.ino;
      if (identities.has(ident)) {
        bad(`artifacts ${identities.get(ident)} and ${a.path} are the SAME file ` +
          `(dev:ino ${ident}) — one file cannot satisfy two manifest rows`);
      }
      identities.set(ident, a.path);
    }
    if (!fs.existsSync(fp)) {
      console.error(`MISSING artifact ${a.path} (stage ${stageName})`);
      process.exit(1);
    }
    const buf = fs.readFileSync(fp);
    if (buf.length !== a.bytes) {
      console.error(`SIZE mismatch ${a.path}: manifest ${a.bytes}, file ${buf.length}`);
      process.exit(1);
    }
    const h = sha256(buf);
    if (h !== a.sha256) {
      console.error(`HASH mismatch ${a.path}: manifest ${a.sha256}, file ${h}`);
      process.exit(1);
    }
    checked++;
  }
}

// No unlisted artifacts in the run dir (manifest.json itself excepted).
const walk = (dir) => {
  for (const e of fs.readdirSync(dir, { withFileTypes: true })) {
    const fp = path.join(dir, e.name);
    if (e.isDirectory()) { walk(fp); continue; }
    const rel = path.relative(runDir, fp);
    if (rel === "manifest.json") continue;
    if (!e.isFile()) {
      console.error(`NON-REGULAR file in run dir: ${rel}`);
      process.exit(1);
    }
    if (!listed.has(rel)) {
      console.error(`UNLISTED file in run dir: ${rel}`);
      process.exit(1);
    }
  }
};
walk(runDir);

if (checked === 0) bad("ZERO artifacts checked — a run that verifies nothing never passes");

console.log(`verify-artifacts: ${checked} artifact(s) OK in ${path.relative(process.cwd(), runDir) || "."}`);
