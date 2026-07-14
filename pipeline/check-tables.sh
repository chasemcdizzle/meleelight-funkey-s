#!/usr/bin/env bash
# Task-level done-check for fix_plan §M1 task 2 (extractor bundle + engine
# tables -> generated C). Proves, per the M1 contract:
#   1. extractor bundle present/current — built from the REAL upstream data
#      modules with upstream's own docker node:8 webpack toolchain
#      (pipeline/extractor/build-extractor.sh, idempotent),
#   2. byte-stability — two FRESH pipeline runs (animations + tables)
#      produce byte-identical manifests (hence identical artifact bytes),
#   3. integrity — every artifact re-hashes to its manifest entry, no
#      stray files,
#   4. coverage — the pinned expected.json contract holds for BOTH stages
#      (counts per char: attributes + framesData + intangibility + ECB +
#      hitboxes),
#   5. ANIM1 cross-check — framesData/ECB per-state frame counts re-derived
#      against the SAME run's decoded animation binaries match the
#      measured-then-frozen reconciliation pins (tables-anim-xref.js),
#   6. round-trip — the generated C tables COMPILE (cc -ffp-contract=off)
#      and, executed, print a canonical leaf dump byte-identical to a
#      FRESH executed-JS walk of the extractor bundle: every emitted value
#      (IEEE-754 bit patterns for doubles, decimals for ints) is bit-equal
#      to the real executed data.
# Prints TABLES OK and exits 0 on success.
set -euo pipefail
cd "$(dirname "$0")"

DIST="${MELEELIGHT_CLONE:-$HOME/.cache/meleelight-funkey-s/upstream}"

bash extractor/build-extractor.sh

rm -rf build/tables-a build/tables-b
node run.js --only animations,tables --dist "$DIST" --out build/tables-a
node run.js --only animations,tables --dist "$DIST" --out build/tables-b

cmp build/tables-a/manifest.json build/tables-b/manifest.json \
  || { echo "FAIL: manifests differ between fresh runs (byte-stability)"; exit 1; }
echo "byte-stability: two fresh runs -> identical manifest.json"

node lib/verify-artifacts.js build/tables-a
node lib/verify-artifacts.js build/tables-b
node lib/check-expected.js build/tables-a "$DIST" animations,tables

node lib/tables-anim-xref.js build/tables-a

# Round-trip: compiled C tables vs a fresh executed-JS walk, bit-exact.
cc -std=c99 -O1 -ffp-contract=off -Wall -Wextra -Werror \
  -Ibuild/tables-a -o build/tables-a/tables_check \
  lib/tables_check.c build/tables-a/ml_tables.c
./build/tables-a/tables_check > build/tables-a/c.dump
node lib/tables-dump.js "$DIST" > build/tables-a/js.dump
cmp build/tables-a/c.dump build/tables-a/js.dump \
  || { echo "FAIL: C tables dump != fresh executed-JS walk (round-trip)"; exit 1; }
LEAVES=$(wc -l < build/tables-a/c.dump | tr -d ' ')
echo "round-trip: compiled C tables == fresh executed-JS walk ($LEAVES leaf values, bit-exact)"
rm -f build/tables-a/tables_check build/tables-a/c.dump build/tables-a/js.dump

echo "TABLES OK"
