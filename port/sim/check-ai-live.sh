#!/usr/bin/env bash
# check-ai-live.sh — M4 task 5 done-check: LIVE CPU integration — the
# real C AI (port/sim/ai.c) at the sim's runAI site, the AIBRIDGE1 path
# retired from the live path (kept as the M2 archival arm).
#
# Composes (fix_plan §M4 task 5; prints AI LIVE CONFORMS, exit 0):
#   [0] PRODUCER BYTE PINS — check-sim.sh (fix_plan §M4: "the M2 EXIT
#       GATE is NEVER edited"; HARD RULE 3 witness) PLUS every other
#       composed evidence producer this check delegates judgment to:
#       check-ai-bridge.sh, check-ai-replay.sh, wrap-run.js,
#       verify-stream.js (iter 83, review-81 M2). A reviewed change to
#       any of them updates its pin here IN THE SAME COMMIT.
#   [1] bash port/sim/check-sim.sh — the UNCHANGED M2 EXIT GATE, its
#       bridge-fed form intact (g07/g08 via AIBRIDGE1); also produces
#       the shared build artifacts (CTAB1/STAB1, simdata, trace txts).
#       The CPU goldens' bridge artifacts are REMOVED up front so the
#       gate always runs its cold capture-record arm — the leg's log
#       shape is deterministic and the captures are provably fresh.
#   [2] sim_host_live build: check-sim.sh's EXACT TU list (kept in sync
#       — the M3 device-rig precedent; link failure catches a dropped
#       TU) + port/sim/ai.c + port/sim/sim/sim_ai_live.c (the live-AI
#       driver TU behind the ml_sim_runai_live pointer seam, sim.h).
#       M2-CONTRACT WITNESS: the gate's own sim_host (no live TU) must
#       still REFUSE --cpu without --ai-bridge — rc 1 by CASE-SPLIT
#       plus the SPECIFIC 2-line usage message by anchored full-line
#       grammar (iter 83, review-81 M1: bare rc 1 could be any future
#       usage rejection; the message is the witness).
#   [3] LIVE g07/g08: replayed with NO --ai-bridge (the live C AI draws
#       off the seeded chain), judged by the PINNED-UNCHANGED
#       oracle/harness/verify-stream.js against the FROZEN
#       oracle/goldens streams — the streams don't move, so live-vs-
#       bridge bit-identity is proven against the same contract.
#   [4] LIVE m01/m02 (port/goldens-m4/ — the d1/d9 CPU-difficulty
#       coverage goldens, browser x2-identity + quality contract at
#       record time): same live replay, judged by the same unchanged
#       verifier against port/goldens-m4/*.sha256.json.
#   [5] bash port/sim/calib/check-ai-bridge.sh — the M2 archival rig
#       intact (AI BRIDGE OK).
#   [6] bash port/sim/calib/check-ai-replay.sh — the task-4 aiport
#       capture-replay rig intact (AI MATCH).
# Never weakened: every stream judgment is the unchanged verify-stream.js
# (exact per-frame equality, full length, rngCalls + specVersion pins).
#
# AGGREGATOR CLASSES (iter 83 — review-81 round-1 closure,
# .loop/review-81-triage.md; the check-vfx-seam.sh kit adapted):
#   - FRESHNESS / rm-before-produce (H): every artifact this check
#     produces or consumes-fresh is REMOVED before its producer runs
#     and made()-asserted after — a producer that exits 0 without
#     writing (or a planted stale run JSON) dies loudly, never judges
#     stale evidence.
#   - PRODUCER PINS (M2): see [0].
#   - INVENTORY-EXECUTION BINDING (M3): ORACLE_CPU_IDS/M4_CPU_IDS are
#     the pinned live corpus; BOTH manifests' cpu==true id sets are
#     asserted == the arrays (both directions) and the oracle
#     manifest's full name list == the frozen SIM_GOLDENS judge set,
#     BEFORE the lock and any run; every loop below derives from the
#     arrays and the strictly parsed params — no independent literals.
#   - VERDICT + EVIDENCE GRAMMAR (M1): per sub-check, a fresh raw log
#     (tee; relayed '  | '-prefixed) judged by anchored full-line
#     grammar measured from the REAL corpus (.loop/m4-task5-donecheck
#     .log): exact verdict line exactly once + affinity counter + FINAL
#     line, per-golden banner/STREAM MATCH identity binding at the
#     measured counts, byte-stable section positions, and the
#     'REPLAY RAN …, 0 divergences' anchors. Resembles-but-doesn't-
#     match = corruption = death (PROCESS §3 whitelist rule).
#   - MANIFEST LINE-PARSE (M4): the eval class is dead — golden params
#     come through read_golden()'s rc-checked key=value line-parse
#     with per-key anchored whitelists, duplicate rejection, exact
#     line count, and trace === name+'.trace.json' (basename-only by
#     construction; no path escape from the golden home).
#   - RELAY CONTRACT: every relayed sub-check byte carries the '  | '
#     prefix, so the final AI LIVE CONFORMS echo is the only possible
#     unprefixed anchored occurrence in composed output.
#   - RUN LOCK (iter-41/66 no-reclaim pattern) + rc-case-split
#     no-commit guard (now also covering port/goldens-m4/).
set -euo pipefail
cd "$(dirname "$0")/../.."

CAL=port/sim/calib
BUILD=$CAL/build
SIM=port/sim/sim
TABLES=pipeline/build/sim-tables
M4G=port/goldens-m4

fail() { echo "AI LIVE FAIL: $*" >&2; exit 1; }
grammar_die() { echo "AI LIVE FAIL: $*" >&2; exit 2; }

# RELAY (log-sentinel contract): every relayed sub-check byte carries
# the visible '  | ' prefix in this script's own output.
relay_lines() { sed 's/^/  | /'; }

# made <file...> — the rm-before-produce freshness guard (riglib.sh
# pattern, host-local): a produced artifact must exist non-empty AFTER
# its producer ran (the rm above the producer guarantees it cannot be
# stale bytes).
made() {
  local f
  for f in "$@"; do
    if ! [ -s "$f" ]; then
      fail "artifact $f missing or empty after its producer ran (rm-before-produce freshness guard)"
    fi
  done
}

# count_x <file> <exact-line> / count_e <file> <ere> — grep count
# helpers with the rc CASE-SPLIT (the vfx-kit form): grep rc 0/1 is
# count semantics (rc 1 IS "0 matches"); rc >= 2 is a read/pattern
# error and DIES. Never a silent 0 count.
count_x() {
  local c rc=0
  c="$(grep -cxF -- "$2" "$1")" || rc=$?
  if [ "$rc" -ge 2 ]; then
    grammar_die "count helper — grep -cxF rc $rc reading '$1' (a read error is CORRUPT evidence, never a 0 count)"
  fi
  printf '%s' "$c"
}
count_e() {
  local c rc=0
  c="$(grep -cE -- "$2" "$1")" || rc=$?
  if [ "$rc" -ge 2 ]; then
    grammar_die "count helper — grep -cE rc $rc reading '$1' (a read error is CORRUPT evidence, never a 0 count)"
  fi
  printf '%s' "$c"
}

# count_aff <file> <literal> — the AFFINITY counter: nonempty lines
# that EXTEND the literal or are a proper PREFIX of it (a torn write
# truncates the tail; '^literal' resemblance alone cannot see a torn
# 'AI LIVE CONFOR'). Any nonzero awk rc is a read error -> death.
count_aff() {
  local c rc=0
  c="$(awk -v lit="$2" 'length($0) > 0 && (index($0, lit) == 1 || index(lit, $0) == 1) { n++ } END { printf "%d", n + 0 }' "$1")" || rc=$?
  if [ "$rc" -ne 0 ]; then
    grammar_die "affinity counter — awk rc $rc reading '$1' (a read error is CORRUPT evidence, never a 0 count)"
  fi
  printf '%s' "$c"
}

# byteid_sections <file> — positional identity for the id-less
# byte-stable line: sections delimited by the '== gNN (' runA banner
# family; emits "<nsections> <pre-banner count> <per-section counts>".
BYTEID_LINE='   capture byte-identical across two fresh runs'
byteid_sections() {
  local out rc=0
  out="$(awk -v bi="$BYTEID_LINE" '
    /^== g[0-9][0-9] \(/ { nsec++; next }
    $0 == bi { cnt[nsec]++ }
    END { printf "%d", nsec + 0; for (i = 0; i <= nsec; i++) printf " %d", cnt[i] + 0 }
  ' "$1")" || rc=$?
  if [ "$rc" -ne 0 ]; then
    grammar_die "byte-stable sections — awk rc $rc reading '$1' (a read error is CORRUPT evidence)"
  fi
  printf '%s' "$out"
}

# --- [0] PRODUCER BYTE PINS (before anything runs) -----------------------------
# fix_plan §M4: check-sim.sh is never edited — its bytes are pinned, as
# are every composed evidence producer's (iter 83, review-81 M2). A
# differing hash is a loud review moment, never silent drift. UPDATE
# DISCIPLINE (binding): a reviewed change to a pinned producer updates
# its line below IN THE SAME COMMIT (shasum -a 256 <path>).
PRODUCER_PINS="\
ce0882bee2a0bb0ad11ac51366ef467c3811d832f9dc932c4eb10dd3ccc4c8cb port/sim/check-sim.sh
3f2193ad7794e240964881f29feb5a7fc665ee5f42f2110f979f43cd89bb5156 port/sim/calib/check-ai-bridge.sh
d9665489bc495a7a12d5bc62dc5c2fe2c79fadae7c01fb22269378610daefc53 port/sim/calib/check-ai-replay.sh
b835b5f886225e0015dae152576eea5a42fa69d7ba0699f4de0e31438d05c5b9 port/sim/sim/wrap-run.js
f420723433b19166b53a80aedf54931ffdfbc6d2505c773fd73b7a13bbcdf60e oracle/harness/verify-stream.js"
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
  pout="$(shasum -a 256 "$ppath")" || fail "cannot hash producer $ppath"
  have="${pout%% *}"
  if ! [[ "$have" =~ ^[0-9a-f]{64}$ ]] || [ "$pout" != "$have  $ppath" ]; then
    fail "producer hash read malformed for $ppath: '$pout'"
  fi
  if [ "$have" != "$psha" ]; then
    fail "producer byte pin — $ppath sha256 $have != pinned $psha (a producer change invalidates this check's evidence; a legitimate, reviewed change must update the pin in the same commit)"
  fi
  n_pins=$((n_pins + 1))
done <<< "$PRODUCER_PINS"
if [ "$n_pins" != "$N_PINS_WANT" ]; then
  fail "producer pin inventory — $n_pins pins verified, want exactly $N_PINS_WANT (a dropped pin line is corruption, never a partial pass)"
fi

# --- [0b] INVENTORY-EXECUTION BINDING (before the lock and any run) -----------
# The live corpus is pinned HERE and every loop below derives from these
# arrays (iter 83, review-81 M3 — no independent literals). Growing
# either manifest's cpu set is a reviewed pin update.
ORACLE_CPU_IDS=(g07 g08)
M4_CPU_IDS=(m01 m02)
LIVE_IDS=("${ORACLE_CPU_IDS[@]}" "${M4_CPU_IDS[@]}")

# The 8-golden identity set the check-sim leg must judge (the M2 EXIT
# gate's full manifest, frozen names — the vfx-kit SIM_GOLDENS).
SIM_GOLDENS=(
  "g01-fox-marth-battlefield"
  "g02-falco-puff-ystory"
  "g03-falcon-fox-pstadium"
  "g04-puff-falcon-dreamland"
  "g05-marth-falco-fdest"
  "g06-falcon-marth-fountain"
  "g07-falco-falcon-battlefield"
  "g08-fox-puff-fdest"
)
if [ "${#ORACLE_CPU_IDS[@]}" != 2 ] || [ "${#M4_CPU_IDS[@]}" != 2 ] || [ "${#SIM_GOLDENS[@]}" != 8 ]; then
  fail "inventory pin — pinned array lengths off (oracle ${#ORACLE_CPU_IDS[@]}/2, m4 ${#M4_CPU_IDS[@]}/2, sim ${#SIM_GOLDENS[@]}/8)"
fi
for arr in ORACLE_CPU_IDS M4_CPU_IDS SIM_GOLDENS LIVE_IDS; do
  dupes="$(eval 'printf "%s\n" "${'"$arr"'[@]}"' | sort | uniq -d)"
  if [ -n "$dupes" ]; then
    fail "inventory pin — duplicate entries in $arr: $dupes"
  fi
done
gids="$(node -e '
  const fs = require("fs");
  const m = JSON.parse(fs.readFileSync("oracle/goldens/manifest.json", "utf8"));
  process.stdout.write(m.goldens.filter((g) => g.cpu === true).map((g) => g.id).sort().join(" "));
')" || fail "cannot read the oracle manifest"
if [ "$gids" != "${ORACLE_CPU_IDS[*]}" ]; then
  fail "oracle CPU inventory pin — manifest cpu goldens {$gids} != pinned {${ORACLE_CPU_IDS[*]}} (both directions; a grown/shrunk cpu set is a reviewed pin update, never a silent coverage change)"
fi
m4ids="$(node -e '
  const fs = require("fs");
  const m = JSON.parse(fs.readFileSync("port/goldens-m4/manifest.json", "utf8"));
  process.stdout.write(m.goldens.filter((g) => g.cpu === true).map((g) => g.id).sort().join(" "));
')" || fail "cannot read the m4 manifest"
if [ "$m4ids" != "${M4_CPU_IDS[*]}" ]; then
  fail "m4 CPU-golden inventory pin — manifest cpu goldens {$m4ids} != pinned {${M4_CPU_IDS[*]}}"
fi
gnames="$(node -e '
  const fs = require("fs");
  const m = JSON.parse(fs.readFileSync("oracle/goldens/manifest.json", "utf8"));
  process.stdout.write(m.goldens.map((g) => g.name).join(" "));
')" || fail "cannot read the oracle manifest"
if [ "$gnames" != "${SIM_GOLDENS[*]}" ]; then
  fail "sim-golden inventory pin — oracle manifest names {$gnames} != frozen SIM_GOLDENS {${SIM_GOLDENS[*]}} (the check-sim leg's judge set must cover the gate's whole manifest)"
fi

# --- [0c] STRICT GOLDEN PARAM PARSE (the eval class is dead) ------------------
# read_golden <manifest> <id> — rc-checked key=value line-parse with
# per-key anchored whitelists, duplicate rejection, exact line count,
# and the name-derived trace pin (PROCESS §3; iter 83, review-81 M4).
read_golden() {
  local mf="$1" id="$2" out line v n=0
  G_NAME= G_TRACE= G_SEED= G_P1= G_P2= G_STAGE= G_FRAMES= G_DIFF=
  out="$(node -e '
    const fs = require("fs");
    const m = JSON.parse(fs.readFileSync(process.argv[1], "utf8"));
    const g = m.goldens.find((x) => x.id === process.argv[2]);
    if (!g) { console.error("no golden " + process.argv[2] + " in " + process.argv[1]); process.exit(1); }
    if (g.cpu !== true) { console.error(process.argv[2] + " is not a cpu golden"); process.exit(1); }
    const emit = (k, v) => process.stdout.write(k + "=" + String(v) + "\n");
    emit("name", g.name); emit("trace", g.trace); emit("seed", g.seed);
    emit("p1", g.p1); emit("p2", g.p2); emit("stage", g.stage);
    emit("frames", g.frames); emit("difficulty", g.difficulty);
  ' "$mf" "$id")" || fail "manifest read for $id from $mf failed"
  while IFS= read -r line; do
    n=$((n + 1))
    v="${line#*=}"
    case "$line" in
      (name=*)
        [[ "$v" =~ ^[a-z0-9][a-z0-9-]*$ ]] || grammar_die "manifest grammar — $id name '$v' fails the whitelist [a-z0-9-]"
        [ -z "$G_NAME" ] || grammar_die "manifest grammar — duplicate name line for $id"
        G_NAME="$v" ;;
      (trace=*)
        [[ "$v" =~ ^[a-z0-9][a-z0-9.-]*$ ]] || grammar_die "manifest grammar — $id trace '$v' fails the whitelist (basename characters only)"
        [ -z "$G_TRACE" ] || grammar_die "manifest grammar — duplicate trace line for $id"
        G_TRACE="$v" ;;
      (seed=*)
        [[ "$v" =~ ^[0-9]{1,10}$ ]] || grammar_die "manifest grammar — $id seed '$v' is not a plain integer"
        [ -z "$G_SEED" ] || grammar_die "manifest grammar — duplicate seed line for $id"
        G_SEED="$v" ;;
      (p1=*)
        [[ "$v" =~ ^[0-4]$ ]] || grammar_die "manifest grammar — $id p1 '$v' outside the char domain 0-4"
        [ -z "$G_P1" ] || grammar_die "manifest grammar — duplicate p1 line for $id"
        G_P1="$v" ;;
      (p2=*)
        [[ "$v" =~ ^[0-4]$ ]] || grammar_die "manifest grammar — $id p2 '$v' outside the char domain 0-4"
        [ -z "$G_P2" ] || grammar_die "manifest grammar — duplicate p2 line for $id"
        G_P2="$v" ;;
      (stage=*)
        [[ "$v" =~ ^[0-5]$ ]] || grammar_die "manifest grammar — $id stage '$v' outside the stage domain 0-5"
        [ -z "$G_STAGE" ] || grammar_die "manifest grammar — duplicate stage line for $id"
        G_STAGE="$v" ;;
      (frames=*)
        [[ "$v" =~ ^[1-9][0-9]{0,5}$ ]] || grammar_die "manifest grammar — $id frames '$v' is not a positive integer"
        [ -z "$G_FRAMES" ] || grammar_die "manifest grammar — duplicate frames line for $id"
        G_FRAMES="$v" ;;
      (difficulty=*)
        [[ "$v" =~ ^[1-9]$ ]] || grammar_die "manifest grammar — $id difficulty '$v' outside the cpu domain 1-9"
        [ -z "$G_DIFF" ] || grammar_die "manifest grammar — duplicate difficulty line for $id"
        G_DIFF="$v" ;;
      (*)
        grammar_die "manifest grammar — unrecognized param line '$line' for $id (whitelist parse; resembles-but-doesn't-match is corruption)" ;;
    esac
  done <<< "$out"
  [ "$n" = 8 ] || grammar_die "manifest grammar — $id emitted $n param lines, want exactly 8"
  if [ "$G_TRACE" != "$G_NAME.trace.json" ]; then
    grammar_die "manifest grammar — $id trace '$G_TRACE' != name-derived '$G_NAME.trace.json' (basename-only by construction; no path escape from the golden home)"
  fi
}

# Parse ALL four live goldens up front (fail-fast, before the lock);
# the replay/judge loops below consume ONLY these parsed values.
P_NAME=() P_SEED=() P_P1=() P_P2=() P_STAGE=() P_FRAMES=() P_DIFF=() P_TRACE=() P_SRC=()
for id in "${ORACLE_CPU_IDS[@]}"; do
  read_golden oracle/goldens/manifest.json "$id"
  P_NAME+=("$G_NAME"); P_SEED+=("$G_SEED"); P_P1+=("$G_P1"); P_P2+=("$G_P2")
  P_STAGE+=("$G_STAGE"); P_FRAMES+=("$G_FRAMES"); P_DIFF+=("$G_DIFF")
  P_TRACE+=("$G_TRACE"); P_SRC+=("oracle")
done
for id in "${M4_CPU_IDS[@]}"; do
  read_golden "$M4G/manifest.json" "$id"
  P_NAME+=("$G_NAME"); P_SEED+=("$G_SEED"); P_P1+=("$G_P1"); P_P2+=("$G_P2")
  P_STAGE+=("$G_STAGE"); P_FRAMES+=("$G_FRAMES"); P_DIFF+=("$G_DIFF")
  P_TRACE+=("$G_TRACE"); P_SRC+=("m4")
done

# --- RUN LOCK (the iter-41/66 no-reclaim pattern) -----------------------------
# The shared resource is this checkout's build/ artifacts (also rewritten
# by the composed children, which hold their own locks only while running).
mkdir -p "$BUILD"
LOCK="$BUILD/ai-live.lock"
if ! mkdir "$LOCK" 2>/dev/null; then
  lockage="unknown"
  if lockmtime="$(stat -f %m "$LOCK" 2>/dev/null || stat -c %Y "$LOCK" 2>/dev/null)"; then
    lockage="$(( $(date +%s) - lockmtime )) s"
  fi
  echo "AI LIVE REFUSED: run lock $LOCK already exists (age: $lockage)." >&2
  echo "  Another check-ai-live.sh run may be rewriting the shared artifacts" >&2
  echo "  in $BUILD right now. NO auto-reclaim (iter-41 posture). If you are" >&2
  echo "  sure no run is live, remove it manually: rm -rf '$LOCK'" >&2
  exit 1
fi
trap 'rm -rf "$LOCK"' EXIT

# --- shared verdict/evidence grammar ------------------------------------------
STREAM_RE_TAIL='[0-9]{1,6}/[0-9]{1,6} frames exact, rngCalls=[0-9]{1,6}, rngCallsOutsideStep=1, specVersion=1$'

# judge_verdict <clog> <want> — exact verdict line exactly once, exactly
# one verdict-affine line, and the verdict is the log's FINAL line.
judge_verdict() {
  local clog="$1" want="$2" c a last
  test -s "$clog" || grammar_die "empty or missing log $clog"
  c="$(count_x "$clog" "$want")"
  if [ "$c" != 1 ]; then
    grammar_die "verdict grammar — expected exactly 1 '$want' line in $clog, found $c (a duplicated or missing verdict is CORRUPT evidence)"
  fi
  a="$(count_aff "$clog" "$want")"
  if [ "$a" != 1 ]; then
    grammar_die "verdict grammar — $clog carries $a verdict-affine lines (extensions or torn prefixes of '$want') but exactly 1 exact verdict; a verdict-RESEMBLING line is CORRUPTION, never ignorable"
  fi
  last="$(tail -n 1 "$clog")"
  if [ "$last" != "$want" ]; then
    echo "AI LIVE FAIL: verdict grammar — final line of $clog is not the verdict:" >&2
    printf '%s\n' "$last" | relay_lines >&2
    exit 2
  fi
}

# run_subcheck <script> <clog> — fresh raw log + relay + rc.
run_subcheck() {
  local script="$1" clog="$2"
  rm -f "$clog"
  if ! { bash "$script" | tee "$clog"; } 2>&1 | relay_lines; then
    fail "$script rc != 0 (log: $clog)"
  fi
  made "$clog"
}

# judge_stream_id <clog> <name> <want> — per-golden STREAM MATCH
# identity binding: full-grammar count AND stem-affinity count == want.
judge_stream_id() {
  local clog="$1" name="$2" want="$3" c a
  c="$(count_e "$clog" "^STREAM MATCH $name: $STREAM_RE_TAIL")"
  a="$(count_aff "$clog" "STREAM MATCH $name: ")"
  if [ "$c" != "$want" ] || [ "$a" != "$want" ]; then
    grammar_die "identity binding — STREAM MATCH for $name in $clog: full-grammar $c/$want, stem-affine $a/$want (a torn, foreign, or missing stream line is CORRUPTION, never a pass)"
  fi
}

# judge_banner <clog> <banner> — exact banner exactly once + affine once.
judge_banner() {
  local clog="$1" banner="$2" c a
  c="$(count_x "$clog" "$banner")"
  a="$(count_aff "$clog" "$banner")"
  if [ "$c" != 1 ] || [ "$a" != 1 ]; then
    grammar_die "identity binding — banner '$banner' in $clog: exact $c/1, affine $a/1 (a repeated, omitted, or torn banner is CORRUPTION)"
  fi
}

# --- [1] the UNCHANGED M2 EXIT GATE (bridge-fed form intact) ------------------
CS_LOG="$BUILD/ai-live.check-sim.log"
echo "=== [1] port/sim/check-sim.sh (the pinned M2 EXIT GATE, cold shape)"
# FRESHNESS: remove everything this leg produces that later legs consume
# (rm-before-produce); the bridge artifacts go too so the gate's cold
# capture-record arm ALWAYS runs (deterministic log shape + fresh
# captures — check-ai-bridge.sh re-records its own later).
rm -f "$CS_LOG" "$BUILD/sim_host" "$BUILD/simdata.txt" \
      "$TABLES/ml_tables.c" "$TABLES/ml_stages.c"
for id in "${ORACLE_CPU_IDS[@]}"; do
  rm -f "$BUILD/$id.trace.txt" "$BUILD/$id.ai-bridge.txt"
done
run_subcheck port/sim/check-sim.sh "$CS_LOG"
made "$BUILD/sim_host" "$BUILD/simdata.txt" "$TABLES/ml_tables.c" "$TABLES/ml_stages.c"
for id in "${ORACLE_CPU_IDS[@]}"; do
  made "$BUILD/$id.trace.txt" "$BUILD/$id.ai-bridge.txt"
done
# grammar: verdict + all 8 goldens judged exactly once, CPU goldens
# carrying the forced-cold double stream (capture verify + sim replay).
judge_verdict "$CS_LOG" "SIM CONFORMS"
for name in "${SIM_GOLDENS[@]}"; do
  id="${name%%-*}"
  judge_banner "$CS_LOG" "== $id ($name)"
  wantsm=1
  case " ${ORACLE_CPU_IDS[*]} " in
    (*" $id "*) wantsm=2 ;;
  esac
  judge_stream_id "$CS_LOG" "$name" "$wantsm"
done
banners="$(count_e "$CS_LOG" '^== ')"
if [ "$banners" != 8 ]; then
  grammar_die "freshness contract — $CS_LOG carries $banners '== '-family banner lines, want exactly 8 (one per golden); an extra or torn banner is CORRUPTION"
fi
sm="$(count_e "$CS_LOG" '^STREAM MATCH ')"
smfull="$(count_e "$CS_LOG" "^STREAM MATCH [a-z0-9-]+: $STREAM_RE_TAIL")"
if [ "$sm" != 10 ] || [ "$smfull" != 10 ]; then
  grammar_die "freshness contract — $CS_LOG STREAM MATCH evidence off (prefix count: $sm/10, full grammar: $smfull/10 — 6 human + 2x2 CPU cold shape); a malformed or missing stream line is CORRUPTION"
fi
absent="$(count_x "$CS_LOG" "   AI bridge artifact absent — recording the ai capture")"
if [ "$absent" != 2 ]; then
  grammar_die "freshness contract — $CS_LOG carries $absent cold capture-record lines, want exactly 2 (the bridge artifacts were removed up front; a warm arm here means stale bridge evidence)"
fi
c="$(count_x "$CS_LOG" "simdata byte-identical across two fresh dumps")"
if [ "$c" != 1 ]; then
  grammar_die "freshness contract — $CS_LOG carries $c simdata-determinism lines, want exactly 1"
fi
echo "    -> SIM CONFORMS (cold shape, 10/10 stream evidence)"

# --- [2] build sim_host_live --------------------------------------------------
# TU list = check-sim.sh's exact list (KEPT IN SYNC — a dropped TU fails
# the link loudly) + ai.c + sim_ai_live.c.
rm -f "$BUILD/sim_host_live"
cc -O2 -ffp-contract=off -Wall -Wextra -Werror \
  -I"$TABLES" -Iport/ryu -Iport/sim -Ioracle/qjs \
  -o "$BUILD/sim_host_live" \
  "$SIM/sim_main.c" "$SIM/sim_boot.c" "$SIM/sim_tick.c" \
  "$SIM/sim_ser.c" "$SIM/sim_data.c" "$SIM/sim_ai_live.c" \
  "$CAL/canon.c" "$CAL/player_canon.c" \
  port/sim/ai.c \
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
  oracle/qjs/sha256.c \
  port/fdlibm/fdlibm.c -lm
made "$BUILD/sim_host_live"
echo "build OK: $BUILD/sim_host_live (cc -O2 -ffp-contract=off)"

# M2-CONTRACT WITNESS: the gate's own binary (no live TU) must still
# refuse --cpu without --ai-bridge — rc 1 by CASE-SPLIT plus the
# SPECIFIC usage message by anchored grammar (iter 83, review-81 M1:
# bare rc 1 could be any future usage rejection). Params come from the
# parsed g07 row (index 0 — no independent literals).
WIT_LOG="$BUILD/ai-live.witness.log"
rm -f "$WIT_LOG"
rc=0
"$BUILD/sim_host" --trace "$BUILD/${ORACLE_CPU_IDS[0]}.trace.txt" \
  --simdata "$BUILD/simdata.txt" \
  --seed "${P_SEED[0]}" --p1 "${P_P1[0]}" --p2 "${P_P2[0]}" \
  --stage "${P_STAGE[0]}" --frames 1 --cpu --difficulty "${P_DIFF[0]}" \
  > /dev/null 2> "$WIT_LOG" || rc=$?
if [ "$rc" = 0 ]; then
  fail "M2-contract witness — sim_host (bridge-only build) ACCEPTED --cpu without --ai-bridge (rc 0)"
fi
if [ "$rc" != 1 ]; then
  fail "M2-contract witness — sim_host rc $rc, want the usage rejection rc 1"
fi
made "$WIT_LOG"
wit_nl="$(grep -c '' "$WIT_LOG")" || fail "cannot read $WIT_LOG"
wit_u="$(count_x "$WIT_LOG" "usage: sim_host --trace t.txt --simdata s.txt --seed N --p1 N --p2 N --stage N --frames N [--cpu --difficulty N [--ai-bridge f]] [--timing f] [--tapjump-off-p1] [--ai-cover]")"
wit_c="$(count_x "$WIT_LOG" "(--cpu without --ai-bridge needs the live-AI build)")"
if [ "$wit_nl" != 2 ] || [ "$wit_u" != 1 ] || [ "$wit_c" != 1 ]; then
  grammar_die "M2-contract witness — stderr is not EXACTLY the 2-line usage rejection (lines $wit_nl/2, usage line $wit_u/1, bridge line $wit_c/1; log: $WIT_LOG) — a different rc-1 error is NOT the witness"
fi
echo "M2-contract witness OK: sim_host still refuses --cpu without --ai-bridge (rc 1 + the exact usage message)"

# --- [3]+[4] LIVE replays: g07/g08 vs the frozen oracle streams, --------------
#             m01/m02 vs the frozen port/goldens-m4/ streams
k=0
for id in "${LIVE_IDS[@]}"; do
  name="${P_NAME[$k]}"
  if [ "${P_SRC[$k]}" = "m4" ]; then
    rm -f "$BUILD/$id.trace.txt"
    node "$SIM/trace-to-txt.js" "$M4G/${P_TRACE[$k]}" "$BUILD/$id.trace.txt"
    made "$BUILD/$id.trace.txt"
    frozen="$M4G/$name.sha256.json"
    wrapman="$M4G/manifest.json"
  else
    made "$BUILD/$id.trace.txt" # produced fresh by leg [1]
    frozen="oracle/goldens/$name.sha256.json"
    wrapman=""
  fi
  test -f "$frozen" || fail "LIVE $id: frozen stream missing: $frozen"
  echo "== LIVE $id ($name, difficulty ${P_DIFF[$k]} — no --ai-bridge)"
  rm -f "$BUILD/$id.ai-live-out.txt" "$BUILD/$id.ai-live-cov.txt" \
        "$BUILD/$id.ai-live-run.json"
  "$BUILD/sim_host_live" \
    --trace "$BUILD/$id.trace.txt" --simdata "$BUILD/simdata.txt" \
    --seed "${P_SEED[$k]}" --p1 "${P_P1[$k]}" --p2 "${P_P2[$k]}" \
    --stage "${P_STAGE[$k]}" --frames "${P_FRAMES[$k]}" \
    --cpu --difficulty "${P_DIFF[$k]}" --ai-cover \
    > "$BUILD/$id.ai-live-out.txt" 2> "$BUILD/$id.ai-live-cov.txt"
  made "$BUILD/$id.ai-live-out.txt"
  if [ -n "$wrapman" ]; then
    { node "$SIM/wrap-run.js" "$id" "$BUILD/$id.ai-live-out.txt" \
        "$BUILD/$id.ai-live-run.json" "$wrapman"; } 2>&1 | relay_lines
  else
    { node "$SIM/wrap-run.js" "$id" "$BUILD/$id.ai-live-out.txt" \
        "$BUILD/$id.ai-live-run.json"; } 2>&1 | relay_lines
  fi
  made "$BUILD/$id.ai-live-run.json"
  VLOG="$BUILD/$id.ai-live-verify.log"
  rm -f "$VLOG"
  if ! { node oracle/harness/verify-stream.js "$BUILD/$id.ai-live-run.json" \
           "$frozen" | tee "$VLOG"; } 2>&1 | relay_lines; then
    fail "verify-stream rc != 0 for LIVE $id (log: $VLOG)"
  fi
  made "$VLOG"
  live_re="^STREAM MATCH $name: ${P_FRAMES[$k]}/${P_FRAMES[$k]} frames exact, rngCalls=[0-9]{1,6}, rngCallsOutsideStep=1, specVersion=1$"
  c="$(count_e "$VLOG" "$live_re")"
  a="$(count_aff "$VLOG" "STREAM MATCH $name: ")"
  if [ "$c" != 1 ] || [ "$a" != 1 ]; then
    grammar_die "live evidence — STREAM MATCH grammar for $id in $VLOG: full $c/1 (frames pinned ${P_FRAMES[$k]}), stem-affine $a/1 — a torn or foreign verdict is CORRUPTION"
  fi
  last="$(tail -n 1 "$VLOG")"
  if ! [[ "$last" =~ $live_re ]]; then
    grammar_die "live evidence — final line of $VLOG is not the STREAM MATCH verdict for $name"
  fi
  echo "    -> STREAM MATCH $name (live, frames ${P_FRAMES[$k]} pinned)"
  k=$((k + 1))
done

# --- [5]+[6] the archival + port rigs intact ----------------------------------
# judge_ai_leg <clog> <verdict> <spec> <replay-ere> <nreplay> — the
# ai/aiport capture-leg grammar (measured from the archived corpus):
# per CPU golden a runA/runB banner pair + per-name STREAM MATCH x2,
# byte-stable positional sections, the 0-divergences replay anchor.
judge_ai_leg() {
  local clog="$1" verdict="$2" spec="$3" replay_re="$4" nreplay="$5"
  local j gid gname secs banners sm smfull c
  judge_verdict "$clog" "$verdict"
  j=0
  for gid in "${ORACLE_CPU_IDS[@]}"; do
    gname="${P_NAME[$j]}"
    judge_banner "$clog" "== $gid ($gname): $spec capture run A"
    judge_banner "$clog" "== $gid: $spec capture run B"
    judge_stream_id "$clog" "$gname" 2
    j=$((j + 1))
  done
  c="$(count_x "$clog" "$BYTEID_LINE")"
  local aff
  aff="$(count_aff "$clog" "$BYTEID_LINE")"
  secs="$(byteid_sections "$clog")"
  if [ "$c" != 2 ] || [ "$aff" != 2 ] || [ "$secs" != "2 0 1 1" ]; then
    grammar_die "freshness contract — byte-stable evidence in $clog off (exact $c/2, affine $aff/2, sections '$secs' want '2 0 1 1'); each golden section must prove its own two fresh runs byte-identical"
  fi
  banners="$(count_e "$clog" '^== ')"
  if [ "$banners" != 4 ]; then
    grammar_die "freshness contract — $clog carries $banners '== '-family banner lines, want exactly 4 (2 run A + 2 run B)"
  fi
  sm="$(count_e "$clog" '^STREAM MATCH ')"
  smfull="$(count_e "$clog" "^STREAM MATCH [a-z0-9-]+: $STREAM_RE_TAIL")"
  if [ "$sm" != 4 ] || [ "$smfull" != 4 ]; then
    grammar_die "freshness contract — $clog STREAM MATCH evidence off (prefix count: $sm/4, full grammar: $smfull/4)"
  fi
  c="$(count_e "$clog" "$replay_re")"
  if [ "$c" != "$nreplay" ]; then
    grammar_die "replay evidence — $clog carries $c lines matching '$replay_re', want exactly $nreplay (a missing or resembling 0-divergences anchor is CORRUPTION)"
  fi
}

AB_LOG="$BUILD/ai-live.check-ai-bridge.log"
echo "=== [5] port/sim/calib/check-ai-bridge.sh (the pinned M2 archival rig)"
run_subcheck "$CAL/check-ai-bridge.sh" "$AB_LOG"
judge_ai_leg "$AB_LOG" "AI BRIDGE OK" "ai" \
  '^AI BRIDGE REPLAY RAN [0-9]{1,8} records, 0 divergences$' 2
c="$(count_x "$AB_LOG" "   bridge artifact deterministic (x2 byte-identical)")"
if [ "$c" != 2 ]; then
  grammar_die "freshness contract — $AB_LOG carries $c bridge-determinism lines, want exactly 2"
fi
c="$(count_x "$AB_LOG" "build OK: port/sim/calib/build/ai_replay (cc -O2 -ffp-contract=off)")"
if [ "$c" != 1 ]; then
  grammar_die "freshness contract — $AB_LOG carries $c ai_replay build lines, want exactly 1"
fi
echo "    -> AI BRIDGE OK"

AR_LOG="$BUILD/ai-live.check-ai-replay.log"
echo "=== [6] port/sim/calib/check-ai-replay.sh (the pinned aiport rig)"
run_subcheck "$CAL/check-ai-replay.sh" "$AR_LOG"
judge_ai_leg "$AR_LOG" "AI MATCH" "aiport" \
  '^AI PORT REPLAY RAN [0-9]{1,8} records, 0 divergences$' 2
c="$(count_x "$AR_LOG" "build OK: port/sim/calib/build/ai_port_replay (cc -O2 -ffp-contract=off)")"
if [ "$c" != 1 ]; then
  grammar_die "freshness contract — $AR_LOG carries $c ai_port_replay build lines, want exactly 1"
fi
echo "    -> AI MATCH"

# --- no-commit guard (rc CASE-SPLIT; build output is never tracked, ----------
#     and the golden CONTRACT artifacts — manifest, traces, frozen
#     streams — must be clean for this evidence to mean anything;
#     script files under $M4G are deliberately NOT guarded so an
#     in-flight reviewed edit can still run its own done-check) ---------------
rc=0
dirty="$(git status --porcelain -- "$BUILD" "$TABLES" \
  "$M4G/manifest.json" "$M4G/*.trace.json" "$M4G/*.sha256.json")" || rc=$?
if [ "$rc" -ne 0 ]; then
  fail "no-commit guard — git status rc $rc (a status read error is CORRUPT evidence, never a clean pass)"
fi
if [ -n "$dirty" ]; then
  echo "AI LIVE FAIL: build output not gitignored (or the golden contract dirtied):" >&2
  printf '%s\n' "$dirty" >&2
  exit 1
fi

echo "AI LIVE CONFORMS"
