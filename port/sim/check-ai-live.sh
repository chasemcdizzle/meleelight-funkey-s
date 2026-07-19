#!/usr/bin/env bash
# check-ai-live.sh — M4 task 5 done-check: LIVE CPU integration — the
# real C AI (port/sim/ai.c) at the sim's runAI site, the AIBRIDGE1 path
# retired from the live path (kept as the M2 archival arm).
#
# Composes (fix_plan §M4 task 5; prints AI LIVE CONFORMS, exit 0):
#   [0] BYTE PIN on port/sim/check-sim.sh — fix_plan §M4 conventions:
#       "the M2 EXIT GATE is NEVER edited"; this pin turns the
#       convention into a mechanical check (HARD RULE 3 witness).
#   [1] bash port/sim/check-sim.sh — the UNCHANGED M2 EXIT GATE, its
#       bridge-fed form intact (g07/g08 via AIBRIDGE1); also produces
#       the shared build artifacts (CTAB1/STAB1, simdata, trace txts).
#   [2] sim_host_live build: check-sim.sh's EXACT TU list (kept in sync
#       — the M3 device-rig precedent; link failure catches a dropped
#       TU) + port/sim/ai.c + port/sim/sim/sim_ai_live.c (the live-AI
#       driver TU behind the ml_sim_runai_live pointer seam, sim.h).
#       M2-CONTRACT WITNESS: the gate's own sim_host (no live TU) must
#       still REFUSE --cpu without --ai-bridge.
#   [3] LIVE g07/g08: replayed with NO --ai-bridge (the live C AI draws
#       off the seeded chain), judged by the UNCHANGED
#       oracle/harness/verify-stream.js against the FROZEN
#       oracle/goldens streams — the streams don't move, so live-vs-
#       bridge bit-identity is proven against the same contract.
#   [4] LIVE m01/m02 (port/goldens-m4/ — the d1/d9 CPU-difficulty
#       coverage goldens, browser x2-identity + quality contract at
#       record time): same live replay, judged by the same unchanged
#       verifier against port/goldens-m4/*.sha256.json.
#   [5] bash port/sim/calib/check-ai-bridge.sh — the M2 archival rig
#       intact (AI BRIDGE OK).
#   [6] bash port/sim/calib/check-ai-replay.sh — the task-4 aiport
#       capture-replay rig intact (AI MATCH).
# Never weakened: every stream judgment is the unchanged verify-stream.js
# (exact per-frame equality, full length, rngCalls + specVersion pins).
set -euo pipefail
cd "$(dirname "$0")/../.."

CAL=port/sim/calib
BUILD=$CAL/build
SIM=port/sim/sim
TABLES=pipeline/build/sim-tables
M4G=port/goldens-m4

fail() { echo "AI LIVE FAIL: $*" >&2; exit 1; }

# --- [0] the M2-gate byte pin (before anything runs) --------------------------
# fix_plan §M4: check-sim.sh is never edited. Its bytes are pinned here;
# a differing hash is a loud review moment, never a silent gate drift.
CHECK_SIM_SHA256=ce0882bee2a0bb0ad11ac51366ef467c3811d832f9dc932c4eb10dd3ccc4c8cb
have="$(shasum -a 256 port/sim/check-sim.sh | cut -d' ' -f1)" || fail "cannot hash check-sim.sh"
case "$have" in
  ([0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f]*) : ;;
  (*) fail "check-sim.sh hash read is malformed: '$have'" ;;
esac
if [ "$have" != "$CHECK_SIM_SHA256" ]; then
  fail "check-sim.sh byte pin — sha256 $have != pinned $CHECK_SIM_SHA256 (the M2 EXIT GATE is never edited; a legitimate, reviewed change must update this pin explicitly)"
fi

# --- M4 CPU-golden inventory pin (before the lock and any run) ----------------
# The live corpus is every cpu==true golden in port/goldens-m4/manifest.json,
# pinned to exactly {m01 m02} in both directions (the corpus-inventory
# pattern, iter 77). Growing the manifest (task-11 target goldens are NOT
# cpu match traces) that changes this set is a reviewed pin update.
m4ids="$(node -e '
  const fs = require("fs");
  const m = JSON.parse(fs.readFileSync("port/goldens-m4/manifest.json", "utf8"));
  const ids = m.goldens.filter((g) => g.cpu === true).map((g) => g.id);
  process.stdout.write(ids.sort().join(" "));
')" || fail "cannot read the m4 manifest"
if [ "$m4ids" != "m01 m02" ]; then
  fail "m4 CPU-golden inventory pin — manifest cpu goldens {$m4ids} != pinned {m01 m02}"
fi

# --- RUN LOCK (the iter-41/66 no-reclaim pattern) -----------------------------
# The shared resource is this checkout's build/ artifacts (also rewritten
# by the composed children, which hold their own locks only while running).
mkdir -p "$BUILD"
LOCK="$BUILD/ai-live.lock"
if ! mkdir "$LOCK" 2>/dev/null; then
  lockage="unknown"
  if lockmtime="$(stat -f %m "$LOCK" 2>/dev/null || stat -c %Y "$LOCK" 2>/dev/null)"; then
    lockage="$(( $(date +%s) - lockmtime )) s"
  fi
  echo "AI LIVE REFUSED: run lock $LOCK already exists (age: $lockage)." >&2
  echo "  Another check-ai-live.sh run may be rewriting the shared artifacts" >&2
  echo "  in $BUILD right now. NO auto-reclaim (iter-41 posture). If you are" >&2
  echo "  sure no run is live, remove it manually: rm -rf '$LOCK'" >&2
  exit 1
fi
trap 'rm -rf "$LOCK"' EXIT

# --- [1] the UNCHANGED M2 EXIT GATE (bridge-fed form intact) ------------------
bash port/sim/check-sim.sh

# --- [2] build sim_host_live --------------------------------------------------
# TU list = check-sim.sh's exact list (KEPT IN SYNC — a dropped TU fails
# the link loudly) + ai.c + sim_ai_live.c.
test -f "$TABLES/ml_tables.c" || fail "check-sim.sh did not leave $TABLES/ml_tables.c"
test -f "$TABLES/ml_stages.c" || fail "check-sim.sh did not leave $TABLES/ml_stages.c"
test -f "$BUILD/simdata.txt" || fail "check-sim.sh did not leave $BUILD/simdata.txt"
cc -O2 -ffp-contract=off -Wall -Wextra -Werror \
  -I"$TABLES" -Iport/ryu -Iport/sim -Ioracle/qjs \
  -o "$BUILD/sim_host_live" \
  "$SIM/sim_main.c" "$SIM/sim_boot.c" "$SIM/sim_tick.c" \
  "$SIM/sim_ser.c" "$SIM/sim_data.c" "$SIM/sim_ai_live.c" \
  "$CAL/canon.c" "$CAL/player_canon.c" \
  port/sim/ai.c \
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
echo "build OK: $BUILD/sim_host_live (cc -O2 -ffp-contract=off)"

# M2-CONTRACT WITNESS: the gate's own binary (no live TU) must still
# refuse --cpu without --ai-bridge (rc 1, usage) — the live seam changed
# NOTHING about the frozen gate's behavior.
rc=0
"$BUILD/sim_host" --trace "$BUILD/g07.trace.txt" --simdata "$BUILD/simdata.txt" \
  --seed 7307 --p1 3 --p2 4 --stage 0 --frames 1 --cpu --difficulty 5 \
  > /dev/null 2>&1 || rc=$?
if [ "$rc" != 1 ]; then
  fail "M2-contract witness — sim_host (bridge-only build) accepted --cpu without --ai-bridge (rc $rc, want the usage error 1)"
fi
echo "M2-contract witness OK: sim_host still refuses --cpu without --ai-bridge"

# --- [3] LIVE g07/g08 vs the frozen oracle streams ----------------------------
live_replay() { # id name seed p1 p2 stage frames difficulty tracetxt frozen manifest
  local id="$1" name="$2" seed="$3" p1="$4" p2="$5" stage="$6" frames="$7" \
        difficulty="$8" tracetxt="$9" frozen="${10}" manifest="${11}"
  test -f "$tracetxt" || fail "$id: trace txt missing: $tracetxt"
  echo "== LIVE $id ($name, difficulty $difficulty — no --ai-bridge)"
  "$BUILD/sim_host_live" \
    --trace "$tracetxt" --simdata "$BUILD/simdata.txt" \
    --seed "$seed" --p1 "$p1" --p2 "$p2" --stage "$stage" \
    --frames "$frames" --cpu --difficulty "$difficulty" --ai-cover \
    > "$BUILD/$id.ai-live-out.txt" 2> "$BUILD/$id.ai-live-cov.txt"
  if [ "$manifest" = "-" ]; then
    node "$SIM/wrap-run.js" "$id" "$BUILD/$id.ai-live-out.txt" \
      "$BUILD/$id.ai-live-run.json"
  else
    node "$SIM/wrap-run.js" "$id" "$BUILD/$id.ai-live-out.txt" \
      "$BUILD/$id.ai-live-run.json" "$manifest"
  fi
  node oracle/harness/verify-stream.js "$BUILD/$id.ai-live-run.json" "$frozen"
}

for id in g07 g08; do
  eval "$(node -e "
    const m=require('./oracle/goldens/manifest.json');
    const g=m.goldens.find(x=>x.id==='$id');
    console.log('name='+g.name);
    console.log('seed='+g.seed);
    console.log('p1='+g.p1); console.log('p2='+g.p2);
    console.log('stage='+g.stage);
    console.log('frames='+g.frames);
    console.log('difficulty='+(g.difficulty||5));
  ")"
  live_replay "$id" "$name" "$seed" "$p1" "$p2" "$stage" "$frames" \
    "$difficulty" "$BUILD/$id.trace.txt" \
    "oracle/goldens/$name.sha256.json" "-"
done

# --- [4] LIVE m01/m02 vs the frozen M4 streams --------------------------------
for id in m01 m02; do
  eval "$(node -e "
    const fs=require('fs');
    const m=JSON.parse(fs.readFileSync('port/goldens-m4/manifest.json','utf8'));
    const g=m.goldens.find(x=>x.id==='$id');
    console.log('name='+g.name);
    console.log('trace='+g.trace);
    console.log('seed='+g.seed);
    console.log('p1='+g.p1); console.log('p2='+g.p2);
    console.log('stage='+g.stage);
    console.log('frames='+g.frames);
    console.log('difficulty='+g.difficulty);
  ")"
  node "$SIM/trace-to-txt.js" "$M4G/$trace" "$BUILD/$id.trace.txt"
  live_replay "$id" "$name" "$seed" "$p1" "$p2" "$stage" "$frames" \
    "$difficulty" "$BUILD/$id.trace.txt" \
    "$M4G/$name.sha256.json" "$M4G/manifest.json"
done

# --- [5]+[6] the archival + port rigs intact ----------------------------------
bash "$CAL/check-ai-bridge.sh"
bash "$CAL/check-ai-replay.sh"

# --- no-commit guard (rc CASE-SPLIT; build output is never tracked) -----------
rc=0
dirty="$(git status --porcelain -- "$BUILD" "$TABLES")" || rc=$?
if [ "$rc" -ne 0 ]; then
  fail "no-commit guard — git status rc $rc (a status read error is CORRUPT evidence, never a clean pass)"
fi
if [ -n "$dirty" ]; then
  echo "AI LIVE FAIL: build output not gitignored:" >&2
  printf '%s\n' "$dirty" >&2
  exit 1
fi

echo "AI LIVE CONFORMS"
