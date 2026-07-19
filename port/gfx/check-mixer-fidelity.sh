#!/usr/bin/env bash
# check-mixer-fidelity.sh — M4 task 6 done-check: mixer fidelity by
# OFFLINE DETERMINISTIC RENDER DIFFERENTIAL + real play-ids + stop-path
# live coverage (fix_plan §M4 task 6; pre-registration AGENT-LOG iter 82;
# HARDENED iter 84 — review-82 round-1 closure, .loop/review-82-triage.md).
#
# Composes, in order:
#   [0] PRODUCER BYTE PINS + INVENTORY-EXECUTION BINDING + the frozen
#       EXPOSURE/WITNESS tables (iter 84): every composed evidence
#       producer is sha256-pinned (check-sim.sh / wrap-run.js /
#       verify-stream.js — the ai-live [0] kit; a reviewed producer
#       change updates its pin IN THE SAME COMMIT); both manifests' FULL
#       id+name inventories are asserted == the pinned arrays (both
#       directions) BEFORE the lock and any run; every loop below
#       derives from the arrays + a strict key=value manifest parse
#       (the eval class is dead — PROCESS §3).
#   [1] SIM CONFORMS — bash port/sim/check-sim.sh (the frozen M2 gate;
#       the id-plumbing edits touch sim TUs and the checksum surface
#       must not move). Its shared artifacts (tables, simdata, g07/g08
#       AIBRIDGE1) are REMOVED up front and made()-asserted after
#       (rm-before-produce: the cold capture-record shape is FORCED).
#   [2] SNDPACK — fresh pipeline audio stage, pack x2 byte-identical,
#       sha256 pinned AND cross-checked against check-device-audio.sh's
#       frozen pin (one pack identity across the audio surface).
#   [3] THE DIFFERENTIAL — for EVERY golden (8 oracle + m01/m02 +
#       s01/s02 — s02 joined iter 85 with the GUARDON depletion-break
#       scenario; its exposure row was measured out-of-band on the
#       fixed sim then frozen, .loop/m4-iter85-donecheck.log,
#       all inventory-pinned): replay on sim_host_snd with the event tap
#       (each run judged STREAM MATCH by the PINNED-UNCHANGED
#       verify-stream.js against its frozen stream — instrumentation
#       cannot perturb the sim), then render the schedule OFFLINE twice
#       through the C mixer math (snd_render.c, byte-stable x2) and
#       through the INDEPENDENT reference (snd_reference.js), and cmp
#       the PCM byte-for-byte. Comparison basis = BIT-DIFF, decided by
#       the pre-registered measurement (AGENT-LOG iter 82): the
#       reference renders TWICE — UNLIMITED voices (browser howler
#       truth; measures true concurrency) and CAPPED at 8 with the
#       documented steal-oldest-by-start-sequence policy. The C mixer
#       must be bit-identical to the CAPPED reference on EVERY golden,
#       and to the UNLIMITED reference wherever measured concurrency
#       <= 8. IterATION-84 EXPOSURE PINS (review-82 High: the basis is
#       no longer decided dynamically): per-golden unlimited-ref
#       maxvoices, C steals, and the stop matched/unmatched split are
#       MEASURED-THEN-FROZEN below — the over-cap set is pinned to
#       exactly {g06, m02} (peak 9, 1 steal each; the registered
#       device-vs-browser exposure), 9 goldens take the unlimited
#       comparison, and any concurrency/steal/stop drift is a loud pin
#       update, never a silent basis switch.
#   [4b] STOP-PATH WITNESSES — the s01 scenario's four stop events are
#       pinned as EXACT frozen witnesses (frame, sound, preceding-play
#       ordinal + frame; arm attribution documented per witness) and
#       the whole stop stream must equal the witness list — aggregate
#       token counts can no longer mask an arm swap (review-82 H). The
#       mixer's stop counter is SPLIT matched/unmatched on both sides
#       of the differential and the measured split is pinned
#       (unmatched == 0 on every golden — measured iter 84).
#   [5] GFX_APP LEG — s01 replayed through the REAL app (headless
#       backend, mixer attached via the ml_snd_sink + ml_snd_stop_id_sink
#       chokepoints): STREAM MATCH + voice starts/stops == the schedule
#       counts (the live path consumes the same events). Params derive
#       from the parsed s01 manifest row — no independent literals.
#   [6] TEETH — standing negative tests (tooth flags are TEST-ONLY
#       seams): on the s01 render — gain nibble, dropped stop,
#       resample-step skew, stop-id skew (howler stale-id no-op -> the
#       loop rings on), schedule-grammar corruption (both renderers
#       fail closed); on the g06 render — steal-policy flip (g06 is a
#       schedule where a steal REALLY fires, so the flip must diverge).
#       Positive controls throughout.
#
# AGGREGATOR CLASSES (iter 84; the check-vfx-seam/ai-live kit):
#   - FRESHNESS / rm-before-produce + made() on EVERY produced artifact
#     (pack, schedules, run JSONs, PCM, verdict files, app-leg outputs);
#   - RUN LOCK (iter-41 no-reclaim posture) around the shared build
#     scratch ($MF, $AUDIO_OUT, and the composed children's calib
#     build/ — children hold their own locks only while running);
#   - DECISION GRAMMARS from FILE BYTES (the $(cat)/$(tail -1) class is
#     dead): single-line verdict files with trailing-newline asserted,
#     exactly-one full-grammar line + exactly-one resembling line;
#   - git-guard rc CASE-SPLIT (a status read error is CORRUPT evidence,
#     never a clean pass).
#
# Prints `MIXER FIDELITY OK (...)`, exit 0; ANY divergence, stream
# mismatch, count disagreement, pin mismatch, or missing artifact ->
# nonzero.
#
# PROVENANCE: the pack + rendered PCM derive from Nintendo-derived
# blobs — gitignored build output only, never committed (guarded below).
set -euo pipefail
cd "$(dirname "$0")/../.."

GFX=port/gfx
B=$GFX/build
MF=$B/mixerfid
SIM=port/sim/sim
CAL=port/sim/calib
TABLES=pipeline/build/sim-tables
AUDIO_OUT=pipeline/build/audio-mixerfid
M4G=port/goldens-m4
DIST="${MELEELIGHT_CLONE:-$HOME/.cache/meleelight-funkey-s/upstream}"

# Pack identity (measured-then-frozen, iter 57; must equal the
# check-device-audio.sh pin — cross-checked below).
SNDPACK_SHA256=f69579082fe569249879faa5ceccb7a810d94d8092695ddc8bb543f3bda3ccb4

host_sha256() { shasum -a 256 "$1" | cut -d' ' -f1; }

fail() { echo "MIXER FIDELITY FAIL: $1" >&2; exit 1; }
grammar_die() { echo "MIXER FIDELITY FAIL: $1" >&2; exit 2; }

# made <file...> — the rm-before-produce freshness guard (riglib
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

# count_x / count_e — grep count helpers with the rc CASE-SPLIT (the
# vfx-kit form): grep rc 0/1 is count semantics (rc 1 IS "0 matches");
# rc >= 2 is a read/pattern error and DIES. Never a silent 0 count.
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

# judge_verdict_file <file> <full-line ERE> <resemblance ERE> — the
# iter-84 M4 posture: a decision-bearing verdict FILE must be EXACTLY
# one line, newline-terminated, that one line matches the full grammar,
# and exactly one line resembles it (zero extra resembling lines).
# Judged from file bytes, never from $(cat) (which eats trailing
# newlines and hides extra blank terminators).
judge_verdict_file() {
  local f="$1" full="$2" resem="$3" nl c r
  test -s "$f" || grammar_die "verdict file $f missing or empty"
  nl="$(grep -c '' "$f")" || grammar_die "cannot count lines of $f"
  [ "$nl" = 1 ] || grammar_die "verdict file $f has $nl lines, want exactly 1 (extra or missing lines are CORRUPTION)"
  if [ -n "$(tail -c 1 "$f")" ]; then
    grammar_die "verdict file $f lacks the trailing newline (torn write)"
  fi
  c="$(count_e "$f" "$full")"
  [ "$c" = 1 ] || grammar_die "verdict file $f: $c lines match the full grammar '$full', want exactly 1"
  r="$(count_e "$f" "$resem")"
  [ "$r" = 1 ] || grammar_die "verdict file $f: $r lines RESEMBLE the verdict ('$resem') but 1 matches the full grammar — a resembling non-matching line is CORRUPTION, never ignorable"
}

# --- [0] PRODUCER BYTE PINS (before anything runs; ai-live kit) ------------
# UPDATE DISCIPLINE (binding): a reviewed change to a pinned producer
# updates its line below IN THE SAME COMMIT (shasum -a 256 <path>).
PRODUCER_PINS="\
ce0882bee2a0bb0ad11ac51366ef467c3811d832f9dc932c4eb10dd3ccc4c8cb port/sim/check-sim.sh
b835b5f886225e0015dae152576eea5a42fa69d7ba0699f4de0e31438d05c5b9 port/sim/sim/wrap-run.js
f420723433b19166b53a80aedf54931ffdfbc6d2505c773fd73b7a13bbcdf60e oracle/harness/verify-stream.js"
N_PINS_WANT=3
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

# --- [0b] INVENTORY-EXECUTION BINDING (iter 84, review-82 M2) --------------
# The differential corpus is pinned HERE; both manifests' FULL id AND
# name inventories are asserted == these arrays (both directions), and
# every loop below derives from them. Growing either manifest is a
# reviewed pin update, never a silent coverage change.
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
# The M2-gate bridge-fed CPU goldens (their differential replays feed
# the AIBRIDGE1 artifacts leg [1] records fresh).
BRIDGE_CPU_IDS=(g07 g08)
if [ "${#ORACLE_IDS[@]}" != 8 ] || [ "${#ORACLE_NAMES[@]}" != 8 ] || \
   [ "${#M4_IDS[@]}" != 4 ] || [ "${#M4_NAMES[@]}" != 4 ] || \
   [ "${#BRIDGE_CPU_IDS[@]}" != 2 ]; then
  fail "inventory pin — pinned array lengths off (oracle ${#ORACLE_IDS[@]}/8+${#ORACLE_NAMES[@]}/8, m4 ${#M4_IDS[@]}/4+${#M4_NAMES[@]}/4, bridge ${#BRIDGE_CPU_IDS[@]}/2)"
fi
for arr in ORACLE_IDS ORACLE_NAMES M4_IDS M4_NAMES BRIDGE_CPU_IDS; do
  dupes="$(eval 'printf "%s\n" "${'"$arr"'[@]}"' | sort | uniq -d)"
  if [ -n "$dupes" ]; then
    fail "inventory pin — duplicate entries in $arr: $dupes"
  fi
done
inv="$(node -e '
  const fs = require("fs");
  const m = JSON.parse(fs.readFileSync(process.argv[1], "utf8"));
  process.stdout.write(m.goldens.map((g) => g.id).join(" ") + "|" +
    m.goldens.map((g) => g.name).join(" "));
' oracle/goldens/manifest.json)" || fail "cannot read the oracle manifest"
if [ "$inv" != "${ORACLE_IDS[*]}|${ORACLE_NAMES[*]}" ]; then
  fail "oracle inventory pin — manifest {$inv} != pinned {${ORACLE_IDS[*]}|${ORACLE_NAMES[*]}} (both directions; a grown/shrunk/renamed golden set is a reviewed pin update)"
fi
inv="$(node -e '
  const fs = require("fs");
  const m = JSON.parse(fs.readFileSync(process.argv[1], "utf8"));
  process.stdout.write(m.goldens.map((g) => g.id).join(" ") + "|" +
    m.goldens.map((g) => g.name).join(" "));
' "$M4G/manifest.json")" || fail "cannot read the m4 manifest"
if [ "$inv" != "${M4_IDS[*]}|${M4_NAMES[*]}" ]; then
  fail "m4 inventory pin — manifest {$inv} != pinned {${M4_IDS[*]}|${M4_NAMES[*]}} (both directions; a grown/shrunk/renamed golden set is a reviewed pin update)"
fi

# EXPOSURE PINS (iter 84, review-82 High #2 — measured-then-frozen,
# .loop/m4-mixrig84-probes.log + the iter-82 measurement): per golden —
#   <id> <unlimited-ref maxvoices> <C steals> <stops> <stopsm> <stopsu>
# The over-cap set is EXACTLY {g06, m02} (peak 9 voices, 1 steal each —
# the registered device-vs-browser exposure, AGENT-LOG iter 82); every
# other golden takes the additional unlimited-reference bit-diff. The
# stop split (review-82 High #3) is pinned matched-only: unmatched == 0
# on EVERY golden (measured — all live stops deactivate a live voice).
# A legitimate sound change re-measures and updates this table loudly.
EXPOSURE_PINS=(
  "g01 6 0 0 0 0"
  "g02 4 0 0 0 0"
  "g03 8 0 0 0 0"
  "g04 5 0 0 0 0"
  "g05 7 0 12 12 0"
  "g06 9 1 0 0 0"
  "g07 3 0 0 0 0"
  "g08 6 0 0 0 0"
  "m01 4 0 0 0 0"
  "m02 9 1 0 0 0"
  "s01 5 0 4 4 0"
  "s02 4 0 2 2 0"
)
MAXV_ALL_PIN=9
STEALS_ALL_PIN=2
DIFF_COUNT_PIN=12
if [ "${#EXPOSURE_PINS[@]}" != "$DIFF_COUNT_PIN" ]; then
  fail "exposure pin — table has ${#EXPOSURE_PINS[@]} rows, want $DIFF_COUNT_PIN"
fi

# judge_exposure <id> <maxv> <steals> <stops> <stopsm> <stopsu> —
# compare a golden's measured values against its frozen pin row (exact
# row lookup; a golden without a row is corruption).
judge_exposure() {
  local id="$1" got="$1 $2 $3 $4 $5 $6" row found=0
  for row in "${EXPOSURE_PINS[@]}"; do
    case "$row" in
      ("$id "*)
        found=1
        if [ "$got" != "$row" ]; then
          fail "exposure pin — $id measured '$got' != frozen '$row' (concurrency/steal/stop drift is a loud reviewed re-measure, never a silent basis switch)"
        fi
        ;;
    esac
  done
  [ "$found" = 1 ] || fail "exposure pin — no frozen row for golden $id"
}

# STOP-ARM WITNESSES (iter 84, review-82 High #1 — measured-then-frozen
# from the STREAM-MATCH-guarded s01 schedule, AGENT-LOG iter 82 arm
# attribution): <stop-frame>:<sound>:<play-ordinal>:<play-frame>, in
# schedule order. The play ordinal is howler's global counter parallel
# (id = 1000 + ordinal — ml_events.c/snd_mixer.h derivation), so each
# witness binds its stop to the EXACT preceding play instance:
#   191:shieldbreakercharge:5:171    — NSG B-release arm (NSG.js:55)
#   697:shieldbreakercharge:18:571   — NSG charge==122 auto arm (NSG.js:67)
#   701:furaloop:21:649              — hitDetection FURAFURA arm (hitDetection.js:762)
#   1419:furaloop:30:1181            — FURAFURA stuckTimer wake arm (FURAFURA.js:56)
# The witness judge asserts the schedule's ENTIRE stop stream equals
# this list (order, frames, sounds, ids) AND that each referenced play
# ordinal is the pinned (frame, sound) play — an arm swap that
# preserves aggregate token counts dies here.
S01_WITNESSES=(
  "191:shieldbreakercharge:5:171"
  "697:shieldbreakercharge:18:571"
  "701:furaloop:21:649"
  "1419:furaloop:30:1181"
)
[ "${#S01_WITNESSES[@]}" = 4 ] || fail "witness pin — ${#S01_WITNESSES[@]} rows, want 4"

# judge_witnesses <schedule> <witness...> — strict full-line schedule
# walk (the tap grammar) + exact stop-stream == witness-list equality.
judge_witnesses() {
  local sched="$1"; shift
  node -e '
    const fs = require("fs");
    const [sched, ...wspecs] = process.argv.slice(1);
    function die(m) { console.error("witness-judge FAIL: " + m); process.exit(1); }
    const W = wspecs.map((s) => {
      const m = /^([0-9]+):([0-9A-Za-z]+):([0-9]+):([0-9]+)$/.exec(s);
      if (!m) die("bad witness spec " + JSON.stringify(s));
      return { stopFrame: +m[1], sound: m[2], ord: +m[3], playFrame: +m[4] };
    });
    const raw = fs.readFileSync(sched, "utf8");
    if (raw.length === 0 || raw[raw.length - 1] !== "\n") die("schedule not newline-terminated");
    const P_RE = /^P (0|[1-9][0-9]*) ([0-9A-Za-z]+)$/;
    const S_RE = /^S (0|[1-9][0-9]*) ([0-9A-Za-z]+)\.stop ([01]) ([0-9a-f]{16})$/;
    const T_RE = /^SNDEV OK plays=(0|[1-9][0-9]*) stops=(0|[1-9][0-9]*) lastFrame=(0|[1-9][0-9]*)$/;
    const plays = [], stops = [];
    let sawTerm = false;
    for (const line of raw.slice(0, -1).split("\n")) {
      if (sawTerm) die("bytes after the terminator");
      let m;
      if ((m = P_RE.exec(line))) plays.push({ frame: +m[1], name: m[2] });
      else if ((m = S_RE.exec(line))) {
        if (m[3] !== "1") die("id-less stop in the schedule: " + line);
        stops.push({ frame: +m[1], sound: m[2],
          id: Buffer.from(m[4], "hex").readDoubleBE(0) });
      } else if ((m = T_RE.exec(line))) {
        if (+m[1] !== plays.length || +m[2] !== stops.length) die("terminator counts disagree");
        sawTerm = true;
      } else die("schedule grammar violation: " + JSON.stringify(line));
    }
    if (!sawTerm) die("missing terminator");
    if (stops.length !== W.length) {
      die("stop stream has " + stops.length + " events, witness list pins " + W.length);
    }
    for (let i = 0; i < W.length; i++) {
      const w = W[i], s = stops[i];
      if (s.frame !== w.stopFrame || s.sound !== w.sound) {
        die("stop[" + i + "] is (frame " + s.frame + ", " + s.sound +
          "), witness pins (frame " + w.stopFrame + ", " + w.sound + ")");
      }
      if (s.id !== 1000 + w.ord) {
        die("stop[" + i + "] id " + s.id + " != pinned 1000+" + w.ord +
          " (the preceding-play binding broke — an arm now stops a different instance)");
      }
      const p = plays[w.ord - 1];
      if (!p || p.frame !== w.playFrame || p.name !== w.sound) {
        die("play ordinal " + w.ord + " is " + (p ? "(frame " + p.frame + ", " + p.name + ")" : "absent") +
          ", witness pins (frame " + w.playFrame + ", " + w.sound + ")");
      }
    }
    console.log("witnesses OK: " + W.length + " stop events bound (frame/sound/preceding-play-id exact)");
  ' "$sched" "$@" || fail "s01 stop-arm witness judge failed"
}

# --- [0c] STRICT GOLDEN PARAM PARSE (the eval class is dead; iter 84) -------
# read_golden <manifest> <id> — rc-checked key=value line-parse with
# per-key anchored whitelists, duplicate rejection, exact line count,
# and the name-derived trace pin (PROCESS §3; the record-m4.sh form —
# cpu true/false + difficulty null accepted for non-cpu goldens).
read_golden() {
  local mf="$1" id="$2" out line v n=0
  G_NAME= G_TRACE= G_FRAMES= G_SEED= G_P1= G_P2= G_STAGE= G_CPU= G_DIFF=
  out="$(node -e '
    const fs = require("fs");
    const m = JSON.parse(fs.readFileSync(process.argv[1], "utf8"));
    const g = m.goldens.find((x) => x.id === process.argv[2]);
    if (!g) { console.error("no golden " + process.argv[2] + " in " + process.argv[1]); process.exit(1); }
    const emit = (k, v) => process.stdout.write(k + "=" + String(v) + "\n");
    emit("name", g.name); emit("trace", g.trace); emit("frames", g.frames);
    emit("seed", g.seed); emit("p1", g.p1); emit("p2", g.p2);
    emit("stage", g.stage); emit("cpu", g.cpu); emit("difficulty", g.difficulty);
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
      (frames=*)
        [[ "$v" =~ ^[1-9][0-9]{0,5}$ ]] || grammar_die "manifest grammar — $id frames '$v' is not a positive integer"
        [ -z "$G_FRAMES" ] || grammar_die "manifest grammar — duplicate frames line for $id"
        G_FRAMES="$v" ;;
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
      (cpu=*)
        [[ "$v" =~ ^(true|false)$ ]] || grammar_die "manifest grammar — $id cpu '$v' is not a boolean"
        [ -z "$G_CPU" ] || grammar_die "manifest grammar — duplicate cpu line for $id"
        G_CPU="$v" ;;
      (difficulty=*)
        [[ "$v" =~ ^([1-9]|null)$ ]] || grammar_die "manifest grammar — $id difficulty '$v' outside 1-9/null"
        [ -z "$G_DIFF" ] || grammar_die "manifest grammar — duplicate difficulty line for $id"
        G_DIFF="$v" ;;
      (*)
        grammar_die "manifest grammar — unrecognized param line '$line' for $id (whitelist parse; resembles-but-doesn't-match is corruption)" ;;
    esac
  done <<< "$out"
  [ "$n" = 9 ] || grammar_die "manifest grammar — $id emitted $n param lines, want exactly 9"
  if [ "$G_TRACE" != "$G_NAME.trace.json" ]; then
    grammar_die "manifest grammar — $id trace '$G_TRACE' != name-derived '$G_NAME.trace.json' (basename-only by construction; no path escape from the golden home)"
  fi
  if [ "$G_CPU" = "true" ]; then
    [ "$G_DIFF" != "null" ] || grammar_die "manifest grammar — $id cpu golden without a difficulty"
  else
    [ "$G_DIFF" = "null" ] || grammar_die "manifest grammar — $id non-cpu golden with a difficulty"
  fi
}

# --- RUN LOCK (iter-41/66 no-reclaim pattern; iter 84, review-82 M3) --------
mkdir -p "$B"
LOCK="$B/mixerfid.lock"
if ! mkdir "$LOCK" 2>/dev/null; then
  lockage="unknown"
  if lockmtime="$(stat -f %m "$LOCK" 2>/dev/null || stat -c %Y "$LOCK" 2>/dev/null)"; then
    lockage="$(( $(date +%s) - lockmtime )) s"
  fi
  echo "MIXER FIDELITY REFUSED: run lock $LOCK already exists (age: $lockage)." >&2
  echo "  Another check-mixer-fidelity.sh run may be rewriting the shared" >&2
  echo "  scratch in $MF / $AUDIO_OUT right now. NO auto-reclaim (iter-41" >&2
  echo "  posture). If you are sure no run is live, remove it manually:" >&2
  echo "  rm -rf '$LOCK'" >&2
  exit 1
fi
trap 'rm -rf "$LOCK"' EXIT
mkdir -p "$MF"

# --- [1] SIM CONFORMS (the pinned M2 gate; forced-cold shared artifacts) ----
echo "== [1/6] SIM CONFORMS regression (check-sim.sh) =="
# rm-before-produce: everything leg [1] produces that later legs consume
# (tables, simdata, sim_host, the bridge-fed trace txts + AIBRIDGE1
# artifacts — forced cold capture-record shape, the ai-live pattern).
rm -f "$CAL/build/sim_host" "$CAL/build/simdata.txt" \
      "$TABLES/ml_tables.c" "$TABLES/ml_stages.c"
for id in "${BRIDGE_CPU_IDS[@]}"; do
  rm -f "$CAL/build/$id.trace.txt" "$CAL/build/$id.ai-bridge.txt"
done
bash port/sim/check-sim.sh
made "$CAL/build/sim_host" "$CAL/build/simdata.txt" \
     "$TABLES/ml_tables.c" "$TABLES/ml_stages.c"
for id in "${BRIDGE_CPU_IDS[@]}"; do
  made "$CAL/build/$id.trace.txt" "$CAL/build/$id.ai-bridge.txt"
done

# --- [2] SNDPACK (fresh stage, x2 pack determinism, pinned identity) -------
echo "== [2/6] SNDPACK1 (fresh audio stage, x2, pinned) =="
rm -rf "$AUDIO_OUT"
node pipeline/run.js --only audio --dist "$DIST" --out "$AUDIO_OUT"
made "$AUDIO_OUT/sounds.json"
pack_re='^pack-snd OK count=180 dataBytes=[0-9]{1,12} fileBytes=[0-9]{1,12}$'
pack_resem='pack-snd '
rm -f "$MF/sndpack.bin" "$MF/sndpack-a.bin" "$MF/sndpack-b.bin"
for side in a b; do
  rm -f "$MF/pack-out-$side.txt"
  node "$GFX/pack-snd.js" "$AUDIO_OUT" "$MF/sndpack-$side.bin" \
    > "$MF/pack-out-$side.txt" || fail "pack-snd.js failed (side $side)"
  made "$MF/sndpack-$side.bin" "$MF/pack-out-$side.txt"
  judge_verdict_file "$MF/pack-out-$side.txt" "$pack_re" "$pack_resem"
done
cmp "$MF/sndpack-a.bin" "$MF/sndpack-b.bin" || fail "pack not byte-stable x2"
mv "$MF/sndpack-a.bin" "$MF/sndpack.bin"; rm -f "$MF/sndpack-b.bin"
psum="$(host_sha256 "$MF/sndpack.bin")"
[ "$psum" = "$SNDPACK_SHA256" ] || \
  fail "sndpack sha256 $psum != pinned $SNDPACK_SHA256 (pipeline/ffmpeg drift — reviewed re-freeze required)"
# one pack identity across the audio surface: the device check's pin
# must be the same constant (anchored full-line extraction; exactly one)
devpins="$(grep -E '^SNDPACK_SHA256=[0-9a-f]{64}$' "$GFX/check-device-audio.sh")" || \
  fail "cannot extract the check-device-audio.sh pack pin"
[ "$(printf '%s\n' "$devpins" | wc -l | tr -d ' ')" = 1 ] || \
  fail "check-device-audio.sh pack pin not unique"
[ "$devpins" = "SNDPACK_SHA256=$SNDPACK_SHA256" ] || \
  fail "pack pin disagrees with check-device-audio.sh ($devpins)"
echo "   pack x2 byte-identical, sha pinned + device-pin cross-checked"

# --- [3] builds ------------------------------------------------------------
echo "== [3/6] builds (sim_host_snd + snd_render + gfx_app headless) =="
CFLAGS=(-ffp-contract=off -Wall -Wextra -Werror
  -I"$TABLES" -Iport/ryu -Iport/sim -Ioracle/qjs)
SIM_CORE=("$SIM/sim_boot.c" "$SIM/sim_tick.c" "$SIM/sim_ser.c"
  "$SIM/sim_data.c" "$SIM/sim_ai_live.c" port/sim/ai.c
  "$CAL/canon.c" "$CAL/player_canon.c"
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
  oracle/qjs/sha256.c port/fdlibm/fdlibm.c)
MOVES=(port/sim/characters/shared/moves/*.c
  port/sim/characters/fox/moves/*.c
  port/sim/characters/falco/moves/*.c
  port/sim/characters/falcon/moves/*.c
  port/sim/characters/marth/moves/*.c
  port/sim/characters/puff/moves/*.c)
rm -f "$MF/sim_host_snd" "$MF/snd_render" "$MF/gfx_app_headless" "$MF/raster.o"
cc -O2 "${CFLAGS[@]}" -o "$MF/sim_host_snd" \
  "$SIM/sim_main.c" "$GFX/snd_events_tap.c" \
  "${SIM_CORE[@]}" "${MOVES[@]}" -lm
cc -O2 -ffp-contract=off -Wall -Wextra -Werror \
  -o "$MF/snd_render" "$GFX/snd_render.c"
cc -O3 "${CFLAGS[@]}" -c "$GFX/raster.c" -o "$MF/raster.o"
cc -O2 "${CFLAGS[@]}" -o "$MF/gfx_app_headless" \
  "$MF/raster.o" "$GFX/gfx_app.c" "$GFX/platform_headless.c" \
  "$GFX/anim1.c" "$GFX/gfx_render.c" "$GFX/gfx_vfx.c" \
  "$GFX/gfx_overlay.c" "$GFX/gfx_bg.c" \
  "${SIM_CORE[@]}" "${MOVES[@]}" -lm
made "$MF/sim_host_snd" "$MF/snd_render" "$MF/gfx_app_headless"
echo "   builds OK (every TU -ffp-contract=off -Wall -Wextra -Werror)"

# --- schedule + differential helpers ---------------------------------------
rend_re='^snd-render OK frames=([0-9]{1,7}) plays=([0-9]{1,9}) stops=([0-9]{1,9}) stopsm=([0-9]{1,9}) stopsu=([0-9]{1,9}) steals=([0-9]{1,9}) maxvoices=([0-9]{1,3}) bytes=([0-9]{1,12})$'
ref_re='^snd-ref OK frames=([0-9]{1,7}) plays=([0-9]{1,9}) stops=([0-9]{1,9}) stopsm=([0-9]{1,9}) stopsu=([0-9]{1,9}) maxvoices=([0-9]{1,3}) steals=([0-9]{1,9}) bytes=([0-9]{1,12})$'
rend_resem='snd-render '
ref_resem='snd-ref '
term_re='^SNDEV OK plays=([0-9]{1,9}) stops=([0-9]{1,9}) lastFrame=([0-9]{1,7})$'

MAXV_ALL=0
DIFF_COUNT=0
STEALS_ALL=0

# read_terminator <schedule> — file-byte terminator judgment (iter 84):
# the terminator must match the full grammar, be the FINAL line, occur
# exactly once, and the file must be newline-terminated. Sets
# sched_plays/sched_stops/sched_last.
read_terminator() {
  local sd="$1" tline c
  test -s "$sd" || grammar_die "schedule $sd missing or empty"
  if [ -n "$(tail -c 1 "$sd")" ]; then
    grammar_die "schedule $sd lacks the trailing newline (torn write)"
  fi
  c="$(count_e "$sd" "$term_re")"
  [ "$c" = 1 ] || grammar_die "schedule $sd carries $c terminator-grammar lines, want exactly 1"
  c="$(count_e "$sd" '^SNDEV ')"
  [ "$c" = 1 ] || grammar_die "schedule $sd carries $c SNDEV-resembling lines, want exactly 1"
  tline="$(tail -1 "$sd")"
  [[ "$tline" =~ $term_re ]] || grammar_die "schedule $sd: final line is not the terminator: '$tline'"
  sched_plays="${BASH_REMATCH[1]}"; sched_stops="${BASH_REMATCH[2]}"
  sched_last="${BASH_REMATCH[3]}"
}

# run_one <id> <manifest-dir(oracle|m4)> — replay+tap (STREAM MATCH),
# render C x2 + reference (unlimited + capped), bit-diff, exposure pins.
run_one() {
  local id=$1 src=$2 manifest tdir frozen
  if [ "$src" = "oracle" ]; then
    manifest=oracle/goldens/manifest.json; tdir=oracle/goldens
  else
    manifest=$M4G/manifest.json; tdir=$M4G
  fi
  read_golden "$manifest" "$id"
  frozen="$tdir/$G_NAME.sha256.json"
  test -f "$frozen" || fail "$id: frozen stream missing: $frozen"
  echo "== $id ($G_NAME)"
  rm -f "$MF/$id.trace.txt"
  node "$SIM/trace-to-txt.js" "$tdir/$G_TRACE" "$MF/$id.trace.txt" >/dev/null
  made "$MF/$id.trace.txt"
  cpu_args=()
  if [ "$G_CPU" = "true" ]; then
    case " ${BRIDGE_CPU_IDS[*]} " in
      (*" $id "*) cpu_args=(--cpu --difficulty "$G_DIFF" --ai-bridge "$CAL/build/$id.ai-bridge.txt");;
      (*)         cpu_args=(--cpu --difficulty "$G_DIFF");;  # live C AI
    esac
  fi
  rm -f "$MF/$id.sndev.txt" "$MF/$id.sim-out.txt" "$MF/$id.sim-run.json"
  ML_SND_EVENTS_OUT="$MF/$id.sndev.txt" "$MF/sim_host_snd" \
    --trace "$MF/$id.trace.txt" --simdata "$CAL/build/simdata.txt" \
    --seed "$G_SEED" --p1 "$G_P1" --p2 "$G_P2" --stage "$G_STAGE" \
    --frames "$G_FRAMES" ${cpu_args[@]+"${cpu_args[@]}"} \
    > "$MF/$id.sim-out.txt" || fail "$id: sim_host_snd replay failed"
  made "$MF/$id.sndev.txt" "$MF/$id.sim-out.txt"
  node "$SIM/wrap-run.js" "$id" "$MF/$id.sim-out.txt" \
    "$MF/$id.sim-run.json" "$manifest" || fail "$id: wrap-run failed"
  made "$MF/$id.sim-run.json"
  node oracle/harness/verify-stream.js "$MF/$id.sim-run.json" "$frozen" \
    || fail "$id: tapped replay does NOT stream-match the frozen golden"
  # terminator (strict, file bytes): counts + lastFrame == the golden's
  read_terminator "$MF/$id.sndev.txt"
  [ "$sched_last" = "$G_FRAMES" ] || fail "$id: schedule lastFrame != frames"
  # C render x2 (byte-stable) + the independent reference + bit-diff
  rm -f "$MF/$id.c.pcm" "$MF/$id.c2.pcm" "$MF/$id.rend.txt"
  "$MF/snd_render" --pack "$MF/sndpack.bin" --events "$MF/$id.sndev.txt" \
    --frames "$G_FRAMES" --out "$MF/$id.c.pcm" > "$MF/$id.rend.txt" \
    || fail "$id: snd_render failed"
  "$MF/snd_render" --pack "$MF/sndpack.bin" --events "$MF/$id.sndev.txt" \
    --frames "$G_FRAMES" --out "$MF/$id.c2.pcm" > /dev/null \
    || fail "$id: snd_render (2nd) failed"
  made "$MF/$id.c.pcm" "$MF/$id.c2.pcm" "$MF/$id.rend.txt"
  cmp "$MF/$id.c.pcm" "$MF/$id.c2.pcm" || fail "$id: C render not byte-stable x2"
  rm -f "$MF/$id.c2.pcm"
  # the reference, BOTH ways: unlimited (browser truth / concurrency
  # measurement) and capped-8 (the device contract; the binding diff)
  rm -f "$MF/$id.refu.pcm" "$MF/$id.refu.txt" "$MF/$id.ref8.pcm" "$MF/$id.ref8.txt"
  node "$GFX/snd_reference.js" --audio "$AUDIO_OUT" \
    --events "$MF/$id.sndev.txt" --frames "$G_FRAMES" \
    --out "$MF/$id.refu.pcm" > "$MF/$id.refu.txt" \
    || fail "$id: snd_reference (unlimited) failed"
  node "$GFX/snd_reference.js" --audio "$AUDIO_OUT" \
    --events "$MF/$id.sndev.txt" --frames "$G_FRAMES" --voices 8 \
    --out "$MF/$id.ref8.pcm" > "$MF/$id.ref8.txt" \
    || fail "$id: snd_reference (capped) failed"
  made "$MF/$id.refu.pcm" "$MF/$id.refu.txt" "$MF/$id.ref8.pcm" "$MF/$id.ref8.txt"
  # verdict files judged from FILE BYTES (single line, newline, exactly
  # one full-grammar + one resembling line — iter 84 M4)
  judge_verdict_file "$MF/$id.rend.txt" "$rend_re" "$rend_resem"
  judge_verdict_file "$MF/$id.refu.txt" "$ref_re" "$ref_resem"
  judge_verdict_file "$MF/$id.ref8.txt" "$ref_re" "$ref_resem"
  rline="$(cat "$MF/$id.rend.txt")"
  [[ "$rline" =~ $rend_re ]] || grammar_die "$id: snd-render verdict re-read failed: '$rline'"
  c_plays="${BASH_REMATCH[2]}"; c_stops="${BASH_REMATCH[3]}"
  c_stopsm="${BASH_REMATCH[4]}"; c_stopsu="${BASH_REMATCH[5]}"
  c_steals="${BASH_REMATCH[6]}"
  uline="$(cat "$MF/$id.refu.txt")"
  [[ "$uline" =~ $ref_re ]] || grammar_die "$id: snd-ref (unlimited) verdict re-read failed: '$uline'"
  u_plays="${BASH_REMATCH[2]}"; u_stops="${BASH_REMATCH[3]}"
  u_stopsm="${BASH_REMATCH[4]}"; u_stopsu="${BASH_REMATCH[5]}"
  u_maxv="${BASH_REMATCH[6]}"; u_steals="${BASH_REMATCH[7]}"
  cline="$(cat "$MF/$id.ref8.txt")"
  [[ "$cline" =~ $ref_re ]] || grammar_die "$id: snd-ref (capped) verdict re-read failed: '$cline'"
  r8_stopsm="${BASH_REMATCH[4]}"; r8_stopsu="${BASH_REMATCH[5]}"
  r8_steals="${BASH_REMATCH[7]}"
  [ "$u_steals" = 0 ] || fail "$id: unlimited reference reported steals"
  [ "$c_plays" = "$sched_plays" ] && [ "$u_plays" = "$sched_plays" ] \
    || fail "$id: play counts disagree (sched=$sched_plays C=$c_plays ref=$u_plays)"
  [ "$c_stops" = "$sched_stops" ] && [ "$u_stops" = "$sched_stops" ] \
    || fail "$id: stop counts disagree (sched=$sched_stops C=$c_stops ref=$u_stops)"
  # THE BINDING DIFFERENTIAL: C == capped reference, every golden
  cmp "$MF/$id.c.pcm" "$MF/$id.ref8.pcm" \
    || fail "$id: RENDER DIFFERENTIAL DIVERGED (C mixer vs capped independent reference)"
  [ "$c_steals" = "$r8_steals" ] \
    || fail "$id: steal counts disagree (C=$c_steals capped-ref=$r8_steals)"
  # the matched/unmatched stop split must agree C vs capped reference
  # (independent counts of the same documented semantics — iter 84 H)
  [ "$c_stopsm" = "$r8_stopsm" ] && [ "$c_stopsu" = "$r8_stopsu" ] \
    || fail "$id: stop split disagrees (C=$c_stopsm/$c_stopsu capped-ref=$r8_stopsm/$r8_stopsu)"
  if [ "$u_maxv" -le 8 ]; then
    # <=8 domain: the unlimited (cap-free) reference must ALSO match —
    # the stronger independence check — and nothing may have stolen
    [ "$c_steals" = 0 ] || fail "$id: steals in a <=8-voice schedule"
    [ "$c_stopsm" = "$u_stopsm" ] && [ "$c_stopsu" = "$u_stopsu" ] \
      || fail "$id: stop split disagrees with the unlimited reference (C=$c_stopsm/$c_stopsu ref=$u_stopsm/$u_stopsu)"
    cmp "$MF/$id.c.pcm" "$MF/$id.refu.pcm" \
      || fail "$id: C != unlimited reference despite <=8 concurrency"
    echo "   $id: STREAM MATCH + C x2 stable + C==ref (capped AND unlimited) BIT-IDENTICAL (plays=$sched_plays stops=$sched_stops matched=$c_stopsm maxvoices=$u_maxv)"
  else
    # >8 domain (the pinned {g06,m02} class): the capped diff is
    # binding; the unlimited delta is the REGISTERED device-vs-browser
    # exposure (frozen in EXPOSURE_PINS — never a silent basis switch)
    [ "$c_steals" -ge 1 ] || fail "$id: concurrency $u_maxv > 8 but no steal fired"
    echo "   $id: STREAM MATCH + C x2 stable + C==capped-ref BIT-IDENTICAL; PINNED EXPOSURE: peak $u_maxv voices, $c_steals steal(s) vs browser-unlimited (registered, AGENT-LOG iter 82/84)"
  fi
  # the frozen per-golden exposure row (iter 84, review-82 High #2)
  judge_exposure "$id" "$u_maxv" "$c_steals" "$sched_stops" "$c_stopsm" "$c_stopsu"
  [ "$u_maxv" -gt "$MAXV_ALL" ] && MAXV_ALL=$u_maxv
  STEALS_ALL=$((STEALS_ALL + c_steals))
  DIFF_COUNT=$((DIFF_COUNT + 1))
  rm -f "$MF/$id.c.pcm" "$MF/$id.refu.pcm" "$MF/$id.ref8.pcm" # Nintendo-derived; keep the dir lean
}

echo "== [4/6] the offline render differential ($DIFF_COUNT_PIN goldens) =="
for id in "${ORACLE_IDS[@]}"; do
  run_one "$id" oracle
done
for id in "${M4_IDS[@]}"; do
  run_one "$id" m4
done
[ "$DIFF_COUNT" = "$DIFF_COUNT_PIN" ] || fail "differential covered $DIFF_COUNT goldens, want $DIFF_COUNT_PIN"
[ "$MAXV_ALL" = "$MAXV_ALL_PIN" ] || fail "aggregate maxvoices $MAXV_ALL != pinned $MAXV_ALL_PIN"
[ "$STEALS_ALL" = "$STEALS_ALL_PIN" ] || fail "aggregate steals $STEALS_ALL != pinned $STEALS_ALL_PIN"

# --- [4b] stop-path witnesses + liveness (the s01 coverage contract) -------
echo "== [4b] s01 stop-path witnesses =="
SD=$MF/s01.sndev.txt
sbc_stops=$(count_e "$SD" '^S [0-9]+ shieldbreakercharge\.stop 1 [0-9a-f]{16}$')
fl_stops=$(count_e "$SD" '^S [0-9]+ furaloop\.stop 1 [0-9a-f]{16}$')
fl_plays=$(count_e "$SD" '^P [0-9]+ furaloop$')
breaks=$(count_e "$SD" '^P [0-9]+ shieldbreak$')
kills=$(count_e "$SD" '^P [0-9]+ kill$')
noid_stops=$(count_e "$SD" '^S [0-9]+ [0-9A-Za-z]+\.stop 0 ')
[ "$sbc_stops" = 2 ] || fail "s01 shieldbreakercharge.stop count $sbc_stops != 2"
[ "$fl_stops" = 2 ] || fail "s01 furaloop.stop count $fl_stops != 2"
[ "$fl_plays" = 2 ] || fail "s01 furaloop play count $fl_plays != 2"
[ "$breaks" = 2 ] || fail "s01 shieldbreak count $breaks != 2"
[ "$kills" = 1 ] || fail "s01 kill count $kills != 1"
[ "$noid_stops" = 0 ] || fail "s01 carries $noid_stops id-less stops (all four arms pass ids)"
# the EXACT four-arm witness binding (iter 84, review-82 High #1)
judge_witnesses "$SD" "${S01_WITNESSES[@]}"
echo "   s01: four stop arms witness-bound (NSG release 191 / NSG auto 697 / hitdet-FURAFURA 701 / FURAFURA-wake 1419), 2 breaks, 2 furaloop episodes, 1 KO"

# --- [5] the real-app leg (gfx_app headless, mixer attached) --------------
echo "== [5/6] gfx_app leg (s01 through the live sink path) =="
read_golden "$M4G/manifest.json" s01
rm -f "$MF/s01.app-out.txt" "$MF/s01.app-tim.txt" "$MF/s01.app-log.txt" \
      "$MF/s01.app-run.json"
"$MF/gfx_app_headless" \
  --trace "$MF/s01.trace.txt" --simdata "$CAL/build/simdata.txt" \
  --gfxdata "$GFX/gfxdata-frozen.txt" --vfxdata "$GFX/vfxdata-frozen.txt" \
  --glyphs "$GFX/vfxglyphs-frozen.txt" --anim-dir "$TABLES" \
  --seed "$G_SEED" --p1 "$G_P1" --p2 "$G_P2" --stage "$G_STAGE" \
  --frames "$G_FRAMES" --pace 0 \
  --out "$MF/s01.app-out.txt" --timing "$MF/s01.app-tim.txt" \
  --sndpack "$MF/sndpack.bin" 2> "$MF/s01.app-log.txt" \
  || fail "gfx_app s01 replay failed"
made "$MF/s01.app-out.txt" "$MF/s01.app-tim.txt" "$MF/s01.app-log.txt"
node "$SIM/wrap-run.js" s01 "$MF/s01.app-out.txt" "$MF/s01.app-run.json" \
  "$M4G/manifest.json" || fail "gfx_app wrap failed"
made "$MF/s01.app-run.json"
node oracle/harness/verify-stream.js "$MF/s01.app-run.json" \
  "$M4G/$G_NAME.sha256.json" \
  || fail "gfx_app s01 stream mismatch (mixer attach perturbed the sim)"
au_re='^gfx_app audio: ([0-9]{1,12}) callbacks, ([0-9]{1,12}) underruns, ([0-9]{1,12}) badlen, ([0-9]{1,12}) voice starts, ([0-9]{1,12}) voice stops, ([0-9]{1,12}) steals, rate=0 samples=0 channels=0$'
[ "$(count_e "$MF/s01.app-log.txt" '^gfx_app audio: ')" = 1 ] || fail "gfx_app audio summary count != 1"
aline="$(grep -E '^gfx_app audio: ' "$MF/s01.app-log.txt")" || fail "no gfx_app audio summary"
[[ "$aline" =~ $au_re ]] || fail "gfx_app audio summary grammar: '$aline'"
app_starts="${BASH_REMATCH[4]}"; app_stops="${BASH_REMATCH[5]}"
# s01 schedule truth (from [4], file-byte re-read)
read_terminator "$SD"
[ "$app_starts" = "$sched_plays" ] || fail "gfx_app starts $app_starts != schedule $sched_plays"
[ "$app_stops" = "$sched_stops" ] || fail "gfx_app stops $app_stops != schedule $sched_stops"
echo "   gfx_app: STREAM MATCH; starts/stops == schedule ($app_starts/$app_stops)"

# --- [6] teeth (s01 render; every tooth must fire) -------------------------
echo "== [6/6] teeth =="
tooth_diff() { # <label> <extra renderer args...> — MUST diverge from ref
  lbl=$1; shift
  rm -f "$MF/tooth.pcm"
  "$MF/snd_render" --pack "$MF/sndpack.bin" --events "$SD" --frames 3600 \
    --out "$MF/tooth.pcm" "$@" > /dev/null || fail "tooth $lbl: renderer died"
  made "$MF/tooth.pcm"
  if cmp -s "$MF/tooth.pcm" "$MF/s01.ref.keep.pcm"; then
    fail "TOOTH $lbl DID NOT FIRE (output identical to the reference)"
  fi
  echo "   tooth $lbl fired (divergence detected)"
  rm -f "$MF/tooth.pcm"
}
# reference copy for tooth comparisons (regenerate once)
rm -f "$MF/s01.ref.keep.pcm"
node "$GFX/snd_reference.js" --audio "$AUDIO_OUT" --events "$SD" \
  --frames 3600 --out "$MF/s01.ref.keep.pcm" > /dev/null
made "$MF/s01.ref.keep.pcm"
# positive control: the clean render still matches
rm -f "$MF/clean.pcm"
"$MF/snd_render" --pack "$MF/sndpack.bin" --events "$SD" --frames 3600 \
  --out "$MF/clean.pcm" > /dev/null
cmp "$MF/clean.pcm" "$MF/s01.ref.keep.pcm" || fail "positive control broke"
rm -f "$MF/clean.pcm"
tooth_diff "T1-gain-nibble" --tooth-gain furaloop
tooth_diff "T2-dropped-stop" --tooth-drop-first-stop
tooth_diff "T3-step-skew" --tooth-step-skew
tooth_diff "T4-stop-id-skew" --tooth-stop-id-skew
# T4b (iter 84): the stop SPLIT sees the de-routing — a skewed id
# matches no play, so the C verdict must report stopsm=0 stopsu=4
# (proves the matched counter measures routing, not event arrival).
rm -f "$MF/tooth4b.pcm" "$MF/tooth4b.txt"
"$MF/snd_render" --pack "$MF/sndpack.bin" --events "$SD" --frames 3600 \
  --out "$MF/tooth4b.pcm" --tooth-stop-id-skew > "$MF/tooth4b.txt" \
  || fail "T4b renderer died"
t4b_c="$(count_e "$MF/tooth4b.txt" '^snd-render OK frames=3600 plays=[0-9]+ stops=4 stopsm=0 stopsu=4 ')"
[ "$t4b_c" = 1 ] || fail "T4b: skewed-id render did not report stopsm=0 stopsu=4 (the split counter must SEE de-routing)"
echo "   tooth T4b fired (stop split reports 0 matched / 4 unmatched under id skew)"
rm -f "$MF/tooth4b.pcm" "$MF/tooth4b.txt"
# T5 steal-policy flip, on g06 — the ONE schedule where a steal really
# fires (pinned, 9-voice peak): flipping to steal-newest must diverge
# from the capped reference (which implements the documented
# steal-oldest policy).
rm -f "$MF/g06.ref8.keep.pcm" "$MF/tooth5.pcm" "$MF/tooth5c.pcm"
node "$GFX/snd_reference.js" --audio "$AUDIO_OUT" --events "$MF/g06.sndev.txt" \
  --frames 3600 --voices 8 --out "$MF/g06.ref8.keep.pcm" > /dev/null
made "$MF/g06.ref8.keep.pcm"
"$MF/snd_render" --pack "$MF/sndpack.bin" --events "$MF/g06.sndev.txt" \
  --frames 3600 --out "$MF/tooth5.pcm" --tooth-steal-newest > /dev/null
if cmp -s "$MF/tooth5.pcm" "$MF/g06.ref8.keep.pcm"; then
  fail "T5: steal-policy flip did NOT diverge on g06 (the schedule with a live steal)"
fi
# positive control: the unflipped render still matches
"$MF/snd_render" --pack "$MF/sndpack.bin" --events "$MF/g06.sndev.txt" \
  --frames 3600 --out "$MF/tooth5c.pcm" > /dev/null
cmp "$MF/tooth5c.pcm" "$MF/g06.ref8.keep.pcm" || fail "T5 positive control broke"
echo "   tooth T5-steal-flip fired on g06 (divergence detected; positive control clean)"
rm -f "$MF/tooth5.pcm" "$MF/tooth5c.pcm" "$MF/g06.ref8.keep.pcm"
# T6 grammar corruption: dropped terminator + malformed line — BOTH
# renderers must die nonzero (fail closed)
sed '$d' "$SD" > "$MF/tooth6a.txt"
if "$MF/snd_render" --pack "$MF/sndpack.bin" --events "$MF/tooth6a.txt" \
  --frames 3600 --out "$MF/t6.pcm" > /dev/null 2>&1; then
  fail "T6a: C renderer accepted a terminator-less schedule"
fi
if node "$GFX/snd_reference.js" --audio "$AUDIO_OUT" \
  --events "$MF/tooth6a.txt" --frames 3600 --out "$MF/t6.pcm" \
  > /dev/null 2>&1; then
  fail "T6a: reference accepted a terminator-less schedule"
fi
{ head -1 "$SD"; echo "P xx bogus!"; tail -n +2 "$SD"; } > "$MF/tooth6b.txt"
if "$MF/snd_render" --pack "$MF/sndpack.bin" --events "$MF/tooth6b.txt" \
  --frames 3600 --out "$MF/t6.pcm" > /dev/null 2>&1; then
  fail "T6b: C renderer accepted a malformed line"
fi
if node "$GFX/snd_reference.js" --audio "$AUDIO_OUT" \
  --events "$MF/tooth6b.txt" --frames 3600 --out "$MF/t6.pcm" \
  > /dev/null 2>&1; then
  fail "T6b: reference accepted a malformed line"
fi
echo "   tooth T6 fired (both renderers fail closed on grammar corruption)"
rm -f "$MF/tooth6a.txt" "$MF/tooth6b.txt" "$MF/t6.pcm" "$MF/s01.ref.keep.pcm"

# --- no-commit guard (rc CASE-SPLIT — iter 84; build output incl. -----------
#     Nintendo-derived PCM is never tracked, and the golden contract
#     artifacts must be clean for this evidence to mean anything) ------------
rc=0
dirty="$(git status --porcelain -- "$B" "$AUDIO_OUT" \
  "$M4G/manifest.json" "$M4G/*.trace.json" "$M4G/*.sha256.json")" || rc=$?
if [ "$rc" -ne 0 ]; then
  fail "no-commit guard — git status rc $rc (a status read error is CORRUPT evidence, never a clean pass)"
fi
if [ -n "$dirty" ]; then
  echo "MIXER FIDELITY FAIL: build output not gitignored (or the golden contract dirtied):" >&2
  printf '%s\n' "$dirty" >&2
  exit 1
fi

echo "MIXER FIDELITY OK (goldens=$DIFF_COUNT diff=bit-identical maxvoices=$MAXV_ALL steals=$STEALS_ALL s01stops=4)"
