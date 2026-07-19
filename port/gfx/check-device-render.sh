#!/usr/bin/env bash
# M3 task 4 done-check, EXTENDED by M4 task 3 (stage-surface legibility
# at device scale + the device rung for the M4 visual surface — vfx +
# overlay + banner + background live on the FunKey): platform seam +
# SDL1.2 device backend + LIVE device render. Prints
#   DEVICE RENDER OK (full p99 X ms, render-only p99 Y ms, skips N)
# exit 0, iff ALL of:
#
# M4 task 3 additions (fix_plan §M4 task 3; AGENT-LOG iter 73):
#   - gfx_app now REQUIRES --vfxdata/--glyphs (M4 task 2): the DEVICE
#     path consumes the COMMITTED frozen artifacts vfxdata-frozen.txt +
#     vfxglyphs-frozen.txt (browser-free; iter-72 rule — pin the FROZEN
#     file's sha256, never a fresh capture's), sha-pinned below and
#     sha-verified onto the device.
#   - LEGIBILITY (deliberate documented device-scale adaptation): every
#     device-path gfx_app run passes --legible (stage-surface strokes
#     clamped to GFX_LEGIBLE_MIN_DEV_PX device px — rationale in gfx.h;
#     value twin-pinned below). The host reference leg passes it too, so
#     the host<->device shot bit-compare stays valid. Standing
#     legibility WITNESS: a host run WITHOUT --legible must produce a
#     shot that DIFFERS from the legible host shot — proves the flag has
#     an observable effect, and (because the device shot is cmp'd
#     against the LEGIBLE reference) makes a flag-off device
#     mechanically detectable.
#   - The paced device run therefore renders WITH vfx + HUD overlay +
#     Ready/GO banner + background art live; p99 limits unchanged.
#
#   1. THREE-BACKEND SEAM: the platform seam (port/gfx/platform.h —
#      platform_init/present/poll/quit + PlatformInput) builds against
#      all three backend TUs, exactly ONE linked per target: headless
#      (gfx_app_headless, host judge runs), SDL2 (gfx_app_sdl2, host dev
#      window — built, not run: no GUI in a check), SDL1.2 (gfx_device,
#      armv7 via the shared rig build — DYNAMIC libSDL-1.2, LGPL rule,
#      asserted in rig_arm_build).
#   2. HOST TRUTH: x2 headless replays (--pace 0) produce byte-identical
#      streams AND byte-identical shot PPM/PGM; the stream passes the
#      UNCHANGED oracle/harness/verify-stream.js vs frozen g01; the host
#      shot passes the structural judge (judge-shot.js).
#   3. FRAMESKIP VALVE (standing tooth, every run): a short paced run
#      with a 1000 ns budget must SKIP renders (the sim always runs) and
#      flag them in the timing artifact — the valve and its logging are
#      proven live before the device is trusted with them.
#   4. DEVICE LIVE RENDER: g01 replayed ON the FunKey-S through the
#      SDL1.2 backend (real SetVideoMode surface, SDL_Flip presents,
#      paced 60 fps, frameskip armed), launched DETACHED via the setsid
#      + rc-file lifecycle (this adbd drops exit codes; review-50 H1);
#      frontend parked ONLY around the run and ALWAYS restored — by the
#      trap (transport-retry wrapped) AND, if the transport dies for
#      good, by the nonce-scoped on-device DEADMAN armed before the
#      park (cancel-on-success verified every run); the app RAM-buffers
#      everything and writes to tmpfs post-run (no I/O in the frame
#      loop, no SD writes during play).
#   5. DEVICE JUDGMENT (all host-side; the device never self-reports):
#      stream -> UNCHANGED verify-stream.js vs the frozen golden (the
#      renderer + SDL present must not perturb the sim ON DEVICE);
#      timing -> judge-render-timing.js (strict grammar), asserting
#        full-frame p99 (sim+render+present work time) <  16,670,000 ns
#        render-only p99 (rendered frames)             <= 8,000,000 ns
#        skips == 0 AND rendered == 3600 (review-50 H3: the frameskip
#        valve stays for real-time resilience, but a GATE pass may not
#        consume it)
#      over the FULL 3600-frame match (integer compares, no float);
#      summary -> the app's post-run summary line under the pinned
#      whitelist grammar: 0 failed presents (review-50 M2; SDL_Flip rc
#      propagated through platform_present) and device wall-clock in
#      [58,66] s (review-50 M3 — the pinned pace=1/budget in the same
#      line means silently-disabled pacing cannot even parse);
#      screenshot -> the app's OWN framebuffer (never the kernel fb —
#      240x720 triple-page, CLAUDE.md gotcha), pulled + judged by
#      judge-shot.js (letterbox structure, colour count, ink coverage)
#      and cmp'd BIT-EXACT against the host headless shot, PPM and PGM
#      both GATING (review-50 H4; measured achievable iters 50-51 — a
#      future legitimate cross-platform divergence is a REVIEWED pin
#      change at those cmp sites, never a silent allowance).
#
# GFXDATA staging (pre-registered, AGENT-LOG iter 50): the committed
# port/gfx/gfxdata-frozen.txt (deterministic EXECUTED page data from the
# pinned upstream build; bytes from a STREAM-MATCH + servedDistSha256
# guarded capture) — sha256-pinned here, cross-checked against every
# fresh browser capture by check-render.sh, sha-verified onto the
# device. The device path needs NO browser.
#
# Rig plumbing INHERITED from port/sim/device/riglib.sh (iters 38-42
# Tier-A arc, VERDICT: GO): nonce-dsh, pullv, rm-before-produce+made(),
# shared stamp-cached arm build (this script's bytes are stamp input via
# RIG_SCRIPTS), rehash-adjacent-to-push + push provenance, the SHARED
# no-reclaim device-keyed lock, the no-commit guard.
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
FDC=oracle/fdlibm-crosscheck
DTMP=/tmp/mlfk
DSD=/mnt/mlfk-scratch
mkdir -p "$BUILD" "$DEVB"

# --- frozen pins (M3 task 4; changing any is a reviewed repo change) ---------
P99_FULL_LIMIT_NS=16670000  # PLAN §4/M3 frame budget (16.67 ms)
P99_RENDER_LIMIT_NS=8000000 # PLAN §5 render allowance (8 ms)
BUDGET_NS=16666667          # 60 fps pacing budget for the live run
FRAMES_PIN=3600             # review-52 M1: the gate's rendered count is
                            # asserted against this LITERAL, and the
                            # manifest's g01.frames is cross-asserted
                            # equal — a legitimate trace-length change
                            # fails loudly HERE (reviewed pin change),
                            # never as a silently shortened gate
SHOT_FRAME=900              # pinned screenshot frame (mid-match, both players
                            # live — an IoU corpus member, so task 3 keeps a
                            # browser-vs-C reference for the same frame)
GFXDATA_FROZEN=$GFX/gfxdata-frozen.txt
GFXDATA_SHA256=5499a3dd5fc374d6ed988faf0bef6fa2e189eb314e892bdd83c7534dc0865c94
# M4 task 3: the committed vfx render-plane artifacts (M4 task 2 outputs;
# device path is browser-free — iter-72 rule: pin the FROZEN files' shas,
# never a fresh capture's; check-render.sh cross-checks these bytes
# against every fresh browser capture).
VFXDATA_FROZEN=$GFX/vfxdata-frozen.txt
VFXDATA_SHA256=545015a3d7e3bc138059fcb9711040758e729a7d21aac650b009ed7fdb5bd662
VFXGLYPHS_FROZEN=$GFX/vfxglyphs-frozen.txt
VFXGLYPHS_SHA256=8926cab4d648579d099053994bf309943b5a6bc3c5abf733af9ac6b71f3cbbeb
# M4 task 3: the frozen legibility constant (documented adaptation;
# rationale + authority in gfx.h). Twin-pinned: this literal must match
# the gfx.h #define — constant drift dies loudly here, never silently.
LEGIBLE_MIN_DEV_PX=2.0
WALL_MIN_MS=58000 # review-50 M3: paced 3600-frame run wall window —
WALL_MAX_MS=66000 # measured 60000 ms (iter 50); pacing can't silently die
DEADMAN_S="${MLFK_DEADMAN_S:-300}" # frontend-park deadman window (~4x the
                    # healthy park span; MLFK_DEADMAN_S = negative-testing
                    # override ONLY, default unchanged)
APPRC_TRIES=90      # x2 s host poll for the detached run's rc file
                    # (paced run is ~60 s; generous, bounded)

source port/sim/device/adbsh.sh # (also defines $DEV — it keys the lock)
source port/sim/device/riglib.sh

rig_lock_acquire

# INHERITED-STATE PESSIMISM (iter 56, review-55 H x2 — the arc's
# principle completed: pessimism precedes the FIRST fallible action):
# from the instant the lock is held, a PRIOR run's park/deadman state
# is treated as POSSIBLY PRESENT — before require_device, the devsha
# selftest, or any device probe can fail into the cleanup trap.
# RIG_PRESERVE_DTMP=1 (GLOBAL — rig_cleanup reads it at call time)
# makes ANY cleanup preserve $DTMP: a foreign deadman's nonce may live
# there, and wiping a nonce this run does not own disarms a backstop
# whose marker may still be parked (the review-55 stranding). ONLY the
# step-0 normalization below may clear it, and only after it has
# POSITIVELY verified inherited-state ownership: marker absent + no
# foreign deadman state (both RC-verified probes) — or stale state
# normalized through its designed channels. PARKED / DEADMAN_ARMED
# stay 0 pre-park: the trap must not touch a marker or a cancel
# channel it does not own — an inherited marker belongs to the
# inherited deadman (which the preserved nonce keeps able to fire).
RIG_PRESERVE_DTMP=1
PARKED=0
DEADMAN_ARMED=0

# Cleanup (installed AFTER lock acquisition, riglib contract): kill our
# app, RESTORE the frontend if we parked it, cancel the deadman ONLY
# once the frontend is PROVABLY restored (it is the backstop), then the
# shared rig cleanup. Best-effort by design, WARN-visible; every device
# command rides rig_dsh_retry (review-50 H1: one attempt through the
# transport that just died stranded the parked frontend).
#
# DISARM ORDERING (iter 54, review-52 H): the deadman's NONCE lives in
# $DTMP, so rig_cleanup's scratch wipe IS a disarm — a failed restore
# followed by a transport that recovered just in time for the wipe
# deleted the nonce while /mnt/disable_frontend remained, and the
# sleeping deadman then failed its nonce check and never restored the
# frontend. Now: the marker must be VERIFIED GONE by an RC-checked test
# (restore-command rc alone is never proof) before EITHER disarm channel
# (cancel file, nonce wipe) runs; on unverified restore the deadman
# STAYS armed and rig_cleanup preserves $DTMP (RIG_PRESERVE_DTMP=1).
# Cross-RUN interleavings of that end state are normalized by the
# step-0 startup chokepoint of the NEXT run (iter 55, review-54 H),
# and the lock-time pessimism above covers every failure BEFORE that
# chokepoint has run (iter 56, review-55 H).
task4_cleanup() {
  local prc restore_verified
  # pkill by process NAME — a `pkill -f <path>` pattern matches the adb
  # shell's OWN command line and kills it before the dsh RC marker can
  # print (measured iter 50: rc 71 on every cleanup). rc CAPTURED
  # (review-50 M1): 0 = a stray app was killed (WARN), 1 = nothing
  # matched (the healthy path), anything else = visible WARN.
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
    # RC-checked verification — the ONLY evidence that permits a disarm
    if rig_dsh_retry "test ! -f /mnt/disable_frontend"; then
      restore_verified=1
    else
      echo "WARN: could not VERIFY /mnt/disable_frontend is gone — the armed deadman will remove it on-device within ${DEADMAN_S}s of the park (or remove it by hand)" >&2
    fi
  else
    restore_verified=1 # never parked — nothing to restore, disarm is safe
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
    # this run's own restore is unverified: the nonce must survive so
    # the deadman stays armed (iter 54)
    RIG_PRESERVE_DTMP=1
  fi
  # rig_cleanup honors the GLOBAL RIG_PRESERVE_DTMP — still 1 whenever
  # step-0 normalization has not yet POSITIVELY owned inherited state
  # (iter 56: a pre-normalization death must never wipe a foreign nonce)
  rig_cleanup
}
trap task4_cleanup EXIT

require_device
# iter 56 (review-55 H): step 0 runs IMMEDIATELY after require_device —
# ahead of the devsha selftest and every other fallible probe.
# require_device is the minimum needed to talk to the device at all;
# the pessimism flags above are set before even that, so its failure
# path (and the selftest's, and the step-0 probes' own) preserves any
# inherited state.

echo "== [0/7] stale-state startup normalization (cross-run chokepoint) =="
# iter 55 (review-54 H — the iter-54 "disarm-shares-a-path-with-cleanup"
# class, SECOND face: cross-RUN sequencing): run A can exit with an
# unverified restore (marker present, deadman armed, nonce preserved);
# run B then owns the device, wipes A's nonce at its step-5 wipe, and if
# B fails pre-park its cleanup "has nothing to restore" while A's
# deadman rejects the missing nonce → stranded frontend. Instead of
# patching each interleaving, EVERY stale prior-run state is normalized
# HERE — one chokepoint, before any of this run's own parking/build:
#   1. the stale MARKER is restored FIRST and RC-VERIFIED gone
#      (PARKED=1 pessimistically around it: a mid-restore death hands
#      the retry-wrapped trap ownership of the marker);
#   2. only AFTER the marker is proven gone is a stale deadman disarmed
#      through its DESIGNED cancel channel and its exit VERIFIED — a
#      reboot/kill-orphaned pid FILE (recorded pid dead) is stale
#      state; a LIVE watcher ignoring its cancel is a defect → loud
#      death;
#   3. only then is the stale state wiped (rm -rf $DTMP).
# RIG_PRESERVE_DTMP has been 1 since lock acquisition (iter 56 — the
# pessimism block above), so ANY death here — a failed probe, a
# mid-normalization transport loss — preserves the old nonce and keeps
# the old backstop armed exactly until the thing it guards is
# verifiably restored (the iter-54 rule applied across runs). ONLY the
# completed normalization clears it, below. Transport failures during
# the probes are LOUD deaths, never "no stale state".
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
  # (RIG_PRESERVE_DTMP is already 1 — held since lock acquisition; a
  # death before the disarm-verify must not wipe the old nonce)
  if [ "$stale_marker" = 1 ]; then
    PARKED=1 # pessimistic: the trap owns the stale marker until verified gone
    dsh "rm -f /mnt/disable_frontend"
    dsh "test ! -f /mnt/disable_frontend" # RC-verified gone; errexit dies loud
    PARKED=0
    echo "   stale /mnt/disable_frontend removed (RC-verified gone)"
  fi
  if [ "$stale_deadman" = 1 ]; then
    dsh "mkdir -p $DTMP && touch $DTMP/deadman.cancel"
    sdm_gone=0
    for _ in $(seq 1 6); do # §7#1 bounded foreground poll (deadman polls its cancel every 2 s)
      if dsh "test ! -f $DTMP/deadman.pid" >/dev/null 2>&1; then sdm_gone=1; break; fi
      sleep 2
    done
    if [ "$sdm_gone" = 1 ]; then
      echo "   stale deadman disarmed via its cancel channel (exit verified: pid file gone)"
    else
      # pid file persists — whitelist-grammar pid parse, then fork on
      # liveness (never dereference /proc with an unvalidated value)
      sdm_pid="$(dsh "cat $DTMP/deadman.pid")" || {
        echo "DEVICE FAIL: startup normalization could not read the stale deadman pid file" >&2
        exit 1
      }
      sdm_pid="${sdm_pid%$'\n'}" # the single measured dsh trailing-newline artifact
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
# PESSIMISM CLEARED (iter 56): inherited-state ownership is now
# POSITIVELY verified — either both RC-verified probes found nothing,
# or every stale artifact was normalized through its designed channel
# above. From here $DTMP holds only THIS run's state and cleanup may
# wipe it (until/unless this run's OWN restore goes unverified, which
# re-sets the flag in the trap).
RIG_PRESERVE_DTMP=0
rig_devsha_selftest

# parse_timing_judge <timing-file> <frames> — judge-render-timing.js
# (strict row grammar: exactly <frames> lines, 4 anchored columns) with
# its key=value output consumed by the reviewed no-eval strict parser
# (unexpected key = loud death; iter 54, review-52 M2: duplicate key =
# corruption death — accidental output concatenation could put a stale
# PASSING duplicate after a failing value under last-wins; every key
# must occur exactly once, and every consumed key must be PRESENT).
# Numeric grammars carry digit bounds (review-52 M3): an oversized value
# dies as corruption BEFORE any bash arithmetic can see it — a bash
# integer test on an out-of-range operand exits status 2, which an `if`
# reads as FALSE (a silent failure-branch evasion). Sets: full_p99_ns
# full_p99_ms render_p99_ns render_p99_ms sim_p99_ms present_p99_ms
# skips rendered.
# FILE-BYTE READ (iter 62 — trailing-blank class completion, the
# iter-61 audio-judge pattern): the judge's stdout is captured to a
# FILE and timing_judge_bytes_assert requires the file to END with
# EXACTLY the 17 bytes 'judge_complete=1\n' (reference bytes GENERATED
# by printf + cmp, never hand-transcribed) BEFORE the (newline-
# stripping) $(cat) hands content to the parser — trailing blank
# lines, truncation, and a missing final newline all violate the
# grammar as written.
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
        : # reported by the judge; not asserted here
        ;;
      judge_complete)
        # the integrity terminator (iter 62): position already proven
        # by the file-byte assert; the value must still be exactly 1
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
  timing_judge_out="$jout" # for display; the judge ran exactly once
}

# parse_app_summary <log> <frames> <pace> <budget-ns> — gfx_app's
# post-run summary line under the PROCESS §3 whitelist grammar
# (producer: the single fprintf in gfx_app.c, grammar comment there —
# measured corpus: iter-50/51 app logs + this iteration's fresh runs):
#   gfx_app: <frames> frames, <skips> render skips, <fails> failed
#   presents, wall <ms> ms, pace=<pace> budget=<budget> ns
# EXACTLY ONE log line may match the full anchored pattern (the
# skipped-frames list line also starts with 'gfx_app: ' and must not);
# frames/pace/budget are PINNED into the pattern, so a run with the
# wrong length or silently-disabled pacing cannot even parse. Sets:
# app_skips app_present_fails app_wall_ms.
parse_app_summary() {
  local log="$1" fr="$2" pace="$3" budget="$4" re cnt line
  unset app_skips app_present_fails app_wall_ms
  # iter 54 (review-52 M3): every numeric field carries a 1-12 digit
  # bound in BOTH the count grep and the extraction regex — an oversized
  # value is corruption that dies at the grammar, never a status-2
  # bash-test evasion downstream.
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

echo "== [1/7] host data plane (g01 params + M1 tables + SIMDATA1 + trace + GFXDATA pin) =="
# g01 match params — the reviewed no-eval strict parser (check-device-g01.sh
# class: iter 39 review M4 / iter 40 round 2).
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
      # iter 54 (review-52 M3): digit bound BEFORE any bash arithmetic
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
# review-52 M1: cross-assert the manifest against the frozen literal —
# the step-7 gate asserts rendered == $FRAMES_PIN (the LITERAL), so a
# legitimate manifest change must fail loudly here at the pin, never
# silently shorten the gate.
if [ "$frames" -ne "$FRAMES_PIN" ]; then
  echo "DEVICE FAIL: manifest g01.frames ($frames) != pinned FRAMES_PIN ($FRAMES_PIN) — reviewed pin change required" >&2
  exit 1
fi
[ "$frames" -le 5000 ] || { echo "DEVICE FAIL: g01 frames $frames > 5000" >&2; exit 1; }
[ "$stage" -le 5 ] || { echo "DEVICE FAIL: g01 stage $stage > 5" >&2; exit 1; }
[ "$p1" -le 4 ] || { echo "DEVICE FAIL: g01 p1 $p1 > 4" >&2; exit 1; }
[ "$p2" -le 4 ] || { echo "DEVICE FAIL: g01 p2 $p2 > 4" >&2; exit 1; }
[ "$SHOT_FRAME" -le "$frames" ] || { echo "DEVICE FAIL: SHOT_FRAME $SHOT_FRAME > frames" >&2; exit 1; }
FROZEN=oracle/goldens/$name.sha256.json
made "$FROZEN"

# char id -> ANIM1 file (oracle char-id order, CLAUDE.md §Commands)
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

# GFXDATA pin: the committed frozen artifact must hash to the pin
# (content integrity; freshness is git's — this is committed INPUT, the
# same class as the frozen goldens). check-render.sh cross-checks these
# bytes against every fresh browser capture.
made "$GFXDATA_FROZEN"
# iter 56 (review-55 M): full-line host shasum grammar — never cut -f1
gsum="$(rig_host_sha256 "$GFXDATA_FROZEN")" || exit 1
if [ "$gsum" != "$GFXDATA_SHA256" ]; then
  echo "DEVICE FAIL: $GFXDATA_FROZEN sha256 $gsum != pinned $GFXDATA_SHA256" >&2
  exit 1
fi
echo "   gfxdata-frozen pin OK ($GFXDATA_SHA256)"
# M4 task 3: the two committed vfx render-plane artifacts (same class as
# the GFXDATA pin — committed INPUT, content integrity here, freshness
# is git's; asserted BEFORE any build/device work).
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
echo "   vfxdata/vfxglyphs frozen pins OK"
# M4 task 3 twin pin: the frozen legibility constant must equal gfx.h's
# #define (exact anchored line — a drifted constant is a reviewed change
# to BOTH sites, never a silent divergence).
if ! grep -q "^#define GFX_LEGIBLE_MIN_DEV_PX ${LEGIBLE_MIN_DEV_PX}\$" "$GFX/gfx.h"; then
  echo "DEVICE FAIL: gfx.h GFX_LEGIBLE_MIN_DEV_PX != pinned ${LEGIBLE_MIN_DEV_PX} (twin-pin drift — reviewed change required)" >&2
  exit 1
fi
echo "   legibility twin pin OK (GFX_LEGIBLE_MIN_DEV_PX ${LEGIBLE_MIN_DEV_PX})"

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

echo "== [2/7] host build: headless + SDL2 backends (ONE backend TU per binary) =="
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
  "$GFX/gfx_vfx.c" "$GFX/gfx_overlay.c" "$GFX/gfx_bg.c" \
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
    "$GFX/gfx_vfx.c" "$GFX/gfx_overlay.c" "$GFX/gfx_bg.c" \
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

echo "== [3/7] host truth: x2 headless replays + stream verify + shot judge =="
for side in a b; do
  rm -f "$BUILD/g01.app-out-$side.txt" "$BUILD/g01.app-tim-$side.txt" \
    "$BUILD/g01.app-shot-$side.ppm" "$BUILD/g01.app-shot-$side.pgm"
  "$BUILD/gfx_app_headless" \
    --trace "$DEVB/g01.trace.txt" --simdata "$DEVB/simdata.txt" \
    --gfxdata "$GFXDATA_FROZEN" --vfxdata "$VFXDATA_FROZEN" \
    --glyphs "$VFXGLYPHS_FROZEN" --legible --anim-dir "$TABLES" \
    --seed "$seed" --p1 "$p1" --p2 "$p2" --stage "$stage" \
    --frames "$frames" --pace 0 \
    --out "$BUILD/g01.app-out-$side.txt" \
    --timing "$BUILD/g01.app-tim-$side.txt" \
    --shot-frame "$SHOT_FRAME" \
    --shot-ppm "$BUILD/g01.app-shot-$side.ppm" \
    --shot-pgm "$BUILD/g01.app-shot-$side.pgm" \
    2> "$BUILD/g01.app-log-$side.txt"
  made "$BUILD/g01.app-out-$side.txt" "$BUILD/g01.app-tim-$side.txt" \
    "$BUILD/g01.app-shot-$side.ppm" "$BUILD/g01.app-shot-$side.pgm"
done
cmp "$BUILD/g01.app-out-a.txt" "$BUILD/g01.app-out-b.txt"
cmp "$BUILD/g01.app-shot-a.ppm" "$BUILD/g01.app-shot-b.ppm"
cmp "$BUILD/g01.app-shot-a.pgm" "$BUILD/g01.app-shot-b.pgm"
echo "   x2 headless replays byte-identical (stream + shot PPM/PGM)"
rm -f "$BUILD/g01.app-run.json"
node "$SIM/wrap-run.js" g01 "$BUILD/g01.app-out-a.txt" "$BUILD/g01.app-run.json"
made "$BUILD/g01.app-run.json"
node oracle/harness/verify-stream.js "$BUILD/g01.app-run.json" "$FROZEN"
echo "   host headless stream verified (seam app does not perturb the sim)"
node "$GFX/judge-shot.js" "$BUILD/g01.app-shot-a.ppm" "$BUILD/g01.app-shot-a.pgm"
echo "   host shot passes the structural judge"
# host-leg summary (strict grammar): pace 0 renders every frame and the
# headless backend must report zero failed presents (review-50 M2).
parse_app_summary "$BUILD/g01.app-log-a.txt" "$frames" 0 "$BUDGET_NS"
if [ "$app_skips" -ne 0 ] || [ "$app_present_fails" -ne 0 ]; then
  echo "DEVICE FAIL: host headless leg reports skips=$app_skips presentFails=$app_present_fails (want 0/0)" >&2
  exit 1
fi
echo "   host summary OK (0 skips, 0 failed presents)"

# M4 task 3 — the standing LEGIBILITY WITNESS: the same replay WITHOUT
# --legible up to the shot frame must produce a DIFFERENT shot (PPM and
# PGM ink plane both). Proves every run that the adaptation has an
# observable effect at the pinned frame; and because the device shot is
# bit-compared against the LEGIBLE host reference below, a device run
# whose --legible flag was dropped produces exactly these no-legible
# bytes and dies at that gating cmp — flag-off-on-device is
# mechanically detectable, not assumed.
rm -f "$BUILD/g01.app-shot-noleg.ppm" "$BUILD/g01.app-shot-noleg.pgm" \
  "$BUILD/g01.app-out-noleg.txt" "$BUILD/g01.app-tim-noleg.txt"
"$BUILD/gfx_app_headless" \
  --trace "$DEVB/g01.trace.txt" --simdata "$DEVB/simdata.txt" \
  --gfxdata "$GFXDATA_FROZEN" --vfxdata "$VFXDATA_FROZEN" \
  --glyphs "$VFXGLYPHS_FROZEN" --anim-dir "$TABLES" \
  --seed "$seed" --p1 "$p1" --p2 "$p2" --stage "$stage" \
  --frames "$SHOT_FRAME" --pace 0 \
  --out "$BUILD/g01.app-out-noleg.txt" \
  --timing "$BUILD/g01.app-tim-noleg.txt" \
  --shot-frame "$SHOT_FRAME" \
  --shot-ppm "$BUILD/g01.app-shot-noleg.ppm" \
  --shot-pgm "$BUILD/g01.app-shot-noleg.pgm" \
  2> "$BUILD/g01.app-log-noleg.txt"
made "$BUILD/g01.app-shot-noleg.ppm" "$BUILD/g01.app-shot-noleg.pgm"
if cmp -s "$BUILD/g01.app-shot-noleg.ppm" "$BUILD/g01.app-shot-a.ppm"; then
  echo "DEVICE FAIL: legibility witness — the no-legible shot PPM is byte-identical to the legible shot (the adaptation has NO observable effect at frame $SHOT_FRAME)" >&2
  exit 1
fi
if cmp -s "$BUILD/g01.app-shot-noleg.pgm" "$BUILD/g01.app-shot-a.pgm"; then
  echo "DEVICE FAIL: legibility witness — the no-legible shot PGM (ink plane) is byte-identical to the legible shot" >&2
  exit 1
fi
echo "   legibility witness OK (no-legible shot differs from the legible reference, PPM + PGM)"

echo "== [4/7] frameskip valve (standing tooth: 1000 ns budget => skips, flagged) =="
rm -f "$BUILD/valve-out.txt" "$BUILD/valve-tim.txt" \
  "$BUILD/valve-shot.ppm" "$BUILD/valve-shot.pgm"
"$BUILD/gfx_app_headless" \
  --trace "$DEVB/g01.trace.txt" --simdata "$DEVB/simdata.txt" \
  --gfxdata "$GFXDATA_FROZEN" --vfxdata "$VFXDATA_FROZEN" \
  --glyphs "$VFXGLYPHS_FROZEN" --legible --anim-dir "$TABLES" \
  --seed "$seed" --p1 "$p1" --p2 "$p2" --stage "$stage" \
  --frames 120 --pace 1 --budget-ns 1000 \
  --out "$BUILD/valve-out.txt" --timing "$BUILD/valve-tim.txt" \
  --shot-frame 100 --shot-ppm "$BUILD/valve-shot.ppm" \
  --shot-pgm "$BUILD/valve-shot.pgm" \
  2> "$BUILD/valve-log.txt"
made "$BUILD/valve-out.txt" "$BUILD/valve-tim.txt"
# review-50 M4 (whitelist grammar): the old permissive awk flag-count +
# substring grep accepted a truncated timing file. Now the SAME strict
# judge as the gate run parses the valve artifact (exactly 120 anchored
# rows, complete stream) and the flagged count is asserted EXACTLY:
# a 1000 ns budget always overruns, so every frame skips EXCEPT the
# forced shot-frame render — 119/1, deterministic.
parse_timing_judge "$BUILD/valve-tim.txt" 120
if [ "$skips" -ne 119 ] || [ "$rendered" -ne 1 ]; then
  echo "DEVICE FAIL: frameskip valve tooth — expected exactly 119 flagged skips + 1 rendered (the forced shot frame) in 120 frames at a 1000 ns budget, got skips=$skips rendered=$rendered" >&2
  exit 1
fi
# the app's own summary line must agree (strict full-line grammar; also
# proves the skip logging + zero failed presents on the valve leg)
parse_app_summary "$BUILD/valve-log.txt" 120 1 1000
if [ "$app_skips" -ne 119 ] || [ "$app_present_fails" -ne 0 ]; then
  echo "DEVICE FAIL: valve summary line disagrees (skips=$app_skips presentFails=$app_present_fails; want 119/0)" >&2
  exit 1
fi
echo "   valve tooth OK (119/120 renders skipped — timing artifact and app summary agree; sim ran every frame; 0 failed presents)"

echo "== [5/7] armv7 build (shared rig stamp) + push + provenance =="
rig_arm_build
rig_stamp_rehash gfx_device
dsh "rm -rf $DTMP $DSD && mkdir -p $DTMP $DSD"
adb -s "$DEV" push "$DEVB/gfx_device" "$DEVB/simdata.txt" \
  "$DEVB/g01.trace.txt" "$GFXDATA_FROZEN" \
  "$VFXDATA_FROZEN" "$VFXGLYPHS_FROZEN" \
  "$TABLES/$ANIM_P1" "$TABLES/$ANIM_P2" "$DTMP/" >/dev/null
rig_push_provenance "$DTMP" gfx_device
dsh "chmod +x $DTMP/gfx_device"
# sha-verify every pushed DATA file device-side against the host bytes
# (the binary is covered by push provenance above). Digests come through
# the strict full-line rig_dev_sha256 parser (iter 52, PROCESS §3).
# M4 task 3: vfxdata/vfxglyphs join the list — their host bytes were
# pinned to the COMMITTED frozen shas at step 1 (iter-72 rule), so this
# device-side compare transitively binds the device copies to the pins.
for hf in "$DEVB/simdata.txt" "$DEVB/g01.trace.txt" "$GFXDATA_FROZEN" \
          "$VFXDATA_FROZEN" "$VFXGLYPHS_FROZEN" \
          "$TABLES/$ANIM_P1" "$TABLES/$ANIM_P2"; do
  bn="$(basename "$hf")"
  hsum="$(rig_host_sha256 "$hf")" || exit 1 # full-line grammar (iter 56)
  dsum="$(rig_dev_sha256 "$DTMP/$bn")" || exit 1
  if [ "$dsum" != "$hsum" ]; then
    echo "DEVICE FAIL: pushed $bn device sha ($dsum) != host sha ($hsum)" >&2
    exit 1
  fi
done
echo "   pushed data sha-verified on device (simdata, trace, gfxdata, vfxdata, vfxglyphs, 2 anim bins)"

echo "== [6/7] device: LIVE paced g01 render (deadman-guarded park; detached launch) =="
# DEADMAN (review-50 H1): a device-side backstop generated + pushed +
# armed BEFORE the park, so a transport death at ANY later point still
# un-strands the frontend on-device. It polls a cancel file every 2 s
# inside a $DEADMAN_S window; on expiry it acts ONLY if the nonce file
# still carries THIS install's nonce (a later run's startup
# normalization cancel-disarms any stale deadman at the step-0
# chokepoint; the $DTMP wipe is a second, incidental disarm) and no
# cancel exists — actions are idempotent by design: rm -f marker + a
# NONCE-SCOPED kill (iter 55, review-54 H: only the pid recorded under
# THIS nonce, and only while its cmdline is still gfx_device — an old
# deadman can never kill a NEW run's app). The healthy path cancels
# and VERIFIES the deadman exited without firing (hygiene tooling must
# never fire during a healthy run — proven every run, not argued).
DM_NONCE="$RANDOM$RANDOM$$"
rm -f "$BUILD/deadman.sh"
cat > "$BUILD/deadman.sh" << EOF
#!/bin/sh
# generated by check-device-render.sh — frontend-park DEADMAN (iter 52)
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
  # iter 55 (review-54 H): the kill is NONCE-SCOPED — only the pid this
  # run's launcher recorded under THIS nonce, and only while that pid's
  # cmdline is still gfx_device (pid-reuse guard). A bare
  # `pkill gfx_device` could kill a LATER run's app. (An argv nonce tag
  # is infeasible: gfx_app rejects unknown args and gfx_app.c is
  # outside this task's surface — the nonce-named pid file is the
  # equivalent scoping.)
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
# LAUNCHER (review-50 H1): detached setsid + rc-file lifecycle (the
# CLAUDE.md detached recipe; task-5 precedent — this adbd drops exit
# codes, the rc file is the only truthful exit evidence). Login shell
# at invocation time (/etc/profile sets SDL_NOMOUSE=1).
rm -f "$BUILD/render-launch.sh"
cat > "$BUILD/render-launch.sh" << EOF
#!/bin/sh
# generated by check-device-render.sh — paced live-render launcher
# iter 55 (review-54 H): gfx_device is backgrounded and its pid recorded
# under the NONCE-NAMED file the deadman's scoped kill arm reads —
# `wait \$!` preserves the app's exit code for render.apprc.
cd $DTMP || exit 9
rm -f render.apprc gfx.pid.$DM_NONCE
setsid sh -c './gfx_device \
  --trace $DTMP/g01.trace.txt --simdata $DTMP/simdata.txt \
  --gfxdata $DTMP/gfxdata-frozen.txt --vfxdata $DTMP/vfxdata-frozen.txt \
  --glyphs $DTMP/vfxglyphs-frozen.txt --legible --anim-dir $DTMP \
  --seed $seed --p1 $p1 --p2 $p2 --stage $stage --frames $frames \
  --pace 1 --budget-ns $BUDGET_NS \
  --out $DTMP/g01.dev-out.txt --timing $DTMP/g01.dev-tim.txt \
  --shot-frame $SHOT_FRAME --shot-ppm $DTMP/g01.dev-shot.ppm \
  --shot-pgm $DTMP/g01.dev-shot.pgm 2> $DTMP/g01.dev-log.txt & \
  echo \$! > $DTMP/gfx.pid.$DM_NONCE; \
  wait \$!; \
  echo "RC=\$?" > $DTMP/render.apprc' \
  </dev/null >/dev/null 2>&1 &
sleep 2
EOF
made "$BUILD/render-launch.sh"
adb -s "$DEV" push "$BUILD/deadman.sh" "$BUILD/render-launch.sh" "$DTMP/" >/dev/null
for hf in "$BUILD/deadman.sh" "$BUILD/render-launch.sh"; do
  bn="$(basename "$hf")"
  hsum="$(rig_host_sha256 "$hf")" || exit 1 # full-line grammar (iter 56)
  dsum="$(rig_dev_sha256 "$DTMP/$bn")" || exit 1
  if [ "$dsum" != "$hsum" ]; then
    echo "DEVICE FAIL: pushed $bn device sha ($dsum) != host sha ($hsum)" >&2
    exit 1
  fi
done
dsh "chmod +x $DTMP/deadman.sh $DTMP/render-launch.sh"

# PRE-RUN SYNC (M4 task 3 — transient-skip class attribution, measured
# across 5 paced attempts): the ~10 MB of pushes above go to SD through
# the page cache; the kernel's dirty-expiry writeback (~30 s) then
# flushed them MID-RUN on the single-core V3s, landing isolated 7-14 ms
# sim stalls in a stable frame zone (~1150-1250 ≈ 19-21 s in — the SAME
# zone as iters 54/59's recorded spikes; reproduced with host polling
# fully quiet, so adbd polls are refuted as the cause). Force the
# flush to complete BEFORE the paced run: the stall happens here, on
# rig time, never on frame time. (CLAUDE.md gotcha class: "SD streaming
# = multi-second stalls".)
dsh "sync"
# arm the deadman BEFORE parking
dsh "printf '%s' '$DM_NONCE' > $DTMP/deadman.nonce; rm -f $DTMP/deadman.cancel $DTMP/deadman.fired"
dsh "setsid sh $DTMP/deadman.sh </dev/null >/dev/null 2>&1 & sleep 1"
if ! dsh "test -f $DTMP/deadman.pid" >/dev/null 2>&1; then
  echo "DEVICE FAIL: park deadman did not start (no pid file)" >&2
  exit 1
fi
DEADMAN_ARMED=1
echo "   deadman armed (window ${DEADMAN_S}s, nonce-scoped, cancel-on-success verified below)"

# PARK. PARKED=1 is set PESSIMISTICALLY BEFORE the park command
# (review-50 H2): if the transport dies after the device executed the
# touch but before the ack, the trap still restores (rm -f idempotent).
PARKED=1
dsh "touch /mnt/disable_frontend"
prc=0
dsh "pkill gmenu2x" >/dev/null 2>&1 || prc=$?
case "$prc" in # rc captured (review-50 M1): busybox pkill 1 = no match
  0) : ;;
  1) echo "WARN: gmenu2x was not running at park time" >&2 ;;
  *) echo "DEVICE FAIL: pkill gmenu2x failed (rc $prc)" >&2; exit 1 ;;
esac

t0=$(date +%s)
dsh "sh -lc $DTMP/render-launch.sh"
# bounded host-side poll for the detached run's rc file (§7#1 shape).
# QUIET WINDOW (M4 task 3 — the iter-62 "probe after-effect" class,
# second face, measured: every dsh poll forks a shell through adbd ON
# the single-core V3s DURING the paced run; isolated 7-14 ms sim
# spikes — the registered transient-skip class — clustered on attempts
# with mid-run polling): the run's duration is PINNED (3600 paced
# frames; the wall window [58,66] s is asserted below), so the first
# 50 s need no probes at all. Host-side sleep only, then the 2 s
# cadence — completion detection is unchanged, mid-run device forks
# drop from ~30 to ~5.
sleep 50
apprc_seen=0
for _ in $(seq 1 "$APPRC_TRIES"); do
  if dsh "test -f $DTMP/render.apprc" >/dev/null 2>&1; then apprc_seen=1; break; fi
  sleep 2
done
t1=$(date +%s)
if [ "$apprc_seen" != 1 ]; then
  dsh "cat $DTMP/g01.dev-log.txt" >&2 || true
  echo "DEVICE FAIL: live render never finished (rc file absent after $((APPRC_TRIES * 2))s)" >&2
  exit 1
fi
pullv "$DTMP/render.apprc" "$DEVB/render.apprc"
if [ "$(cat "$DEVB/render.apprc")" != "RC=0" ]; then # exact whole-file grammar
  dsh "cat $DTMP/g01.dev-log.txt" >&2 || true
  echo "DEVICE FAIL: live render app exited nonzero ($(cat "$DEVB/render.apprc"))" >&2
  exit 1
fi
# iter 55: the deadman's scoped-kill arm depends on the launcher's
# nonce-named pid record — prove it was written on every healthy run
# (a silently-unwritten pid file = a dead kill arm; the marker-restore
# arm is independent and unaffected).
dsh "test -s $DTMP/gfx.pid.$DM_NONCE"
dsh "rm -f /mnt/disable_frontend"
# iter 54 (review-52 H): RC-checked VERIFICATION that the marker is
# actually gone — only then may either disarm channel run (PARKED=0
# skips the trap's restore; the cancel below disarms the deadman). A
# failure here dies loud (errexit) into the trap with PARKED=1, which
# retries the restore and leaves the deadman armed if unverifiable.
dsh "test ! -f /mnt/disable_frontend"
PARKED=0
# cancel the deadman and PROVE it exited without firing
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
echo "   live run done (host-observed $((t1 - t0)) s; app rc 0; frontend restored; deadman cancelled without firing)"
pullv "$DTMP/g01.dev-out.txt" "$DEVB/g01.dev-out.txt"
pullv "$DTMP/g01.dev-tim.txt" "$DEVB/g01.dev-tim.txt"
pullv "$DTMP/g01.dev-shot.ppm" "$DEVB/g01.dev-shot.ppm"
pullv "$DTMP/g01.dev-shot.pgm" "$DEVB/g01.dev-shot.pgm"
pullv "$DTMP/g01.dev-log.txt" "$DEVB/g01.dev-log.txt"

echo "== [7/7] host judgment: stream + timing + screenshot =="
rm -f "$DEVB/g01.dev-run.json"
node "$SIM/wrap-run.js" g01 "$DEVB/g01.dev-out.txt" "$DEVB/g01.dev-run.json"
made "$DEVB/g01.dev-run.json"
node oracle/harness/verify-stream.js "$DEVB/g01.dev-run.json" "$FROZEN"
echo "   device stream verified (render+present did not perturb the sim on device)"

parse_timing_judge "$DEVB/g01.dev-tim.txt" "$frames"
printf '%s\n' "$timing_judge_out" | sed 's/^/   judge: /'
if [ "$full_p99_ns" -ge "$P99_FULL_LIMIT_NS" ]; then
  echo "RENDER P99 FAIL: full-frame p99 ${full_p99_ms} ms (${full_p99_ns} ns) >= limit ${P99_FULL_LIMIT_NS} ns" >&2
  exit 1
fi
if [ "$render_p99_ns" -gt "$P99_RENDER_LIMIT_NS" ]; then
  echo "RENDER P99 FAIL: render-only p99 ${render_p99_ms} ms (${render_p99_ns} ns) > limit ${P99_RENDER_LIMIT_NS} ns" >&2
  exit 1
fi
# review-50 H3 (skip gate): the valve exists for real-time resilience,
# but a GATE pass may not consume it — every frame must have rendered.
# review-52 M1: rendered is asserted against the LITERAL pin ($FRAMES_PIN
# = 3600, frozen above; the manifest is cross-asserted equal at step 1),
# never the manifest-derived value alone — an accidentally regenerated
# shorter corpus can no longer satisfy a shortened gate.
if [ "$skips" -ne 0 ] || [ "$rendered" -ne "$FRAMES_PIN" ]; then
  echo "DEVICE FAIL: gate run rendered $rendered/$FRAMES_PIN with $skips skips — a GATE pass may not consume the frameskip valve, and rendered must equal the literal pin" >&2
  exit 1
fi
# review-50 M2 + M3: the app summary under the pinned whitelist grammar
# (frames/pace/budget pinned into the pattern). Assert 0 failed
# presents, summary/timing skip agreement, and the wall-clock window.
parse_app_summary "$DEVB/g01.dev-log.txt" "$frames" 1 "$BUDGET_NS"
if [ "$app_present_fails" -ne 0 ]; then
  echo "DEVICE FAIL: $app_present_fails failed presents on the device run (SDL_Flip/lock failures — nothing may silently miss the screen)" >&2
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

node "$GFX/judge-shot.js" "$DEVB/g01.dev-shot.ppm" "$DEVB/g01.dev-shot.pgm"
echo "   device shot passes the structural judge"
# review-50 H4: the bit-compare is GATING, PPM and PGM (ink plane) both
# (measured achievable: iter-50/51 device shots were bit-identical to
# the host render — the render plane is cross-platform deterministic by
# the iter-50 fdlibm/integer-helper class fix). A future LEGITIMATE
# cross-platform divergence is a REVIEWED pin change at these two cmp
# sites (e.g. a committed reference-shot pin), never a silent allowance.
cmp "$DEVB/g01.dev-shot.ppm" "$BUILD/g01.app-shot-a.ppm" || {
  echo "DEVICE FAIL: device shot PPM != host headless shot (bit-compare is GATING — reviewed pin change required for a legitimate divergence)" >&2
  exit 1
}
cmp "$DEVB/g01.dev-shot.pgm" "$BUILD/g01.app-shot-a.pgm" || {
  echo "DEVICE FAIL: device shot PGM (ink plane) != host headless shot (bit-compare is GATING)" >&2
  exit 1
}
echo "   device shot == host headless shot BIT-IDENTICAL (PPM + PGM, gating)"

rig_no_commit_guard "$BUILD" "$DEVB" "$TABLES"

echo "DEVICE RENDER OK (full p99 ${full_p99_ms} ms, render-only p99 ${render_p99_ms} ms, sim p99 ${sim_p99_ms} ms, present p99 ${present_p99_ms} ms, skips ${skips}/${frames})"
