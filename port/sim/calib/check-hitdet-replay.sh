#!/usr/bin/env bash
# M2 task 6 done-check: hitDetection + hitQueue + hitbox value model —
# capture + bit-exact replay. For each captured golden (g01 g04 g06):
#   - two fresh hitdet-spec capture runs produce byte-identical JSONL
#     (determinism; includes the fixed rule-11 sweep and the frame-0
#     hdFlags dump, whose post-run finalCheck re-dump hard-fails on any
#     in-match move-flag drift)
#   - BOTH runs' checksum streams pass the unchanged verify-stream.js
#     against the frozen golden (instrumentation non-perturbation guard)
#   - measured-then-frozen pins hold (record/function counts incl. the
#     zero pins for internal-only exports and Math.randomW, undef-ret
#     allowlist, post-state field set)
#   - EVERY record replays through the C translations bit-identically
#     (--strict): pipeline post envelopes {alias,hq,phq,players,rng,snd},
#     pure getter/collision returns, the knockbackSounds sound queue, the
#     full seeded-RNG chain draw-for-draw, and every dispatch SEAM in
#     call order with bit-exact arguments (FORMAT.md "The hitdet spec");
#     attribute reads (weight) go through the M1 CTAB1 generated tables
#     (ml_tables), regenerated here by the executed-JS pipeline.
# Leaves run A's artifacts in port/sim/calib/build/ as the canonical
# captures. Prints HITDET MATCH, exit 0. Never weakened: exact equality
# only.
set -euo pipefail
cd "$(dirname "$0")/../../.."

CAL=port/sim/calib
BUILD=$CAL/build
TABLES=pipeline/build/hitdet-tables
mkdir -p "$BUILD"

# M1 data plane (FORMATS.md section 3): CTAB1 tables for attributes.
bash pipeline/extractor/build-extractor.sh
node pipeline/run.js --only animations,tables --out "$TABLES"
test -f "$TABLES/ml_tables.c"

# build: every TU with -ffp-contract=off (PLAN §2)
cc -O2 -ffp-contract=off -Wall -Wextra -Werror \
  -I"$TABLES" \
  -o "$BUILD/hitdet_replay" \
  "$CAL/replay_hitdet.c" "$CAL/player_canon.c" "$CAL/canon.c" \
  port/sim/hit_detection.c port/sim/interpolated_collision.c \
  port/sim/environmental_collision.c port/sim/action_state_shortcuts.c \
  port/sim/ml_events.c \
  "$TABLES/ml_tables.c" \
  port/fdlibm/fdlibm.c -lm
echo "build OK: $BUILD/hitdet_replay (cc -O2 -ffp-contract=off)"

for id in g01 g04 g06; do
  name=$(node -e "const m=require('./oracle/goldens/manifest.json');console.log(m.goldens.find(g=>g.id==='$id').name)")
  echo "== $id ($name): hitdet capture run A"
  node "$CAL/run-capture.js" --spec hitdet --golden "$id" \
    --out-jsonl "$BUILD/$id.hitdet.jsonl" --out-run "$BUILD/$id.hitdet-run.json"
  echo "== $id: hitdet capture run B"
  node "$CAL/run-capture.js" --spec hitdet --golden "$id" \
    --out-jsonl "$BUILD/$id.hitdet.b.jsonl" --out-run "$BUILD/$id.hitdet-run.b.json"
  cmp "$BUILD/$id.hitdet.jsonl" "$BUILD/$id.hitdet.b.jsonl"
  echo "   capture byte-identical across two fresh runs"
  node oracle/harness/verify-stream.js "$BUILD/$id.hitdet-run.json"   "oracle/goldens/$name.sha256.json"
  node oracle/harness/verify-stream.js "$BUILD/$id.hitdet-run.b.json" "oracle/goldens/$name.sha256.json"
  node "$CAL/check-spec-pins.js" hitdet "$id" "$BUILD/$id.hitdet.jsonl" "$BUILD/$id.hitdet-run.json"
  rm -f "$BUILD/$id.hitdet.b.jsonl" "$BUILD/$id.hitdet-run.b.json"
  # bit-exact replay: post envelopes + pure returns + RNG chain + seams
  "$BUILD/hitdet_replay" "$BUILD/$id.hitdet.jsonl" --strict --max-print 5
done

# no-commit guard: captures/tables are build output, never tracked
if git status --porcelain -- "$BUILD" "$TABLES" | grep -q .; then
  echo "HITDET FAIL: build output not gitignored" >&2
  exit 1
fi

echo "HITDET MATCH"
