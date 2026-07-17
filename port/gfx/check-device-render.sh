#!/usr/bin/env bash
# M3 task 4 done-check: platform seam + SDL1.2 device backend + LIVE
# device render (fix_plan §M3 task 4). Prints
#   DEVICE RENDER OK (full p99 X ms, render-only p99 Y ms, skips N)
# exit 0, iff ALL of:
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
#      paced 60 fps, frameskip armed), frontend parked ONLY around the
#      run and ALWAYS restored (trap); the app RAM-buffers everything
#      and writes to tmpfs post-run (no I/O in the frame loop, no SD
#      writes during play).
#   5. DEVICE JUDGMENT (all host-side; the device never self-reports):
#      stream -> UNCHANGED verify-stream.js vs the frozen golden (the
#      renderer + SDL present must not perturb the sim ON DEVICE);
#      timing -> judge-render-timing.js (strict grammar), asserting
#        full-frame p99 (sim+render+present work time) <  16,670,000 ns
#        render-only p99 (rendered frames)             <= 8,000,000 ns
#      over the FULL 3600-frame match (integer compares, no float);
#      screenshot -> the app's OWN framebuffer (never the kernel fb —
#      240x720 triple-page, CLAUDE.md gotcha), pulled + judged by
#      judge-shot.js (letterbox structure, colour count, ink coverage)
#      and cmp'd against the host headless shot (bit-equality is
#      REPORTED as evidence, not required — cross-compiler float
#      identity is not a pinned claim).
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
SHOT_FRAME=900              # pinned screenshot frame (mid-match, both players
                            # live — an IoU corpus member, so task 3 keeps a
                            # browser-vs-C reference for the same frame)
GFXDATA_FROZEN=$GFX/gfxdata-frozen.txt
GFXDATA_SHA256=5499a3dd5fc374d6ed988faf0bef6fa2e189eb314e892bdd83c7534dc0865c94

source port/sim/device/adbsh.sh # (also defines $DEV — it keys the lock)
source port/sim/device/riglib.sh

rig_lock_acquire

# Cleanup (installed AFTER lock acquisition, riglib contract): kill our
# app, RESTORE the frontend if we parked it, then the shared rig cleanup
# (device scratch + lock). Best-effort by design, WARN-visible.
PARKED=0
task4_cleanup() {
  # pkill by process NAME — a `pkill -f <path>` pattern matches the adb
  # shell's OWN command line and kills it before the dsh RC marker can
  # print (measured this iter: rc 71 on every cleanup).
  dsh "pkill gfx_device; true" >/dev/null 2>&1 \
    || echo "WARN: could not pkill gfx_device on the device" >&2
  if [ "$PARKED" = 1 ]; then
    dsh "rm -f /mnt/disable_frontend" >/dev/null 2>&1 \
      || echo "WARN: could not restore the frontend — remove /mnt/disable_frontend by hand" >&2
  fi
  rig_cleanup
}
trap task4_cleanup EXIT

require_device
rig_devsha_selftest

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
      if ! [[ "$gv" =~ ^[0-9]+$ ]]; then
        echo "DEVICE FAIL: manifest g01.$gk not a decimal integer ('$gv')" >&2
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
    --gfxdata "$GFXDATA_FROZEN" --anim-dir "$TABLES" \
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

echo "== [4/7] frameskip valve (standing tooth: 1000 ns budget => skips, flagged) =="
rm -f "$BUILD/valve-out.txt" "$BUILD/valve-tim.txt" \
  "$BUILD/valve-shot.ppm" "$BUILD/valve-shot.pgm"
"$BUILD/gfx_app_headless" \
  --trace "$DEVB/g01.trace.txt" --simdata "$DEVB/simdata.txt" \
  --gfxdata "$GFXDATA_FROZEN" --anim-dir "$TABLES" \
  --seed "$seed" --p1 "$p1" --p2 "$p2" --stage "$stage" \
  --frames 120 --pace 1 --budget-ns 1000 \
  --out "$BUILD/valve-out.txt" --timing "$BUILD/valve-tim.txt" \
  --shot-frame 100 --shot-ppm "$BUILD/valve-shot.ppm" \
  --shot-pgm "$BUILD/valve-shot.pgm" \
  2> "$BUILD/valve-log.txt"
made "$BUILD/valve-out.txt" "$BUILD/valve-tim.txt"
valve_skips="$(awk '$4=="1" {n++} END {print n+0}' "$BUILD/valve-tim.txt")"
if ! [[ "$valve_skips" =~ ^[0-9]+$ ]] || [ "$valve_skips" -lt 100 ]; then
  echo "DEVICE FAIL: frameskip valve tooth — expected >=100 flagged skips in 120 frames at a 1000 ns budget, got '$valve_skips'" >&2
  exit 1
fi
grep -q "render skips" "$BUILD/valve-log.txt" || {
  echo "DEVICE FAIL: frameskip valve tooth — skip summary missing from the app log" >&2
  exit 1
}
echo "   valve tooth OK ($valve_skips/120 renders skipped, flagged in the timing artifact; sim ran every frame)"

echo "== [5/7] armv7 build (shared rig stamp) + push + provenance =="
rig_arm_build
rig_stamp_rehash gfx_device
dsh "rm -rf $DTMP $DSD && mkdir -p $DTMP $DSD"
adb -s "$DEV" push "$DEVB/gfx_device" "$DEVB/simdata.txt" \
  "$DEVB/g01.trace.txt" "$GFXDATA_FROZEN" \
  "$TABLES/$ANIM_P1" "$TABLES/$ANIM_P2" "$DTMP/" >/dev/null
rig_push_provenance "$DTMP" gfx_device
dsh "chmod +x $DTMP/gfx_device"
# sha-verify every pushed DATA file device-side against the host bytes
# (the binary is covered by push provenance above)
for hf in "$DEVB/simdata.txt" "$DEVB/g01.trace.txt" "$GFXDATA_FROZEN" \
          "$TABLES/$ANIM_P1" "$TABLES/$ANIM_P2"; do
  bn="$(basename "$hf")"
  hsum="$(shasum -a 256 "$hf" | cut -d' ' -f1)"
  dsum="$(dsh "sha256sum $DTMP/$bn" | awk 'NF{print $1; exit}')"
  if [ "$dsum" != "$hsum" ]; then
    echo "DEVICE FAIL: pushed $bn device sha ($dsum) != host sha ($hsum)" >&2
    exit 1
  fi
done
echo "   pushed data sha-verified on device (simdata, trace, gfxdata, 2 anim bins)"

echo "== [6/7] device: LIVE paced g01 render (frontend parked around the run) =="
dsh "touch /mnt/disable_frontend && { pkill gmenu2x; true; }"
PARKED=1
t0=$(date +%s)
# Foreground on purpose (M3 convention: device runs are slow, generous
# host-side timeouts, foreground). Login shell: /etc/profile sets
# SDL_NOMOUSE=1 — without it SDL_Init dies "Unable to open mouse".
dsh "sh -lc 'cd $DTMP && ./gfx_device \
  --trace $DTMP/g01.trace.txt --simdata $DTMP/simdata.txt \
  --gfxdata $DTMP/gfxdata-frozen.txt --anim-dir $DTMP \
  --seed $seed --p1 $p1 --p2 $p2 --stage $stage --frames $frames \
  --pace 1 --budget-ns $BUDGET_NS \
  --out $DTMP/g01.dev-out.txt --timing $DTMP/g01.dev-tim.txt \
  --shot-frame $SHOT_FRAME --shot-ppm $DTMP/g01.dev-shot.ppm \
  --shot-pgm $DTMP/g01.dev-shot.pgm 2> $DTMP/g01.dev-log.txt'"
t1=$(date +%s)
dsh "rm -f /mnt/disable_frontend"
PARKED=0
echo "   live run done (device wall $((t1 - t0)) s; frontend restored)"
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

unset full_p99_ns full_p99_ms render_p99_ns render_p99_ms sim_p99_ms \
  present_p99_ms skips rendered
jout="$(node "$GFX/judge-render-timing.js" "$DEVB/g01.dev-tim.txt" "$frames")" || {
  echo "DEVICE FAIL: timing judgment failed" >&2
  exit 1
}
while IFS='=' read -r jk jv; do
  case "$jk" in
    full_p99_ns|render_p99_ns|skips|rendered)
      if ! [[ "$jv" =~ ^[0-9]+$ ]]; then
        echo "DEVICE FAIL: timing judge $jk not a decimal integer ('$jv')" >&2
        exit 1
      fi
      printf -v "$jk" '%s' "$jv"
      ;;
    full_p99_ms|render_p99_ms|sim_p99_ms|present_p99_ms)
      if ! [[ "$jv" =~ ^[0-9]+\.[0-9]{3}$ ]]; then
        echo "DEVICE FAIL: timing judge $jk malformed ('$jv')" >&2
        exit 1
      fi
      printf -v "$jk" '%s' "$jv"
      ;;
    full_p50_ns|full_p50_ms|full_max_ns|full_max_ms|sim_p50_ns|sim_p50_ms|sim_p99_ns|render_p50_ns|render_p50_ms|render_max_ns|render_max_ms|present_p50_ns|present_p50_ms|present_p99_ns)
      : # reported by the judge; not asserted here
      ;;
    *)
      echo "DEVICE FAIL: unexpected timing judge line '$jk=$jv'" >&2
      exit 1
      ;;
  esac
done <<< "$jout"
: "$full_p99_ns" "$full_p99_ms" "$render_p99_ns" "$render_p99_ms" \
  "$sim_p99_ms" "$present_p99_ms" "$skips" "$rendered"
printf '%s\n' "$jout" | sed 's/^/   judge: /'
if [ "$full_p99_ns" -ge "$P99_FULL_LIMIT_NS" ]; then
  echo "RENDER P99 FAIL: full-frame p99 ${full_p99_ms} ms (${full_p99_ns} ns) >= limit ${P99_FULL_LIMIT_NS} ns" >&2
  exit 1
fi
if [ "$render_p99_ns" -gt "$P99_RENDER_LIMIT_NS" ]; then
  echo "RENDER P99 FAIL: render-only p99 ${render_p99_ms} ms (${render_p99_ns} ns) > limit ${P99_RENDER_LIMIT_NS} ns" >&2
  exit 1
fi

node "$GFX/judge-shot.js" "$DEVB/g01.dev-shot.ppm" "$DEVB/g01.dev-shot.pgm"
echo "   device shot passes the structural judge"
if cmp -s "$DEVB/g01.dev-shot.ppm" "$BUILD/g01.app-shot-a.ppm"; then
  echo "   device shot == host headless shot BIT-IDENTICAL (evidence bonus, not a pinned claim)"
else
  echo "   device shot differs from host shot at the byte level (allowed: cross-compiler float identity is not pinned; structure judged above)"
fi

rig_no_commit_guard "$BUILD" "$DEVB" "$TABLES"

echo "DEVICE RENDER OK (full p99 ${full_p99_ms} ms, render-only p99 ${render_p99_ms} ms, sim p99 ${sim_p99_ms} ms, present p99 ${present_p99_ms} ms, skips ${skips}/${frames})"
