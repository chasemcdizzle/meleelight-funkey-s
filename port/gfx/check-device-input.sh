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
#   3. COVERAGE (host judgment): judge-s1-coverage.js validates the
#      pulled recorded trace against the recorder's RAW anchored line
#      grammar (duplicate-key class closed BEFORE JSON.parse, iter 53)
#      + the strict 22-key shape + S1 invariants, then finds every
#      pre-registered chord signature (values, never frame indices —
#      the settling strategy). The raw-keysym sidecar the app records
#      (--record-keys) is the SOCD LIVE WITNESS (iter 53, review-51
#      M4): frames where opposed d-pad cardinals were RAW-held must
#      have resolved to a neutral axis, >= 5 frames per pair.
#   4. THREE-WAY REPLAY DETERMINISM (host judgment): the recorded trace
#      replays through the host headless sim TWICE and the device sim
#      binary ONCE — all three checksum streams byte-identical (cmp),
#      AND all three identical to the LIVE session's own stream (the
#      recording-fidelity witness). Self-consistency only: this is a
#      NEW trace, judged against itself, never against a frozen golden.
#      EVERY stream (live, both host replays, device replay, neutral
#      control) is validated against the strict full-stream grammar —
#      contiguous F 1..1080 + RNG + SIM OK, nothing else (iter 53,
#      review-51 H1: a stream stopping at frame 400 four ways must
#      never pass).
#   5. NON-VACUITY: the live stream DIFFERS from an all-neutral
#      1080-frame trace's stream (the sim provably consumed the input —
#      the four-way check alone would pass if input were ignored).
#   6. TAPJUMP ORACLE (host differential, iter 53, review-51 M5): a
#      synthetic up-flick trace replayed with vs without
#      --tapjump-off-p1 MUST diverge exactly at the up frame — the flag
#      is provably consumed, not decorative.
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
TJ_FRAMES=200       # tapjump-oracle synthetic trace length (host-only)
TJ_DIVERGE_FRAME=121 # MEASURED-then-frozen (iter 53; also iter-51's
                    # tooth): up rows 120..135 (0-based) = frames
                    # 121-136; with-vs-without --tapjump-off-p1 streams
                    # first diverge exactly at frame 121, the frame the
                    # up-flick lands. A different divergence frame is a
                    # REAL finding, never a pin to widen.

source port/sim/device/adbsh.sh # (also defines $DEV — it keys the lock)
source port/sim/device/riglib.sh

rig_lock_acquire

# Cleanup (installed AFTER lock acquisition, riglib contract): kill our
# processes by NAME (a -f pattern would self-match the adb shell — the
# iter-50 gotcha), restore the frontend if we parked it, then the shared
# rig cleanup (device scratch + lock). Best-effort, WARN-visible. Both
# device steps ride riglib's rig_dsh_retry (iter 53, review-51 M6 /
# iter-52 H1c class: a mid-run ADB transport death must not strand the
# parked frontend when the transport recovers).
PARKED=0
task5_cleanup() {
  rig_dsh_retry "pkill gfx_device; pkill fk_input; true" \
    || echo "WARN: could not pkill gfx_device/fk_input on the device" >&2
  if [ "$PARKED" = 1 ]; then
    rig_dsh_retry "rm -f /mnt/disable_frontend" \
      || echo "WARN: could not restore the frontend — remove /mnt/disable_frontend by hand" >&2
  fi
  rig_cleanup
}
trap task5_cleanup EXIT

require_device
rig_devsha_selftest

echo "== [1/9] host data plane (M1 tables + SIMDATA1 + GFXDATA pin + script) =="
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

echo "== [2/9] host build: s1_sweep + host replay sim (-ffp-contract=off everywhere) =="
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

# Strict full-stream validator (iter 53, review-51 H1; PROCESS §3
# whitelist grammar, measured from the real stream corpus — producers:
# gfx_app.c's stream fprintf and sim_main.c's printf, both emit EXACTLY
# lines `F <n> <64-lowercase-hex>` contiguous 1..N, one
# `RNG <uint> <uint>`, then `SIM OK`, trailing newline). Generated from
# this script's own bytes (RIG_SCRIPTS stamp input) so the grammar is
# reviewed WITH the check. Any shortfall, gap, resemblance, or trailing
# junk = corruption -> nonzero (gate-fatal under set -e).
rm -f "$BUILD/s1-stream-check.js"
cat > "$BUILD/s1-stream-check.js" << 'EOF'
"use strict";
const fs = require("fs");
function die(m) { console.error("s1-stream-check: " + m); process.exit(3); }
const [p, framesArg] = process.argv.slice(2);
if (!p || !framesArg || !/^[0-9]+$/.test(framesArg)) {
  console.error("usage: node s1-stream-check.js <stream.txt> <frames>");
  process.exit(1);
}
const N = parseInt(framesArg, 10);
const raw = fs.readFileSync(p, "utf8");
if (raw.length === 0 || raw[raw.length - 1] !== "\n") {
  die(p + " does not end with a newline (truncated write?)");
}
const lines = raw.split("\n");
lines.pop();
if (lines.length !== N + 2) {
  die(p + " has " + String(lines.length) + " lines, expected exactly " +
      String(N + 2) + " (F 1.." + String(N) + " + RNG + SIM OK)");
}
const F_RE = /^F ([0-9]+) ([0-9a-f]{64})$/;
for (let i = 0; i < N; i++) {
  const m = F_RE.exec(lines[i]);
  if (!m) die(p + " line " + String(i + 1) + " is not a frame line: " +
              JSON.stringify(lines[i].slice(0, 80)));
  if (m[1] !== String(i + 1)) {
    die(p + " frame lines not contiguous: got F " + m[1] + " at line " +
        String(i + 1));
  }
}
if (!/^RNG (0|[1-9][0-9]*) (0|[1-9][0-9]*)$/.test(lines[N])) {
  die(p + " line " + String(N + 1) + " is not the RNG trailer: " +
      JSON.stringify(lines[N]));
}
if (lines[N + 1] !== "SIM OK") {
  die(p + " last line is not exactly 'SIM OK'");
}
console.log("stream OK (" + String(N) + " frames + RNG + SIM OK): " + p);
EOF
made "$BUILD/s1-stream-check.js"
check_stream() { # <stream-file> <frames> — gate-fatal on any violation
  node "$BUILD/s1-stream-check.js" "$1" "$2" | sed 's/^/   /'
}

echo "== [3/9] S1 chord-table unit sweep (15 PLAN §6 rows, x2 byte-stable) =="
for side in a b; do
  rm -f "$BUILD/s1-sweep-$side.txt"
  "$BUILD/s1_sweep" > "$BUILD/s1-sweep-$side.txt"
  made "$BUILD/s1-sweep-$side.txt"
done
cmp "$BUILD/s1-sweep-a.txt" "$BUILD/s1-sweep-b.txt"
# iter 52 parser audit (whitelist grammar): full-line match of the
# measured s1_sweep.c OK literal (prefix-match accepted a resembling
# line; the producer emits exactly this line or S1 SWEEP FAIL). Iter 53
# (review-51 M3): exactly-ONE-match posture — the producer emits exactly
# one line starting `S1 SWEEP` (the OK or FAIL verdict); a second
# resembling line alongside the genuine one is corruption.
sweep_n="$(grep -c '^S1 SWEEP' "$BUILD/s1-sweep-a.txt" || true)"
if [ "$sweep_n" != 1 ]; then
  echo "DEVICE FAIL: expected exactly one 'S1 SWEEP' verdict line, got $sweep_n (corrupt sweep output)" >&2
  exit 1
fi
grep -qx "S1 SWEEP OK (15 pinned chord checks, 2048 combos, all coordinates on the 1/80 grid)" "$BUILD/s1-sweep-a.txt" || {
  echo "DEVICE FAIL: S1 sweep did not print its exact OK line" >&2
  exit 1
}
grep -v "^C " "$BUILD/s1-sweep-a.txt" | sed 's/^/   /'

echo "== [4/9] armv7 build (shared rig stamp) + push + provenance =="
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

echo "== [5/9] device: LIVE uinput-driven S1 session (frontend parked around the run) =="
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
  --record-keys $DTMP/s1.keys.txt \
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

# PARKED set pessimistically BEFORE the park dsh (iter 53, review-51
# M6 — the iter-52 H2 class): if the transport dies after the touch
# takes effect but before dsh returns, cleanup must still restore.
PARKED=1
dsh "touch /mnt/disable_frontend && { pkill gmenu2x; true; }"
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
# iter 53 (review-51 M3): BYTE-exact compare — the producer writes
# exactly the 5 bytes `RC=0\n` (measured corpus). The iter-52 `$(cat)`
# compare stripped trailing newlines, so `RC=0\n\n\n` still passed;
# cmp against the exact bytes closes the class.
if ! printf 'RC=0\n' | cmp -s - "$DEVB/s1.apprc"; then
  pullv "$DTMP/s1.app-log.txt" "$DEVB/s1.app-log.txt" || true
  cat "$DEVB/s1.app-log.txt" >&2 || true
  echo "DEVICE FAIL: live app rc file is not exactly 'RC=0\\n' ($(cat "$DEVB/s1.apprc"))" >&2
  exit 1
fi
echo "   live session done (device wall $((t1 - t0)) s; app rc 0; frontend restored)"
pullv "$DTMP/s1.trace.json" "$DEVB/s1.trace.json"
pullv "$DTMP/s1.keys.txt" "$DEVB/s1.keys.txt"
pullv "$DTMP/s1.live-out.txt" "$DEVB/s1.live-out.txt"
pullv "$DTMP/s1.live-tim.txt" "$DEVB/s1.live-tim.txt"
pullv "$DTMP/s1.app-log.txt" "$DEVB/s1.app-log.txt"
# stream completeness (review-51 H1): the LIVE stream must carry the
# full contiguous 1..SESSION_FRAMES grammar — gate-fatal on shortfall
check_stream "$DEVB/s1.live-out.txt" "$SESSION_FRAMES"

echo "== [6/9] host judgment: chord coverage + SOCD witness over the recorded trace =="
node "$GFX/judge-s1-coverage.js" "$DEVB/s1.trace.json" "$SESSION_FRAMES" \
  "$DEVB/s1.keys.txt" | sed 's/^/   /'

echo "== [7/9] three-way replay determinism (host x2 + device, vs the live stream) =="
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
  check_stream "$DEVB/s1.rep-$side.txt" "$SESSION_FRAMES" # review-51 H1
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
check_stream "$DEVB/s1.rep-neutral.txt" "$SESSION_FRAMES" # review-51 H1:
# a malformed control stream must never satisfy non-vacuity trivially
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
check_stream "$DEVB/s1.rep-dev.txt" "$SESSION_FRAMES" # review-51 H1
cmp "$DEVB/s1.rep-a.txt" "$DEVB/s1.rep-dev.txt"
echo "   device replay byte-identical to the host replays (three-way)"
cmp "$DEVB/s1.rep-a.txt" "$DEVB/s1.live-out.txt"
echo "   live session stream byte-identical to all three replays (recording fidelity)"

echo "== [8/9] tapJumpOff behavioral oracle (host differential; review-51 M5) =="
# The flag must be CONSUMED, not decorative: a synthetic up-flick trace
# (neutral except lsY=1/rawY=1 on 0-based rows 120..135 = frames
# 121-136) replayed with vs without --tapjump-off-p1 must produce
# streams whose FIRST divergence is exactly frame $TJ_DIVERGE_FRAME —
# with tap-jump live the flick jumps at the frame it lands; with the
# flag consumed it does not.
rm -f "$DEVB/s1.tj.json" "$DEVB/s1.tj.txt" "$DEVB/s1.tj-def.txt" "$DEVB/s1.tj-off.txt"
node -e '
  const fs = require("fs");
  const mk = () => {
    const r = {};
    for (const k of ["a","b","x","y","z","r","l","s","du","dr","dd","dl"]) r[k] = false;
    for (const k of ["lsX","lsY","csX","csY","lA","rA","rawX","rawY","rawcsX","rawcsY"]) r[k] = 0;
    return r;
  };
  const neutral = mk();
  const up = mk(); up.lsY = 1; up.rawY = 1;
  const fr = [];
  const n = Number(process.argv[2]);
  for (let i = 0; i < n; i++) fr.push([(i >= 120 && i < 136) ? up : neutral, neutral, null, null]);
  fs.writeFileSync(process.argv[1], JSON.stringify(fr));
' "$DEVB/s1.tj.json" "$TJ_FRAMES"
made "$DEVB/s1.tj.json"
node "$SIM/trace-to-txt.js" "$DEVB/s1.tj.json" "$DEVB/s1.tj.txt"
made "$DEVB/s1.tj.txt"
"$BUILD/sim_host_s1" --trace "$DEVB/s1.tj.txt" \
  --simdata "$DEVB/simdata.txt" \
  --seed "$SESSION_SEED" --p1 "$SESSION_P1" --p2 "$SESSION_P2" \
  --stage "$SESSION_STAGE" --frames "$TJ_FRAMES" > "$DEVB/s1.tj-def.txt"
made "$DEVB/s1.tj-def.txt"
"$BUILD/sim_host_s1" --trace "$DEVB/s1.tj.txt" \
  --simdata "$DEVB/simdata.txt" \
  --seed "$SESSION_SEED" --p1 "$SESSION_P1" --p2 "$SESSION_P2" \
  --stage "$SESSION_STAGE" --frames "$TJ_FRAMES" \
  --tapjump-off-p1 > "$DEVB/s1.tj-off.txt"
made "$DEVB/s1.tj-off.txt"
check_stream "$DEVB/s1.tj-def.txt" "$TJ_FRAMES"
check_stream "$DEVB/s1.tj-off.txt" "$TJ_FRAMES"
# first-divergence assert: streams MUST differ, at an aligned F line,
# at exactly the pinned frame. Two identical streams (flag ignored) or
# a divergence anywhere else = loud death.
node -e '
  const fs = require("fs");
  const a = fs.readFileSync(process.argv[1], "utf8").split("\n");
  const b = fs.readFileSync(process.argv[2], "utf8").split("\n");
  const want = process.argv[3];
  let i = 0;
  while (i < a.length && i < b.length && a[i] === b[i]) i++;
  if (i >= a.length && i >= b.length) {
    console.error("tapjump oracle FAIL: streams IDENTICAL — --tapjump-off-p1 is not consumed");
    process.exit(2);
  }
  const F_RE = /^F ([0-9]+) [0-9a-f]{64}$/;
  const ma = F_RE.exec(a[i] || ""); const mb = F_RE.exec(b[i] || "");
  if (!ma || !mb || ma[1] !== mb[1]) {
    console.error("tapjump oracle FAIL: first divergence is not an aligned frame line: " +
                  JSON.stringify((a[i] || "").slice(0, 40)) + " vs " +
                  JSON.stringify((b[i] || "").slice(0, 40)));
    process.exit(2);
  }
  if (ma[1] !== want) {
    console.error("tapjump oracle FAIL: streams diverge at frame " + ma[1] +
                  ", pinned frame " + want + " (the up-flick frame)");
    process.exit(2);
  }
  console.log("tapjump oracle OK: with/without --tapjump-off-p1 diverge at frame " + want);
' "$DEVB/s1.tj-def.txt" "$DEVB/s1.tj-off.txt" "$TJ_DIVERGE_FRAME" | sed 's/^/   /'

echo "== [9/9] hygiene =="
rig_no_commit_guard "$BUILD" "$DEVB" "$TABLES"

echo "S1 INPUT OK (session ${SESSION_FRAMES} frames live on device, all 5 streams grammar-complete; host x2 + device replays and the live stream all byte-identical; 15 chord rows unit-swept; coverage + SOCD witness judged; tapjump oracle diverged at frame ${TJ_DIVERGE_FRAME})"
