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
const manifest = JSON.parse(fs.readFileSync(manifestPath, "utf8"));

let checked = 0;
const listed = new Set();
for (const [stageName, stage] of Object.entries(manifest.stages)) {
  for (const a of stage.artifacts) {
    listed.add(a.path);
    const fp = path.join(runDir, a.path);
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
    if (!listed.has(rel)) {
      console.error(`UNLISTED file in run dir: ${rel}`);
      process.exit(1);
    }
  }
};
walk(runDir);

console.log(`verify-artifacts: ${checked} artifact(s) OK in ${path.relative(process.cwd(), runDir) || "."}`);
