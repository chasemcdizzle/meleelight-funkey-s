#!/usr/bin/env node
"use strict";
// Assert a pipeline run's manifest against the pinned coverage contract
// pipeline/expected.json (measured-then-frozen, like the oracle goldens),
// and re-derive the animation source-file reconciliation LIVE against the
// upstream src tree (anatomy's "754 files" = exported states + index.js
// files + dead files; see FORMATS.md §2.7).
// Usage: node lib/check-expected.js <run-dir> <upstream-clone-root> [stages]
//   [stages] — comma-separated stage sections to assert (for --only runs,
//   e.g. "animations"). Default: EVERY stage section pinned in
//   expected.json (the full contract; the M1 exit gate uses the default).

const fs = require("fs");
const path = require("path");
const { sha256 } = require("./manifest");
// JSON.parse is silently LAST-WINS on duplicate keys, so a contract or a
// manifest can carry a decoy value ahead of the genuine one and this
// checker would read only the second (review-117-plib-1 [M]; PROCESS §3's
// whitelist rule names duplicate keys explicitly). The rig already owns a
// reviewed string-aware scanner (port/goldens-m4/json-dup-key-scan.js,
// closed GO in .loop/review-98-1.log) — reused rather than re-written.
const { assertNoDuplicateKeys } = require(
  path.join(__dirname, "..", "..", "port", "goldens-m4", "json-dup-key-scan.js"));

const readJson = (file, what) => {
  const raw = fs.readFileSync(file, "utf8");
  try {
    assertNoDuplicateKeys(raw, what);
  } catch (e) {
    console.error("check-expected: " + e.message);
    process.exit(1);
  }
  return JSON.parse(raw);
};

const runDir = path.resolve(process.argv[2] || "");
const distRoot = path.resolve(process.argv[3] || "");
const expected = readJson(path.join(__dirname, "..", "expected.json"), "expected.json");
const manifest = readJson(path.join(runDir, "manifest.json"), "manifest.json");

// THE CONTRACT'S WIDTH IS PINNED HERE, NOT IN THE FILE BEING CHECKED
// (iter 117; review-116-plib-1 [H]2 and review-116-plib-2-grok [H],
// independently reproduced). Deriving the required section list from
// expected.json's own keys meant a DELETED section was simply never
// requested and therefore never asserted — an expected.json stripped to
// {_comment, upstreamPinPrefix} printed "coverage contract OK" with zero
// assertions run. A checker whose contract can be edited by the artifact
// it checks is not a checker. These two literals are the closed schema:
// every name here MUST be present in expected.json and MUST have an
// assertion branch below, and expected.json may carry nothing else.
const REQUIRED_SECTIONS = ["animations", "tables", "stages", "targets", "audio"];
const NON_STAGE_KEYS = new Set(["_comment", "upstreamPinPrefix"]);
{
  const present = Object.keys(expected);
  const missingMeta = [...NON_STAGE_KEYS].filter((k) => !present.includes(k));
  if (missingMeta.length) {
    console.error(`check-expected: expected.json is missing required key(s): ${missingMeta.join(", ")}`);
    process.exit(1);
  }
  const sections = present.filter((k) => !NON_STAGE_KEYS.has(k));
  const missing = REQUIRED_SECTIONS.filter((k) => !sections.includes(k));
  const extra = sections.filter((k) => !REQUIRED_SECTIONS.includes(k));
  if (missing.length || extra.length) {
    console.error("check-expected: expected.json section set does not match the pinned contract" +
      (missing.length ? ` — MISSING: ${missing.join(", ")}` : "") +
      (extra.length ? ` — UNPINNED EXTRA (no assertion branch exists for it): ${extra.join(", ")}` : ""));
    process.exit(1);
  }
  for (const s of REQUIRED_SECTIONS) {
    const v = expected[s];
    if (v === null || typeof v !== "object" || Array.isArray(v) || Object.keys(v).length === 0) {
      console.error(`check-expected: pinned section "${s}" is not a non-empty object`);
      process.exit(1);
    }
  }
}
// THE CONTRACT'S DEPTH IS PINNED HERE TOO (iter 117; review-117-plib-1
// [H]2). Fixing the top-level section list was not enough: every nested
// assertion loop still walked whatever keys expected.json happened to
// carry, so DELETING a nested pin (measured: `audio.coverage.musicBytes`)
// removed the assertion along with the pin and the checker still printed
// OK. The complete leaf SHAPE of the five sections — every leaf path and
// its JSON type, sorted — is digested and pinned in this file. Adding,
// removing, renaming, retyping or moving ANY pin at ANY depth changes the
// digest and fails here, so a pin can no longer be silently retired: it
// has to be retired in code, under review, in the same change.
// The path encoding must be INJECTIVE. A dotted string is not: a literal
// key "coverage.musicBytes" at section level flattens to exactly the same
// token as the nested coverage -> musicBytes pin, so a pin could be
// deleted and replaced by a flattened decoy at an identical digest — and
// the decoy is asserted by nothing. (Demonstrated by a reviewer against
// the first version of this arm: shape-before == shape-after with
// nested-pin-present false. Separator injection, the whitelist-grammar
// rule's own failure mode.) Each path component is JSON-encoded into an
// array, so a "." inside a key stays inside that key's quoted token.
const CONTRACT_SHAPE_SHA256 = "a55f23b7379a1e3ddbadcb60b244d7188b76787401a46f7e7a2fd3b2e050d416";
const CONTRACT_SHAPE_LEAVES = 274;
{
  const leaves = [];
  const walk = (v, p) => {
    if (v && typeof v === "object" && !Array.isArray(v)) {
      const ks = Object.keys(v).sort();
      // A zero-key object emits no leaf at all, so inert subtrees could be
      // ADDED at an identical digest (review-117-plib-2o [L]1). It could
      // never retire an assertion, but "ANY pin at ANY depth" has to be
      // true as stated, so an empty object is itself a leaf token.
      if (ks.length === 0) { leaves.push(JSON.stringify(p) + ":emptyobject"); return; }
      for (const k of ks) walk(v[k], p.concat([k]));
    } else leaves.push(JSON.stringify(p) + ":" + (Array.isArray(v) ? "array" : typeof v));
  };
  for (const s of REQUIRED_SECTIONS) walk(expected[s], [s]);
  const digest = sha256(leaves.join("\n") + "\n");
  if (leaves.length !== CONTRACT_SHAPE_LEAVES || digest !== CONTRACT_SHAPE_SHA256) {
    console.error("check-expected: expected.json's pin SHAPE does not match the " +
      "code-pinned contract — leaves " + leaves.length + " (pinned " + CONTRACT_SHAPE_LEAVES +
      "), digest " + digest + " (pinned " + CONTRACT_SHAPE_SHA256 + ").");
    console.error("check-expected: a pin cannot be added or removed without updating " +
      "CONTRACT_SHAPE_SHA256 in this file. Current leaf shape:");
    for (const l of leaves) console.error("  " + l);
    process.exit(1);
  }
}
const allSections = REQUIRED_SECTIONS;
const wantStages = process.argv[4]
  ? String(process.argv[4]).split(",")
  : allSections.slice();
if (wantStages.length === 0 || wantStages.some((s) => s === "")) {
  console.error("check-expected: empty stage selection — a run that asserts no section never passes");
  process.exit(1);
}
for (const s of wantStages) {
  if (!allSections.includes(s)) {
    console.error(`check-expected: no pinned section "${s}" in expected.json`);
    process.exit(1);
  }
}
if (new Set(wantStages).size !== wantStages.length) {
  console.error("check-expected: duplicate stage in selection");
  process.exit(1);
}

let failures = 0;
const fail = (msg) => { console.error("EXPECTED-FAIL: " + msg); failures++; };
const eq = (what, got, want) => {
  if (got !== want) fail(`${what}: got ${JSON.stringify(got)}, pinned ${JSON.stringify(want)}`);
};

// upstream pin. Both sides must be object-ID grammar before the prefix
// comparison means anything (review-116-plib-1 [M]7: a non-object-ID
// string starting with the prefix satisfied startsWith()).
// FULL object ID, strict equality (review-117-plib-1 [L]): a 7-char
// prefix accepted any of the ~2^132 other IDs sharing it, and prefix
// comparison accepted non-ID strings outright.
if (!/^[0-9a-f]{40}$/.test(String(expected.upstreamPinPrefix))) {
  fail(`upstreamPinPrefix ${JSON.stringify(expected.upstreamPinPrefix)} is not a full 40-hex object ID`);
} else if (!/^[0-9a-f]{40}$/.test(String(manifest.upstreamHead))) {
  fail(`upstreamHead ${JSON.stringify(manifest.upstreamHead)} is not a full 40-hex object ID`);
} else if (manifest.upstreamHead !== expected.upstreamPinPrefix) {
  fail(`upstreamHead ${manifest.upstreamHead} != pin ${expected.upstreamPinPrefix}`);
}

// ---- animations ----
if (wantStages.includes("animations")) {
const exp = expected.animations;
const st = manifest.stages.animations;
if (!st) {
  fail("manifest has no animations stage");
} else {
  eq("animations.format", st.format, "ANIM1");
  for (const k of Object.keys(exp.coverage)) {
    eq(`animations.coverage.${k}`, st.coverage[k], exp.coverage[k]);
  }
  for (const [charName, expChar] of Object.entries(exp.perChar)) {
    const got = st.perChar && st.perChar[charName];
    if (!got) { fail(`animations.perChar.${charName} missing`); continue; }
    eq(`animations.perChar.${charName}.states`, got.states, expChar.states);
    eq(`animations.perChar.${charName}.stateNames.length`,
      got.stateNames.length, expChar.states);
  }
  eq("animations.artifacts.length", st.artifacts.length, 5);

  // Live source-tree reconciliation: 754 files == states + index.js + dead.
  const animSrc = path.join(distRoot, "src", "animations");
  let totalFiles = 0;
  const deadFound = [];
  for (const [charName, char] of Object.entries(st.perChar)) {
    const dir = path.join(animSrc, charName);
    const files = fs.readdirSync(dir).filter((f) => f.endsWith(".js"));
    totalFiles += files.length;
    const exported = new Set(char.stateNames);
    const stateFiles = files.filter((f) => f !== "index.js")
      .map((f) => f.replace(/\.js$/, ""));
    for (const sf of stateFiles) {
      if (!exported.has(sf)) deadFound.push(`${charName}/${sf}`);
    }
    for (const name of exported) {
      if (!stateFiles.includes(name)) {
        fail(`exported state ${charName}/${name} has no same-named source file`);
      }
    }
  }
  eq("animations source files total", totalFiles, exp.sourceFilesTotal);
  deadFound.sort();
  const expDead = [...exp.deadFiles].sort();
  if (JSON.stringify(deadFound) !== JSON.stringify(expDead)) {
    fail(`dead-file set drifted: found ${JSON.stringify(deadFound)}, ` +
      `pinned ${JSON.stringify(expDead)}`);
  }
  // The reconciliation identity itself:
  const idx = Object.keys(st.perChar).length; // one index.js per char dir
  eq("reconciliation: states + index files + dead files",
    st.coverage.states + idx + deadFound.length, exp.sourceFilesTotal);
}
}

// ---- tables ----
if (wantStages.includes("tables")) {
  const exp = expected.tables;
  const st = manifest.stages.tables;
  if (!st) {
    fail("manifest has no tables stage");
  } else {
    eq("tables.format", st.format, "CTAB1");
    for (const k of Object.keys(exp.coverage)) {
      eq(`tables.coverage.${k}`, st.coverage[k], exp.coverage[k]);
    }
    for (const [charName, expChar] of Object.entries(exp.perChar)) {
      const got = st.perChar && st.perChar[charName];
      if (!got) { fail(`tables.perChar.${charName} missing`); continue; }
      for (const k of Object.keys(expChar)) {
        eq(`tables.perChar.${charName}.${k}`, got[k], expChar[k]);
      }
    }
    eq("tables.artifacts.length", st.artifacts.length, 3);
    for (const name of ["ml_tables.h", "ml_tables.c", "tables.json"]) {
      if (!st.artifacts.some((a) => a.path === name)) {
        fail(`tables artifact ${name} missing from manifest`);
      }
    }
  }
}

// ---- stages (VS-stage geometry, STAB1) ----
if (wantStages.includes("stages")) {
  const exp = expected.stages;
  const st = manifest.stages.stages;
  if (!st) {
    fail("manifest has no stages stage");
  } else {
    eq("stages.format", st.format, "STAB1");
    for (const k of Object.keys(exp.coverage)) {
      eq(`stages.coverage.${k}`, st.coverage[k], exp.coverage[k]);
    }
    for (const [stageName, expStage] of Object.entries(exp.perStage)) {
      const got = st.perStage && st.perStage[stageName];
      if (!got) { fail(`stages.perStage.${stageName} missing`); continue; }
      for (const k of Object.keys(expStage)) {
        eq(`stages.perStage.${stageName}.${k}`, got[k], expStage[k]);
      }
    }
    eq("stages.artifacts.length", st.artifacts.length, 3);
    for (const name of ["ml_stages.h", "ml_stages.c", "stages.json"]) {
      if (!st.artifacts.some((a) => a.path === name)) {
        fail(`stages artifact ${name} missing from manifest`);
      }
    }
  }
}

// ---- targets (authored target-test stage tables, TTAB1) ----
if (wantStages.includes("targets")) {
  const exp = expected.targets;
  const st = manifest.stages.targets;
  if (!st) {
    fail("manifest has no targets stage");
  } else {
    eq("targets.format", st.format, "TTAB1");
    for (const k of Object.keys(exp.coverage)) {
      eq(`targets.coverage.${k}`, st.coverage[k], exp.coverage[k]);
    }
    for (const [stageName, expStage] of Object.entries(exp.perStage)) {
      const got = st.perStage && st.perStage[stageName];
      if (!got) { fail(`targets.perStage.${stageName} missing`); continue; }
      for (const k of Object.keys(expStage)) {
        eq(`targets.perStage.${stageName}.${k}`, got[k], expStage[k]);
      }
    }
    eq("targets.artifacts.length", st.artifacts.length, 3);
    for (const name of ["ml_targets.h", "ml_targets.c", "targets.json"]) {
      if (!st.artifacts.some((a) => a.path === name)) {
        fail(`targets artifact ${name} missing from manifest`);
      }
    }
  }
}

// ---- audio (converted PCM blobs + sound map, SND1) ----
if (wantStages.includes("audio")) {
  const exp = expected.audio;
  const st = manifest.stages.audio;
  if (!st) {
    fail("manifest has no audio stage");
  } else {
    eq("audio.format", st.format, "SND1");
    // ffmpeg pin: version AND exact argv — a loosened flag or different
    // build must fail here, never drift frozen bytes silently.
    eq("audio.tool.ffmpeg", st.tool && st.tool.ffmpeg, exp.tool.ffmpeg);
    for (const args of ["sfxArgs", "musicArgs"]) {
      eq(`audio.tool.${args}`, JSON.stringify(st.tool && st.tool[args]),
        JSON.stringify(exp.tool[args]));
    }
    for (const k of Object.keys(exp.coverage)) {
      eq(`audio.coverage.${k}`, st.coverage[k], exp.coverage[k]);
    }
    for (const k of Object.keys(exp.provenance)) {
      eq(`audio.provenance.${k}`, st.provenance && st.provenance[k],
        exp.provenance[k]);
    }
    eq("audio.artifacts.length", st.artifacts.length, exp.artifactsTotal);
    for (const name of ["sounds.json", "audio/README.md"]) {
      if (!st.artifacts.some((a) => a.path === name)) {
        fail(`audio artifact ${name} missing from manifest`);
      }
    }
    // Blob shape: raw S16LE — bytes divisible by the frame size and equal
    // to samples * frame size, at the pinned rate (fix_plan task 4
    // done-check: "byte length ≡ 0 mod frame size and sample count
    // recorded in the manifest").
    // Metadata sidecars are EXACTLY these two paths. The old test — "no
    // channels key means metadata" — let any PCM entry opt out of every
    // blob check simply by omitting its shape fields (review-117-plib-1
    // [H]2), which is the one thing the M4 leg cites this contract for.
    const AUDIO_META = new Set(["sounds.json", "audio/README.md"]);
    for (const a of st.artifacts) {
      if (AUDIO_META.has(a.path)) {
        if (a.channels !== undefined || a.samples !== undefined || a.rate !== undefined) {
          fail(`audio metadata artifact ${a.path} carries PCM shape fields`);
        }
        continue;
      }
      for (const k of ["channels", "samples", "rate", "bytes"]) {
        if (!Number.isSafeInteger(a[k]) || a[k] <= 0) {
          fail(`audio blob ${a.path}: ${k} ${JSON.stringify(a[k])} is not a positive safe integer`);
        }
      }
      // Channel count is bound to the PATH CLASS, not merely to {1,2}
      // (review-117-plib-2o [L]5): sfx is mono x204, music stereo x8, and
      // a music row claiming mono would otherwise satisfy every arithmetic
      // check below at half the true byte count.
      const wantCh = a.path.startsWith("audio/sfx/") ? 1
        : a.path.startsWith("audio/music/") ? 2 : null;
      if (wantCh === null) {
        fail(`audio blob ${a.path}: not under audio/sfx/ or audio/music/`);
      } else if (a.channels !== wantCh) {
        fail(`audio blob ${a.path}: channels ${a.channels} != ${wantCh} for its path class`);
      }
      const frameSize = 2 * a.channels;
      if (a.bytes % frameSize !== 0) {
        fail(`audio blob ${a.path}: ${a.bytes} bytes not 0 mod frame size ${frameSize}`);
      }
      if (a.samples * frameSize !== a.bytes || !(a.samples > 0)) {
        fail(`audio blob ${a.path}: samples ${a.samples} * ${frameSize} != bytes ${a.bytes}`);
      }
      if (a.rate !== 22050) fail(`audio blob ${a.path}: rate ${a.rate} != 22050`);
    }
    // Frozen output bytes: aggregate over path+sha256 of every artifact,
    // recomputed here from the manifest (which verify-artifacts.js has
    // re-hashed against the files) and compared to BOTH the stage's own
    // aggregate and the expected.json pin.
    const agg = sha256(st.artifacts.map((a) => `${a.path} ${a.sha256}\n`).join(""));
    eq("audio.artifactsSha256 (manifest-internal)", st.artifactsSha256, agg);
    eq("audio.artifactsSha256 (frozen pin)", agg, exp.artifactsSha256);
  }
}

// ---- EXTERNAL ARTIFACT IDENTITY, EVERY STAGE ----
// Until iter 117 only `audio` had a FROZEN aggregate; the other four
// stages were checked solely against the run's OWN manifest, which the
// same run wrote (review-116-plib-1 [H]1, re-raised as review-117-plib-1
// [H]1 with a live repro: change tables.json, update its manifest hash,
// and both verifiers exit 0). Self-consistency is not identity. Each
// stage's aggregate over path+sha256 is now compared to a pin frozen in
// expected.json, so a deterministic generator regression that preserves
// every count still fails — including on M4's partial run, which invokes
// this checker for exactly these four stages.
for (const s of REQUIRED_SECTIONS) {
  if (!wantStages.includes(s)) continue;
  const st = manifest.stages[s];
  if (!st || !Array.isArray(st.artifacts) || st.artifacts.length === 0) {
    fail(`${s}: manifest stage missing or lists no artifacts`);
    continue;
  }
  const agg = sha256(st.artifacts.map((a) => `${a.path} ${a.sha256}\n`).join(""));
  eq(`${s}.artifactsSha256 (frozen pin)`, agg, expected[s].artifactsSha256);
}

if (failures > 0) {
  console.error(`check-expected: ${failures} failure(s)`);
  process.exit(1);
}
console.log(`check-expected: coverage contract OK for stage(s) ` +
  `${wantStages.join(", ")}` +
  (wantStages.includes("animations") ? " (incl. live 754-file reconciliation)" : ""));
