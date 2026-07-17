#!/usr/bin/env bash
# port/gfx/check-render.sh — M3 task 3 done-check (fix_plan §M3 task 3):
# the C renderer core, host-side. Prints RENDER OK, exit 0 iff ALL of:
#   1. fresh M1 data plane (ANIM1 + CTAB1 + STAB1) + SIMDATA1 + GFXDATA1;
#   2. gfx_replay builds (every TU -ffp-contract=off -Wall -Wextra
#      -Werror; -O2 everywhere, -O3 ONLY on the hot raster TU);
#   3. the browser render-reference capture replays g01 with the reduced
#      render sequence after EVERY step and its checksum stream passes
#      the UNCHANGED verify-stream.js vs the frozen golden
#      (non-perturbation guard for the capture instrumentation);
#   4. TWO fresh C render-on replays produce byte-identical stdout
#      streams AND byte-identical PPM+PGM sets (x2 byte-stable renders);
#   5. the C render-on replay's stream passes verify-stream.js vs the
#      frozen golden (the renderer must not perturb the sim);
#   6. silhouette IoU (iou.js, frozen methodology) >= the frozen
#      threshold in expected-render.json on every sampled frame.
#
# Tier-A hygiene (PROCESS §3 review bar, iter-42 class rule): every
# produced artifact is rm-before-produce + made()-asserted; every
# judgment is host-side (cmp / verify-stream.js / deterministic iou.js);
# no eval of untrusted text (golden params come from the committed
# manifest via node -p JSON access).
#
# Canvas dumps are NON-FROZEN reference material regenerated per run.
# Dev-only reuse hatch (pre-registered): MLFK_GFX_REUSE_CANVAS=1 reuses
# an existing capture (its run JSON is still verify-streamed).
set -euo pipefail
cd "$(dirname "$0")/../.."

GFX=port/gfx
BUILD=$GFX/build
TABLES=$BUILD/tables
CANVAS=$BUILD/canvas
mkdir -p "$BUILD"

made() {
  local f
  for f in "$@"; do
    if [ ! -s "$f" ]; then
      echo "check-render: expected artifact missing/empty: $f" >&2
      exit 1
    fi
  done
}

EXP=$GFX/expected-render.json
made "$EXP"

# corpus pin (review-44 fix 1): the frozen 16-frame corpus, exactly —
# unique positive integers, count == sampledFrameCount == 16. Asserted
# BEFORE any build/capture work; changing the corpus is a reviewed repo
# change to expected-render.json + this pin + iou.js's twin pin.
node -e '
const e = require("./'"$EXP"'");
const f = e.sampledFrames;
if (!Array.isArray(f) || !Number.isInteger(e.sampledFrameCount) ||
    e.sampledFrameCount !== 16 || f.length !== e.sampledFrameCount) {
  console.error("check-render: corpus pin violated (want exactly 16 pinned frames: sampledFrames + sampledFrameCount)");
  process.exit(1);
}
const seen = new Set();
for (const v of f) {
  if (!Number.isInteger(v) || v < 1 || seen.has(v)) {
    console.error("check-render: corpus pin violated (bad/duplicate frame " + v + ")");
    process.exit(1);
  }
  seen.add(v);
}
'
echo "corpus pin OK (16 unique sampled frames)"

# frozen params (strict JSON reads; node -p, no eval of untrusted text)
FRAMES_LIST=$(node -p "require('./$EXP').sampledFrames.join(',')")
GOLDEN=$(node -p "require('./$EXP').golden")
[ "$GOLDEN" = "g01" ] || { echo "check-render: expected golden g01, got $GOLDEN" >&2; exit 1; }
G_NAME=$(node -p "require('./oracle/goldens/manifest.json').goldens.find(g=>g.id==='g01').name")
G_TRACE=$(node -p "require('./oracle/goldens/manifest.json').goldens.find(g=>g.id==='g01').trace")
G_SEED=$(node -p "String(require('./oracle/goldens/manifest.json').goldens.find(g=>g.id==='g01').seed)")
G_P1=$(node -p "String(require('./oracle/goldens/manifest.json').goldens.find(g=>g.id==='g01').p1)")
G_P2=$(node -p "String(require('./oracle/goldens/manifest.json').goldens.find(g=>g.id==='g01').p2)")
G_STAGE=$(node -p "String(require('./oracle/goldens/manifest.json').goldens.find(g=>g.id==='g01').stage)")
G_FRAMES=$(node -p "String(require('./oracle/goldens/manifest.json').goldens.find(g=>g.id==='g01').frames)")
FROZEN=oracle/goldens/$G_NAME.sha256.json
made "$FROZEN" "oracle/goldens/$G_TRACE"

# --- 1. data planes -------------------------------------------------------------
rm -rf "$TABLES"
bash pipeline/extractor/build-extractor.sh
node pipeline/run.js --only animations,tables,stages --out "$TABLES"
made "$TABLES/ml_tables.c" "$TABLES/ml_stages.c" "$TABLES/ml_tables.h" \
     "$TABLES/ml_stages.h" "$TABLES/anim_0_marth.bin" "$TABLES/anim_2_fox.bin"

rm -f "$BUILD/simdata.txt"
node port/sim/calib/dump-sim-data.js --out "$BUILD/simdata.txt"
made "$BUILD/simdata.txt"

# --- 2. build (rm-before-produce; -O3 ONLY on the raster TU) ---------------------
CFLAGS_COMMON=(-ffp-contract=off -Wall -Wextra -Werror
  -I"$TABLES" -Iport/ryu -Iport/sim -Ioracle/qjs)
rm -f "$BUILD/raster.o" "$BUILD/anim1.o" "$BUILD/gfx_render.o" \
      "$BUILD/gfx_replay.o" "$BUILD/gfx_replay"
cc -O3 "${CFLAGS_COMMON[@]}" -c "$GFX/raster.c" -o "$BUILD/raster.o"
cc -O2 "${CFLAGS_COMMON[@]}" -c "$GFX/anim1.c" -o "$BUILD/anim1.o"
cc -O2 "${CFLAGS_COMMON[@]}" -c "$GFX/gfx_render.c" -o "$BUILD/gfx_render.o"
cc -O2 "${CFLAGS_COMMON[@]}" -c "$GFX/gfx_replay.c" -o "$BUILD/gfx_replay.o"
cc -O2 "${CFLAGS_COMMON[@]}" -o "$BUILD/gfx_replay" \
  "$BUILD/raster.o" "$BUILD/anim1.o" "$BUILD/gfx_render.o" "$BUILD/gfx_replay.o" \
  port/sim/sim/sim_boot.c port/sim/sim/sim_tick.c port/sim/sim/sim_ser.c \
  port/sim/sim/sim_data.c \
  port/sim/calib/canon.c port/sim/calib/player_canon.c \
  port/sim/physics.c port/sim/interpolated_collision.c \
  port/sim/environmental_collision.c port/sim/hit_detection.c \
  port/sim/article.c port/sim/action_state_shortcuts.c \
  port/sim/ml_events.c port/sim/ml_fmt.c port/sim/ml_ser.c \
  port/sim/ai_bridge.c port/sim/input/interpret_inputs.c \
  port/sim/stages/moving_platforms.c port/sim/stages/ystory.c \
  port/sim/stages/fountain.c \
  port/sim/characters/shared/moves_index.c port/sim/characters/shared/moves/*.c \
  port/sim/characters/fox/moves_index.c port/sim/characters/fox/moves/*.c \
  port/sim/characters/falco/moves_index.c port/sim/characters/falco/moves/*.c \
  port/sim/characters/falcon/moves_index.c port/sim/characters/falcon/moves/*.c \
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
made "$BUILD/gfx_replay"
echo "build OK: $BUILD/gfx_replay (raster TU -O3, all else -O2; -ffp-contract=off everywhere)"

# --- 3. browser reference capture (STREAM-MATCH guarded) --------------------------
NEED_CAPTURE=1
if [ "${MLFK_GFX_REUSE_CANVAS:-0}" = "1" ]; then
  # Reuse hatch is DIGEST-BOUND (review-44 fix 3): the cached capture
  # carries sha256 of the driver bytes that produced it + the GFXDATA it
  # wrote (capture.digests.json, written by capture-canvas.js). Reuse
  # REFUSES loudly on any mismatch/missing piece — never a silent
  # fallback to a fresh capture, so a stale-cache mistake always surfaces.
  node -e '
const fs = require("fs"), crypto = require("crypto");
const h = (fp) => crypto.createHash("sha256").update(fs.readFileSync(fp)).digest("hex");
const side = "'"$CANVAS"'/capture.digests.json";
if (!fs.existsSync(side)) {
  console.error("check-render: reuse REFUSED: " + side + " missing (no digest-bound capture cached)");
  process.exit(1);
}
const d = JSON.parse(fs.readFileSync(side, "utf8"));
const cur = {
  captureCanvasJs: h("port/gfx/capture-canvas.js"),
  gfxPagelibJs: h("port/gfx/gfx-pagelib.js"),
  gfxdata: h("'"$BUILD"'/gfxdata.txt"),
};
for (const k of Object.keys(cur)) {
  if (d[k] !== cur[k]) {
    console.error("check-render: reuse REFUSED: digest mismatch on " + k + " (cached capture is stale vs current bytes)");
    process.exit(1);
  }
}
'
  if [ ! -s "$BUILD/g01.render-run.json" ]; then
    echo "check-render: reuse REFUSED: cached run JSON missing/empty" >&2
    exit 1
  fi
  NEED_CAPTURE=0
  echo "capture: reusing cached canvas dumps (MLFK_GFX_REUSE_CANVAS=1; digest-bound dev hatch)"
fi
if [ "$NEED_CAPTURE" = "1" ]; then
  rm -rf "$CANVAS"
  rm -f "$BUILD/g01.render-run.json" "$BUILD/gfxdata.txt"
  node "$GFX/capture-canvas.js" --golden g01 --frames-list "$FRAMES_LIST" \
    --out-dir "$CANVAS" --out-run "$BUILD/g01.render-run.json" \
    --gfxdata "$BUILD/gfxdata.txt"
fi
made "$BUILD/g01.render-run.json" "$BUILD/gfxdata.txt" "$CANVAS/capture.digests.json"
IFS=',' read -r -a SAMPLED <<< "$FRAMES_LIST"
for f in "${SAMPLED[@]}"; do
  tag=$(printf 'f%04d' "$f")
  made "$CANVAS/$tag.mask.bin" "$CANVAS/$tag.png"
done

# oracle build digest pin (review-44 fix 2, asserted BEFORE any judging):
# the bytes the capture actually SERVED must be the pinned built bundle
# (upstream 27af171 + the committed oracle/build-upstream.sh recipe make
# the digest stable). If the clone is legitimately rebuilt and this
# digest changes, updating servedDistSha256 in expected-render.json is a
# REVIEWED repo change, never a runtime accommodation.
SERVED_DIGEST=$(node -p "require('./$BUILD/g01.render-run.json').meta.gfxCapture.served.digest")
PINNED_DIGEST=$(node -p "require('./$EXP').servedDistSha256")
if [ -z "$SERVED_DIGEST" ] || [ "$SERVED_DIGEST" = "undefined" ] || \
   [ "$SERVED_DIGEST" != "$PINNED_DIGEST" ]; then
  echo "check-render: served-dist digest mismatch: capture '$SERVED_DIGEST' != pinned '$PINNED_DIGEST'" >&2
  exit 1
fi
echo "served-dist digest matches the frozen pin"

node oracle/harness/verify-stream.js "$BUILD/g01.render-run.json" "$FROZEN"
echo "capture stream verified (render instrumentation did not perturb the sim)"

# --- 4. C render-on replay x2 (byte stability) -------------------------------------
rm -f "$BUILD/g01.trace.txt"
node port/sim/sim/trace-to-txt.js "oracle/goldens/$G_TRACE" "$BUILD/g01.trace.txt"
made "$BUILD/g01.trace.txt"

for side in a b; do
  rm -rf "$BUILD/render-$side"
  rm -f "$BUILD/g01.gfx-out-$side.txt"
  mkdir -p "$BUILD/render-$side"
  "$BUILD/gfx_replay" \
    --trace "$BUILD/g01.trace.txt" --simdata "$BUILD/simdata.txt" \
    --gfxdata "$BUILD/gfxdata.txt" --anim-dir "$TABLES" \
    --seed "$G_SEED" --p1 "$G_P1" --p2 "$G_P2" --stage "$G_STAGE" \
    --frames "$G_FRAMES" \
    --render-frames "$FRAMES_LIST" --render-out "$BUILD/render-$side" \
    > "$BUILD/g01.gfx-out-$side.txt" \
    2> "$BUILD/g01.gfx-rtiming-$side.txt"
  made "$BUILD/g01.gfx-out-$side.txt"
  for f in "${SAMPLED[@]}"; do
    tag=$(printf 'f%04d' "$f")
    made "$BUILD/render-$side/$tag.ppm" "$BUILD/render-$side/$tag.pgm"
  done
done
cmp "$BUILD/g01.gfx-out-a.txt" "$BUILD/g01.gfx-out-b.txt"
for f in "${SAMPLED[@]}"; do
  tag=$(printf 'f%04d' "$f")
  cmp "$BUILD/render-a/$tag.ppm" "$BUILD/render-b/$tag.ppm"
  cmp "$BUILD/render-a/$tag.pgm" "$BUILD/render-b/$tag.pgm"
done
echo "x2 C renders byte-identical (stream + all PPM/PGM)"
grep '^render-only ns:' "$BUILD/g01.gfx-rtiming-a.txt" || {
  echo "check-render: render timing line missing" >&2; exit 1; }

# --- 5. C stream still conforms with the renderer linked + active ------------------
rm -f "$BUILD/g01.gfx-run.json"
node port/sim/sim/wrap-run.js g01 "$BUILD/g01.gfx-out-a.txt" "$BUILD/g01.gfx-run.json"
made "$BUILD/g01.gfx-run.json"
node oracle/harness/verify-stream.js "$BUILD/g01.gfx-run.json" "$FROZEN"
echo "C render-on replay stream verified (renderer does not perturb the sim)"

# --- 6. silhouette IoU vs the frozen threshold --------------------------------------
rm -f "$BUILD/iou-report.json"
node "$GFX/iou.js" --canvas "$CANVAS" --render "$BUILD/render-a" \
  --expected "$EXP" --report "$BUILD/iou-report.json"
made "$BUILD/iou-report.json"

echo "RENDER OK"
