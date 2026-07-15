#!/usr/bin/env bash
# M2 task 12 done-check: characters/puff moves — capture + bit-exact
# replay. For each captured golden (g02 g04 g08 — the puff carriers,
# probe-measured for live puff-origin coverage; g08's CPU puff fires
# live moves, unlike g07's falcon):
#   - two fresh moves-puff-spec capture runs produce byte-identical
#     JSONL (determinism; includes the rule-11/12 sweep over the puff
#     moves and the frame-0 mvData dump — task 7's dump EXTENDED with the
#     puff origin map (incl. the FURAFURA/JUMPAERIALB/JUMPAERIALF table
#     OVERRIDES) + move-object data arrays — whose post-run finalCheck
#     re-dump hard-fails on any in-match move-data drift)
#   - BOTH runs' checksum streams pass the unchanged verify-stream.js
#     against the frozen golden (instrumentation non-perturbation guard)
#   - measured-then-frozen pins hold (record/function counts, undef-ret
#     allowlist, post-state field set, and the ARTICLE count pinned ZERO —
#     puff has ZERO `articles` references, measured)
#   - EVERY record replays through the C translations bit-identically
#     (--strict): move post envelopes {alias,hq,players,rng,snd,vfx} —
#     hq carried opaque with puff THROW* hitQueue.push modeled as canon
#     row appends; every move record's pre carries "chd" (the EXECUTED
#     charHitboxes dmg/size plane — puff's rollout writes dmg through
#     STALE id aliases and sing cycles id[0].size, so hitbox assigns feed
#     from the measured plane, never assumed-pristine CTAB1; live drift
#     MEASURED on g04 from frame 1038) — ret tri-states, the full
#     seeded-RNG chain draw-for-draw (sweep records on the separate sweep
#     chain), every mdispatch SEAM in call order with bit-exact arguments
#     (g08 carries the first LIVE per-char-cluster seam: the CPU puff's
#     back-throw on fox), and puff's onPlayerHit/onWallCollide special
#     phases routed through mv_register_special_phases (FORMAT.md "The
#     moves-puff spec"); framesData/attributes/charHitboxes shape reads
#     go through the M1 CTAB1 generated tables (ml_tables), regenerated
#     here by the executed-JS pipeline; the puff move-object data arrays
#     come from the mvData dump (rule 15).
# Leaves run A's artifacts in port/sim/calib/build/ as the canonical
# captures. Prints MOVES puff MATCH, exit 0. Never weakened: exact
# equality only.
set -euo pipefail
cd "$(dirname "$0")/../../.."

CAL=port/sim/calib
BUILD=$CAL/build
TABLES=pipeline/build/moves-puff-tables
mkdir -p "$BUILD"

# M1 data plane (FORMATS.md section 3): CTAB1 tables.
bash pipeline/extractor/build-extractor.sh
node pipeline/run.js --only animations,tables --out "$TABLES"
test -f "$TABLES/ml_tables.c"

# build: every TU with -ffp-contract=off (PLAN §2)
cc -O2 -ffp-contract=off -Wall -Wextra -Werror \
  -I"$TABLES" \
  -o "$BUILD/moves_puff_replay" \
  "$CAL/replay_moves_puff.c" "$CAL/player_canon.c" "$CAL/input_canon.c" \
  "$CAL/canon.c" \
  port/sim/characters/shared/moves_index.c \
  port/sim/characters/shared/moves/*.c \
  port/sim/characters/puff/moves_index.c \
  port/sim/characters/puff/puff_multi_jump_drift.c \
  port/sim/characters/puff/puff_next_jump.c \
  port/sim/characters/puff/moves/*.c \
  port/sim/action_state_shortcuts.c port/sim/ml_events.c \
  "$TABLES/ml_tables.c" \
  port/fdlibm/fdlibm.c -lm
echo "build OK: $BUILD/moves_puff_replay (cc -O2 -ffp-contract=off)"

for id in g02 g04 g08; do
  name=$(node -e "const m=require('./oracle/goldens/manifest.json');console.log(m.goldens.find(g=>g.id==='$id').name)")
  echo "== $id ($name): moves-puff capture run A"
  node "$CAL/run-capture.js" --spec moves-puff --golden "$id" \
    --out-jsonl "$BUILD/$id.moves-puff.jsonl" \
    --out-run "$BUILD/$id.moves-puff-run.json"
  echo "== $id: moves-puff capture run B"
  node "$CAL/run-capture.js" --spec moves-puff --golden "$id" \
    --out-jsonl "$BUILD/$id.moves-puff.b.jsonl" \
    --out-run "$BUILD/$id.moves-puff-run.b.json"
  cmp "$BUILD/$id.moves-puff.jsonl" "$BUILD/$id.moves-puff.b.jsonl"
  echo "   capture byte-identical across two fresh runs"
  node oracle/harness/verify-stream.js "$BUILD/$id.moves-puff-run.json"   "oracle/goldens/$name.sha256.json"
  node oracle/harness/verify-stream.js "$BUILD/$id.moves-puff-run.b.json" "oracle/goldens/$name.sha256.json"
  node "$CAL/check-spec-pins.js" moves-puff "$id" \
    "$BUILD/$id.moves-puff.jsonl" "$BUILD/$id.moves-puff-run.json"
  rm -f "$BUILD/$id.moves-puff.b.jsonl" "$BUILD/$id.moves-puff-run.b.json"
  # bit-exact replay: post envelopes + rets + RNG chains + chd-fed
  # assigns + seams
  "$BUILD/moves_puff_replay" "$BUILD/$id.moves-puff.jsonl" --strict --max-print 5
done

# no-commit guard: captures/tables are build output, never tracked
if git status --porcelain -- "$BUILD" "$TABLES" | grep -q .; then
  echo "MOVES puff FAIL: build output not gitignored" >&2
  exit 1
fi

echo "MOVES puff MATCH"
