#!/usr/bin/env bash
# port/gfx/check-snd-playid.sh — A40 play-id agreement gate (HOST ONLY;
# no device, no riglib, no ADB — the check-snd-idle.sh model).
#
# The owner-visible claim: a sound the SIM started (marth's looping
# shieldbreakercharge, puff's furaloop) must still STOP when the sim
# stops it by play id, no matter how many MENU sounds were played first.
# It did not, because the mixer minted a play id on every snd_event()
# while ml_events.c minted one only on ml_sound_play(), and the menu
# chokepoint called snd_event() directly (A40 — full diagnosis in the
# snd_mixer.h header note and fix_plan.md).
#
# port/gfx/snd_playid_check.c drives the REAL ml_events.c through
# foh_dev.c's exact wiring and asserts the OUTCOME: stopsUnmatched stays
# 0 AND the mixer goes bit-exactly silent. Two teeth, one per assertion.
#
#   bash port/gfx/check-snd-playid.sh   ->  SND PLAYID OK ... / exit 0
set -eu

cd "$(dirname "$0")/../.."
GFX=port/gfx
OUT=port/gfx/build/snd-playid
mkdir -p "$OUT"

fail() { echo "FAIL: $*" >&2; exit 1; }

echo "== [1/4] build (same flags as every other sim/audio TU) =="
rm -f "$OUT/snd_playid_check"
cc -O2 -ffp-contract=off -Wall -Wextra -Werror -I"$GFX" -Iport/sim \
  -o "$OUT/snd_playid_check" "$GFX/snd_playid_check.c" port/sim/ml_events.c \
  || fail "build failed"
[ -x "$OUT/snd_playid_check" ] || fail "no binary produced"

# The menu-plane call site this whole ticket turns on. Asserted here so a
# future edit that points foh_snd back at snd_event() cannot pass by
# leaving the checker's own copy of the wiring correct.
grep -q 'snd_event_menu(&g_mix, name);' port/foh/foh_dev.c \
  || fail "port/foh/foh_dev.c's foh_snd no longer calls snd_event_menu — the "\
"menu plane is minting sim play ids again (A40)"

echo "== [2/4] tooth: the pre-fix wiring must be REJECTED =="
# --tooth-legacy puts the menu plane back on snd_event(), i.e. the defect.
if "$OUT/snd_playid_check" "$OUT/pack.snd" --tooth-legacy > "$OUT/t-legacy.log" 2>&1
then
  fail "tooth PASSED — the check cannot detect a drifting play-id counter"
fi
grep -q 'id-routed stop matched NO voice' "$OUT/t-legacy.log" \
  || fail "legacy tooth failed for the wrong reason (see $OUT/t-legacy.log)"
grep -q 'menuPlays=0' "$OUT/t-legacy.log" \
  || fail "legacy tooth did not reach the menuPlays=0 case, so it does not "\
"show that the MENU PLAY is what breaks routing (see $OUT/t-legacy.log)"
echo "   rejected: $(grep -m1 'FAIL:' "$OUT/t-legacy.log" | cut -c1-76)..."

echo "== [3/4] tooth: a stop that bookkeeps clean and silences nothing =="
# --tooth-deaf reports a MATCHED stop and leaves the voice sounding: only
# the audible assertion can catch it.
if "$OUT/snd_playid_check" "$OUT/pack.snd" --tooth-deaf > "$OUT/t-deaf.log" 2>&1
then
  fail "tooth PASSED — the check judges only counters, not whether the "\
"sound actually stopped"
fi
grep -q 'THE SOUND IS STILL PLAYING' "$OUT/t-deaf.log" \
  || fail "deaf tooth failed for the wrong reason (see $OUT/t-deaf.log)"
if grep -q 'id-routed stop matched NO voice' "$OUT/t-deaf.log"; then
  fail "the two teeth fail on the SAME assertion — one of them is not "\
"covering what it claims to"
fi
echo "   rejected: $(grep -m1 'FAIL:' "$OUT/t-deaf.log" | cut -c1-76)..."

echo "== [4/4] sim-started voices stop after N menu sounds =="
"$OUT/snd_playid_check" "$OUT/pack.snd" > "$OUT/verdict.txt" 2>&1 \
  || { cat "$OUT/verdict.txt" >&2; fail "a sim-started sound did not stop"; }
grep -Eq '^SND PLAYID OK cases=[0-9]+ menuplays=[0-9]+ stops=[0-9]+$' \
  "$OUT/verdict.txt" \
  || fail "verdict grammar mismatch: $(tail -1 "$OUT/verdict.txt")"
if grep -Eq '^SND PLAYID OK cases=[0-9]+ menuplays=0 ' "$OUT/verdict.txt"; then
  fail "no menu sounds were played — the run cannot have exercised the "\
"drift it claims to rule out"
fi

cat "$OUT/verdict.txt"
