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

# INVENTORY PIN (iter 66, review-64 M4): 10 components, all three
# parallel arrays same length, every entry unique — asserted before the
# lock and before anything runs.
NCOMP=10
for pair in "CHECKS:${#CHECKS[@]}" "VERDICTS:${#VERDICTS[@]}" "SPECS:${#SPECS[@]}"; do
  if [ "${pair#*:}" != "$NCOMP" ]; then
    echo "VFX SEAM FAIL: inventory pin — array ${pair%%:*} has ${pair#*:} entries, want exactly $NCOMP (a dropped/added entry is corruption, never a partial pass)" >&2
    exit 1
  fi
done
for arr in CHECKS VERDICTS SPECS; do
  dupes="$(eval 'printf "%s\n" "${'"$arr"'[@]}"' | sort | uniq -d)"
  if [ -n "$dupes" ]; then
    echo "VFX SEAM FAIL: inventory pin — duplicate entries in $arr:" >&2
    printf '%s\n' "$dupes" | sed 's/^/  | /' >&2
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

# Fresh-record evidence grammar (M1) — measured from the full iter-64
# corpus (PROCESS §3 whitelist rule: producer grammar from real logs,
# validated 68/68 STREAM lines + 30/30 run-A/B/byte-stable lines, zero
# false rejections). Length/exactness judgment belongs to the
# rc-enforced verify-stream.js + cmp inside the components; these
# grammars prove the evidence lines are GENUINE producer output at the
# measured counts, so a no-op re-record can never pass.
STREAM_RE='^STREAM MATCH [a-z0-9-]+: [0-9]{1,6}/[0-9]{1,6} frames exact, rngCalls=[0-9]{1,6}, rngCallsOutsideStep=1, specVersion=1$'
BYTEID_LINE='   capture byte-identical across two fresh runs'

# count_x <file> <exact-line>  /  count_e <file> <ere> — grep count
# helpers ('' -c prints 0 but exits 1; the || true keeps set -e happy).
count_x() { local c; c="$(grep -cxF "$2" "$1")" || true; printf '%s' "$c"; }
count_e() { local c; c="$(grep -cE "$2" "$1")" || true; printf '%s' "$c"; }

# Component stdout is EVIDENCE (STREAM MATCH lines, byte-stability cmp,
# pins) — captured RAW per component under build/ (lock-guarded fixed
# paths) for the grammar checks, and relayed '  | '-prefixed to this
# script's own output. $3 = spec token; empty = the check-sim
# regression leg (no capture re-record; 8-golden stream evidence).
run_component() { # $1 = script path, $2 = expected verdict line, $3 = spec token
  local script="$1" want="$2" spec="$3"
  local clog="$BUILD/vfx-seam.$(basename "$script" .sh).log"
  local c r last sm smfull ra rb bi
  rm -f "$clog"
  if ! { bash "$script" | tee "$clog"; } 2>&1 | relay_lines; then
    echo "VFX SEAM FAIL: $script rc != 0 (log: $clog)" >&2
    exit 1
  fi
  test -s "$clog" || { echo "VFX SEAM FAIL: empty log $clog" >&2; exit 1; }
  # VERDICT GRAMMAR (M2): exactly one exact verdict line, it is the
  # FINAL line, and exactly one line matches the verdict-prefix
  # resemblance grammar (verdicts are letters+spaces only — safe as an
  # ERE prefix; measured 1× in every genuine log).
  c="$(count_x "$clog" "$want")"
  if [ "$c" != 1 ]; then
    echo "VFX SEAM FAIL: verdict grammar — expected exactly 1 '$want' line in $clog, found $c (a duplicated or missing verdict is CORRUPT evidence)" >&2
    exit 1
  fi
  r="$(count_e "$clog" "^$want")"
  if [ "$r" != 1 ]; then
    echo "VFX SEAM FAIL: verdict grammar — $clog carries $r lines matching '^$want' but exactly 1 exact verdict; a verdict-RESEMBLING torn/extended line is CORRUPTION, never ignorable" >&2
    exit 1
  fi
  last="$(tail -n 1 "$clog")"
  if [ "$last" != "$want" ]; then
    echo "VFX SEAM FAIL: verdict grammar — final line of $clog is not the verdict:" >&2
    printf '%s\n' "$last" | relay_lines >&2
    exit 1
  fi
  # FRESHNESS CONTRACT (M1)
  sm="$(count_e "$clog" '^STREAM MATCH ')"
  smfull="$(count_e "$clog" "$STREAM_RE")"
  if [ -n "$spec" ]; then
    ra="$(count_e "$clog" "^== g[0-9]{2} \([a-z0-9-]+\): $spec capture run A\$")"
    rb="$(count_e "$clog" "^== g[0-9]{2}: $spec capture run B\$")"
    bi="$(count_x "$clog" "$BYTEID_LINE")"
    if [ "$ra" != 3 ] || [ "$rb" != 3 ] || [ "$bi" != 3 ]; then
      echo "VFX SEAM FAIL: freshness contract — $clog lacks the fresh re-record evidence (capture run A: $ra/3, run B: $rb/3, byte-stable: $bi/3 for spec '$spec'); a MATCH without fresh-record evidence means a stale capture may have replayed — never a pass" >&2
      exit 1
    fi
    if [ "$sm" != 6 ] || [ "$smfull" != 6 ]; then
      echo "VFX SEAM FAIL: freshness contract — $clog STREAM MATCH evidence off (prefix count: $sm/6, full grammar: $smfull/6); a STREAM-MATCH-resembling malformed line or a missing per-run stream guard is CORRUPTION" >&2
      exit 1
    fi
  else
    if [ "$sm" != 8 ] || [ "$smfull" != 8 ]; then
      echo "VFX SEAM FAIL: freshness contract — $clog STREAM MATCH evidence off (prefix count: $sm/8, full grammar: $smfull/8); check-sim must judge all 8 goldens" >&2
      exit 1
    fi
  fi
  echo "    -> $want"
}

for idx in "${!CHECKS[@]}"; do
  echo "=== [$((idx + 1))/${#CHECKS[@]}] ${CHECKS[$idx]}"
  run_component "${CHECKS[$idx]}" "${VERDICTS[$idx]}" "${SPECS[$idx]}"
done

echo "=== [regression] port/sim/check-sim.sh (checksum stream untouched)"
run_component port/sim/check-sim.sh "SIM CONFORMS" ""

echo "VFX SEAM MATCH"
