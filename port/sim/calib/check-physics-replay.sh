#!/usr/bin/env bash
# M2 task 5 done-check: physics.js core + interpolatedCollision — capture +
# bit-exact replay. For each captured golden (g01 g04 g06):
#   - two fresh physics-spec capture runs produce byte-identical JSONL
#     (determinism; includes the fixed interpolatedCollision sweep and the
#     frame-0 asFlags dump, whose post-run finalCheck re-dump hard-fails
#     on any in-match move-flag drift)
#   - BOTH runs' checksum streams pass the unchanged verify-stream.js
#     against the frozen golden (instrumentation non-perturbation guard)
#   - measured-then-frozen pins hold (record/function counts, undef-ret
#     allowlist, post-state field set)
#   - EVERY record replays through the C translations bit-identically
#     (--strict): physics post envelopes {alias,hq,players,snd}, pure
#     interpolatedCollision returns, and every dispatch/getter SEAM in
#     call order with bit-exact arguments (FORMAT.md "The physics spec");
#     ecb/framesData/attribute reads go through the M1 CTAB1 generated
#     tables (ml_tables), regenerated here by the executed-JS pipeline.
# Leaves run A's artifacts in port/sim/calib/build/ as the canonical
# captures. Prints PHYSICS MATCH, exit 0. Never weakened: exact equality
# only.
set -euo pipefail
cd "$(dirname "$0")/../../.."

CAL=port/sim/calib
BUILD=$CAL/build
TABLES=pipeline/build/physics-tables
mkdir -p "$BUILD"

# M1 data plane (FORMATS.md section 3): CTAB1 tables for ecb/framesData/
# attributes. build-extractor.sh is stamp-cached; the run is executed-JS.
bash pipeline/extractor/build-extractor.sh
node pipeline/run.js --only animations,tables --out "$TABLES"
test -f "$TABLES/ml_tables.c"

# build: every TU with -ffp-contract=off (PLAN §2)
cc -O2 -ffp-contract=off -Wall -Wextra -Werror \
  -I"$TABLES" \
  -o "$BUILD/physics_replay" \
  "$CAL/replay_physics.c" "$CAL/player_canon.c" "$CAL/input_canon.c" \
  "$CAL/canon.c" \
  port/sim/physics.c port/sim/interpolated_collision.c \
  port/sim/environmental_collision.c port/sim/action_state_shortcuts.c \
  port/sim/ml_events.c \
  "$TABLES/ml_tables.c" \
  port/fdlibm/fdlibm.c -lm
echo "build OK: $BUILD/physics_replay (cc -O2 -ffp-contract=off)"

for id in g01 g04 g06; do
  name=$(node -e "const m=require('./oracle/goldens/manifest.json');console.log(m.goldens.find(g=>g.id==='$id').name)")
  echo "== $id ($name): physics capture run A"
  node "$CAL/run-capture.js" --spec physics --golden "$id" \
    --out-jsonl "$BUILD/$id.physics.jsonl" --out-run "$BUILD/$id.physics-run.json"
  echo "== $id: physics capture run B"
  node "$CAL/run-capture.js" --spec physics --golden "$id" \
    --out-jsonl "$BUILD/$id.physics.b.jsonl" --out-run "$BUILD/$id.physics-run.b.json"
  cmp "$BUILD/$id.physics.jsonl" "$BUILD/$id.physics.b.jsonl"
  echo "   capture byte-identical across two fresh runs"
  node oracle/harness/verify-stream.js "$BUILD/$id.physics-run.json"   "oracle/goldens/$name.sha256.json"
  node oracle/harness/verify-stream.js "$BUILD/$id.physics-run.b.json" "oracle/goldens/$name.sha256.json"
  node "$CAL/check-spec-pins.js" physics "$id" "$BUILD/$id.physics.jsonl" "$BUILD/$id.physics-run.json"
  rm -f "$BUILD/$id.physics.b.jsonl" "$BUILD/$id.physics-run.b.json"
  # bit-exact replay: post envelopes + pure returns + seam order/args
  "$BUILD/physics_replay" "$BUILD/$id.physics.jsonl" --strict --max-print 5
done

# no-commit guard: captures/tables are build output, never tracked
if git status --porcelain -- "$BUILD" "$TABLES" | grep -q .; then
  echo "PHYSICS FAIL: build output not gitignored" >&2
  exit 1
fi

echo "PHYSICS MATCH"
