#!/usr/bin/env bash
# port/sim/device/verify_m3.sh — THE M3 EXIT GATE (CLAUDE.md §Commands
# "M3 EXIT GATE"; PLAN §4/M3 EXIT verbatim; assembled by fix_plan §M3
# task 7, iter 58). Prints `M3 GATE OK`, exit 0, iff ALL of:
#
#   [0] FREEZE MANIFEST (PROCESS §4 reviewed-pin rule — "hashes prove
#       identity, not approval"): every evidence PRODUCER listed in
#       port/sim/device/m3-freeze-manifest.txt (check scripts, riglib,
#       adbsh, judges, pack-snd.js, wrap/trace tools, verify-stream.js,
#       the OPK assets, this script itself) re-hashes to its pinned
#       sha256 BEFORE any leg runs. ANY mismatch, grammar violation,
#       missing producer, or extra entry = HARD REFUSAL — prior gate
#       evidence is invalid the moment a producer changes; the gate
#       reruns only after the manifest is re-pinned by a REVIEWED
#       change (the update path documented in the manifest header).
#   [1] DEVICE CONFORMANCE — every golden in oracle/goldens/
#       manifest.json replayed ON the FunKey-S, judged host-side by the
#       UNCHANGED oracle/harness/verify-stream.js vs the frozen
#       *.sha256.json (exact per-frame hash equality, FULL length,
#       rngCalls/rngCallsOutsideStep/specVersion pins) — engine:
#       port/sim/device/check-device-conform.sh (Tier-A arc GO).
#   [2] PERF — g01 full match ON DEVICE with live SDL1.2 render AND the
#       44100/S16LSB/2ch/512 audio callback + 8-voice SFX mixer
#       running; per-frame wall-clock logged in-app to tmpfs, pulled,
#       judged host-side: full p99 < 16.67 ms, underruns == 0 —
#       engine: port/gfx/check-device-audio.sh (its pre-registered
#       internal <= 2-attempt retry for the skip/underrun transient
#       class is the ONLY sanctioned retry; the attempt count is
#       surfaced in this gate's output, never hidden).
#   [3] OPK — packaged with the SDK container's mksquashfs 4.4 ONLY,
#       launched via the FRONTEND path (gmenu2x, uinput-driven), boot
#       marker + in-app screenshot pulled and judged (installed-binary
#       identity == stamp; structural + non-blank) — engine:
#       port/gfx/check-device-opk.sh.
#   [4] LIVE SESSION — the S1-input session driven through the real
#       SDL keysym path (uinput injector); the recorded input trace
#       replays to byte-identical checksum streams host x2 + device
#       (three-way cmp, plus the live stream itself) — engine:
#       port/gfx/check-device-input.sh (Tier-A arc GO).
#
# SUB-CHECK REUSE DISCIPLINE (PROCESS §3): the legs INVOKE the
# arc-hardened check scripts — no device logic is reimplemented here.
# Each leg's PASS verdict is parsed by an EXACT ANCHORED grammar with
# an exactly-one-match posture; a leg that exits 0 without exactly its
# expected verdict line(s) — or with a resembling-but-different line —
# is CORRUPTION, never a pass. Leg output goes to
# port/sim/calib/build/device/verify-m3/leg-*.log (gitignored).
#
# The gate does NOT print the LOOP STOP sentinel — a pass is followed
# by the driver's phase-advance duty
# (`LOOP STOP: m3-device — needed: Chase S1 ratification playtest`).
#
# Env: FUNKEY_ADB_ID (device id), MLFK_FORCE_ARM=1 (forwarded to the
# legs' shared arm build), MLFK_M3_FAKE_LEG_DIR (NEGATIVE TESTING
# ONLY: replay canned leg logs/rcs instead of running the real legs —
# proves the verdict parsers' corruption teeth without device time;
# the freeze manifest is verified regardless).
set -euo pipefail
cd "$(dirname "$0")/../../.."

DEVB=port/sim/calib/build/device
VDIR=$DEVB/verify-m3
MANIFEST=port/sim/device/m3-freeze-manifest.txt
mkdir -p "$VDIR"

# rig_host_sha256 (strict full-line shasum grammar) comes from riglib;
# sourcing defines helpers only — this script takes NO rig lock and
# runs NO device command itself (the legs own the lock, serially).
source port/sim/device/adbsh.sh
source port/sim/device/riglib.sh

# The pinned producer set: the manifest must list EXACTLY these paths
# (a truncated manifest can never silently narrow gate coverage).
REQUIRED_PRODUCERS="port/sim/device/adbsh.sh
port/sim/device/riglib.sh
port/sim/device/check-device-g01.sh
port/sim/device/check-device-conform.sh
port/sim/device/percentiles.js
port/gfx/check-device-render.sh
port/gfx/judge-render-timing.js
port/gfx/judge-shot.js
port/gfx/check-device-input.sh
port/gfx/judge-s1-coverage.js
port/gfx/s1-session.script
port/tools/fk_input.c
port/gfx/check-device-audio.sh
port/gfx/judge-audio-summary.js
port/gfx/pack-snd.js
port/gfx/check-device-opk.sh
port/gfx/opk/mlfk.sh
port/gfx/opk/meleelight.funkey-s.desktop
port/gfx/opk/icon32.png
port/sim/sim/wrap-run.js
port/sim/sim/trace-to-txt.js
oracle/harness/verify-stream.js
port/sim/device/verify_m3.sh"

echo "== [0] freeze-manifest verification (PROCESS §4 — refusal before any leg) =="
if [ ! -f "$MANIFEST" ]; then
  echo "M3 GATE REFUSED: freeze manifest $MANIFEST missing" >&2
  exit 1
fi
# grammar pass: every non-comment, non-empty line must be EXACTLY
#   <64-lowercase-hex><SP><path><SP><status><SP><cite>
# status in the closed token set; anything that merely resembles a
# record is corruption -> refusal.
n_data=0
while IFS= read -r mline; do
  case "$mline" in
    '#'*|'') continue ;;
  esac
  if ! [[ "$mline" =~ ^[0-9a-f]{64}\ [A-Za-z0-9._/-]+\ (reviewed-go|oracle-frozen|grandfathered-m2|arc-in-flight|arc-pending)\ [A-Za-z0-9._/:+-]+$ ]]; then
    echo "M3 GATE REFUSED: manifest line fails the anchored grammar: '$mline'" >&2
    exit 1
  fi
  n_data=$((n_data + 1))
done < "$MANIFEST"
n_req=0
while IFS= read -r prod; do
  [ -n "$prod" ] || continue
  n_req=$((n_req + 1))
  # exactly ONE record per required producer (duplicates = corruption)
  mcount="$(awk -v p="$prod" 'substr($0,1,1) != "#" && $2 == p' "$MANIFEST" | grep -c '' )" || true
  if [ "$mcount" != 1 ]; then
    echo "M3 GATE REFUSED: manifest has $mcount records for producer $prod (want exactly 1)" >&2
    exit 1
  fi
  mrec="$(awk -v p="$prod" 'substr($0,1,1) != "#" && $2 == p' "$MANIFEST")"
  msha="${mrec%% *}"
  if [ ! -f "$prod" ]; then
    echo "M3 GATE REFUSED: pinned producer $prod is missing from the tree" >&2
    exit 1
  fi
  cursha="$(rig_host_sha256 "$prod")" || {
    echo "M3 GATE REFUSED: could not hash producer $prod" >&2
    exit 1
  }
  if [ "$cursha" != "$msha" ]; then
    mstatus="$(printf '%s\n' "$mrec" | awk '{print $3}')"
    echo "M3 GATE REFUSED: producer $prod bytes do not match its pinned sha" >&2
    echo "  pinned:  $msha ($mstatus)" >&2
    echo "  current: $cursha" >&2
    echo "  A producer change invalidates prior gate evidence — land the change" >&2
    echo "  through its review arc and re-pin the manifest (reviewed change only)." >&2
    exit 1
  fi
done <<< "$REQUIRED_PRODUCERS"
if [ "$n_data" != "$n_req" ]; then
  echo "M3 GATE REFUSED: manifest carries $n_data records, pinned producer set has $n_req — extra/unknown entries are corruption" >&2
  exit 1
fi
echo "   freeze manifest OK: $n_req producers, all bytes match their pins"

# --- leg machinery -----------------------------------------------------------
run_leg() { # <name> <script> — runs (or replays) a leg, dies on rc != 0
  local name="$1" script="$2" rc=0 legf
  legf="$VDIR/leg-$name.log"
  rm -f "$legf"
  if [ -n "${MLFK_M3_FAKE_LEG_DIR:-}" ]; then
    echo "WARN: MLFK_M3_FAKE_LEG_DIR set — replaying canned '$name' leg evidence (NEGATIVE TESTING ONLY)" >&2
    cp "$MLFK_M3_FAKE_LEG_DIR/leg-$name.log" "$legf" 2>/dev/null || {
      echo "M3 GATE FAIL: canned leg log for '$name' missing" >&2
      exit 1
    }
    rc="$(cat "$MLFK_M3_FAKE_LEG_DIR/leg-$name.rc" 2>/dev/null)" || rc=missing
    if ! [[ "$rc" =~ ^[0-9]{1,3}$ ]]; then
      echo "M3 GATE FAIL: canned leg rc for '$name' malformed ('$rc')" >&2
      exit 1
    fi
  else
    bash "$script" > "$legf" 2>&1 || rc=$?
  fi
  if [ "$rc" != 0 ]; then
    tail -30 "$legf" >&2
    echo "M3 GATE FAIL: leg '$name' exited $rc (full log: $legf)" >&2
    exit 1
  fi
}

expect_exactly() { # <legf> <fixed-verdict-literal>
  local c
  c="$(grep -cxF "$2" "$1")" || true
  if [ "$c" != 1 ]; then
    echo "M3 GATE FAIL: expected exactly 1 verdict line in $1, found $c:" >&2
    echo "  want: $2" >&2
    echo "  — a leg that exits 0 without its exact verdict is CORRUPT evidence" >&2
    exit 1
  fi
}

echo "== [1/4] DEVICE CONFORMANCE (engine: check-device-conform.sh) =="
run_leg conform port/sim/device/check-device-conform.sh
expect_exactly "$VDIR/leg-conform.log" "DEVICE CONFORMS 8/8"
expect_exactly "$VDIR/leg-conform.log" "SIM P99 OK"
LEG1="DEVICE CONFORMS 8/8 + SIM P99 OK"
echo "   leg 1 PASS: $LEG1"

echo "== [2/4] PERF: full match, live render + audio callback (engine: check-device-audio.sh) =="
run_leg audio port/gfx/check-device-audio.sh
AUDIO_RE='^DEVICE AUDIO OK \(full p99 [0-9]{1,9}\.[0-9]{3} ms, underruns=0, attempts=[12]; cbs=[0-9]{1,12} starts=[0-9]{1,12} stops=[0-9]{1,12} skips=0/3600\)$'
acnt="$(grep -Ec "$AUDIO_RE" "$VDIR/leg-audio.log")" || true
if [ "$acnt" != 1 ]; then
  echo "M3 GATE FAIL: expected exactly 1 DEVICE AUDIO OK line matching the pinned grammar in $VDIR/leg-audio.log, found $acnt — corrupt leg evidence" >&2
  exit 1
fi
aline="$(grep -E "$AUDIO_RE" "$VDIR/leg-audio.log")"
if [[ "$aline" =~ attempts=([12])\; ]]; then
  attempts="${BASH_REMATCH[1]}"
else
  echo "M3 GATE FAIL: could not extract the attempt count from '$aline'" >&2
  exit 1
fi
LEG2="$aline [retries used: $((attempts - 1)) of 1 — skip/underrun class only]"
echo "   leg 2 PASS: $LEG2"

echo "== [3/4] OPK: mksquashfs-4.4 package + FRONTEND launch (engine: check-device-opk.sh) =="
run_leg opk port/gfx/check-device-opk.sh
OPK_LINE="OPK LAUNCH OK (frontend-launched via gmenu2x, boot marker bin-sha == stamp, evidence rc=0, stream prefix 900/900 vs frozen g01, shot structural)"
expect_exactly "$VDIR/leg-opk.log" "$OPK_LINE"
LEG3="$OPK_LINE"
echo "   leg 3 PASS: $LEG3"

echo "== [4/4] LIVE SESSION: S1 uinput session, three-way stream identity (engine: check-device-input.sh) =="
run_leg input port/gfx/check-device-input.sh
INPUT_LINE="S1 INPUT OK (session 1080 frames live on device, all 5 streams grammar-complete; host x2 + device replays and the live stream all byte-identical; 15 chord rows unit-swept; coverage + SOCD witness judged; tapjump oracle diverged at frame 121)"
expect_exactly "$VDIR/leg-input.log" "$INPUT_LINE"
LEG4="$INPUT_LINE"
echo "   leg 4 PASS: $LEG4"

echo ""
echo "M3 GATE SUMMARY (all legs judged host-side; device never self-reports):"
echo "  [1] conformance: $LEG1"
echo "  [2] perf+audio:  $LEG2"
echo "  [3] opk:         $LEG3"
echo "  [4] live input:  $LEG4"
echo "M3 GATE OK"
