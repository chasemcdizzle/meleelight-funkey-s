#!/usr/bin/env bash
# M2 task 10 done-check: characters/falcon moves — capture + bit-exact
# replay. For each captured golden (g03 g04 g06 — the falcon carriers;
# g07's CPU falcon was measured firing ZERO live falcon-origin moves, so
# it is not a carrier):
#   - two fresh moves-falcon-spec capture runs produce byte-identical
#     JSONL (determinism; includes the rule-11/12 sweep over the falcon
#     moves and the frame-0 mvData dump — task 7's dump EXTENDED with the
#     falcon origin map + move-object data arrays — whose post-run
#     finalCheck re-dump hard-fails on any in-match move-data drift)
#   - BOTH runs' checksum streams pass the unchanged verify-stream.js
#     against the frozen golden (instrumentation non-perturbation guard)
#   - measured-then-frozen pins hold (record/function counts, undef-ret
#     allowlist, post-state field set, and the ARTICLE count pinned ZERO —
#     falcon has no article call sites: dead imports, measured)
#   - EVERY record replays through the C translations bit-identically
#     (--strict): move post envelopes {alias,hq,players,rng,snd,vfx} —
#     hq carried opaque with falcon THROW*/UPSPECIALCATCH hitQueue.push
#     modeled as canon row appends — ret tri-states, the full seeded-RNG
#     chain draw-for-draw (sweep records on the separate sweep chain;
#     UPSPECIALCATCH/UPSPECIALTHROW draw INLINE), every mdispatch SEAM in
#     call order with bit-exact arguments, and falcon's non-phase dispatch
#     surfaces (onPlayerHit / onWallCollide with [wallFace,wallNum]
#     extras) routed through mv_register_special_phases (FORMAT.md "The
#     moves-falcon spec"); framesData/attributes/charHitboxes reads go
#     through the M1 CTAB1 generated tables (ml_tables), regenerated here
#     by the executed-JS pipeline; the falcon move-object data arrays come
#     from the mvData dump (rule 15).
# Leaves run A's artifacts in port/sim/calib/build/ as the canonical
# captures. Prints MOVES falcon MATCH, exit 0. Never weakened: exact
# equality only.
set -euo pipefail
cd "$(dirname "$0")/../../.."

CAL=port/sim/calib
BUILD=$CAL/build
TABLES=pipeline/build/moves-falcon-tables
mkdir -p "$BUILD"

# M1 data plane (FORMATS.md section 3): CTAB1 tables.
bash pipeline/extractor/build-extractor.sh
node pipeline/run.js --only animations,tables --out "$TABLES"
test -f "$TABLES/ml_tables.c"

# build: every TU with -ffp-contract=off (PLAN §2)
cc -O2 -ffp-contract=off -Wall -Wextra -Werror \
  -I"$TABLES" \
  -o "$BUILD/moves_falcon_replay" \
  "$CAL/replay_moves_falcon.c" "$CAL/player_canon.c" "$CAL/input_canon.c" \
  "$CAL/canon.c" \
  port/sim/characters/shared/moves_index.c \
  port/sim/characters/shared/moves/*.c \
  port/sim/characters/falcon/moves_index.c \
  port/sim/characters/falcon/moves/*.c \
  port/sim/action_state_shortcuts.c port/sim/ml_events.c \
  "$TABLES/ml_tables.c" \
  port/fdlibm/fdlibm.c -lm
echo "build OK: $BUILD/moves_falcon_replay (cc -O2 -ffp-contract=off)"

for id in g03 g04 g06; do
  name=$(node -e "const m=require('./oracle/goldens/manifest.json');console.log(m.goldens.find(g=>g.id==='$id').name)")
  echo "== $id ($name): moves-falcon capture run A"
  node "$CAL/run-capture.js" --spec moves-falcon --golden "$id" \
    --out-jsonl "$BUILD/$id.moves-falcon.jsonl" \
    --out-run "$BUILD/$id.moves-falcon-run.json"
  echo "== $id: moves-falcon capture run B"
  node "$CAL/run-capture.js" --spec moves-falcon --golden "$id" \
    --out-jsonl "$BUILD/$id.moves-falcon.b.jsonl" \
    --out-run "$BUILD/$id.moves-falcon-run.b.json"
  cmp "$BUILD/$id.moves-falcon.jsonl" "$BUILD/$id.moves-falcon.b.jsonl"
  echo "   capture byte-identical across two fresh runs"
  node oracle/harness/verify-stream.js "$BUILD/$id.moves-falcon-run.json"   "oracle/goldens/$name.sha256.json"
  node oracle/harness/verify-stream.js "$BUILD/$id.moves-falcon-run.b.json" "oracle/goldens/$name.sha256.json"
  node "$CAL/check-spec-pins.js" moves-falcon "$id" \
    "$BUILD/$id.moves-falcon.jsonl" "$BUILD/$id.moves-falcon-run.json"
  rm -f "$BUILD/$id.moves-falcon.b.jsonl" "$BUILD/$id.moves-falcon-run.b.json"
  # bit-exact replay: post envelopes + rets + RNG chains + seams
  "$BUILD/moves_falcon_replay" "$BUILD/$id.moves-falcon.jsonl" --strict --max-print 5
done

# no-commit guard: captures/tables are build output, never tracked
if git status --porcelain -- "$BUILD" "$TABLES" | grep -q .; then
  echo "MOVES falcon FAIL: build output not gitignored" >&2
  exit 1
fi

echo "MOVES falcon MATCH"
