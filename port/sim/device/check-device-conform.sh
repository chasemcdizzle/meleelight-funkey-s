#!/usr/bin/env bash
# M3 task 2 done-check: ALL-8 device conformance + sim-only p99 timing
# (fix_plan §M3 task 2).
#
# Replays EVERY golden in oracle/goldens/manifest.json ON the FunKey-S
# (g01-g06 human traces; g07/g08 CPU with their AIBRIDGE1 artifacts —
# built via the task-16 recipe when absent, pushed and fed to sim_device
# as --cpu --difficulty --ai-bridge, mirroring check-sim.sh), pulls every
# checksum stream and judges ALL of them ON THE HOST with the UNCHANGED
# oracle/harness/verify-stream.js against the frozen
# oracle/goldens/*.sha256.json — exact per-frame hash equality, FULL
# length, rngCalls/rngCallsOutsideStep/specVersion pins. Exact equality
# only — an armv7 divergence is a REAL finding (ledger + class fix,
# localized via sim_device --dump-frames vs the oracle harness's
# --capture-frames), never an epsilon.
#
# SIM-ONLY TIMING: each device run also passes --timing (sim_main.c —
# CLOCK_MONOTONIC ns around sim_game_tick + sim_frame_hash, buffered in
# RAM, written post-run: zero file I/O inside the frame loop). The device
# only RECORDS; the ns files are pulled and judged host-side by
# port/sim/device/percentiles.js (nearest-rank p50/p99, strict grammar).
# ASSERT per golden: p99_ns < 16670000 (16.67 ms). The measured table
# (golden, frames, p50, p99, wall) is printed and written to
# $DEVB/device-perf-rows.md.
#
# PERF-HISTORY SPLIT (iter 45, review 43-1 Medium): the tracked
# docs/research/device-perf.md is append-only measured history whose
# CONTENT is maintained by the WRITER from the printed rows artifact — a
# check script must never dirty the tracked tree (rig_no_commit_guard).
# This script's duty is a READ-ONLY structural PRESENCE assertion: after
# the current run's percentiles are judged, device-perf.md must carry a
# measured table row (golden id + numeric p50/p99 columns) for EVERY
# golden in the pinned matrix — stale/missing history (a skipped writer
# duty) is a loud death, never a silent pass.
#
# MATRIX PIN (iter 45, review 43-1 High): the task-2 matrix is EXACTLY
# the 8 frozen goldens g01..g08 with g07/g08 (and ONLY they) in the CPU
# role. That set is asserted against the manifest BEFORE any build or
# device work, and the <n>/<n> count derives from the PINNED set — a
# truncated, duplicated, or role-flipped manifest can never shrink the
# matrix and still print DEVICE CONFORMS. The manifest path is
# MLFK_MANIFEST-overridable for NEGATIVE TESTING ONLY (default
# oracle/goldens/manifest.json unchanged; traces and frozen streams
# always resolve under oracle/goldens/ regardless, so a doctored copy
# can rename nothing past the pin + the frozen-stream judgment).
#
# Rig plumbing is INHERITED from port/sim/device/riglib.sh (the
# iters-38-42 Tier-A arc's package, VERDICT: GO): nonce-dsh for every
# device command, pullv for every pulled artifact, rm-before-produce +
# made() for every host-side artifact between a fallible producer and a
# judge, the shared stamp-cached arm build (RIG_SCRIPTS: this script's
# bytes are stamp input), rehash-adjacent-to-push + push provenance, the
# SHARED no-reclaim device-keyed lock, the no-commit guard. The fdlibm/
# mathsweep/format CORPUS pins stay check-device-g01.sh-OWNED — this
# script does not duplicate the sweeps.
#
# ALL judgment happens on the host; the device never self-reports.
# Device hygiene: writes only under /tmp/mlfk + /mnt/mlfk-scratch, both
# removed on exit (trap).
#
# Env: FUNKEY_ADB_ID (device id), MLFK_FORCE_ARM=1 (ignore build stamp),
# MLFK_MANIFEST (manifest path override — negative testing only).
# Prints DEVICE CONFORMS <n>/<n> and SIM P99 OK, exit 0.
set -euo pipefail
cd "$(dirname "$0")/../../.."

CAL=port/sim/calib
BUILD=$CAL/build
DEVB=$BUILD/device
SIM=port/sim/sim
TABLES=pipeline/build/sim-tables
FDC=oracle/fdlibm-crosscheck
DTMP=/tmp/mlfk
DSD=/mnt/mlfk-scratch
MANIFEST="${MLFK_MANIFEST:-oracle/goldens/manifest.json}"
mkdir -p "$DEVB"

# THE PINNED MATRIX (iter 45, review 43-1 High) — the frozen golden set
# this check exists to prove, and the ONLY set it will accept from the
# manifest. Sorted, space-separated (set equality below is string
# equality on the sorted id list — implies count AND uniqueness).
PINNED_GOLDEN_SET="g01 g02 g03 g04 g05 g06 g07 g08"
PINNED_CPU_SET="g07 g08" # exactly these carry the CPU role (cpu=1)

# 16.67 ms in integer ns — the PLAN §4/M3 frame budget. The compare is
# integer (p99_ns < P99_LIMIT_NS): no float arithmetic in the gate.
P99_LIMIT_NS=16670000

source port/sim/device/adbsh.sh # (also defines $DEV — it keys the lock)
source port/sim/device/riglib.sh

rig_lock_acquire
trap rig_cleanup EXIT

require_device
rig_devsha_selftest

# golden_params <id> — manifest extraction under the reviewed no-eval
# strict line parser (check-device-g01.sh's gparams class: iter 39
# review M4 / iter 40 round 2), extended with the cpu/difficulty/trace
# fields the CPU goldens need. Sets: name seed p1 p2 stage frames cpu
# difficulty trace. Any unexpected key, malformed value, or out-of-range
# param is a loud death.
golden_params() {
  local id gp gk gv
  id="$1"
  unset name seed p1 p2 stage frames cpu difficulty trace
  gp="$(MLFK_MANIFEST="$MANIFEST" node -e "
    const m=require(require('path').resolve(process.env.MLFK_MANIFEST));
    const g=m.goldens.find(x=>x.id==='$id');
    if(!g) throw new Error('$id missing from manifest');
    console.log('name='+g.name);
    console.log('seed='+g.seed);
    console.log('p1='+g.p1); console.log('p2='+g.p2);
    console.log('stage='+g.stage);
    console.log('frames='+g.frames);
    console.log('cpu='+(g.cpu?1:0));
    console.log('difficulty='+(g.cpu?g.difficulty:'-'));
    console.log('trace='+g.trace);
  ")" || { echo "DEVICE FAIL: $id manifest param extraction failed" >&2; exit 1; }
  if [ -z "$gp" ]; then
    echo "DEVICE FAIL: $id manifest param extraction returned nothing" >&2
    exit 1
  fi
  while IFS='=' read -r gk gv; do
    case "$gk" in
      name)
        if ! [[ "$gv" =~ ^[a-z0-9][a-z0-9-]*$ ]]; then
          echo "DEVICE FAIL: manifest $id.name fails validation ('$gv')" >&2
          exit 1
        fi
        name=$gv
        ;;
      trace)
        if ! [[ "$gv" =~ ^[a-z0-9][a-z0-9-]*\.trace\.json$ ]]; then
          echo "DEVICE FAIL: manifest $id.trace fails validation ('$gv')" >&2
          exit 1
        fi
        trace=$gv
        ;;
      cpu)
        if ! [[ "$gv" =~ ^[01]$ ]]; then
          echo "DEVICE FAIL: manifest $id.cpu not 0/1 ('$gv')" >&2
          exit 1
        fi
        cpu=$gv
        ;;
      difficulty)
        if ! [[ "$gv" =~ ^([1-9]|-)$ ]]; then
          echo "DEVICE FAIL: manifest $id.difficulty not 1-9 or '-' ('$gv')" >&2
          exit 1
        fi
        difficulty=$gv
        ;;
      seed|p1|p2|stage|frames)
        if ! [[ "$gv" =~ ^[0-9]+$ ]]; then
          echo "DEVICE FAIL: manifest $id.$gk not a decimal integer ('$gv')" >&2
          exit 1
        fi
        printf -v "$gk" '%s' "$gv"
        ;;
      *)
        echo "DEVICE FAIL: unexpected manifest extraction line '$gk=$gv'" >&2
        exit 1
        ;;
    esac
  done <<< "$gp"
  : "$name" "$seed" "$p1" "$p2" "$stage" "$frames" "$cpu" "$difficulty" "$trace"
  # domain sanity (manifest is trusted-but-verified data)
  [ "$frames" -le 5000 ] || { echo "DEVICE FAIL: $id frames $frames > 5000" >&2; exit 1; }
  [ "$stage" -le 5 ] || { echo "DEVICE FAIL: $id stage $stage > 5" >&2; exit 1; }
  [ "$p1" -le 4 ] || { echo "DEVICE FAIL: $id p1 $p1 > 4" >&2; exit 1; }
  [ "$p2" -le 4 ] || { echo "DEVICE FAIL: $id p2 $p2 > 4" >&2; exit 1; }
  if [ "$cpu" = 1 ] && [ "$difficulty" = "-" ]; then
    echo "DEVICE FAIL: $id is cpu but has no difficulty" >&2
    exit 1
  fi
  if [ ! -f "oracle/goldens/$trace" ]; then
    echo "DEVICE FAIL: $id trace file oracle/goldens/$trace missing" >&2
    exit 1
  fi
}

echo "== [1/5] host data plane (matrix pin + M1 tables + SIMDATA1 + all trace texts) =="
if [ ! -f "$MANIFEST" ]; then
  echo "DEVICE FAIL: manifest $MANIFEST missing" >&2
  exit 1
fi
gids="$(MLFK_MANIFEST="$MANIFEST" node -e "
  const m=require(require('path').resolve(process.env.MLFK_MANIFEST));
  if(!Array.isArray(m.goldens)||m.goldens.length===0) throw new Error('manifest has no goldens');
  console.log(m.goldens.map(g=>String(g.id)).join('\n'));
")" || { echo "DEVICE FAIL: golden id extraction failed" >&2; exit 1; }
if [ -z "$gids" ]; then
  echo "DEVICE FAIL: golden id extraction returned nothing" >&2
  exit 1
fi
for id in $PINNED_GOLDEN_SET; do
  if ! [[ "$id" =~ ^g[0-9]{2}$ ]]; then
    echo "DEVICE FAIL: golden id fails validation ('$id')" >&2
    exit 1
  fi
done
# MATRIX PIN (iter 45, review 43-1 High) — BEFORE any build/device work:
# (a) exact-set: the sorted manifest id list must be string-identical to
# the pinned set (implies exactly 8 entries, unique, exactly those ids —
# a duplicate g07 replacing g08, or a truncated 7-entry manifest, dies
# HERE, never as a smaller-but-passing matrix);
# (b) CPU role: g07/g08 (and only they) must carry cpu=1 (checked below
# via golden_params, whose strict parser also validates every field).
sorted_gids="$(printf '%s\n' $gids | LC_ALL=C sort | tr '\n' ' ')"
sorted_gids="${sorted_gids% }"
if [ "$sorted_gids" != "$PINNED_GOLDEN_SET" ]; then
  echo "DEVICE FAIL: matrix pin — manifest golden set is not the pinned matrix" >&2
  echo "  pinned:   {$PINNED_GOLDEN_SET}" >&2
  echo "  manifest: {$sorted_gids}" >&2
  exit 1
fi
for id in $PINNED_GOLDEN_SET; do
  golden_params "$id"
  case " $PINNED_CPU_SET " in
    *" $id "*) want_cpu=1 ;;
    *) want_cpu=0 ;;
  esac
  if [ "$cpu" != "$want_cpu" ]; then
    echo "DEVICE FAIL: matrix pin — $id cpu role is $cpu, pinned $want_cpu (CPU set = {$PINNED_CPU_SET})" >&2
    exit 1
  fi
done
# the <n>/<n> count derives from the PINNED set, never raw manifest
# cardinality; every later loop iterates the pinned set.
n=0
for id in $PINNED_GOLDEN_SET; do n=$((n + 1)); done
echo "   matrix pin OK: manifest == pinned set {$PINNED_GOLDEN_SET}, CPU role on {$PINNED_CPU_SET} only ($n goldens)"
bash pipeline/extractor/build-extractor.sh
rm -f "$TABLES/ml_tables.c" "$TABLES/ml_tables.h" \
  "$TABLES/ml_stages.c" "$TABLES/ml_stages.h"
node pipeline/run.js --only animations,tables,stages --out "$TABLES"
made "$TABLES/ml_tables.c" "$TABLES/ml_tables.h" \
  "$TABLES/ml_stages.c" "$TABLES/ml_stages.h"
rm -f "$DEVB/simdata.txt"
node "$CAL/dump-sim-data.js" --out "$DEVB/simdata.txt"
made "$DEVB/simdata.txt"
for id in $PINNED_GOLDEN_SET; do
  golden_params "$id"
  rm -f "$DEVB/$id.trace.txt"
  node "$SIM/trace-to-txt.js" "oracle/goldens/$trace" "$DEVB/$id.trace.txt"
  made "$DEVB/$id.trace.txt"
done

echo "== [2/5] AI-bridge artifacts for the CPU goldens =="
# Present artifacts are REUSED (check-sim.sh precedent): an AIBRIDGE1
# file is a cached INPUT whose validity is proven downstream three ways
# — sim_main hard-fails unless the bridge header's seed/boot match the
# run, ml_ai_bridge_apply bit-verifies every recorded draw against the
# CHAINED mulberry32, and the run's stream must equal the frozen golden.
# A wrong/stale bridge cannot yield a passing stream. Fresh builds
# (absent artifact) get the full rm-before-produce + made() +
# STREAM-MATCH-guarded capture treatment (fix_plan §M2 task 16 recipe).
for id in $PINNED_GOLDEN_SET; do
  golden_params "$id"
  [ "$cpu" = 1 ] || continue
  if [ ! -f "$BUILD/$id.ai-bridge.txt" ]; then
    echo "   $id: AI bridge artifact absent — recording the ai capture"
    rm -f "$BUILD/$id.ai.jsonl" "$BUILD/$id.ai-run.json"
    node "$CAL/run-capture.js" --spec ai --golden "$id" \
      --out-jsonl "$BUILD/$id.ai.jsonl" \
      --out-run "$BUILD/$id.ai-run.json"
    made "$BUILD/$id.ai.jsonl" "$BUILD/$id.ai-run.json"
    node oracle/harness/verify-stream.js "$BUILD/$id.ai-run.json" \
      "oracle/goldens/$name.sha256.json"
    rm -f "$BUILD/$id.ai-bridge.txt"
    node "$CAL/build-ai-bridge.js" "$id" "$BUILD/$id.ai.jsonl" \
      "$BUILD/$id.ai-bridge.txt"
    made "$BUILD/$id.ai-bridge.txt"
  else
    echo "   $id: AI bridge present ($BUILD/$id.ai-bridge.txt) — reused (validity proven downstream: header pins + draw-chain verify + frozen-stream judgment)"
  fi
done

echo "== [3/5] armv7 static cross-build (SDK gcc; stamp-cached, shared with check-device-g01.sh) =="
rig_arm_build

echo "== [4/5] device: replay all $n goldens (streams + --timing pulled, judged on host) =="
rig_stamp_rehash sim_device
dsh "rm -rf $DTMP $DSD && mkdir -p $DTMP $DSD"
push_files=("$DEVB/sim_device" "$DEVB/simdata.txt")
for id in $PINNED_GOLDEN_SET; do
  push_files+=("$DEVB/$id.trace.txt")
  golden_params "$id"
  if [ "$cpu" = 1 ]; then
    push_files+=("$BUILD/$id.ai-bridge.txt")
  fi
done
adb -s "$DEV" push "${push_files[@]}" "$DTMP/" >/dev/null
rig_push_provenance "$DTMP" sim_device
dsh "chmod +x $DTMP/sim_device"

pass=0
rows=""
for id in $PINNED_GOLDEN_SET; do
  golden_params "$id"
  cmd="$DTMP/sim_device --trace $DTMP/$id.trace.txt --simdata $DTMP/simdata.txt"
  cmd="$cmd --seed $seed --p1 $p1 --p2 $p2 --stage $stage --frames $frames"
  if [ "$cpu" = 1 ]; then
    cmd="$cmd --cpu --difficulty $difficulty --ai-bridge $DTMP/$id.ai-bridge.txt"
  fi
  cmd="$cmd --timing $DTMP/$id.timing.txt > $DTMP/$id.sim-out.txt"
  t0=$(date +%s)
  dsh "sh -lc '$cmd'"
  t1=$(date +%s)
  wall=$((t1 - t0))
  pullv "$DTMP/$id.sim-out.txt" "$DEVB/$id.sim-out.device.txt"
  pullv "$DTMP/$id.timing.txt" "$DEVB/$id.timing.device.txt"
  dsh "rm -f $DTMP/$id.sim-out.txt $DTMP/$id.timing.txt $DTMP/$id.trace.txt"
  # stream judgment: the UNCHANGED oracle verifier vs the frozen stream
  rm -f "$DEVB/$id.sim-run.device.json" # freshness: wrap-run exiting 0
  # without writing must never let verify-stream judge a PRIOR run's wrap
  node "$SIM/wrap-run.js" "$id" "$DEVB/$id.sim-out.device.txt" \
    "$DEVB/$id.sim-run.device.json"
  made "$DEVB/$id.sim-run.device.json"
  node oracle/harness/verify-stream.js "$DEVB/$id.sim-run.device.json" \
    "oracle/goldens/$name.sha256.json"
  # timing judgment (host-side; strict grammar + no-eval strict parse)
  unset p50_ns p99_ns p50_ms p99_ms
  pout="$(node port/sim/device/percentiles.js \
    "$DEVB/$id.timing.device.txt" "$frames")" || {
    echo "DEVICE FAIL: percentile judgment failed for $id" >&2
    exit 1
  }
  while IFS='=' read -r pk pv; do
    case "$pk" in
      p50_ns|p99_ns)
        if ! [[ "$pv" =~ ^[0-9]+$ ]]; then
          echo "DEVICE FAIL: percentiles $id.$pk not a decimal integer ('$pv')" >&2
          exit 1
        fi
        printf -v "$pk" '%s' "$pv"
        ;;
      p50_ms|p99_ms)
        if ! [[ "$pv" =~ ^[0-9]+\.[0-9]{3}$ ]]; then
          echo "DEVICE FAIL: percentiles $id.$pk malformed ('$pv')" >&2
          exit 1
        fi
        printf -v "$pk" '%s' "$pv"
        ;;
      *)
        echo "DEVICE FAIL: unexpected percentiles line '$pk=$pv'" >&2
        exit 1
        ;;
    esac
  done <<< "$pout"
  : "$p50_ns" "$p99_ns" "$p50_ms" "$p99_ms"
  if [ "$p99_ns" -ge "$P99_LIMIT_NS" ]; then
    echo "SIM P99 FAIL: $id sim-only p99 ${p99_ms} ms (${p99_ns} ns) >= limit ${P99_LIMIT_NS} ns" >&2
    exit 1
  fi
  echo "   $id: STREAM MATCH + sim-only p50 ${p50_ms} ms / p99 ${p99_ms} ms (device wall ${wall} s)"
  rows="$rows| $id | $frames | $p50_ms | $p99_ms | $wall |"$'\n'
  pass=$((pass + 1))
done
dsh "rm -rf $DTMP"

echo "== [5/5] measured sim-only timing (device; nearest-rank percentiles) =="
rm -f "$DEVB/device-perf-rows.md"
{
  echo "| golden | frames | p50 ms | p99 ms | wall s |"
  echo "|---|---|---|---|---|"
  printf '%s' "$rows"
} > "$DEVB/device-perf-rows.md"
made "$DEVB/device-perf-rows.md"
cat "$DEVB/device-perf-rows.md"

# PERF-HISTORY PRESENCE (iter 45, review 43-1 Medium; header "PERF-
# HISTORY SPLIT"): READ-ONLY structural assertion that the tracked
# append-only history carries a measured table row for EVERY pinned
# golden — `| gNN | <int> | <numeric p50> | <numeric p99> | <int> |`.
# The row CONTENT (appending the fresh table above) stays a WRITER duty;
# this script never writes the tracked tree (rig_no_commit_guard).
# Missing/mangled history = the writer duty was skipped = loud death.
PERFDOC=docs/research/device-perf.md
if [ ! -f "$PERFDOC" ]; then
  echo "DEVICE FAIL: $PERFDOC missing — the measured perf history is a tracked writer-maintained doc" >&2
  exit 1
fi
for id in $PINNED_GOLDEN_SET; do
  if ! grep -Eq "^\| $id \| [0-9]+ \| [0-9]+(\.[0-9]+)? \| [0-9]+(\.[0-9]+)? \| [0-9]+ \|" "$PERFDOC"; then
    echo "DEVICE FAIL: $PERFDOC has no measured table row for $id — append the printed rows artifact ($DEVB/device-perf-rows.md) to the doc (writer duty; the check only asserts presence)" >&2
    exit 1
  fi
done
echo "   perf-history presence: $PERFDOC carries a measured row for all $n pinned goldens"

if [ "$pass" != "$n" ]; then
  echo "DEVICE FAIL: $pass of $n goldens verified" >&2
  exit 1
fi

rig_no_commit_guard "$BUILD" "$TABLES"

echo "DEVICE CONFORMS $pass/$n"
echo "SIM P99 OK"
