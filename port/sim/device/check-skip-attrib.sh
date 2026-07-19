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
#                 (trap-restored + pidof-verified; device hygiene)
#   MLFK_SKATTRIB_STOP     = daemon list for the quiesce arm; allowlist
#     ONLY low_bat_check / system_stats / fkgpiod (their start-stop
#     init scripts are the designed stop/restart channels)
#   MLFK_SKATTRIB_MATRIX=1 — evidence-gathering mode: the AGENT-LOG
#     needle assert is skipped (the matrix runs BEFORE the verdict can
#     exist). The done-check never sets it.
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
NEEDLE='SKIP ATTRIB VERDICT: \((a|b|c)\)' # AGENT-LOG verdict needle (§4)

# --- arm selection -----------------------------------------------------------
ARM="${MLFK_SKATTRIB_ARM:-sampler}"
case "$ARM" in
  nosampler|sampler|quiesce) : ;;
  *) echo "SKIP ATTRIB FAIL: MLFK_SKATTRIB_ARM '$ARM' not in {nosampler,sampler,quiesce}" >&2; exit 1 ;;
esac
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

ska_cleanup() {
  local rc=$?
  local prc restore_verified d isc
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
  # restarted through the init-script START channel and its liveness
  # VERIFIED by comm-scan — a restart that cannot be verified is a
  # LOUD warn naming the exact manual recovery command (never silent).
  for d in $DAEMONS_STOPPED; do
    isc="$(init_script "$d")" || continue
    if rig_daemon_restore "$d" "$isc"; then
      echo "   quiesce restore: $d verified running again (comm-scan)" >&2
    else
      echo "WARN: $d did NOT verify as running after restart — run '$isc start' on the device manually" >&2
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

echo "== [3/6] pre-run kernel snapshot + arm '$ARM' setup =="
snapshot pre
if [ "$ARM" = quiesce ]; then
  for d in $STOP_LIST; do
    # COMM-SCAN stop (riglib rig_daemon_stop: the init scripts' -K arm
    # is a measured no-op for shell daemons — see init_script comment):
    # exactly-one instance expected, killed by pid, VERIFIED gone.
    DAEMONS_STOPPED="$DAEMONS_STOPPED $d" # pessimistic: trap owns restore from here
    qpid="$(rig_daemon_stop "$d")" || exit 1
    echo "   quiesce: $d (pid $qpid) stopped (comm-scan-verified gone; trap restores)"
  done
fi

echo "== [4/6] device: LIVE paced g01 render + attribution capture =="
DM_NONCE="$RANDOM$RANDOM$$"
rm -f "$SKAB/deadman.sh"
cat > "$SKAB/deadman.sh" << EOF
#!/bin/sh
# generated by check-skip-attrib.sh — frontend-park DEADMAN (the
# check-device-render.sh iter-52/54/55 form, adapted)
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
# (check-device-render.sh detached setsid + rc-file lifecycle)
cd $DTMP || exit 9
$SAMPLER_LINES
rm -f attrib.apprc gfx.pid.$DM_NONCE
setsid sh -c './gfx_device \
  --trace $DTMP/g01.trace.txt --simdata $DTMP/simdata.txt \
  --gfxdata $DTMP/gfxdata-frozen.txt --vfxdata $DTMP/vfxdata-frozen.txt \
  --glyphs $DTMP/vfxglyphs-frozen.txt --legible --anim-dir $DTMP \
  --seed $seed --p1 $p1 --p2 $p2 --stage $stage --frames $frames \
  --pace 1 --budget-ns $BUDGET_NS \
  --out $DTMP/g01.att-out.txt --timing $DTMP/g01.att-tim.txt \
  --attrib $DTMP/g01.att-attrib.txt 2> $DTMP/g01.att-log.txt & \
  echo \$! > $DTMP/gfx.pid.$DM_NONCE; \
  wait \$!; \
  echo "RC=\$?" > $DTMP/attrib.apprc' \
  </dev/null >/dev/null 2>&1 &
sleep 2
EOF
made "$SKAB/attrib-launch.sh"
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
pullv "$DTMP/attrib.apprc" "$SKAB/attrib.apprc"
if [ "$(cat "$SKAB/attrib.apprc")" != "RC=0" ]; then
  dsh "cat $DTMP/g01.att-log.txt" >&2 || true
  echo "SKIP ATTRIB FAIL: attribution run app exited nonzero ($(cat "$SKAB/attrib.apprc"))" >&2
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

echo "== [5/6] post-run snapshot + quiesce restore + pulls =="
snapshot post
if [ "$ARM" = quiesce ]; then
  for d in $DAEMONS_STOPPED; do
    isc="$(init_script "$d")"
    if rig_daemon_restore "$d" "$isc"; then
      echo "   quiesce restore: $d running again (comm-scan-verified)"
    else
      echo "SKIP ATTRIB FAIL: $d did not verify as running after restart — run '$isc start' on the device manually" >&2
      exit 1
    fi
  done
  DAEMONS_STOPPED="" # restored + verified; trap has nothing left to own
fi
pullv "$DTMP/g01.att-out.txt" "$SKAB/g01.att-out.txt"
pullv "$DTMP/g01.att-tim.txt" "$SKAB/g01.att-tim.txt"
pullv "$DTMP/g01.att-attrib.txt" "$SKAB/g01.att-attrib.txt"
pullv "$DTMP/g01.att-log.txt" "$SKAB/g01.att-log.txt"
if [ "$ARM" != nosampler ]; then
  pullv "$DTMP/sampler.txt" "$SKAB/sampler.txt"
fi

echo "== [6/6] host judgment: stream + app summary + correlator + needle =="
rm -f "$SKAB/g01.att-run.json"
node "$SIM/wrap-run.js" g01 "$SKAB/g01.att-out.txt" "$SKAB/g01.att-run.json"
made "$SKAB/g01.att-run.json"
node oracle/harness/verify-stream.js "$SKAB/g01.att-run.json" "$FROZEN"
echo "   device stream verified (the instrument did not perturb the sim)"

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
# the correlated evidence itself: every skip must appear as an EV record
ev_cnt="$(grep -Ec '^EV\|frame=[0-9]{1,7}\|' "$CORR")" || true
if [ "$ev_cnt" != "$events" ]; then
  echo "SKIP ATTRIB FAIL: correlator EV record count ($ev_cnt) != events key ($events)" >&2
  exit 1
fi
sed -n 's/^/   corr: /p' "$CORR" | grep -E '^   corr: (frames|budget_ns|skips|over_budget|late_start|events|nivcsw_total|nvcsw_total|minflt_total|majflt_total|mono_raw_drift_ns|sampler_samples)' || true
grep -E '^EV\|' "$CORR" | sed 's/^/   corr: /' || true

# AGENT-LOG verdict needle (M2CAL-report precedent). Matrix mode
# (evidence-gathering, BEFORE the verdict exists) skips this assert.
if [ "${MLFK_SKATTRIB_MATRIX:-0}" != 1 ]; then
  ncnt="$(grep -Ec "$NEEDLE" docs/AGENT-LOG.md)" || true
  if [ "$ncnt" -lt 1 ]; then
    echo "SKIP ATTRIB FAIL: docs/AGENT-LOG.md carries no 'SKIP ATTRIB VERDICT: (a|b|c)' needle — the attribution verdict must be recorded with its data" >&2
    exit 1
  fi
  echo "   AGENT-LOG verdict needle present ($ncnt)"
else
  echo "   matrix mode: needle assert skipped (evidence-gathering run)"
fi

rig_no_commit_guard "$DEVB" "$TABLES"

SKA_OK=1 # the exit-guard release: ONLY this line may precede a 0 exit
echo "SKIP ATTRIB OK (arm=$ARM, skips=${skips}/${frames}, events=${events}, stream MATCH)"
