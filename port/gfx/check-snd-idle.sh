#!/usr/bin/env bash
# port/gfx/check-snd-idle.sh — A28 idle-silence gate (HOST ONLY; no
# device, no riglib, no ADB).
#
# Proves that the audio fill the SDL callback delegates to
# (platform_audio_sdl.h:103 -> snd_mix_fill) INITIALISES every sample
# frame of the driver buffer when nothing is playing, so an idle app
# emits digital silence rather than replaying stale buffer contents.
# See port/gfx/snd_idle_check.c for the defect shape and why the
# existing `underruns == 0` bars are structurally blind to it.
#
#   bash port/gfx/check-snd-idle.sh   ->  SND IDLE SILENT ... / exit 0
set -eu

cd "$(dirname "$0")/../.."
GFX=port/gfx
OUT=port/gfx/build/snd-idle
mkdir -p "$OUT"

fail() { echo "FAIL: $*" >&2; exit 1; }

echo "== [1/3] build (same flags as every other sim/audio TU) =="
rm -f "$OUT/snd_idle_check"
cc -O2 -ffp-contract=off -Wall -Wextra -Werror \
  -o "$OUT/snd_idle_check" "$GFX/snd_idle_check.c" \
  || fail "build failed"
[ -x "$OUT/snd_idle_check" ] || fail "no binary produced"

echo "== [2/3] tooth: a stale (unwritten) buffer must be REJECTED =="
# --tooth-stale skips the fill, leaving the poisoned buffer intact — the
# exact hypothesis-1 defect. A judgment that cannot fail proves nothing.
if "$OUT/snd_idle_check" --tooth-stale > "$OUT/tooth.log" 2>&1; then
  fail "tooth PASSED — the check cannot detect a stale driver buffer"
fi
grep -q '^FAIL: ' "$OUT/tooth.log" \
  || fail "tooth failed for the wrong reason (see $OUT/tooth.log)"
echo "   tooth rejected as expected: $(head -1 "$OUT/tooth.log" | cut -c1-72)..."

echo "== [3/3] idle path emits bit-exact silence =="
"$OUT/snd_idle_check" > "$OUT/verdict.txt" 2>&1 \
  || { cat "$OUT/verdict.txt" >&2; fail "idle path is NOT silent"; }
grep -Eq '^SND IDLE SILENT cases=[0-9]+ frames=[0-9]+$' "$OUT/verdict.txt" \
  || fail "verdict grammar mismatch: $(cat "$OUT/verdict.txt")"

cat "$OUT/verdict.txt"
