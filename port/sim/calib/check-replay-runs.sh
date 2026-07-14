#!/usr/bin/env bash
# M2-CAL task 2 done-check: the structure-parallel C translation builds and
# the replay driver runs the g01 capture END-TO-END (every record parsed,
# marshalled, dispatched; divergences are COUNTED and reported, not fatal —
# burning them down to zero is task 3 / check-envcoll.sh).
set -euo pipefail
cd "$(dirname "$0")/../../.."

CAL=port/sim/calib
BUILD=$CAL/build
mkdir -p "$BUILD"

# build: every TU with -ffp-contract=off (PLAN §2)
cc -O2 -ffp-contract=off -Wall -Wextra -Werror \
  -o "$BUILD/envcoll_replay" \
  "$CAL/replay_envcoll.c" "$CAL/canon.c" \
  port/sim/environmental_collision.c \
  port/fdlibm/fdlibm.c -lm
echo "build OK: $BUILD/envcoll_replay (cc -O2 -ffp-contract=off)"

# the g01 capture must exist (task 1's rig records it)
if [ ! -s "$BUILD/g01.envcoll.jsonl" ]; then
  echo "g01 capture missing — recording via run-capture.js"
  node "$CAL/run-capture.js" --golden g01 \
    --out-jsonl "$BUILD/g01.envcoll.jsonl" --out-run "$BUILD/g01.capture-run.json"
fi

# end-to-end replay (report mode): exit 0 == ran to completion
"$BUILD/envcoll_replay" "$BUILD/g01.envcoll.jsonl" --max-print 3
