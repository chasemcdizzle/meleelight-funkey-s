#!/usr/bin/env bash
# Record + freeze a golden's checksum stream (M0 task 5; reused for the
# whole golden set in task 7).
#
# Usage: bash oracle/record.sh <golden-id> [--refreeze]
#   <golden-id>  id or name from oracle/goldens/manifest.json (e.g. g01)
#   --refreeze   allow overwriting an existing, DIFFERING frozen stream —
#                only legitimate together with a spec version bump
#                (oracle/CHECKSUM.md §8)
#
# Procedure (all params come from manifest.json — never from flags):
#   1. TWO fresh browser runs of the golden's trace via the oracle harness
#      (seeded mulberry32 PRNG + virtual clock + fdlibm shim, all default-on);
#   2. compare.js asserts the runs are bit-identical (else abort);
#   3. freeze-stream.js re-asserts identity on every conformance channel,
#      checks rngCallsOutsideStep == 1 (CHECKSUM.md §1.3), and writes
#      oracle/goldens/<name>.sha256.json;
#   4. verify-stream.js self-check: a fresh run verifies against the file
#      just frozen (exact equality, full length).
#
# The frozen file is deliberately timestamp-free and environment-free:
# re-running this script for an already-frozen golden must end with
# "unchanged (byte-identical re-freeze)". Any diff = drift = investigate.
set -euo pipefail
cd "$(dirname "$0")/harness"

ID="${1:?usage: bash oracle/record.sh <golden-id> [--refreeze]}"
REFREEZE=""
[ "${2:-}" = "--refreeze" ] && REFREEZE="--refreeze"

DIST="${MELEELIGHT_CLONE:-$HOME/.cache/meleelight-funkey-s/upstream}"
if [ ! -f "$DIST/dist/meleelight.html" ]; then
  echo "record.sh: no built upstream at $DIST — run oracle/build-upstream.sh" >&2
  exit 1
fi

# Pull the golden's params out of the manifest as shell assignments.
eval "$(node -e '
  const g = require("./streamlib").goldenById(process.argv[1]);
  const sq = (s) => "'\''" + String(s) + "'\''";
  console.log("NAME=" + sq(g.name));
  console.log("TRACE=" + sq(g.trace));
  console.log("FRAMES=" + sq(g.frames));
  console.log("SEED=" + sq(g.seed));
  console.log("P1=" + sq(g.p1));
  console.log("P2=" + sq(g.p2));
  console.log("STAGE=" + sq(g.stage));
  console.log("CPU=" + sq(g.cpu));
  console.log("DIFFICULTY=" + sq(g.difficulty));
' "$ID")"

RUN=(node run.js --dist "$DIST" --trace "../goldens/$TRACE"
     --frames "$FRAMES" --seed "$SEED"
     --p1 "$P1" --p2 "$P2" --stage "$STAGE")
if [ "$CPU" = "true" ]; then
  RUN+=(--cpu --difficulty "$DIFFICULTY")
fi

echo "record.sh: $NAME — fresh run A"
"${RUN[@]}" --out "out/record-$ID-a.json"
echo "record.sh: $NAME — fresh run B"
"${RUN[@]}" --out "out/record-$ID-b.json"

echo "record.sh: comparing the two fresh runs"
node compare.js "out/record-$ID-a.json" "out/record-$ID-b.json"

echo "record.sh: freezing"
node freeze-stream.js "$ID" "out/record-$ID-a.json" "out/record-$ID-b.json" $REFREEZE

echo "record.sh: self-check — fresh run vs frozen stream"
node verify-stream.js "out/record-$ID-a.json" "../goldens/$NAME.sha256.json"

echo "RECORDED $ID -> oracle/goldens/$NAME.sha256.json"
