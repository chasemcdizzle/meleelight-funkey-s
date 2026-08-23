#!/usr/bin/env bash
# A37 POSITIVE WITNESS — upstream's versusMode (main.js:140/237) is a real,
# settable, SIM-VISIBLE binary mode, not a switch wired to nothing.
#
# check-sim.sh proves the NEGATIVE half (flag OFF => all 8 goldens
# bit-identical). This proves the POSITIVE half: flag ON actually changes
# the simulation, at all three sites the mode touches.
#
#   [A] startGame's stocks arm (main.js:1334-1336, port/sim/sim/sim_boot.c):
#       versusMode => every slot starts on 1 stock. Judged on the frame-1
#       envelope: 4,4 without the flag, 1,1 with it.
#   [B] the endless-respawn arm (physics.js:980, port/sim/physics.c:1308)
#       + isFinalDeath (actionStateShortcuts.js:155,
#       port/sim/action_state_shortcuts.c:147): a KO'd player on 0 stocks is
#       put back on 1 and the match stays live. Judged differentially over
#       29,000 frames of the g01 trace: WITHOUT the flag the sim dies at
#       frame 4749 on final death; WITH it the same trace runs to the end.
#   [C] the matchTimer arm (main.js:1079, port/sim/sim/sim_tick.c): in the
#       endless mode upstream takes the startTimer ELSE branch forever, so
#       matchTimer never ticks and never expires. The same 29,000-frame run
#       carries this: 480s / 0.016667 expires at frame 28890, INSIDE the run.
#       TOOTH (measured 2026-08-23, reverse-edit): dropping the
#       `versusMode == 0` conjunct from sim_tick.c and rebuilding makes the
#       endless run die with
#         SIM FATAL frame 28890: matchTimer expired (finishGame)
#       so the guard is load-bearing, not decorative.
#
# Prints VERSUS ENDLESS OK, exit 0; any shortfall -> nonzero.
#
# This check CONSUMES check-sim.sh's build products (sim_host, simdata.txt,
# g01.trace.txt) rather than duplicating its TU list — so run
# `bash port/sim/check-sim.sh` first. check-sim.sh's bytes are PINNED below
# (the check-ai-live.sh producer-pin pattern): it is never edited, and a
# reviewed change to it must update this pin in the same commit.
set -euo pipefail
cd "$(dirname "$0")/../.."

BUILD=port/sim/calib/build
SIM=$BUILD/sim_host
fail() { echo "VERSUS ENDLESS FAIL: $*" >&2; exit 1; }

# --- [0] producer byte pin ----------------------------------------------------
PIN=ce0882bee2a0bb0ad11ac51366ef467c3811d832f9dc932c4eb10dd3ccc4c8cb
have=$(shasum -a 256 port/sim/check-sim.sh | cut -d' ' -f1)
[ "$have" = "$PIN" ] || fail "check-sim.sh sha256 $have != pinned $PIN"

for f in "$SIM" "$BUILD/simdata.txt" "$BUILD/g01.trace.txt"; do
  test -f "$f" || fail "missing $f — run 'bash port/sim/check-sim.sh' first"
done

# g01: fox vs marth on battlefield, seed 1337 (oracle/goldens/manifest.json)
g01() { # $1 = extra flags, $2 = frames, $3 = --dump-frames arg or ""
  local dump=()
  if [ -n "$3" ]; then dump=(--dump-frames "$3"); fi
  $SIM --trace "$BUILD/g01.trace.txt" --simdata "$BUILD/simdata.txt" \
    --seed 1337 --p1 2 --p2 0 --stage 0 --frames "$2" \
    ${dump[@]+"${dump[@]}"} $1
}

# --- [A] startGame's stocks arm ------------------------------------------------
stocks_at_frame1() { # $1 = extra flags -> "N N" for the two active slots
  g01 "$1" 3 1 2>&1 >/dev/null |
    sed -n 's/^E 1	//p' |
    grep -o '"stocks":-\?[0-9]*' | cut -d: -f2 | tr '\n' ' ' | sed 's/ $//'
}
base1=$(stocks_at_frame1 "")
end1=$(stocks_at_frame1 --versus-endless)
[ "$base1" = "4 4" ] || fail "[A] versusMode 0 frame-1 stocks '$base1' != '4 4'"
[ "$end1" = "1 1" ] || fail "[A] versusMode 1 frame-1 stocks '$end1' != '1 1'"
echo "[A] startGame stocks arm: versusMode 0 -> $base1, versusMode 1 -> $end1"

# --- [B]+[C] the 29,000-frame differential ------------------------------------
# 29000 > 4749 (g01's final death) and > 28890 (matchTimer expiry).
rc=0; g01 "" 29000 "" >/dev/null 2>"$BUILD/versus-base.err" || rc=$?
[ "$rc" = 3 ] || fail "[B] versusMode 0 exited $rc, expected 3 (final death)"
grep -q 'SIM FATAL frame 4749: DEADDOWN: finishGame (final death)' \
  "$BUILD/versus-base.err" ||
  fail "[B] versusMode 0 did not die on final death at frame 4749: $(cat "$BUILD/versus-base.err")"
echo "[B] versusMode 0: $(tail -1 "$BUILD/versus-base.err")"

rc=0; g01 --versus-endless 29000 "" >"$BUILD/versus-endless.out" \
  2>"$BUILD/versus-endless.err" || rc=$?
[ "$rc" = 0 ] || fail "[B/C] versusMode 1 exited $rc: $(cat "$BUILD/versus-endless.err")"
tail -1 "$BUILD/versus-endless.out" | grep -q '^SIM OK$' ||
  fail "[B/C] versusMode 1 did not finish cleanly"
n=$(grep -c '^F ' "$BUILD/versus-endless.out")
[ "$n" = 29000 ] || fail "[B/C] versusMode 1 emitted $n frames, expected 29000"
echo "[B] versusMode 1: survived frame 4749 (0-stock KOs respawn to 1)"
echo "[C] versusMode 1: survived frame 28890 (matchTimer never ticks)"

echo "VERSUS ENDLESS OK"
