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
port/sim/device/check-device-fullgame.sh \
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
  # review-106 round-6: BYTE semantics for every pattern below. The producer's
  # bounds are BYTE bounds (comm is 15 BYTES in the kernel), while bash's
  # regex engine counts CHARACTERS in the inherited locale — under a UTF-8
  # locale a 30-byte, 15-character comm would have matched a {1,15} bound.
  # `local LC_ALL=C` switches the match to bytes and is restored on return.
  local LC_ALL=C
  # M-3 (review-104): the MLFKPROC row grammar, anchored full-line
  # (whitelist-grammar rule). mlfk_scan emits `MLFKPROC <pid> <comm>
  # <cmdline...>`: pid a positive decimal (no leading zero), comm one
  # non-empty non-space token of at most 15 bytes over an OPEN alphabet (the
  # `?` read-fallback is just one such token — see the measurement note
  # below), then a NON-EMPTY cmdline field. A resembling row
  # (missing cmdline, pid 0, over-long comm, out-of-range pid) is CORRUPTION DEATH, not a
  # silently-counted find. review-106 round-5: BOTH open quantifiers are now
  # bounded by the producer MEASURED ON THE DEVICE 2026-07-25
  # (.loop/m4-per107-measure-pidcomm.log), not by assumption:
  #   * /proc/sys/kernel/pid_max = 32768 -> allocated pids are < pid_max,
  #     so the largest genuine pid is 32767 (checked NUMERICALLY below; a
  #     digit-width class alone would accept 99999).
  #   * comm is <= 15 bytes (TASK_COMM_LEN-1; the longest live comm measured
  #     is exactly 15, "irq/42-axp20x_i") over an OPEN alphabet — the live
  #     device carries `/`, `:` and UPPERCASE comms (kworker/0:1H,
  #     jbd2/mmcblk0p2-), and an MLFKPROC row is emitted for ANY process
  #     whose cmdline contains a rig root (every pushed ARMBINS binary, or
  #     anything a human ran against the rig dirs). The round-4 attempt at
  #     an [a-z0-9_.-] class was therefore a FALSE-REJECTION bug: it would
  #     have killed the scan on a genuine `/tmp/mlfk/MyTool`. The measured
  #     grammar of the field is "one non-empty, non-space token <= 15 bytes"
  #     — structure pinned exactly, alphabet left where the producer leaves
  #     it.
  # HONEST LIMIT of that field split (review-106 round-6, dispositioned in
  # writing rather than fixed): the row is space-delimited, so a comm that
  # ITSELF contains a space is not detectable from the host — the match
  # simply frames the first word as comm and the rest as cmdline (`My Tool`
  # parses as comm `My`, cmdline `Tool ...`) and an all-space comm is
  # rejected. Making this exact would need a producer change (escaping comm
  # on the wire), i.e. a device-behaviour change, and it would buy nothing:
  # the comm/cmdline SPLIT is not decision-bearing. Kills use the
  # DEVICE-side pid list, a dropped or added row is caught by the
  # nproc==nfound reconciliation, and both fields are only echoed into the
  # WARN log. No comm on this device contains a space (measured, 41/41).
  # Structure stays strict where decisions live: canonical pid, a NON-EMPTY
  # cmdline field, exactly one trailing MLFKSCAN, rows == reaped count.
  local proc_re='^MLFKPROC ([1-9][0-9]{0,4}) ([^ ]{1,15}) .+$'
  local pid_max_last=32767   # MEASURED (pid_max 32768, exclusive)
  out="${out%$'\n'}"
  last="${out##*$'\n'}"
  nscan=0; nproc=0; nunread=0
  while IFS= read -r line; do
    case "$line" in
      MLFKSCAN\ *)       nscan=$((nscan + 1)) ;;
      MLFKPROC\ *)
        [[ "$line" =~ $proc_re ]] \
          || { printf 'FAIL malformed MLFKPROC row (not `MLFKPROC <pid> <comm> <cmdline>`): %s' "$line"; return 1; }
        [ "${BASH_REMATCH[1]}" -le "$pid_max_last" ] \
          || { printf 'FAIL MLFKPROC pid %s exceeds the MEASURED device pid_max-1 (%s) - corruption (remeasure /proc/sys/kernel/pid_max if the kernel changed): %s' "${BASH_REMATCH[1]}" "$pid_max_last" "$line"; return 1; }
        nproc=$((nproc + 1)) ;;
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
  # The device scan block below is kept COMPACT — every byte rides the adb
  # shell command line, which has a hard length limit (an over-long block
  # aborts with "shell command too long"). Design (rationale kept HOST-side,
  # off the wire):
  #  * cat reads cmdline with its OWN stderr -> /dev/null; a bare
  #    `< $p/cmdline` redirect would leak an un-suppressible SHELL open error
  #    into the captured grammar (so cat, never a redirect).
  #  * M-2 (review-104 + review-106 M-A): capture the rc of EVERY reader in
  #    the chain EXPLICITLY — cat (crc), wc (crc=91 / non-numeric count 92)
  #    and tr (crc=93) — never behind a `| tr` pipe, whose $? is trs (the
  #    round-2 hole) and never coerced to a benign 0/empty (the round-4
  #    hole: a wc/tr failure on a GENUINE rig cmdline used to read as an
  #    empty kernel-thread cmdline and get skipped). Any reader failure on a
  #    still-present /proc entry is UNREADABLE = fail closed. A read that
  #    ERRORED (crc!=0) while /proc/<pid> is present is LIVE UNREADABLE (the
  #    -r stat missed it) -> fail closed; a readable-EMPTY cmdline while
  #    present is a kernel thread/zombie (benign — a rig process ALWAYS
  #    carries argv; MEASURED 28/41 kthreads empty+readable) UNLESS its cwd
  #    resolves under a rig dir (then it is OURS, unidentifiable -> fail
  #    closed); dir gone on re-check is VANISHED (benign, skip).
  #  * cwd_rig() matches the rig scratch roots INCL. their ` (deleted)` forms
  #    (L-a + L-2). SPLIT LITERALS ("/tmp/m""lfk") keep the scanning shells
  #    OWN cmdline from ever matching itself; NEVER write a contiguous rig
  #    path anywhere in the block (a comment would count — it is on the wire
  #    and in the shells cmdline). No apostrophes inside the single-quoted
  #    block. tf is a NON-rig-matched scratch path.
  out="$(dsh '
cwd_rig() {
  case "$1" in
    "/tmp/m""lfk"|"/tmp/m""lfk/"*|"/tmp/m""lfk (deleted)"|"/tmp/m""lfk/"*" (deleted)"|"/mnt/m""lfk-scratch"|"/mnt/m""lfk-scratch/"*|"/mnt/m""lfk-scratch (deleted)"|"/mnt/m""lfk-scratch/"*" (deleted)") return 0 ;;
  esac
  return 1
}
mlfk_scan() {
  for p in /proc/[0-9]*; do
    pid="${p#/proc/}"
    [ "$pid" = "$$" ] && continue
    [ "$pid" = "$PPID" ] && continue
    tf="/tmp/rigscancl.$$"
    cat "$p/cmdline" 2>/dev/null > "$tf"; crc=$?
    nb="$(wc -c < "$tf" 2>/dev/null)" || crc=91
    nb="${nb##* }"
    case "$nb" in ""|*[!0-9]*) crc=92 ;; esac
    if [ "$crc" != 0 ] || [ "$nb" = 0 ]; then
      rm -f "$tf"
      [ -d "$p" ] || continue
      if [ "$crc" != 0 ]; then echo "MLFKUNREADABLE $pid"; continue; fi
      cwd_rig "$(readlink "$p/cwd" 2>/dev/null)" && echo "MLFKUNREADABLE $pid"
      continue
    fi
    cl="$(tr "\0" " " < "$tf")" || crc=93
    rm -f "$tf"
    if [ "$crc" != 0 ]; then
      [ -d "$p" ] || continue
      echo "MLFKUNREADABLE $pid"
      continue
    fi
    cm=""
    matched=0
    case "$cl" in
      *"/tmp/m""lfk"*|*"/mnt/m""lfk-scratch"*) matched=1 ;;
    esac
    if [ "$matched" = 0 ]; then
      cm="$(cat "$p/comm" 2>/dev/null)"
      case "$cm" in
        sh|ash|busybox|deadman.sh|foh_device)
          cwd_rig "$(readlink "$p/cwd" 2>/dev/null)" && matched=1 ;;
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
      # lock release: the EXIT trap is the sole releaser (review-109-5 H1)
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
      # lock release: the EXIT trap is the sole releaser (review-109-5 H1)
      exit 1
    fi
    echo "WARN: cleanup mlfk orphan reap output failed validation (stale processes may remain): ${presult#FAIL }" >&2
    printf '%s\n' "$out" >&2
    return 1
  fi
  [[ "$presult" =~ ^OK\ ([0-9]+)\ ([0-9]+)$ ]] || {
    echo "DEVICE FAIL: rig_orphan_parse returned an unparseable OK line ('$presult')" >&2
    # lock release: the EXIT trap is the sole releaser (review-109-5 H1)
    [ "$mode" = step0 ] && exit 1
    return 1
  }
  nfound="${BASH_REMATCH[1]}"
  nleft="${BASH_REMATCH[2]}"
  if [ "$nleft" != 0 ]; then
    if [ "$mode" = step0 ]; then
      echo "DEVICE FAIL: $nleft stale mlfk process(es) SURVIVED the step-0 reap (found $nfound) — inspect the device before running any check" >&2
      # lock release: the EXIT trap is the sole releaser (review-109-5 H1)
      exit 1
    fi
    echo "WARN: $nleft stale mlfk process(es) survived the cleanup reap (found $nfound)" >&2
    return 1
  fi
  if [ "$nfound" != 0 ] && [ "$mode" = step0 ]; then
    # POST-KILL RE-VERIFY (review-109-4 H1). The inherited state was
    # already restored and VERIFIED by rig_inherited_restore BEFORE this
    # reap ran (see rig_lock_acquire) — the duty transfer is transactional
    # in that direction. Re-running it here is idempotent and closes the
    # remaining window: a stale deadman killed PART-WAY through its own
    # restore arm (e.g. after re-parking nothing but before its daemon
    # start verified) leaves state this second pass picks up. A failure
    # here is still a loud death, and by then the marker/park evidence is
    # intact for the next run's step 0.
    rig_inherited_restore
    echo "   step-0 reap: $nfound stale mlfk process(es) cleaned; device comb-free" >&2
  fi
  return 0
}

# rig_inherited_restore — INHERITED-STATE RESTORATION, run BEFORE the
# step-0 orphan reap (review-109-4 H1: the recurring "inherited-state
# startup" objection, rounds 2/3/4).
#
# THE DEFECT IT CLOSES: the reap kills every stale rig process INCLUDING
# the prior run's recovery deadman — the device's ONLY remaining backstop
# for an abandoned run — and the restoration used to run AFTERWARDS. That
# is a non-transactional duty transfer: any failure between the kill and
# the restore (a dropped ADB transport, a daemon that will not start, this
# process itself dying before the caller installs its own cleanup trap)
# left the device parked with low_bat_check down and NOTHING armed to
# recover it.
#
# THE ORDER IS THE FIX: restore-and-verify the inherited state FIRST, and
# only then reap the now-redundant deadman. Every failure path below dies
# with the stale deadman still ARMED and its nonce untouched ($DTMP is
# wiped by nobody on this path), which is the correct failure direction.
# Both operations are idempotent (rig_daemon_restore is exact-cardinality
# and never touches the START channel when the daemon is already up; the
# park-marker arm is a probe + rm + absence re-probe), so the healthy case
# costs four no-op probes and changes nothing.
rig_inherited_restore() {
  local mrc=0 qrc=0
  # ATOMIC OWNERSHIP (review-109-5 H2): hold the device recovery claim for
  # the whole daemon-plane section, so a still-live stale deadman cannot be
  # driving the same non-idempotent START channel at the same moment.
  rig_qd_claim || exit 1
  # TRANSACTIONAL HANDOFF (review-109-7 H2 — the ordering settled after
  # rounds 4/6 pulled it in opposite directions):
  #   round-4 H1 said "do not kill the inherited deadman before restoring"
  #     (killing first leaves no backstop if the restore then fails);
  #   round-6 H3 said "a claimless deadman can race the restore"
  #     (both actors take the non-idempotent START channel).
  # BOTH are satisfied by keeping the OLD watchdog armed until the NEW
  # owner has finished and VERIFIED the restore, and only then retiring
  # it. So the order here is: claim -> restore + verify (inherited deadman
  # still armed the whole time) -> unpark -> quiesce the inherited deadman
  # -> release the claim. There is never an instant in which this run has
  # taken responsibility while nothing on the device could recover it.
  #
  # The residual race (a CLAIMLESS deadman from an older producer firing
  # during our restore) is bounded and LOUD, not silent: two instances
  # make rig_daemon_restore refuse with an exact-cardinality death, the
  # marker is retained, and the inherited deadman is still armed. That is
  # strictly better than the alternative it replaces, in which a host
  # death right after the cancel left the device with no restorer at all.
  # Centralising every producer's deadman body remains the durable fix and
  # is registered as a driver-owned item.
  rig_qd_normalize || qrc=$?
  if [ "$qrc" != 0 ]; then
    # release the claim so the STILL-ARMED inherited deadman can act on it
    # (holding it would gate the very backstop we are relying on)
    rig_qd_unclaim
    echo "DEVICE FAIL: an inherited marker-quiesced daemon could not be restored — NOTHING has been retired yet, so the inherited deadman is still ARMED as the backstop; inspect the device" >&2
    # lock release: the EXIT trap is the sole releaser (review-109-5 H1)
    exit 1
  fi
  dsh "test -f /mnt/disable_frontend" >/dev/null 2>&1 || mrc=$?
  case "$mrc" in
    0)
      if ! dsh "rm -f /mnt/disable_frontend" >/dev/null 2>&1 ||
         ! dsh "test ! -f /mnt/disable_frontend" >/dev/null 2>&1; then
        rig_qd_unclaim
        echo "DEVICE FAIL: inherited /mnt/disable_frontend could not be removed and verified absent — the inherited deadman is left ARMED (unretired) so the device can still recover; inspect it" >&2
        # lock release: the EXIT trap is the sole releaser (review-109-5 H1)
        exit 1
      fi
      echo "   step-0: inherited /mnt/disable_frontend removed (RC-verified gone) BEFORE any stale-deadman reap" >&2
      ;;
    1) : ;;
    *)
      rig_qd_unclaim
      echo "DEVICE FAIL: could not probe the frontend park marker (rc $mrc) — refusing to retire the inherited recovery deadman on unknown state" >&2
      # lock release: the EXIT trap is the sole releaser (review-109-5 H1)
      exit 1
      ;;
  esac
  # D44 / TICKET-1: the park marker can no longer outlive a power cut.
  # Installed HERE — after the inherited marker is gone and BEFORE this run
  # (or any consumer sourcing riglib) is allowed to park anything.
  if ! rig_boot_unpark_install; then
    rig_qd_unclaim
    echo "DEVICE FAIL: the boot-unpark line could not be installed/verified in /etc/rc.local — REFUSING to continue, because parking the frontend without it can strand the device across a power cut (D44)" >&2
    # lock release: the EXIT trap is the sole releaser (review-109-5 H1)
    exit 1
  fi
  # THE HANDOFF COMPLETES HERE, and not one statement earlier: the
  # inherited state is now restored AND verified, so the old watchdog has
  # nothing left to recover and is retired through the universal cancel
  # channel (every generation of the deadman script in this repo polls
  # $DTMP/deadman.cancel). Retiring it is what lets THIS run arm its own.
  if ! rig_deadman_quiesce; then
    rig_qd_unclaim
    echo "DEVICE FAIL: the inherited deadman could not be retired after a verified restore — refusing to continue with a foreign watchdog still armed (it could fire mid-run); inspect the device" >&2
    # lock release: the EXIT trap is the sole releaser (review-109-5 H1)
    exit 1
  fi
  rig_qd_unclaim
}

# rig_boot_unpark_install — D44, THE CRASH-SAFETY INSTRUMENT (owner incident
# 2026-08-24: a run parked the frontend, the device was powered off mid-test,
# and it rebooted to a splash screen with no frontend. It looked bricked.)
#
# THE CLASS. `/mnt/disable_frontend` is the ONLY reversible way to stop
# `frontend init` from respawning gmenu2x under a test binary, and it lives on
# the SD card, so it survives a power cut. Every producer in this tree parks
# with it and unparks afterwards; the deadman covers a host death; NOTHING
# covered a power cut. That is the whole class, and it is a persistence
# property of the marker, not a mistake in any one script.
#
# WHY NOT THE OBVIOUS FIXES — all three were MEASURED on device and all three
# are unavailable, which is why this one looks indirect:
#   * "remove the marker before the test binary launches" — the literal rule
#     cannot be implemented: `/usr/local/sbin/frontend:74-122` consults the
#     marker at the TOP of its relaunch loop and sleeps only 5 s, so a marker
#     removed before launch puts gmenu2x back on the framebuffer within 5 s,
#     fighting the test binary for the display and the input devices.
#   * a tmpfs-backed marker (symlink /mnt/disable_frontend -> /tmp/...) —
#     `/mnt` is `vfat` (measured: mount(8) on device), which has no symlinks.
#   * `/run/rebooting`, the loop's OTHER park condition and genuinely tmpfs —
#     `frontend:120-122` BREAKS out of the loop on it, so it is a one-way
#     park: removing it does not bring the frontend back. Unusable.
#
# SO THE FIX IS AT THE OTHER END: make BOOT clear the marker. One idempotent
# line appended after the shebang of `/etc/rc.local`, which `S03rclocal` runs
# from `rcS` — i.e. before `frontend init` is ever started by the login shell.
# After this, no marker on the SD card can survive a boot, whoever wrote it
# and whenever they died. The class is IMPOSSIBLE rather than unlikely,
# because it no longer depends on any script reaching its cleanup: the worst
# case a power cut can produce is "power it back on", which is what the owner
# does anyway.
#
# IT DOES NOT OVERRIDE AN OWNER SETTING. `frontend set none` writes BOTH the
# marker and `$HOME/.frontend=none` (`frontend:38-46`), and the loop reads
# `.frontend` independently (`get_frontend`), so a deliberate "no frontend"
# choice still holds with the marker cleared. The marker ALONE is only ever a
# rig park.
#
# AND WHY THE RIG STILL TOUCHES THE MARKER DIRECTLY RATHER THAN CALLING THE
# OFFICIAL `frontend set none` / `frontend set gmenu2x` VERBS — three measured
# reasons, all on this device, 2026-08-24:
#   1. `$HOME/.frontend` is REAL, PERSISTENT OWNER STATE. `/root/.profile:51`
#      relocates HOME to /mnt/FunKey, and /mnt/FunKey/.frontend currently
#      reads `gmenu2x`. `set none` OVERWRITES it with `none` — a second SD
#      file the boot-unpark line above does NOT clear, so the official verb
#      is strictly WORSE for the very class this function closes. And the
#      unpark verb `set gmenu2x` hardcodes a frontend the owner may not use
#      (`set retrofe` is equally legal), silently changing his choice.
#   2. dsh RUNS A NON-LOGIN SHELL, where HOME is `/` and `/` is mounted ro.
#      Measured: `frontend get` in that context dies with
#      `can't create //.frontend: Read-only file system` and falls back to
#      the built-in default `retrofe` — which is NOT what is running. So
#      `frontend set` from the rig would pkill the wrong frontend name and
#      fail to record, AFTER having already touched the marker, and
#      set_frontend checks none of it.
#   3. It buys nothing: `set_frontend` is `touch`/`rm -f` on the same marker
#      plus a pkill the rigs already do themselves.
# The rig's park is a rig park, not a user preference change, so it stays on
# the one file that expresses exactly that.
#
# THE SIBLING PERSISTENT FILES ARE CLEAN. `frontend` also reads
# `/mnt/last_opk` (SD, same outlive-the-test property) and `/run/rebooting`
# (tmpfs, but one-way — see above). Grepped 2026-08-24: NOTHING in port/
# writes either; both are written only by the OS's own gmenu2x/opkrun and
# powerdown paths. No second instance of this class in the rig.
#
# FAIL CLOSED. Called from rig_inherited_restore, i.e. step 0, before any
# consumer can park: if the line cannot be installed AND verified, the run
# dies rather than parking a device it cannot guarantee is recoverable.
# Idempotent: the healthy case is one grep and no rootfs write at all (`/` is
# mounted `ro` in normal operation and is left exactly as it was found).
rig_boot_unpark_install() {
  # Already installed => nothing to do, and NOTHING is remounted.
  if dsh "grep -q MLFK-BOOT-UNPARK /etc/rc.local" >/dev/null 2>&1; then
    return 0
  fi
  echo "   step-0: installing the D44 boot-unpark line into /etc/rc.local (one-time)" >&2
  # rw -> append after the shebang -> sync -> ro. `rm -f` on an absent vfat
  # path is rc 0, so the `|| :` is belt-and-braces against rc.local's `-e`.
  # `ro` runs unconditionally so a failed edit still leaves / read-only.
  # NOTE: no `exit` in a dsh payload — dsh appends its RC marker as a further
  # command in the SAME shell, so an `exit` swallows the marker and every call
  # reads back as a transport failure (measured while writing this).
  dsh "rw && sed -i '1a rm -f /mnt/disable_frontend || :   # MLFK-BOOT-UNPARK (D44)' /etc/rc.local && sync; r=\$?; ro >/dev/null 2>&1 || true; test \$r -eq 0" >/dev/null 2>&1 || {
    echo "DEVICE FAIL: could not write the boot-unpark line to /etc/rc.local" >&2
    return 1
  }
  # VERIFY what is now on disk, not what we believe we wrote: exactly one
  # occurrence, the file still parses as a shell script, still executable,
  # and / is back to read-only.
  if ! dsh "test \$(grep -c MLFK-BOOT-UNPARK /etc/rc.local) -eq 1 && sh -n /etc/rc.local && test -x /etc/rc.local" >/dev/null 2>&1; then
    echo "DEVICE FAIL: /etc/rc.local did not verify after the boot-unpark edit (count/syntax/exec bit)" >&2
    return 1
  fi
  dsh "mount | grep -q 'ext4 (ro'" >/dev/null 2>&1 \
    || echo "WARN: / is still mounted read-write after the boot-unpark install — run 'ro' on the device" >&2
  echo "   step-0: boot-unpark line installed and verified (D44)" >&2
  return 0
}

# rig_lock_release — THE ONLY code path that removes the rig lock
# (review-109-5 H1). It covers two duties:
#   * the EXIT trap installed by rig_lock_acquire, covering the window
#     between acquisition and the CALLER's own cleanup trap (review-109-4
#     H1) — without it, a death inside step 0 exits with the lock held;
#   * the release inside rig_cleanup, i.e. the normal end of every run.
# OWNERSHIP-CHECKED: the lock is removed only while $LOCK/owner still
# carries THIS run's nonce. A second release attempt (or a release by a
# run whose lock was already reclaimed by hand) finds a foreign/absent
# nonce and refuses loudly instead of deleting another run's lock.
rig_lock_release() {
  local own=""
  [ -n "${LOCK:-}" ] || return 0
  [ -d "$LOCK" ] || return 0
  own="$(cat "$LOCK/owner" 2>/dev/null)" || own="<unreadable>"
  if [ "$own" != "${RIG_LOCK_NONCE:-<unset>}" ]; then
    echo "WARN: rig lock $LOCK is owned by '$own', not by this run ('${RIG_LOCK_NONCE:-<unset>}') — NOT releasing it (releasing another run's lock would put two rigs on one device). Remove it by hand if you are sure no rig is running: rm -rf '$LOCK'" >&2
    return 0
  fi
  rm -rf "$LOCK" 2>/dev/null \
    || echo "WARN: could not release rig lock $LOCK — remove manually: rm -rf '$LOCK'" >&2
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
# Iter 109 (review-109-4 H1): step 0 is now ORDERED — an interim EXIT
# trap (lock release only), then rig_inherited_restore (restore + verify
# what a dead run left behind), and only THEN the reap that kills that
# run's recovery deadman. Acquisition, recovery and reaping are three
# separate steps and the recovery duty is transferred before the backstop
# is destroyed, never after.
rig_lock_acquire() {
  LOCK="${TMPDIR:-/tmp}/mlfk-rig-${DEV}.lock"
  # OWNERSHIP NONCE (review-109-5 H1). The lock was released by whichever
  # code path happened to run — several step-0 failure paths did their own
  # `rm -rf "$LOCK"` and the EXIT trap then released it AGAIN. Between the
  # two, a waiting run can mkdir the lock and legitimately own it, and the
  # first run's second release deletes the SECOND run's lock: two rigs on
  # one device. Every release now goes through rig_lock_release, which
  # removes the lock ONLY when the owner file still carries this run's
  # nonce. A double release is therefore a no-op with a loud warning.
  RIG_LOCK_NONCE="$$.$RANDOM.$(date +%s)"
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
  # stamp ownership BEFORE any release path can run
  printf '%s\n' "$RIG_LOCK_NONCE" > "$LOCK/owner" || {
    rm -rf "$LOCK" 2>/dev/null
    echo "DEVICE FAIL: could not write the rig lock ownership file $LOCK/owner" >&2
    exit 1
  }
  # the lock is ours from here — cover the window until the caller's own
  # cleanup trap replaces this one. This trap is the SOLE releaser for
  # every step-0 failure path below (none of them removes the lock
  # itself any more — review-109-5 H1).
  trap rig_lock_release EXIT
  # RESTORE FIRST, REAP SECOND (the H1 ordering — see rig_inherited_restore)
  rig_inherited_restore
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
  # ownership-checked, single release path (review-109-5 H1)
  rig_lock_release
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
# STATUS-CHECKED SCAN (review-109-5 H3 — the round-4 masked-read class,
# found surviving inside this SHARED helper after it was closed in the
# fullgame deadman). The scan used to discard every `cat` status, so a
# LIVE but unreadable /proc entry simply vanished from the inventory:
# rig_daemon_restore would then see "0 instances", take the NON-IDEMPOTENT
# START channel and create a duplicate, or certify "exactly 1" while only
# the readable copy was counted. Now the device emits `U<pid>` for any
# entry whose comm read FAILED while /proc/<pid> is still present
# (unclassifiable), and only a verified-vanished entry is the tolerated
# race. An unclassifiable inventory is a loud refusal here, which
# propagates as a refusal to stop OR start anything.
rig_comm_pids() {
  local d out line unread=0 attempt=0
  d="$1"
  # ALL-READABLE SNAPSHOT OR NOTHING (review-109-6 H4). Round 5 emitted
  # U<pid> only when a `[ -d /proc/<pid> ]` re-probe still saw the entry —
  # but `test -d` is ALSO false for a lookup/stat failure, so a live daemon
  # whose comm read AND re-probe both hit a procfs error still vanished
  # from the inventory and rig_daemon_restore could START a duplicate. The
  # device now reports EVERY failed read as U<pid> with no downgrade, and
  # the genuine vanished-pid race is handled the only sound way: RETRY THE
  # WHOLE SCAN. A pid that really disappeared is simply absent from the
  # next snapshot; one that is live-but-unreadable keeps reporting U and,
  # after the retries, the inventory is refused as unclassifiable.
  while :; do
    attempt=$((attempt + 1))
    unread=0
    out="$(dsh 'for c in /proc/[0-9]*/comm; do n="$(cat "$c" 2>/dev/null)"; crc=$?; p="${c#/proc/}"; p="${p%/comm}"; if [ "$crc" != 0 ]; then echo "U$p"; continue; fi; if [ "x$n" = "x'"$d"'" ]; then echo "$p"; fi; done')" || {
      echo "DEVICE FAIL: comm scan for $d failed on the device" >&2
      return 1
    }
    out="${out%$'\n'}" # the single measured dsh trailing-newline artifact
    case "$out" in
      *U[0-9]*) unread=1 ;;
    esac
    [ "$unread" = 0 ] && break
    if [ "$attempt" -ge 3 ]; then
      echo "DEVICE FAIL: comm scan for $d still reports UNREADABLE /proc entries after $attempt full-scan attempts — the daemon inventory cannot be classified, so this rig refuses to stop or start anything; inspect the device" >&2
      printf '%s\n' "$out" >&2
      return 1
    fi
    sleep 1
  done
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
# rig_qd_probe <name> — WHITELIST-GRAMMAR marker presence (review-109-5
# M4). The old probe read `ls`'s exit status and treated BOTH rc 1 and
# rc 2 as "no marker", but those statuses also cover a failed directory
# traversal or read — an unreadable $DTMP therefore certified "no daemon
# is quiesced" and licensed the caller to reap the recovery deadman. The
# device now answers with one of three DEFINITE tokens; any transport
# failure is dsh's own nonzero rc, and any other output is corruption.
rig_qd_probe() {
  local d="$1" out
  # STATUS-DISTINGUISHABLE ENUMERATION (review-109-6 M5). Round 5's tokens
  # renamed the old answer without separating the cases: `[ ! -d $DTMP ]`
  # is also false-on-stat-failure and any nonzero `ls` (traversal error,
  # I/O error, ARG_MAX) read as QDABSENT — so a failed enumeration could
  # certify "nothing is quiesced" and license the stale-deadman reap while
  # a marker existed. Now the directory is ensured with a CHECKED
  # operation and enumeration goes through `find`, whose successful-empty
  # result is distinct from its nonzero traversal failure; a failure is its
  # OWN token and dies here.
  out="$(dsh "if ! mkdir -p $DTMP 2>/dev/null; then echo QDMKFAIL; else f=\"\$(find $DTMP -maxdepth 1 -name 'qd.$d.*' 2>/dev/null)\"; frc=\$?; if [ \$frc != 0 ]; then echo QDPROBEFAIL; elif [ -n \"\$f\" ]; then echo QDPRESENT; else echo QDABSENT; fi; fi")" || {
    echo "DEVICE FAIL: could not probe for stale quiesce markers of $d (transport)" >&2
    return 1
  }
  out="${out%$'\n'}"
  case "$out" in
    QDPRESENT|QDABSENT) printf '%s\n' "$out" ;;
    QDMKFAIL)
      echo "DEVICE FAIL: could not ensure the device scratch dir $DTMP exists while probing $d's quiesce marker — refusing to treat that as 'no marker'" >&2
      return 1
      ;;
    QDPROBEFAIL)
      echo "DEVICE FAIL: enumerating $d's quiesce markers under $DTMP FAILED (traversal/IO) — refusing to treat a failed enumeration as absence" >&2
      return 1
      ;;
    *)
      echo "DEVICE FAIL: quiesce-marker probe for $d answered '$out' (want exactly QDPRESENT|QDABSENT)" >&2
      return 1
      ;;
  esac
}

# --- ATOMIC RECOVERY OWNERSHIP (review-109-5 H2) -----------------------------
# The arc's finding: "recovery has no atomic owner" — at startup the HOST's
# inherited-state restore and a still-live STALE deadman can both observe
# zero daemon instances and both take the NON-IDEMPOTENT init START
# channel, producing two instances (which every later helper then refuses,
# loudly, but only after the damage).
#
# The primitive is `mkdir`, which is atomic on the device's filesystem:
# exactly one actor can create $DTMP/qd.claim, and only its holder may
# touch the daemon plane or consume a quiesce marker. The holder releases
# it when done.
#
# TAKEOVER: a claim held longer than the recovery arm can possibly take
# (its holder died mid-recovery) would otherwise deadlock the next run, so
# the host force-takes after a bounded wait, LOUDLY — and because
# rig_daemon_restore re-derives the inventory by comm-scan every time, the
# post-takeover state is verified from the DEVICE, never inferred from the
# marker's presence (the reviewer's "reverify cardinality independently of
# marker presence after takeover").
#
# TRANSITION (honest exposure): a deadman deployed by an EARLIER run
# carries that run's script and does not take the claim. The claim
# therefore protects every pairing from this run onward; the one
# already-deployed generation remains covered only by the loud
# exact-cardinality refusals. Nothing can retro-fit a script that is
# already on the device.
# OWNED claim (review-109-6 H2 — the claim was an unowned lock bit, so its
# takeover repeated the very ABA race the host lock had just fixed: the
# host deleted it on a timer and a resuming holder could then delete the
# HOST's replacement claim). The claim directory now carries an owner
# token written by whoever created it:
#     HOST:<rig lock nonce>     |     DM:<deadman pid>
# and the rules are:
#   * release ONLY a claim this run owns (rig_qd_unclaim re-reads the
#     token; a foreign token is left alone, loudly);
#   * NEVER steal from a holder that is positively ALIVE — a DM:<pid>
#     whose /proc entry exists means the deadman is mid-recovery, and the
#     init START channel is not idempotent, so this run WAITS and then
#     dies loudly rather than racing it;
#   * steal ONLY when the holder is positively verified dead, or when the
#     claim carries no readable owner at all (a pre-protocol claim), and
#     then say so loudly. Cardinality is re-derived by comm-scan after any
#     takeover, so nothing is ever inferred from the claim itself.
rig_qd_claim() {
  local tries=0 out own tok
  tok="HOST:${RIG_LOCK_NONCE:-nolock}"
  while :; do
    # PUBLICATION IS STATUS-CHECKED (review-109-8 L): a claim whose owner
    # token failed to write is an UNOWNED claim — it would be reported as
    # ours, released by nobody, and taken over by the next actor. The
    # device answers CLAIMFAIL and we treat it as a failure to claim.
    out="$(dsh "mkdir -p $DTMP >/dev/null 2>&1; if mkdir $DTMP/qd.claim 2>/dev/null; then if printf '%s\n' '$tok' > $DTMP/qd.claim/owner; then echo CLAIMED; else rm -rf $DTMP/qd.claim; echo CLAIMFAIL; fi; else echo BUSY; fi")" || {
      echo "DEVICE FAIL: could not take the device recovery claim (transport)" >&2
      return 1
    }
    out="${out%$'\n'}"
    case "$out" in
      CLAIMED) return 0 ;;
      BUSY) : ;;
      CLAIMFAIL)
        echo "DEVICE FAIL: the recovery claim was created but its owner token could not be written — an unowned claim is worse than none, so it was removed and this run refuses to proceed" >&2
        return 1
        ;;
      *)
        echo "DEVICE FAIL: recovery-claim probe answered '$out' (want exactly CLAIMED|BUSY|CLAIMFAIL)" >&2
        return 1
        ;;
    esac
    tries=$((tries + 1))
    if [ "$tries" -ge 15 ]; then
      # who holds it, and is that holder still alive?
      own="$(dsh "if [ -f $DTMP/qd.claim/owner ]; then cat $DTMP/qd.claim/owner; else echo NOOWNER; fi")" || {
        echo "DEVICE FAIL: could not read the recovery claim's owner token" >&2
        return 1
      }
      own="${own%$'\n'}"
      case "$own" in
        DM:[0-9]*)
          # DEFINITE-ABSENCE ONLY (review-109-8 H4): `dsh test -d` returns
          # the DEVICE command's rc, but 70/71 are TRANSPORT failures. The
          # old form treated every nonzero as "the holder is dead", so a
          # transient transport blip licensed deleting a LIVE deadman's
          # claim and racing its non-idempotent START. Only rc 1 — the
          # device answering "no such directory" — is death.
          local prc=0
          dsh "test -d /proc/${own#DM:}" >/dev/null 2>&1 || prc=$?
          case "$prc" in
            0)
              echo "DEVICE FAIL: the device recovery claim is held by a LIVE deadman (pid ${own#DM:}) that is mid-recovery — refusing to steal it (the init START channel is not idempotent, and racing it would create a second daemon). Wait for it to finish, or inspect the device." >&2
              return 1
              ;;
            1)
              echo "WARN: the recovery claim's holder (deadman pid ${own#DM:}) is verified DEAD (device answered rc 1) — taking the claim over. Cardinality is re-derived by comm-scan below, so nothing is inferred from the claim." >&2
              ;;
            *)
              echo "DEVICE FAIL: could not determine whether the recovery claim's holder (deadman pid ${own#DM:}) is alive (rc $prc — transport or malformed marker). Refusing to take over on an indefinite answer; inspect the device." >&2
              return 1
              ;;
          esac
          ;;
        NOOWNER|"")
          echo "WARN: the recovery claim $DTMP/qd.claim carries no owner token (a pre-protocol claim) and has been held >30 s — taking it over. Cardinality is re-derived by comm-scan below." >&2
          ;;
        *)
          echo "WARN: the recovery claim is held by '$own', which is not a live-verifiable holder, and has been held >30 s — taking it over. Cardinality is re-derived by comm-scan below." >&2
          ;;
      esac
      dsh "rm -rf $DTMP/qd.claim" >/dev/null 2>&1 || true
    fi
    if [ "$tries" -ge 20 ]; then
      echo "DEVICE FAIL: could not take the device recovery claim $DTMP/qd.claim even after a takeover attempt — inspect the device" >&2
      return 1
    fi
    sleep 2
  done
}

# rig_deadman_quiesce — stop ANY inherited deadman, of ANY generation,
# before this run touches the daemon plane (review-109-6 H3). Uses the one
# channel every generated deadman in this repo honours: the presence of
# $DTMP/deadman.cancel, polled on a 2 s cadence, and $DTMP/deadman.pid,
# which each generation writes at start and removes at exit. Returns 0 only
# when no deadman process remains (either it acknowledged the cancel, or
# its pid file was orphaned and the pid is verifiably dead).
rig_deadman_quiesce() {
  local out tries=0
  dsh "mkdir -p $DTMP && touch $DTMP/deadman.cancel" >/dev/null || {
    echo "DEVICE FAIL: could not write the deadman cancel marker $DTMP/deadman.cancel" >&2
    return 1
  }
  while :; do
    out="$(dsh "if [ -f $DTMP/deadman.pid ]; then cat $DTMP/deadman.pid; else echo NONE; fi")" || {
      echo "DEVICE FAIL: could not read $DTMP/deadman.pid while quiescing an inherited deadman" >&2
      return 1
    }
    out="${out%$'\n'}"
    [ "$out" = NONE ] && return 0
    if ! [[ "$out" =~ ^(0|[1-9][0-9]{0,6})$ ]]; then
      echo "DEVICE FAIL: $DTMP/deadman.pid is not a bounded decimal pid ('$out')" >&2
      return 1
    fi
    # DEFINITE-ABSENCE ONLY (review-109-8 H4): the same rc conflation as
    # rig_qd_claim — a transport rc (70/71) must never read as "the
    # watchdog is dead", which would retire a backstop that is still armed.
    local prc=0
    dsh "test -d /proc/$out" >/dev/null 2>&1 || prc=$?
    case "$prc" in
      0) : ;;
      1)
        echo "   step-0: the inherited deadman pid file was orphaned (pid $out verifiably dead)" >&2
        return 0
        ;;
      *)
        echo "DEVICE FAIL: could not determine whether the inherited deadman (pid $out) is alive (rc $prc — transport or malformed marker); refusing to proceed on an indefinite answer" >&2
        return 1
        ;;
    esac
    tries=$((tries + 1))
    if [ "$tries" -ge 8 ]; then
      echo "DEVICE FAIL: an inherited deadman (pid $out) is STILL RUNNING 16 s after its cancel marker was written — it is not honouring the universal cancel channel; inspect the device" >&2
      return 1
    fi
    sleep 2
  done
}

# rig_qd_reassert — REVALIDATE OWNERSHIP IMMEDIATELY BEFORE ANY
# DAEMON-PLANE ACTION (review-109-9 H1: the suspended-host case).
# A host that blocks for a long time (laptop sleep, a stalled adb round
# trip) while holding the claim can be declared dead by the deadman: the
# deadman sees a frozen lease, reclaims the claim and republishes it as
# DM:<pid>. If the host then resumes and trusts its own in-memory
# "I hold the claim" flag, BOTH actors scan zero and BOTH take the
# non-idempotent START channel. An in-memory flag cannot survive a
# blocking interval, so it is never trusted across one: this re-reads the
# DEVICE's own owner token and, if the claim is no longer ours, takes a
# fresh one through rig_qd_claim (which never steals from a live holder).
# Callers invoke it immediately before stopping or restoring a daemon.
rig_qd_reassert() {
  local own tok
  tok="HOST:${RIG_LOCK_NONCE:-nolock}"
  own="$(dsh "if [ -f $DTMP/qd.claim/owner ]; then cat $DTMP/qd.claim/owner; elif [ -d $DTMP/qd.claim ]; then echo NOOWNER; else echo GONE; fi")" || {
    echo "DEVICE FAIL: could not re-read the recovery claim's owner token before a daemon-plane action" >&2
    return 1
  }
  own="${own%$'\n'}"
  if [ "$own" = "$tok" ]; then
    return 0
  fi
  echo "WARN: this run's recovery claim was taken over while it was blocked (owner is now '$own') — re-acquiring before touching the daemon plane, so two actors can never drive the non-idempotent START channel at once" >&2
  rig_qd_claim
}

rig_qd_unclaim() {
  local own tok
  tok="HOST:${RIG_LOCK_NONCE:-nolock}"
  own="$(dsh "if [ -f $DTMP/qd.claim/owner ]; then cat $DTMP/qd.claim/owner; elif [ -d $DTMP/qd.claim ]; then echo NOOWNER; else echo GONE; fi")" || {
    echo "WARN: could not read the recovery claim's owner token while releasing it — leaving $DTMP/qd.claim in place" >&2
    return 0
  }
  own="${own%$'\n'}"
  case "$own" in
    GONE) return 0 ;;
    "$tok")
      dsh "rm -rf $DTMP/qd.claim" >/dev/null 2>&1 \
        || echo "WARN: could not release the device recovery claim $DTMP/qd.claim — remove it by hand before the next run" >&2
      ;;
    *)
      echo "WARN: the device recovery claim is now owned by '$own', not by this run — NOT releasing it (deleting another actor's claim is the ABA race this ownership token exists to prevent)" >&2
      ;;
  esac
}

rig_qd_normalize() {
  local d isc st st2
  for d in low_bat_check system_stats fkgpiod; do
    case "$d" in
      low_bat_check) isc=/etc/init.d/S12low-bat-check ;;
      system_stats)  isc=/etc/init.d/S13system-stats ;;
      fkgpiod)       isc=/etc/init.d/S11gpio ;;
    esac
    st="$(rig_qd_probe "$d")" || return 1
    case "$st" in
      QDPRESENT)
        echo "WARN: stale quiesce marker for $d on the device — a prior run may have left the daemon down; restoring BEFORE any disarm/wipe" >&2
        if rig_daemon_restore "$d" "$isc"; then
          # VERIFIED REMOVAL (review-109-5 M4): the removal used to be
          # `|| true` and its result was never re-probed, so this function
          # could report success — and license the deadman reap — with the
          # marker still sitting on the device.
          dsh "rm -f $DTMP/qd.$d.*" >/dev/null || {
            echo "DEVICE FAIL: stale quiesce marker for $d could not be removed after a verified restore" >&2
            return 1
          }
          st2="$(rig_qd_probe "$d")" || return 1
          if [ "$st2" = QDPRESENT ]; then
            echo "DEVICE FAIL: stale quiesce marker for $d is STILL present after removal — refusing to report the device normalized" >&2
            return 1
          fi
          echo "   stale-quiesced $d verified running (comm-scan, exactly 1); marker cleared and absence verified" >&2
        else
          echo "DEVICE FAIL: stale-quiesced $d could not be restored to exactly one instance — run '$isc start' on the device manually and inspect" >&2
          return 1
        fi
        ;;
      QDABSENT) : ;; # no marker — the healthy path (the ONLY absence token)
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
        port/foh/foh_pause.c \
        # A45 T3/T4 — the target builder engine and the custom-stage
        # plane. foh_tbuild.c is the FOH-side editor (behind the
        # foh_tbuild_ops pointer seam so no OTHER build had to change);
        # custom_stage.c + stage_code.c are A45 T1/T2, which foh_dev.c
        # now calls directly to play a custom slot.
        port/foh/foh_tbuild.c \
        port/sim/stage_code.c port/sim/target/custom_stage.c \
        port/gfx/ctl_style.c \
        port/gfx/img1.c \
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
