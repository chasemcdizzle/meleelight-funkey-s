#!/usr/bin/env bash
# port/gfx/check-alsa-headroom.sh — A28 buffer-headroom gate (HOST ONLY;
# no device, no riglib, no ADB).
#
# Proves that port/gfx/judge-alsa-headroom.js actually DETECTS audio
# starvation from the kernel's own number (`avail_max` out of
# /proc/asound/card0/pcm0p/sub0/status), so the A28 buzz can never again
# be a question only an ear can answer. Modelled on check-snd-idle.sh:
# every case is a real run of the real judge, and the teeth come first.
#
# The two "MEASURED" fixtures below are the numbers pulled off the
# FunKey-S on 2026-08-24 (fix_plan A28) — the shipped 512-sample config
# the owner heard buzz, and the 2048-sample config he confirmed clean.
# The judge must reject the first and accept the second; that is the
# whole claim, and it is checked against real device bytes.
#
# DEVICE LEG (later, same judge, same bar): while a match runs, `cat`
# /proc/asound/card0/pcm0p/sub0/hw_params and .../status off the device
# into two files and hand them to judge-alsa-headroom.js on the host.
# Nothing in the judge is host- or fixture-specific.
#
#   bash port/gfx/check-alsa-headroom.sh  ->  ALSA HEADROOM CHECK OK ... / exit 0
set -eu

cd "$(dirname "$0")/../.."
J=port/gfx/judge-alsa-headroom.js
OUT=port/gfx/build/alsa-headroom
rm -rf "$OUT"; mkdir -p "$OUT"

fail() { echo "FAIL: $*" >&2; exit 1; }
cases=0

# hw <name> <period> <buffer> [rate]
hw() {
  local n=$1 p=$2 b=$3 r=${4:-44100}
  cat > "$OUT/$n.hw" <<EOF
access: RW_INTERLEAVED
format: S16_LE
subformat: STD
channels: 2
rate: $r ($r/1)
period_size: $p
buffer_size: $b
EOF
}
# st <name> <avail_max>
st() {
  local n=$1 a=$2
  cat > "$OUT/$n.st" <<EOF
state: RUNNING
owner_pid   : 421
trigger_time: 1234.567890123
tstamp      : 0.000000000
delay       : 1024
avail       : 512
avail_max   : $a
-----
hw_ptr      : 1234567
appl_ptr    : 1235591
EOF
}

# expect_reject <label> <name> <substring-of-reason>
expect_reject() {
  local label=$1 n=$2 want=$3
  if node "$J" "$OUT/$n.hw" "$OUT/$n.st" > "$OUT/$n.log" 2>&1; then
    cat "$OUT/$n.log" >&2
    fail "$label was ACCEPTED — the judge cannot see it"
  fi
  grep -q '^FAIL: ' "$OUT/$n.log" || fail "$label failed for the wrong reason"
  grep -qF "$want" "$OUT/$n.log" \
    || fail "$label rejected for an unexpected reason: $(head -1 "$OUT/$n.log")"
  cases=$((cases + 1))
  echo "   reject OK: $label"
}
# expect_accept <label> <name>
expect_accept() {
  local label=$1 n=$2
  node "$J" "$OUT/$n.hw" "$OUT/$n.st" > "$OUT/$n.log" 2>&1 \
    || { cat "$OUT/$n.log" >&2; fail "$label was REJECTED"; }
  grep -Eq '^ALSA HEADROOM OK rate=[0-9]+ period=[0-9]+ buffer=[0-9]+ availMax=[0-9]+ inHand=[0-9]+ [0-9]+\.[0-9][0-9]ms$' \
    "$OUT/$n.log" || fail "$label verdict grammar mismatch: $(cat "$OUT/$n.log")"
  cases=$((cases + 1))
  echo "   accept OK: $label -> $(cat "$OUT/$n.log")"
}

echo "== [1/4] MEASURED device fixtures (2026-08-24, fix_plan A28) =="
# The shipped config. 512 frames = 11.61 ms — shorter than one 16.67 ms
# frame — and avail_max 768 of 1024 left only 256 frames (5.80 ms) in
# hand at worst. This is the config that buzzed.
hw dev512 512 1024; st dev512 768
expect_reject "MEASURED 512/1024 avail_max 768 (the buzz)" dev512 "SHORTER THAN ONE"
# The config the owner confirmed clean by ear.
hw dev2048 2048 4096; st dev2048 2096
expect_accept "MEASURED 2048/4096 avail_max 2096 (clean)" dev2048

echo "== [2/4] the bar itself: one frame of audio in hand, either side =="
# Period is fine (2048), but the ring ran down to exactly one frame:
# 44100/60 = 735 frames. The bar is STRICTLY greater, so 735 fails...
hw edgeLo 2048 4096; st edgeLo $((4096 - 735))
expect_reject "in hand == exactly one frame (735)" edgeLo "which is starvation"
# ...and one frame more passes. Off-by-one in the comparison is caught.
hw edgeHi 2048 4096; st edgeHi $((4096 - 736))
expect_accept "in hand == one frame + 1 (736)" edgeHi
# A healthy ring with a period that is still under a frame must STILL
# fail: the deadline is a defect even on a run that got lucky.
hw luckyShort 1024 32768 88200; st luckyShort 0   # 1024/88200 = 11.61 ms
expect_reject "lucky run, period still under a frame" luckyShort "SHORTER THAN ONE"

echo "== [3/4] evidence that is not evidence must be refused, not parsed =="
printf 'closed\n' > "$OUT/closed.hw"; st closed 0
expect_reject "hw_params reads 'closed'" closed "was not open"
hw closed2 2048 4096; printf 'closed\n' > "$OUT/closed2.st"
expect_reject "status reads 'closed'" closed2 "was not open"
hw noavail 2048 4096; st noavail 100
grep -v avail_max "$OUT/noavail.st" > "$OUT/noavail.st.tmp"
mv "$OUT/noavail.st.tmp" "$OUT/noavail.st"
expect_reject "status with no avail_max field" noavail "no \`avail_max\` field"
hw garbled 2048 4096; st garbled 100
sed 's/^avail_max   : 100/avail_max   : ?/' "$OUT/garbled.st" > "$OUT/g.tmp"
mv "$OUT/g.tmp" "$OUT/garbled.st"
expect_reject "non-integer avail_max" garbled "not an integer"
hw mismatch 2048 4096; st mismatch 9999
expect_reject "avail_max larger than the ring" mismatch "exceeds buffer_size"
hw absurd 4096 2048; st absurd 0
expect_reject "buffer smaller than one period" absurd "cannot hold one period"

echo "== [4/4] the value the code actually ships =="
# platform.h's PLATFORM_AUDIO_SAMPLES_DEFAULT is the requested PERIOD,
# and platform_audio_sdl.h refuses any renegotiation, so the period the
# device reports must be that number. Assert the constant clears the bar
# on its own — a future edit that lowers it below one frame fails HERE,
# on the host, without waiting for a device or an ear.
DEF=$(sed -n 's/^#define PLATFORM_AUDIO_SAMPLES_DEFAULT \([0-9]*\)$/\1/p' \
      port/gfx/platform.h)
[ -n "$DEF" ] || fail "PLATFORM_AUDIO_SAMPLES_DEFAULT not found in port/gfx/platform.h"
# SDL 1.2's ALSA backend takes buffer = 2 * period; avail_max 0 isolates
# the period bar from any runtime luck.
hw shipped "$DEF" $((DEF * 2)); st shipped 0
expect_accept "shipped default period=$DEF" shipped

echo "ALSA HEADROOM CHECK OK cases=$cases default=$DEF"
