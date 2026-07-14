#!/usr/bin/env node
"use strict";
// M1 data-pipeline runner (PLAN §4 M1; fix_plan §M1 conventions).
//
// Usage:
//   node pipeline/run.js --out <dir> [--dist <upstream-clone-root>]
//     [--only <stage>[,<stage>...]]
//
// Executes every registered stage (executed-JS serialization of the data
// plane) into <dir> and writes ONE deterministic manifest.json (schema:
// pipeline/FORMATS.md §1 — sorted keys, sha256+bytes per artifact,
// upstream git HEAD + source hashes, no timestamps, no absolute paths).
// Stages register here as they land (fix_plan §M1); unknown --only names
// are an error, never a silent no-op.

const fs = require("fs");
const path = require("path");
const { execFileSync } = require("child_process");
const { stableStringify, sha256 } = require("./lib/manifest");

const STAGES = [
  require("./stages/animations"),
  require("./stages/tables"),
  require("./stages/stages"),
  // fix_plan §M1 task 4 registers here: audio
];

function arg(name, dflt) {
  const i = process.argv.indexOf("--" + name);
  if (i === -1) return dflt;
  const v = process.argv[i + 1];
  return v === undefined || v.startsWith("--") ? true : v;
}

function main() {
  const distRoot = path.resolve(arg("dist",
    process.env.MELEELIGHT_CLONE ||
    path.join(process.env.HOME, ".cache", "meleelight-funkey-s", "upstream")));
  const outArg = arg("out", "");
  if (!outArg || outArg === true) {
    console.error("--out <dir> is required");
    process.exit(1);
  }
  const outDir = path.resolve(outArg);
  if (!fs.existsSync(path.join(distRoot, "dist", "meleelight.html"))) {
    console.error(`--dist must point at a built upstream clone ` +
      `(missing ${distRoot}/dist/meleelight.html); run oracle/build-upstream.sh`);
    process.exit(1);
  }

  let selected = STAGES;
  const only = arg("only", null);
  if (only && only !== true) {
    const names = String(only).split(",");
    selected = names.map((n) => {
      const s = STAGES.find((st) => st.name === n);
      if (!s) {
        console.error(`unknown stage "${n}" (registered: ` +
          STAGES.map((st) => st.name).join(", ") + ")");
        process.exit(1);
      }
      return s;
    });
  }

  const upstreamHead = execFileSync("git", ["-C", distRoot, "rev-parse", "HEAD"],
    { encoding: "utf8" }).trim();

  fs.mkdirSync(outDir, { recursive: true });
  const manifest = {
    schema: "meleelight-funkey-s data pipeline manifest v1",
    upstreamHead,
    stages: {},
  };
  for (const stage of selected) {
    console.error(`stage ${stage.name}:`);
    manifest.stages[stage.name] = stage.run({ distRoot, outDir, log: console.error });
  }

  const manifestStr = stableStringify(manifest);
  fs.writeFileSync(path.join(outDir, "manifest.json"), manifestStr);
  console.log(`${path.relative(process.cwd(), outDir)}/manifest.json: ` +
    `${selected.length} stage(s) [${selected.map((s) => s.name).join(", ")}], ` +
    `sha256 ${sha256(manifestStr)}`);
}

main();
