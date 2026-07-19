#!/usr/bin/env bash
# M4 task 4 done-check: the ai.js structure-parallel C port, verified by
# strict record-by-record replay of the aiport captures. For each CPU
# golden (g07 g08):
#   - two fresh aiport-spec capture runs produce byte-identical JSONL
#     (determinism; the frame-0 sweep battery + the in-page write-set
#     recon run on both)
#   - BOTH runs' checksum streams pass the unchanged verify-stream.js
#     against the frozen golden (instrumentation + sweep non-perturbation
#     guard — rule 12: an unrestored sweep write or a stolen seeded draw
#     fails here mechanically)
#   - measured-then-frozen pins hold (expected-capture-aiport.json;
#     wsViol pinned ZERO)
#   - EVERY record replays through the C port bit-identically (--strict):
#     runAI records marshal the recorded pre state (strict, rule 7) into
#     MlAiSim/MlPlayer/tagged bank rows, call port/sim/ai.c's ml_runAI on
#     the chained C mulberry32 (frame-0 records on the mirrored sweep
#     generator), and compare the {bank, bk, rng} post envelope
#     byte-for-byte; standalone draws chain-verify draw-for-draw.
# Leaves run A's artifacts in port/sim/calib/build/ as the canonical
# captures. Prints AI MATCH, exit 0. Never weakened: exact equality only.
#
# The AIBRIDGE1 path (check-ai-bridge.sh, M2 task 16) is untouched by
# this check — fix_plan §M4 task 5 retires it from the live path.
set -euo pipefail
cd "$(dirname "$0")/../../.."

CAL=port/sim/calib
BUILD=$CAL/build
mkdir -p "$BUILD"

# build: every TU with -ffp-contract=off (PLAN §2); fdlibm linked for the
# ai.js transcendental surface (pow/atan/cos/sin/tan)
cc -O2 -ffp-contract=off -Wall -Wextra -Werror \
  -o "$BUILD/ai_port_replay" \
  "$CAL/replay_ai_port.c" port/sim/ai.c "$CAL/canon.c" \
  port/sim/ai_bridge.c port/sim/ml_events.c port/fdlibm/fdlibm.c \
  -lm
echo "build OK: $BUILD/ai_port_replay (cc -O2 -ffp-contract=off)"

for id in g07 g08; do
  name=$(node -e "const m=require('./oracle/goldens/manifest.json');console.log(m.goldens.find(g=>g.id==='$id').name)")
  echo "== $id ($name): aiport capture run A"
  node "$CAL/run-capture.js" --spec aiport --golden "$id" \
    --out-jsonl "$BUILD/$id.aiport.jsonl" --out-run "$BUILD/$id.aiport-run.json"
  echo "== $id: aiport capture run B"
  node "$CAL/run-capture.js" --spec aiport --golden "$id" \
    --out-jsonl "$BUILD/$id.aiport.b.jsonl" --out-run "$BUILD/$id.aiport-run.b.json"
  cmp "$BUILD/$id.aiport.jsonl" "$BUILD/$id.aiport.b.jsonl"
  echo "   capture byte-identical across two fresh runs"
  node oracle/harness/verify-stream.js "$BUILD/$id.aiport-run.json"   "oracle/goldens/$name.sha256.json"
  node oracle/harness/verify-stream.js "$BUILD/$id.aiport-run.b.json" "oracle/goldens/$name.sha256.json"
  node "$CAL/check-spec-pins.js" aiport "$id" "$BUILD/$id.aiport.jsonl" "$BUILD/$id.aiport-run.json"
  rm -f "$BUILD/$id.aiport.b.jsonl" "$BUILD/$id.aiport-run.b.json"
  # bit-exact replay, full record set (pre marshal + C runAI + post compare)
  "$BUILD/ai_port_replay" "$BUILD/$id.aiport.jsonl" --strict --max-print 5
done

# no-commit guard: captures are build output, never tracked
if git status --porcelain -- "$BUILD" | grep -q .; then
  echo "AI REPLAY FAIL: build output not gitignored" >&2
  exit 1
fi

echo "AI MATCH"
