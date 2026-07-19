#!/usr/bin/env bash
# M4 task 1 done-check: vfx seam widening, sim + capture side.
#
# The ml_events vfx plane carries FULL drawVfx configs (MlVfx; FORMAT.md
# "vfx posts (WIDENED, M4 task 1)") and every affected capture spec
# records them (call-time canon). This check COMPOSES the unweakened
# component checks — it adds no comparison logic of its own, so it cannot
# weaken anything (PROCESS §3 posture):
#
#   [1] every affected cluster check (moves-shared, moves-fox,
#       moves-falco, moves-falcon, moves-marth, moves-puff, article,
#       asshort, physics, hitdet — the grep-measured vfx-emitting set,
#       AGENT-LOG iter 64). Each records its captures FRESH ×2
#       (byte-stability cmp), STREAM-MATCH-guards every capture run
#       against the frozen goldens via the unchanged verify-stream.js,
#       asserts the measured-then-frozen pins, and replays EVERY record
#       through the C translations bit-exactly — the widened vfx configs
#       are inside the compared post envelopes, so a single differing
#       bit in any config is a divergence.
#   [2] `bash port/sim/check-sim.sh` — the M2 EXIT GATE unchanged: the
#       checksum stream is untouched by construction (vfx is not on the
#       CHECKSUM.md surface); SIM CONFORMS must still hold.
#
# AGGREGATOR CLASSES (iter 66 — review-64 round-1 closure; the
# verify_m3.sh patterns adapted host-side, .loop/review-64-triage.md):
#   - RUN LOCK (M3; iter-41 no-reclaim pattern): ONE mkdir-atomic host
#     lock guards the shared build/ capture files + fixed per-component
#     log paths. NO reclamation logic — an existing lock is a loud
#     refusal telling the operator to remove it by hand; the release
#     trap is installed only AFTER acquisition, so a losing contender
#     can never release the winner's lock.
#   - INVENTORY PIN (M4): CHECKS/VERDICTS/SPECS asserted length == 10
#     literal + unique entries, BEFORE the lock and any component run
#     (a merge that drops one array entry dies here, never a 9/10 pass).
#   - VERDICT GRAMMAR (M2; expect_verdict): per component, the exact
#     verdict line must occur exactly ONCE, be the log's FINAL line,
#     and exactly one line may match the verdict-prefix resemblance
#     grammar — a duplicated/torn/extended/non-final verdict is
#     CORRUPTION, never a pass. Discriminators MEASURED from the real
#     iter-64 log corpus (the wide word-prefixes were rejected: genuine
#     `… REPLAY RAN … 0 divergences` lines share them).
#   - FRESHNESS CONTRACT (M1): a component MATCH alone is not evidence
#     the re-record happened (a recorder that no-ops leaves a stale
#     capture that replays clean). Each capture component's log must
#     carry the fresh-record evidence lines at their measured counts,
#     parsed by anchored full-line grammar (whitelist rule, PROCESS §3;
#     corpus: the 11 iter-64 vfx-seam.check-*.log files, 0 false
#     rejections): 3× capture run A, 3× capture run B, 3× byte-stable,
#     6× STREAM MATCH (full grammar AND prefix count must agree — a
#     STREAM-MATCH-resembling malformed line is corruption). check-sim
#     carries exactly 8 STREAM MATCH lines (the 8-golden M2 gate).
#   - RELAY CONTRACT (verify_m3.sh log-sentinel class): every component
#     byte reaches this script's own output only through the '  | '
#     prefix; the log FILE keeps the raw component stdout (the grammar
#     corpus). The final `VFX SEAM MATCH` echo is therefore the only
#     possible unprefixed column-0 occurrence in composed output.
#
# ROUND-2 REVISIONS (iter 68 — .loop/review-66-triage.md; grammars
# re-validated 55/55 against the full archived corpus — the 11 raw
# iter-66 component logs + all 44 sections reconstructed from the 4
# archived composed runs — zero false rejections):
#   - IDENTITY BINDING (M1): counts bind IDENTITY, not cardinality. Per
#     carrier golden (frozen CARRIERS array): runA banner exact ×1, runB
#     banner exact ×1, per-NAME STREAM MATCH full grammar ×2; the
#     id-less byte-stable line is bound POSITIONALLY (exactly one per
#     runA-banner-delimited section, none before the first). check-sim:
#     each of the frozen 8 SIM_GOLDENS judged exactly once (stream +
#     `== gNN (name)` banner). A looping recorder that repeats one
#     golden while omitting another now dies even with totals preserved.
#   - AFFINITY COUNTERS (M2): every decision literal (verdict, runA/runB
#     banners, byte-stable, per-name STREAM stems) gets an affinity
#     count — nonempty lines that EXTEND the literal or are a proper
#     PREFIX of it (a torn write truncates the tail; '^literal' alone
#     cannot see 'MOVES fox MAT'). Exactly the expected number of
#     affine lines, all of them the exact literals — any torn attempt
#     is CORRUPTION.
#   - GREP RC CASE-SPLIT (M3): count helpers split grep's rc — 0/1 is
#     count semantics (rc 1 IS "0 matches"), rc >= 2 (and any nonzero
#     awk rc) is a READ ERROR and dies loudly. Never a silent 0 count.
#
# Prints VFX SEAM MATCH, exit 0. Never weakened: exact equality only.
set -euo pipefail
cd "$(dirname "$0")/../../.."

CAL=port/sim/calib
BUILD=$CAL/build

CHECKS=(
  "$CAL/check-moves-shared-replay.sh"
  "$CAL/check-moves-fox-replay.sh"
  "$CAL/check-moves-falco-replay.sh"
  "$CAL/check-moves-falcon-replay.sh"
  "$CAL/check-moves-marth-replay.sh"
  "$CAL/check-moves-puff-replay.sh"
  "$CAL/check-article-replay.sh"
  "$CAL/check-asshort-replay.sh"
  "$CAL/check-physics-replay.sh"
  "$CAL/check-hitdet-replay.sh"
)

# Expected verdict line per component (anchored full-line match on the
# component's OWN final verdict — whitelist-grammar posture; the verdict
# strings are the committed scripts' literal `echo` payloads).
VERDICTS=(
  "MOVES SHARED MATCH"
  "MOVES fox MATCH"
  "MOVES falco MATCH"
  "MOVES falcon MATCH"
  "MOVES marth MATCH"
  "MOVES puff MATCH"
  "ARTICLE MATCH"
  "ASSHORT MATCH"
  "PHYSICS MATCH"
  "HITDET MATCH"
)

# Spec token per component — the literal that appears in the component's
# own fresh-record evidence lines (`== gNN (<name>): <spec> capture run
# A`); drives the M1 freshness grammar below.
SPECS=(
  "moves-shared"
  "moves-fox"
  "moves-falco"
  "moves-falcon"
  "moves-marth"
  "moves-puff"
  "article"
  "asshort"
  "physics"
  "hitdet"
)

# Carrier golden IDENTITIES per component (iter 68, review-66 M1): the
# three golden NAMES whose fresh-record evidence must each appear
# exactly once per phase in the component's log — measured from the
# archived iter-64/66 corpus (probe-measured live-coverage carrier
# sets, AGENT-LOG M2 tasks 7-14). NOT unique across components by
# design: g01/g04/g06 is the shared/asshort/physics/hitdet carrier set,
# so the uniqueness pin below deliberately excludes CARRIERS (its shape
# pin — exactly 3 gNN-name tokens per entry — is asserted instead).
CARRIERS=(
  "g01-fox-marth-battlefield g04-puff-falcon-dreamland g06-falcon-marth-fountain" # moves-shared
  "g01-fox-marth-battlefield g03-falcon-fox-pstadium g08-fox-puff-fdest"          # moves-fox
  "g02-falco-puff-ystory g05-marth-falco-fdest g07-falco-falcon-battlefield"      # moves-falco
  "g03-falcon-fox-pstadium g04-puff-falcon-dreamland g06-falcon-marth-fountain"   # moves-falcon
  "g01-fox-marth-battlefield g05-marth-falco-fdest g06-falcon-marth-fountain"     # moves-marth
  "g02-falco-puff-ystory g04-puff-falcon-dreamland g08-fox-puff-fdest"            # moves-puff
  "g01-fox-marth-battlefield g02-falco-puff-ystory g08-fox-puff-fdest"            # article
  "g01-fox-marth-battlefield g04-puff-falcon-dreamland g06-falcon-marth-fountain" # asshort
  "g01-fox-marth-battlefield g04-puff-falcon-dreamland g06-falcon-marth-fountain" # physics
  "g01-fox-marth-battlefield g04-puff-falcon-dreamland g06-falcon-marth-fountain" # hitdet
)

# The 8-golden identity set the check-sim regression leg must judge
# (the M2 EXIT gate's full manifest, frozen names).
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

# INVENTORY PIN (iter 66, review-64 M4; CARRIERS/SIM_GOLDENS iter 68):
# 10 components, all four parallel arrays same length, entries unique
# (CARRIERS excepted — see above), SIM_GOLDENS exactly 8 unique — all
# asserted before the lock and before anything runs.
NCOMP=10
for pair in "CHECKS:${#CHECKS[@]}" "VERDICTS:${#VERDICTS[@]}" "SPECS:${#SPECS[@]}" "CARRIERS:${#CARRIERS[@]}"; do
  if [ "${pair#*:}" != "$NCOMP" ]; then
    echo "VFX SEAM FAIL: inventory pin — array ${pair%%:*} has ${pair#*:} entries, want exactly $NCOMP (a dropped/added entry is corruption, never a partial pass)" >&2
    exit 1
  fi
done
if [ "${#SIM_GOLDENS[@]}" != 8 ]; then
  echo "VFX SEAM FAIL: inventory pin — SIM_GOLDENS has ${#SIM_GOLDENS[@]} entries, want exactly 8 (the frozen M2-gate golden set)" >&2
  exit 1
fi
for arr in CHECKS VERDICTS SPECS SIM_GOLDENS; do
  dupes="$(eval 'printf "%s\n" "${'"$arr"'[@]}"' | sort | uniq -d)"
  if [ -n "$dupes" ]; then
    echo "VFX SEAM FAIL: inventory pin — duplicate entries in $arr:" >&2
    printf '%s\n' "$dupes" | sed 's/^/  | /' >&2
    exit 1
  fi
done
for entry in "${CARRIERS[@]}"; do
  ntok=0
  for tok in $entry; do
    ntok=$((ntok + 1))
    case "$tok" in
      g[0-9][0-9]-*) ;;
      *)
        echo "VFX SEAM FAIL: inventory pin — CARRIERS token '$tok' (entry '$entry') is not a gNN-name golden name" >&2
        exit 1
        ;;
    esac
  done
  if [ "$ntok" != 3 ]; then
    echo "VFX SEAM FAIL: inventory pin — CARRIERS entry '$entry' has $ntok tokens, want exactly 3 carrier golden names" >&2
    exit 1
  fi
done

# RUN LOCK (iter 66, review-64 M3 — the iter-41 riglib pattern,
# host-scoped): the shared resource is THIS checkout's build/ capture
# files and fixed log paths, so the lock lives beside them. mkdir is the
# atomic primitive; no reclamation by design.
mkdir -p "$BUILD"
LOCK="$BUILD/vfx-seam.lock"
if ! mkdir "$LOCK" 2>/dev/null; then
  lockage="unknown"
  if lockmtime="$(stat -f %m "$LOCK" 2>/dev/null || stat -c %Y "$LOCK" 2>/dev/null)"; then
    lockage="$(( $(date +%s) - lockmtime )) s"
  fi
  echo "VFX SEAM REFUSED: run lock $LOCK already exists (age: $lockage)." >&2
  echo "  Another check-vfx-seam.sh run may be rewriting the shared captures/logs" >&2
  echo "  in $BUILD right now. NO auto-reclaim (iter-41 posture). If you are sure" >&2
  echo "  no run is live, remove it manually: rm -rf '$LOCK'" >&2
  exit 1
fi
trap 'rm -rf "$LOCK"' EXIT

# RELAY (log-sentinel contract): every relayed component byte carries
# the visible '  | ' prefix in this script's own output.
relay_lines() { sed 's/^/  | /'; }

# Fresh-record evidence grammar (M1) — measured from the full archived
# corpus (PROCESS §3 whitelist rule: producer grammar from real logs —
# the 11 raw iter-66 component logs + 44 sections reconstructed from
# the 4 archived composed runs, 55/55 validated, zero false rejections;
# .loop/m4-rig68-corpusval.log). Length/exactness judgment belongs to
# the rc-enforced verify-stream.js + cmp inside the components; these
# grammars prove the evidence lines are GENUINE producer output at the
# measured counts AND identities, so a no-op or misrouted re-record can
# never pass.
STREAM_RE_TAIL='[0-9]{1,6}/[0-9]{1,6} frames exact, rngCalls=[0-9]{1,6}, rngCallsOutsideStep=1, specVersion=1$'
STREAM_RE="^STREAM MATCH [a-z0-9-]+: $STREAM_RE_TAIL"
BYTEID_LINE='   capture byte-identical across two fresh runs'

# grammar_die <msg> — every helper read error or grammar violation is a
# loud corruption death (iter 68, review-66 M3: never a silent 0).
grammar_die() { echo "VFX SEAM FAIL: $*" >&2; exit 2; }

# count_x <file> <exact-line> / count_e <file> <ere> — grep count
# helpers with the rc CASE-SPLIT (iter 68, review-66 M3): grep rc 0/1
# is count semantics (rc 1 IS "0 matches" — grep -c still prints 0);
# rc >= 2 is a read/pattern error and DIES. No blanket suppression.
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

# count_aff <file> <literal> — the AFFINITY counter (iter 68, review-66
# M2): counts nonempty lines that either EXTEND the literal or are a
# proper PREFIX of it — a torn write truncates the tail, so
# '^literal'-style resemblance alone cannot see 'MOVES fox MAT'. Any
# nonzero awk rc is a read error → death (M3 posture: awk has no
# "no match" rc, so 0 is the only clean exit).
count_aff() {
  local c rc=0
  c="$(awk -v lit="$2" 'length($0) > 0 && (index($0, lit) == 1 || index(lit, $0) == 1) { n++ } END { printf "%d", n + 0 }' "$1")" || rc=$?
  if [ "$rc" -ne 0 ]; then
    grammar_die "affinity counter — awk rc $rc reading '$1' (a read error is CORRUPT evidence, never a 0 count)"
  fi
  printf '%s' "$c"
}

# byteid_sections <file> — positional identity for the id-less
# byte-stable line (iter 68, review-66 M1): sections are delimited by
# the runA banner family; emits "<nsections> <pre-banner count>
# <per-section counts...>". The capture grammar requires "3 0 1 1 1".
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

# vfx_judge_log <clog> <want> <spec> <carriers> — the whole
# per-component log grammar: verdict (review-64 M2 + review-66 M2
# affinity), freshness counts (review-64 M1) and identity binding
# (review-66 M1). $3 empty = the check-sim regression leg ($4 ignored;
# SIM_GOLDENS is its frozen identity set).
vfx_judge_log() {
  local clog="$1" want="$2" spec="$3" carriers="$4"
  local c a b last sm smfull banners secs name id ra_line rb_line
  test -s "$clog" || grammar_die "empty or missing log $clog"
  # VERDICT GRAMMAR: exactly one exact verdict line, it is the FINAL
  # line, and exactly one line is verdict-AFFINE (extension OR torn
  # prefix; measured 1× in every genuine corpus log).
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
    echo "VFX SEAM FAIL: verdict grammar — final line of $clog is not the verdict:" >&2
    printf '%s\n' "$last" | relay_lines >&2
    exit 2
  fi
  # FRESHNESS + IDENTITY
  sm="$(count_e "$clog" '^STREAM MATCH ')"
  smfull="$(count_e "$clog" "$STREAM_RE")"
  if [ -n "$spec" ]; then
    for name in $carriers; do
      id="${name%%-*}"
      ra_line="== $id ($name): $spec capture run A"
      rb_line="== $id: $spec capture run B"
      c="$(count_x "$clog" "$ra_line")"
      a="$(count_aff "$clog" "$ra_line")"
      if [ "$c" != 1 ] || [ "$a" != 1 ]; then
        grammar_die "identity binding — '$ra_line' in $clog: exact $c/1, affine $a/1 (every carrier golden must be re-recorded exactly once; a repeated-or-omitted golden or a torn banner is CORRUPTION, never a pass)"
      fi
      c="$(count_x "$clog" "$rb_line")"
      a="$(count_aff "$clog" "$rb_line")"
      if [ "$c" != 1 ] || [ "$a" != 1 ]; then
        grammar_die "identity binding — '$rb_line' in $clog: exact $c/1, affine $a/1 (every carrier golden must be re-recorded exactly once; a repeated-or-omitted golden or a torn banner is CORRUPTION, never a pass)"
      fi
      c="$(count_e "$clog" "^STREAM MATCH $name: $STREAM_RE_TAIL")"
      a="$(count_aff "$clog" "STREAM MATCH $name: ")"
      if [ "$c" != 2 ] || [ "$a" != 2 ]; then
        grammar_die "identity binding — STREAM MATCH for $name in $clog: full-grammar $c/2, stem-affine $a/2 (each carrier's two capture runs must each STREAM MATCH; a torn or foreign stream line is CORRUPTION)"
      fi
    done
    c="$(count_x "$clog" "$BYTEID_LINE")"
    a="$(count_aff "$clog" "$BYTEID_LINE")"
    secs="$(byteid_sections "$clog")"
    if [ "$c" != 3 ] || [ "$a" != 3 ] || [ "$secs" != "3 0 1 1 1" ]; then
      grammar_die "freshness contract — byte-stable evidence in $clog off (exact $c/3, affine $a/3, sections '$secs' want '3 0 1 1 1'); each carrier section must prove its own two fresh runs byte-identical"
    fi
    banners="$(count_e "$clog" '^== ')"
    if [ "$banners" != 6 ]; then
      grammar_die "freshness contract — $clog carries $banners '== '-family banner lines, want exactly 6 (3 run A + 3 run B); an extra or torn banner is CORRUPTION"
    fi
    if [ "$sm" != 6 ] || [ "$smfull" != 6 ]; then
      grammar_die "freshness contract — $clog STREAM MATCH evidence off (prefix count: $sm/6, full grammar: $smfull/6); a STREAM-MATCH-resembling malformed line or a missing per-run stream guard is CORRUPTION"
    fi
  else
    for name in "${SIM_GOLDENS[@]}"; do
      id="${name%%-*}"
      c="$(count_e "$clog" "^STREAM MATCH $name: $STREAM_RE_TAIL")"
      a="$(count_aff "$clog" "STREAM MATCH $name: ")"
      b="$(count_x "$clog" "== $id ($name)")"
      if [ "$c" != 1 ] || [ "$a" != 1 ] || [ "$b" != 1 ]; then
        grammar_die "identity binding — check-sim evidence for $name in $clog: stream full-grammar $c/1, stem-affine $a/1, banner $b/1 (all 8 goldens must each be judged exactly once)"
      fi
    done
    banners="$(count_e "$clog" '^== ')"
    if [ "$banners" != 8 ]; then
      grammar_die "freshness contract — $clog carries $banners '== '-family banner lines, want exactly 8 (one per golden); an extra or torn banner is CORRUPTION"
    fi
    if [ "$sm" != 8 ] || [ "$smfull" != 8 ]; then
      grammar_die "freshness contract — $clog STREAM MATCH evidence off (prefix count: $sm/8, full grammar: $smfull/8); check-sim must judge all 8 goldens"
    fi
  fi
}

# Component stdout is EVIDENCE (STREAM MATCH lines, byte-stability cmp,
# pins) — captured RAW per component under build/ (lock-guarded fixed
# paths) for the grammar checks, and relayed '  | '-prefixed to this
# script's own output. $3 = spec token; empty = the check-sim
# regression leg (no capture re-record; 8-golden stream evidence).
# $4 = the component's carrier golden names (review-66 M1).
run_component() { # $1 = script path, $2 = expected verdict line, $3 = spec token, $4 = carriers
  local script="$1" want="$2" spec="$3" carriers="$4"
  local clog="$BUILD/vfx-seam.$(basename "$script" .sh).log"
  rm -f "$clog"
  if ! { bash "$script" | tee "$clog"; } 2>&1 | relay_lines; then
    echo "VFX SEAM FAIL: $script rc != 0 (log: $clog)" >&2
    exit 1
  fi
  vfx_judge_log "$clog" "$want" "$spec" "$carriers"
  echo "    -> $want"
}

for idx in "${!CHECKS[@]}"; do
  echo "=== [$((idx + 1))/${#CHECKS[@]}] ${CHECKS[$idx]}"
  run_component "${CHECKS[$idx]}" "${VERDICTS[$idx]}" "${SPECS[$idx]}" "${CARRIERS[$idx]}"
done

echo "=== [regression] port/sim/check-sim.sh (checksum stream untouched)"
run_component port/sim/check-sim.sh "SIM CONFORMS" "" ""

echo "VFX SEAM MATCH"
