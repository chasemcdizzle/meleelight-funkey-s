#!/usr/bin/env bash
# Task-level done-check for fix_plan §M1 task 1 (pipeline skeleton +
# animations serializer). Proves, per the M1 contract:
#   1. byte-stability — two FRESH pipeline runs produce byte-identical
#      manifests (and therefore identical artifact hash sets),
#   2. integrity — every artifact re-hashes to its manifest entry, no
#      stray files,
#   3. coverage — the pinned expected.json contract holds, including the
#      live 754-file reconciliation against the upstream src tree,
#   4. round-trip — the in-run decoder comparison ran (it is a hard gate
#      inside the stage; a run that completes has passed it).
# Prints ANIMATIONS OK and exits 0 on success.
set -euo pipefail
cd "$(dirname "$0")"

DIST="${MELEELIGHT_CLONE:-$HOME/.cache/meleelight-funkey-s/upstream}"

rm -rf build/check-a build/check-b
node run.js --only animations --dist "$DIST" --out build/check-a
node run.js --only animations --dist "$DIST" --out build/check-b

cmp build/check-a/manifest.json build/check-b/manifest.json \
  || { echo "FAIL: manifests differ between fresh runs (byte-stability)"; exit 1; }
echo "byte-stability: two fresh runs -> identical manifest.json"

node lib/verify-artifacts.js build/check-a
node lib/verify-artifacts.js build/check-b
node lib/check-expected.js build/check-a "$DIST"

echo "ANIMATIONS OK"
