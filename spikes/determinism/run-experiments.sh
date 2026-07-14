#!/usr/bin/env bash
# Runs the four determinism-spike experiments end to end.
# Prereq: a built, patched meleelight clone (see README.md), path in $1.
set -euo pipefail
CLONE="${1:?usage: run-experiments.sh /path/to/built-meleelight-clone}"
cd "$(dirname "$0")/harness"
mkdir -p out

echo "== A: seeded RNG, 2 humans, two fresh page loads =="
node run.js --dist "$CLONE" --frames 3600 --seed 42 --out out/a1.json
node run.js --dist "$CLONE" --frames 3600 --seed 42 --out out/a2.json
node compare.js out/a1.json out/a2.json || true

echo "== B: NATIVE (unseeded) Math.random, 2 humans =="
node run.js --dist "$CLONE" --frames 3600 --native-rng --out out/b1.json
node run.js --dist "$CLONE" --frames 3600 --native-rng --out out/b2.json
node compare.js out/b1.json out/b2.json || true

echo "== C: seeded RNG, P2 = CPU (difficulty 5) =="
node run.js --dist "$CLONE" --frames 3600 --seed 42 --cpu --out out/c1.json
node run.js --dist "$CLONE" --frames 3600 --seed 42 --cpu --out out/c2.json
node compare.js out/c1.json out/c2.json || true

echo "== D: transcendental exposure (from A/C coverage.mathCalls) =="
node -e '
for (const f of ["out/a1.json","out/c1.json"]) {
  const j = JSON.parse(require("fs").readFileSync(f));
  console.log(f, JSON.stringify(j.coverage.mathCalls));
}'
