#!/usr/bin/env bash
# M3 task 6 done-check: audio-on (fix_plan §M3 task 6). Prints
#   DEVICE AUDIO OK (full p99 X ms, underruns=0, attempts=N)
# exit 0, iff ALL of:
#
#   1. SND1 PLANE (reuse, never reinvent): the pipeline audio stage
#      (node pipeline/run.js --only audio — the M1 task-4 committed
#      form; ffmpeg triple-pinned in pipeline/expected.json) generates
#      fresh SND1 artifacts into gitignored build output; pack-snd.js
#      packs the 180-name sfx map + blobs into ONE SNDPACK1 file, x2
#      byte-identical, count pinned (180), sha256 pinned below
#      (measured-then-frozen; a pipeline/ffmpeg drift fails HERE loudly,
#      never as silently different device audio). PROVENANCE: the pack
#      is Nintendo-derived — PRIVATE USE ONLY, gitignored build output,
#      pushed only to device scratch, never committed (the no-commit
#      guard asserts).
#   2. HOST TRUTH: x2 headless replays WITH --sndpack produce
#      byte-identical streams that pass the UNCHANGED
#      oracle/harness/verify-stream.js vs frozen g01 (the ml_snd_sink
#      tap + mixer bookkeeping must not perturb the sim), and their
#      audio summaries agree (deterministic voice starts/stops — the
#      HOST TRUTH the device leg is cross-asserted against; headless
#      grammar pins granted spec 0/0/0, so a host log can never pose as
#      device evidence).
#   3. STANDING TEETH (every run; the audio plane fails STRUCTURALLY,
#      never silently): T1 truncated pack -> loader loud death; T2 pack
#      with --drop-name land (g01's first fired sound, frame 75) ->
#      replay dies loud at the first play; T3 underrun-count
#      perturbation of a real log -> judge exit 2 (gate-fail); T4
#      malformed + duplicated audio-summary lines -> judge parse death;
#      T6 (iter 59, review-57 H) judge output truncated after
#      fail_underruns=1 (integrity terminator dropped) -> ingest
#      corruption death, never a retryable classification; T7 (review-57
#      M3) torn duplicate app-summary line -> parse death; T8 (review-57
#      L) doubled pack verdict -> death. PLUS the standing DEVICE probe
#      T5 (review-57 M1), step 7 below.
#   4. DEVICE LIVE RENDER+AUDIO: g01 replayed ON the FunKey-S through
#      the SDL1.2 backend with the audio callback LIVE (44100/S16LSB/
#      2ch/512 — the measured spike config; 8-voice SFX mixer fed by the
#      sim's sound-event queue), paced 60 fps, deadman-guarded park,
#      detached setsid + rc-file launch (task-4 apparatus inherited
#      VERBATIM).
#   5. DEVICE JUDGMENT (all host-side; the device never self-reports):
#      stream -> UNCHANGED verify-stream.js vs frozen g01 (audio must
#        not perturb the sim ON DEVICE);
#      timing -> judge-render-timing.js: full-frame p99 (sim+render+
#        present work time; the audio callback's cost is included BY
#        CONSTRUCTION — same single-core app, audio thread live)
#        < 16,670,000 ns AND render-only p99 <= 8,000,000 ns;
#      audio -> judge-audio-summary.js (anchored whitelist grammar,
#        granted spec 44100/512/2 pinned into the pattern):
#        underruns == 0 (ASSERTED, never merely printed — the spike's
#        late200 proxy: callback gap > 2x the 11.61 ms period), badlen
#        == 0 (ABI tripwire), callback count within the frozen cadence
#        window (proves the audio thread ran the WHOLE match), device
#        voice starts/stops == the host-truth counts (the event plane
#        is deterministic; divergence while the stream matches = sink
#        wiring defect, HARD fail);
#      hygiene -> app summary whitelist grammar: 0 failed presents,
#        skips == 0 + rendered == 3600 (gate may not consume the
#        frameskip valve), wall in [58,66] s.
#
# RETRY POLICY (pre-registered, AGENT-LOG iter 57 — the measured
# transient-spike class: 2/6 device runs show one ~19 ms sim frame):
# at most ATTEMPTS_MAX=2 paced device attempts. An attempt may be
# retried ONLY when its sole failures are skips>0 and/or underruns>0
# (both downstream symptoms of one transient stall); the stream, p99,
# and every audio-structural leg must pass on EVERY attempt — any such
# failure is immediate, unretried gate failure. Every attempt is
# logged, counted, and the attempt count is printed in the OK line.
# CLASSIFICATION INTEGRITY (iter 59, review-57 H): retryability is
# bound to the ACTUAL COUNTERS (skips>0 / underruns>0 with every other
# counter clean vs its pin), NEVER to the judge's optional fail_*
# lines; the judge's verdict block must terminate with its
# judge_complete=1 integrity line — missing/partial/truncated judge
# output is a CORRUPTION death, never a retry.
#
# Rig plumbing INHERITED from port/sim/device/riglib.sh (Tier-A arc,
# VERDICT: GO): nonce-dsh, pullv, rm-before-produce+made(), shared
# stamp-cached arm build (this script's bytes are stamp input via
# RIG_SCRIPTS), rehash-adjacent-to-push + push provenance, the SHARED
# no-reclaim device-keyed lock, the no-commit guard. Startup
# normalization chokepoint + inherited-state pessimism + deadman
# apparatus copied VERBATIM from check-device-render.sh (iters 50-56).
#
# Device hygiene: writes only /tmp/mlfk + /mnt/mlfk-scratch + the
# frontend-park marker /mnt/disable_frontend (removed on exit, trap'd);
# own processes killed on exit.
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
AUDIO_OUT=pipeline/build/audio-dev
FDC=oracle/fdlibm-crosscheck
DIST="${MELEELIGHT_CLONE:-$HOME/.cache/meleelight-funkey-s/upstream}"
DTMP=/tmp/mlfk
DSD=/mnt/mlfk-scratch
mkdir -p "$BUILD" "$DEVB"

# --- frozen pins (M3 task 6; changing any is a reviewed repo change) ---------
P99_FULL_LIMIT_NS=16670000  # PLAN §4/M3 frame budget (16.67 ms)
P99_RENDER_LIMIT_NS=8000000 # PLAN §5 render allowance (kept from task 4)
BUDGET_NS=16666667          # 60 fps pacing budget for the live run
FRAMES_PIN=3600             # task-4 literal pin; manifest cross-asserted
WALL_MIN_MS=58000           # task-4 measured paced-run wall window
WALL_MAX_MS=66000
AUDIO_RATE=44100            # the measured spike verdict (PLAN §7);
AUDIO_SAMPLES=512           # pinned into the judge's grammar — a
AUDIO_CHANNELS=2            # renegotiated device cannot even parse
SND_COUNT_PIN=180           # SND1 sfx map size (pipeline/expected.json)
# Pack identity (measured-then-frozen, iter 57): sha256 of the SNDPACK1
# built from the pinned pipeline audio artifacts (ffmpeg 8.1.1 triple
# pin upstream of this). Content pin only — freshness is rm-before-
# produce + the x2 cmp below.
SNDPACK_SHA256=f69579082fe569249879faa5ceccb7a810d94d8092695ddc8bb543f3bda3ccb4
# Callback-cadence window (measured-then-frozen, iter 57): nominal
# 512/44100 = 11.61 ms -> ~86.13 cb/s; the audio device is open from
# just before the frame loop to just after it (~60.3 s measured), so a
# healthy run sits near 5190. Window = the task-4 wall window mapped to
# callbacks with margin; a stalled/dead callback thread (cbs ~ 0) or a
# renegotiated cadence lands far outside.
CBS_MIN=4900
CBS_MAX=5900
ATTEMPTS_MAX=2              # retry policy (header) — pre-registered
# T5 STANDING STARVATION PROBE (iter 59, review-57 M1 — frozen at the
# iter-57 measured probe config, which counted 16-17 underruns): a
# 64-sample buffer under an unpaced CPU-saturating replay (budget 1000
# ns -> the frame loop never sleeps) starves the callback thread. The
# probe runs INSIDE every check invocation and MUST count >0 underruns
# AND be rejected by the judge — proving the gap accounting live in
# the FAILING direction per run (T3 only proves the judge; T5 proves
# the device-side instrumentation).
T5_SAMPLES=64
T5_FRAMES=600
T5_BUDGET_NS=1000
T5_TRIES=20         # x2 s host poll for the probe's detached rc file
DEADMAN_S="${MLFK_DEADMAN_S:-420}" # park deadman window: covers the T5
                    # probe (~10 s) + both paced attempts (~2x62 s) +
                    # pulls/judgment between
                    # (MLFK_DEADMAN_S = negative-testing override ONLY)
APPRC_TRIES=90      # x2 s host poll per attempt for the detached rc file
GFXDATA_FROZEN=$GFX/gfxdata-frozen.txt
GFXDATA_SHA256=5499a3dd5fc374d6ed988faf0bef6fa2e189eb314e892bdd83c7534dc0865c94

source port/sim/device/adbsh.sh # (also defines $DEV — it keys the lock)
source port/sim/device/riglib.sh

rig_lock_acquire

# INHERITED-STATE PESSIMISM (task-4 apparatus VERBATIM — iter 56,
# review-55 H; see check-device-render.sh for the full provenance
# comments): from the instant the lock is held, a PRIOR run's
# park/deadman state is treated as POSSIBLY PRESENT. Only the step-0
# normalization below may clear the preserve flag, after POSITIVELY
# verifying inherited-state ownership.
RIG_PRESERVE_DTMP=1
PARKED=0
DEADMAN_ARMED=0

task6_cleanup() {
  local prc restore_verified
  prc=0
  rig_dsh_retry "pkill gfx_device" || prc=$?
  case "$prc" in
    0) echo "WARN: gfx_device was still running at cleanup — killed" >&2 ;;
    1) : ;; # no process matched — the healthy path
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
    if [ "$restore_verified" = 1 ]; then
      rig_dsh_retry "touch $DTMP/deadman.cancel" \
        || echo "WARN: could not cancel the park deadman — it will fire once (idempotent actions) within ${DEADMAN_S}s" >&2
    else
      echo "WARN: frontend restore unverified — leaving the deadman ARMED as the backstop (its purpose)" >&2
    fi
  fi
  if [ "$restore_verified" != 1 ]; then
    RIG_PRESERVE_DTMP=1
  fi
  rig_cleanup
}
trap task6_cleanup EXIT

require_device

echo "== [0/8] stale-state startup normalization (cross-run chokepoint) =="
# VERBATIM task-4 chokepoint (iters 55-56; provenance comments in
# check-device-render.sh): every stale prior-run park/deadman state is
# normalized HERE, before any of this run's own parking/build.
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
    for _ in $(seq 1 6); do # §7#1 bounded foreground poll
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

# parse_timing_judge <timing-file> <frames> — VERBATIM task-4 parser
# (check-device-render.sh provenance: iters 43/52/54 — strict judge
# grammar, duplicate-key death, digit bounds before bash arithmetic).
parse_timing_judge() {
  local tf="$1" fr="$2" jout jk jv dup
  unset full_p99_ns full_p99_ms render_p99_ns render_p99_ms sim_p99_ms \
    present_p99_ms skips rendered
  jout="$(node "$GFX/judge-render-timing.js" "$tf" "$fr")" || {
    echo "DEVICE FAIL: timing judgment failed for $tf" >&2
    exit 1
  }
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
        : # reported by the judge; not asserted here
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

# parse_app_summary <log> <frames> <pace> <budget-ns> — task-4 parser
# (check-device-render.sh provenance: iters 52/54 — anchored full-line
# whitelist grammar, pinned frames/pace/budget, digit bounds) PLUS the
# iter-59 resemblance rule (review-57 M3): the log must carry exactly
# one FULL-grammar summary line AND zero summary-RESEMBLING malformed
# lines. Resemblance measured from the full real corpus (gfx_app.c's
# stderr producers + iter-57/58 host+device logs): ONLY the summary
# line matches `^gfx_app: [0-9]` — the legit non-summary lines start
# with letters ("gfx_app: skipped frames:", "gfx_app: bad argument")
# or use a different prefix ("gfx_app audio:", judged separately). A
# torn/stale duplicate therefore = corruption death, never acceptance
# of the one line that happens to parse.
parse_app_summary() {
  local log="$1" fr="$2" pace="$3" budget="$4" re cnt line rcnt
  unset app_skips app_present_fails app_wall_ms
  re="^gfx_app: ${fr} frames, [0-9]{1,12} render skips, [0-9]{1,12} failed presents, wall [0-9]{1,12} ms, pace=${pace} budget=${budget} ns\$"
  cnt="$(grep -Ec "$re" "$log")" || true
  if [ "$cnt" != 1 ]; then
    echo "DEVICE FAIL: app log $log has $cnt lines matching the pinned summary grammar (want exactly 1: frames=$fr pace=$pace budget=$budget ns)" >&2
    exit 1
  fi
  rcnt="$(grep -Ec '^gfx_app: [0-9]' "$log")" || true
  if [ "$rcnt" != 1 ]; then
    echo "DEVICE FAIL: app log $log has $rcnt summary-resembling 'gfx_app: <digit>...' lines but exactly 1 matches the full grammar — torn/stale duplicate summary = corrupt evidence" >&2
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

# audio_judge_ingest <judge-stdout> <judge-rc> — the strict verdict-
# block ingester (iter 59, review-57 H; factored out of
# parse_audio_judge so the T6 truncation tooth exercises the REAL
# validation path in a subshell). Requirements, all corruption deaths:
# rc in {0,2}; the LAST line is EXACTLY `judge_complete=1` (the judge's
# mandatory integrity terminator — truncated/partial judge output can
# never be classified, in particular never as "retryable"); no
# duplicate keys; every key from the closed whitelist with bounded
# values; all six counters present; rc<->fail_* coherence. Sets:
# au_cbs au_underruns au_badlen au_starts au_stops au_steals au_fails
# (space-joined fail legs, empty = all green) and au_rc (0 green, 2 =
# >=1 assertion violated). fail_* lines are REPORTING — decision sites
# bind to the counters (see the device loop / host truth legs).
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

# parse_audio_judge <log> <rate> <samples> <channels> [extra judge args...]
# Runs judge-audio-summary.js (anchored grammar; assertions inside the
# judge) and ingests its verdict block via audio_judge_ingest above.
parse_audio_judge() {
  local log="$1" rate="$2" samples="$3" channels="$4"
  shift 4
  local jout jrc
  jrc=0
  jout="$(node "$GFX/judge-audio-summary.js" "$log" \
    --rate "$rate" --samples "$samples" --channels "$channels" "$@")" || jrc=$?
  audio_judge_ingest "$jout" "$jrc"
}

# pack_verdict_assert <pack-snd-stdout> — the packer verdict grammar
# (iter 59, review-57 L): the output must be EXACTLY ONE line and that
# line must match the anchored full-line pattern $pack_line_re (set in
# step 2). Any extra/malformed/doubled line = death.
pack_verdict_assert() {
  local pout="$1" plines pcnt
  plines="$(printf '%s\n' "$pout" | grep -c '')" || true
  pcnt="$(printf '%s\n' "$pout" | grep -Ec "$pack_line_re")" || true
  if [ "$plines" != 1 ] || [ "$pcnt" != 1 ]; then
    echo "DEVICE FAIL: pack-snd output failed the exactly-one-full-line verdict grammar (lines=$plines full-matches=$pcnt, want 1/1 with count=${SND_COUNT_PIN}): '$pout'" >&2
    exit 1
  fi
}

echo "== [1/8] host data plane (g01 params + M1 tables + SIMDATA1 + trace + GFXDATA pin) =="
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
echo "   gfxdata-frozen pin OK ($GFXDATA_SHA256)"

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

echo "== [2/8] SND1 plane: pipeline audio stage (REUSED) -> SNDPACK1 (x2, pinned) =="
# The M1 task-4 committed pipeline stage generates the SND1 artifacts
# fresh (ffmpeg triple-pinned in pipeline/expected.json — a different
# ffmpeg fails INSIDE the stage before converting anything).
rm -rf "$AUDIO_OUT"
node pipeline/run.js --only audio --dist "$DIST" --out "$AUDIO_OUT"
made "$AUDIO_OUT/sounds.json"
# pack x2 -> byte-identical (determinism), strict output grammar with
# the count pinned, then the frozen content pin.
pack_line_re="^pack-snd OK count=${SND_COUNT_PIN} dataBytes=[0-9]{1,12} fileBytes=[0-9]{1,12}\$"
for side in a b; do
  rm -f "$BUILD/sndpack-$side.bin"
  pout="$(node "$GFX/pack-snd.js" "$AUDIO_OUT" "$BUILD/sndpack-$side.bin")" || {
    echo "DEVICE FAIL: pack-snd.js failed (side $side)" >&2
    exit 1
  }
  pack_verdict_assert "$pout" # iter 59, review-57 L: exactly-one full line
  made "$BUILD/sndpack-$side.bin"
done
cmp "$BUILD/sndpack-a.bin" "$BUILD/sndpack-b.bin"
rm -f "$BUILD/sndpack.bin"
cp "$BUILD/sndpack-a.bin" "$BUILD/sndpack.bin"
made "$BUILD/sndpack.bin"
psum="$(rig_host_sha256 "$BUILD/sndpack.bin")" || exit 1
if [ "$psum" != "$SNDPACK_SHA256" ]; then
  echo "DEVICE FAIL: sndpack sha256 $psum != pinned $SNDPACK_SHA256 — pipeline/ffmpeg drift (reviewed re-freeze required, never silent)" >&2
  exit 1
fi
echo "   SNDPACK1 x2 byte-identical, count=$SND_COUNT_PIN, sha pinned ($SNDPACK_SHA256)"

echo "== [3/8] host build: headless + SDL2 backends (ONE backend TU per binary) =="
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
    "${SIM_TUS[@]}" \
    port/sim/characters/shared/moves/*.c \
    port/sim/characters/fox/moves/*.c \
    port/sim/characters/falco/moves/*.c \
    port/sim/characters/falcon/moves/*.c \
    port/sim/characters/marth/moves/*.c \
    port/sim/characters/puff/moves/*.c \
    $(sdl2-config --libs) -lm
  made "$BUILD/gfx_app_sdl2"
  echo "   SDL2 dev backend built (audio seam compiles against SDL2's legacy API; not run — no GUI in a check)"
else
  echo "DEVICE FAIL: sdl2-config not found — the SDL2 dev backend must BUILD (three-backend seam)" >&2
  exit 1
fi
echo "   host build OK (raster TU -O3, all else -O2; -ffp-contract=off everywhere)"

echo "== [4/8] host truth: x2 headless audio replays + stream verify + event counts =="
for side in a b; do
  rm -f "$BUILD/g01.au-out-$side.txt" "$BUILD/g01.au-tim-$side.txt" \
    "$BUILD/g01.au-log-$side.txt"
  "$BUILD/gfx_app_headless" \
    --trace "$DEVB/g01.trace.txt" --simdata "$DEVB/simdata.txt" \
    --gfxdata "$GFXDATA_FROZEN" --anim-dir "$TABLES" \
    --seed "$seed" --p1 "$p1" --p2 "$p2" --stage "$stage" \
    --frames "$frames" --pace 0 \
    --out "$BUILD/g01.au-out-$side.txt" \
    --timing "$BUILD/g01.au-tim-$side.txt" \
    --sndpack "$BUILD/sndpack.bin" \
    2> "$BUILD/g01.au-log-$side.txt"
  made "$BUILD/g01.au-out-$side.txt" "$BUILD/g01.au-tim-$side.txt" \
    "$BUILD/g01.au-log-$side.txt"
done
cmp "$BUILD/g01.au-out-a.txt" "$BUILD/g01.au-out-b.txt"
echo "   x2 headless audio replays byte-identical (stream)"
rm -f "$BUILD/g01.au-run.json"
node "$SIM/wrap-run.js" g01 "$BUILD/g01.au-out-a.txt" "$BUILD/g01.au-run.json"
made "$BUILD/g01.au-run.json"
node oracle/harness/verify-stream.js "$BUILD/g01.au-run.json" "$FROZEN"
echo "   host audio-on stream verified (ml_snd_sink + mixer do not perturb the sim)"
# host summary legs: pace-0 headless — 0 skips, 0 failed presents;
# audio judge under the HEADLESS granted-spec grammar (0/0/0): zero
# callbacks/underruns/badlen, and the deterministic event counts.
parse_app_summary "$BUILD/g01.au-log-a.txt" "$frames" 0 "$BUDGET_NS"
if [ "$app_skips" -ne 0 ] || [ "$app_present_fails" -ne 0 ]; then
  echo "DEVICE FAIL: host audio leg reports skips=$app_skips presentFails=$app_present_fails (want 0/0)" >&2
  exit 1
fi
# Decision sites bind to the COUNTERS (iter 59, review-57 H), with the
# judge rc as a cross-check — never to fail_* presence alone.
parse_audio_judge "$BUILD/g01.au-log-a.txt" 0 0 0 \
  --cbs-min 0 --cbs-max 0 --max-underruns 0 --max-badlen 0
if [ "$au_rc" != 0 ] || [ "$au_cbs" -ne 0 ] || [ "$au_underruns" -ne 0 ] \
  || [ "$au_badlen" -ne 0 ]; then
  echo "DEVICE FAIL: host truth run a failed audio legs (rc=$au_rc cbs=$au_cbs underruns=$au_underruns badlen=$au_badlen fails='$au_fails') — headless must report 0 cbs/underruns/badlen" >&2
  exit 1
fi
HOST_STARTS=$au_starts
HOST_STOPS=$au_stops
if [ "$HOST_STARTS" -lt 1 ]; then
  echo "DEVICE FAIL: host truth run played ZERO sounds — g01 must exercise the audio plane (vacuous audio evidence)" >&2
  exit 1
fi
parse_audio_judge "$BUILD/g01.au-log-b.txt" 0 0 0 \
  --cbs-min 0 --cbs-max 0 --max-underruns 0 --max-badlen 0 \
  --expect-starts "$HOST_STARTS" --expect-stops "$HOST_STOPS"
if [ "$au_rc" != 0 ] || [ "$au_cbs" -ne 0 ] || [ "$au_underruns" -ne 0 ] \
  || [ "$au_badlen" -ne 0 ] || [ "$au_starts" -ne "$HOST_STARTS" ] \
  || [ "$au_stops" -ne "$HOST_STOPS" ]; then
  echo "DEVICE FAIL: host truth runs disagree (a: starts=$HOST_STARTS stops=$HOST_STOPS; b: rc=$au_rc starts=$au_starts stops=$au_stops fails='$au_fails') — nondeterministic event plane" >&2
  exit 1
fi
echo "   host truth: starts=$HOST_STARTS stops=$HOST_STOPS (x2 agree; cbs=0 headless)"

echo "== [5/8] standing teeth: pack structure + dropped blob + judge legs =="
# T1: truncated pack -> loader loud death BEFORE any frame runs.
rm -f "$BUILD/sndpack-trunc.bin"
pk_bytes="$(wc -c < "$BUILD/sndpack.bin" | tr -d ' ')"
if ! [[ "$pk_bytes" =~ ^[0-9]{1,12}$ ]] || [ "$pk_bytes" -le 16 ]; then
  echo "DEVICE FAIL: sndpack byte count implausible ('$pk_bytes')" >&2
  exit 1
fi
head -c "$((pk_bytes - 1))" "$BUILD/sndpack.bin" > "$BUILD/sndpack-trunc.bin"
made "$BUILD/sndpack-trunc.bin"
t1rc=0
"$BUILD/gfx_app_headless" \
  --trace "$DEVB/g01.trace.txt" --simdata "$DEVB/simdata.txt" \
  --gfxdata "$GFXDATA_FROZEN" --anim-dir "$TABLES" \
  --seed "$seed" --p1 "$p1" --p2 "$p2" --stage "$stage" \
  --frames 10 --pace 0 \
  --out "$BUILD/t1-out.txt" --timing "$BUILD/t1-tim.txt" \
  --sndpack "$BUILD/sndpack-trunc.bin" \
  2> "$BUILD/t1-log.txt" || t1rc=$?
if [ "$t1rc" -eq 0 ] || ! grep -q "truncated or corrupt pack" "$BUILD/t1-log.txt"; then
  echo "DEVICE FAIL: T1 tooth — truncated pack did NOT die loud (rc $t1rc)" >&2
  exit 1
fi
# T2: pack missing one SND1 blob g01 fires (land, frame 75) -> loud
# death at the first play event (the "dropped SND1 blob" tooth).
rm -f "$BUILD/sndpack-drop.bin"
node "$GFX/pack-snd.js" "$AUDIO_OUT" "$BUILD/sndpack-drop.bin" \
  --drop-name land >/dev/null 2>&1
made "$BUILD/sndpack-drop.bin"
t2rc=0
"$BUILD/gfx_app_headless" \
  --trace "$DEVB/g01.trace.txt" --simdata "$DEVB/simdata.txt" \
  --gfxdata "$GFXDATA_FROZEN" --anim-dir "$TABLES" \
  --seed "$seed" --p1 "$p1" --p2 "$p2" --stage "$stage" \
  --frames "$frames" --pace 0 \
  --out "$BUILD/t2-out.txt" --timing "$BUILD/t2-tim.txt" \
  --sndpack "$BUILD/sndpack-drop.bin" \
  2> "$BUILD/t2-log.txt" || t2rc=$?
if [ "$t2rc" -eq 0 ] || ! grep -q "play event for a name not in the pack" "$BUILD/t2-log.txt"; then
  echo "DEVICE FAIL: T2 tooth — dropped SND1 blob did NOT die loud at play time (rc $t2rc)" >&2
  exit 1
fi
# T3: underrun-count perturbation of a REAL log -> judge names the leg
# and exits 2 (the gate-fail direction).
rm -f "$BUILD/t3-log.txt"
sed 's/ 0 underruns,/ 3 underruns,/' "$BUILD/g01.au-log-a.txt" > "$BUILD/t3-log.txt"
made "$BUILD/t3-log.txt"
t3rc=0
t3out="$(node "$GFX/judge-audio-summary.js" "$BUILD/t3-log.txt" \
  --rate 0 --samples 0 --channels 0 --max-underruns 0)" || t3rc=$?
if [ "$t3rc" -ne 2 ] || ! printf '%s\n' "$t3out" | grep -q '^fail_underruns=1$'; then
  echo "DEVICE FAIL: T3 tooth — perturbed underrun count did NOT fail the judge leg (rc $t3rc)" >&2
  exit 1
fi
# T4: grammar corruption — a malformed audio line AND a duplicated
# audio line must both be parse DEATHS (rc 1), never partial parses.
rm -f "$BUILD/t4a-log.txt" "$BUILD/t4b-log.txt"
sed 's/ steals,/ stealz,/' "$BUILD/g01.au-log-a.txt" > "$BUILD/t4a-log.txt"
{ cat "$BUILD/g01.au-log-a.txt"; grep '^gfx_app audio:' "$BUILD/g01.au-log-a.txt"; } > "$BUILD/t4b-log.txt"
made "$BUILD/t4a-log.txt" "$BUILD/t4b-log.txt"
for tl in t4a t4b; do
  t4rc=0
  node "$GFX/judge-audio-summary.js" "$BUILD/$tl-log.txt" \
    --rate 0 --samples 0 --channels 0 >/dev/null 2>&1 || t4rc=$?
  if [ "$t4rc" -ne 1 ]; then
    echo "DEVICE FAIL: T4 tooth ($tl) — corrupt audio summary did NOT die as grammar corruption (rc $t4rc)" >&2
    exit 1
  fi
done
# T6 (iter 59, review-57 H): judge output TRUNCATED right after
# fail_underruns=1 (the integrity terminator dropped) must die in the
# REAL ingest path as corruption — never be classified retryable.
# Positive control first: the untruncated output ends with the
# terminator and exits 2.
t6rc=0
t6out="$(node "$GFX/judge-audio-summary.js" "$BUILD/t3-log.txt" \
  --rate 0 --samples 0 --channels 0 --max-underruns 0)" || t6rc=$?
if [ "$t6rc" -ne 2 ] \
  || [ "$(printf '%s\n' "$t6out" | tail -n 1)" != "judge_complete=1" ] \
  || ! printf '%s\n' "$t6out" | grep -q '^fail_underruns=1$'; then
  echo "DEVICE FAIL: T6 positive control — judge on the T3 log must exit 2 with fail_underruns=1 and the judge_complete=1 terminator (rc $t6rc)" >&2
  exit 1
fi
t6trunc="$(printf '%s\n' "$t6out" | sed -n '1,/^fail_underruns=1$/p')"
t6irc=0
( audio_judge_ingest "$t6trunc" 2 ) >/dev/null 2>&1 || t6irc=$?
if [ "$t6irc" -eq 0 ]; then
  echo "DEVICE FAIL: T6 tooth — judge output truncated after fail_underruns=1 was ACCEPTED by the ingest (must be corruption death, never a retry)" >&2
  exit 1
fi
# T7 (iter 59, review-57 M3): a torn duplicate of the app summary line
# appended to a REAL log must be a parse death (summary-resembling
# malformed line = corruption).
rm -f "$BUILD/t7-log.txt"
{ cat "$BUILD/g01.au-log-a.txt"; printf '%s\n' "gfx_app: ${frames} frames, 0 render sk"; } > "$BUILD/t7-log.txt"
made "$BUILD/t7-log.txt"
t7rc=0
( parse_app_summary "$BUILD/t7-log.txt" "$frames" 0 "$BUDGET_NS" ) >/dev/null 2>&1 || t7rc=$?
if [ "$t7rc" -eq 0 ]; then
  echo "DEVICE FAIL: T7 tooth — torn duplicate summary line was ACCEPTED by parse_app_summary" >&2
  exit 1
fi
# T8 (iter 59, review-57 L): a doubled pack verdict must die.
t8rc=0
( pack_verdict_assert "pack-snd OK count=${SND_COUNT_PIN} dataBytes=1 fileBytes=1
pack-snd OK count=${SND_COUNT_PIN} dataBytes=1 fileBytes=1" ) >/dev/null 2>&1 || t8rc=$?
if [ "$t8rc" -eq 0 ]; then
  echo "DEVICE FAIL: T8 tooth — doubled pack-snd verdict was ACCEPTED" >&2
  exit 1
fi
echo "   standing teeth OK (T1 truncation death, T2 dropped-blob death at play, T3 underrun leg fires, T4 grammar deaths x2, T6 truncated-judge corruption death, T7 torn-summary death, T8 doubled-pack-verdict death)"

echo "== [6/8] armv7 build (shared rig stamp) + push + provenance =="
rig_arm_build
rig_stamp_rehash gfx_device
dsh "rm -rf $DTMP $DSD && mkdir -p $DTMP $DSD"
adb -s "$DEV" push "$DEVB/gfx_device" "$DEVB/simdata.txt" \
  "$DEVB/g01.trace.txt" "$GFXDATA_FROZEN" \
  "$TABLES/$ANIM_P1" "$TABLES/$ANIM_P2" "$DTMP/" >/dev/null
rig_push_provenance "$DTMP" gfx_device
dsh "chmod +x $DTMP/gfx_device"
# SND1 pack -> SD scratch (big artifact; blobs NEVER live in the repo)
adb -s "$DEV" push "$BUILD/sndpack.bin" "$DSD/" >/dev/null
for hf in "$DEVB/simdata.txt" "$DEVB/g01.trace.txt" "$GFXDATA_FROZEN" \
          "$TABLES/$ANIM_P1" "$TABLES/$ANIM_P2"; do
  bn="$(basename "$hf")"
  hsum="$(rig_host_sha256 "$hf")" || exit 1
  dsum="$(rig_dev_sha256 "$DTMP/$bn")" || exit 1
  if [ "$dsum" != "$hsum" ]; then
    echo "DEVICE FAIL: pushed $bn device sha ($dsum) != host sha ($hsum)" >&2
    exit 1
  fi
done
# pack push provenance: device bytes == host bytes == the frozen pin
dsum="$(rig_dev_sha256 "$DSD/sndpack.bin")" || exit 1
if [ "$dsum" != "$SNDPACK_SHA256" ]; then
  echo "DEVICE FAIL: device-side sndpack sha ($dsum) != frozen pin ($SNDPACK_SHA256)" >&2
  exit 1
fi
echo "   pushed data sha-verified on device (simdata, trace, gfxdata, 2 anim bins, sndpack -> $DSD)"

echo "== [7/8] device: T5 starvation probe + LIVE paced g01 render+audio (deadman-guarded park; <= $ATTEMPTS_MAX attempts) =="
# Deadman + launcher: task-4 apparatus VERBATIM (provenance comments in
# check-device-render.sh, iters 50-55). The launcher takes the attempt
# number as \$1 so retries never overwrite attempt-1 evidence.
DM_NONCE="$RANDOM$RANDOM$$"
rm -f "$BUILD/deadman.sh"
cat > "$BUILD/deadman.sh" << EOF
#!/bin/sh
# generated by check-device-audio.sh — frontend-park DEADMAN (iter 52 design)
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
fi
rm -f $DTMP/deadman.pid
exit 0
EOF
made "$BUILD/deadman.sh"
# One launcher PER ATTEMPT with literals baked in — the task-4
# single-quoted setsid pattern kept VERBATIM (no nested-expansion
# quoting risk); retries never overwrite attempt-1 evidence.
launch_files=("$BUILD/deadman.sh")
for at in $(seq 1 "$ATTEMPTS_MAX"); do
  rm -f "$BUILD/audio-launch-$at.sh"
  cat > "$BUILD/audio-launch-$at.sh" << EOF
#!/bin/sh
# generated by check-device-audio.sh — paced live render+audio launcher (attempt $at)
cd $DTMP || exit 9
rm -f audio.apprc.$at gfx.pid.$DM_NONCE
setsid sh -c './gfx_device \
  --trace $DTMP/g01.trace.txt --simdata $DTMP/simdata.txt \
  --gfxdata $DTMP/gfxdata-frozen.txt --anim-dir $DTMP \
  --seed $seed --p1 $p1 --p2 $p2 --stage $stage --frames $frames \
  --pace 1 --budget-ns $BUDGET_NS \
  --sndpack $DSD/sndpack.bin \
  --out $DTMP/g01.dev-out.$at.txt --timing $DTMP/g01.dev-tim.$at.txt \
  2> $DTMP/g01.dev-log.$at.txt & \
  echo \$! > $DTMP/gfx.pid.$DM_NONCE; \
  wait \$!; \
  echo "RC=\$?" > $DTMP/audio.apprc.$at' \
  </dev/null >/dev/null 2>&1 &
sleep 2
EOF
  made "$BUILD/audio-launch-$at.sh"
  launch_files+=("$BUILD/audio-launch-$at.sh")
done
# T5 standing starvation probe launcher (iter 59, review-57 M1): the
# frozen iter-57 probe config — 64-sample buffer, unpaced (1000 ns
# budget -> the frame loop never sleeps) CPU-saturating 600-frame
# replay. Same detached setsid + rc-file + deadman-pid pattern as the
# attempts (a hung probe is killed by the armed deadman).
rm -f "$BUILD/t5-launch.sh"
cat > "$BUILD/t5-launch.sh" << EOF
#!/bin/sh
# generated by check-device-audio.sh — T5 standing starvation probe
cd $DTMP || exit 9
rm -f t5.apprc gfx.pid.$DM_NONCE
setsid sh -c './gfx_device \
  --trace $DTMP/g01.trace.txt --simdata $DTMP/simdata.txt \
  --gfxdata $DTMP/gfxdata-frozen.txt --anim-dir $DTMP \
  --seed $seed --p1 $p1 --p2 $p2 --stage $stage --frames $T5_FRAMES \
  --pace 1 --budget-ns $T5_BUDGET_NS \
  --sndpack $DSD/sndpack.bin --audio-samples $T5_SAMPLES \
  --out $DTMP/t5-out.txt --timing $DTMP/t5-tim.txt \
  2> $DTMP/t5-log.txt & \
  echo \$! > $DTMP/gfx.pid.$DM_NONCE; \
  wait \$!; \
  echo "RC=\$?" > $DTMP/t5.apprc' \
  </dev/null >/dev/null 2>&1 &
sleep 2
EOF
made "$BUILD/t5-launch.sh"
launch_files+=("$BUILD/t5-launch.sh")
adb -s "$DEV" push "${launch_files[@]}" "$DTMP/" >/dev/null
for hf in "${launch_files[@]}"; do
  bn="$(basename "$hf")"
  hsum="$(rig_host_sha256 "$hf")" || exit 1
  dsum="$(rig_dev_sha256 "$DTMP/$bn")" || exit 1
  if [ "$dsum" != "$hsum" ]; then
    echo "DEVICE FAIL: pushed $bn device sha ($dsum) != host sha ($hsum)" >&2
    exit 1
  fi
done
dsh "chmod +x $DTMP/deadman.sh $DTMP/audio-launch-*.sh $DTMP/t5-launch.sh"

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

# -- T5 STANDING STARVATION PROBE (iter 59, review-57 M1) --------------------
# Runs INSIDE every check invocation, in the parked window, BEFORE the
# gate attempts: the 64-sample buffer under a CPU-saturating replay
# MUST count >0 underruns and be REJECTED by the judge — proving the
# device-side gap accounting live in the failing direction per run
# (T3 proves only the judge; this proves the instrumentation). The
# samples=64 grammar pin also proves a wrong-spec run cannot pose as
# the gate run. A CLEAN probe is a check FAILURE (pre-registered,
# AGENT-LOG iter 59 — never loosened to "record honestly").
echo "   -- T5 standing starvation probe (samples=$T5_SAMPLES frames=$T5_FRAMES budget=${T5_BUDGET_NS}ns) --"
dsh "sh -lc $DTMP/t5-launch.sh"
t5_seen=0
for _ in $(seq 1 "$T5_TRIES"); do # §7#1 bounded foreground poll
  if dsh "test -f $DTMP/t5.apprc" >/dev/null 2>&1; then t5_seen=1; break; fi
  sleep 2
done
if [ "$t5_seen" != 1 ]; then
  dsh "cat $DTMP/t5-log.txt" >&2 || true
  echo "DEVICE FAIL: T5 probe never finished (rc file absent after $((T5_TRIES * 2))s)" >&2
  exit 1
fi
pullv "$DTMP/t5.apprc" "$DEVB/t5.apprc"
if [ "$(cat "$DEVB/t5.apprc")" != "RC=0" ]; then
  dsh "cat $DTMP/t5-log.txt" >&2 || true
  echo "DEVICE FAIL: T5 probe app exited nonzero ($(cat "$DEVB/t5.apprc"))" >&2
  exit 1
fi
pullv "$DTMP/t5-log.txt" "$DEVB/t5-log.txt"
parse_audio_judge "$DEVB/t5-log.txt" \
  "$AUDIO_RATE" "$T5_SAMPLES" "$AUDIO_CHANNELS" --max-underruns 0
if [ "$au_rc" -ne 2 ] || [ "$au_underruns" -le 0 ] \
  || [ "$au_fails" != "underruns" ]; then
  echo "DEVICE FAIL: T5 standing probe did NOT starve (rc=$au_rc underruns=$au_underruns fails='$au_fails') — the gap accounting is not provably live; refutation path per AGENT-LOG iter 59 (one bounded re-measure, then STOP+report)" >&2
  exit 1
fi
echo "   T5 probe OK: underruns=$au_underruns counted and rejected (cbs=$au_cbs starts=$au_starts) — gap accounting LIVE"

attempt=0
attempt_pass=0
while [ "$attempt" -lt "$ATTEMPTS_MAX" ]; do
  attempt=$((attempt + 1))
  echo "   -- attempt $attempt/$ATTEMPTS_MAX --"
  t0=$(date +%s)
  dsh "sh -lc $DTMP/audio-launch-$attempt.sh"
  apprc_seen=0
  for _ in $(seq 1 "$APPRC_TRIES"); do # §7#1 bounded foreground poll
    if dsh "test -f $DTMP/audio.apprc.$attempt" >/dev/null 2>&1; then apprc_seen=1; break; fi
    sleep 2
  done
  t1=$(date +%s)
  if [ "$apprc_seen" != 1 ]; then
    dsh "cat $DTMP/g01.dev-log.$attempt.txt" >&2 || true
    echo "DEVICE FAIL: live run never finished (rc file absent after $((APPRC_TRIES * 2))s)" >&2
    exit 1
  fi
  pullv "$DTMP/audio.apprc.$attempt" "$DEVB/audio.apprc.$attempt"
  if [ "$(cat "$DEVB/audio.apprc.$attempt")" != "RC=0" ]; then
    dsh "cat $DTMP/g01.dev-log.$attempt.txt" >&2 || true
    echo "DEVICE FAIL: live app exited nonzero ($(cat "$DEVB/audio.apprc.$attempt"))" >&2
    exit 1
  fi
  dsh "test -s $DTMP/gfx.pid.$DM_NONCE"
  echo "   attempt $attempt run done (host-observed $((t1 - t0)) s; app rc 0)"
  pullv "$DTMP/g01.dev-out.$attempt.txt" "$DEVB/g01.au-dev-out.$attempt.txt"
  pullv "$DTMP/g01.dev-tim.$attempt.txt" "$DEVB/g01.au-dev-tim.$attempt.txt"
  pullv "$DTMP/g01.dev-log.$attempt.txt" "$DEVB/g01.au-dev-log.$attempt.txt"

  # --- HARD legs (must pass EVERY attempt; pre-registered) -------------------
  rm -f "$DEVB/g01.au-dev-run.$attempt.json"
  node "$SIM/wrap-run.js" g01 "$DEVB/g01.au-dev-out.$attempt.txt" \
    "$DEVB/g01.au-dev-run.$attempt.json"
  made "$DEVB/g01.au-dev-run.$attempt.json"
  node oracle/harness/verify-stream.js "$DEVB/g01.au-dev-run.$attempt.json" "$FROZEN"
  echo "   attempt $attempt: device stream verified (audio did not perturb the sim)"

  parse_timing_judge "$DEVB/g01.au-dev-tim.$attempt.txt" "$frames"
  printf '%s\n' "$timing_judge_out" | sed "s/^/   judge[$attempt]: /"
  if [ "$full_p99_ns" -ge "$P99_FULL_LIMIT_NS" ]; then
    echo "AUDIO P99 FAIL: full-frame p99 ${full_p99_ms} ms (${full_p99_ns} ns) >= limit ${P99_FULL_LIMIT_NS} ns (attempt $attempt — p99 legs are never retried)" >&2
    exit 1
  fi
  if [ "$render_p99_ns" -gt "$P99_RENDER_LIMIT_NS" ]; then
    echo "AUDIO P99 FAIL: render-only p99 ${render_p99_ms} ms (${render_p99_ns} ns) > limit ${P99_RENDER_LIMIT_NS} ns (attempt $attempt)" >&2
    exit 1
  fi
  if [ "$((skips + rendered))" -ne "$FRAMES_PIN" ]; then
    echo "DEVICE FAIL: timing artifact inconsistent (skips $skips + rendered $rendered != $FRAMES_PIN)" >&2
    exit 1
  fi

  parse_app_summary "$DEVB/g01.au-dev-log.$attempt.txt" "$frames" 1 "$BUDGET_NS"
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

  # Audio judge under the DEVICE granted-spec grammar. Structural legs
  # (badlen, cbs window, starts/stops vs host truth) are HARD; the
  # underruns leg alone is retryable (the transient-stall class).
  # CLASSIFICATION IS COUNTER-BOUND (iter 59, review-57 H): the hard/
  # retryable split below reads the ACTUAL counters, never the judge's
  # optional fail_* lines; the fail_* set is then CROSS-CHECKED against
  # a counter-derived expectation — any disagreement (a judge that
  # under- or over-reports) is a corruption death.
  parse_audio_judge "$DEVB/g01.au-dev-log.$attempt.txt" \
    "$AUDIO_RATE" "$AUDIO_SAMPLES" "$AUDIO_CHANNELS" \
    --cbs-min "$CBS_MIN" --cbs-max "$CBS_MAX" \
    --max-underruns 0 --max-badlen 0 \
    --expect-starts "$HOST_STARTS" --expect-stops "$HOST_STOPS"
  echo "   audio[$attempt]: cbs=$au_cbs underruns=$au_underruns badlen=$au_badlen starts=$au_starts stops=$au_stops steals=$au_steals"
  exp_fails=""
  if [ "$au_cbs" -lt "$CBS_MIN" ]; then exp_fails="$exp_fails cbs_low"; fi
  if [ "$au_cbs" -gt "$CBS_MAX" ]; then exp_fails="$exp_fails cbs_high"; fi
  if [ "$au_underruns" -gt 0 ]; then exp_fails="$exp_fails underruns"; fi
  if [ "$au_badlen" -gt 0 ]; then exp_fails="$exp_fails badlen"; fi
  if [ "$au_starts" -ne "$HOST_STARTS" ]; then exp_fails="$exp_fails starts"; fi
  if [ "$au_stops" -ne "$HOST_STOPS" ]; then exp_fails="$exp_fails stops"; fi
  exp_norm="$(printf '%s\n' $exp_fails | sort | tr '\n' ' ')"
  got_norm="$(printf '%s\n' $au_fails | sort | tr '\n' ' ')"
  if [ "$exp_norm" != "$got_norm" ]; then
    echo "DEVICE FAIL: judge fail_* set ('$au_fails') disagrees with the counter-derived expectation ('${exp_fails# }') — corrupt judge output" >&2
    exit 1
  fi
  hard_audio_fail=""
  if [ "$au_badlen" -ne 0 ]; then hard_audio_fail="$hard_audio_fail badlen"; fi
  if [ "$au_cbs" -lt "$CBS_MIN" ] || [ "$au_cbs" -gt "$CBS_MAX" ]; then
    hard_audio_fail="$hard_audio_fail cbs"
  fi
  if [ "$au_starts" -ne "$HOST_STARTS" ]; then hard_audio_fail="$hard_audio_fail starts"; fi
  if [ "$au_stops" -ne "$HOST_STOPS" ]; then hard_audio_fail="$hard_audio_fail stops"; fi
  if [ -n "$hard_audio_fail" ]; then
    echo "DEVICE FAIL: audio structural leg(s) failed:$hard_audio_fail (cbs=$au_cbs badlen=$au_badlen starts=$au_starts/$HOST_STARTS stops=$au_stops/$HOST_STOPS — hard, counter-bound, never retried)" >&2
    exit 1
  fi

  # --- RETRYABLE legs (skips == 0 AND underruns == 0) ------------------------
  if [ "$skips" -eq 0 ] && [ "$au_underruns" -eq 0 ]; then
    if [ "$rendered" -ne "$FRAMES_PIN" ]; then
      echo "DEVICE FAIL: rendered $rendered != literal pin $FRAMES_PIN with 0 skips — corrupt evidence" >&2
      exit 1
    fi
    attempt_pass=1
    break
  fi
  if [ "$attempt" -lt "$ATTEMPTS_MAX" ]; then
    echo "   attempt $attempt: retryable transient (skips=$skips underruns=$au_underruns) — RETRYING (logged + counted)" >&2
  else
    echo "   attempt $attempt: retryable transient (skips=$skips underruns=$au_underruns) — attempt budget exhausted" >&2
  fi
done

# restore + disarm (task-4 sequencing VERBATIM: marker VERIFIED gone
# before either disarm channel)
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
  echo "DEVICE FAIL: park deadman FIRED during a healthy run (window/cancel defect)" >&2
  exit 1
fi
DEADMAN_ARMED=0
echo "   frontend restored; deadman cancelled without firing"

if [ "$attempt_pass" != 1 ]; then
  echo "DEVICE FAIL: retryable legs (skips/underruns) did not pass within $ATTEMPTS_MAX attempts" >&2
  exit 1
fi

echo "== [8/8] no-commit guard =="
rig_no_commit_guard "$BUILD" "$DEVB" "$TABLES" "$AUDIO_OUT"

# Every value in this success line is ASSERTED above (iter-53 content-
# blind-banner rule): p99 < limit, underruns == 0, attempts bounded by
# the loop, cbs in the frozen window, starts/stops == host truth,
# skips == 0. (steals is deliberately NOT here — it is device-timing-
# dependent, unasserted, and reported only in the per-attempt detail.)
echo "DEVICE AUDIO OK (full p99 ${full_p99_ms} ms, underruns=${au_underruns}, attempts=${attempt}; cbs=${au_cbs} starts=${au_starts} stops=${au_stops} skips=${skips}/${frames})"
