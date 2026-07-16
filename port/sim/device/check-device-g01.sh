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
# Exclusive per-host lock: $DEVB/.rig.lock (concurrent runs die loudly).
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

# exclusive rig lock (iter 39, review H3): fixed shared artifact paths
# mean two concurrent runs could judge each other's pulls. mkdir is the
# atomic primitive here (flock(1) does not exist on macOS hosts); die
# loudly when held by a LIVE pid, reclaim only a provably dead holder's
# leftover.
LOCK=$DEVB/.rig.lock
if ! mkdir "$LOCK" 2>/dev/null; then
  otherpid="$(cat "$LOCK/pid" 2>/dev/null || true)"
  if [ -n "$otherpid" ] && kill -0 "$otherpid" 2>/dev/null; then
    echo "DEVICE FAIL: another device-rig run holds $LOCK (pid $otherpid)" >&2
    exit 1
  fi
  echo "   reclaiming stale device-rig lock (dead holder pid ${otherpid:-unknown})"
  rm -rf "$LOCK"
  mkdir "$LOCK" || { echo "DEVICE FAIL: cannot acquire $LOCK" >&2; exit 1; }
fi
echo "$$" > "$LOCK/pid"

source port/sim/device/adbsh.sh

# hygiene: device scratch never outlives the script. Best-effort BY
# DESIGN (cleanup must never mask the run's real exit code) but routed
# through dsh and VISIBLE when it fails (iter 39, review L2).
rig_cleanup() {
  dsh "rm -rf $DTMP $DSD" >/dev/null 2>&1 \
    || echo "WARN: device scratch cleanup failed — $DTMP $DSD may remain on the device" >&2
  rm -rf "$LOCK" 2>/dev/null || true
}
trap rig_cleanup EXIT

require_device

# device-side hash tool self-test (iter 39, review M2): every pull below
# is digest-verified via busybox sha256sum on the device — prove the tool
# exists and is correct (empty-input vector) before trusting it.
EMPTY_SHA=e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855
devsha="$(dsh "printf '' | sha256sum" | awk 'NF{print $1; exit}')"
if [ "$devsha" != "$EMPTY_SHA" ]; then
  echo "DEVICE FAIL: device sha256sum missing/broken (got '$devsha')" >&2
  exit 1
fi

# pullv <device-path> <host-dest> — freshness-proven pull (iter 39,
# review M2): the host destination is removed BEFORE the pull (a stale
# file can never be judged), and the device-side sha256 of the source
# must equal the host sha256 of the pulled bytes.
pullv() {
  local dsum hsum
  rm -f "$2"
  adb -s "$DEV" pull "$1" "$2" >/dev/null
  dsum="$(dsh "sha256sum $1" | awk 'NF{print $1; exit}')"
  if [ -z "$dsum" ] || [ "${#dsum}" -ne 64 ]; then
    echo "DEVICE FAIL: no device-side sha256 for $1 (got '$dsum')" >&2
    return 1
  fi
  case "$dsum" in
    *[!0-9a-f]*)
      echo "DEVICE FAIL: bad device-side sha256 for $1 ('$dsum')" >&2
      return 1
      ;;
  esac
  hsum="$(shasum -a 256 "$2" | cut -d' ' -f1)"
  if [ "$dsum" != "$hsum" ]; then
    echo "DEVICE FAIL: pulled $2 != device $1 (device $dsum, host $hsum)" >&2
    return 1
  fi
}

echo "== [1/7] host data plane (M1 tables + SIMDATA1 + g01 trace text) =="
# g01 match params — SINGLE SOURCE: the goldens manifest. Extracted HERE,
# before any device work, with explicit failure checks (iter 39, review
# M4: eval "$(node …)" swallowed a node failure; set -u alone can't catch
# inherited/partial values, so unset first).
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
eval "$gparams"
: "$name" "$seed" "$p1" "$p2" "$stage" "$frames" # set -u backstop: all six present
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
# review M3: the sweep must have consumed the WHOLE corpus (host leg;
# the device leg is asserted in step [5] after the pull)
corpuslines="$(wc -l < "$DEVB/fdlibm-inputs.txt" | tr -d ' ')"
tail -1 "$DEVB/mathsweep-host.txt" | grep -qx "n $corpuslines" || {
  echo "DEVICE FAIL: host mathsweep trailer != corpus line count ($corpuslines)" >&2
  exit 1
}
cc -O2 -ffp-contract=off -Wall -Wextra -Werror \
  -Iport/ryu -Iport/sim -Ioracle/qjs \
  -o "$DEVB/fmt_diff_host" \
  "$CAL/fmt_diff.c" "$CAL/canon.c" port/sim/ml_ser.c port/sim/ml_fmt.c \
  oracle/qjs/sha256.c -lm
"$DEVB/fmt_diff_host" --gen "$DEVB/fmt-adv.hex"
node "$CAL/check-format-pins.js" adversarial "$DEVB/fmt-adv.hex"
"$DEVB/fmt_diff_host" --format "$DEVB/fmt-adv.hex" "$DEVB/fmt-adv.host.txt"

echo "== [3/7] armv7 static cross-build (SDK gcc; stamp-cached) =="
# Stamp inputs (iter 39, review H2): sources + generated tables + THIS
# SCRIPT'S OWN BYTES (the build recipe/flags live in the heredoc below)
# + adbsh.sh + the docker image Id. The stamp also records each produced
# binary's sha256; the cache-HIT path re-hashes the cached binaries and
# any mismatch forces a rebuild — a stamped-but-tampered binary is never
# judged.
ARMIMG=jondbell/funkey-s-sdk
ARMIMGID="$(docker image inspect -f '{{.Id}}' "$ARMIMG" 2>/dev/null)" || {
  echo "   docker image $ARMIMG not local — pulling"
  docker pull "$ARMIMG" >/dev/null
  ARMIMGID="$(docker image inspect -f '{{.Id}}' "$ARMIMG")"
}
ARMBINS="sim_device csweep_arm fmt_diff_arm mathsweep_arm"
srchash() {
  # review M1: this runs inside $() where errexit is cleared — pipefail +
  # explicit stage checks + a minimum-file-count floor, so a failed find
  # can never yield a partial-but-plausible hash.
  set -o pipefail
  local files n
  files="$(find port/sim port/fdlibm port/ryu oracle/qjs "$FDC/csweep.c" \
    -type f \( -name '*.c' -o -name '*.h' \) | LC_ALL=C sort)" || {
    echo "DEVICE FAIL: srchash: find failed" >&2
    return 1
  }
  n="$(printf '%s\n' "$files" | grep -c .)"
  if [ "$n" -lt 450 ]; then
    echo "DEVICE FAIL: srchash: only $n source files found (>= 450 expected)" >&2
    return 1
  fi
  {
    printf '%s\n' "$files" | tr '\n' '\0' | xargs -0 shasum -a 256 || exit 1
    shasum -a 256 "$TABLES/ml_tables.c" "$TABLES/ml_stages.c" \
      port/sim/device/check-device-g01.sh port/sim/device/adbsh.sh || exit 1
    printf 'dockerimage %s\n' "$ARMIMGID"
  } | shasum -a 256 | cut -d' ' -f1
}
STAMP=$DEVB/arm-build.stamp
want="$(srchash)"
stamp_ok() {
  [ -f "$STAMP" ] || return 1
  [ "$(sed -n '1s/^srchash=//p' "$STAMP")" = "$want" ] || return 1
  local f rec cur
  for f in $ARMBINS; do
    [ -f "$DEVB/$f" ] || return 1
    rec="$(awk -v f="$f" '$1=="bin" && $2==f {print $3}' "$STAMP")"
    [ -n "$rec" ] || return 1
    cur="$(shasum -a 256 "$DEVB/$f" | cut -d' ' -f1)"
    if [ "$cur" != "$rec" ]; then
      echo "   cached $f sha256 != stamp record — forcing rebuild" >&2
      return 1
    fi
  done
  return 0
}
if [ "${MLFK_FORCE_ARM:-0}" != 0 ] || ! stamp_ok; then
  rm -f "$STAMP"
  echo "   stamp MISS (or forced) — rebuilding armv7 binaries"
  # SERIAL docker only (CLAUDE.md §Commands arm32 recipe)
  docker run --rm -v "$PWD":/work -w /work "$ARMIMG" bash -lc '
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
  for f in $ARMBINS; do
    file "$DEVB/$f" | grep -q "ELF 32-bit LSB executable, ARM" || {
      echo "DEVICE FAIL: $f is not an armv7 static executable" >&2
      exit 1
    }
  done
  # H4 residue (iter 39): the fdlibm strong overrides must BE the linked
  # definitions in the device sim — exactly one T symbol each.
  for s in floor ceil fmod; do
    cnt="$(nm "$DEVB/sim_device" | awk -v s="$s" '$2=="T" && $3==s' | grep -c . || true)"
    if [ "$cnt" != 1 ]; then
      echo "DEVICE FAIL: expected exactly 1 T definition of $s in sim_device, found $cnt" >&2
      exit 1
    fi
  done
  {
    printf 'srchash=%s\n' "$want"
    for f in $ARMBINS; do
      printf 'bin %s %s\n' "$f" "$(shasum -a 256 "$DEVB/$f" | cut -d' ' -f1)"
    done
  } > "$STAMP"
  echo "   arm binaries rebuilt (stamp $want)"
else
  echo "   arm binaries up to date (stamp $want; cached binaries sha-verified)"
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
pullv "$DTMP/fdlibm-device.txt" "$DEVB/fdlibm-device.txt"
cmp "$DEVB/fdlibm-host.txt" "$DEVB/fdlibm-device.txt"
echo "   fdlibm sweep: device == host ($(wc -l < "$DEVB/fdlibm-device.txt" | tr -d ' ') lines)"
dsh "rm -f $DTMP/fdlibm-device.txt"

echo "== [5/7] device: exact-math family sweep (floor/ceil/sqrt/fabs/fmod/js_round) =="
dsh "sh -lc '$DTMP/mathsweep_arm $DTMP/fdlibm-inputs.txt > $DSD/mathsweep-device.txt'"
pullv "$DSD/mathsweep-device.txt" "$DEVB/mathsweep-device.txt"
# review M3, device leg: the sweep must have consumed the WHOLE corpus
tail -1 "$DEVB/mathsweep-device.txt" | grep -qx "n $corpuslines" || {
  echo "DEVICE FAIL: device mathsweep trailer != corpus line count ($corpuslines)" >&2
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
node "$SIM/wrap-run.js" g01 "$DEVB/g01.sim-out.device.txt" \
  "$DEVB/g01.sim-run.device.json"
node oracle/harness/verify-stream.js "$DEVB/g01.sim-run.device.json" \
  "oracle/goldens/$name.sha256.json"
echo "   device sim wall clock: $((t1-t0)) s for $frames frames (informational)"
dsh "rm -rf $DTMP"

# no-commit guard: build output is never tracked (iter 39, review L1:
# a git ERROR must be loud, never read as clean output)
gitout="$(git status --porcelain -- "$BUILD" "$TABLES")" || {
  echo "DEVICE FAIL: git status failed — cannot prove build output is untracked" >&2
  exit 1
}
if [ -n "$gitout" ]; then
  echo "DEVICE FAIL: build output not gitignored:" >&2
  printf '%s\n' "$gitout" >&2
  exit 1
fi

echo "DEVICE CONFORMS g01"
