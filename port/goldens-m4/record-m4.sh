#!/usr/bin/env bash
# record-m4.sh — record + freeze an M4 golden's checksum stream into
# port/goldens-m4/ (M4 task 5; the oracle/record.sh procedure REUSING the
# oracle/harness bytes VERBATIM BY PATH — fix_plan §M4 conventions;
# HARD RULE 3: oracle/ is read-only here, nothing in it is written).
#
# Usage: bash port/goldens-m4/record-m4.sh <golden-id> [--refreeze]
#   <golden-id>  id or name from port/goldens-m4/manifest.json (e.g. m01)
#   --refreeze   allow overwriting an existing, DIFFERING frozen stream —
#                only legitimate with a spec version bump (CHECKSUM.md §8)
#
# Procedure (params come ONLY from port/goldens-m4/manifest.json):
#   1. the golden's trace must exist (generated once by
#      `node oracle/harness/gen-trace.js <trace> 3800 <seed>` — the
#      manifest seed doubles as the gen-trace seed, M0 convention);
#   2. TWO fresh browser runs via the UNCHANGED oracle/harness/run.js
#      (seeded mulberry32 + virtual clock + fdlibm shim, all default-on);
#   3. oracle/harness/compare.js asserts the runs bit-identical;
#   4. check-quality.js asserts the M0 gameplay-quality contract on run A
#      (>=1 KO, >=1 DAMAGE*/CAPTUREDAMAGE, both players >=1 stock at the
#      final frame, match still live) — a failing seed is REJECTED here;
#   5. freeze-stream-m4.js writes port/goldens-m4/<name>.sha256.json
#      (M0-identical format; streamlib primitives required by path);
#   6. verify-stream.js self-check: run A verifies against the frozen file.
set -euo pipefail
cd "$(dirname "$0")/../.."

ID="${1:?usage: bash port/goldens-m4/record-m4.sh <golden-id> [--refreeze]}"
REFREEZE=""
[ "${2:-}" = "--refreeze" ] && REFREEZE="--refreeze"

M4G=port/goldens-m4
HARNESS=oracle/harness

DIST="${MELEELIGHT_CLONE:-$HOME/.cache/meleelight-funkey-s/upstream}"
if [ ! -f "$DIST/dist/meleelight.html" ]; then
  echo "record-m4.sh: no built upstream at $DIST — run oracle/build-upstream.sh" >&2
  exit 1
fi

# Pull the golden's params out of the M4 manifest as shell assignments.
eval "$(node -e '
  const fs = require("fs");
  const m = JSON.parse(fs.readFileSync("port/goldens-m4/manifest.json", "utf8"));
  const g = m.goldens.find((x) => x.id === process.argv[1] || x.name === process.argv[1]);
  if (!g) { console.error("unknown m4 golden: " + process.argv[1]); process.exit(1); }
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

if [ ! -f "$M4G/$TRACE" ]; then
  echo "record-m4.sh: generating trace ($SEED)"
  node "$HARNESS/gen-trace.js" "$M4G/$TRACE" 3800 "$SEED"
fi

REPO="$PWD"
RUN=(node run.js --dist "$DIST" --trace "$REPO/$M4G/$TRACE"
     --frames "$FRAMES" --seed "$SEED"
     --p1 "$P1" --p2 "$P2" --stage "$STAGE")
if [ "$CPU" = "true" ]; then
  RUN+=(--cpu --difficulty "$DIFFICULTY")
fi

cd "$HARNESS"
echo "record-m4.sh: $NAME — fresh run A"
"${RUN[@]}" --out "out/record-$ID-a.json"
echo "record-m4.sh: $NAME — fresh run B"
"${RUN[@]}" --out "out/record-$ID-b.json"

echo "record-m4.sh: comparing the two fresh runs"
node compare.js "out/record-$ID-a.json" "out/record-$ID-b.json"
cd "$REPO"

echo "record-m4.sh: gameplay-quality contract (mechanical)"
node "$M4G/check-quality.js" "$HARNESS/out/record-$ID-a.json"

echo "record-m4.sh: freezing into $M4G/"
node "$M4G/freeze-stream-m4.js" "$ID" "$HARNESS/out/record-$ID-a.json" \
  "$HARNESS/out/record-$ID-b.json" $REFREEZE

echo "record-m4.sh: self-check — run A vs frozen stream (unchanged verifier)"
node "$HARNESS/verify-stream.js" "$HARNESS/out/record-$ID-a.json" \
  "$M4G/$NAME.sha256.json"

echo "RECORDED $ID -> $M4G/$NAME.sha256.json"
