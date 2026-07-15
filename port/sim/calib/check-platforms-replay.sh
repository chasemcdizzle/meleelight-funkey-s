#!/usr/bin/env bash
# M2 task 14 done-check: movingPlatforms stage-tick logic
# (src/stages/vs-stages/*.js — the M1-externals-stubbed god-module
# bodies) — capture + bit-exact replay. For each captured golden
# (g01 g02 g06: g02 ystory + g06 fountain are the ONLY stages with
# non-empty movingPlatforms bodies upstream — measured; g01 battlefield
# represents the static-stage class, its lean records chain-verifying
# that the platform plane never drifts):
#   - two fresh platforms-spec capture runs produce byte-identical JSONL
#     (determinism; includes the 52-call rule-11/12 sweep — Randall's four
#     rail arms + both double-fire corners + the rider arm, fountain's
#     starting reset / arrival / decrement / all three destination-
#     selection and all three newTimer arms / both moving directions /
#     all four transfer arms + negatives — and fountain's module-private
#     platformStates observed via the run-capture.js served-bytes getter)
#   - BOTH runs' checksum streams pass the unchanged verify-stream.js
#     against the frozen golden (instrumentation non-perturbation guard)
#   - measured-then-frozen pins hold (record/function counts, undef-ret
#     allowlist, post-state field set)
#   - EVERY record replays through the C translation
#     (port/sim/stages/{moving_platforms,ystory,fountain}.c)
#     bit-identically (--strict): per-stage pre/post envelopes, the
#     rule-18 STAGE-PLANE CHAIN instrument across in-match records
#     (platform plane on every stage + platformStates on fountain), the
#     full seeded-RNG chain draw-for-draw (frame-0 records on the
#     separate sweep chain), fountain owner draws in call order
#     (FORMAT.md "The platforms spec").
# Leaves run A's artifacts in port/sim/calib/build/ as the canonical
# captures. Prints PLATFORMS MATCH, exit 0. Never weakened: exact
# equality only.
set -euo pipefail
cd "$(dirname "$0")/../../.."

CAL=port/sim/calib
BUILD=$CAL/build
mkdir -p "$BUILD"

# build: every TU with -ffp-contract=off (PLAN §2); no M1 tables needed —
# the moving-platform constants are upstream CODE literals, not STAB1 data
cc -O2 -ffp-contract=off -Wall -Wextra -Werror \
  -o "$BUILD/platforms_replay" \
  "$CAL/replay_platforms.c" "$CAL/canon.c" \
  port/sim/stages/moving_platforms.c \
  port/sim/stages/ystory.c \
  port/sim/stages/fountain.c \
  port/sim/ml_events.c -lm
echo "build OK: $BUILD/platforms_replay (cc -O2 -ffp-contract=off)"

for id in g01 g02 g06; do
  name=$(node -e "const m=require('./oracle/goldens/manifest.json');console.log(m.goldens.find(g=>g.id==='$id').name)")
  echo "== $id ($name): platforms capture run A"
  node "$CAL/run-capture.js" --spec platforms --golden "$id" \
    --out-jsonl "$BUILD/$id.platforms.jsonl" \
    --out-run "$BUILD/$id.platforms-run.json"
  echo "== $id: platforms capture run B"
  node "$CAL/run-capture.js" --spec platforms --golden "$id" \
    --out-jsonl "$BUILD/$id.platforms.b.jsonl" \
    --out-run "$BUILD/$id.platforms-run.b.json"
  cmp "$BUILD/$id.platforms.jsonl" "$BUILD/$id.platforms.b.jsonl"
  echo "   capture byte-identical across two fresh runs"
  node oracle/harness/verify-stream.js "$BUILD/$id.platforms-run.json"   "oracle/goldens/$name.sha256.json"
  node oracle/harness/verify-stream.js "$BUILD/$id.platforms-run.b.json" "oracle/goldens/$name.sha256.json"
  node "$CAL/check-spec-pins.js" platforms "$id" \
    "$BUILD/$id.platforms.jsonl" "$BUILD/$id.platforms-run.json"
  rm -f "$BUILD/$id.platforms.b.jsonl" "$BUILD/$id.platforms-run.b.json"
  # bit-exact replay: stage-plane chain + post envelopes + RNG chains
  "$BUILD/platforms_replay" "$BUILD/$id.platforms.jsonl" --strict --max-print 5
done

# no-commit guard: captures are build output, never tracked
if git status --porcelain -- "$BUILD" | grep -q .; then
  echo "PLATFORMS FAIL: build output not gitignored" >&2
  exit 1
fi

echo "PLATFORMS MATCH"
