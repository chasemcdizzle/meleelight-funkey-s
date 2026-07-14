"use strict";
// Deterministic manifest helpers (pipeline/FORMATS.md §1).
// No timestamps, no absolute paths, recursively sorted keys.

const crypto = require("crypto");
const fs = require("fs");

function sha256(bufOrStr) {
  return crypto.createHash("sha256").update(bufOrStr).digest("hex");
}

function sha256File(path) {
  return sha256(fs.readFileSync(path));
}

// Recursively sort object keys so JSON.stringify is order-independent.
function sortValue(v) {
  if (Array.isArray(v)) return v.map(sortValue);
  if (v && typeof v === "object") {
    const out = {};
    for (const k of Object.keys(v).sort()) out[k] = sortValue(v[k]);
    return out;
  }
  return v;
}

function stableStringify(v) {
  return JSON.stringify(sortValue(v), null, 2) + "\n";
}

module.exports = { sha256, sha256File, stableStringify };
