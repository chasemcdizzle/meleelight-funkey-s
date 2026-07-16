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

source port/sim/device/adbsh.sh
require_device
# hygiene: device scratch never outlives the script
trap 'adb -s "$DEV" shell "rm -rf '"$DTMP"' '"$DSD"'" </dev/null >/dev/null 2>&1 || true' EXIT

echo "== [1/7] host data plane (M1 tables + SIMDATA1 + g01 trace text) =="
bash pipeline/extractor/build-extractor.sh
node pipeline/run.js --only animations,tables,stages --out "$TABLES"
test -f "$TABLES/ml_tables.c"
test -f "$TABLES/ml_stages.c"
node "$CAL/dump-sim-data.js" --out "$DEVB/simdata.txt"
node "$SIM/trace-to-txt.js" \
  oracle/goldens/g01-fox-marth-battlefield.trace.json "$DEVB/g01.trace.txt"

echo "== [2/7] host references (fdlibm sweep + libm anchor + pinned format corpus) =="
node "$FDC/gen-inputs.js" "$DEVB/fdlibm-inputs.txt"
cc -O2 -ffp-contract=off -std=c99 -Wall -Iport/fdlibm \
  "$FDC/csweep.c" port/fdlibm/fdlibm.c -o "$DEVB/csweep_host"
"$DEVB/csweep_host" "$DEVB/fdlibm-inputs.txt" > "$DEVB/fdlibm-host.txt"
# HOST-LIBM ANCHOR: no fdlibm.c in this link (see header note [5])
cc -O2 -ffp-contract=off -Wall -Wextra -Werror -Iport/sim \
  port/sim/device/mathsweep.c -o "$DEVB/mathsweep_host" -lm
"$DEVB/mathsweep_host" "$DEVB/fdlibm-inputs.txt" > "$DEVB/mathsweep-host.txt"
cc -O2 -ffp-contract=off -Wall -Wextra -Werror \
  -Iport/ryu -Iport/sim -Ioracle/qjs \
  -o "$DEVB/fmt_diff_host" \
  "$CAL/fmt_diff.c" "$CAL/canon.c" port/sim/ml_ser.c port/sim/ml_fmt.c \
  oracle/qjs/sha256.c -lm
"$DEVB/fmt_diff_host" --gen "$DEVB/fmt-adv.hex"
node "$CAL/check-format-pins.js" adversarial "$DEVB/fmt-adv.hex"
"$DEVB/fmt_diff_host" --format "$DEVB/fmt-adv.hex" "$DEVB/fmt-adv.host.txt"

echo "== [3/7] armv7 static cross-build (SDK gcc; stamp-cached) =="
srchash() {
  {
    find port/sim port/fdlibm port/ryu oracle/qjs "$FDC/csweep.c" \
      -type f \( -name '*.c' -o -name '*.h' \) -print0 \
      | sort -z | xargs -0 shasum -a 256
    shasum -a 256 "$TABLES/ml_tables.c" "$TABLES/ml_stages.c"
  } | shasum -a 256 | cut -d' ' -f1
}
STAMP=$DEVB/arm-build.stamp
want="$(srchash)"
have=""
[ -f "$STAMP" ] && have="$(cat "$STAMP")"
if [ "${MLFK_FORCE_ARM:-0}" != 0 ] || [ "$have" != "$want" ] \
   || [ ! -f "$DEVB/sim_device" ] || [ ! -f "$DEVB/csweep_arm" ] \
   || [ ! -f "$DEVB/fmt_diff_arm" ] || [ ! -f "$DEVB/mathsweep_arm" ]; then
  rm -f "$STAMP"
  # SERIAL docker only (CLAUDE.md §Commands arm32 recipe)
  docker run --rm -v "$PWD":/work -w /work jondbell/funkey-s-sdk bash -lc '
    set -e
    export PATH=/opt/FunKey-sdk-2.3.0/bin:$PATH
    CC=arm-funkey-linux-musleabihf-gcc
    DEVB=port/sim/calib/build/device
    TABLES=pipeline/build/sim-tables
    CAL=port/sim/calib
    SIM=port/sim/sim
    # keep this TU list in sync with port/sim/check-sim.sh (the M2 gate)
    $CC -O2 -ffp-contract=off -Wall -Wextra -Werror -static \
      -I"$TABLES" -Iport/ryu -Iport/sim -Ioracle/qjs \
      -o "$DEVB/sim_device" \
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
    $CC -O2 -ffp-contract=off -std=c99 -Wall -static -Iport/fdlibm \
      oracle/fdlibm-crosscheck/csweep.c port/fdlibm/fdlibm.c \
      -o "$DEVB/csweep_arm"
    # fdlibm.c linked (exactly like sim_device): floor/ceil overrides live
    $CC -O2 -ffp-contract=off -Wall -Wextra -Werror -static -Iport/sim \
      port/sim/device/mathsweep.c port/fdlibm/fdlibm.c \
      -o "$DEVB/mathsweep_arm" -lm
    $CC -O2 -ffp-contract=off -Wall -Wextra -Werror -static \
      -Iport/ryu -Iport/sim -Ioracle/qjs \
      -o "$DEVB/fmt_diff_arm" \
      "$CAL/fmt_diff.c" "$CAL/canon.c" port/sim/ml_ser.c port/sim/ml_fmt.c \
      oracle/qjs/sha256.c -lm
  '
  for f in sim_device csweep_arm fmt_diff_arm mathsweep_arm; do
    file "$DEVB/$f" | grep -q "ELF 32-bit LSB executable, ARM" || {
      echo "DEVICE FAIL: $f is not an armv7 static executable" >&2
      exit 1
    }
  done
  printf '%s\n' "$want" > "$STAMP"
  echo "   arm binaries rebuilt (stamp $want)"
else
  echo "   arm binaries up to date (stamp $want)"
fi

echo "== [4/7] device: fdlibm sweep (armv7 vs host, byte-exact) =="
dsh "rm -rf $DTMP $DSD && mkdir -p $DTMP $DSD"
adb -s "$DEV" push \
  "$DEVB/csweep_arm" "$DEVB/fmt_diff_arm" "$DEVB/mathsweep_arm" \
  "$DEVB/sim_device" \
  "$DEVB/fdlibm-inputs.txt" "$DEVB/g01.trace.txt" "$DEVB/simdata.txt" \
  "$DTMP/" >/dev/null
dsh "chmod +x $DTMP/csweep_arm $DTMP/fmt_diff_arm $DTMP/mathsweep_arm $DTMP/sim_device"
dsh "sh -lc '$DTMP/csweep_arm $DTMP/fdlibm-inputs.txt > $DTMP/fdlibm-device.txt'"
adb -s "$DEV" pull "$DTMP/fdlibm-device.txt" "$DEVB/fdlibm-device.txt" >/dev/null
cmp "$DEVB/fdlibm-host.txt" "$DEVB/fdlibm-device.txt"
echo "   fdlibm sweep: device == host ($(wc -l < "$DEVB/fdlibm-device.txt" | tr -d ' ') lines)"
dsh "rm -f $DTMP/fdlibm-device.txt"

echo "== [5/7] device: exact-math family sweep (floor/ceil/sqrt/fabs/fmod/js_round) =="
dsh "sh -lc '$DTMP/mathsweep_arm $DTMP/fdlibm-inputs.txt > $DSD/mathsweep-device.txt'"
adb -s "$DEV" pull "$DSD/mathsweep-device.txt" "$DEVB/mathsweep-device.txt" >/dev/null
cmp "$DEVB/mathsweep-host.txt" "$DEVB/mathsweep-device.txt"
echo "   exact-math sweep: device(fdlibm overrides) == host libm anchor ($(wc -l < "$DEVB/mathsweep-device.txt" | tr -d ' ') lines)"
dsh "rm -f $DTMP/fdlibm-inputs.txt $DSD/mathsweep-device.txt"
rm -f "$DEVB/mathsweep-device.txt" # byte-dup of the kept host anchor

echo "== [6/7] device: formatter self-test + FULL adversarial corpus =="
dsh "sh -lc '$DTMP/fmt_diff_arm --self-test'"
dsh "sh -lc '$DTMP/fmt_diff_arm --gen $DSD/fmt-adv.hex'"
adb -s "$DEV" pull "$DSD/fmt-adv.hex" "$DEVB/fmt-adv.device.hex" >/dev/null
cmp "$DEVB/fmt-adv.hex" "$DEVB/fmt-adv.device.hex"
echo "   corpus generator: device == host (the expected-format.json pinned corpus)"
dsh "sh -lc '$DTMP/fmt_diff_arm --format $DSD/fmt-adv.hex $DSD/fmt-adv.out.txt'"
adb -s "$DEV" pull "$DSD/fmt-adv.out.txt" "$DEVB/fmt-adv.device.txt" >/dev/null
cmp "$DEVB/fmt-adv.host.txt" "$DEVB/fmt-adv.device.txt"
echo "   adversarial format sweep: device == host ($(wc -l < "$DEVB/fmt-adv.device.txt" | tr -d ' ') lines)"
dsh "rm -rf $DSD"
rm -f "$DEVB/fmt-adv.device.hex" "$DEVB/fmt-adv.device.txt" # byte-dups of the kept host copies

echo "== [7/7] device: g01 golden replay, judged by the frozen stream =="
eval "$(node -e "
  const m=require('./oracle/goldens/manifest.json');
  const g=m.goldens.find(x=>x.id==='g01');
  console.log('name='+g.name);
  console.log('seed='+g.seed);
  console.log('p1='+g.p1); console.log('p2='+g.p2);
  console.log('stage='+g.stage);
  console.log('frames='+g.frames);
")"
t0=$(date +%s)
dsh "sh -lc '$DTMP/sim_device --trace $DTMP/g01.trace.txt --simdata $DTMP/simdata.txt --seed $seed --p1 $p1 --p2 $p2 --stage $stage --frames $frames > $DTMP/g01.sim-out.txt'"
t1=$(date +%s)
adb -s "$DEV" pull "$DTMP/g01.sim-out.txt" "$DEVB/g01.sim-out.device.txt" >/dev/null
node "$SIM/wrap-run.js" g01 "$DEVB/g01.sim-out.device.txt" \
  "$DEVB/g01.sim-run.device.json"
node oracle/harness/verify-stream.js "$DEVB/g01.sim-run.device.json" \
  "oracle/goldens/$name.sha256.json"
echo "   device sim wall clock: $((t1-t0)) s for $frames frames (informational)"
dsh "rm -rf $DTMP"

# no-commit guard: build output is never tracked
if git status --porcelain -- "$BUILD" "$TABLES" | grep -q .; then
  echo "DEVICE FAIL: build output not gitignored" >&2
  exit 1
fi

echo "DEVICE CONFORMS g01"
