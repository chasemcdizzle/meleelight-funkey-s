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
#  [4] HOST TEETH on copies (T-H1..T-H7): corrupt-sum / version-bump /
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
cleanup() {
  rig_dsh_retry "pkill foh_device; true" \
    || echo "WARN: could not pkill foh_device on the device" >&2
  # product-surface residue: our test bytes leave; a pre-existing user
  # file returns (pulled aside in [5]).
  if [ "$PREEXIST" = 1 ] && [ -s "$BUILD/preexisting-mlfk-persist.dat" ]; then
    if adb -s "$DEV" push "$BUILD/preexisting-mlfk-persist.dat" "$DFILE" >/dev/null 2>&1; then
      echo "   pre-existing $DFILE restored" >&2
    else
      echo "WARN: could not restore the pre-existing $DFILE (copy kept at $BUILD/preexisting-mlfk-persist.dat)" >&2
    fi
  else
    rig_dsh_retry "rm -f $DFILE $DDATA/mlfk-persist.tmp" \
      || echo "WARN: could not wipe the persist test residue in $DDATA" >&2
  fi
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
JUDGE_SHA=2267f8b796b1881d6ef749b5931a5fb08ae9f914b7a67a0e2608d4cada99616e
have="$(rig_host_sha256 "$FOH/judge-foh-trace.js")" || exit 1
[ "$have" = "$JUDGE_SHA" ] || fail "judge-foh-trace.js sha $have != pinned $JUDGE_SHA (reviewed pin update in the same commit)"
c="$(grep -cF "$JUDGE_SHA port/foh/judge-foh-trace.js" "$FOH/check-foh-flows.sh")" || true
[ "$c" = 1 ] || fail "twin pin — check-foh-flows.sh does not carry the same judge sha exactly once (count $c; paired change rule)"
bash pipeline/extractor/build-extractor.sh
rm -f "$TABLES/ml_tables.c" "$TABLES/ml_tables.h" \
  "$TABLES/ml_stages.c" "$TABLES/ml_stages.h" \
  "$TABLES/ml_targets.c" "$TABLES/ml_targets.h"
node pipeline/run.js --only animations,tables,stages,targets --out "$TABLES"
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
  "$FOH/foh_render.c" "$FOH/foh_persist.c" "$GFX/platform_headless.c" \
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
# tapjump P2 (row 2 col 1); shot; B-exit = the upstream cookie-save
# point (gameplaymenu.js:29-33) -> foh_persist save.
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
I 430 D
I 431 -
I 435 R
I 436 -
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
  rm -rf "$od"
  mkdir -p "$od/shots"
  MLFK_PERSIST_DIR="$pd" \
  "$BUILD/foh_dev_headless" --flow "$FLOWD/$id.flow" --input flow \
    --flow-out "$od/trace.txt" --shots-dir "$od/shots" --pace 0 \
    2> "$od/log.txt" || { cat "$od/log.txt" >&2; fail "host leg $id failed"; }
  made "$od/trace.txt" "$od/log.txt"
  node "$FOH/judge-foh-trace.js" "$od/trace.txt" "$id" 0 >/dev/null \
    || fail "host leg $id: trace failed the pinned judge"
}

# strict MLFKPERSIST1 file verification, INDEPENDENT of the C loader
# (the whitelist-grammar rule: anchored full-line counts + a shasum
# recompute of the SUM seal).
verify_persist_file() { # <file> <ctx>
  local f="$1" ctx="$2" nl sum want
  made "$f"
  nl="$(grep -c "" "$f")" || fail "$ctx: cannot count lines"
  [ "$nl" = 55 ] || grammar_die "$ctx: $nl lines != 55 (MLFKPERSIST1 is exactly 55 LF lines)"
  [ "$(count_xl "$f" "MLFKPERSIST1")" = 1 ] || grammar_die "$ctx: header line missing/duplicated"
  [ "$(sed -n 1p "$f")" = "MLFKPERSIST1" ] || grammar_die "$ctx: line 1 is not the header"
  [ "$(count_e "$f" '^turbo [01]$')" = 1 ] || grammar_die "$ctx: turbo line grammar"
  [ "$(count_e "$f" '^lcancel [0-2]$')" = 1 ] || grammar_die "$ctx: lcancel line grammar"
  [ "$(count_e "$f" '^tapjump [01] [01] [01] [01]$')" = 1 ] || grammar_die "$ctx: tapjump line grammar"
  [ "$(count_e "$f" '^rec [0-4] [0-9] [0-9a-f]{16}$')" = 50 ] || grammar_die "$ctx: rec row count != 50"
  [ "$(count_e "$f" '^SUM [0-9a-f]{64}$')" = 1 ] || grammar_die "$ctx: SUM line grammar"
  sum="$(sed -n 55p "$f")"
  [[ "$sum" =~ ^SUM\ ([0-9a-f]{64})$ ]] || grammar_die "$ctx: line 55 is not the SUM line"
  sum="${BASH_REMATCH[1]}"
  want="$(head -n 54 "$f" | shasum -a 256 | cut -d' ' -f1)" || fail "$ctx: shasum failed"
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

# --- [4] host teeth (COPIES; the probe flow boots the loader) -----------------
echo "== [4/10] host teeth =="
tooth_boot() { # <name> <persist-dir> <want-line>
  local nm="$1" pd="$2" wantln="$3"
  run_host p00-persist-probe "$HP/tooth-$nm" "$pd"
  [ "$(count_xl "$HP/tooth-$nm/log.txt" "$wantln")" = 1 ] \
    || grammar_die "$nm: expected exactly one '$wantln' line"
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
# T-H2 version bump WITH a recomputed (valid) SUM
mk_pdir "$HP/th2" -
{ printf 'MLFKPERSIST2\n'; tail -n +2 "$FILE_REC" | head -n 53; } > "$HP/th2/body"
{ cat "$HP/th2/body"; printf 'SUM %s\n' "$(shasum -a 256 "$HP/th2/body" | cut -d' ' -f1)"; } \
  > "$HP/th2/mlfk-persist.dat"
rm -f "$HP/th2/body"
tooth_boot h2 "$PWD/$HP/th2" "foh_persist: reset cause=version"
teeth=$((teeth + 1))
echo "    T-H2 OK: version bump (checksum-valid) resets loudly"
# T-H3 domain: NaN record bits with a recomputed SUM
mk_pdir "$HP/th3" -
sed "s/^rec $REC_CHAR $REC_TSTAGE $REC_BITS\$/rec $REC_CHAR $REC_TSTAGE 7ff8000000000000/" \
  "$FILE_REC" | head -n 54 > "$HP/th3/body"
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

# --- [5] arm build + push + pre-existing product state ------------------------
echo "== [5/10] armv7 build (shared rig stamp) + push + provenance =="
rig_arm_build
rig_stamp_rehash foh_device
dsh "rm -rf $DTMP $DSD && mkdir -p $DTMP $DSD"
provision() { # push binary + flows into a fresh $DTMP (rerun post-reboot)
  adb -s "$DEV" push "$DEVB/foh_device" \
    "$FLOWD/p00-persist-probe.flow" "$FLOWD/p01-persist-edit.flow" \
    "$FLOWD/p02-persist-verify.flow" "$DTMP/" >/dev/null
  rig_push_provenance "$DTMP" foh_device
  dsh "chmod +x $DTMP/foh_device"
  local hf bn hsum dsum
  for hf in "$FLOWD/p00-persist-probe.flow" "$FLOWD/p01-persist-edit.flow" \
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
    pullv "$DFILE" "$BUILD/preexisting-mlfk-persist.dat"
    PREEXIST=1
    dsh "rm -f $DFILE $DDATA/mlfk-persist.tmp"
    echo "   pre-existing $DFILE pulled aside (restored at cleanup)"
    ;;
  1) dsh "rm -f $DDATA/mlfk-persist.tmp" ;;
  *) fail "cannot probe for a pre-existing $DFILE (rc $prc)" ;;
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
setsid sh -c './foh_device --flow $DTMP/$id.flow --input flow \\
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
dsh "cd $DTMP && ./foh_device --tooth-persist-finish $REC_CHAR $REC_TSTAGE $REC_BITS 2> $DTMP/arm.applog.txt" \
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
# our test residue leaves the product dir now (cleanup also covers the
# failure paths); a pre-existing user file is restored by the trap.
if [ "$PREEXIST" != 1 ]; then
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

echo "PERSIST OK (sessions=2 powercycle=reboot bootwait=${BOOTWAIT_S}s legs=5 pulls=4 roundtrip=byte-exact record=$REC_DISPLAY resets missing=1 loud-corrupt=2 teeth=$teeth)"
