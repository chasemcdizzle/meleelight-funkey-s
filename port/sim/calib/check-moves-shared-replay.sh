#!/usr/bin/env bash
# M2 task 7 done-check: characters/shared moves — capture + bit-exact
# replay. For each captured golden (g01 g04 g06):
#   - two fresh moves-shared-spec capture runs produce byte-identical
#     JSONL (determinism; includes the rule-11/12 sweep over the
#     zero-live shared moves and the frame-0 mvData dump, whose post-run
#     finalCheck re-dump hard-fails on any in-match move-data drift)
#   - BOTH runs' checksum streams pass the unchanged verify-stream.js
#     against the frozen golden (instrumentation non-perturbation guard)
#   - measured-then-frozen pins hold (record/function counts, undef-ret
#     allowlist, post-state field set)
#   - EVERY record replays through the C translations bit-identically
#     (--strict): move post envelopes {alias,hq,players,rng,snd,vfx},
#     ret tri-states, the full seeded-RNG chain draw-for-draw (owner
#     draws + seam-window draws + standalone draws in file order; the
#     rule-12 sweep records on a separate sweep chain), and every
#     mdispatch SEAM in call order with bit-exact arguments (FORMAT.md
#     "The moves-shared spec"); framesData/attributes/charHitboxes reads
#     go through the M1 CTAB1 generated tables (ml_tables), regenerated
#     here by the executed-JS pipeline.
# Leaves run A's artifacts in port/sim/calib/build/ as the canonical
# captures. Prints MOVES SHARED MATCH, exit 0. Never weakened: exact
# equality only.
set -euo pipefail
cd "$(dirname "$0")/../../.."

CAL=port/sim/calib
BUILD=$CAL/build
TABLES=pipeline/build/moves-shared-tables
mkdir -p "$BUILD"

# M1 data plane (FORMATS.md section 3): CTAB1 tables (attributes/
# framesData/intangibility/charHitboxes).
bash pipeline/extractor/build-extractor.sh
node pipeline/run.js --only animations,tables --out "$TABLES"
test -f "$TABLES/ml_tables.c"

# build: every TU with -ffp-contract=off (PLAN §2)
cc -O2 -ffp-contract=off -Wall -Wextra -Werror \
  -I"$TABLES" \
  -o "$BUILD/moves_shared_replay" \
  "$CAL/replay_moves_shared.c" "$CAL/player_canon.c" "$CAL/input_canon.c" \
  "$CAL/canon.c" \
  port/sim/characters/shared/moves_index.c \
  port/sim/characters/shared/moves/*.c \
  port/sim/action_state_shortcuts.c port/sim/ml_events.c \
  "$TABLES/ml_tables.c" \
  port/fdlibm/fdlibm.c -lm
echo "build OK: $BUILD/moves_shared_replay (cc -O2 -ffp-contract=off)"

for id in g01 g04 g06; do
  name=$(node -e "const m=require('./oracle/goldens/manifest.json');console.log(m.goldens.find(g=>g.id==='$id').name)")
  echo "== $id ($name): moves-shared capture run A"
  node "$CAL/run-capture.js" --spec moves-shared --golden "$id" \
    --out-jsonl "$BUILD/$id.moves-shared.jsonl" \
    --out-run "$BUILD/$id.moves-shared-run.json"
  echo "== $id: moves-shared capture run B"
  node "$CAL/run-capture.js" --spec moves-shared --golden "$id" \
    --out-jsonl "$BUILD/$id.moves-shared.b.jsonl" \
    --out-run "$BUILD/$id.moves-shared-run.b.json"
  cmp "$BUILD/$id.moves-shared.jsonl" "$BUILD/$id.moves-shared.b.jsonl"
  echo "   capture byte-identical across two fresh runs"
  node oracle/harness/verify-stream.js "$BUILD/$id.moves-shared-run.json"   "oracle/goldens/$name.sha256.json"
  node oracle/harness/verify-stream.js "$BUILD/$id.moves-shared-run.b.json" "oracle/goldens/$name.sha256.json"
  node "$CAL/check-spec-pins.js" moves-shared "$id" \
    "$BUILD/$id.moves-shared.jsonl" "$BUILD/$id.moves-shared-run.json"
  rm -f "$BUILD/$id.moves-shared.b.jsonl" "$BUILD/$id.moves-shared-run.b.json"
  # bit-exact replay: post envelopes + rets + RNG chains + seams
  "$BUILD/moves_shared_replay" "$BUILD/$id.moves-shared.jsonl" --strict --max-print 5
done

# no-commit guard: captures/tables are build output, never tracked
if git status --porcelain -- "$BUILD" "$TABLES" | grep -q .; then
  echo "MOVES SHARED FAIL: build output not gitignored" >&2
  exit 1
fi

echo "MOVES SHARED MATCH"
