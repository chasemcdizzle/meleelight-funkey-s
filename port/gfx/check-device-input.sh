#!/usr/bin/env bash
# M3 task 5 done-check: S1 input layer at the poll seam + uinput live
# session (fix_plan §M3 task 5). Prints
#   S1 INPUT OK (...)
# exit 0, iff ALL of:
#
#   1. S1 UNIT SWEEP (host): port/gfx/s1_sweep.c over the data-driven
#      chord table (port/gfx/s1_input.h) — all 15 pinned PLAN §6
#      chord→coordinate checks bit-exact (1/80 quantized), the
#      exhaustive 2^11 combo dump byte-stable x2, every coordinate on
#      the melee grid, S1 invariants (SOCD neutral, C-layer left-stick
#      neutral, digital shield r/rA=1).
#   2. LIVE SESSION (device): gfx_device --live runs a 1080-frame paced
#      match on the FunKey-S taking input from the REAL SDL keysym path
#      while port/tools/fk_input (our OWN uinput device — writing the
#      existing event device does not inject; ssb64 pattern) plays the
#      committed deterministic script port/gfx/s1-session.script. The
#      injector is launched only after the app's ready marker exists
#      (handshake); the app records EVERY frame's injected rows and
#      writes the trace JSON + checksum stream to tmpfs post-run; app
#      exit code proven via an rc file (detached runs drop rc).
#   3. COVERAGE (host judgment): judge-s1-coverage.js finds every
#      pre-registered chord signature (values, never frame indices —
#      the settling strategy) in the pulled recorded trace, plus the
#      S1 invariants and the strict 22-key golden-trace shape.
#   4. THREE-WAY REPLAY DETERMINISM (host judgment): the recorded trace
#      replays through the host headless sim TWICE and the device sim
#      binary ONCE — all three checksum streams byte-identical (cmp),
#      AND all three identical to the LIVE session's own stream (the
#      recording-fidelity witness). Self-consistency only: this is a
#      NEW trace, judged against itself, never against a frozen golden.
#   5. NON-VACUITY: the live stream DIFFERS from an all-neutral
#      1080-frame trace's stream (the sim provably consumed the input —
#      the four-way check alone would pass if input were ignored).
#
# The S1 session runs with --tapjump-off-p1 (the fix_plan §M3 Input
# contract) on the live app AND on every replay.
#
# Rig plumbing INHERITED from port/sim/device/riglib.sh (Tier-A arc,
# VERDICT: GO): nonce-dsh, pullv, rm-before-produce+made(), shared
# stamp-cached arm build (this script's bytes are stamp input via
# RIG_SCRIPTS), rehash-adjacent-to-push + push provenance, the shared
# no-reclaim device-keyed lock, the no-commit guard.
#
# Device hygiene: writes only /tmp/mlfk + /mnt/mlfk-scratch + the
# frontend-park marker /mnt/disable_frontend (removed on exit, trap'd);
# own processes (gfx_device, fk_input) killed on exit.
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

# --- frozen pins (M3 task 5; changing any is a reviewed repo change) ---------
SESSION_SEED=1337   # S1 live-session match params (fox vs marth on
SESSION_P1=2        # battlefield — the g01 pairing; the trace itself is
SESSION_P2=0        # NEW, self-consistency judged)
SESSION_STAGE=0
SESSION_FRAMES=1080 # 18 s paced @ 60 fps (~990 live frames post-starting;
                    # headroom for ready-poll + injector-launch latency —
                    # the ~11.6 s script must END inside the session)
BUDGET_NS=16666667  # 60 fps pacing budget
S1_SCRIPT=$GFX/s1-session.script  # committed deterministic input script
GFXDATA_FROZEN=$GFX/gfxdata-frozen.txt
GFXDATA_SHA256=5499a3dd5fc374d6ed988faf0bef6fa2e189eb314e892bdd83c7534dc0865c94
READY_TRIES=30      # x1 s: app boot to ready marker
DONE_TRIES=30       # x2 s: session end after the injector returns
ANIM_P1=anim_2_fox.bin
ANIM_P2=anim_0_marth.bin

source port/sim/device/adbsh.sh # (also defines $DEV — it keys the lock)
source port/sim/device/riglib.sh

rig_lock_acquire

# Cleanup (installed AFTER lock acquisition, riglib contract): kill our
# processes by NAME (a -f pattern would self-match the adb shell — the
# iter-50 gotcha), restore the frontend if we parked it, then the shared
# rig cleanup (device scratch + lock). Best-effort, WARN-visible.
PARKED=0
task5_cleanup() {
  dsh "pkill gfx_device; pkill fk_input; true" >/dev/null 2>&1 \
    || echo "WARN: could not pkill gfx_device/fk_input on the device" >&2
  if [ "$PARKED" = 1 ]; then
    dsh "rm -f /mnt/disable_frontend" >/dev/null 2>&1 \
      || echo "WARN: could not restore the frontend — remove /mnt/disable_frontend by hand" >&2
  fi
  rig_cleanup
}
trap task5_cleanup EXIT

require_device
rig_devsha_selftest

echo "== [1/8] host data plane (M1 tables + SIMDATA1 + GFXDATA pin + script) =="
made "$S1_SCRIPT"
# GFXDATA pin: committed input, content integrity by sha256 (freshness
# is git's — same class as the frozen goldens; task-4 precedent).
made "$GFXDATA_FROZEN"
gsum="$(shasum -a 256 "$GFXDATA_FROZEN" | cut -d' ' -f1)"
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

echo "== [2/8] host build: s1_sweep + host replay sim (-ffp-contract=off everywhere) =="
CFLAGS_COMMON=(-ffp-contract=off -Wall -Wextra -Werror
  -I"$TABLES" -Iport/ryu -Iport/sim -Ioracle/qjs)
rm -f "$BUILD/s1_sweep" "$BUILD/sim_host_s1"
cc -O2 "${CFLAGS_COMMON[@]}" -o "$BUILD/s1_sweep" "$GFX/s1_sweep.c" -lm
made "$BUILD/s1_sweep"
# host replay sim: the check-sim.sh/riglib sim_device TU list verbatim
cc -O2 "${CFLAGS_COMMON[@]}" -o "$BUILD/sim_host_s1" \
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
  oracle/qjs/sha256.c port/fdlibm/fdlibm.c -lm
made "$BUILD/sim_host_s1"
echo "   host build OK (s1_sweep + sim_host_s1)"

echo "== [3/8] S1 chord-table unit sweep (15 PLAN §6 rows, x2 byte-stable) =="
for side in a b; do
  rm -f "$BUILD/s1-sweep-$side.txt"
  "$BUILD/s1_sweep" > "$BUILD/s1-sweep-$side.txt"
  made "$BUILD/s1-sweep-$side.txt"
done
cmp "$BUILD/s1-sweep-a.txt" "$BUILD/s1-sweep-b.txt"
# iter 52 parser audit (whitelist grammar): full-line match of the
# measured s1_sweep.c OK literal (prefix-match accepted a resembling
# line; the producer emits exactly this line or S1 SWEEP FAIL).
grep -qx "S1 SWEEP OK (15 pinned chord checks, 2048 combos, all coordinates on the 1/80 grid)" "$BUILD/s1-sweep-a.txt" || {
  echo "DEVICE FAIL: S1 sweep did not print its exact OK line" >&2
  exit 1
}
grep -v "^C " "$BUILD/s1-sweep-a.txt" | sed 's/^/   /'

echo "== [4/8] armv7 build (shared rig stamp) + push + provenance =="
rig_arm_build
rig_stamp_rehash gfx_device fk_input sim_device
dsh "rm -rf $DTMP $DSD && mkdir -p $DTMP $DSD"
adb -s "$DEV" push "$DEVB/gfx_device" "$DEVB/fk_input" "$DEVB/sim_device" \
  "$DEVB/simdata.txt" "$GFXDATA_FROZEN" "$S1_SCRIPT" \
  "$TABLES/$ANIM_P1" "$TABLES/$ANIM_P2" "$DTMP/" >/dev/null
rig_push_provenance "$DTMP" gfx_device fk_input sim_device
dsh "chmod +x $DTMP/gfx_device $DTMP/fk_input $DTMP/sim_device"
# iter 52 parser audit: device digests via the strict full-line
# rig_dev_sha256 parser (was a first-nonempty-line awk scrape)
for hf in "$DEVB/simdata.txt" "$GFXDATA_FROZEN" "$S1_SCRIPT" \
          "$TABLES/$ANIM_P1" "$TABLES/$ANIM_P2"; do
  bn="$(basename "$hf")"
  hsum="$(shasum -a 256 "$hf" | cut -d' ' -f1)"
  dsum="$(rig_dev_sha256 "$DTMP/$bn")" || exit 1
  if [ "$dsum" != "$hsum" ]; then
    echo "DEVICE FAIL: pushed $bn device sha ($dsum) != host sha ($hsum)" >&2
    exit 1
  fi
done
echo "   pushed data sha-verified on device (simdata, gfxdata, script, 2 anim bins)"

echo "== [5/8] device: LIVE uinput-driven S1 session (frontend parked around the run) =="
# Generated launcher (host-expanded, pushed + sha-verified): the app is
# DETACHED (setsid, </dev/null, trailing sleep — CLAUDE.md device
# recipe) and its exit code lands in s1.apprc (this adbd drops rc; the
# rc-file is the only truthful exit evidence for a detached run).
rm -f "$BUILD/s1-launch.sh"
cat > "$BUILD/s1-launch.sh" << EOF
#!/bin/sh
# generated by check-device-input.sh — S1 live-session launcher
cd $DTMP || exit 9
rm -f s1.apprc s1.ready
setsid sh -c './gfx_device --live --record-trace $DTMP/s1.trace.json \
  --ready-file $DTMP/s1.ready \
  --simdata $DTMP/simdata.txt --gfxdata $DTMP/gfxdata-frozen.txt \
  --anim-dir $DTMP \
  --seed $SESSION_SEED --p1 $SESSION_P1 --p2 $SESSION_P2 \
  --stage $SESSION_STAGE --frames $SESSION_FRAMES \
  --pace 1 --budget-ns $BUDGET_NS --tapjump-off-p1 \
  --out $DTMP/s1.live-out.txt --timing $DTMP/s1.live-tim.txt \
  2> $DTMP/s1.app-log.txt; echo "RC=\$?" > $DTMP/s1.apprc' \
  </dev/null >/dev/null 2>&1 &
sleep 2
EOF
made "$BUILD/s1-launch.sh"
adb -s "$DEV" push "$BUILD/s1-launch.sh" "$DTMP/" >/dev/null
lsum="$(shasum -a 256 "$BUILD/s1-launch.sh" | cut -d' ' -f1)"
# iter 52 parser audit: strict full-line device digest parse
dsum="$(rig_dev_sha256 "$DTMP/s1-launch.sh")" || exit 1
if [ "$dsum" != "$lsum" ]; then
  echo "DEVICE FAIL: pushed launcher sha mismatch (device $dsum, host $lsum)" >&2
  exit 1
fi
dsh "chmod +x $DTMP/s1-launch.sh"

dsh "touch /mnt/disable_frontend && { pkill gmenu2x; true; }"
PARKED=1
t0=$(date +%s)
dsh "sh -lc $DTMP/s1-launch.sh"
# handshake: the injector starts ONLY after the app's ready marker
ready=0
for _ in $(seq 1 "$READY_TRIES"); do
  if dsh "test -f $DTMP/s1.ready" >/dev/null 2>&1; then ready=1; break; fi
  sleep 1
done
if [ "$ready" != 1 ]; then
  dsh "cat $DTMP/s1.app-log.txt" >&2 || true
  echo "DEVICE FAIL: app ready marker never appeared (${READY_TRIES}s) — SDL init/boot failure?" >&2
  exit 1
fi
echo "   app ready — launching the uinput injector (foreground, ~12 s)"
dsh "sh -lc 'cd $DTMP && ./fk_input s1-session.script'" || {
  echo "DEVICE FAIL: fk_input injector failed" >&2
  exit 1
}
# session end: the rc file appears when the detached app exits
done_f=0
for _ in $(seq 1 "$DONE_TRIES"); do
  if dsh "test -f $DTMP/s1.apprc" >/dev/null 2>&1; then done_f=1; break; fi
  sleep 2
done
t1=$(date +%s)
dsh "rm -f /mnt/disable_frontend"
PARKED=0
if [ "$done_f" != 1 ]; then
  dsh "cat $DTMP/s1.app-log.txt" >&2 || true
  echo "DEVICE FAIL: session never finished (rc file absent after $((DONE_TRIES * 2))s past injection)" >&2
  exit 1
fi
pullv "$DTMP/s1.apprc" "$DEVB/s1.apprc"
# iter 52 parser audit: EXACT whole-file compare (grep -qx passed if ANY
# line matched — a multi-line/appended rc file resembles-but-fails now)
[ "$(cat "$DEVB/s1.apprc")" = "RC=0" ] || {
  pullv "$DTMP/s1.app-log.txt" "$DEVB/s1.app-log.txt" || true
  cat "$DEVB/s1.app-log.txt" >&2 || true
  echo "DEVICE FAIL: live app exited nonzero ($(cat "$DEVB/s1.apprc"))" >&2
  exit 1
}
echo "   live session done (device wall $((t1 - t0)) s; app rc 0; frontend restored)"
pullv "$DTMP/s1.trace.json" "$DEVB/s1.trace.json"
pullv "$DTMP/s1.live-out.txt" "$DEVB/s1.live-out.txt"
pullv "$DTMP/s1.live-tim.txt" "$DEVB/s1.live-tim.txt"
pullv "$DTMP/s1.app-log.txt" "$DEVB/s1.app-log.txt"

echo "== [6/8] host judgment: chord coverage over the recorded trace =="
node "$GFX/judge-s1-coverage.js" "$DEVB/s1.trace.json" "$SESSION_FRAMES" \
  | sed 's/^/   /'

echo "== [7/8] three-way replay determinism (host x2 + device, vs the live stream) =="
# strict golden-trace contract enforced by the UNCHANGED trace-to-txt.js
rm -f "$DEVB/s1.trace.txt"
node "$SIM/trace-to-txt.js" "$DEVB/s1.trace.json" "$DEVB/s1.trace.txt"
made "$DEVB/s1.trace.txt"
for side in a b; do
  rm -f "$DEVB/s1.rep-$side.txt"
  "$BUILD/sim_host_s1" --trace "$DEVB/s1.trace.txt" \
    --simdata "$DEVB/simdata.txt" \
    --seed "$SESSION_SEED" --p1 "$SESSION_P1" --p2 "$SESSION_P2" \
    --stage "$SESSION_STAGE" --frames "$SESSION_FRAMES" \
    --tapjump-off-p1 > "$DEVB/s1.rep-$side.txt"
  made "$DEVB/s1.rep-$side.txt"
done
cmp "$DEVB/s1.rep-a.txt" "$DEVB/s1.rep-b.txt"
echo "   host replay x2 byte-identical"
# non-vacuity: an all-neutral session must NOT reproduce the live stream
rm -f "$DEVB/s1.neutral.json" "$DEVB/s1.neutral.txt" "$DEVB/s1.rep-neutral.txt"
node -e '
  const fs = require("fs");
  const row = {};
  for (const k of ["a","b","x","y","z","r","l","s","du","dr","dd","dl"]) row[k] = false;
  for (const k of ["lsX","lsY","csX","csY","lA","rA","rawX","rawY","rawcsX","rawcsY"]) row[k] = 0;
  const fr = [];
  for (let i = 0; i < Number(process.argv[2]); i++) fr.push([row, row, null, null]);
  fs.writeFileSync(process.argv[1], JSON.stringify(fr));
' "$DEVB/s1.neutral.json" "$SESSION_FRAMES"
made "$DEVB/s1.neutral.json"
node "$SIM/trace-to-txt.js" "$DEVB/s1.neutral.json" "$DEVB/s1.neutral.txt"
made "$DEVB/s1.neutral.txt"
"$BUILD/sim_host_s1" --trace "$DEVB/s1.neutral.txt" \
  --simdata "$DEVB/simdata.txt" \
  --seed "$SESSION_SEED" --p1 "$SESSION_P1" --p2 "$SESSION_P2" \
  --stage "$SESSION_STAGE" --frames "$SESSION_FRAMES" \
  --tapjump-off-p1 > "$DEVB/s1.rep-neutral.txt"
made "$DEVB/s1.rep-neutral.txt"
if cmp -s "$DEVB/s1.rep-neutral.txt" "$DEVB/s1.live-out.txt"; then
  echo "DEVICE FAIL: live stream == all-neutral stream — the session was VACUOUS (input never reached the sim)" >&2
  exit 1
fi
echo "   non-vacuity OK (live stream != all-neutral stream)"
# device replay of the SAME recorded trace
adb -s "$DEV" push "$DEVB/s1.trace.txt" "$DTMP/" >/dev/null
tsum="$(shasum -a 256 "$DEVB/s1.trace.txt" | cut -d' ' -f1)"
# iter 52 parser audit: strict full-line device digest parse
dsum="$(rig_dev_sha256 "$DTMP/s1.trace.txt")" || exit 1
if [ "$dsum" != "$tsum" ]; then
  echo "DEVICE FAIL: pushed replay trace sha mismatch (device $dsum, host $tsum)" >&2
  exit 1
fi
dsh "sh -lc 'cd $DTMP && ./sim_device --trace s1.trace.txt --simdata simdata.txt \
  --seed $SESSION_SEED --p1 $SESSION_P1 --p2 $SESSION_P2 \
  --stage $SESSION_STAGE --frames $SESSION_FRAMES \
  --tapjump-off-p1 > s1.rep-dev.txt'"
pullv "$DTMP/s1.rep-dev.txt" "$DEVB/s1.rep-dev.txt"
cmp "$DEVB/s1.rep-a.txt" "$DEVB/s1.rep-dev.txt"
echo "   device replay byte-identical to the host replays (three-way)"
cmp "$DEVB/s1.rep-a.txt" "$DEVB/s1.live-out.txt"
echo "   live session stream byte-identical to all three replays (recording fidelity)"

echo "== [8/8] hygiene =="
rig_no_commit_guard "$BUILD" "$DEVB" "$TABLES"

frames_seen="$(grep -c '^F ' "$DEVB/s1.live-out.txt")"
echo "S1 INPUT OK (session ${frames_seen} frames live on device; host x2 + device replays and the live stream all byte-identical; 15 chord rows unit-swept; coverage judged)"
