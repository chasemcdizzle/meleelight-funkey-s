#!/usr/bin/env bash
# M2 EXIT GATE (fix_plan §M2 task 17; CLAUDE.md §Commands "M2 EXIT GATE"):
# the full headless C sim replays EVERY golden in
# oracle/goldens/manifest.json end-to-end and its checksum stream is
# judged by the UNCHANGED oracle/harness/verify-stream.js against the
# frozen streams — exact per-frame hash equality over the FULL length,
# plus rngCalls + rngCallsOutsideStep equality and the specVersion/trace
# pins. Prints SIM CONFORMS, exit 0; any mismatch, length shortfall, or
# missing golden -> nonzero. Never weakened: exact equality only.
#
# Inputs built here:
#  - M1 data plane: generated CTAB1 tables + STAB1 stages (executed-JS
#    pipeline; framesData carries the ANIM1-reconciled frame counts).
#  - SIMDATA1: the executed move-data plane + asFlags/hdFlags dumps
#    (port/sim/calib/dump-sim-data.js; run TWICE, byte-identical).
#  - AI bridge artifacts for the CPU goldens (task 16's AIBRIDGE1;
#    captures recorded via the ai spec when absent, each capture run
#    STREAM-MATCH guarded against the frozen golden).
set -euo pipefail
cd "$(dirname "$0")/../.."

CAL=port/sim/calib
BUILD=$CAL/build
SIM=port/sim/sim
TABLES=pipeline/build/sim-tables
mkdir -p "$BUILD"

# --- M1 data plane -----------------------------------------------------------
bash pipeline/extractor/build-extractor.sh
node pipeline/run.js --only animations,tables,stages --out "$TABLES"
test -f "$TABLES/ml_tables.c"
test -f "$TABLES/ml_stages.c"

# --- SIMDATA1 (executed move-data plane; determinism x2) -----------------------
node "$CAL/dump-sim-data.js" --out "$BUILD/simdata.txt"
node "$CAL/dump-sim-data.js" --out "$BUILD/simdata.b.txt"
cmp "$BUILD/simdata.txt" "$BUILD/simdata.b.txt"
rm -f "$BUILD/simdata.b.txt"
echo "simdata byte-identical across two fresh dumps"

# --- build the headless sim (every TU cc -O2 -ffp-contract=off) ----------------
cc -O2 -ffp-contract=off -Wall -Wextra -Werror \
  -I"$TABLES" -Iport/ryu -Iport/sim -Ioracle/qjs \
  -o "$BUILD/sim_host" \
  "$SIM/sim_main.c" "$SIM/sim_boot.c" "$SIM/sim_tick.c" \
  "$SIM/sim_ser.c" "$SIM/sim_data.c" \
  "$CAL/canon.c" "$CAL/player_canon.c" \
  port/sim/physics.c port/sim/interpolated_collision.c \
  port/sim/environmental_collision.c port/sim/hit_detection.c \
  port/sim/article.c port/sim/action_state_shortcuts.c \
  port/sim/ml_events.c port/sim/ml_fmt.c port/sim/ml_ser.c \
  port/sim/ai_bridge.c port/sim/input/interpret_inputs.c \
  port/sim/stages/moving_platforms.c port/sim/stages/ystory.c \
  port/sim/stages/fountain.c \
  port/sim/characters/shared/moves_index.c \
  port/sim/characters/shared/moves/*.c \
  port/sim/characters/fox/moves_index.c \
  port/sim/characters/fox/moves/*.c \
  port/sim/characters/falco/moves_index.c \
  port/sim/characters/falco/moves/*.c \
  port/sim/characters/falcon/moves_index.c \
  port/sim/characters/falcon/moves/*.c \
  port/sim/characters/marth/moves_index.c \
  port/sim/characters/marth/dancing_blade_combo.c \
  port/sim/characters/marth/dancing_blade_air_mobility.c \
  port/sim/characters/marth/moves/*.c \
  port/sim/characters/puff/moves_index.c \
  port/sim/characters/puff/puff_multi_jump_drift.c \
  port/sim/characters/puff/puff_next_jump.c \
  port/sim/characters/puff/moves/*.c \
  "$TABLES/ml_tables.c" "$TABLES/ml_stages.c" \
  oracle/qjs/sha256.c \
  port/fdlibm/fdlibm.c -lm
echo "build OK: $BUILD/sim_host (cc -O2 -ffp-contract=off)"

# --- replay every golden --------------------------------------------------------
ids=$(node -e "const m=require('./oracle/goldens/manifest.json');console.log(m.goldens.map(g=>g.id).join(' '))")
pass=0
for id in $ids; do
  eval "$(node -e "
    const m=require('./oracle/goldens/manifest.json');
    const g=m.goldens.find(x=>x.id==='$id');
    console.log('name='+g.name);
    console.log('seed='+g.seed);
    console.log('p1='+g.p1); console.log('p2='+g.p2);
    console.log('stage='+g.stage);
    console.log('frames='+g.frames);
    console.log('cpu='+(g.cpu?1:0));
    console.log('difficulty='+(g.difficulty||5));
    console.log('trace='+g.trace);
  ")"
  echo "== $id ($name)"

  node "$SIM/trace-to-txt.js" "oracle/goldens/$trace" "$BUILD/$id.trace.txt"

  bridge_args=()
  if [ "$cpu" = "1" ]; then
    if [ ! -f "$BUILD/$id.ai-bridge.txt" ]; then
      echo "   AI bridge artifact absent — recording the ai capture"
      node "$CAL/run-capture.js" --spec ai --golden "$id" \
        --out-jsonl "$BUILD/$id.ai.jsonl" \
        --out-run "$BUILD/$id.ai-run.json"
      node oracle/harness/verify-stream.js "$BUILD/$id.ai-run.json" \
        "oracle/goldens/$name.sha256.json"
      node "$CAL/build-ai-bridge.js" "$id" "$BUILD/$id.ai.jsonl" \
        "$BUILD/$id.ai-bridge.txt"
    fi
    bridge_args=(--cpu --difficulty "$difficulty" --ai-bridge "$BUILD/$id.ai-bridge.txt")
  fi

  "$BUILD/sim_host" \
    --trace "$BUILD/$id.trace.txt" --simdata "$BUILD/simdata.txt" \
    --seed "$seed" --p1 "$p1" --p2 "$p2" --stage "$stage" \
    --frames "$frames" ${bridge_args[@]+"${bridge_args[@]}"} \
    > "$BUILD/$id.sim-out.txt"

  node "$SIM/wrap-run.js" "$id" "$BUILD/$id.sim-out.txt" \
    "$BUILD/$id.sim-run.json"
  node oracle/harness/verify-stream.js "$BUILD/$id.sim-run.json" \
    "oracle/goldens/$name.sha256.json"
  pass=$((pass + 1))
done

n=$(node -e "console.log(String(require('./oracle/goldens/manifest.json').goldens.length))")
if [ "$pass" != "$n" ]; then
  echo "SIM FAIL: $pass of $n goldens verified" >&2
  exit 1
fi

# no-commit guard: build output is never tracked
if git status --porcelain -- "$BUILD" "$TABLES" | grep -q .; then
  echo "SIM FAIL: build output not gitignored" >&2
  exit 1
fi

echo "SIM CONFORMS"
