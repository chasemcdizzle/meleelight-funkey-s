#!/usr/bin/env bash
# M3 task 1 done-check: the armv7 CORRECTNESS RUNG (fix_plan §M3 task 1).
#
# Validates the entire M2 result on the real FunKey-S CPU before anything
# visual lands. Cross-builds the FULL headless sim (check-sim.sh's exact
# TU list — keep in sync), the fdlibm sweep (oracle/fdlibm-crosscheck)
# and the format differential (calib/fmt_diff.c) as STATIC armv7 binaries
# (SDK gcc, -O2 -ffp-contract=off), pushes them over ADB, and runs ON THE
# DEVICE:
#   [4] the ~257k-input fdlibm sweep      -> cmp vs the host-C output
#       (which the standing M0 gate proves == JS == browser)
#   [5] the exact-math family sweep (mathsweep.c: floor/ceil/sqrt/fabs/
#       fmod/js_round — the sim's whole non-transcendental libm surface)
#       -> cmp vs the HOST-LIBM ANCHOR build (mathsweep_host is
#       deliberately NOT linked with fdlibm.c, so the device's fdlibm.c
#       floor/ceil strong overrides are proven equal to a known-good
#       libm, never merely to themselves). Background: the SDK's static
#       musl libc.a floor/ceil/round are BROKEN (identity for
#       non-integers — measured iter 38); fdlibm.c carries the fix.
#   [6] fmt_diff --self-test, then the FULL adversarial format corpus:
#       GENERATED on the device (cmp vs the host corpus, whose sha is the
#       frozen expected-format.json pin) and formatted on the device
#       (cmp vs the host output, which check-format.sh proves == V8/oracle)
#   [7] the full g01 golden replay        -> pulled and judged by the
#       UNCHANGED oracle/harness/verify-stream.js against the frozen
#       g01 stream (exact per-frame equality, full length, rng pins)
#
# ALL judgment happens on the host; the device never self-reports.
# Device hygiene: writes only under /tmp/mlfk + /mnt/mlfk-scratch, both
# removed on exit (trap). Exact equality only — an armv7 divergence is a
# REAL finding (ledger + class fix), never an epsilon.
#
# Env: FUNKEY_ADB_ID (device id), MLFK_FORCE_ARM=1 (ignore build stamp).
# Exclusive host lock: ${TMPDIR:-/tmp}/mlfk-rig-<device-id>.lock — SHARED
# across checkouts/worktrees (the device is the shared resource); an
# existing lock is a loud death, removed only by hand.
# Prints DEVICE CONFORMS g01, exit 0.
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
mkdir -p "$DEVB"

source port/sim/device/adbsh.sh # (also defines $DEV — it keys the lock)
# SHARED rig plumbing (extracted verbatim after the iters-38-42 Tier-A
# arc closed GO; the provenance comments live at the definitions):
# rig_lock_acquire, rig_cleanup, rig_devsha_selftest, pullv, made,
# rig_arm_build (+ srchash/stamp; RIG_SCRIPTS makes every rig script's
# bytes stamp input), rig_stamp_rehash, rig_push_provenance,
# rig_no_commit_guard, $ARMBINS.
source port/sim/device/riglib.sh

rig_lock_acquire
trap rig_cleanup EXIT

require_device
rig_devsha_selftest

echo "== [1/7] host data plane (M1 tables + SIMDATA1 + g01 trace text) =="
# g01 match params — SINGLE SOURCE: the goldens manifest. Extracted HERE,
# before any device work, with explicit failure checks (iter 39, review
# M4: eval "$(node …)" swallowed a node failure; set -u alone can't catch
# inherited/partial values, so unset first). NO EVAL (iter 40, review M4
# round 2: manifest values are data, never shell text) — parsed
# line-by-line into variables under strict per-field validation; any
# unexpected key, malformed value, or out-of-range param is a loud death.
unset name seed p1 p2 stage frames
gparams="$(node -e "
  const m=require('./oracle/goldens/manifest.json');
  const g=m.goldens.find(x=>x.id==='g01');
  if(!g) throw new Error('g01 missing from manifest');
  console.log('name='+g.name);
  console.log('seed='+g.seed);
  console.log('p1='+g.p1); console.log('p2='+g.p2);
  console.log('stage='+g.stage);
  console.log('frames='+g.frames);
")" || { echo "DEVICE FAIL: g01 manifest param extraction failed" >&2; exit 1; }
if [ -z "$gparams" ]; then
  echo "DEVICE FAIL: g01 manifest param extraction returned nothing" >&2
  exit 1
fi
while IFS='=' read -r gk gv; do
  case "$gk" in
    name)
      if ! [[ "$gv" =~ ^[a-z0-9][a-z0-9-]*$ ]]; then
        echo "DEVICE FAIL: manifest g01.name fails validation ('$gv')" >&2
        exit 1
      fi
      name=$gv
      ;;
    seed|p1|p2|stage|frames)
      if ! [[ "$gv" =~ ^[0-9]+$ ]]; then
        echo "DEVICE FAIL: manifest g01.$gk not a decimal integer ('$gv')" >&2
        exit 1
      fi
      printf -v "$gk" '%s' "$gv"
      ;;
    *)
      echo "DEVICE FAIL: unexpected manifest extraction line '$gk=$gv'" >&2
      exit 1
      ;;
  esac
done <<< "$gparams"
: "$name" "$seed" "$p1" "$p2" "$stage" "$frames" # set -u backstop: all six present
# domain sanity (manifest is trusted-but-verified data)
[ "$frames" -le 5000 ] || { echo "DEVICE FAIL: g01 frames $frames > 5000" >&2; exit 1; }
[ "$stage" -le 5 ] || { echo "DEVICE FAIL: g01 stage $stage > 5" >&2; exit 1; }
[ "$p1" -le 4 ] || { echo "DEVICE FAIL: g01 p1 $p1 > 4" >&2; exit 1; }
[ "$p2" -le 4 ] || { echo "DEVICE FAIL: g01 p2 $p2 > 4" >&2; exit 1; }
bash pipeline/extractor/build-extractor.sh
rm -f "$TABLES/ml_tables.c" "$TABLES/ml_tables.h" \
  "$TABLES/ml_stages.c" "$TABLES/ml_stages.h"
node pipeline/run.js --only animations,tables,stages --out "$TABLES"
made "$TABLES/ml_tables.c" "$TABLES/ml_tables.h" \
  "$TABLES/ml_stages.c" "$TABLES/ml_stages.h"
rm -f "$DEVB/simdata.txt"
node "$CAL/dump-sim-data.js" --out "$DEVB/simdata.txt"
made "$DEVB/simdata.txt"
rm -f "$DEVB/g01.trace.txt"
node "$SIM/trace-to-txt.js" \
  oracle/goldens/g01-fox-marth-battlefield.trace.json "$DEVB/g01.trace.txt"
made "$DEVB/g01.trace.txt"

echo "== [2/7] host references (fdlibm sweep + libm anchor + pinned format corpus) =="
# FROZEN corpus size (iter 40, review M3 round 2): the expected line
# count is a LITERAL measured-then-frozen from gen-inputs.js
# (2026-07-16: 257,287 lines — deterministic generator), never
# re-derived from the freshly generated file, so a generator regression
# can no longer satisfy its own trailer checks. The generated corpus
# AND both sweep legs' `n` trailers are asserted against this literal.
CORPUS_LINES=257287
# CORPUS IDENTITY pin (iter 41, review M round 3): the line count pins
# CARDINALITY, not identity — a generator regression repeating one
# valid operand the right number of times passes the count, the
# grammar, and the host==device comparison while losing coverage. The
# corpus is deterministic (measured ×2 byte-identical, 2026-07-16), so
# its sha256 is a measured-then-frozen LITERAL and the freshly
# generated bytes must match BEFORE any use. NOTE: the fdlibm sweep
# [4] and the exact-math sweep [5] consume this ONE generated file, so
# this single literal is the identity pin for BOTH sweeps' corpora
# (the format corpus already carries its own frozen pin via
# check-format-pins.js / expected-format.json).
CORPUS_SHA256=b164802a98932c2c8780febfe2c857d5771d12a327d3465291221861da6b3d05
rm -f "$DEVB/fdlibm-inputs.txt" # freshness (round 4, Medium :204): a
# stale corpus carries the frozen sha too — rm-before-produce is the
# only freshness proof
node "$FDC/gen-inputs.js" "$DEVB/fdlibm-inputs.txt"
made "$DEVB/fdlibm-inputs.txt"
corpuslines="$(wc -l < "$DEVB/fdlibm-inputs.txt" | tr -d ' ')"
if [ "$corpuslines" != "$CORPUS_LINES" ]; then
  echo "DEVICE FAIL: generated corpus has $corpuslines lines, frozen pin is $CORPUS_LINES" >&2
  exit 1
fi
corpussha="$(shasum -a 256 "$DEVB/fdlibm-inputs.txt" | cut -d' ' -f1)"
if [ "$corpussha" != "$CORPUS_SHA256" ]; then
  echo "DEVICE FAIL: generated corpus sha256 $corpussha != frozen pin $CORPUS_SHA256" >&2
  exit 1
fi
rm -f "$DEVB/csweep_host"
cc -O2 -ffp-contract=off -std=c99 -Wall -Iport/fdlibm \
  "$FDC/csweep.c" port/fdlibm/fdlibm.c -o "$DEVB/csweep_host"
made "$DEVB/csweep_host"
rm -f "$DEVB/fdlibm-host.txt"
"$DEVB/csweep_host" "$DEVB/fdlibm-inputs.txt" > "$DEVB/fdlibm-host.txt"
made "$DEVB/fdlibm-host.txt"
# HOST-LIBM ANCHOR: no fdlibm.c in this link (see header note [5])
rm -f "$DEVB/mathsweep_host"
cc -O2 -ffp-contract=off -Wall -Wextra -Werror -Iport/sim \
  port/sim/device/mathsweep.c -o "$DEVB/mathsweep_host" -lm
made "$DEVB/mathsweep_host"
rm -f "$DEVB/mathsweep-host.txt"
"$DEVB/mathsweep_host" "$DEVB/fdlibm-inputs.txt" > "$DEVB/mathsweep-host.txt"
made "$DEVB/mathsweep-host.txt"
# review M3: the sweep must have consumed the WHOLE corpus (host leg;
# the device leg is asserted in step [5] after the pull) — judged
# against the FROZEN literal, never the regenerated file
tail -1 "$DEVB/mathsweep-host.txt" | grep -qx "n $CORPUS_LINES" || {
  echo "DEVICE FAIL: host mathsweep trailer != frozen corpus pin ($CORPUS_LINES)" >&2
  exit 1
}
rm -f "$DEVB/fmt_diff_host"
cc -O2 -ffp-contract=off -Wall -Wextra -Werror \
  -Iport/ryu -Iport/sim -Ioracle/qjs \
  -o "$DEVB/fmt_diff_host" \
  "$CAL/fmt_diff.c" "$CAL/canon.c" port/sim/ml_ser.c port/sim/ml_fmt.c \
  oracle/qjs/sha256.c -lm
made "$DEVB/fmt_diff_host"
rm -f "$DEVB/fmt-adv.hex" # freshness: a stale corpus satisfies the
# expected-format.json pin too — rm-before-produce
"$DEVB/fmt_diff_host" --gen "$DEVB/fmt-adv.hex"
made "$DEVB/fmt-adv.hex"
node "$CAL/check-format-pins.js" adversarial "$DEVB/fmt-adv.hex"
rm -f "$DEVB/fmt-adv.host.txt"
"$DEVB/fmt_diff_host" --format "$DEVB/fmt-adv.hex" "$DEVB/fmt-adv.host.txt"
made "$DEVB/fmt-adv.host.txt"

echo "== [3/7] armv7 static cross-build (SDK gcc; stamp-cached) =="
# Stamp inputs (iter 39, review H2; now shared via riglib.sh): sources +
# generated tables + the RIG SCRIPTS' bytes (RIG_SCRIPTS — incl. this
# script; the build recipe/flags live in rig_arm_build's heredoc) + the
# docker image Id. The stamp also records each produced binary's sha256;
# the cache-HIT path re-hashes the cached binaries and any mismatch
# forces a rebuild — a stamped-but-tampered binary is never judged.
rig_arm_build


echo "== [4/7] device: fdlibm sweep (armv7 vs host, byte-exact) =="
# rehash ADJACENT to the push + PUSH PROVENANCE (riglib.sh: iter 40
# review M2 round 2 / iter 41 review M round 3 — provenance comments at
# the definitions).
rig_stamp_rehash $ARMBINS
dsh "rm -rf $DTMP $DSD && mkdir -p $DTMP $DSD"
adb -s "$DEV" push \
  "$DEVB/csweep_arm" "$DEVB/fmt_diff_arm" "$DEVB/mathsweep_arm" \
  "$DEVB/sim_device" \
  "$DEVB/fdlibm-inputs.txt" "$DEVB/g01.trace.txt" "$DEVB/simdata.txt" \
  "$DTMP/" >/dev/null
rig_push_provenance "$DTMP" $ARMBINS
dsh "chmod +x $DTMP/csweep_arm $DTMP/fmt_diff_arm $DTMP/mathsweep_arm $DTMP/sim_device"
dsh "sh -lc '$DTMP/csweep_arm $DTMP/fdlibm-inputs.txt > $DTMP/fdlibm-device.txt'"
pullv "$DTMP/fdlibm-device.txt" "$DEVB/fdlibm-device.txt"
cmp "$DEVB/fdlibm-host.txt" "$DEVB/fdlibm-device.txt"
echo "   fdlibm sweep: device == host ($(wc -l < "$DEVB/fdlibm-device.txt" | tr -d ' ') lines)"
dsh "rm -f $DTMP/fdlibm-device.txt"

echo "== [5/7] device: exact-math family sweep (floor/ceil/sqrt/fabs/fmod/js_round) =="
dsh "sh -lc '$DTMP/mathsweep_arm $DTMP/fdlibm-inputs.txt > $DSD/mathsweep-device.txt'"
pullv "$DSD/mathsweep-device.txt" "$DEVB/mathsweep-device.txt"
# review M3, device leg: the sweep must have consumed the WHOLE corpus
# (judged against the FROZEN literal, never the regenerated file)
tail -1 "$DEVB/mathsweep-device.txt" | grep -qx "n $CORPUS_LINES" || {
  echo "DEVICE FAIL: device mathsweep trailer != frozen corpus pin ($CORPUS_LINES)" >&2
  exit 1
}
cmp "$DEVB/mathsweep-host.txt" "$DEVB/mathsweep-device.txt"
echo "   exact-math sweep: device(fdlibm overrides) == host libm anchor ($(wc -l < "$DEVB/mathsweep-device.txt" | tr -d ' ') lines)"
dsh "rm -f $DTMP/fdlibm-inputs.txt $DSD/mathsweep-device.txt"
rm -f "$DEVB/mathsweep-device.txt" # byte-dup of the kept host anchor

echo "== [6/7] device: formatter self-test + FULL adversarial corpus =="
dsh "sh -lc '$DTMP/fmt_diff_arm --self-test'"
dsh "sh -lc '$DTMP/fmt_diff_arm --gen $DSD/fmt-adv.hex'"
pullv "$DSD/fmt-adv.hex" "$DEVB/fmt-adv.device.hex"
cmp "$DEVB/fmt-adv.hex" "$DEVB/fmt-adv.device.hex"
echo "   corpus generator: device == host (the expected-format.json pinned corpus)"
dsh "sh -lc '$DTMP/fmt_diff_arm --format $DSD/fmt-adv.hex $DSD/fmt-adv.out.txt'"
pullv "$DSD/fmt-adv.out.txt" "$DEVB/fmt-adv.device.txt"
cmp "$DEVB/fmt-adv.host.txt" "$DEVB/fmt-adv.device.txt"
echo "   adversarial format sweep: device == host ($(wc -l < "$DEVB/fmt-adv.device.txt" | tr -d ' ') lines)"
dsh "rm -rf $DSD"
rm -f "$DEVB/fmt-adv.device.hex" "$DEVB/fmt-adv.device.txt" # byte-dups of the kept host copies

echo "== [7/7] device: g01 golden replay, judged by the frozen stream =="
# (match params extracted + validated in step [1] — single source, fail-loud)
t0=$(date +%s)
dsh "sh -lc '$DTMP/sim_device --trace $DTMP/g01.trace.txt --simdata $DTMP/simdata.txt --seed $seed --p1 $p1 --p2 $p2 --stage $stage --frames $frames > $DTMP/g01.sim-out.txt'"
t1=$(date +%s)
pullv "$DTMP/g01.sim-out.txt" "$DEVB/g01.sim-out.device.txt"
rm -f "$DEVB/g01.sim-run.device.json" # freshness (round 4, High :534):
# wrap-run exiting 0 without writing must never let verify-stream judge
# a PRIOR successful run's wrap
node "$SIM/wrap-run.js" g01 "$DEVB/g01.sim-out.device.txt" \
  "$DEVB/g01.sim-run.device.json"
made "$DEVB/g01.sim-run.device.json"
node oracle/harness/verify-stream.js "$DEVB/g01.sim-run.device.json" \
  "oracle/goldens/$name.sha256.json"
echo "   device sim wall clock: $((t1-t0)) s for $frames frames (informational)"
dsh "rm -rf $DTMP"

# no-commit guard (riglib.sh: iter 39 review L1 / iter 40 review L2 —
# provenance comments at the definition).
rig_no_commit_guard "$BUILD" "$TABLES"

echo "DEVICE CONFORMS g01"
