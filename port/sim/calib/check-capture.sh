#!/usr/bin/env bash
# M2-CAL task 1 done-check: module-boundary capture rig.
# For each captured golden (g01 g04 g06):
#   - two fresh capture runs produce byte-identical JSONL (determinism)
#   - BOTH runs' checksum streams pass the unchanged verify-stream.js
#     against the frozen golden (instrumentation non-perturbation guard)
#   - measured-then-frozen pins hold (record/function counts, no-undef-ret)
# Leaves run A's artifacts in port/sim/calib/build/ as the canonical
# captures the replay driver consumes. Prints CAPTURE OK, exit 0.
set -euo pipefail
cd "$(dirname "$0")/../../.."

CAL=port/sim/calib
BUILD=$CAL/build
mkdir -p "$BUILD"

for id in g01 g04 g06; do
  name=$(node -e "const m=require('./oracle/goldens/manifest.json');console.log(m.goldens.find(g=>g.id==='$id').name)")
  echo "== $id ($name): capture run A"
  node "$CAL/run-capture.js" --golden "$id" \
    --out-jsonl "$BUILD/$id.envcoll.jsonl" --out-run "$BUILD/$id.capture-run.json"
  echo "== $id: capture run B"
  node "$CAL/run-capture.js" --golden "$id" \
    --out-jsonl "$BUILD/$id.envcoll.b.jsonl" --out-run "$BUILD/$id.capture-run.b.json"
  cmp "$BUILD/$id.envcoll.jsonl" "$BUILD/$id.envcoll.b.jsonl"
  echo "   capture byte-identical across two fresh runs"
  node oracle/harness/verify-stream.js "$BUILD/$id.capture-run.json"   "oracle/goldens/$name.sha256.json"
  node oracle/harness/verify-stream.js "$BUILD/$id.capture-run.b.json" "oracle/goldens/$name.sha256.json"
  node "$CAL/check-capture-pins.js" "$id" "$BUILD/$id.envcoll.jsonl" "$BUILD/$id.capture-run.json"
  rm -f "$BUILD/$id.envcoll.b.jsonl" "$BUILD/$id.capture-run.b.json"
done

# no-commit guard: captures are build output, never tracked
if git status --porcelain -- "$BUILD" | grep -q .; then
  echo "CAPTURE FAIL: build output not gitignored" >&2
  exit 1
fi

echo "CAPTURE OK"
