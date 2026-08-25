#!/usr/bin/env bash
# check-device-target.sh — M4 task 12 done-check: TARGET TEST on device
# (fix_plan §M4 task 12; pre-registration AGENT-LOG iter 99).
#
# THE CLAIM: the target-test SELECT FLOW + the target launch bridge run
# ON the FunKey-S through the REAL input path (fk_input -> uinput -> SDL
# keysyms -> platform_poll, `--input poll` pinned — the M3 binding), and
# EVERY target golden (t01, t02 — port/goldens-m4/manifest-target.json)
# is replayed ON DEVICE with LIVE render (the gfx_target compositor +
# the timer HUD) + SFX + the targettest MUSIC track streaming from SD,
# with BOTH streams pulled and judged ON THE HOST by BOTH verifiers:
# the UNCHANGED oracle/harness/verify-stream.js (player plane) +
# port/goldens-m4/verify-target-stream.js (target plane); per leg
# p99 < 16.67 ms, skips == 0, presentFails == 0, underruns == 0,
# badlen == 0, music starves == 0.
#
# Composition (the check-device-foh.sh conventions inherited whole):
#  [0] startup normalization + inherited-state ownership (riglib
#      chokepoints; qd-normalize; sha tool self-test);
#  [1] producer byte pins (12) + twin pins (judge-foh-trace sha ==
#      check-foh-flows.sh's; sndpack + menu.pcm == the sibling device
#      checks' literals) + the SHARED strict target-manifest validator
#      + committed-generator trace regen guard + the flow inventory
#      (paired with check-foh-flows.sh's 7-flow pin) + fresh data
#      planes (tables incl. the TTAB1 targets stage, anim bins,
#      simdata, golden traces);
#  [2] audio build: fresh pipeline audio stage, sndpack x2 byte-stable
#      + pinned, menu.pcm twin-pinned, targettest.pcm NEW
#      measured-then-frozen pin + its sounds.json sprite/volume pins;
#  [3] host TWIN legs (foh_dev_headless, --input flow, pace 0): traces
#      cmp'd against the frozen flows/*.expect, TBRIDGE-STATE cmp'd,
#      f06 x2 byte-stability, twin shots as byte-exact device
#      references, twin streams judged by BOTH verifiers (whole-log
#      byte-exact constructed verdicts), keymap SSOT dump == frozen;
#  [4] fk scripts derived x2 byte-stable;
#  [5] shared arm build (riglib stamp; the recipe owns the TTAB1
#      input) + push + sha provenance for every pushed byte;
#  [6] DEVICE legs f06/f07 under the low_bat_check quiesce bracket +
#      deadman/park: trace judged by judge-foh-trace.js + NORMALIZED
#      byte-equality + the BOUNDED-DELTA cadence judge vs the frozen
#      .expect; shots byte-exact vs the twins + fb-witness (FBWIT1
#      re-judged); TBRIDGE-STATE cmp; BOTH streams wrap-target'd and
#      judged by BOTH verifiers; timing p99 < 16.67 ms via
#      judge-render-timing.js (ordered whitelist), skips == 0; foh/
#      match/audio/music summaries strict-parsed (voice starts == the
#      FROZEN per-flow pins == the twins' — iter 101, review-99 M3;
#      stops == the twins'); music evidence is TRACK-IDENTIFIED
#      (iter 101, review-99 M2): every leg (twins incl. the f06 b twin
#      + device) must carry the exact `foh_dev mustrack:` pair — boot
#      menu program + the menu->targettest switch at the TLAUNCH seam,
#      each naming the pcm path whose bytes this check sha-pins;
#      aggregate music counters gate liveness only, never identity;
#      the `foh_dev tfinish:` line ABSENT on ALL legs incl. the f06 b
#      twin, and ANY 'tfinish' substring resemblance = death
#      (iter 101, review-99 L2; no committed flow can reach the finish
#      seam — AGENT-LOG iter-99 refutation; the finish probe owns
#      mechanical coverage in check-target-sim.sh);
#  [7] teeth (COPIES only, committed bytes never edited): T1 A/B-swap
#      flow variant dies at the normalized judge; T2 device T-line
#      nibble -> verify-target-stream dies; T3 TFIN perturb -> the
#      finals pin dies with the SEMANTIC divergence class (rc 2) + the
#      exact named finals-pin diagnostic (iter 101, review-99 L3);
#      T4 corrupted device-shot copy -> the shot judge
#      dies; T5 TBRIDGE-STATE perturb -> the frozen cmp dies; T6
#      fb-witness eq=0 copy -> the witness judge dies;
#  [8] park restore + deadman cancel + no-commit guard.
#
# Prints `DEVICE TARGET CONFORMS (...)`, exit 0; ANY divergence, pin
# mismatch, grammar violation, perf/audio shortfall, or missing
# artifact -> nonzero.
set -euo pipefail
# ascii_text_ok <file> — returns 0 iff the file contains NO control byte other
# than TAB and LF. STATUS-HONEST BY CONSTRUCTION (review-119-delta-5b [H2]):
# every measurement is status-checked and an unmeasurable file returns 1
# (CORRUPT), because the previous shape — `[ "$(a|b)" = "$(c)" ]` — discarded
# both pipelines' statuses inside `[ ]`, where errexit does not apply, so a
# failed read made BOTH sides empty and the guard passed VACUOUSLY.
# WHY THIS CLASS AT ALL (review-118-delta-4 [M], review-119-delta-5b [H2]):
# bash `read -r` STOPS AT NUL, so `full_p99_ns=1<NUL>20000000` is parsed as
# `full_p99_ns=1` and a 120 ms p99 is judged as 1 ns; likewise a witness row
# `... eq=1<NUL> eq=0 CORRUPT` is parsed as a passing row. Line counts and
# trailing-newline checks agree with the lie, so no line-level guard can see
# it. Every reader of a text evidence file must refuse the class up front.
# MEASURED SAFE: the FBWIT1 and judge-render-timing.js emitters are pure
# ASCII by construction (fixed format strings, decimal numbers, fixed-table
# names), so a genuine file can never contain one.
ascii_text_ok() {
  local f="$1" nraw nclean rc
  nraw="$(wc -c < "$f")" && rc=0 || rc=$?
  [ "$rc" = 0 ] || return 1
  nclean="$(LC_ALL=C tr -d '\000-\010\013-\037\177' < "$f" | wc -c)" && rc=0 || rc=$?
  [ "$rc" = 0 ] || return 1
  nraw="${nraw// /}"; nclean="${nclean// /}"
  [ -n "$nraw" ] && [ -n "$nclean" ] || return 1
  [ "$nraw" = "$nclean" ] || return 1
  return 0
}

cd "$(dirname "$0")/../../.."

GFX=port/gfx
FOH=port/foh
FLOWS=$FOH/flows
SIM=port/sim/sim
CAL=port/sim/calib
TGT=port/sim/target
M4G=port/goldens-m4
DEVB=port/sim/calib/build/device
TABLES=pipeline/build/sim-tables
AUDIO_OUT=pipeline/build/audio-target
BUILD=$TGT/build/device-target
FDC=oracle/fdlibm-crosscheck
DTMP=/tmp/mlfk
DSD=/mnt/mlfk-scratch

BUDGET_NS=16666667
P99_FULL_LIMIT_NS=16670000
WALL_MIN_MS=58000
WALL_MAX_MS=66000
QW_PRE_SLACK_S=10
QW_POST_SLACK_S=10
READY_TRIES=30
DEADMAN_S="${MLFK_DEADMAN_S:-900}"
# present-witness envelope pins (== foh_dev.c FBWIT_* — the iter-95
# measured kernel policy; twin values in check-device-foh.sh)
FBWIT_XFORM_PIN=0
FBWIT_LL_PIN=480
FBWIT_VYRES_PIN=720

# canonical decimals (the iter-97 M-e class)
NUM12='(0|[1-9][0-9]{0,11})'
NUM19='(0|[1-9][0-9]{0,18})'

fail() { echo "DEVICE TARGET FAIL: $1" >&2; exit 1; }
grammar_die() { echo "DEVICE TARGET FAIL: $1" >&2; exit 2; }

# --- producer byte pins -------------------------------------------------------
PRODUCER_PINS="\
f420723433b19166b53a80aedf54931ffdfbc6d2505c773fd73b7a13bbcdf60e oracle/harness/verify-stream.js
0bc801ea46b06a63e79377aae164636a5e9f649ee45835748e5f2387b9e04281 oracle/harness/streamlib.js
4160a35b36e8d3d6896ad2c3c6239d4a4860a0d7f43814a7a9b53b7c136742ab port/sim/sim/trace-to-txt.js
7186734f8c3ff9bfad04f59bf9e13f201663e82481e399911433136673721bba port/sim/calib/dump-sim-data.js
594f1925628259bf702f12b21d7991e9be0dcf3d3e9fa0a8de1cca311259b9db port/foh/judge-foh-trace.js
a1353a71a66bb05bc28d547eee9385cfa8da7baf784f9e038bd31834cabb9cb8 port/foh/normalize-foh-trace.js
1163e9c18323ac06aaaec4ee3068691d7d67ebbf98b3500a343a69c80ca793ea port/foh/flow-to-fkscript.js
4b68fba5a804b281a73003b29eac1a0290707f2b6260ee39c900a0262962f421 port/gfx/judge-render-timing.js
2b208cfe18c9e5aac370e0212fc74721489fd404aeb67c9deeddee88ba1bfc1e port/foh/keymap-frozen.txt
2cf5c5a532207372b70c4cee57412c7ac65643ac4f4066c745d9eb7fe4aa0e9b port/goldens-m4/wrap-target.js
415335239fcc04df97eba07298a1fa521602d5ea45b087aa8d7d40bd740c122a port/goldens-m4/verify-target-stream.js
6b1b6b5be3700c51dfae8c0c4cb1f012e5b61239394ae4146c2e5e19cc4fcc47 port/goldens-m4/validate-target-manifest.js
624956898890e749170a4768af0f8ef86e05ce4dd75046d084701747c9d9121f port/goldens-m4/json-dup-key-scan.js"
N_PINS_WANT=13
# audio artifact pins: sndpack + menu.pcm are TWIN-PINNED to the
# reviewed sibling literals (asserted via rig_pin_assert_once below);
# targettest.pcm is NEW here (measured-then-frozen, iter 99).
SNDPACK_SHA256=f69579082fe569249879faa5ceccb7a810d94d8092695ddc8bb543f3bda3ccb4
MUSIC_MENU_SHA256=bbf52720a559ca7b0cf21837a1425a42fd612719442a006b041c913d5f8c4856
MUSIC_TT_SHA256=0c922c6f7111e6d90888c3fe692acc2d03306c858135c1501261b36f216a951d
# targettest track metadata pins (sounds.json music.targettest —
# music.js:102-113 sprite windows; the global 0.3 changeVolume).
MUSIC_TT_VOLBITS=3fd3333333333333
MUSIC_TT_START='0,1'
MUSIC_TT_LOOP='0,224459'
MUSIC_MENU_VOLBITS=3fd3333333333333
MUSIC_MENU_START='0,7425'
MUSIC_MENU_LOOP='7425,173500'
# SFX evidence pins (iter 101, review-99 M3): per-flow voice-START
# counts MEASURED-THEN-FROZEN from the committed cold evidence
# (verdict lines `starts f06-target-t01=15 f07-target-t02=31` in
# .loop/m4-task12-devtarget-run4.log AND
# .loop/m4-task12-driver-cold.log). Twin AND device starts must EQUAL
# these exactly — a deleted snd_push plane (the both-zero
# self-consistency hole) is death by construction. Re-freeze =
# reviewed change in the same commit.
SFX_STARTS_PIN=(15 31)

source port/sim/device/adbsh.sh
require_device
source port/sim/device/riglib.sh
mkdir -p "$BUILD" "$DEVB"
rig_lock_acquire
RIG_PRESERVE_DTMP=1
PARKED=0
DEADMAN_ARMED=0
LBC_STOPPED=0
DM_NONCE=""
cleanup() {
  rig_dsh_retry "pkill foh_device; pkill fk_input; true" \
    || echo "WARN: could not pkill foh_device/fk_input on the device" >&2
  restore_verified=0
  if [ "$LBC_STOPPED" = 1 ]; then
    if rig_daemon_restore low_bat_check /etc/init.d/S12low-bat-check; then
      rig_dsh_retry "rm -f $DTMP/qd.low_bat_check.$DM_NONCE" \
        || echo "WARN: could not remove the quiesce marker (deadman restore arm is idempotent)" >&2
      LBC_STOPPED=0
    else
      echo "WARN: low_bat_check did NOT verify as running after restart — the armed deadman restores it within ${DEADMAN_S}s, or run '/etc/init.d/S12low-bat-check start' manually" >&2
    fi
  fi
  if [ "$PARKED" = 1 ]; then
    rig_dsh_retry "rm -f /mnt/disable_frontend" \
      || echo "WARN: could not remove /mnt/disable_frontend" >&2
    if rig_dsh_retry "test ! -f /mnt/disable_frontend"; then
      PARKED=0
      restore_verified=1
    else
      echo "WARN: could not VERIFY /mnt/disable_frontend gone — the armed deadman removes it within ${DEADMAN_S}s" >&2
    fi
  else
    restore_verified=1
  fi
  if [ "$DEADMAN_ARMED" = 1 ]; then
    if [ "$restore_verified" = 1 ] && [ "$LBC_STOPPED" = 0 ]; then
      rig_dsh_retry "touch $DTMP/deadman.cancel" \
        || echo "WARN: could not cancel the park deadman — it will fire once (idempotent) within ${DEADMAN_S}s" >&2
    else
      echo "WARN: restore unverified — leaving the deadman ARMED as the backstop" >&2
      RIG_PRESERVE_DTMP=1
    fi
  fi
  rig_cleanup
}
trap cleanup EXIT

# --- [0] startup normalization (the check-device-foh chokepoint) --------------
echo "== [0/8] startup normalization + inherited-state ownership =="
rig_qd_normalize
stale_marker=0
stale_deadman=0
nrc=0
dsh "test -f /mnt/disable_frontend" >/dev/null || nrc=$?
case "$nrc" in
  0) stale_marker=1 ;;
  1) : ;;
  *) fail "startup normalization could not probe the frontend marker (rc $nrc)" ;;
esac
nrc=0
dsh "test -e $DTMP/deadman.nonce -o -e $DTMP/deadman.pid" >/dev/null || nrc=$?
case "$nrc" in
  0) stale_deadman=1 ;;
  1) : ;;
  *) fail "startup normalization could not probe for stale deadman state (rc $nrc)" ;;
esac
if [ "$stale_marker" = 1 ] || [ "$stale_deadman" = 1 ]; then
  echo "WARN: stale prior-run state (marker=$stale_marker deadman=$stale_deadman) — normalizing" >&2
  if [ "$stale_marker" = 1 ]; then
    PARKED=1
    dsh "rm -f /mnt/disable_frontend"
    dsh "test ! -f /mnt/disable_frontend"
    PARKED=0
    echo "   stale /mnt/disable_frontend removed (RC-verified gone)"
  fi
  if [ "$stale_deadman" = 1 ]; then
    dsh "mkdir -p $DTMP && touch $DTMP/deadman.cancel"
    sdm_gone=0
    for _ in $(seq 1 6); do
      if dsh "test ! -f $DTMP/deadman.pid" >/dev/null 2>&1; then sdm_gone=1; break; fi
      sleep 2
    done
    if [ "$sdm_gone" != 1 ]; then
      sdm_pid="$(dsh "cat $DTMP/deadman.pid")" || fail "could not read the stale deadman pid file"
      sdm_pid="${sdm_pid%$'\n'}"
      [[ "$sdm_pid" =~ ^(0|[1-9][0-9]{0,6})$ ]] || fail "stale deadman.pid not canonical ('$sdm_pid')"
      nrc=0
      dsh "test -d /proc/$sdm_pid" >/dev/null || nrc=$?
      case "$nrc" in
        0) fail "stale deadman (pid $sdm_pid) STILL RUNNING and ignored its cancel — inspect the device" ;;
        1) echo "   stale deadman.pid orphaned (pid $sdm_pid dead)" ;;
        *) fail "could not probe pid $sdm_pid liveness (rc $nrc)" ;;
      esac
    fi
    dsh "rm -rf $DTMP"
    echo "   stale deadman state wiped ($DTMP)"
  fi
fi
RIG_PRESERVE_DTMP=0
rig_devsha_selftest
echo "   device state clean; sha tool self-tested"

# --- [1] pins + manifest + flows + data planes --------------------------------
echo "== [1/8] pins + target manifest + flows + tables/simdata/traces =="
n_pins=0
while IFS= read -r pline; do
  [ -n "$pline" ] || continue
  if ! [[ "$pline" =~ ^[0-9a-f]{64}\ [A-Za-z0-9._/-]+$ ]]; then
    fail "producer pin table — line fails the anchored grammar: '$pline'"
  fi
  psha="${pline%% *}"
  ppath="${pline#* }"
  test -f "$ppath" || fail "pinned producer $ppath missing from the tree"
  have="$(rig_host_sha256 "$ppath")" || fail "cannot hash producer $ppath"
  [ "$have" = "$psha" ] || fail "producer byte pin — $ppath sha256 $have != pinned $psha (reviewed pin update in the same commit)"
  n_pins=$((n_pins + 1))
done <<< "$PRODUCER_PINS"
[ "$n_pins" = "$N_PINS_WANT" ] || fail "producer pin inventory — $n_pins/$N_PINS_WANT pins verified"
# twin pins: the judge sha must sit in check-foh-flows.sh's pin table
# exactly once; sndpack/menu.pcm must equal the sibling device checks'.
c="$(grep -cF "594f1925628259bf702f12b21d7991e9be0dcf3d3e9fa0a8de1cca311259b9db port/foh/judge-foh-trace.js" "$FOH/check-foh-flows.sh")" || true
[ "$c" = 1 ] || fail "twin pin — check-foh-flows.sh does not carry the same judge-foh-trace.js sha exactly once (count $c; paired change rule)"
c="$(grep -cF "594f1925628259bf702f12b21d7991e9be0dcf3d3e9fa0a8de1cca311259b9db port/foh/judge-foh-trace.js" "$FOH/check-device-foh.sh")" || true
# exactly 2 there: its PRODUCER_PINS row + its own twin grep of
# check-foh-flows.sh (both carry the sha+path pair)
[ "$c" = 2 ] || fail "twin pin — check-device-foh.sh does not carry the same judge-foh-trace.js sha exactly twice (count $c; pin row + its twin grep)"
rig_pin_assert_once "$GFX/check-device-music.sh" SNDPACK_SHA256 "$SNDPACK_SHA256" || exit 1
rig_pin_assert_once "$FOH/check-device-foh.sh" MUSIC_MENU_SHA256 "$MUSIC_MENU_SHA256" || exit 1
echo "   producer pins OK ($N_PINS_WANT) + twin pins (judge sha x2, sndpack, menu.pcm)"

# the SHARED strict manifest validator FIRST (review-94 H1 discipline)
node "$M4G/validate-target-manifest.js" >/dev/null \
  || fail "manifest-target.json failed the shared strict validator"
tline="$(node -e '
  const v = require("./port/goldens-m4/validate-target-manifest");
  const m = v.loadValidatedManifest();
  for (const id of ["t01", "t02"]) {
    const g = v.goldenByIdOrName(m, id);
    console.log([g.id, g.name, g.trace, g.frames, g.seed, g.char,
      g.tstage].join(" "));
  }
')" || fail "cannot pull t01/t02 params from the target manifest"
read -r T01_ID T01_NAME T01_TRACE T01_FRAMES T01_SEED T01_CHAR T01_TSTAGE <<< "$(sed -n 1p <<< "$tline")"
read -r T02_ID T02_NAME T02_TRACE T02_FRAMES T02_SEED T02_CHAR T02_TSTAGE <<< "$(sed -n 2p <<< "$tline")"
[ "$T01_ID" = t01 ] && [ "$T02_ID" = t02 ] || fail "target manifest ids off"
for tv in "$T01_FRAMES" "$T01_SEED" "$T01_CHAR" "$T01_TSTAGE" \
          "$T02_FRAMES" "$T02_SEED" "$T02_CHAR" "$T02_TSTAGE"; do
  [[ "$tv" =~ ^(0|[1-9][0-9]{0,11})$ ]] || fail "target manifest param grammar ('$tv')"
done
[ "$T01_FRAMES" = 3600 ] && [ "$T02_FRAMES" = 3600 ] \
  || fail "target golden frames != 3600 (wall bounds below assume it; reviewed change)"
made "$M4G/$T01_NAME.sha256.json" "$M4G/$T01_NAME.target.sha256.json"
made "$M4G/$T02_NAME.sha256.json" "$M4G/$T02_NAME.target.sha256.json"
# committed-generator regen guard (the check-target-sim discipline)
for tid in "$T01_ID:$T01_NAME:$T01_TRACE" "$T02_ID:$T02_NAME:$T02_TRACE"; do
  gid="${tid%%:*}"
  rest="${tid#*:}"
  gname="${rest%%:*}"
  gtrace="${rest#*:}"
  gen="$M4G/gen-${gname%%-*}-trace.js"
  [ -f "$gen" ] || fail "$gid: committed generator $gen missing"
  rm -f "$BUILD/$gid.regen.trace.json"
  node "$gen" "$BUILD/$gid.regen.trace.json" >/dev/null
  cmp "$M4G/$gtrace" "$BUILD/$gid.regen.trace.json" \
    || fail "$gid: committed trace != its generator's fresh output (regen drift)"
done
echo "   target manifest validated; t01/t02 traces regen-guarded"

# flow inventory (paired with check-foh-flows.sh's 7-flow pin)
FLOW_IDS=(f06-target-t01 f07-target-t02)
FLOW_SHOTS=("menu-targettest tss-t01" "tss-addcode tss-t02")
FLOW_SEED=("$T01_SEED" "$T02_SEED")
FLOW_FRAMES=("$T01_FRAMES" "$T02_FRAMES")
FLOW_GID=(t01 t02)
FLOW_GNAME=("$T01_NAME" "$T02_NAME")
FLOW_CHAR=("$T01_CHAR" "$T02_CHAR")
globbed="$(ls "$FLOWS"/*.flow | wc -l | tr -d ' ')"
[ "$globbed" = 7 ] || fail "flow inventory — flows/*.flow count $globbed != 7 (paired with check-foh-flows.sh's pin)"
for k in 0 1; do
  id="${FLOW_IDS[$k]}"
  made "$FLOWS/$id.flow" "$FLOWS/$id.expect" "$FLOWS/$id.bstate.expect"
done

anim_file() {
  case "$1" in
    0) echo anim_0_marth.bin ;;
    1) echo anim_1_puff.bin ;;
    2) echo anim_2_fox.bin ;;
    3) echo anim_3_falco.bin ;;
    4) echo anim_4_falcon.bin ;;
    *) echo "DEVICE TARGET FAIL: bad char id '$1'" >&2; return 1 ;;
  esac
}
ANIM_T01="$(anim_file "$T01_CHAR")"
ANIM_T02="$(anim_file "$T02_CHAR")"
GFXDATA_FROZEN=$GFX/gfxdata-frozen.txt
VFXDATA_FROZEN=$GFX/vfxdata-frozen.txt
VFXGLYPHS_FROZEN=$GFX/vfxglyphs-frozen.txt
made "$GFXDATA_FROZEN" "$VFXDATA_FROZEN" "$VFXGLYPHS_FROZEN"

bash pipeline/extractor/build-extractor.sh
rm -f "$TABLES/ml_tables.c" "$TABLES/ml_tables.h" \
  "$TABLES/ml_stages.c" "$TABLES/ml_stages.h" \
  "$TABLES/ml_targets.c" "$TABLES/ml_targets.h" \
  "$TABLES/$ANIM_T01" "$TABLES/$ANIM_T02" \
  "$TABLES/assets/menu.img1"
node pipeline/run.js --only animations,tables,stages,targets,assets --out "$TABLES"
made "$TABLES/ml_tables.c" "$TABLES/ml_stages.c" "$TABLES/ml_targets.c" \
  "$TABLES/ml_targets.h" "$TABLES/$ANIM_T01" "$TABLES/$ANIM_T02"
# A1 restyle Phase 1: the FOH's CSS/SSS screens render REAL upstream artwork
# from the `assets` stage's IMG1 pack, and foh_render's art_load treats a
# missing pack as FATAL. Both sides must be pointed at THIS run's freshly
# regenerated file: the host side via the exported var, the device side via
# its launcher env (sha-verified below). Mirrors
# port/foh/check-device-foh.sh. PROVENANCE: Nintendo-derived, private use
# only, gitignored build output — never committed, never distributed.
made "$TABLES/assets/menu.img1"
export MLFK_MENU_IMG1="$PWD/$TABLES/assets/menu.img1"
rm -f "$BUILD/simdata.txt"
node "$CAL/dump-sim-data.js" --out "$BUILD/simdata.txt"
made "$BUILD/simdata.txt"
rm -f "$BUILD/t01.trace.txt" "$BUILD/t02.trace.txt"
node "$SIM/trace-to-txt.js" "$M4G/$T01_TRACE" "$BUILD/t01.trace.txt"
node "$SIM/trace-to-txt.js" "$M4G/$T02_TRACE" "$BUILD/t02.trace.txt"
made "$BUILD/t01.trace.txt" "$BUILD/t02.trace.txt"
echo "   data planes OK (tables incl. TTAB1 + 2 anim bins + simdata + 2 traces)"

# --- [2] audio build ----------------------------------------------------------
echo "== [2/8] audio build (fresh pipeline audio stage; pinned) =="
DIST="${MELEELIGHT_CLONE:-$HOME/.cache/meleelight-funkey-s/upstream}"
rm -rf "$AUDIO_OUT"
node pipeline/run.js --only audio --dist "$DIST" --out "$AUDIO_OUT"
made "$AUDIO_OUT/sounds.json" "$AUDIO_OUT/audio/music/menu.pcm" \
     "$AUDIO_OUT/audio/music/targettest.pcm"
msum="$(rig_host_sha256 "$AUDIO_OUT/audio/music/menu.pcm")" || exit 1
[ "$msum" = "$MUSIC_MENU_SHA256" ] || fail "menu.pcm sha256 $msum != pinned $MUSIC_MENU_SHA256"
tsum="$(rig_host_sha256 "$AUDIO_OUT/audio/music/targettest.pcm")" || exit 1
[ "$tsum" = "$MUSIC_TT_SHA256" ] || fail "targettest.pcm sha256 $tsum != pinned $MUSIC_TT_SHA256 (pipeline/ffmpeg drift — reviewed re-freeze)"
pack_re="^pack-snd OK count=180 dataBytes=${NUM12} fileBytes=${NUM12}\$"
for side in a b; do
  rm -f "$BUILD/sndpack-$side.bin" "$BUILD/pack-out-$side.txt"
  node "$GFX/pack-snd.js" "$AUDIO_OUT" "$BUILD/sndpack-$side.bin" \
    > "$BUILD/pack-out-$side.txt" || fail "pack-snd.js failed (side $side)"
  made "$BUILD/sndpack-$side.bin" "$BUILD/pack-out-$side.txt"
  c="$(grep -cE "$pack_re" "$BUILD/pack-out-$side.txt")" || true
  [ "$c" = 1 ] || grammar_die "pack-snd output fails the anchored grammar (side $side)"
done
cmp "$BUILD/sndpack-a.bin" "$BUILD/sndpack-b.bin" || fail "sndpack not byte-stable x2"
rm -f "$BUILD/sndpack.bin"
cp "$BUILD/sndpack-a.bin" "$BUILD/sndpack.bin"
made "$BUILD/sndpack.bin"
psum="$(rig_host_sha256 "$BUILD/sndpack.bin")" || exit 1
[ "$psum" = "$SNDPACK_SHA256" ] || fail "sndpack sha256 $psum != pinned $SNDPACK_SHA256"
# targettest + menu cfg extraction (strict) + the pinned-metadata asserts
mcfg="$(node -e '
  const die = (m) => { console.error(m); process.exit(1); };
  const s = JSON.parse(require("fs").readFileSync(process.argv[1], "utf8"));
  if (s.formatVersion !== 1) die("sounds.json formatVersion != 1");
  for (const key of ["menu", "targettest"]) {
    const e = s.music && s.music[key];
    if (!e) die("music." + key + " missing");
    if (e.blob !== "audio/music/" + key + ".pcm") die("non-canonical blob path");
    const sp = e.sprite;
    if (!sp || !Array.isArray(sp.start) || sp.start.length !== 2 ||
        !Array.isArray(sp.loop) || sp.loop.length !== 2) die("sprite shape");
    for (const v of [...sp.start, ...sp.loop]) {
      if (!Number.isInteger(v) || v < 0) die("sprite window not a non-negative integer");
    }
    if (!/^[0-9a-f]{16}$/.test(e.volume.bits)) die("volume bits grammar");
    console.log(key + "_volbits=" + e.volume.bits);
    console.log(key + "_start=" + sp.start[0] + "," + sp.start[1]);
    console.log(key + "_loop=" + sp.loop[0] + "," + sp.loop[1]);
  }
' "$AUDIO_OUT/sounds.json")" || fail "music cfg extraction failed"
unset MENU_VB MENU_SO MENU_LO TT_VB TT_SO TT_LO
while IFS='=' read -r mk mv; do
  case "$mk" in
    menu_volbits) MENU_VB="$mv" ;;
    menu_start) MENU_SO="$mv" ;;
    menu_loop) MENU_LO="$mv" ;;
    targettest_volbits) TT_VB="$mv" ;;
    targettest_start) TT_SO="$mv" ;;
    targettest_loop) TT_LO="$mv" ;;
    *) fail "unexpected music cfg line '$mk=$mv'" ;;
  esac
done <<< "$mcfg"
[ "${MENU_VB:-}" = "$MUSIC_MENU_VOLBITS" ] || fail "menu volbits ${MENU_VB:-} != pinned"
[ "${MENU_SO:-}" = "$MUSIC_MENU_START" ] || fail "menu start window ${MENU_SO:-} != pinned"
[ "${MENU_LO:-}" = "$MUSIC_MENU_LOOP" ] || fail "menu loop window ${MENU_LO:-} != pinned"
[ "${TT_VB:-}" = "$MUSIC_TT_VOLBITS" ] || fail "targettest volbits ${TT_VB:-} != pinned"
[ "${TT_SO:-}" = "$MUSIC_TT_START" ] || fail "targettest start window ${TT_SO:-} != pinned"
[ "${TT_LO:-}" = "$MUSIC_TT_LOOP" ] || fail "targettest loop window ${TT_LO:-} != pinned"
rm -f "$BUILD/mus-host.txt" "$BUILD/mus-dev.txt"
{
  echo "track menu $AUDIO_OUT/audio/music/menu.pcm $MENU_VB ${MENU_SO/,/ } ${MENU_LO/,/ }"
  echo "track targettest $AUDIO_OUT/audio/music/targettest.pcm $TT_VB ${TT_SO/,/ } ${TT_LO/,/ }"
} > "$BUILD/mus-host.txt"
{
  echo "track menu $DSD/menu.pcm $MENU_VB ${MENU_SO/,/ } ${MENU_LO/,/ }"
  echo "track targettest $DSD/targettest.pcm $TT_VB ${TT_SO/,/ } ${TT_LO/,/ }"
} > "$BUILD/mus-dev.txt"
made "$BUILD/mus-host.txt" "$BUILD/mus-dev.txt"
echo "   audio OK (sndpack + menu/targettest PCM pinned; meta pins verified)"

# --- [3] host twin legs -------------------------------------------------------
echo "== [3/8] host twin build + twin legs (references + BOTH-verifier judgments) =="
CFLAGS_COMMON=(-ffp-contract=off -Wall -Wextra -Werror
  -I"$TABLES" -Iport/ryu -Iport/sim -Ioracle/qjs)
SIM_TUS=(
  port/sim/sim/sim_boot.c port/sim/sim/sim_tick.c port/sim/sim/sim_ser.c
  port/sim/sim/sim_data.c port/sim/sim/sim_ai_live.c
  port/sim/calib/canon.c port/sim/calib/player_canon.c
  port/sim/ai.c
  port/sim/physics.c port/sim/interpolated_collision.c
  port/sim/environmental_collision.c port/sim/hit_detection.c
  port/sim/article.c port/sim/action_state_shortcuts.c
  port/sim/ml_events.c port/sim/ml_fmt.c port/sim/ml_ser.c
  port/sim/ai_bridge.c port/sim/input/interpret_inputs.c
  port/sim/stages/moving_platforms.c port/sim/stages/ystory.c
  port/sim/stages/fountain.c
  port/sim/characters/shared/moves_index.c
  port/sim/characters/fox/moves_index.c
  port/sim/characters/falco/moves_index.c
  port/sim/characters/falcon/moves_index.c
  port/sim/characters/marth/moves_index.c
  port/sim/characters/marth/dancing_blade_combo.c
  port/sim/characters/marth/dancing_blade_air_mobility.c
  port/sim/characters/puff/moves_index.c
  port/sim/characters/puff/puff_multi_jump_drift.c
  port/sim/characters/puff/puff_next_jump.c
  "$TABLES/ml_tables.c" "$TABLES/ml_stages.c" "$TABLES/ml_targets.c"
  oracle/qjs/sha256.c port/fdlibm/fdlibm.c
)
rm -f "$BUILD/raster.o" "$BUILD/foh_dev_headless"
cc -O3 "${CFLAGS_COMMON[@]}" -c "$GFX/raster.c" -o "$BUILD/raster.o"
cc -O2 "${CFLAGS_COMMON[@]}" -o "$BUILD/foh_dev_headless" \
  "$BUILD/raster.o" "$FOH/foh_dev.c" "$FOH/foh.c" "$FOH/foh_font.c" \
  "$FOH/foh_render.c" "$FOH/foh_persist.c" "$FOH/foh_pause.c" \
  "$GFX/ctl_style.c" "$GFX/img1.c" \
  "$GFX/platform_headless.c" \
  "$GFX/anim1.c" "$GFX/gfx_render.c" "$GFX/gfx_target.c" \
  "$GFX/gfx_vfx.c" "$GFX/gfx_overlay.c" "$GFX/gfx_glyphs.c" "$GFX/gfx_bg.c" \
  "$TGT/target_play.c" \
  "${SIM_TUS[@]}" \
  port/sim/characters/shared/moves/*.c \
  port/sim/characters/fox/moves/*.c \
  port/sim/characters/falco/moves/*.c \
  port/sim/characters/falcon/moves/*.c \
  port/sim/characters/marth/moves/*.c \
  port/sim/characters/puff/moves/*.c \
  -lm -lpthread
made "$BUILD/foh_dev_headless"
# keymap SSOT: the compiled dump == the frozen file (the iter-97 proof
# teeth live in check-device-foh.sh; this is the paired identity gate)
rm -f "$BUILD/keymap-dump.txt"
"$BUILD/foh_dev_headless" --dump-keymap > "$BUILD/keymap-dump.txt" \
  || fail "foh_dev --dump-keymap failed"
made "$BUILD/keymap-dump.txt"
cmp "$BUILD/keymap-dump.txt" "$FOH/keymap-frozen.txt" \
  || fail "foh_dev's compiled keymap != the frozen keymap-frozen.txt (SSOT drift)"
echo "   host twin built; keymap dump == frozen"

# whole-log verdict assert (the check-foh-flows discipline)
assert_verdict() { # <vlog> <want-file> <ctx>
  local rc=0
  cmp -s "$1" "$2" || rc=$?
  if [ "$rc" = 1 ]; then
    grammar_die "$3 — verify log is not BYTE-IDENTICAL to the constructed verdict line"
  elif [ "$rc" -ge 2 ]; then
    grammar_die "$3 — cmp rc $rc reading the verify log (corrupt evidence)"
  fi
}
judge_both_streams() { # <ctx> <sim-out> <gid> <gname> <frames> <outdir>
  local ctx="$1" simout="$2" gid="$3" gname="$4" frames="$5" od="$6"
  rm -f "$od/$gid.player.json" "$od/$gid.target.json"
  node "$M4G/wrap-target.js" "$gid" "$simout" \
    "$od/$gid.player.json" "$od/$gid.target.json" >/dev/null \
    || fail "$ctx: wrap-target failed"
  made "$od/$gid.player.json" "$od/$gid.target.json"
  local vlog="$od/$gid.verify.log" rng
  rm -f "$vlog"
  node oracle/harness/verify-stream.js "$od/$gid.player.json" \
    "$M4G/$gname.sha256.json" > "$vlog" 2>&1 \
    || { cat "$vlog" >&2; fail "$ctx: verify-stream rc != 0"; }
  made "$vlog"
  rng="$(node -e '
    const j = JSON.parse(require("fs").readFileSync(process.argv[1], "utf8"));
    console.log(String(j.rngCalls));
  ' "$M4G/$gname.sha256.json")" || fail "$ctx: cannot read rngCalls"
  [[ "$rng" =~ ^[0-9]{1,6}$ ]] || fail "$ctx: frozen rngCalls grammar ('$rng')"
  printf 'STREAM MATCH %s: %s/%s frames exact, rngCalls=%s, rngCallsOutsideStep=1, specVersion=1\n' \
    "$gname" "$frames" "$frames" "$rng" > "$od/$gid.verdict-want.txt"
  assert_verdict "$vlog" "$od/$gid.verdict-want.txt" "$ctx player"
  local tvlog="$od/$gid.tverify.log" tfin tmin tetg
  rm -f "$tvlog"
  node "$M4G/verify-target-stream.js" "$od/$gid.target.json" \
    "$M4G/$gname.target.sha256.json" > "$tvlog" 2>&1 \
    || { cat "$tvlog" >&2; fail "$ctx: verify-target-stream rc != 0"; }
  made "$tvlog"
  read -r tfin tmin tetg <<< "$(node -e '
    const j = JSON.parse(require("fs").readFileSync(process.argv[1], "utf8"));
    console.log([j.params.finalTargetsDestroyed, j.params.minTargets,
      j.params.finalEndTargetGame].join(" "));
  ' "$M4G/$gname.target.sha256.json")" || fail "$ctx: frozen target metadata read failed"
  [[ "$tfin" =~ ^(0|[1-9][0-9]?)$ ]] || fail "$ctx: finalTargetsDestroyed grammar"
  [[ "$tmin" =~ ^([1-9]|10)$ ]] || fail "$ctx: minTargets grammar"
  [ "$tetg" = false ] || fail "$ctx: frozen finalEndTargetGame not false"
  printf 'TARGET STREAM MATCH %s: %s/%s target frames exact, targetsDestroyed=%s (>= minTargets %s), endTargetGame=false, sibling seal OK, manifest bound, specVersion=1\n' \
    "$gname" "$frames" "$frames" "$tfin" "$tmin" > "$od/$gid.tverdict-want.txt"
  assert_verdict "$tvlog" "$od/$gid.tverdict-want.txt" "$ctx target"
}

# strict summary parsers (the check-device-foh grammars — same producer)
# --- app-log reader hygiene, LIFTED VERBATIM from check-device-fullgame.sh
# (review-119-delta-6b [H2]): these two helpers already existed there and
# were already reviewed; target/foh simply never used them, so their app-log
# parsers asserted no newline termination and laundered grep status with
# `|| true`. Reusing the reviewed bytes rather than writing a third variant.
nl_terminated() {
  local f="$1" label="$2" n
  [ -f "$f" ] || grammar_die "$label: $f missing"
  [ -s "$f" ] || grammar_die "$label: $f is empty"
  # STANDALONE, STATUS-CHECKED capture (review-109-3 M2): inside
  # `[ "$(...)" = 1 ]` the surrounding `[` reports success on the TEXT
  # alone, so a producer that printed `1` and then failed still certified
  # newline termination. The assignment is its own command, so `set -e`
  # (or the explicit guard) sees the pipeline's real status.
  n="$(tail -c 1 "$f" | wc -l | tr -d ' ')" \
    || grammar_die "$label: could not read the final byte of $f"
  [ "$n" = 1 ] \
    || grammar_die "$label: $f does not end in a newline (torn write)"
}

grep_count() {
  local re="$1" f="$2" label="$3" n rc=0
  n="$(grep -cE -e "$re" "$f")" || rc=$?
  [ "$rc" -le 1 ] || grammar_die "$label: grep failed reading $f (rc $rc)"
  printf '%s\n' "$n"
}

parse_foh_summary() { # <log> <launched 0|1> <want-shots>
  local log="$1" launched="$2" wshots="$3" re cnt line pcnt
  unset foh_skips foh_fails foh_transitions
  nl_terminated "$log" "foh summary"
  pcnt="$(grep_count 'foh_dev foh:' "$log" "foh summary")"
  [ "$pcnt" = 1 ] || grammar_die "app log $log has $pcnt 'foh_dev foh:' needles (want 1)"
  re="^foh_dev foh: ${NUM12} ticks, ${NUM12} transitions, ${wshots} shots, ${NUM12} render skips, ${NUM12} failed presents, launched=${launched}\$"
  cnt="$(grep_count "$re" "$log" "foh summary")"
  [ "$cnt" = 1 ] || grammar_die "app log $log has $cnt foh-summary grammar matches (want 1)"
  line="$(grep -E "$re" "$log")"
  if [[ "$line" =~ ^foh_dev\ foh:\ (0|[1-9][0-9]{0,11})\ ticks,\ (0|[1-9][0-9]{0,11})\ transitions,\ ${wshots}\ shots,\ (0|[1-9][0-9]{0,11})\ render\ skips,\ (0|[1-9][0-9]{0,11})\ failed\ presents,\ launched=${launched}$ ]]; then
    foh_transitions="${BASH_REMATCH[2]}"
    foh_skips="${BASH_REMATCH[3]}"
    foh_fails="${BASH_REMATCH[4]}"
  else
    grammar_die "foh summary line failed re-extraction ('$line')"
  fi
}
parse_match_summary() { # <log> <frames> <pace>
  local log="$1" fr="$2" pace="$3" re cnt line pcnt
  unset match_skips match_fails match_wall_ms
  nl_terminated "$log" "match summary"
  pcnt="$(grep_count 'foh_dev match:' "$log" "match summary")"
  [ "$pcnt" = 1 ] || grammar_die "app log $log has $pcnt 'foh_dev match:' needles (want 1)"
  re="^foh_dev match: ${fr} frames, ${NUM12} render skips, ${NUM12} failed presents, wall ${NUM12} ms, pace=${pace} budget=${BUDGET_NS} ns\$"
  cnt="$(grep_count "$re" "$log" "match summary")"
  [ "$cnt" = 1 ] || grammar_die "app log $log has $cnt match-summary grammar matches (want 1)"
  line="$(grep -E "$re" "$log")"
  if [[ "$line" =~ ^foh_dev\ match:\ ${fr}\ frames,\ (0|[1-9][0-9]{0,11})\ render\ skips,\ (0|[1-9][0-9]{0,11})\ failed\ presents,\ wall\ (0|[1-9][0-9]{0,11})\ ms,\ pace=${pace}\ budget=${BUDGET_NS}\ ns$ ]]; then
    match_skips="${BASH_REMATCH[1]}"
    match_fails="${BASH_REMATCH[2]}"
    match_wall_ms="${BASH_REMATCH[3]}"
  else
    grammar_die "match summary line failed re-extraction ('$line')"
  fi
}
parse_audio_summary() { # <log>
  local log="$1" re cnt line pcnt
  unset au_underruns au_badlen au_starts au_stops
  nl_terminated "$log" "audio summary"
  pcnt="$(grep_count 'foh_dev audio:' "$log" "audio summary")"
  [ "$pcnt" = 1 ] || grammar_die "app log $log has $pcnt 'foh_dev audio:' needles (want 1)"
  re="^foh_dev audio: ${NUM12} callbacks, ${NUM12} underruns, ${NUM12} badlen, ${NUM12} voice starts, ${NUM12} voice stops, ${NUM12} steals, rate=(0|44100) samples=(0|512) channels=(0|2)\$"
  cnt="$(grep_count "$re" "$log" "audio summary")"
  [ "$cnt" = 1 ] || grammar_die "app log $log has $cnt audio-summary grammar matches (want 1)"
  line="$(grep -E "$re" "$log")"
  if [[ "$line" =~ ^foh_dev\ audio:\ (0|[1-9][0-9]{0,11})\ callbacks,\ (0|[1-9][0-9]{0,11})\ underruns,\ (0|[1-9][0-9]{0,11})\ badlen,\ (0|[1-9][0-9]{0,11})\ voice\ starts,\ (0|[1-9][0-9]{0,11})\ voice\ stops,\ (0|[1-9][0-9]{0,11})\ steals, ]]; then
    au_underruns="${BASH_REMATCH[2]}"
    au_badlen="${BASH_REMATCH[3]}"
    au_starts="${BASH_REMATCH[4]}"
    au_stops="${BASH_REMATCH[5]}"
  else
    grammar_die "audio summary line failed re-extraction ('$line')"
  fi
}
parse_music_summary() { # <log>
  local log="$1" re cnt line pcnt
  unset mu_out mu_starves mu_refills
  nl_terminated "$log" "music summary"
  pcnt="$(grep_count 'foh_dev music:' "$log" "music summary")"
  [ "$pcnt" = 1 ] || grammar_die "app log $log has $pcnt 'foh_dev music:' needles (want 1)"
  re="^foh_dev music: ${NUM19} out frames, ${NUM12} starves, ${NUM12} refills, ring=32768 chunk=16384\$"
  cnt="$(grep_count "$re" "$log" "music summary")"
  [ "$cnt" = 1 ] || grammar_die "app log $log has $cnt music-summary grammar matches (want 1)"
  line="$(grep -E "$re" "$log")"
  if [[ "$line" =~ ^foh_dev\ music:\ (0|[1-9][0-9]{0,18})\ out\ frames,\ (0|[1-9][0-9]{0,11})\ starves,\ (0|[1-9][0-9]{0,11})\ refills, ]]; then
    mu_out="${BASH_REMATCH[1]}"
    mu_starves="${BASH_REMATCH[2]}"
    mu_refills="${BASH_REMATCH[3]}"
  else
    grammar_die "music summary line failed re-extraction ('$line')"
  fi
}
# the finish line is ABSENT on green legs (no committed flow reaches
# the finish seam — AGENT-LOG iter-99 refutation; probe-covered)
assert_no_tfinish() { # <log>
  local c m
  c="$(grep -cF 'foh_dev tfinish:' "$1")" || true
  [ "$c" = 0 ] || fail "app log $1 carries a 'foh_dev tfinish:' line — the finish seam fired on a committed leg (outside the measured domain; investigate)"
  # malformed-resemblance arm (iter 101, review-99 L2): ANY 'tfinish'
  # substring — torn/mangled/prefixed variants included — is death;
  # resembles-but-doesn't-match is corruption, never tolerated
  # (PROCESS §3). Corpus-validated: zero occurrences across all
  # archived genuine logs (.loop/m4-task12-* + .loop/m4-task13-*).
  m="$(grep -cF 'tfinish' "$1")" || true
  [ "$m" = 0 ] || fail "app log $1 carries a 'tfinish' resemblance on $m line(s) without the exact seam line — corrupted/mangled finish evidence (fail closed)"
}

# track-identified music evidence (iter 101, review-99 M2): every
# target leg must carry EXACTLY the mustrack pair — the boot menu
# program and the menu->targettest switch at the TLAUNCH seam — as
# exact FULL-LINE fixed-string matches (whitelist by construction;
# the producer grammar note lives in foh_dev.c's header). The named
# pcm paths are exactly the paths whose bytes this check sha-pins
# (host: targettest.pcm == MUSIC_TT_SHA256 / menu.pcm ==
# MUSIC_MENU_SHA256 at [2]; device: $DSD copies sha-verified == host
# at [5]) — track identity binds by path join, so a menu track that
# kept playing (no switch) or an unpinned substitute PCM can never
# pass. Aggregate out/refill/starve counters gate liveness ONLY.
MUSTRACK_LEGS=0
assert_mustrack() { # <log> <pcm-dir> <ctx>
  local log="$1" pdir="$2" ctx="$3" c
  c="$(grep -cF 'foh_dev mustrack:' "$log")" || true
  [ "$c" = 2 ] || grammar_die "$ctx: $c 'foh_dev mustrack:' needles (want exactly 2 — boot menu program + TLAUNCH targettest switch)"
  c="$(grep -cxF "foh_dev mustrack: from=none to=menu on=0 pcm=$pdir/menu.pcm" "$log")" || true
  [ "$c" = 1 ] || grammar_die "$ctx: the boot menu-track program line is absent/malformed (want exactly 'foh_dev mustrack: from=none to=menu on=0 pcm=$pdir/menu.pcm')"
  c="$(grep -cxF "foh_dev mustrack: from=menu to=targettest on=1 pcm=$pdir/targettest.pcm" "$log")" || true
  [ "$c" = 1 ] || grammar_die "$ctx: the menu->targettest switch line is absent/malformed — the track-identified TLAUNCH music witness (want exactly 'foh_dev mustrack: from=menu to=targettest on=1 pcm=$pdir/targettest.pcm')"
  MUSTRACK_LEGS=$((MUSTRACK_LEGS + 1))
}

judge_dev_shot() { # <ctx> <device.ppm> <ref.ppm>
  local ctx="$1" df="$2" rf="$3"
  made "$df" "$rf"
  node -e '
    const fs = require("fs");
    const b = fs.readFileSync(process.argv[1]);
    const hdr = Buffer.from("P6\n240 240\n255\n", "latin1");
    if (b.length < hdr.length || !b.subarray(0, hdr.length).equals(hdr)) {
      console.error("shot header is not exactly P6/240 240/255"); process.exit(1);
    }
    if (b.length !== hdr.length + 240 * 240 * 3) {
      console.error("shot payload != 240*240*3"); process.exit(1);
    }
  ' "$df" || fail "shot $ctx: structural validation failed on the device shot"
  cmp "$df" "$rf" || fail "shot $ctx: device shot != host twin reference (byte-exact judgment)"
}

judge_fbwit() { # <file> <flow-id> <shot-names...>  (the iter-97 strict reader)
  local wf="$1" fid="$2"
  shift 2
  local want=("$@") nw=${#want[@]} i=0 ln sawEnd=0
  made "$wf"
  # NO CONTROL BYTES except TAB/LF (review-118-delta-4-codex [M]): bash's
  # `read -r` STOPS AT NUL, so the bytes `W 12 shot-name yoff=0 eq=1\0 eq=0
  # CORRUPT` are parsed as just `W 12 shot-name yoff=0 eq=1` — the row
  # grammar passes, the line count agrees, the trailing-LF check agrees,
  # and the engine emits a clean verdict over a witness whose real bytes
  # say the pixels did NOT match. Every guard in this reader reasons about
  # LINES, and a control byte breaks line reasoning in ways no line-level
  # guard can see (this is the same class verify_m4.sh refuses in its
  # evidence logs; closed HERE too, because the gate never sees the
  # witness file itself). A control byte in an ASCII witness is corruption
  # by definition, so the class is refused once, before anything parses.
  # MEASURED SAFE: the FBWIT1 grammar is pure ASCII (header literals plus
  # `[a-z0-9-]` shot names), so a genuine witness can never contain one.
  ascii_text_ok "$wf" \
    || grammar_die "fbwit $fid: witness contains control byte(s) other than TAB/LF, or could not be measured — CORRUPT witness (control bytes defeat line-oriented row parsing)"
  [ -z "$(tail -c 1 "$wf")" ] \
    || grammar_die "fbwit $fid: missing trailing newline (torn write)"
  local nlines
  nlines="$(grep -c "" "$wf")" || fail "fbwit $fid: cannot count lines"
  [ "$nlines" = "$((nw + 2))" ] || grammar_die "fbwit $fid: $nlines lines (want $((nw + 2)))"
  while IFS= read -r ln; do
    [ "$sawEnd" = 0 ] || grammar_die "fbwit $fid: content after the END terminator"
    if [ "$i" = 0 ]; then
      [ "$ln" = "FBWIT1 flow=$fid xform=$FBWIT_XFORM_PIN ll=$FBWIT_LL_PIN vyres=$FBWIT_VYRES_PIN" ] \
        || grammar_die "fbwit $fid: header '$ln' != pinned envelope"
    elif [ "$i" -le "$nw" ]; then
      if ! [[ "$ln" =~ ^W\ (0|[1-9][0-9]{0,6})\ ([a-z0-9-]{1,32})\ yoff=0\ eq=1$ ]]; then
        grammar_die "fbwit $fid: row $i fails the FBWIT1 grammar: '$ln'"
      fi
      [ "${BASH_REMATCH[2]}" = "${want[$((i - 1))]}" ] \
        || grammar_die "fbwit $fid: row $i shot '${BASH_REMATCH[2]}' != expected '${want[$((i - 1))]}'"
    else
      [ "$ln" = "END shots=$nw" ] \
        || grammar_die "fbwit $fid: trailer '$ln' != 'END shots=$nw'"
      sawEnd=1
    fi
    i=$((i + 1))
  done < "$wf"
  [ "$i" = "$nlines" ] || grammar_die "fbwit $fid: reader iterated $i of $nlines lines"
  [ "$sawEnd" = 1 ] || grammar_die "fbwit $fid: END terminator never seen"
}

declare -a TWIN_STARTS TWIN_STOPS
run_twin() { # <k> <side>
  local k="$1" side="$2" id
  id="${FLOW_IDS[$k]}"
  rm -rf "$BUILD/twin-$id-$side"
  mkdir -p "$BUILD/twin-$id-$side/shots"
  # task 13 hermeticity (iter 100): fresh per-invocation persist dir
  MLFK_PERSIST_DIR="$PWD/$BUILD/twin-$id-$side/persist" \
  "$BUILD/foh_dev_headless" --flow "$FLOWS/$id.flow" --input flow \
    --flow-out "$BUILD/twin-$id-$side/trace.txt" \
    --shots-dir "$BUILD/twin-$id-$side/shots" --pace 0 \
    --bridge tverify --simdata "$BUILD/simdata.txt" --seed "${FLOW_SEED[$k]}" \
    --trace "$BUILD/${FLOW_GID[$k]}.trace.txt" --frames "${FLOW_FRAMES[$k]}" \
    --out "$BUILD/twin-$id-$side/stream.txt" \
    --timing "$BUILD/twin-$id-$side/tim.txt" \
    --bstate-out "$BUILD/twin-$id-$side/bstate.txt" \
    --gfxdata "$GFXDATA_FROZEN" --vfxdata "$VFXDATA_FROZEN" \
    --glyphs "$VFXGLYPHS_FROZEN" --anim-dir "$TABLES" --legible \
    --sndpack "$BUILD/sndpack.bin" \
    --music-manifest "$BUILD/mus-host.txt" \
    2> "$BUILD/twin-$id-$side/log.txt"
  made "$BUILD/twin-$id-$side/trace.txt" "$BUILD/twin-$id-$side/stream.txt" \
       "$BUILD/twin-$id-$side/bstate.txt" "$BUILD/twin-$id-$side/log.txt"
}
for k in 0 1; do
  id="${FLOW_IDS[$k]}"
  run_twin "$k" a
  cmp "$BUILD/twin-$id-a/trace.txt" "$FLOWS/$id.expect" \
    || fail "twin $id: flow-mode trace differs from the frozen $FLOWS/$id.expect"
  cmp "$BUILD/twin-$id-a/bstate.txt" "$FLOWS/$id.bstate.expect" \
    || fail "twin $id: TBRIDGE-STATE differs from the frozen witness"
  nshots_want="$(printf '%s\n' ${FLOW_SHOTS[$k]} | wc -l | tr -d ' ')"
  nshots_got="$(ls "$BUILD/twin-$id-a/shots" | wc -l | tr -d ' ')"
  [ "$nshots_got" = "$nshots_want" ] || fail "twin $id: $nshots_got shots != $nshots_want"
  judge_both_streams "twin $id" "$BUILD/twin-$id-a/stream.txt" \
    "${FLOW_GID[$k]}" "${FLOW_GNAME[$k]}" "${FLOW_FRAMES[$k]}" "$BUILD/twin-$id-a"
  parse_audio_summary "$BUILD/twin-$id-a/log.txt"
  assert_no_tfinish "$BUILD/twin-$id-a/log.txt"
  assert_mustrack "$BUILD/twin-$id-a/log.txt" "$AUDIO_OUT/audio/music" "twin $id"
  # frozen SFX pin (iter 101, review-99 M3): the twin must reproduce
  # the committed per-flow start count — refutation shape (c) of the
  # iter-101 pre-registration fires here if the count is run-varying.
  [ "$au_starts" = "${SFX_STARTS_PIN[$k]}" ] \
    || fail "twin $id: voice starts $au_starts != the frozen pin ${SFX_STARTS_PIN[$k]} (measured-then-frozen; reviewed re-freeze only)"
  TWIN_STARTS[$k]="$au_starts"
  TWIN_STOPS[$k]="$au_stops"
done
# f06 x2 byte-stability
run_twin 0 b
cmp "$BUILD/twin-f06-target-t01-a/trace.txt" "$BUILD/twin-f06-target-t01-b/trace.txt" \
  || fail "twin f06 x2: traces not byte-identical"
cmp "$BUILD/twin-f06-target-t01-a/stream.txt" "$BUILD/twin-f06-target-t01-b/stream.txt" \
  || fail "twin f06 x2: streams not byte-identical"
for sname in ${FLOW_SHOTS[0]}; do
  cmp "$BUILD/twin-f06-target-t01-a/shots/$sname.ppm" \
      "$BUILD/twin-f06-target-t01-b/shots/$sname.ppm" \
    || fail "twin f06 x2: shot $sname not byte-identical"
done
parse_audio_summary "$BUILD/twin-f06-target-t01-b/log.txt"
[ "$au_starts" = "${TWIN_STARTS[0]}" ] || fail "twin f06 x2: voice starts differ"
[ "$au_stops" = "${TWIN_STOPS[0]}" ] || fail "twin f06 x2: voice stops differ"
# the second f06 twin joins the full evidence scan (iter 101,
# review-99 L2/M2/M3 — no leg is exempt)
[ "$au_starts" = "${SFX_STARTS_PIN[0]}" ] \
  || fail "twin f06 x2: voice starts $au_starts != the frozen pin ${SFX_STARTS_PIN[0]}"
assert_no_tfinish "$BUILD/twin-f06-target-t01-b/log.txt"
assert_mustrack "$BUILD/twin-f06-target-t01-b/log.txt" "$AUDIO_OUT/audio/music" "twin f06 x2"
echo "   twin legs OK (2 traces + 2 TBRIDGE-STATEs == frozen; f06 x2 stable; BOTH streams verdict-exact per leg; starts == frozen pins; mustrack pair per leg)"

# --- [4] fk scripts -----------------------------------------------------------
echo "== [4/8] fk_input scripts (derived x2, byte-stable) =="
for k in 0 1; do
  id="${FLOW_IDS[$k]}"
  rm -f "$BUILD/$id.fks" "$BUILD/$id.fks.b"
  node "$FOH/flow-to-fkscript.js" "$FLOWS/$id.flow" "$BUILD/$id.fks" >/dev/null
  node "$FOH/flow-to-fkscript.js" "$FLOWS/$id.flow" "$BUILD/$id.fks.b" >/dev/null
  made "$BUILD/$id.fks" "$BUILD/$id.fks.b"
  cmp "$BUILD/$id.fks" "$BUILD/$id.fks.b" || fail "fk script $id not byte-stable x2"
done
echo "   2 fk scripts derived (LEAD 8200 ms, 1 device frame per flow frame)"

# --- [5] arm build + push -----------------------------------------------------
echo "== [5/8] armv7 build (shared rig stamp) + push + provenance =="
rig_arm_build
rig_stamp_rehash foh_device fk_input
dsh "rm -rf $DTMP $DSD && mkdir -p $DTMP $DSD"
adb -s "$DEV" push "$DEVB/foh_device" "$DEVB/fk_input" \
  "$BUILD/simdata.txt" "$BUILD/t01.trace.txt" "$BUILD/t02.trace.txt" \
  "$GFXDATA_FROZEN" "$VFXDATA_FROZEN" "$VFXGLYPHS_FROZEN" \
  "$TABLES/$ANIM_T01" "$TABLES/$ANIM_T02" \
  "$TABLES/assets/menu.img1" \
  "$BUILD/mus-dev.txt" "$DTMP/" >/dev/null
for k in 0 1; do
  id="${FLOW_IDS[$k]}"
  adb -s "$DEV" push "$FLOWS/$id.flow" "$BUILD/$id.fks" "$DTMP/" >/dev/null
done
adb -s "$DEV" push "$BUILD/sndpack.bin" \
  "$AUDIO_OUT/audio/music/menu.pcm" \
  "$AUDIO_OUT/audio/music/targettest.pcm" "$DSD/" >/dev/null
rig_push_provenance "$DTMP" foh_device fk_input
dsh "chmod +x $DTMP/foh_device $DTMP/fk_input"
for hf in "$BUILD/simdata.txt" "$BUILD/t01.trace.txt" "$BUILD/t02.trace.txt" \
          "$GFXDATA_FROZEN" "$VFXDATA_FROZEN" "$VFXGLYPHS_FROZEN" \
          "$TABLES/$ANIM_T01" "$TABLES/$ANIM_T02" "$TABLES/assets/menu.img1" \
          "$BUILD/mus-dev.txt"; do
  bn="$(basename "$hf")"
  hsum="$(rig_host_sha256 "$hf")" || exit 1
  dsum="$(rig_dev_sha256 "$DTMP/$bn")" || exit 1
  [ "$dsum" = "$hsum" ] || fail "pushed $bn device sha ($dsum) != host sha ($hsum)"
done
for k in 0 1; do
  id="${FLOW_IDS[$k]}"
  for hf in "$FLOWS/$id.flow" "$BUILD/$id.fks"; do
    bn="$(basename "$hf")"
    hsum="$(rig_host_sha256 "$hf")" || exit 1
    dsum="$(rig_dev_sha256 "$DTMP/$bn")" || exit 1
    [ "$dsum" = "$hsum" ] || fail "pushed $bn device sha ($dsum) != host sha ($hsum)"
  done
done
for hf in "$BUILD/sndpack.bin" "$AUDIO_OUT/audio/music/menu.pcm" \
          "$AUDIO_OUT/audio/music/targettest.pcm"; do
  bn="$(basename "$hf")"
  hsum="$(rig_host_sha256 "$hf")" || exit 1
  dsum="$(rig_dev_sha256 "$DSD/$bn")" || exit 1
  [ "$dsum" = "$hsum" ] || fail "pushed $bn device sha ($dsum) != host sha ($hsum)"
done
dsh "sync" # writeback BEFORE the paced legs (the iter-73 mitigation)
echo "   pushed + sha-verified (binaries via stamp provenance)"

# --- deadman + park (spanning the device legs) --------------------------------
DM_NONCE="$RANDOM$RANDOM$$"
rm -f "$BUILD/deadman.sh"
cat > "$BUILD/deadman.sh" << EOF
#!/bin/sh
# generated by check-device-target.sh — frontend-park DEADMAN (the
# check-device-foh.sh design, foh_device scoped)
echo \$\$ > $DTMP/deadman.pid
i=0
while [ \$i -lt $DEADMAN_S ]; do
  sleep 2
  if [ -f $DTMP/deadman.cancel ]; then rm -f $DTMP/deadman.pid; exit 0; fi
  i=\$((i+2))
done
if [ "\$(cat $DTMP/deadman.nonce 2>/dev/null)" = "$DM_NONCE" ] && [ ! -f $DTMP/deadman.cancel ]; then
  echo fired > $DTMP/deadman.fired
  rm -f /mnt/disable_frontend
  gp="\$(cat $DTMP/foh.pid.$DM_NONCE 2>/dev/null)"
  case "\$gp" in
    ''|*[!0-9]*) : ;;
    *) if grep -q foh_device "/proc/\$gp/cmdline" 2>/dev/null; then kill "\$gp"; fi ;;
  esac
  if [ -f $DTMP/qd.low_bat_check.$DM_NONCE ]; then
    n=0
    for c in /proc/[0-9]*/comm; do
      if [ "x\$(cat "\$c" 2>/dev/null)" = "xlow_bat_check" ]; then n=\$((n+1)); fi
    done
    if [ "\$n" = 0 ]; then /etc/init.d/S12low-bat-check start; fi
    n2=0
    for c in /proc/[0-9]*/comm; do
      if [ "x\$(cat "\$c" 2>/dev/null)" = "xlow_bat_check" ]; then n2=\$((n2+1)); fi
    done
    if [ "\$n2" != 0 ]; then rm -f $DTMP/qd.low_bat_check.$DM_NONCE; fi
  fi
fi
rm -f $DTMP/deadman.pid
exit 0
EOF
made "$BUILD/deadman.sh"
adb -s "$DEV" push "$BUILD/deadman.sh" "$DTMP/" >/dev/null
hsum="$(rig_host_sha256 "$BUILD/deadman.sh")" || exit 1
dsum="$(rig_dev_sha256 "$DTMP/deadman.sh")" || exit 1
[ "$dsum" = "$hsum" ] || fail "pushed deadman.sh sha mismatch"
dsh "printf '%s' '$DM_NONCE' > $DTMP/deadman.nonce; rm -f $DTMP/deadman.cancel $DTMP/deadman.fired"
dsh "setsid sh $DTMP/deadman.sh </dev/null >/dev/null 2>&1 & sleep 1"
dsh "test -f $DTMP/deadman.pid" >/dev/null 2>&1 || fail "park deadman did not start"
DEADMAN_ARMED=1
PARKED=1
dsh "touch /mnt/disable_frontend"
prc=0
dsh "pkill gmenu2x" >/dev/null 2>&1 || prc=$?
case "$prc" in
  0) : ;;
  1) echo "WARN: gmenu2x was not running at park time" >&2 ;;
  *) fail "pkill gmenu2x failed (rc $prc)" ;;
esac
echo "   deadman armed (${DEADMAN_S}s) + frontend parked"

# --- [6] device legs ----------------------------------------------------------
echo "== [6/8] device legs: fk_input -> uinput -> SDL keysyms -> platform_poll =="
DEV_STARTS=""
FBWIT_TOTAL=0
P99_WORST_NS=0
P99_WORST_MS=""
for k in 0 1; do
  id="${FLOW_IDS[$k]}"
  endf="$(grep -E '^END (0|[1-9][0-9]*)$' "$FLOWS/$id.flow" | awk '{print $2}')"
  [[ "$endf" =~ ^(0|[1-9][0-9]{0,5})$ ]] || fail "leg $id: flow END frame grammar ('$endf')"
  # CSS mechanics arc: the injector's scale is now 1:1 (one flow frame ==
  # one device frame — the CSS cursor is level-driven, css.js:195-196), so
  # this bound is LEAD in ticks + the flow's own frame span + margin. The
  # old `*3/50` restated the retired STEP_MS=50 model.
  fohmax=$(( (8200 * 60 / 1000) + (endf - 370) + 600 ))
  args="--flow $DTMP/$id.flow --input poll --flow-out $DTMP/$id.trace.txt"
  args="$args --shots-dir $DTMP/$id-shots --ready-file $DTMP/$id.ready"
  args="$args --foh-max $fohmax --pace 1 --budget-ns $BUDGET_NS"
  args="$args --fb-witness $DTMP/$id.fbwit.txt"
  args="$args --sndpack $DSD/sndpack.bin --music-manifest $DTMP/mus-dev.txt"
  args="$args --bridge tverify --simdata $DTMP/simdata.txt --seed ${FLOW_SEED[$k]}"
  args="$args --trace $DTMP/${FLOW_GID[$k]}.trace.txt --frames ${FLOW_FRAMES[$k]}"
  args="$args --out $DTMP/$id.out.txt --timing $DTMP/$id.tim.txt"
  args="$args --bstate-out $DTMP/$id.bstate.txt"
  args="$args --gfxdata $DTMP/gfxdata-frozen.txt --vfxdata $DTMP/vfxdata-frozen.txt"
  args="$args --glyphs $DTMP/vfxglyphs-frozen.txt --anim-dir $DTMP --legible"
  rm -f "$BUILD/$id.argv"
  printf '%s\n' "$args" > "$BUILD/$id.argv"
  rig_argv_assert_once "$BUILD/$id.argv" "--input" || exit 1
  c="$(grep -c -- "--input poll" "$BUILD/$id.argv")" || true
  [ "$c" = 1 ] || fail "leg $id: device argv does not pin '--input poll' (the M3 binding)"
  rm -f "$BUILD/$id-launch.sh"
  cat > "$BUILD/$id-launch.sh" << EOF
#!/bin/sh
# generated by check-device-target.sh — leg launcher for $id
cd $DTMP || exit 9
rm -rf $id.apprc $id.ready $id-shots $id-persist foh.pid.$DM_NONCE app.start.ts app.end.ts
mkdir -p $id-shots
# task 13 hermeticity (iter 100): fresh tmpfs persist dir per leg
setsid sh -c 'date +%s > $DTMP/app.start.ts; MLFK_PERSIST_DIR=$DTMP/$id-persist MLFK_MENU_IMG1=$DTMP/menu.img1 ./foh_device $args \\
  2> $DTMP/$id.applog.txt & \\
  echo \$! > $DTMP/foh.pid.$DM_NONCE; \\
  wait \$!; arc=\$?; \\
  date +%s > $DTMP/app.end.ts; \\
  echo "RC=\$arc" > $DTMP/$id.apprc' \\
  </dev/null >/dev/null 2>&1 &
sleep 2
EOF
  made "$BUILD/$id-launch.sh"
  adb -s "$DEV" push "$BUILD/$id-launch.sh" "$DTMP/" >/dev/null
  hsum="$(rig_host_sha256 "$BUILD/$id-launch.sh")" || exit 1
  dsum="$(rig_dev_sha256 "$DTMP/$id-launch.sh")" || exit 1
  [ "$dsum" = "$hsum" ] || fail "pushed $id-launch.sh sha mismatch"
  dsh "chmod +x $DTMP/$id-launch.sh"

  echo "== leg $id (tverify, foh-max=$fohmax)"
  dsh "printf '' > $DTMP/qd.low_bat_check.$DM_NONCE"
  LBC_STOPPED=1
  lbc_pid="$(rig_daemon_stop low_bat_check)"
  dsh "date +%s > $DTMP/qstop.ts"
  dsh "sh -lc $DTMP/$id-launch.sh"
  ready=0
  for _ in $(seq 1 "$READY_TRIES"); do
    if dsh "test -f $DTMP/$id.ready" >/dev/null 2>&1; then ready=1; break; fi
    sleep 1
  done
  if [ "$ready" != 1 ]; then
    dsh "cat $DTMP/$id.applog.txt" >&2 || true
    fail "leg $id: app ready marker never appeared (${READY_TRIES}s)"
  fi
  echo "   app ready — playing $id.fks through fk_input"
  dsh "sh -lc 'cd $DTMP && ./fk_input $id.fks'" || fail "leg $id: fk_input injector failed"
  # bounded quiet window UNDER the app's expected end (~73 s from
  # ready: ~13 s FOH/injection + 60 s paced match), then the apprc
  # poll below closes the gap fast — the restore must start within
  # the 10 s post-slack of app exit (the quiesce bracket).
  sleep 55
  done_f=0
  for _ in $(seq 1 30); do
    if dsh "test -f $DTMP/$id.apprc" >/dev/null 2>&1; then done_f=1; break; fi
    sleep 2
  done
  if [ "$done_f" != 1 ]; then
    dsh "cat $DTMP/$id.applog.txt" >&2 || true
    fail "leg $id: app rc file never appeared"
  fi
  if rig_daemon_restore low_bat_check /etc/init.d/S12low-bat-check "$DTMP/qrestore.ts"; then
    dsh "rm -f $DTMP/qd.low_bat_check.$DM_NONCE"
    dsh "test ! -f $DTMP/qd.low_bat_check.$DM_NONCE"
    LBC_STOPPED=0
  else
    fail "leg $id: low_bat_check did not verify as running after restart"
  fi
  qstop_ts="$(rig_dev_ts "$DTMP/qstop.ts")" || exit 1
  appstart_ts="$(rig_dev_ts "$DTMP/app.start.ts")" || exit 1
  append_ts="$(rig_dev_ts "$DTMP/app.end.ts")" || exit 1
  qrestore_ts="$(rig_dev_ts "$DTMP/qrestore.ts")" || exit 1
  rig_quiesce_bracket_assert "target $id low_bat_check" \
    "$qstop_ts" "$appstart_ts" "$append_ts" "$qrestore_ts" \
    "$QW_PRE_SLACK_S" "$QW_POST_SLACK_S" || exit 1
  dsh "test -s $DTMP/foh.pid.$DM_NONCE"
  pullv "$DTMP/$id.apprc" "$BUILD/$id.apprc"
  if ! cmp -s "$BUILD/$id.apprc" <(printf 'RC=0\n'); then
    dsh "cat $DTMP/$id.applog.txt" >&2 || true
    fail "leg $id: app rc file is not EXACTLY 'RC=0<newline>' (got: '$(cat "$BUILD/$id.apprc")')"
  fi
  pullv "$DTMP/$id.trace.txt" "$BUILD/$id.dev-trace.txt"
  pullv "$DTMP/$id.applog.txt" "$BUILD/$id.dev-applog.txt"
  node "$FOH/judge-foh-trace.js" "$BUILD/$id.dev-trace.txt" "$id" 1
  rm -f "$BUILD/$id.dev-trace.norm" "$BUILD/$id.norm.expect"
  node "$FOH/normalize-foh-trace.js" "$BUILD/$id.dev-trace.txt" "$BUILD/$id.dev-trace.norm"
  node "$FOH/normalize-foh-trace.js" "$FLOWS/$id.expect" "$BUILD/$id.norm.expect"
  made "$BUILD/$id.dev-trace.norm" "$BUILD/$id.norm.expect"
  cmp "$BUILD/$id.dev-trace.norm" "$BUILD/$id.norm.expect" \
    || fail "leg $id: DEVICE trace (normalized) != frozen $FLOWS/$id.expect (normalized)"
  node "$FOH/normalize-foh-trace.js" --bounded "$FLOWS/$id.expect" \
    "$BUILD/$id.dev-trace.txt" "$FLOWS/$id.flow" "$fohmax" \
    || fail "leg $id: BOUNDED-DELTA judgment failed (mid-run stall or cadence defect)"
  nshots_want="$(printf '%s\n' ${FLOW_SHOTS[$k]} | wc -l | tr -d ' ')"
  ndev="$(dsh "ls $DTMP/$id-shots | wc -l")" || fail "leg $id: cannot enumerate device shots"
  ndev="${ndev%$'\n'}"
  ndev="$(printf '%s' "$ndev" | tr -d ' ')"
  [[ "$ndev" =~ ^(0|[1-9][0-9]{0,2})$ ]] || fail "leg $id: device shot count grammar ('$ndev')"
  [ "$ndev" = "$nshots_want" ] || fail "leg $id: device shot count $ndev != pinned $nshots_want"
  for sname in ${FLOW_SHOTS[$k]}; do
    pullv "$DTMP/$id-shots/$sname.ppm" "$BUILD/$id.dev-shot-$sname.ppm"
    judge_dev_shot "$id/$sname" "$BUILD/$id.dev-shot-$sname.ppm" \
      "$BUILD/twin-$id-a/shots/$sname.ppm"
  done
  pullv "$DTMP/$id.fbwit.txt" "$BUILD/$id.fbwit.txt"
  # shellcheck disable=SC2086
  judge_fbwit "$BUILD/$id.fbwit.txt" "$id" ${FLOW_SHOTS[$k]}
  FBWIT_TOTAL=$((FBWIT_TOTAL + nshots_want))
  parse_foh_summary "$BUILD/$id.dev-applog.txt" 1 "$nshots_want"
  [ "$foh_skips" = 0 ] || fail "leg $id: $foh_skips FOH render skips (want 0; quiesced leg)"
  [ "$foh_fails" = 0 ] || fail "leg $id: $foh_fails failed presents in the FOH phase"
  parse_audio_summary "$BUILD/$id.dev-applog.txt"
  [ "$au_underruns" = 0 ] || fail "leg $id: $au_underruns audio underruns (want 0)"
  [ "$au_badlen" = 0 ] || fail "leg $id: $au_badlen audio badlen callbacks (want 0)"
  # device starts must equal the FROZEN twin counts exactly (iter 101,
  # review-99 M3): the pin kills the both-zero self-consistency hole;
  # the twin equality keeps the device bound to the freshly-run twin.
  [ "$au_starts" = "${SFX_STARTS_PIN[$k]}" ] || fail "leg $id: device voice starts $au_starts != the frozen pin ${SFX_STARTS_PIN[$k]}"
  [ "$au_starts" = "${TWIN_STARTS[$k]}" ] || fail "leg $id: device voice starts $au_starts != twin ${TWIN_STARTS[$k]}"
  [ "$au_stops" = "${TWIN_STOPS[$k]}" ] || fail "leg $id: device voice stops $au_stops != twin ${TWIN_STOPS[$k]}"
  parse_music_summary "$BUILD/$id.dev-applog.txt"
  [ "$mu_starves" = 0 ] || fail "leg $id: $mu_starves music starves (want 0)"
  [ "$mu_out" != 0 ] || fail "leg $id: music consumed 0 output frames"
  [ "$mu_refills" != 0 ] || fail "leg $id: music refills == 0 (the SD streamer never ran)"
  # track-IDENTIFIED music evidence (iter 101, review-99 M2): the
  # counters above prove the streamer LIVED; this proves WHICH track —
  # the targettest program at the TLAUNCH seam, from the menu track,
  # naming the $DSD pcm whose sha [5] verified against the pinned host
  # bytes. A menu track that never switched can no longer pass.
  assert_mustrack "$BUILD/$id.dev-applog.txt" "$DSD" "leg $id"
  assert_no_tfinish "$BUILD/$id.dev-applog.txt"
  DEV_STARTS="$DEV_STARTS $id=$au_starts"
  pullv "$DTMP/$id.bstate.txt" "$BUILD/$id.dev-bstate.txt"
  cmp "$BUILD/$id.dev-bstate.txt" "$FLOWS/$id.bstate.expect" \
    || fail "leg $id: DEVICE TBRIDGE-STATE != frozen witness"
  pullv "$DTMP/$id.out.txt" "$BUILD/$id.dev-out.txt"
  pullv "$DTMP/$id.tim.txt" "$BUILD/$id.dev-tim.txt"
  # BOTH streams judged host-side (the claim's core)
  judge_both_streams "device $id" "$BUILD/$id.dev-out.txt" \
    "${FLOW_GID[$k]}" "${FLOW_GNAME[$k]}" "${FLOW_FRAMES[$k]}" "$BUILD"
  # timing: judge-render-timing ordered whitelist (check-device-foh class)
  rm -f "$BUILD/$id.timjudge.txt"
  node "$GFX/judge-render-timing.js" "$BUILD/$id.dev-tim.txt" "${FLOW_FRAMES[$k]}" \
    > "$BUILD/$id.timjudge.txt" || fail "leg $id: timing judgment failed"
  made "$BUILD/$id.timjudge.txt"
  ascii_text_ok "$BUILD/$id.timjudge.txt" \
    || fail "leg $id: timing judge output contains control byte(s) other than TAB/LF, or could not be measured — CORRUPT timing evidence (a NUL truncates a value mid-read, so a failing p99 parses as a passing one)"
  if ! tail -c 17 "$BUILD/$id.timjudge.txt" | cmp -s - <(printf 'judge_complete=1\n'); then
    fail "leg $id: timing judge output does not END with 'judge_complete=1'"
  fi
  TIMING_KEYS=(full_p50_ns full_p50_ms full_p99_ns full_p99_ms
    full_max_ns full_max_ms sim_p50_ns sim_p50_ms sim_p99_ns sim_p99_ms
    render_p50_ns render_p50_ms render_p99_ns render_p99_ms
    render_max_ns render_max_ms present_p50_ns present_p50_ms
    present_p99_ns present_p99_ms skips rendered judge_complete)
  unset full_p99_ns full_p99_ms skips rendered
  ji=0
  while IFS= read -r jline; do
    [ "$ji" -lt "${#TIMING_KEYS[@]}" ] || fail "leg $id: timing judge output too long"
    jk="${TIMING_KEYS[$ji]}"
    case "$jline" in
      "$jk="*) : ;;
      *) fail "leg $id: timing judge line $((ji + 1)) is '$jline' (want key '$jk')" ;;
    esac
    jv="${jline#"$jk="}"
    case "$jk" in
      judge_complete) [ "$jv" = 1 ] || fail "leg $id: judge_complete value ('$jv')" ;;
      *_ms) [[ "$jv" =~ ^(0|[1-9][0-9]{0,8})\.[0-9]{3}$ ]] || fail "leg $id: timing $jk grammar ('$jv')" ;;
      *) [[ "$jv" =~ ^(0|[1-9][0-9]{0,11})$ ]] || fail "leg $id: timing $jk grammar ('$jv')" ;;
    esac
    case "$jk" in
      full_p99_ns|full_p99_ms|skips|rendered) printf -v "$jk" '%s' "$jv" ;;
    esac
    ji=$((ji + 1))
  done < "$BUILD/$id.timjudge.txt"
  [ "$ji" = "${#TIMING_KEYS[@]}" ] || fail "leg $id: timing judge output has $ji lines"
  for jk in full_p99_ns full_p99_ms skips rendered; do
    [ -n "${!jk:-}" ] || fail "leg $id: timing judge output missing '$jk'"
  done
  [ "$full_p99_ns" -lt "$P99_FULL_LIMIT_NS" ] || fail "leg $id: p99 ${full_p99_ms} ms >= 16.67 ms"
  [ "$skips" = 0 ] || fail "leg $id: timing artifact reports $skips skips (want 0)"
  [ "$rendered" = "${FLOW_FRAMES[$k]}" ] || fail "leg $id: rendered $rendered != ${FLOW_FRAMES[$k]}"
  if [ "$full_p99_ns" -gt "$P99_WORST_NS" ]; then
    P99_WORST_NS="$full_p99_ns"
    P99_WORST_MS="$full_p99_ms"
  fi
  parse_match_summary "$BUILD/$id.dev-applog.txt" "${FLOW_FRAMES[$k]}" 1
  [ "$match_skips" = 0 ] || fail "leg $id: match summary reports $match_skips skips"
  [ "$match_fails" = 0 ] || fail "leg $id: match summary reports $match_fails failed presents"
  [ "$match_wall_ms" -ge "$WALL_MIN_MS" ] && [ "$match_wall_ms" -le "$WALL_MAX_MS" ] \
    || fail "leg $id: match wall ${match_wall_ms} ms outside [$WALL_MIN_MS,$WALL_MAX_MS]"
  echo "   -> leg $id OK (trace normalized-match + bounded, shots byte-exact + witnessed, BOTH streams verdict-exact, p99 ${full_p99_ms} ms, skips 0)"
done

# --- [6b] LIVE PLAY leg: the installed-OPK path (punch-list A2) ----------------
# THE GAP THIS CLOSES (iter 121). Every leg above drives the target plane
# through `--bridge tverify`; check-device-opk.sh drives the REAL installed
# OPK but pins a VS flow and injects no in-app input. So NOTHING exercised
# `--bridge live` + a TARGET launch — the exact play-path combination whose
# cross-guard made "Target Test" quit the app (rc 4) on the acceptance
# playthrough. This leg IS that combination, end to end: the PLAY argv shape
# (VS flow file, poll input, live bridge, mandatory recording, no --out /
# --timing), a real target launch through uinput, real target play, and the
# upstream START-quit (endGame, main.js:1013-1015) exit.
echo "== [6b/8] live play leg (bridge=live + TARGET launch + START quit) =="
LIVE_ID=f08-live-target
LIVE_FRAMES=900   # 15 s bound; the START quit must land well under it
LIVE_FOHMAX=1400
# fk script: the f06 NAV (title -> menu -> target-select -> fox -> slot 0 ->
# launch), then real gameplay (jabs — deliberately NO movement, so fox
# cannot self-destruct and switch which exit arm runs), then START.
# Derived x2, byte-stable.
# The `q` marker presses the generator emits are no-ops here BY DESIGN: this
# leg runs without --shots-dir, exactly like the OPK play path.
rm -f "$BUILD/$LIVE_ID.fks" "$BUILD/$LIVE_ID.fks.b"
for v in "" ".b"; do
  node "$FOH/flow-to-fkscript.js" "$FLOWS/f06-target-t01.flow" \
    "$BUILD/$LIVE_ID.fks$v" >/dev/null || fail "live leg: fk script gen failed"
  printf 's 1500\nd a\ns 120\nu a\ns 900\nd a\ns 120\nu a\ns 2600\n' \
    >> "$BUILD/$LIVE_ID.fks$v"
  printf 'd s\ns 100\nu s\ns 2000\n' >> "$BUILD/$LIVE_ID.fks$v"
done
cmp -s "$BUILD/$LIVE_ID.fks" "$BUILD/$LIVE_ID.fks.b" \
  || fail "live leg: fk script not byte-stable x2"
made "$BUILD/$LIVE_ID.fks"
# the PLAY path's flow file is a VS one — in poll mode the flow supplies only
# the shot schedule and firstInputFrame, so the LAUNCH KIND comes from real
# input. Pushing it makes this leg a true mirror of the installed OPK argv.
adb -s "$DEV" push "$FLOWS/f01-vs-g01.flow" "$DTMP/" >/dev/null
adb -s "$DEV" push "$BUILD/$LIVE_ID.fks" "$DTMP/" >/dev/null
hsum="$(rig_host_sha256 "$FLOWS/f01-vs-g01.flow")" || exit 1
dsum="$(rig_dev_sha256 "$DTMP/f01-vs-g01.flow")" || exit 1
[ "$dsum" = "$hsum" ] || fail "live leg: pushed f01-vs-g01.flow sha mismatch"
hsum="$(rig_host_sha256 "$BUILD/$LIVE_ID.fks")" || exit 1
dsum="$(rig_dev_sha256 "$DTMP/$LIVE_ID.fks")" || exit 1
[ "$dsum" = "$hsum" ] || fail "live leg: pushed $LIVE_ID.fks sha mismatch"

largs="--flow $DTMP/f01-vs-g01.flow --input poll --flow-out $DTMP/$LIVE_ID.trace.txt"
largs="$largs --ready-file $DTMP/$LIVE_ID.ready --foh-max $LIVE_FOHMAX"
largs="$largs --pace 1 --budget-ns $BUDGET_NS"
largs="$largs --sndpack $DSD/sndpack.bin --music-manifest $DTMP/mus-dev.txt"
largs="$largs --bridge live --simdata $DTMP/simdata.txt --seed 1337"
largs="$largs --bstate-out $DTMP/$LIVE_ID.bstate.txt --frames $LIVE_FRAMES"
largs="$largs --record-trace $DTMP/$LIVE_ID.rec.json"
largs="$largs --record-keys $DTMP/$LIVE_ID.keys.txt"
largs="$largs --gfxdata $DTMP/gfxdata-frozen.txt --vfxdata $DTMP/vfxdata-frozen.txt"
largs="$largs --glyphs $DTMP/vfxglyphs-frozen.txt --anim-dir $DTMP --legible"
largs="$largs --tapjump-off-p1"
rm -f "$BUILD/$LIVE_ID.argv"
printf '%s\n' "$largs" > "$BUILD/$LIVE_ID.argv"
rig_argv_assert_once "$BUILD/$LIVE_ID.argv" "--input" || exit 1
c="$(grep -c -- "--bridge live" "$BUILD/$LIVE_ID.argv")" || true
[ "$c" = 1 ] || fail "live leg: argv does not pin '--bridge live'"
# the live contract: NO evidence sinks (they are argv-rejected for live)
for forbidden in "--out " "--timing " "--trace "; do
  if grep -q -- "$forbidden" "$BUILD/$LIVE_ID.argv"; then
    fail "live leg: argv carries $forbidden (rejected for --bridge live)"
  fi
done

rm -f "$BUILD/$LIVE_ID-launch.sh"
cat > "$BUILD/$LIVE_ID-launch.sh" << EOF
#!/bin/sh
# generated by check-device-target.sh — live play leg launcher
cd $DTMP || exit 9
rm -rf $LIVE_ID.apprc $LIVE_ID.ready $LIVE_ID-persist foh.pid.$DM_NONCE
rm -f app.start.ts app.end.ts
rm -f $LIVE_ID.trace.txt $LIVE_ID.bstate.txt $LIVE_ID.rec.json $LIVE_ID.keys.txt
setsid sh -c 'date +%s > $DTMP/app.start.ts; MLFK_PERSIST_DIR=$DTMP/$LIVE_ID-persist MLFK_MENU_IMG1=$DTMP/menu.img1 ./foh_device $largs \\
  2> $DTMP/$LIVE_ID.applog.txt & \\
  echo \$! > $DTMP/foh.pid.$DM_NONCE; \\
  wait \$!; arc=\$?; \\
  date +%s > $DTMP/app.end.ts; \\
  echo "RC=\$arc" > $DTMP/$LIVE_ID.apprc' \\
  </dev/null >/dev/null 2>&1 &
sleep 2
EOF
made "$BUILD/$LIVE_ID-launch.sh"
adb -s "$DEV" push "$BUILD/$LIVE_ID-launch.sh" "$DTMP/" >/dev/null
hsum="$(rig_host_sha256 "$BUILD/$LIVE_ID-launch.sh")" || exit 1
dsum="$(rig_dev_sha256 "$DTMP/$LIVE_ID-launch.sh")" || exit 1
[ "$dsum" = "$hsum" ] || fail "live leg: pushed launcher sha mismatch"
dsh "chmod +x $DTMP/$LIVE_ID-launch.sh"

dsh "printf '' > $DTMP/qd.low_bat_check.$DM_NONCE"
LBC_STOPPED=1
lbc_pid="$(rig_daemon_stop low_bat_check)"
dsh "date +%s > $DTMP/qstop.ts"
dsh "sh -lc $DTMP/$LIVE_ID-launch.sh"
ready=0
for _ in $(seq 1 "$READY_TRIES"); do
  if dsh "test -f $DTMP/$LIVE_ID.ready" >/dev/null 2>&1; then ready=1; break; fi
  sleep 1
done
if [ "$ready" != 1 ]; then
  dsh "cat $DTMP/$LIVE_ID.applog.txt" >&2 || true
  fail "live leg: app ready marker never appeared (${READY_TRIES}s)"
fi
echo "   app ready — playing $LIVE_ID.fks through fk_input"
dsh "sh -lc 'cd $DTMP && ./fk_input $LIVE_ID.fks'" \
  || fail "live leg: fk_input injector failed"
done_f=0
for _ in $(seq 1 45); do
  if dsh "test -f $DTMP/$LIVE_ID.apprc" >/dev/null 2>&1; then done_f=1; break; fi
  sleep 2
done
if [ "$done_f" != 1 ]; then
  dsh "cat $DTMP/$LIVE_ID.applog.txt" >&2 || true
  fail "live leg: app rc file never appeared"
fi
if rig_daemon_restore low_bat_check /etc/init.d/S12low-bat-check "$DTMP/qrestore.ts"; then
  dsh "rm -f $DTMP/qd.low_bat_check.$DM_NONCE"
  dsh "test ! -f $DTMP/qd.low_bat_check.$DM_NONCE"
  LBC_STOPPED=0
else
  fail "live leg: low_bat_check did not verify as running after restart"
fi

lqstop_ts="$(rig_dev_ts "$DTMP/qstop.ts")" || exit 1
lappstart_ts="$(rig_dev_ts "$DTMP/app.start.ts")" || exit 1
lappend_ts="$(rig_dev_ts "$DTMP/app.end.ts")" || exit 1
lqrestore_ts="$(rig_dev_ts "$DTMP/qrestore.ts")" || exit 1
rig_quiesce_bracket_assert "target $LIVE_ID low_bat_check" \
  "$lqstop_ts" "$lappstart_ts" "$lappend_ts" "$lqrestore_ts" \
  "$QW_PRE_SLACK_S" "$QW_POST_SLACK_S" || exit 1
pullv "$DTMP/$LIVE_ID.apprc"       "$BUILD/$LIVE_ID.apprc"       || exit 1
pullv "$DTMP/$LIVE_ID.rec.json"    "$BUILD/$LIVE_ID.rec.json"    || exit 1
pullv "$DTMP/$LIVE_ID.applog.txt"  "$BUILD/$LIVE_ID.applog.txt"  || exit 1
pullv "$DTMP/$LIVE_ID.trace.txt"   "$BUILD/$LIVE_ID.trace.txt"   || exit 1
pullv "$DTMP/$LIVE_ID.keys.txt"    "$BUILD/$LIVE_ID.keys.txt"    || exit 1
pullv "$DTMP/$LIVE_ID.bstate.txt"  "$BUILD/$LIVE_ID.bstate.txt"  || exit 1
# (1) the app must EXIT CLEANLY — rc 4 here is the A2 cross-guard regression
printf 'RC=0\n' | cmp -s - "$BUILD/$LIVE_ID.apprc" \
  || fail "live leg: apprc != RC=0 (got '$(cat "$BUILD/$LIVE_ID.apprc")') — a
  TARGET launch under --bridge live must PLAY, not refuse (punch-list A2)"
# (2) a real TARGET launch happened through the polled input path
grep -qE '^TLAUNCH [0-9]+ char=2 tstage=0$' "$BUILD/$LIVE_ID.trace.txt" \
  || fail "live leg: no TLAUNCH char=2 tstage=0 in the FOH trace"
# (3) the targettest music switch fired at the launch seam
grep -qE '^foh_dev mustrack: from=menu to=targettest on=1 ' \
  "$BUILD/$LIVE_ID.applog.txt" \
  || fail "live leg: targettest music switch missing from the applog"
# (4) TBRIDGE-STATE witness reads back a live target match
grep -qE '^TBRIDGE-STATE char=2 tstage=0 gamemode=5 targets=10 playing=1 ' \
  "$BUILD/$LIVE_ID.bstate.txt" || fail "live leg: TBRIDGE-STATE witness wrong"
# (4b) the FOH options plane reached the target sim: --tapjump-off-p1 is
#      the S1 contract the launcher always passes, and tp_setup_target
#      zeroes it, so this is the tooth for the live-only reapplication.
grep -qE '^TBRIDGE-OPTS turbo=[01] lcancel=[0-9]+ tapjump=1,0,0,0$' \
  "$BUILD/$LIVE_ID.bstate.txt" \
  || fail "live leg: TBRIDGE-OPTS missing/wrong — the FOH options plane did
  not reach the target sim (tapjump=1,0,0,0 expected from --tapjump-off-p1)"
# (5) START ENDED the match: strictly fewer frames than the bound, and enough
#     frames that real play happened. This is the endGame hook — with the
#     default NULL hook target_play.c traps and the app would die instead.
N12='(0|[1-9][0-9]{0,11})'
lre="^foh_dev match: ${N12} frames, ${N12} render skips, ${N12} failed"
lre="$lre presents, wall ${N12} ms, pace=1 budget=${BUDGET_NS} ns$"
lc="$(grep_count "$lre" "$BUILD/$LIVE_ID.applog.txt" "live match summary")"
[ "$lc" = 1 ] \
  || grammar_die "live leg: $lc match-summary grammar matches (want 1)"
mline="$(grep -E "$lre" "$BUILD/$LIVE_ID.applog.txt")" \
  || fail "live leg: no match summary line"
lf="$(printf '%s' "$mline" | awk '{print $3}')"
[[ "$lf" =~ ^[1-9][0-9]*$ ]] || fail "live leg: match frame count grammar ('$lf')"
[ "$lf" -lt "$LIVE_FRAMES" ] \
  || fail "live leg: match ran the full $LIVE_FRAMES frames — START did not end it"
[ "$lf" -ge 100 ] || fail "live leg: match only $lf frames — no real play happened"
# (6) the recording is exactly the frames that ran (START row rolled back)
kn="$(wc -l < "$BUILD/$LIVE_ID.keys.txt" | tr -d ' ')"
# the START frame IS ticked/rendered/paced but is NOT in the replay
# prefix, so the recording is exactly one row shorter than the run.
[ "$kn" = "$((lf - 1))" ] \
  || fail "live leg: recorded key rows ($kn) != match frames-1 ($((lf - 1)))"
rn="$(grep -c -- ",null,null,null\]" "$BUILD/$LIVE_ID.rec.json")" || true
[ "$rn" = "$kn" ] \
  || fail "live leg: --record-trace rows ($rn) != --record-keys rows ($kn)"
# the capture must contain REAL input, not a constant neutral row: the
# injected jabs have to show up, or the recorder is not recording.
grep -q '"a":true' "$BUILD/$LIVE_ID.rec.json" \
  || fail "live leg: no pressed field in --record-trace — the recorder
  emitted only neutral rows despite injected gameplay"
# SHAPE GUARD (not a tooth — rec_frame_solo hardcodes the 1-slot row, so
# no reachable input fails this today; it guards future recorder edits):
# a non-null slot 1-3 would make the capture unreplayable via tverify.
if grep -qE "\\{[^]]*\\},[^n]" "$BUILD/$LIVE_ID.rec.json"; then
  fail "live leg: --record-trace carries a non-null slot 1-3 row"
fi
# (7) real-time health on the play path
# field map of the pinned grammar:
#   1 foh_dev  2 match:  3 <frames>  4 frames,  5 <skips>  6 render
#   7 skips,   8 <failedpresents>    9 failed   10 presents,
lskips="$(printf '%s' "$mline" | awk '{print $5}')"
lpf="$(printf '%s' "$mline" | awk '{print $8}')"
[[ "$lskips" =~ ^(0|[1-9][0-9]*)$ ]] || fail "live leg: skips grammar ('$lskips')"
[[ "$lpf" =~ ^(0|[1-9][0-9]*)$ ]] || fail "live leg: present-fail grammar ('$lpf')"
[ "$lskips" = 0 ] || fail "live leg: $lskips render skips (expected 0)"
[ "$lpf" = 0 ] || fail "live leg: $lpf failed presents (expected 0)"
# POSITIVE liveness first: '0 callbacks, 0 underruns' would otherwise pass
# a completely silent run.
are="^foh_dev audio: [1-9][0-9]{0,11} callbacks, 0 underruns, 0 badlen,"
are="$are ${N12} voice starts, ${N12} voice stops, ${N12} steals,"
are="$are rate=44100 samples=512 channels=2$"
ac="$(grep_count "$are" "$BUILD/$LIVE_ID.applog.txt" "live audio summary")"
[ "$ac" = 1 ] \
  || grammar_die "live leg: $ac audio-summary grammar matches (want 1)"
mre="^foh_dev music: [1-9][0-9]{0,11} out frames, 0 starves,"
mre="$mre [1-9][0-9]{0,11} refills, ring=32768 chunk=16384$"
mc="$(grep_count "$mre" "$BUILD/$LIVE_ID.applog.txt" "live music summary")"
[ "$mc" = 1 ] \
  || grammar_die "live leg: $mc music-summary grammar matches (want 1)"
echo "   -> live leg OK (TLAUNCH char=2 tstage=0, targettest music, $lf frames"
echo "      ended by START via tp_endgame_hook, $kn recorded rows, skips 0,"
echo "      underruns 0, starves 0)"

# --- [6c] live play leg 2: the ORDINARY --frames bound exit ------------------
# WHY A SECOND LEG (review-121 fallback, Opus 5 MEDIUM): leg [6b] asserts
# `rows == frames - 1`, an identity that holds ONLY for the START-quit exit.
# The other live exits record `rows == frames`, so [6b] STRUCTURALLY excludes
# them and their counter arithmetic was judged nowhere. This leg drives the
# ordinary --frames bound (no START press at all) and pins `rows == frames ==
# the bound`, so both accountings are covered.
# STILL UNCOVERED, and NOT claimed by this leg's verdict token: the FINISH
# arm (TFIN tail + music stop + per-frame banner composite), which needs all
# 10 targets destroyed — unscriptable through fk_input. Registered for the
# driver under BLOCKERS rather than implied covered here.
echo "== [6c/8] live play leg 2 (bridge=live + TARGET launch + --frames bound) =="
BND_ID=f09-live-bound
BND_FRAMES=240   # 4 s of target play, then the ordinary bound ends the match
rm -f "$BUILD/$BND_ID.fks" "$BUILD/$BND_ID.fks.b"
for v in "" ".b"; do
  node "$FOH/flow-to-fkscript.js" "$FLOWS/f06-target-t01.flow" \
    "$BUILD/$BND_ID.fks$v" >/dev/null || fail "bound leg: fk script gen failed"
  # jabs only, then WAIT: the app must end itself on the frame bound.
  printf 's 1200\nd a\ns 120\nu a\ns 9000\n' >> "$BUILD/$BND_ID.fks$v"
done
cmp -s "$BUILD/$BND_ID.fks" "$BUILD/$BND_ID.fks.b" \
  || fail "bound leg: fk script not byte-stable x2"
made "$BUILD/$BND_ID.fks"
adb -s "$DEV" push "$BUILD/$BND_ID.fks" "$DTMP/" >/dev/null
hsum="$(rig_host_sha256 "$BUILD/$BND_ID.fks")" || exit 1
dsum="$(rig_dev_sha256 "$DTMP/$BND_ID.fks")" || exit 1
[ "$dsum" = "$hsum" ] || fail "bound leg: pushed $BND_ID.fks sha mismatch"

bargs="--flow $DTMP/f01-vs-g01.flow --input poll --flow-out $DTMP/$BND_ID.trace.txt"
bargs="$bargs --ready-file $DTMP/$BND_ID.ready --foh-max $LIVE_FOHMAX"
bargs="$bargs --pace 1 --budget-ns $BUDGET_NS"
bargs="$bargs --sndpack $DSD/sndpack.bin --music-manifest $DTMP/mus-dev.txt"
bargs="$bargs --bridge live --simdata $DTMP/simdata.txt --seed 1337"
bargs="$bargs --bstate-out $DTMP/$BND_ID.bstate.txt --frames $BND_FRAMES"
bargs="$bargs --record-trace $DTMP/$BND_ID.rec.json"
bargs="$bargs --record-keys $DTMP/$BND_ID.keys.txt"
bargs="$bargs --gfxdata $DTMP/gfxdata-frozen.txt --vfxdata $DTMP/vfxdata-frozen.txt"
bargs="$bargs --glyphs $DTMP/vfxglyphs-frozen.txt --anim-dir $DTMP --legible"
bargs="$bargs --tapjump-off-p1"
rm -f "$BUILD/$BND_ID-launch.sh"
cat > "$BUILD/$BND_ID-launch.sh" << EOF
#!/bin/sh
# generated by check-device-target.sh — live bound leg launcher
cd $DTMP || exit 9
rm -rf $BND_ID.apprc $BND_ID.ready $BND_ID-persist foh.pid.$DM_NONCE
rm -f app.start.ts app.end.ts
rm -f $BND_ID.trace.txt $BND_ID.bstate.txt $BND_ID.rec.json $BND_ID.keys.txt
setsid sh -c 'date +%s > $DTMP/app.start.ts; MLFK_PERSIST_DIR=$DTMP/$BND_ID-persist MLFK_MENU_IMG1=$DTMP/menu.img1 ./foh_device $bargs \\
  2> $DTMP/$BND_ID.applog.txt & \\
  echo \$! > $DTMP/foh.pid.$DM_NONCE; \\
  wait \$!; arc=\$?; \\
  date +%s > $DTMP/app.end.ts; \\
  echo "RC=\$arc" > $DTMP/$BND_ID.apprc' \\
  </dev/null >/dev/null 2>&1 &
sleep 2
EOF
made "$BUILD/$BND_ID-launch.sh"
adb -s "$DEV" push "$BUILD/$BND_ID-launch.sh" "$DTMP/" >/dev/null
hsum="$(rig_host_sha256 "$BUILD/$BND_ID-launch.sh")" || exit 1
dsum="$(rig_dev_sha256 "$DTMP/$BND_ID-launch.sh")" || exit 1
[ "$dsum" = "$hsum" ] || fail "bound leg: pushed launcher sha mismatch"
dsh "chmod +x $DTMP/$BND_ID-launch.sh"

dsh "printf '' > $DTMP/qd.low_bat_check.$DM_NONCE"
LBC_STOPPED=1
lbc_pid="$(rig_daemon_stop low_bat_check)"
dsh "date +%s > $DTMP/qstop.ts"
dsh "sh -lc $DTMP/$BND_ID-launch.sh"
ready=0
for _ in $(seq 1 "$READY_TRIES"); do
  if dsh "test -f $DTMP/$BND_ID.ready" >/dev/null 2>&1; then ready=1; break; fi
  sleep 1
done
[ "$ready" = 1 ] || fail "bound leg: app ready marker never appeared"
echo "   app ready — playing $BND_ID.fks through fk_input"
dsh "sh -lc 'cd $DTMP && ./fk_input $BND_ID.fks'" \
  || fail "bound leg: fk_input injector failed"
done_f=0
for _ in $(seq 1 45); do
  if dsh "test -f $DTMP/$BND_ID.apprc" >/dev/null 2>&1; then done_f=1; break; fi
  sleep 2
done
if [ "$done_f" != 1 ]; then
  dsh "cat $DTMP/$BND_ID.applog.txt" >&2 || true
  fail "bound leg: app rc file never appeared"
fi
if rig_daemon_restore low_bat_check /etc/init.d/S12low-bat-check "$DTMP/qrestore.ts"; then
  dsh "rm -f $DTMP/qd.low_bat_check.$DM_NONCE"
  dsh "test ! -f $DTMP/qd.low_bat_check.$DM_NONCE"
  LBC_STOPPED=0
else
  fail "bound leg: low_bat_check did not verify as running after restart"
fi
bqstop_ts="$(rig_dev_ts "$DTMP/qstop.ts")" || exit 1
bappstart_ts="$(rig_dev_ts "$DTMP/app.start.ts")" || exit 1
bappend_ts="$(rig_dev_ts "$DTMP/app.end.ts")" || exit 1
bqrestore_ts="$(rig_dev_ts "$DTMP/qrestore.ts")" || exit 1
rig_quiesce_bracket_assert "target $BND_ID low_bat_check" \
  "$bqstop_ts" "$bappstart_ts" "$bappend_ts" "$bqrestore_ts" \
  "$QW_PRE_SLACK_S" "$QW_POST_SLACK_S" || exit 1
pullv "$DTMP/$BND_ID.apprc"      "$BUILD/$BND_ID.apprc"      || exit 1
pullv "$DTMP/$BND_ID.applog.txt" "$BUILD/$BND_ID.applog.txt" || exit 1
pullv "$DTMP/$BND_ID.trace.txt"  "$BUILD/$BND_ID.trace.txt"  || exit 1
pullv "$DTMP/$BND_ID.keys.txt"   "$BUILD/$BND_ID.keys.txt"   || exit 1
printf 'RC=0\n' | cmp -s - "$BUILD/$BND_ID.apprc" \
  || fail "bound leg: apprc != RC=0 (got '$(cat "$BUILD/$BND_ID.apprc")')"
grep -qE '^TLAUNCH [0-9]+ char=2 tstage=0$' "$BUILD/$BND_ID.trace.txt" \
  || fail "bound leg: no TLAUNCH char=2 tstage=0 in the FOH trace"
bre="^foh_dev match: ${N12} frames, ${N12} render skips, ${N12} failed"
bre="$bre presents, wall ${N12} ms, pace=1 budget=${BUDGET_NS} ns$"
bc="$(grep_count "$bre" "$BUILD/$BND_ID.applog.txt" "bound match summary")"
[ "$bc" = 1 ] \
  || grammar_die "bound leg: $bc match-summary grammar matches (want 1)"
bmline="$(grep -E "$bre" "$BUILD/$BND_ID.applog.txt")"
lf2="$(printf '%s' "$bmline" | awk '{print $3}')"
# the ORDINARY bound: the match ends at exactly --frames, and because no
# START row is rolled back the recording is the SAME length as the run.
[ "$lf2" = "$BND_FRAMES" ] \
  || fail "bound leg: match ran $lf2 frames, want exactly $BND_FRAMES"
kn2="$(wc -l < "$BUILD/$BND_ID.keys.txt" | tr -d ' ')"
[ "$kn2" = "$lf2" ] \
  || fail "bound leg: recorded rows ($kn2) != match frames ($lf2)"
bskips="$(printf '%s' "$bmline" | awk '{print $5}')"
bpf="$(printf '%s' "$bmline" | awk '{print $8}')"
[ "$bskips" = 0 ] || fail "bound leg: $bskips render skips (expected 0)"
[ "$bpf" = 0 ] || fail "bound leg: $bpf failed presents (expected 0)"
echo "   -> bound leg OK ($lf2 frames == --frames bound, $kn2 recorded rows,"
echo "      no START rollback, skips 0)"

# --- park restore + deadman cancel --------------------------------------------
dsh "rm -f /mnt/disable_frontend"
dsh "test ! -f /mnt/disable_frontend"
PARKED=0
dsh "touch $DTMP/deadman.cancel"
dmgone=0
for _ in $(seq 1 6); do
  if dsh "test ! -f $DTMP/deadman.pid" >/dev/null 2>&1; then dmgone=1; break; fi
  sleep 2
done
[ "$dmgone" = 1 ] || fail "park deadman did not exit within 12s of cancellation"
dsh "test ! -f $DTMP/deadman.fired" >/dev/null 2>&1 \
  || fail "park deadman FIRED during a healthy run"
DEADMAN_ARMED=0
echo "   frontend restored; deadman cancelled without firing"

# --- [7] teeth (COPIES only) --------------------------------------------------
echo "== [7/8] teeth (host-side, on COPIES; committed bytes never edited) =="
teeth=0
# T1 — A/B-swap flow variant (the key-translation kill chain, host twin):
# swapped letters on f06's I rows -> the normalized judge MUST diverge.
rm -rf "$BUILD/tooth-abswap"
mkdir -p "$BUILD/tooth-abswap/shots"
node -e '
  const fs = require("fs");
  const [src, dst] = process.argv.slice(1);
  const out = fs.readFileSync(src, "utf8").split("\n").map((ln) => {
    const m = /^I ([0-9]+) ([-A-Z]+)$/.exec(ln);
    if (!m || m[2] === "-") return ln;
    const tok = [...m[2]].map((c) => (c === "A" ? "B" : c === "B" ? "A" : c)).join("");
    return "I " + m[1] + " " + tok;
  }).join("\n");
  fs.writeFileSync(dst, out);
' "$FLOWS/f06-target-t01.flow" "$BUILD/tooth-abswap/f06-target-t01.flow"
made "$BUILD/tooth-abswap/f06-target-t01.flow"
cmp -s "$BUILD/tooth-abswap/f06-target-t01.flow" "$FLOWS/f06-target-t01.flow" && \
  fail "T1: swap variant is byte-identical to the committed flow (dead tooth)"
MLFK_PERSIST_DIR="$PWD/$BUILD/tooth-abswap/persist" \
"$BUILD/foh_dev_headless" --flow "$BUILD/tooth-abswap/f06-target-t01.flow" \
  --input flow --flow-out "$BUILD/tooth-abswap/trace.txt" \
  --shots-dir "$BUILD/tooth-abswap/shots" --pace 0 \
  2> "$BUILD/tooth-abswap/log.txt" || fail "T1: variant run failed outright"
rm -f "$BUILD/tooth-abswap/trace.norm"
node "$FOH/normalize-foh-trace.js" "$BUILD/tooth-abswap/trace.txt" \
  "$BUILD/tooth-abswap/trace.norm"
rc=0
cmp -s "$BUILD/tooth-abswap/trace.norm" "$BUILD/f06-target-t01.norm.expect" || rc=$?
[ "$rc" = 1 ] || fail "T1: A/B-swap variant normalized cmp rc $rc (want exactly 1)"
teeth=$((teeth + 1))
echo "    T1 OK: A/B swap dies against the frozen f06 trace (normalized judge)"
# T2 — device T-line nibble: flip a target-plane hash in a COPY of the
# pulled device sim-out -> verify-target-stream must diverge.
cp "$BUILD/f06-target-t01.dev-out.txt" "$BUILD/tooth2.out"
python3 - "$BUILD/tooth2.out" <<'PY'
import sys, re
p=sys.argv[1]; L=open(p).read().splitlines()
for i,l in enumerate(L):
    m=re.match(r'^T 1800 ([0-9a-f]{64})$', l)
    if m:
        h=list(m.group(1)); h[0]='0' if h[0]!='0' else '1'; L[i]='T 1800 '+''.join(h); break
else: sys.exit("T2: T 1800 line not found")
open(p,'w').write('\n'.join(L)+'\n')
PY
node "$M4G/wrap-target.js" t01 "$BUILD/tooth2.out" \
  "$BUILD/tooth2.player.json" "$BUILD/tooth2.target.json" >/dev/null
rc=0
node "$M4G/verify-target-stream.js" "$BUILD/tooth2.target.json" \
  "$M4G/$T01_NAME.target.sha256.json" >/dev/null 2>&1 || rc=$?
[ "$rc" = 2 ] || fail "T2: target-plane nibble rc $rc (want the divergence class 2)"
teeth=$((teeth + 1))
echo "    T2 OK: device T-line nibble dies in verify-target-stream (rc 2)"
# T3 — TFIN perturb in a device-out COPY -> the finals pin dies with
# the SEMANTIC divergence class (rc 2) + the EXACT named finals-pin
# diagnostic (iter 101, review-99 L3): a bare nonzero — crash, usage
# error, unrelated earlier death — no longer passes as a live tooth.
cp "$BUILD/f06-target-t01.dev-out.txt" "$BUILD/tooth3.out"
sed -i.bak -E 's/^TFIN [0-9]+ /TFIN 9 /' "$BUILD/tooth3.out"; rm -f "$BUILD/tooth3.out.bak"
cmp -s "$BUILD/tooth3.out" "$BUILD/f06-target-t01.dev-out.txt" && \
  fail "T3: TFIN perturb was a no-op (dead tooth)"
t3_tfin="$(node -e '
  const j = JSON.parse(require("fs").readFileSync(process.argv[1], "utf8"));
  console.log(String(j.params.finalTargetsDestroyed));
' "$M4G/$T01_NAME.target.sha256.json")" || fail "T3: cannot read the frozen finalTargetsDestroyed"
[[ "$t3_tfin" =~ ^(0|[1-9][0-9]?)$ ]] || fail "T3: frozen finalTargetsDestroyed grammar ('$t3_tfin')"
[ "$t3_tfin" != 9 ] || fail "T3: frozen finals == the perturb value 9 (dead tooth — pick a new perturb value, reviewed change)"
rc=0
rm -f "$BUILD/tooth3.vlog"
node "$M4G/wrap-target.js" t01 "$BUILD/tooth3.out" \
  "$BUILD/tooth3.player.json" "$BUILD/tooth3.target.json" >/dev/null
node "$M4G/verify-target-stream.js" "$BUILD/tooth3.target.json" \
    "$M4G/$T01_NAME.target.sha256.json" > "$BUILD/tooth3.vlog" 2>&1 || rc=$?
made "$BUILD/tooth3.vlog"
[ "$rc" = 2 ] || fail "T3: TFIN perturb rc $rc (want EXACTLY the semantic divergence class 2)"
c="$(grep -cxF "TARGET STREAM MISMATCH: final targetsDestroyed 9 != frozen $t3_tfin" "$BUILD/tooth3.vlog")" || true
[ "$c" = 1 ] || fail "T3: the exact finals-pin diagnostic is absent (want 'TARGET STREAM MISMATCH: final targetsDestroyed 9 != frozen $t3_tfin' exactly once; got $c — the death came from somewhere else)"
teeth=$((teeth + 1))
echo "    T3 OK: TFIN perturb dies at the run-finals binding (rc 2 + the exact diagnostic)"
# T4 — corrupted device-shot COPY -> the production shot judge dies.
cp "$BUILD/f06-target-t01.dev-shot-tss-t01.ppm" "$BUILD/tooth4.ppm"
printf 'x' >> "$BUILD/tooth4.ppm"
rc=0
( judge_dev_shot "tooth4" "$BUILD/tooth4.ppm" \
    "$BUILD/twin-f06-target-t01-a/shots/tss-t01.ppm" ) 2>/dev/null || rc=$?
[ "$rc" != 0 ] || fail "T4: the shot judge accepted a corrupted device-shot copy"
teeth=$((teeth + 1))
echo "    T4 OK: corrupted device-shot copy dies in the production judge"
# T5 — TBRIDGE-STATE perturb (COPY) -> the frozen cmp dies.
sed 's/tstage=0/tstage=9/' "$BUILD/f06-target-t01.dev-bstate.txt" > "$BUILD/tooth5.txt"
cmp -s "$BUILD/tooth5.txt" "$BUILD/f06-target-t01.dev-bstate.txt" && \
  fail "T5: substitution was a no-op (dead tooth)"
rc=0
cmp -s "$BUILD/tooth5.txt" "$FLOWS/f06-target-t01.bstate.expect" || rc=$?
[ "$rc" = 1 ] || fail "T5: perturbed TBRIDGE-STATE copy cmp rc $rc (want 1)"
teeth=$((teeth + 1))
echo "    T5 OK: perturbed TBRIDGE-STATE copy dies against the frozen witness"
# T6 — fb-witness eq=0 COPY -> the witness judge dies.
sed 's/ eq=1$/ eq=0/' "$BUILD/f06-target-t01.fbwit.txt" > "$BUILD/tooth6.txt"
cmp -s "$BUILD/tooth6.txt" "$BUILD/f06-target-t01.fbwit.txt" && \
  fail "T6: eq substitution was a no-op (dead tooth)"
rc=0
# shellcheck disable=SC2086
( judge_fbwit "$BUILD/tooth6.txt" f06-target-t01 ${FLOW_SHOTS[0]} ) 2>/dev/null || rc=$?
[ "$rc" != 0 ] || fail "T6: the witness judge accepted eq=0 rows"
teeth=$((teeth + 1))
echo "    T6 OK: fb-witness eq=0 copy dies in the witness judge"

# --- [8] hygiene + verdict ----------------------------------------------------
echo "== [8/8] hygiene =="
rig_no_commit_guard "$BUILD" "$DEVB" "$TABLES" "$AUDIO_OUT"

# verdict evidence tokens (iter 101, review-99 M2/M3): sfxpin = the
# frozen per-flow start counts every leg matched; music = the
# track-identified transition each of the MUSTRACK_LEGS legs witnessed.
[ "$MUSTRACK_LEGS" = 5 ] || fail "mustrack leg coverage $MUSTRACK_LEGS != 5 (3 twins + 2 device legs must all be scanned)"
echo "DEVICE TARGET CONFORMS (goldens=2 flows=2 shots=4 fbwit=$FBWIT_TOTAL p99=${P99_WORST_MS}ms skips=0 underruns=0 starves=0 starts${DEV_STARTS} sfxpin=${SFX_STARTS_PIN[0]}/${SFX_STARTS_PIN[1]} music=menu>targettest:$MUSTRACK_LEGS/5 live=$LIVE_ID:${lf}f/${kn}rows/opts-ok bound=$BND_ID:${lf2}f/${kn2}rows teeth=$teeth)"
