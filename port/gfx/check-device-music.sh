#!/usr/bin/env bash
# check-device-music.sh — M4 task 7 done-check: MUSIC STREAMING ON THE
# FunKey-S (fix_plan §M4 task 7; pre-registration AGENT-LOG iter 87).
# Prints
#   DEVICE MUSIC OK (full p99 X ms, underruns 0, starves 0,
#     refill-read p99 Y ms, skips N/3600)
# exit 0, iff ALL of:
#
#   [1] HOST FIDELITY — bash port/gfx/check-music-fidelity.sh (composed;
#       the 12-golden + 3-synthetic-leg offline music differential, all
#       teeth). This script holds NO shared-scratch lock itself — the
#       composed child takes its own locks (mixer-check composition
#       pattern).
#   [2] DATA PLANE — g01 strict params, fresh M1 tables/simdata/trace,
#       the committed GFXDATA/VFXDATA/VFXGLYPHS pins + legibility twin
#       pin (the render check's constants, cross-pinned), the child's
#       fresh audio build REUSED (sounds.json + battlefield.pcm
#       made()-asserted; the battlefield PCM sha pinned here AND
#       cross-grepped against check-music-fidelity.sh's pin table row —
#       one music identity across the surface), pack REBUILT x2 +
#       pinned (verdicts judged exact-line, review-87 L1), battlefield
#       music cfg extracted by strict whitelist AND asserted against
#       the FROZEN metadata pins (volbits + sprite windows, review-87
#       H1a; the fidelity check's battlefield meta row cross-grepped —
#       one metadata identity across the surface; perturbed-volbits
#       tooth standing).
#   [3] HOST TRUTH — x2 headless replays WITH --sndpack + --music
#       (--pace 0) produce byte-identical streams; STREAM MATCH vs
#       frozen g01 (UNCHANGED verify-stream.js); app summary 0 skips /
#       0 failed presents; audio summary rate=0/samples=0/channels=0
#       (headless never fakes a device spec); music summary EXACTLY
#       `0 out frames, 0 starves, 0 refills` (no callback consumes on
#       headless — honest plumbing-only leg); PARSER TEETH: a crafted
#       duplicate music-summary line, a crafted starves=1 line, a
#       ring-constant-drift line, a leading-zero counter, and a log
#       missing its final newline all die in the strict parser/gate
#       (exact-token grammar, review-87 M3); T-WEDGE: a
#       --tooth-music-wedge run (reader ignores quit) dies LOUD at the
#       app's bounded join deadline, never hangs (review-87 L3).
#   [4] DEVICE STAGING — shared rig arm build (stamp includes port/gfx
#       bytes), push provenance + device-side sha verification of every
#       artifact; music PCM staged on $DSD (REAL SD — the streaming
#       gate measures genuine SD reads; SFX pack to $DTMP: pre-decoded
#       RAM by design); T5 PCM-CORRUPTION TOOTH: an appended byte on
#       the staged SD copy MUST flip the device sha verify (loud
#       assert), then re-push + re-verify clean.
#   [5] SD PROBE (measurement, logged NOT gated): drop_caches +
#       dd 64 KiB-chunk full-file read of the staged PCM — throughput
#       context for the sidecar distribution.
#   [6] PACED DEVICE RUN — g01 full match ON the FunKey-S with live
#       SDL1.2 render + audio callback + MUSIC STREAMING from SD
#       (reader thread; ring 32768 / chunk 16384 — PLAN §7's 2x64 KB),
#       the render check's full hygiene block inherited verbatim:
#       deadman-guarded frontend park, low_bat_check quiesce bracketed
#       by device-clock stamps, pre-run sync + drop_caches, detached
#       setsid launch + rc-file lifecycle, quiet-window host polling.
#   [7] HOST JUDGMENT (the device never self-reports): stream ->
#       UNCHANGED verify-stream.js vs frozen g01; timing -> strict
#       judge: full p99 < 16.67 ms, render-only p99 <= 8 ms,
#       skips == 0, rendered == 3600; app summary -> 0 failed presents
#       + wall in [58,66] s; audio summary -> underruns == 0,
#       badlen == 0, granted spec 44100/512/2 pinned, cbs in the
#       audio-check window; music summary -> starves == 0, refills in
#       the derived sanity window, musout == cbs*512 EXACTLY (valid
#       because badlen == 0: every callback rendered exactly 512
#       frames); --music-lat sidecar -> strict grammar, row count ==
#       refills, refill-read p50/p99/max reported (PORTABILITY Layer 2
#       evidence).
#
# Rig plumbing INHERITED from port/sim/device/riglib.sh (Tier-A GO) +
# the render check's reviewed stale-state/pessimism/cleanup machinery
# (iters 50-80 arc) — copied, not reimplemented. Device hygiene: writes
# only /tmp/mlfk + /mnt/mlfk-scratch + the park marker (trap-restored).
#
# PROVENANCE: sndpack + music PCM are Nintendo-derived — device scratch
# and gitignored build output only, NEVER committed (rig_no_commit_guard).
#
# Env: FUNKEY_ADB_ID (device id), MLFK_FORCE_ARM=1 (ignore build stamp).
set -euo pipefail
cd "$(dirname "$0")/../.."

GFX=port/gfx
BUILD=$GFX/build
CAL=port/sim/calib
DEVB=$CAL/build/device
SIM=port/sim/sim
TABLES=pipeline/build/sim-tables
AUDIO_OUT=pipeline/build/audio-musicfid
FDC=oracle/fdlibm-crosscheck
DTMP=/tmp/mlfk
DSD=/mnt/mlfk-scratch
mkdir -p "$BUILD" "$DEVB"

# --- frozen pins (M4 task 7; changing any is a reviewed repo change) ---------
P99_FULL_LIMIT_NS=16670000  # PLAN §4 frame budget (16.67 ms)
P99_RENDER_LIMIT_NS=8000000 # PLAN §5 render allowance (8 ms)
BUDGET_NS=16666667          # 60 fps pacing budget for the live run
FRAMES_PIN=3600             # the literal gate length; manifest cross-asserted
AUDIO_SAMPLES=512           # the measured spike config (pinned into the judge)
# REQUESTED, not inherited (A28, 2026-08-24) — same reason and same
# registered gap as check-device-audio.sh:123-140: A28 raised the app
# default (platform.h PLATFORM_AUDIO_SAMPLES_DEFAULT) off 512, so this
# leg passes --audio-samples explicitly rather than pinning a number it
# never asked for. Re-pinning to the shipped size needs a device
# re-measurement of CBS_MIN/CBS_MAX (borrowed from check-device-audio.sh).
GFXDATA_FROZEN=$GFX/gfxdata-frozen.txt
GFXDATA_SHA256=5499a3dd5fc374d6ed988faf0bef6fa2e189eb314e892bdd83c7534dc0865c94
VFXDATA_FROZEN=$GFX/vfxdata-frozen.txt
VFXDATA_SHA256=545015a3d7e3bc138059fcb9711040758e729a7d21aac650b009ed7fdb5bd662
VFXGLYPHS_FROZEN=$GFX/vfxglyphs-frozen.txt
VFXGLYPHS_SHA256=a9f08ad66cbf3f52d4003b87b7ff8f529367aa8b28bf08dbebfc3d6296d995d6
LEGIBLE_MIN_DEV_PX=2.0      # twin pin vs gfx.h (render-check constant)
SNDPACK_SHA256=f69579082fe569249879faa5ceccb7a810d94d8092695ddc8bb543f3bda3ccb4
# battlefield music PCM identity (measured-then-frozen iter 87; must
# equal check-music-fidelity.sh's pin-table row — cross-grepped below)
MUSIC_BF_SHA256=c7c1fa2262389496beaba8854d9ffa254861a14f89741cde0432e61197649f44
# battlefield music METADATA identity (measured-then-frozen iter 89;
# review-87 H1a: the staged volume-bits + sprite windows feed the LIVE
# device run — the SAME frozen values check-music-fidelity.sh pins, and
# its battlefield meta row is cross-grepped below so the two checks can
# never drift apart; a mismatch is pipeline drift = reviewed re-freeze)
MUSIC_BF_VOLBITS=3fd3333333333333
MUSIC_BF_SO=0
MUSIC_BF_SD=12366
MUSIC_BF_LO=12366
MUSIC_BF_LD=184256
WALL_MIN_MS=58000
WALL_MAX_MS=66000
CBS_MIN=4900                # check-device-audio.sh's measured callback window
CBS_MAX=5900
# refills sanity window, DERIVED from the cbs window before the run:
# src frames consumed = cbs*512/2 in [1254400,1510400]; refills ~=
# (src - ring 32768)/chunk 16384 in [74.6, 90.2]; slack for the
# teardown boundary -> [70, 95]. A value outside is a streaming defect
# (dead reader / runaway refill), never a tunable.
REFILLS_MIN=70
REFILLS_MAX=95
QW_PRE_SLACK_S=10
QW_POST_SLACK_S=10
DEADMAN_S="${MLFK_DEADMAN_S:-300}"
APPRC_TRIES=90

source port/sim/device/adbsh.sh # (also defines $DEV — it keys the lock)
source port/sim/device/riglib.sh

rig_lock_acquire

# INHERITED-STATE PESSIMISM (the render check's block, verbatim
# rationale — iter 56, review-55 H): pessimism precedes the FIRST
# fallible action; only step-0 normalization may clear it.
RIG_PRESERVE_DTMP=1
PARKED=0
DEADMAN_ARMED=0
LBC_STOPPED=0
DM_NONCE=""
MUSIC_OK=0 # fail-closed exit guard (iter 76 class: bash-3.2 expansion
           # error + EXIT trap exits 0 — the ONLY legal rc-0 exit is
           # through the DEVICE MUSIC OK line)

task7_cleanup() {
  local rc=$?
  local prc restore_verified lbc_ok
  lbc_ok=1
  if [ "$LBC_STOPPED" = 1 ]; then
    lbc_ok=0
    if rig_daemon_restore low_bat_check /etc/init.d/S12low-bat-check; then
      lbc_ok=1
      rig_dsh_retry "rm -f $DTMP/qd.low_bat_check.${DM_NONCE:-none}" \
        || echo "WARN: could not remove the quiesce marker (harmless: the deadman's restore arm is comm-scan-guarded and idempotent)" >&2
      echo "   mitigation restore: low_bat_check verified running again (comm-scan, exactly 1)" >&2
    else
      echo "WARN: low_bat_check did NOT verify as running after restart — the armed deadman will comm-scan-restore it on-device within ${DEADMAN_S}s, or run '/etc/init.d/S12low-bat-check start' on the device manually" >&2
    fi
  fi
  prc=0
  rig_dsh_retry "pkill gfx_device" || prc=$?
  case "$prc" in
    0) echo "WARN: gfx_device was still running at cleanup — killed" >&2 ;;
    1) : ;;
    *) echo "WARN: could not pkill gfx_device on the device (rc $prc)" >&2 ;;
  esac
  restore_verified=0
  if [ "$PARKED" = 1 ]; then
    rig_dsh_retry "rm -f /mnt/disable_frontend" \
      || echo "WARN: frontend restore command failed" >&2
    if rig_dsh_retry "test ! -f /mnt/disable_frontend"; then
      restore_verified=1
    else
      echo "WARN: could not VERIFY /mnt/disable_frontend is gone — the armed deadman will remove it on-device within ${DEADMAN_S}s of the park (or remove it by hand)" >&2
    fi
  else
    restore_verified=1
  fi
  if [ "$DEADMAN_ARMED" = 1 ]; then
    if [ "$restore_verified" = 1 ] && [ "$lbc_ok" = 1 ]; then
      rig_dsh_retry "touch $DTMP/deadman.cancel" \
        || echo "WARN: could not cancel the park deadman — it will fire once (idempotent actions) within ${DEADMAN_S}s" >&2
    else
      echo "WARN: frontend restore or daemon restore unverified — leaving the deadman ARMED as the backstop (its purpose)" >&2
    fi
  fi
  if [ "$restore_verified" != 1 ] || [ "$lbc_ok" != 1 ]; then
    RIG_PRESERVE_DTMP=1
  fi
  rig_cleanup
  if [ "$rc" = 0 ] && [ "${MUSIC_OK:-0}" != 1 ]; then
    # review-87 L2: this diagnostic must NEVER carry the anchored
    # success needle — an unanchored consumer grepping the log would
    # otherwise read success off this exact failure path.
    echo "DEVICE FAIL: script exited rc 0 without reaching the success verdict line (bash expansion-error class) — forcing nonzero" >&2
    exit 70
  fi
}
trap task7_cleanup EXIT

require_device

echo "== [0/7] stale-state startup normalization (cross-run chokepoint) =="
# (the render check's step-0 chokepoint, copied: every stale prior-run
# state is normalized HERE — marker first, then deadman, then wipe;
# quiesce markers first of all via rig_qd_normalize.)
rig_qd_normalize
stale_marker=0
stale_deadman=0
nrc=0
dsh "test -f /mnt/disable_frontend" >/dev/null || nrc=$?
case "$nrc" in
  0) stale_marker=1 ;;
  1) : ;;
  *) echo "DEVICE FAIL: startup normalization could not probe the frontend marker (rc $nrc)" >&2; exit 1 ;;
esac
nrc=0
dsh "test -e $DTMP/deadman.nonce -o -e $DTMP/deadman.pid" >/dev/null || nrc=$?
case "$nrc" in
  0) stale_deadman=1 ;;
  1) : ;;
  *) echo "DEVICE FAIL: startup normalization could not probe for stale deadman state (rc $nrc)" >&2; exit 1 ;;
esac
if [ "$stale_marker" = 1 ] || [ "$stale_deadman" = 1 ]; then
  echo "WARN: stale prior-run state on the device (marker=$stale_marker deadman-state=$stale_deadman) — normalizing before any parking" >&2
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
    if [ "$sdm_gone" = 1 ]; then
      echo "   stale deadman disarmed via its cancel channel (exit verified: pid file gone)"
    else
      sdm_pid="$(dsh "cat $DTMP/deadman.pid")" || {
        echo "DEVICE FAIL: startup normalization could not read the stale deadman pid file" >&2
        exit 1
      }
      sdm_pid="${sdm_pid%$'\n'}"
      if ! [[ "$sdm_pid" =~ ^[0-9]{1,7}$ ]]; then
        echo "DEVICE FAIL: stale deadman.pid is not a bounded decimal pid ('$sdm_pid')" >&2
        exit 1
      fi
      nrc=0
      dsh "test -d /proc/$sdm_pid" >/dev/null || nrc=$?
      case "$nrc" in
        0)
          echo "DEVICE FAIL: stale deadman (pid $sdm_pid) is STILL RUNNING and ignored its cancel for 12 s — a defect, not stale state; inspect the device" >&2
          exit 1
          ;;
        1)
          echo "   stale deadman.pid was reboot/kill-orphaned (pid $sdm_pid dead) — stale state"
          ;;
        *)
          echo "DEVICE FAIL: startup normalization could not probe pid $sdm_pid liveness (rc $nrc)" >&2
          exit 1
          ;;
      esac
    fi
    dsh "rm -rf $DTMP"
    echo "   stale deadman state wiped ($DTMP)"
  fi
  echo "   startup normalization complete — device state is clean"
else
  echo "   no stale prior-run state (marker absent, no deadman state)"
fi
RIG_PRESERVE_DTMP=0
rig_devsha_selftest

# --- strict judges (copied reviewed forms) -----------------------------------
timing_judge_bytes_assert() {
  local jf="$1"
  if ! tail -c 17 "$jf" | cmp -s - <(printf 'judge_complete=1\n'); then
    echo "DEVICE FAIL: timing judge output file $jf does not END with the exact bytes 'judge_complete=1<newline>' (trailing blank lines / missing final newline / truncation) — corrupt judge output" >&2
    exit 1
  fi
}
parse_timing_judge() {
  local tf="$1" fr="$2" jout jk jv dup jf
  unset full_p99_ns full_p99_ms render_p99_ns render_p99_ms sim_p99_ms \
    present_p99_ms skips rendered
  jf="$BUILD/timjudge-out.txt"
  rm -f "$jf"
  node "$GFX/judge-render-timing.js" "$tf" "$fr" > "$jf" || {
    echo "DEVICE FAIL: timing judgment failed for $tf" >&2
    exit 1
  }
  made "$jf"
  timing_judge_bytes_assert "$jf"
  jout="$(cat "$jf")"
  dup="$(printf '%s\n' "$jout" | awk -F= '{print $1}' | sort | uniq -d)"
  if [ -n "$dup" ]; then
    echo "DEVICE FAIL: timing judge output carries duplicate key(s) — corrupt evidence: $dup" >&2
    exit 1
  fi
  while IFS='=' read -r jk jv; do
    case "$jk" in
      full_p99_ns|render_p99_ns|skips|rendered)
        if ! [[ "$jv" =~ ^[0-9]{1,12}$ ]]; then
          echo "DEVICE FAIL: timing judge $jk not a bounded decimal integer (1-12 digits) ('$jv')" >&2
          exit 1
        fi
        printf -v "$jk" '%s' "$jv"
        ;;
      full_p99_ms|render_p99_ms|sim_p99_ms|present_p99_ms)
        if ! [[ "$jv" =~ ^[0-9]{1,9}\.[0-9]{3}$ ]]; then
          echo "DEVICE FAIL: timing judge $jk malformed ('$jv')" >&2
          exit 1
        fi
        printf -v "$jk" '%s' "$jv"
        ;;
      full_p50_ns|full_p50_ms|full_max_ns|full_max_ms|sim_p50_ns|sim_p50_ms|sim_p99_ns|render_p50_ns|render_p50_ms|render_max_ns|render_max_ms|present_p50_ns|present_p50_ms|present_p99_ns)
        :
        ;;
      judge_complete)
        if [ "$jv" != 1 ]; then
          echo "DEVICE FAIL: timing judge judge_complete carries unexpected value ('$jv')" >&2
          exit 1
        fi
        ;;
      *)
        echo "DEVICE FAIL: unexpected timing judge line '$jk=$jv'" >&2
        exit 1
        ;;
    esac
  done <<< "$jout"
  for jk in full_p99_ns full_p99_ms render_p99_ns render_p99_ms \
    sim_p99_ms present_p99_ms skips rendered; do
    if [ -z "${!jk:-}" ]; then
      echo "DEVICE FAIL: timing judge output missing required key '$jk'" >&2
      exit 1
    fi
  done
  timing_judge_out="$jout"
}

parse_app_summary() {
  local log="$1" fr="$2" pace="$3" budget="$4" re cnt line
  unset app_skips app_present_fails app_wall_ms
  re="^gfx_app: ${fr} frames, [0-9]{1,12} render skips, [0-9]{1,12} failed presents, wall [0-9]{1,12} ms, pace=${pace} budget=${budget} ns\$"
  cnt="$(grep -Ec "$re" "$log")" || true
  if [ "$cnt" != 1 ]; then
    echo "DEVICE FAIL: app log $log has $cnt lines matching the pinned summary grammar (want exactly 1: frames=$fr pace=$pace budget=$budget ns)" >&2
    exit 1
  fi
  line="$(grep -E "$re" "$log")"
  if [[ "$line" =~ ^gfx_app:\ ${fr}\ frames,\ ([0-9]{1,12})\ render\ skips,\ ([0-9]{1,12})\ failed\ presents,\ wall\ ([0-9]{1,12})\ ms,\ pace=${pace}\ budget=${budget}\ ns$ ]]; then
    app_skips="${BASH_REMATCH[1]}"
    app_present_fails="${BASH_REMATCH[2]}"
    app_wall_ms="${BASH_REMATCH[3]}"
  else
    echo "DEVICE FAIL: summary line failed re-extraction ('$line')" >&2
    exit 1
  fi
  : "$app_skips" "$app_present_fails" "$app_wall_ms"
}

# audio judge trio (check-device-audio.sh's reviewed forms, copied)
audio_judge_ingest() {
  local jout="$1" jrc="$2"
  local jk jv dup last
  unset au_cbs au_underruns au_badlen au_starts au_stops au_steals
  au_fails=""
  if [ "$jrc" != 0 ] && [ "$jrc" != 2 ]; then
    echo "DEVICE FAIL: audio summary judge died (rc $jrc — grammar/corruption)" >&2
    exit 1
  fi
  au_rc=$jrc
  last="$(printf '%s\n' "$jout" | tail -n 1)"
  if [ "$last" != "judge_complete=1" ]; then
    echo "DEVICE FAIL: audio judge output does not end with its judge_complete=1 integrity terminator (last line: '$last') — truncated/partial judge output = corruption, never a retry" >&2
    exit 1
  fi
  dup="$(printf '%s\n' "$jout" | awk -F= '{print $1}' | sort | uniq -d)"
  if [ -n "$dup" ]; then
    echo "DEVICE FAIL: audio judge output carries duplicate key(s): $dup" >&2
    exit 1
  fi
  while IFS='=' read -r jk jv; do
    case "$jk" in
      cbs|underruns|badlen|starts|stops|steals)
        if ! [[ "$jv" =~ ^[0-9]{1,12}$ ]]; then
          echo "DEVICE FAIL: audio judge $jk not a bounded decimal ('$jv')" >&2
          exit 1
        fi
        printf -v "au_$jk" '%s' "$jv"
        ;;
      fail_underruns|fail_badlen|fail_cbs_low|fail_cbs_high|fail_starts|fail_stops)
        if [ "$jv" != 1 ]; then
          echo "DEVICE FAIL: audio judge $jk carries unexpected value ('$jv')" >&2
          exit 1
        fi
        au_fails="$au_fails ${jk#fail_}"
        ;;
      judge_complete)
        if [ "$jv" != 1 ]; then
          echo "DEVICE FAIL: audio judge judge_complete carries unexpected value ('$jv')" >&2
          exit 1
        fi
        ;;
      *)
        echo "DEVICE FAIL: unexpected audio judge line '$jk=$jv'" >&2
        exit 1
        ;;
    esac
  done <<< "$jout"
  for jk in au_cbs au_underruns au_badlen au_starts au_stops au_steals; do
    if [ -z "${!jk:-}" ]; then
      echo "DEVICE FAIL: audio judge output missing required key '${jk#au_}'" >&2
      exit 1
    fi
  done
  au_fails="${au_fails# }"
  if [ "$au_rc" = 2 ] && [ -z "$au_fails" ]; then
    echo "DEVICE FAIL: audio judge exited 2 without naming a failed leg — corrupt judge output" >&2
    exit 1
  fi
  if [ "$au_rc" = 0 ] && [ -n "$au_fails" ]; then
    echo "DEVICE FAIL: audio judge exited 0 but named failed leg(s) '$au_fails' — corrupt judge output" >&2
    exit 1
  fi
}
audio_judge_bytes_assert() {
  local jf="$1"
  if ! tail -c 17 "$jf" | cmp -s - <(printf 'judge_complete=1\n'); then
    echo "DEVICE FAIL: audio judge output file $jf does not END with the exact bytes 'judge_complete=1<newline>' — corrupt judge output, never a retry" >&2
    exit 1
  fi
}
parse_audio_judge() {
  local log="$1" rate="$2" samples="$3" channels="$4"
  shift 4
  local jout jrc jf
  jf="$BUILD/aujudge-out.txt"
  rm -f "$jf"
  jrc=0
  node "$GFX/judge-audio-summary.js" "$log" \
    --rate "$rate" --samples "$samples" --channels "$channels" "$@" \
    > "$jf" || jrc=$?
  made "$jf"
  if [ "$jrc" != 0 ] && [ "$jrc" != 2 ]; then
    echo "DEVICE FAIL: audio summary judge died (rc $jrc — grammar/corruption)" >&2
    exit 1
  fi
  audio_judge_bytes_assert "$jf"
  jout="$(cat "$jf")"
  audio_judge_ingest "$jout" "$jrc"
}

# --- verdict-file judge (the fidelity check's reviewed form, copied;
# review-87 L1): exactly one newline-terminated line matching the full
# grammar + exactly one resembling line — extra lines, torn writes, or
# resembling non-matching lines are CORRUPTION, never ignorable.
count_e() {
  local c rc=0
  c="$(grep -cE -- "$2" "$1")" || rc=$?
  if [ "$rc" -ge 2 ]; then
    echo "DEVICE FAIL: count helper — grep -cE rc $rc reading '$1' (a read error is CORRUPT evidence, never a 0 count)" >&2
    exit 1
  fi
  printf '%s' "$c"
}
judge_verdict_file() { # <file> <full-line ERE> <resemblance ERE>
  local f="$1" full="$2" resem="$3" nl c r
  if [ ! -s "$f" ]; then
    echo "DEVICE FAIL: verdict file $f missing or empty" >&2
    exit 1
  fi
  nl="$(grep -c '' "$f")" || { echo "DEVICE FAIL: cannot count lines of $f" >&2; exit 1; }
  if [ "$nl" != 1 ]; then
    echo "DEVICE FAIL: verdict file $f has $nl lines, want exactly 1 (extra or missing lines are CORRUPTION)" >&2
    exit 1
  fi
  if [ -n "$(tail -c 1 "$f")" ]; then
    echo "DEVICE FAIL: verdict file $f lacks the trailing newline (torn write)" >&2
    exit 1
  fi
  c="$(count_e "$f" "$full")"
  if [ "$c" != 1 ]; then
    echo "DEVICE FAIL: verdict file $f: $c lines match the full grammar '$full', want exactly 1" >&2
    exit 1
  fi
  r="$(count_e "$f" "$resem")"
  if [ "$r" != 1 ]; then
    echo "DEVICE FAIL: verdict file $f: $r lines RESEMBLE the verdict ('$resem') but 1 matches the full grammar — a resembling non-matching line is CORRUPTION" >&2
    exit 1
  fi
}

# --- battlefield metadata pin assert (review-87 H1a; device twin) ------------
bf_meta_assert() { # <volbits> <so> <sd> <lo> <ld> — staged values vs the pins
  [ "$1" = "$MUSIC_BF_VOLBITS" ] || { echo "DEVICE FAIL: battlefield meta pin — volbits $1 != frozen $MUSIC_BF_VOLBITS (metadata drift is pipeline drift — reviewed re-freeze)" >&2; exit 1; }
  [ "$2" = "$MUSIC_BF_SO" ] || { echo "DEVICE FAIL: battlefield meta pin — so $2 != frozen $MUSIC_BF_SO (metadata drift is pipeline drift — reviewed re-freeze)" >&2; exit 1; }
  [ "$3" = "$MUSIC_BF_SD" ] || { echo "DEVICE FAIL: battlefield meta pin — sd $3 != frozen $MUSIC_BF_SD (metadata drift is pipeline drift — reviewed re-freeze)" >&2; exit 1; }
  [ "$4" = "$MUSIC_BF_LO" ] || { echo "DEVICE FAIL: battlefield meta pin — lo $4 != frozen $MUSIC_BF_LO (metadata drift is pipeline drift — reviewed re-freeze)" >&2; exit 1; }
  [ "$5" = "$MUSIC_BF_LD" ] || { echo "DEVICE FAIL: battlefield meta pin — ld $5 != frozen $MUSIC_BF_LD (metadata drift is pipeline drift — reviewed re-freeze)" >&2; exit 1; }
}

# --- MUSIC summary parser + gate (NEW; PROCESS §3 whitelist grammar) ---------
# PRODUCER GRAMMAR (measured from gfx_app.c's single music fprintf —
# the paired change site; corpus: this iteration's host smoke + host
# truth logs):
#   gfx_app music: <out> out frames, <starves> starves, <refills>
#   refills, ring=32768 chunk=16384
# ring/chunk are PINNED INTO the pattern (PLAN §7's 2x64 KB constants;
# a rebuilt app with drifted buffer constants cannot even parse).
# EXACTLY ONE line may match AND exactly one line may resemble
# ('gfx_app music: ' prefix) — duplicates/truncations are corruption.
# review-87 M3 (the iter-86 exact-token class): counters are exact
# tokens 0|[1-9][0-9]* — the producer is a single %llu-family fprintf
# that can never emit a leading zero — and the WHOLE log must be
# newline-terminated (a final line missing only its newline is a torn
# write grep would otherwise still match).
mus_re='^gfx_app music: (0|[1-9][0-9]{0,11}) out frames, (0|[1-9][0-9]{0,11}) starves, (0|[1-9][0-9]{0,11}) refills, ring=32768 chunk=16384$'
parse_music_summary() { # <log> — sets mus_out mus_starves mus_refills
  local log="$1" cnt rcnt line
  unset mus_out mus_starves mus_refills
  if [ ! -s "$log" ]; then
    echo "DEVICE FAIL: log $log missing or empty (no music summary to parse)" >&2
    exit 1
  fi
  if [ -n "$(tail -c 1 "$log")" ]; then
    echo "DEVICE FAIL: log $log is not newline-terminated (torn write) — the music summary cannot be trusted" >&2
    exit 1
  fi
  cnt="$(grep -Ec "$mus_re" "$log")" || true
  if [ "$cnt" != 1 ]; then
    echo "DEVICE FAIL: log $log has $cnt lines matching the pinned music summary grammar (want exactly 1)" >&2
    exit 1
  fi
  rcnt="$(grep -Ec '^gfx_app music: ' "$log")" || true
  if [ "$rcnt" != 1 ]; then
    echo "DEVICE FAIL: log $log has $rcnt music-summary-resembling lines but 1 matches the full grammar — a resembling non-matching or duplicate line is CORRUPTION" >&2
    exit 1
  fi
  line="$(grep -E "$mus_re" "$log")"
  if [[ "$line" =~ $mus_re ]]; then
    mus_out="${BASH_REMATCH[1]}"
    mus_starves="${BASH_REMATCH[2]}"
    mus_refills="${BASH_REMATCH[3]}"
  else
    echo "DEVICE FAIL: music summary line failed re-extraction ('$line')" >&2
    exit 1
  fi
  : "$mus_out" "$mus_starves" "$mus_refills"
}
judge_music_gate() { # <log> — parse + the starve gate (starves == 0)
  parse_music_summary "$1"
  if [ "$mus_starves" -ne 0 ]; then
    echo "DEVICE FAIL: music channel starved $mus_starves output frames (gate: starves == 0 — the ring/reader must never run dry)" >&2
    exit 1
  fi
}

# --- MUSIC lat sidecar judge (NEW; strict grammar, PROCESS §3) ---------------
# Producer: gfx_app.c --music-lat flush — rows `<start_ns> <read_ns>
# <frames>` (frames always 16384, the chunk), terminator
# `MUSLAT OK rows=<n>`. Judge: full-line rows, monotone start stamps,
# terminator count binding, row count == the summary's refills; emits
# nearest-rank p50/p99/max of read_ns + judge_complete=1.
parse_muslat() { # <sidecar> <expected-rows> — sets lat_p50_ns lat_p99_ns lat_max_ns lat_p99_ms
  local sf="$1" want="$2" jout jk jv dup jf
  unset lat_p50_ns lat_p99_ns lat_max_ns lat_p99_ms
  jf="$BUILD/muslat-out.txt"
  rm -f "$jf"
  node -e '
    const fs = require("fs");
    function die(m) { console.error("muslat-judge FAIL: " + m); process.exit(1); }
    const [sf, wantS] = process.argv.slice(1);
    if (!/^(0|[1-9][0-9]{0,11})$/.test(wantS)) die("bad expected-rows arg");
    const want = Number(wantS);
    const raw = fs.readFileSync(sf, "utf8");
    if (raw.length === 0 || raw[raw.length - 1] !== "\n") die("sidecar not newline-terminated (torn write)");
    const R = /^([1-9][0-9]{0,18}) (0|[1-9][0-9]{0,11}) 16384$/;
    const T = /^MUSLAT OK rows=(0|[1-9][0-9]{0,11})$/;
    const reads = [];
    let prevStart = -1, term = null;
    for (const line of raw.slice(0, -1).split("\n")) {
      if (term !== null) die("bytes after the terminator");
      let m;
      if ((m = R.exec(line))) {
        const st = Number(m[1]);
        if (st <= prevStart) die("refill start stamps not strictly monotone");
        prevStart = st;
        reads.push(Number(m[2]));
      } else if ((m = T.exec(line))) {
        term = Number(m[1]);
      } else {
        die("sidecar grammar violation: " + JSON.stringify(line));
      }
    }
    if (term === null) die("missing MUSLAT OK terminator");
    if (term !== reads.length) die("terminator rows=" + term + " != " + reads.length + " data rows");
    if (reads.length !== want) die("sidecar rows " + reads.length + " != summary refills " + want);
    if (reads.length === 0) die("zero refill rows — nothing to judge");
    reads.sort((a, b) => a - b);
    const rank = (p) => reads[Math.max(0, Math.ceil((p / 100) * reads.length) - 1)];
    const p50 = rank(50), p99 = rank(99), max = reads[reads.length - 1];
    const out = [
      "muslat_rows=" + String(reads.length),
      "muslat_p50_ns=" + String(p50),
      "muslat_p99_ns=" + String(p99),
      "muslat_max_ns=" + String(max),
      "muslat_p99_ms=" + (p99 / 1e6).toFixed(3),
      "judge_complete=1",
    ].join("\n") + "\n";
    process.stdout.write(out);
  ' "$sf" "$want" > "$jf" || {
    echo "DEVICE FAIL: --music-lat sidecar judge died for $sf" >&2
    exit 1
  }
  made "$jf"
  timing_judge_bytes_assert "$jf" # same 17-byte terminator contract
  jout="$(cat "$jf")"
  dup="$(printf '%s\n' "$jout" | awk -F= '{print $1}' | sort | uniq -d)"
  if [ -n "$dup" ]; then
    echo "DEVICE FAIL: muslat judge output carries duplicate key(s): $dup" >&2
    exit 1
  fi
  while IFS='=' read -r jk jv; do
    case "$jk" in
      muslat_rows|muslat_p50_ns|muslat_p99_ns|muslat_max_ns)
        if ! [[ "$jv" =~ ^[0-9]{1,19}$ ]]; then
          echo "DEVICE FAIL: muslat judge $jk not a bounded decimal ('$jv')" >&2
          exit 1
        fi
        printf -v "lat_${jk#muslat_}" '%s' "$jv"
        ;;
      muslat_p99_ms)
        if ! [[ "$jv" =~ ^[0-9]{1,9}\.[0-9]{3}$ ]]; then
          echo "DEVICE FAIL: muslat judge $jk malformed ('$jv')" >&2
          exit 1
        fi
        lat_p99_ms="$jv"
        ;;
      judge_complete)
        [ "$jv" = 1 ] || { echo "DEVICE FAIL: muslat judge_complete value ('$jv')" >&2; exit 1; }
        ;;
      *)
        echo "DEVICE FAIL: unexpected muslat judge line '$jk=$jv'" >&2
        exit 1
        ;;
    esac
  done <<< "$jout"
  for jk in lat_rows lat_p50_ns lat_p99_ns lat_max_ns lat_p99_ms; do
    if [ -z "${!jk:-}" ]; then
      echo "DEVICE FAIL: muslat judge output missing required key '$jk'" >&2
      exit 1
    fi
  done
}

echo "== [1/7] HOST FIDELITY (composed check-music-fidelity.sh) =="
# rm-before-produce on the child artifacts this script reuses in [2]
rm -rf "$AUDIO_OUT"
bash "$GFX/check-music-fidelity.sh"
made "$AUDIO_OUT/sounds.json" "$AUDIO_OUT/audio/music/battlefield.pcm"
echo "   MUSIC FIDELITY OK (composed child; fresh $AUDIO_OUT reused below)"

echo "== [2/7] data plane (g01 params + tables + simdata + trace + pins + music cfg) =="
unset name seed p1 p2 stage frames trace
gparams="$(node -e "
  const m=require('./oracle/goldens/manifest.json');
  const g=m.goldens.find(x=>x.id==='g01');
  if(!g) throw new Error('g01 missing from manifest');
  console.log('name='+g.name);
  console.log('seed='+g.seed);
  console.log('p1='+g.p1); console.log('p2='+g.p2);
  console.log('stage='+g.stage);
  console.log('frames='+g.frames);
  console.log('trace='+g.trace);
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
    trace)
      if ! [[ "$gv" =~ ^[a-z0-9][a-z0-9-]*\.trace\.json$ ]]; then
        echo "DEVICE FAIL: manifest g01.trace fails validation ('$gv')" >&2
        exit 1
      fi
      trace=$gv
      ;;
    seed|p1|p2|stage|frames)
      if ! [[ "$gv" =~ ^[0-9]{1,12}$ ]]; then
        echo "DEVICE FAIL: manifest g01.$gk not a bounded decimal integer (1-12 digits) ('$gv')" >&2
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
: "$name" "$seed" "$p1" "$p2" "$stage" "$frames" "$trace"
if [ "$frames" -ne "$FRAMES_PIN" ]; then
  echo "DEVICE FAIL: manifest g01.frames ($frames) != pinned FRAMES_PIN ($FRAMES_PIN) — reviewed pin change required" >&2
  exit 1
fi
[ "$frames" -le 5000 ] || { echo "DEVICE FAIL: g01 frames $frames > 5000" >&2; exit 1; }
[ "$stage" -le 5 ] || { echo "DEVICE FAIL: g01 stage $stage > 5" >&2; exit 1; }
[ "$p1" -le 4 ] || { echo "DEVICE FAIL: g01 p1 $p1 > 4" >&2; exit 1; }
[ "$p2" -le 4 ] || { echo "DEVICE FAIL: g01 p2 $p2 > 4" >&2; exit 1; }
# g01 is the battlefield golden — the staged track is stage 0's
# (main.js:1342 map, pinned in the composed fidelity check)
if [ "$stage" -ne 0 ]; then
  echo "DEVICE FAIL: g01.stage ($stage) != 0 — this check stages the battlefield track by the pinned stage->track map" >&2
  exit 1
fi
FROZEN=oracle/goldens/$name.sha256.json
made "$FROZEN"

anim_file() {
  case "$1" in
    0) echo anim_0_marth.bin ;;
    1) echo anim_1_puff.bin ;;
    2) echo anim_2_fox.bin ;;
    3) echo anim_3_falco.bin ;;
    4) echo anim_4_falcon.bin ;;
    *) echo "DEVICE FAIL: bad char id '$1'" >&2; return 1 ;;
  esac
}
ANIM_P1="$(anim_file "$p1")"
ANIM_P2="$(anim_file "$p2")"

made "$GFXDATA_FROZEN"
gsum="$(rig_host_sha256 "$GFXDATA_FROZEN")" || exit 1
if [ "$gsum" != "$GFXDATA_SHA256" ]; then
  echo "DEVICE FAIL: $GFXDATA_FROZEN sha256 $gsum != pinned $GFXDATA_SHA256" >&2
  exit 1
fi
made "$VFXDATA_FROZEN" "$VFXGLYPHS_FROZEN"
vsum="$(rig_host_sha256 "$VFXDATA_FROZEN")" || exit 1
if [ "$vsum" != "$VFXDATA_SHA256" ]; then
  echo "DEVICE FAIL: $VFXDATA_FROZEN sha256 $vsum != pinned $VFXDATA_SHA256" >&2
  exit 1
fi
gsum2="$(rig_host_sha256 "$VFXGLYPHS_FROZEN")" || exit 1
if [ "$gsum2" != "$VFXGLYPHS_SHA256" ]; then
  echo "DEVICE FAIL: $VFXGLYPHS_FROZEN sha256 $gsum2 != pinned $VFXGLYPHS_SHA256" >&2
  exit 1
fi
if ! grep -q "^#define GFX_LEGIBLE_MIN_DEV_PX ${LEGIBLE_MIN_DEV_PX}\$" "$GFX/gfx.h"; then
  echo "DEVICE FAIL: gfx.h GFX_LEGIBLE_MIN_DEV_PX != pinned ${LEGIBLE_MIN_DEV_PX} (twin-pin drift — reviewed change required)" >&2
  exit 1
fi
echo "   gfxdata/vfxdata/vfxglyphs pins + legibility twin pin OK"

bash pipeline/extractor/build-extractor.sh
rm -f "$TABLES/ml_tables.c" "$TABLES/ml_tables.h" \
  "$TABLES/ml_stages.c" "$TABLES/ml_stages.h" \
  "$TABLES/$ANIM_P1" "$TABLES/$ANIM_P2"
node pipeline/run.js --only animations,tables,stages --out "$TABLES"
made "$TABLES/ml_tables.c" "$TABLES/ml_tables.h" \
  "$TABLES/ml_stages.c" "$TABLES/ml_stages.h" \
  "$TABLES/$ANIM_P1" "$TABLES/$ANIM_P2"
rm -f "$DEVB/simdata.txt"
node "$CAL/dump-sim-data.js" --out "$DEVB/simdata.txt"
made "$DEVB/simdata.txt"
if [ ! -f "oracle/goldens/$trace" ]; then
  echo "DEVICE FAIL: g01 trace file oracle/goldens/$trace missing" >&2
  exit 1
fi
rm -f "$DEVB/g01.trace.txt"
node "$SIM/trace-to-txt.js" "oracle/goldens/$trace" "$DEVB/g01.trace.txt"
made "$DEVB/g01.trace.txt"

# battlefield music PCM identity: pinned here + cross-grepped against
# the composed fidelity check's pin-table row (ONE identity across the
# music surface — the SNDPACK cross-pin pattern).
bfsum="$(rig_host_sha256 "$AUDIO_OUT/audio/music/battlefield.pcm")" || exit 1
if [ "$bfsum" != "$MUSIC_BF_SHA256" ]; then
  echo "DEVICE FAIL: battlefield.pcm sha256 $bfsum != pinned $MUSIC_BF_SHA256 (pipeline/ffmpeg drift — reviewed re-freeze)" >&2
  exit 1
fi
fidrow="$(grep -Ec "^${MUSIC_BF_SHA256} battlefield\$" "$GFX/check-music-fidelity.sh")" || true
if [ "$fidrow" != 1 ]; then
  echo "DEVICE FAIL: check-music-fidelity.sh carries $fidrow pin rows matching '$MUSIC_BF_SHA256 battlefield' (want exactly 1 — one music identity across the surface)" >&2
  exit 1
fi
echo "   battlefield.pcm pin OK + fidelity-check pin row cross-checked"

# pack: REBUILD fresh x2 from the child's audio dir, pin
pack_re='^pack-snd OK count=180 dataBytes=[0-9]{1,12} fileBytes=[0-9]{1,12}$'
rm -f "$BUILD/sndpack.bin" "$BUILD/sndpack-a.bin" "$BUILD/sndpack-b.bin"
for side in a b; do
  rm -f "$BUILD/pack-out-$side.txt"
  node "$GFX/pack-snd.js" "$AUDIO_OUT" "$BUILD/sndpack-$side.bin" \
    > "$BUILD/pack-out-$side.txt" || { echo "DEVICE FAIL: pack-snd.js failed (side $side)" >&2; exit 1; }
  made "$BUILD/sndpack-$side.bin" "$BUILD/pack-out-$side.txt"
  # review-87 L1: exact-line judge — one valid line plus garbage, or a
  # missing final newline, is corruption, never a pass.
  judge_verdict_file "$BUILD/pack-out-$side.txt" "$pack_re" 'pack-snd '
done
cmp "$BUILD/sndpack-a.bin" "$BUILD/sndpack-b.bin"
mv "$BUILD/sndpack-a.bin" "$BUILD/sndpack.bin"; rm -f "$BUILD/sndpack-b.bin"
psum="$(rig_host_sha256 "$BUILD/sndpack.bin")" || exit 1
if [ "$psum" != "$SNDPACK_SHA256" ]; then
  echo "DEVICE FAIL: sndpack sha256 $psum != pinned $SNDPACK_SHA256" >&2
  exit 1
fi
echo "   SNDPACK1 x2 byte-identical, sha pinned"

# battlefield music cfg (strict whitelist; the fidelity check's
# extractor contract, subsetted to the one staged track)
mcfg="$(node -e '
  const fs = require("fs");
  const s = JSON.parse(fs.readFileSync(process.argv[1], "utf8"));
  function die(m) { console.error("music-extract FAIL: " + m); process.exit(1); }
  if (s.formatVersion !== 1) die("sounds.json formatVersion != 1");
  const e = s.music && s.music.battlefield;
  if (!e) die("battlefield not in the SND1 music map");
  const bits = e.volume && e.volume.bits;
  if (typeof bits !== "string" || !/^[0-9a-f]{16}$/.test(bits)) die("bad volume bits");
  if (e.blob !== "audio/music/battlefield.pcm") die("non-canonical blob path");
  const sp = e.sprite;
  if (!sp || !Array.isArray(sp.start) || sp.start.length !== 2 ||
      !Array.isArray(sp.loop) || sp.loop.length !== 2) die("bad sprite windows");
  for (const x of [...sp.start, ...sp.loop]) {
    if (!Number.isInteger(x) || x < 0 || x > 1000000000) die("sprite ms outside the sane domain");
  }
  const emit = (k, v) => process.stdout.write(k + "=" + String(v) + "\n");
  emit("volbits", bits);
  emit("so", sp.start[0]); emit("sd", sp.start[1]);
  emit("lo", sp.loop[0]); emit("ld", sp.loop[1]);
' "$AUDIO_OUT/sounds.json")" || { echo "DEVICE FAIL: music cfg extraction failed" >&2; exit 1; }
unset M_VOLBITS M_SO M_SD M_LO M_LD
mn=0
while IFS='=' read -r mk mv; do
  mn=$((mn + 1))
  case "$mk" in
    volbits)
      [[ "$mv" =~ ^[0-9a-f]{16}$ ]] || { echo "DEVICE FAIL: music volbits grammar ('$mv')" >&2; exit 1; }
      [ -z "${M_VOLBITS:-}" ] || { echo "DEVICE FAIL: duplicate volbits line" >&2; exit 1; }
      M_VOLBITS="$mv" ;;
    so|lo)
      [[ "$mv" =~ ^(0|[1-9][0-9]{0,9})$ ]] || { echo "DEVICE FAIL: music $mk grammar ('$mv')" >&2; exit 1; }
      vname="M_$(printf '%s' "$mk" | tr 'a-z' 'A-Z')"
      [ -z "${!vname:-}" ] || { echo "DEVICE FAIL: duplicate $mk line" >&2; exit 1; }
      printf -v "$vname" '%s' "$mv" ;;
    sd|ld)
      [[ "$mv" =~ ^[1-9][0-9]{0,9}$ ]] || { echo "DEVICE FAIL: music $mk grammar ('$mv')" >&2; exit 1; }
      vname="M_$(printf '%s' "$mk" | tr 'a-z' 'A-Z')"
      [ -z "${!vname:-}" ] || { echo "DEVICE FAIL: duplicate $mk line" >&2; exit 1; }
      printf -v "$vname" '%s' "$mv" ;;
    *)
      echo "DEVICE FAIL: unexpected music cfg line '$mk=$mv'" >&2
      exit 1 ;;
  esac
done <<< "$mcfg"
[ "$mn" = 5 ] || { echo "DEVICE FAIL: music cfg emitted $mn lines, want 5" >&2; exit 1; }
: "$M_VOLBITS" "$M_SO" "$M_SD" "$M_LO" "$M_LD"
# review-87 H1a: the staged metadata must equal the FROZEN pins (the
# grammar above proves shape, never values — a valid drifted volume or
# loop window would otherwise reach the live run), and the fidelity
# check's battlefield meta row must carry the SAME values (one metadata
# identity across the surface — the MUSIC_BF_SHA256 pattern).
bf_meta_assert "$M_VOLBITS" "$M_SO" "$M_SD" "$M_LO" "$M_LD"
metarow="$(grep -Ec "^battlefield ${MUSIC_BF_VOLBITS} ${MUSIC_BF_SO} ${MUSIC_BF_SD} ${MUSIC_BF_LO} ${MUSIC_BF_LD}\$" "$GFX/check-music-fidelity.sh")" || true
if [ "$metarow" != 1 ]; then
  echo "DEVICE FAIL: check-music-fidelity.sh carries $metarow meta pin rows matching 'battlefield $MUSIC_BF_VOLBITS $MUSIC_BF_SO $MUSIC_BF_SD $MUSIC_BF_LO $MUSIC_BF_LD' (want exactly 1 — one metadata identity across the surface)" >&2
  exit 1
fi
# standing tooth (review-87 H1a): a perturbed volbits nibble must die
# in the pin assert — proves the assert compares values, not shape.
if (bf_meta_assert "ffd3333333333333" "$M_SO" "$M_SD" "$M_LO" "$M_LD") \
    > /dev/null 2> "$BUILD/tooth-bfmeta.err"; then
  echo "DEVICE FAIL: TOOTH bf-meta did NOT fire (assert accepted a perturbed volbits)" >&2
  exit 1
fi
if ! grep -q 'battlefield meta pin — volbits' "$BUILD/tooth-bfmeta.err"; then
  echo "DEVICE FAIL: TOOTH bf-meta died for the wrong reason: $(cat "$BUILD/tooth-bfmeta.err")" >&2
  exit 1
fi
rm -f "$BUILD/tooth-bfmeta.err"
echo "   battlefield music cfg: volbits=$M_VOLBITS start=$M_SO,$M_SD loop=$M_LO,$M_LD (meta pins verified + fidelity row cross-checked; tooth bf-meta fired)"

echo "== [3/7] host build + host truth (x2 headless WITH music) + parser teeth =="
CFLAGS_COMMON=(-ffp-contract=off -Wall -Wextra -Werror
  -I"$TABLES" -Iport/ryu -Iport/sim -Ioracle/qjs)
SIM_TUS=(
  port/sim/sim/sim_boot.c port/sim/sim/sim_tick.c port/sim/sim/sim_ser.c
  port/sim/sim/sim_data.c
  port/sim/calib/canon.c port/sim/calib/player_canon.c
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
  "$TABLES/ml_tables.c" "$TABLES/ml_stages.c"
  oracle/qjs/sha256.c port/fdlibm/fdlibm.c
)
rm -f "$BUILD/raster.o" "$BUILD/gfx_app_headless" "$BUILD/gfx_app_sdl2"
cc -O3 "${CFLAGS_COMMON[@]}" -c "$GFX/raster.c" -o "$BUILD/raster.o"
cc -O2 "${CFLAGS_COMMON[@]}" -o "$BUILD/gfx_app_headless" \
  "$BUILD/raster.o" "$GFX/gfx_app.c" "$GFX/platform_headless.c" \
  "$GFX/anim1.c" "$GFX/gfx_render.c" \
  "$GFX/gfx_vfx.c" "$GFX/gfx_overlay.c" "$GFX/gfx_glyphs.c" "$GFX/gfx_bg.c" \
  "${SIM_TUS[@]}" \
  port/sim/characters/shared/moves/*.c \
  port/sim/characters/fox/moves/*.c \
  port/sim/characters/falco/moves/*.c \
  port/sim/characters/falcon/moves/*.c \
  port/sim/characters/marth/moves/*.c \
  port/sim/characters/puff/moves/*.c \
  -lm
made "$BUILD/gfx_app_headless"
if command -v sdl2-config >/dev/null 2>&1; then
  cc -O2 "${CFLAGS_COMMON[@]}" $(sdl2-config --cflags) \
    -o "$BUILD/gfx_app_sdl2" \
    "$BUILD/raster.o" "$GFX/gfx_app.c" "$GFX/platform_sdl2.c" \
    "$GFX/anim1.c" "$GFX/gfx_render.c" \
    "$GFX/gfx_vfx.c" "$GFX/gfx_overlay.c" "$GFX/gfx_glyphs.c" "$GFX/gfx_bg.c" \
    "${SIM_TUS[@]}" \
    port/sim/characters/shared/moves/*.c \
    port/sim/characters/fox/moves/*.c \
    port/sim/characters/falco/moves/*.c \
    port/sim/characters/falcon/moves/*.c \
    port/sim/characters/marth/moves/*.c \
    port/sim/characters/puff/moves/*.c \
    $(sdl2-config --libs) -lm
  made "$BUILD/gfx_app_sdl2"
  echo "   SDL2 dev backend built (gfx_app_sdl2; not run — no GUI in a check)"
else
  echo "DEVICE FAIL: sdl2-config not found — the SDL2 dev backend must BUILD (three-backend seam)" >&2
  exit 1
fi
echo "   host build OK (raster TU -O3, all else -O2; -ffp-contract=off everywhere)"

for side in a b; do
  rm -f "$BUILD/g01.mus-out-$side.txt" "$BUILD/g01.mus-tim-$side.txt" \
    "$BUILD/g01.mus-log-$side.txt" "$BUILD/g01.mus-lat-$side.txt"
  "$BUILD/gfx_app_headless" \
    --trace "$DEVB/g01.trace.txt" --simdata "$DEVB/simdata.txt" \
    --gfxdata "$GFXDATA_FROZEN" --vfxdata "$VFXDATA_FROZEN" \
    --glyphs "$VFXGLYPHS_FROZEN" --legible --anim-dir "$TABLES" \
    --seed "$seed" --p1 "$p1" --p2 "$p2" --stage "$stage" \
    --frames "$frames" --pace 0 \
    --out "$BUILD/g01.mus-out-$side.txt" \
    --timing "$BUILD/g01.mus-tim-$side.txt" \
    --sndpack "$BUILD/sndpack.bin" \
    --music "$AUDIO_OUT/audio/music/battlefield.pcm" \
    --music-volbits "$M_VOLBITS" --music-start "$M_SO,$M_SD" \
    --music-loop "$M_LO,$M_LD" \
    --music-lat "$BUILD/g01.mus-lat-$side.txt" \
    2> "$BUILD/g01.mus-log-$side.txt"
  made "$BUILD/g01.mus-out-$side.txt" "$BUILD/g01.mus-tim-$side.txt" \
    "$BUILD/g01.mus-log-$side.txt" "$BUILD/g01.mus-lat-$side.txt"
done
cmp "$BUILD/g01.mus-out-a.txt" "$BUILD/g01.mus-out-b.txt"
echo "   x2 headless replays byte-identical (stream)"
rm -f "$BUILD/g01.mus-run.json"
node "$SIM/wrap-run.js" g01 "$BUILD/g01.mus-out-a.txt" "$BUILD/g01.mus-run.json"
made "$BUILD/g01.mus-run.json"
node oracle/harness/verify-stream.js "$BUILD/g01.mus-run.json" "$FROZEN"
echo "   host headless stream verified (music plumbing does not perturb the sim)"
parse_app_summary "$BUILD/g01.mus-log-a.txt" "$frames" 0 "$BUDGET_NS"
if [ "$app_skips" -ne 0 ] || [ "$app_present_fails" -ne 0 ]; then
  echo "DEVICE FAIL: host headless leg reports skips=$app_skips presentFails=$app_present_fails (want 0/0)" >&2
  exit 1
fi
parse_audio_judge "$BUILD/g01.mus-log-a.txt" 0 0 0 \
  --cbs-min 0 --cbs-max 0 --max-underruns 0 --max-badlen 0
if [ "$au_rc" != 0 ] || [ "$au_cbs" -ne 0 ]; then
  echo "DEVICE FAIL: host truth audio judge (rc=$au_rc cbs=$au_cbs fails='$au_fails') — headless must report 0 cbs" >&2
  exit 1
fi
# host music summary: EXACT zeros (no callback consumes on headless —
# plumbing-only honesty; a nonzero here means a phantom consumer)
judge_music_gate "$BUILD/g01.mus-log-a.txt"
if [ "$mus_out" -ne 0 ] || [ "$mus_refills" -ne 0 ]; then
  echo "DEVICE FAIL: headless music summary out=$mus_out refills=$mus_refills (want 0/0 — no callback consumes on the headless backend)" >&2
  exit 1
fi
# headless lat sidecar: zero rows, terminator exact
if ! cmp -s "$BUILD/g01.mus-lat-a.txt" <(printf 'MUSLAT OK rows=0\n'); then
  echo "DEVICE FAIL: headless --music-lat sidecar is not EXACTLY 'MUSLAT OK rows=0<newline>' (no refills can occur without a consumer)" >&2
  exit 1
fi
echo "   host summary OK (0 skips, 0 failed presents; audio 0 cbs; music 0/0/0; lat rows=0)"

# MUSIC PARSER TEETH (pre-registered T6-parse class): crafted logs
# against the REAL parser/gate functions, run in subshells.
tooth_log="$BUILD/mus-tooth-log.txt"
# (a) duplicate summary line -> resemblance count 2 -> parser dies
cp "$BUILD/g01.mus-log-a.txt" "$tooth_log"
grep -E "$mus_re" "$BUILD/g01.mus-log-a.txt" >> "$tooth_log"
if (parse_music_summary "$tooth_log") >/dev/null 2>&1; then
  echo "DEVICE FAIL: TOOTH parse-dup did NOT fire (parser accepted a duplicated music summary line)" >&2
  exit 1
fi
# (b) starves=1 -> grammar parses, the starve GATE must die
sed -E 's/^gfx_app music: ([0-9]+) out frames, 0 starves,/gfx_app music: \1 out frames, 1 starves,/' \
  "$BUILD/g01.mus-log-a.txt" > "$tooth_log"
if (judge_music_gate "$tooth_log") >/dev/null 2>&1; then
  echo "DEVICE FAIL: TOOTH parse-starves did NOT fire (gate accepted starves=1)" >&2
  exit 1
fi
# (c) drifted ring constant -> cannot even parse (pinned into the grammar)
sed 's/ring=32768/ring=16384/' "$BUILD/g01.mus-log-a.txt" > "$tooth_log"
if (parse_music_summary "$tooth_log") >/dev/null 2>&1; then
  echo "DEVICE FAIL: TOOTH parse-ring-drift did NOT fire (parser accepted a drifted ring constant)" >&2
  exit 1
fi
# (d) review-87 M3: leading-zero counter ('0' -> '00') -> the exact-
# token grammar refuses (the resemblance count then flags corruption)
sed -E 's/^(gfx_app music: [0-9]+ out frames, [0-9]+ starves,) ([0-9]+) (refills,)/\1 0\2 \3/' \
  "$BUILD/g01.mus-log-a.txt" > "$tooth_log"
if (parse_music_summary "$tooth_log") >/dev/null 2>&1; then
  echo "DEVICE FAIL: TOOTH parse-leading-zero did NOT fire (parser accepted a '00' counter)" >&2
  exit 1
fi
# (e) review-87 M3: a log missing ONLY its final newline is a torn
# write -> the parser refuses before any grep can match
printf '%s' "$(cat "$BUILD/g01.mus-log-a.txt")" > "$tooth_log"
if (parse_music_summary "$tooth_log") >/dev/null 2>&1; then
  echo "DEVICE FAIL: TOOTH parse-truncated did NOT fire (parser accepted a non-newline-terminated log)" >&2
  exit 1
fi
# positive control: the genuine log still parses green
judge_music_gate "$BUILD/g01.mus-log-a.txt"
rm -f "$tooth_log"
echo "   music parser teeth fired (dup line / starves=1 / ring drift / leading zero / torn final line all die; positive control clean)"

# JOIN-DEADLINE TOOTH (review-87 L3): --tooth-music-wedge makes the
# reader thread ignore quit; the app's BOUNDED teardown (5 s deadline
# poll on the reader's done flag) must die LOUD instead of hanging in
# pthread_join. Host-side 60 s kill guard so a deadline defect can
# never hang this check.
rm -f "$BUILD/wedge-log.txt" "$BUILD/wedge.out" "$BUILD/wedge.tim"
wrc=0
"$BUILD/gfx_app_headless" \
  --trace "$DEVB/g01.trace.txt" --simdata "$DEVB/simdata.txt" \
  --gfxdata "$GFXDATA_FROZEN" --vfxdata "$VFXDATA_FROZEN" \
  --glyphs "$VFXGLYPHS_FROZEN" --legible --anim-dir "$TABLES" \
  --seed "$seed" --p1 "$p1" --p2 "$p2" --stage "$stage" \
  --frames 60 --pace 0 \
  --out "$BUILD/wedge.out" --timing "$BUILD/wedge.tim" \
  --sndpack "$BUILD/sndpack.bin" \
  --music "$AUDIO_OUT/audio/music/battlefield.pcm" \
  --music-volbits "$M_VOLBITS" --music-start "$M_SO,$M_SD" \
  --music-loop "$M_LO,$M_LD" --tooth-music-wedge \
  2> "$BUILD/wedge-log.txt" &
wpid=$!
walive=1
for _ in $(seq 1 60); do
  if ! kill -0 "$wpid" 2>/dev/null; then walive=0; break; fi
  sleep 1
done
if [ "$walive" = 1 ]; then
  kill -9 "$wpid" 2>/dev/null || true
  wait "$wpid" 2>/dev/null || true
  echo "DEVICE FAIL: TOOTH T-WEDGE — the wedged-reader run is STILL ALIVE after 60 s (the join deadline did not fire)" >&2
  exit 1
fi
wait "$wpid" || wrc=$?
if [ "$wrc" = 0 ]; then
  echo "DEVICE FAIL: TOOTH T-WEDGE did NOT fire (wedged-reader run exited 0)" >&2
  exit 1
fi
wcnt="$(grep -c 'reader thread did not exit within the teardown deadline' "$BUILD/wedge-log.txt")" || true
if [ "$wcnt" != 1 ]; then
  echo "DEVICE FAIL: TOOTH T-WEDGE — expected exactly 1 join-deadline death message, got $wcnt ($(cat "$BUILD/wedge-log.txt" 2>/dev/null | tail -3))" >&2
  exit 1
fi
rm -f "$BUILD/wedge-log.txt" "$BUILD/wedge.out" "$BUILD/wedge.tim"
echo "   tooth T-WEDGE fired (wedged reader -> loud join-deadline death rc=$wrc, never a hang)"

echo "== [4/7] armv7 build (shared rig stamp) + push + provenance + T5 tooth =="
rig_arm_build
rig_stamp_rehash gfx_device
dsh "rm -rf $DTMP $DSD && mkdir -p $DTMP $DSD"
adb -s "$DEV" push "$DEVB/gfx_device" "$DEVB/simdata.txt" \
  "$DEVB/g01.trace.txt" "$GFXDATA_FROZEN" \
  "$VFXDATA_FROZEN" "$VFXGLYPHS_FROZEN" \
  "$TABLES/$ANIM_P1" "$TABLES/$ANIM_P2" "$BUILD/sndpack.bin" "$DTMP/" >/dev/null
rig_push_provenance "$DTMP" gfx_device
dsh "chmod +x $DTMP/gfx_device"
for hf in "$DEVB/simdata.txt" "$DEVB/g01.trace.txt" "$GFXDATA_FROZEN" \
          "$VFXDATA_FROZEN" "$VFXGLYPHS_FROZEN" \
          "$TABLES/$ANIM_P1" "$TABLES/$ANIM_P2" "$BUILD/sndpack.bin"; do
  bn="$(basename "$hf")"
  hsum="$(rig_host_sha256 "$hf")" || exit 1
  dsum="$(rig_dev_sha256 "$DTMP/$bn")" || exit 1
  if [ "$dsum" != "$hsum" ]; then
    echo "DEVICE FAIL: pushed $bn device sha ($dsum) != host sha ($hsum)" >&2
    exit 1
  fi
done
echo "   pushed data sha-verified on device (simdata, trace, gfxdata, vfxdata, vfxglyphs, 2 anim bins, sndpack)"
# music PCM to REAL SD (the streaming gate measures genuine SD reads;
# RAM would prove nothing about PLAN §7's storage path)
adb -s "$DEV" push "$AUDIO_OUT/audio/music/battlefield.pcm" "$DSD/" >/dev/null
dsum="$(rig_dev_sha256 "$DSD/battlefield.pcm")" || exit 1
if [ "$dsum" != "$MUSIC_BF_SHA256" ]; then
  echo "DEVICE FAIL: staged battlefield.pcm device sha ($dsum) != pinned ($MUSIC_BF_SHA256)" >&2
  exit 1
fi
# T5 — PCM CORRUPTION TOOTH (pre-registered): an appended byte on the
# staged SD copy MUST flip the device-side sha verify; then re-push +
# re-verify clean. Proves the identity check would catch silent SD
# corruption of the streamed source.
dsh "printf x >> $DSD/battlefield.pcm"
dsum="$(rig_dev_sha256 "$DSD/battlefield.pcm")" || exit 1
if [ "$dsum" = "$MUSIC_BF_SHA256" ]; then
  echo "DEVICE FAIL: TOOTH T5 did NOT fire — device sha unchanged after an appended byte (the identity check is blind)" >&2
  exit 1
fi
adb -s "$DEV" push "$AUDIO_OUT/audio/music/battlefield.pcm" "$DSD/" >/dev/null
dsum="$(rig_dev_sha256 "$DSD/battlefield.pcm")" || exit 1
if [ "$dsum" != "$MUSIC_BF_SHA256" ]; then
  echo "DEVICE FAIL: battlefield.pcm re-push did not restore the pinned sha ($dsum)" >&2
  exit 1
fi
echo "   music PCM staged on SD, sha-verified; TOOTH T5 fired (corruption flips the verify; re-push restored)"

echo "== [5/7] SD read probe (measurement, logged NOT gated) =="
# drop_caches so the dd measures the SD, not the page cache. Refutation
# shape (d), pre-registered: if the kernel lacks the knob, proceed and
# record the page-cache exposure honestly.
DROP_CACHES_OK=1
if ! dsh "sync && echo 3 > /proc/sys/vm/drop_caches" >/dev/null 2>&1; then
  DROP_CACHES_OK=0
  echo "WARN: /proc/sys/vm/drop_caches unavailable on this kernel — SD probe + paced run measure through the page cache (refutation shape (d): recorded exposure, cold-boot coverage is the M4 gate's fresh-boot discipline)" >&2
fi
rm -f "$DEVB/g01.sd-probe.txt"
dsh "time dd if=$DSD/battlefield.pcm of=/dev/null bs=65536" \
  > "$DEVB/g01.sd-probe.txt" 2>&1 || {
  echo "DEVICE FAIL: SD read probe dd failed" >&2
  exit 1
}
made "$DEVB/g01.sd-probe.txt"
sed 's/^/   sd-probe: /' "$DEVB/g01.sd-probe.txt"

echo "== [6/7] device: LIVE paced g01 render + SFX + MUSIC (deadman-guarded park) =="
DM_NONCE="$RANDOM$RANDOM$$"
rm -f "$BUILD/deadman.sh"
cat > "$BUILD/deadman.sh" << EOF
#!/bin/sh
# generated by check-device-music.sh — frontend-park DEADMAN (the
# render check's reviewed form, iters 52-76)
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
  gp="\$(cat $DTMP/gfx.pid.$DM_NONCE 2>/dev/null)"
  case "\$gp" in
    ''|*[!0-9]*) : ;;
    *) if grep -q gfx_device "/proc/\$gp/cmdline" 2>/dev/null; then kill "\$gp"; fi ;;
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
rm -f "$BUILD/music-launch.sh"
cat > "$BUILD/music-launch.sh" << EOF
#!/bin/sh
# generated by check-device-music.sh — paced live render+SFX+MUSIC launcher
cd $DTMP || exit 9
rm -f music.apprc gfx.pid.$DM_NONCE app.start.ts app.end.ts
setsid sh -c 'date +%s > $DTMP/app.start.ts; ./gfx_device \
  --trace $DTMP/g01.trace.txt --simdata $DTMP/simdata.txt \
  --gfxdata $DTMP/gfxdata-frozen.txt --vfxdata $DTMP/vfxdata-frozen.txt \
  --glyphs $DTMP/vfxglyphs-frozen.txt --legible --anim-dir $DTMP \
  --seed $seed --p1 $p1 --p2 $p2 --stage $stage --frames $frames \
  --pace 1 --budget-ns $BUDGET_NS \
  --out $DTMP/g01.mus-out.txt --timing $DTMP/g01.mus-tim.txt \
  --sndpack $DTMP/sndpack.bin --audio-samples $AUDIO_SAMPLES \
  --music $DSD/battlefield.pcm --music-volbits $M_VOLBITS \
  --music-start $M_SO,$M_SD --music-loop $M_LO,$M_LD \
  --music-lat $DTMP/g01.mus-lat.txt 2> $DTMP/g01.mus-log.txt & \
  echo \$! > $DTMP/gfx.pid.$DM_NONCE; \
  wait \$!; arc=\$?; \
  date +%s > $DTMP/app.end.ts; \
  echo "RC=\$arc" > $DTMP/music.apprc' \
  </dev/null >/dev/null 2>&1 &
sleep 2
EOF
made "$BUILD/music-launch.sh"
adb -s "$DEV" push "$BUILD/deadman.sh" "$BUILD/music-launch.sh" "$DTMP/" >/dev/null
for hf in "$BUILD/deadman.sh" "$BUILD/music-launch.sh"; do
  bn="$(basename "$hf")"
  hsum="$(rig_host_sha256 "$hf")" || exit 1
  dsum="$(rig_dev_sha256 "$DTMP/$bn")" || exit 1
  if [ "$dsum" != "$hsum" ]; then
    echo "DEVICE FAIL: pushed $bn device sha ($dsum) != host sha ($hsum)" >&2
    exit 1
  fi
done
dsh "chmod +x $DTMP/deadman.sh $DTMP/music-launch.sh"

# PRE-RUN SYNC (the render check's dirty-writeback class fix) + a
# SECOND drop_caches AFTER all pushes: the gate must measure genuine
# SD reads of the staged PCM, not the push's page-cache residue.
dsh "sync"
if [ "$DROP_CACHES_OK" = 1 ]; then
  dsh "echo 3 > /proc/sys/vm/drop_caches"
else
  echo "WARN: paced run proceeds without drop_caches (exposure recorded above)" >&2
fi
# arm the deadman BEFORE parking
dsh "printf '%s' '$DM_NONCE' > $DTMP/deadman.nonce; rm -f $DTMP/deadman.cancel $DTMP/deadman.fired"
dsh "setsid sh $DTMP/deadman.sh </dev/null >/dev/null 2>&1 & sleep 1"
if ! dsh "test -f $DTMP/deadman.pid" >/dev/null 2>&1; then
  echo "DEVICE FAIL: park deadman did not start (no pid file)" >&2
  exit 1
fi
DEADMAN_ARMED=1
echo "   deadman armed (window ${DEADMAN_S}s, nonce-scoped, cancel-on-success verified below)"

PARKED=1
dsh "touch /mnt/disable_frontend"
prc=0
dsh "pkill gmenu2x" >/dev/null 2>&1 || prc=$?
case "$prc" in
  0) : ;;
  1) echo "WARN: gmenu2x was not running at park time" >&2 ;;
  *) echo "DEVICE FAIL: pkill gmenu2x failed (rc $prc)" >&2; exit 1 ;;
esac

# SKIP-STALL MITIGATION (M4 task 8 verdict: low_bat_check; quiesced for
# EXACTLY the paced run — the render check's bracketed form)
dsh "printf '' > $DTMP/qd.low_bat_check.$DM_NONCE"
LBC_STOPPED=1
lbc_pid="$(rig_daemon_stop low_bat_check)"
dsh "date +%s > $DTMP/qstop.ts"
echo "   mitigation: low_bat_check (pid $lbc_pid) quiesced for the paced run (trap + deadman restore)"

t0=$(date +%s)
dsh "sh -lc $DTMP/music-launch.sh"
# QUIET WINDOW + bounded rc poll (§7#1 shape; the render check's form)
sleep 50
apprc_seen=0
for _ in $(seq 1 "$APPRC_TRIES"); do
  if dsh "test -f $DTMP/music.apprc" >/dev/null 2>&1; then apprc_seen=1; break; fi
  sleep 2
done
t1=$(date +%s)
if [ "$apprc_seen" != 1 ]; then
  dsh "cat $DTMP/g01.mus-log.txt" >&2 || true
  echo "DEVICE FAIL: live music run never finished (rc file absent after $((APPRC_TRIES * 2))s)" >&2
  exit 1
fi
if rig_daemon_restore low_bat_check /etc/init.d/S12low-bat-check "$DTMP/qrestore.ts"; then
  dsh "rm -f $DTMP/qd.low_bat_check.$DM_NONCE"
  dsh "test ! -f $DTMP/qd.low_bat_check.$DM_NONCE"
  LBC_STOPPED=0
  echo "   mitigation restore: low_bat_check running again (comm-scan-verified, exactly 1; quiesce marker cleared)"
else
  echo "DEVICE FAIL: low_bat_check did not verify as running after restart — run '/etc/init.d/S12low-bat-check start' on the device manually" >&2
  exit 1
fi
qstop_ts="$(rig_dev_ts "$DTMP/qstop.ts")" || exit 1
appstart_ts="$(rig_dev_ts "$DTMP/app.start.ts")" || exit 1
append_ts="$(rig_dev_ts "$DTMP/app.end.ts")" || exit 1
qrestore_ts="$(rig_dev_ts "$DTMP/qrestore.ts")" || exit 1
rig_quiesce_bracket_assert "music low_bat_check" \
  "$qstop_ts" "$appstart_ts" "$append_ts" "$qrestore_ts" \
  "$QW_PRE_SLACK_S" "$QW_POST_SLACK_S" || exit 1
pullv "$DTMP/music.apprc" "$DEVB/music.apprc"
if ! cmp -s "$DEVB/music.apprc" <(printf 'RC=0\n'); then
  dsh "cat $DTMP/g01.mus-log.txt" >&2 || true
  echo "DEVICE FAIL: live music run rc file is not EXACTLY the bytes 'RC=0<newline>' (got: '$(cat "$DEVB/music.apprc")') — app failed or the completion record is corrupt" >&2
  exit 1
fi
dsh "test -s $DTMP/gfx.pid.$DM_NONCE"
dsh "rm -f /mnt/disable_frontend"
dsh "test ! -f /mnt/disable_frontend"
PARKED=0
dsh "touch $DTMP/deadman.cancel"
dmgone=0
for _ in $(seq 1 6); do
  if dsh "test ! -f $DTMP/deadman.pid" >/dev/null 2>&1; then dmgone=1; break; fi
  sleep 2
done
if [ "$dmgone" != 1 ]; then
  echo "DEVICE FAIL: park deadman did not exit within 12s of cancellation" >&2
  exit 1
fi
if ! dsh "test ! -f $DTMP/deadman.fired" >/dev/null 2>&1; then
  echo "DEVICE FAIL: park deadman FIRED during a healthy run (window/cancel defect — do not trust this run's park hygiene)" >&2
  exit 1
fi
DEADMAN_ARMED=0
echo "   live run done (host-observed $((t1 - t0)) s; app rc 0; daemon restored; frontend restored; deadman cancelled without firing)"
pullv "$DTMP/g01.mus-out.txt" "$DEVB/g01.mus-out.txt"
pullv "$DTMP/g01.mus-tim.txt" "$DEVB/g01.mus-tim.txt"
pullv "$DTMP/g01.mus-log.txt" "$DEVB/g01.mus-log.txt"
pullv "$DTMP/g01.mus-lat.txt" "$DEVB/g01.mus-lat.txt"

echo "== [7/7] host judgment: stream + timing + audio + music + sidecar =="
rm -f "$DEVB/g01.mus-run.json"
node "$SIM/wrap-run.js" g01 "$DEVB/g01.mus-out.txt" "$DEVB/g01.mus-run.json"
made "$DEVB/g01.mus-run.json"
node oracle/harness/verify-stream.js "$DEVB/g01.mus-run.json" "$FROZEN"
echo "   device stream verified (render+SFX+music did not perturb the sim on device)"

parse_timing_judge "$DEVB/g01.mus-tim.txt" "$frames"
printf '%s\n' "$timing_judge_out" | sed 's/^/   judge: /'
if [ "$full_p99_ns" -ge "$P99_FULL_LIMIT_NS" ]; then
  echo "MUSIC P99 FAIL: full-frame p99 ${full_p99_ms} ms (${full_p99_ns} ns) >= limit ${P99_FULL_LIMIT_NS} ns" >&2
  exit 1
fi
if [ "$render_p99_ns" -gt "$P99_RENDER_LIMIT_NS" ]; then
  echo "MUSIC P99 FAIL: render-only p99 ${render_p99_ms} ms (${render_p99_ns} ns) > limit ${P99_RENDER_LIMIT_NS} ns" >&2
  exit 1
fi
if [ "$skips" -ne 0 ] || [ "$rendered" -ne "$FRAMES_PIN" ]; then
  echo "DEVICE FAIL: gate run rendered $rendered/$FRAMES_PIN with $skips skips — a GATE pass may not consume the frameskip valve" >&2
  exit 1
fi
parse_app_summary "$DEVB/g01.mus-log.txt" "$frames" 1 "$BUDGET_NS"
if [ "$app_present_fails" -ne 0 ]; then
  echo "DEVICE FAIL: $app_present_fails failed presents on the device run" >&2
  exit 1
fi
if [ "$app_skips" -ne "$skips" ]; then
  echo "DEVICE FAIL: summary skips ($app_skips) != timing-artifact skips ($skips) — corrupt evidence" >&2
  exit 1
fi
if [ "$app_wall_ms" -lt "$WALL_MIN_MS" ] || [ "$app_wall_ms" -gt "$WALL_MAX_MS" ]; then
  echo "DEVICE FAIL: device wall clock ${app_wall_ms} ms outside [${WALL_MIN_MS},${WALL_MAX_MS}] ms — pacing is not running at 60 fps" >&2
  exit 1
fi
echo "   device summary OK (0 failed presents; wall ${app_wall_ms} ms in [${WALL_MIN_MS},${WALL_MAX_MS}])"

# audio: granted spec pinned into the grammar; underruns/badlen 0; cbs
# in the audio-check window
parse_audio_judge "$DEVB/g01.mus-log.txt" 44100 "$AUDIO_SAMPLES" 2 \
  --cbs-min "$CBS_MIN" --cbs-max "$CBS_MAX" --max-underruns 0 --max-badlen 0
if [ "$au_rc" != 0 ]; then
  echo "DEVICE FAIL: device audio judge failed leg(s): $au_fails (cbs=$au_cbs underruns=$au_underruns badlen=$au_badlen)" >&2
  exit 1
fi
echo "   device audio OK (cbs=$au_cbs in [$CBS_MIN,$CBS_MAX], underruns=0, badlen=0, spec 44100/$AUDIO_SAMPLES/2)"

# music: starves == 0 (the gate), refills in the derived window,
# musout == cbs*512 EXACTLY (valid because badlen == 0: every callback
# rendered exactly the granted 512 frames and the music channel
# advances once per output frame)
judge_music_gate "$DEVB/g01.mus-log.txt"
if [ "$mus_refills" -lt "$REFILLS_MIN" ] || [ "$mus_refills" -gt "$REFILLS_MAX" ]; then
  echo "DEVICE FAIL: music refills $mus_refills outside the derived sanity window [$REFILLS_MIN,$REFILLS_MAX] (dead reader / runaway refill)" >&2
  exit 1
fi
want_musout=$((au_cbs * 512))
if [ "$mus_out" -ne "$want_musout" ]; then
  echo "DEVICE FAIL: musout $mus_out != cbs*512 = $want_musout (badlen==0 makes this an exact cross-bind — a mismatch is dropped/duplicated callback consumption)" >&2
  exit 1
fi
echo "   device music OK (out=$mus_out == cbs*512, starves=0, refills=$mus_refills in [$REFILLS_MIN,$REFILLS_MAX])"

# sidecar: strict grammar; rows == refills; refill-read distribution
parse_muslat "$DEVB/g01.mus-lat.txt" "$mus_refills"
echo "   muslat: rows=$lat_rows p50=${lat_p50_ns}ns p99=${lat_p99_ns}ns max=${lat_max_ns}ns (refill-read distribution; PORTABILITY Layer 2 evidence)"

rig_no_commit_guard "$BUILD" "$DEVB" "$TABLES" "$AUDIO_OUT"

MUSIC_OK=1 # the exit-guard release: ONLY this line may precede a 0 exit
echo "DEVICE MUSIC OK (full p99 ${full_p99_ms} ms, underruns 0, starves 0, refill-read p99 ${lat_p99_ms} ms, skips ${skips}/${frames})"
