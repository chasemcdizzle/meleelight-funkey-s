// Shared helpers for freezing and verifying golden checksum streams
// (M0 task 5). Used by freeze-stream.js and verify-stream.js so the
// stream-digest convention has exactly ONE implementation.
"use strict";
const crypto = require("crypto");
const fs = require("fs");
const path = require("path");

// Canonical digest of a checksum stream: SHA-256 (lowercase hex) over the
// UTF-8 lines "<f>:<h>\n" for every frame in order. This is an integrity
// seal on the frozen file only — the BINDING comparison is always the
// frame-by-frame exact string equality (oracle/CHECKSUM.md §1.1).
function streamDigest(frames) {
  const h = crypto.createHash("sha256");
  for (const fr of frames) h.update(fr.f + ":" + fr.h + "\n");
  return h.digest("hex");
}

function sha256File(fp) {
  return crypto.createHash("sha256").update(fs.readFileSync(fp)).digest("hex");
}

const GOLDENS_DIR = path.join(__dirname, "..", "goldens");

function loadManifest() {
  return JSON.parse(
    fs.readFileSync(path.join(GOLDENS_DIR, "manifest.json"), "utf8"));
}

function goldenById(id) {
  const g = loadManifest().goldens.find((x) => x.id === id || x.name === id);
  if (!g) throw new Error("golden not in oracle/goldens/manifest.json: " + id);
  return g;
}

// Current checksum-spec version, parsed from the frozen spec itself
// (oracle/CHECKSUM.md "**Spec version: N**"). Frozen streams record the
// version they were recorded under; verification fails on mismatch, so a
// spec bump without a re-freeze cannot go unnoticed (CHECKSUM.md §8).
function specVersion() {
  const txt = fs.readFileSync(path.join(__dirname, "..", "CHECKSUM.md"), "utf8");
  const m = txt.match(/\*\*Spec version: (\d+)\*\*/);
  if (!m) throw new Error("cannot parse Spec version from oracle/CHECKSUM.md");
  return parseInt(m[1], 10);
}

module.exports = {
  streamDigest, sha256File, GOLDENS_DIR, loadManifest, goldenById, specVersion,
};
