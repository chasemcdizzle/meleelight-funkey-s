#!/usr/bin/env node
"use strict";
// Assert a pipeline run's manifest against the pinned coverage contract
// pipeline/expected.json (measured-then-frozen, like the oracle goldens),
// and re-derive the animation source-file reconciliation LIVE against the
// upstream src tree (anatomy's "754 files" = exported states + index.js
// files + dead files; see FORMATS.md §2.7).
// Usage: node lib/check-expected.js <run-dir> <upstream-clone-root>

const fs = require("fs");
const path = require("path");

const runDir = path.resolve(process.argv[2] || "");
const distRoot = path.resolve(process.argv[3] || "");
const expected = JSON.parse(
  fs.readFileSync(path.join(__dirname, "..", "expected.json"), "utf8"));
const manifest = JSON.parse(
  fs.readFileSync(path.join(runDir, "manifest.json"), "utf8"));

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

if (failures > 0) {
  console.error(`check-expected: ${failures} failure(s)`);
  process.exit(1);
}
console.log("check-expected: coverage contract OK " +
  "(incl. live 754-file reconciliation)");
