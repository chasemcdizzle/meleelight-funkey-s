#!/usr/bin/env bash
# check-custom-stage.sh — the A45 T2 done-check: prove a PLAYER-AUTHORED
# target stage actually PLAYS, not merely that it parses.
#
# THE DIFFERENTIAL. Parsing is T1's (STAGECODE MATCH, differential against
# upstream's own executed encode.js). What T2 must prove is that a stage
# arriving as a .mlstage file collides, hosts a match, and has destroyable
# targets — through the real sim, nothing hand-poked. The strongest oracle
# available for that is an AUTHORED stage that already carries a FROZEN
# golden: re-express it as a share code, load it back through the custom
# path, replay the SAME golden trace, and require the two runs to be
# byte-identical — then judge BOTH with the UNCHANGED oracle verifiers
# against the frozen streams. Every frame of collision response, every
# hitbox, every target break is compared against evidence recorded from
# the browser, so "it plays" is measured and not asserted.
#
# The differential is sound only because the round trip is EXACT. MEASURED
# (executed walk over pipeline targets.json, all 10 authored stages): 210
# numbers, ZERO lossy at toFixed(2). Leg [3] re-measures it every run by
# requiring bit-identical streams; a lossy coordinate would show as a
# divergence, never as a silent pass.
#
# Legs:
#   [0] producer byte pins — the UNCHANGED oracle judges this check reuses
#   [1] build (every TU cc -O2 -ffp-contract=off -Wall -Wextra -Werror)
#       + the value-model/refusal self-test
#   [2] publish each golden's authored stage as a .mlstage and prove the
#       slot model: D43 index-addressing (writing slot 3 leaves the others
#       exactly as they were — upstream's list clobbers here), and the
#       load path's refusals
#   [3] per target golden: replay AUTHORED (--tstage) and CUSTOM (--custom)
#       over the same trace; cmp the two full outputs; judge BOTH runs'
#       player streams with the UNCHANGED verify-stream.js and both target
#       planes with verify-target-stream.js against the frozen goldens;
#       assert the CUSTOM run really destroyed targets
#   [1b] TWO LINK WITNESSES — gfx_target.o's undefined-symbol set is
#       unchanged, and target_main.c built WITHOUT custom_stage.c refuses
#       --custom: together, no existing builder of either file changes.
#       SEAM WITNESS — target_main.c built WITHOUT custom_stage.c refuses
#       --custom, so the other two builders of target_main.c
#       (check-target-sim.sh, port/foh/check-foh-flows.sh) need no change
#   [4] five ORTHOGONAL teeth, each biting a DIFFERENT leg
#   [5] no-commit guard
#
# Prints CUSTOM STAGE PLAYS, exit 0. Exact equality only; never weakened.
set -euo pipefail
cd "$(dirname "$0")/../../.."

fail() { echo "CUSTOM STAGE FAIL: $*" >&2; exit 1; }
relay_lines() { sed 's/^/  | /'; }
made() {
  local f
  for f in "$@"; do
    [ -s "$f" ] || fail "artifact $f missing or empty after its producer ran"
  done
}

M4G=port/goldens-m4
HARNESS=oracle/harness
TGT=port/sim/target
SIM=port/sim/sim
CAL=port/sim/calib
BUILD="$TGT/build-custom"
TABLES="$BUILD/tt-tables"
SLOTS="$BUILD/slots"

# --- [0] producer byte pins ---------------------------------------------------
# The judges are oracle/ bytes this check must never edit; if one drifts,
# this check's evidence is void and it says so instead of passing.
PRODUCER_PINS="\
f420723433b19166b53a80aedf54931ffdfbc6d2505c773fd73b7a13bbcdf60e oracle/harness/verify-stream.js
0bc801ea46b06a63e79377aae164636a5e9f649ee45835748e5f2387b9e04281 oracle/harness/streamlib.js"
n_pins=0
while IFS=' ' read -r psha ppath; do
  [ -n "$psha" ] || continue
  have="$(shasum -a 256 "$ppath" | awk '{print $1}')" || fail "cannot hash $ppath"
  [ "$have" = "$psha" ] \
    || fail "producer byte pin — $ppath sha256 $have != pinned $psha"
  n_pins=$((n_pins + 1))
done <<< "$PRODUCER_PINS"
[ "$n_pins" = 2 ] || fail "producer pin inventory — $n_pins verified, want 2"
echo "[0] producer byte pins OK ($n_pins)"

mkdir -p "$BUILD"
LOCK="$BUILD/check-custom-stage.lock"
if ! mkdir "$LOCK" 2>/dev/null; then
  echo "check-custom-stage.sh REFUSED: run lock $LOCK already exists (NO auto-reclaim)." >&2
  echo "  If you are sure no run is live: rm -rf '$LOCK'" >&2
  exit 1
fi
trap 'rm -rf "$LOCK"' EXIT

# --- [1] build ----------------------------------------------------------------
echo "[1] build sim_host_target (+ custom arm) and custom_stage_tool"
bash pipeline/extractor/build-extractor.sh >/dev/null
rm -rf "$TABLES"
node pipeline/run.js --only animations,tables,stages,targets --out "$TABLES" >/dev/null
made "$TABLES/ml_tables.c" "$TABLES/ml_stages.c" "$TABLES/ml_targets.c"

rm -f "$BUILD/simdata.txt"
node "$CAL/dump-sim-data.js" --out "$BUILD/simdata.txt" >/dev/null
made "$BUILD/simdata.txt"

# check-target-sim.sh's EXACT TU list (KEPT IN SYNC) plus the two A45 TUs
# the custom path adds: stage_code.c (T1's codec) and custom_stage.c.
CLUSTER_TUS=(
  "$SIM/sim_boot.c" "$SIM/sim_tick.c" "$SIM/sim_ser.c" "$SIM/sim_data.c"
  "$CAL/canon.c" "$CAL/player_canon.c"
  port/sim/physics.c port/sim/interpolated_collision.c
  port/sim/environmental_collision.c port/sim/hit_detection.c
  port/sim/article.c port/sim/action_state_shortcuts.c
  port/sim/ml_events.c port/sim/ml_fmt.c port/sim/ml_ser.c
  port/sim/ai_bridge.c port/sim/input/interpret_inputs.c
  port/sim/stages/moving_platforms.c port/sim/stages/ystory.c
  port/sim/stages/fountain.c
  port/sim/characters/shared/moves_index.c
  port/sim/characters/shared/moves/*.c
  port/sim/characters/fox/moves_index.c
  port/sim/characters/fox/moves/*.c
  port/sim/characters/falco/moves_index.c
  port/sim/characters/falco/moves/*.c
  port/sim/characters/falcon/moves_index.c
  port/sim/characters/falcon/moves/*.c
  port/sim/characters/marth/moves_index.c
  port/sim/characters/marth/dancing_blade_combo.c
  port/sim/characters/marth/dancing_blade_air_mobility.c
  port/sim/characters/marth/moves/*.c
  port/sim/characters/puff/moves_index.c
  port/sim/characters/puff/puff_multi_jump_drift.c
  port/sim/characters/puff/puff_next_jump.c
  port/sim/characters/puff/moves/*.c
  "$TABLES/ml_tables.c" "$TABLES/ml_stages.c" "$TABLES/ml_targets.c"
  oracle/qjs/sha256.c port/fdlibm/fdlibm.c
)
CFLAGS=(-O2 -ffp-contract=off -Wall -Wextra -Werror
        -I"$TABLES" -Iport/ryu -Iport/sim -Ioracle/qjs)
# The A45 TUs the custom path adds on top of that list.
A45_TUS=(port/sim/stage_code.c "$TGT/custom_stage.c")

# sim_host_target and custom_stage_tool each carry their OWN main and
# share everything else — including custom_stage.c itself, so the tool
# that PUBLISHES a .mlstage and the sim that PLAYS one are the same bytes.
rm -f "$BUILD/sim_host_target" "$BUILD/custom_stage_tool"
cc "${CFLAGS[@]}" -o "$BUILD/sim_host_target" "$TGT/target_main.c" \
  "$TGT/target_play.c" "${A45_TUS[@]}" "${CLUSTER_TUS[@]}" -lm
cc "${CFLAGS[@]}" -o "$BUILD/custom_stage_tool" "$TGT/custom_stage_tool.c" \
  "$TGT/target_play.c" "${A45_TUS[@]}" "${CLUSTER_TUS[@]}" -lm
made "$BUILD/sim_host_target" "$BUILD/custom_stage_tool"
echo "    build OK (cc -O2 -ffp-contract=off -Wall -Wextra -Werror)"
"$BUILD/custom_stage_tool" --self-test | relay_lines

# GFX LINK WITNESS. gfx_target.c is linked by SIX scripts, four of them in
# port/foh/ or the device rigs. gfx_target_init_custom must therefore add no
# link edge: MlkStage is a TYPE there and nothing from custom_stage.c is
# called. Proven, not asserted — the undefined-symbol set of gfx_target.o
# must be IDENTICAL to the committed file's.
git show HEAD:port/gfx/gfx_target.c > "$BUILD/gfx_target.orig.c"
cc "${CFLAGS[@]}" -Iport/gfx -c -o "$BUILD/gt.new.o" port/gfx/gfx_target.c
cc "${CFLAGS[@]}" -Iport/gfx -c -o "$BUILD/gt.orig.o" "$BUILD/gfx_target.orig.c"
diff <(nm -u "$BUILD/gt.orig.o" | sort) <(nm -u "$BUILD/gt.new.o" | sort) \
  >/dev/null \
  || fail "gfx_target.c gained an undefined symbol — every builder of it (incl. port/foh/check-foh-flows.sh) would need a TU list change"
echo "    gfx link witness: gfx_target.o undefined-symbol set unchanged"

# IN-CHECK CONTRACT WITNESS. The --custom arm sits behind the tp_custom_setup
# pointer seam precisely so that check-target-sim.sh and port/foh/'s
# check-foh-flows.sh — the other two builders of target_main.c — keep their TU
# lists unchanged. Build target_main.c WITHOUT custom_stage.c and require that
# --custom is REFUSED, which is the same claim stated as an executable fact
# instead of a comment.
rm -f "$BUILD/sim_host_nocustom"
cc "${CFLAGS[@]}" -o "$BUILD/sim_host_nocustom" "$TGT/target_main.c" \
  "$TGT/target_play.c" port/sim/stage_code.c "${CLUSTER_TUS[@]}" -lm
made "$BUILD/sim_host_nocustom"
if "$BUILD/sim_host_nocustom" --trace /dev/null --simdata /dev/null --seed 1 \
   --char 0 --custom /tmp 0 --frames 1 >/dev/null 2>"$BUILD/nocustom.err"; then
  fail "a build without custom_stage.c must REFUSE --custom, not accept it"
fi
grep -q "no custom-stage support" "$BUILD/nocustom.err" \
  || fail "the unlinked build refused --custom for the wrong reason: $(cat "$BUILD/nocustom.err")"
echo "    seam witness: without custom_stage.c, --custom is refused (so the"
echo "                  other two builders of target_main.c are unaffected)"

# --- [2] the slot plane -------------------------------------------------------
echo "[2] the .mlstage slot plane (D43: ten slots, addressed by INDEX)"
rm -rf "$SLOTS"; mkdir -p "$SLOTS"
# Publish three DIFFERENT authored stages into slots 0, 1, 3 — the exact
# sequence that destroys data upstream (targetselect.js:164-166: add A,
# add B, add C leaves ["B","C","C"]). Here each slot is its own file and
# writing one cannot reach another, so all three survive and slot 2 stays
# empty rather than being shifted into.
"$BUILD/custom_stage_tool" --emit 0 "$SLOTS/custom0.mlstage"
"$BUILD/custom_stage_tool" --emit 1 "$SLOTS/custom1.mlstage"
sum0="$(shasum -a 256 "$SLOTS/custom0.mlstage" | awk '{print $1}')"
sum1="$(shasum -a 256 "$SLOTS/custom1.mlstage" | awk '{print $1}')"
"$BUILD/custom_stage_tool" --emit 2 "$SLOTS/custom3.mlstage"
[ "$(shasum -a 256 "$SLOTS/custom0.mlstage" | awk '{print $1}')" = "$sum0" ] \
  || fail "D43: publishing slot 3 disturbed slot 0 (the upstream clobber)"
[ "$(shasum -a 256 "$SLOTS/custom1.mlstage" | awk '{print $1}')" = "$sum1" ] \
  || fail "D43: publishing slot 3 disturbed slot 1 (the upstream clobber)"
[ "$sum0" != "$sum1" ] || fail "slots 0 and 1 must hold DIFFERENT stages"
for s in 0 1 3; do
  "$BUILD/custom_stage_tool" --load "$SLOTS" "$s" >/dev/null \
    || fail "slot $s did not load back"
done
# An empty slot is a NAMED refusal, never a silent hole and never a shift.
out="$("$BUILD/custom_stage_tool" --load "$SLOTS" 2 || true)"
case "$out" in
  "REFUSED no such slot file") : ;;
  *) fail "empty slot 2 must refuse by name, got: $out" ;;
esac
echo "    3 slots published by index, all three intact, empty slot named"

# --- [3] the differential -----------------------------------------------------
node "$M4G/validate-target-manifest.js" >/dev/null \
  || fail "manifest-target.json failed the shared strict validator"
IDS="$(node -e 'const m=require("./port/goldens-m4/validate-target-manifest").loadValidatedManifest(); process.stdout.write(m.goldens.map(g=>g.id).join(" "));')"
[ -n "$IDS" ] || fail "no target goldens in the manifest"
echo "[3] authored vs custom over every target golden: $IDS"

for id in $IDS; do
  read -r name trace frames seed char tstage <<< "$(node -e '
    const v=require("./port/goldens-m4/validate-target-manifest");
    const g=v.goldenByIdOrName(v.loadValidatedManifest(),process.argv[1]);
    process.stdout.write([g.name,g.trace,g.frames,g.seed,g.char,g.tstage].join(" "));
  ' "$id")"

  rm -f "$BUILD/$id.trace.txt"
  node "$SIM/trace-to-txt.js" "$M4G/$trace" "$BUILD/$id.trace.txt" >/dev/null
  made "$BUILD/$id.trace.txt"

  # (a) the AUTHORED run — the control.
  rm -f "$BUILD/$id.auth.out"
  "$BUILD/sim_host_target" --trace "$BUILD/$id.trace.txt" \
    --simdata "$BUILD/simdata.txt" --seed "$seed" --char "$char" \
    --tstage "$tstage" --frames "$frames" > "$BUILD/$id.auth.out"
  made "$BUILD/$id.auth.out"

  # (b) the SAME stage, re-expressed as a share code and played from a file.
  rm -rf "$BUILD/$id.slots"; mkdir -p "$BUILD/$id.slots"
  "$BUILD/custom_stage_tool" --emit "$tstage" "$BUILD/$id.slots/custom0.mlstage"
  made "$BUILD/$id.slots/custom0.mlstage"
  rm -f "$BUILD/$id.cust.out"
  "$BUILD/sim_host_target" --trace "$BUILD/$id.trace.txt" \
    --simdata "$BUILD/simdata.txt" --seed "$seed" --char "$char" \
    --custom "$BUILD/$id.slots" 0 --frames "$frames" > "$BUILD/$id.cust.out"
  made "$BUILD/$id.cust.out"

  # THE DIFFERENTIAL: same geometry, two delivery paths, one stream.
  cmp "$BUILD/$id.auth.out" "$BUILD/$id.cust.out" \
    || fail "$id: the custom path diverged from the authored path"

  # And BOTH judged against the FROZEN browser-recorded goldens by the
  # UNCHANGED verifiers — so the differential cannot pass by both sides
  # being wrong in the same way.
  for side in auth cust; do
    rm -f "$BUILD/$id.$side.player.json" "$BUILD/$id.$side.target.json"
    node "$M4G/wrap-target.js" "$id" "$BUILD/$id.$side.out" \
      "$BUILD/$id.$side.player.json" "$BUILD/$id.$side.target.json" >/dev/null
    made "$BUILD/$id.$side.player.json" "$BUILD/$id.$side.target.json"
    (cd "$HARNESS" && node verify-stream.js \
      "../../$BUILD/$id.$side.player.json" "../../$M4G/$name.sha256.json") \
      >/dev/null || fail "$id ($side): player stream != the frozen golden"
    node "$M4G/verify-target-stream.js" "$BUILD/$id.$side.target.json" \
      "$M4G/$name.target.sha256.json" >/dev/null \
      || fail "$id ($side): target stream != the frozen golden"
  done

  # PLAYED, not merely loaded: the custom run really broke targets. (The
  # manifest's quality contract pins minTargets >= 2 for every golden.)
  tfin="$(grep '^TFIN ' "$BUILD/$id.cust.out")"
  n="$(echo "$tfin" | awk '{print $2}')"
  [ "$n" -ge 2 ] \
    || fail "$id: the custom run destroyed only $n targets (want >= 2)"
  echo "    $id: authored == custom, both == frozen, $n targets destroyed"
done

# --- [4] teeth ----------------------------------------------------------------
# Five teeth, five DIFFERENT legs. Each perturbs a SCRATCH artifact; no
# tracked file is ever edited.
echo "[4] teeth"
TID="$(echo "$IDS" | awk '{print $1}')"
read -r tname ttrace tframes tseed tchar ttstage <<< "$(node -e '
  const v=require("./port/goldens-m4/validate-target-manifest");
  const g=v.goldenByIdOrName(v.loadValidatedManifest(),process.argv[1]);
  process.stdout.write([g.name,g.trace,g.frames,g.seed,g.char,g.tstage].join(" "));
' "$TID")"
TOOTH="$BUILD/tooth"; rm -rf "$TOOTH"; mkdir -p "$TOOTH"

# Control: the unperturbed file loads. Without it a tooth that "bites"
# because the rig is broken would look like a pass.
"$BUILD/custom_stage_tool" --emit "$ttstage" "$TOOTH/custom0.mlstage"
"$BUILD/custom_stage_tool" --load "$TOOTH" 0 >/dev/null \
  || fail "tooth control: the unperturbed file must load"

bites() { # <name> <expected-reason-substring>
  local name=$1 want=$2
  local out
  out="$("$BUILD/custom_stage_tool" --load "$TOOTH" 0 2>&1 || true)"
  case "$out" in
    REFUSED*"$want"*) echo "    tooth '$name' bites: $out" ;;
    *) fail "tooth '$name' did not bite — got: $out" ;;
  esac
}

# (a) INTEGRITY leg: one flipped byte in the code, SUM left alone.
python3 - "$TOOTH/custom0.mlstage" <<'PY'
import sys
p=sys.argv[1]; b=bytearray(open(p,'rb').read())
i=b.index(b'\n')+5          # inside the code line
b[i]=ord('9') if b[i]!=ord('9') else ord('8')
open(p,'wb').write(b)
PY
bites corrupt-byte "SUM mismatch"

# (b) GRAMMAR leg: a truncated file — the measured power-loss shape on a
#     journal-less vfat /mnt. SUM would also fail, so the truncation must
#     be caught by the grammar FIRST for this to be a distinct leg: the
#     reason string is what distinguishes them.
"$BUILD/custom_stage_tool" --emit "$ttstage" "$TOOTH/custom0.mlstage"
python3 - "$TOOTH/custom0.mlstage" <<'PY'
import sys
p=sys.argv[1]; b=open(p,'rb').read()
open(p,'wb').write(b[:len(b)//2])
PY
bites truncated "truncated file"

# (c) DAMAGE leg: a VALID file (correct SUM) carrying a damaging surface —
#     the plane no golden covers. The content rule must refuse it, not the
#     integrity rule.
"$BUILD/custom_stage_tool" --emit-damage "$ttstage" "$TOOTH/custom0.mlstage"
bites damage-surface "damaging surface"

# (d) CAP leg (R2): a VALID file with 11 targets — legal in the codec
#     (cap 20) and legal upstream (JS arrays grow), over this build's cap.
"$BUILD/custom_stage_tool" --emit-targets "$ttstage" 11 "$TOOTH/custom0.mlstage"
bites over-cap-targets "too many targets"

# (e) THE PLAY leg — the one that proves [3] is not vacuous. Move the
#     stage's collision geometry and require the replay to DIVERGE from
#     the authored run. Without this, [3] would pass just as happily if
#     the sim ignored the custom geometry entirely.
#     MEASURED: a first version shifted only GROUND SURFACE 0 and did NOT
#     bite — on t01 that surface is a ledge at y=88 the fox never touches.
#     Same class as the project's three earlier razor-thin-nudge no-ops
#     (fix_plan rule-12 corollary): a tooth must perturb the domain that
#     OCCURS, not the first row of it. Shifting every collision surface in
#     all five lists is the version that cannot miss.
"$BUILD/custom_stage_tool" --emit "$ttstage" "$TOOTH/raw.mlstage"
python3 - "$TOOTH/raw.mlstage" "$TOOTH/moved.mlstage" <<'PY'
import sys
src, dst = sys.argv[1], sys.argv[2]
lines = open(src, 'r').read().split('\n')
f = lines[1].split('&')
moved = 0
for i in (2, 3, 4, 5, 6):          # ground, ceiling, wallL, wallR, platform
    if not f[i]:
        continue
    recs = []
    for r in f[i].split('~'):
        c = r.split(',')                       # x1,y1,x2,y2[,d] — a
        c[1] = "%.2f" % (float(c[1]) + 1.0)    # TRAILING EMPTY d token is
        c[3] = "%.2f" % (float(c[3]) + 1.0)    # upstream BUG 1's own
        recs.append(','.join(c))               # output; it must survive
        moved += 1
    f[i] = '~'.join(recs)
if moved == 0:
    raise SystemExit("tooth: no collision surface to move")
lines[1] = '&'.join(f)
open(dst, 'w').write(lines[0] + '\n' + lines[1] + '\n')
PY
"$BUILD/custom_stage_tool" --resum "$TOOTH/moved.mlstage" "$TOOTH/custom0.mlstage"
"$BUILD/custom_stage_tool" --load "$TOOTH" 0 >/dev/null \
  || fail "tooth 'moved-ground': the perturbed file must still be VALID"
rm -f "$TOOTH/moved.out"
"$BUILD/sim_host_target" --trace "$BUILD/$TID.trace.txt" \
  --simdata "$BUILD/simdata.txt" --seed "$tseed" --char "$tchar" \
  --custom "$TOOTH" 0 --frames "$tframes" > "$TOOTH/moved.out"
if cmp -s "$TOOTH/moved.out" "$BUILD/$TID.auth.out"; then
  fail "tooth 'moved-ground' did not bite — moving the ground by one world unit left the stream unchanged, so the sim is not reading the custom geometry"
fi
echo "    tooth 'moved-ground' bites: a 1.00-unit shift of every collision surface changes the stream"

# --- [5] no-commit guard ------------------------------------------------------
if git status --porcelain -- "$BUILD" | grep -q .; then
  fail "build output is not gitignored"
fi

echo "CUSTOM STAGE PLAYS"
