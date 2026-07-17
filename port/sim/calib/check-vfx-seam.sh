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
# Prints VFX SEAM MATCH, exit 0. Never weakened: exact equality only.
set -euo pipefail
cd "$(dirname "$0")/../../.."

CAL=port/sim/calib

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

# Component stdout is EVIDENCE (STREAM MATCH lines, byte-stability cmp,
# pins) — it is passed through to this script's own stdout AND captured
# per component under build/ for the verdict grammar check (full-line
# anchored match on the component's committed literal verdict).
run_component() { # $1 = script path, $2 = expected verdict line
  local script="$1" want="$2"
  local clog="port/sim/calib/build/vfx-seam.$(basename "$script" .sh).log"
  rm -f "$clog"
  if ! bash "$script" | tee "$clog"; then
    echo "VFX SEAM FAIL: $script rc != 0" >&2
    exit 1
  fi
  test -s "$clog" || { echo "VFX SEAM FAIL: empty log $clog" >&2; exit 1; }
  if ! grep -qx "$want" "$clog"; then
    echo "VFX SEAM FAIL: $script did not print its verdict line '$want'" >&2
    exit 1
  fi
  echo "    -> $want"
}

for idx in "${!CHECKS[@]}"; do
  echo "=== [$((idx + 1))/${#CHECKS[@]}] ${CHECKS[$idx]}"
  run_component "${CHECKS[$idx]}" "${VERDICTS[$idx]}"
done

echo "=== [regression] port/sim/check-sim.sh (checksum stream untouched)"
run_component port/sim/check-sim.sh "SIM CONFORMS"

echo "VFX SEAM MATCH"
