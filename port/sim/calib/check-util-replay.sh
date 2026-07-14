#!/usr/bin/env bash
# M2 task 1 done-check: util/math substrate — capture + bit-exact replay.
# For each captured golden (g01 g04 g06):
#   - two fresh util-spec capture runs produce byte-identical JSONL
#     (determinism)
#   - BOTH runs' checksum streams pass the unchanged verify-stream.js
#     against the frozen golden (instrumentation non-perturbation guard)
#   - measured-then-frozen pins hold (record/function counts, the
#     accessor-allowlisted no-undef-ret invariant)
#   - EVERY recorded boundary call replays through the C util translations
#     bit-identically (--strict: a single differing bit fails)
# Leaves run A's artifacts in port/sim/calib/build/ as the canonical
# captures. Prints UTIL MATCH, exit 0. Never weakened: exact equality only.
set -euo pipefail
cd "$(dirname "$0")/../../.."

CAL=port/sim/calib
BUILD=$CAL/build
mkdir -p "$BUILD"

# build: every TU with -ffp-contract=off (PLAN §2)
cc -O2 -ffp-contract=off -Wall -Wextra -Werror \
  -o "$BUILD/util_replay" \
  "$CAL/replay_util.c" "$CAL/canon.c" \
  port/sim/environmental_collision.c \
  port/fdlibm/fdlibm.c -lm
echo "build OK: $BUILD/util_replay (cc -O2 -ffp-contract=off)"

for id in g01 g04 g06; do
  name=$(node -e "const m=require('./oracle/goldens/manifest.json');console.log(m.goldens.find(g=>g.id==='$id').name)")
  echo "== $id ($name): util capture run A"
  node "$CAL/run-capture.js" --spec util --golden "$id" \
    --out-jsonl "$BUILD/$id.util.jsonl" --out-run "$BUILD/$id.util-run.json"
  echo "== $id: util capture run B"
  node "$CAL/run-capture.js" --spec util --golden "$id" \
    --out-jsonl "$BUILD/$id.util.b.jsonl" --out-run "$BUILD/$id.util-run.b.json"
  cmp "$BUILD/$id.util.jsonl" "$BUILD/$id.util.b.jsonl"
  echo "   capture byte-identical across two fresh runs"
  node oracle/harness/verify-stream.js "$BUILD/$id.util-run.json"   "oracle/goldens/$name.sha256.json"
  node oracle/harness/verify-stream.js "$BUILD/$id.util-run.b.json" "oracle/goldens/$name.sha256.json"
  node "$CAL/check-spec-pins.js" util "$id" "$BUILD/$id.util.jsonl" "$BUILD/$id.util-run.json"
  rm -f "$BUILD/$id.util.b.jsonl" "$BUILD/$id.util-run.b.json"
  # bit-exact replay, full record set
  "$BUILD/util_replay" "$BUILD/$id.util.jsonl" --strict --max-print 5
done

# no-commit guard: captures are build output, never tracked
if git status --porcelain -- "$BUILD" | grep -q .; then
  echo "UTIL FAIL: build output not gitignored" >&2
  exit 1
fi

echo "UTIL MATCH"
