#!/usr/bin/env bash
# oracle/verify_goldens.sh — the M0 EXIT GATE (CLAUDE.md §Gates / §Commands).
#
# Usage: bash oracle/verify_goldens.sh
#
# For EVERY golden in oracle/goldens/manifest.json (the single param
# source):
#   1. TWO fresh browser runs of the golden's trace via the oracle harness
#      (seeded mulberry32 PRNG + virtual clock + fdlibm shim, default-on);
#   2. compare.js: the two fresh runs are bit-identical to each other;
#   3. verify-stream.js: BOTH runs match the committed frozen stream
#      oracle/goldens/<name>.sha256.json — exact string equality per frame,
#      FULL length, rngCalls + rngCallsOutsideStep equality, specVersion /
#      trace-sha256 / param pins, frozen-file integrity seal;
#   4. oracle/qjs/replay.sh: the fdlibm-patched QuickJS oracle runtime
#      reproduces the same stream (judged by the same verify-stream.js).
# Plus the manifest COVERAGE assertion: all 5 characters (0-4) and all 6
# VS stages (0-5) appear across the set, and >=1 golden is a CPU trace.
#
# PASS: prints "ALL GOLDENS OK", exit 0. Any divergence, missing artifact,
# or coverage shortfall: nonzero exit. Exact equality only — no epsilon,
# no prefix, no frame-skip (CLAUDE.md HARD RULE 3).
set -euo pipefail
cd "$(dirname "$0")"

DIST="${MELEELIGHT_CLONE:-$HOME/.cache/meleelight-funkey-s/upstream}"
if [ ! -f "$DIST/dist/meleelight.html" ]; then
  echo "verify_goldens.sh: no built upstream at $DIST — run oracle/build-upstream.sh" >&2
  exit 1
fi
if [ ! -x qjs/build-host/qjs-oracle ]; then
  echo "verify_goldens.sh: qjs/build-host/qjs-oracle missing — run oracle/qjs/build.sh" >&2
  exit 1
fi

# --- manifest coverage assertion (computed from the manifest, so a golden
# --- removed or re-parameterized later cannot silently shrink coverage) ---
echo "verify_goldens.sh: manifest coverage assertion"
IDS="$(cd harness && node -e '
  const { loadManifest, GOLDENS_DIR } = require("./streamlib");
  const fs = require("fs"), path = require("path");
  const gs = loadManifest().goldens;
  if (gs.length < 1) { console.error("COVERAGE FAIL: empty manifest"); process.exit(1); }
  const chars = new Set(), stages = new Set();
  let cpus = 0;
  const seen = new Set();
  for (const g of gs) {
    if (seen.has(g.id)) { console.error("COVERAGE FAIL: duplicate id " + g.id); process.exit(1); }
    seen.add(g.id);
    chars.add(g.p1); chars.add(g.p2);
    stages.add(g.stage);
    if (g.cpu) cpus++;
    for (const f of [g.trace, g.name + ".sha256.json"]) {
      if (!fs.existsSync(path.join(GOLDENS_DIR, f))) {
        console.error("COVERAGE FAIL: missing artifact oracle/goldens/" + f);
        process.exit(1);
      }
    }
  }
  const want = (label, got, need) => {
    for (const v of need) if (!got.has(v)) {
      console.error(`COVERAGE FAIL: ${label} ${v} not covered by any golden`);
      process.exit(1);
    }
  };
  want("character", chars, [0, 1, 2, 3, 4]);
  want("stage", stages, [0, 1, 2, 3, 4, 5]);
  if (cpus < 1) { console.error("COVERAGE FAIL: no CPU golden in the set"); process.exit(1); }
  console.error(`coverage: ${gs.length} goldens, chars {0,1,2,3,4}, stages {0,1,2,3,4,5}, cpu goldens: ${cpus}`);
  console.log(gs.map((g) => g.id).join(" "));
')"

# --- per-golden verification --------------------------------------------
for ID in $IDS; do
  eval "$(cd harness && node -e '
    const g = require("./streamlib").goldenById(process.argv[1]);
    const sq = (s) => "'\''" + String(s) + "'\''";
    console.log("NAME=" + sq(g.name));
    console.log("TRACE=" + sq(g.trace));
    console.log("FRAMES=" + sq(g.frames));
    console.log("SEED=" + sq(g.seed));
    console.log("P1=" + sq(g.p1));
    console.log("P2=" + sq(g.p2));
    console.log("STAGE=" + sq(g.stage));
    console.log("CPU=" + sq(g.cpu));
    console.log("DIFFICULTY=" + sq(g.difficulty));
  ' "$ID")"

  RUN=(node run.js --dist "$DIST" --trace "../goldens/$TRACE"
       --frames "$FRAMES" --seed "$SEED"
       --p1 "$P1" --p2 "$P2" --stage "$STAGE")
  if [ "$CPU" = "true" ]; then
    RUN+=(--cpu --difficulty "$DIFFICULTY")
  fi

  echo "verify_goldens.sh: $NAME — fresh browser run A"
  (cd harness && "${RUN[@]}" --out "out/verify-$ID-a.json")
  echo "verify_goldens.sh: $NAME — fresh browser run B"
  (cd harness && "${RUN[@]}" --out "out/verify-$ID-b.json")

  echo "verify_goldens.sh: $NAME — run A vs run B (bit-identity)"
  (cd harness && node compare.js "out/verify-$ID-a.json" "out/verify-$ID-b.json")

  echo "verify_goldens.sh: $NAME — runs vs frozen stream"
  (cd harness && node verify-stream.js "out/verify-$ID-a.json" "../goldens/$NAME.sha256.json")
  (cd harness && node verify-stream.js "out/verify-$ID-b.json" "../goldens/$NAME.sha256.json")

  echo "verify_goldens.sh: $NAME — QuickJS oracle-runtime replay"
  bash qjs/replay.sh "$ID"

  echo "GOLDEN OK $ID ($NAME)"
done

echo "ALL GOLDENS OK"
