#!/usr/bin/env bash
# M2 task 3 done-check: interpretInputs + the 8-deep input buffer +
# meleeInputs — capture + bit-exact replay. For each captured golden
# (g01 g04 g06):
#   - two fresh input-spec capture runs produce byte-identical JSONL
#     (determinism; includes the fixed synthetic-domain sweep)
#   - BOTH runs' checksum streams pass the unchanged verify-stream.js
#     against the frozen golden (instrumentation non-perturbation guard —
#     also proves the sweep perturbs nothing)
#   - measured-then-frozen pins hold (record/function counts, the
#     void-mutator undef-ret allowlist)
#   - EVERY record replays through the C translations bit-identically
#     (--strict: a single differing bit fails): pure records marshal->
#     call->compare; the interpretInputs CHAIN records (pollInputs ->
#     physics args projection) drive the C buffer state machine over the
#     full 3600-frame recurrence — slot 0 poll injection, z/s always-shift,
#     pause-aware pastOffset, pause/frameAdvance bookkeeping, end-of-tick
#     frameByFrame handling — chained from the C values, never the capture.
# Leaves run A's artifacts in port/sim/calib/build/ as the canonical
# captures. Prints INPUT MATCH, exit 0. Never weakened: exact equality
# only.
set -euo pipefail
cd "$(dirname "$0")/../../.."

CAL=port/sim/calib
BUILD=$CAL/build
mkdir -p "$BUILD"

# build: every TU with -ffp-contract=off (PLAN §2)
cc -O2 -ffp-contract=off -Wall -Wextra -Werror \
  -o "$BUILD/input_replay" \
  "$CAL/replay_input.c" "$CAL/input_canon.c" "$CAL/canon.c" \
  port/sim/input/interpret_inputs.c \
  -lm
echo "build OK: $BUILD/input_replay (cc -O2 -ffp-contract=off)"

for id in g01 g04 g06; do
  name=$(node -e "const m=require('./oracle/goldens/manifest.json');console.log(m.goldens.find(g=>g.id==='$id').name)")
  echo "== $id ($name): input capture run A"
  node "$CAL/run-capture.js" --spec input --golden "$id" \
    --out-jsonl "$BUILD/$id.input.jsonl" --out-run "$BUILD/$id.input-run.json"
  echo "== $id: input capture run B"
  node "$CAL/run-capture.js" --spec input --golden "$id" \
    --out-jsonl "$BUILD/$id.input.b.jsonl" --out-run "$BUILD/$id.input-run.b.json"
  cmp "$BUILD/$id.input.jsonl" "$BUILD/$id.input.b.jsonl"
  echo "   capture byte-identical across two fresh runs"
  node oracle/harness/verify-stream.js "$BUILD/$id.input-run.json"   "oracle/goldens/$name.sha256.json"
  node oracle/harness/verify-stream.js "$BUILD/$id.input-run.b.json" "oracle/goldens/$name.sha256.json"
  node "$CAL/check-spec-pins.js" input "$id" "$BUILD/$id.input.jsonl" "$BUILD/$id.input-run.json"
  rm -f "$BUILD/$id.input.b.jsonl" "$BUILD/$id.input-run.b.json"
  # bit-exact replay, full record set (chain + pure records)
  "$BUILD/input_replay" "$BUILD/$id.input.jsonl" --strict --max-print 5
done

# no-commit guard: captures are build output, never tracked
if git status --porcelain -- "$BUILD" | grep -q .; then
  echo "INPUT FAIL: build output not gitignored" >&2
  exit 1
fi

echo "INPUT MATCH"
