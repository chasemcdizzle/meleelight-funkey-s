#!/usr/bin/env bash
# M2-CAL EXIT GATE (CLAUDE.md §Gates / §Commands; PLAN §4 M2-CAL).
#
# Proves the calibration slice end-to-end:
#   1. the structure-parallel C translation builds (-ffp-contract=off);
#   2. module-boundary captures exist for g01/g04/g06 (recorded fresh via
#      run-capture.js when absent) and each capture's checksum stream
#      passes the UNCHANGED oracle/harness/verify-stream.js against the
#      frozen golden (instrumentation non-perturbation);
#   3. capture pins hold (expected-capture.json — counts, no-undef-ret);
#   4. EVERY recorded boundary call of ALL three captures replays through
#      the C module bit-identically (--strict: a single differing bit
#      anywhere fails the gate);
#   5. docs/M2CAL-REPORT.md carries the burn-down metrics + verdict.
# Prints ENVCOLL MATCH, exit 0. Never weakened: exact equality only.
set -euo pipefail
cd "$(dirname "$0")/../.."

CAL=port/sim/calib
BUILD=$CAL/build
mkdir -p "$BUILD"

# 1. build
cc -O2 -ffp-contract=off -Wall -Wextra -Werror \
  -o "$BUILD/envcoll_replay" \
  "$CAL/replay_envcoll.c" "$CAL/canon.c" \
  port/sim/environmental_collision.c \
  port/fdlibm/fdlibm.c -lm
echo "build OK (cc -O2 -ffp-contract=off)"

for id in g01 g04 g06; do
  name=$(node -e "const m=require('./oracle/goldens/manifest.json');console.log(m.goldens.find(g=>g.id==='$id').name)")
  if [ ! -s "$BUILD/$id.envcoll.jsonl" ] || [ ! -s "$BUILD/$id.capture-run.json" ]; then
    echo "== $id: capture missing — recording fresh"
    node "$CAL/run-capture.js" --golden "$id" \
      --out-jsonl "$BUILD/$id.envcoll.jsonl" --out-run "$BUILD/$id.capture-run.json"
  fi
  # 2. non-perturbation guard (always re-judged, cheap)
  node oracle/harness/verify-stream.js "$BUILD/$id.capture-run.json" \
    "oracle/goldens/$name.sha256.json"
  # 3. capture pins
  node "$CAL/check-capture-pins.js" "$id" "$BUILD/$id.envcoll.jsonl" \
    "$BUILD/$id.capture-run.json"
  # 4. bit-exact replay, full record set
  "$BUILD/envcoll_replay" "$BUILD/$id.envcoll.jsonl" --strict --max-print 5
done

# 5. the calibration report must exist with its metrics filled
REPORT=docs/M2CAL-REPORT.md
for needle in "Divergences (replay comparator)" "divergences/KLOC" \
              "Projection" "VERDICT: GO"; do
  if ! grep -q "$needle" "$REPORT"; then
    echo "ENVCOLL GATE FAIL: $REPORT missing '$needle'" >&2
    exit 1
  fi
done

# no-commit guard
if git status --porcelain -- "$BUILD" | grep -q .; then
  echo "ENVCOLL GATE FAIL: build output not gitignored" >&2
  exit 1
fi

echo "ENVCOLL MATCH"
