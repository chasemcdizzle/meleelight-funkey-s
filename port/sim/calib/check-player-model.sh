#!/usr/bin/env bash
# M2 task 2 done-check: player/game-state value model + mutation-capture
# rig upgrade. For each captured golden (g01 g04 g06):
#   - two fresh player-spec capture runs produce byte-identical JSONL
#     (determinism; also proves the snapshot projection excludes every
#     timing-dependent field — percentShake would break this by
#     construction)
#   - BOTH runs' checksum streams pass the unchanged verify-stream.js
#     against the frozen golden (instrumentation non-perturbation guard)
#   - measured-then-frozen pins hold (record/function counts, 5-field
#     post-state records, void-mutator undef-ret allowlist)
#   - EVERY recorded post-update(i) snapshot round-trips through the C
#     value model canon->MlPlayer->canon byte-identically, survives the
#     deep-copy independence probe, and satisfies the deepObjectMerge
#     property check (--strict: a single differing bit fails)
# Leaves run A's artifacts in port/sim/calib/build/ as the canonical
# captures. Prints PLAYER MODEL MATCH, exit 0. Never weakened: exact
# equality only.
set -euo pipefail
cd "$(dirname "$0")/../../.."

CAL=port/sim/calib
BUILD=$CAL/build
mkdir -p "$BUILD"

# build: every TU with -ffp-contract=off (PLAN §2)
cc -O2 -ffp-contract=off -Wall -Wextra -Werror \
  -o "$BUILD/player_replay" \
  "$CAL/replay_player.c" "$CAL/player_canon.c" "$CAL/canon.c" \
  -lm
echo "build OK: $BUILD/player_replay (cc -O2 -ffp-contract=off)"

for id in g01 g04 g06; do
  name=$(node -e "const m=require('./oracle/goldens/manifest.json');console.log(m.goldens.find(g=>g.id==='$id').name)")
  echo "== $id ($name): player capture run A"
  node "$CAL/run-capture.js" --spec player --golden "$id" \
    --out-jsonl "$BUILD/$id.player.jsonl" --out-run "$BUILD/$id.player-run.json"
  echo "== $id: player capture run B"
  node "$CAL/run-capture.js" --spec player --golden "$id" \
    --out-jsonl "$BUILD/$id.player.b.jsonl" --out-run "$BUILD/$id.player-run.b.json"
  cmp "$BUILD/$id.player.jsonl" "$BUILD/$id.player.b.jsonl"
  echo "   capture byte-identical across two fresh runs"
  node oracle/harness/verify-stream.js "$BUILD/$id.player-run.json"   "oracle/goldens/$name.sha256.json"
  node oracle/harness/verify-stream.js "$BUILD/$id.player-run.b.json" "oracle/goldens/$name.sha256.json"
  node "$CAL/check-spec-pins.js" player "$id" "$BUILD/$id.player.jsonl" "$BUILD/$id.player-run.json"
  rm -f "$BUILD/$id.player.b.jsonl" "$BUILD/$id.player-run.b.json"
  # bit-exact model round-trip + copy/merge probes, full record set
  "$BUILD/player_replay" "$BUILD/$id.player.jsonl" --strict --max-print 5
done

# no-commit guard: captures are build output, never tracked
if git status --porcelain -- "$BUILD" | grep -q .; then
  echo "PLAYER MODEL FAIL: build output not gitignored" >&2
  exit 1
fi

echo "PLAYER MODEL MATCH"
