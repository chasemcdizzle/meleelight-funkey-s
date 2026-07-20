#!/usr/bin/env bash
# Task-level check for fix_plan §M4 task 11 (target-test stage data ->
# generated C tables). Proves, per the M1 contract EXTENDED to the M4
# targets stage (fix_plan §M4 conventions: extended measured-then-frozen,
# never weakened):
#   1. extractor bundle present/current — the ten REAL upstream authored
#      target-stage modules bundled with upstream's own docker node:8
#      webpack toolchain (pipeline/extractor/build-extractor.sh; the
#      __targetStages grep guard + hard DOM-leak guard),
#   2. byte-stability — two FRESH pipeline runs produce byte-identical
#      manifests (hence identical artifact bytes),
#   3. integrity — every artifact re-hashes to its manifest entry,
#   4. coverage — the pinned expected.json targets contract holds (10
#      authored stages; per-stage box/surface/ledge/ledgePos/target
#      element counts measured-then-frozen from executed data; the
#      targetstage9 ledgePos quirk carried verbatim),
#   5. round-trip — the generated C tables COMPILE (cc -ffp-contract=off)
#      and, executed, print a canonical leaf dump byte-identical to a
#      FRESH executed-JS walk of the extractor bundle: every emitted value
#      (IEEE-754 bit patterns for doubles, decimals for ints) is bit-equal
#      to the real executed target-stage data.
# Prints TARGETS OK and exits 0 on success.
set -euo pipefail
cd "$(dirname "$0")"

DIST="${MELEELIGHT_CLONE:-$HOME/.cache/meleelight-funkey-s/upstream}"

bash extractor/build-extractor.sh

rm -rf build/targets-a build/targets-b
node run.js --only targets --dist "$DIST" --out build/targets-a
node run.js --only targets --dist "$DIST" --out build/targets-b

cmp build/targets-a/manifest.json build/targets-b/manifest.json \
  || { echo "FAIL: manifests differ between fresh runs (byte-stability)"; exit 1; }
echo "byte-stability: two fresh runs -> identical manifest.json"

node lib/verify-artifacts.js build/targets-a
node lib/verify-artifacts.js build/targets-b
node lib/check-expected.js build/targets-a "$DIST" targets

# Round-trip: compiled C target tables vs a fresh executed-JS walk, bit-exact.
cc -std=c99 -O1 -ffp-contract=off -Wall -Wextra -Werror \
  -Ibuild/targets-a -o build/targets-a/targets_check \
  lib/targets_check.c build/targets-a/ml_targets.c
./build/targets-a/targets_check > build/targets-a/c.dump
node lib/targets-dump.js "$DIST" > build/targets-a/js.dump
cmp build/targets-a/c.dump build/targets-a/js.dump \
  || { echo "FAIL: C target dump != fresh executed-JS walk (round-trip)"; exit 1; }
LEAVES=$(wc -l < build/targets-a/c.dump | tr -d ' ')
if [ "$LEAVES" != "718" ]; then
  echo "FAIL: round-trip leaf count $LEAVES != pinned 718 (measured-then-frozen, iter 94)"
  exit 1
fi
echo "round-trip: compiled C target tables == fresh executed-JS walk ($LEAVES leaf values, bit-exact)"
rm -f build/targets-a/targets_check build/targets-a/c.dump build/targets-a/js.dump

echo "TARGETS OK"
