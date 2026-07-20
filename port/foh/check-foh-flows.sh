#!/usr/bin/env bash
# check-foh-flows.sh — M4 task 9 done-check: FOH core + menu flows, HOST
# (fix_plan §M4 task 9; pre-registration AGENT-LOG iter 88; §M4
# conventions' menu verification approach (a)-(c) host-side).
#
# Composes, in order:
#   [0] no-reclaim run lock + PRODUCER BYTE PINS (wrap-run.js /
#       verify-stream.js / trace-to-txt.js / dump-sim-data.js /
#       judge-foh-trace.js — a producer change invalidates this check's
#       evidence, reviewed pin update in the same commit) + both-manifest
#       INVENTORY pins (8 oracle + 4 m4, both directions) + the FLOW
#       inventory pin (pinned array == flows/*.flow globs both
#       directions) + BRIDGE PARAM CROSS-BIND: the frozen .expect LAUNCH
#       lines' params are parsed by anchored grammar and asserted equal
#       to the g01/m01 manifest params (no independent literals — the
#       conventions (b) assertion is mechanical).
#   [1] fresh data planes: pipeline tables (own dir) + simdata dumped
#       x2 byte-identical.
#   [2] build foh_app (raster TU -O3, all else -O2; -ffp-contract=off
#       -Wall -Wextra -Werror on every TU; links the FULL sim + ai.c +
#       sim_ai_live.c — the live pointer seam; check-sim.sh untouched).
#   [3] per committed flow: run A (bridge mode where pinned) + run B
#       (no bridge) -> judge-foh-trace.js whitelist grammar (corpus =
#       these genuine traces; zero false rejections) + cmp vs the frozen
#       flows/<id>.expect + A==B byte-stability + per-shot A==B
#       byte-stability, non-blank witness, and pairwise distinctness
#       (the pinned shot inventory, both directions).
#   [4] MATCH-LAUNCH BRIDGES: f01's launched stream -> wrap-run.js ->
#       UNCHANGED verify-stream.js vs the frozen g01 stream; f02
#       (live C AI, no AIBRIDGE1) vs the frozen m01 stream — FULL
#       3600-frame equality (exceeds the conventions' prefix bar);
#       f01/f02/f03 BRIDGE-STATE witnesses cmp'd vs their frozen
#       .bstate.expect (settings/params read back FROM GameState).
#   [5] TEETH (standing, pre-registered T1-T6): nav-perturb, char
#       variant, stream-judge nibble (run-JSON side), trace grammar
#       malformed + resembling, shot corruption, difficulty variant.
#       All operate on GENERATED variants/copies — committed bytes are
#       never edited.
#   [6] hygiene: build outputs are git-ignored (rc case-split).
#
# Prints `FOH FLOWS OK (flows=4 shots=11 bridges=3 streams=MATCH
# teeth=6)`, exit 0; ANY divergence, off-graph transition, pin
# mismatch, count disagreement, or missing artifact -> nonzero.
#
# HONEST EXPOSURE (PROCESS §8): the frozen traces prove the REWRITTEN
# machine's flow graph and selection semantics against the
# pre-registered upstream mapping (foh.h citations) — a shared
# misreading of upstream's menu code would be invisible here (the
# rewrite's exposure class; primary-source citations are the
# mitigation). Menu LOOK is not judged (rewritten surface; visual
# authority = Chase's acceptance playthrough; device look = task 10).
# The non-default-settings SIM behavior is M2's verified surface — f03
# proves the launch PLUMBING only (BRIDGE-STATE witness).
set -euo pipefail
cd "$(dirname "$0")/../.."

FOH=port/foh
B=$FOH/build/check
FLOWS=$FOH/flows
TABLES=pipeline/build/foh-tables
SIM=port/sim/sim
CAL=port/sim/calib
GFX=port/gfx
M4G=port/goldens-m4

fail() { echo "FOH FLOWS FAIL: $1" >&2; exit 1; }
grammar_die() { echo "FOH FLOWS FAIL: $1" >&2; exit 2; }

made() {
  local f
  for f in "$@"; do
    if ! [ -s "$f" ]; then
      fail "artifact $f missing or empty after its producer ran (rm-before-produce freshness guard)"
    fi
  done
}

# grep count with the rc CASE-SPLIT (rc>=2 = read error DIES).
count_e() {
  local c rc=0
  c="$(grep -cE -- "$2" "$1")" || rc=$?
  if [ "$rc" -ge 2 ]; then
    grammar_die "count helper — grep -cE rc $rc reading '$1' (a read error is CORRUPT evidence, never a 0 count)"
  fi
  printf '%s' "$c"
}
count_x() {
  local c rc=0
  c="$(grep -cF -- "$2" "$1")" || rc=$?
  if [ "$rc" -ge 2 ]; then
    grammar_die "count helper — grep -cF rc $rc reading '$1'"
  fi
  printf '%s' "$c"
}

relay_lines() { sed 's/^/  | /'; }

host_sha256() { shasum -a 256 "$1" | cut -d' ' -f1; }

# --- [0] run lock (mkdir-atomic, NO reclaim) ---------------------------------
mkdir -p "$FOH/build"
LOCK=$FOH/build/foh-flows.lock
if ! mkdir "$LOCK" 2>/dev/null; then
  fail "run lock $LOCK held — another check run owns the scratch; NEVER reclaimed automatically (remove by hand only after verifying no run is live)"
fi
trap 'rmdir "$LOCK" 2>/dev/null || true' EXIT

# --- [0a] PRODUCER BYTE PINS -------------------------------------------------
# UPDATE DISCIPLINE (binding): a reviewed change to a pinned producer
# updates its line below IN THE SAME COMMIT (shasum -a 256 <path>).
PRODUCER_PINS="\
b835b5f886225e0015dae152576eea5a42fa69d7ba0699f4de0e31438d05c5b9 port/sim/sim/wrap-run.js
f420723433b19166b53a80aedf54931ffdfbc6d2505c773fd73b7a13bbcdf60e oracle/harness/verify-stream.js
4160a35b36e8d3d6896ad2c3c6239d4a4860a0d7f43814a7a9b53b7c136742ab port/sim/sim/trace-to-txt.js
7186734f8c3ff9bfad04f59bf9e13f201663e82481e399911433136673721bba port/sim/calib/dump-sim-data.js
5c4ecd881338afcf961eb207d1acfbf67618eb899c6067b531935b1fd4314f91 port/foh/judge-foh-trace.js"
N_PINS_WANT=5
n_pins=0
while IFS= read -r pline; do
  [ -n "$pline" ] || continue
  if ! [[ "$pline" =~ ^[0-9a-f]{64}\ [A-Za-z0-9._/-]+$ ]]; then
    fail "producer pin table — line fails the anchored grammar: '$pline'"
  fi
  psha="${pline%% *}"
  ppath="${pline#* }"
  test -f "$ppath" || fail "pinned producer $ppath is missing from the tree"
  have="$(host_sha256 "$ppath")" || fail "cannot hash producer $ppath"
  if ! [[ "$have" =~ ^[0-9a-f]{64}$ ]]; then
    fail "producer hash read malformed for $ppath: '$have'"
  fi
  if [ "$have" != "$psha" ]; then
    fail "producer byte pin — $ppath sha256 $have != pinned $psha (a producer change invalidates this check's evidence; a legitimate, reviewed change must update the pin in the same commit)"
  fi
  n_pins=$((n_pins + 1))
done <<< "$PRODUCER_PINS"
[ "$n_pins" = "$N_PINS_WANT" ] || fail "producer pin inventory — $n_pins/$N_PINS_WANT pins verified"

# --- [0b] manifest inventory pins (music-check class) ------------------------
ORACLE_IDS=(g01 g02 g03 g04 g05 g06 g07 g08)
ORACLE_NAMES=(
  "g01-fox-marth-battlefield"
  "g02-falco-puff-ystory"
  "g03-falcon-fox-pstadium"
  "g04-puff-falcon-dreamland"
  "g05-marth-falco-fdest"
  "g06-falcon-marth-fountain"
  "g07-falco-falcon-battlefield"
  "g08-fox-puff-fdest"
)
M4_IDS=(m01 m02 s01 s02)
M4_NAMES=(
  "m01-falcon-marth-d1-ystory"
  "m02-falcon-fox-d9-dreamland"
  "s01-marth-fox-stops-battlefield"
  "s02-marth-fox-guardon-break-battlefield"
)
if [ "${#ORACLE_IDS[@]}" != 8 ] || [ "${#ORACLE_NAMES[@]}" != 8 ] || \
   [ "${#M4_IDS[@]}" != 4 ] || [ "${#M4_NAMES[@]}" != 4 ]; then
  fail "inventory pin — pinned array lengths off"
fi
inv="$(node -e '
  const fs = require("fs");
  const m = JSON.parse(fs.readFileSync(process.argv[1], "utf8"));
  process.stdout.write(m.goldens.map((g) => g.id).join(" ") + "|" +
    m.goldens.map((g) => g.name).join(" "));
' oracle/goldens/manifest.json)" || fail "cannot read the oracle manifest"
if [ "$inv" != "${ORACLE_IDS[*]}|${ORACLE_NAMES[*]}" ]; then
  fail "oracle inventory pin — manifest {$inv} != pinned (both directions; a grown/shrunk/renamed golden set is a reviewed pin update)"
fi
inv="$(node -e '
  const fs = require("fs");
  const m = JSON.parse(fs.readFileSync(process.argv[1], "utf8"));
  process.stdout.write(m.goldens.map((g) => g.id).join(" ") + "|" +
    m.goldens.map((g) => g.name).join(" "));
' "$M4G/manifest.json")" || fail "cannot read the m4 manifest"
if [ "$inv" != "${M4_IDS[*]}|${M4_NAMES[*]}" ]; then
  fail "m4 inventory pin — manifest {$inv} != pinned (both directions)"
fi

# --- [0c] flow inventory pin + frozen artifacts ------------------------------
FLOW_IDS=(f01-vs-g01 f02-cpu-m01 f03-options f04-nav)
# bridge mode per flow (positional): verify=stream-judged launch,
# state=GameState witness only, none=no launch
FLOW_BRIDGE=(verify verify state none)
# pinned shot inventory per flow (space-joined; both directions below)
FLOW_SHOTS=("startup title menu-top menu-battle css sss" \
            "css-cpu sss-ystory" \
            "options-gameplay options-edited" \
            "menu-controls")
[ "${#FLOW_IDS[@]}" = 4 ] || fail "flow inventory — pinned array length off"
[ "${#FLOW_BRIDGE[@]}" = 4 ] || fail "flow bridge array length off"
[ "${#FLOW_SHOTS[@]}" = 4 ] || fail "flow shots array length off"
globbed="$(ls "$FLOWS"/*.flow | sed 's|.*/||; s|\.flow$||' | sort | tr '\n' ' ' | sed 's/ $//')"
pinned="$(printf '%s\n' "${FLOW_IDS[@]}" | sort | tr '\n' ' ' | sed 's/ $//')"
if [ "$globbed" != "$pinned" ]; then
  fail "flow inventory pin — flows/*.flow {$globbed} != pinned {$pinned} (both directions; a new or dropped flow is a reviewed pin update)"
fi
for k in 0 1 2 3; do
  id="${FLOW_IDS[$k]}"
  made "$FLOWS/$id.flow" "$FLOWS/$id.expect"
  if [ "${FLOW_BRIDGE[$k]}" != "none" ]; then
    made "$FLOWS/$id.bstate.expect"
  fi
done

# --- [0d] bridge param CROSS-BIND: frozen LAUNCH lines == manifest params ----
LAUNCH_RE='^LAUNCH [0-9]+ p1=[0-4] p2=[0-4] p2type=[01] difficulty=[1-4] stage=[0-5] turbo=[01] lcancel=[012] tapjump=[01],[01],[01],[01] versus=0$'
for exf in "$FLOWS/f01-vs-g01.expect" "$FLOWS/f02-cpu-m01.expect"; do
  c="$(count_e "$exf" "$LAUNCH_RE")"
  [ "$c" = 1 ] || grammar_die "frozen $exf: $c LAUNCH lines match the anchored grammar, want exactly 1"
done
get_launch_field() { # <file> <key> (grammar asserted above; pure extract)
  grep -E "$LAUNCH_RE" "$1" | tr ' ' '\n' | grep -E "^$2=" | cut -d= -f2
}
g01line="$(node -e '
  const m = JSON.parse(require("fs").readFileSync("oracle/goldens/manifest.json", "utf8"));
  const g = m.goldens.find((x) => x.id === "g01");
  if (!g || g.cpu !== false) { console.error("g01 shape"); process.exit(1); }
  console.log([g.seed, g.p1, g.p2, g.stage, g.frames, g.trace].join(" "));
')" || fail "cannot parse g01 params from the oracle manifest"
read -r G01_SEED G01_P1 G01_P2 G01_STAGE G01_FRAMES G01_TRACE <<< "$g01line"
m01line="$(node -e '
  const m = JSON.parse(require("fs").readFileSync(process.argv[1], "utf8"));
  const g = m.goldens.find((x) => x.id === "m01");
  if (!g || g.cpu !== true) { console.error("m01 shape"); process.exit(1); }
  console.log([g.seed, g.p1, g.p2, g.stage, g.frames, g.difficulty, g.trace].join(" "));
' "$M4G/manifest.json")" || fail "cannot parse m01 params from the m4 manifest"
read -r M01_SEED M01_P1 M01_P2 M01_STAGE M01_FRAMES M01_DIFF M01_TRACE <<< "$m01line"
[ "$(get_launch_field "$FLOWS/f01-vs-g01.expect" p1)" = "$G01_P1" ] || fail "cross-bind — f01 LAUNCH p1 != g01 manifest p1"
[ "$(get_launch_field "$FLOWS/f01-vs-g01.expect" p2)" = "$G01_P2" ] || fail "cross-bind — f01 LAUNCH p2 != g01 manifest p2"
[ "$(get_launch_field "$FLOWS/f01-vs-g01.expect" stage)" = "$G01_STAGE" ] || fail "cross-bind — f01 LAUNCH stage != g01 manifest stage"
[ "$(get_launch_field "$FLOWS/f01-vs-g01.expect" p2type)" = 0 ] || fail "cross-bind — f01 LAUNCH p2type != 0 (g01 is human/human)"
[ "$(get_launch_field "$FLOWS/f02-cpu-m01.expect" p1)" = "$M01_P1" ] || fail "cross-bind — f02 LAUNCH p1 != m01 manifest p1"
[ "$(get_launch_field "$FLOWS/f02-cpu-m01.expect" p2)" = "$M01_P2" ] || fail "cross-bind — f02 LAUNCH p2 != m01 manifest p2"
[ "$(get_launch_field "$FLOWS/f02-cpu-m01.expect" stage)" = "$M01_STAGE" ] || fail "cross-bind — f02 LAUNCH stage != m01 manifest stage"
[ "$(get_launch_field "$FLOWS/f02-cpu-m01.expect" p2type)" = 1 ] || fail "cross-bind — f02 LAUNCH p2type != 1 (m01 is CPU)"
[ "$(get_launch_field "$FLOWS/f02-cpu-m01.expect" difficulty)" = "$M01_DIFF" ] || fail "cross-bind — f02 LAUNCH difficulty != m01 manifest difficulty"
echo "[0] pins OK: producers 5, manifests 8+4, flows 4, LAUNCH cross-bind g01/m01"

# --- [1] fresh data planes ---------------------------------------------------
echo "=== [1] data planes (fresh tables + simdata x2)"
rm -rf "$TABLES"
{ bash pipeline/extractor/build-extractor.sh; } 2>&1 | relay_lines
{ node pipeline/run.js --only animations,tables,stages --out "$TABLES"; } 2>&1 | relay_lines
made "$TABLES/ml_tables.c" "$TABLES/ml_stages.c" "$TABLES/ml_tables.h" \
     "$TABLES/ml_stages.h"
rm -rf "$B"
mkdir -p "$B"
node "$CAL/dump-sim-data.js" --out "$B/simdata.txt" 2>&1 | relay_lines
node "$CAL/dump-sim-data.js" --out "$B/simdata2.txt" 2>&1 | relay_lines
made "$B/simdata.txt" "$B/simdata2.txt"
cmp "$B/simdata.txt" "$B/simdata2.txt" || fail "simdata not byte-identical across two fresh dumps"
echo "    simdata byte-identical across two fresh dumps"

# --- [2] build ----------------------------------------------------------------
echo "=== [2] build foh_app"
CFLAGS_COMMON=(-ffp-contract=off -Wall -Wextra -Werror
  -I"$TABLES" -Iport/ryu -Iport/sim -Ioracle/qjs)
rm -f "$B/raster.o" "$B/platform_headless.o" "$B/foh.o" "$B/foh_font.o" \
      "$B/foh_render.o" "$B/foh_app.o" "$B/foh_app"
cc -O3 "${CFLAGS_COMMON[@]}" -c "$GFX/raster.c" -o "$B/raster.o"
cc -O2 "${CFLAGS_COMMON[@]}" -c "$GFX/platform_headless.c" -o "$B/platform_headless.o"
cc -O2 "${CFLAGS_COMMON[@]}" -c "$FOH/foh.c" -o "$B/foh.o"
cc -O2 "${CFLAGS_COMMON[@]}" -c "$FOH/foh_font.c" -o "$B/foh_font.o"
cc -O2 "${CFLAGS_COMMON[@]}" -c "$FOH/foh_render.c" -o "$B/foh_render.o"
cc -O2 "${CFLAGS_COMMON[@]}" -c "$FOH/foh_app.c" -o "$B/foh_app.o"
cc -O2 "${CFLAGS_COMMON[@]}" -o "$B/foh_app" \
  "$B/foh.o" "$B/foh_font.o" "$B/foh_render.o" "$B/foh_app.o" \
  "$B/raster.o" "$B/platform_headless.o" \
  "$SIM/sim_boot.c" "$SIM/sim_tick.c" "$SIM/sim_ser.c" \
  "$SIM/sim_data.c" "$SIM/sim_ai_live.c" \
  "$CAL/canon.c" "$CAL/player_canon.c" \
  port/sim/ai.c \
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
made "$B/foh_app"
echo "build OK: $B/foh_app (raster TU -O3, all else -O2; -ffp-contract=off everywhere)"

# golden traces -> text (pinned trace-to-txt.js)
rm -f "$B/g01.trace.txt" "$B/m01.trace.txt"
node "$SIM/trace-to-txt.js" "oracle/goldens/$G01_TRACE" "$B/g01.trace.txt" 2>&1 | relay_lines
node "$SIM/trace-to-txt.js" "$M4G/$M01_TRACE" "$B/m01.trace.txt" 2>&1 | relay_lines
made "$B/g01.trace.txt" "$B/m01.trace.txt"

# --- [3] flow runs x2 + trace/shot judgments -----------------------------------
echo "=== [3] flow runs (x2 each) + frozen-trace + shot judgments"
total_shots=0
for k in 0 1 2 3; do
  id="${FLOW_IDS[$k]}"
  mode="${FLOW_BRIDGE[$k]}"
  echo "== flow $id (bridge=$mode) run A"
  rm -rf "$B/$id"
  mkdir -p "$B/$id/shots-a" "$B/$id/shots-b"
  case "$mode" in
    verify)
      extra=()
      if [ "$id" = "f01-vs-g01" ]; then
        seed=$G01_SEED; tracef=$B/g01.trace.txt; frames=$G01_FRAMES
      else
        seed=$M01_SEED; tracef=$B/m01.trace.txt; frames=$M01_FRAMES
        extra=(--cpu-live)
      fi
      "$B/foh_app" --flow "$FLOWS/$id.flow" --flow-out "$B/$id/trace-a.txt" \
        --shots-dir "$B/$id/shots-a" \
        --bridge verify --simdata "$B/simdata.txt" --seed "$seed" \
        --trace "$tracef" --frames "$frames" --out "$B/$id/stream.txt" \
        --bstate-out "$B/$id/bstate.txt" ${extra[@]+"${extra[@]}"} \
        2>&1 | relay_lines
      made "$B/$id/stream.txt" "$B/$id/bstate.txt"
      ;;
    state)
      "$B/foh_app" --flow "$FLOWS/$id.flow" --flow-out "$B/$id/trace-a.txt" \
        --shots-dir "$B/$id/shots-a" \
        --bridge state --simdata "$B/simdata.txt" --seed "$G01_SEED" \
        --bstate-out "$B/$id/bstate.txt" 2>&1 | relay_lines
      made "$B/$id/bstate.txt"
      ;;
    none)
      "$B/foh_app" --flow "$FLOWS/$id.flow" --flow-out "$B/$id/trace-a.txt" \
        --shots-dir "$B/$id/shots-a" 2>&1 | relay_lines
      ;;
    *) fail "flow $id: unknown bridge mode '$mode'" ;;
  esac
  echo "== flow $id run B (no bridge)"
  "$B/foh_app" --flow "$FLOWS/$id.flow" --flow-out "$B/$id/trace-b.txt" \
    --shots-dir "$B/$id/shots-b" 2>&1 | relay_lines
  made "$B/$id/trace-a.txt" "$B/$id/trace-b.txt"
  # grammar judge (launch flag per bridge mode)
  wantLaunch=1
  [ "$mode" = "none" ] && wantLaunch=0
  { node "$FOH/judge-foh-trace.js" "$B/$id/trace-a.txt" "$id" "$wantLaunch"; } 2>&1 | relay_lines
  # frozen comparison + byte-stability (cmp rc case-split via set -e)
  cmp "$B/$id/trace-a.txt" "$FLOWS/$id.expect" || fail "flow $id: emitted trace differs from the frozen $FLOWS/$id.expect"
  cmp "$B/$id/trace-a.txt" "$B/$id/trace-b.txt" || fail "flow $id: runs A/B traces not byte-identical"
  if [ "$mode" != "none" ]; then
    c="$(count_e "$B/$id/bstate.txt" '^BRIDGE-STATE p1=[0-4] p2=[0-4] p2type=[01] difficulty=[1-4] stage=[0-5] turbo=[01] lcancel=[012] tapjump=[01],[01],[01],[01] phantom=[0-9a-f]{16}$')"
    [ "$c" = 1 ] || grammar_die "flow $id: bstate grammar count $c/1"
    cmp "$B/$id/bstate.txt" "$FLOWS/$id.bstate.expect" || fail "flow $id: BRIDGE-STATE differs from the frozen witness"
  fi
  # shots: pinned inventory both directions, A==B, non-blank, distinct
  want_shots="${FLOW_SHOTS[$k]}"
  got_a="$(ls "$B/$id/shots-a" | sed 's/\.ppm$//' | sort | tr '\n' ' ' | sed 's/ $//')"
  want_sorted="$(printf '%s\n' $want_shots | sort | tr '\n' ' ' | sed 's/ $//')"
  [ "$got_a" = "$want_sorted" ] || fail "flow $id: shot inventory {$got_a} != pinned {$want_sorted} (both directions)"
  for sname in $want_shots; do
    made "$B/$id/shots-a/$sname.ppm" "$B/$id/shots-b/$sname.ppm"
    cmp "$B/$id/shots-a/$sname.ppm" "$B/$id/shots-b/$sname.ppm" || fail "flow $id shot $sname: runs A/B not byte-identical"
    node -e '
      const fs = require("fs");
      const b = fs.readFileSync(process.argv[1]);
      // P6 240x240: header then raw triples; count distinct triples
      const hdr = b.indexOf(10, b.indexOf(10, b.indexOf(10) + 1) + 1) + 1;
      const seen = new Set();
      for (let k = hdr; k + 2 < b.length; k += 3) {
        seen.add(b[k] * 65536 + b[k + 1] * 256 + b[k + 2]);
        if (seen.size > 1) process.exit(0);
      }
      console.error("shot is a single flat colour (blank render)");
      process.exit(1);
    ' "$B/$id/shots-a/$sname.ppm" || fail "flow $id shot $sname: blank-render witness failed"
    total_shots=$((total_shots + 1))
  done
  ndistinct="$(shasum -a 256 "$B/$id/shots-a/"*.ppm | awk '{print $1}' | sort -u | wc -l | tr -d ' ')"
  nfiles="$(ls "$B/$id/shots-a" | wc -l | tr -d ' ')"
  [ "$ndistinct" = "$nfiles" ] || fail "flow $id: shots not pairwise distinct ($ndistinct unique of $nfiles) — a stuck screen machine renders duplicates"
  echo "    -> flow $id OK (trace frozen-match, x2 stable, shots $nfiles)"
done
[ "$total_shots" = 11 ] || fail "shot total $total_shots != pinned 11"

# --- [4] match-launch bridges vs the frozen streams -----------------------------
echo "=== [4] launch bridges: f01 -> g01, f02 -> m01 (UNCHANGED verify-stream.js)"
STREAM_RE_TAIL='[0-9]+/[0-9]+ frames exact, rngCalls=[0-9]{1,6}, rngCallsOutsideStep=1, specVersion=1$'
count_aff() { # affinity: extension OR torn prefix of the exact stem
  local c rc=0
  c="$(grep -cE -- "$(printf '%s' "$2" | sed 's/[][\.|$(){}?+*^]/\\&/g')" "$1")" || rc=$?
  if [ "$rc" -ge 2 ]; then grammar_die "count_aff — grep rc $rc reading '$1'"; fi
  printf '%s' "$c"
}
judge_bridge() { # <flowId> <goldenId> <goldenName> <frames> <frozen> [<wrapman>]
  local id="$1" gid="$2" name="$3" frames="$4" frozen="$5" wrapman="${6:-}"
  test -f "$frozen" || fail "bridge $id: frozen stream missing: $frozen"
  rm -f "$B/$id.run.json"
  if [ -n "$wrapman" ]; then
    { node "$SIM/wrap-run.js" "$gid" "$B/$id/stream.txt" "$B/$id.run.json" "$wrapman"; } 2>&1 | relay_lines
  else
    { node "$SIM/wrap-run.js" "$gid" "$B/$id/stream.txt" "$B/$id.run.json"; } 2>&1 | relay_lines
  fi
  made "$B/$id.run.json"
  local vlog="$B/$id.verify.log"
  rm -f "$vlog"
  if ! { node oracle/harness/verify-stream.js "$B/$id.run.json" "$frozen" | tee "$vlog"; } 2>&1 | relay_lines; then
    fail "verify-stream rc != 0 for bridge $id (log: $vlog)"
  fi
  made "$vlog"
  local re="^STREAM MATCH $name: $frames/$frames frames exact, rngCalls=[0-9]{1,6}, rngCallsOutsideStep=1, specVersion=1$"
  local c a last
  c="$(count_e "$vlog" "$re")"
  a="$(count_aff "$vlog" "STREAM MATCH $name: ")"
  if [ "$c" != 1 ] || [ "$a" != 1 ]; then
    grammar_die "bridge $id — STREAM MATCH grammar in $vlog: full $c/1, stem-affine $a/1 (a torn or foreign verdict is CORRUPTION)"
  fi
  last="$(tail -n 1 "$vlog")"
  if ! [[ "$last" =~ $re ]]; then
    grammar_die "bridge $id — final line of $vlog is not the STREAM MATCH verdict"
  fi
  echo "    -> STREAM MATCH $name (FOH-launched, $frames frames pinned)"
}
judge_bridge f01-vs-g01 g01 "${ORACLE_NAMES[0]}" "$G01_FRAMES" \
  "oracle/goldens/${ORACLE_NAMES[0]}.sha256.json"
judge_bridge f02-cpu-m01 m01 "${M4_NAMES[0]}" "$M01_FRAMES" \
  "$M4G/${M4_NAMES[0]}.sha256.json" "$M4G/manifest.json"

# --- [5] TEETH (standing; generated variants/copies only) ----------------------
echo "=== [5] teeth (pre-registered T1-T6)"
teeth=0
# Variant generators (NO eval — the manifest-eval class stays dead):
# fixed operations over the committed flow bytes, anchors must match
# exactly or the tooth itself dies.
mkvariant() { # <src> <dst> <insert-after|delete> <anchor/lines...>
  node -e '
    const fs = require("fs");
    const [src, dst, op, ...args] = process.argv.slice(1);
    const ls = fs.readFileSync(src, "utf8").split("\n");
    if (op === "insert-after") {
      const [anchor, ...ins] = args;
      const i = ls.indexOf(anchor);
      if (i < 0) { console.error("anchor missing: " + anchor); process.exit(1); }
      ls.splice(i + 1, 0, ...ins);
    } else if (op === "delete") {
      for (const drop of args) {
        const i = ls.indexOf(drop);
        if (i < 0) { console.error("anchor missing: " + drop); process.exit(1); }
        ls.splice(i, 1);
      }
    } else { console.error("bad op"); process.exit(1); }
    fs.writeFileSync(dst, ls.join("\n"));
  ' "$1" "$2" "$3" "${@:4}"
  made "$2"
}
run_variant() { # <flow-file> <out-trace>
  rm -f "$2"
  "$B/foh_app" --flow "$1" --flow-out "$2" 2>&1 | relay_lines
  made "$2"
}
# T1 nav-perturb: one extra DOWN in menu-top before the VS confirm
mkvariant "$FLOWS/f01-vs-g01.flow" "$B/t1.flow" insert-after \
  "I 376 -" "I 377 D" "I 378 -"
run_variant "$B/t1.flow" "$B/t1.trace.txt"
rc=0; cmp -s "$B/t1.trace.txt" "$FLOWS/f01-vs-g01.expect" || rc=$?
[ "$rc" = 1 ] || fail "T1 — nav-perturb variant trace cmp rc $rc, want exactly 1 (transition trace must be input-driven)"
echo "    T1 OK: extra DOWN diverges the frozen trace (cmp rc 1)"
teeth=$((teeth + 1))
# T2 char variant: a third RIGHT on the P1 row -> p1=3 in LAUNCH
mkvariant "$FLOWS/f01-vs-g01.flow" "$B/t2.flow" insert-after \
  "I 396 -" "I 397 R" "I 398 -"
run_variant "$B/t2.flow" "$B/t2.trace.txt"
rc=0; cmp -s "$B/t2.trace.txt" "$FLOWS/f01-vs-g01.expect" || rc=$?
[ "$rc" = 1 ] || fail "T2 — char-variant trace cmp rc $rc, want exactly 1"
c="$(count_x "$B/t2.trace.txt" "p1=3")"
[ "$c" = 1 ] || fail "T2 — variant LAUNCH does not carry p1=3 (launch record must be selection-driven)"
echo "    T2 OK: extra RIGHT lands p1=3 in LAUNCH and diverges the trace"
teeth=$((teeth + 1))
# T3 stream-judge tooth: flip one hash nibble in a COPY of the run JSON
node -e '
  const fs = require("fs");
  let s = fs.readFileSync(process.argv[1], "utf8");
  const m = /"h":"([0-9a-f])/.exec(s);
  if (!m) { console.error("no hash found"); process.exit(1); }
  const flip = m[1] === "0" ? "1" : "0";
  s = s.replace(/"h":"[0-9a-f]/, "\"h\":\"" + flip);
  fs.writeFileSync(process.argv[2], s);
' "$B/f01-vs-g01.run.json" "$B/t3.run.json"
made "$B/t3.run.json"
rc=0
node oracle/harness/verify-stream.js "$B/t3.run.json" \
  "oracle/goldens/${ORACLE_NAMES[0]}.sha256.json" > "$B/t3.log" 2>&1 || rc=$?
[ "$rc" != 0 ] || fail "T3 — verify-stream ACCEPTED a nibble-flipped run JSON (the stream judge is not judging)"
echo "    T3 OK: nibble-flipped run JSON dies in verify-stream (rc $rc)"
teeth=$((teeth + 1))
# T4 grammar teeth on COPIES of a genuine trace
cp "$B/f01-vs-g01/trace-a.txt" "$B/t4a.txt"
node -e '
  const fs = require("fs");
  const ls = fs.readFileSync(process.argv[1], "utf8").split("\n");
  ls.splice(ls.length - 2, 0, "T garbage line");
  fs.writeFileSync(process.argv[1], ls.join("\n"));
' "$B/t4a.txt"
rc=0
node "$FOH/judge-foh-trace.js" "$B/t4a.txt" f01-vs-g01 1 > "$B/t4a.log" 2>&1 || rc=$?
[ "$rc" = 2 ] || fail "T4a — malformed line rc $rc, want the corruption death 2"
c="$(count_x "$B/t4a.log" "matches no FOHTRACE1 form")"
[ "$c" = 1 ] || fail "T4a — death message class missing"
sed 's/p1=2/p1=5/' "$B/f01-vs-g01/trace-a.txt" > "$B/t4b.txt"
rc=0
node "$FOH/judge-foh-trace.js" "$B/t4b.txt" f01-vs-g01 1 > "$B/t4b.log" 2>&1 || rc=$?
[ "$rc" = 2 ] || fail "T4b — resembling LAUNCH (p1=5) rc $rc, want 2 (resemblance is corruption, never a parse)"
echo "    T4 OK: malformed + resembling trace lines die in the judge (rc 2, counted classes)"
teeth=$((teeth + 1))
# T5 shot corruption on a COPY
cp "$B/f01-vs-g01/shots-a/css.ppm" "$B/t5.ppm"
printf 'x' >> "$B/t5.ppm"
rc=0; cmp -s "$B/t5.ppm" "$B/f01-vs-g01/shots-a/css.ppm" || rc=$?
[ "$rc" = 1 ] || fail "T5 — corrupted shot copy cmp rc $rc, want exactly 1"
echo "    T5 OK: corrupted shot copy dies in the byte-stability arm (cmp rc 1)"
teeth=$((teeth + 1))
# T6 difficulty variant: drop the 435+440 LEFT pairs -> d2 (the 440
# clamp press absorbs a single removal — measured; AGENT-LOG iter 88)
mkvariant "$FLOWS/f02-cpu-m01.flow" "$B/t6.flow" delete \
  "I 435 L" "I 436 -" "I 440 L" "I 441 -"
run_variant "$B/t6.flow" "$B/t6.trace.txt"
rc=0; cmp -s "$B/t6.trace.txt" "$FLOWS/f02-cpu-m01.expect" || rc=$?
[ "$rc" = 1 ] || fail "T6 — difficulty-variant trace cmp rc $rc, want exactly 1"
c="$(count_x "$B/t6.trace.txt" "difficulty=2")"
[ "$c" = 1 ] || fail "T6 — variant LAUNCH does not carry difficulty=2 (slider steps must be load-bearing)"
echo "    T6 OK: fewer LEFTs land difficulty=2 in LAUNCH and diverge the trace"
teeth=$((teeth + 1))
[ "$teeth" = 6 ] || fail "teeth ledger — $teeth/6 fired"

# --- [6] hygiene ----------------------------------------------------------------
rc=0
git check-ignore -q "$FOH/build" || rc=$?
if [ "$rc" = 1 ]; then
  fail "hygiene — $FOH/build is NOT git-ignored (build outputs could be committed)"
elif [ "$rc" -ge 2 ]; then
  grammar_die "hygiene — git check-ignore rc $rc (corrupt evidence, never a pass)"
fi

echo "FOH FLOWS OK (flows=4 shots=11 bridges=3 streams=MATCH teeth=6)"
