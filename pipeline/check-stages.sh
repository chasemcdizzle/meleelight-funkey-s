#!/usr/bin/env bash
# Task-level done-check for fix_plan §M1 task 3 (stage geometry -> generated
# C tables). Proves, per the M1 contract:
#   1. extractor bundle present/current — the six REAL upstream VS-stage
#      modules bundled with upstream's own docker node:8 webpack toolchain
#      (pipeline/extractor/build-extractor.sh; god-module externals-stubbed
#      with a hard DOM-leak guard),
#   2. byte-stability — two FRESH pipeline runs produce byte-identical
#      manifests (hence identical artifact bytes),
#   3. integrity — every artifact re-hashes to its manifest entry,
#   4. coverage — the pinned expected.json stage contract holds (6 VS
#      stages; per-stage polygon/surface/ledge/connected/movingPlats
#      element counts measured-then-frozen from executed data),
#   5. round-trip — the generated C tables COMPILE (cc -ffp-contract=off)
#      and, executed, print a canonical leaf dump byte-identical to a
#      FRESH executed-JS walk of the extractor bundle: every emitted value
#      (IEEE-754 bit patterns for doubles, decimals for ints) is bit-equal
#      to the real executed stage data.
# Prints STAGES OK and exits 0 on success.
set -euo pipefail
cd "$(dirname "$0")"

DIST="${MELEELIGHT_CLONE:-$HOME/.cache/meleelight-funkey-s/upstream}"

bash extractor/build-extractor.sh

rm -rf build/stages-a build/stages-b
node run.js --only stages --dist "$DIST" --out build/stages-a
node run.js --only stages --dist "$DIST" --out build/stages-b

cmp build/stages-a/manifest.json build/stages-b/manifest.json \
  || { echo "FAIL: manifests differ between fresh runs (byte-stability)"; exit 1; }
echo "byte-stability: two fresh runs -> identical manifest.json"

node lib/verify-artifacts.js build/stages-a
node lib/verify-artifacts.js build/stages-b
node lib/check-expected.js build/stages-a "$DIST" stages

# Round-trip: compiled C stage tables vs a fresh executed-JS walk, bit-exact.
cc -std=c99 -O1 -ffp-contract=off -Wall -Wextra -Werror \
  -Ibuild/stages-a -o build/stages-a/stages_check \
  lib/stages_check.c build/stages-a/ml_stages.c
./build/stages-a/stages_check > build/stages-a/c.dump
node lib/stages-dump.js "$DIST" > build/stages-a/js.dump
cmp build/stages-a/c.dump build/stages-a/js.dump \
  || { echo "FAIL: C stage dump != fresh executed-JS walk (round-trip)"; exit 1; }
LEAVES=$(wc -l < build/stages-a/c.dump | tr -d ' ')
echo "round-trip: compiled C stage tables == fresh executed-JS walk ($LEAVES leaf values, bit-exact)"
rm -f build/stages-a/stages_check build/stages-a/c.dump build/stages-a/js.dump

echo "STAGES OK"
