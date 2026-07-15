#!/usr/bin/env bash
# M2 task 11 done-check: characters/marth moves — capture + bit-exact
# replay. For each captured golden (g01 g05 g06 — the marth carriers,
# probe-measured for live marth-origin coverage; g04 fields no marth):
#   - two fresh moves-marth-spec capture runs produce byte-identical
#     JSONL (determinism; includes the rule-11/12 sweep over the marth
#     moves and the frame-0 mvData dump — task 7's dump EXTENDED with the
#     marth origin map + move-object data arrays — whose post-run
#     finalCheck re-dump hard-fails on any in-match move-data drift)
#   - BOTH runs' checksum streams pass the unchanged verify-stream.js
#     against the frozen golden (instrumentation non-perturbation guard)
#   - measured-then-frozen pins hold (record/function counts, undef-ret
#     allowlist, post-state field set, and the ARTICLE count pinned ZERO —
#     marth has ZERO `articles` imports, measured)
#   - EVERY record replays through the C translations bit-identically
#     (--strict): move post envelopes {alias,hq,players,rng,sbid,snd,vfx}
#     — hq carried opaque with marth THROW* hitQueue.push modeled as
#     canon row appends; "sbid" carries the Howl play ids consumed by
#     player.shieldBreakerID = sounds.shieldbreakercharge.play() (oracle-
#     fed seam: the driver injects the recorded ids via mv_howl_play_id
#     and re-emits the consumed list) — ret tri-states, the full
#     seeded-RNG chain draw-for-draw (sweep records on the separate sweep
#     chain), every mdispatch SEAM in call order with bit-exact arguments,
#     and marth's onClank special phases routed through
#     mv_register_special_phases (FORMAT.md "The moves-marth spec");
#     framesData/attributes/charHitboxes reads go through the M1 CTAB1
#     generated tables (ml_tables), regenerated here by the executed-JS
#     pipeline; the marth move-object data arrays come from the mvData
#     dump (rule 15).
# Leaves run A's artifacts in port/sim/calib/build/ as the canonical
# captures. Prints MOVES marth MATCH, exit 0. Never weakened: exact
# equality only.
set -euo pipefail
cd "$(dirname "$0")/../../.."

CAL=port/sim/calib
BUILD=$CAL/build
TABLES=pipeline/build/moves-marth-tables
mkdir -p "$BUILD"

# M1 data plane (FORMATS.md section 3): CTAB1 tables.
bash pipeline/extractor/build-extractor.sh
node pipeline/run.js --only animations,tables --out "$TABLES"
test -f "$TABLES/ml_tables.c"

# build: every TU with -ffp-contract=off (PLAN §2)
cc -O2 -ffp-contract=off -Wall -Wextra -Werror \
  -I"$TABLES" \
  -o "$BUILD/moves_marth_replay" \
  "$CAL/replay_moves_marth.c" "$CAL/player_canon.c" "$CAL/input_canon.c" \
  "$CAL/canon.c" \
  port/sim/characters/shared/moves_index.c \
  port/sim/characters/shared/moves/*.c \
  port/sim/characters/marth/moves_index.c \
  port/sim/characters/marth/dancing_blade_combo.c \
  port/sim/characters/marth/dancing_blade_air_mobility.c \
  port/sim/characters/marth/moves/*.c \
  port/sim/action_state_shortcuts.c port/sim/ml_events.c \
  "$TABLES/ml_tables.c" \
  port/fdlibm/fdlibm.c -lm
echo "build OK: $BUILD/moves_marth_replay (cc -O2 -ffp-contract=off)"

for id in g01 g05 g06; do
  name=$(node -e "const m=require('./oracle/goldens/manifest.json');console.log(m.goldens.find(g=>g.id==='$id').name)")
  echo "== $id ($name): moves-marth capture run A"
  node "$CAL/run-capture.js" --spec moves-marth --golden "$id" \
    --out-jsonl "$BUILD/$id.moves-marth.jsonl" \
    --out-run "$BUILD/$id.moves-marth-run.json"
  echo "== $id: moves-marth capture run B"
  node "$CAL/run-capture.js" --spec moves-marth --golden "$id" \
    --out-jsonl "$BUILD/$id.moves-marth.b.jsonl" \
    --out-run "$BUILD/$id.moves-marth-run.b.json"
  cmp "$BUILD/$id.moves-marth.jsonl" "$BUILD/$id.moves-marth.b.jsonl"
  echo "   capture byte-identical across two fresh runs"
  node oracle/harness/verify-stream.js "$BUILD/$id.moves-marth-run.json"   "oracle/goldens/$name.sha256.json"
  node oracle/harness/verify-stream.js "$BUILD/$id.moves-marth-run.b.json" "oracle/goldens/$name.sha256.json"
  node "$CAL/check-spec-pins.js" moves-marth "$id" \
    "$BUILD/$id.moves-marth.jsonl" "$BUILD/$id.moves-marth-run.json"
  rm -f "$BUILD/$id.moves-marth.b.jsonl" "$BUILD/$id.moves-marth-run.b.json"
  # bit-exact replay: post envelopes + rets + RNG chains + sbid + seams
  "$BUILD/moves_marth_replay" "$BUILD/$id.moves-marth.jsonl" --strict --max-print 5
done

# no-commit guard: captures/tables are build output, never tracked
if git status --porcelain -- "$BUILD" "$TABLES" | grep -q .; then
  echo "MOVES marth FAIL: build output not gitignored" >&2
  exit 1
fi

echo "MOVES marth MATCH"
