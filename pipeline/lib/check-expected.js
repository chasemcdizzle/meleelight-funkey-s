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

const runDir = path.resolve(process.argv[2] || "");
const distRoot = path.resolve(process.argv[3] || "");
const expected = JSON.parse(
  fs.readFileSync(path.join(__dirname, "..", "expected.json"), "utf8"));
const manifest = JSON.parse(
  fs.readFileSync(path.join(runDir, "manifest.json"), "utf8"));

const NON_STAGE_KEYS = new Set(["_comment", "upstreamPinPrefix"]);
const allSections = Object.keys(expected).filter((k) => !NON_STAGE_KEYS.has(k));
const wantStages = process.argv[4]
  ? String(process.argv[4]).split(",")
  : allSections;
for (const s of wantStages) {
  if (!allSections.includes(s)) {
    console.error(`check-expected: no pinned section "${s}" in expected.json`);
    process.exit(1);
  }
}

let failures = 0;
const fail = (msg) => { console.error("EXPECTED-FAIL: " + msg); failures++; };
const eq = (what, got, want) => {
  if (got !== want) fail(`${what}: got ${JSON.stringify(got)}, pinned ${JSON.stringify(want)}`);
};

// upstream pin
if (!manifest.upstreamHead.startsWith(expected.upstreamPinPrefix)) {
  fail(`upstreamHead ${manifest.upstreamHead} != pin ${expected.upstreamPinPrefix}*`);
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
    for (const a of st.artifacts) {
      if (a.channels === undefined) continue; // sounds.json / README
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

if (failures > 0) {
  console.error(`check-expected: ${failures} failure(s)`);
  process.exit(1);
}
console.log(`check-expected: coverage contract OK for stage(s) ` +
  `${wantStages.join(", ")}` +
  (wantStages.includes("animations") ? " (incl. live 754-file reconciliation)" : ""));
