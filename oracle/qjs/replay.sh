#!/usr/bin/env bash
# oracle/qjs/replay.sh — replay a frozen golden under the fdlibm-patched
# QuickJS oracle runtime and verify the checksum stream (M0 task 6).
#
# Usage: bash oracle/qjs/replay.sh <golden-id>
#
# Procedure:
#   1. golden params come ONLY from oracle/goldens/manifest.json (the
#      single param source, same as oracle/record.sh);
#   2. build-host/qjs-oracle (build.sh) runs replay-main.js: environment
#      shim + verbatim harness init.js/pagelib.js + the built bundles,
#      fed the golden's trace through the __harness seam, emitting a
#      run.json-shaped checksum stream;
#   3. the UNCHANGED browser-oracle verifier (oracle/harness/
#      verify-stream.js — exact equality, full length, RNG channels,
#      spec/trace/param pins) judges it against the frozen stream.
#
# PASS: prints "QJS MATCH <id>", exit 0. Anything else: nonzero.
set -euo pipefail
cd "$(dirname "$0")"

ID="${1:?usage: bash oracle/qjs/replay.sh <golden-id>}"

REPO="$(cd ../.. && pwd)"
DIST="${MELEELIGHT_CLONE:-$HOME/.cache/meleelight-funkey-s/upstream}"
if [ ! -f "$DIST/dist/meleelight.html" ]; then
  echo "replay.sh: no built upstream at $DIST — run oracle/build-upstream.sh" >&2
  exit 1
fi
if [ ! -x build-host/qjs-oracle ]; then
  echo "replay.sh: build-host/qjs-oracle missing — run oracle/qjs/build.sh" >&2
  exit 1
fi

# Golden params from the manifest (same eval pattern as oracle/record.sh).
eval "$(cd ../harness && node -e '
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

mkdir -p out
RUN=(build-host/qjs-oracle replay-main.js
     --repo "$REPO" --dist "$DIST"
     --trace "$REPO/oracle/goldens/$TRACE"
     --frames "$FRAMES" --seed "$SEED"
     --p1 "$P1" --p2 "$P2" --stage "$STAGE"
     --out "out/qjs-$ID.json")
if [ "$CPU" = "true" ]; then
  RUN+=(--cpu --difficulty "$DIFFICULTY")
fi

echo "replay.sh: $NAME under qjs-oracle"
"${RUN[@]}"

echo "replay.sh: verifying against the frozen stream (verify-stream.js)"
node ../harness/verify-stream.js "out/qjs-$ID.json" \
  "../goldens/$NAME.sha256.json"

echo "QJS MATCH $ID"
