#!/usr/bin/env bash
# M2 task 9 done-check: characters/falco moves — capture + bit-exact replay.
# For each captured golden (g02 g05 g07 — the falco carriers; g07 is the
# second CPU-golden capture: the AI plane stays JS-side and its seeded
# draws chain-verify as standalone records):
#   - two fresh moves-falco-spec capture runs produce byte-identical JSONL
#     (determinism; includes the rule-11/12 sweep over the falco moves and
#     the frame-0 mvData dump — task 7's dump EXTENDED with the falco
#     origin map + move-object data arrays — whose post-run finalCheck
#     re-dump hard-fails on any in-match move-data drift)
#   - BOTH runs' checksum streams pass the unchanged verify-stream.js
#     against the frozen golden (instrumentation non-perturbation guard)
#   - measured-then-frozen pins hold (record/function counts, undef-ret
#     allowlist, post-state field set)
#   - EVERY record replays through the C translations bit-identically
#     (--strict): move post envelopes {alias,hq,players,rng,snd,vfx} —
#     hq carried opaque with falco THROW*'s hitQueue.push modeled as canon
#     row appends — ret tri-states, the full seeded-RNG chain draw-for-draw
#     (sweep records on the separate sweep chain), every mdispatch SEAM
#     and every ARTICLE seam (the task-13 boundary; falco options carry
#     isFox:false + THROWDOWN's partOfThrow:true) in call order with
#     bit-exact arguments (FORMAT.md "The moves-falco spec"); framesData/
#     attributes/charHitboxes reads go through the M1 CTAB1 generated
#     tables (ml_tables), regenerated here by the executed-JS pipeline;
#     the falco move-object data arrays come from the mvData dump
#     (rule 15).
# Leaves run A's artifacts in port/sim/calib/build/ as the canonical
# captures. Prints MOVES falco MATCH, exit 0. Never weakened: exact
# equality only.
set -euo pipefail
cd "$(dirname "$0")/../../.."

CAL=port/sim/calib
BUILD=$CAL/build
TABLES=pipeline/build/moves-falco-tables
mkdir -p "$BUILD"

# M1 data plane (FORMATS.md section 3): CTAB1 tables.
bash pipeline/extractor/build-extractor.sh
node pipeline/run.js --only animations,tables --out "$TABLES"
test -f "$TABLES/ml_tables.c"

# build: every TU with -ffp-contract=off (PLAN §2)
cc -O2 -ffp-contract=off -Wall -Wextra -Werror \
  -I"$TABLES" \
  -o "$BUILD/moves_falco_replay" \
  "$CAL/replay_moves_falco.c" "$CAL/player_canon.c" "$CAL/input_canon.c" \
  "$CAL/canon.c" \
  port/sim/characters/shared/moves_index.c \
  port/sim/characters/shared/moves/*.c \
  port/sim/characters/falco/moves_index.c \
  port/sim/characters/falco/moves/*.c \
  port/sim/action_state_shortcuts.c port/sim/ml_events.c \
  "$TABLES/ml_tables.c" \
  port/fdlibm/fdlibm.c -lm
echo "build OK: $BUILD/moves_falco_replay (cc -O2 -ffp-contract=off)"

for id in g02 g05 g07; do
  name=$(node -e "const m=require('./oracle/goldens/manifest.json');console.log(m.goldens.find(g=>g.id==='$id').name)")
  echo "== $id ($name): moves-falco capture run A"
  node "$CAL/run-capture.js" --spec moves-falco --golden "$id" \
    --out-jsonl "$BUILD/$id.moves-falco.jsonl" \
    --out-run "$BUILD/$id.moves-falco-run.json"
  echo "== $id: moves-falco capture run B"
  node "$CAL/run-capture.js" --spec moves-falco --golden "$id" \
    --out-jsonl "$BUILD/$id.moves-falco.b.jsonl" \
    --out-run "$BUILD/$id.moves-falco-run.b.json"
  cmp "$BUILD/$id.moves-falco.jsonl" "$BUILD/$id.moves-falco.b.jsonl"
  echo "   capture byte-identical across two fresh runs"
  node oracle/harness/verify-stream.js "$BUILD/$id.moves-falco-run.json"   "oracle/goldens/$name.sha256.json"
  node oracle/harness/verify-stream.js "$BUILD/$id.moves-falco-run.b.json" "oracle/goldens/$name.sha256.json"
  node "$CAL/check-spec-pins.js" moves-falco "$id" \
    "$BUILD/$id.moves-falco.jsonl" "$BUILD/$id.moves-falco-run.json"
  rm -f "$BUILD/$id.moves-falco.b.jsonl" "$BUILD/$id.moves-falco-run.b.json"
  # bit-exact replay: post envelopes + rets + RNG chains + seams
  "$BUILD/moves_falco_replay" "$BUILD/$id.moves-falco.jsonl" --strict --max-print 5
done

# no-commit guard: captures/tables are build output, never tracked
if git status --porcelain -- "$BUILD" "$TABLES" | grep -q .; then
  echo "MOVES falco FAIL: build output not gitignored" >&2
  exit 1
fi

echo "MOVES falco MATCH"
