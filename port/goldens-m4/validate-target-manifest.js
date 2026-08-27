#!/usr/bin/env node
// validate-target-manifest.js — THE shared strict validator for
// port/goldens-m4/manifest-target.json (review-94 H1: the freezer's
// iter-94 manifest grammar EXTRACTED VERBATIM so EVERY done-check
// consumer — freeze-target.js, wrap-target.js, verify-target-stream.js,
// record-target.sh, check-target-sim.sh — runs the ONE validator before
// trusting any row; PROCESS §3 whitelist rule; iter 96).
//
// Grammar (exact schema, fail closed): top level {comment,goldens};
// per golden the exact key set/ORDER {id,name,trace,frames,seed,char,
// tstage,minTargets,wantArticles} with id ^t[0-9]{2}$, name
// ^t[0-9]{2}(-[a-z0-9]+)+$ beginning with its id, trace ==
// name+".trace.json" (bare basename, resolved-path containment in the
// golden home), frames int 1..999999, seed int 0..2^32-1, char 0-4,
// tstage 0-9, minTargets 1-10, wantArticles boolean; duplicate
// id/name/trace rejection; the SHARED string-aware duplicate-JSON-key
// scanner (json-dup-key-scan.js, review-96 C-M1, iter 98) over the raw
// bytes BEFORE JSON.parse — a duplicated key at ANY scope is silently
// last-wins under JSON.parse and is refused (the old byte-literal
// '"key":' token count was blind to legal whitespace spellings like
// `"id" : "t02"` — live-confirmed by the round-2 probe — and is
// DELETED, not kept alongside).
//
// Module: loadValidatedManifest([manifestPath]) -> the parsed manifest;
// THROWS Error("manifest grammar — ...") naming the violation.
// goldenByIdOrName(m, id) -> the row, or throws.
// CLI: node validate-target-manifest.js [manifestPath] — prints
// "TARGET MANIFEST OK (...)" exit 0, or dies exit 1 naming the
// violation (tooth surface: a duplicate-id COPY must die naming the
// dup, never report its goldens).
"use strict";
const fs = require("fs");
const path = require("path");
const { assertNoDuplicateKeys } = require("./json-dup-key-scan");

const GOLDENS_DIR = __dirname; // port/goldens-m4

// A45 T6: `customStage` is REQUIRED on every row and is the empty string on
// an authored one. Required-and-possibly-empty rather than optional, because
// an optional key is a key a row can silently lack, and this one decides
// WHICH STAGE the golden plays.
const GOLDEN_KEYS = ["id", "name", "trace", "frames", "seed",
  "char", "tstage", "minTargets", "wantArticles", "customStage"];

function loadValidatedManifest(manifestPath) {
  const mPath = manifestPath ||
    path.join(GOLDENS_DIR, "manifest-target.json");
  function vdie(msg) { throw new Error("manifest grammar — " + msg); }
  const raw = fs.readFileSync(mPath, "utf8");
  // Duplicate-key scan of the RAW bytes BEFORE JSON.parse (review-96
  // C-M1): a duplicate at ANY scope — row, top level, anywhere — is
  // last-wins corruption and dies here naming the key + scope.
  try { assertNoDuplicateKeys(raw, path.basename(mPath)); } catch (e) {
    vdie(e.message);
  }
  let m;
  try { m = JSON.parse(raw); } catch (e) {
    vdie(path.basename(mPath) + " is not valid JSON: " + e.message);
  }
  if (typeof m !== "object" || m === null || Array.isArray(m)) {
    vdie("top level is not an object");
  }
  const topKeys = Object.keys(m).sort().join(",");
  if (topKeys !== "comment,goldens") {
    vdie("top-level keys {" + topKeys + "} != {comment,goldens} (exact schema)");
  }
  if (typeof m.comment !== "string") vdie("comment is not a string");
  if (!Array.isArray(m.goldens) || m.goldens.length < 1) {
    vdie("goldens is not a nonempty array");
  }
  const dir = path.resolve(GOLDENS_DIR);
  const ids = new Set(), names = new Set(), traces = new Set();
  m.goldens.forEach(function (g, idx) {
    const where = "goldens[" + idx + "]";
    if (typeof g !== "object" || g === null || Array.isArray(g)) {
      vdie(where + " is not an object");
    }
    const keys = Object.keys(g);
    if (keys.length !== GOLDEN_KEYS.length ||
        GOLDEN_KEYS.some(function (k, j) { return keys[j] !== k; })) {
      vdie(where + " key set/order {" + keys.join(",") + "} != {" +
        GOLDEN_KEYS.join(",") + "} (exact schema, fail closed)");
    }
    if (typeof g.id !== "string" || !/^t[0-9]{2}$/.test(g.id)) {
      vdie(where + " id '" + g.id + "' fails ^t[0-9]{2}$");
    }
    if (typeof g.name !== "string" || !/^t[0-9]{2}(-[a-z0-9]+)+$/.test(g.name)) {
      vdie(where + " name '" + g.name + "' fails ^t[0-9]{2}(-[a-z0-9]+)+$");
    }
    if (g.name.slice(0, 3) !== g.id) {
      vdie(where + " name '" + g.name + "' does not begin with its id '" + g.id + "'");
    }
    if (g.trace !== g.name + ".trace.json") {
      vdie(where + " trace '" + g.trace + "' != name-derived '" + g.name +
        ".trace.json' (basename-only by construction)");
    }
    if (path.basename(g.trace) !== g.trace) {
      vdie(where + " trace '" + g.trace + "' is not a bare basename");
    }
    if (path.dirname(path.resolve(dir, g.trace)) !== dir) {
      vdie(where + " trace resolves outside the golden home " + dir);
    }
    for (const suffix of [".sha256.json", ".target.sha256.json"]) {
      if (path.dirname(path.resolve(dir, g.name + suffix)) !== dir) {
        vdie(where + " frozen path " + suffix + " resolves outside " + dir);
      }
    }
    if (!Number.isInteger(g.frames) || g.frames < 1 || g.frames > 999999) {
      vdie(where + " frames " + g.frames + " is not an integer in 1..999999");
    }
    if (!Number.isInteger(g.seed) || g.seed < 0 || g.seed > 4294967295) {
      vdie(where + " seed " + g.seed + " is not an integer in 0..2^32-1");
    }
    if (!Number.isInteger(g.char) || g.char < 0 || g.char > 4) {
      vdie(where + " char " + g.char + " outside the char domain 0-4");
    }
    // 0-9 authored; 10-19 a CUSTOM slot — upstream's own numbering at
    // targetselect.js:140-146, the port's MLK_PLAYING_BASE + slot (D52).
    if (!Number.isInteger(g.tstage) || g.tstage < 0 || g.tstage > 19) {
      vdie(where + " tstage " + g.tstage + " outside the target-stage domain 0-19");
    }
    if (typeof g.customStage !== "string") {
      vdie(where + " customStage is not a string");
    }
    // ALL OR NOTHING, both ways: a custom tstage without a code has no
    // stage to play, and a code on an authored tstage would be ignored
    // silently — which is worse, because the row would LOOK covered.
    if ((g.tstage >= 10) !== (g.customStage.length > 0)) {
      vdie(where + " tstage " + g.tstage + " and customStage (" +
        (g.customStage.length > 0 ? "present" : "empty") +
        ") disagree — custom is tstage 10-19 WITH a code, authored is 0-9 " +
        "WITHOUT one");
    }
    // The share-code alphabet, anchored: 14 `&`-separated fields whose
    // number tokens are what createStageCode can emit (`-?\d+(\.\d\d)?`,
    // D39). This is a GRAMMAR check, not a parse — the browser's own
    // parseStageCode is the authority and run-target.js makes it prove the
    // code is its own fixed point before playing it.
    if (g.customStage.length > 0) {
      if (!/^[-0-9.,~&]+$/.test(g.customStage)) {
        vdie(where + " customStage carries a character outside the share-code alphabet");
      }
      const fields = g.customStage.split("&");
      if (fields.length !== 14) {
        vdie(where + " customStage has " + fields.length + " `&` fields (want exactly 14)");
      }
    }
    if (!Number.isInteger(g.minTargets) || g.minTargets < 1 || g.minTargets > 10) {
      vdie(where + " minTargets " + g.minTargets + " outside 1..10");
    }
    if (typeof g.wantArticles !== "boolean") {
      vdie(where + " wantArticles is not a boolean");
    }
    if (ids.has(g.id)) vdie("duplicate golden id " + g.id);
    if (names.has(g.name)) vdie("duplicate golden name " + g.name);
    if (traces.has(g.trace)) vdie("duplicate golden trace " + g.trace);
    ids.add(g.id); names.add(g.name); traces.add(g.trace);
  });
  return m;
}

function goldenByIdOrName(m, id) {
  const g = m.goldens.find((x) => x.id === id || x.name === id);
  if (!g) {
    throw new Error("golden not in port/goldens-m4/manifest-target.json: " + id);
  }
  return g;
}

module.exports = { loadValidatedManifest, goldenByIdOrName, GOLDEN_KEYS };

if (require.main === module) {
  const argPath = process.argv[2];
  let m;
  try {
    m = loadValidatedManifest(argPath);
  } catch (e) {
    console.error("validate-target-manifest: " + e.message);
    process.exit(1);
  }
  console.log("TARGET MANIFEST OK (" + m.goldens.length + " goldens: " +
    m.goldens.map((g) => g.id).join(" ") + ")");
}
