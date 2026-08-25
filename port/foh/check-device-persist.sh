#!/usr/bin/env bash
# check-device-persist.sh — M4 task 13 done-check: PERSISTENCE TO SD
# (fix_plan §M4 task 13; pre-registration AGENT-LOG iter 100).
#
# THE CLAIM: settings + target-test records persist at /mnt/mlfk-data
# through the ONE foh_persist chokepoint (atomic tmp+fsync+rename;
# corrupt/missing/version = LOUD reset-to-defaults, never silent — the
# qjs getCookie lesson inverted), and SURVIVE A POWER CYCLE: two full
# device app sessions separated by a REAL reboot over ADB (the
# pre-registered primary form), every persisted byte judged ON THE
# HOST against host-constructed twin files, and the records READ path
# witnessed end-to-end (SD bytes -> chokepoint -> FohState -> the
# target-select PERSONAL BEST pixels, byte-exact vs host twin shots).
#
# Composition (the check-device-target.sh conventions inherited):
#  [0] startup normalization + inherited-state ownership (riglib
#      chokepoints; sha tool self-test);
#  [1] producer twin pin (judge-foh-trace.js == check-foh-flows.sh's
#      row) + data planes (tables incl. TTAB1);
#  [2] host foh_dev_headless build (the device-target recipe +
#      foh_persist.c);
#  [3] check-owned flows p00/p01/p02 (in-check heredocs, the wit-g01
#      precedent) + HOST REFERENCES: p01 x2 byte-stable persist file,
#      post-record-arm file, p02 persisted-twin shots, p02
#      DEFAULTS-CONTROL shots (must DIFFER — the display is
#      load-bearing), every reference file independently
#      grammar+SUM-verified host-side (anchored full-line; shasum
#      recompute — the whitelist-grammar rule);
#  [4] HOST TEETH on copies (T-H1..T-H15): corrupt-sum / unsupported-
#      version / v1+v2 migration / per-version grammar /
#      NaN-domain / truncation -> the exact loud reset lines; torn
#      .tmp beside a valid file -> loaded intact (rename-atomicity:
#      rename is the only publish); read-only dir -> save dies LOUD
#      with the real file byte-unchanged; record-regress ->
#      improved=0, no save, file byte-identical;
#  [5] shared arm build (riglib stamp; foh_persist.c joined the
#      foh_device recipe) + push + sha provenance; pre-existing
#      product file pulled aside (restored at cleanup);
#  [6] SESSION A on device (park+deadman window 1; legs run --input
#      flow --pace 0, PRE-REGISTERED deviation from the M3 poll
#      binding — the input path is task 10/12's proven surface, not
#      this check's claim): leg dp01 edits settings through the REAL
#      options UI -> save on the upstream B-exit; pull #1 must be
#      BYTE-IDENTICAL to the host p01 reference; then the device
#      record arm (--tooth-persist-finish: the REAL tp_finish_game ->
#      hook -> chokepoint chain; a genuinely completing run is
#      authored-unreachable — iter-99 refutation, honest-coverage
#      note) -> pull #2 == the host post-arm reference;
#  [7] POWER CYCLE: unpark -> `adb reboot` -> bounded adbd wait ->
#      re-verify + re-provision (tmpfs wiped) -> re-park (window 2);
#      the park marker NEVER spans the reboot (a mid-reboot death
#      cannot strand the frontend with the tmpfs deadman gone);
#  [8] SESSION B: leg dp02 boots -> `foh_persist: loaded` (NOT reset)
#      + pull #3 byte-identical to pull #2 (the power-cycle claim) +
#      the options + PERSONAL BEST shots byte-exact vs the host
#      persisted twins;
#  [9] DEVICE TEETH: a nibble-flipped COPY pushed over the file ->
#      probe leg says `reset cause=corrupt detail=sum`; recovery leg
#      dp03 (the p02 flow) saves DEFAULTS -> pull #4 == the host
#      defaults-control file + its PB shot == the control shot (the
#      loud inverse witnessed ON the product surface);
#  [10] hygiene: unpark verified, deadman cancelled unfired, product
#      residue wiped / pre-existing restored, no-commit guard.
#
# Prints `PERSIST OK (...)`, exit 0; ANY byte mismatch, missing/extra
# persist event line, grammar violation, or missing artifact ->
# nonzero.
#
# HONEST EXPOSURE (PROCESS §8; pre-registered): the
# sim-reaches-completion -> tp_finish_game edge is NOT exercised here
# (authored-unreachable in committed flows; live finish = acceptance
# surface; the trigger is probe-covered in check-target-sim.sh). The
# records write is proven from tp_finish_game DOWNWARD via the real
# hook/chokepoint/SD/HUD chain. Unpaced legs make no perf claim.
set -euo pipefail
cd "$(dirname "$0")/../.."

GFX=port/gfx
FOH=port/foh
SIM=port/sim/sim
CAL=port/sim/calib
TGT=port/sim/target
DEVB=port/sim/calib/build/device
TABLES=pipeline/build/sim-tables
BUILD=$FOH/build/device-persist
FDC=oracle/fdlibm-crosscheck
DTMP=/tmp/mlfk
DSD=/mnt/mlfk-scratch
DDATA=/mnt/mlfk-data
DFILE=$DDATA/mlfk-persist.dat

# the record parameters (pinned; 14.5 s = bits 402d000000000000 ->
# HUD "00:14.50"; the regress arm uses 16.0 s = 4030000000000000)
REC_CHAR=0
REC_TSTAGE=0
REC_BITS=402d000000000000
REC_DISPLAY="00:14.50"
WORSE_BITS=4030000000000000

# review-102 M-b: a genuine MLFKPERSIST7 file is EXACTLY this many bytes
# (line-derived: header 13 + turbo 8 + lcancel 10 + tapjump 16 +
# ctlstyle 11 + modonr 9 (fix_plan A4) + 50 rec rows x25 + the v4 options
# block 124 (flash 8 + walljump 11 + blastzone 12 + dustless 11 +
# phantom 25 + soundslevel 29 + musiclevel 28; MENU-SPEC §3/§4) + the v5
# bind block 92 (4 rows x23; fix_plan A31) + the v6 sel row 12
# (`sel c c c c` + LF; fix_plan A49, DEVIATION D45) + the v7 resume row 10
# (`resume NN` + LF; fix_plan A26, DEVIATION D53) + SUM 69
# = 1624). A dropped/added byte — including an embedded NUL that command
# substitution silently swallows through the per-line sed reads — breaks
# this reconciliation.
PERSIST_BYTES=1624

DEADMAN_S="${MLFK_DEADMAN_S:-900}"
READY_TRIES=60

fail() { echo "PERSIST FAIL: $1" >&2; exit 1; }
grammar_die() { echo "PERSIST FAIL: $1" >&2; exit 2; }

count_xl() { # FULL-LINE fixed-string count (rc case-split)
  local c rc=0
  c="$(grep -cxF -- "$2" "$1")" || rc=$?
  if [ "$rc" -ge 2 ]; then
    grammar_die "count helper — grep -cxF rc $rc reading '$1' (a read error is CORRUPT evidence, never a 0 count)"
  fi
  printf '%s' "$c"
}
count_e() { # anchored-regex count (rc case-split)
  local c rc=0
  c="$(grep -cE -- "$2" "$1")" || rc=$?
  if [ "$rc" -ge 2 ]; then
    grammar_die "count helper — grep -cE rc $rc reading '$1'"
  fi
  printf '%s' "$c"
}

# L1 (review-100): derive the expected PERSONAL BEST display string
# INDEPENDENTLY from a record's hex16 bit pattern via the SPEC rule
# (targetselect.js:411-419 + the registered integer-centisecond delta
# cs=floor(rec*100+0.5) -> "0<cs/6000>:<pad2 (cs%6000)/100>.<pad2
# cs%100>"). bit-pattern -> double -> string, in node — NEVER the C
# renderer / foh_render.c. -1.0 -> the "--:--:--" defaults display.
derive_pb() { # <hex16> -> the spec display string on stdout
  node -e '
    const h = process.argv[1];
    if (!/^[0-9a-f]{16}$/.test(h)) { process.stderr.write("bad hex16\n"); process.exit(3); }
    const rec = Buffer.from(h, "hex").readDoubleBE(0);
    if (rec === -1) { process.stdout.write("--:--:--"); process.exit(0); }
    if (!(isFinite(rec) && rec >= 0 && rec < 6000)) { process.stderr.write("out of domain\n"); process.exit(4); }
    const cs = Math.floor(rec * 100 + 0.5);
    const p2 = n => String(n).padStart(2, "0");
    process.stdout.write("0" + Math.floor(cs / 6000) + ":" + p2(Math.floor((cs % 6000) / 100)) + "." + p2(cs % 100));
  ' "$1"
}

# H1 (review-100): identity-grade reboot witness. The old down-only
# evidence (adb get-state != device) can be faked by a silently-failed
# reboot + a USB/adbd blip. capture_bootid reads the device boot
# identity HOST-SIDE: /proc/sys/kernel/random/boot_id (canonical UUID)
# when present, else `btime` from /proc/stat (bounded decimal) — the
# source is MEASURED at runtime and recorded; the SAME source must be
# used on both sides (bootid_judge enforces it). Emits "<src> <id>".
BOOTID_SRC=""
# raw_single_line <rawfile> — review-102 M-a: enforce the EXACT producer
# newline shape on a captured raw device stream. Command substitution
# ($(...)) silently DROPS trailing LFs (and NULs — the M-b byte-drop
# class), so freshness tokens MUST be validated as bytes on disk, not
# through $(). review-104 M-1 (contract corrected review-106 L-b): the
# capture file is READ-ONLY here — nothing is normalized, de-CR'd or
# rewritten. The whole file must be EXACTLY <printable-ASCII body><CR><LF>
# (the MEASURED adb-pty producer shape); the body is echoed WITHOUT the
# terminator (the CALLER applies the producer's exact-token grammar).
# Returns 1 on any byte-shape violation (empty body, missing CR or LF,
# interior CR/LF/NUL/control, high bytes, any trailing byte after the
# CRLF). Pure host logic (reads a local file) — teeth invoke it.
raw_single_line() {
  # review-104 M-1: BYTE-EXACT validation of the RAW device capture — NO
  # tr/squeeze/normalization on a decision-bearing stream, and no
  # $()-through laundering (command substitution silently DROPS NULs and
  # trailing LFs, the exact holes the round-2 tr masked). Render the WHOLE
  # file to hex (od -An -v -tx1 — every captured byte, incl. CR/LF/NUL,
  # preserved as a hex pair; the tr here strips only od's column layout
  # from the HEX RENDERING, never a captured byte) and require the exact
  # MEASURED adb-pty producer grammar: <printable-ASCII-body><CR><LF>. The
  # body bytes must be strictly 0x20-0x7e — an interior CR/LF/NUL/control
  # byte, a de-CR'd token, a missing CR, or ANY trailing byte after the
  # CRLF all fail closed. The decoded body is echoed WITHOUT the
  # terminator; the CALLER applies the exact token grammar.
  local f="$1" hex body pair ch ascii=""
  hex="$(od -An -v -tx1 "$f" 2>/dev/null | tr -d ' \n')" || return 1
  case "$hex" in
    *0d0a) : ;;                       # whole file MUST end in exactly one CRLF
    *) return 1 ;;
  esac
  body="${hex%0d0a}"                  # bytes before the CRLF terminator
  [ -n "$body" ] || return 1          # a bare CRLF (empty token) is not valid
  # body must be printable ASCII only (0x20-0x7e): rejects interior CR (0d),
  # LF (0a), NUL (00), any control byte, DEL (7f), and high bytes.
  [[ "$body" =~ ^([2-6][0-9a-f]|7[0-9a-e])*$ ]] || return 1
  # decode the validated printable body hex -> ASCII (bounded; tokens <= 40)
  while [ -n "$body" ]; do
    pair="${body:0:2}"; body="${body:2}"
    printf -v ch "\\x$pair" || return 1
    ascii="$ascii$ch"
  done
  printf '%s' "$ascii"
}
capture_bootid() {
  local rawf s
  rawf="$BUILD/.bootid.raw.$$"; rm -f "$rawf"
  adb -s "$DEV" shell 'cat /proc/sys/kernel/random/boot_id 2>/dev/null' > "$rawf" 2>/dev/null || true
  # RAW-byte grammar FIRST (M-a/M-1): validate the exact MEASURED producer
  # shape (36-char canonical UUID body + a single trailing CRLF) BEFORE
  # extraction — never a whitespace squeeze that would launder junk in.
  if s="$(raw_single_line "$rawf")" \
     && [[ "$s" =~ ^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$ ]]; then
    rm -f "$rawf"; printf 'bootid %s' "$s"; return 0
  fi
  rm -f "$rawf"
  rawf="$BUILD/.btime.raw.$$"; rm -f "$rawf"
  adb -s "$DEV" shell 'grep ^btime /proc/stat 2>/dev/null' > "$rawf" 2>/dev/null || true
  if s="$(raw_single_line "$rawf")" \
     && [[ "$s" =~ ^btime\ ([1-9][0-9]{5,12})$ ]]; then
    rm -f "$rawf"; printf 'btime %s' "${BASH_REMATCH[1]}"; return 0
  fi
  rm -f "$rawf"
  fail "boot-identity: NEITHER /proc/sys/kernel/random/boot_id (canonical UUID) NOR /proc/stat btime is available on this kernel — no identity-grade reboot witness exists (H1-a refutation: STOP)"
}
# bootid_judge <pre-src> <pre-id> <post-src> <post-id> <post-uptime-s> <gap-s>
# PURE HOST judge (the rig_quiesce_bracket_assert precedent): same
# source both sides, POST != PRE (an identity change = the cycle
# happened), and the rebooted integer uptime is YOUNGER than the
# host-measured dispatch->read gap (a FRESH boot, not a stale one).
# Loud death on any failure; no device I/O.
bootid_judge() {
  local psrc="$1" pid="$2" qsrc="$3" qid="$4" upt="$5" gap="$6"
  [ "$psrc" = "$qsrc" ] || fail "boot-identity: source flipped across the cycle ($psrc -> $qsrc) — cannot compare identities"
  [ "$pid" != "$qid" ] || fail "boot-identity: POST identity == PRE identity ($qid) — the device did NOT actually reboot (a silently-failed reboot + an adbd blip; do NOT pass a non-cycle as a cycle)"
  [[ "$upt" =~ ^(0|[1-9][0-9]{0,7})$ ]] || fail "boot-identity: POST uptime '$upt' is not a canonical integer"
  [[ "$gap" =~ ^(0|[1-9][0-9]{0,7})$ ]] || fail "boot-identity: dispatch->read gap '$gap' is not a canonical integer"
  [ "$upt" -lt "$gap" ] || fail "boot-identity: POST uptime ${upt}s >= the dispatch->read gap ${gap}s — this is a STALE boot, not the cycle we triggered"
  return 0
}

# M4 (review-100): ZERO-BYTE-SAFE pull — pullv with the non-empty assert
# dropped. A pre-existing user file may legitimately be zero bytes; the
# empty-file sha is well-defined, so sha equality is the sole judge (a
# zero-byte file must be preserved as bytes, not treated as absent).
pull_bytes() { # <device-path> <host-dst>
  local dsum hsum
  rm -f "$2"
  adb -s "$DEV" pull "$1" "$2" >/dev/null
  dsum="$(rig_dev_sha256 "$1")" || return 1
  hsum="$(rig_host_sha256 "$2")" || return 1
  [ "$dsum" = "$hsum" ] \
    || { echo "DEVICE FAIL: pulled $2 != device $1 (device $dsum, host $hsum)" >&2; return 1; }
}

source port/sim/device/adbsh.sh
require_device
source port/sim/device/riglib.sh
mkdir -p "$BUILD" "$DEVB"
rig_lock_acquire
RIG_PRESERVE_DTMP=1
PARKED=0
DEADMAN_ARMED=0
DM_NONCE=""
PREEXIST=0
PREEXIST_RESTORED=0
# review-102 H (DATA-LOSS): a three-state model for the user's product
# file. The EXIT trap installed here runs at PERSIST_STATE=UNPROBED,
# BEFORE the device file is probed in [5]; an early [0]-[4] failure must
# NEVER delete $DFILE (the user's only copy). The state advances ONLY at
# the [5] probe: prc=1 -> ABSENT (genuinely absent; any bytes there now
# are OUR residue); prc=0 AND backup pulled+hash-verified -> PRESENT
# (restore, never delete). A backup pull/hash failure leaves the state
# UNPROBED, so the `fail` abort fires BEFORE any $DFILE device write.
PERSIST_STATE=UNPROBED

# persist_residue_decide <state> <restored> <backup-present> -> action
# PURE (no I/O) — the trap dispatches device ops on the returned action;
# a host tooth invokes THIS body directly. The ONLY arm that may DELETE
# the device file is ABSENT. UNPROBED and any UNKNOWN state KEEP
# (fail-safe: never delete the user's only copy on an early/unknown
# path). PRESENT restores from the verified backup (never deletes); a
# missing backup KEEPS the file untouched, loudly.
persist_residue_decide() {
  local state="$1" restored="$2" backup="$3"
  case "$state" in
    PRESENT)
      if [ "$restored" = 1 ]; then printf 'noop'
      elif [ "$backup" = 1 ]; then printf 'restore'
      else printf 'keep-nobackup'; fi ;;
    ABSENT)   printf 'delete' ;;
    UNPROBED) printf 'keep' ;;
    *)        printf 'keep' ;;
  esac
}
cleanup() {
  rig_dsh_retry "pkill foh_device; true" \
    || echo "WARN: could not pkill foh_device on the device" >&2
  # product-surface residue — THREE-STATE (review-102 H, data-loss):
  # dispatch on persist_residue_decide (the security-critical decision
  # lives in ONE pure function, toothed host-side). The success path
  # restores + byte-verifies in [10] BEFORE the verdict
  # (PREEXIST_RESTORED then makes PRESENT a noop).
  local resid_backup=0
  [ -f "$BUILD/preexisting-mlfk-persist.dat" ] && resid_backup=1
  local resid_act
  resid_act="$(persist_residue_decide "$PERSIST_STATE" "$PREEXIST_RESTORED" "$resid_backup")"
  case "$resid_act" in
    noop)
      : ;; # PRESENT already restored + byte-verified in [10]
    restore)
      if adb -s "$DEV" push "$BUILD/preexisting-mlfk-persist.dat" "$DFILE" >/dev/null 2>&1; then
        echo "   pre-existing $DFILE restored (trap backstop)" >&2
      else
        echo "WARN: could not restore the pre-existing $DFILE (copy kept at $BUILD/preexisting-mlfk-persist.dat)" >&2
      fi ;;
    keep-nobackup)
      echo "WARN: PRESENT state but the backup copy is missing — leaving $DFILE UNTOUCHED (never delete without a verified backup)" >&2 ;;
    delete)
      rig_dsh_retry "rm -f $DFILE $DDATA/mlfk-persist.tmp" \
        || echo "WARN: could not wipe the persist test residue in $DDATA" >&2 ;;
    keep|*)
      echo "   persist file state=$PERSIST_STATE — leaving $DFILE UNTOUCHED (no completed probe / unknown state; the user's only copy is never deleted on an early-failure path)" >&2 ;;
  esac
  restore_verified=0
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
    if [ "$restore_verified" = 1 ]; then
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

# --- [0] startup normalization (the device-check chokepoint) ------------------
echo "== [0/10] startup normalization + inherited-state ownership =="
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
    dsh "rm -f /mnt/disable_frontend"
    dsh "test ! -f /mnt/disable_frontend"
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

# --- [1] twin pin + data planes -----------------------------------------------
echo "== [1/10] judge twin pin + data planes =="
JUDGE_SHA=2cf26a5b8b7065c17ffa934d8a027d3bba3a8ea50b121cdd6d8bd8e9155b8668
have="$(rig_host_sha256 "$FOH/judge-foh-trace.js")" || exit 1
[ "$have" = "$JUDGE_SHA" ] || fail "judge-foh-trace.js sha $have != pinned $JUDGE_SHA (reviewed pin update in the same commit)"
c="$(grep -cF "$JUDGE_SHA port/foh/judge-foh-trace.js" "$FOH/check-foh-flows.sh")" || true
[ "$c" = 1 ] || fail "twin pin — check-foh-flows.sh does not carry the same judge sha exactly once (count $c; paired change rule)"
bash pipeline/extractor/build-extractor.sh
rm -f "$TABLES/ml_tables.c" "$TABLES/ml_tables.h" \
  "$TABLES/ml_stages.c" "$TABLES/ml_stages.h" \
  "$TABLES/ml_targets.c" "$TABLES/ml_targets.h" \
  "$TABLES/assets/menu.img1"
node pipeline/run.js --only animations,tables,stages,targets,assets --out "$TABLES"
# A1 restyle Phase 1: the FOH's CSS/SSS screens render REAL upstream artwork
# from the `assets` stage's IMG1 pack, and foh_render's art_load treats a
# missing pack as FATAL. Both sides must be pointed at THIS run's freshly
# regenerated file: the host side via the exported var, the device side via
# its launcher env (sha-verified below). Mirrors
# port/foh/check-device-foh.sh. PROVENANCE: Nintendo-derived, private use
# only, gitignored build output — never committed, never distributed.
made "$TABLES/assets/menu.img1"
export MLFK_MENU_IMG1="$PWD/$TABLES/assets/menu.img1"
made "$TABLES/ml_tables.c" "$TABLES/ml_stages.c" "$TABLES/ml_targets.c" \
  "$TABLES/ml_targets.h"
echo "   judge twin pin OK; tables fresh"

# --- [2] host build -----------------------------------------------------------
echo "== [2/10] host foh_dev_headless build =="
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
  "$FOH/foh_tbuild.c" port/sim/stage_code.c "$TGT/custom_stage.c" \
  "$GFX/ctl_style.c" "$GFX/img1.c" \
  "$GFX/platform_headless.c" \
  "$GFX/anim1.c" "$GFX/gfx_render.c" "$GFX/gfx_target.c" \
  "$GFX/gfx_vfx.c" "$GFX/gfx_overlay.c" "$GFX/gfx_bg.c" \
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
echo "   host twin built"

# --- [3] check-owned flows + host references ----------------------------------
echo "== [3/10] flows + host reference construction =="
FLOWD=$BUILD/flows
rm -rf "$FLOWD"
mkdir -p "$FLOWD"
cat > "$FLOWD/p00-persist-probe.flow" << 'EOF'
FLOW1
# check-owned boot probe (iter 100): load fires at boot; no input.
I 1 -
END 30
EOF
cat > "$FLOWD/p01-persist-edit.flow" << 'EOF'
FLOW1
# check-owned settings-edit session (iter 100): menu-top -> Options ->
# Gameplay; edits turbo 0->1 (A row 0), lcancel 0->1->2 (A x2 row 1),
# tapjump P2 (row 4 col 1); shot; B-exit = the upstream cookie-save
# point (gameplaymenu.js:29-33) -> foh_persist save.
# NOTE (R5, 2026-07-31): the tapjump row is FOUR down from turbo, not two
# — the gameplay screen carries upstream's five rows (turbo, lcancel,
# flashlcancel, walljump, tapjump; foh.c kOptColMax / FOH_OPT_ROWMAX).
# This flow was authored against a three-row screen and had never been
# executed, so the drift was invisible until the first device run. The
# D-presses below are the ONLY change; every asserted line (including
# `S 440 tapjump2 1`) keeps its exact frame.
I 1 -
I 375 S
I 376 -
I 380 D
I 381 -
I 385 D
I 386 -
I 390 D
I 391 -
I 395 A
I 396 -
I 400 D
I 401 -
I 405 A
I 406 -
I 410 A
I 411 -
I 415 D
I 416 -
I 420 A
I 421 -
I 425 A
I 426 -
I 428 D
I 429 -
I 431 D
I 432 -
I 434 D
I 435 -
I 437 R
I 438 -
I 440 A
I 441 -
SHOT 445 opt-edited
I 450 B
I 451 -
END 460
EOF
cat > "$FLOWD/p02-persist-verify.flow" << 'EOF'
FLOW1
# check-owned verify session (iter 100): options screen shows the
# PERSISTED values (shot), B-exit resaves (idempotence witness),
# B to menu-top (cursor 3 = OPTIONS), U x2 to Target Test, A ->
# target-select: the PERSONAL BEST line shows the persisted record
# for (char 0, tstage 0) — the task-13 records READ path (shot).
I 1 -
I 375 S
I 376 -
I 380 D
I 381 -
I 385 D
I 386 -
I 390 D
I 391 -
I 395 A
I 396 -
I 400 D
I 401 -
I 405 A
I 406 -
SHOT 410 opt-persisted
I 415 B
I 416 -
I 420 B
I 421 -
I 425 U
I 426 -
I 430 U
I 431 -
I 435 A
I 436 -
SHOT 440 tss-record
END 450
EOF
made "$FLOWD/p00-persist-probe.flow" "$FLOWD/p01-persist-edit.flow" \
     "$FLOWD/p02-persist-verify.flow"

# run_host <flow-id> <outdir> <persist-dir> — one host leg (flow mode,
# unpaced). stderr -> <outdir>/log.txt; trace judged by the PINNED
# judge; rc must be 0.
run_host() {
  local id="$1" od="$2" pd="$3"
  shift 3 # any remaining args pass through (the M1 witness --tooth-finish-at)
  rm -rf "$od"
  mkdir -p "$od/shots"
  MLFK_PERSIST_DIR="$pd" \
  "$BUILD/foh_dev_headless" --flow "$FLOWD/$id.flow" --input flow \
    --flow-out "$od/trace.txt" --shots-dir "$od/shots" --pace 0 "$@" \
    2> "$od/log.txt" || { cat "$od/log.txt" >&2; fail "host leg $id failed"; }
  made "$od/trace.txt" "$od/log.txt"
  node "$FOH/judge-foh-trace.js" "$od/trace.txt" "$id" 0 >/dev/null \
    || fail "host leg $id: trace failed the pinned judge"
  # review-100 M3: the degraded dir-durability token must be ABSENT on
  # every healthy leg (a saved-nodirsync here = an unexpected dir-open
  # failure on the product/host path — never masked).
  [ "$(count_xl "$od/log.txt" "foh_persist: saved-nodirsync")" = 0 ] \
    || grammar_die "host leg $id: the degraded 'saved-nodirsync' token appeared on a healthy leg"
}

# EXACT POSITIONAL MLFKPERSIST7 whitelist verification, INDEPENDENT of
# the C loader (review-100 M2 + the whitelist-grammar rule, PROCESS §3).
# The format is a FIXED shape — so this asserts it BY POSITION:
# final byte LF, exactly 68 lines, each line matched at its exact index
# by an anchored full-line pattern, the 50 rec rows carrying the
# canonical c-major (c 0..4, s 0..9) progression at their exact
# position (uniqueness by position, not a global count), each rec bit
# pattern in the C loader's domain (== the -1.0 sentinel or finite in
# [0,6000)), the four v5 bind rows carrying the port-major progression and
# each being a PERMUTATION of 0..7, and a shasum recompute of the SUM seal
# over lines 1..69 (the whole body; the SUM line itself is line 70).
# Binary outcome: exact match -> pass; resembles-but-doesn't -> fail
# closed (grammar_die). NO global counts, NO permissive scan.
hex_lt() ( LC_ALL=C; [[ "$1" < "$2" ]]; ) # fixed 16-hex: byte order == numeric order
verify_persist_file() { # <file> <ctx>
  local f="$1" ctx="$2" nl L sum want ln c s bits
  made "$f"
  # review-102 M-b: byte-level final-LF check. `$(tail -c1)` DROPS a
  # trailing NUL (command substitution), so a NUL-terminated torn write
  # passes a `-z` test as if it ended in LF — compare the raw byte hex.
  local lastbyte nbytes
  lastbyte="$(tail -c1 "$f" | od -An -tx1 | tr -d ' \n')"
  [ "$lastbyte" = 0a ] || grammar_die "$ctx: final byte is 0x$lastbyte, not 0x0a (a clean final LF) — torn/NUL-terminated write"
  # byte-count reconciliation: the whole-file size must equal the exact
  # line-derived expectation. Catches any dropped/added byte (embedded
  # NUL, stray CR, truncation) the per-line $(sed) reads would launder.
  nbytes="$(wc -c < "$f" | tr -d ' ')"
  [ "$nbytes" = "$PERSIST_BYTES" ] || grammar_die "$ctx: file is $nbytes bytes != $PERSIST_BYTES (MLFKPERSIST7 fixed size; byte-count reconciliation failed — dropped/added/NUL byte)"
  nl="$(grep -c "" "$f")" || grammar_die "$ctx: cannot count lines"
  [ "$nl" = 70 ] || grammar_die "$ctx: $nl lines != 70 (MLFKPERSIST7 is exactly 70 LF lines)"
  L="$(sed -n 1p "$f")"; [ "$L" = "MLFKPERSIST7" ] || grammar_die "$ctx: line 1 is not the exact header ('$L')"
  L="$(sed -n 2p "$f")"; [[ "$L" =~ ^turbo\ [01]$ ]] || grammar_die "$ctx: line 2 turbo grammar ('$L')"
  L="$(sed -n 3p "$f")"; [[ "$L" =~ ^lcancel\ [0-2]$ ]] || grammar_die "$ctx: line 3 lcancel grammar ('$L')"
  L="$(sed -n 4p "$f")"; [[ "$L" =~ ^tapjump\ [01]\ [01]\ [01]\ [01]$ ]] || grammar_die "$ctx: line 4 tapjump grammar ('$L')"
  # line 5: ctlstyle (MLFKPERSIST2+, fix_plan A4). Domain is the CtlStyle
  # enum {0 normal, 1 box, 2 natural} — anchored, single digit, no
  # permissive scan.
  L="$(sed -n 5p "$f")"; [[ "$L" =~ ^ctlstyle\ [0-2]$ ]] || grammar_die "$ctx: line 5 ctlstyle grammar ('$L')"
  # line 6: modonr (MLFKPERSIST3, owner ruling 2026-07-29) — 0 = the
  # ratified Mod-on-L arrangement, 1 = swapped.
  L="$(sed -n 6p "$f")"; [[ "$L" =~ ^modonr\ [01]$ ]] || grammar_die "$ctx: line 6 modonr grammar ('$L')"
  ln=7
  for c in 0 1 2 3 4; do
    for s in 0 1 2 3 4 5 6 7 8 9; do
      L="$(sed -n "${ln}p" "$f")"
      [[ "$L" =~ ^rec\ ([0-4])\ ([0-9])\ ([0-9a-f]{16})$ ]] \
        || grammar_die "$ctx: line $ln is not a rec row ('$L')"
      [ "${BASH_REMATCH[1]}" = "$c" ] && [ "${BASH_REMATCH[2]}" = "$s" ] \
        || grammar_die "$ctx: line $ln rec (char,stage)=(${BASH_REMATCH[1]},${BASH_REMATCH[2]}) != canonical ($c,$s) — order/progression violated"
      bits="${BASH_REMATCH[3]}"
      if [ "$bits" = bff0000000000000 ]; then
        : # the -1.0 no-record sentinel
      elif hex_lt "$bits" 40b7700000000000; then
        : # finite non-negative in [0,6000): unsigned-hex < the 6000.0 cap
      else
        grammar_die "$ctx: line $ln rec bits $bits out of domain (not -1.0 and not finite [0,6000))"
      fi
      ln=$((ln + 1))
    done
  done
  # lines 57-63: the v4 options block (MENU-SPEC §3/§4), APPENDED after the
  # rec rows so every older version stays a strict prefix through them.
  # Four 0/1 flags then three hex16 doubles, each with its own domain.
  for k in flash walljump blastzone dustless; do
    L="$(sed -n "${ln}p" "$f")"
    [[ "$L" =~ ^${k}\ [01]$ ]] || grammar_die "$ctx: line $ln is not '$k [01]' ('$L')"
    ln=$((ln + 1))
  done
  # phantomThreshold is ON THE CHECKSUM SURFACE (hitDetection.js:335/337/348)
  # — an out-of-domain value silently flips physics (the qjs getCookie
  # class), so its bits are range-checked, not merely shape-checked.
  L="$(sed -n "${ln}p" "$f")"
  [[ "$L" =~ ^phantom\ ([0-9a-f]{16})$ ]] || grammar_die "$ctx: line $ln is not 'phantom <hex16>' ('$L')"
  # INCLUSIVE at 1000.0 (review-r12 MINOR): foh_persist.c's fp_in_range uses
  # `d <= hi`, so a product-written file at exactly the cap is VALID and this
  # independent whitelist must not reject what the product accepts. An
  # off-by-one here is a gate that fails on a legitimate save.
  b="${BASH_REMATCH[1]}"
  [ "$b" = 408f400000000000 ] || hex_lt "$b" 408f400000000000 \
    || grammar_die "$ctx: line $ln phantom bits $b out of domain (want finite non-negative <= 1000.0)"
  ln=$((ln + 1))
  # the two master levels are clamped to [0,1] by the audio screen
  # (audiomenu.js:104-112), so anything above the 1.0 pattern is corrupt.
  for k in soundslevel musiclevel; do
    L="$(sed -n "${ln}p" "$f")"
    [[ "$L" =~ ^${k}\ ([0-9a-f]{16})$ ]] || grammar_die "$ctx: line $ln is not '$k <hex16>' ('$L')"
    b="${BASH_REMATCH[1]}"
    [ "$b" = 3ff0000000000000 ] || hex_lt "$b" 3ff0000000000000 \
      || grammar_die "$ctx: line $ln $k bits $b out of domain (want finite non-negative <= 1.0)"
    ln=$((ln + 1))
  done
  [ "$ln" = 64 ] || grammar_die "$ctx: v4 block ended at line $ln, want 64 (line accounting drifted)"
  # lines 64-67: the v5 bind block (fix_plan A31), one row per port,
  # port-major, APPENDED after the v4 block for the same prefix reason. A
  # row that is not a PERMUTATION of 0..7 would leave an action on no button
  # at all — the player stranded mid-match with no PAUSE — so it is rejected
  # here INDEPENDENTLY of the C loader, exactly as the rec-row domain is.
  local bk bslot bseen
  for bk in 0 1 2 3; do
    L="$(sed -n "${ln}p" "$f")"
    [[ "$L" =~ ^bind\ ([0-3])\ ([0-7])\ ([0-7])\ ([0-7])\ ([0-7])\ ([0-7])\ ([0-7])\ ([0-7])\ ([0-7])$ ]] \
      || grammar_die "$ctx: line $ln is not a bind row ('$L')"
    [ "${BASH_REMATCH[1]}" = "$bk" ] \
      || grammar_die "$ctx: line $ln bind port=${BASH_REMATCH[1]} != canonical $bk — order/progression violated"
    bseen=""
    for bslot in "${BASH_REMATCH[@]:2:8}"; do
      case "$bseen" in
        *",$bslot,"*) grammar_die "$ctx: line $ln repeats slot $bslot — not a permutation, so an action would be on no button at all";;
      esac
      bseen="$bseen,$bslot,"
    done
    ln=$((ln + 1))
  done
  [ "$ln" = 68 ] || grammar_die "$ctx: v5 block ended at line $ln, want 68 (line accounting drifted)"
  # v6 block (fix_plan A49, DEVIATION D45): the CSS selection, one row,
  # port-major, each a roster id 0..4. Positional like every row above it —
  # a `sel` row that parsed but sat in the wrong place would be a file this
  # loader accepts and the previous one does not.
  L="$(sed -n 68p "$f")"
  [[ "$L" =~ ^sel\ ([0-4])\ ([0-4])\ ([0-4])\ ([0-4])$ ]] \
    || grammar_die "$ctx: line 68 is not the v6 sel row ('$L')"
  # v7 block (fix_plan A26, DEVIATION D53): the hibernate resume screen, one
  # row, TWO digits. The domain here is deliberately NARROWER than "a
  # FohScreen": it is exactly the screens foh_persist_resume_target() maps to
  # themselves, restated INDEPENDENTLY of the C loader the way every rec/bind
  # domain above is. 00 = FOH_STARTUP = nothing armed; the rest are the
  # resumable screens (title 01, menu-top 02, menu-options 03, menu-controls
  # 05, css 06, opt-gameplay 08, opt-audio 09, ctrl-pad 10, ctrl-key 11,
  # tss 14). NOT here, and that is the point: 04 menu-battle, 07 sss,
  # 12 credits, 13 match, 15 tmatch — screens a resume must never restore.
  L="$(sed -n 69p "$f")"
  [[ "$L" =~ ^resume\ (00|01|02|03|05|06|08|09|10|11|14)$ ]] \
    || grammar_die "$ctx: line 69 is not a v7 resume row in the resumable
  domain ('$L') — a screen the driver would refuse to restore must never
  reach the file either"
  ln=70
  L="$(sed -n 70p "$f")"
  [[ "$L" =~ ^SUM\ ([0-9a-f]{64})$ ]] || grammar_die "$ctx: line 70 is not the SUM line ('$L')"
  sum="${BASH_REMATCH[1]}"
  # review-102 M-b: validate the COMPLETE recomputed shasum line grammar
  # (`<64hex>  -` on stdin), never a `cut -d' ' -f1` first-field scrape —
  # a truncated line with a plausible first field is corruption.
  local sumline
  # 69, not 67: the body is EVERY line before the SUM. A49 added the `sel`
  # row and moved the SUM's own index from 68 to 69 but left this recompute
  # at 67, so from that commit until this one the whitelist hashed two lines
  # short and would have rejected every genuine file. Derived from the SUM's
  # index above rather than retyped, so the next bump cannot repeat it.
  sumline="$(head -n $((ln - 1)) "$f" | shasum -a 256)" || fail "$ctx: shasum failed"
  [[ "$sumline" =~ ^([0-9a-f]{64})\ \ -$ ]] || grammar_die "$ctx: recomputed shasum line is not exactly '<64hex>  -' ('$sumline')"
  want="${BASH_REMATCH[1]}"
  [ "$want" = "$sum" ] || grammar_die "$ctx: SUM seal $sum != recomputed $want (torn/corrupt file passed as evidence)"
}

HP=$BUILD/host
rm -rf "$HP"
mkdir -p "$HP"
# p01 x2 byte-stability (fresh dirs)
run_host p01-persist-edit "$HP/p01a" "$PWD/$HP/p01a-persist"
run_host p01-persist-edit "$HP/p01b" "$PWD/$HP/p01b-persist"
made "$HP/p01a-persist/mlfk-persist.dat" "$HP/p01b-persist/mlfk-persist.dat"
cmp "$HP/p01a-persist/mlfk-persist.dat" "$HP/p01b-persist/mlfk-persist.dat" \
  || fail "p01 persist file not byte-stable across two fresh host runs"
cmp "$HP/p01a/trace.txt" "$HP/p01b/trace.txt" \
  || fail "p01 traces not byte-stable x2"
[ "$(count_xl "$HP/p01a/log.txt" "foh_persist: reset cause=missing")" = 1 ] \
  || grammar_die "p01 host: expected exactly one missing-reset line (the loud first boot)"
[ "$(count_xl "$HP/p01a/log.txt" "foh_persist: saved")" = 1 ] \
  || grammar_die "p01 host: expected exactly one saved line (the options B-exit)"
[ "$(count_xl "$HP/p01a/log.txt" "foh_persist: loaded")" = 0 ] \
  || grammar_die "p01 host: unexpected loaded line on a fresh dir"
# the settings-edit witness in the structural trace + the file values
for ln in "S 410 turbo 1" "S 420 lcancel 1" "S 425 lcancel 2" \
          "S 440 tapjump2 1" "T 450 options-gameplay menu-options b"; do
  [ "$(count_xl "$HP/p01a/trace.txt" "$ln")" = 1 ] \
    || fail "p01 host trace: missing exact line '$ln'"
done
FILE_P01=$HP/file-p01-want.dat
cp "$HP/p01a-persist/mlfk-persist.dat" "$FILE_P01"
verify_persist_file "$FILE_P01" "p01 reference"
[ "$(count_xl "$FILE_P01" "turbo 1")" = 1 ] || fail "p01 file: turbo != 1"
[ "$(count_xl "$FILE_P01" "lcancel 2")" = 1 ] || fail "p01 file: lcancel != 2"
[ "$(count_xl "$FILE_P01" "tapjump 0 1 0 0")" = 1 ] || fail "p01 file: tapjump != 0 1 0 0"
[ "$(count_e "$FILE_P01" '^rec [0-4] [0-9] bff0000000000000$')" = 50 ] \
  || fail "p01 file: records not all fresh (-1)"
echo "   p01 host reference OK (x2 byte-stable, values exact, SUM sealed)"

# record arm (the REAL tp_finish_game -> hook -> chokepoint chain)
rm -f "$HP/arm.log"
MLFK_PERSIST_DIR="$PWD/$HP/p01a-persist" \
"$BUILD/foh_dev_headless" --tooth-persist-finish "$REC_CHAR" "$REC_TSTAGE" \
  "$REC_BITS" 2> "$HP/arm.log" || { cat "$HP/arm.log" >&2; fail "host record arm failed"; }
made "$HP/arm.log"
[ "$(count_xl "$HP/arm.log" "foh_persist: loaded")" = 1 ] || grammar_die "arm: loaded line"
[ "$(count_xl "$HP/arm.log" "foh_persist: record char=$REC_CHAR tstage=$REC_TSTAGE improved=1")" = 1 ] \
  || grammar_die "arm: record improved=1 line"
[ "$(count_xl "$HP/arm.log" "foh_persist: saved")" = 1 ] || grammar_die "arm: saved line"
[ "$(count_xl "$HP/arm.log" "foh_dev tfinish: complete=1 frame=0")" = 1 ] \
  || grammar_die "arm: tfinish witness line"
FILE_REC=$HP/file-rec-want.dat
cp "$HP/p01a-persist/mlfk-persist.dat" "$FILE_REC"
verify_persist_file "$FILE_REC" "post-record reference"
[ "$(count_xl "$FILE_REC" "rec $REC_CHAR $REC_TSTAGE $REC_BITS")" = 1 ] \
  || fail "post-record file: the $REC_BITS record row is missing"
rc=0; cmp -s "$FILE_REC" "$FILE_P01" || rc=$?
[ "$rc" = 1 ] || fail "post-record file identical to pre-record (dead record write; cmp rc $rc)"
echo "   record arm OK (improved=1 through the REAL finish chain; file sealed)"

# T-H7 record regress: a WORSE time must not touch the file
rm -f "$HP/arm2.log"
MLFK_PERSIST_DIR="$PWD/$HP/p01a-persist" \
"$BUILD/foh_dev_headless" --tooth-persist-finish "$REC_CHAR" "$REC_TSTAGE" \
  "$WORSE_BITS" 2> "$HP/arm2.log" || fail "regress arm failed outright"
[ "$(count_xl "$HP/arm2.log" "foh_persist: record char=$REC_CHAR tstage=$REC_TSTAGE improved=0")" = 1 ] \
  || grammar_die "T-H7: expected improved=0"
[ "$(count_xl "$HP/arm2.log" "foh_persist: saved")" = 0 ] \
  || grammar_die "T-H7: a non-improving record SAVED (improve-only semantics broken)"
cmp "$HP/p01a-persist/mlfk-persist.dat" "$FILE_REC" \
  || fail "T-H7: file changed under a non-improving record"
teeth=1
echo "    T-H7 OK: worse time -> improved=0, no save, file byte-identical"

# p02 persisted twin (fed a COPY of the post-record dir) x2 stable
mk_pdir() { # <dst-dir> <src-file|-> : fresh persist dir, optional seed file
  rm -rf "$1"
  mkdir -p "$1"
  if [ "$2" != - ]; then cp "$2" "$1/mlfk-persist.dat"; fi
}
mk_pdir "$HP/twin-persist" "$FILE_REC"
run_host p02-persist-verify "$HP/p02twin" "$PWD/$HP/twin-persist"
mk_pdir "$HP/twin-persist2" "$FILE_REC"
run_host p02-persist-verify "$HP/p02twin2" "$PWD/$HP/twin-persist2"
for s in opt-persisted tss-record; do
  made "$HP/p02twin/shots/$s.ppm" "$HP/p02twin2/shots/$s.ppm"
  cmp "$HP/p02twin/shots/$s.ppm" "$HP/p02twin2/shots/$s.ppm" \
    || fail "p02 twin shot $s not byte-stable x2"
done
[ "$(count_xl "$HP/p02twin/log.txt" "foh_persist: loaded")" = 1 ] \
  || grammar_die "p02 twin: loaded line"
[ "$(count_xl "$HP/p02twin/log.txt" "foh_persist: saved")" = 1 ] \
  || grammar_die "p02 twin: saved line (the B-exit resave)"
cmp "$HP/twin-persist/mlfk-persist.dat" "$FILE_REC" \
  || fail "p02 twin resave is not IDEMPOTENT (load->save must reproduce the same bytes)"
# defaults CONTROL: fresh dir -> the persisted displays MUST differ
mk_pdir "$HP/ctrl-persist" -
run_host p02-persist-verify "$HP/p02ctrl" "$PWD/$HP/ctrl-persist"
[ "$(count_xl "$HP/p02ctrl/log.txt" "foh_persist: reset cause=missing")" = 1 ] \
  || grammar_die "p02 control: missing-reset line"
for s in opt-persisted tss-record; do
  rc=0; cmp -s "$HP/p02twin/shots/$s.ppm" "$HP/p02ctrl/shots/$s.ppm" || rc=$?
  [ "$rc" = 1 ] || fail "p02 $s shot identical between persisted twin and defaults control (the display is NOT load-bearing; cmp rc $rc)"
done
cmp "$HP/p02twin/trace.txt" "$HP/p02ctrl/trace.txt" \
  || fail "p02 structural trace depends on the persisted plane (frozen-flow hermeticity broken — the (c) refutation shape)"
made "$HP/ctrl-persist/mlfk-persist.dat"
FILE_DEFAULTS=$HP/file-defaults-want.dat
cp "$HP/ctrl-persist/mlfk-persist.dat" "$FILE_DEFAULTS"
verify_persist_file "$FILE_DEFAULTS" "defaults-control reference"
[ "$(count_xl "$FILE_DEFAULTS" "turbo 0")" = 1 ] || fail "defaults file: turbo != 0"
echo "   p02 twin + control OK (records/settings displays load-bearing; resave idempotent)"

# L1 (review-100): the PERSONAL BEST display is pinned to a SPEC-derived
# truth, not a self-consistent echo — so the byte-exact shot judges are
# anchored to the format rule independently of the C renderer.
L1_PB="$(derive_pb "$REC_BITS")" || fail "L1: PB derivation failed"
[ "$L1_PB" = "$REC_DISPLAY" ] || fail "L1: spec-derived PB '$L1_PB' != pinned REC_DISPLAY '$REC_DISPLAY' (the display pin drifted from the format rule)"
L1_DEF="$(derive_pb bff0000000000000)" || fail "L1: defaults derivation failed"
[ "$L1_DEF" = "--:--:--" ] || fail "L1: spec-derived defaults display '$L1_DEF' != '--:--:--'"
L1_PERT="$(derive_pb 402e000000000000)" || fail "L1: perturbed derivation failed"
[ "$L1_PERT" != "$REC_DISPLAY" ] || fail "L1: dead-tooth — perturbed bits derived the same display as REC_BITS"
echo "   L1 OK: PERSONAL BEST '$REC_DISPLAY' derived independently from the record bits (spec rule); defaults '--:--:--'; dead-tooth guarded"

# L-b (review-102; RE-TARGETED R5, 2026-07-31): CONNECT the
# independently-derived display string to the SHOT PIXELS.
# decode-pb-glyphs.js reads the FOH 5x7 font tables from foh_font.c AS
# DATA and decodes the PERSONAL BEST region of the shot back into a
# string — NOT the C renderer — so the display is bound to actual pixels,
# not a renderer-vs-renderer echo. Pinned like the judge twin (artifact
# identity, PROCESS §4).
#
# THE REGION MOVED, and this is the ONLY assertion that changed shape.
# When this check was authored (iter 100) the row was ONE kAccent
# text_center line at y=194 carrying all 22 glyphs. The menus lane made
# it a PANEL — foh_render.c render_tselect: x=92, y=160, w=140, h=62 —
# carrying TWO text_in lines in TWO colour classes:
#   label "PERSONAL BEST" at y+22 = 182, scale 1, kDim    (120,120,140)
#   time  "0X:XX.XX"      at y+38 = 198, scale 2, kAccent (255,200,60)
# So the decode is two calls whose composition is the SAME 22-character
# string this leg has always asserted, against the SAME expectations.
# Nothing here got weaker: the decoder's on-test went from an accent
# luminance threshold to EXACT post-565 colour equality, and the two
# classes are now both load-bearing (tooth below). The drift survived a
# lane that updated every committed flow only because this check had
# never been executed (R5, AGENT-LOG 2026-07-31).
PB_WIN=92,140            # the panel's text_in window (x,w)
PB_LBL_Y=182;  PB_LBL_SCALE=1;  PB_LBL_N=13; PB_LBL_RGB=120,120,140
PB_TIME_Y=198; PB_TIME_SCALE=2; PB_TIME_N=8; PB_TIME_RGB=255,200,60
DECODE_SHA=809ea4f6cc361014f75be8034d8fef69fd2a683213c1bc111574dfbbe98a31f9
have="$(rig_host_sha256 "$FOH/decode-pb-glyphs.js")" || exit 1
[ "$have" = "$DECODE_SHA" ] || fail "decode-pb-glyphs.js sha $have != pinned $DECODE_SHA (reviewed pin update in the same commit)"
decode_pb_part() { # <shot.ppm> <y> <scale> <nglyphs> <r,g,b> -> decoded chars
  # `flat-panel` (review-r6-r7 [HIGH]): BOTH PB lines sit inside the
  # target-select info panel, whose fill is a flat (16,4,0) over kBg — no
  # gameplay renders behind them — so the decoder may treat any BRIGHT pixel
  # inside the line rectangle that is not this line's ink as damage. That is
  # the one discrimination the pre-panel accent-threshold decoder had and
  # exact colour equality alone does not: foreign ink dropped into an OFF
  # cell. It is NOT passed by the finish-banner decode in
  # check-device-foh.sh, which is drawn over a live frame where bright
  # background is legitimate.
  node "$FOH/decode-pb-glyphs.js" "$FOH/foh_font.c" "$1" "$2" "$3" "$4" "$PB_WIN" "$5" flat-panel
}
decode_pb_line() { # <shot.ppm> -> the decoded 22-char PERSONAL BEST line
  # `local` on its own line: `local x="$(...)"` reports the DECLARATION's
  # status, so a decoder death would be laundered into success here.
  # The decoder's OWN rc is propagated, never collapsed to 1
  # (review-r6-r3 [LOW]): the teeth below require rc 3 exactly — the
  # no-matching-glyph / mixed-cell / blank-line death — so that a node
  # crash, an unreadable file or a usage error (rc 1/2) can never be
  # credited as "the decoder rejected these pixels".
  local lbl tm rc=0
  lbl="$(decode_pb_part "$1" "$PB_LBL_Y" "$PB_LBL_SCALE" "$PB_LBL_N" "$PB_LBL_RGB")" || rc=$?
  [ "$rc" = 0 ] || return "$rc"
  tm="$(decode_pb_part "$1" "$PB_TIME_Y" "$PB_TIME_SCALE" "$PB_TIME_N" "$PB_TIME_RGB")" || rc=$?
  [ "$rc" = 0 ] || return "$rc"
  printf '%s %s' "$lbl" "$tm"
}
L1_PB_LINE="PERSONAL BEST $L1_PB"    # persisted-twin expectation
L1_DEF_LINE="PERSONAL BEST $L1_DEF"  # defaults-control expectation
dec_twin="$(decode_pb_line "$HP/p02twin/shots/tss-record.ppm")" \
  || fail "L-b: glyph decode of the persisted-twin tss-record shot failed"
[ "$dec_twin" = "$L1_PB_LINE" ] \
  || fail "L-b: persisted-twin shot decodes to '$dec_twin' != derived '$L1_PB_LINE' — the PB display string is NOT the pixels that were rendered"
dec_ctrl="$(decode_pb_line "$HP/p02ctrl/shots/tss-record.ppm")" \
  || fail "L-b: glyph decode of the defaults-control tss-record shot failed"
[ "$dec_ctrl" = "$L1_DEF_LINE" ] \
  || fail "L-b: defaults-control shot decodes to '$dec_ctrl' != '$L1_DEF_LINE'"
[ "$dec_twin" != "$dec_ctrl" ] \
  || fail "L-b: dead-tooth — twin and control shots decoded to the SAME string ('$dec_twin')"
# dead-tooth: a one-pixel-perturbed COPY of the twin shot must NOT decode
# the same string (proves the decoder reads pixels, not a constant).
# The perturbed pixel is the TOP-LEFT pixel of the first lit cell of the
# time line — derived from the same window/scale constants the decoder
# uses, not hunted for by luminance in a hand-typed band (the old band was
# additionally addressing the pre-panel y=194 layout). Top-left and NOT
# centre on purpose (review-r6-r3 [MEDIUM]): at scale 2 the decoder used
# to look only at each cell's centre pixel, so a tooth that perturbs the
# centre would still pass against that weaker sampler. This one perturbs a
# pixel the centre sampler could not see, so it fires only because the
# decoder now reads the WHOLE cell — and it lands as a MIXED cell, which
# is the decoder's rc-3 death.
LBPERT="$HP/lb-pert.ppm"
rm -f "$LBPERT"
node -e '
  const fs=require("fs"); const b=fs.readFileSync(process.argv[1]);
  const [winX,winW]=process.argv[3].split(",").map(Number);
  const y=+process.argv[4], scale=+process.argv[5], n=+process.argv[6];
  const [cr,cg,cb]=process.argv[7].split(",").map(Number);
  const onR=cr&0xf8, onG=cg&0xfc, onB=cb&0xf8; // pack565 -> write_shot_ppm
  let i=2, tok=()=>{while(i<b.length){const c=b[i];if(c===0x23){while(i<b.length&&b[i]!==0x0a)i++;}else if(c===0x20||c===9||c===10||c===13)i++;else break;}let s="";while(i<b.length){const c=b[i];if(c===0x20||c===9||c===10||c===13)break;s+=String.fromCharCode(c);i++;}return s;};
  const w=+tok(),h=+tok(),mx=+tok(); i++;
  const xStart = winX + Math.trunc((winW - (n*6-1)*scale)/2);
  for(let gi=0; gi<n; gi++) for(let r=0;r<7;r++) for(let c=0;c<5;c++){
    // the cell TOP-LEFT, which a centre sampler cannot see at scale >= 2
    const sx = xStart + gi*6*scale + c*scale;
    const sy = y + r*scale;
    const o = i + (sy*w+sx)*3;
    if(b[o]===onR && b[o+1]===onG && b[o+2]===onB){
      b[o]=12; b[o+1]=12; b[o+2]=28; // any non-on value; kBg-ish by intent
      fs.writeFileSync(process.argv[2],b); process.exit(0);
    }
  }
  process.stderr.write("no SAMPLED on-pixel found to perturb\n"); process.exit(9);
' "$HP/p02twin/shots/tss-record.ppm" "$LBPERT" \
  "$PB_WIN" "$PB_TIME_Y" "$PB_TIME_SCALE" "$PB_TIME_N" "$PB_TIME_RGB" \
  || fail "L-b: could not build the perturbed shot"
# rc CASE-SPLIT, not `cmp -s && fail` (review-r6-r2 [LOW]): `&&` treats rc 1
# (genuinely different) and rc >1 (cmp could not read a file) the same, and
# the decoder death below accepts any nonzero too — so an unreadable
# perturbation would be credited as a fired tooth. Exactly 1 is the only
# outcome that says "the perturbation changed bytes".
rc=0; cmp -s "$LBPERT" "$HP/p02twin/shots/tss-record.ppm" || rc=$?
[ "$rc" = 1 ] \
  || fail "L-b: the perturbed shot vs the twin gives cmp rc $rc (want exactly 1 — rc 0 means the perturbation was a no-op and the tooth is dead; rc >1 means cmp could not read one of them)"
rc=0; dec_pert="$(decode_pb_line "$LBPERT")" || rc=$?
[ "$rc" = 3 ] \
  || fail "L-b: dead-tooth — clearing one top-left cell pixel gave decoder rc $rc (want EXACTLY 3, the mixed-cell death). rc 0 means the decoder is not reading those pixels; any other nonzero is an operational failure (crash, unreadable file, usage) being credited as pixel discrimination. Decoded: '$dec_pert'"
teeth=$((teeth + 1))
# COLOUR-CLASS tooth (new with the panel layout): the label is kDim and
# the time is kAccent, and the decode now names which. Reading the LABEL
# row under the ACCENT class must DIE — otherwise the colour argument is
# decorative and a future single-colour layout would decode green while
# asserting nothing about which line it read.
rc=0
dec_wrongcol="$(decode_pb_part "$HP/p02twin/shots/tss-record.ppm" \
  "$PB_LBL_Y" "$PB_LBL_SCALE" "$PB_LBL_N" "$PB_TIME_RGB" 2>/dev/null)" || rc=$?
[ "$rc" = 3 ] \
  || fail "L-b: dead-tooth — reading the kDim label row under the kAccent colour class gave decoder rc $rc (want EXACTLY 3, the blank-line death). rc 0 means the colour parameter is not load-bearing; any other nonzero is an operational failure being credited as rejection. Decoded: '$dec_wrongcol'"
teeth=$((teeth + 1))
# FOREIGN-INK tooth (review-r6-r7 [HIGH]). Exact-colour matching answers "is
# this pixel this line's ink" and says nothing about ink of ANOTHER colour
# landing in a cell that should be background — and the byte-exact
# device-vs-twin shot compare cannot catch that either, because both sides
# run the SAME renderer, so a shared defect is identical on both. The
# `flat-panel` declaration closes it; this tooth proves the declaration is
# load-bearing by planting one bright-but-wrong pixel on a KNOWN-OFF pixel of
# the time line and requiring the decoder to die on it. Without the guard the
# same shot decodes clean (measured, 2026-07-31).
LBINK="$HP/lb-foreign-ink.ppm"
rm -f "$LBINK"
node -e '
  const fs=require("fs"); const b=fs.readFileSync(process.argv[1]);
  const [winX,winW]=process.argv[3].split(",").map(Number);
  const y=+process.argv[4], scale=+process.argv[5], n=+process.argv[6];
  const [cr,cg,cb]=process.argv[7].split(",").map(Number);
  const onR=cr&0xf8, onG=cg&0xfc, onB=cb&0xf8;
  let i=2, tok=()=>{while(i<b.length){const c=b[i];if(c===0x23){while(i<b.length&&b[i]!==0x0a)i++;}else if(c===0x20||c===9||c===10||c===13)i++;else break;}let s="";while(i<b.length){const c=b[i];if(c===0x20||c===9||c===10||c===13)break;s+=String.fromCharCode(c);i++;}return s;};
  const w=+tok(),h=+tok(),mx=+tok(); i++;
  const xStart = winX + Math.trunc((winW - (n*6-1)*scale)/2);
  // the first sampled pixel that is NOT this line ink, i.e. a genuine OFF
  // pixel inside a glyph cell — planting there cannot be mistaken for
  // removing ink (that is the other tooth).
  for(let gi=0; gi<n; gi++) for(let r=0;r<7;r++) for(let c=0;c<5;c++)
   for(let dy=0;dy<scale;dy++) for(let dx=0;dx<scale;dx++){
    const sx = xStart + gi*6*scale + c*scale + dx, sy = y + r*scale + dy;
    const o = i + (sy*w+sx)*3;
    if(!(b[o]===onR && b[o+1]===onG && b[o+2]===onB)){
      b[o]=200; b[o+1]=100; b[o+2]=112; // bright + warm: the foreign-ink class
      fs.writeFileSync(process.argv[2],b); process.exit(0);
    }
   }
  process.stderr.write("no OFF pixel found inside a glyph cell\n"); process.exit(9);
' "$HP/p02twin/shots/tss-record.ppm" "$LBINK" \
  "$PB_WIN" "$PB_TIME_Y" "$PB_TIME_SCALE" "$PB_TIME_N" "$PB_TIME_RGB" \
  || fail "L-b: could not build the foreign-ink shot"
rc=0; cmp -s "$LBINK" "$HP/p02twin/shots/tss-record.ppm" || rc=$?
[ "$rc" = 1 ] \
  || fail "L-b: the foreign-ink shot vs the twin gives cmp rc $rc (want exactly 1)"
rc=0
dec_ink="$(decode_pb_part "$LBINK" "$PB_TIME_Y" "$PB_TIME_SCALE" "$PB_TIME_N" "$PB_TIME_RGB" 2>/dev/null)" || rc=$?
[ "$rc" = 3 ] \
  || fail "L-b: dead-tooth — one bright foreign pixel planted on an OFF pixel of the time line gave decoder rc $rc (want EXACTLY 3, the foreign-ink death). rc 0 means the flat-panel declaration is not load-bearing and added ink is invisible to this check. Decoded: '$dec_ink'"
teeth=$((teeth + 1))
echo "   L-b OK: twin shot decodes to '$dec_twin' == derived; control '$dec_ctrl'; distinct; perturb-tooth + colour-class tooth + foreign-ink tooth fired"

# M1 witness (review-100 PRODUCT BUG: same-process stale PB render). The
# p02-persist-verify flow over a dir seeded with the PRE-record file
# (FILE_P01: the edited settings, records all -1), with an improving
# record fired MID-FLOW at frame 100 through --tooth-finish-at (the REAL
# tp_finish_game -> hook -> chokepoint chain). Under the fix the
# chokepoint refreshes the bound FohState at the record write, so the
# SAME-PROCESS frame-440 tss-record shot renders the NEW record and is
# BYTE-IDENTICAL to the persisted-twin shot (which BOOTS with the record
# already on disk). Under the shipped bug that shot shows the stale
# "--:--:--" (proven in this iteration's smoke: an unfixed build's shot
# DIFFERS) — the tooth discriminates by construction.
mk_pdir "$HP/m1-persist" "$FILE_P01"
run_host p02-persist-verify "$HP/p02m1" "$PWD/$HP/m1-persist" \
  --tooth-finish-at 100 "$REC_CHAR" "$REC_TSTAGE" "$REC_BITS"
made "$HP/p02m1/shots/tss-record.ppm"
cmp "$HP/p02m1/shots/tss-record.ppm" "$HP/p02twin/shots/tss-record.ppm" \
  || fail "M1: the same-process improve->return-to-select tss-record shot != the persisted twin — the stale-PB product bug is NOT fixed (bound-FohState refresh refuted; STOP per the frozen refutation shape)"
[ "$(count_xl "$HP/p02m1/log.txt" "foh_persist: record char=$REC_CHAR tstage=$REC_TSTAGE improved=1")" = 1 ] \
  || grammar_die "M1 witness: expected exactly one improved=1 record line"
[ "$(count_xl "$HP/p02m1/log.txt" "foh_persist: saved")" = 2 ] \
  || grammar_die "M1 witness: expected exactly two saved lines (the mid-flow improve-save + the B-exit resave)"
cmp "$HP/m1-persist/mlfk-persist.dat" "$FILE_REC" \
  || fail "M1 witness: post-leg file != the host post-record reference (the record write path diverged)"
cmp "$HP/p02m1/trace.txt" "$HP/p02twin/trace.txt" \
  || fail "M1 witness: the mid-flow finish perturbed the structural trace (hermeticity broken)"
teeth=$((teeth + 1))
echo "    M1 witness OK: same-process PB refresh renders the new record (tss shot == persisted twin; file/trace hermetic)"

# --- [4] host teeth (COPIES; the probe flow boots the loader) -----------------
echo "== [4/10] host teeth =="
tooth_boot() { # <name> <persist-dir> <want-line>
  local nm="$1" pd="$2" wantln="$3" nterm
  run_host p00-persist-probe "$HP/tooth-$nm" "$pd"
  [ "$(count_xl "$HP/tooth-$nm/log.txt" "$wantln")" = 1 ] \
    || grammar_die "$nm: expected exactly one '$wantln' line"
  # review-ctl r4: counting only the REQUESTED event let a broken loader
  # pass while ALSO emitting a contradictory terminal event (e.g. both a
  # grammar reset AND `loaded`). Exactly one TERMINAL event per boot, and
  # it must be the expected one. `migrated from=1` is a prelude, not a
  # terminal event, so it is deliberately not counted here.
  nterm="$(grep -c '^foh_persist: \(loaded\|reset cause=\)' "$HP/tooth-$nm/log.txt")" || true
  [ "$nterm" = 1 ] \
    || grammar_die "$nm: expected exactly ONE terminal loaded/reset event, saw $nterm"
}
# T-H1 corrupt-sum: flip one rec nibble, SUM left stale
mk_pdir "$HP/th1" "$FILE_REC"
sed "s/^rec $REC_CHAR $REC_TSTAGE $REC_BITS\$/rec $REC_CHAR $REC_TSTAGE 402e000000000000/" \
  "$FILE_REC" > "$HP/th1/mlfk-persist.dat"
rc=0; cmp -s "$HP/th1/mlfk-persist.dat" "$FILE_REC" || rc=$?
[ "$rc" = 1 ] || fail "T-H1: corrupt variant is a no-op (dead tooth)"
tooth_boot h1 "$PWD/$HP/th1" "foh_persist: reset cause=corrupt detail=sum"
teeth=$((teeth + 1))
echo "    T-H1 OK: nibble-flipped rec dies on the SUM seal (loud reset)"
# T-H2 unsupported-version bump WITH a recomputed (valid) SUM.
# review-ctl r1 / the 2026-07-29 v3 bump / the v4 menus bump: v1, v2 AND
# v3 were legitimate MIGRATIONS rather than resets, and since fix_plan
# A31's v5 bump so is v4, since A49's v6 bump so is v5, and since A26's v7
# bump so is v6 — v7 is the CURRENT version, so the unsupported-version tooth
# moves to v8, a version this build cannot know. (It has moved with every bump; leaving it behind
# turns this tooth into a no-op that asserts the CURRENT format resets.)
mk_pdir "$HP/th2" -
{ printf 'MLFKPERSIST8\n'; tail -n +2 "$FILE_REC" | head -n 68; } > "$HP/th2/body"
{ cat "$HP/th2/body"; printf 'SUM %s\n' "$(shasum -a 256 "$HP/th2/body" | cut -d' ' -f1)"; } \
  > "$HP/th2/mlfk-persist.dat"
rm -f "$HP/th2/body"
tooth_boot h2 "$PWD/$HP/th2" "foh_persist: reset cause=version"
teeth=$((teeth + 1))
echo "    T-H2 OK: unsupported version (checksum-valid) resets loudly"
# T-H3 domain: NaN record bits with a recomputed SUM
mk_pdir "$HP/th3" -
sed "s/^rec $REC_CHAR $REC_TSTAGE $REC_BITS\$/rec $REC_CHAR $REC_TSTAGE 7ff8000000000000/" \
  "$FILE_REC" | head -n 69 > "$HP/th3/body"
{ cat "$HP/th3/body"; printf 'SUM %s\n' "$(shasum -a 256 "$HP/th3/body" | cut -d' ' -f1)"; } \
  > "$HP/th3/mlfk-persist.dat"
rm -f "$HP/th3/body"
tooth_boot h3 "$PWD/$HP/th3" "foh_persist: reset cause=corrupt detail=domain"
teeth=$((teeth + 1))
echo "    T-H3 OK: NaN record bits die on the domain whitelist"
# T-H4 truncation
mk_pdir "$HP/th4" -
head -c 900 "$FILE_REC" > "$HP/th4/mlfk-persist.dat"
tooth_boot h4 "$PWD/$HP/th4" "foh_persist: reset cause=corrupt detail=truncated"
teeth=$((teeth + 1))
echo "    T-H4 OK: truncated file resets loudly"
# T-H5 torn tmp beside a valid file -> loaded intact (rename is the
# only publish; a crashed writer's tmp is inert)
mk_pdir "$HP/th5" "$FILE_REC"
printf 'GARBAGE-TORN-WRITE' > "$HP/th5/mlfk-persist.tmp"
tooth_boot h5 "$PWD/$HP/th5" "foh_persist: loaded"
cmp "$HP/th5/mlfk-persist.dat" "$FILE_REC" || fail "T-H5: the real file changed under a torn tmp"
teeth=$((teeth + 1))
echo "    T-H5 OK: torn .tmp beside the real file is inert (loaded, bytes intact)"
# T-H6 read-only dir: an improving save must die LOUD, file untouched
mk_pdir "$HP/th6" "$FILE_REC"
chmod -w "$HP/th6"
rc=0
MLFK_PERSIST_DIR="$PWD/$HP/th6" \
"$BUILD/foh_dev_headless" --tooth-persist-finish "$REC_CHAR" "$REC_TSTAGE" \
  4020000000000000 2> "$HP/th6.log" || rc=$?
chmod +w "$HP/th6"
[ "$rc" != 0 ] || fail "T-H6: the save SUCCEEDED into a read-only dir"
c="$(grep -cF "foh_persist: save failed" "$HP/th6.log")" || true
[ "$c" = 1 ] || grammar_die "T-H6: loud save-failure message missing"
cmp "$HP/th6/mlfk-persist.dat" "$FILE_REC" \
  || fail "T-H6: the real file changed under a FAILED save (rename-atomicity broken)"
teeth=$((teeth + 1))
echo "    T-H6 OK: failed save dies loud with the real file byte-unchanged"
# T-H8 (review-100 M3): dir-durability degraded token. chmod u=wx on the
# persist dir keeps the tmp write + rename publish alive (search+write)
# but makes open(dir, O_RDONLY) fail — an improving save must then emit
# the DISTINCT loud token saved-nodirsync (NEVER plain saved), with the
# published bytes still byte-exact.
mk_pdir "$HP/th8" "$FILE_P01"
chmod u=wx "$HP/th8"
rc=0
MLFK_PERSIST_DIR="$PWD/$HP/th8" \
"$BUILD/foh_dev_headless" --tooth-persist-finish "$REC_CHAR" "$REC_TSTAGE" \
  "$REC_BITS" 2> "$HP/th8.log" || rc=$?
chmod u=rwx "$HP/th8"
[ "$rc" = 0 ] || { cat "$HP/th8.log" >&2; fail "T-H8: the save did not succeed under u=wx (the tmp/rename publish must stay alive)"; }
[ "$(count_xl "$HP/th8.log" "foh_persist: saved-nodirsync")" = 1 ] \
  || grammar_die "T-H8: expected exactly one degraded 'saved-nodirsync' token"
[ "$(count_xl "$HP/th8.log" "foh_persist: saved")" = 0 ] \
  || grammar_die "T-H8: a plain 'saved' appeared when the dir-durability fsync could not run (degraded save masked)"
cmp "$HP/th8/mlfk-persist.dat" "$FILE_REC" \
  || fail "T-H8: the published file bytes are not exact under the degraded save"
teeth=$((teeth + 1))
echo "    T-H8 OK: dir-open failure -> loud saved-nodirsync (never plain saved), bytes exact"
# T-H9 (review-ctl r2, strengthened r3): a GENUINE MLFKPERSIST1 file
# MIGRATES — and the migration is proven BY BYTES, not by log lines. A
# reset (or a silent field/record drop) here would destroy every
# target-test personal best on an upgrading device, so this is a
# data-loss tooth. Observing only `migrated`+`loaded` would still pass
# for a loader that discarded every setting and record (review-ctl r3),
# hence the byte-for-byte comparison below.
#
# Fixture: the settings lines of a real v2 file, but with ALL 50 record
# slots SEEDED with distinct non-default values (review-ctl r4). Using
# $FILE_P01's records directly would be a dead tooth: those are all the
# -1 default, so a loader that discarded every record would start from
# the same state and still produce the expected bytes. Seeded records
# make loss detectable — 49 slots must survive untouched while one
# improves.
th9_rows() { # <bits for the target slot> -> the 50 canonical rec rows
  local tb="$1" i=0 c s bits
  for c in 0 1 2 3 4; do
    for s in 0 1 2 3 4 5 6 7 8 9; do
      if [ "$c" = "$REC_CHAR" ] && [ "$s" = "$REC_TSTAGE" ]; then
        bits="$tb"
      else
        bits="$(printf '4031%012x' "$i")" # ~17s: finite, in [0,6000)
      fi
      printf 'rec %d %d %s\n' "$c" "$s" "$bits"
      i=$((i + 1))
    done
  done
}
# The v4 options block AS foh_persist_defaults() writes it (MENU-SPEC §3/§4).
# Bit patterns are the authored defaults: 0.01 / 0.5 / 0.3. Emitted from ONE
# place so an expectation and a fixture can never disagree.
v4_defaults() {
  printf 'flash 0\nwalljump 0\nblastzone 0\ndustless 0\n'
  printf 'phantom 3f847ae147ae147b\n'
  printf 'soundslevel 3fe0000000000000\n'
  printf 'musiclevel 3fd3333333333333\n'
}
# fix_plan A31: the four v5 bind rows at their fresh-install value, the
# IDENTITY. Every migration fills them with exactly this, because no older
# file ever carried an opinion about the bindings — a pre-A31 build had no
# rebinder at all, so the identity IS the mapping that device already had.
v5_defaults() {
  printf 'bind 0 0 1 2 3 4 5 6 7\n'
  printf 'bind 1 0 1 2 3 4 5 6 7\n'
  printf 'bind 2 0 1 2 3 4 5 6 7\n'
  printf 'bind 3 0 1 2 3 4 5 6 7\n'
}
# fix_plan A49 (DEVIATION D45): the v6 selection row at its fresh-install
# value, MARTH on every port. Every migration fills it with exactly this,
# because no v1..v5 file ever carried an opinion about characters — those
# builds persisted no CSS state at all, so marth IS the selection that
# device booted its CSS with.
v6_defaults() {
  printf 'sel 0 0 0 0\n'
}
# fix_plan A26 (DEVIATION D53): the v7 resume row at its fresh-install value,
# FOH_STARTUP = NOTHING ARMED. Every migration fills it with exactly this,
# because no v1..v6 file ever carried an opinion about where the player was —
# those builds could not record a screen, so "do not resume" is the only thing
# such a file can honestly say. Inventing a screen here would be the
# "restores the wrong screen" defect the row exists to avoid.
v7_defaults() {
  printf 'resume 00\n'
}
mk_pdir "$HP/th9" -
{ printf 'MLFKPERSIST1\n'; sed -n '2,4p' "$FILE_P01"; th9_rows "$WORSE_BITS"; } \
  > "$HP/th9/body"
{ cat "$HP/th9/body"; printf 'SUM %s\n' "$(shasum -a 256 "$HP/th9/body" | cut -d' ' -f1)"; } \
  > "$HP/th9/mlfk-persist.dat"
rm -f "$HP/th9/body"
[ "$(grep -c "" "$HP/th9/mlfk-persist.dat")" = 55 ] \
  || fail "T-H9: the v1 fixture is not 55 lines (fixture construction broken — dead tooth)"
[ "$(grep -c '^rec .* 4031' "$HP/th9/mlfk-persist.dat")" = 49 ] \
  || fail "T-H9: expected 49 seeded non-target records in the fixture (dead tooth)"
# EXPECTED post-migration save, built INDEPENDENTLY of the loader: the
# same settings, the SAME 49 seeded records untouched, the target slot
# improved to $REC_BITS, and ctlstyle 1 — because the migration must
# carry the ratified BOX mapping forward rather than re-map to the
# fresh-install NATURAL.
{ printf 'MLFKPERSIST7\n'; sed -n '2,4p' "$FILE_P01"; printf 'ctlstyle 1\n';
  printf 'modonr 0\n'; th9_rows "$REC_BITS"; v4_defaults; v5_defaults;
  v6_defaults; v7_defaults; } \
  > "$HP/th9.expect.body"
{ cat "$HP/th9.expect.body"; printf 'SUM %s\n' "$(shasum -a 256 "$HP/th9.expect.body" | cut -d' ' -f1)"; } \
  > "$HP/th9.expect.dat"
rm -f "$HP/th9.expect.body"
rc=0; cmp -s "$HP/th9/mlfk-persist.dat" "$HP/th9.expect.dat" || rc=$?
[ "$rc" = 1 ] || fail "T-H9: fixture and expectation are already identical (dead tooth)"
rc=0
MLFK_PERSIST_DIR="$PWD/$HP/th9" \
"$BUILD/foh_dev_headless" --tooth-persist-finish "$REC_CHAR" "$REC_TSTAGE" \
  "$REC_BITS" 2> "$HP/th9.log" || rc=$?
[ "$rc" = 0 ] || { cat "$HP/th9.log" >&2; fail "T-H9: the migrate+improve+save run failed"; }
[ "$(count_xl "$HP/th9.log" "foh_persist: migrated from=1")" = 1 ] \
  || grammar_die "T-H9: expected exactly one 'foh_persist: migrated from=1'"
[ "$(count_xl "$HP/th9.log" "foh_persist: loaded")" = 1 ] \
  || grammar_die "T-H9: a migrated v1 file must also report exactly one 'loaded'"
c="$(grep -c '^foh_persist: reset cause=' "$HP/th9.log")" || true
[ "$c" = 0 ] \
  || grammar_die "T-H9: a reset occurred while migrating a VALID v1 file (PB data-loss regression)"
verify_persist_file "$HP/th9/mlfk-persist.dat" "T-H9 migrated"
cmp "$HP/th9/mlfk-persist.dat" "$HP/th9.expect.dat" \
  || fail "T-H9: the migrated+saved file != the independently built v5 (settings, the 49 untouched target records, the improved slot, the carried-forward BOX style, or the default-filled v4 options block were not preserved) — the PB data-loss regression"
teeth=$((teeth + 1))
echo "    T-H9 OK: genuine v1 migrates byte-for-byte (settings + 49 seeded records intact + 1 improved, style carried forward as BOX, no reset)"
# T-H10 a v1 file that DOES carry the newer lines is not a v1 file: the
# migration arm skips them, so the rec-row parser meets ctlstyle and the
# file must die on grammar. (Guards the migration arms against becoming a
# permissive "skip whatever is there" path.)
mk_pdir "$HP/th10" -
{ printf 'MLFKPERSIST1\n'; sed -n '2,56p' "$FILE_REC"; } > "$HP/th10/body"
{ cat "$HP/th10/body"; printf 'SUM %s\n' "$(shasum -a 256 "$HP/th10/body" | cut -d' ' -f1)"; } \
  > "$HP/th10/mlfk-persist.dat"
rm -f "$HP/th10/body"
tooth_boot h10 "$PWD/$HP/th10" "foh_persist: reset cause=corrupt detail=grammar"
teeth=$((teeth + 1))
echo "    T-H10 OK: v1 header WITH the newer lines dies on grammar"
# T-H11 the mirror: a v3 file MISSING its ctlstyle line must die on
# grammar too (the version arms must not be interchangeable).
mk_pdir "$HP/th11" -
{ printf 'MLFKPERSIST3\n'; sed -n '2,4p;6,56p' "$FILE_REC"; } > "$HP/th11/body"
{ cat "$HP/th11/body"; printf 'SUM %s\n' "$(shasum -a 256 "$HP/th11/body" | cut -d' ' -f1)"; } \
  > "$HP/th11/mlfk-persist.dat"
rm -f "$HP/th11/body"
tooth_boot h11 "$PWD/$HP/th11" "foh_persist: reset cause=corrupt detail=grammar"
teeth=$((teeth + 1))
echo "    T-H11 OK: v3 header WITHOUT a ctlstyle line dies on grammar"
# T-H12 (owner ruling 2026-07-29): a v3 file MISSING its modonr line must
# die on grammar — the new field is not optional in the current version.
mk_pdir "$HP/th12" -
{ printf 'MLFKPERSIST3\n'; sed -n '2,5p;7,56p' "$FILE_REC"; } > "$HP/th12/body"
{ cat "$HP/th12/body"; printf 'SUM %s\n' "$(shasum -a 256 "$HP/th12/body" | cut -d' ' -f1)"; } \
  > "$HP/th12/mlfk-persist.dat"
rm -f "$HP/th12/body"
tooth_boot h12 "$PWD/$HP/th12" "foh_persist: reset cause=corrupt detail=grammar"
teeth=$((teeth + 1))
echo "    T-H12 OK: v3 header WITHOUT a modonr line dies on grammar"
# T-H13 (owner ruling 2026-07-29): a GENUINE v2 file MIGRATES to v3 by
# BYTES — its ctlstyle carries over UNCHANGED (the enum numbers are
# frozen) and modonr takes the ratified 0. Same data-loss shape as T-H9:
# settings + 49 seeded records survive, one improves.
mk_pdir "$HP/th13" -
{ printf 'MLFKPERSIST2\n'; sed -n '2,4p' "$FILE_P01"; printf 'ctlstyle 1\n';
  th9_rows "$WORSE_BITS"; } > "$HP/th13/body"
{ cat "$HP/th13/body"; printf 'SUM %s\n' "$(shasum -a 256 "$HP/th13/body" | cut -d' ' -f1)"; } \
  > "$HP/th13/mlfk-persist.dat"
rm -f "$HP/th13/body"
[ "$(grep -c "" "$HP/th13/mlfk-persist.dat")" = 56 ] \
  || fail "T-H13: the v2 fixture is not 56 lines (fixture construction broken — dead tooth)"
# expectation: identical to T-H9's (ctlstyle 1 preserved, modonr 0 filled)
rc=0
MLFK_PERSIST_DIR="$PWD/$HP/th13" \
"$BUILD/foh_dev_headless" --tooth-persist-finish "$REC_CHAR" "$REC_TSTAGE" \
  "$REC_BITS" 2> "$HP/th13.log" || rc=$?
[ "$rc" = 0 ] || { cat "$HP/th13.log" >&2; fail "T-H13: the v2 migrate+improve+save run failed"; }
[ "$(count_xl "$HP/th13.log" "foh_persist: migrated from=2")" = 1 ] \
  || grammar_die "T-H13: expected exactly one 'foh_persist: migrated from=2'"
[ "$(count_xl "$HP/th13.log" "foh_persist: loaded")" = 1 ] \
  || grammar_die "T-H13: a migrated v2 file must also report exactly one 'loaded'"
c="$(grep -c '^foh_persist: reset cause=' "$HP/th13.log")" || true
[ "$c" = 0 ] \
  || grammar_die "T-H13: a reset occurred while migrating a VALID v2 file (PB data-loss regression)"
verify_persist_file "$HP/th13/mlfk-persist.dat" "T-H13 migrated"
cmp "$HP/th13/mlfk-persist.dat" "$HP/th9.expect.dat" \
  || fail "T-H13: the migrated v2 file != the independently built v5 (settings, the 49 untouched records, the improved slot, the PRESERVED ctlstyle, the ratified modonr default, or the default-filled v4 options block were not carried forward)"
teeth=$((teeth + 1))
echo "    T-H13 OK: genuine v2 migrates byte-for-byte (ctlstyle preserved, modonr ratified default)"
# T-H16 (driver ruling 2026-07-29, the MLFKPERSIST4 merge): a GENUINE v3
# file MIGRATES to v4 rather than resetting, and the 50 target records plus
# every v3 setting survive BYTE-FOR-BYTE. This is the arm the v4 bump added,
# and it was the ONLY migration arm without a tooth — which is precisely
# where a silent save-wipe hides, and the owner has a real save on his
# device. ctlstyle AND modonr both carry over unchanged (v3 already had
# both); the seven appended v4 options lines take the fresh-install
# defaults, because no v3 file ever carried an opinion about them.
mk_pdir "$HP/th16" -
{ printf 'MLFKPERSIST3\n'; sed -n '2,4p' "$FILE_P01"; printf 'ctlstyle 1\n';
  printf 'modonr 0\n'; th9_rows "$WORSE_BITS"; } > "$HP/th16/body"
{ cat "$HP/th16/body"; printf 'SUM %s\n' "$(shasum -a 256 "$HP/th16/body" | cut -d' ' -f1)"; } \
  > "$HP/th16/mlfk-persist.dat"
rm -f "$HP/th16/body"
[ "$(grep -c "" "$HP/th16/mlfk-persist.dat")" = 57 ] \
  || fail "T-H16: the v3 fixture is not 57 lines (fixture construction broken — dead tooth)"
# The fixture must NOT already equal the expectation, or the tooth proves
# nothing (the same dead-tooth guard T-H9 carries).
rc=0; cmp -s "$HP/th16/mlfk-persist.dat" "$HP/th9.expect.dat" || rc=$?
[ "$rc" = 1 ] || fail "T-H16: fixture and expectation are already identical (dead tooth)"
rc=0
MLFK_PERSIST_DIR="$PWD/$HP/th16" \
"$BUILD/foh_dev_headless" --tooth-persist-finish "$REC_CHAR" "$REC_TSTAGE" \
  "$REC_BITS" 2> "$HP/th16.log" || rc=$?
[ "$rc" = 0 ] || { cat "$HP/th16.log" >&2; fail "T-H16: the v3 migrate+improve+save run failed"; }
[ "$(count_xl "$HP/th16.log" "foh_persist: migrated from=3")" = 1 ] \
  || grammar_die "T-H16: expected exactly one 'foh_persist: migrated from=3'"
[ "$(count_xl "$HP/th16.log" "foh_persist: loaded")" = 1 ] \
  || grammar_die "T-H16: a migrated v3 file must also report exactly one 'loaded'"
c="$(grep -c '^foh_persist: reset cause=' "$HP/th16.log")" || true
[ "$c" = 0 ] \
  || grammar_die "T-H16: a reset occurred while migrating a VALID v3 file (PB data-loss regression — the owner has a real save)"
verify_persist_file "$HP/th16/mlfk-persist.dat" "T-H16 migrated"
# Same expectation as T-H9/T-H13: identical settings, ctlstyle 1, modonr 0,
# the 49 seeded records untouched, the target slot improved, and the v4
# options block at its defaults.
cmp "$HP/th16/mlfk-persist.dat" "$HP/th9.expect.dat" \
  || fail "T-H16: the migrated v3 file != the independently built v5 (settings, the 49 untouched records, the improved slot, ctlstyle/modonr, or the default-filled v4 options block were not carried forward) — the PB data-loss regression"
teeth=$((teeth + 1))
echo "    T-H16 OK: genuine v3 migrates byte-for-byte to v5 (records + ctlstyle + modonr intact, options block default-filled, no reset)"
# T-H17 the mirror of T-H10/T-H15 for the new version: a v3 header that ALSO
# carries the v4 options block is not a v3 file. v3's grammar ends at the
# last rec row, so the "nothing may sit between the last content line and
# the SUM line" guard must reject it rather than silently skip 7 lines.
mk_pdir "$HP/th17" -
{ printf 'MLFKPERSIST3\n'; sed -n '2,4p' "$FILE_P01"; printf 'ctlstyle 1\n';
  printf 'modonr 0\n'; th9_rows "$WORSE_BITS"; v4_defaults; } > "$HP/th17/body"
{ cat "$HP/th17/body"; printf 'SUM %s\n' "$(shasum -a 256 "$HP/th17/body" | cut -d' ' -f1)"; } \
  > "$HP/th17/mlfk-persist.dat"
rm -f "$HP/th17/body"
tooth_boot h17 "$PWD/$HP/th17" "foh_persist: reset cause=corrupt detail=grammar"
teeth=$((teeth + 1))
echo "    T-H17 OK: v3 header WITH a v4 options block dies on grammar"
# T-H14 (review-ctl n1): a v2 file may NOT carry a v3-era style value, and
# may NOT carry a modonr line. MLFKPERSIST2 predates CTL_STYLE_NATURAL, so
# its ctlstyle grammar was {0,1}; accepting `ctlstyle 2` would install a
# state no v2 writer could produce. Both fixtures are RESEALED so they die
# on the version-specific grammar, not on the SHA gate.
mk_pdir "$HP/th14" -
{ printf 'MLFKPERSIST2\n'; sed -n '2,4p' "$FILE_P01"; printf 'ctlstyle 2\n';
  th9_rows "$WORSE_BITS"; } > "$HP/th14/body"
{ cat "$HP/th14/body"; printf 'SUM %s\n' "$(shasum -a 256 "$HP/th14/body" | cut -d' ' -f1)"; } \
  > "$HP/th14/mlfk-persist.dat"
rm -f "$HP/th14/body"
tooth_boot h14 "$PWD/$HP/th14" "foh_persist: reset cause=corrupt detail=grammar"
teeth=$((teeth + 1))
echo "    T-H14 OK: v2 claiming the v3-era ctlstyle 2 dies on grammar"
# T-H15 the mirror: a v2 header that ALSO carries a modonr line must die —
# v2's grammar ends at ctlstyle, so the rec parser meets `modonr`.
mk_pdir "$HP/th15" -
{ printf 'MLFKPERSIST2\n'; sed -n '2,4p' "$FILE_P01"; printf 'ctlstyle 1\n';
  printf 'modonr 0\n'; th9_rows "$WORSE_BITS"; } > "$HP/th15/body"
{ cat "$HP/th15/body"; printf 'SUM %s\n' "$(shasum -a 256 "$HP/th15/body" | cut -d' ' -f1)"; } \
  > "$HP/th15/mlfk-persist.dat"
rm -f "$HP/th15/body"
tooth_boot h15 "$PWD/$HP/th15" "foh_persist: reset cause=corrupt detail=grammar"
teeth=$((teeth + 1))
echo "    T-H15 OK: v2 header WITH a modonr line dies on grammar"
# H1 standing teeth (review-100): the REAL bootid_judge body must reject
# every non-cycle and accept a valid one (dead-tooth guard). Run in a
# subshell (bootid_judge's fail() exits) and require the exit class.
bootid_tooth() { # <desc> <die|pass> <judge args...>
  local desc="$1" expect="$2"; shift 2
  local trc=0
  ( bootid_judge "$@" ) >/dev/null 2>&1 || trc=$?
  if [ "$expect" = die ]; then
    [ "$trc" != 0 ] || fail "H1 tooth [$desc]: bootid_judge ACCEPTED a bad cycle (dead tooth)"
  else
    [ "$trc" = 0 ] || fail "H1 tooth [$desc]: bootid_judge REJECTED a valid cycle (dead-tooth guard)"
  fi
}
bootid_tooth "no-reboot POST==PRE"     die  bootid AAAA bootid AAAA 5 60
bootid_tooth "stale boot uptime>=gap"  die  bootid AAAA bootid BBBB 9000 60
bootid_tooth "source flip"             die  btime  100  bootid 200  5 60
bootid_tooth "valid cycle guard"       pass bootid AAAA bootid BBBB 5 60
teeth=$((teeth + 1))
echo "    H1 standing teeth OK: bootid_judge rejects non-cycle/stale/source-flip, accepts a valid cycle"

# H standing tooth (review-102 DATA-LOSS): the trap's residue decision
# must NEVER delete the user's file on an UNPROBED (early-failure) path.
# Prove it with a planted COPY 'user file' + the REAL
# persist_residue_decide, applying the decided action to the local copy.
apply_resid() { # <action> <file> : the trap's device dispatch, mirrored
  case "$1" in
    delete) rm -f "$2" ;;                 # ABSENT only
    keep|keep-nobackup|noop|restore) : ;; # never removes the user's bytes
    *) echo "H tooth: unknown action '$1'" >&2; return 9 ;;
  esac
}
HT="$HP/htooth"
rm -rf "$HT"; mkdir -p "$HT"
USERF="$HT/user-persist.dat"
cp "$FILE_REC" "$USERF"          # a planted user file (byte-known)
usersha="$(rig_host_sha256 "$USERF")" || exit 1
# UNPROBED (the hazard): decision must be keep; applying it must leave the
# planted file byte-identical (this is the literal triage tooth: a COPY
# run "killed at step [1]" -> file survives byte-identical).
act="$(persist_residue_decide UNPROBED 0 0)"
[ "$act" = keep ] || fail "H tooth: UNPROBED decided '$act' != keep (an unprobed file could be deleted — the data-loss hazard)"
apply_resid "$act" "$USERF"
[ -f "$USERF" ] || fail "H tooth: UNPROBED action DELETED the planted user file (data loss)"
now="$(rig_host_sha256 "$USERF")" || exit 1
[ "$now" = "$usersha" ] || fail "H tooth: UNPROBED action altered the planted user file ($now != $usersha)"
# ABSENT: the only delete arm (probe completed, genuinely absent)
[ "$(persist_residue_decide ABSENT 0 0)" = delete ] || fail "H tooth: ABSENT must decide delete"
# PRESENT with a verified backup -> restore; already-restored -> noop;
# a MISSING backup -> keep (never delete without a backup)
[ "$(persist_residue_decide PRESENT 0 1)" = restore ] || fail "H tooth: PRESENT+backup must decide restore"
[ "$(persist_residue_decide PRESENT 1 1)" = noop ] || fail "H tooth: PRESENT+restored must decide noop"
[ "$(persist_residue_decide PRESENT 0 0)" = keep-nobackup ] || fail "H tooth: PRESENT with no backup must decide keep-nobackup (never delete)"
# UNKNOWN state -> keep (fail-safe): a corrupted state var never deletes
cp "$FILE_REC" "$USERF"
act="$(persist_residue_decide WHATEVER 0 0)"
[ "$act" = keep ] || fail "H tooth: an unknown state decided '$act' != keep (fail-safe violated)"
apply_resid "$act" "$USERF"
[ -f "$USERF" ] || fail "H tooth: an unknown state DELETED the planted user file"
teeth=$((teeth + 1))
echo "    H trap tooth OK: UNPROBED/unknown keep the user file byte-identical; delete only on ABSENT; PRESENT restores/never-deletes"

# H COPY-LEVEL tooth (review-104 L-1): the unit tooth above proves the PURE
# decision; THIS proves the REAL `trap cleanup EXIT` + REAL cleanup()
# dispatch, on a COPY of THIS check killed right after the trap installs
# (PERSIST_STATE=UNPROBED, before any probe runs). The device seam is
# host-simulated: no-op stubs plus a rig_dsh_retry that RECORDS every device
# command verbatim and NEVER executes one — it performs a single fixed,
# quoted `rm -f` of the two LOCAL override paths iff the recorded string
# equals the exact delete command the real cleanup emits (review-106
# rounds 4-5). So the tooth consumes NO paced run, cannot reach the device,
# and cannot run a device command against the host. Contract: a planted override file SURVIVES
# byte-identical under UNPROBED; a dead-tooth COPY at PERSIST_STATE=ABSENT
# DELETES it (proving the residue dispatch really reaches the device-delete
# arm through the real trap, not a mirror). The awk injects the seam stubs
# after the two library `source` lines and a state-override + early `exit`
# right after the trap install; anchored guards fail loudly if a future
# refactor renames those lines and silently defangs the tooth.
htc="$HP/htcopy"; rm -rf "$htc"; mkdir -p "$htc"
L1_COPY="$htc/l1-copy.sh"
awk '
  /^cd "\$\(dirname/ { print "cd \"${MLFK_L1_REPO:?}\""; next }
  { print }
  /^source port\/sim\/device\/adbsh\.sh$/ { print "require_device() { :; }" }
  /^source port\/sim\/device\/riglib\.sh$/ {
    print "rig_lock_acquire() { :; }"
    print "rig_cleanup() { :; }"
    print "rig_dsh_retry() { printf \"%s\\n\" \"$1\" >> \"${MLFK_L1_CMDS:?}\"; if [ \"$1\" = \"rm -f ${MLFK_L1_DFILE:?} ${MLFK_L1_DDATA:?}/mlfk-persist.tmp\" ]; then rm -f \"${MLFK_L1_DFILE}\" \"${MLFK_L1_DDATA}/mlfk-persist.tmp\"; fi; return 0; }"
    print "dsh() { printf \"DSH %s\\n\" \"$*\" >> \"${MLFK_L1_CMDS:?}\"; return 0; }"
    print "adb() { printf \"ADB %s\\n\" \"$*\" >> \"${MLFK_L1_CMDS:?}\"; return 0; }"
  }
  /^trap cleanup EXIT$/ {
    print "PERSIST_STATE=\"${MLFK_L1_STATE:?}\"; DFILE=\"${MLFK_L1_DFILE:?}\"; DDATA=\"${MLFK_L1_DDATA:?}\"; BUILD=\"${MLFK_L1_BUILD:?}\"; PREEXIST_RESTORED=0; exit 0"
  }
' "$0" > "$L1_COPY"
[ "$(grep -c '^cd .*MLFK_L1_REPO' "$L1_COPY")" = 1 ] \
  || fail "H copy tooth: repo-cd replacement did not land (the check's dirname-cd line changed?)"
[ "$(grep -c '^require_device() { :; }$' "$L1_COPY")" = 1 ] \
  || fail "H copy tooth: require_device stub injection did not land (adbsh source-line anchor changed?)"
# review-106 M-C (round 4) + round-5 hardening: the COPY must never EXECUTE a
# device command string on the HOST. The round-3 stub `eval`ed every one — so
# cleanup's `pkill foh_device` ran locally, twice per cold run
# (host-side collateral destruction) — and the round-4 attempt still `eval`ed
# any string that MENTIONED the override dir, which is membership, not
# confinement (`rm -f $DFILE ...; rm -f /elsewhere` would have run whole).
# There is now NO eval anywhere in the seam: the stub RECORDS every device
# command verbatim, and performs ONE hardcoded local deletion of the two
# override paths iff the recorded string is EXACTLY the delete command the
# real cleanup emits (whitelist by full-string equality, fixed action). The
# injected seam is pinned as an EXACT FULL LINE: any drift — a reintroduced
# eval, a widened predicate, a dropped recorder — fails here instead of
# running device commands against the host. (Anchored -x so this pin cannot
# match its own source line.)
L1_STUB='rig_dsh_retry() { printf "%s\n" "$1" >> "${MLFK_L1_CMDS:?}"; if [ "$1" = "rm -f ${MLFK_L1_DFILE:?} ${MLFK_L1_DDATA:?}/mlfk-persist.tmp" ]; then rm -f "${MLFK_L1_DFILE}" "${MLFK_L1_DDATA}/mlfk-persist.tmp"; fi; return 0; }'
[ "$(grep -cxF "$L1_STUB" "$L1_COPY")" = 1 ] \
  || fail "H copy tooth: the device-seam stub is not the exact recorded+confined form (injection missed, or a refactor reintroduced host execution)"
[ "$(grep -c '^adb() { printf' "$L1_COPY")" = 1 ] \
  || fail "H copy tooth: adb stub injection did not land (the COPY could reach the REAL device)"
[ "$(grep -c 'eval' "$L1_COPY")" = "$(grep -c 'eval' "$0")" ] \
  || fail "H copy tooth: the generated COPY introduced an eval the check itself does not have (host-execution class)"
# review-106 round-5 tooth: the seam predicate is EXACT-STRING, not membership.
# Define the very function the COPY carries (from the pinned text) and prove:
# a command that merely MENTIONS the override paths does NOT execute (the
# round-4 hole — it would have deleted both files), the EXACT delete command
# DOES delete exactly the two override paths, and every command is recorded.
subu="$htc/stubunit"; rm -rf "$subu"; mkdir -p "$subu"
( set +e
  MLFK_L1_CMDS="$subu/cmds"; : > "$MLFK_L1_CMDS"
  MLFK_L1_DDATA="$subu"; MLFK_L1_DFILE="$subu/mlfk-persist.dat"
  printf 'victim' > "$MLFK_L1_DFILE"; printf 'collateral' > "$subu/other.dat"
  eval "$L1_STUB"
  rig_dsh_retry "pkill foh_device; true"
  rig_dsh_retry "rm -f $MLFK_L1_DFILE $subu/other.dat"
  [ -f "$MLFK_L1_DFILE" ] || exit 21   # a MENTIONING command must not run
  [ -f "$subu/other.dat" ] || exit 22   # ...and must not take collateral
  rig_dsh_retry "rm -f $MLFK_L1_DFILE $MLFK_L1_DDATA/mlfk-persist.tmp"
  [ -f "$MLFK_L1_DFILE" ] && exit 23    # the EXACT command must delete
  [ -f "$subu/other.dat" ] || exit 24   # ...and still take no collateral
  [ "$(grep -c '' "$MLFK_L1_CMDS")" = 3 ] || exit 25
  exit 0 )
subu_rc=$?
[ "$subu_rc" = 0 ] \
  || fail "H copy tooth (seam predicate unit): exact-string confinement failed (rc $subu_rc: 21/22 = a merely-MENTIONING device command executed on the host, 23 = the exact delete did not act, 24 = collateral, 25 = commands not recorded)"
[ "$(grep -c '^PERSIST_STATE=.*MLFK_L1_STATE' "$L1_COPY")" = 1 ] \
  || fail "H copy tooth: early-exit injection did not land (trap-line anchor changed?)"
l1_run() { # <state> : run the COPY with an override dir + a planted file
  local st="$1" ovd plant rc
  ovd="$htc/override-$st"; rm -rf "$ovd"; mkdir -p "$ovd" "$htc/build-$st"
  plant="$ovd/mlfk-persist.dat"
  cp "$FILE_REC" "$plant"           # a planted user file (byte-known)
  L1_SHA="$(rig_host_sha256 "$plant")" || exit 1
  L1_CMDS="$htc/cmds-$st.txt"; : > "$L1_CMDS"
  rc=0
  MLFK_L1_STATE="$st" MLFK_L1_DFILE="$plant" MLFK_L1_DDATA="$ovd" \
    MLFK_L1_BUILD="$htc/build-$st" MLFK_L1_REPO="$PWD" MLFK_L1_CMDS="$L1_CMDS" \
    bash "$L1_COPY" >"$htc/log-$st.txt" 2>&1 || rc=$?
  L1_RC="$rc"; L1_PLANT="$plant"
  # review-106 M-C: cleanup's FIRST device command on every path is the
  # foh_device pkill. It must have been RECORDED by the seam stub (proof the
  # REAL cleanup body ran through the REAL seam) and NEVER executed on the
  # host. Full-line fixed match, exactly once.
  [ "$(grep -cxF 'pkill foh_device; true' "$L1_CMDS")" = 1 ] \
    || fail "H copy tooth ($st): cleanup's device pkill was not recorded exactly once by the seam stub — the tooth is not observing the real cleanup (or a command escaped to the HOST)"
}
# UNPROBED (the hazard): the COPY exits at the trap with no probe -> the REAL
# cleanup KEEPS the planted file byte-identical.
l1_run UNPROBED
[ "$L1_RC" = 0 ] || fail "H copy tooth: the UNPROBED copy exited nonzero ($L1_RC); see $htc/log-UNPROBED.txt"
[ -f "$L1_PLANT" ] || fail "H copy tooth: the REAL trap DELETED the planted file on an UNPROBED (early) exit — DATA LOSS"
l1_now="$(rig_host_sha256 "$L1_PLANT")" || exit 1
[ "$l1_now" = "$L1_SHA" ] || fail "H copy tooth: the REAL trap ALTERED the planted file on UNPROBED ($l1_now != $L1_SHA)"
[ "$(grep -c '^rm -f ' "$L1_CMDS")" = 0 ] \
  || fail "H copy tooth: the UNPROBED trap DISPATCHED a device delete command (it must not reach the delete arm at all)"
# dead-tooth: at PERSIST_STATE=ABSENT the SAME real trap MUST delete the
# override file (proves the dispatch reaches the device-delete arm; a no-op
# here would mean the tooth could never observe a real deletion).
l1_run ABSENT
[ "$L1_RC" = 0 ] || fail "H copy tooth: the ABSENT copy exited nonzero ($L1_RC); see $htc/log-ABSENT.txt"
[ ! -f "$L1_PLANT" ] || fail "H copy tooth: dead-tooth — the ABSENT real trap did NOT delete the override file (the copy is not dispatching the real cleanup)"
[ "$(grep -cxF "rm -f $L1_PLANT $htc/override-ABSENT/mlfk-persist.tmp" "$L1_CMDS")" = 1 ] \
  || fail "H copy tooth: the ABSENT delete was not the exact override-confined device command the real cleanup emits (see $L1_CMDS)"
teeth=$((teeth + 2))
echo "    H copy-trap tooth OK: the REAL trap+cleanup on a killed COPY keeps the planted file byte-identical under UNPROBED (zero delete commands dispatched), deletes it under ABSENT via the exact expected delete command (dead-tooth guarded); no paced run"
echo "    H seam-predicate tooth OK: the COPY never EVALs a device command — every command is recorded verbatim, only the EXACT expected delete acts (as a fixed local rm on the two override paths), and a merely-MENTIONING command takes no action and no collateral"

# M-a standing teeth (review-102): the RAW freshness-token grammars reject
# resemblance. raw_single_line enforces the newline shape; the caller
# regexes reject squeezed junk. Feed crafted raw files.
mat="$HP/matooth"; rm -rf "$mat"; mkdir -p "$mat"
# ma_run <file> : set globals MA_OUT (raw_single_line content) + MA_RC (its
# rc; nonzero = newline-shape rejection), run in the CURRENT shell (never
# a $() subshell — globals must propagate) with set -e neutralized.
MA_OUT=""; MA_RC=0
ma_run() { MA_OUT=""; MA_RC=0; MA_OUT="$(raw_single_line "$1" 2>/dev/null)" || MA_RC=$?; }
BID_RE='^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$'
BT_RE='^btime ([1-9][0-9]{5,12})$'
UP_RE='^([0-9]+)\.[0-9]{2} [0-9]+\.[0-9]{2}$'   # M-1: exactly 2 frac digits (measured)
# review-104 M-1: the MEASURED adb-pty producer terminates every captured
# line with CRLF (0d 0a). raw_single_line validates that BYTE-EXACTLY (no
# de-CR normalization, no $() NUL/LF laundering); the caller regex then
# rejects a resembling token. Genuine CRLF passes; LF-only / no-terminator
# / double-CRLF / INTERIOR-CR / INTERIOR-NUL / trailing-junk all die on the
# BYTE SHAPE (rc!=0) — the interior-CR/NUL cases are the exact holes a
# `tr -d '\r'` normalize + `$(cat)` capture would have laundered into a match.
printf '5b9b339e-1234-4abc-8def-0011223344ff\r\n' > "$mat/bid-ok"
ma_run "$mat/bid-ok"; { [ "$MA_RC" = 0 ] && [[ "$MA_OUT" =~ $BID_RE ]]; } || fail "M-a tooth: genuine (CRLF) boot_id rejected (rc=$MA_RC out='$MA_OUT')"
printf '5b9b339e-1234-4abc-8def-0011223344ff\n' > "$mat/bid-lfonly"
ma_run "$mat/bid-lfonly"; [ "$MA_RC" != 0 ] || fail "M-a tooth: LF-only boot_id accepted (missing CR; producer is CRLF) (rc0, raw='$MA_OUT')"
printf '5b9b339e-1234-4abc-8def-0011223344ff' > "$mat/bid-nolf"
ma_run "$mat/bid-nolf"; [ "$MA_RC" != 0 ] || fail "M-a tooth: no-terminator boot_id accepted (rc0, raw='$MA_OUT')"
printf '5b9b339e-1234-4abc-8def-0011223344ff\r\n\r\n' > "$mat/bid-2crlf"
ma_run "$mat/bid-2crlf"; [ "$MA_RC" != 0 ] || fail "M-a tooth: double-CRLF boot_id accepted (rc0, raw='$MA_OUT')"
printf '5b9b339e-1234-4abc-8def-00112233\r44ff\r\n' > "$mat/bid-icr"
ma_run "$mat/bid-icr"; [ "$MA_RC" != 0 ] || fail "M-a tooth: interior-CR boot_id laundered into a match (rc0, raw='$MA_OUT')"
{ printf '5b9b339e-1234-4abc-8def-00112233'; printf '\000'; printf '44ff\r\n'; } > "$mat/bid-inul"
ma_run "$mat/bid-inul"; [ "$MA_RC" != 0 ] || fail "M-a tooth: interior-NUL boot_id laundered into a match (rc0, raw='$MA_OUT')"
printf '5b9b339e-1234-4abc-8def-0011223344ff\r\nX' > "$mat/bid-trail"
ma_run "$mat/bid-trail"; [ "$MA_RC" != 0 ] || fail "M-a tooth: trailing-byte-after-CRLF boot_id accepted (rc0, raw='$MA_OUT')"
# btime: genuine CRLF passes; 'btime 7 garbage' (valid byte shape) dies on the GRAMMAR
printf 'btime 19283746\r\n' > "$mat/bt-ok"
ma_run "$mat/bt-ok"; { [ "$MA_RC" = 0 ] && [[ "$MA_OUT" =~ $BT_RE ]]; } || fail "M-a tooth: genuine btime rejected (rc=$MA_RC out='$MA_OUT')"
printf 'btime 7 garbage\r\n' > "$mat/bt-junk"
ma_run "$mat/bt-junk"; { [ "$MA_RC" = 0 ] && [[ "$MA_OUT" =~ $BT_RE ]]; } && fail "M-a tooth: 'btime 7 garbage' laundered into a match" || true
# uptime: exact two 2-frac decimals; single field / trailing junk / wrong frac width die
printf '1234.56 2345.67\r\n' > "$mat/up-ok"
ma_run "$mat/up-ok"; { [ "$MA_RC" = 0 ] && [[ "$MA_OUT" =~ $UP_RE ]]; } || fail "M-a tooth: genuine uptime rejected (rc=$MA_RC out='$MA_OUT')"
printf '7 garbage\r\n' > "$mat/up-junk"
ma_run "$mat/up-junk"; { [ "$MA_RC" = 0 ] && [[ "$MA_OUT" =~ $UP_RE ]]; } && fail "M-a tooth: '7 garbage' accepted as uptime" || true
printf '1234.56 garbage\r\n' > "$mat/up-junk2"
ma_run "$mat/up-junk2"; { [ "$MA_RC" = 0 ] && [[ "$MA_OUT" =~ $UP_RE ]]; } && fail "M-a tooth: '1234.56 garbage' accepted as uptime" || true
printf '1234.5 2345.67\r\n' > "$mat/up-frac1"
ma_run "$mat/up-frac1"; { [ "$MA_RC" = 0 ] && [[ "$MA_OUT" =~ $UP_RE ]]; } && fail "M-a tooth: 1-frac-digit uptime accepted (producer emits exactly 2)" || true
teeth=$((teeth + 1))
echo "    M-a teeth OK: raw CRLF byte-grammar rejects LF-only/no-term/double-CRLF/interior-CR/interior-NUL/trailing-junk + token-grammar junk, accepts genuine shapes"

# M-c standing teeth (review-102): the PURE device-scan validator
# rig_orphan_parse (from riglib) reconciles MLFKPROC rows == reaped
# count and fails closed on unreadable/junk/absent-summary. The
# device-side scan (mlfk_scan's unreadable detection + L-a relative-cwd
# predicate) is integration-covered by the cold done-check's step-0 +
# cleanup reaps; this toothes the host reconciliation logic directly.
mc_ok()   { [[ "$(rig_orphan_parse "$1")" =~ ^OK\  ]] || fail "M-c tooth: '$1' should parse OK"; }
# `|| true`: rig_orphan_parse returns 1 on its FAIL verdict by design —
# neutralize the check's set -e so we can inspect the prefix.
mc_fail() { local r; r="$(rig_orphan_parse "$1")" || true; [ "${r%% *}" = FAIL ] || fail "M-c tooth: '$2' should FAIL closed (got '$r')"; }
mc_ok "MLFKSCAN 0 0"                                              # clean
mc_ok "$(printf 'MLFKPROC 123 sh /tmp/mlfk/deadman.sh\nMLFKSCAN 1 0')" # reconciled 1-found (conforming row)
mc_ok "$(printf 'MLFKPROC 9 sh /tmp/mlfk/deadman.sh\nMLFKSCAN 1 1')"   # left>0 still parses (caller judges left)
# M-3 (review-104): a resembling MLFKPROC row (missing cmdline field, pid 0)
# is CORRUPTION DEATH now, never a silently-accepted count.
mc_fail "$(printf 'MLFKPROC 9 z\nMLFKSCAN 1 1')" "malformed MLFKPROC (no cmdline field — the exact round-2 hole)"
mc_fail "$(printf 'MLFKPROC 12 sh\nMLFKSCAN 1 0')" "malformed MLFKPROC (comm only, no cmdline)"
mc_fail "$(printf 'MLFKPROC 0 sh /tmp/mlfk/a\nMLFKSCAN 1 0')" "malformed MLFKPROC (pid 0)"
# review-106 round-5: the producer bounds are the MEASURED device ones
# (.loop/m4-per107-measure-pidcomm.log — pid_max 32768 so max pid 32767; comm
# <= 15 bytes over an OPEN alphabet: the live device carries `/`, `:` and
# UPPERCASE comms). Past a bound = corruption; INSIDE the bounds, including
# every measured comm spelling, must still ACCEPT (zero false rejections).
mc_fail "$(printf 'MLFKPROC 32768 sh /tmp/mlfk/a\nMLFKSCAN 1 0')" "pid == pid_max (exclusive; not allocatable)"
mc_fail "$(printf 'MLFKPROC 99999 sh /tmp/mlfk/a\nMLFKSCAN 1 0')" "pid past the measured pid_max (digit-width alone would accept it)"
mc_fail "$(printf 'MLFKPROC 12345678 sh /tmp/mlfk/a\nMLFKSCAN 1 0')" "pid far past the measured pid_max (8 digits)"
# round-6: the bounds are BYTE bounds (LC_ALL=C in the parser) — a 15-CHARACTER
# but 30-BYTE comm is corruption, not a find (it would have matched under a
# UTF-8 locale). The genuine 15-BYTE comm above still accepts.
mc_fail "$(printf 'MLFKPROC 12 \303\251\303\251\303\251\303\251\303\251\303\251\303\251\303\251\303\251\303\251\303\251\303\251\303\251\303\251\303\251 /tmp/mlfk/a\nMLFKSCAN 1 0')" "15-character/30-byte comm (locale-dependent bound)"
# round-6 (dispositioned, pinned so it cannot drift silently): the row is
# space-delimited, so a space-bearing comm is framed as comm=first word +
# cmdline=rest. It PARSES (the split is not decision-bearing: kills use the
# device-side pid list and the count reconciliation catches drops).
mc_ok "$(printf 'MLFKPROC 12 My Tool /tmp/mlfk/MyTool\nMLFKSCAN 1 0')"
mc_fail "$(printf 'MLFKPROC 12 abcdefghijklmnop /tmp/mlfk/a\nMLFKSCAN 1 0')" "over-long comm (16 bytes > TASK_COMM_LEN-1)"
mc_ok "$(printf 'MLFKPROC 32767 irq/42-axp20x_i /tmp/mlfk/a\nMLFKSCAN 1 0')"  # max pid + a MEASURED 15-byte comm with `/`
mc_ok "$(printf 'MLFKPROC 7 kworker/0:1H /tmp/mlfk/a\nMLFKSCAN 1 0')"         # measured comm with `:` + UPPERCASE
mc_ok "$(printf 'MLFKPROC 8 MyTool /tmp/mlfk/MyTool\nMLFKSCAN 1 0')"          # human-launched tool (the round-4 false-rejection bug)
mc_fail "$(printf 'MLFKPROC 1 sh /tmp/mlfk/a\nMLFKPROC 2 sh /tmp/mlfk/b\nMLFKSCAN 1 0')" "reconciliation mismatch (2 rows, found 1)"
mc_fail "$(printf 'MLFKUNREADABLE 55\nMLFKSCAN 0 0')" "live unreadable proc"
mc_fail "$(printf 'GARBAGE\nMLFKSCAN 0 0')" "non-whitelisted line"
mc_fail "MLFKPROC 1 sh /tmp/mlfk/a" "absent MLFKSCAN summary"
teeth=$((teeth + 1))
echo "    M-c teeth OK: rig_orphan_parse enforces the anchored MLFKPROC row grammar + reconciles rows==found; fails closed on malformed-row/unreadable/mismatch/junk/absent-summary"

# M-b corpus (review-102, mandatory): every genuine reference passes;
# byte-drop / trailing-NUL / embedded-NUL / bad-SUM-line variants reject.
mbc="$HP/mbcorpus"; rm -rf "$mbc"; mkdir -p "$mbc"
mb_pass=0; mb_rej=0
for gf in "$FILE_P01" "$FILE_REC" "$FILE_DEFAULTS" \
          "$HP/twin-persist/mlfk-persist.dat" "$HP/m1-persist/mlfk-persist.dat"; do
  ( verify_persist_file "$gf" "M-b corpus genuine $(basename "$(dirname "$gf")")/$(basename "$gf")" ) >/dev/null 2>&1 \
    || fail "M-b corpus: genuine file $gf was REJECTED (false rejection — remeasure, never loosen)"
  mb_pass=$((mb_pass + 1))
done
mb_reject() { # <name> : build a corrupt variant, assert verify_persist_file REJECTS
  local nm="$1"
  local v="$mbc/$nm.dat" rc=0
  ( verify_persist_file "$v" "M-b corpus corrupt $nm" ) >/dev/null 2>&1 || rc=$?
  [ "$rc" != 0 ] || fail "M-b corpus: corrupt variant $nm was ACCEPTED (byte-exactness hole)"
  mb_rej=$((mb_rej + 1))
}
# trailing-NUL replacing the final LF (the exact $(tail -c1) hole)
head -c $((PERSIST_BYTES - 1)) "$FILE_REC" > "$mbc/nul.dat"; printf '\000' >> "$mbc/nul.dat"
mb_reject nul
# one embedded NUL inside a rec line (byte-count grows; $() would drop it)
{ head -c 900 "$FILE_REC"; printf '\000'; tail -c +901 "$FILE_REC"; } > "$mbc/embed.dat"
mb_reject embed
# one byte dropped (byte-count short)
head -c $((PERSIST_BYTES - 1)) "$FILE_REC" > "$mbc/short1.dat"
mb_reject short1
# SUM seal mismatch at the EXACT byte length (a nibble flip in a rec row,
# SUM left stale): $PERSIST_BYTES bytes, final LF intact -> must reject at the seal
# recompute, not slip through a permissive field parse.
sed "s/^rec $REC_CHAR $REC_TSTAGE $REC_BITS\$/rec $REC_CHAR $REC_TSTAGE 402e000000000000/" \
  "$FILE_REC" > "$mbc/sealmiss.dat"
[ "$(wc -c < "$mbc/sealmiss.dat" | tr -d ' ')" = "$PERSIST_BYTES" ] \
  || fail "M-b corpus: sealmiss variant byte count drifted (test-fixture bug)"
mb_reject sealmiss
teeth=$((teeth + 1))
echo "    M-b corpus OK: $mb_pass genuine PASS / $mb_rej corrupt REJECT (byte-exactness: NUL/embed/short/seal-mismatch)"

# --- [5] arm build + push + pre-existing product state ------------------------
echo "== [5/10] armv7 build (shared rig stamp) + push + provenance =="
rig_arm_build
rig_stamp_rehash foh_device
dsh "rm -rf $DTMP $DSD && mkdir -p $DTMP $DSD"
provision() { # push binary + flows into a fresh $DTMP (rerun post-reboot)
  adb -s "$DEV" push "$DEVB/foh_device" "$TABLES/assets/menu.img1" \
    "$FLOWD/p00-persist-probe.flow" "$FLOWD/p01-persist-edit.flow" \
    "$FLOWD/p02-persist-verify.flow" "$DTMP/" >/dev/null
  rig_push_provenance "$DTMP" foh_device
  dsh "chmod +x $DTMP/foh_device"
  local hf bn hsum dsum
  for hf in "$TABLES/assets/menu.img1" \
            "$FLOWD/p00-persist-probe.flow" "$FLOWD/p01-persist-edit.flow" \
            "$FLOWD/p02-persist-verify.flow"; do
    bn="$(basename "$hf")"
    hsum="$(rig_host_sha256 "$hf")" || exit 1
    dsum="$(rig_dev_sha256 "$DTMP/$bn")" || exit 1
    [ "$dsum" = "$hsum" ] || fail "pushed $bn device sha ($dsum) != host sha ($hsum)"
  done
}
provision
dsh "mkdir -p $DDATA"
prc=0
dsh "test -f $DFILE" >/dev/null || prc=$?
case "$prc" in
  0)
    # review-102 H: PRESENT only AFTER the backup is pulled + hash-verified.
    # pull_bytes fails closed -> the state stays UNPROBED and this `fail`
    # aborts BEFORE the `rm` below (the only $DFILE device write), so a
    # backup failure never touches the user's file.
    pull_bytes "$DFILE" "$BUILD/preexisting-mlfk-persist.dat" \
      || fail "could not pull aside the pre-existing $DFILE (M4: the user's data must be preserved; state stays UNPROBED — file untouched)"
    PERSIST_STATE=PRESENT
    PREEXIST=1
    dsh "rm -f $DFILE $DDATA/mlfk-persist.tmp"
    echo "   pre-existing $DFILE pulled aside (zero-byte-safe; state=PRESENT; verdict-bound restore in [10])"
    ;;
  1)
    PERSIST_STATE=ABSENT
    dsh "rm -f $DDATA/mlfk-persist.tmp"
    echo "   no pre-existing $DFILE (state=ABSENT; test residue is ours to wipe)"
    ;;
  *) fail "cannot probe for a pre-existing $DFILE (rc $prc; state stays UNPROBED — file untouched)" ;;
esac
echo "   pushed + sha-verified; product dir clean"

# --- deadman + park machinery (per parked window; never spans the reboot) ----
arm_park() { # arms a fresh deadman + parks the frontend
  DM_NONCE="$RANDOM$RANDOM$$"
  rm -f "$BUILD/deadman.sh"
  cat > "$BUILD/deadman.sh" << EOF
#!/bin/sh
# generated by check-device-persist.sh — frontend-park DEADMAN
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
fi
rm -f $DTMP/deadman.pid
exit 0
EOF
  made "$BUILD/deadman.sh"
  adb -s "$DEV" push "$BUILD/deadman.sh" "$DTMP/" >/dev/null
  local hsum dsum
  hsum="$(rig_host_sha256 "$BUILD/deadman.sh")" || exit 1
  dsum="$(rig_dev_sha256 "$DTMP/deadman.sh")" || exit 1
  [ "$dsum" = "$hsum" ] || fail "pushed deadman.sh sha mismatch"
  dsh "printf '%s' '$DM_NONCE' > $DTMP/deadman.nonce; rm -f $DTMP/deadman.cancel $DTMP/deadman.fired"
  dsh "setsid sh $DTMP/deadman.sh </dev/null >/dev/null 2>&1 & sleep 1"
  dsh "test -f $DTMP/deadman.pid" >/dev/null 2>&1 || fail "park deadman did not start"
  DEADMAN_ARMED=1
  PARKED=1
  dsh "touch /mnt/disable_frontend"
  local prc2=0
  dsh "pkill gmenu2x" >/dev/null 2>&1 || prc2=$?
  case "$prc2" in
    0) : ;;
    1) echo "WARN: gmenu2x was not running at park time" >&2 ;;
    *) fail "pkill gmenu2x failed (rc $prc2)" ;;
  esac
  echo "   deadman armed (${DEADMAN_S}s) + frontend parked"
}
unpark() { # verified frontend restore + deadman cancel
  dsh "rm -f /mnt/disable_frontend"
  dsh "test ! -f /mnt/disable_frontend"
  PARKED=0
  dsh "touch $DTMP/deadman.cancel"
  local dmgone=0
  for _ in $(seq 1 6); do
    if dsh "test ! -f $DTMP/deadman.pid" >/dev/null 2>&1; then dmgone=1; break; fi
    sleep 2
  done
  [ "$dmgone" = 1 ] || fail "park deadman did not exit within 12s of cancellation"
  dsh "test ! -f $DTMP/deadman.fired" >/dev/null 2>&1 \
    || fail "park deadman FIRED during a healthy run"
  DEADMAN_ARMED=0
  echo "   frontend restored; deadman cancelled without firing"
}

# run_leg <leg-name> <flow-id> — one device app leg (flow-fed, unpaced,
# product persist dir = the chokepoint's /mnt/mlfk-data default; the
# rc-file pattern because SDL apps outlive plain adb sessions).
run_leg() {
  local leg="$1" id="$2"
  rm -f "$BUILD/$leg-launch.sh"
  cat > "$BUILD/$leg-launch.sh" << EOF
#!/bin/sh
# generated by check-device-persist.sh — leg launcher for $leg ($id)
cd $DTMP || exit 9
rm -rf $leg.apprc $leg-shots foh.pid.$DM_NONCE
mkdir -p $leg-shots
setsid sh -c 'MLFK_MENU_IMG1=$DTMP/menu.img1 ./foh_device --flow $DTMP/$id.flow --input flow \\
  --flow-out $DTMP/$leg.trace.txt --shots-dir $DTMP/$leg-shots --pace 0 \\
  2> $DTMP/$leg.applog.txt & \\
  echo \$! > $DTMP/foh.pid.$DM_NONCE; \\
  wait \$!; arc=\$?; \\
  echo "RC=\$arc" > $DTMP/$leg.apprc' \\
  </dev/null >/dev/null 2>&1 &
sleep 1
EOF
  made "$BUILD/$leg-launch.sh"
  adb -s "$DEV" push "$BUILD/$leg-launch.sh" "$DTMP/" >/dev/null
  local hsum dsum
  hsum="$(rig_host_sha256 "$BUILD/$leg-launch.sh")" || exit 1
  dsum="$(rig_dev_sha256 "$DTMP/$leg-launch.sh")" || exit 1
  [ "$dsum" = "$hsum" ] || fail "pushed $leg-launch.sh sha mismatch"
  dsh "chmod +x $DTMP/$leg-launch.sh"
  dsh "sh -lc $DTMP/$leg-launch.sh"
  local done_f=0
  for _ in $(seq 1 "$READY_TRIES"); do
    if dsh "test -f $DTMP/$leg.apprc" >/dev/null 2>&1; then done_f=1; break; fi
    sleep 2
  done
  if [ "$done_f" != 1 ]; then
    dsh "cat $DTMP/$leg.applog.txt" >&2 || true
    fail "leg $leg: app rc file never appeared"
  fi
  pullv "$DTMP/$leg.apprc" "$BUILD/$leg.apprc"
  if ! cmp -s "$BUILD/$leg.apprc" <(printf 'RC=0\n'); then
    dsh "cat $DTMP/$leg.applog.txt" >&2 || true
    fail "leg $leg: app rc file is not EXACTLY 'RC=0<newline>' (got: '$(cat "$BUILD/$leg.apprc")')"
  fi
  pullv "$DTMP/$leg.trace.txt" "$BUILD/$leg.dev-trace.txt"
  pullv "$DTMP/$leg.applog.txt" "$BUILD/$leg.dev-applog.txt"
  # review-100 M3: the degraded dir-durability token must be ABSENT on
  # every DEVICE leg — the product path /mnt/mlfk-data is a real dir, so
  # open(dir) must succeed and every save reports the plain 'saved'. A
  # saved-nodirsync here means dir open failed on the product path (the
  # M3 refutation shape) — STOP, do not weaken the assert.
  [ "$(count_xl "$BUILD/$leg.dev-applog.txt" "foh_persist: saved-nodirsync")" = 0 ] \
    || fail "leg $leg: the degraded 'foh_persist: saved-nodirsync' token appeared on the DEVICE product path — dir open failed on /mnt/mlfk-data (M3 refutation shape: STOP and report the measured device behavior)"
}

# strict foh summary assert for an unpaced flow leg (needle + anchored
# grammar; skips/fails structurally 0 at pace 0)
assert_leg_summary() { # <log> <shots>
  local log="$1" wshots="$2" pcnt cnt
  pcnt="$(grep -cF 'foh_dev foh:' "$log")" || true
  [ "$pcnt" = 1 ] || grammar_die "app log $log has $pcnt 'foh_dev foh:' needles (want 1)"
  cnt="$(count_e "$log" "^foh_dev foh: (0|[1-9][0-9]{0,5}) ticks, (0|[1-9][0-9]{0,3}) transitions, ${wshots} shots, 0 render skips, 0 failed presents, launched=0\$")"
  [ "$cnt" = 1 ] || grammar_die "app log $log: foh summary fails the unpaced-leg grammar (want shots=$wshots, skips=0, fails=0, launched=0)"
}

# --- [6] SESSION A -------------------------------------------------------------
echo "== [6/10] SESSION A: settings edit + record write (product dir) =="
arm_park
run_leg dp01 p01-persist-edit
assert_leg_summary "$BUILD/dp01.dev-applog.txt" 1
[ "$(count_xl "$BUILD/dp01.dev-applog.txt" "foh_persist: reset cause=missing")" = 1 ] \
  || grammar_die "dp01: expected exactly one missing-reset line (fresh product dir)"
[ "$(count_xl "$BUILD/dp01.dev-applog.txt" "foh_persist: saved")" = 1 ] \
  || grammar_die "dp01: expected exactly one saved line"
cmp "$BUILD/dp01.dev-trace.txt" "$HP/p01a/trace.txt" \
  || fail "dp01: device trace != host trace (flow mode is frame-exact)"
pullv "$DTMP/dp01-shots/opt-edited.ppm" "$BUILD/dp01.opt-edited.ppm"
cmp "$BUILD/dp01.opt-edited.ppm" "$HP/p01a/shots/opt-edited.ppm" \
  || fail "dp01: options-edited shot != host twin (byte-exact judgment)"
pullv "$DFILE" "$BUILD/pull1.dat"
verify_persist_file "$BUILD/pull1.dat" "device pull #1"
cmp "$BUILD/pull1.dat" "$FILE_P01" \
  || fail "device persist file (pull #1) != the host p01 reference — the format is not byte-deterministic cross-platform (refutation shape (d): STOP)"
echo "   dp01 OK: device file BYTE-IDENTICAL to the host reference"
# the record arm ON DEVICE (no SDL — plain RC-echo dsh run)
dsh "cd $DTMP && MLFK_MENU_IMG1=$DTMP/menu.img1 ./foh_device --tooth-persist-finish $REC_CHAR $REC_TSTAGE $REC_BITS 2> $DTMP/arm.applog.txt" \
  || { dsh "cat $DTMP/arm.applog.txt" >&2 || true; fail "device record arm failed"; }
pullv "$DTMP/arm.applog.txt" "$BUILD/arm.dev-applog.txt"
[ "$(count_xl "$BUILD/arm.dev-applog.txt" "foh_persist: loaded")" = 1 ] \
  || grammar_die "device arm: loaded line"
[ "$(count_xl "$BUILD/arm.dev-applog.txt" "foh_persist: record char=$REC_CHAR tstage=$REC_TSTAGE improved=1")" = 1 ] \
  || grammar_die "device arm: record improved=1 line"
[ "$(count_xl "$BUILD/arm.dev-applog.txt" "foh_persist: saved")" = 1 ] \
  || grammar_die "device arm: saved line"
pullv "$DFILE" "$BUILD/pull2.dat"
verify_persist_file "$BUILD/pull2.dat" "device pull #2"
cmp "$BUILD/pull2.dat" "$FILE_REC" \
  || fail "device persist file (pull #2) != the host post-record reference"
echo "   device record arm OK: pull #2 == host post-record reference"
unpark

# --- [7] POWER CYCLE (pre-registered primary form: reboot over ADB) -----------
echo "== [7/10] POWER CYCLE: device reboot + bounded adbd wait =="
dsh "sync"
# H1 (review-100): capture the PRE boot identity HOST-SIDE before the
# dispatch. The offline-witness below proves the device went DOWN, but a
# silently-failed reboot + a USB/adbd blip can fake that; the boot-id
# judge (POST != PRE, POST uptime < gap) is the identity-grade witness.
BOOTID_PRE="$(capture_bootid)"
BOOTID_SRC="${BOOTID_PRE%% *}"
BOOTID_PRE_ID="${BOOTID_PRE#* }"
echo "   boot-identity source=$BOOTID_SRC PRE=$BOOTID_PRE_ID captured (identity-grade reboot witness)"
# dispatch the device's own /sbin/reboot DETACHED via the HOUSE detach
# recipe (CLAUDE.md §Device access: setsid … </dev/null + a trailing
# sleep). MEASURED (iter 100 evidence round): a raw `adb shell "… &"`
# dispatch is killed by this old adbd's session teardown before the
# detach takes — the device never went down; the dsh form (the sleep-2
# delay lets the RC marker return first) took it down within 2 s and
# adbd returned healthy (uptime 0, /mnt mounted, gmenu2x live).
dsh "setsid sh -c 'sleep 2; /sbin/reboot' </dev/null >/dev/null 2>&1 & sleep 1" \
  || fail "reboot dispatch failed"
BOOT_T0=$(date +%s)
# the OFFLINE witness first: the device must actually GO DOWN — a
# reboot that never took effect must not pass as a power cycle.
down=0
for _ in $(seq 1 30); do
  sleep 2
  st="$(adb -s "$DEV" get-state 2>/dev/null || true)"
  st="${st//$'\r'/}"
  if [ "$st" != "device" ]; then down=1; break; fi
done
[ "$down" = 1 ] || fail "device never went offline after the reboot dispatch — the power cycle did NOT happen (do not pass a non-cycle as a cycle)"
up=0
for _ in $(seq 1 60); do
  sleep 2
  st="$(adb -s "$DEV" get-state 2>/dev/null || true)"
  st="${st//$'\r'/}"
  if [ "$st" = "device" ]; then
    if dsh "echo up" >/dev/null 2>&1; then up=1; break; fi
  fi
done
if [ "$up" != 1 ]; then
  fail "adbd did not return within the bounded 120 s window after reboot — REFUTATION SHAPE (a): engage the registered fallback form (two app sessions + sync + drop_caches), record the measurement (AGENT-LOG iter 100); do NOT retry blind"
fi
BOOTWAIT_S=$(( $(date +%s) - BOOT_T0 ))
require_device
rig_devsha_selftest
# H1: the identity-grade JUDGE (the real witness; the offline check and
# BOOTWAIT_S are now diagnostics). POST identity must DIFFER from PRE (a
# real cycle happened) AND the rebooted uptime must be younger than the
# host-measured dispatch->read gap (a FRESH boot, not a stale one).
BOOTID_POST="$(capture_bootid)"
BOOTID_POST_SRC="${BOOTID_POST%% *}"
BOOTID_POST_ID="${BOOTID_POST#* }"
# M-1 (review-104): validate the RAW /proc/uptime bytes against the exact
# MEASURED producer shape FIRST — raw_single_line enforces the byte-exact
# CRLF terminator + printable body (no de-CR, no squeeze), then the token
# grammar requires two space-separated decimals with EXACTLY two fractional
# digits (kernel `%lu.%02lu`; measured 2026-07-20). Extract integer seconds
# of field 1 — never a permissive split.
UPT_RAW="$BUILD/.uptime.raw.$$"; rm -f "$UPT_RAW"
adb -s "$DEV" shell 'cat /proc/uptime 2>/dev/null' > "$UPT_RAW" 2>/dev/null || true
UPT_LINE="$(raw_single_line "$UPT_RAW")" \
  || fail "boot-identity: POST /proc/uptime capture invalid (not one CRLF-terminated printable line)"
rm -f "$UPT_RAW"
[[ "$UPT_LINE" =~ ^([0-9]+)\.[0-9]{2}\ [0-9]+\.[0-9]{2}$ ]] \
  || fail "boot-identity: POST /proc/uptime line does not match the exact '<up>.NN <idle>.NN' shape ('$UPT_LINE')"
POST_UPTIME="${BASH_REMATCH[1]}"   # integer seconds of field 1
[[ "$POST_UPTIME" =~ ^(0|[1-9][0-9]{0,7})$ ]] || fail "boot-identity: POST uptime integer seconds '$POST_UPTIME' not canonical"
BOOT_GAP=$(( $(date +%s) - BOOT_T0 ))
bootid_judge "$BOOTID_SRC" "$BOOTID_PRE_ID" "$BOOTID_POST_SRC" "$BOOTID_POST_ID" "$POST_UPTIME" "$BOOT_GAP"
echo "   boot-identity JUDGED ($BOOTID_SRC): PRE=$BOOTID_PRE_ID != POST=$BOOTID_POST_ID; POST uptime ${POST_UPTIME}s < dispatch->read gap ${BOOT_GAP}s (a real fresh cycle)"
# tmpfs wiped by the reboot: re-provision (same stamp, provenance re-verified)
dsh "rm -rf $DTMP $DSD && mkdir -p $DTMP $DSD"
provision
echo "   device rebooted (adbd back in ${BOOTWAIT_S}s); re-provisioned + sha-verified"

# --- [8] SESSION B -------------------------------------------------------------
echo "== [8/10] SESSION B: post-power-cycle load + display witnesses =="
arm_park
run_leg dp02 p02-persist-verify
assert_leg_summary "$BUILD/dp02.dev-applog.txt" 2
[ "$(count_xl "$BUILD/dp02.dev-applog.txt" "foh_persist: loaded")" = 1 ] \
  || grammar_die "dp02: expected exactly one loaded line (the power-cycle survival witness)"
c="$(grep -cF "foh_persist: reset" "$BUILD/dp02.dev-applog.txt")" || true
[ "$c" = 0 ] || fail "dp02: a reset line appeared after the power cycle — the persisted file did NOT survive/parse"
[ "$(count_xl "$BUILD/dp02.dev-applog.txt" "foh_persist: saved")" = 1 ] \
  || grammar_die "dp02: expected exactly one saved line (the B-exit resave)"
cmp "$BUILD/dp02.dev-trace.txt" "$HP/p02twin/trace.txt" \
  || fail "dp02: device trace != host twin trace"
for s in opt-persisted tss-record; do
  pullv "$DTMP/dp02-shots/$s.ppm" "$BUILD/dp02.$s.ppm"
  cmp "$BUILD/dp02.$s.ppm" "$HP/p02twin/shots/$s.ppm" \
    || fail "dp02: $s shot != the host PERSISTED twin (the records/settings READ path is broken on device)"
done
pullv "$DFILE" "$BUILD/pull3.dat"
verify_persist_file "$BUILD/pull3.dat" "device pull #3"
cmp "$BUILD/pull3.dat" "$BUILD/pull2.dat" \
  || fail "pull #3 != pull #2 — the persisted bytes did not round-trip the power cycle"
cmp "$BUILD/pull3.dat" "$FILE_REC" \
  || fail "pull #3 != the host post-record reference"
echo "   SESSION B OK: loaded (no reset), file byte-identical across the power cycle, PERSONAL BEST $REC_DISPLAY shot byte-exact vs host twin"

# --- [9] DEVICE TEETH ----------------------------------------------------------
echo "== [9/10] device teeth: loud corrupt reset + save recovery on the product surface =="
# T-D1: nibble-flipped COPY pushed over the real file -> loud reset
sed "s/^rec $REC_CHAR $REC_TSTAGE $REC_BITS\$/rec $REC_CHAR $REC_TSTAGE 402e000000000000/" \
  "$BUILD/pull3.dat" > "$BUILD/corrupt.dat"
rc=0; cmp -s "$BUILD/corrupt.dat" "$BUILD/pull3.dat" || rc=$?
[ "$rc" = 1 ] || fail "T-D1: corrupt variant is a no-op (dead tooth)"
adb -s "$DEV" push "$BUILD/corrupt.dat" "$DFILE" >/dev/null
hsum="$(rig_host_sha256 "$BUILD/corrupt.dat")" || exit 1
dsum="$(rig_dev_sha256 "$DFILE")" || exit 1
[ "$dsum" = "$hsum" ] || fail "T-D1: corrupt push sha mismatch"
run_leg dpc p00-persist-probe
[ "$(count_xl "$BUILD/dpc.dev-applog.txt" "foh_persist: reset cause=corrupt detail=sum")" = 1 ] \
  || grammar_die "T-D1: expected exactly one corrupt/sum reset line on the device"
teeth=$((teeth + 1))
echo "    T-D1 OK: corrupted product file resets LOUDLY on device (never silent zeroes)"
# T-D2: recovery — the next options save publishes DEFAULTS
run_leg dp03 p02-persist-verify
[ "$(count_xl "$BUILD/dp03.dev-applog.txt" "foh_persist: reset cause=corrupt detail=sum")" = 1 ] \
  || grammar_die "T-D2: recovery leg should boot from the corrupt file (loud again)"
[ "$(count_xl "$BUILD/dp03.dev-applog.txt" "foh_persist: saved")" = 1 ] \
  || grammar_die "T-D2: recovery leg saved-line count"
pullv "$DFILE" "$BUILD/pull4.dat"
verify_persist_file "$BUILD/pull4.dat" "device pull #4 (recovery)"
cmp "$BUILD/pull4.dat" "$FILE_DEFAULTS" \
  || fail "T-D2: the recovery save != the host defaults-control file (reset-to-defaults is not the authored defaults)"
pullv "$DTMP/dp03-shots/tss-record.ppm" "$BUILD/dp03.tss-record.ppm"
cmp "$BUILD/dp03.tss-record.ppm" "$HP/p02ctrl/shots/tss-record.ppm" \
  || fail "T-D2: post-reset PERSONAL BEST shot != the host defaults control"
teeth=$((teeth + 1))
echo "    T-D2 OK: recovery save publishes the authored defaults (file + display)"
unpark

# --- [10] hygiene + verdict -----------------------------------------------------
echo "== [10/10] hygiene =="
# M4 (review-100): the pre-existing user file restore is VERDICT-BOUND —
# done HERE, before the PERSIST OK line, and BYTE-VERIFIED (pull-back
# sha == backup sha). The EXIT trap stays a backstop for failure/death
# paths only (PREEXIST_RESTORED then makes it a no-op).
if [ "$PREEXIST" = 1 ]; then
  [ -f "$BUILD/preexisting-mlfk-persist.dat" ] \
    || fail "M4: PREEXIST set but the backup is missing — cannot restore the user's data"
  adb -s "$DEV" push "$BUILD/preexisting-mlfk-persist.dat" "$DFILE" >/dev/null \
    || fail "M4: could not restore the pre-existing $DFILE before the verdict"
  pull_bytes "$DFILE" "$BUILD/preexist-restored.dat" \
    || fail "M4: could not pull back the restored pre-existing file for byte-verification"
  bsum="$(rig_host_sha256 "$BUILD/preexisting-mlfk-persist.dat")" || exit 1
  rsum="$(rig_host_sha256 "$BUILD/preexist-restored.dat")" || exit 1
  [ "$bsum" = "$rsum" ] \
    || fail "M4: restored pre-existing $DFILE sha ($rsum) != backup sha ($bsum) — the user's file was NOT faithfully restored"
  PREEXIST_RESTORED=1
  echo "   pre-existing $DFILE restored + BYTE-VERIFIED (sha $bsum) before the verdict"
else
  dsh "rm -f $DFILE $DDATA/mlfk-persist.tmp"
  dsh "test ! -f $DFILE"
fi
# frontend liveness probe (best effort; the respawn belongs to the OS;
# comm-scan — the iter-74 pidof caveat is for script daemons, but the
# scan form is the proven one)
gm=0
for _ in $(seq 1 10); do
  if dsh "grep -lx gmenu2x /proc/[0-9]*/comm >/dev/null 2>&1" >/dev/null 2>&1; then gm=1; break; fi
  sleep 2
done
[ "$gm" = 1 ] || echo "WARN: gmenu2x not observed running after unpark (frontend respawn is OS-owned; verify by eye)" >&2
rig_no_commit_guard "$BUILD" "$DEVB" "$TABLES"

echo "PERSIST OK (sessions=2 powercycle=reboot bootid=${BOOTID_SRC}:PRE!=POST bootwait=${BOOTWAIT_S}s legs=5 pulls=4 roundtrip=byte-exact record=$REC_DISPLAY resets missing=1 loud-corrupt=2 dirsync=plain-saved+degraded-tooth teeth=$teeth)"
