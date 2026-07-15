#!/usr/bin/env bash
# M2 task 16 done-check: the AI-input bridge for CPU goldens. For each CPU
# golden (g07 g08):
#   - two fresh ai-spec capture runs produce byte-identical JSONL
#     (determinism; the in-page write-set recon runs on both)
#   - BOTH runs' checksum streams pass the unchanged verify-stream.js
#     against the frozen golden (instrumentation non-perturbation guard —
#     the per-call recon canons + runAI wrapper provably perturb nothing)
#   - measured-then-frozen pins hold (record/function counts; wsViol
#     pinned ZERO = the write-set reconciliation hard-check: the AI wrote
#     nothing outside aiInputBank[i][0] + its private player bookkeeping)
#   - the AIBRIDGE1 artifact distills deterministically (built twice,
#     byte-identical) — the replayable bridge task 17's sim consumes
#   - EVERY record replays through the C chain bit-identically (--strict):
#     the human slot through ml_interpret_inputs, the CPU slot through the
#     tagged core + the C bridge (artifact-driven bank installs, seeded
#     draws burned draw-for-draw at the runAI call sites, rule-18 bank
#     chain-verify at every poll, never-AI-written-field verification,
#     alias write-through at slot 0) — chained from the C values, never
#     the capture.
# Leaves run A's artifacts in port/sim/calib/build/ as the canonical
# captures. Prints AI BRIDGE OK, exit 0. Never weakened: exact equality
# only.
set -euo pipefail
cd "$(dirname "$0")/../../.."

CAL=port/sim/calib
BUILD=$CAL/build
mkdir -p "$BUILD"

# build: every TU with -ffp-contract=off (PLAN §2)
cc -O2 -ffp-contract=off -Wall -Wextra -Werror \
  -o "$BUILD/ai_replay" \
  "$CAL/replay_ai.c" "$CAL/input_canon.c" "$CAL/canon.c" \
  port/sim/input/interpret_inputs.c port/sim/ai_bridge.c \
  -lm
echo "build OK: $BUILD/ai_replay (cc -O2 -ffp-contract=off)"

for id in g07 g08; do
  name=$(node -e "const m=require('./oracle/goldens/manifest.json');console.log(m.goldens.find(g=>g.id==='$id').name)")
  echo "== $id ($name): ai capture run A"
  node "$CAL/run-capture.js" --spec ai --golden "$id" \
    --out-jsonl "$BUILD/$id.ai.jsonl" --out-run "$BUILD/$id.ai-run.json"
  echo "== $id: ai capture run B"
  node "$CAL/run-capture.js" --spec ai --golden "$id" \
    --out-jsonl "$BUILD/$id.ai.b.jsonl" --out-run "$BUILD/$id.ai-run.b.json"
  cmp "$BUILD/$id.ai.jsonl" "$BUILD/$id.ai.b.jsonl"
  echo "   capture byte-identical across two fresh runs"
  node oracle/harness/verify-stream.js "$BUILD/$id.ai-run.json"   "oracle/goldens/$name.sha256.json"
  node oracle/harness/verify-stream.js "$BUILD/$id.ai-run.b.json" "oracle/goldens/$name.sha256.json"
  node "$CAL/check-spec-pins.js" ai "$id" "$BUILD/$id.ai.jsonl" "$BUILD/$id.ai-run.json"
  rm -f "$BUILD/$id.ai.b.jsonl" "$BUILD/$id.ai-run.b.json"
  # the replayable bridge artifact: deterministic distillation (x2 + cmp)
  node "$CAL/build-ai-bridge.js" "$id" "$BUILD/$id.ai.jsonl" "$BUILD/$id.ai-bridge.txt"
  node "$CAL/build-ai-bridge.js" "$id" "$BUILD/$id.ai.jsonl" "$BUILD/$id.ai-bridge.b.txt"
  cmp "$BUILD/$id.ai-bridge.txt" "$BUILD/$id.ai-bridge.b.txt"
  rm -f "$BUILD/$id.ai-bridge.b.txt"
  echo "   bridge artifact deterministic (x2 byte-identical)"
  # bit-exact replay, full record set (chain + bridge + RNG stream)
  "$BUILD/ai_replay" "$BUILD/$id.ai.jsonl" "$BUILD/$id.ai-bridge.txt" \
    --strict --max-print 5
done

# no-commit guard: captures are build output, never tracked
if git status --porcelain -- "$BUILD" | grep -q .; then
  echo "AI BRIDGE FAIL: build output not gitignored" >&2
  exit 1
fi

echo "AI BRIDGE OK"
