#!/usr/bin/env node
"use strict";
// Assert an `assets` pipeline run's manifest against the pinned coverage
// contract pipeline/expected-assets.json (measured-then-frozen).
// Usage: node lib/check-assets-expected.js <run-dir> <upstream-clone-root>
//
// WHY A SIBLING FILE AND NOT pipeline/expected.json (deliberate, iter A9):
// `pipeline/expected.json` and `pipeline/lib/check-expected.js` are BOTH
// sha256-pinned `reviewed-go` in port/sim/device/m4-freeze-manifest.txt and
// in check-device-fullgame.sh's PRODUCER_PINS. Adding an `assets` section
// would have to edit both — and check-expected.js's schema is closed in
// two directions on purpose, so the section cannot simply be added to the
// data file alone. Re-pinning reviewed-go bytes is a driver-owned act
// (three pin rows plus verify_m4.sh's MANIFEST_SHA256 anchor, two of which
// files are being edited by another lane), so the assets contract lands
// beside them instead of inside them. Nothing is weakened: the SAME two
// closed-schema arms that protect expected.json protect this file — the
// contract's WIDTH (top-level key set) and its DEPTH (every leaf path +
// type, digested) are pinned HERE, in code, not in the file being checked.
// Promotion into expected.json is a one-commit follow-up.

const fs = require("fs");
const path = require("path");
const { sha256, sha256File } = require("./manifest");
const { assertNoDuplicateKeys } = require(
  path.join(__dirname, "..", "..", "port", "goldens-m4", "json-dup-key-scan.js"));

const readJson = (file, what) => {
  const raw = fs.readFileSync(file, "utf8");
  try {
    assertNoDuplicateKeys(raw, what);
  } catch (e) {
    console.error("check-assets-expected: " + e.message);
    process.exit(1);
  }
  return JSON.parse(raw);
};

const runDir = path.resolve(process.argv[2] || "");
const distRoot = path.resolve(process.argv[3] || "");
if (!process.argv[2] || !process.argv[3]) {
  console.error("usage: check-assets-expected.js <run-dir> <upstream-clone-root>");
  process.exit(1);
}
const REPO_ROOT = path.join(__dirname, "..", "..");
const expected = readJson(path.join(__dirname, "..", "expected-assets.json"),
  "expected-assets.json");
const manifest = readJson(path.join(runDir, "manifest.json"), "manifest.json");

// ---- contract WIDTH (top-level key set), pinned in code -------------------
const NON_STAGE_KEYS = ["_comment", "upstreamPinPrefix"];
const REQUIRED_SECTIONS = ["assets"];
{
  const present = Object.keys(expected);
  const missingMeta = NON_STAGE_KEYS.filter((k) => !present.includes(k));
  if (missingMeta.length) {
    console.error(`check-assets-expected: missing required key(s): ${missingMeta.join(", ")}`);
    process.exit(1);
  }
  const sections = present.filter((k) => !NON_STAGE_KEYS.includes(k));
  const missing = REQUIRED_SECTIONS.filter((k) => !sections.includes(k));
  const extra = sections.filter((k) => !REQUIRED_SECTIONS.includes(k));
  if (missing.length || extra.length) {
    console.error("check-assets-expected: section set does not match the pinned contract" +
      (missing.length ? ` — MISSING: ${missing.join(", ")}` : "") +
      (extra.length ? ` — UNPINNED EXTRA (no assertion branch exists): ${extra.join(", ")}` : ""));
    process.exit(1);
  }
}

// ---- contract DEPTH (every leaf path + type), pinned in code --------------
// Same construction as check-expected.js: an INJECTIVE path encoding (each
// component JSON-encoded into an array, so a "." inside a key cannot be
// forged by flattening), and an empty object is itself a leaf token so
// inert subtrees cannot be added at an identical digest. Adding, removing,
// renaming, retyping or moving ANY pin at ANY depth fails here — a pin can
// only be retired in code, under review, in the same change.
const CONTRACT_SHAPE_SHA256 = "eef481984f807bc787359d1a8f60ccde427cfdb11be935258a1868d9680ed96e";
const CONTRACT_SHAPE_LEAVES = 146;
{
  const leaves = [];
  const walk = (v, p) => {
    if (v && typeof v === "object" && !Array.isArray(v)) {
      const ks = Object.keys(v).sort();
      if (ks.length === 0) { leaves.push(JSON.stringify(p) + ":emptyobject"); return; }
      for (const k of ks) walk(v[k], p.concat([k]));
    } else leaves.push(JSON.stringify(p) + ":" + (Array.isArray(v) ? "array" : typeof v));
  };
  for (const s of REQUIRED_SECTIONS) walk(expected[s], [s]);
  const digest = sha256(leaves.join("\n") + "\n");
  if (leaves.length !== CONTRACT_SHAPE_LEAVES || digest !== CONTRACT_SHAPE_SHA256) {
    console.error("check-assets-expected: expected-assets.json's pin SHAPE does not " +
      "match the code-pinned contract — leaves " + leaves.length + " (pinned " +
      CONTRACT_SHAPE_LEAVES + "), digest " + digest + " (pinned " +
      CONTRACT_SHAPE_SHA256 + ").");
    process.exit(1);
  }
}

let failures = 0;
const fail = (msg) => { console.error("EXPECTED-FAIL: " + msg); failures++; };
const eq = (what, got, want) => {
  if (got !== want) fail(`${what}: got ${JSON.stringify(got)}, pinned ${JSON.stringify(want)}`);
};

// Upstream pin: full 40-hex object ID on both sides, strict equality.
if (!/^[0-9a-f]{40}$/.test(String(expected.upstreamPinPrefix))) {
  fail(`upstreamPinPrefix ${JSON.stringify(expected.upstreamPinPrefix)} is not a full 40-hex object ID`);
} else if (!/^[0-9a-f]{40}$/.test(String(manifest.upstreamHead))) {
  fail(`upstreamHead ${JSON.stringify(manifest.upstreamHead)} is not a full 40-hex object ID`);
} else if (manifest.upstreamHead !== expected.upstreamPinPrefix) {
  fail(`upstreamHead ${manifest.upstreamHead} != pin ${expected.upstreamPinPrefix}`);
}

// ---- assets (pre-scaled menu artwork, IMG1) ------------------------------
{
  const exp = expected.assets;
  const st = manifest.stages && manifest.stages.assets;
  if (!st) {
    fail("manifest has no assets stage");
  } else {
    // The stage entry's SHAPE is closed (review-a9-1 [H]2): the checker used
    // to read only the fields it happened to name, so a stage that simply
    // stopped emitting one — measured: `sources: []`, which voids FORMATS.md
    // §0/§1 provenance — still printed ASSETS OK. A missing or extra field
    // is now a failure, so provenance cannot be dropped silently.
    const STAGE_KEYS = ["artifacts", "artifactsSha256", "coverage", "directory",
      "format", "perImage", "provenance", "sources"];
    const gotKeys = Object.keys(st).sort();
    if (JSON.stringify(gotKeys) !== JSON.stringify(STAGE_KEYS)) {
      fail(`assets stage field set drifted: got ${JSON.stringify(gotKeys)}, ` +
        `pinned ${JSON.stringify(STAGE_KEYS)}`);
    }
    eq("assets.format", st.format, "IMG1");
    for (const k of Object.keys(exp.coverage)) {
      eq(`assets.coverage.${k}`, st.coverage[k], exp.coverage[k]);
    }
    for (const k of Object.keys(exp.provenance)) {
      eq(`assets.provenance.${k}`, st.provenance && st.provenance[k], exp.provenance[k]);
    }
    eq("assets.artifacts.length", st.artifacts.length, exp.artifactsTotal);
    for (const name of ["assets/menu.img1", "assets/README.md"]) {
      if (!st.artifacts.some((a) => a.path === name)) {
        fail(`assets artifact ${name} missing from manifest`);
      }
    }

    // perImage is a CLOSED set in BOTH directions: a pinned image that
    // stopped being emitted, and an image that appeared without a pin, are
    // both failures — a run must not be able to choose which of its own
    // rows get asserted.
    const got = st.perImage || {};
    const pinned = Object.keys(exp.perImage).sort();
    const emitted = Object.keys(got).sort();
    if (JSON.stringify(pinned) !== JSON.stringify(emitted)) {
      fail(`assets.perImage name set drifted: emitted ${JSON.stringify(emitted)}, ` +
        `pinned ${JSON.stringify(pinned)}`);
    }

    // Directory ORDER (the consumer's index space) is pinned exactly. The
    // shape digest sees an array as a single leaf, so the values are
    // compared here explicitly, and the order must be a permutation of the
    // pinned perImage name set — a name present in one and not the other is
    // a failure from either side.
    eq("assets.directory (index order)",
      JSON.stringify(st.directory), JSON.stringify(exp.directory));
    if (JSON.stringify([...exp.directory].sort()) !== JSON.stringify(pinned)) {
      fail(`assets.directory names ${JSON.stringify([...exp.directory].sort())} ` +
        `!= perImage names ${JSON.stringify(pinned)}`);
    }

    const KINDS = { portrait: "portraits", stagePreview: "stagePreviews", cursor: "cursors" };
    const ALPHA = new Set(["opaque", "binary", "aa"]);
    const kindCount = { portraits: 0, stagePreviews: 0, cursors: 0 };
    let pixels = 0;
    for (const name of pinned) {
      const e = exp.perImage[name], g = got[name];
      if (!g) continue; // already reported by the set comparison
      for (const k of Object.keys(e)) eq(`assets.perImage.${name}.${k}`, g[k], e[k]);

      // Self-consistency of the pinned row itself, so a careless re-freeze
      // cannot enshrine a nonsensical image.
      if (!KINDS[e.kind]) {
        fail(`assets.perImage.${name}.kind ${JSON.stringify(e.kind)} unknown`);
        continue;
      }
      kindCount[KINDS[e.kind]]++;
      for (const k of ["srcW", "srcH", "w", "h"]) {
        if (!Number.isSafeInteger(e[k]) || e[k] <= 0) {
          fail(`assets.perImage.${name}.${k} ${JSON.stringify(e[k])} is not a positive integer`);
        }
      }
      // The stage resamples by area average only (lib/img1.js hard-throws
      // on upscale), so a pin claiming an upscale is unreachable by code.
      if (e.w > e.srcW || e.h > e.srcH) {
        fail(`assets.perImage.${name}: emitted ${e.w}x${e.h} exceeds source ${e.srcW}x${e.srcH}`);
      }
      if (!ALPHA.has(e.srcAlpha) || !ALPHA.has(e.alpha)) {
        fail(`assets.perImage.${name}: alpha class ${JSON.stringify(e.srcAlpha)}/` +
          `${JSON.stringify(e.alpha)} not in {opaque,binary,aa}`);
      }
      if (e.srcColorType !== 2 && e.srcColorType !== 6) {
        fail(`assets.perImage.${name}: srcColorType ${e.srcColorType} not in {2,6}`);
      }
      if (e.srcColorType === 2 && e.srcAlpha !== "opaque") {
        fail(`assets.perImage.${name}: colour type 2 (RGB) cannot carry alpha class ${e.srcAlpha}`);
      }
      pixels += e.w * e.h;
    }
    eq("assets.coverage.images (== perImage rows)", exp.coverage.images, pinned.length);
    eq("assets.coverage.pixels (== sum of w*h)", exp.coverage.pixels, pixels);
    for (const k of Object.keys(kindCount)) {
      eq(`assets.coverage.${k} (== perImage kind tally)`, exp.coverage[k], kindCount[k]);
    }

    // IMG1 layout arithmetic (FORMATS.md §7.1): 12-byte header + 24 bytes
    // of directory per image + each image's 3 bytes/px, each padded to 4.
    const pad4 = (n) => (n + 3) & ~3;
    let want = pad4(12 + pinned.length * 24);
    for (const name of pinned) {
      want = pad4(want + exp.perImage[name].w * exp.perImage[name].h * 3);
    }
    eq("assets.coverage.img1Bytes (== IMG1 layout arithmetic)", exp.coverage.img1Bytes, want);
    const blob = st.artifacts.find((a) => a.path === "assets/menu.img1");
    if (blob) eq("assets/menu.img1 bytes", blob.bytes, exp.coverage.img1Bytes);

    // SOURCE PROVENANCE, RE-DERIVED LIVE (review-a9-1 [H]2). The pinned set
    // is the exact list of files the stage is allowed to have consumed —
    // the 15 upstream PNGs plus the three generator sources — and each
    // recorded sha256 is RE-HASHED from disk here, so the manifest's
    // provenance is verified against the tree rather than believed.
    // Generator-code hashes are deliberately NOT frozen in the contract
    // (that would make every code edit a contract re-freeze, which the
    // audio stage does not require either); what is frozen is WHICH files
    // were consumed, and what is verified is that the recorded hashes are
    // truthful.
    {
      const wantSrc = [
        ...Object.keys(exp.perImage).map((n) => "dist/" + exp.perImage[n].source),
        "pipeline/lib/img1.js", "pipeline/lib/png.js", "pipeline/stages/assets.js"].sort();
      const gotSrc = (st.sources || []).map((s) => s.path).sort();
      if (JSON.stringify(gotSrc) !== JSON.stringify(wantSrc)) {
        fail(`assets.sources path set drifted: got ${JSON.stringify(gotSrc)}, ` +
          `pinned ${JSON.stringify(wantSrc)}`);
      }
      for (const s of st.sources || []) {
        if (!/^[0-9a-f]{64}$/.test(String(s.sha256))) {
          fail(`assets.sources ${s.path}: sha256 ${JSON.stringify(s.sha256)} is not a sha256`);
          continue;
        }
        const abs = s.path.startsWith("dist/")
          ? path.join(distRoot, ...s.path.split("/"))
          : path.join(REPO_ROOT, ...s.path.split("/"));
        if (!fs.existsSync(abs)) { fail(`assets.sources ${s.path}: no such file (${abs})`); continue; }
        const live = sha256File(abs);
        if (live !== s.sha256) {
          fail(`assets.sources ${s.path}: recorded ${s.sha256} != live ${live}`);
        }
      }
    }

    // EXTERNAL ARTIFACT IDENTITY: the frozen output bytes. Recomputed here
    // from the manifest (which verify-artifacts.js has re-hashed against
    // the files on disk) and compared to BOTH the stage's own aggregate and
    // the frozen pin — self-consistency is not identity.
    const agg = sha256(st.artifacts.map((a) => `${a.path} ${a.sha256}\n`).join(""));
    eq("assets.artifactsSha256 (manifest-internal)", st.artifactsSha256, agg);
    eq("assets.artifactsSha256 (frozen pin)", agg, exp.artifactsSha256);
  }
}

if (failures > 0) {
  console.error(`check-assets-expected: ${failures} failure(s)`);
  process.exit(1);
}
console.log("check-assets-expected: coverage contract OK for stage(s) assets");
