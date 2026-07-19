#!/usr/bin/env bash
# port/gfx/check-render.sh — M3 task 3 done-check, EXTENDED by M4 task 2
# (fix_plan §M4 task 2: renderer vfx + overlay/banner/background + IoU
# re-freeze). Prints RENDER OK, exit 0 iff ALL of:
#   1. fresh M1 data plane (ANIM1 + CTAB1 + STAB1) + SIMDATA1 + GFXDATA1
#      + VFXDATA1 (executed vfx templates) + VFXGLYPHS1 (browser-
#      rasterized text atlas), the latter three cmp'd against their
#      committed frozen artifacts;
#   2. gfx_replay builds (every TU -ffp-contract=off -Wall -Wextra
#      -Werror; -O2 everywhere, -O3 ONLY on the hot raster TU);
#   3. the browser render-reference capture replays g01 with the FULL
#      render sequence (clearScreen/drawStage/renderPlayer/renderArticles
#      /renderVfx/renderOverlay(true); fg1|fg2|UI mask) + the frozen
#      synthetic-injection table after EVERY step, and its checksum
#      stream passes the UNCHANGED verify-stream.js vs the frozen golden
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

# corpus pin (review-44 fix 1; EXTENDED 16 -> 24 by M4 task 2's reviewed
# corpus change): the frozen 24-frame corpus, exactly — unique positive
# integers, count == sampledFrameCount == 24. Asserted BEFORE any
# build/capture work; changing the corpus is a reviewed repo change to
# expected-render.json + this pin + iou.js's twin pin.
node -e '
const e = require("./'"$EXP"'");
const f = e.sampledFrames;
if (!Array.isArray(f) || !Number.isInteger(e.sampledFrameCount) ||
    e.sampledFrameCount !== 24 || f.length !== e.sampledFrameCount) {
  console.error("check-render: corpus pin violated (want exactly 24 pinned frames: sampledFrames + sampledFrameCount)");
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
echo "corpus pin OK (24 unique sampled frames)"

# injection-set pin (review-65 M1, iter 67): the RUNTIME inject table must
# equal the frozen reviewed injectPin — exact ordered name list,
# uniqueness, count, frame identity, and inject.frame ∈ sampledFrames.
# Asserted BEFORE any build/capture work (and re-asserted at the INJECT1
# emitter below — the C-side feed). capture-canvas.js (browser side) and
# iou.js (the judge) carry their own independent copies of this assert:
# a dropped/renamed effect dies loudly on every side, never a silent
# both-sides omission. Changing the injection set is a reviewed repo
# change to inject + injectPin together.
node -e '
const e = require("./'"$EXP"'");
const pin = e.injectPin, inj = e.inject;
if (!pin || !Array.isArray(pin.names) || !Array.isArray(pin.inkNames) ||
    !Number.isInteger(pin.frame) || !Number.isInteger(pin.count)) {
  console.error("check-render: injectPin missing/malformed in expected-render.json");
  process.exit(1);
}
if (!inj || !Number.isInteger(inj.frame) || !Array.isArray(inj.configs)) {
  console.error("check-render: inject table missing/malformed in expected-render.json");
  process.exit(1);
}
const names = inj.configs.map(c => c && c.name);
if (names.some(n => typeof n !== "string" || n.length === 0)) {
  console.error("check-render: inject pin violated (config without a name)");
  process.exit(1);
}
if (new Set(names).size !== names.length) {
  console.error("check-render: inject pin violated (duplicate config name)");
  process.exit(1);
}
if (pin.count !== pin.names.length || names.length !== pin.count ||
    names.join(" ") !== pin.names.join(" ")) {
  console.error("check-render: inject pin violated — runtime configs [" +
    names.join(", ") + "] != pinned reviewed set [" + pin.names.join(", ") + "]");
  process.exit(1);
}
if (inj.frame !== pin.frame) {
  console.error("check-render: inject pin violated (inject.frame " + inj.frame +
    " != pinned " + pin.frame + ")");
  process.exit(1);
}
if (!e.sampledFrames.includes(inj.frame)) {
  console.error("check-render: inject pin violated (inject.frame " + inj.frame +
    " not in sampledFrames — the injection would never be judged)");
  process.exit(1);
}
if (pin.inkNames.length === 0 || new Set(pin.inkNames).size !== pin.inkNames.length ||
    !pin.inkNames.every(n => pin.names.includes(n))) {
  console.error("check-render: inject pin violated (inkNames must be a nonempty unique subset of names)");
  process.exit(1);
}
// Exact ordered inkNames pin (review-65 r2 M5): hard-coded here and in
// capture-canvas.js + iou.js (twin-pin class) — a name silently dropped
// from expected-render.json inkNames must die on every side, never
// quietly shrink the leave-one-out/regional coverage.
if (pin.inkNames.length !== 5 ||
    pin.inkNames.join(" ") !== "firefoxcharge firefoxtail shine dashDust groundBounce") {
  console.error("check-render: inject pin violated — inkNames [" + pin.inkNames.join(", ") +
    "] != the hard-pinned reviewed 5-name set");
  process.exit(1);
}
if (pin.names.some(n => !/^[A-Za-z][A-Za-z0-9]*$/.test(n))) {
  console.error("check-render: inject pin violated (names must match the identifier grammar — they double as artifact path tokens)");
  process.exit(1);
}
'
echo "inject pin OK (7-name reviewed set, 5-name inkNames pin, frame in corpus)"

# frozen params. FRAMES_LIST/GOLDEN come from expected-render.json whose
# corpus pin was validated above (node JSON reads, exact compare below).
FRAMES_LIST=$(node -p "require('./$EXP').sampledFrames.join(',')")
GOLDEN=$(node -p "require('./$EXP').golden")
[ "$GOLDEN" = "g01" ] || { echo "check-render: expected golden g01, got $GOLDEN" >&2; exit 1; }
# g01 match params — iter 52 parser audit: the reviewed no-eval STRICT
# line parser (check-device-g01.sh gparams class: iter 39 M4 / iter 40
# round 2) replaces eight bare `node -p` scrapes whose missing-field
# failure mode was the string "undefined" leaking into command args.
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
")" || { echo "check-render: g01 manifest param extraction failed" >&2; exit 1; }
if [ -z "$gparams" ]; then
  echo "check-render: g01 manifest param extraction returned nothing" >&2
  exit 1
fi
while IFS='=' read -r gk gv; do
  case "$gk" in
    name)
      if ! [[ "$gv" =~ ^[a-z0-9][a-z0-9-]*$ ]]; then
        echo "check-render: manifest g01.name fails validation ('$gv')" >&2
        exit 1
      fi
      name=$gv
      ;;
    trace)
      if ! [[ "$gv" =~ ^[a-z0-9][a-z0-9-]*\.trace\.json$ ]]; then
        echo "check-render: manifest g01.trace fails validation ('$gv')" >&2
        exit 1
      fi
      trace=$gv
      ;;
    seed|p1|p2|stage|frames)
      if ! [[ "$gv" =~ ^[0-9]+$ ]]; then
        echo "check-render: manifest g01.$gk not a decimal integer ('$gv')" >&2
        exit 1
      fi
      printf -v "$gk" '%s' "$gv"
      ;;
    *)
      echo "check-render: unexpected manifest extraction line '$gk=$gv'" >&2
      exit 1
      ;;
  esac
done <<< "$gparams"
: "$name" "$seed" "$p1" "$p2" "$stage" "$frames" "$trace"
G_NAME=$name
G_TRACE=$trace
G_SEED=$seed
G_P1=$p1
G_P2=$p2
G_STAGE=$stage
G_FRAMES=$frames
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
      "$BUILD/gfx_vfx.o" "$BUILD/gfx_overlay.o" "$BUILD/gfx_bg.o" \
      "$BUILD/gfx_replay.o" "$BUILD/gfx_replay"
cc -O3 "${CFLAGS_COMMON[@]}" -c "$GFX/raster.c" -o "$BUILD/raster.o"
cc -O2 "${CFLAGS_COMMON[@]}" -c "$GFX/anim1.c" -o "$BUILD/anim1.o"
cc -O2 "${CFLAGS_COMMON[@]}" -c "$GFX/gfx_render.c" -o "$BUILD/gfx_render.o"
cc -O2 "${CFLAGS_COMMON[@]}" -c "$GFX/gfx_vfx.c" -o "$BUILD/gfx_vfx.o"
cc -O2 "${CFLAGS_COMMON[@]}" -c "$GFX/gfx_overlay.c" -o "$BUILD/gfx_overlay.o"
cc -O2 "${CFLAGS_COMMON[@]}" -c "$GFX/gfx_bg.c" -o "$BUILD/gfx_bg.o"
cc -O2 "${CFLAGS_COMMON[@]}" -c "$GFX/gfx_replay.c" -o "$BUILD/gfx_replay.o"
cc -O2 "${CFLAGS_COMMON[@]}" -o "$BUILD/gfx_replay" \
  "$BUILD/raster.o" "$BUILD/anim1.o" "$BUILD/gfx_render.o" \
  "$BUILD/gfx_vfx.o" "$BUILD/gfx_overlay.o" "$BUILD/gfx_bg.o" \
  "$BUILD/gfx_replay.o" \
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
  # Reuse hatch is INPUT-CLOSURE-BOUND (review-44 fix 3, widened by
  # review-46 fix 2): the cached capture carries sha256 of EVERY
  # capture-input-closure member (port/gfx/capture-closure.js — the ONE
  # enumeration: drivers, page init scripts, expected-render.json as
  # the allowlist/pin source, manifest, trace) + the GFXDATA it wrote
  # (capture.digests.json, written by capture-canvas.js). Reuse REFUSES
  # loudly on a missing sidecar, a pre-closure-format sidecar, closure
  # member-set drift in EITHER direction, any per-member digest
  # mismatch, or GFXDATA drift — never a silent fallback to a fresh
  # capture, so a stale-cache / stale-policy mistake always surfaces.
  # (The served dist + the cached run JSON are covered below by the
  # servedDistSha256 pin and verify-stream.js, reuse mode included.)
  node -e '
const fs = require("fs"), crypto = require("crypto");
const h = (fp) => crypto.createHash("sha256").update(fs.readFileSync(fp)).digest("hex");
const side = "'"$CANVAS"'/capture.digests.json";
if (!fs.existsSync(side)) {
  console.error("check-render: reuse REFUSED: " + side + " missing (no digest-bound capture cached)");
  process.exit(1);
}
const d = JSON.parse(fs.readFileSync(side, "utf8"));
if (!d.closure || typeof d.closure !== "object" || Array.isArray(d.closure)) {
  console.error("check-render: reuse REFUSED: sidecar has no closure map (cached capture predates the input-closure binding — recapture)");
  process.exit(1);
}
const { closureFiles } = require("./port/gfx/capture-closure.js");
const files = closureFiles("'"$G_TRACE"'");
const want = Object.keys(files).sort();
const got = Object.keys(d.closure).sort();
if (want.join("\n") !== got.join("\n")) {
  console.error("check-render: reuse REFUSED: closure member-set drift (cached: [" +
    got.join(", ") + "] vs current: [" + want.join(", ") + "])");
  process.exit(1);
}
for (const k of want) {
  if (d.closure[k] !== h(files[k])) {
    console.error("check-render: reuse REFUSED: digest mismatch on " + k + " (cached capture is stale vs current bytes)");
    process.exit(1);
  }
}
for (const [key, fp] of [["gfxdata", "'"$BUILD"'/gfxdata.txt"],
                         ["vfxdata", "'"$BUILD"'/vfxdata.txt"],
                         ["glyphs", "'"$BUILD"'/vfxglyphs.txt"]]) {
  if (d[key] !== h(fp)) {
    console.error("check-render: reuse REFUSED: digest mismatch on " + key + " (cached capture is stale vs current bytes)");
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
  rm -f "$BUILD/g01.render-run.json" "$BUILD/gfxdata.txt" \
        "$BUILD/vfxdata.txt" "$BUILD/vfxglyphs.txt"
  node "$GFX/capture-canvas.js" --golden g01 --frames-list "$FRAMES_LIST" \
    --out-dir "$CANVAS" --out-run "$BUILD/g01.render-run.json" \
    --gfxdata "$BUILD/gfxdata.txt" --vfxdata "$BUILD/vfxdata.txt" \
    --glyphs "$BUILD/vfxglyphs.txt"
fi
made "$BUILD/g01.render-run.json" "$BUILD/gfxdata.txt" \
     "$BUILD/vfxdata.txt" "$BUILD/vfxglyphs.txt" "$CANVAS/capture.digests.json"

# GFXDATA freeze tripwire (M3 task 4, iter 50): the committed
# port/gfx/gfxdata-frozen.txt is the browser-free device-path copy of
# this capture's GFXDATA1 dump (deterministic executed page data;
# sha-pinned by check-device-render.sh). Every fresh capture must
# byte-match it — legitimate upstream drift becomes a REVIEWED re-freeze
# of the committed artifact + its pin, never silent divergence.
cmp "$BUILD/gfxdata.txt" "$GFX/gfxdata-frozen.txt" || {
  echo "check-render: captured GFXDATA differs from the committed port/gfx/gfxdata-frozen.txt (re-freeze is a reviewed change)" >&2
  exit 1
}
echo "captured GFXDATA matches the committed frozen artifact"
# VFXDATA freeze tripwire (M4 task 2, same class): the committed
# port/gfx/vfxdata-frozen.txt is the executed vfx template plane; every
# fresh capture must byte-match it.
cmp "$BUILD/vfxdata.txt" "$GFX/vfxdata-frozen.txt" || {
  echo "check-render: captured VFXDATA differs from the committed port/gfx/vfxdata-frozen.txt (re-freeze is a reviewed change)" >&2
  exit 1
}
echo "captured VFXDATA matches the committed frozen artifact"
# VFXGLYPHS freeze tripwire (M4 task 2; comparison REPLACED iter 72 —
# glyph-jitter class fix): the iter-65 "x2 byte-identical in-session"
# measurement under-sampled the cross-session distribution — browser
# canvas TEXT rasterization flips single antialiased pixels across
# Math.round boundaries (~1-in-8 cold captures, driver find). The
# bit-exact cmp was therefore a FALSE oracle on healthy code; replacing
# it with the measured-then-frozen glyph-compare.js contract is a
# measurement-honesty correction, NOT a weakening: structure (names,
# dimensions, offsets, advances, counts, line shape) stays EXACT, pixel
# channels get the frozen tolerance (delta<=4, <=16 differing pixels of
# 19,764; measured max 1/1, .loop/m4-task2r72-probe.log), twin-pinned in
# glyph-compare.js + expected-render.json glyphComparePins. Legitimate
# browser/font drift still becomes a REVIEWED re-freeze, never silence.
node "$GFX/glyph-compare.js" --judge "$GFX/vfxglyphs-frozen.txt" \
  "$BUILD/vfxglyphs.txt" "$EXP" || {
  echo "check-render: captured VFXGLYPHS violates the frozen glyph-compare contract vs port/gfx/vfxglyphs-frozen.txt (re-freeze is a reviewed change)" >&2
  exit 1
}
echo "captured VFXGLYPHS matches the committed frozen artifact (glyph-compare contract)"
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

# INJECT1 (M4 task 2): the frozen expected-render.json inject table,
# re-emitted for the C side via String(x) (shortest round-trip — strtod
# recovers the exact doubles; the capture injected the same configs
# page-side). The injection-set pin is RE-asserted here (review-65 M1):
# this block is the C side's feed, so the pin check sits directly on the
# bytes the replay driver consumes.
rm -f "$BUILD/inject.txt" "$BUILD"/inject-loo-*.txt
node -e '
const e = require("./'"$EXP"'");
const inj = e.inject, pin = e.injectPin;
if (!inj || !Number.isInteger(inj.frame) || inj.frame < 1 ||
    !Array.isArray(inj.configs) || inj.configs.length === 0) {
  console.error("check-render: inject table missing/malformed in expected-render.json");
  process.exit(1);
}
if (!pin || !Array.isArray(pin.names) ||
    inj.configs.map(c => c && c.name).join(" ") !== pin.names.join(" ") ||
    inj.configs.length !== pin.count || inj.frame !== pin.frame ||
    !e.sampledFrames.includes(inj.frame)) {
  console.error("check-render: INJECT1 emitter — inject table does not match the reviewed injectPin");
  process.exit(1);
}
// review-65 r2 M5: the exact 5-name inkNames pin sits on the emitter
// too — the leave-one-out tables below are derived FROM inkNames.
if (!Array.isArray(pin.inkNames) || pin.inkNames.length !== 5 ||
    pin.inkNames.join(" ") !== "firefoxcharge firefoxtail shine dashDust groundBounce") {
  console.error("check-render: INJECT1 emitter — inkNames != the hard-pinned reviewed 5-name set");
  process.exit(1);
}
// Emit-side number grammar guard (review-65 r2 M1): the C parser is
// whitelist-exact — every emitted number must match the exact-decimal
// grammar it pins (String(x) emits exponent forms for extreme values;
// an out-of-grammar config is a config error, die at emit time).
const NUM_RE = /^-?(0|[1-9][0-9]*)(\.[0-9]+)?$/;
const numTok = (v, what) => {
  const s = String(v);
  if (typeof v !== "number" || !NUM_RE.test(s)) {
    console.error("check-render: inject config " + what +
      " fails the INJECT1 exact-decimal grammar: " + JSON.stringify(v));
    process.exit(1);
  }
  return s;
};
const vline = (c) => {
  if (typeof c.name !== "string" || !c.pos ||
      typeof c.pos.x !== "number" || typeof c.pos.y !== "number") {
    console.error("check-render: bad inject config " + JSON.stringify(c));
    process.exit(1);
  }
  const face = ("face" in c) ? numTok(c.face, c.name + ".face") : "-";
  const f = ("f" in c) ? numTok(c.f, c.name + ".f") : "-";
  return "V " + c.name + " " + numTok(c.pos.x, c.name + ".pos.x") + " " +
         numTok(c.pos.y, c.name + ".pos.y") + " " + face + " " + f;
};
const emit = (fp, configs) => {
  const lines = ["INJECT1", "AT " + inj.frame];
  for (const c of configs) lines.push(vline(c));
  lines.push("END");
  require("fs").writeFileSync(fp, lines.join("\n") + "\n");
};
emit("'"$BUILD"'/inject.txt", inj.configs);
// leave-one-out tables (review-65 M2, iter 67): for every inkNames
// effect, the SAME table minus exactly that config — the per-effect
// attribution baselines (iou.js diffs the full render against each).
// Names double as file-path tokens: enforce the identifier grammar.
for (const nm of pin.inkNames) {
  if (!/^[A-Za-z][A-Za-z0-9]*$/.test(nm)) {
    console.error("check-render: inkNames entry fails identifier grammar: " + JSON.stringify(nm));
    process.exit(1);
  }
  emit("'"$BUILD"'/inject-loo-" + nm + ".txt",
       inj.configs.filter(c => c.name !== nm));
}
'
made "$BUILD/inject.txt"

for side in a b; do
  rm -rf "$BUILD/render-$side"
  rm -f "$BUILD/g01.gfx-out-$side.txt"
  mkdir -p "$BUILD/render-$side"
  "$BUILD/gfx_replay" \
    --trace "$BUILD/g01.trace.txt" --simdata "$BUILD/simdata.txt" \
    --gfxdata "$BUILD/gfxdata.txt" --vfxdata "$BUILD/vfxdata.txt" \
    --glyphs "$BUILD/vfxglyphs.txt" --inject "$BUILD/inject.txt" \
    --anim-dir "$TABLES" \
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

# NO-INJECT baseline render (review-65 M2, iter 67): a third replay,
# identical to run-a but WITHOUT --inject, rendering ONLY the injection
# frame. Two consumers: (1) its full stdout stream must be byte-identical
# to run-a's — mechanical proof the injection is render-plane-only and
# never perturbs the sim; (2) iou.js diffs its injection-frame ink plane
# against run-a's to prove EACH injected effect contributes ink
# (per-effect differential tripwires — a stubbed draw arm cannot hide
# inside the aggregate IoU).
# String() — the registered node -p ANSI-colour class (CLAUDE.md M2
# task-17 gotcha: bare numbers get colorized inspect output).
INJ_FRAME=$(node -p "String(require('./$EXP').injectPin.frame)")
if ! [[ "$INJ_FRAME" =~ ^[0-9]+$ ]]; then
  echo "check-render: injectPin.frame extraction failed ('$INJ_FRAME')" >&2
  exit 1
fi
rm -rf "$BUILD/render-noinject"
rm -f "$BUILD/g01.gfx-out-noinject.txt"
mkdir -p "$BUILD/render-noinject"
"$BUILD/gfx_replay" \
  --trace "$BUILD/g01.trace.txt" --simdata "$BUILD/simdata.txt" \
  --gfxdata "$BUILD/gfxdata.txt" --vfxdata "$BUILD/vfxdata.txt" \
  --glyphs "$BUILD/vfxglyphs.txt" \
  --anim-dir "$TABLES" \
  --seed "$G_SEED" --p1 "$G_P1" --p2 "$G_P2" --stage "$G_STAGE" \
  --frames "$G_FRAMES" \
  --render-frames "$INJ_FRAME" --render-out "$BUILD/render-noinject" \
  > "$BUILD/g01.gfx-out-noinject.txt" \
  2> /dev/null
INJ_TAG=$(printf 'f%04d' "$INJ_FRAME")
made "$BUILD/g01.gfx-out-noinject.txt" "$BUILD/render-noinject/$INJ_TAG.pgm"
cmp "$BUILD/g01.gfx-out-noinject.txt" "$BUILD/g01.gfx-out-a.txt"
echo "no-inject baseline stream == run-a stream (injection is render-plane-only)"

# LEAVE-ONE-OUT baselines (review-65 M2, iter 67): one render per
# inkNames effect with exactly that config dropped from INJECT1 — the
# per-effect attribution baselines. diff(full run-a, leave-out-X) at the
# injection frame is caused only by X (queue spawn order bounds the
# render-RNG ripple; iou.js documents the argument), so a stubbed X draw
# arm makes the diff empty by construction — that is the tripwire the
# aggregate IoU cannot provide. Each stream is cmp'd against run-a's
# (every modified injection is render-plane-only too).
INK_NAMES=$(node -e '
const pin = require("./'"$EXP"'").injectPin;
for (const nm of pin.inkNames) {
  if (!/^[A-Za-z][A-Za-z0-9]*$/.test(nm)) { console.error("bad inkNames token"); process.exit(1); }
}
console.log(pin.inkNames.join(" "))')
if [ -z "$INK_NAMES" ]; then
  echo "check-render: inkNames extraction failed" >&2
  exit 1
fi
for nm in $INK_NAMES; do
  made "$BUILD/inject-loo-$nm.txt"
  rm -rf "$BUILD/render-loo-$nm"
  rm -f "$BUILD/g01.gfx-out-loo-$nm.txt"
  mkdir -p "$BUILD/render-loo-$nm"
  "$BUILD/gfx_replay" \
    --trace "$BUILD/g01.trace.txt" --simdata "$BUILD/simdata.txt" \
    --gfxdata "$BUILD/gfxdata.txt" --vfxdata "$BUILD/vfxdata.txt" \
    --glyphs "$BUILD/vfxglyphs.txt" --inject "$BUILD/inject-loo-$nm.txt" \
    --anim-dir "$TABLES" \
    --seed "$G_SEED" --p1 "$G_P1" --p2 "$G_P2" --stage "$G_STAGE" \
    --frames "$G_FRAMES" \
    --render-frames "$INJ_FRAME" --render-out "$BUILD/render-loo-$nm" \
    > "$BUILD/g01.gfx-out-loo-$nm.txt" \
    2> /dev/null
  made "$BUILD/g01.gfx-out-loo-$nm.txt" "$BUILD/render-loo-$nm/$INJ_TAG.pgm"
  cmp "$BUILD/g01.gfx-out-loo-$nm.txt" "$BUILD/g01.gfx-out-a.txt"
done
echo "leave-one-out baselines rendered (streams all == run-a: render-plane-only)"
# iter 52 parser audit (whitelist grammar): measured producer line
# (gfx_replay.c) is exactly
#   render-only ns: avg=<int> p50=<int> p99=<int> max=<int> (n=<frames>, host)
# — anchored full-line match with EXACTLY one occurrence (the old prefix
# grep accepted a truncated/mangled tail).
rt_re="^render-only ns: avg=[0-9]+ p50=[0-9]+ p99=[0-9]+ max=[0-9]+ \(n=${G_FRAMES}, host\)\$"
rt_cnt="$(grep -Ec "$rt_re" "$BUILD/g01.gfx-rtiming-a.txt")" || true
if [ "$rt_cnt" != 1 ]; then
  echo "check-render: expected exactly 1 render-timing line matching the pinned grammar, got $rt_cnt" >&2
  exit 1
fi
grep -E "$rt_re" "$BUILD/g01.gfx-rtiming-a.txt"

# --- 5. C stream still conforms with the renderer linked + active ------------------
rm -f "$BUILD/g01.gfx-run.json"
node port/sim/sim/wrap-run.js g01 "$BUILD/g01.gfx-out-a.txt" "$BUILD/g01.gfx-run.json"
made "$BUILD/g01.gfx-run.json"
node oracle/harness/verify-stream.js "$BUILD/g01.gfx-run.json" "$FROZEN"
echo "C render-on replay stream verified (renderer does not perturb the sim)"

# --- 6. silhouette IoU vs the frozen threshold + per-effect ink assertions ---------
# iou.js additionally consumes (iter 67): the no-inject baseline render
# (per-effect differential), the COMMITTED frozen VFXDATA1 (template
# bounds for region derivation) and the executed stages.json (stage
# transform — never hand-retyped engine values).
made "$TABLES/stages.json"
rm -f "$BUILD/iou-report.json"
node "$GFX/iou.js" --canvas "$CANVAS" --render "$BUILD/render-a" \
  --render-noinject "$BUILD/render-noinject" \
  --render-loo "$BUILD/render-loo" \
  --vfxdata "$GFX/vfxdata-frozen.txt" \
  --stages "$TABLES/stages.json" --stage "$G_STAGE" \
  --expected "$EXP" --report "$BUILD/iou-report.json"
made "$BUILD/iou-report.json"

# --- 7. FINAL closure-identity re-check (review-65 r2 M6) --------------------
# The capture bound the input-closure bytes it consumed into the sidecar
# (pre-consumption snapshot, re-verified at sidecar-write time), but the
# judging stages above run for tens of seconds AFTER that: a member
# edited post-capture (a normal editor save of gfx-pagelib.js is enough)
# would let this run print RENDER OK for current bytes that were never
# captured. Close the TOCTOU window: immediately before the verdict,
# re-derive the closure, re-hash EVERY member and the three data-plane
# digests, and require identity with the sidecar snapshot. Any drift =
# loud death, no verdict.
node -e '
const fs = require("fs"), crypto = require("crypto");
const h = (fp) => crypto.createHash("sha256").update(fs.readFileSync(fp)).digest("hex");
const side = "'"$CANVAS"'/capture.digests.json";
const d = JSON.parse(fs.readFileSync(side, "utf8"));
if (!d.closure || typeof d.closure !== "object" || Array.isArray(d.closure)) {
  console.error("check-render: FINAL closure check: sidecar has no closure map");
  process.exit(1);
}
const { closureFiles } = require("./port/gfx/capture-closure.js");
const files = closureFiles("'"$G_TRACE"'");
const want = Object.keys(files).sort();
const got = Object.keys(d.closure).sort();
if (want.join("\n") !== got.join("\n")) {
  console.error("check-render: FINAL closure check: member-set drift (sidecar: [" +
    got.join(", ") + "] vs current: [" + want.join(", ") + "])");
  process.exit(1);
}
for (const k of want) {
  if (d.closure[k] !== h(files[k])) {
    console.error("check-render: FINAL closure check: " + k +
      " changed after the capture (the judged masks were produced from different bytes) — no verdict");
    process.exit(1);
  }
}
for (const [key, fp] of [["gfxdata", "'"$BUILD"'/gfxdata.txt"],
                         ["vfxdata", "'"$BUILD"'/vfxdata.txt"],
                         ["glyphs", "'"$BUILD"'/vfxglyphs.txt"]]) {
  if (d[key] !== h(fp)) {
    console.error("check-render: FINAL closure check: " + key +
      " drifted after the capture — no verdict");
    process.exit(1);
  }
}
'
echo "final closure identity verified (no post-capture drift)"

echo "RENDER OK"
