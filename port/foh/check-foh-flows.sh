#!/usr/bin/env bash
# check-foh-flows.sh — M4 task 9 done-check: FOH core + menu flows, HOST
# (fix_plan §M4 task 9; pre-registration AGENT-LOG iter 88; hardened
# iter 90 per the review-88 round-1 triage — .loop/review-88-triage.md;
# hardened iter 91 per the review-90 round-2 triage —
# .loop/review-90-triage.md; hardened iter 92 per the review-91
# round-3 findings — .loop/review-91-1.log; §M4 conventions' menu
# verification approach (a)-(c) host-side).
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
#       flows/<id>.expect + A==B byte-stability + per-shot judgments
#       through the PRODUCTION judge_shot_pair (review-88 M5/M6: exact
#       P6 header bytes, payload == 240*240*3 with no trailing bytes,
#       non-blank, A==B) + pairwise distinctness (the pinned shot
#       inventory EXACT-SET on BOTH runs' dirs — review-90 M2: an extra
#       run-B file is death; dotfile-inclusive, review-91 L —
#       judge_shot_inventory).
#   [4] MATCH-LAUNCH BRIDGES (full-stream-judged: the HONEST bridge
#       count, review-88 H1): f01 -> frozen g01, f02 (live C AI, no
#       AIBRIDGE1) -> frozen m01, f05 -> frozen g03 (p2Char != 0
#       stream-load-bearing, review-88 M2) — FULL 3600-frame equality
#       via wrap-run.js -> UNCHANGED verify-stream.js, each wrapped run
#       re-validated structurally (validate_run_shape, review-90 H(b)),
#       each verify log captured stdout+stderr (verify_capture,
#       review-90 M1) and judged BYTE-EXACT against a verdict line
#       constructed from the frozen file's own counts (whole-log
#       whitelist, review-88 M4);
#       f01/f02/f03/f05 BRIDGE-STATE witnesses cmp'd vs their frozen
#       .bstate.expect (settings/params read back FROM GameState —
#       f03 stays the STATE witness for the edited-settings plane).
#   [4w] DIVERGENCE WITNESS + CONTROL (review-88 H1; review-90 H): a
#       check-owned synthetic flow selects lcancel=1 through
#       options-gameplay then g01's exact chars/stage and replays g01's
#       trace through the FOH-fed settings plane; verify-stream vs the
#       frozen g01 stream MUST report a first divergence at the PINNED
#       frame (rc 2, 3-line exact grammar) whose frozen hash == the
#       frozen file's own frame entry and whose run hash == the
#       measured-then-frozen lcancel=1 TREATMENT PIN (review-90 H(c) —
#       arbitrary unequal hashes can no longer masquerade). A MATCH =
#       DEATH. Then the CONTROL (review-90 H(a)): the SAME flow with
#       ONLY the lcancel press deleted (derived mechanically) MUST
#       fully MATCH frozen g01 (rc 0, whole-log byte-exact) — an
#       options-path boot/serialization defect breaks the control, so
#       the witness divergence is attributable to lcancel itself.
#       review-91 H: BOTH legs share ONE flow id (wit-g01, sibling
#       dirs wit/ vs ctrl/); the control-trace derivation carries NO
#       header normalization — all-else-identical includes the
#       observable flow id, and tooth T15 proves the stream is
#       flow-id-independent every run.
#   [5] TEETH (standing, pre-registered T1-T10 AGENT-LOG iter 90 +
#       T11-T14 iter 91 + T15-T16 iter 92):
#       nav-perturb under the SAME flow header with an exact
#       first-divergent-line pair (L1), char variant, stream-judge
#       nibble (run-JSON side), trace grammar malformed + resembling,
#       shot corruption x3 through the production judge_shot_pair
#       (M5/M6), difficulty variant, witness fail-closed (H1), f04
#       sss->css edge variant (M1), f05 p2 variant (M2), verdict-log
#       corruption x2 (M4), witness treatment-pin corruption (r90 H),
#       control fed a divergence (r90 H), stderr-injection wrapper
#       through the production capture (r90 M1), unexpected.ppm planted
#       in a shots-b copy (r90 M2), stream flow-id independence — the
#       witness flow bytes under a renamed basename change ONLY the
#       trace header, stream + bstate byte-identical (r91 H), dotfile
#       .unexpected.ppm planted in a shots-b copy (r91 L). All operate
#       on GENERATED variants/copies — committed bytes are never
#       edited.
#   [6] hygiene: build outputs are git-ignored (rc case-split).
#
#   [5c] the C23 FOH sound-plane witness + the audio-bus audibility legs.
#   [5d] the CSS launch-guard grid witness (codex B1): the full
#        (p1Type,p2Type) grid over {-1,0,1} through the real foh_tick,
#        judged against an AUTHORED 9-row table — the refusing side of
#        the D6 guard that no committed flow can reach.
#
# Prints `FOH FLOWS OK (flows=7 shots=19 bridges=3 tbridges=2 states=4
# tstates=2 diverge=1 control=1 banner=1 snd=1 launch=1 teeth=40)`, exit 0; ANY divergence,
# off-graph transition, pin mismatch, count disagreement, or missing
# artifact -> nonzero. (iter 99, M4 task 12: flows 5->7 — the
# target-select screen + f06/f07 target bridges judged by BOTH
# verifiers; the [4t] leg; teeth 16->18. CSS mechanics arc, MENU-SPEC
# items 1+2+3+4: teeth 21->26 — T19 a CPU P1 is reachable on the CSS and
# refuses at the launch seam, T20 a too-short direction press is refused
# by the device translator rather than silently widened, T21 two
# intervals on one physical keysym are refused (T21b through a swapped keymap), T22 a token held across
# CSS->SSS->CSS un-readies the screen on return — plus a standing assert
# that all 7 committed flows translate to fk_input scripts.)
#
# HONEST EXPOSURE (PROCESS §8): the frozen traces prove the REWRITTEN
# machine's flow graph and selection semantics against the
# pre-registered upstream mapping (foh.h citations) — a shared
# misreading of upstream's menu code would be invisible here (the
# rewrite's exposure class; primary-source citations are the
# mitigation). Menu LOOK is not judged (rewritten surface; visual
# authority = Chase's acceptance playthrough; device look = task 10).
# The non-default-settings SIM behavior is M2's verified surface — f03
# proves the launch PLUMBING (BRIDGE-STATE witness) and the [4w]
# divergence witness proves the FOH-fed settings plane REACHES TICKING
# (lcancel=1 diverges the stream at the pinned frame). The d9/m02
# fragment of review-88 M2 is REFUSED WITH CITATION (AGENT-LOG iter 90:
# the upstream CSS slider domain is 1..4, css.js:326-327 — d9 is
# unreachable in upstream's own UI; f05 consumes g03 instead).
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
count_xl() { # FULL-LINE fixed-string count (grep -cxF; rc case-split)
  local c rc=0
  c="$(grep -cxF -- "$2" "$1")" || rc=$?
  if [ "$rc" -ge 2 ]; then
    grammar_die "count helper — grep -cxF rc $rc reading '$1'"
  fi
  printf '%s' "$c"
}

relay_lines() { sed 's/^/  | /'; }

host_sha256() { shasum -a 256 "$1" | cut -d' ' -f1; }

# --- [0-pre] persistence hermeticity (M4 task 13) ----------------------------
# foh_app now loads/saves the persisted plane through the foh_persist
# chokepoint at boot + the options B-exit. EVERY foh_app invocation in
# this check gets a FRESH persist dir so flows start from the authored
# defaults (the frozen expectations' domain) and no leg's save leaks
# into another (the wit leg saves lcancel=1 on its B-exit — a shared
# dir would poison the control). The chokepoint's loud
# `foh_persist: reset cause=missing` on each boot is the designed
# first-boot event (relayed, not judged here — the persist plane's own
# judges live in check-device-persist.sh).
export MLFK_PERSIST_DIR="$PWD/port/foh/build/check/persist"
fresh_persist() { rm -rf "$MLFK_PERSIST_DIR"; }

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
# HONEST LIMIT (Tier A+ round-6 MINOR-2): these are CHANGE detectors, never
# LOOSENING detectors — re-pinning a line here is one edit, and a judge that
# widens while its pin is refreshed passes this leg. What actually disagrees
# with judge-foh-trace.js from outside it is check-judge-regression.sh leg
# [0n]'s hand-authored port/foh/judge-domains.authored.txt, whose rows are not
# re-derivable from the judge. Read that leg's header before trusting this one.
PRODUCER_PINS="\
b835b5f886225e0015dae152576eea5a42fa69d7ba0699f4de0e31438d05c5b9 port/sim/sim/wrap-run.js
f420723433b19166b53a80aedf54931ffdfbc6d2505c773fd73b7a13bbcdf60e oracle/harness/verify-stream.js
4160a35b36e8d3d6896ad2c3c6239d4a4860a0d7f43814a7a9b53b7c136742ab port/sim/sim/trace-to-txt.js
7186734f8c3ff9bfad04f59bf9e13f201663e82481e399911433136673721bba port/sim/calib/dump-sim-data.js
8658ae0b23d2d853605f5495fe0fa02b02b645b70aee47944bdb5503a34e10e4 port/foh/judge-foh-trace.js
2cf5c5a532207372b70c4cee57412c7ac65643ac4f4066c745d9eb7fe4aa0e9b port/goldens-m4/wrap-target.js
415335239fcc04df97eba07298a1fa521602d5ea45b087aa8d7d40bd740c122a port/goldens-m4/verify-target-stream.js
6b1b6b5be3700c51dfae8c0c4cb1f012e5b61239394ae4146c2e5e19cc4fcc47 port/goldens-m4/validate-target-manifest.js
624956898890e749170a4768af0f8ef86e05ce4dd75046d084701747c9d9121f port/goldens-m4/json-dup-key-scan.js"
N_PINS_WANT=9
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
FLOW_IDS=(f01-vs-g01 f02-cpu-m01 f03-options f04-nav f05-vs-g03 \
          f06-target-t01 f07-target-t02)
# bridge mode per flow (positional): verify=stream-judged launch,
# state=GameState witness only, none=no launch, tverify=target-launch
# with BOTH streams judged (iter 99, M4 task 12)
FLOW_BRIDGE=(verify verify state none verify tverify tverify)
# pinned shot inventory per flow (space-joined; both directions below)
FLOW_SHOTS=("startup title menu-top css sss" \
            "css-cpu sss-ystory" \
            "options-audio options-gameplay options-edited" \
            "controls-keyboard" \
            "css-p2 sss-pstadium" \
            "menu-targettest tss-t01" \
            "tss-addcode tss-t02")
[ "${#FLOW_IDS[@]}" = 7 ] || fail "flow inventory — pinned array length off"
[ "${#FLOW_BRIDGE[@]}" = 7 ] || fail "flow bridge array length off"
[ "${#FLOW_SHOTS[@]}" = 7 ] || fail "flow shots array length off"
globbed="$(ls "$FLOWS"/*.flow | sed 's|.*/||; s|\.flow$||' | sort | tr '\n' ' ' | sed 's/ $//')"
pinned="$(printf '%s\n' "${FLOW_IDS[@]}" | sort | tr '\n' ' ' | sed 's/ $//')"
if [ "$globbed" != "$pinned" ]; then
  fail "flow inventory pin — flows/*.flow {$globbed} != pinned {$pinned} (both directions; a new or dropped flow is a reviewed pin update)"
fi
for k in 0 1 2 3 4 5 6; do
  id="${FLOW_IDS[$k]}"
  made "$FLOWS/$id.flow" "$FLOWS/$id.expect"
  if [ "${FLOW_BRIDGE[$k]}" != "none" ]; then
    made "$FLOWS/$id.bstate.expect"
  fi
done
# target-manifest params (iter 99): the SHARED strict validator FIRST,
# then t01/t02 rows for the tverify legs (single param source).
node "$M4G/validate-target-manifest.js" >/dev/null 2>&1 \
  || fail "manifest-target.json failed the shared strict validator"
tline="$(node -e '
  const v = require("./port/goldens-m4/validate-target-manifest");
  const m = v.loadValidatedManifest();
  for (const id of ["t01", "t02"]) {
    const g = v.goldenByIdOrName(m, id);
    console.log([g.name, g.trace, g.frames, g.seed, g.char, g.tstage,
      g.minTargets].join(" "));
  }
')" || fail "cannot pull t01/t02 params from the target manifest"
read -r T01_NAME T01_TRACE T01_FRAMES T01_SEED T01_CHAR T01_TSTAGE T01_MIN <<< "$(sed -n 1p <<< "$tline")"
read -r T02_NAME T02_TRACE T02_FRAMES T02_SEED T02_CHAR T02_TSTAGE T02_MIN <<< "$(sed -n 2p <<< "$tline")"
for tv in "$T01_FRAMES" "$T01_SEED" "$T01_CHAR" "$T01_TSTAGE" "$T01_MIN" \
          "$T02_FRAMES" "$T02_SEED" "$T02_CHAR" "$T02_TSTAGE" "$T02_MIN"; do
  [[ "$tv" =~ ^(0|[1-9][0-9]{0,11})$ ]] || fail "target manifest param grammar ('$tv')"
done

# --- [0d] bridge param CROSS-BIND: frozen LAUNCH lines == manifest params ----
# UNCHANGED by the CSS mechanics arc: p1type/p1difficulty are machine state
# and appear as S events, but never on this line — foh.c refuses to launch any
# port configuration the launch plane cannot honour (sim_setup_match pins a
# human port 0), so the record's shape is provably the same one.
LAUNCH_RE='^LAUNCH [0-9]+ p1=[0-4] p2=[0-4] p2type=[01] difficulty=[1-4] stage=[0-5] turbo=[01] lcancel=[012] flashlcancel=[01] walljump=[01] tapjump=[01],[01],[01],[01] versus=0$'
for exf in "$FLOWS/f01-vs-g01.expect" "$FLOWS/f02-cpu-m01.expect" \
           "$FLOWS/f05-vs-g03.expect"; do
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
g03line="$(node -e '
  const m = JSON.parse(require("fs").readFileSync("oracle/goldens/manifest.json", "utf8"));
  const g = m.goldens.find((x) => x.id === "g03");
  if (!g || g.cpu !== false) { console.error("g03 shape"); process.exit(1); }
  console.log([g.seed, g.p1, g.p2, g.stage, g.frames, g.trace].join(" "));
')" || fail "cannot parse g03 params from the oracle manifest"
read -r G03_SEED G03_P1 G03_P2 G03_STAGE G03_FRAMES G03_TRACE <<< "$g03line"
[ "$(get_launch_field "$FLOWS/f01-vs-g01.expect" p1)" = "$G01_P1" ] || fail "cross-bind — f01 LAUNCH p1 != g01 manifest p1"
[ "$(get_launch_field "$FLOWS/f01-vs-g01.expect" p2)" = "$G01_P2" ] || fail "cross-bind — f01 LAUNCH p2 != g01 manifest p2"
[ "$(get_launch_field "$FLOWS/f01-vs-g01.expect" stage)" = "$G01_STAGE" ] || fail "cross-bind — f01 LAUNCH stage != g01 manifest stage"
[ "$(get_launch_field "$FLOWS/f01-vs-g01.expect" p2type)" = 0 ] || fail "cross-bind — f01 LAUNCH p2type != 0 (g01 is human/human)"
[ "$(get_launch_field "$FLOWS/f02-cpu-m01.expect" p1)" = "$M01_P1" ] || fail "cross-bind — f02 LAUNCH p1 != m01 manifest p1"
[ "$(get_launch_field "$FLOWS/f02-cpu-m01.expect" p2)" = "$M01_P2" ] || fail "cross-bind — f02 LAUNCH p2 != m01 manifest p2"
[ "$(get_launch_field "$FLOWS/f02-cpu-m01.expect" stage)" = "$M01_STAGE" ] || fail "cross-bind — f02 LAUNCH stage != m01 manifest stage"
[ "$(get_launch_field "$FLOWS/f02-cpu-m01.expect" p2type)" = 1 ] || fail "cross-bind — f02 LAUNCH p2type != 1 (m01 is CPU)"
[ "$(get_launch_field "$FLOWS/f02-cpu-m01.expect" difficulty)" = "$M01_DIFF" ] || fail "cross-bind — f02 LAUNCH difficulty != m01 manifest difficulty"
[ "$(get_launch_field "$FLOWS/f05-vs-g03.expect" p1)" = "$G03_P1" ] || fail "cross-bind — f05 LAUNCH p1 != g03 manifest p1"
[ "$(get_launch_field "$FLOWS/f05-vs-g03.expect" p2)" = "$G03_P2" ] || fail "cross-bind — f05 LAUNCH p2 != g03 manifest p2 (p2Char load-bearing, review-88 M2)"
[ "$G03_P2" != 0 ] || fail "cross-bind — g03 manifest p2 is 0; f05 exists to make p2Char != 0 load-bearing"
[ "$(get_launch_field "$FLOWS/f05-vs-g03.expect" stage)" = "$G03_STAGE" ] || fail "cross-bind — f05 LAUNCH stage != g03 manifest stage"
[ "$(get_launch_field "$FLOWS/f05-vs-g03.expect" p2type)" = 0 ] || fail "cross-bind — f05 LAUNCH p2type != 0 (g03 is human/human)"
# Every bridged golden has a HUMAN port 0, and the launch plane only supports
# that (sim_setup_match pins types[0]=0). foh.c enforces it with a loud
# `refused portconfig` arm, and T19 below proves the arm fires — so the frozen
# traces must carry exactly zero such refusals.
for exf in "$FLOWS/f01-vs-g01.expect" "$FLOWS/f02-cpu-m01.expect" \
           "$FLOWS/f05-vs-g03.expect"; do
  c="$(count_e "$exf" '^S [0-9]+ refused portconfig$')"
  [ "$c" = 0 ] || fail "cross-bind — $exf carries $c portconfig refusals; a bridged flow must launch cleanly"
done
# TLAUNCH cross-bind (iter 99): the frozen target-flow launch records
# must equal the target-manifest rows (no independent literals).
TLAUNCH_RE='^TLAUNCH [0-9]+ char=[0-4] tstage=[0-9]$'
for exf in "$FLOWS/f06-target-t01.expect" "$FLOWS/f07-target-t02.expect"; do
  c="$(count_e "$exf" "$TLAUNCH_RE")"
  [ "$c" = 1 ] || grammar_die "frozen $exf: $c TLAUNCH lines match the anchored grammar, want exactly 1"
done
get_tlaunch_field() { # <file> <key>
  grep -E "$TLAUNCH_RE" "$1" | tr ' ' '\n' | grep -E "^$2=" | cut -d= -f2
}
[ "$(get_tlaunch_field "$FLOWS/f06-target-t01.expect" char)" = "$T01_CHAR" ] || fail "cross-bind — f06 TLAUNCH char != t01 manifest char"
[ "$(get_tlaunch_field "$FLOWS/f06-target-t01.expect" tstage)" = "$T01_TSTAGE" ] || fail "cross-bind — f06 TLAUNCH tstage != t01 manifest tstage"
[ "$(get_tlaunch_field "$FLOWS/f07-target-t02.expect" char)" = "$T02_CHAR" ] || fail "cross-bind — f07 TLAUNCH char != t02 manifest char"
[ "$(get_tlaunch_field "$FLOWS/f07-target-t02.expect" tstage)" = "$T02_TSTAGE" ] || fail "cross-bind — f07 TLAUNCH tstage != t02 manifest tstage"
echo "[0] pins OK: producers 9, manifests 8+4+target, flows 7, LAUNCH cross-bind g01/m01/g03 + TLAUNCH t01/t02"

# --- [1] fresh data planes ---------------------------------------------------
echo "=== [1] data planes (fresh tables + simdata x2)"
rm -rf "$TABLES"
{ bash pipeline/extractor/build-extractor.sh; } 2>&1 | relay_lines
{ node pipeline/run.js --only animations,tables,stages,targets,assets --out "$TABLES"; } 2>&1 | relay_lines
made "$TABLES/ml_tables.c" "$TABLES/ml_stages.c" "$TABLES/ml_tables.h" \
     "$TABLES/ml_stages.h" "$TABLES/ml_targets.c" "$TABLES/ml_targets.h"
# A1 restyle Phase 1: the CSS/SSS renders consume the `assets` stage's IMG1
# menu artwork (port/gfx/img1.h). foh_render resolves it from MLFK_MENU_IMG1
# first (art_load), and a MISSING file is fatal there by design — a target
# that silently rendered the portrait-less CSS would diverge from its twin.
# Pointing it at THIS run's freshly regenerated file keeps the check hermetic
# for the artwork plane exactly as it already is for tables/stages/targets.
# (The artifact's own byte-stability + sha pins live in
# pipeline/check-assets.sh; they are not re-derived here.)
made "$TABLES/assets/menu.img1"
export MLFK_MENU_IMG1="$PWD/$TABLES/assets/menu.img1"
rm -rf "$B"
mkdir -p "$B"
node "$CAL/dump-sim-data.js" --out "$B/simdata.txt" 2>&1 | relay_lines
node "$CAL/dump-sim-data.js" --out "$B/simdata2.txt" 2>&1 | relay_lines
made "$B/simdata.txt" "$B/simdata2.txt"
cmp "$B/simdata.txt" "$B/simdata2.txt" || fail "simdata not byte-identical across two fresh dumps"
echo "    simdata byte-identical across two fresh dumps"

# --- [0j] JUDGE-PATH REGRESSION (PROCESS §3 Tier A+; driver 2026-07-29) ------
# This arc modified judge-foh-trace.js and normalize-foh-trace.js — the JUDGE
# PATH. A judge that silently gets LOOSER keeps printing OK while proving less,
# so a judge change owes an archived old-vs-new regression on top of the normal
# arc. Composed in HERE rather than left standalone so it cannot rot: it runs
# on every flows check, against a PINNED pre-arc commit (not HEAD, which would
# self-destruct the moment this lane merges).
echo "=== [0j] judge-path old-vs-new regression"
JUDGEREG_OK='JUDGE REGRESSION OK (corpus=7 pairs=24 identical=4 moved=10 rejects=5 negs=26 reaccept=5 norms=3 normmoved=4 normrej=5 tables=26)'
rc=0
bash "$FOH/check-judge-regression.sh" > "$B/judgereg.out" 2>&1 || rc=$?
relay_lines < "$B/judgereg.out"
[ "$rc" = 0 ] || fail "judge-path regression failed (rc $rc) — see relayed output above"
# Anchored FULL-LINE verdict with its counts pinned: a regression whose corpus
# or intended-movement table silently shrank would still print OK, so the
# SHAPE of the result is pinned too, not just its success.
[ "$(count_xl "$B/judgereg.out" "$JUDGEREG_OK")" = 1 ] \
  || fail "judge-path regression did not print the exact anchored verdict '$JUDGEREG_OK' exactly once (its corpus or movement table moved — re-measure and re-pin deliberately)"
echo "    [0j] OK: untouched flows byte-identical (judge + normalizer), all movement enumerated, re-frozen expects accepted"

# --- [0m] CONTROL-ROLE TRUTH TABLE: one predicate, both copies compared,
# every style x Mod arrangement COMPILED and pinned ------------------------
# The Controls screen renders its X/Y and L/R action labels from the ACTIVE
# style, so it needs the C-layer predicate that port/gfx/s1_input.h:162
# defines (`ctl_style_has_clayer`). foh_render.c cannot include that header
# (it drags the sim input + platform planes into a UI TU) and the header is
# owned by another in-flight lane, so the truth table is restated ONCE in
# port/foh/foh_ctl_labels.h. review-r15 MAJOR: pinning only the DEFINITION
# site left the copy free to drift, and the frozen keyboard screenshot only
# ever exercises the fresh-install style (NATURAL), so the BOX and NORMAL
# label rows had no coverage at all. This leg closes both halves:
#   (a) the two predicate EXPRESSIONS are compared to each other, so neither
#       side can move alone;
#   (b) the label table is COMPILED and every style x Mod arrangement is
#       checked against the table authored below.
S1H=port/gfx/s1_input.h
LBLH=port/foh/foh_ctl_labels.h
for f in "$S1H" "$LBLH"; do
  [ -f "$f" ] || fail "[0m] $f missing — the control-role truth table has no definition site"
done
# (a) both copies of the predicate, reduced to a bare expression and compared.
s1expr="$(sed -n '/^static inline bool ctl_style_has_clayer(CtlStyle style) {$/,/^}$/p' "$S1H" \
  | sed -n '2p' | sed -e 's/^ *return //' -e 's/; *$//' | tr -s ' ')"
lblexpr="$(sed -n '/^static inline bool foh_ctl_has_clayer(CtlStyle style) {$/,/^}$/p' "$LBLH" \
  | sed -n '2p' | sed -e 's/^ *return //' -e 's/; *$//' | tr -s ' ')"
[ -n "$s1expr" ] || fail "[0m] could not extract ctl_style_has_clayer's body from $S1H (its shape moved)"
[ -n "$lblexpr" ] || fail "[0m] could not extract foh_ctl_has_clayer's body from $LBLH (its shape moved)"
[ "$s1expr" = "$lblexpr" ] \
  || fail "[0m] the two C-layer predicates disagree: $S1H says '$s1expr' but $LBLH says '$lblexpr' — they are the SAME truth table and must move together (or be collapsed onto one predicate once s1_input.h is free)"
# and the renderer must not grow a THIRD copy behind the header's back.
# (the pattern is the predicate's own SHAPE — it moved from
# `CTL_STYLE_BOX || ...` to `!= CTL_STYLE_BOX` with D32, and the guard moved
# with it. foh_render.c names CTL_STYLE_BOX nowhere at all, measured.)
c="$(grep -c 'CTL_STYLE_BOX' port/foh/foh_render.c)" || true
[ "$c" = 0 ] \
  || fail "[0m] port/foh/foh_render.c restates the C-layer predicate ($c site(s)) — it must call foh_ctl_labels() so there is exactly one copy to pin"
# (b) COMPILE the label table and pin all 3 styles x Mod-on-L/R. The expected
# table is AUTHORED here from the mapping rules (s1_input.h ctl_roles +
# s1_input_row_style), never dumped from the code under test. Re-authored
# 2026-08-24 for the owner's control re-ratification (DEVIATIONS D31/D32/D33 —
# "X->grab, A->jump, Y->special, B->attack", L-only shielding, grab on BOX):
#   A / B / X / Y : STYLE-INDEPENDENT now — jump / attack / grab (Z) / special
#           in every style, BOX included.
#   L / R : L shields everywhere except the BOX arrangement that puts Mod on it;
#           R is Mod in BOX (on the shoulder modOnR names) and the C-layer hold
#           in NORMAL and NATURAL. BOX has NO C-layer: with a Mod shoulder and
#           four spent face buttons there is no seventh gameplay button left.
cat > "$B/ctl_labels_probe.c" <<'PROBE'
#include "foh_ctl_labels.h"
#include <stdio.h>
int main(void) {
  static const char *const kStyle[CTL_STYLE_COUNT] = {"NORMAL", "BOX", "NATURAL"};
  for (int st = 0; st < CTL_STYLE_COUNT; st++) {
    for (int md = 0; md < 2; md++) {
      const char *out[FOH_CTL_LABEL_ROWS];
      foh_ctl_labels((CtlStyle)st, md != 0, out);
      for (int i = 0; i < FOH_CTL_LABEL_ROWS; i++)
        printf("%s mod=%d %d %s\n", kStyle[st], md, i, out[i]);
    }
  }
  return 0;
}
PROBE
cc -O0 -ffp-contract=off -Wall -Wextra -Werror -I port/foh -o "$B/ctl_labels_probe" \
   "$B/ctl_labels_probe.c" >"$B/ctl_labels_probe.cc.log" 2>&1 \
  || { sed -n '1,20p' "$B/ctl_labels_probe.cc.log"; fail "[0m] the label table does not compile standalone (see $B/ctl_labels_probe.cc.log)"; }
"$B/ctl_labels_probe" > "$B/ctl_labels.got" 2>"$B/ctl_labels.err" \
  || fail "[0m] the label probe exited nonzero"
cat > "$B/ctl_labels.want" <<'WANT'
NORMAL mod=0 0 CONTROL STICK
NORMAL mod=0 1 JUMP
NORMAL mod=0 2 ATTACK
NORMAL mod=0 3 GRAB
NORMAL mod=0 4 SPECIAL
NORMAL mod=0 5 SHIELD
NORMAL mod=0 6 C-STICK (HOLD)
NORMAL mod=0 7 PAUSE
NORMAL mod=0 8 PAUSE MENU
NORMAL mod=1 0 CONTROL STICK
NORMAL mod=1 1 JUMP
NORMAL mod=1 2 ATTACK
NORMAL mod=1 3 GRAB
NORMAL mod=1 4 SPECIAL
NORMAL mod=1 5 SHIELD
NORMAL mod=1 6 C-STICK (HOLD)
NORMAL mod=1 7 PAUSE
NORMAL mod=1 8 PAUSE MENU
BOX mod=0 0 CONTROL STICK
BOX mod=0 1 JUMP
BOX mod=0 2 ATTACK
BOX mod=0 3 GRAB
BOX mod=0 4 SPECIAL
BOX mod=0 5 MOD / TILT
BOX mod=0 6 SHIELD
BOX mod=0 7 PAUSE
BOX mod=0 8 PAUSE MENU
BOX mod=1 0 CONTROL STICK
BOX mod=1 1 JUMP
BOX mod=1 2 ATTACK
BOX mod=1 3 GRAB
BOX mod=1 4 SPECIAL
BOX mod=1 5 SHIELD
BOX mod=1 6 MOD / TILT
BOX mod=1 7 PAUSE
BOX mod=1 8 PAUSE MENU
NATURAL mod=0 0 CONTROL STICK
NATURAL mod=0 1 JUMP
NATURAL mod=0 2 ATTACK
NATURAL mod=0 3 GRAB
NATURAL mod=0 4 SPECIAL
NATURAL mod=0 5 SHIELD
NATURAL mod=0 6 C-STICK (HOLD)
NATURAL mod=0 7 PAUSE
NATURAL mod=0 8 PAUSE MENU
NATURAL mod=1 0 CONTROL STICK
NATURAL mod=1 1 JUMP
NATURAL mod=1 2 ATTACK
NATURAL mod=1 3 GRAB
NATURAL mod=1 4 SPECIAL
NATURAL mod=1 5 SHIELD
NATURAL mod=1 6 C-STICK (HOLD)
NATURAL mod=1 7 PAUSE
NATURAL mod=1 8 PAUSE MENU
WANT
cmp -s "$B/ctl_labels.got" "$B/ctl_labels.want" \
  || { diff "$B/ctl_labels.want" "$B/ctl_labels.got" | sed -n '1,20p'; fail "[0m] the compiled Controls-screen label table does not match the authored table above (left = authored, right = compiled) — if the MAPPING really changed, change s1_input.h/foh_ctl_labels.h and this table in the SAME change and say why"; }
lblrows="$(wc -l < "$B/ctl_labels.want" | tr -d ' ')"
[ "$lblrows" = 54 ] || fail "[0m] authored label table is $lblrows rows, expected 54 (3 styles x Mod-on-L/R x 9 buttons) — a style or a button row was added without extending the pin"
echo "  [0m] control-role predicate agrees at both copies; label table compiled and pinned for all 6 style/Mod arrangements (54 rows)"

# --- [1p] phantomThreshold: NO HAND-TYPED ENGINE VALUE MAY DRIFT (C-review
# r11 BLOCKER; HARD RULE 5 + HARD RULE 8's "instrument > class fix").
# `phantomThreshold` is a gameSettings value ON THE CHECKSUM SURFACE
# (hitDetection.js:335/337/348), and its authored default 0.01 is currently
# HAND-TYPED at four C sites. HARD RULE 5 says engine values come from the
# executed-data pipeline and are never retyped by hand; the proper fix is to
# emit this default through the pipeline, which changes a PINNED M1 producer
# and therefore belongs to a pipeline arc, not this one (REGISTERED, reported
# to the driver — see MENU-SPEC §4's note).
#
# What is fixable HERE, and is strictly better than consolidating the four
# literals into one hand-typed constant, is an INSTRUMENT: assert every C site
# equals the value parsed from UPSTREAM'S OWN BYTES. That makes silent drift
# impossible at ALL FOUR sites (two of which predate this arc) and it catches a
# FIFTH site being added, because the site count is pinned. It is the project's
# standing "read the oracle's own bytes, never transcribe" pattern.
echo "=== [1p] phantomThreshold hand-typed-value instrument"
# Same clone resolution pipeline/run.js:39 uses (already a hard prerequisite
# of [1], so this introduces no new one).
PT_DIST="${MELEELIGHT_CLONE:-$HOME/.cache/meleelight-funkey-s/upstream}"
PT_SETTINGS="$PT_DIST/src/settings.js"
[ -f "$PT_SETTINGS" ] || fail "phantomThreshold instrument — upstream $PT_SETTINGS missing (the clone is already a hard prerequisite of [1])"
# Upstream's authored default, parsed from the source bytes (never transcribed).
pt_up="$(sed -n 's/^[[:space:]]*phantomThreshold[[:space:]]*:[[:space:]]*\([0-9.eE+-]*\)[[:space:]]*,.*$/\1/p' "$PT_SETTINGS")"
[ -n "$pt_up" ] || grammar_die "phantomThreshold instrument — could not parse the authored default out of $PT_SETTINGS (upstream shape changed; this is corrupt evidence, never a pass)"
[ "$(printf '%s\n' "$pt_up" | wc -l | tr -d ' ')" = 1 ] || grammar_die "phantomThreshold instrument — $PT_SETTINGS declares phantomThreshold more than once (ambiguous authored default)"
# ROUND-19 CLASS FIX (Tier A+ round-19 MAJOR): the first form of this
# instrument asked its questions with the RHS SPELLING baked into the regex --
# the fifth-site sweep required the right-hand side to begin with `[0-9]`, and
# the literal extractor's character class had no parentheses. codex showed what
# that leaves open: `foh.phantomThreshold = .01;` is a compiled FIFTH hand-typed
# literal with identical behavior, yet `pt_found` stayed 4, the sweep did not
# list the file, and this leg still printed "no fifth site" (`+0.01` and
# `(0.01)` evade it the same way). "Does any C site hand-type this engine
# value?" must not depend on how the author spelled the number. So the sweep is
# now SPELLING-INDEPENDENT -- it matches every assignment to the field, whatever
# follows the `=` -- and each right-hand side is CLASSIFIED instead of
# pattern-matched: a numerically-spelled RHS is a hand-typed literal (and must
# live in a pinned literal site and equal upstream's value NUMERICALLY, so
# `.01` is judged equal to `0.01` rather than reported as drift); anything else
# is a propagation from another variable.
#
# Two PINNED file sets, because "assigns the field" and "hand-types a number"
# are different questions and only the second is a HARD RULE 5 matter:
PT_LIT_SITES="port/foh/foh.c port/foh/foh_persist.c port/sim/sim/sim_boot.c port/sim/target/target_play.c"
PT_ASSIGN_SITES="port/foh/foh.c port/foh/foh_app.c port/foh/foh_persist.c port/sim/calib/replay_hitdet.c port/sim/calib/replay_physics.c port/sim/hit_detection.c port/sim/sim/sim_boot.c port/sim/target/target_play.c"
# Whole-tree sweep with NO constraint on the right-hand side: every file that
# assigns phantomThreshold at all must be pinned. A new site cannot hide behind
# an unusual spelling of the number, because the spelling is not consulted.
pt_all="$(grep -rlE 'phantomThreshold[[:space:]]*=' port/ --include='*.c' --include='*.h' 2>/dev/null | grep -v '/build/' | sort | tr '\n' ' ')"
pt_want="$(printf '%s\n' $PT_ASSIGN_SITES | sort | tr '\n' ' ')"
[ "$pt_all" = "$pt_want" ] || fail "phantomThreshold instrument — the set of files ASSIGNING phantomThreshold is '$pt_all', pinned '$pt_want' (a new site appeared, or one was removed without updating this pin)"
pt_found=0
for f in $PT_ASSIGN_SITES; do
  [ -f "$f" ] || fail "phantomThreshold instrument — pinned site $f is missing (the site list is stale)"
  # Every assignment must PARSE to a right-hand side. A spelling this extractor
  # cannot read is UNJUDGED, which is exactly the failure this round found, so
  # it fails loudly instead of being skipped.
  pt_asg="$(grep -cE 'phantomThreshold[[:space:]]*=' "$f" | tr -d ' ')"
  pt_par="$(grep -nE 'phantomThreshold[[:space:]]*=' "$f" | sed -nE 's/^([0-9]+):.*phantomThreshold[[:space:]]*=[[:space:]]*([^;]*);.*/\1:\2/p')"
  pt_np="$(printf '%s\n' "$pt_par" | grep -c . || true)"
  [ "$pt_np" = "$pt_asg" ] || grammar_die "phantomThreshold instrument — $f has $pt_asg assignment(s) but only $pt_np parse to a right-hand side; an unparsed spelling would go UNJUDGED (this is corrupt evidence, never a pass)"
  while IFS= read -r rec; do
    [ -n "$rec" ] || continue
    pt_ln="${rec%%:*}"; pt_rhs="${rec#*:}"
    # CLASSIFY: numerically spelled (optionally signed/parenthesised) => a
    # hand-typed literal. Anything else (an identifier, a field read, a call)
    # is a propagation and is not a HARD RULE 5 site.
    printf '%s' "$pt_rhs" | grep -qE '^[[:space:]]*[-+(]*[[:space:]]*[0-9.][0-9.eE+-]*[[:space:]]*[)]*[[:space:]]*$' || continue
    pt_found=$((pt_found + 1))
    case " $PT_LIT_SITES " in
      *" $f "*) ;;
      *) fail "phantomThreshold instrument — $f:$pt_ln hand-types the literal '$pt_rhs', but $f is not a pinned literal site (HARD RULE 5: a new hand-typed CHECKSUM-SURFACE engine value appeared)" ;;
    esac
    # NUMERIC equality, so a legal respelling is not reported as drift while a
    # changed VALUE still is.
    pt_norm="$(printf '%s' "$pt_rhs" | tr -d '() ')"
    awk -v a="$pt_norm" -v b="$pt_up" 'BEGIN { exit !(a ~ /[0-9]/ && (a + 0) == (b + 0)) }' \
      || fail "phantomThreshold instrument — $f:$pt_ln assigns '$pt_rhs' but upstream settings.js authors $pt_up (a hand-typed CHECKSUM-SURFACE engine value has drifted; HARD RULE 5)"
  done <<EOF
$pt_par
EOF
done
[ "$pt_found" = 4 ] || fail "phantomThreshold instrument — found $pt_found hand-typed literal assignment(s) across the pinned sites, want 4 (a fifth hand-typed site appeared, or one was removed without updating this pin)"
# HONEST LIMIT (the round-6 MINOR-2 discipline: say what this does NOT prove).
# A right-hand side that is a NAMED constant classifies as a propagation here,
# so a `#define PHANTOM_DEFAULT 0.01` would move the hand-typed value out of
# this instrument's view. Closed by pinning that no such macro exists: if one
# is ever wanted, it must be added together with the check that judges it.
pt_mac="$( { grep -rnE '^[[:space:]]*#[[:space:]]*define[[:space:]]+[A-Za-z_]*([Pp][Hh][Aa][Nn][Tt][Oo][Mm])[A-Za-z_]*' port/ --include='*.c' --include='*.h' 2>/dev/null || true; } | { grep -v '/build/' || true; } | wc -l | tr -d ' ')"
[ "$pt_mac" = 0 ] || fail "phantomThreshold instrument — a phantom* macro is now defined ($pt_mac site(s)); a named constant reads as a propagation to the classifier above, so extend this instrument to judge the macro's value in the SAME change"
echo "    [1p] OK: every file assigning phantomThreshold is pinned (${pt_want% }); all 4 hand-typed literals == upstream settings.js's authored $pt_up numerically; no fifth hand-typed site, no phantom* macro"

# --- [2] build ----------------------------------------------------------------
echo "=== [2] build foh_app"
CFLAGS_COMMON=(-ffp-contract=off -Wall -Wextra -Werror
  -I"$TABLES" -Iport/ryu -Iport/sim -Ioracle/qjs)
rm -f "$B/raster.o" "$B/platform_headless.o" "$B/foh.o" "$B/foh_font.o" "$B/ctl_style.o" \
      "$B/foh_render.o" "$B/foh_persist.o" "$B/foh_app.o" "$B/img1.o" \
      "$B/foh_app"
cc -O3 "${CFLAGS_COMMON[@]}" -c "$GFX/raster.c" -o "$B/raster.o"
# A1 restyle Phase 1: the IMG1 menu-artwork loader (port/gfx/img1.h).
cc -O2 "${CFLAGS_COMMON[@]}" -c "$GFX/img1.c" -o "$B/img1.o"
cc -O2 "${CFLAGS_COMMON[@]}" -c "$GFX/platform_headless.c" -o "$B/platform_headless.o"
cc -O2 "${CFLAGS_COMMON[@]}" -c "$FOH/foh.c" -o "$B/foh.o"
cc -O2 "${CFLAGS_COMMON[@]}" -c "$FOH/foh_font.c" -o "$B/foh_font.o"
cc -O2 "${CFLAGS_COMMON[@]}" -c "$FOH/foh_render.c" -o "$B/foh_render.o"
cc -O2 "${CFLAGS_COMMON[@]}" -c "$FOH/foh_persist.c" -o "$B/foh_persist.o"
cc -O2 "${CFLAGS_COMMON[@]}" -c "$FOH/foh_app.c" -o "$B/foh_app.o"
# C30(c): the control-style/Mod-shoulder cells. A TU, not a header, on
# purpose (ctl_style.c's own note): the Controls screen writes them in one
# TU and the input path reads them in another, so a per-TU static copy
# would be a live desync.
cc -O2 "${CFLAGS_COMMON[@]}" -c "$GFX/ctl_style.c" -o "$B/ctl_style.o"
# ONE list, two consumers. The phantomThreshold witness ([1pw]) must link the
# SAME bodies the app links -- a second copy of this list would drift, and the
# witness would then be witnessing code the app does not run.
FOH_LINK_OBJS=(
  "$B/foh.o" "$B/foh_font.o" "$B/foh_render.o" "$B/foh_persist.o"
  "$B/ctl_style.o" "$B/raster.o" "$B/img1.o" "$B/platform_headless.o"
)
FOH_LINK_SRCS=(
  "$SIM/sim_boot.c" "$SIM/sim_tick.c" "$SIM/sim_ser.c"
  "$SIM/sim_data.c" "$SIM/sim_ai_live.c"
  "$CAL/canon.c" "$CAL/player_canon.c"
  port/sim/ai.c
  port/sim/physics.c port/sim/interpolated_collision.c
  port/sim/environmental_collision.c port/sim/hit_detection.c
  port/sim/article.c port/sim/action_state_shortcuts.c
  port/sim/ml_events.c port/sim/ml_fmt.c port/sim/ml_ser.c
  port/sim/ai_bridge.c port/sim/input/interpret_inputs.c
  port/sim/stages/moving_platforms.c port/sim/stages/ystory.c
  port/sim/stages/fountain.c
  port/sim/characters/shared/moves_index.c port/sim/characters/shared/moves/*.c
  port/sim/characters/fox/moves_index.c port/sim/characters/fox/moves/*.c
  port/sim/characters/falco/moves_index.c port/sim/characters/falco/moves/*.c
  port/sim/characters/falcon/moves_index.c port/sim/characters/falcon/moves/*.c
  port/sim/characters/marth/moves_index.c
  port/sim/characters/marth/dancing_blade_combo.c
  port/sim/characters/marth/dancing_blade_air_mobility.c
  port/sim/characters/marth/moves/*.c
  port/sim/characters/puff/moves_index.c
  port/sim/characters/puff/puff_multi_jump_drift.c
  port/sim/characters/puff/puff_next_jump.c
  port/sim/characters/puff/moves/*.c
  port/sim/target/target_play.c
  "$TABLES/ml_tables.c" "$TABLES/ml_stages.c" "$TABLES/ml_targets.c"
  oracle/qjs/sha256.c port/fdlibm/fdlibm.c -lm
)
cc -O2 "${CFLAGS_COMMON[@]}" -o "$B/foh_app" \
  "${FOH_LINK_OBJS[@]}" "$B/foh_app.o" "${FOH_LINK_SRCS[@]}"
made "$B/foh_app"
echo "build OK: $B/foh_app (raster TU -O3, all else -O2; -ffp-contract=off everywhere)"

# --- [1pw] phantomThreshold BEHAVIORAL witness ---------------------------------
# Tier A+ round-20 MAJORs 1-3, closed as ONE CLASS. [1p] above answers the
# STRUCTURAL half of the HARD RULE 5 question by reading the source, and codex
# defeated three successive readings in three successive rounds: `.01`, then
# `+0.01` / `(0.01)`, then `0x1.47ae147ae147bp-7` / `0.01f` / `(double)0.01`, a
# named `static const double phantom_default = .01;`, and a comment wedged
# before the `=`. Widening the regex a fourth time would lose the same race a
# fourth time, because a regex enumerates the spellings its author thought of
# while C admits infinitely many spellings of one double.
#
# So the VALUE half is settled the way this lane has already settled its frame,
# field-value and separator planes: BY EXECUTION. port/foh/foh_phantom_witness.c
# CALLS all four owning initialisers and reads the field back out of the
# initialised objects, comparing bit-for-bit against the value parsed from
# UPSTREAM'S OWN settings.js bytes. After compilation no spelling survives, so
# the question stops depending on how the number was written.
#
# The two legs are complementary and neither is redundant: [1p] pins the SHAPE
# of the site set (which files assign the field at all, how many hand-type a
# number, no phantom* macro), [1pw] pins the VALUE every site actually
# produces. A respelling that hides from [1p]'s classifier cannot hide here.
echo "=== [1pw] phantomThreshold behavioral witness (compiled, not read)"
# The expected bit pattern is computed INDEPENDENTLY of the C under test, from
# the same upstream bytes, by V8. That makes the assertion a differential
# (V8's parse vs the compiler's) rather than the witness grading its own paper.
pt_bits="$(node -e 'const b=Buffer.alloc(8);b.writeDoubleLE(Number(process.argv[1]));console.log(String(b.toString("hex").match(/../g).reverse().join("")))' "$pt_up")"
printf '%s' "$pt_bits" | grep -qE '^[0-9a-f]{16}$' \
  || grammar_die "phantomThreshold witness — could not compute the expected bit pattern for upstream's '$pt_up' (got '$pt_bits'); this is corrupt evidence, never a pass"
PHANTOM_OK_LINE="PHANTOM WITNESS OK (sites=4 value=$pt_up bits=$pt_bits)"
# ROUND-21 M2: the witness's COVERAGE is a function of [1p]'s pinned rule set,
# not of what its author chose to call. codex's finding was that re-aiming
# site 3's read from `g_target` at `g_match` left the output byte-identical, so
# tp_setup_target() was called but never judged. The witness side is fixed by
# poisoned per-site slots; this is the OUTSIDE half: one labelled row per setup
# path, and the label set CROSS-BOUND to PT_LIT_SITES, so a fifth hand-typed
# literal site cannot appear without a fifth witnessed path (and vice versa).
PT_WIT_MAP="port/foh/foh.c:foh_init port/foh/foh_persist.c:foh_persist_defaults port/sim/sim/sim_boot.c:sim_setup_match port/sim/target/target_play.c:tp_setup_target"
pt_map_files="$(printf '%s\n' $PT_WIT_MAP | sed 's/:.*//' | sort | tr '\n' ' ')"
pt_lit_sorted="$(printf '%s\n' $PT_LIT_SITES | sort | tr '\n' ' ')"
[ "$pt_map_files" = "$pt_lit_sorted" ] \
  || fail "phantomThreshold witness — the witnessed setup paths cover files '$pt_map_files' but [1p] pins the hand-typed literal sites as '$pt_lit_sorted' (both directions: a literal site with no witnessed initialiser would go unjudged, a witnessed path with no literal site is stale)"
PHANTOM_EXPECT="$B/phantomwit.expect"
: > "$PHANTOM_EXPECT"
pt_idx=0
pt_fn_list=""
for pt_pair in $PT_WIT_MAP; do
  pt_file="${pt_pair%%:*}"; pt_fn="${pt_pair#*:}"
  # The map is not taken on trust: the named initialiser must actually be
  # defined in the file [1p] pinned as its literal site.
  grep -qE "\b$pt_fn[[:space:]]*\(" "$pt_file" \
    || fail "phantomThreshold witness — the map claims $pt_file's hand-typed literal is owned by $pt_fn(), but $pt_fn is not mentioned in $pt_file (stale map)"
  printf 'PHANTOM SITE %d %s bits=%s\n' "$pt_idx" "$pt_fn" "$pt_bits" >> "$PHANTOM_EXPECT"
  pt_fn_list="$pt_fn_list $pt_fn"
  pt_idx=$((pt_idx + 1))
done
pt_fn_list="${pt_fn_list# }"
printf '%s\n' "$PHANTOM_OK_LINE" >> "$PHANTOM_EXPECT"
[ "$pt_idx" = 4 ] || grammar_die "phantomThreshold witness — built $pt_idx expected site row(s), want 4 (corrupt evidence, never a pass)"
# WHOLE-OUTPUT whitelist, the [5c]/[5d] rule: success output is EXACTLY this
# block, byte-compared, so a witness that also printed a complaint could not
# be read as a pass, and a renamed/duplicated/dropped path is a diff.
phantom_verdict_ok() { # <out-file>
  cmp -s "$PHANTOM_EXPECT" "$1"
}
# The witness links the SAME bodies foh_app links (FOH_LINK_OBJS/SRCS above) so
# that it witnesses the code the app actually runs; only foh_app.o is swapped
# out, because both define main().
phantom_link() { # <out-binary> <persist-object> <witness-object>
  cc -O2 "${CFLAGS_COMMON[@]}" -o "$1" \
    "$B/foh.o" "$B/foh_font.o" "$B/foh_render.o" "$2" \
    "$B/ctl_style.o" "$B/raster.o" "$B/img1.o" "$B/platform_headless.o" \
    "$3" "${FOH_LINK_SRCS[@]}"
}
rm -f "$B/foh_phantom_witness.o" "$B/foh_phantom_witness"
cc -O2 "${CFLAGS_COMMON[@]}" -c "$FOH/foh_phantom_witness.c" -o "$B/foh_phantom_witness.o"
phantom_link "$B/foh_phantom_witness" "$B/foh_persist.o" "$B/foh_phantom_witness.o"
made "$B/foh_phantom_witness"
if ! "$B/foh_phantom_witness" "$pt_up" > "$B/phantomwit.out" 2>&1; then
  relay_lines < "$B/phantomwit.out"
  fail "phantomThreshold witness exited non-zero — a CHECKSUM-SURFACE engine value has drifted at one of the four owning initialisers (HARD RULE 5; see relayed output above)"
fi
relay_lines < "$B/phantomwit.out"
phantom_verdict_ok "$B/phantomwit.out" \
  || fail "phantomThreshold witness — success output is not the exact anchored line '$PHANTOM_OK_LINE' (permissive-parse guard, PROCESS §3) — see relayed output above"
echo "    [1pw] OK: one labelled row per pinned literal site ($pt_fn_list) — each initialiser was CALLED, read through its OWN poisoned slot, and produced phantomThreshold == upstream settings.js's $pt_up bit-for-bit ($pt_bits)"

# --- [1pw] TEETH. All on COPIES of foh_persist.c; the committed source is
# never touched. Two prove the witness fails CLOSED on value drift wearing
# exactly the spellings that defeated [1p]'s classifier; the third is a
# NEGATIVE CONTROL proving it does not fire on a legal respelling, which is the
# property that makes the arms race stop.
PT_TOOTHDIR="$B/ptwit"
rm -rf "$PT_TOOTHDIR"; mkdir -p "$PT_TOOTHDIR"
pt_tooth() { # <id> <perl-expr> <expect: fail|pass> <needle> <label>
  local id="$1" expr="$2" mode="$3" needle="$4" label="$5" rc=0
  local d="$PT_TOOTHDIR/$id"
  mkdir -p "$d"
  cp "$FOH/foh_persist.c" "$d/foh_persist.c"
  perl -0pi -e "$expr" "$d/foh_persist.c"
  cmp -s "$FOH/foh_persist.c" "$d/foh_persist.c" \
    && fail "$id — the tooth edit was a NO-OP on the copy (the pattern did not match); a tooth that changes nothing proves nothing"
  cc -O2 "${CFLAGS_COMMON[@]}" -I"$FOH" -c "$d/foh_persist.c" -o "$d/foh_persist.o"
  phantom_link "$d/wit" "$d/foh_persist.o" "$B/foh_phantom_witness.o"
  "$d/wit" "$pt_up" > "$d/out" 2>&1 || rc=$?
  if [ "$mode" = fail ]; then
    [ "$rc" = 1 ] || fail "$id — the perturbed build exited rc $rc, want 1 ($label): $(cat "$d/out")"
    grep -qF "$needle" "$d/out" \
      || fail "$id — the perturbed build failed, but not at the declared diagnostic '$needle' ($label): $(cat "$d/out")"
  else
    [ "$rc" = 0 ] || fail "$id — the respelled build exited rc $rc, want 0 ($label): $(cat "$d/out")"
    phantom_verdict_ok "$d/out" \
      || fail "$id — the respelled build did not print the exact anchored success line ($label): $(cat "$d/out")"
  fi
  echo "    $id OK: $label"
}
# T-PT1 (round-20 MAJOR 2): a HEX FLOAT, the spelling [1p]'s numeric classifier
# skips entirely — carrying a ONE-ULP drift. Nothing short of reading the
# compiled value can see this.
pt_tooth ptwit1 \
  's/p->phantomThreshold = 0\.01;/p->phantomThreshold = 0x1.47ae147ae147cp-7;/' \
  fail 'phantom witness: foh_persist_defaults() produced phantomThreshold bits' \
  'a one-ulp drift spelled as a hex float is caught (MAJOR 2)'
# T-PT2 (round-20 MAJOR 1): a NAMED CONSTANT, which [1p] classifies as a
# propagation and therefore does not judge at all.
pt_tooth ptwit2 \
  's/p->phantomThreshold = 0\.01;/static const double phantom_default = 0.02; p->phantomThreshold = phantom_default;/' \
  fail 'phantom witness: foh_persist_defaults() produced phantomThreshold bits' \
  'drift hidden behind a named constant is caught (MAJOR 1)'
# T-PT3 (round-20 MAJOR 3, NEGATIVE CONTROL): a comment before the `=` plus a
# hex float holding the CORRECT value. The witness must stay silent: being
# blind to spelling is the point, and a witness that cried drift here would
# just be a slower regex.
pt_tooth ptwit3 \
  's{p->phantomThreshold = 0\.01;}{p->phantomThreshold /* authored default */ = 0x1.47ae147ae147bp-7;}' \
  pass '-' \
  'a legal respelling (comment before =, hex float, same value) does NOT fire (MAJOR 3)'

# --- [1pw] TEETH, round-21 M2 group. These perturb COPIES of the WITNESS
# itself, because the defect codex found lived there: a read aimed at the wrong
# object. All three edits are NO-OPS under the pre-M2 witness (identical bytes
# out); each must now fail, and fail at ITS OWN declared diagnostic.
pt_wtooth() { # <id> <perl-expr> <mode: nonzero|block> <needle> <label>
  local id="$1" expr="$2" mode="$3" needle="$4" label="$5" rc=0
  local d="$PT_TOOTHDIR/$id"
  mkdir -p "$d"
  cp "$FOH/foh_phantom_witness.c" "$d/foh_phantom_witness.c"
  perl -0pi -e "$expr" "$d/foh_phantom_witness.c"
  cmp -s "$FOH/foh_phantom_witness.c" "$d/foh_phantom_witness.c" \
    && fail "$id — the tooth edit was a NO-OP on the copy (the pattern did not match); a tooth that changes nothing proves nothing"
  cc -O2 "${CFLAGS_COMMON[@]}" -I"$FOH" -c "$d/foh_phantom_witness.c" -o "$d/wit.o"
  phantom_link "$d/wit" "$B/foh_persist.o" "$d/wit.o"
  "$d/wit" "$pt_up" > "$d/out" 2>&1 || rc=$?
  if [ "$mode" = nonzero ]; then
    [ "$rc" != 0 ] || fail "$id — the perturbed witness exited 0 ($label); the round-21 M2 defect is exactly that this edit is invisible: $(cat "$d/out")"
  else
    # The values are all still correct, so the witness legitimately exits 0.
    # What must move is the OUTPUT BLOCK the leg byte-compares.
    [ "$rc" = 0 ] || fail "$id — the perturbed witness exited $rc, want 0 ($label): $(cat "$d/out")"
    phantom_verdict_ok "$d/out" \
      && fail "$id — the perturbed witness still printed the exact expected block ($label); the labelled rows are not being judged: $(cat "$d/out")"
  fi
  grep -qF "$needle" "$d/out" \
    || fail "$id — the perturbed witness moved (rc $rc) but not at the declared diagnostic '$needle' ($label): $(cat "$d/out")"
  echo "    $id OK: $label"
}
# T-PT4 (round-21 M2, the EXACT edit codex constructed): site 3 reads the
# object site 2 owns. Pre-M2 this produced byte-identical output; now two sites
# would share one slot, which the witness refuses outright.
pt_wtooth ptwit4 \
  's/sites\[3\]\.slot = &g_target\.sim\.phantomThreshold;/sites[3].slot = \&g_match.sim.phantomThreshold;/' \
  nonzero \
  'sites 2 (sim_setup_match) and 3 (tp_setup_target) read the SAME slot' \
  'the exact cross-read codex constructed is refused up front (round-21 M2)'
# T-PT5 (the ATTACKER'S FULL REMEDY, the lane's quiet-re-freeze idiom): the
# same cross-read, PLUS deleting the guard that caught it. The second line of
# defence — the per-site SPENT stamp — must still name the object actually hit.
pt_wtooth ptwit5 \
  's/sites\[3\]\.slot = &g_target\.sim\.phantomThreshold;/sites[3].slot = \&g_match.sim.phantomThreshold;/; s/for \(j = 0; j < i; j\+\+\) \{/for (j = 0; j < 0; j++) {/' \
  nonzero \
  'site 3 (tp_setup_target) read the slot of site 2 (sim_setup_match), already measured' \
  'with the duplicate-slot guard REMOVED, the SPENT stamp still catches the cross-read by name (round-21 M2)'
# T-PT6 (the other sentinel direction): an owning initialiser stops being
# CALLED at all. Its slot then still holds the UNSET poison, so "called but
# unjudged" and "never called" are both failures, not silent passes.
pt_wtooth ptwit6 \
  's{foh_init\(&g_foh\);}{(void)0; /* tooth: initialiser not called */}' \
  nonzero \
  'site 0 (foh_init) still holds its UNSET poison -- foh_init() did not write phantomThreshold' \
  'an owning initialiser that is never CALLED is caught by the UNSET poison (round-21 M2)'
# T-PT7 (the labelling half): a DUPLICATED label. Every value assertion still
# passes — only the per-row output moves, which is precisely why the leg
# byte-compares the whole block against labels derived from PT_LIT_SITES.
pt_wtooth ptwit7 \
  's/sites\[3\]\.name = "tp_setup_target";/sites[3].name = "sim_setup_match";/' \
  block \
  'PHANTOM SITE 3 sim_setup_match' \
  'a DUPLICATED setup-path label changes the whole-output block (round-21 M2)'

# golden traces -> text (pinned trace-to-txt.js)
rm -f "$B/g01.trace.txt" "$B/m01.trace.txt" "$B/g03.trace.txt" \
      "$B/t01.trace.txt" "$B/t02.trace.txt"
node "$SIM/trace-to-txt.js" "oracle/goldens/$G01_TRACE" "$B/g01.trace.txt" 2>&1 | relay_lines
node "$SIM/trace-to-txt.js" "$M4G/$M01_TRACE" "$B/m01.trace.txt" 2>&1 | relay_lines
node "$SIM/trace-to-txt.js" "oracle/goldens/$G03_TRACE" "$B/g03.trace.txt" 2>&1 | relay_lines
node "$SIM/trace-to-txt.js" "$M4G/$T01_TRACE" "$B/t01.trace.txt" 2>&1 | relay_lines
node "$SIM/trace-to-txt.js" "$M4G/$T02_TRACE" "$B/t02.trace.txt" 2>&1 | relay_lines
made "$B/g01.trace.txt" "$B/m01.trace.txt" "$B/g03.trace.txt" \
     "$B/t01.trace.txt" "$B/t02.trace.txt"

# PRODUCTION shot judge (review-88 M5/M6): structural validation of
# BOTH files (exact P6 header bytes as write_shot_ppm emits them, exact
# payload length 240*240*3, no trailing bytes, non-blank), then the A/B
# byte-equality arm. Leg [3] AND tooth T5 run THIS function — there is
# no separate hard-coded twin comparison.
judge_shot_pair() { # <ctx> <fileA> <fileB>
  local ctx="$1" fa="$2" fb="$3" f
  for f in "$fa" "$fb"; do
    made "$f"
    node -e '
      const fs = require("fs");
      const b = fs.readFileSync(process.argv[1]);
      const hdr = Buffer.from("P6\n240 240\n255\n", "latin1");
      if (b.length < hdr.length || !b.subarray(0, hdr.length).equals(hdr)) {
        console.error("shot header is not exactly P6/240 240/255 (structural)");
        process.exit(1);
      }
      const want = hdr.length + 240 * 240 * 3;
      if (b.length !== want) {
        console.error("shot payload " + (b.length - hdr.length) +
                      " bytes != 240*240*3 (truncated or trailing bytes)");
        process.exit(1);
      }
      const seen = new Set();
      let multi = false;
      for (let k = hdr.length; k + 2 < b.length; k += 3) {
        seen.add(b[k] * 65536 + b[k + 1] * 256 + b[k + 2]);
        if (seen.size > 1) { multi = true; break; }
      }
      if (!multi) {
        console.error("shot is a single flat colour (blank render)");
        process.exit(1);
      }
    ' "$f" || fail "shot $ctx: structural/blank validation failed on $f"
  done
  cmp "$fa" "$fb" || fail "shot $ctx: runs A/B not byte-identical"
}

# PRODUCTION shot-inventory judge (review-90 M2; review-91 L): EXACT-SET
# enumeration of a shots dir vs the pinned inventory, both directions —
# an extra, missing, or renamed file is death. DOTFILE-INCLUSIVE
# (review-91 L: plain `ls` omits dotfiles — a planted .unexpected.ppm
# escaped the claimed exact set); enumeration failure is grammar death,
# never an empty set. Applied to BOTH runs' dirs in leg [3]; teeth T14
# (unexpected.ppm) and T16 (.unexpected.ppm) run THIS function.
judge_shot_inventory() { # <ctx> <dir> <want-sorted>
  local got rc=0
  got="$(cd "$2" && find . -mindepth 1 -maxdepth 1 | sed -e 's|^\./||' -e 's/\.ppm$//' | sort | tr '\n' ' ' | sed 's/ $//')" || rc=$?
  [ "$rc" = 0 ] || grammar_die "shot inventory $1: enumeration failed (rc $rc) — corrupt evidence, never a pass"
  [ "$got" = "$3" ] || fail "shot inventory $1: {$got} != pinned {$3} (both directions, dotfiles included)"
}

# --- [3] flow runs x2 + trace/shot judgments -----------------------------------
echo "=== [3] flow runs (x2 each) + frozen-trace + shot judgments"
total_shots=0
states=0
tstates=0
for k in 0 1 2 3 4 5 6; do
  id="${FLOW_IDS[$k]}"
  mode="${FLOW_BRIDGE[$k]}"
  echo "== flow $id (bridge=$mode) run A"
  rm -rf "$B/$id"
  mkdir -p "$B/$id/shots-a" "$B/$id/shots-b"
  fresh_persist # defaults domain (task 13)
  case "$mode" in
    verify)
      extra=()
      case "$id" in
        f01-vs-g01) seed=$G01_SEED; tracef=$B/g01.trace.txt; frames=$G01_FRAMES ;;
        f02-cpu-m01) seed=$M01_SEED; tracef=$B/m01.trace.txt; frames=$M01_FRAMES
                     extra=(--cpu-live) ;;
        f05-vs-g03) seed=$G03_SEED; tracef=$B/g03.trace.txt; frames=$G03_FRAMES ;;
        *) fail "flow $id: verify bridge with no registered golden params" ;;
      esac
      "$B/foh_app" --flow "$FLOWS/$id.flow" --flow-out "$B/$id/trace-a.txt" \
        --shots-dir "$B/$id/shots-a" \
        --bridge verify --simdata "$B/simdata.txt" --seed "$seed" \
        --trace "$tracef" --frames "$frames" --out "$B/$id/stream.txt" \
        --bstate-out "$B/$id/bstate.txt" ${extra[@]+"${extra[@]}"} \
        2>&1 | relay_lines
      made "$B/$id/stream.txt" "$B/$id/bstate.txt"
      ;;
    tverify)
      # iter 99: the target-launch bridge — BOTH streams out in the
      # target_main.c producer grammar (wrap-target judges in [4]).
      case "$id" in
        f06-target-t01) seed=$T01_SEED; tracef=$B/t01.trace.txt; frames=$T01_FRAMES ;;
        f07-target-t02) seed=$T02_SEED; tracef=$B/t02.trace.txt; frames=$T02_FRAMES ;;
        *) fail "flow $id: tverify bridge with no registered golden params" ;;
      esac
      "$B/foh_app" --flow "$FLOWS/$id.flow" --flow-out "$B/$id/trace-a.txt" \
        --shots-dir "$B/$id/shots-a" \
        --bridge tverify --simdata "$B/simdata.txt" --seed "$seed" \
        --trace "$tracef" --frames "$frames" --out "$B/$id/stream.txt" \
        --bstate-out "$B/$id/bstate.txt" \
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
  fresh_persist # defaults domain (task 13; A's save must not leak to B)
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
  if [ "$mode" = "tverify" ]; then
    c="$(count_e "$B/$id/bstate.txt" '^TBRIDGE-STATE char=[0-4] tstage=[0-9] gamemode=5 targets=(0|[1-9][0-9]?) playing=1 starting=1 stocks=1$')"
    [ "$c" = 1 ] || grammar_die "flow $id: TBRIDGE-STATE grammar count $c/1"
    cmp "$B/$id/bstate.txt" "$FLOWS/$id.bstate.expect" || fail "flow $id: TBRIDGE-STATE differs from the frozen witness"
    tstates=$((tstates + 1))
  elif [ "$mode" != "none" ]; then
    c="$(count_e "$B/$id/bstate.txt" '^BRIDGE-STATE p1=[0-4] p2=[0-4] p2type=[01] difficulty=[1-4] stage=[0-5] turbo=[01] lcancel=[012] tapjump=[01],[01],[01],[01] phantom=[0-9a-f]{16}$')"
    [ "$c" = 1 ] || grammar_die "flow $id: bstate grammar count $c/1"
    cmp "$B/$id/bstate.txt" "$FLOWS/$id.bstate.expect" || fail "flow $id: BRIDGE-STATE differs from the frozen witness"
    states=$((states + 1))
  fi
  # shots: pinned inventory EXACT-SET on BOTH runs' dirs (review-90
  # M2), A==B, non-blank, distinct
  want_shots="${FLOW_SHOTS[$k]}"
  want_sorted="$(printf '%s\n' $want_shots | sort | tr '\n' ' ' | sed 's/ $//')"
  judge_shot_inventory "flow $id run A" "$B/$id/shots-a" "$want_sorted"
  judge_shot_inventory "flow $id run B" "$B/$id/shots-b" "$want_sorted"
  for sname in $want_shots; do
    judge_shot_pair "flow $id shot $sname" \
      "$B/$id/shots-a/$sname.ppm" "$B/$id/shots-b/$sname.ppm"
    total_shots=$((total_shots + 1))
  done
  ndistinct="$(shasum -a 256 "$B/$id/shots-a/"*.ppm | awk '{print $1}' | sort -u | wc -l | tr -d ' ')"
  nfiles="$(ls "$B/$id/shots-a" | wc -l | tr -d ' ')"
  [ "$ndistinct" = "$nfiles" ] || fail "flow $id: shots not pairwise distinct ($ndistinct unique of $nfiles) — a stuck screen machine renders duplicates"
  echo "    -> flow $id OK (trace frozen-match, x2 stable, shots $nfiles)"
done
# 17 since DEVIATION D27 (was 19): the collapsed CONTROLS route makes f04's
# `menu-controls` and `controls-controller` shots unshootable, because those
# two screens are no longer reachable by navigation — the FLOW_SHOTS pin above
# carries the same measurement per flow, and this is its total.
[ "$total_shots" = 17 ] || fail "shot total $total_shots != pinned 17"
[ "$states" = 4 ] || fail "BRIDGE-STATE witness total $states != pinned 4"
[ "$tstates" = 2 ] || fail "TBRIDGE-STATE witness total $tstates != pinned 2"

# --- [4] match-launch bridges vs the frozen streams -----------------------------
echo "=== [4] launch bridges: f01 -> g01, f02 -> m01, f05 -> g03 (UNCHANGED verify-stream.js)"
# Whole-log BYTE-EXACT verdict whitelist (review-88 M4, the iter-86
# exact-token class in its strongest form): the expected verdict line is
# CONSTRUCTED from the frozen file's own counts and the captured
# verify-stream stdout must equal those exact bytes — foreign STREAM
# MATCH lines, torn lines, and a missing final newline are all
# structurally dead. Corpus-validated against the archived iter-88 logs
# (rngCalls 134/59; zero false rejections).
assert_stream_verdict() { # <vlog> <want-file> <ctx>
  local rc=0
  cmp -s "$1" "$2" || rc=$?
  if [ "$rc" = 1 ]; then
    grammar_die "bridge $3 — verify log is not BYTE-IDENTICAL to the constructed verdict line (a foreign/torn/newline-stripped verdict is CORRUPTION)"
  elif [ "$rc" -ge 2 ]; then
    grammar_die "bridge $3 — cmp rc $rc reading the verify log (corrupt evidence, never a pass)"
  fi
}
# review-90 M1: the judged verify log captures stdout AND stderr — a
# foreign stderr diagnostic lands in the byte-judged log and kills the
# whole-log equality (tooth T13 proves it through THIS function; the
# green corpus is stderr-quiet — validated vs the archived iter-90
# logs, zero false rejections, .loop/m4-foh91-dev.log).
verify_capture() { # <vlog> <cmd...>  (rc = the command's rc, pipefail)
  local vlog="$1"
  shift
  rm -f "$vlog"
  { "$@" 2>&1 | tee "$vlog"; } | relay_lines
}
# review-90 H(b): explicit full-run structural validation of EVERY
# wrapped run this check judges (bridges, control, AND the divergence
# witness — the witness gets the SAME validation as bridge runs):
# frames contiguous 1..N with N == the golden's count, every hash
# 64-lowercase-hex, integer coverage counters.
validate_run_shape() { # <run.json> <frames> <ctx>
  node -e '
    const fs = require("fs");
    const j = JSON.parse(fs.readFileSync(process.argv[1], "utf8"));
    const want = Number(process.argv[2]);
    if (!Number.isInteger(want) || want <= 0) {
      console.error("bad frames arg: " + process.argv[2]); process.exit(1);
    }
    if (!Array.isArray(j.frames) || j.frames.length !== want) {
      console.error("run has " +
        (Array.isArray(j.frames) ? j.frames.length : "no") +
        " frames, want exactly " + want); process.exit(1);
    }
    for (let i = 0; i < j.frames.length; i++) {
      const fr = j.frames[i];
      if (!fr || fr.f !== i + 1 || typeof fr.h !== "string" ||
          !/^[0-9a-f]{64}$/.test(fr.h)) {
        console.error("frame record " + (i + 1) +
          " malformed (want contiguous f + 64-hex h)"); process.exit(1);
      }
    }
    if (!j.coverage || !Number.isInteger(j.coverage.rngCalls) ||
        !Number.isInteger(j.coverage.rngCallsOutsideStep)) {
      console.error("coverage counters missing/non-integer");
      process.exit(1);
    }
  ' "$1" "$2" || fail "run shape $3: wrapped run JSON failed the full-run structural validation (review-90 H(b))"
}
bridges=0
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
  validate_run_shape "$B/$id.run.json" "$frames" "bridge $id"
  local vlog="$B/$id.verify.log"
  if ! verify_capture "$vlog" node oracle/harness/verify-stream.js \
      "$B/$id.run.json" "$frozen"; then
    fail "verify-stream rc != 0 for bridge $id (log: $vlog)"
  fi
  made "$vlog"
  local rng
  rng="$(node -e '
    const j = JSON.parse(require("fs").readFileSync(process.argv[1], "utf8"));
    console.log(String(j.rngCalls));
  ' "$frozen")" || fail "bridge $id — cannot read rngCalls from $frozen"
  [[ "$rng" =~ ^[0-9]{1,6}$ ]] || fail "bridge $id — frozen rngCalls '$rng' fails the anchored grammar"
  printf 'STREAM MATCH %s: %s/%s frames exact, rngCalls=%s, rngCallsOutsideStep=1, specVersion=1\n' \
    "$name" "$frames" "$frames" "$rng" > "$B/$id.verdict-want.txt"
  assert_stream_verdict "$vlog" "$B/$id.verdict-want.txt" "$id"
  bridges=$((bridges + 1))
  echo "    -> STREAM MATCH $name (FOH-launched, $frames frames pinned, rngCalls=$rng, whole-log byte-exact)"
}
judge_bridge f01-vs-g01 g01 "${ORACLE_NAMES[0]}" "$G01_FRAMES" \
  "oracle/goldens/${ORACLE_NAMES[0]}.sha256.json"
judge_bridge f02-cpu-m01 m01 "${M4_NAMES[0]}" "$M01_FRAMES" \
  "$M4G/${M4_NAMES[0]}.sha256.json" "$M4G/manifest.json"
judge_bridge f05-vs-g03 g03 "${ORACLE_NAMES[2]}" "$G03_FRAMES" \
  "oracle/goldens/${ORACLE_NAMES[2]}.sha256.json"
[ "$bridges" = 3 ] || fail "bridge ledger — $bridges/3 full-stream bridges judged"

# --- [4t] TARGET launch bridges (iter 99, M4 task 12): f06 -> t01,
# f07 -> t02 — the conventions' (c) bar EXCEEDED: FULL both-stream
# equality via wrap-target.js -> the UNCHANGED verify-stream.js (player
# plane) + verify-target-stream.js (target plane), each verdict log
# whole-log BYTE-EXACT against a verdict constructed from the frozen
# files' own counts (the [4] discipline).
tbridges=0
judge_tbridge() { # <flowId> <goldenId> <goldenName> <frames>
  local id="$1" gid="$2" name="$3" frames="$4"
  test -f "$M4G/$name.sha256.json" || fail "tbridge $id: frozen player stream missing"
  test -f "$M4G/$name.target.sha256.json" || fail "tbridge $id: frozen target stream missing"
  rm -f "$B/$id.player.json" "$B/$id.target.json"
  { node "$M4G/wrap-target.js" "$gid" "$B/$id/stream.txt" \
      "$B/$id.player.json" "$B/$id.target.json"; } 2>&1 | relay_lines
  made "$B/$id.player.json" "$B/$id.target.json"
  validate_run_shape "$B/$id.player.json" "$frames" "tbridge $id"
  local vlog="$B/$id.verify.log"
  if ! verify_capture "$vlog" node oracle/harness/verify-stream.js \
      "$B/$id.player.json" "$M4G/$name.sha256.json"; then
    fail "verify-stream rc != 0 for tbridge $id (log: $vlog)"
  fi
  made "$vlog"
  local rng
  rng="$(node -e '
    const j = JSON.parse(require("fs").readFileSync(process.argv[1], "utf8"));
    console.log(String(j.rngCalls));
  ' "$M4G/$name.sha256.json")" || fail "tbridge $id — cannot read rngCalls"
  [[ "$rng" =~ ^[0-9]{1,6}$ ]] || fail "tbridge $id — frozen rngCalls grammar ('$rng')"
  printf 'STREAM MATCH %s: %s/%s frames exact, rngCalls=%s, rngCallsOutsideStep=1, specVersion=1\n' \
    "$name" "$frames" "$frames" "$rng" > "$B/$id.verdict-want.txt"
  assert_stream_verdict "$vlog" "$B/$id.verdict-want.txt" "$id"
  # the TARGET plane (the SEPARATE stream + its own frozen file)
  local tvlog="$B/$id.tverify.log"
  if ! verify_capture "$tvlog" node "$M4G/verify-target-stream.js" \
      "$B/$id.target.json" "$M4G/$name.target.sha256.json"; then
    fail "verify-target-stream rc != 0 for tbridge $id (log: $tvlog)"
  fi
  made "$tvlog"
  local tfin tmin tetg
  read -r tfin tmin tetg <<< "$(node -e '
    const j = JSON.parse(require("fs").readFileSync(process.argv[1], "utf8"));
    console.log([j.params.finalTargetsDestroyed, j.params.minTargets,
      j.params.finalEndTargetGame].join(" "));
  ' "$M4G/$name.target.sha256.json")" || fail "tbridge $id — frozen target metadata read failed"
  [[ "$tfin" =~ ^(0|[1-9][0-9]?)$ ]] || fail "tbridge $id — finalTargetsDestroyed grammar ('$tfin')"
  [[ "$tmin" =~ ^([1-9]|10)$ ]] || fail "tbridge $id — minTargets grammar ('$tmin')"
  [ "$tetg" = false ] || fail "tbridge $id — frozen finalEndTargetGame is not false"
  printf 'TARGET STREAM MATCH %s: %s/%s target frames exact, targetsDestroyed=%s (>= minTargets %s), endTargetGame=false, sibling seal OK, manifest bound, specVersion=1\n' \
    "$name" "$frames" "$frames" "$tfin" "$tmin" > "$B/$id.tverdict-want.txt"
  assert_stream_verdict "$tvlog" "$B/$id.tverdict-want.txt" "$id-target"
  tbridges=$((tbridges + 1))
  echo "    -> TARGET STREAM MATCH $name (FOH-launched, BOTH streams, $frames frames, whole-log byte-exact x2)"
}
judge_tbridge f06-target-t01 t01 "$T01_NAME" "$T01_FRAMES"
judge_tbridge f07-target-t02 t02 "$T02_NAME" "$T02_FRAMES"
[ "$tbridges" = 2 ] || fail "tbridge ledger — $tbridges/2 target bridges judged"

# Variant generator (NO eval — the manifest-eval class stays dead):
# fixed operations over flow bytes, anchors must match exactly or the
# caller dies. Used by the [4w] control derivation and the [5] teeth.
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

# --- [4w] divergence witness + lcancel=0 control (review-88 H1; review-90 H) -----
echo "=== [4w] divergence witness (lcancel=1) + control (lcancel=0) vs frozen g01"
# Measured-then-frozen (AGENT-LOG iter 90 dev probe): lCancelType=1
# sets phys.lCancel=true for every player every frame
# (port/sim/physics.c:962-965) — first divergence from the frozen g01
# stream at this exact frame. Drift in this pin = a settings-plane or
# spec change = reviewed pin update, never a silent re-measure.
WIT_DIV_FRAME=1
# review-90 H(c): the lcancel=1 TREATMENT frame-1 hash,
# measured-then-frozen (iter-90 committed-check cold-run residue,
# re-confirmed by every run of this check; AGENT-LOG iter 91). The
# witness report's run hash MUST equal this pin — "some divergence at
# frame 1" (an options-path serialization defect, a corrupted stream)
# can no longer masquerade as the lcancel effect. REVIEWED RE-FREEZE
# required if oracle/CHECKSUM.md ever bumps specVersion or the
# settings plane changes — never a silent re-measure.
WIT_RUN_HASH_F1=9cd2843dd70fcef4cf29cb3a4c53d8fd29d70c6f3b02c5b633e3ff757e0ecb7f
[[ "$WIT_RUN_HASH_F1" =~ ^[0-9a-f]{64}$ ]] || fail "witness — treatment pin malformed"
# The report's frozen-side hash binds to the frozen g01 file's OWN
# frame entry, read mechanically (no independent literal).
WIT_FROZEN_HASH_F1="$(node -e '
  const j = JSON.parse(require("fs").readFileSync(process.argv[1], "utf8"));
  const n = Number(process.argv[2]);
  const fr = j.frames[n - 1];
  if (!fr || fr.f !== n) { console.error("bad frozen frame " + n); process.exit(1); }
  console.log(fr.h);
' "oracle/goldens/${ORACLE_NAMES[0]}.sha256.json" "$WIT_DIV_FRAME")" || fail "witness — cannot read the frozen frame-$WIT_DIV_FRAME hash from the g01 sha256.json"
[[ "$WIT_FROZEN_HASH_F1" =~ ^[0-9a-f]{64}$ ]] || fail "witness — frozen frame-$WIT_DIV_FRAME hash read malformed"
[ "$WIT_FROZEN_HASH_F1" != "$WIT_RUN_HASH_F1" ] || fail "witness — treatment pin EQUALS the frozen hash (a pin that cannot diverge is no pin)"
WIT=$B/wit
rm -rf "$WIT"
mkdir -p "$WIT"
# Check-owned synthetic flow (NOT a committed flow — the [0c] inventory
# pin stays 5): options detour turns lcancel 0->1 (ONE A press on the
# lcancel row, gameplaymenu.js:44-48), then g01's exact selections
# (fox via the CSS token gesture, P2 toggled to HMN, battlefield default).
# The CSS leg was re-authored with the CSS mechanics arc (MENU-SPEC items
# 1+2+3+4) — it is f01's leg verbatim, so the two stay in step; only the
# frames after the options detour moved, which is why the lcancel witness
# below still pins frame 415.
# review-91 H: the treatment and control share ONE basename (wit-g01),
# disambiguated by the sibling dirs wit/ vs ctrl/ only — the observable
# flow id is identical between the legs, so no bridge/sim path keying
# on the id can fake the treatment divergence while the control matches.
cat > "$WIT/wit-g01.flow" << 'WEOF'
FLOW1
# check-owned witness flow (iter 90, review-88 H1): lcancel=1 + g01
# params; the launched stream MUST diverge from the frozen g01 stream.
I 1 -
I 375 S
I 376 -
I 380 D
I 381 -
I 385 D
I 386 -
I 390 D
I 391 -
I 395 A
I 396 -
I 400 D
I 401 -
I 405 A
I 406 -
I 410 D
I 411 -
I 415 A
I 416 -
I 420 B
I 421 -
I 425 B
I 426 -
I 430 D
I 431 -
I 435 A
I 436 -
I 445 U
I 515 -
I 520 D
I 532 -
I 535 B
I 538 -
I 545 R
I 583 -
I 603 A
I 606 -
I 615 LD
I 720 -
I 725 R
I 758 -
I 763 U
I 799 -
I 805 A
I 808 -
I 820 S
I 823 -
I 830 A
I 833 -
END 840
WEOF
fresh_persist # defaults domain (task 13; the wit leg saves lcancel=1)
"$B/foh_app" --flow "$WIT/wit-g01.flow" --flow-out "$WIT/trace.txt" \
  --bridge verify --simdata "$B/simdata.txt" --seed "$G01_SEED" \
  --trace "$B/g01.trace.txt" --frames "$G01_FRAMES" --out "$WIT/stream.txt" \
  --bstate-out "$WIT/bstate.txt" 2>&1 | relay_lines
made "$WIT/trace.txt" "$WIT/stream.txt" "$WIT/bstate.txt"
{ node "$FOH/judge-foh-trace.js" "$WIT/trace.txt" wit-g01 1; } 2>&1 | relay_lines
# LAUNCH line EXACT: g01 cross-bound params + lcancel=1 (nothing else).
witlaunch="LAUNCH 830 p1=$G01_P1 p2=$G01_P2 p2type=0 difficulty=3 stage=$G01_STAGE turbo=0 lcancel=1 flashlcancel=0 walljump=0 tapjump=0,0,0,0 versus=0"
c="$(count_xl "$WIT/trace.txt" "$witlaunch")"
[ "$c" = 1 ] || fail "witness — LAUNCH line != g01-params-plus-lcancel=1 (count $c/1)"
# The settings-edit line itself (the control derivation below deletes
# exactly this edit; assert it is real in the treatment first).
c="$(count_xl "$WIT/trace.txt" "S 415 lcancel 1")"
[ "$c" = 1 ] || fail "witness — trace does not carry exactly one 'S 415 lcancel 1' settings edit (count $c/1)"
# BRIDGE-STATE EXACT: byte-equal to the FROZEN f01 witness with ONLY
# lcancel substituted (binds every other field to the frozen bytes).
sed 's/lcancel=0/lcancel=1/' "$FLOWS/f01-vs-g01.bstate.expect" > "$WIT/bstate-want.txt"
rc=0; cmp -s "$WIT/bstate-want.txt" "$FLOWS/f01-vs-g01.bstate.expect" || rc=$?
[ "$rc" = 1 ] || fail "witness — lcancel substitution in the frozen f01 bstate was a no-op (rc $rc)"
cmp "$WIT/bstate.txt" "$WIT/bstate-want.txt" || fail "witness — BRIDGE-STATE differs from frozen-f01-with-lcancel=1 (settings did not reach the GameState slice)"
{ node "$SIM/wrap-run.js" g01 "$WIT/stream.txt" "$WIT/run.json"; } 2>&1 | relay_lines
made "$WIT/run.json"
# review-90 H(b): the witness run passes the SAME full-run structural
# validation as the bridge runs.
validate_run_shape "$WIT/run.json" "$G01_FRAMES" witness
# The witness judgment: verify-stream vs frozen g01 MUST report a
# per-frame divergence (rc 2) at the PINNED frame; a MATCH = DEATH.
witness_judge() { # <wlog> <rc>
  local wlog="$1" rc="$2"
  if [ "$rc" = 0 ]; then
    fail "divergence witness UNSOUND — verify-stream MATCHED the frozen g01 stream with lcancel=1 (the FOH-fed settings plane did NOT reach ticking; review-88 H1 fail-closed arm)"
  fi
  [ "$rc" = 2 ] || fail "divergence witness — verify-stream rc $rc, want exactly 2 (per-frame divergence class; any other class is the wrong failure)"
  # whole-log exact grammar: exactly 3 lines + final newline
  [ "$(tail -c 1 "$wlog" | od -An -c | tr -d ' \n')" = '\n' ] || grammar_die "witness — divergence report missing its final newline (torn write)"
  local nl
  nl="$(wc -l < "$wlog" | tr -d ' ')"
  [ "$nl" = 3 ] || grammar_die "witness — divergence report has $nl lines, want exactly 3"
  local l1
  l1="$(head -n 1 "$wlog")"
  [ "$l1" = "STREAM MISMATCH: first divergence at frame $WIT_DIV_FRAME of $G01_FRAMES" ] || grammar_die "witness — first line '$l1' != the pinned divergence report (frame $WIT_DIV_FRAME of $G01_FRAMES)"
  c="$(count_e "$wlog" '^  frozen: [0-9a-f]{64}$')"
  [ "$c" = 1 ] || grammar_die "witness — frozen-hash line count $c/1"
  c="$(count_e "$wlog" '^  run:    [0-9a-f]{64}$')"
  [ "$c" = 1 ] || grammar_die "witness — run-hash line count $c/1"
  local hf hr
  hf="$(sed -n '2s/^  frozen: //p' "$wlog")"
  hr="$(sed -n '3s/^  run:    //p' "$wlog")"
  [ "$hf" != "$hr" ] || grammar_die "witness — frozen and run hashes are EQUAL in a divergence report (corrupt report)"
  # review-90 H(c): both hashes are BOUND, not merely unequal.
  [ "$hf" = "$WIT_FROZEN_HASH_F1" ] || grammar_die "witness — reported frozen hash != the frozen g01 frame-$WIT_DIV_FRAME entry (the report is not derived from the pinned golden)"
  [ "$hr" = "$WIT_RUN_HASH_F1" ] || grammar_die "witness — reported run hash != the pinned lcancel=1 treatment hash (the frame-$WIT_DIV_FRAME divergence is NOT the pinned lcancel effect; reviewed re-freeze only with a spec/settings-plane change)"
}
diverge=0
wrc=0
node oracle/harness/verify-stream.js "$WIT/run.json" \
  "oracle/goldens/${ORACLE_NAMES[0]}.sha256.json" > "$WIT/verify.log" 2>&1 || wrc=$?
made "$WIT/verify.log"
witness_judge "$WIT/verify.log" "$wrc"
diverge=1
echo "    -> DIVERGENCE WITNESS OK: lcancel=1 diverges frozen g01 at frame $WIT_DIV_FRAME (rc 2, exact 3-line report, both hashes pinned)"

# CONTROL (review-90 H(a)): the SAME options-path flow with ONLY the
# lcancel press pair deleted — derived MECHANICALLY from the witness
# flow bytes (mkvariant delete), so "all other steps identical" holds
# by construction. lcancel stays 0; the launched stream MUST fully
# MATCH the frozen g01 stream (rc 0 + the same whole-log byte-exact
# constructed verdict as the f01 bridge). An options-path
# boot/serialization defect would break THIS leg, so the witness
# divergence above is attributable to the lcancel setting itself.
CTRL=$B/ctrl
rm -rf "$CTRL"
mkdir -p "$CTRL"
mkvariant "$WIT/wit-g01.flow" "$CTRL/wit-g01.flow" delete \
  "I 415 A" "I 416 -"
fresh_persist # defaults domain (task 13; the wit save must not leak here)
"$B/foh_app" --flow "$CTRL/wit-g01.flow" --flow-out "$CTRL/trace.txt" \
  --bridge verify --simdata "$B/simdata.txt" --seed "$G01_SEED" \
  --trace "$B/g01.trace.txt" --frames "$G01_FRAMES" --out "$CTRL/stream.txt" \
  --bstate-out "$CTRL/bstate.txt" 2>&1 | relay_lines
made "$CTRL/trace.txt" "$CTRL/stream.txt" "$CTRL/bstate.txt"
{ node "$FOH/judge-foh-trace.js" "$CTRL/trace.txt" wit-g01 1; } 2>&1 | relay_lines
# Behavioral same-path binding (review-91 H: the header id is SHARED —
# no normalization): the control trace must equal the witness trace
# with EXACTLY the lcancel settings-edit line dropped and the LAUNCH
# lcancel field 1->0 — nothing else, header included, may differ
# between the two runs' emitted machines.
sed -e '/^S 415 lcancel 1$/d' \
    -e '/^LAUNCH 830 /s/ lcancel=1 / lcancel=0 /' \
  "$WIT/trace.txt" > "$CTRL/trace-want.txt"
made "$CTRL/trace-want.txt"
cmp "$CTRL/trace.txt" "$CTRL/trace-want.txt" || fail "control — trace != witness-trace-minus-the-lcancel-edit (the treatment and control did NOT share the options path)"
ctrllaunch="LAUNCH 830 p1=$G01_P1 p2=$G01_P2 p2type=0 difficulty=3 stage=$G01_STAGE turbo=0 lcancel=0 flashlcancel=0 walljump=0 tapjump=0,0,0,0 versus=0"
c="$(count_xl "$CTRL/trace.txt" "$ctrllaunch")"
[ "$c" = 1 ] || fail "control — LAUNCH line != g01-params-with-lcancel=0 (count $c/1)"
# Pure-defaults GameState: byte-equal to the FROZEN f01 witness.
cmp "$CTRL/bstate.txt" "$FLOWS/f01-vs-g01.bstate.expect" || fail "control — BRIDGE-STATE differs from the frozen f01 witness (the control must reach GameState with pure defaults)"
{ node "$SIM/wrap-run.js" g01 "$CTRL/stream.txt" "$CTRL/run.json"; } 2>&1 | relay_lines
made "$CTRL/run.json"
validate_run_shape "$CTRL/run.json" "$G01_FRAMES" control
control_judge() { # <vlog> <rc>
  local vlog="$1" rc="$2"
  if [ "$rc" != 0 ]; then
    fail "lcancel=0 CONTROL DIVERGED from frozen g01 (verify-stream rc $rc) — the options path itself is NOT stream-clean; the [4w] witness divergence can no longer be attributed to lcancel (review-90 H refutation arm: STOP, this is a real options-path bug)"
  fi
  assert_stream_verdict "$vlog" "$B/f01-vs-g01.verdict-want.txt" control
}
control=0
crc=0
verify_capture "$CTRL/verify.log" node oracle/harness/verify-stream.js \
  "$CTRL/run.json" "oracle/goldens/${ORACLE_NAMES[0]}.sha256.json" || crc=$?
made "$CTRL/verify.log"
control_judge "$CTRL/verify.log" "$crc"
control=1
echo "    -> CONTROL OK: the same options path with lcancel=0 fully MATCHES frozen g01 (whole-log byte-exact)"

# --- [5] TEETH (standing; generated variants/copies only) ----------------------
echo "=== [5] teeth (pre-registered T1-T10 AGENT-LOG iter 90; T11-T14 iter 91; T15-T16 iter 92; T17-T18 iter 99; T19-T22 the CSS mechanics arc)"
teeth=0
run_variant() { # <flow-file> <out-trace>
  rm -f "$2"
  fresh_persist # defaults domain (task 13)
  "$B/foh_app" --flow "$1" --flow-out "$2" 2>&1 | relay_lines
  made "$2"
}
# T1 nav-perturb (review-88 L1 form): the variant lives at the SAME
# BASENAME (own dir) so the emitted header is byte-identical to f01's —
# ONLY the injected DOWN differs; divergence is asserted at the exact
# first divergent TRANSITION line pair, never just cmp!=0.
mkdir -p "$B/t1"
mkvariant "$FLOWS/f01-vs-g01.flow" "$B/t1/f01-vs-g01.flow" insert-after \
  "I 376 -" "I 377 D" "I 378 -"
run_variant "$B/t1/f01-vs-g01.flow" "$B/t1.trace.txt"
rc=0; cmp -s "$B/t1.trace.txt" "$FLOWS/f01-vs-g01.expect" || rc=$?
[ "$rc" = 1 ] || fail "T1 — nav-perturb variant trace cmp rc $rc, want exactly 1 (transition trace must be input-driven)"
node -e '
  const fs = require("fs");
  const a = fs.readFileSync(process.argv[1], "utf8").split("\n"); // frozen
  const b = fs.readFileSync(process.argv[2], "utf8").split("\n"); // variant
  if (a[0] !== b[0]) {
    console.error("headers differ — divergence would be metadata (the L1 class)");
    process.exit(1);
  }
  let i = 0;
  while (i < a.length && i < b.length && a[i] === b[i]) i++;
  if (a[i] !== "T 380 menu-top css a" ||
      b[i] !== "T 380 menu-top target-select a") {
    console.error("first divergent pair is not the injected-DOWN transition: " +
                  "frozen=" + JSON.stringify(a[i]) + " variant=" +
                  JSON.stringify(b[i]));
    process.exit(1);
  }
' "$FLOWS/f01-vs-g01.expect" "$B/t1.trace.txt" || fail "T1 — first-divergent-line witness failed"
echo "    T1 OK: same header, first divergent pair = injected-DOWN transition (T 380 css vs target-select — row 0 goes straight to the CSS under C5)"
teeth=$((teeth + 1))
# T2 char variant: carry the token one cell further -> p1=3 in LAUNCH.
# Deleting the RIGHT release lets the hold run to the drop press instead
# of stopping at fox, so the token lands on cell 3 (falco) — the same
# assertion as before (the launch record must be selection-driven), now
# expressed in the gesture the screen actually has (MENU-SPEC §2.5).
mkdir -p "$B/t2"
mkvariant "$FLOWS/f01-vs-g01.flow" "$B/t2/f01-vs-g01.flow" delete \
  "I 528 -"
run_variant "$B/t2/f01-vs-g01.flow" "$B/t2.trace.txt"
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
# T5 (review-88 M5+M6): three death arms through the PRODUCTION
# judge_shot_pair — no hard-coded twin comparison.
# (a) comparator arm: structurally-valid B copy with ONE payload byte
# flipped -> the A/B cmp arm dies.
node -e '
  const fs = require("fs");
  const b = fs.readFileSync(process.argv[1]);
  b[b.length - 1] ^= 0xff; // last payload byte: size/header preserved
  fs.writeFileSync(process.argv[2], b);
' "$B/f01-vs-g01/shots-a/css.ppm" "$B/t5-flip.ppm"
made "$B/t5-flip.ppm"
rc=0
( judge_shot_pair "t5a" "$B/f01-vs-g01/shots-a/css.ppm" "$B/t5-flip.ppm" ) \
  > "$B/t5a.log" 2>&1 || rc=$?
[ "$rc" != 0 ] || fail "T5a — the production comparator ACCEPTED a payload-flipped B copy"
c="$(count_x "$B/t5a.log" "not byte-identical")"
[ "$c" = 1 ] || fail "T5a — comparator death message class missing"
# (b) structural arm: truncated payload dies.
head -c 1000 "$B/f01-vs-g01/shots-a/css.ppm" > "$B/t5-trunc.ppm"
rc=0
( judge_shot_pair "t5b" "$B/f01-vs-g01/shots-a/css.ppm" "$B/t5-trunc.ppm" ) \
  > "$B/t5b.log" 2>&1 || rc=$?
[ "$rc" != 0 ] || fail "T5b — the structural arm ACCEPTED a truncated shot"
c="$(count_x "$B/t5b.log" "truncated or trailing bytes")"
[ "$c" = 1 ] || fail "T5b — truncation death message class missing"
# (c) structural arm: doctored header dimensions die.
node -e '
  const fs = require("fs");
  const b = fs.readFileSync(process.argv[1]);
  const s = b.toString("latin1").replace("P6\n240 240\n", "P6\n239 240\n");
  fs.writeFileSync(process.argv[2], Buffer.from(s, "latin1"));
' "$B/f01-vs-g01/shots-a/css.ppm" "$B/t5-dims.ppm"
made "$B/t5-dims.ppm"
rc=0
( judge_shot_pair "t5c" "$B/f01-vs-g01/shots-a/css.ppm" "$B/t5-dims.ppm" ) \
  > "$B/t5c.log" 2>&1 || rc=$?
[ "$rc" != 0 ] || fail "T5c — the structural arm ACCEPTED doctored header dimensions"
c="$(count_x "$B/t5c.log" "shot header is not exactly")"
[ "$c" = 1 ] || fail "T5c — header death message class missing"
echo "    T5 OK: flip/truncate/dims copies all die in the PRODUCTION judge_shot_pair"
teeth=$((teeth + 1))
# T6 difficulty variant: end the knob drag early -> d2. Inserting a release
# 6 frames into the LEFT hold stops the hand at x = 81.60 — inside level 2's
# band [78,90) and deliberately NOT on level 2's stop (84.00).
#
# HONEST LIMIT (do not restate this as a continuity witness): landing off-stop
# does NOT prove the slider is continuous. This tooth judges the structural
# trace, and an implementation that snapped x = 81.60 to the level-2 stop at
# 84.00 would produce the identical trace and launch record. The slider's
# continuity (css.js:324 stores the raw hand x) is implemented and reviewed
# but is NOT witnessed by any host check here — it is a render-plane position,
# and the shot arm only compares run A against run B, never against a frozen
# reference. Registered as an unwitnessed property for the device lane. The
# flow's own later release then does nothing. Same assertion as before — the
# slider must be load-bearing on the launch record — in the drag gesture
# (MENU-SPEC §2.8, DEVIATION D7).
mkdir -p "$B/t6"
mkvariant "$FLOWS/f02-cpu-m01.flow" "$B/t6/f02-cpu-m01.flow" insert-after \
  "I 980 L" "I 986 -"
run_variant "$B/t6/f02-cpu-m01.flow" "$B/t6.trace.txt"
rc=0; cmp -s "$B/t6.trace.txt" "$FLOWS/f02-cpu-m01.expect" || rc=$?
[ "$rc" = 1 ] || fail "T6 — difficulty-variant trace cmp rc $rc, want exactly 1"
c="$(count_x "$B/t6.trace.txt" "difficulty=2")"
[ "$c" = 1 ] || fail "T6 — variant LAUNCH does not carry difficulty=2 (slider steps must be load-bearing)"
echo "    T6 OK: fewer LEFTs land difficulty=2 in LAUNCH and diverge the trace"
teeth=$((teeth + 1))
# T7 (review-88 H1 fail-closed): the witness judge fed a synthesized
# MATCH (rc 0 + the exact STREAM MATCH line) MUST die naming the
# unsound-witness class.
printf 'STREAM MATCH %s: %s/%s frames exact, rngCalls=134, rngCallsOutsideStep=1, specVersion=1\n' \
  "${ORACLE_NAMES[0]}" "$G01_FRAMES" "$G01_FRAMES" > "$B/t7.log"
rc=0
( witness_judge "$B/t7.log" 0 ) > "$B/t7.out" 2>&1 || rc=$?
[ "$rc" != 0 ] || fail "T7 — witness_judge ACCEPTED a MATCH (the H1 fail-closed arm is dead)"
c="$(count_x "$B/t7.out" "divergence witness UNSOUND")"
[ "$c" = 1 ] || fail "T7 — unsound-witness death message class missing"
echo "    T7 OK: a MATCH fed to the witness judge dies with the UNSOUND class"
teeth=$((teeth + 1))
# T8 (review-88 M1): delete the f04 sss->css B press -> the frozen
# sss->css edge line 'T 750 sss css b' is the first divergent frozen line.
# (Anchors moved iter 93: the f04 sss segment gained the RANDOM-slot
# traversal — designed re-freeze channel, AGENT-LOG iter 93.)
mkdir -p "$B/t8"
mkvariant "$FLOWS/f04-nav.flow" "$B/t8/f04-nav.flow" delete \
  "I 750 B" "I 753 -"
run_variant "$B/t8/f04-nav.flow" "$B/t8.trace.txt"
rc=0; cmp -s "$B/t8.trace.txt" "$FLOWS/f04-nav.expect" || rc=$?
[ "$rc" = 1 ] || fail "T8 — sss->css edge variant trace cmp rc $rc, want exactly 1"
node -e '
  const fs = require("fs");
  const a = fs.readFileSync(process.argv[1], "utf8").split("\n"); // frozen
  const b = fs.readFileSync(process.argv[2], "utf8").split("\n"); // variant
  if (a[0] !== b[0]) { console.error("headers differ (L1 class)"); process.exit(1); }
  let i = 0;
  while (i < a.length && i < b.length && a[i] === b[i]) i++;
  if (a[i] !== "T 750 sss css b") {
    console.error("first divergent frozen line is not the 15th edge: " +
                  JSON.stringify(a[i]));
    process.exit(1);
  }
' "$FLOWS/f04-nav.expect" "$B/t8.trace.txt" || fail "T8 — first-divergent-line witness failed"
echo "    T8 OK: dropping the B press diverges exactly at the frozen 'T 750 sss css b' edge"
teeth=$((teeth + 1))
# T19 (CSS mechanics arc): P1 CAN be set to CPU on the CSS — togglePort has
# no port-0 special case (main.js:504-520) — but the LAUNCH plane cannot honour
# it (sim_setup_match pins types[0]=0), so START must REFUSE loudly instead of
# booting a human P1 while the record claims otherwise. This tooth proves the
# guard fires, and it is what lets the LAUNCH grammar keep its [01] type
# columns: an unlaunchable port configuration never reaches the record.
# Check-owned flow (not a committed one — the [0c] inventory pin is untouched);
# the committed flows have no room to insert a 25-frame cursor move.
mkdir -p "$B/t19"
cat > "$B/t19/t19-p1cpu.flow" << 'T19EOF'
FLOW1
# check-owned: reach the CSS, make port 2 HMN, then make port 1 CPU and
# press START. Clamp-anchored exactly like the committed flows.
I 1 -
I 375 S
I 376 -
I 380 A
I 381 -
I 390 LD
I 495 -
I 500 R
I 533 -
I 538 U
I 574 -
I 580 A
I 583 -
I 590 L
I 615 -
I 620 A
I 623 -
I 630 S
I 633 -
END 640
T19EOF
run_variant "$B/t19/t19-p1cpu.flow" "$B/t19.trace.txt"
# The PRODUCTION whole-trace whitelist judge runs FIRST (PROCESS §3,
# fail-closed): counting selected lines alone would mint this tooth from a
# trace truncated before END, or one carrying foreign/off-graph lines.
# Launch expectation 0 — a CPU P1 must refuse, so no launch record may exist.
{ node "$FOH/judge-foh-trace.js" "$B/t19.trace.txt" t19-p1cpu 0; } 2>&1 | relay_lines
c="$(count_e "$B/t19.trace.txt" '^S [0-9]+ p1type 1$')"
[ "$c" = 1 ] || fail "T19 — the flow did not set P1 to CPU (count $c/1); the port-0 type box is not clickable"
c="$(count_e "$B/t19.trace.txt" '^S [0-9]+ refused portconfig$')"
[ "$c" = 1 ] || fail "T19 — no 'refused portconfig' with a CPU P1 (count $c/1); the launch-plane guard is dead"
c="$(count_x "$B/t19.trace.txt" "LAUNCH ")"
[ "$c" = 0 ] || fail "T19 — LAUNCHED with a CPU P1 ($c launch lines); the record would lie about the match"
c="$(count_x "$B/t19.trace.txt" "css sss start")"
[ "$c" = 0 ] || fail "T19 — the refused START still crossed to the SSS"
echo "    T19 OK: a CPU P1 is reachable on the CSS and refuses at the launch seam"
teeth=$((teeth + 1))
# T20/T21 (CSS mechanics arc): flow-to-fkscript.js's two REFUSALS must fire.
# The device injector is wall-clock scheduled, and the free cursor made a
# direction's held DURATION semantic, so the translator refuses (a) any
# direction press too short to be authored safely — it must never guess that
# one is an edge tap and stretch it — and (b) any two intervals on the same
# PHYSICAL keysym that overlap, which is reachable because an authored `Q` and
# a SHOT marker are both `q`. Both are generated flows; a translator that
# accepts either has a dead guard.
FKT="$B/fkteeth"
rm -rf "$FKT"; mkdir -p "$FKT"
# FAIL-CLOSED refusal assert (PROCESS §3: propagate the RC, never `|| true`,
# and judge a FULL-LINE grammar rather than a floating substring). A refusal
# must be all four of: exit code EXACTLY 2 (the translator's die()), NO output
# file written at all (not merely an empty one), stderr of EXACTLY ONE line,
# and that line anchored to the producer's own prefix carrying the class. A
# translator that crashed, or that half-wrote a script, or that printed a
# stack trace, would otherwise mint the tooth without refusing anything.
# FAIL-CLOSED refusal assert (PROCESS §3: propagate the RC, never `|| true`,
# and judge the WHOLE diagnostic, not a floating substring). A refusal must be
# all three of: exit code EXACTLY 2 (the translator's die()), NO output file
# written at all (not merely an empty one), and stderr BYTE-IDENTICAL to the
# pinned diagnostic below — one line, newline-terminated, nothing around it.
#
# Byte equality rather than a pattern is deliberate. A `case` glob like
# `"flow-to-fkscript: "*"$class"*` accepts unrestricted text on both sides, so
# a refusal for the WRONG reason that merely mentions the phrase would mint
# the tooth; and probing the final byte through `$(tail -c 1 ...)` cannot see
# a trailing NUL and swallows a read failure. `cmp` against a file has neither
# hole. These messages are deterministic (their numbers are pure functions of
# the generated flow), so pinning them is the same discipline as every other
# frozen artifact here: a reworded diagnostic fails loudly and is re-pinned.
fk_refuses() { # <name> [keymap]
  local rc=0
  rm -f "$FKT/$1.fks" "$FKT/$1.err"
  set +e
  node "$FOH/flow-to-fkscript.js" "$FKT/$1.flow" "$FKT/$1.fks" \
    ${2:+"$2"} >/dev/null 2>"$FKT/$1.err"
  rc=$?
  set -e
  [ "$rc" = 2 ] || fail "$1 — flow-to-fkscript exited $rc, want exactly 2 (its refusal code)"
  [ ! -e "$FKT/$1.fks" ] || fail "$1 — flow-to-fkscript WROTE an output file while refusing"
  made "$FKT/$1.err" "$FKT/$1.want"
  cmp "$FKT/$1.err" "$FKT/$1.want" \
    || fail "$1 — refusal diagnostic is not byte-identical to the pinned one"
}
cat > "$FKT/shortdir.flow" << 'FKEOF'
FLOW1
I 1 -
I 375 R
I 377 -
END 400
FKEOF
printf '%s\n' "flow-to-fkscript: direction 'R' at t=8283 ms is held 34 ms (2 flow frames) — shorter than the 3-frame minimum. A direction's DURATION is semantic on the free-pointer screens (css.js:195-196), so this translator will not stretch it: author the press as at least 3 flow frames." > "$FKT/shortdir.want"
fk_refuses shortdir
echo "    T20 OK: a 2-frame direction press is refused, never silently widened"
teeth=$((teeth + 1))
cat > "$FKT/qoverlap.flow" << 'FKEOF'
FLOW1
I 1 -
I 375 Q
I 379 -
SHOT 376 overlap
END 400
FKEOF
printf '%s\n' "flow-to-fkscript: keysym 'q' intervals overlap: press Q [8283,8350) then marker overlap [8300,8340) — the second press would emit a down edge with no release between. Separate them by at least 3 flow frames." > "$FKT/qoverlap.want"
fk_refuses qoverlap
echo "    T21 OK: an authored Q overlapping a SHOT marker is refused (same keysym)"
teeth=$((teeth + 1))
# T21b: the SAME overlap reached through a NON-default keymap. The SHOT
# marker is always PHYSICAL q, while an authored letter goes through the
# keymap — so validating the marker as `LETTER["Q"]` instead of the physical
# symbol agrees with emission only under the committed mapping. With `a` and
# `menu` swapped, an authored A IS physical q: the old code compared it
# against LETTER["Q"] == "a", found no overlap, and emitted `d q` twice with
# the marker's edge swallowed. This tooth is the one that separates the two
# implementations; T21 alone cannot.
{
  printf 'KEYMAP1\n'
  printf 'map up U u\nmap down D d\nmap left L l\nmap right R r\n'
  printf 'map a A q\nmap b B b\nmap x X x\nmap y Y y\n'
  printf 'map start S s\nmap l K k\nmap r N n\nmap menu Q a\n'
} > "$FKT/swap-aq.keymap"
cat > "$FKT/aqoverlap.flow" << 'FKEOF'
FLOW1
I 1 -
I 375 A
I 379 -
SHOT 376 overlap
END 400
FKEOF
printf '%s\n' "flow-to-fkscript: keysym 'q' intervals overlap: press A [8283,8350) then marker overlap [8300,8340) — the second press would emit a down edge with no release between. Separate them by at least 3 flow frames." > "$FKT/aqoverlap.want"
fk_refuses aqoverlap "$FKT/swap-aq.keymap"
echo "    T21b OK: under a swapped keymap the marker's PHYSICAL keysym still catches the overlap"
teeth=$((teeth + 1))
# And every COMMITTED flow must still translate — the guards above must not
# have made the real scripts underivable (round-2 regression: widening put
# f01's START release onto a marker instant and the translator exited 2).
for fid in "${FLOW_IDS[@]}"; do
  rm -f "$FKT/$fid.fks"
  node "$FOH/flow-to-fkscript.js" "$FLOWS/$fid.flow" "$FKT/$fid.fks" \
    >/dev/null 2>&1 || fail "flow $fid does not translate to an fk_input script"
  made "$FKT/$fid.fks"
done
echo "    fkscript OK: all ${#FLOW_IDS[@]} committed flows translate"
# T22 (CSS mechanics arc, review round 5): readyToFight is the DRAW pass's,
# and the draw pass belongs to the screen the tick ENDS on — upstream's
# drawCSS runs from a separate rAF loop dispatching on the CURRENT gameMode
# (main.js:1153-1183 at the pin). The reachable consequence: B and START on the
# SAME frame grabs your token AND launches on the previous frame's stale ready
# value (css.js:209-215 then :446-451); the SSS's B-back sets mode 2, so the
# RETURN frame's drawCSS recomputes with the token still held and the screen is
# NOT ready — a second START must launch nothing. Getting this wrong (keeping
# the recompute inside the CSS step) let a held token survive the round trip
# and relaunch.
mkdir -p "$B/t22"
cat > "$B/t22/t22-heldtoken.flow" << 'T22EOF'
FLOW1
# check-owned: make port 2 HMN (ready), drop the hand into the roster band,
# then press B+START on one frame — the token is grabbed and the stale-ready
# START still launches. B back out of the SSS, then press START again.
I 1 -
I 375 S
I 376 -
I 380 A
I 381 -
I 390 LD
I 495 -
I 500 R
I 533 -
I 538 U
I 574 -
I 580 A
I 583 -
I 590 U
I 660 -
I 665 D
I 677 -
I 685 BS
I 690 -
I 700 B
I 703 -
I 715 S
I 718 -
END 730
T22EOF
run_variant "$B/t22/t22-heldtoken.flow" "$B/t22.trace.txt"
# Whole-trace judge first, same reason as T19. Launch expectation 0: the
# stale-ready START crosses to the SSS but this flow never presses A there,
# so no LAUNCH RECORD is ever written — the thing under test is the css->sss
# transition count, asserted below.
{ node "$FOH/judge-foh-trace.js" "$B/t22.trace.txt" t22-heldtoken 0; } 2>&1 | relay_lines
c="$(count_e "$B/t22.trace.txt" '^S [0-9]+ carry 0$')"
[ "$c" = 1 ] || fail "T22 — the B+START frame did not grab a token (carry 0 count $c/1)"
c="$(count_x "$B/t22.trace.txt" "css sss start")"
[ "$c" = 1 ] || fail "T22 — want exactly 1 css->sss launch (the stale-ready one), got $c; a second means the return frame did NOT recompute readiness with the token held"
c="$(count_x "$B/t22.trace.txt" "sss css b")"
[ "$c" = 1 ] || fail "T22 — the SSS B-back did not fire (count $c/1)"
echo "    T22 OK: a token held across CSS->SSS->CSS un-readies the screen on return"
teeth=$((teeth + 1))
# T9 (review-88 M2): carry P2's token one cell further -> p2=3 in LAUNCH.
# Same shape as T2: delete the RIGHT release so the carry runs on to
# cell 3 (falco) before the drop press.
mkdir -p "$B/t9"
mkvariant "$FLOWS/f05-vs-g03.flow" "$B/t9/f05-vs-g03.flow" delete \
  "I 984 -"
run_variant "$B/t9/f05-vs-g03.flow" "$B/t9.trace.txt"
rc=0; cmp -s "$B/t9.trace.txt" "$FLOWS/f05-vs-g03.expect" || rc=$?
[ "$rc" = 1 ] || fail "T9 — p2-variant trace cmp rc $rc, want exactly 1"
c="$(count_x "$B/t9.trace.txt" "p2=3")"
[ "$c" = 1 ] || fail "T9 — variant LAUNCH does not carry p2=3 (p2 selection must be load-bearing)"
echo "    T9 OK: extra RIGHT lands p2=3 in LAUNCH and diverges the trace"
teeth=$((teeth + 1))
# T10 (review-88 M4): doctored verdict-log copies die in the extracted
# byte-equality assert — (a) appended foreign STREAM MATCH line,
# (b) stripped final newline.
cp "$B/f01-vs-g01.verify.log" "$B/t10a.log"
printf 'STREAM MATCH g99-foreign-golden: 3600/3600 frames exact, rngCalls=999, rngCallsOutsideStep=1, specVersion=1\n' >> "$B/t10a.log"
rc=0
( assert_stream_verdict "$B/t10a.log" "$B/f01-vs-g01.verdict-want.txt" t10a ) \
  > "$B/t10a.out" 2>&1 || rc=$?
[ "$rc" != 0 ] || fail "T10a — a foreign STREAM MATCH line PASSED the whole-log byte-equality"
c="$(count_x "$B/t10a.out" "not BYTE-IDENTICAL")"
[ "$c" = 1 ] || fail "T10a — byte-equality death message class missing"
printf '%s' "$(cat "$B/f01-vs-g01.verify.log")" > "$B/t10b.log"
rc=0
( assert_stream_verdict "$B/t10b.log" "$B/f01-vs-g01.verdict-want.txt" t10b ) \
  > "$B/t10b.out" 2>&1 || rc=$?
[ "$rc" != 0 ] || fail "T10b — a newline-stripped verdict PASSED the whole-log byte-equality"
c="$(count_x "$B/t10b.out" "not BYTE-IDENTICAL")"
[ "$c" = 1 ] || fail "T10b — byte-equality death message class missing"
echo "    T10 OK: foreign-line and torn-newline verdict logs die in assert_stream_verdict"
teeth=$((teeth + 1))
# T11 (review-90 H(c)): flip the leading nibble of the run-hash line in
# a COPY of the witness report -> witness_judge MUST die on the
# treatment pin (grammar otherwise intact: rc 2, 3 lines, pinned frame).
node -e '
  const fs = require("fs");
  const lines = fs.readFileSync(process.argv[1], "utf8").split("\n");
  const m = /^  run:    ([0-9a-f]{64})$/.exec(lines[2]);
  if (!m) { console.error("no run-hash line in the witness report"); process.exit(1); }
  const h = m[1];
  lines[2] = "  run:    " + (h[0] === "0" ? "1" : "0") + h.slice(1);
  fs.writeFileSync(process.argv[2], lines.join("\n"));
' "$WIT/verify.log" "$B/t11.log" || fail "T11 — variant generation failed"
made "$B/t11.log"
rc=0
( witness_judge "$B/t11.log" 2 ) > "$B/t11.out" 2>&1 || rc=$?
[ "$rc" != 0 ] || fail "T11 — witness_judge ACCEPTED a corrupted frame-1 run hash (the treatment pin is dead)"
c="$(count_x "$B/t11.out" "pinned lcancel=1 treatment hash")"
[ "$c" = 1 ] || fail "T11 — treatment-pin death message class missing"
echo "    T11 OK: a nibble-flipped run hash dies on the treatment pin"
teeth=$((teeth + 1))
# T12 (review-90 H(a)): the control judge fed the witness's own
# DIVERGENCE report + rc 2 MUST die naming the CONTROL-DIVERGED class
# (the H refutation arm has teeth).
rc=0
( control_judge "$WIT/verify.log" 2 ) > "$B/t12.out" 2>&1 || rc=$?
[ "$rc" != 0 ] || fail "T12 — control_judge ACCEPTED a diverging control (the H refutation arm is dead)"
c="$(count_x "$B/t12.out" "CONTROL DIVERGED")"
[ "$c" = 1 ] || fail "T12 — control-diverged death message class missing"
echo "    T12 OK: a divergence fed to the control judge dies with the CONTROL DIVERGED class"
teeth=$((teeth + 1))
# T13 (review-90 M1): a wrapper injecting ONE stderr line around the
# REAL verify-stream, run through the PRODUCTION verify_capture on the
# green f01 run: the underlying verify passes (rc 0), the injected line
# MUST land in the judged vlog, and the byte-equality MUST die.
cat > "$B/t13-wrap.sh" << 'TEOF'
#!/usr/bin/env bash
echo "WARN: foreign stderr diagnostic (T13)" >&2
exec node oracle/harness/verify-stream.js "$@"
TEOF
chmod +x "$B/t13-wrap.sh"
rc=0
verify_capture "$B/t13.vlog" "$B/t13-wrap.sh" "$B/f01-vs-g01.run.json" \
  "oracle/goldens/${ORACLE_NAMES[0]}.sha256.json" || rc=$?
[ "$rc" = 0 ] || fail "T13 — the wrapped verify run rc $rc (the underlying verify must PASS; only stderr is foreign)"
made "$B/t13.vlog"
c="$(count_x "$B/t13.vlog" "foreign stderr diagnostic (T13)")"
[ "$c" = 1 ] || fail "T13 — the injected stderr line did NOT land in the judged log (the M1 capture fix is dead)"
rc=0
( assert_stream_verdict "$B/t13.vlog" "$B/f01-vs-g01.verdict-want.txt" t13 ) \
  > "$B/t13.out" 2>&1 || rc=$?
[ "$rc" != 0 ] || fail "T13 — a stderr-polluted verify log PASSED the whole-log byte-equality"
c="$(count_x "$B/t13.out" "not BYTE-IDENTICAL")"
[ "$c" = 1 ] || fail "T13 — byte-equality death message class missing"
echo "    T13 OK: an injected stderr line lands in the judged log and kills the byte-equality"
teeth=$((teeth + 1))
# T14 (review-90 M2): plant unexpected.ppm in a COPY of f01's shots-b
# inventory -> the PRODUCTION judge_shot_inventory MUST die.
rm -rf "$B/t14-shots"
mkdir -p "$B/t14-shots"
cp "$B/f01-vs-g01/shots-b/"*.ppm "$B/t14-shots/"
printf 'P6\n1 1\n255\nxyz' > "$B/t14-shots/unexpected.ppm"
made "$B/t14-shots/unexpected.ppm"
t14want="$(printf '%s\n' ${FLOW_SHOTS[0]} | sort | tr '\n' ' ' | sed 's/ $//')"
rc=0
( judge_shot_inventory "t14 (f01 shots-b copy)" "$B/t14-shots" "$t14want" ) \
  > "$B/t14.out" 2>&1 || rc=$?
[ "$rc" != 0 ] || fail "T14 — a planted unexpected.ppm PASSED the exact-set shot inventory"
c="$(count_x "$B/t14.out" "shot inventory")"
[ "$c" = 1 ] || fail "T14 — inventory death message class missing"
echo "    T14 OK: a planted unexpected.ppm dies in the production judge_shot_inventory"
teeth=$((teeth + 1))
# T15 (review-91 H): STREAM FLOW-ID INDEPENDENCE — the witness flow
# BYTES under a DIFFERENT basename must (a) emit a trace differing in
# EXACTLY the header line (the id IS observable — the probe is not
# vacuous) and (b) produce a byte-identical sim stream + BRIDGE-STATE,
# so nothing in the bridge/sim path keys on the observable flow id:
# the [4w] treatment pin is rename-invariant and the shared-id control
# is all-else-identical by MEASUREMENT, not assumption.
mkdir -p "$B/t15"
cp "$WIT/wit-g01.flow" "$B/t15/wit-idprobe-g01.flow"
fresh_persist # defaults domain (task 13; same posture as the wit leg)
"$B/foh_app" --flow "$B/t15/wit-idprobe-g01.flow" --flow-out "$B/t15/trace.txt" \
  --bridge verify --simdata "$B/simdata.txt" --seed "$G01_SEED" \
  --trace "$B/g01.trace.txt" --frames "$G01_FRAMES" --out "$B/t15/stream.txt" \
  --bstate-out "$B/t15/bstate.txt" 2>&1 | relay_lines
made "$B/t15/trace.txt" "$B/t15/stream.txt" "$B/t15/bstate.txt"
node -e '
  const fs = require("fs");
  const a = fs.readFileSync(process.argv[1], "utf8").split("\n"); // witness
  const b = fs.readFileSync(process.argv[2], "utf8").split("\n"); // idprobe
  if (a[0] !== "FOHTRACE1 flow=wit-g01" ||
      b[0] !== "FOHTRACE1 flow=wit-idprobe-g01") {
    console.error("headers not the expected pair: " + JSON.stringify(a[0]) +
                  " / " + JSON.stringify(b[0]) +
                  " — the flow id is not observable where expected");
    process.exit(1);
  }
  if (a.length !== b.length) {
    console.error("trace line counts differ beyond the header");
    process.exit(1);
  }
  for (let i = 1; i < a.length; i++) {
    if (a[i] !== b[i]) {
      console.error("non-header divergence at line " + (i + 1) + ": " +
                    JSON.stringify(a[i]) + " != " + JSON.stringify(b[i]));
      process.exit(1);
    }
  }
' "$WIT/trace.txt" "$B/t15/trace.txt" || fail "T15 — renamed-flow trace does not differ in exactly the header line"
cmp "$B/t15/stream.txt" "$WIT/stream.txt" || fail "T15 — the sim stream DEPENDS on the flow id (renamed flow produced different stream bytes; the review-91 H confound is REAL — STOP, do not re-pin)"
cmp "$B/t15/bstate.txt" "$WIT/bstate.txt" || fail "T15 — BRIDGE-STATE depends on the flow id"
echo "    T15 OK: renamed flow id changes ONLY the trace header — stream + BRIDGE-STATE byte-identical (flow-id independence)"
teeth=$((teeth + 1))
# T16 (review-91 L): plant a DOTFILE .unexpected.ppm in a COPY of
# f01's shots-b inventory -> the dotfile-inclusive production
# judge_shot_inventory MUST die (plain ls would have passed it).
rm -rf "$B/t16-shots"
mkdir -p "$B/t16-shots"
cp "$B/f01-vs-g01/shots-b/"*.ppm "$B/t16-shots/"
printf 'P6\n1 1\n255\nxyz' > "$B/t16-shots/.unexpected.ppm"
made "$B/t16-shots/.unexpected.ppm"
rc=0
( judge_shot_inventory "t16 (f01 shots-b copy + dotfile)" "$B/t16-shots" "$t14want" ) \
  > "$B/t16.out" 2>&1 || rc=$?
[ "$rc" != 0 ] || fail "T16 — a planted DOTFILE .unexpected.ppm PASSED the exact-set shot inventory (enumeration is not dotfile-inclusive)"
c="$(count_x "$B/t16.out" "shot inventory")"
[ "$c" = 1 ] || fail "T16 — inventory death message class missing"
echo "    T16 OK: a planted dotfile dies in the production judge_shot_inventory"
teeth=$((teeth + 1))
# T17 (iter 99): tstage variant — an injected DOWN in a COPY of the f06
# flow moves the selection to slot 1: TLAUNCH must carry tstage=1 and the
# trace must diverge from the frozen f06 expectation.
#
# REPAIRED 2026-08-24 (D29 / A25c). This tooth used to inject a ONE-FRAME
# DOWN TAP, which was exactly right for the EDGE-driven grid cursor D29
# retired — and is a no-op for the LEVEL-driven free hand, which moves
# FOH_CURSOR_VY = 3.84 px per held frame. A 1-frame tap moves the hand
# 39.5 -> 43.34, still inside slot 0's rect (y 30..49), so tstage stayed 0,
# the trace matched the frozen expectation, and the tooth SILENTLY STOPPED
# BITING. That is the A39 class verbatim: a tooth is hostage to the
# mechanism it asserts, and D29 replaced the mechanism.
#
# The tooth's INTENT is unchanged and still correct — "moving the cursor
# must change which target launches" — so it is REPAIRED, NOT RETIRED
# (HARD RULE 3). The gesture is re-expressed in the idiom f07 already
# uses: a HELD run. 6 held frames (397..402) move the hand 39.5 -> 62.54,
# inside slot 1's rect, measured against foh_tss_slots' geometry exactly
# as f07-target-t02.flow's own header records it.
mkdir -p "$B/t17"
mkvariant "$FLOWS/f06-target-t01.flow" "$B/t17/f06-target-t01.flow" insert-after \
  "I 396 -" "I 397 D" "I 403 -"
run_variant "$B/t17/f06-target-t01.flow" "$B/t17.trace.txt"
rc=0; cmp -s "$B/t17.trace.txt" "$FLOWS/f06-target-t01.expect" || rc=$?
[ "$rc" = 1 ] || fail "T17 — tstage-variant trace cmp rc $rc, want exactly 1"
c="$(count_x "$B/t17.trace.txt" "tstage=1")"
[ "$c" = 1 ] || fail "T17 — variant TLAUNCH does not carry tstage=1 (the grid cursor must be load-bearing)"
echo "    T17 OK: injected grid DOWN lands tstage=1 in TLAUNCH and diverges the trace"
teeth=$((teeth + 1))
# T18 (iter 99): target-plane nibble — flip one target-frame hash in a
# COPY of the f06 target run JSON -> verify-target-stream must diverge
# (rc 2; the target plane is judged, not decorative).
node -e '
  const fs = require("fs");
  const j = JSON.parse(fs.readFileSync(process.argv[1], "utf8"));
  const h = j.target.frames[1799].h;
  j.target.frames[1799].h = (h[0] === "0" ? "1" : "0") + h.slice(1);
  fs.writeFileSync(process.argv[2], JSON.stringify(j));
' "$B/f06-target-t01.target.json" "$B/t18.target.json"
made "$B/t18.target.json"
rc=0
node "$M4G/verify-target-stream.js" "$B/t18.target.json" \
  "$M4G/$T01_NAME.target.sha256.json" > "$B/t18.log" 2>&1 || rc=$?
[ "$rc" = 2 ] || fail "T18 — verify-target-stream rc $rc on a nibble-flipped target-plane copy (want the divergence class 2)"
echo "    T18 OK: nibble-flipped target-plane run copy dies in verify-target-stream (rc 2)"
teeth=$((teeth + 1))
# --- [5b] FINISH-BANNER GLYPH WITNESS (M4 micro, iter 103; review-101
# round-2 M — the authored-unreachable-path class). gfx_target_banner
# draws COMPLETE!/FAILURE; before iter 103 it used the frozen VFXGLYPHS
# atlas font 0, which carries digits + ':' ONLY, so gfx_overlay.c's FATAL
# missing-glyph path would ABORT the FIRST real finish (latent — no
# committed device leg reaches the finish seam: foh_dev.c draws the
# banner only when g_tfin_fired, which check-device-target.sh's
# assert_no_tfinish forbids on every green leg). The fix routes the
# banner text through the letter-complete self-authored FOH 5x7 font
# (gfx_target_banner_text). This leg drives that REAL banner-text render
# for BOTH reachable strings and proves the fatal is structurally
# unreachable for them. It links gfx_target.o + the [2] raster.o/foh_font.o
# with -Wl,-dead_strip (the unreferenced scene-frame draws are dropped;
# a revert to gfx_glyph_text inside gfx_target_banner_text would leave
# that symbol UNRESOLVED — no atlas linked here — a LINK FAILURE, so the
# FONT choice is guarded, not merely the strings).
echo "=== [5b] finish-banner glyph witness (iter 103)"
# PRODUCER GRAMMAR (PROCESS §3 whitelist rule; iter 105 — close
# review-103 round-3 M "permissive decision-output parsing"). MEASURED
# empirically, NOT from memory: the success line from the archived
# genuine runs .loop/m4-ban103-{fohflows,driver}-cold.log:138 (both
# byte-identical) + a fresh witness run (.loop/m4-ban105-measure.log);
# the tooth diagnostic + exit code from a manual relink of the SAME
# witness .o against a COMPLETE?-perturbed copy (rc=3 measured). The ink
# counts are DETERMINISTIC pins — foh_text blits a 5x7 BITMAP font at
# integer scale 4 / fixed centre, so lit-pixel count is pure integer, no
# FP, platform-independent. Anchored FULL-LINE + resemblance-death only,
# never a bare prefix/substring.
BANNER_OK_LINE='BANNER WITNESS OK (complete_ink=2016 failure_ink=1680 distinct=yes)'
BANNER_OK_NEEDLE='BANNER WITNESS OK'
BANNER_TOOTH_LINE='foh_banner_witness: gfx_fatal: foh_font: no glyph for requested character'
BANNER_TOOTH_NEEDLE='foh_font: no glyph for requested character'
BANNER_TOOTH_RC=3
# Anchored full-line verdict parse: the file must carry the EXACT success
# line exactly ONCE (count_xl = grep -cxF, full line) AND the OK needle
# must appear ONLY on that anchored line (needle count == full-line
# count) — a garbled/truncated success line (prefix kept, tail corrupt)
# or an extra resembling line = corruption. Returns 0 iff genuine; the
# caller fails closed on nonzero.
banner_verdict_ok() { # <out-file>
  local f="$1" full needle
  full="$(count_xl "$f" "$BANNER_OK_LINE")"
  needle="$(count_x "$f" "$BANNER_OK_NEEDLE")"
  [ "$full" = 1 ] || return 1
  [ "$needle" = "$full" ] || return 1
  return 0
}
banner=0
rm -f "$B/gfx_target.o" "$B/foh_banner_witness.o" "$B/foh_banner_witness"
cc -O2 "${CFLAGS_COMMON[@]}" -c "$GFX/gfx_target.c" -o "$B/gfx_target.o"
cc -O2 "${CFLAGS_COMMON[@]}" -c "$FOH/foh_banner_witness.c" -o "$B/foh_banner_witness.o"
cc -O2 -Wl,-dead_strip -o "$B/foh_banner_witness" \
  "$B/foh_banner_witness.o" "$B/gfx_target.o" "$B/raster.o" "$B/foh_font.o"
made "$B/foh_banner_witness"
rm -rf "$B/banner-shots"; mkdir -p "$B/banner-shots"
if ! "$B/foh_banner_witness" --shot-dir "$B/banner-shots" > "$B/banner.out" 2>&1; then
  relay_lines < "$B/banner.out"
  fail "banner witness exited non-zero (a reachable banner string hit the missing-glyph fatal?)"
fi
relay_lines < "$B/banner.out"
banner_verdict_ok "$B/banner.out" || fail "banner witness — success output is not the exact anchored line '$BANNER_OK_LINE' exactly once, with no other 'BANNER WITNESS OK' resemblance (permissive-parse guard, PROCESS §3) — see relayed output above"
made "$B/banner-shots/banner-complete.pgm" "$B/banner-shots/banner-failure.pgm"
if cmp -s "$B/banner-shots/banner-complete.pgm" "$B/banner-shots/banner-failure.pgm"; then
  fail "banner witness — COMPLETE! and FAILURE rendered identical shots (a blank/stubbed draw)"
fi
banner=1
echo "    [5b] OK: gfx_target_banner_text renders COMPLETE! + FAILURE (both non-blank, distinct shots); missing-glyph fatal unreachable for both reachable strings"
# banner TOOTH (COPY of gfx_target.c; the committed source is never
# touched): a banner string carrying a glyph the FOH font lacks
# (COMPLETE! -> COMPLETE?, '?' is not in foh_font.c's kGlyphs) must STILL
# die FATAL — the loud-failure guard (HARD RULE 2) stays; only the
# reachable strings are proven covered. The SAME witness .o is relinked
# against the perturbed copy. (-Iport/gfx lets the build-dir copy resolve
# its "gfx.h"/"../foh/foh.h" quoted includes.)
cp "$GFX/gfx_target.c" "$B/gt-banner-tooth.c"
sed -i.bak 's/"COMPLETE!"/"COMPLETE?"/' "$B/gt-banner-tooth.c"; rm -f "$B/gt-banner-tooth.c.bak"
grep -q '"COMPLETE?"' "$B/gt-banner-tooth.c" || fail "banner tooth — the missing-glyph perturb did not take"
cc -O2 "${CFLAGS_COMMON[@]}" -Iport/gfx -c "$B/gt-banner-tooth.c" -o "$B/gt-banner-tooth.o"
cc -O2 -Wl,-dead_strip -o "$B/foh_banner_witness_tooth" \
  "$B/foh_banner_witness.o" "$B/gt-banner-tooth.o" "$B/raster.o" "$B/foh_font.o"
rc=0
"$B/foh_banner_witness_tooth" > "$B/banner-tooth.out" 2>&1 || rc=$?
# EXACT measured missing-glyph class: rc 3 (foh_font gfx_fatal via the
# witness override's exit(3)); any other exit — including a mere nonzero
# — is the wrong death and dies here.
[ "$rc" = "$BANNER_TOOTH_RC" ] || fail "banner tooth — a missing-glyph banner string exited rc $rc, want the measured missing-glyph fatal class $BANNER_TOOTH_RC (got: $(cat "$B/banner-tooth.out"))"
tfull="$(count_xl "$B/banner-tooth.out" "$BANNER_TOOTH_LINE")"
tneedle="$(count_x "$B/banner-tooth.out" "$BANNER_TOOTH_NEEDLE")"
[ "$tfull" = 1 ] || fail "banner tooth — died but not at the EXACT foh_font missing-glyph diagnostic line (anchored full-line match $tfull; got: $(cat "$B/banner-tooth.out"))"
[ "$tneedle" = "$tfull" ] || fail "banner tooth — the missing-glyph needle appears on $tneedle lines but the anchored diagnostic once (garbled/extra output; got: $(cat "$B/banner-tooth.out"))"
[ "$(count_x "$B/banner-tooth.out" "$BANNER_OK_NEEDLE")" = 0 ] || fail "banner tooth — printed the OK verdict despite a missing glyph"
rm -f "$B/gt-banner-tooth.c" "$B/gt-banner-tooth.o" "$B/foh_banner_witness_tooth"
echo "    banner tooth OK: COMPLETE? (missing glyph) dies loud (rc $BANNER_TOOTH_RC) at the exact foh_font guard line; no false OK"
teeth=$((teeth + 1))

# PARSER TEETH (PROCESS §3 grammar fail-closed proof; iter 105). Prove
# banner_verdict_ok — the anchored success-line parser — REJECTS the two
# permissive holes the round-3 M named: (a) a garbled success line (the
# exact line + trailing corruption; prefix intact), and (b) a
# substring-resemblance line (the OK needle on an extra line that is not
# the anchored success line). Synthetic verdict outputs on COPIES,
# removed after; the committed producer output is never touched.
printf '%sCORRUPTED\n' "$BANNER_OK_LINE" > "$B/banner-garble.out"
if banner_verdict_ok "$B/banner-garble.out"; then
  fail "parser tooth — a garbled success line (exact line + trailing corruption) was ACCEPTED (permissive prefix parse survives)"
fi
printf '%s\n%s but garbled\n' "$BANNER_OK_LINE" "$BANNER_OK_NEEDLE" > "$B/banner-resemble.out"
if banner_verdict_ok "$B/banner-resemble.out"; then
  fail "parser tooth — an extra 'BANNER WITNESS OK' resemblance line was ACCEPTED (needle-vs-full resemblance guard is dead)"
fi
rm -f "$B/banner-garble.out" "$B/banner-resemble.out"
echo "    parser teeth OK: garbled-success + substring-resemblance verdict outputs both rejected by the anchored full-line parser"
teeth=$((teeth + 2))

# T23-T25 (review-r1 BLOCKER): the judge must hold a trace to the
# COMPILED profile and to the SCREEN each event can come from, not merely
# to a global token list. Each arm feeds a MINIMAL synthetic trace whose
# only defect is the one under test, so the death message is attributable.
# The positive controls are the committed flows themselves: f04 carries
# `refused targetbuilder` on menu-top and f03 carries `soundsvol` on
# options-audio, both of which pass in leg [3].
mkdir -p "$B/t23"
t23_head() { # <file> — a legal prefix ending on menu-top
  printf 'FOHTRACE1 flow=t23\nT 370 startup title timer\nT 375 title menu-top start\n' > "$1"
}
# (a) the hidden battle page: legal only in a FOH_NETPLAY 1 build
t23_head "$B/t23/edge.txt"
printf 'T 380 menu-top menu-battle a\nEND 400 transitions=3\n' >> "$B/t23/edge.txt"
rc=0
node "$FOH/judge-foh-trace.js" "$B/t23/edge.txt" t23 0 > "$B/t23/edge.log" 2>&1 || rc=$?
[ "$rc" = 2 ] || fail "T23 — a menu-battle edge under FOH_NETPLAY 0 returned rc $rc, want 2 (the judge must read the build profile out of foh.h)"
c="$(count_x "$B/t23/edge.log" "off-graph transition")"
[ "$c" = 1 ] || fail "T23 — death message class missing (want the off-graph class)"
echo "    T23 OK: a hidden-page transition dies as off-graph under the compiled profile"
teeth=$((teeth + 1))
# (b) a retired refusal token: `audio` is a real screen now, in every build
t23_head "$B/t23/ref.txt"
printf 'S 380 refused audio\nEND 400 transitions=2\n' >> "$B/t23/ref.txt"
rc=0
node "$FOH/judge-foh-trace.js" "$B/t23/ref.txt" t23 0 > "$B/t23/ref.log" 2>&1 || rc=$?
[ "$rc" = 2 ] || fail "T24 — a retired `refused audio` returned rc $rc, want 2"
c="$(count_x "$B/t23/ref.log" "unregistered refused entry")"
[ "$c" = 1 ] || fail "T24 — death message class missing"
echo "    T24 OK: a retired refusal token dies as unregistered"
teeth=$((teeth + 1))
# (c) a real field on a screen that cannot write it
t23_head "$B/t23/scr.txt"
printf 'S 380 soundsvol 5\nEND 400 transitions=2\n' >> "$B/t23/scr.txt"
rc=0
node "$FOH/judge-foh-trace.js" "$B/t23/scr.txt" t23 0 > "$B/t23/scr.log" 2>&1 || rc=$?
[ "$rc" = 2 ] || fail "T25 — a volume edit on menu-top returned rc $rc, want 2"
c="$(count_x "$B/t23/scr.log" "cannot write it")"
[ "$c" = 1 ] || fail "T25 — death message class missing (want the wrong-screen class)"
echo "    T25 OK: a settings field emitted on the wrong screen dies"
teeth=$((teeth + 1))
# T26 (review-r2 MAJOR): the profile parse must be fail-closed against
# AMBIGUOUS input, not merely present. The judge reads its SIBLING foh.h,
# so the lever is a copy of the judge next to a doctored header — which is
# also the only way to exercise the arm without editing the real header.
mkdir -p "$B/t26"
cp "$FOH/judge-foh-trace.js" "$B/t26/judge-foh-trace.js"
{ cat "$FOH/foh.h"; printf '#define FOH_NETPLAY 1\n'; } > "$B/t26/foh.h"
t23_head "$B/t26/t.txt"
printf 'END 400 transitions=2\n' >> "$B/t26/t.txt"
rc=0
node "$B/t26/judge-foh-trace.js" "$B/t26/t.txt" t23 0 > "$B/t26/dup.log" 2>&1 || rc=$?
[ "$rc" = 2 ] || fail "T26 — two FOH_NETPLAY definitions returned rc $rc, want 2 (a first-match parse is not fail-closed)"
c="$(count_x "$B/t26/dup.log" "FOH_NETPLAY definitions, want")"
[ "$c" = 1 ] || fail "T26 — death message class missing (want the ambiguous-profile class)"
# and the SAME judge copy beside the REAL header still passes, so the tooth
# proves the duplicate — not the copy.
cp "$FOH/foh.h" "$B/t26/foh.h"
node "$B/t26/judge-foh-trace.js" "$B/t26/t.txt" t23 0 > "$B/t26/ok.log" 2>&1 \
  || fail "T26 — the relocated judge fails against the REAL header (the tooth would be proving the wrong thing)"
echo "    T26 OK: an ambiguous FOH_NETPLAY dies; the same judge passes beside the real header"
teeth=$((teeth + 1))
# --- [5c] FOH SOUND-PLANE WITNESS (C23; menu-fidelity arc round 10;
# review-r9 Standards BLOCKER + the writer's own escalation). The FOH
# structural trace carries transitions/selections/launches and NOTHING
# about SOUND: `FohState.snd[]` was unobserved by every check, so a screen
# that played the wrong sound, played it twice, or fell silent passed all
# eight green flow runs (same class as the U1 star false-green — the judge
# could not see the plane the change lived in). It needs neither device
# audio nor a trace-format change (foh.h:538-539 exposes foh_init/foh_tick;
# snd[] is populated per tick), so it lives here.
#
# The witness (port/foh/foh_snd_witness.c) covers TWO planes:
#   [A] EMISSION — real foh_tick() over crafted one-tick input edges,
#       asserting the EXACT token sequence/count for the five menuMove=true
#       arms that emit a SECOND menuSelect, the single-sound changeGamemode
#       leaves that must NOT (the negative side), the deny refusals, A-over-B
#       and up-over-down priority, and the audio screen's own arms.
#   [B] AUDIBILITY — real snd_mix_fill() over a synthetic SNDPACK1 + music
#       ring, asserting that the options-audio master levels reach the
#       OUTPUT SAMPLES: byte-identical to the PRE-WIRE mixer at the upstream
#       defaults (independent reference formula — this is what keeps the
#       frozen mixer/music fidelity streams green), exact silence at 0.0,
#       authored full scale at 1.0, strict monotonicity across the rail, and
#       snd_bus_q12's level/default RATIO against hand-computed pins, and
#       that the SFX bus is SNAPSHOTTED per voice at play time (howler).
# The DEVICE half (real SDL audio through a speaker) is NOT claimed here —
# it is the work order .loop/menus-p2-device-workorder-audio.md.
echo "=== [5c] FOH sound-plane witness (C23)"
# PRODUCER GRAMMAR (PROCESS §3 whitelist rule): anchored FULL-LINE verdict,
# counts are structural pins measured from the committed witness — growing a
# sound plane without growing the witness moves them and this dies.
SND_OK_LINE='SND WITNESS OK (cases=24 buspins=10 gainvec=3 sfx=7 music=6)'
SND_OK_NEEDLE='SND WITNESS OK'
# WHOLE-OUTPUT whitelist (review-r10 MAJOR): the witness's entire combined
# stdout+stderr on success is EXACTLY this one line. An "exact line appears
# once" parser still accepts arbitrary unrelated extra lines, which is not
# PROCESS §3's whole-output rule and would let noisy/corrupt producer output
# through. So: byte equality against the single expected line, nothing else.
snd_verdict_ok() { # <out-file>
  printf '%s\n' "$SND_OK_LINE" | cmp -s - "$1"
}
sndwit=0
rm -f "$B/foh_snd_witness.o" "$B/foh_snd_witness"
cc -O2 "${CFLAGS_COMMON[@]}" -c "$FOH/foh_snd_witness.c" -o "$B/foh_snd_witness.o"
# Links the SAME [2] objects the app links (foh.o is the machine under test;
# foh_render.o carries foh_anim_tick, which foh_tick calls — stubbing it would
# be a fake, so the real TU is linked and -dead_strip drops the draw code).
snd_link() { # <out-binary> <witness.o> <foh.o>
  cc -O2 -Wl,-dead_strip -o "$1" "$2" "$3" \
    "$B/foh_render.o" "$B/foh_font.o" "$B/raster.o" "$B/img1.o" \
    "$B/ctl_style.o" -lm
}
snd_link "$B/foh_snd_witness" "$B/foh_snd_witness.o" "$B/foh.o"
made "$B/foh_snd_witness"
rm -f "$B/sndwit-pack.bin"
if ! "$B/foh_snd_witness" "$B/sndwit-pack.bin" > "$B/sndwit.out" 2>&1; then
  relay_lines < "$B/sndwit.out"
  fail "sound witness exited non-zero — the FOH sound plane or the audio bus regressed (see relayed output above)"
fi
relay_lines < "$B/sndwit.out"
snd_verdict_ok "$B/sndwit.out" || fail "sound witness — success output is not the exact anchored line '$SND_OK_LINE' exactly once, with no other 'SND WITNESS OK' resemblance (permissive-parse guard, PROCESS §3) — see relayed output above"
sndwit=1
echo "    [5c] OK: 24 emission cases (5 menuMove doubles, the single-sound leaves, deny, A/B + up/down priority, the audio screen) + the audio bus reaches snd_mix_fill's samples, survives a track switch, and is linear at level 0.1 (byte-identical at the upstream defaults)"

# --- [5c] TEETH. All EIGHT on COPIES; the committed sources are never
# touched. Each must kill the witness with the RIGHT diagnostic, not merely
# exit non-zero.
# WHOLE-OUTPUT parsing on the TOOTH side (review-r11 MAJOR): "rc 1 + the
# expected substring appears" still tolerates arbitrary EXTRA output, so a
# perturbation that also crashed, warned, or printed junk would pass. Every
# line must match the witness's OWN line grammar, the trailer must be the exact
# failure-count line with the PINNED count, and the expected diagnostic must be
# present. `snd_tooth_lines` counts lines that do NOT match the grammar.
SND_LINE_RE='^foh_snd_witness: (FAIL: .+|[0-9]+ failure\(s\))$'
snd_tooth() { # <id> <binary> <want-needle> <want-failures> <label>
  local id="$1" bin="$2" needle="$3" wantf="$4" label="$5" rc=0 bad last
  "$bin" "$B/$id-pack.bin" > "$B/$id.out" 2>&1 || rc=$?
  [ "$rc" = 1 ] || fail "$id — the perturbed build exited rc $rc, want 1 (the witness's own failure class; got: $(cat "$B/$id.out"))"
  bad="$(grep -cvE "$SND_LINE_RE" "$B/$id.out" || true)"
  [ "$bad" = 0 ] || fail "$id — $bad line(s) of the tooth's output are outside the witness's own line grammar (crash/warning/junk alongside the expected failure; got: $(cat "$B/$id.out"))"
  last="$(tail -n 1 "$B/$id.out")"
  [ "$last" = "foh_snd_witness: $wantf failure(s)" ] || fail "$id — trailer is '$last', want the pinned 'foh_snd_witness: $wantf failure(s)' (a changed failure COUNT means the perturbation moved which assertions fire)"
  [ "$(count_x "$B/$id.out" "$needle")" -ge 1 ] || fail "$id — died but not at the expected diagnostic '$needle' (got: $(cat "$B/$id.out"))"
  [ "$(count_x "$B/$id.out" "$SND_OK_NEEDLE")" = 0 ] || fail "$id — printed the OK verdict despite the perturbation"
  echo "    $label"
  teeth=$((teeth + 1))
}
# T27: a DROPPED second menuSelect (the exact r9 regression: the five
# menuMove=true arms could all have lost their sound and every flow run
# would still have been green).
rm -rf "$B/st27"; mkdir -p "$B/st27"
sed 's|^          snd_push(s, "menuSelect"); // menu.js:236 (menuMove at :97)$||' \
  "$FOH/foh.c" > "$B/st27/foh.c"
[ "$(count_x "$B/st27/foh.c" 'menuMove at :97')" = 0 ] || fail "T27 — the dropped-menuSelect perturb did not take"
cc -O2 "${CFLAGS_COMMON[@]}" -I"$FOH" -c "$B/st27/foh.c" -o "$B/st27/foh.o"
snd_link "$B/st27/w" "$B/foh_snd_witness.o" "$B/st27/foh.o"
snd_tooth st27 "$B/st27/w" "want 'menuForward,menuSelect'" 1 \
  "T27 OK: a dropped second menuSelect dies at the exact emission case"
# T28: the up/down PRIORITY regression — upstream's ONE else-if chain turned
# back into independent ifs (which double the sound AND cancel the cursor).
rm -rf "$B/st28"; mkdir -p "$B/st28"
awk '{ if ($0 == "  } else if (dE) {") { print "  }"; print "  if (dE) {" } else print }' \
  "$FOH/foh.c" > "$B/st28/foh.c"
[ "$(count_xl "$B/st28/foh.c" '  } else if (dE) {')" = 0 ] || fail "T28 — the else-if split perturb did not take"
cc -O2 "${CFLAGS_COMMON[@]}" -I"$FOH" -c "$B/st28/foh.c" -o "$B/st28/foh.o"
snd_link "$B/st28/w" "$B/foh_snd_witness.o" "$B/st28/foh.o"
snd_tooth st28 "$B/st28/w" "'menuSelect,menuSelect'" 2 \
  "T28 OK: a simultaneous up+down that runs BOTH arms dies (two sounds, cancelled cursor)"
# T29/T30 perturb snd_mixer.h, which the witness includes as
# "../gfx/snd_mixer.h" — so they need a foh/ + gfx/ MIRROR (the files the
# witness's include chain reaches; a new include breaks the build loudly).
# It did exactly that on 2026-08-24: D29/A25c made foh.h include foh_hand.h,
# and this mirror failed with `fatal error: 'foh_hand.h' file not found`.
# Working as designed — the list is deliberately explicit so a widened
# include chain cannot be absorbed silently.
snd_mirror() { # <dir> <sed-expr> <must-vanish>
  local d="$1"
  rm -rf "$d"; mkdir -p "$d/foh" "$d/gfx"
  cp "$FOH/foh_snd_witness.c" "$d/foh/"
  cp "$FOH/foh.h" "$FOH/foh_hand.h" "$d/foh/"
  cp "$GFX/platform.h" "$GFX/raster.h" "$d/gfx/"
  sed "$2" "$GFX/snd_mixer.h" > "$d/gfx/snd_mixer.h"
  [ "$(count_x "$d/gfx/snd_mixer.h" "$3")" = 0 ] || fail "$d — the snd_mixer perturb did not take"
  cc -O2 "${CFLAGS_COMMON[@]}" -c "$d/foh/foh_snd_witness.c" -o "$d/w.o"
  snd_link "$d/w" "$d/w.o" "$B/foh.o"
}
# T29: the bus WIRED BUT INERT — the sliders persist and the mixer ignores
# them. This is literally the bug the owner reported ("audio tab doesn't
# work"), so it must be the loudest tooth in the set. snd_bus_apply is the
# single chokepoint both channels go through, so neutering it neuters the whole
# wire in one edit.
snd_mirror "$B/st29" \
  's|if (busQ12 == (uint16_t)SND_BUS_UNITY) {|if (busQ12 \|\| 1) {|' \
  'busQ12 == (uint16_t)SND_BUS_UNITY'
snd_tooth st29 "$B/st29/w" "want exact silence" 17 \
  "T29 OK: a bus that never reaches the fill dies (level 0.0 still audible, 1.0 not louder, sweep flat)"
# T30: the DOUBLE-APPLY bug — pushing the RAW LEVEL instead of the
# level/default ratio, forgetting that SND1's packed gains already carry the
# 0.5/0.3 defaults, so the default lands twice (0.5 x 0.5 at rest). Perturbed
# to the literal raw-level expression (review-r10 NIT: the earlier `level *
# dflt` bit, but modelled a THIRD default rather than the named defect). The
# `(void)dflt` keeps -Werror happy so the tooth dies at RUNTIME, on the pins,
# not at compile time — a compile failure would prove nothing about the pins.
snd_mirror "$B/st30" \
  's|return (uint16_t)(level / dflt \* (double)SND_BUS_UNITY + 0.5);|(void)dflt; return (uint16_t)(level * (double)SND_BUS_UNITY + 0.5);|' \
  'level / dflt'
snd_tooth st30 "$B/st30/w" "want 4096 (sfx default == unity)" 16 \
  "T30 OK: pushing the raw level (default applied twice) dies on the hand-computed bus pins"

# --- [5d] CSS LAUNCH-GUARD GRID WITNESS (codex review round 17/18 finding
# B1). Every committed flow drives the ONE port configuration the shipped
# menu reaches (P1 HMN, P2 HMN or CPU), so the REFUSING side of foh.c's D6
# launch guard had no behavioral coverage: the arm could be deleted,
# inverted or narrowed and every green flow would stay green. The witness
# drives the full (p1Type,p2Type) grid over {-1,0,1} through the REAL
# foh_tick and judges the sound plane, the event plane and the screen.
# The 9 expectations are AUTHORED in the witness (one row per cell, each
# with the reason the configuration owes a launch or a refusal) — nothing
# is computed from the predicate under test.
echo "=== [5d] CSS launch-guard grid witness (B1)"
LAUNCH_OK_LINE='LAUNCH GUARD WITNESS OK (cells=9 launch=2 refuse=7)'
LAUNCH_OK_NEEDLE='LAUNCH GUARD WITNESS OK'
# WHOLE-OUTPUT whitelist, same rule as [5c]: success output is EXACTLY
# this one line, byte-compared.
launch_verdict_ok() { # <out-file>
  printf '%s\n' "$LAUNCH_OK_LINE" | cmp -s - "$1"
}
launchwit=0
rm -f "$B/foh_launch_witness.o" "$B/foh_launch_witness"
cc -O2 "${CFLAGS_COMMON[@]}" -c "$FOH/foh_launch_witness.c" -o "$B/foh_launch_witness.o"
# Same objects and the same -Wl,-dead_strip recipe the sound witness uses
# (foh.o is the machine under test; foh_render.o carries foh_anim_tick,
# which foh_tick calls, so the real TU is linked rather than stubbed).
snd_link "$B/foh_launch_witness" "$B/foh_launch_witness.o" "$B/foh.o"
made "$B/foh_launch_witness"
if ! "$B/foh_launch_witness" > "$B/launchwit.out" 2>&1; then
  relay_lines < "$B/launchwit.out"
  fail "launch-guard witness exited non-zero — the CSS port-configuration guard regressed (see relayed output above)"
fi
relay_lines < "$B/launchwit.out"
launch_verdict_ok "$B/launchwit.out" || fail "launch-guard witness — success output is not the exact anchored line '$LAUNCH_OK_LINE' (permissive-parse guard, PROCESS §3) — see relayed output above"
launchwit=1
echo "    [5d] OK: all 9 (p1Type,p2Type) cells judged against the authored grid — 2 launch (HMN vs HMN, HMN vs CPU), 7 refuse with 'deny' + 'refused portconfig' and NO screen movement"

# --- [5d] TEETH. Both on COPIES of foh.c; the committed sources are never
# touched. Same whole-output treatment as [5c]'s teeth.
LAUNCH_LINE_RE='^foh_launch_witness: (FAIL: .+|[0-9]+ failure\(s\))$'
launch_tooth() { # <id> <binary> <want-needle> <want-failures> <label>
  local id="$1" bin="$2" needle="$3" wantf="$4" label="$5" rc=0 bad last
  "$bin" > "$B/$id.out" 2>&1 || rc=$?
  [ "$rc" = 1 ] || fail "$id — the perturbed build exited rc $rc, want 1 (the witness's own failure class; got: $(cat "$B/$id.out"))"
  bad="$(grep -cvE "$LAUNCH_LINE_RE" "$B/$id.out" || true)"
  [ "$bad" = 0 ] || fail "$id — $bad line(s) of the tooth's output are outside the witness's own line grammar (got: $(cat "$B/$id.out"))"
  last="$(tail -n 1 "$B/$id.out")"
  [ "$last" = "foh_launch_witness: $wantf failure(s)" ] || fail "$id — trailer is '$last', want the pinned 'foh_launch_witness: $wantf failure(s)' (a changed failure COUNT means the perturbation moved which assertions fire)"
  [ "$(count_x "$B/$id.out" "$needle")" -ge 1 ] || fail "$id — died but not at the expected diagnostic '$needle' (got: $(cat "$B/$id.out"))"
  [ "$(count_x "$B/$id.out" "$LAUNCH_OK_NEEDLE")" = 0 ] || fail "$id — printed the OK verdict despite the perturbation"
  echo "    $label"
  teeth=$((teeth + 1))
}
# T35: the guard NARROWED — the CPU-P2 disjunct dropped, so HMN vs CPU (the
# configuration the difficulty widget exists for) starts refusing. This is
# the regression no flow can see.
rm -rf "$B/st35w"; mkdir -p "$B/st35w"
# `@` delimiter on purpose: the guard itself contains `||`.
sed 's@    if (!(s->p1Type == 0 && (s->p2Type == 0 || s->p2Type == 1))) {@    if (!(s->p1Type == 0 \&\& (s->p2Type == 0))) {@' \
  "$FOH/foh.c" > "$B/st35w/foh.c"
[ "$(count_x "$B/st35w/foh.c" 's->p2Type == 1')" = 0 ] || fail "T35 — the narrowed-guard perturb did not take"
cc -O2 "${CFLAGS_COMMON[@]}" -I"$FOH" -c "$B/st35w/foh.c" -o "$B/st35w/foh.o"
snd_link "$B/st35w/w" "$B/foh_launch_witness.o" "$B/st35w/foh.o"
launch_tooth st35w "$B/st35w/w" "(p1=0,p2=1)" 3 \
  "T35 OK: narrowing the guard so HMN vs CPU refuses dies on the authored grid row"
# T36: the refusal's EVENT dropped — it still denies audibly, so a
# sound-only witness would stay green, but the structural trace would lose
# every refusal record.
rm -rf "$B/st36w"; mkdir -p "$B/st36w"
sed 's@^      ev_refused(s, "portconfig");$@@' "$FOH/foh.c" > "$B/st36w/foh.c"
[ "$(count_x "$B/st36w/foh.c" 'ev_refused(s, "portconfig")')" = 0 ] || fail "T36 — the dropped-refusal-event perturb did not take"
cc -O2 "${CFLAGS_COMMON[@]}" -I"$FOH" -c "$B/st36w/foh.c" -o "$B/st36w/foh.o"
snd_link "$B/st36w/w" "$B/foh_launch_witness.o" "$B/st36w/foh.o"
launch_tooth st36w "$B/st36w/w" "want exactly 1" 9 \
  "T36 OK: a refusal that stops emitting its structural event dies, though the deny sound still plays"

# T31: the MUSIC BUS NEVER LEAVES UNITY — the r10 BLOCKER's observable shape.
# The original defect was `snd_music_cfg` resetting the music bus on every
# track switch; that is now STRUCTURALLY IMPOSSIBLE (the bus moved to SndMixer
# scope, and snd_music_cfg only receives SndMusic*), so it cannot be perturbed
# back in one line. What IS still representable is the same OBSERVABLE: the
# music level never reaching the mixer. Both the music cases and the
# lifecycle case must catch it.
snd_mirror "$B/st31" \
  's|  m->musicBusQ12 = snd_bus_q12(musicLevel, SND_MUSIC_MASTER_DEFAULT);|  (void)musicLevel;|' \
  'snd_bus_q12(musicLevel'
snd_tooth st31 "$B/st31/w" "music-lifecycle" 4 \
  "T31 OK: a music level that never reaches the mixer dies in the music + lifecycle cases"

# T33: the SFX bus read LIVE from the mixer instead of the VOICE'S SNAPSHOT.
# This is the plausible-but-unfaithful implementation: howler's Sound.init
# copies parent._volume at play time (dist:2005) and audiomenu's groupType-0
# changeVolume never calls the public .volume() API, so the Sounds slider must
# affect FUTURE plays only. Nothing else in the witness distinguishes the two.
snd_mirror "$B/st33" \
  's|vc->e->gainQ8, vc->busQ12);|vc->e->gainQ8, m->sfxBusQ12);|' \
  'vc->busQ12'
snd_tooth st33 "$B/st33/w" "snapshots parent._volume" 2 \
  "T33 OK: re-gaining an already-playing voice from the live bus dies (howler snapshots at play time)"

# T34: CHAINED TRUNCATION restored — gain truncated to Q8, THEN scaled by the
# bus. This is the shape the r12 review caught: it is invisible on even samples
# (the witness's synthetic PCM used to be even, which is exactly why it passed)
# and loses whole LSBs on ODD ones — s=1 -> 0, s=-1 -> -2, s=32767 -> 32766 at
# master 1.0, against a real pack carrying over a million odd samples.
snd_mirror "$B/st34" \
  's|  const int64_t n = (int64_t)s \* (int64_t)gainQ8 \* (int64_t)busQ12;|  const int32_t sv2 = (s * (int32_t)gainQ8) >> 8; return (sv2 * (int32_t)busQ12 + SND_BUS_HALF) >> SND_BUS_SHIFT; const int64_t n = (int64_t)s * (int64_t)gainQ8 * (int64_t)busQ12;|' \
  'int64_t n = (int64_t)s \* (int64_t)gainQ8 \* (int64_t)busQ12;$'
snd_tooth st34 "$B/st34/w" "chained truncation loses odd samples" 9 \
  "T34 OK: chaining a Q8 truncation into the bus dies on the odd/negative gain vectors"

# T32: the WHOLE-OUTPUT verdict parser (review-r10 MAJOR) — prove it rejects
# the exact success line PLUS an unrelated extra line, which the previous
# count-based parser accepted.
{ printf '%s\n' "$SND_OK_LINE"; printf 'some unrelated producer noise\n'; } > "$B/sndwit-extra.out"
if snd_verdict_ok "$B/sndwit-extra.out"; then
  fail "T32 — the verdict parser ACCEPTED the success line plus an unrelated extra line (not a whole-output whitelist)"
fi
printf '%s\n' "$SND_OK_LINE" > "$B/sndwit-clean.out"
snd_verdict_ok "$B/sndwit-clean.out" || fail "T32 — the verdict parser rejects the CLEAN success line (the tooth would be proving the wrong thing)"
echo "    T32 OK: the whole-output verdict parser rejects extra lines and accepts only the exact line"
teeth=$((teeth + 1))

[ "$teeth" = 40 ] || fail "teeth ledger — $teeth/40 fired"
[ "$banner" = 1 ] || fail "banner ledger — the finish-banner witness leg did not complete"
[ "$sndwit" = 1 ] || fail "sound-witness ledger — the C23 sound-plane witness leg did not complete"

# --- [6] hygiene ----------------------------------------------------------------
rc=0
git check-ignore -q "$FOH/build" || rc=$?
if [ "$rc" = 1 ]; then
  fail "hygiene — $FOH/build is NOT git-ignored (build outputs could be committed)"
elif [ "$rc" -ge 2 ]; then
  grammar_die "hygiene — git check-ignore rc $rc (corrupt evidence, never a pass)"
fi

[ "$diverge" = 1 ] || fail "divergence-witness ledger — witness leg did not complete"
[ "$control" = 1 ] || fail "control ledger — the lcancel=0 control leg did not complete"
echo "FOH FLOWS OK (flows=7 shots=$total_shots bridges=$bridges tbridges=$tbridges states=$states tstates=$tstates diverge=$diverge control=$control banner=$banner snd=$sndwit launch=$launchwit teeth=$teeth)"
