#!/usr/bin/env bash
# oracle/fdlibm-crosscheck/run.sh — M0 task 3 done-check.
#
# Proves the vendored fdlibm is bit-identical on BOTH sides of the oracle:
#   1. CONSTANTS  every decimal literal in fdlibm.c AND fdlibm.js matches
#                 its commented IEEE-754 bit pattern (kills the shared
#                 transcription-typo class the bit-compare can't see).
#   2. SWEEP      C (host cc, -ffp-contract=off) vs JS (node) over a
#                 deterministic edge-case + seeded sweep (~257k inputs,
#                 dense at every algorithm threshold, denormals, +-0,
#                 extremes, sim ranges) — byte-identical bit patterns.
#   3. SANITY     C results within 16 ulp of node's native Math (an
#                 independent implementation; catches gross errors shared
#                 by both ports). Informational bound, never an equality
#                 gate — the bit-exact compares above/below are the gates.
#   4. GOLDEN     golden #1's full recorded Math call stream (fresh
#                 harness run, --capture-math) replayed through the C and
#                 the JS side AND compared against the browser-observed
#                 outputs — three-way byte-identical.
#
# Needs: host cc, node, the harness deps (oracle/harness: npm install),
# and a built upstream clone (oracle/build-upstream.sh) at
# ${MELEELIGHT_CLONE:-$HOME/.cache/meleelight-funkey-s/upstream}.
#
# Prints CROSSCHECK OK and exits 0 iff everything above holds.
set -euo pipefail
cd "$(dirname "$0")"

CLONE="${MELEELIGHT_CLONE:-$HOME/.cache/meleelight-funkey-s/upstream}"
GOLDEN_TRACE="../goldens/g01-fox-marth-battlefield.trace.json"
mkdir -p out

echo "== [1/4] constants vs commented bit patterns (C + JS) =="
node check-constants.js ../../port/fdlibm/fdlibm.c ../../port/fdlibm/fdlibm.js

echo "== [2/4] deterministic sweep: C vs JS bit-exact =="
node gen-inputs.js out/inputs.txt
cc -O2 -ffp-contract=off -std=c99 -Wall -I../../port/fdlibm \
  csweep.c ../../port/fdlibm/fdlibm.c -o out/csweep
./out/csweep out/inputs.txt > out/c-sweep.txt
node jssweep.js out/inputs.txt > out/js-sweep.txt
cmp out/c-sweep.txt out/js-sweep.txt
echo "sweep: C and JS byte-identical ($(wc -l < out/c-sweep.txt | tr -d ' ') lines)"

echo "== [3/4] ulp sanity vs an independent implementation =="
node sanity-ulp.js out/c-sweep.txt

echo "== [4/4] golden #1 Math call stream: browser vs C vs JS =="
if [ ! -f "$CLONE/dist/meleelight.html" ]; then
  echo "no built upstream at $CLONE — run oracle/build-upstream.sh first" >&2
  exit 1
fi
(cd ../harness && node run.js --dist "$CLONE" --trace "$GOLDEN_TRACE" \
  --frames 3600 --seed 1337 --capture-math \
  --out ../fdlibm-crosscheck/out/g01-mathcap.json)
node extract-args.js out/g01-mathcap.json out/g01-args.txt out/g01-browser.txt
./out/csweep out/g01-args.txt > out/g01-c.txt
node jssweep.js out/g01-args.txt > out/g01-js.txt
cmp out/g01-c.txt out/g01-js.txt
cmp out/g01-js.txt out/g01-browser.txt
echo "golden stream: browser, C, JS byte-identical ($(wc -l < out/g01-c.txt | tr -d ' ') calls)"

echo "CROSSCHECK OK"
