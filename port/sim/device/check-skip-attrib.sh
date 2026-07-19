#!/usr/bin/env bash
# M4 task 8 done-check (driver re-spec, AGENT-LOG iter 74): the
# skip-stall ATTRIBUTION INSTRUMENT. Prints
#   SKIP ATTRIB OK (arm=<arm>, skips=N/3600, events=M, stream MATCH)
# exit 0, iff ALL of:
#
#   1. ONE paced 3600-frame g01 live render run ON the FunKey-S in the
#      EXACT task-3 gate configuration (same budget/flags/park/deadman/
#      quiet-window/pre-run-sync machinery — the stall class must be
#      observed under the conditions that block task 3) PLUS the
#      attribution capture:
#        - gfx_app --attrib: per-frame CLOCK_MONOTONIC +
#          CLOCK_MONOTONIC_RAW + getrusage nvcsw/nivcsw/minflt/majflt
#          rows (RAM-buffered, post-run write — grammar in gfx_app.c);
#        - arm "sampler" (DEFAULT): sk_sampler concurrently snapshots
#          /proc/stat + /proc/interrupts + /proc/softirqs every 250 ms
#          (fork-free, held fds, RAM-buffered — sk_sampler.c);
#        - pre/post run kernel dumps pulled host-side OUTSIDE the paced
#          window: interrupts, stat, vmstat, softirqs, dmesg,
#          timer_list, uptime, and the FULL per-pid /proc/*/stat table.
#   2. The run's checksum stream passes the UNCHANGED verify-stream.js
#      vs frozen g01 — the instrument may not perturb the sim.
#   3. correlate-skips.js (strict whitelist grammars) joins the
#      evidence and completes (file tail == 'attrib_complete=1\n');
#      skips/events are REPORTED, never asserted zero — they are the
#      phenomenon under measurement, not the pass criterion (the
#      skips==0 GATE lives in check-device-render.sh, unweakened).
#   4. docs/AGENT-LOG.md carries the attribution verdict needle
#      (`SKIP ATTRIB VERDICT: (a|b|c)` — M2CAL-report precedent).
#
# MATRIX SEAMS (negative-testing / evidence-gathering only; defaults
# unchanged for the done-check):
#   MLFK_SKATTRIB_ARM      = nosampler | sampler (default) | quiesce
#     nosampler — attrib rows only (the sampler-perturbation control)
#     quiesce   — sampler + stop allowlisted daemons around the run
#                 (trap+deadman-restored, comm-scan exact-cardinality
#                 verified; device hygiene)
#   MLFK_SKATTRIB_STOP     = daemon list for the quiesce arm; allowlist
#     ONLY low_bat_check / system_stats / fkgpiod (their start-stop
#     init scripts are the designed stop/restart channels)
#   MLFK_SKATTRIB_MATRIX=1 — evidence-gathering mode: the AGENT-LOG
#     needle assert is skipped (the matrix runs BEFORE the verdict can
#     exist). The done-check never sets it.
#
# DEGRADED-MODE LOCKOUT (iter 76, review-73 M — the verify_m3.sh
# AUTHORITATIVE pattern): SKA_AUTHORITATIVE is computed ONCE up front
# and made readonly — 1 iff ARM is the default 'sampler' AND
# MLFK_SKATTRIB_MATRIX is unset/0. Every other combination (a matrix
# arm, a stale exported env var) prints a DEV banner at arm selection
# AND at exit, and the run finishes with
#   SKIP ATTRIB (DEV — NON-AUTHORITATIVE ...)  + exit 3.
# The gating `SKIP ATTRIB OK` line exists ONLY inside the
# SKA_AUTHORITATIVE=1 branch — structurally unreachable degraded.
#
# This is a Tier-B DIAGNOSTIC instrument (PROCESS §3): it changes no
# gate and no gate limit; its own judgment (stream verify) rides the
# frozen oracle machinery unchanged.
#
# Rig plumbing INHERITED from riglib.sh (Tier-A arc, VERDICT: GO) and
# the reviewed check-device-render.sh park/deadman/launch machinery
# (iters 50-62 arcs), adapted verbatim where possible.
#
# Device hygiene: writes only /tmp/mlfk + /mnt/mlfk-scratch + the
# frontend-park marker (removed on exit, trap'd + deadman backstop);
# quiesce-arm daemon stops are trap-restored AND verified; own
# processes killed on exit. NEVER touches firmware/saves.
#
# Env: FUNKEY_ADB_ID (device id), MLFK_FORCE_ARM=1 (ignore build stamp).
set -euo pipefail
cd "$(dirname "$0")/../../.."

GFX=port/gfx
CAL=port/sim/calib
DEVB=$CAL/build/device
SIM=port/sim/sim
SKA=port/sim/device/skip-attrib
SKAB=$DEVB/skip-attrib
TABLES=pipeline/build/sim-tables
FDC=oracle/fdlibm-crosscheck
DTMP=/tmp/mlfk
DSD=/mnt/mlfk-scratch
mkdir -p "$DEVB" "$SKAB"

# --- frozen pins (task-3 gate configuration reproduced exactly) --------------
BUDGET_NS=16666667          # 60 fps pacing budget (check-device-render.sh pin)
FRAMES_PIN=3600
SAMPLER_PERIOD_MS=250       # pre-registered sampler cadence (iter 74)
SAMPLER_MAX=400             # 100 s cap at 250 ms — run is ~63 s
WALL_MIN_MS=58000           # paced wall window (check-device-render.sh pin)
WALL_MAX_MS=66000
LATE_START_NS=2000000       # correlator event threshold (pre-registered)
DEADMAN_S="${MLFK_DEADMAN_S:-300}"
APPRC_TRIES=90
GFXDATA_FROZEN=$GFX/gfxdata-frozen.txt
VFXDATA_FROZEN=$GFX/vfxdata-frozen.txt
VFXGLYPHS_FROZEN=$GFX/vfxglyphs-frozen.txt
# M7 (iter 76, review-73 M — WORKLOAD FIDELITY): the attribution run
# must be the EXACT task-3 gate workload, so its evidence attributes
# the gated configuration and nothing else. The three frozen-artifact
# sha256 pins and the shot frame are TWIN-PINNED against
# check-device-render.sh (anchored greps below — drift between the two
# scripts dies loudly; the values themselves are the committed frozen
# files' shas, iter-72 rule).
GFXDATA_SHA256=5499a3dd5fc374d6ed988faf0bef6fa2e189eb314e892bdd83c7534dc0865c94
VFXDATA_SHA256=545015a3d7e3bc138059fcb9711040758e729a7d21aac650b009ed7fdb5bd662
VFXGLYPHS_SHA256=8926cab4d648579d099053994bf309943b5a6bc3c5abf733af9ac6b71f3cbbeb
SHOT_FRAME=900
# Quiesce-bracket slacks (iter 78, review-76 M1 — the exact-window
# STANDING TOOTH; quiesce arm only in this script): device-clock stamp
# deltas, judged by riglib rig_quiesce_bracket_assert.
QW_PRE_SLACK_S=10   # stop-complete -> app-start (the launch dsh only)
QW_POST_SLACK_S=10  # app-end -> restore-start (exit-poll latency only)
# M3 (iter 78, review-76 M — SUFFIX-FREE verdict grammar; supersedes
# iter 76's form whose open ' — .+' suffix let
# 'SKIP ATTRIB VERDICT: (a) — superseded; do not use' ride as a full
# match): the verdict line is EXACTLY 'SKIP ATTRIB VERDICT: (a|b|c)'
# — detail lives on a SEPARATE non-gating line that must NOT start
# with the needle prefix. FULL-LINE anchored needle, EXACTLY ONE match
# required in docs/AGENT-LOG.md, plus a resemblance counter — any
# OTHER line-anchored 'SKIP ATTRIB VERDICT' (quoted template text,
# truncation, negation, suffixed variant) is corruption. The canonical
# standalone line lives in the AGENT-LOG iter-76 entry (rewritten
# suffix-free iter 78); future verdicts REPLACE the convention
# consciously (exactly-one is the grammar).
NEEDLE_FULL='^SKIP ATTRIB VERDICT: \((a|b|c)\)$'
NEEDLE_RESEMBLE='^SKIP ATTRIB VERDICT'

# --- arm selection -----------------------------------------------------------
ARM="${MLFK_SKATTRIB_ARM:-sampler}"
case "$ARM" in
  nosampler|sampler|quiesce) : ;;
  *) echo "SKIP ATTRIB FAIL: MLFK_SKATTRIB_ARM '$ARM' not in {nosampler,sampler,quiesce}" >&2; exit 1 ;;
esac
# DEGRADED-MODE LOCKOUT (iter 76, review-73 M): computed ONCE, readonly
# (the verify_m3.sh pattern). The `SKIP ATTRIB OK` sentinel exists only
# inside the =1 branch at the bottom.
SKA_AUTHORITATIVE=1
SKA_AUTH_REASONS=""
if [ "$ARM" != sampler ]; then
  SKA_AUTHORITATIVE=0
  SKA_AUTH_REASONS="$SKA_AUTH_REASONS arm=$ARM(non-default);"
fi
if [ "${MLFK_SKATTRIB_MATRIX:-0}" != 0 ]; then
  SKA_AUTHORITATIVE=0
  SKA_AUTH_REASONS="$SKA_AUTH_REASONS matrix-mode(MLFK_SKATTRIB_MATRIX=${MLFK_SKATTRIB_MATRIX:-});"
fi
readonly SKA_AUTHORITATIVE SKA_AUTH_REASONS
if [ "$SKA_AUTHORITATIVE" != 1 ]; then
  echo "SKIP ATTRIB (DEV — NON-AUTHORITATIVE):${SKA_AUTH_REASONS} evidence-gathering run — the gating verdict is withheld and the exit will be nonzero" >&2
fi

STOP_LIST=""
if [ "$ARM" = quiesce ]; then
  STOP_LIST="${MLFK_SKATTRIB_STOP:-low_bat_check system_stats}"
  for d in $STOP_LIST; do
    case "$d" in
      low_bat_check|system_stats|fkgpiod) : ;;
      *) echo "SKIP ATTRIB FAIL: daemon '$d' not in the quiesce allowlist {low_bat_check,system_stats,fkgpiod}" >&2; exit 1 ;;
    esac
  done
fi
# daemon -> its init script (measured on device). NOTE (measured this
# iteration, .loop/m4-task8-a4-quiesce-lbc.log): the init scripts'
# STOP arm is a NO-OP for the SHELL daemons — busybox
# `start-stop-daemon -K -x <script>` matches /proc/<pid>/exe, which for
# a #!/bin/sh daemon is busybox, so nothing is ever killed (and `-o`
# masks it as rc 0); busybox pidof equally cannot see script comms.
# The quiesce arm therefore stops by COMM-SCAN + kill-by-pid and
# verifies by comm-scan; the START arm of the init script works
# (start-stop-daemon -S execs fine) and is kept as the restore channel.
init_script() {
  case "$1" in
    low_bat_check) echo /etc/init.d/S12low-bat-check ;;
    system_stats)  echo /etc/init.d/S13system-stats ;;
    fkgpiod)       echo /etc/init.d/S11gpio ;;
    *) echo "SKIP ATTRIB FAIL: no init script mapping for '$1'" >&2; return 1 ;;
  esac
}

# comm-scan quiesce machinery: rig_comm_pids / rig_daemon_stop /
# rig_daemon_restore (riglib.sh — factored there this iteration so the
# task-3 run harness shares the SAME reviewed bodies).

source port/sim/device/adbsh.sh
source port/sim/device/riglib.sh

rig_lock_acquire

# Inherited-state pessimism + park/deadman lifecycle: the reviewed
# check-device-render.sh discipline (iters 54-56 arcs) adapted verbatim.
RIG_PRESERVE_DTMP=1
PARKED=0
DEADMAN_ARMED=0
DAEMONS_STOPPED="" # quiesce arm: daemons this run stopped (trap restores)
DM_NONCE=""        # set at deadman generation; empty-safe for pre-arm traps

ska_cleanup() {
  local rc=$?
  local prc restore_verified daemons_ok d isc
  prc=0
  rig_dsh_retry "pkill gfx_device" || prc=$?
  case "$prc" in
    0) echo "WARN: gfx_device was still running at cleanup — killed" >&2 ;;
    1) : ;;
    *) echo "WARN: could not pkill gfx_device on the device (rc $prc)" >&2 ;;
  esac
  prc=0
  rig_dsh_retry "pkill sk_sampler" || prc=$?
  case "$prc" in
    0) echo "WARN: sk_sampler was still running at cleanup — killed" >&2 ;;
    1) : ;;
    *) echo "WARN: could not pkill sk_sampler on the device (rc $prc)" >&2 ;;
  esac
  # QUIESCE RESTORE (device hygiene): every daemon this run stopped is
  # restarted through the init-script START channel (idempotent,
  # exact-cardinality — riglib iter 76) and VERIFIED by comm-scan; on
  # a verified restore the nonce-scoped quiesce marker is removed so
  # the deadman's daemon-restore arm stands down. An unverifiable
  # restore leaves the marker AND the deadman armed (iter 76,
  # review-73 H) — LOUD warn naming the manual recovery command.
  daemons_ok=1
  for d in $DAEMONS_STOPPED; do
    isc="$(init_script "$d")" || { daemons_ok=0; continue; }
    if rig_daemon_restore "$d" "$isc"; then
      rig_dsh_retry "rm -f $DTMP/qd.$d.${DM_NONCE:-none}" \
        || echo "WARN: could not remove the quiesce marker for $d (harmless: the deadman restore arm is comm-scan-guarded)" >&2
      echo "   quiesce restore: $d verified running again (comm-scan, exactly 1)" >&2
    else
      daemons_ok=0
      echo "WARN: $d did NOT verify as running after restart — the armed deadman will comm-scan-restore it on-device, or run '$isc start' on the device manually" >&2
    fi
  done
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
  # iter 76 (review-73 H): the deadman backstops BOTH the frontend park
  # AND the quiesce-arm daemon stops — cancel only when BOTH verified.
  if [ "$DEADMAN_ARMED" = 1 ]; then
    if [ "$restore_verified" = 1 ] && [ "$daemons_ok" = 1 ]; then
      rig_dsh_retry "touch $DTMP/deadman.cancel" \
        || echo "WARN: could not cancel the park deadman — it will fire once (idempotent actions) within ${DEADMAN_S}s" >&2
    else
      echo "WARN: frontend restore or daemon restore unverified — leaving the deadman ARMED as the backstop (its purpose)" >&2
    fi
  fi
  if [ "$restore_verified" != 1 ] || [ "$daemons_ok" != 1 ]; then
    RIG_PRESERVE_DTMP=1
  fi
  rig_cleanup
  # FAIL-CLOSED exit guard (measured this iteration, bash 3.2): a word
  # -expansion error (e.g. set -u) aborts the script but, with an EXIT
  # trap installed, the shell exits 0 — a died-mid-run check could read
  # as green to an `&&` consumer. The ONLY legal rc-0 exit is the one
  # that printed the verdict line (SKA_OK=1 set immediately before it);
  # any other rc-0 arrival here is forced loud.
  if [ "$rc" = 0 ] && [ "${SKA_OK:-0}" != 1 ]; then
    echo "SKIP ATTRIB FAIL: script exited without reaching the verdict line (bash expansion-error class) — forcing nonzero" >&2
    exit 70
  fi
}
trap ska_cleanup EXIT

require_device

echo "== [0/6] stale-state startup normalization (cross-run chokepoint) =="
# check-device-render.sh's step-0 chokepoint, adapted verbatim (iters
# 54-56 arc rationale documented there).
# iter 76 (review-73 H, cross-run face): stale QUIESCE markers are
# normalized FIRST — restoring a possibly-down daemon must precede the
# stale-deadman cancel and the $DTMP wipe below.
rig_qd_normalize
stale_marker=0
stale_deadman=0
nrc=0
dsh "test -f /mnt/disable_frontend" >/dev/null || nrc=$?
case "$nrc" in
  0) stale_marker=1 ;;
  1) : ;;
  *) echo "SKIP ATTRIB FAIL: startup normalization could not probe the frontend marker (rc $nrc)" >&2; exit 1 ;;
esac
nrc=0
dsh "test -e $DTMP/deadman.nonce -o -e $DTMP/deadman.pid" >/dev/null || nrc=$?
case "$nrc" in
  0) stale_deadman=1 ;;
  1) : ;;
  *) echo "SKIP ATTRIB FAIL: startup normalization could not probe for stale deadman state (rc $nrc)" >&2; exit 1 ;;
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
        echo "SKIP ATTRIB FAIL: startup normalization could not read the stale deadman pid file" >&2
        exit 1
      }
      sdm_pid="${sdm_pid%$'\n'}"
      if ! [[ "$sdm_pid" =~ ^[0-9]{1,7}$ ]]; then
        echo "SKIP ATTRIB FAIL: stale deadman.pid is not a bounded decimal pid ('$sdm_pid')" >&2
        exit 1
      fi
      nrc=0
      dsh "test -d /proc/$sdm_pid" >/dev/null || nrc=$?
      case "$nrc" in
        0)
          echo "SKIP ATTRIB FAIL: stale deadman (pid $sdm_pid) is STILL RUNNING and ignored its cancel for 12 s — a defect, not stale state; inspect the device" >&2
          exit 1
          ;;
        1)
          echo "   stale deadman.pid was reboot/kill-orphaned (pid $sdm_pid dead) — stale state"
          ;;
        *)
          echo "SKIP ATTRIB FAIL: startup normalization could not probe pid $sdm_pid liveness (rc $nrc)" >&2
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

echo "== [1/6] host data plane (g01 params + M1 tables + SIMDATA1 + trace) =="
# g01 match params — the reviewed no-eval strict parser (render-check class).
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
")" || { echo "SKIP ATTRIB FAIL: g01 manifest param extraction failed" >&2; exit 1; }
if [ -z "$gparams" ]; then
  echo "SKIP ATTRIB FAIL: g01 manifest param extraction returned nothing" >&2
  exit 1
fi
while IFS='=' read -r gk gv; do
  case "$gk" in
    name)
      if ! [[ "$gv" =~ ^[a-z0-9][a-z0-9-]*$ ]]; then
        echo "SKIP ATTRIB FAIL: manifest g01.name fails validation ('$gv')" >&2
        exit 1
      fi
      name=$gv
      ;;
    trace)
      if ! [[ "$gv" =~ ^[a-z0-9][a-z0-9-]*\.trace\.json$ ]]; then
        echo "SKIP ATTRIB FAIL: manifest g01.trace fails validation ('$gv')" >&2
        exit 1
      fi
      trace=$gv
      ;;
    seed|p1|p2|stage|frames)
      if ! [[ "$gv" =~ ^[0-9]{1,12}$ ]]; then
        echo "SKIP ATTRIB FAIL: manifest g01.$gk not a bounded decimal integer ('$gv')" >&2
        exit 1
      fi
      printf -v "$gk" '%s' "$gv"
      ;;
    *)
      echo "SKIP ATTRIB FAIL: unexpected manifest extraction line '$gk=$gv'" >&2
      exit 1
      ;;
  esac
done <<< "$gparams"
: "$name" "$seed" "$p1" "$p2" "$stage" "$frames" "$trace"
if [ "$frames" -ne "$FRAMES_PIN" ]; then
  echo "SKIP ATTRIB FAIL: manifest g01.frames ($frames) != pinned FRAMES_PIN ($FRAMES_PIN)" >&2
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
    *) echo "SKIP ATTRIB FAIL: bad char id '$1'" >&2; return 1 ;;
  esac
}
ANIM_P1="$(anim_file "$p1")"
ANIM_P2="$(anim_file "$p2")"
made "$GFXDATA_FROZEN" "$VFXDATA_FROZEN" "$VFXGLYPHS_FROZEN"
# M7 (iter 76, review-73 M — workload fidelity): (a) the frozen render
# artifacts this run pushes must hash to the task-3 gate pins — dirty
# assets can never produce "attribution evidence" for a different
# workload; (b) the pins themselves are TWIN-PINNED against
# check-device-render.sh's literal lines, so the two scripts cannot
# drift apart silently. EXACTNESS (iter 78, review-76 M4): the old
# presence-only greps passed a file where a stale pin line coexisted
# with a later last-wins assignment — rig_pin_assert_once now requires
# EXACTLY ONE assignment line per pinned var, carrying the pinned
# value, in check-device-render.sh AND in THIS script's own bytes.
SELF_SH=port/sim/device/check-skip-attrib.sh
for pf in "$GFX/check-device-render.sh" "$SELF_SH"; do
  rig_pin_assert_once "$pf" GFXDATA_SHA256 "$GFXDATA_SHA256" || exit 1
  rig_pin_assert_once "$pf" VFXDATA_SHA256 "$VFXDATA_SHA256" || exit 1
  rig_pin_assert_once "$pf" VFXGLYPHS_SHA256 "$VFXGLYPHS_SHA256" || exit 1
  rig_pin_assert_once "$pf" SHOT_FRAME "$SHOT_FRAME" || exit 1
done
asum="$(rig_host_sha256 "$GFXDATA_FROZEN")" || exit 1
if [ "$asum" != "$GFXDATA_SHA256" ]; then
  echo "SKIP ATTRIB FAIL: $GFXDATA_FROZEN sha256 $asum != pinned $GFXDATA_SHA256" >&2
  exit 1
fi
asum="$(rig_host_sha256 "$VFXDATA_FROZEN")" || exit 1
if [ "$asum" != "$VFXDATA_SHA256" ]; then
  echo "SKIP ATTRIB FAIL: $VFXDATA_FROZEN sha256 $asum != pinned $VFXDATA_SHA256" >&2
  exit 1
fi
asum="$(rig_host_sha256 "$VFXGLYPHS_FROZEN")" || exit 1
if [ "$asum" != "$VFXGLYPHS_SHA256" ]; then
  echo "SKIP ATTRIB FAIL: $VFXGLYPHS_FROZEN sha256 $asum != pinned $VFXGLYPHS_SHA256" >&2
  exit 1
fi
if [ "$SHOT_FRAME" -gt "$frames" ]; then
  echo "SKIP ATTRIB FAIL: SHOT_FRAME $SHOT_FRAME > frames $frames" >&2
  exit 1
fi
echo "   task-3 workload pins OK (gfxdata/vfxdata/vfxglyphs shas + shot frame, twin-pinned)"

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
  echo "SKIP ATTRIB FAIL: g01 trace file oracle/goldens/$trace missing" >&2
  exit 1
fi
rm -f "$DEVB/g01.trace.txt"
node "$SIM/trace-to-txt.js" "oracle/goldens/$trace" "$DEVB/g01.trace.txt"
made "$DEVB/g01.trace.txt"

echo "== [2/6] armv7 build (shared rig stamp) + push + provenance =="
rig_arm_build
rig_stamp_rehash gfx_device sk_sampler
dsh "rm -rf $DTMP $DSD && mkdir -p $DTMP $DSD"
adb -s "$DEV" push "$DEVB/gfx_device" "$DEVB/sk_sampler" \
  "$DEVB/simdata.txt" "$DEVB/g01.trace.txt" "$GFXDATA_FROZEN" \
  "$VFXDATA_FROZEN" "$VFXGLYPHS_FROZEN" \
  "$TABLES/$ANIM_P1" "$TABLES/$ANIM_P2" "$DTMP/" >/dev/null
rig_push_provenance "$DTMP" gfx_device sk_sampler
dsh "chmod +x $DTMP/gfx_device $DTMP/sk_sampler"
for hf in "$DEVB/simdata.txt" "$DEVB/g01.trace.txt" "$GFXDATA_FROZEN" \
          "$VFXDATA_FROZEN" "$VFXGLYPHS_FROZEN" \
          "$TABLES/$ANIM_P1" "$TABLES/$ANIM_P2"; do
  bn="$(basename "$hf")"
  hsum="$(rig_host_sha256 "$hf")" || exit 1
  dsum="$(rig_dev_sha256 "$DTMP/$bn")" || exit 1
  if [ "$dsum" != "$hsum" ]; then
    echo "SKIP ATTRIB FAIL: pushed $bn device sha ($dsum) != host sha ($hsum)" >&2
    exit 1
  fi
done
echo "   pushed binaries + data sha-verified on device"

# snapshot <pre|post> — the kernel-counter dumps, pulled host-side and
# judged by correlate-skips.js. Runs OUTSIDE the paced window only.
# Each dump goes through the RC-checked dsh; empty output = loud death
# (made). The per-pid table is the RAW /proc/<pid>/stat lines.
snapshot() {
  # bash-3.2 gotcha (measured this iteration): `local a="$1" b="$a"`
  # expands BOTH words before either assignment — split the statements.
  local tag d
  tag="$1"
  d="$SKAB/$tag"
  rm -rf "$d"
  mkdir -p "$d"
  dsh "cat /proc/interrupts" > "$d/interrupts.txt"
  dsh "cat /proc/stat" > "$d/stat.txt"
  dsh "cat /proc/vmstat" > "$d/vmstat.txt"
  dsh "cat /proc/softirqs" > "$d/softirqs.txt"
  dsh "cat /proc/uptime" > "$d/uptime.txt"
  dsh "cat /proc/timer_list" > "$d/timer_list.txt"
  dsh "dmesg | tail -100" > "$d/dmesg.txt"
  # cat's stderr is silenced: this adbd MERGES device stderr into the
  # stream (measured — a pid vanishing between glob and cat injected
  # "cat: can't open ..." into the table; .loop/m4-task8-a5 run). A
  # vanished-mid-scan pid is expected churn, not corruption; the
  # correlator's strict grammar still judges every surviving line.
  dsh 'for p in /proc/[0-9]*/stat; do cat "$p" 2>/dev/null; done' > "$d/pidstat.txt"
  made "$d/interrupts.txt" "$d/stat.txt" "$d/vmstat.txt" \
    "$d/softirqs.txt" "$d/uptime.txt" "$d/timer_list.txt" \
    "$d/dmesg.txt" "$d/pidstat.txt"
}

echo "== [3/6] pre-run kernel snapshot =="
snapshot pre
# (iter 76, review-73 H: the quiesce-arm daemon stops moved INTO step 4
# — after sync + deadman-arm + park, immediately before the launch — so
# the quiesce window is exactly the paced run and the on-device deadman
# backstops the whole stopped interval.)

echo "== [4/6] device: LIVE paced g01 render + attribution capture =="
DM_NONCE="$RANDOM$RANDOM$$"
rm -f "$SKAB/deadman.sh"
cat > "$SKAB/deadman.sh" << EOF
#!/bin/sh
# generated by check-skip-attrib.sh — frontend-park DEADMAN (the
# check-device-render.sh iter-52/54/55 form, adapted; iter 76 adds the
# quiesce-restore arms — review-73 H: the deadman backstops the daemon
# stops too, transport-dead or not)
echo \$\$ > $DTMP/deadman.pid
# qd_restore <comm> <init-script>: if THIS run's nonce-scoped quiesce
# marker for <comm> is present, the run stopped it and died before a
# verified restore. COMM-SCAN GUARDED start (zero instances only — the
# measured A4' stacking class); the marker is cleared ONLY after a
# rescan sees the daemon live (a failed start leaves it for the next
# run's rig_qd_normalize). Hard-coded allowlisted call sites, no eval.
qd_restore() {
  [ -f "$DTMP/qd.\$1.$DM_NONCE" ] || return 0
  n=0
  for c in /proc/[0-9]*/comm; do
    if [ "x\$(cat "\$c" 2>/dev/null)" = "x\$1" ]; then n=\$((n+1)); fi
  done
  if [ "\$n" = 0 ]; then "\$2" start; fi
  n2=0
  for c in /proc/[0-9]*/comm; do
    if [ "x\$(cat "\$c" 2>/dev/null)" = "x\$1" ]; then n2=\$((n2+1)); fi
  done
  if [ "\$n2" != 0 ]; then rm -f "$DTMP/qd.\$1.$DM_NONCE"; fi
}
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
  qd_restore low_bat_check /etc/init.d/S12low-bat-check
  qd_restore system_stats /etc/init.d/S13system-stats
  qd_restore fkgpiod /etc/init.d/S11gpio
fi
rm -f $DTMP/deadman.pid
exit 0
EOF
made "$SKAB/deadman.sh"
rm -f "$SKAB/attrib-launch.sh"
SAMPLER_LINES=""
if [ "$ARM" != nosampler ]; then
  SAMPLER_LINES="rm -f $DTMP/sk.pid $DTMP/sk.stop $DTMP/sampler.txt
setsid ./sk_sampler --out $DTMP/sampler.txt --pid-file $DTMP/sk.pid \\
  --stop-file $DTMP/sk.stop --period-ms $SAMPLER_PERIOD_MS \\
  --max-samples $SAMPLER_MAX </dev/null >/dev/null 2>&1 &"
fi
cat > "$SKAB/attrib-launch.sh" << EOF
#!/bin/sh
# generated by check-skip-attrib.sh — paced attribution-run launcher
# (check-device-render.sh detached setsid + rc-file lifecycle).
# iter 76 (review-73 M — workload fidelity): the gfx_device argv is the
# FULL task-3 gate argv (shot frame + shot outputs included) plus
# --attrib; evidence is gathered under the exact gated workload.
# iter 78 (review-76 M1): app.start.ts / app.end.ts device-clock stamps
# feed the quiesce-bracket assert; end.ts is written BEFORE the rc file
# so rc-detection implies the stamp exists.
cd $DTMP || exit 9
$SAMPLER_LINES
rm -f attrib.apprc gfx.pid.$DM_NONCE app.start.ts app.end.ts
setsid sh -c 'date +%s > $DTMP/app.start.ts; ./gfx_device \
  --trace $DTMP/g01.trace.txt --simdata $DTMP/simdata.txt \
  --gfxdata $DTMP/gfxdata-frozen.txt --vfxdata $DTMP/vfxdata-frozen.txt \
  --glyphs $DTMP/vfxglyphs-frozen.txt --legible --anim-dir $DTMP \
  --seed $seed --p1 $p1 --p2 $p2 --stage $stage --frames $frames \
  --pace 1 --budget-ns $BUDGET_NS \
  --out $DTMP/g01.att-out.txt --timing $DTMP/g01.att-tim.txt \
  --shot-frame $SHOT_FRAME --shot-ppm $DTMP/g01.att-shot.ppm \
  --shot-pgm $DTMP/g01.att-shot.pgm \
  --attrib $DTMP/g01.att-attrib.txt 2> $DTMP/g01.att-log.txt & \
  echo \$! > $DTMP/gfx.pid.$DM_NONCE; \
  wait \$!; arc=\$?; \
  date +%s > $DTMP/app.end.ts; \
  echo "RC=\$arc" > $DTMP/attrib.apprc' \
  </dev/null >/dev/null 2>&1 &
sleep 2
EOF
made "$SKAB/attrib-launch.sh"
# M7 in-script argv assert: every task-3-workload token must be present
# in the generated launcher (each token sits on one heredoc line above;
# a refactor that drops one dies HERE, not as silently different
# evidence).
for tok in "--legible" \
           "--shot-frame $SHOT_FRAME --shot-ppm $DTMP/g01.att-shot.ppm" \
           "--shot-pgm $DTMP/g01.att-shot.pgm" \
           "--pace 1 --budget-ns $BUDGET_NS" \
           "--gfxdata $DTMP/gfxdata-frozen.txt --vfxdata $DTMP/vfxdata-frozen.txt" \
           "--glyphs $DTMP/vfxglyphs-frozen.txt" \
           "--attrib $DTMP/g01.att-attrib.txt"; do
  if ! grep -qF -- "$tok" "$SKAB/attrib-launch.sh"; then
    echo "SKIP ATTRIB FAIL: generated launcher is missing the task-3 workload token '$tok'" >&2
    exit 1
  fi
done
# M4 (iter 78, review-76 M — duplicate-later-option lockout): the
# presence asserts above cannot see a DUPLICATE later option (a
# last-wins override that leaves every asserted token in place).
# Extract the gfx_device argv region from the generated launcher (the
# setsid block through the rc write — excludes sk_sampler's own --out)
# and require EXACTLY ONE occurrence of every gfx_device option.
GFXARGV="$SKAB/attrib-launch.gfxargv.txt"
rm -f "$GFXARGV"
sed -n '/setsid sh -c /,/attrib\.apprc/p' "$SKAB/attrib-launch.sh" > "$GFXARGV"
made "$GFXARGV"
if ! grep -q "gfx_device" "$GFXARGV"; then
  echo "SKIP ATTRIB FAIL: extracted argv region carries no gfx_device invocation — launcher shape drifted" >&2
  exit 1
fi
for aopt in --trace --simdata --gfxdata --vfxdata --glyphs --legible \
            --anim-dir --seed --p1 --p2 --stage --frames --pace \
            --budget-ns --out --timing --shot-frame --shot-ppm \
            --shot-pgm --attrib; do
  rig_argv_assert_once "$GFXARGV" "$aopt" || exit 1
done
echo "   launcher carries the full task-3 argv (shot frame included) + --attrib; every gfx_device option occurs exactly once"
adb -s "$DEV" push "$SKAB/deadman.sh" "$SKAB/attrib-launch.sh" "$DTMP/" >/dev/null
for hf in "$SKAB/deadman.sh" "$SKAB/attrib-launch.sh"; do
  bn="$(basename "$hf")"
  hsum="$(rig_host_sha256 "$hf")" || exit 1
  dsum="$(rig_dev_sha256 "$DTMP/$bn")" || exit 1
  if [ "$dsum" != "$hsum" ]; then
    echo "SKIP ATTRIB FAIL: pushed $bn device sha ($dsum) != host sha ($hsum)" >&2
    exit 1
  fi
done
dsh "chmod +x $DTMP/deadman.sh $DTMP/attrib-launch.sh"

# PRE-RUN SYNC (kept: strictly-less-interference mitigation, iter 73)
dsh "sync"
dsh "printf '%s' '$DM_NONCE' > $DTMP/deadman.nonce; rm -f $DTMP/deadman.cancel $DTMP/deadman.fired"
dsh "setsid sh $DTMP/deadman.sh </dev/null >/dev/null 2>&1 & sleep 1"
if ! dsh "test -f $DTMP/deadman.pid" >/dev/null 2>&1; then
  echo "SKIP ATTRIB FAIL: park deadman did not start (no pid file)" >&2
  exit 1
fi
DEADMAN_ARMED=1
echo "   deadman armed (window ${DEADMAN_S}s, nonce-scoped)"
PARKED=1
dsh "touch /mnt/disable_frontend"
prc=0
dsh "pkill gmenu2x" >/dev/null 2>&1 || prc=$?
case "$prc" in
  0) : ;;
  1) echo "WARN: gmenu2x was not running at park time" >&2 ;;
  *) echo "SKIP ATTRIB FAIL: pkill gmenu2x failed (rc $prc)" >&2; exit 1 ;;
esac

# QUIESCE-ARM daemon stops (iter 76 ordering, review-73 H: AFTER sync +
# deadman-arm + park — the window is exactly the paced run and the
# armed deadman backstops the whole stopped interval). The nonce-scoped
# marker is written PESSIMISTICALLY before each kill: a transport death
# mid-stop still gets a deadman-side comm-scan-guarded restore.
if [ "$ARM" = quiesce ]; then
  for d in $STOP_LIST; do
    # COMM-SCAN stop (riglib rig_daemon_stop: the init scripts' -K arm
    # is a measured no-op for shell daemons — see init_script comment):
    # exactly-one instance expected, killed by pid, VERIFIED gone.
    dsh "printf '' > $DTMP/qd.$d.$DM_NONCE" # marker BEFORE the kill
    DAEMONS_STOPPED="$DAEMONS_STOPPED $d" # pessimistic: trap owns restore from here
    qpid="$(rig_daemon_stop "$d")" || exit 1
    echo "   quiesce: $d (pid $qpid) stopped (comm-scan-verified gone; trap + deadman restore)"
  done
  # iter 78 (review-76 M1): device-clock stamp the instant the LAST stop
  # completes — the launch dsh is the ONLY thing between this stamp and
  # app.start.ts (the bracket assert proves it every quiesce run).
  dsh "date +%s > $DTMP/qstop.ts"
fi

t0=$(date +%s)
dsh "sh -lc $DTMP/attrib-launch.sh"
# QUIET WINDOW (iter-73 mitigation, kept): no probes for the first 50 s
sleep 50
apprc_seen=0
for _ in $(seq 1 "$APPRC_TRIES"); do
  if dsh "test -f $DTMP/attrib.apprc" >/dev/null 2>&1; then apprc_seen=1; break; fi
  sleep 2
done
t1=$(date +%s)
if [ "$apprc_seen" != 1 ]; then
  dsh "cat $DTMP/g01.att-log.txt" >&2 || true
  echo "SKIP ATTRIB FAIL: attribution run never finished (rc file absent after $((APPRC_TRIES * 2))s)" >&2
  exit 1
fi
# QUIESCE-ARM restore (iter 78 ordering, review-76 M1: the FIRST device
# action after app-exit DETECTION — ahead of the rc pull, the rc byte
# check, the sampler stop, and every other chore; a hung ADB chore can
# no longer extend the daemon-down window past the app lifetime).
# Hard-gated exact-cardinality restore; marker cleared RC-verified so
# the deadman's restore arm stands down. The bracket assert below is
# the review-76 M1 STANDING TOOTH: device-clock stamps prove the
# stop/restore bracket contains only the app lifetime.
if [ "$ARM" = quiesce ]; then
  dsh "date +%s > $DTMP/qrestore.ts"
  for d in $DAEMONS_STOPPED; do
    isc="$(init_script "$d")"
    if rig_daemon_restore "$d" "$isc"; then
      dsh "rm -f $DTMP/qd.$d.$DM_NONCE"
      dsh "test ! -f $DTMP/qd.$d.$DM_NONCE"
      echo "   quiesce restore: $d running again (comm-scan-verified, exactly 1; marker cleared)"
    else
      echo "SKIP ATTRIB FAIL: $d did not verify as running after restart — run '$isc start' on the device manually" >&2
      exit 1
    fi
  done
  DAEMONS_STOPPED="" # restored + verified; trap has nothing left to own
  qstop_ts="$(rig_dev_ts "$DTMP/qstop.ts")" || exit 1
  appstart_ts="$(rig_dev_ts "$DTMP/app.start.ts")" || exit 1
  append_ts="$(rig_dev_ts "$DTMP/app.end.ts")" || exit 1
  qrestore_ts="$(rig_dev_ts "$DTMP/qrestore.ts")" || exit 1
  rig_quiesce_bracket_assert "skip-attrib quiesce arm" \
    "$qstop_ts" "$appstart_ts" "$append_ts" "$qrestore_ts" \
    "$QW_PRE_SLACK_S" "$QW_POST_SLACK_S" || exit 1
fi
pullv "$DTMP/attrib.apprc" "$SKAB/attrib.apprc"
# iter 76 (review-73 M — rc-file BYTE grammar, the iter-61/62 pattern):
# judge the FILE BYTES against a printf-generated reference — 'RC=0',
# 'RC=0\n\n', and truncation are all corruption, never a pass.
if ! cmp -s "$SKAB/attrib.apprc" <(printf 'RC=0\n'); then
  dsh "cat $DTMP/g01.att-log.txt" >&2 || true
  echo "SKIP ATTRIB FAIL: attribution rc file is not EXACTLY the bytes 'RC=0<newline>' (got: '$(cat "$SKAB/attrib.apprc")') — app failed or the completion record is corrupt" >&2
  exit 1
fi
# stop the sampler through its designed channel and VERIFY exit
if [ "$ARM" != nosampler ]; then
  dsh "touch $DTMP/sk.stop"
  skgone=0
  for _ in $(seq 1 8); do
    if dsh "test ! -f $DTMP/sk.pid" >/dev/null 2>&1; then skgone=1; break; fi
    sleep 1
  done
  if [ "$skgone" != 1 ]; then
    echo "SKIP ATTRIB FAIL: sk_sampler did not exit within 8 s of its stop file" >&2
    exit 1
  fi
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
  echo "SKIP ATTRIB FAIL: park deadman did not exit within 12s of cancellation" >&2
  exit 1
fi
if ! dsh "test ! -f $DTMP/deadman.fired" >/dev/null 2>&1; then
  echo "SKIP ATTRIB FAIL: park deadman FIRED during a healthy run" >&2
  exit 1
fi
DEADMAN_ARMED=0
echo "   run done (host-observed $((t1 - t0)) s; app rc 0; frontend restored; deadman cancelled without firing)"

echo "== [5/6] post-run snapshot + pulls =="
# (quiesce restore happened FIRST, immediately after app-exit DETECTION
# — iter 78 exact-window ordering, bracket-asserted above; the old
# post-rc-check restore site is gone.)
snapshot post
pullv "$DTMP/g01.att-out.txt" "$SKAB/g01.att-out.txt"
pullv "$DTMP/g01.att-tim.txt" "$SKAB/g01.att-tim.txt"
pullv "$DTMP/g01.att-attrib.txt" "$SKAB/g01.att-attrib.txt"
pullv "$DTMP/g01.att-log.txt" "$SKAB/g01.att-log.txt"
# M7: the task-3 shot outputs must exist and pass the structural judge
# (the run really rendered the gated workload's forced shot frame).
pullv "$DTMP/g01.att-shot.ppm" "$SKAB/g01.att-shot.ppm"
pullv "$DTMP/g01.att-shot.pgm" "$SKAB/g01.att-shot.pgm"
if [ "$ARM" != nosampler ]; then
  pullv "$DTMP/sampler.txt" "$SKAB/sampler.txt"
fi

echo "== [6/6] host judgment: stream + app summary + correlator + needle =="
rm -f "$SKAB/g01.att-run.json"
node "$SIM/wrap-run.js" g01 "$SKAB/g01.att-out.txt" "$SKAB/g01.att-run.json"
made "$SKAB/g01.att-run.json"
node oracle/harness/verify-stream.js "$SKAB/g01.att-run.json" "$FROZEN"
echo "   device stream verified (the instrument did not perturb the sim)"
node "$GFX/judge-shot.js" "$SKAB/g01.att-shot.ppm" "$SKAB/g01.att-shot.pgm"
echo "   attribution-run shot passes the structural judge (task-3 workload rendered its shot frame)"

# app summary under the pinned whitelist grammar (check-device-render.sh
# parser, duplicated minimally: frames/pace/budget pinned into the
# pattern; wall window asserted — silently-dead pacing cannot parse).
app_re="^gfx_app: ${frames} frames, [0-9]{1,12} render skips, [0-9]{1,12} failed presents, wall [0-9]{1,12} ms, pace=1 budget=${BUDGET_NS} ns\$"
app_cnt="$(grep -Ec "$app_re" "$SKAB/g01.att-log.txt")" || true
if [ "$app_cnt" != 1 ]; then
  echo "SKIP ATTRIB FAIL: app log has $app_cnt lines matching the pinned summary grammar (want exactly 1)" >&2
  exit 1
fi
app_line="$(grep -E "$app_re" "$SKAB/g01.att-log.txt")"
if [[ "$app_line" =~ ^gfx_app:\ ${frames}\ frames,\ ([0-9]{1,12})\ render\ skips,\ ([0-9]{1,12})\ failed\ presents,\ wall\ ([0-9]{1,12})\ ms,\ pace=1\ budget=${BUDGET_NS}\ ns$ ]]; then
  app_skips="${BASH_REMATCH[1]}"
  app_present_fails="${BASH_REMATCH[2]}"
  app_wall_ms="${BASH_REMATCH[3]}"
else
  echo "SKIP ATTRIB FAIL: summary line failed re-extraction ('$app_line')" >&2
  exit 1
fi
if [ "$app_present_fails" -ne 0 ]; then
  echo "SKIP ATTRIB FAIL: $app_present_fails failed presents on the attribution run" >&2
  exit 1
fi
if [ "$app_wall_ms" -lt "$WALL_MIN_MS" ] || [ "$app_wall_ms" -gt "$WALL_MAX_MS" ]; then
  echo "SKIP ATTRIB FAIL: device wall clock ${app_wall_ms} ms outside [${WALL_MIN_MS},${WALL_MAX_MS}] ms — pacing is not running at 60 fps" >&2
  exit 1
fi
echo "   app summary OK (0 failed presents; wall ${app_wall_ms} ms; skips=${app_skips})"

# correlator (strict grammars inside; output judged from FILE BYTES —
# the iter-62 judge pattern: tail must be exactly 'attrib_complete=1\n')
CORR="$SKAB/correlate-out.txt"
rm -f "$CORR"
corr_args=(--timing "$SKAB/g01.att-tim.txt" --attrib "$SKAB/g01.att-attrib.txt"
  --frames "$frames" --budget-ns "$BUDGET_NS"
  --pre-dir "$SKAB/pre" --post-dir "$SKAB/post")
if [ "$ARM" != nosampler ]; then
  corr_args+=(--sampler "$SKAB/sampler.txt")
fi
node "$SKA/correlate-skips.js" "${corr_args[@]}" > "$CORR" || {
  echo "SKIP ATTRIB FAIL: correlator failed" >&2
  exit 1
}
made "$CORR"
if ! tail -c 18 "$CORR" | cmp -s - <(printf 'attrib_complete=1\n'); then
  echo "SKIP ATTRIB FAIL: correlator output does not END with the exact bytes 'attrib_complete=1<newline>' — corrupt evidence" >&2
  exit 1
fi
# strict key extraction (anchored, exactly-one, bounded digits)
corr_key() {
  local key="$1" cnt line
  cnt="$(grep -Ec "^${key}=[0-9]{1,12}\$" "$CORR")" || true
  if [ "$cnt" != 1 ]; then
    echo "SKIP ATTRIB FAIL: correlator output has $cnt '${key}=' lines (want exactly 1)" >&2
    exit 1
  fi
  line="$(grep -E "^${key}=[0-9]{1,12}\$" "$CORR")"
  printf '%s\n' "${line#*=}"
}
skips="$(corr_key skips)"
events="$(corr_key events)"
if [ "$skips" != "$app_skips" ]; then
  echo "SKIP ATTRIB FAIL: correlator skips ($skips) != app summary skips ($app_skips) — corrupt evidence" >&2
  exit 1
fi
# the correlated evidence itself (iter 76, review-73 M — EV whitelist
# parse + win=none reconciliation): EVERY EV line is validated against
# the FULL measured field grammar (all fields, exact order, bounded
# numerics), frames strictly increasing (duplicates = corruption), EV
# total == the events key, skipped=1 EV count == the timing-derived
# skips key, and — sampler arms — EVERY event must carry a bracketing
# kernel window (win=none = the sampler failed to cover the phenomenon
# = evidence incomplete = fail closed; rerunning IS the re-sample).
EV_ARM=sampler
[ "$ARM" = nosampler ] && EV_ARM=nosampler
node "$SKA/validate-ev.js" --corr "$CORR" --frames "$frames" \
  --arm "$EV_ARM" --skips "$skips" --events "$events" || {
  echo "SKIP ATTRIB FAIL: EV record validation failed (grammar/order/count/window reconciliation)" >&2
  exit 1
}
sed -n 's/^/   corr: /p' "$CORR" | grep -E '^   corr: (frames|budget_ns|skips|over_budget|late_start|events|nivcsw_total|nvcsw_total|minflt_total|majflt_total|mono_raw_drift_ns|sampler_samples)' || true
grep -E '^EV\|' "$CORR" | sed 's/^/   corr: /' || true

# AGENT-LOG verdict needle (M2CAL-report precedent; iter 76 FULL-LINE
# grammar + resemblance counter — review-73 M). Matrix mode
# (evidence-gathering, BEFORE the verdict exists) skips this assert.
if [ "${MLFK_SKATTRIB_MATRIX:-0}" = 0 ]; then
  ncnt="$(grep -Ec "$NEEDLE_FULL" docs/AGENT-LOG.md)" || true
  rcnt="$(grep -Ec "$NEEDLE_RESEMBLE" docs/AGENT-LOG.md)" || true
  case "$ncnt$rcnt" in
    *[!0-9]*) echo "SKIP ATTRIB FAIL: needle grep counts non-numeric ('$ncnt'/'$rcnt')" >&2; exit 1 ;;
  esac
  if [ "$ncnt" != 1 ]; then
    echo "SKIP ATTRIB FAIL: docs/AGENT-LOG.md carries $ncnt full-line 'SKIP ATTRIB VERDICT: (a|b|c)' needle lines (want exactly 1 — the canonical standalone verdict line)" >&2
    exit 1
  fi
  if [ "$rcnt" != "$ncnt" ]; then
    echo "SKIP ATTRIB FAIL: $((rcnt - ncnt)) AGENT-LOG line(s) START like the verdict needle but fail the full-line grammar — corruption (quoted/truncated/negated text can never ride)" >&2
    exit 1
  fi
  echo "   AGENT-LOG verdict needle present (exactly 1 full-line match, 0 resemblances)"
else
  echo "   matrix mode: needle assert skipped (evidence-gathering run)"
fi

rig_no_commit_guard "$DEVB" "$TABLES"

# FINAL VERDICT (iter 76, review-73 M — degraded-mode lockout): the
# gating sentinel exists ONLY inside the authoritative branch; every
# degraded combination lands in the DEV banner + exit 3.
if [ "$SKA_AUTHORITATIVE" = 1 ]; then
  SKA_OK=1 # the exit-guard release: ONLY this line may precede a 0 exit
  echo "SKIP ATTRIB OK (arm=$ARM, skips=${skips}/${frames}, events=${events}, stream MATCH)"
else
  echo "SKIP ATTRIB (DEV — NON-AUTHORITATIVE):${SKA_AUTH_REASONS} arm=$ARM skips=${skips}/${frames} events=${events} stream MATCH — evidence recorded, gate verdict withheld"
  exit 3
fi
