#!/usr/bin/env bash
# check-target-sim.sh — the M4 task 11 done-check: prove the target-test
# data + sim plane conforms host-side (fix_plan §M4 task 11).
#
# Composes:
#   [0] PRODUCER BYTE PINS — the UNCHANGED oracle judges this check reuses
#       (oracle/harness/verify-stream.js + streamlib.js): a drift is a
#       loud fail (a legitimate reviewed change updates the pin same-commit).
#   [1] pipeline/check-targets.sh (relayed) — the TTAB1 targets stage:
#       x2 byte-stability, artifact hashes, expected.json pins, and the
#       compiled-C vs executed-JS round trip (718 leaves).
#   [2] BUILD sim_host_target + target_hq_probe from check-sim.sh's EXACT
#       TU list (kept in sync) + the port/sim/target TUs, every TU
#       cc -O2 -ffp-contract=off -Wall -Wextra -Werror.
#   [3] the SHARED strict manifest validator (validate-target-manifest.js,
#       review-94 H1 — duplicate ids/names/traces, key order, domains,
#       containment; run BEFORE any row is trusted), then per target
#       golden: regen its trace via the committed generator (byte-identity
#       guard), trace->txt, replay, wrap-target, judge the PLAYER stream
#       via the UNCHANGED verify-stream.js and the TARGET plane via
#       verify-target-stream.js — exact per-frame equality, full length,
#       both streams, frozen metadata bound to the manifest + the player
#       sibling's own seal (review-94 H2).
#   [4] target_hq_probe (the stage-damage CONSUME-path probe) + its --drop
#       negative arm.
#   [5] TEETH — run-side T1-T6 (nibble/drop perturbations on generated
#       COPIES) + frozen-side T7-T24 (review-94 L2/H1 + review-96
#       C-M3/C-L6/G-L7/G-M4/C-L5/C-M1/C-M2: perturbed COPIES of the
#       FROZEN/manifest/run files judged by the PRODUCTION judges after
#       untouched-copy PASS controls; T12 is the copied-tree INTEGRATION
#       dup-id tooth — the production consumer bytes, cmp-proven, die on
#       a duplicate-id manifest; committed bytes never edited).
#   [6] no-commit guard over the build + goldens dirs; a no-reclaim run
#       lock; made()/rm-before-produce throughout.
# Prints TARGET SIM CONFORMS and exits 0 on success.
set -euo pipefail
cd "$(dirname "$0")/../../.."

fail() { echo "TARGET SIM FAIL: $*" >&2; exit 1; }

# A PERTURBATION THAT PERTURBS NOTHING IS WORSE THAN NO TOOTH: it reads green
# forever and then accuses the code the day the data moves. MEASURED in this
# feature — check-match-resume.sh's `s/^SUM \(.\)/SUM 0/` flipped the SUM's
# first hex digit unless it was already `0`, and one build in sixteen it is.
# Every in-place edit below goes through here, and a no-op fails LOUDLY,
# naming the fixture instead of the feature.
perturb() { # <file> <sed-arg>... — apply, and prove the bytes moved
  local f="$1"; shift
  local before after
  before="$(shasum -a 256 < "$f")"
  sed -i.bak "$@" "$f"
  rm -f "$f.bak"
  after="$(shasum -a 256 < "$f")"
  [ "$before" != "$after" ] || fail "FIXTURE NO-OP: the perturbation
  $* did not change $f. The tooth it feeds cannot fail, so a green run here
  proves nothing. Fix the fixture; do NOT relax the assertion downstream."
}
grammar_die() { echo "TARGET SIM FAIL: $*" >&2; exit 2; }
relay_lines() { sed 's/^/  | /'; }
made() {
  local f
  for f in "$@"; do
    [ -s "$f" ] || fail "artifact $f missing or empty after its producer ran (rm-before-produce)"
  done
}

DIST="${MELEELIGHT_CLONE:-$HOME/.cache/meleelight-funkey-s/upstream}"
[ -f "$DIST/dist/meleelight.html" ] || fail "no built upstream at $DIST"

M4G=port/goldens-m4
HARNESS=oracle/harness
TGT=port/sim/target
SIM=port/sim/sim
CAL=port/sim/calib
BUILD="$TGT/build"
TABLES="$BUILD/tt-tables"

# --- [0] PRODUCER BYTE PINS ---------------------------------------------------
PRODUCER_PINS="\
f420723433b19166b53a80aedf54931ffdfbc6d2505c773fd73b7a13bbcdf60e oracle/harness/verify-stream.js
0bc801ea46b06a63e79377aae164636a5e9f649ee45835748e5f2387b9e04281 oracle/harness/streamlib.js"
N_PINS_WANT=2
n_pins=0
while IFS=' ' read -r psha ppath; do
  [ -n "$psha" ] || continue
  have="$(shasum -a 256 "$ppath" | awk '{print $1}')" \
    || fail "cannot hash producer $ppath"
  [ "$have" = "$psha" ] \
    || fail "producer byte pin — $ppath sha256 $have != pinned $psha (a producer change invalidates this check's evidence; a reviewed change updates the pin same-commit)"
  n_pins=$((n_pins + 1))
done <<< "$PRODUCER_PINS"
[ "$n_pins" = "$N_PINS_WANT" ] \
  || grammar_die "producer pin inventory — $n_pins verified, want exactly $N_PINS_WANT (a dropped pin line is corruption)"
echo "[0] producer byte pins OK ($n_pins)"

# --- run lock (no-reclaim) ----------------------------------------------------
mkdir -p "$BUILD"
LOCK="$BUILD/check-target-sim.lock"
if ! mkdir "$LOCK" 2>/dev/null; then
  echo "check-target-sim.sh REFUSED: run lock $LOCK already exists (NO auto-reclaim)." >&2
  echo "  If you are sure no run is live: rm -rf '$LOCK'" >&2
  exit 1
fi
trap 'rm -rf "$LOCK"' EXIT

# --- [1] pipeline targets stage (relayed; run ONCE, verdict from the
#         captured output — iter 96 mechanical change, semantics kept) ---------
echo "[1] pipeline/check-targets.sh"
CT_OUT="$BUILD/check-targets.out"
rm -f "$CT_OUT"
if ! bash pipeline/check-targets.sh > "$CT_OUT" 2>&1; then
  relay_lines < "$CT_OUT"
  fail "pipeline/check-targets.sh failed"
fi
relay_lines < "$CT_OUT"
grep -qx "TARGETS OK" "$CT_OUT" \
  || fail "pipeline/check-targets.sh did not print TARGETS OK"
rm -f "$CT_OUT"

# --- [2] build sim_host_target + target_hq_probe ------------------------------
echo "[2] build sim_host_target + target_hq_probe"
bash pipeline/extractor/build-extractor.sh >/dev/null
rm -rf "$TABLES"
node pipeline/run.js --only animations,tables,stages,targets --out "$TABLES" >/dev/null
made "$TABLES/ml_tables.c" "$TABLES/ml_stages.c" "$TABLES/ml_targets.c"

# SIMDATA1 (the executed move-data plane; determinism x2)
rm -f "$BUILD/simdata.txt" "$BUILD/simdata.b.txt"
node "$CAL/dump-sim-data.js" --out "$BUILD/simdata.txt" >/dev/null
node "$CAL/dump-sim-data.js" --out "$BUILD/simdata.b.txt" >/dev/null
cmp "$BUILD/simdata.txt" "$BUILD/simdata.b.txt" \
  || fail "simdata differs across two fresh dumps"
rm -f "$BUILD/simdata.b.txt"
made "$BUILD/simdata.txt"

# TU list: check-sim.sh's EXACT list (KEPT IN SYNC) minus sim_main.c, plus
# the port/sim/target TUs. sim_host_target and target_hq_probe each carry
# their OWN main; the shared cluster TUs are identical.
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
  # A45 T6: t03 plays a CUSTOM stage, so this build needs the codec and the
  # custom-stage loader. sim_host_target refuses `--custom` without them
  # (target_main.c checks tp_custom_setup before any work), so a build that
  # dropped these would fail loudly rather than skip the golden.
  "$TGT/custom_stage.c" port/sim/stage_code.c
  oracle/qjs/sha256.c port/fdlibm/fdlibm.c
)
CFLAGS=(-O2 -ffp-contract=off -Wall -Wextra -Werror
        -I"$TABLES" -Iport/ryu -Iport/sim -Ioracle/qjs)

rm -f "$BUILD/sim_host_target" "$BUILD/target_hq_probe" \
  "$BUILD/target_finish_probe"
cc "${CFLAGS[@]}" -o "$BUILD/sim_host_target" \
  "$TGT/target_main.c" "$TGT/target_play.c" "${CLUSTER_TUS[@]}" -lm
cc "${CFLAGS[@]}" -o "$BUILD/target_hq_probe" \
  "$TGT/target_hq_probe.c" "$TGT/target_play.c" "${CLUSTER_TUS[@]}" -lm
cc "${CFLAGS[@]}" -o "$BUILD/target_finish_probe" \
  "$TGT/target_finish_probe.c" "$TGT/target_play.c" "${CLUSTER_TUS[@]}" -lm
made "$BUILD/sim_host_target" "$BUILD/target_hq_probe" \
  "$BUILD/target_finish_probe"
echo "    build OK (cc -O2 -ffp-contract=off -Wall -Wextra -Werror)"

# --- [3] per target golden: C replay vs BOTH frozen streams -------------------
# SHARED strict manifest validation FIRST (review-94 H1): a duplicate id
# dies HERE naming the dup — it can never surface as "2 goldens" while
# judging one twice. Scratch paths below derive from the now-validated
# unique ids.
# A45 T6. An AUTHORED golden passes `--tstage N`; a CUSTOM one publishes the
# manifest's share code as a slot file and passes `--custom <dir> <slot>`.
# The file is written HERE, from the manifest, in A45 T2's on-disk contract
# (custom_stage.h: three LF lines MLSTAGE1 / code / `SUM <64 hex>` over every
# preceding byte) — so the check exercises the REAL loader, SUM verification
# and all, rather than a back door that hands the sim a parsed stage.
target_stage_args() { # <id> <tstage>
  local id="$1" tstage="$2" code dir body
  if [ "$tstage" -lt 10 ]; then
    printf -- '--tstage %s' "$tstage"
    return 0
  fi
  code="$(node -e '
    const m = require("./port/goldens-m4/validate-target-manifest").loadValidatedManifest();
    const g = m.goldens.find((x) => x.id === process.argv[1]);
    if (!g) { process.exit(1); }
    process.stdout.write(g.customStage);
  ' "$id")" || { echo "TARGET SIM FAIL: no customStage for $id" >&2; exit 1; }
  [ -n "$code" ] || { echo "TARGET SIM FAIL: $id customStage is empty" >&2; exit 1; }
  dir="$BUILD/slots-$id"
  rm -rf "$dir" && mkdir -p "$dir"
  body="$dir/.body"
  printf 'MLSTAGE1\n%s\n' "$code" > "$body"
  printf 'MLSTAGE1\n%s\nSUM %s\n' "$code" \
    "$(shasum -a 256 "$body" | cut -d' ' -f1)" \
    > "$dir/custom$((tstage - 10)).mlstage"
  rm -f "$body"
  printf -- '--custom %s %s' "$dir" "$((tstage - 10))"
}

node port/goldens-m4/validate-target-manifest.js 2>&1 | relay_lines \
  || fail "manifest-target.json failed the shared strict validator"
IDS="$(node -e 'const m=require("./port/goldens-m4/validate-target-manifest").loadValidatedManifest(); process.stdout.write(m.goldens.map(g=>g.id).join(" "));')" \
  || fail "manifest-target.json failed the shared strict validator (IDS pull)"
[ -n "$IDS" ] || fail "no target goldens in the manifest"
echo "[3] C replay per golden: $IDS"
for id in $IDS; do
  read -r name trace frames seed char tstage <<< "$(node -e '
    const v=require("./port/goldens-m4/validate-target-manifest");
    const g=v.goldenByIdOrName(v.loadValidatedManifest(),process.argv[1]);
    process.stdout.write([g.name,g.trace,g.frames,g.seed,g.char,g.tstage].join(" "));
  ' "$id")"
  # committed-generator byte-identity: regen the trace and cmp against the
  # committed copy (a drifted generator can never fabricate a fresh trace).
  gen="$M4G/gen-${name%%-*}-trace.js"
  [ -f "$gen" ] || fail "$id: committed generator $gen missing"
  node "$gen" "$BUILD/$id.regen.trace.json" >/dev/null
  cmp "$M4G/$trace" "$BUILD/$id.regen.trace.json" \
    || fail "$id: committed trace != its generator's fresh output (regen drift)"

  rm -f "$BUILD/$id.trace.txt" "$BUILD/$id.sim.out"
  node "$SIM/trace-to-txt.js" "$M4G/$trace" "$BUILD/$id.trace.txt" >/dev/null
  made "$BUILD/$id.trace.txt"
  "$BUILD/sim_host_target" --trace "$BUILD/$id.trace.txt" \
    --simdata "$BUILD/simdata.txt" --seed "$seed" --char "$char" \
    $(target_stage_args "$id" "$tstage") --frames "$frames" > "$BUILD/$id.sim.out"
  made "$BUILD/$id.sim.out"

  rm -f "$BUILD/$id.player.json" "$BUILD/$id.target.json"
  node "$M4G/wrap-target.js" "$id" "$BUILD/$id.sim.out" \
    "$BUILD/$id.player.json" "$BUILD/$id.target.json"
  made "$BUILD/$id.player.json" "$BUILD/$id.target.json"

  node "$HARNESS/verify-stream.js" "$BUILD/$id.player.json" \
    "$M4G/$name.sha256.json" | relay_lines
  node "$M4G/verify-target-stream.js" "$BUILD/$id.target.json" \
    "$M4G/$name.target.sha256.json" | relay_lines
done

# --- [4] the standing probes --------------------------------------------------
echo "[4] target_hq_probe (stage-damage consume path)"
"$BUILD/target_hq_probe" --simdata "$BUILD/simdata.txt" | relay_lines
if "$BUILD/target_hq_probe" --simdata "$BUILD/simdata.txt" --drop >/dev/null 2>&1; then
  fail "target_hq_probe --drop did NOT fail (the probe does not bite)"
fi
echo "    probe OK; --drop arm bites"
# [4b] the FINISH-SEAM probe (M4 task 12; AGENT-LOG iter 99): the
# endTargetGame -> finishGame arm + the post-finish skip ticks + the
# double-destroy quirk's Failure arm, MECHANICALLY live (the authored
# game cannot reach all-broken — targetstage9's single target is
# topologically sealed, measured; no golden can carry the finish).
echo "[4b] target_finish_probe (finishGame seam + double-destroy quirk)"
"$BUILD/target_finish_probe" --simdata "$BUILD/simdata.txt" | relay_lines
if "$BUILD/target_finish_probe" --simdata "$BUILD/simdata.txt" --drop >/dev/null 2>&1; then
  fail "target_finish_probe --drop did NOT fail (the probe does not bite)"
fi
echo "    finish probe OK; --drop arm bites"

# --- [5] TEETH (generated copies; committed bytes never edited) ----------------
echo "[5] teeth"
TID="$(echo $IDS | awk '{print $1}')" # the first golden (t01)
read -r tname ttrace tframes tseed tchar ttstage tmin <<< "$(node -e '
  const v=require("./port/goldens-m4/validate-target-manifest");
  const g=v.goldenByIdOrName(v.loadValidatedManifest(),process.argv[1]);
  process.stdout.write([g.name,g.trace,g.frames,g.seed,g.char,g.tstage,
    g.minTargets].join(" "));
' "$TID")"

# T1 — PLAYER-stream nibble: flip one hex char of a mid-frame F line in a
# COPY of the sim output -> verify-stream must diverge.
cp "$BUILD/$TID.sim.out" "$BUILD/tooth.sim.out"
python3 - "$BUILD/tooth.sim.out" <<'PY'
import sys, re
p=sys.argv[1]; L=open(p).read().splitlines()
for i,l in enumerate(L):
    m=re.match(r'^F 1800 ([0-9a-f]{64})$', l)
    if m:
        h=list(m.group(1)); h[0]='0' if h[0]!='0' else '1'; L[i]='F 1800 '+''.join(h); break
open(p,'w').write('\n'.join(L)+'\n')
PY
node "$M4G/wrap-target.js" "$TID" "$BUILD/tooth.sim.out" \
  "$BUILD/tooth.player.json" "$BUILD/tooth.target.json" >/dev/null
if node "$HARNESS/verify-stream.js" "$BUILD/tooth.player.json" \
    "$M4G/$tname.sha256.json" >/dev/null 2>&1; then
  fail "T1 player-nibble did NOT diverge"
fi
echo "    T1 player-stream nibble -> verify-stream diverges"

# T2 — TARGET-plane nibble: flip a T line hash in a COPY -> verify-target
# must diverge (run-side tamper per the M3 task-2 lesson).
cp "$BUILD/$TID.sim.out" "$BUILD/tooth2.sim.out"
python3 - "$BUILD/tooth2.sim.out" <<'PY'
import sys, re
p=sys.argv[1]; L=open(p).read().splitlines()
for i,l in enumerate(L):
    m=re.match(r'^T 1800 ([0-9a-f]{64})$', l)
    if m:
        h=list(m.group(1)); h[0]='0' if h[0]!='0' else '1'; L[i]='T 1800 '+''.join(h); break
open(p,'w').write('\n'.join(L)+'\n')
PY
node "$M4G/wrap-target.js" "$TID" "$BUILD/tooth2.sim.out" \
  "$BUILD/tooth2.player.json" "$BUILD/tooth2.target.json" >/dev/null
if node "$M4G/verify-target-stream.js" "$BUILD/tooth2.target.json" \
    "$M4G/$tname.target.sha256.json" >/dev/null 2>&1; then
  fail "T2 target-nibble did NOT diverge"
fi
echo "    T2 target-plane nibble -> verify-target-stream diverges"

# T3 — TFIN count perturb: alter the reported targetsDestroyed in a COPY ->
# verify-target's finals pin must fail.
cp "$BUILD/$TID.sim.out" "$BUILD/tooth3.sim.out"
perturb "$BUILD/tooth3.sim.out" -E 's/^TFIN [0-8] /TFIN 9 /'
node "$M4G/wrap-target.js" "$TID" "$BUILD/tooth3.sim.out" \
  "$BUILD/tooth3.player.json" "$BUILD/tooth3.target.json" >/dev/null
if node "$M4G/verify-target-stream.js" "$BUILD/tooth3.target.json" \
    "$M4G/$tname.target.sha256.json" >/dev/null 2>&1; then
  fail "T3 TFIN-count perturb did NOT fail the finals pin"
fi
echo "    T3 TFIN targetsDestroyed perturb -> verify-target finals pin fails"

# T4 — target-count DATA perturb: flip one emitted double bit in a COPY of
# ml_targets.c, recompile the round-trip C dump -> it must diverge from the
# fresh executed-JS walk.
cp "$TABLES/ml_targets.c" "$BUILD/tooth-targets.c"
perl -0pi -e 's/UINT64_C\(0x([0-9a-fA-F]+)\)/"UINT64_C(0x".($n++?$1:sprintf("%016x",(hex($1))^1)).")"/e' "$BUILD/tooth-targets.c"
cc -std=c99 -O1 -ffp-contract=off -Wall -Wextra -Werror \
  -I"$TABLES" -o "$BUILD/tooth_targets_check" \
  pipeline/lib/targets_check.c "$BUILD/tooth-targets.c" 2>/dev/null \
  || fail "T4 tooth ml_targets.c failed to compile"
"$BUILD/tooth_targets_check" > "$BUILD/tooth-c.dump"
node pipeline/lib/targets-dump.js "$DIST" > "$BUILD/tooth-js.dump"
if cmp -s "$BUILD/tooth-c.dump" "$BUILD/tooth-js.dump"; then
  fail "T4 perturbed ml_targets.c round-trip did NOT diverge"
fi
echo "    T4 ml_targets.c double-bit perturb -> round-trip diverges"

# T5 — hq-row DROP: the target_hq_probe --drop arm (proven in [4]) is the
# consume-path drop tooth; re-assert it here as a named tooth.
if "$BUILD/target_hq_probe" --simdata "$BUILD/simdata.txt" --drop >/dev/null 2>&1; then
  fail "T5 hq-row drop did NOT fail the probe"
fi
echo "    T5 hq-row drop -> target_hq_probe fails"

# T6 — wrap-target GRAMMAR: a resembling-but-malformed F line in a COPY ->
# wrap-target must reject (no partial parse).
cp "$BUILD/$TID.sim.out" "$BUILD/tooth6.sim.out"
perturb "$BUILD/tooth6.sim.out" '5s/^F /F x/'
if node "$M4G/wrap-target.js" "$TID" "$BUILD/tooth6.sim.out" \
    "$BUILD/tooth6.player.json" "$BUILD/tooth6.target.json" >/dev/null 2>&1; then
  fail "T6 malformed F line did NOT fail wrap-target"
fi
echo "    T6 wrap-target grammar -> malformed line rejected"

# --- T7-T24 — FROZEN-SIDE teeth (review-94 L2/H1 + review-96 C-M3/
# C-M1/C-M2/C-L5/C-L6/G-M4/G-L7): perturb COPIES of the frozen/manifest/
# run files with the PRODUCTION judges pointed at the copies; committed
# bytes never edited; every perturbed aspect restored by re-copy from the
# pristine set. CONTROL first: the untouched copy set must PASS (a copy
# rig that cannot pass proves nothing about the perturbations).
FT="$BUILD/ftooth"
rm -rf "$FT"; mkdir -p "$FT"
cp "$M4G/$tname.target.sha256.json" "$M4G/$tname.sha256.json" \
   "$M4G/$ttrace" "$FT/"
FTGT="$FT/$tname.target.sha256.json"
FSIB="$FT/$tname.sha256.json"
ftooth_reset() { cp "$M4G/$tname.target.sha256.json" "$FTGT"
                 cp "$M4G/$tname.sha256.json" "$FSIB"; }
node "$M4G/verify-target-stream.js" "$BUILD/$TID.target.json" "$FTGT" \
    >/dev/null 2>&1 \
  || fail "frozen-copy CONTROL failed: the production judge rejects the UNTOUCHED copy set"
echo "    frozen-copy control -> production judge passes the untouched copies"

# T7 — FROZEN-frame nibble: flip one hex char of a mid-stream frozen frame
# hash in the COPY -> the streamSha256 seal must kill it.
python3 - "$FTGT" <<'PY'
import sys, re
p=sys.argv[1]; L=open(p).read().splitlines()
for i,l in enumerate(L):
    m=re.match(r'^\{"f":1800,"h":"([0-9a-f]{64})"\},?$', l)
    if m:
        h=list(m.group(1)); h[0]='0' if h[0]!='0' else '1'
        L[i]=l.replace(m.group(1), ''.join(h)); break
else: sys.exit("T7: frozen frame 1800 line not found")
open(p,'w').write('\n'.join(L)+'\n')
PY
if node "$M4G/verify-target-stream.js" "$BUILD/$TID.target.json" "$FTGT" \
    >/dev/null 2>&1; then
  fail "T7 frozen-frame nibble did NOT die in the production judge"
fi
ftooth_reset
echo "    T7 frozen-frame nibble -> seal kills it"

# T8 — FROZEN frame-numbering perturb: renumber frame 1800 -> 1801 in the
# COPY -> the strict f==i+1 numbering death.
python3 - "$FTGT" <<'PY'
import sys
p=sys.argv[1]; s=open(p).read()
if s.count('{"f":1800,') != 1: sys.exit("T8: frame 1800 row not unique")
open(p,'w').write(s.replace('{"f":1800,', '{"f":1801,'))
PY
if node "$M4G/verify-target-stream.js" "$BUILD/$TID.target.json" "$FTGT" \
    >/dev/null 2>&1; then
  fail "T8 frozen frame-numbering perturb did NOT die in the production judge"
fi
ftooth_reset
echo "    T8 frozen frame-numbering perturb -> numbering death"

# T9 — FROZEN seal perturb: flip one hex char of the streamSha256 field in
# the COPY -> the seal-vs-frames check must kill it.
python3 - "$FTGT" <<'PY'
import sys, re
p=sys.argv[1]; L=open(p).read().splitlines()
for i,l in enumerate(L):
    m=re.match(r'^"streamSha256": "([0-9a-f]{64})",$', l)
    if m:
        h=list(m.group(1)); h[0]='0' if h[0]!='0' else '1'
        L[i]=l.replace(m.group(1), ''.join(h)); break
else: sys.exit("T9: streamSha256 line not found")
open(p,'w').write('\n'.join(L)+'\n')
PY
if node "$M4G/verify-target-stream.js" "$BUILD/$TID.target.json" "$FTGT" \
    >/dev/null 2>&1; then
  fail "T9 frozen seal perturb did NOT die in the production judge"
fi
ftooth_reset
echo "    T9 frozen streamSha256 perturb -> seal death"

# T10 — FROZEN metadata perturb: minTargets 2 -> 3 in the COPY's params
# (the exact review-94 H2 hole: the seal does not cover it) -> the
# quality/manifest binding must kill it.
python3 - "$FTGT" <<'PY'
import sys
p=sys.argv[1]; s=open(p).read()
if s.count('"minTargets":2') != 1: sys.exit("T10: minTargets token not unique")
open(p,'w').write(s.replace('"minTargets":2', '"minTargets":3'))
PY
if node "$M4G/verify-target-stream.js" "$BUILD/$TID.target.json" "$FTGT" \
    >/dev/null 2>&1; then
  fail "T10 frozen metadata perturb (minTargets) did NOT die in the production judge"
fi
ftooth_reset
echo "    T10 frozen minTargets perturb -> metadata binding death"

# T11 — SIBLING seal perturb: flip one hex char of the PLAYER sibling
# copy's streamSha256 -> the sibling-binding seal must kill the TARGET
# judgment (a corrupted sibling can no longer ride along).
python3 - "$FSIB" <<'PY'
import sys, re
p=sys.argv[1]; L=open(p).read().splitlines()
for i,l in enumerate(L):
    m=re.match(r'^"streamSha256": "([0-9a-f]{64})",$', l)
    if m:
        h=list(m.group(1)); h[0]='0' if h[0]!='0' else '1'
        L[i]=l.replace(m.group(1), ''.join(h)); break
else: sys.exit("T11: sibling streamSha256 line not found")
open(p,'w').write('\n'.join(L)+'\n')
PY
if node "$M4G/verify-target-stream.js" "$BUILD/$TID.target.json" "$FTGT" \
    >/dev/null 2>&1; then
  fail "T11 sibling-seal perturb did NOT die in the production judge"
fi
ftooth_reset
echo "    T11 player-sibling seal perturb -> sibling binding death"

# T12 — DUPLICATE-ID manifest, TRUE INTEGRATION tooth (review-96 C-M3;
# replaces the direct validator-CLI form): a copied TREE mirrors the
# consumer geometry with the PRODUCTION consumer bytes (cmp-proven
# byte-identical), CONTROL-passes on the pristine manifest copy, then
# the dup-id manifest COPY (row 0 cloned over row 1) is put IN the tree
# and the done-check's own manifest-consuming paths — wrap-target.js,
# verify-target-stream.js, and the IDS-pull command form — must each
# die NAMING the duplicate id. No direct validator invocation counts.
IT="$BUILD/itooth-tree"
rm -rf "$IT"
mkdir -p "$IT/port/goldens-m4" "$IT/oracle/harness"
for f in validate-target-manifest.js verify-target-stream.js wrap-target.js \
         json-dup-key-scan.js; do
  cp "$M4G/$f" "$IT/port/goldens-m4/$f"
  cmp -s "$M4G/$f" "$IT/port/goldens-m4/$f" \
    || fail "T12 tree copy of $f is not byte-identical to production"
done
cp "$HARNESS/streamlib.js" "$IT/oracle/harness/streamlib.js"
cp oracle/CHECKSUM.md "$IT/oracle/CHECKSUM.md"
cp "$M4G/$tname.target.sha256.json" "$M4G/$tname.sha256.json" \
   "$M4G/$ttrace" "$M4G/manifest-target.json" "$IT/port/goldens-m4/"
node "$IT/port/goldens-m4/verify-target-stream.js" "$BUILD/$TID.target.json" \
    "$IT/port/goldens-m4/$tname.target.sha256.json" >/dev/null 2>&1 \
  || fail "T12 CONTROL failed: the tree production judge rejects the untouched copy set"
node -e '
  const fs=require("fs");
  const m=JSON.parse(fs.readFileSync("port/goldens-m4/manifest-target.json","utf8"));
  if (m.goldens.length < 2) { console.error("T12 needs >= 2 goldens"); process.exit(1); }
  m.goldens[1]=JSON.parse(JSON.stringify(m.goldens[0]));
  fs.writeFileSync(process.argv[1], JSON.stringify(m));
' "$IT/port/goldens-m4/manifest-target.json"
if T12A="$(node "$IT/port/goldens-m4/wrap-target.js" "$TID" "$BUILD/$TID.sim.out" \
    "$IT/t12.player.json" "$IT/t12.target.json" 2>&1)"; then
  fail "T12 wrap-target consumer did NOT die on the duplicate-id manifest"
fi
grep -q "duplicate golden id" <<< "$T12A" \
  || fail "T12 wrap-target died but did not name the duplicate id (got: $T12A)"
if T12B="$(node "$IT/port/goldens-m4/verify-target-stream.js" "$BUILD/$TID.target.json" \
    "$IT/port/goldens-m4/$tname.target.sha256.json" 2>&1)"; then
  fail "T12 verify-target-stream consumer did NOT die on the duplicate-id manifest"
fi
grep -q "duplicate golden id" <<< "$T12B" \
  || fail "T12 verify-target died but did not name the duplicate id (got: $T12B)"
if T12C="$(node -e '
  const path=require("path");
  const m=require(path.resolve(process.argv[1])).loadValidatedManifest();
  process.stdout.write(m.goldens.map(g=>g.id).join(" "));
' "$IT/port/goldens-m4/validate-target-manifest.js" 2>&1)"; then
  fail "T12 IDS-pull consumer form did NOT die on the duplicate-id manifest"
fi
grep -q "duplicate golden id" <<< "$T12C" \
  || fail "T12 IDS pull died but did not name the duplicate id (got: $T12C)"
rm -rf "$IT"
echo "    T12 duplicate-id manifest -> production consumers (wrap/verify/IDS pull) die naming the dup (tree control passed)"

# T13 — DUP-KEY manifest COPY, the LIVE-CONFIRMED whitespace spelling
# (review-96 C-M1): insert '"id" : "t02"' (legal whitespace before the
# colon) as a duplicate key inside the t02 row of a COPY of the RAW
# committed bytes -> the shared scanner must die naming the key (the
# refuted byte-literal token count was blind to this exact probe).
python3 - "$M4G/manifest-target.json" "$FT/manifest-dupkey.json" <<'PY'
import sys
s=open(sys.argv[1]).read()
needle='"id": "t02",'
if s.count(needle) != 1: sys.exit("T13: t02 id line not unique")
open(sys.argv[2],'w').write(s.replace(needle, needle+'\n      "id" : "t02",'))
PY
if T13_OUT="$(node "$M4G/validate-target-manifest.js" "$FT/manifest-dupkey.json" 2>&1)"; then
  fail "T13 whitespace-colon duplicate-key manifest COPY did NOT fail the validator"
fi
grep -q 'duplicate JSON key "id"' <<< "$T13_OUT" \
  || fail "T13 validator died but did not name the duplicate key (got: $T13_OUT)"
echo "    T13 dup-key probe ('\"id\" : \"t02\"') -> scanner dies naming the key"

# T14 — TOP-LEVEL dup-key manifest COPY (review-96 C-M1): a duplicate
# "comment" key at the TOP scope (wholly unguarded before iter 98).
python3 - "$M4G/manifest-target.json" "$FT/manifest-topdup.json" <<'PY'
import sys
s=open(sys.argv[1]).read()
needle='"goldens": ['
if s.count(needle) != 1: sys.exit("T14: goldens open token not unique")
open(sys.argv[2],'w').write(s.replace(needle, '"comment": "dup",\n  '+needle))
PY
if T14_OUT="$(node "$M4G/validate-target-manifest.js" "$FT/manifest-topdup.json" 2>&1)"; then
  fail "T14 top-level duplicate-key manifest COPY did NOT fail the validator"
fi
grep -q 'duplicate JSON key "comment"' <<< "$T14_OUT" \
  || fail "T14 validator died but did not name the duplicate key (got: $T14_OUT)"
echo "    T14 top-level dup-key manifest copy -> scanner dies naming the key"

# T15 — FROZEN-row dup-key COPY (review-96 C-M2, the codex probe
# {"f":999,"f":1800,...}): the parsed value is last-wins so the seal
# still verifies — ONLY the raw scanner can kill it.
python3 - "$FTGT" <<'PY'
import sys
p=sys.argv[1]; s=open(p).read()
if s.count('{"f":1800,') != 1: sys.exit("T15: frame 1800 row not unique")
open(p,'w').write(s.replace('{"f":1800,', '{"f":999,"f":1800,'))
PY
if T15_OUT="$(node "$M4G/verify-target-stream.js" "$BUILD/$TID.target.json" "$FTGT" 2>&1)"; then
  fail "T15 frozen-row dup-key COPY did NOT die in the production judge"
fi
grep -q 'duplicate JSON key "f"' <<< "$T15_OUT" \
  || fail "T15 judge died but not at the dup-key scanner (got: $T15_OUT)"
ftooth_reset
echo "    T15 frozen-row dup-key ({\"f\":999,\"f\":1800,...}) -> scanner death"

# T16 — SIBLING extra-key COPY (review-96 C-M2): an unknown top-level
# key on the player sibling must die at the sibling exact schema.
python3 - "$FSIB" "$tname" <<'PY'
import sys
p=sys.argv[1]; s=open(p).read()
needle='"golden": "%s",' % sys.argv[2]
if s.count(needle) != 1: sys.exit("T16: sibling golden line not unique")
open(p,'w').write(s.replace(needle, needle+'\n"extra": 1,'))
PY
if T16_OUT="$(node "$M4G/verify-target-stream.js" "$BUILD/$TID.target.json" "$FTGT" 2>&1)"; then
  fail "T16 sibling extra-key COPY did NOT die in the production judge"
fi
grep -q "sibling top level" <<< "$T16_OUT" \
  || fail "T16 judge died but not at the sibling exact schema (got: $T16_OUT)"
ftooth_reset
echo "    T16 sibling extra key -> sibling exact-schema death"

# T17 — FROZEN manifest-bound field BEYOND minTargets (review-96 C-L6):
# params.tstage perturb in the COPY -> the manifest binding must kill it.
python3 - "$FTGT" "$ttstage" <<'PY'
import sys
p=sys.argv[1]; t=int(sys.argv[2]); s=open(p).read()
needle='"tstage":%d' % t
if s.count(needle) != 1: sys.exit("T17: tstage token not unique")
open(p,'w').write(s.replace(needle, '"tstage":%d' % ((t+1)%10)))
PY
if T17_OUT="$(node "$M4G/verify-target-stream.js" "$BUILD/$TID.target.json" "$FTGT" 2>&1)"; then
  fail "T17 frozen tstage perturb did NOT die in the production judge"
fi
grep -q "metadata binding" <<< "$T17_OUT" \
  || fail "T17 judge died but not at the metadata binding (got: $T17_OUT)"
ftooth_reset
echo "    T17 frozen tstage perturb -> manifest metadata-binding death"

# T18 — playerStream NAME binding (review-96 C-L6): repoint the COPY's
# playerStream at the other golden's sibling -> name-derivation death.
python3 - "$FTGT" "$tname" <<'PY'
import sys
p=sys.argv[1]; s=open(p).read()
needle='"playerStream": "%s.sha256.json",' % sys.argv[2]
if s.count(needle) != 1: sys.exit("T18: playerStream line not unique")
open(p,'w').write(s.replace(needle,
    '"playerStream": "t99-not-the-sibling.sha256.json",'))
PY
if T18_OUT="$(node "$M4G/verify-target-stream.js" "$BUILD/$TID.target.json" "$FTGT" 2>&1)"; then
  fail "T18 playerStream repoint did NOT die in the production judge"
fi
grep -q "sibling binding" <<< "$T18_OUT" \
  || fail "T18 judge died but not at the playerStream name binding (got: $T18_OUT)"
ftooth_reset
echo "    T18 playerStream repoint -> sibling name-binding death"

# T19 — SIBLING cross-pin (review-96 C-L6): perturb the sibling COPY's
# rngCalls (outside its seal) -> the run-vs-sibling equality must die.
python3 - "$FSIB" <<'PY'
import sys, re
p=sys.argv[1]; s=open(p).read()
m=re.findall(r'^"rngCalls": (\d+),$', s, re.M)
if len(m) != 1: sys.exit("T19: rngCalls line not unique")
open(p,'w').write(re.sub(r'^"rngCalls": \d+,$',
    '"rngCalls": %d,' % (int(m[0])+1), s, flags=re.M))
PY
if T19_OUT="$(node "$M4G/verify-target-stream.js" "$BUILD/$TID.target.json" "$FTGT" 2>&1)"; then
  fail "T19 sibling rngCalls perturb did NOT die in the production judge"
fi
grep -q "frozen player sibling" <<< "$T19_OUT" \
  || fail "T19 judge died but not at the rngCalls cross-pin (got: $T19_OUT)"
ftooth_reset
echo "    T19 sibling rngCalls perturb -> run-vs-sibling cross-pin death"

# T20 — QUALITY comparison (review-96 C-L6): frozen COPY's
# finalTargetsDestroyed -> minTargets-1 (below the floor) -> quality death.
python3 - "$FTGT" "$tmin" <<'PY'
import sys, re
p=sys.argv[1]; tmin=int(sys.argv[2]); s=open(p).read()
m=re.findall(r'"finalTargetsDestroyed":(\d+)', s)
if len(m) != 1: sys.exit("T20: finalTargetsDestroyed token not unique")
open(p,'w').write(s.replace('"finalTargetsDestroyed":'+m[0],
    '"finalTargetsDestroyed":%d' % (tmin-1)))
PY
if T20_OUT="$(node "$M4G/verify-target-stream.js" "$BUILD/$TID.target.json" "$FTGT" 2>&1)"; then
  fail "T20 finalTargetsDestroyed quality perturb did NOT die in the production judge"
fi
grep -q "mechanical quality" <<< "$T20_OUT" \
  || fail "T20 judge died but not at the quality comparison (got: $T20_OUT)"
ftooth_reset
echo "    T20 finalTargetsDestroyed -> minTargets-1 -> mechanical-quality death"

# T21 — FINALS binding (review-96 G-L7): frozen COPY's
# finalTargetsDestroyed +1 (quality still passes, domain still legal) ->
# the run-finals equality must kill it (the assertion form of the
# finals binding; the seal does not cover the finals).
python3 - "$FTGT" <<'PY'
import sys, re
p=sys.argv[1]; s=open(p).read()
m=re.findall(r'"finalTargetsDestroyed":(\d+)', s)
if len(m) != 1: sys.exit("T21: finalTargetsDestroyed token not unique")
n=int(m[0])+1
if n > 20: sys.exit("T21: +1 leaves the 0..20 domain — pick another perturb")
open(p,'w').write(s.replace('"finalTargetsDestroyed":'+m[0],
    '"finalTargetsDestroyed":%d' % n))
PY
if T21_OUT="$(node "$M4G/verify-target-stream.js" "$BUILD/$TID.target.json" "$FTGT" 2>&1)"; then
  fail "T21 finalTargetsDestroyed finals perturb did NOT die in the production judge"
fi
grep -q "final targetsDestroyed" <<< "$T21_OUT" \
  || fail "T21 judge died but not at the run-finals binding (got: $T21_OUT)"
ftooth_reset
echo "    T21 finalTargetsDestroyed +1 -> run-finals binding death"

# T22 — wrap-target NUMERIC DOMAIN (review-96 C-L5): an unsafe integer
# the producer cannot emit ('RNG 9007199254740993 1', Number() rounds
# it) in a sim.out COPY -> wrap-target must die, never wrap.
cp "$BUILD/$TID.sim.out" "$BUILD/tooth22.sim.out"
perturb "$BUILD/tooth22.sim.out" -E 's/^RNG [0-9]+ /RNG 9007199254740993 /'
if T22_OUT="$(node "$M4G/wrap-target.js" "$TID" "$BUILD/tooth22.sim.out" \
    "$BUILD/tooth22.player.json" "$BUILD/tooth22.target.json" 2>&1)"; then
  fail "T22 unsafe-integer RNG token did NOT fail wrap-target"
fi
grep -q "producer domain" <<< "$T22_OUT" \
  || fail "T22 wrap died but not at the numeric-domain check (got: $T22_OUT)"
echo "    T22 RNG 9007199254740993 -> wrap-target numeric-domain death"

# T23 — run-meta fdlibm pin (review-96 G-M4): a run COPY claiming
# fdlibm:false must die in the production judge.
node -e '
  const fs=require("fs");
  const r=JSON.parse(fs.readFileSync(process.argv[1],"utf8"));
  r.meta.fdlibm=false;
  fs.writeFileSync(process.argv[2], JSON.stringify(r));
' "$BUILD/$TID.target.json" "$BUILD/tooth23.target.json"
if T23_OUT="$(node "$M4G/verify-target-stream.js" "$BUILD/tooth23.target.json" \
    "$M4G/$tname.target.sha256.json" 2>&1)"; then
  fail "T23 run fdlibm:false did NOT die in the production judge"
fi
grep -q "meta.fdlibm" <<< "$T23_OUT" \
  || fail "T23 judge died but not at the fdlibm pin (got: $T23_OUT)"
echo "    T23 run fdlibm:false -> run-identity pin death"

# T24 — run-meta ABSENT field (review-96 G-M4): DELETE meta.p2 in a run
# COPY (undefined) -> the explicit-null expectation must die (an
# undefined-equals-undefined judge would pass this).
node -e '
  const fs=require("fs");
  const r=JSON.parse(fs.readFileSync(process.argv[1],"utf8"));
  delete r.meta.p2;
  fs.writeFileSync(process.argv[2], JSON.stringify(r));
' "$BUILD/$TID.target.json" "$BUILD/tooth24.target.json"
if T24_OUT="$(node "$M4G/verify-target-stream.js" "$BUILD/tooth24.target.json" \
    "$M4G/$tname.target.sha256.json" 2>&1)"; then
  fail "T24 absent meta.p2 did NOT die in the production judge"
fi
grep -q "meta.p2" <<< "$T24_OUT" \
  || fail "T24 judge died but not at the p2 explicit-null pin (got: $T24_OUT)"
echo "    T24 absent meta.p2 -> explicit-null pin death"

rm -rf "$FT" "$IT"
rm -f "$BUILD"/tooth*.sim.out "$BUILD"/tooth*.player.json "$BUILD"/tooth*.target.json \
  "$BUILD"/tooth*.bak "$BUILD/tooth-targets.c" "$BUILD/tooth_targets_check" \
  "$BUILD/tooth-c.dump" "$BUILD/tooth-js.dump" "$BUILD"/*.regen.trace.json

# --- [6] no-commit guard ------------------------------------------------------
echo "[6] no-commit guard"
git check-ignore -q "$BUILD" \
  || fail "$BUILD is not gitignored (build output must never be tracked)"
TRACKED="$(git status --porcelain -- "$BUILD" "$TABLES")"
[ -z "$TRACKED" ] \
  || { echo "$TRACKED"; fail "files under the build dir are tracked/staged"; }
echo "    build output gitignored, nothing tracked"

echo "TARGET SIM CONFORMS ($(echo $IDS | wc -w | tr -d ' ') goldens: $IDS; leaves=718 probes=2 teeth=24)"
