# port/sim/device/riglib.sh — SHARED plumbing for the M3 device rig.
#
# Extracted VERBATIM from check-device-g01.sh after its Tier-A review arc
# closed at VERDICT: GO (.loop/review-42-1.log; iters 38-42) — the block
# comments carry that arc's provenance. This file is the "inheritance
# package" (STATE.md, arc closure): every M3 device check script sources
# it and gets the reviewed plumbing as a CLASS, never a copy-paste.
#
# Contract: source port/sim/device/adbsh.sh FIRST ($DEV, dsh,
# require_device), then this file. Callers must set before use:
#   DEVB   — host build dir (port/sim/calib/build/device)
#   TABLES — generated-tables dir (pipeline/build/sim-tables)
#   FDC    — oracle/fdlibm-crosscheck (srchash input: csweep.c)
#   DTMP DSD — device scratch dirs (/tmp/mlfk, /mnt/mlfk-scratch)
# and call the rig_* functions from a `set -euo pipefail` script running
# at the repo root.
#
# RIG_SCRIPTS — every script whose bytes are ARM-BUILD STAMP INPUT (the
# build recipe/flags live in rig_arm_build's heredoc; a script that
# drives the rig can change what "current" means, so its bytes key the
# cache). ANY new device check script MUST add itself here — both so its
# own edits force a rebuild and so ALL rig scripts always compute the
# SAME stamp (one shared build, no ping-pong between scripts).
RIG_SCRIPTS="port/sim/device/adbsh.sh port/sim/device/riglib.sh \
port/sim/device/check-device-g01.sh port/sim/device/check-device-conform.sh \
port/gfx/check-device-render.sh port/gfx/check-device-input.sh \
port/gfx/check-device-audio.sh port/gfx/check-device-opk.sh \
port/gfx/check-device-music.sh \
port/sim/device/check-skip-attrib.sh \
port/foh/check-device-foh.sh \
port/sim/target/check-device-target.sh \
port/foh/check-device-persist.sh \
port/sim/device/verify_m3.sh"

# The armv7 binaries the shared build produces (one docker run).
# gfx_device (M3 task 4) is the SDL1.2 render app: DYNAMICALLY linked
# against the sysroot's libSDL-1.2 (LGPL — dynamic only, CLAUDE.md
# licensing rule; rig_arm_build asserts it), raster TU at -O3 (the ONE
# -O3 TU), everything else -O2; -ffp-contract=off on every TU.
# fk_input (M3 task 5) is the static uinput button injector
# (port/tools/fk_input.c — the ssb64 pattern; no SDL, no sim).
# sk_sampler (M4 task 8) is the static fork-free kernel-counter sampler
# (port/sim/device/skip-attrib/sk_sampler.c — diagnostic instrument).
# foh_device (M4 task 10) is the SDL1.2 FOH app (menus + launch bridge
# + match render): DYNAMIC SDL link like gfx_device (LGPL rule), the
# same raster -O3 TU, fdlibm strong overrides asserted.
ARMBINS="sim_device csweep_arm fmt_diff_arm mathsweep_arm gfx_device fk_input sk_sampler foh_device"

# rig_orphan_reap <step0|cleanup> — the ORPHANED-DEADMAN LEAK class fix
# (iter 102; driver post-iter-101 adjudication: failed runs leaked
# /tmp/mlfk/deadman.sh 2 s fork combs — the graceful deadman.cancel is
# LOST BY DESIGN on every failure path, because rig_cleanup's $DTMP
# wipe deletes the cancel file inside the deadman's 2 s poll window;
# each leaked comb stalls later paced runs, the iter-74 low_bat_check
# mechanism, skips doubling with orphan count 7->14). ONE dsh: scan
# /proc for any process whose cmdline references the rig's device
# dirs (/tmp/mlfk, /mnt/mlfk-scratch — split-literal patterns so the
# scanning shell's OWN cmdline can never match itself; $$/$PPID
# excluded as belt), LOUDLY log each find (pid+comm+cmdline), kill by
# pid (TERM, bounded wait, KILL escalation), then re-scan — the final
# `MLFKSCAN <found> <left>` line is whitelist-parsed and left must be
# 0. Modes:
#   step0   — the SHARED RIG ENTRY scan (called from rig_lock_acquire,
#             so EVERY rig-sourcing device check inherits it by
#             construction — a check must never start over a stale
#             comb). LOUD clean-then-proceed; on a find it also
#             assumes the killed deadman's duties (rig_qd_normalize
#             daemon restore off the markers + stale
#             /mnt/disable_frontend removal — killing the backstop
#             may never strand what it guarded). Any failure = loud
#             death (the lock is released explicitly: no trap exists
#             yet at rig_lock_acquire time).
#   cleanup — the ALL-EXIT-PATHS teardown (called from rig_cleanup
#             before the $DTMP wipe, RIG_PRESERVE_DTMP != 1 only —
#             the PRESERVE arm keeps the deadman armed ON PURPOSE,
#             the review-52 backstop design). Best-effort +
#             WARN-visible: cleanup never masks the run's rc.
# rig_orphan_parse <out> — PURE host validator of the device scan output
# (review-102 M-c; factored out so the failure paths are host-unit-
# toothable). Whitelist grammar: every line is MLFKPROC/MLFKSCAN/
# MLFKUNREADABLE; EXACTLY one MLFKSCAN (the last line) matching
# `MLFKSCAN <found> <left>`; MLFKPROC row count == <found> (the
# reconciliation the reap previously skipped); ANY MLFKUNREADABLE row is
# a live-unreadable proc we could not classify = fail closed. Echoes
# `OK <found> <left>` and returns 0 on success, or `FAIL <reason>` and
# returns 1. Never touches the device.
rig_orphan_parse() {
  local out="$1" line last nscan nproc nunread nfound nleft
  out="${out%$'\n'}"
  last="${out##*$'\n'}"
  nscan=0; nproc=0; nunread=0
  while IFS= read -r line; do
    case "$line" in
      MLFKSCAN\ *)       nscan=$((nscan + 1)) ;;
      MLFKPROC\ *)       nproc=$((nproc + 1)) ;;
      MLFKUNREADABLE\ *) nunread=$((nunread + 1)) ;;
      *) printf 'FAIL non-whitelisted scan line: %s' "$line"; return 1 ;;
    esac
  done <<< "$out"
  if [ "$nscan" != 1 ] || ! [[ "$last" =~ ^MLFKSCAN\ ([0-9]{1,4})\ ([0-9]{1,4})$ ]]; then
    printf 'FAIL malformed/absent MLFKSCAN summary (nscan=%s last=%s)' "$nscan" "$last"; return 1
  fi
  nfound="${BASH_REMATCH[1]}"; nleft="${BASH_REMATCH[2]}"
  if [ "$nunread" != 0 ]; then
    printf 'FAIL %s live UNREADABLE /proc entr(ies) during the scan (cannot classify - fail closed)' "$nunread"; return 1
  fi
  if [ "$nproc" != "$nfound" ]; then
    printf 'FAIL MLFKPROC rows (%s) != reaped count (%s) - reconciliation mismatch' "$nproc" "$nfound"; return 1
  fi
  printf 'OK %s %s' "$nfound" "$nleft"; return 0
}

rig_orphan_reap() {
  local mode out rc line last nfound nleft presult
  mode="$1"
  rc=0
  out="$(dsh '
mlfk_scan() {
  for p in /proc/[0-9]*; do
    pid="${p#/proc/}"
    [ "$pid" = "$$" ] && continue
    [ "$pid" = "$PPID" ] && continue
    # let cat open the file (its OWN stderr -> /dev/null): a process that
    # vanishes mid-scan is the common case, and a SHELL "< $p/cmdline"
    # redirect prints an un-suppressible open error to real stderr that
    # adb merges into the captured output and breaks the whitelist
    # grammar. cat swallows the vanish; an empty cl simply never matches.
    cl="$(cat "$p/cmdline" 2>/dev/null | tr "\0" " ")"
    if [ -z "$cl" ]; then
      # M-c (review-102): an empty read is NOT automatically clean.
      # Distinguish a VANISHED proc (dir gone -> benign, skip) from a
      # LIVE UNREADABLE one (dir + cmdline present but not readable ->
      # a stale mlfk process we cannot inspect -> LOUD). Kernel threads
      # / zombies have a readable-but-empty cmdline and never match the
      # rig dirs, so they fall through here harmlessly.
      if [ -d "$p" ] && [ -e "$p/cmdline" ] && [ ! -r "$p/cmdline" ]; then
        echo "MLFKUNREADABLE $pid"
      fi
      continue
    fi
    cm=""
    matched=0
    case "$cl" in
      *"/tmp/m""lfk"*|*"/mnt/m""lfk-scratch"*) matched=1 ;;
    esac
    if [ "$matched" = 0 ]; then
      # L-a (review-102): the relative-cwd spelling (cd into a rig scratch
      # dir, then ./deadman.sh) carries NO dir literal in cmdline. Also
      # match when comm is a rig shell/app AND the process CWD resolves
      # under a rig dir. NOTE: the rig-dir paths below are SPLIT literals
      # ("/tmp/m""lfk") so the scanning shell OWN cmdline can never match
      # itself; never write the contiguous path anywhere in this dsh
      # block (a comment counts: it is part of the device cmdline). No
      # apostrophes here either: this whole block is a single-quoted dsh
      # string and one stray quote would close it early.
      cm="$(cat "$p/comm" 2>/dev/null)"
      case "$cm" in
        sh|ash|busybox|deadman.sh|foh_device)
          cwd="$(readlink "$p/cwd" 2>/dev/null)"
          case "$cwd" in
            "/tmp/m""lfk"|"/tmp/m""lfk/"*|"/mnt/m""lfk-scratch"|"/mnt/m""lfk-scratch/"*) matched=1 ;;
          esac
          ;;
      esac
    fi
    if [ "$matched" = 1 ]; then
      [ -n "$cm" ] || cm="$(cat "$p/comm" 2>/dev/null)"
      echo "$pid ${cm:-?} $cl"
    fi
  done
}
n=0
pids=""
OUT="$(mlfk_scan)"
if [ -n "$OUT" ]; then
  # match rows (`<pid> <comm> <cmdline>`) -> MLFKPROC; a live-unreadable
  # row (`MLFKUNREADABLE <pid>`) is passed through verbatim for the host
  # parser to fail on. Only match rows are killed + counted in n.
  printf "%s\n" "$OUT" | while IFS= read -r row; do
    case "$row" in
      MLFKUNREADABLE\ *) echo "$row" ;;
      *) echo "MLFKPROC $row" ;;
    esac
  done
  for pid in $(printf "%s\n" "$OUT" | while IFS= read -r row; do
      case "$row" in MLFKUNREADABLE\ *) : ;; *) echo "${row%% *}" ;; esac
    done); do
    kill "$pid" 2>/dev/null
    pids="$pids $pid"
    n=$((n+1))
  done
  sleep 3
  for pid in $pids; do
    if [ -d "/proc/$pid" ]; then kill -9 "$pid" 2>/dev/null; fi
  done
  sleep 1
fi
left=0
OUT2="$(mlfk_scan)"
if [ -n "$OUT2" ]; then left="$(printf "%s\n" "$OUT2" | grep -c "")"; fi
echo "MLFKSCAN $n $left"')" || rc=$?
  if [ "$rc" != 0 ]; then
    if [ "$mode" = step0 ]; then
      echo "DEVICE FAIL: step-0 mlfk orphan reap could not run (dsh rc $rc)" >&2
      [ -n "${LOCK:-}" ] && rm -rf "$LOCK"
      exit 1
    fi
    echo "WARN: cleanup mlfk orphan reap could not run (dsh rc $rc) — stale processes may remain on the device" >&2
    return 1
  fi
  # LOUD-log each reaped/unreadable row (intent unchanged), then validate
  # + reconcile via the PURE host parser (review-102 M-c: MLFKUNREADABLE
  # = fail closed; MLFKPROC row count reconciled == reaped count).
  while IFS= read -r line; do
    case "$line" in
      MLFKPROC\ *)
        echo "WARN: stale mlfk process found + reaped [$mode]: ${line#MLFKPROC }" >&2 ;;
      MLFKUNREADABLE\ *)
        echo "WARN: LIVE UNREADABLE /proc entry during mlfk scan [$mode]: pid ${line#MLFKUNREADABLE } (cannot classify — failing the scan closed)" >&2 ;;
    esac
  done <<< "${out%$'\n'}"
  # `|| true`: rig_orphan_parse returns 1 on a FAIL verdict BY DESIGN, and
  # we inspect the OK/FAIL prefix below — never let a caller's `set -e`
  # exit here before the loud death + explicit lock release run.
  presult="$(rig_orphan_parse "$out")" || true
  if [ "${presult%% *}" != OK ]; then
    if [ "$mode" = step0 ]; then
      echo "DEVICE FAIL: step-0 mlfk orphan reap output failed validation: ${presult#FAIL }" >&2
      printf '%s\n' "$out" >&2
      [ -n "${LOCK:-}" ] && rm -rf "$LOCK"
      exit 1
    fi
    echo "WARN: cleanup mlfk orphan reap output failed validation (stale processes may remain): ${presult#FAIL }" >&2
    printf '%s\n' "$out" >&2
    return 1
  fi
  [[ "$presult" =~ ^OK\ ([0-9]+)\ ([0-9]+)$ ]] || {
    echo "DEVICE FAIL: rig_orphan_parse returned an unparseable OK line ('$presult')" >&2
    [ "$mode" = step0 ] && { [ -n "${LOCK:-}" ] && rm -rf "$LOCK"; exit 1; }
    return 1
  }
  nfound="${BASH_REMATCH[1]}"
  nleft="${BASH_REMATCH[2]}"
  if [ "$nleft" != 0 ]; then
    if [ "$mode" = step0 ]; then
      echo "DEVICE FAIL: $nleft stale mlfk process(es) SURVIVED the step-0 reap (found $nfound) — inspect the device before running any check" >&2
      [ -n "${LOCK:-}" ] && rm -rf "$LOCK"
      exit 1
    fi
    echo "WARN: $nleft stale mlfk process(es) survived the cleanup reap (found $nfound)" >&2
    return 1
  fi
  if [ "$nfound" != 0 ] && [ "$mode" = step0 ]; then
    # duty transfer: the reaped deadman can no longer restore what a
    # dead run left behind — restore it here, loudly, BEFORE any check
    # work: quiesced daemons off their markers, then a stale frontend
    # park marker (RC-verified gone).
    if ! rig_qd_normalize; then
      echo "DEVICE FAIL: step-0 reap killed a stale deadman but could not restore a marker-quiesced daemon — inspect the device" >&2
      [ -n "${LOCK:-}" ] && rm -rf "$LOCK"
      exit 1
    fi
    local mrc=0
    dsh "test -f /mnt/disable_frontend" >/dev/null 2>&1 || mrc=$?
    if [ "$mrc" = 0 ]; then
      if ! dsh "rm -f /mnt/disable_frontend" >/dev/null 2>&1 ||
         ! dsh "test ! -f /mnt/disable_frontend" >/dev/null 2>&1; then
        echo "DEVICE FAIL: step-0 reap could not remove the stale /mnt/disable_frontend left by the reaped run" >&2
        [ -n "${LOCK:-}" ] && rm -rf "$LOCK"
        exit 1
      fi
      echo "   step-0 reap: stale /mnt/disable_frontend removed (RC-verified gone)" >&2
    elif [ "$mrc" != 1 ]; then
      echo "DEVICE FAIL: step-0 reap could not probe the frontend marker (rc $mrc)" >&2
      [ -n "${LOCK:-}" ] && rm -rf "$LOCK"
      exit 1
    fi
    echo "   step-0 reap: $nfound stale mlfk process(es) cleaned; device comb-free" >&2
  fi
  return 0
}

# rig_lock_acquire — exclusive rig lock (iter 41, review rounds 1-3
# recurring — the class is closed by REMOVING the cleverness): ONE
# mkdir-atomic lock at a SHARED host path keyed by the DEVICE id — the
# device is the shared resource, so every checkout/worktree on this host
# contends on the SAME lock — and NO reclamation logic at all: no pid
# files, no liveness probes, no auto-delete. An existing lock is a loud
# death telling the operator to remove it by hand; a stranded lock
# (crash in the instant between mkdir and trap install) costs one manual
# rm -rf, which the message spells out. The release trap (rig_cleanup)
# is installed by the CALLER only AFTER acquisition, so a losing
# contender can never release the winner's lock.
# Iter 102: acquisition is followed by the step-0 orphan reap
# (rig_orphan_reap above) — the shared rig entry, inherited by every
# consumer; it runs only AFTER the lock is held, so it can never touch
# the device while another rig run legitimately owns it.
rig_lock_acquire() {
  LOCK="${TMPDIR:-/tmp}/mlfk-rig-${DEV}.lock"
  if ! mkdir "$LOCK" 2>/dev/null; then
    local lockage lockmtime
    lockage="unknown"
    if lockmtime="$(stat -f %m "$LOCK" 2>/dev/null || stat -c %Y "$LOCK" 2>/dev/null)"; then
      lockage="$(( $(date +%s) - lockmtime )) s"
    fi
    echo "DEVICE FAIL: rig lock $LOCK already exists (age: $lockage)." >&2
    echo "  Another rig run may be using device $DEV right now. If you" >&2
    echo "  are sure no rig is running, remove it manually: rm -rf '$LOCK'" >&2
    exit 1
  fi
  rig_orphan_reap step0
}

# rig_cleanup — hygiene: device scratch never outlives the script.
# Best-effort BY DESIGN (cleanup must never mask the run's real exit
# code) but routed through dsh and VISIBLE when it fails (iter 39,
# review L2). Callers install it: trap rig_cleanup EXIT (AFTER
# rig_lock_acquire).
#
# RIG_PRESERVE_DTMP=1 (iter 54, review-52 H — deadman disarm ordering):
# a caller that armed an on-device deadman whose NONCE lives in $DTMP
# sets this when the frontend restore could NOT be verified — the
# scratch wipe must not disarm the backstop (a recovered transport
# running `rm -rf $DTMP` would delete the nonce while
# /mnt/disable_frontend remains, and the deadman would then fail its
# nonce check and never restore). With it set, only $DSD is wiped and
# $DTMP (nonce + deadman script) survives so the deadman STAYS armed —
# that is its purpose. A LATER run's re-arm wipe (`rm -rf $DTMP` before
# push) still disarms any stale deadman by design. Default unchanged.
rig_cleanup() {
  if [ "${RIG_PRESERVE_DTMP:-0}" = 1 ]; then
    echo "WARN: preserving $DTMP on the device (armed deadman nonce lives there) — only $DSD is wiped" >&2
    dsh "rm -rf $DSD" >/dev/null 2>&1 \
      || echo "WARN: device scratch cleanup failed — $DSD may remain on the device" >&2
  else
    # iter 102 (the orphaned-deadman leak class): reap stale mlfk
    # processes BY PID before the wipe. The graceful deadman.cancel is
    # structurally lossy on failure paths — the wipe below deletes the
    # cancel file inside the deadman's 2 s poll window, so the comb
    # never sees it and outlives the run (the iter-101 leak). Every
    # consumer's EXIT trap routes through this function, so teardown
    # now covers ALL exit paths by construction. Best-effort +
    # WARN-visible (cleanup never masks the run's real exit code).
    # review-102 M-d (reap-failure race): if the reap FAILS, a still-live
    # deadman may be polling its cancel/nonce in $DTMP — wiping $DTMP now
    # would strand the comb (the very leak this reap exists to prevent).
    # So on reap failure PRESERVE $DTMP (wipe only $DSD, log loud, leave
    # the cancel marker for the deadman to self-terminate on); the next
    # check's step-0 scan reaps the residual. Never wipe the signal a
    # live process depends on.
    if rig_orphan_reap cleanup; then
      dsh "rm -rf $DTMP $DSD" >/dev/null 2>&1 \
        || echo "WARN: device scratch cleanup failed — $DTMP $DSD may remain on the device" >&2
    else
      echo "WARN: cleanup orphan reap FAILED — PRESERVING $DTMP (a surviving deadman may still be polling its cancel/nonce there; wiping now would strand the comb). Only $DSD is wiped; the next check's step-0 scan reaps the residual." >&2
      dsh "rm -rf $DSD" >/dev/null 2>&1 \
        || echo "WARN: device scratch cleanup failed — $DSD may remain on the device" >&2
    fi
  fi
  rm -rf "$LOCK" 2>/dev/null \
    || echo "WARN: could not release rig lock $LOCK — remove manually: rm -rf '$LOCK'" >&2
}

# rig_dsh_retry <cmd> — best-effort dsh with TRANSPORT recovery (iter 52,
# review-50 H1: cleanup was attempted once through the same ADB transport
# that had just failed — a mid-run transport death stranded the parked
# frontend). On dsh rc 70/71 (transport / no-marker — both transport
# classes), reset the adb transport (kill-server / start-server /
# reconnect) and retry, <= 3 attempts, every step WARN-visible. Any
# OTHER rc is the device command's own and is returned as-is. For
# cleanup paths only — main-path commands stay plain dsh (fail loud).
rig_dsh_retry() {
  local tries=0 rc
  while :; do
    rc=0
    dsh "$1" >/dev/null 2>&1 || rc=$?
    if [ "$rc" != 70 ] && [ "$rc" != 71 ]; then return "$rc"; fi
    tries=$((tries + 1))
    if [ "$tries" -ge 3 ]; then
      echo "WARN: rig_dsh_retry: transport still dead after $tries attempts (rc $rc) for: $1" >&2
      return "$rc"
    fi
    echo "WARN: rig_dsh_retry: adb transport failure (rc $rc) — resetting the transport and retrying ($tries/2)" >&2
    adb kill-server >/dev/null 2>&1 || true
    sleep 3
    adb start-server >/dev/null 2>&1 || true
    adb -s "$DEV" reconnect >/dev/null 2>&1 || true
    sleep 3
  done
}

# rig_proc_pid <procname> — RC-checked nonce-dsh `pidof` under the
# measured single-pid grammar (busybox pidof, single-instance gmenu2x:
# one line, one bounded decimal pid). Echoes the pid; returns 1 when
# the process is not running or the answer is not exactly one bounded
# pid. Added iter 62 (review-60 L1-residual) as the PRE-KILL capture
# companion of rig_proc_respawn_poll's true-respawn form.
rig_proc_pid() {
  local ppid
  ppid="$(dsh "pidof $1" 2>/dev/null)" || return 1
  ppid="${ppid%$'\n'}"
  [[ "$ppid" =~ ^[0-9]{1,7}$ ]] || return 1
  printf '%s\n' "$ppid"
}

# rig_proc_respawn_poll <procname> <tries> [oldpid] — bounded FOREGROUND
# poll (PROCESS §7#1) for a device process to be RUNNING, via RC-checked
# nonce-dsh `pidof`. Added iter 60 (review-58 L1: frontend restoration
# must be VERIFIED, never assumed from a masked pkill rc) — the
# measured check-device-opk.sh step-6 respawn-poll pattern factored as
# a CLASS so launch-precondition and restoration sites share ONE body.
# TRUE-RESPAWN form (iter 62, review-60 L1-residual): when the caller
# supplies the PRE-KILL pid (captured via rig_proc_pid BEFORE its
# pkill), a verified respawn requires BOTH (a) the old pid to have
# DISAPPEARED (/proc/<oldpid> gone, RC-checked — a lingering old
# process under a slow SIGTERM can never satisfy the poll) AND (b) a
# live single-pid pidof answer with pid != oldpid (the old pid's mere
# disappearance without a successor never verifies, and a pid echo of
# the old process is never accepted). Without [oldpid] (process was not
# running pre-kill) any verified single live pid is a respawn.
# Measured producer grammar (busybox pidof, single-instance gmenu2x):
# one line, one bounded decimal pid; anything else (multi-pid, empty,
# junk) is NOT a verified respawn and the poll keeps waiting. Echoes
# the NEW pid on success; returns 1 when the process never verifies
# within <tries> x 1 s — the CALLER decides loud-fail vs loud-warn.
rig_proc_respawn_poll() {
  local pname tries oldpid ppid orc
  pname="$1"
  tries="$2"
  oldpid="${3:-}"
  if [ -n "$oldpid" ] && ! [[ "$oldpid" =~ ^[0-9]{1,7}$ ]]; then
    echo "DEVICE FAIL: rig_proc_respawn_poll oldpid not a bounded decimal pid ('$oldpid')" >&2
    return 1
  fi
  for _ in $(seq 1 "$tries"); do
    sleep 1
    if [ -n "$oldpid" ]; then
      orc=0
      dsh "test ! -d /proc/$oldpid" >/dev/null 2>&1 || orc=$?
      # old pid still alive (or the probe failed): NOT a respawn yet
      [ "$orc" = 0 ] || continue
    fi
    ppid="$(dsh "pidof $pname" 2>/dev/null)" || continue
    ppid="${ppid%$'\n'}"
    if [[ "$ppid" =~ ^[0-9]{1,7}$ ]]; then
      if [ -n "$oldpid" ] && [ "$ppid" = "$oldpid" ]; then
        continue # pid echo of the old process — never a verified respawn
      fi
      printf '%s\n' "$ppid"
      return 0
    fi
  done
  return 1
}

# rig_comm_pids <name> — pids whose /proc/<pid>/comm is EXACTLY <name>
# (M4 task 8, iter 74). busybox pidof CANNOT see #!/bin/sh daemons
# (comm = script name but argv[0] = sh), and busybox
# `start-stop-daemon -K -x <script>` matches /proc/<pid>/exe — busybox
# for a script — so the OS's own stop channel is a measured NO-OP for
# the FunKey's shell daemons; the comm scan sees every process class
# uniformly. Whitelist-parsed: every output line must be a bounded
# decimal pid, else loud death. Echoes zero+ pid lines.
rig_comm_pids() {
  local d out line
  d="$1"
  out="$(dsh 'for c in /proc/[0-9]*/comm; do n="$(cat "$c" 2>/dev/null)"; if [ "x$n" = "x'"$d"'" ]; then c2="${c#/proc/}"; echo "${c2%/comm}"; fi; done')" || {
    echo "DEVICE FAIL: comm scan for $d failed on the device" >&2
    return 1
  }
  out="${out%$'\n'}" # the single measured dsh trailing-newline artifact
  [ -z "$out" ] && return 0
  while IFS= read -r line; do
    if ! [[ "$line" =~ ^[0-9]{1,7}$ ]]; then
      echo "DEVICE FAIL: comm scan for $d emitted a non-pid line ('$line')" >&2
      return 1
    fi
    printf '%s\n' "$line"
  done <<< "$out"
}

# rig_daemon_stop <name> — quiesce a device daemon (M4 task 8, iter 74:
# the ATTRIBUTED skip-stall mitigation — low_bat_check's 2 s poll loop
# preempts the paced app ~7-15 ms every ~123 frames; AGENT-LOG iter 74
# verdict). EXACTLY-ONE running instance expected (boot state) — any
# other inventory is a loud refusal, never a blind kill; SIGTERM by
# pid; /proc liveness poll + comm-scan re-verify. The CALLER must
# guarantee restoration (rig_daemon_restore in both the success path
# and its cleanup trap). Echoes the killed pid.
rig_daemon_stop() {
  local d pids n pid rc gone
  d="$1"
  pids="$(rig_comm_pids "$d")" || return 1
  n="$(printf '%s' "$pids" | grep -c '')" || true
  if [ "$n" != 1 ]; then
    echo "DEVICE FAIL: expected exactly 1 running '$d' before quiesce, found $n ('$pids') — unexpected device inventory, refusing to touch it" >&2
    return 1
  fi
  pid="$pids"
  dsh "kill $pid" || {
    echo "DEVICE FAIL: kill $pid ($d) failed" >&2
    return 1
  }
  gone=0
  for _ in $(seq 1 8); do
    rc=0
    dsh "test ! -d /proc/$pid" >/dev/null 2>&1 || rc=$?
    if [ "$rc" = 0 ]; then gone=1; break; fi
    sleep 1
  done
  if [ "$gone" != 1 ]; then
    echo "DEVICE FAIL: $d (pid $pid) still alive 8 s after SIGTERM" >&2
    return 1
  fi
  pids="$(rig_comm_pids "$d")" || return 1
  if [ -n "$pids" ]; then
    echo "DEVICE FAIL: comm-scan still finds '$d' pids after the kill ('$pids')" >&2
    return 1
  fi
  printf '%s\n' "$pid"
}

# rig_restore_stamp <stamp-devpath|""> — write the COUPLED restore
# success witness (iter 80, review-78 M — the causality gap). Internal
# to rig_daemon_restore; an empty path means no stamp was requested
# (trap / rig_qd_normalize callers — no bracket evidence there). A
# failed stamp write on a restored daemon is a LOUD nonzero: the
# daemon IS up, but the run's bracket evidence is unwritable, and
# unwritable evidence is never a pass.
rig_restore_stamp() {
  [ -n "$1" ] || return 0
  dsh "date +%s > $1" >/dev/null || {
    echo "DEVICE FAIL: daemon restored but the coupled restore stamp $1 could not be written — bracket evidence incomplete" >&2
    return 1
  }
}

# rig_daemon_restore <name> <init-script> [<stamp-devpath>] — restore a
# quiesced daemon to the recorded boot cardinality: EXACTLY ONE
# instance (iter 76, review-73 M — rig_daemon_stop REFUSES any
# pre-stop inventory != 1, so ==1 is the only cardinality this rig
# ever owes; FunKey-OS reality: each init script starts exactly one
# instance at boot, measured recon iter 74). IDEMPOTENT + EXACT-COUNT:
#   1 already running -> success WITHOUT touching the START channel
#     (re-entry safe: the trap may run after a verified main-path
#     restore; busybox start-stop-daemon cannot see script daemons,
#     so every extra START would be a NEW instance — the A4'
#     tripled-daemon class);
#   0 running -> ONE init START (start-stop-daemon -S execs fine;
#     only the -K stop arm is broken for scripts), then a bounded
#     comm-scan poll for EXACTLY 1;
#   >1 at any point -> LOUD refusal, never "restored".
# COUPLED RESTORE STAMP (iter 80, review-78 M): when <stamp-devpath>
# is given, THIS helper writes `date +%s` there immediately after the
# comm-scan verifies EXACTLY ONE — the stamp IS the restore's success
# witness, never a caller-side marker. Callers MUST NOT write the
# stamp independently: an elapsed-time witness cannot prove
# first-action sequencing unless coupled to the operation, so any
# chore inserted before this call, or a stall inside this helper
# (pre-scan, init start, verify poll), inflates the bracket's
# app-end->stamp bound and dies there.
# Returns nonzero when the daemon never verifies at exactly one (or a
# requested stamp cannot be written) — the caller decides loud-fail
# (main path) vs loud-warn naming the manual recovery command (trap
# path).
rig_daemon_restore() {
  local d isc stamp pids n
  d="$1"
  isc="$2"
  stamp="${3:-}"
  pids="$(rig_comm_pids "$d")" || return 1
  n="$(printf '%s' "$pids" | grep -c '')" || true
  if [ "$n" = 1 ]; then
    # already at boot cardinality — idempotent success (comm-scan ==1)
    rig_restore_stamp "$stamp" || return 1
    return 0
  fi
  if [ "$n" != 0 ]; then
    echo "DEVICE FAIL: $d has $n instances before restore ('$pids') — want 0 or 1; refusing to start more" >&2
    return 1
  fi
  dsh "$isc start" >/dev/null || return 1
  for _ in $(seq 1 8); do
    sleep 1
    pids="$(rig_comm_pids "$d")" || continue
    n="$(printf '%s' "$pids" | grep -c '')" || true
    if [ "$n" = 1 ]; then
      rig_restore_stamp "$stamp" || return 1
      return 0
    fi
    if [ "$n" != 0 ]; then
      echo "DEVICE FAIL: $d has $n instances after the restore START ('$pids') — want exactly 1" >&2
      return 1
    fi
  done
  return 1
}

# rig_qd_normalize — stale QUIESCE-marker normalization (iter 76,
# review-73 H, cross-run face): a run that quiesced a daemon writes a
# nonce-scoped marker $DTMP/qd.<name>.<nonce> BEFORE the kill; the
# marker is cleared only on a VERIFIED restore (main path, trap, or
# the on-device deadman's restore arm). A surviving marker therefore
# means "a prior run may have left this daemon down". This chokepoint
# runs in EVERY device check's step-0 BEFORE the stale deadman is
# cancel-disarmed and BEFORE $DTMP is wiped — disarming/wiping first
# would destroy the last restore backstop while the daemon is down.
# Restores ride rig_daemon_restore (idempotent, exact-cardinality), so
# a marker whose daemon is actually fine costs nothing. Allowlist is
# CLOSED (the three FunKey shell daemons with designed START channels);
# an unrestorable daemon is a loud death naming the manual command —
# the stale deadman then STAYS armed (the caller's errexit path
# preserves $DTMP), which is the correct failure direction.
rig_qd_normalize() {
  local d isc nrc
  for d in low_bat_check system_stats fkgpiod; do
    case "$d" in
      low_bat_check) isc=/etc/init.d/S12low-bat-check ;;
      system_stats)  isc=/etc/init.d/S13system-stats ;;
      fkgpiod)       isc=/etc/init.d/S11gpio ;;
    esac
    nrc=0
    dsh "ls $DTMP/qd.$d.* >/dev/null 2>&1" >/dev/null || nrc=$?
    case "$nrc" in
      0)
        echo "WARN: stale quiesce marker for $d on the device — a prior run may have left the daemon down; restoring BEFORE any disarm/wipe" >&2
        if rig_daemon_restore "$d" "$isc"; then
          dsh "rm -f $DTMP/qd.$d.*" >/dev/null || true
          echo "   stale-quiesced $d verified running (comm-scan, exactly 1); marker cleared" >&2
        else
          echo "DEVICE FAIL: stale-quiesced $d could not be restored to exactly one instance — run '$isc start' on the device manually and inspect" >&2
          return 1
        fi
        ;;
      1|2) : ;; # no marker (ls found nothing) — the healthy path
      *)
        echo "DEVICE FAIL: could not probe for stale quiesce markers of $d (rc $nrc)" >&2
        return 1
        ;;
    esac
  done
}

# rig_dev_ts <device-path> — read a device-written `date +%s` stamp
# (iter 78, review-76 M1 — the exact-quiesce-window bracket): RC-checked
# dsh cat, the single measured dsh trailing-newline artifact stripped,
# bounded-decimal whitelist. NOTE (measured iter 78): this device's RTC
# is NOT wall-synced (epoch ~1.9e7) — stamps are only ever compared to
# OTHER device stamps (deltas), never to host time.
rig_dev_ts() {
  local out
  out="$(dsh "cat $1")" || {
    echo "DEVICE FAIL: could not read device timestamp $1" >&2
    return 1
  }
  out="${out%$'\n'}"
  if ! [[ "$out" =~ ^[0-9]{1,12}$ ]]; then
    echo "DEVICE FAIL: device timestamp $1 not a bounded decimal ('$out')" >&2
    return 1
  fi
  printf '%s\n' "$out"
}

# rig_quiesce_bracket_assert <label> <qstop> <astart> <aend> <qrestore>
#   <pre-slack-s> <post-slack-s>
# (iter 78, review-76 M1 — the EXACT-quiesce-window STANDING TOOTH):
# all four stamps are device-clock epoch seconds (rig_dev_ts); asserts
# the daemon-down window brackets ONLY the app lifetime:
#   qstop <= astart                (the stop completed before app start)
#   astart - qstop <= pre-slack    (nothing sits between the stop and
#                                   the launch but the launch dsh itself)
#   astart <= aend                 (sane lifetime ordering)
#   aend <= qrestore               (restore verified after app exit)
#   qrestore - aend <= post-slack  (the restore is the FIRST post-exit
#                                   step AND qrestore is the COUPLED
#                                   stamp rig_daemon_restore ITSELF
#                                   writes on verified success — iter
#                                   80, review-78 M: the bound measures
#                                   the REAL app-end->restore-verified
#                                   latency, incl. the helper's
#                                   comm-scans and >=1 s verify poll;
#                                   any pull/hash/cmp chore inserted
#                                   before the restore call, or a stall
#                                   inside the helper, blows this bound)
# Pure host logic (no dsh) so teeth invoke the REAL body standalone.
rig_quiesce_bracket_assert() {
  local label qstop astart aend qrestore pre post v
  label="$1"; qstop="$2"; astart="$3"; aend="$4"; qrestore="$5"
  pre="$6"; post="$7"
  for v in "$qstop" "$astart" "$aend" "$qrestore" "$pre" "$post"; do
    if ! [[ "$v" =~ ^[0-9]{1,12}$ ]]; then
      echo "DEVICE FAIL: quiesce bracket [$label]: timestamp/slack not a bounded decimal ('$v')" >&2
      return 1
    fi
  done
  if [ "$qstop" -gt "$astart" ]; then
    echo "DEVICE FAIL: quiesce bracket [$label]: daemon stop ($qstop) completed AFTER app start ($astart) — the stop is not immediately before the launch" >&2
    return 1
  fi
  if [ $((astart - qstop)) -gt "$pre" ]; then
    echo "DEVICE FAIL: quiesce bracket [$label]: $((astart - qstop)) s between daemon stop and app start (> ${pre} s slack) — non-launch work ran inside the quiesce window" >&2
    return 1
  fi
  if [ "$astart" -gt "$aend" ]; then
    echo "DEVICE FAIL: quiesce bracket [$label]: app start ($astart) after app end ($aend) — corrupt stamps" >&2
    return 1
  fi
  if [ "$aend" -gt "$qrestore" ]; then
    echo "DEVICE FAIL: quiesce bracket [$label]: restore stamp ($qrestore) precedes app exit ($aend) — a restore initiated mid-run is corrupt evidence" >&2
    return 1
  fi
  if [ $((qrestore - aend)) -gt "$post" ]; then
    echo "DEVICE FAIL: quiesce bracket [$label]: $((qrestore - aend)) s between app exit and restore start (> ${post} s slack) — post-run chores ran before the daemon restore" >&2
    return 1
  fi
  echo "   quiesce bracket OK [$label]: stop->start $((astart - qstop)) s, app $((aend - astart)) s, end->restore $((qrestore - aend)) s"
}

# rig_pin_assert_once <file> <var> <value> — twin-pin EXACTNESS (iter
# 78, review-76 M4: a presence-only `grep -q '^VAR=VALUE$'` passed a
# file where an OLD pin line remained while a LATER assignment won
# last-wins): EXACTLY ONE `^<var>=` assignment line may exist in
# <file>, and that single line must pin <value> (value terminated by
# end-of-line or whitespace, so a trailing comment is legal but a
# value prefix is not). Pure host logic — teeth invoke the REAL body.
rig_pin_assert_once() {
  local f var val cnt line
  f="$1"; var="$2"; val="$3"
  cnt="$(grep -Ec "^${var}=" "$f")" || true
  case "$cnt" in
    ''|*[!0-9]*)
      echo "DEVICE FAIL: pin-assignment count for $var in $f non-numeric ('$cnt')" >&2
      return 1
      ;;
  esac
  if [ "$cnt" != 1 ]; then
    echo "DEVICE FAIL: $f carries $cnt '^${var}=' assignment lines (want exactly 1) — a duplicate assignment can win last-wins while a stale pin line satisfies a presence grep" >&2
    return 1
  fi
  line="$(grep -E "^${var}=" "$f")"
  if ! [[ "$line" =~ ^${var}=${val}([[:space:]].*)?$ ]]; then
    echo "DEVICE FAIL: the single ${var}= line in $f does not pin ${val} ('$line') — twin-pin drift, reviewed change required at BOTH sites" >&2
    return 1
  fi
}

# rig_argv_assert_once <region-file> <opt> — duplicate-option lockout
# (iter 78, review-76 M4: a duplicate LATER option in a generated
# launcher is a last-wins override that satisfies every presence
# assert): the option must occur EXACTLY ONCE in the caller-extracted
# gfx_device argv region (word-bounded: line start or whitespace
# before, whitespace or line end after — `--frames` never matches
# `--shot-frame`). Pure host logic — teeth invoke the REAL body.
rig_argv_assert_once() {
  local f o cnt
  f="$1"; o="$2"
  cnt="$(grep -oE "(^|[[:space:]])${o}([[:space:]]|\$)" "$f" | grep -c '')" || true
  case "$cnt" in
    ''|*[!0-9]*)
      echo "DEVICE FAIL: argv occurrence count for $o non-numeric ('$cnt')" >&2
      return 1
      ;;
  esac
  if [ "$cnt" != 1 ]; then
    echo "DEVICE FAIL: option $o occurs $cnt times in the gfx_device argv region $f (want exactly 1) — a duplicate later option is a last-wins override" >&2
    return 1
  fi
}

# rig_dev_sha256 <device-path> — device-side sha256, WHITELIST-GRAMMAR
# parsed (iter 52, PROCESS §3 rule; replaces every permissive
# `| awk 'NF{print $1; exit}'` first-nonempty-line scrape). Measured
# producer grammar (busybox sha256sum on this device): EXACTLY
#   <64-lowercase-hex><SP><SP><path>
# on one line. dsh's guaranteed-leading-newline RC marker leaves exactly
# ONE trailing empty line when the command's stdout ends in \n (the
# transport artifact — measured, adbsh.sh iter 39 H1); strip exactly
# that, then require the WHOLE remaining output to reconstruct as
# `<sum>  <path>` — anything that merely resembles it (warning lines,
# multi-line output, wrong filename, short/odd digest) is corruption →
# loud death. Echoes the digest on success.
rig_dev_sha256() {
  local p out sum rest
  p="$1"
  out="$(dsh "sha256sum $p")" || {
    echo "DEVICE FAIL: device-side sha256sum $p failed" >&2
    return 1
  }
  out="${out%$'\n'}" # the single measured dsh trailing-newline artifact
  case "$out" in
    *$'\n'*)
      echo "DEVICE FAIL: device sha256sum $p emitted multiple lines (corrupt output):" >&2
      printf '%s\n' "$out" >&2
      return 1
      ;;
  esac
  sum="${out%%  *}"
  rest="${out#*  }"
  if [ "${#sum}" -ne 64 ]; then
    echo "DEVICE FAIL: device sha256sum $p digest not 64 chars ('$sum')" >&2
    return 1
  fi
  case "$sum" in
    *[!0-9a-f]*)
      echo "DEVICE FAIL: device sha256sum $p digest not lowercase hex ('$sum')" >&2
      return 1
      ;;
  esac
  if [ "$rest" != "$p" ] || [ "$out" != "$sum  $p" ]; then
    echo "DEVICE FAIL: device sha256sum $p output is not exactly '<sha>  $p' (got '$out')" >&2
    return 1
  fi
  printf '%s\n' "$sum"
}

# rig_devsha_selftest — device-side hash tool self-test (iter 39, review
# M2): every pull is digest-verified via busybox sha256sum on the device
# — prove the tool exists and is correct (empty-input vector) before
# trusting it. Iter 52: EXACT whole-output compare (whitelist grammar —
# busybox prints `<sha>  -` for stdin; the dsh trailing-newline artifact
# stripped like rig_dev_sha256).
rig_devsha_selftest() {
  local EMPTY_SHA out
  EMPTY_SHA=e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855
  out="$(dsh "printf '' | sha256sum")" || {
    echo "DEVICE FAIL: device sha256sum self-test command failed" >&2
    exit 1
  }
  out="${out%$'\n'}"
  if [ "$out" != "$EMPTY_SHA  -" ]; then
    echo "DEVICE FAIL: device sha256sum missing/broken (want '$EMPTY_SHA  -', got '$out')" >&2
    exit 1
  fi
}

# rig_host_sha256 <path> — host-side sha256, FULL-LINE whitelist
# grammar (iter 56, review-55 M — closes the last `| cut -d' ' -f1`
# first-field scrapes; PROCESS §3 rule, mirror of rig_dev_sha256).
# Producer grammar (macOS/perl shasum, text mode): EXACTLY
#   <64-lowercase-hex><SP><SP><path>
# on one line, path matched against the ACTUAL argument. A malformed or
# truncated line whose first field merely happens to be 64 hex chars is
# corruption -> loud death, never an accepted digest. Echoes the digest
# on success.
rig_host_sha256() {
  local p out sum rest
  p="$1"
  out="$(shasum -a 256 "$p")" || {
    echo "DEVICE FAIL: host shasum -a 256 $p failed" >&2
    return 1
  }
  case "$out" in
    *$'\n'*)
      echo "DEVICE FAIL: host shasum $p emitted multiple lines (corrupt output):" >&2
      printf '%s\n' "$out" >&2
      return 1
      ;;
  esac
  sum="${out%%  *}"
  rest="${out#*  }"
  if [ "${#sum}" -ne 64 ]; then
    echo "DEVICE FAIL: host shasum $p digest not 64 chars ('$sum')" >&2
    return 1
  fi
  case "$sum" in
    *[!0-9a-f]*)
      echo "DEVICE FAIL: host shasum $p digest not lowercase hex ('$sum')" >&2
      return 1
      ;;
  esac
  if [ "$rest" != "$p" ] || [ "$out" != "$sum  $p" ]; then
    echo "DEVICE FAIL: host shasum $p output is not exactly '<sha>  $p' (got '$out')" >&2
    return 1
  fi
  printf '%s\n' "$sum"
}

# pullv <device-path> <host-dest> — freshness-proven pull (iter 39,
# review M2): the host destination is removed BEFORE the pull (a stale
# file can never be judged), and the device-side sha256 of the source
# must equal the host sha256 of the pulled bytes. Non-empty assert
# (iter 42): closes the both-sides-empty cmp pass.
pullv() {
  local dsum hsum
  rm -f "$2"
  adb -s "$DEV" pull "$1" "$2" >/dev/null
  # iter 52 (whitelist grammar): the device digest comes through the
  # strict full-line rig_dev_sha256 parser, never a first-field scrape.
  dsum="$(rig_dev_sha256 "$1")" || return 1
  hsum="$(rig_host_sha256 "$2")" || return 1 # full-line grammar (iter 56)
  if [ "$dsum" != "$hsum" ]; then
    echo "DEVICE FAIL: pulled $2 != device $1 (device $dsum, host $hsum)" >&2
    return 1
  fi
  if ! [ -s "$2" ]; then
    echo "DEVICE FAIL: pulled $2 is empty (device produced no output)" >&2
    return 1
  fi
}

# made <file...> — freshness assert (iter 42, review round 4 — the
# rm-before-produce CLASS, round-2's rm-before-pull generalized to ALL
# produced artifacts): every host-side file a judge/comparison later
# consumes is `rm -f`'d immediately before its producer runs and
# asserted to exist NON-EMPTY immediately after, so a fallible producer
# exiting 0 without writing (node script, generator, no-op redirection)
# can never leave a PRIOR run's file — even a byte-identical one that
# would satisfy a content pin — to be judged. Content pins prove
# CONTENT; only rm-before-produce proves FRESHNESS.
made() {
  local f
  for f in "$@"; do
    if ! [ -s "$f" ]; then
      echo "DEVICE FAIL: artifact $f missing or empty after its producer ran (rm-before-produce freshness guard)" >&2
      exit 1
    fi
  done
}

# rig_srchash — the arm-build stamp key. Inputs (iter 39, review H2):
# sources + generated tables + THE RIG SCRIPTS' OWN BYTES (RIG_SCRIPTS —
# the build recipe/flags live in rig_arm_build's heredoc) + the docker
# image Id ($ARMIMGID, resolved by rig_arm_build before calling this).
# review M1: this runs inside $() where errexit is cleared — pipefail +
# explicit stage checks + a minimum-file-count floor, so a failed find
# can never yield a partial-but-plausible hash. Round 3 (iter 41,
# review M rounds 2-3): find runs with -L so symlinked DIRECTORIES are
# descended (the compiler globs follow them — a symlinked moves/
# subtree must be hashed) and file symlinks are hashed THROUGH the
# link. Under -L, `-type f` matches regular files plus resolvable
# file links, and `-type l` matches ONLY broken links — so the
# predicate splits: a SEPARATE broken-link scan over the same roots
# dies loudly on ANY broken link (never a silent skip), then the
# `-type f` pass builds the hash list. The list stays -print0/NUL-
# framed end to end (a newline in a filename can never split one
# record into two).
rig_srchash() {
  set -o pipefail
  local listf brokenf n h hline
  listf="$DEVB/.srclist.$$"
  brokenf="$DEVB/.srcbroken.$$"
  find -L port/sim port/gfx port/tools port/foh port/fdlibm port/ryu oracle/qjs "$FDC/csweep.c" \
    -type l -print0 > "$brokenf" || {
    echo "DEVICE FAIL: srchash: broken-link scan failed" >&2
    rm -f "$brokenf"
    return 1
  }
  if [ -s "$brokenf" ]; then
    echo "DEVICE FAIL: srchash: broken symlink(s) in the source tree:" >&2
    tr '\0' '\n' < "$brokenf" >&2
    rm -f "$brokenf"
    return 1
  fi
  rm -f "$brokenf"
  find -L port/sim port/gfx port/tools port/foh port/fdlibm port/ryu oracle/qjs "$FDC/csweep.c" \
    -type f \( -name '*.c' -o -name '*.h' \) -print0 \
    > "$listf" || {
    echo "DEVICE FAIL: srchash: find failed" >&2
    rm -f "$listf"
    return 1
  }
  # round 3 (iter 41, review L): the count pipeline gets an explicit
  # status check (pipefail is set above and $() inherits it) plus an
  # empty/non-numeric guard — errexit is cleared in here, and a bare
  # [ -lt ] on a garbage value errors and reads as "condition false".
  n="$(tr -cd '\0' < "$listf" | wc -c | tr -d ' ')" || {
    echo "DEVICE FAIL: srchash: file-count pipeline failed" >&2
    rm -f "$listf"
    return 1
  }
  case "$n" in
    ''|*[!0-9]*)
      echo "DEVICE FAIL: srchash: non-numeric source-file count ('$n')" >&2
      rm -f "$listf"
      return 1
      ;;
  esac
  # iter 54 (review-52 M3 sweep): digit bound BEFORE the arithmetic test
  if [ "${#n}" -gt 12 ]; then
    echo "DEVICE FAIL: srchash: oversized source-file count ('$n')" >&2
    rm -f "$listf"
    return 1
  fi
  if [ "$n" -lt 450 ]; then
    echo "DEVICE FAIL: srchash: only $n source files found (>= 450 expected)" >&2
    rm -f "$listf"
    return 1
  fi
  hline="$({
    LC_ALL=C sort -z < "$listf" | xargs -0 shasum -a 256 || exit 1
    # shellcheck disable=SC2086 — RIG_SCRIPTS is a fixed space-separated list
    shasum -a 256 "$TABLES/ml_tables.c" "$TABLES/ml_stages.c" \
      $RIG_SCRIPTS || exit 1
    printf 'dockerimage %s\n' "$ARMIMGID"
  } | shasum -a 256)" || {
    rm -f "$listf"
    return 1
  }
  rm -f "$listf"
  # iter 55 (review-54 M) + iter 56 (review-55 M): PRODUCE-TIME grammar
  # on the FULL output line — shasum on stdin prints exactly
  # '<64-lowercase-hex><SP><SP>-'; the digest is extracted only after
  # the whole line reconstructs against that grammar (no first-field
  # cut: a truncated line with a plausible first field is corruption).
  # An empty/short/misaligned digest dies loudly here and is never
  # stamped; rig_stamp_ok re-asserts the value grammar on every
  # cache-HIT read.
  case "$hline" in
    *$'\n'*)
      echo "DEVICE FAIL: srchash: final shasum emitted multiple lines ('$hline')" >&2
      return 1
      ;;
  esac
  h="${hline%%  *}"
  if [ "$hline" != "$h  -" ]; then
    echo "DEVICE FAIL: srchash: final shasum output is not exactly '<sha>  -' ('$hline')" >&2
    return 1
  fi
  case "$h" in
    ''|*[!0-9a-f]*)
      echo "DEVICE FAIL: srchash produced a non-lowercase-hex digest ('$h')" >&2
      return 1
      ;;
  esac
  if [ "${#h}" -ne 64 ]; then
    echo "DEVICE FAIL: srchash digest not 64 chars ('$h')" >&2
    return 1
  fi
  printf '%s\n' "$h"
}

# rig_stamp_bin_sha <bin> — the stamp's recorded sha256 for one binary,
# WHITELIST-GRAMMAR parsed (iter 52, PROCESS §3 rule). Producer grammar
# (rig_arm_build writes it below): exactly one line
#   bin<SP><name><SP><64-lowercase-hex>
# per binary. The old `awk '{print $3}'` field pick accepted duplicate
# records, extra fields, and any-shaped third field; now: EXACTLY ONE
# matching record, whole-line reconstruction, 64-lowercase-hex digest —
# anything else is a corrupt stamp → loud death (used by the push-side
# decisions; the cache-HIT path keeps its fail-direction = rebuild).
rig_stamp_bin_sha() {
  local f lines n rec
  f="$1"
  lines="$(awk -v f="$f" '$1=="bin" && $2==f' "$STAMP")"
  n="$(printf '%s' "$lines" | grep -c '' )" || true
  # iter 54 (review-52 M3 sweep): the count is guarded non-numeric +
  # <=12 digits BEFORE the arithmetic test — an oversized/garbage value
  # must die as corruption, never error the test into a false branch.
  case "$n" in
    ''|*[!0-9]*)
      echo "DEVICE FAIL: stamp record count for $f non-numeric ('$n')" >&2
      exit 1
      ;;
  esac
  if [ "${#n}" -gt 12 ]; then
    echo "DEVICE FAIL: stamp record count for $f oversized ('$n')" >&2
    exit 1
  fi
  if [ "$n" -ne 1 ]; then
    echo "DEVICE FAIL: stamp has $n 'bin $f' records (want exactly 1) — corrupt stamp" >&2
    exit 1
  fi
  rec="${lines##* }"
  if [ "$lines" != "bin $f $rec" ] || [ "${#rec}" -ne 64 ]; then
    echo "DEVICE FAIL: stamp record for $f malformed ('$lines')" >&2
    exit 1
  fi
  case "$rec" in
    *[!0-9a-f]*)
      echo "DEVICE FAIL: stamp sha256 for $f not lowercase hex ('$rec')" >&2
      exit 1
      ;;
  esac
  printf '%s\n' "$rec"
}

# rig_stamp_ok — cache-HIT re-verify: the stamp's srchash line must match
# $want AND every recorded binary must still hash to its stamped sha256
# (a stamped-but-tampered binary is never judged). Iter 54 (review-52 L,
# whitelist grammar on the WHOLE FILE — the iter-52 form validated the
# required records but ignored extra/malformed lines, so an accidentally
# appended partial record could still ride a cache HIT): one awk pass
# now requires EVERY line to match a known record form — line 1 exactly
# `srchash=<want>` AND anchored ^srchash=[0-9a-f]{64}$ (iter 55,
# review-54 M: the equality alone accepted `srchash=` under an empty
# $want — the VALUE grammar is part of the whitelist, so an
# empty/short/non-hex key can never ride a cache HIT), then EXACTLY one
# `bin <name> <64-lowercase-hex>` per
# ARMBINS member (whole-line reconstruction, membership, uniqueness),
# total line count == 1 + #bins, nothing else. ANY unrecognized, extra,
# or malformed line → return 1 (the SAFE direction here is rebuild,
# never a loud stop: a corrupt stamp is a stale cache).
rig_stamp_ok() {
  [ -f "$STAMP" ] || return 1
  local f rec cur
  awk -v want="$want" -v bins="$ARMBINS" '
    BEGIN { nb = split(bins, b, " "); for (i = 1; i <= nb; i++) need[b[i]] = 1; bad = 0 }
    bad { next }
    NR == 1 { if ($0 != "srchash=" want || $0 !~ /^srchash=[0-9a-f]{64}$/) bad = 1; next }
    {
      if (NF != 3 || $1 != "bin" || !($2 in need) || seen[$2]++ ||
          length($3) != 64 || $3 ~ /[^0-9a-f]/ || $0 != "bin " $2 " " $3) bad = 1
      next
    }
    END {
      if (bad || NR != nb + 1) exit 1
      for (k in need) if (seen[k] != 1) exit 1
    }
  ' "$STAMP" 2>/dev/null || return 1
  for f in $ARMBINS; do
    [ -f "$DEVB/$f" ] || return 1
    # grammar already validated whole-file above: exactly one record
    rec="$(awk -v f="$f" '$1=="bin" && $2==f {print $3}' "$STAMP")"
    [ "${#rec}" -eq 64 ] || return 1
    # iter 56 (review-55 M): full-line host grammar; a malformed shasum
    # line -> rebuild (this path's fail direction), corruption WARN
    # visible from the helper
    cur="$(rig_host_sha256 "$DEVB/$f")" || return 1
    if [ "$cur" != "$rec" ]; then
      echo "   cached $f sha256 != stamp record — forcing rebuild" >&2
      return 1
    fi
  done
  return 0
}

# rig_arm_build — the SHARED armv7 static cross-build (SDK gcc; stamp-
# cached; MLFK_FORCE_ARM=1 ignores the stamp). Builds all of $ARMBINS in
# ONE serial docker run; asserts ELF/ARM type, the fdlibm strong-override
# link (exactly one T definition each of floor/ceil/fmod in sim_device —
# H4 residue, iter 39), and records each binary's sha256 in the stamp.
# Sets: ARMIMGID, want, STAMP.
rig_arm_build() {
  ARMIMG=jondbell/funkey-s-sdk
  ARMIMGID="$(docker image inspect -f '{{.Id}}' "$ARMIMG" 2>/dev/null)" || {
    echo "   docker image $ARMIMG not local — pulling"
    docker pull "$ARMIMG" >/dev/null
    ARMIMGID="$(docker image inspect -f '{{.Id}}' "$ARMIMG")"
  }
  STAMP=$DEVB/arm-build.stamp
  want="$(rig_srchash)"
  local f
  if [ "${MLFK_FORCE_ARM:-0}" != 0 ] || ! rig_stamp_ok; then
    # M4 task 12: the foh_device recipe consumes $TABLES/ml_targets.c
    # (TTAB1). Callers historically generate animations/tables/stages
    # only — the recipe's own input is produced HERE (class fix, iter
    # 99): idempotent extractor build + the targets stage into the
    # SHARED sim-tables dir, made-checked before docker ever runs.
    bash pipeline/extractor/build-extractor.sh >/dev/null || {
      echo "DEVICE FAIL: rig_arm_build: extractor build failed (targets stage input)" >&2
      exit 1
    }
    node pipeline/run.js --only targets --out pipeline/build/sim-tables >/dev/null || {
      echo "DEVICE FAIL: rig_arm_build: pipeline targets stage failed" >&2
      exit 1
    }
    if ! [ -s pipeline/build/sim-tables/ml_targets.c ] || \
       ! [ -s pipeline/build/sim-tables/ml_targets.h ]; then
      echo "DEVICE FAIL: rig_arm_build: ml_targets.{c,h} missing after the targets stage" >&2
      exit 1
    fi
    rm -f "$STAMP"
    # freshness (round 4 class): remove the prior binaries before the
    # docker build — a build that exits 0 without compiling must never
    # re-stamp a PRIOR run's binaries as fresh
    for f in $ARMBINS; do rm -f "$DEVB/$f"; done
    rm -f "$DEVB/raster_arm.o" # gfx_device's -O3 intermediate (same class)
    echo "   stamp MISS (or forced) — rebuilding armv7 binaries"
    # SERIAL docker only (CLAUDE.md §Commands arm32 recipe). Run by the
    # RESOLVED image Id — the same Id the stamp records — never the
    # mutable tag (iter 40, review M2 round 2: closes the inspect-to-run
    # tag-drift window).
    docker run --rm -v "$PWD":/work -w /work "$ARMIMGID" bash -lc '
      set -e
      export PATH=/opt/FunKey-sdk-2.3.0/bin:$PATH
      CC=arm-funkey-linux-musleabihf-gcc
      DEVB=port/sim/calib/build/device
      TABLES=pipeline/build/sim-tables
      CAL=port/sim/calib
      SIM=port/sim/sim
      # keep this TU list in sync with port/sim/check-sim.sh (the M2 gate)
      $CC -O2 -ffp-contract=off -Wall -Wextra -Werror -static \
        -I"$TABLES" -Iport/ryu -Iport/sim -Ioracle/qjs \
        -o "$DEVB/sim_device" \
        "$SIM/sim_main.c" "$SIM/sim_boot.c" "$SIM/sim_tick.c" \
        "$SIM/sim_ser.c" "$SIM/sim_data.c" \
        "$CAL/canon.c" "$CAL/player_canon.c" \
        port/sim/physics.c port/sim/interpolated_collision.c \
        port/sim/environmental_collision.c port/sim/hit_detection.c \
        port/sim/article.c port/sim/action_state_shortcuts.c \
        port/sim/ml_events.c port/sim/ml_fmt.c port/sim/ml_ser.c \
        port/sim/ai_bridge.c port/sim/input/interpret_inputs.c \
        port/sim/stages/moving_platforms.c port/sim/stages/ystory.c \
        port/sim/stages/fountain.c \
        port/sim/characters/shared/moves_index.c \
        port/sim/characters/shared/moves/*.c \
        port/sim/characters/fox/moves_index.c \
        port/sim/characters/fox/moves/*.c \
        port/sim/characters/falco/moves_index.c \
        port/sim/characters/falco/moves/*.c \
        port/sim/characters/falcon/moves_index.c \
        port/sim/characters/falcon/moves/*.c \
        port/sim/characters/marth/moves_index.c \
        port/sim/characters/marth/dancing_blade_combo.c \
        port/sim/characters/marth/dancing_blade_air_mobility.c \
        port/sim/characters/marth/moves/*.c \
        port/sim/characters/puff/moves_index.c \
        port/sim/characters/puff/puff_multi_jump_drift.c \
        port/sim/characters/puff/puff_next_jump.c \
        port/sim/characters/puff/moves/*.c \
        "$TABLES/ml_tables.c" "$TABLES/ml_stages.c" \
        oracle/qjs/sha256.c \
        port/fdlibm/fdlibm.c -lm
      $CC -O2 -ffp-contract=off -std=c99 -Wall -static -Iport/fdlibm \
        oracle/fdlibm-crosscheck/csweep.c port/fdlibm/fdlibm.c \
        -o "$DEVB/csweep_arm"
      # fdlibm.c linked (exactly like sim_device): floor/ceil overrides live
      $CC -O2 -ffp-contract=off -Wall -Wextra -Werror -static -Iport/sim \
        port/sim/device/mathsweep.c port/fdlibm/fdlibm.c \
        -o "$DEVB/mathsweep_arm" -lm
      $CC -O2 -ffp-contract=off -Wall -Wextra -Werror -static \
        -Iport/ryu -Iport/sim -Ioracle/qjs \
        -o "$DEVB/fmt_diff_arm" \
        "$CAL/fmt_diff.c" "$CAL/canon.c" port/sim/ml_ser.c port/sim/ml_fmt.c \
        oracle/qjs/sha256.c -lm
      # fk_input (M3 task 5): static uinput button injector — no SDL,
      # no sim, no libm; kernel headers from the SDK sysroot.
      $CC -O2 -ffp-contract=off -Wall -Wextra -Werror -static \
        port/tools/fk_input.c -o "$DEVB/fk_input"
      # sk_sampler (M4 task 8): static fork-free /proc counter sampler
      # for the skip-stall attribution instrument — no SDL, no sim.
      $CC -O2 -ffp-contract=off -Wall -Wextra -Werror -static \
        port/sim/device/skip-attrib/sk_sampler.c -o "$DEVB/sk_sampler"
      # gfx_device (M3 task 4; M4 task 3 adds the vfx render plane TUs
      # gfx_vfx/gfx_overlay/gfx_bg — keep in sync with check-render.sh +
      # check-device-render.sh host lists): the SDL1.2 live-render app.
      # DYNAMIC link
      # (SDL 1.2 is LGPL — dynamic only; asserted after the build), -no-pie
      # for a deterministic file(1) signature + addr2line-able crashes.
      # raster.c is the ONE -O3 TU (PLAN 5); every TU -ffp-contract=off.
      SDLCFG=/opt/FunKey-sdk-2.3.0/arm-funkey-linux-musleabihf/sysroot/usr/bin/sdl-config
      GFX=port/gfx
      $CC -O3 -ffp-contract=off -Wall -Wextra -Werror \
        -I"$TABLES" -Iport/ryu -Iport/sim -Ioracle/qjs \
        -c "$GFX/raster.c" -o "$DEVB/raster_arm.o"
      $CC -O2 -ffp-contract=off -Wall -Wextra -Werror -no-pie \
        -I"$TABLES" -Iport/ryu -Iport/sim -Ioracle/qjs \
        $($SDLCFG --cflags) \
        -o "$DEVB/gfx_device" \
        "$DEVB/raster_arm.o" \
        "$GFX/gfx_app.c" "$GFX/platform_sdl1.c" \
        "$GFX/anim1.c" "$GFX/gfx_render.c" \
        "$GFX/gfx_vfx.c" "$GFX/gfx_overlay.c" "$GFX/gfx_bg.c" \
        "$SIM/sim_boot.c" "$SIM/sim_tick.c" "$SIM/sim_ser.c" \
        "$SIM/sim_data.c" \
        "$CAL/canon.c" "$CAL/player_canon.c" \
        port/sim/physics.c port/sim/interpolated_collision.c \
        port/sim/environmental_collision.c port/sim/hit_detection.c \
        port/sim/article.c port/sim/action_state_shortcuts.c \
        port/sim/ml_events.c port/sim/ml_fmt.c port/sim/ml_ser.c \
        port/sim/ai_bridge.c port/sim/input/interpret_inputs.c \
        port/sim/stages/moving_platforms.c port/sim/stages/ystory.c \
        port/sim/stages/fountain.c \
        port/sim/characters/shared/moves_index.c \
        port/sim/characters/shared/moves/*.c \
        port/sim/characters/fox/moves_index.c \
        port/sim/characters/fox/moves/*.c \
        port/sim/characters/falco/moves_index.c \
        port/sim/characters/falco/moves/*.c \
        port/sim/characters/falcon/moves_index.c \
        port/sim/characters/falcon/moves/*.c \
        port/sim/characters/marth/moves_index.c \
        port/sim/characters/marth/dancing_blade_combo.c \
        port/sim/characters/marth/dancing_blade_air_mobility.c \
        port/sim/characters/marth/moves/*.c \
        port/sim/characters/puff/moves_index.c \
        port/sim/characters/puff/puff_multi_jump_drift.c \
        port/sim/characters/puff/puff_next_jump.c \
        port/sim/characters/puff/moves/*.c \
        "$TABLES/ml_tables.c" "$TABLES/ml_stages.c" \
        oracle/qjs/sha256.c port/fdlibm/fdlibm.c \
        $($SDLCFG --libs) -lm
      # foh_device (M4 task 10): the SDL1.2 FOH app — the same sim +
      # render TU set as gfx_device with the FOH machine TUs and
      # foh_dev.c as the driver (gfx_app.c stays byte-untouched).
      # DYNAMIC SDL link (LGPL — asserted below like gfx_device).
      $CC -O2 -ffp-contract=off -Wall -Wextra -Werror -no-pie \
        -I"$TABLES" -Iport/ryu -Iport/sim -Ioracle/qjs \
        $($SDLCFG --cflags) \
        -o "$DEVB/foh_device" \
        "$DEVB/raster_arm.o" \
        port/foh/foh_dev.c port/foh/foh.c port/foh/foh_font.c \
        port/foh/foh_render.c port/foh/foh_persist.c \
        port/sim/target/target_play.c \
        "$GFX/platform_sdl1.c" \
        "$GFX/anim1.c" "$GFX/gfx_render.c" "$GFX/gfx_target.c" \
        "$GFX/gfx_vfx.c" "$GFX/gfx_overlay.c" "$GFX/gfx_bg.c" \
        "$SIM/sim_boot.c" "$SIM/sim_tick.c" "$SIM/sim_ser.c" \
        "$SIM/sim_data.c" "$SIM/sim_ai_live.c" \
        "$CAL/canon.c" "$CAL/player_canon.c" \
        port/sim/ai.c \
        port/sim/physics.c port/sim/interpolated_collision.c \
        port/sim/environmental_collision.c port/sim/hit_detection.c \
        port/sim/article.c port/sim/action_state_shortcuts.c \
        port/sim/ml_events.c port/sim/ml_fmt.c port/sim/ml_ser.c \
        port/sim/ai_bridge.c port/sim/input/interpret_inputs.c \
        port/sim/stages/moving_platforms.c port/sim/stages/ystory.c \
        port/sim/stages/fountain.c \
        port/sim/characters/shared/moves_index.c \
        port/sim/characters/shared/moves/*.c \
        port/sim/characters/fox/moves_index.c \
        port/sim/characters/fox/moves/*.c \
        port/sim/characters/falco/moves_index.c \
        port/sim/characters/falco/moves/*.c \
        port/sim/characters/falcon/moves_index.c \
        port/sim/characters/falcon/moves/*.c \
        port/sim/characters/marth/moves_index.c \
        port/sim/characters/marth/dancing_blade_combo.c \
        port/sim/characters/marth/dancing_blade_air_mobility.c \
        port/sim/characters/marth/moves/*.c \
        port/sim/characters/puff/moves_index.c \
        port/sim/characters/puff/puff_multi_jump_drift.c \
        port/sim/characters/puff/puff_next_jump.c \
        port/sim/characters/puff/moves/*.c \
        "$TABLES/ml_tables.c" "$TABLES/ml_stages.c" "$TABLES/ml_targets.c" \
        oracle/qjs/sha256.c port/fdlibm/fdlibm.c \
        $($SDLCFG --libs) -lm -lpthread
    '
    for f in $ARMBINS; do
      made "$DEVB/$f"
      file "$DEVB/$f" | grep -q "ELF 32-bit LSB executable, ARM" || {
        echo "DEVICE FAIL: $f is not an armv7 static executable" >&2
        exit 1
      }
    done
    # H4 residue (iter 39): the fdlibm strong overrides must BE the linked
    # definitions in the device sim — exactly one T symbol each. Round 2
    # (iter 40, review L1): nm's exit status is checked explicitly (its
    # output goes to a file first — no || true, no grep in the pipeline;
    # a truncated symbol table can never pass on a lucky prefix).
    # M3 task 4: gfx_device runs the SAME sim, so the same overrides must
    # be its linked definitions too (dynamic libm.so is only consulted
    # for symbols the binary does not define — these it defines).
    local nmout s cnt b
    for b in sim_device gfx_device foh_device; do
      nmout="$DEVB/.nm-$b.$$"
      if ! nm "$DEVB/$b" > "$nmout"; then
        rm -f "$nmout"
        echo "DEVICE FAIL: nm failed on $b — cannot verify fdlibm overrides" >&2
        exit 1
      fi
      for s in floor ceil fmod round lround; do
        cnt="$(awk -v s="$s" '$2=="T" && $3==s {n++} END {print n+0}' "$nmout")"
        if [ "$cnt" != 1 ]; then
          echo "DEVICE FAIL: expected exactly 1 T definition of $s in $b, found $cnt" >&2
          exit 1
        fi
      done
      rm -f "$nmout"
    done
    # LGPL compliance + launchability (M3 task 4): gfx_device must be
    # DYNAMICALLY linked against the shared libSDL-1.2 — file(1) says
    # "dynamically linked" AND the DT_NEEDED soname string is present in
    # the binary. A silent static libSDL.a link fails BOTH (no soname
    # string, "statically linked"), loudly.
    if ! file "$DEVB/gfx_device" | grep -q "dynamically linked"; then
      echo "DEVICE FAIL: gfx_device is not dynamically linked (SDL 1.2 is LGPL — dynamic only)" >&2
      exit 1
    fi
    if ! grep -q "libSDL-1.2.so.0" "$DEVB/gfx_device"; then
      echo "DEVICE FAIL: gfx_device carries no libSDL-1.2.so.0 NEEDED entry — SDL not dynamically linked" >&2
      exit 1
    fi
    # M4 task 10: foh_device under the same LGPL dynamic-link asserts
    if ! file "$DEVB/foh_device" | grep -q "dynamically linked"; then
      echo "DEVICE FAIL: foh_device is not dynamically linked (SDL 1.2 is LGPL — dynamic only)" >&2
      exit 1
    fi
    if ! grep -q "libSDL-1.2.so.0" "$DEVB/foh_device"; then
      echo "DEVICE FAIL: foh_device carries no libSDL-1.2.so.0 NEEDED entry — SDL not dynamically linked" >&2
      exit 1
    fi
    # iter 56 (review-55 M): full-line host grammar at the stamp WRITE —
    # a malformed shasum line must die loudly BEFORE it is recorded,
    # never ride into the stamp as a plausible first field.
    local bsum
    printf 'srchash=%s\n' "$want" > "$STAMP"
    for f in $ARMBINS; do
      bsum="$(rig_host_sha256 "$DEVB/$f")" || exit 1
      printf 'bin %s %s\n' "$f" "$bsum" >> "$STAMP"
    done
    echo "   arm binaries rebuilt (stamp $want)"
  else
    echo "   arm binaries up to date (stamp $want; cached binaries sha-verified)"
  fi
}

# rig_stamp_rehash <bin...> — rehash ADJACENT to the push (iter 40,
# review M2 round 2): every binary is re-verified against the stamp's
# recorded sha256 immediately before it leaves the host — nothing can
# mutate between the stamp check in rig_arm_build and the push.
rig_stamp_rehash() {
  local f rec cur
  for f in "$@"; do
    # iter 52 (whitelist grammar): strict exactly-one-record stamp parse
    rec="$(rig_stamp_bin_sha "$f")"
    cur="$(rig_host_sha256 "$DEVB/$f")" || exit 1 # full-line grammar (iter 56)
    if [ "$cur" != "$rec" ]; then
      echo "DEVICE FAIL: $f changed between stamp verification and push ($cur != $rec)" >&2
      exit 1
    fi
  done
}

# rig_push_provenance <device-dir> <bin...> — PUSH PROVENANCE (iter 41,
# review M round 3): the pre-push rehash proves what the HOST held; this
# proves what the DEVICE received — device-side sha256 (nonce dsh) of
# every pushed binary must equal the stamp's record BEFORE anything
# runs. This is the only observable edge of the concurrent-mutator
# TOCTOU class (dispositioned iter 40): whatever happened host-side, the
# bytes the device executes are now bound to the stamp that was
# sha-verified against the sources.
rig_push_provenance() {
  local ddir f rec dsum
  ddir="$1"; shift
  for f in "$@"; do
    # iter 52 (whitelist grammar): both sides of the compare now come
    # through strict full-line parsers — the stamp record via
    # rig_stamp_bin_sha (exactly one record, reconstruction-checked)
    # and the device digest via rig_dev_sha256 (one dsh per file
    # replaces the old batched output scraped by first-field awk).
    rec="$(rig_stamp_bin_sha "$f")"
    dsum="$(rig_dev_sha256 "$ddir/$f")" || exit 1
    if [ "$dsum" != "$rec" ]; then
      echo "DEVICE FAIL: device-side $f sha256 != stamp record (device $dsum, stamp $rec) — refusing to run it" >&2
      exit 1
    fi
  done
  echo "   push provenance: all $# device-side binaries match the stamp"
}

# rig_no_commit_guard <path...> — build output is never tracked (iter 39,
# review L1: a git ERROR must be loud, never read as clean output).
# Round 2 (iter 40, review L2): --untracked-files=all overrides any
# status.showUntrackedFiles=no config, and `git ls-files` proves
# untracked-ness DIRECTLY — already-tracked-but-clean build output can
# no longer read as clean porcelain.
rig_no_commit_guard() {
  local gitout lsout
  gitout="$(git status --porcelain --untracked-files=all -- "$@")" || {
    echo "DEVICE FAIL: git status failed — cannot prove build output is untracked" >&2
    exit 1
  }
  if [ -n "$gitout" ]; then
    echo "DEVICE FAIL: build output not gitignored:" >&2
    printf '%s\n' "$gitout" >&2
    exit 1
  fi
  lsout="$(git ls-files -- "$@")" || {
    echo "DEVICE FAIL: git ls-files failed — cannot prove build output is untracked" >&2
    exit 1
  }
  if [ -n "$lsout" ]; then
    echo "DEVICE FAIL: build output is TRACKED by git:" >&2
    printf '%s\n' "$lsout" >&2
    exit 1
  fi
}
