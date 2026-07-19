#!/usr/bin/env bash
# record-m4.sh — record + freeze an M4 golden's checksum stream into
# port/goldens-m4/ (M4 task 5; the oracle/record.sh procedure REUSING the
# oracle/harness bytes VERBATIM BY PATH — fix_plan §M4 conventions;
# HARD RULE 3: oracle/ is read-only here, nothing tracked in it is
# written — the gitignored oracle/harness/out/ scratch dir is the only
# write target outside port/goldens-m4/).
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
#      (M0-identical format; streamlib primitives required by path; the
#      freezer also validates the FULL manifest grammar — schema, types,
#      ranges, duplicates, basename-only paths);
#   6. verify-stream.js self-check: run A verifies against the frozen file.
#
# HARDENING (iter 83 — review-81 round-1 closure, .loop/review-81-triage
# .md): the eval class is DEAD — params come through an rc-checked
# key=value line-parse with per-key anchored whitelists (PROCESS §3),
# duplicate rejection, exact line count, and trace === name+'.trace.json'
# (basename-only by construction — no path escape from the golden home);
# unknown extra args are refused; a no-reclaim run lock guards the fixed
# A/B output paths; both run JSONs are rm'd BEFORE their producer runs
# and asserted non-empty after (rm-before-produce — a run.js that exits 0
# without writing can never leave stale bytes masquerading as fresh).
set -euo pipefail
cd "$(dirname "$0")/../.."

die() { echo "record-m4.sh: $*" >&2; exit 1; }

ID="${1:?usage: bash port/goldens-m4/record-m4.sh <golden-id> [--refreeze]}"
REFREEZE=""
if [ "$#" -gt 2 ]; then
  die "too many arguments (usage: record-m4.sh <golden-id> [--refreeze])"
fi
if [ "$#" = 2 ]; then
  case "$2" in
    (--refreeze) REFREEZE="--refreeze" ;;
    (*) die "unknown argument '$2' (only --refreeze is accepted)" ;;
  esac
fi
case "$ID" in
  (*[!a-z0-9-]*|'') die "golden id '$ID' fails the whitelist [a-z0-9-]" ;;
esac

M4G=port/goldens-m4
HARNESS=oracle/harness

DIST="${MELEELIGHT_CLONE:-$HOME/.cache/meleelight-funkey-s/upstream}"
if [ ! -f "$DIST/dist/meleelight.html" ]; then
  die "no built upstream at $DIST — run oracle/build-upstream.sh"
fi

# Pull the golden's params out of the M4 manifest — strict key=value
# line-parse with per-key anchored whitelists (the eval class is dead;
# PROCESS §3 whitelist-grammar rule).
out="$(node -e '
  const fs = require("fs");
  const m = JSON.parse(fs.readFileSync("port/goldens-m4/manifest.json", "utf8"));
  const g = m.goldens.find((x) => x.id === process.argv[1] || x.name === process.argv[1]);
  if (!g) { console.error("unknown m4 golden: " + process.argv[1]); process.exit(1); }
  const emit = (k, v) => process.stdout.write(k + "=" + String(v) + "\n");
  emit("name", g.name); emit("trace", g.trace); emit("frames", g.frames);
  emit("seed", g.seed); emit("p1", g.p1); emit("p2", g.p2);
  emit("stage", g.stage); emit("cpu", g.cpu); emit("difficulty", g.difficulty);
' "$ID")" || die "manifest read failed for '$ID'"
NAME= TRACE= FRAMES= SEED= P1= P2= STAGE= CPU= DIFFICULTY=
n=0
while IFS= read -r line; do
  n=$((n + 1))
  v="${line#*=}"
  case "$line" in
    (name=*)
      [[ "$v" =~ ^[a-z0-9][a-z0-9-]*$ ]] || die "manifest grammar — name '$v' fails the whitelist [a-z0-9-]"
      [ -z "$NAME" ] || die "manifest grammar — duplicate name line"
      NAME="$v" ;;
    (trace=*)
      [[ "$v" =~ ^[a-z0-9][a-z0-9.-]*$ ]] || die "manifest grammar — trace '$v' fails the whitelist (basename characters only)"
      [ -z "$TRACE" ] || die "manifest grammar — duplicate trace line"
      TRACE="$v" ;;
    (frames=*)
      [[ "$v" =~ ^[1-9][0-9]{0,5}$ ]] || die "manifest grammar — frames '$v' is not a positive integer"
      [ -z "$FRAMES" ] || die "manifest grammar — duplicate frames line"
      FRAMES="$v" ;;
    (seed=*)
      [[ "$v" =~ ^[0-9]{1,10}$ ]] || die "manifest grammar — seed '$v' is not a plain integer"
      [ -z "$SEED" ] || die "manifest grammar — duplicate seed line"
      SEED="$v" ;;
    (p1=*)
      [[ "$v" =~ ^[0-4]$ ]] || die "manifest grammar — p1 '$v' outside the char domain 0-4"
      [ -z "$P1" ] || die "manifest grammar — duplicate p1 line"
      P1="$v" ;;
    (p2=*)
      [[ "$v" =~ ^[0-4]$ ]] || die "manifest grammar — p2 '$v' outside the char domain 0-4"
      [ -z "$P2" ] || die "manifest grammar — duplicate p2 line"
      P2="$v" ;;
    (stage=*)
      [[ "$v" =~ ^[0-5]$ ]] || die "manifest grammar — stage '$v' outside the stage domain 0-5"
      [ -z "$STAGE" ] || die "manifest grammar — duplicate stage line"
      STAGE="$v" ;;
    (cpu=*)
      [[ "$v" =~ ^(true|false)$ ]] || die "manifest grammar — cpu '$v' is not a boolean"
      [ -z "$CPU" ] || die "manifest grammar — duplicate cpu line"
      CPU="$v" ;;
    (difficulty=*)
      [[ "$v" =~ ^([1-9]|null)$ ]] || die "manifest grammar — difficulty '$v' outside 1-9/null"
      [ -z "$DIFFICULTY" ] || die "manifest grammar — duplicate difficulty line"
      DIFFICULTY="$v" ;;
    (*)
      die "manifest grammar — unrecognized param line '$line' (whitelist parse; resembles-but-doesn't-match is corruption)" ;;
  esac
done <<< "$out"
[ "$n" = 9 ] || die "manifest grammar — got $n param lines, want exactly 9"
if [ "$TRACE" != "$NAME.trace.json" ]; then
  die "manifest grammar — trace '$TRACE' != name-derived '$NAME.trace.json' (basename-only by construction; no path escape from the golden home)"
fi
if [ "$CPU" = "true" ]; then
  [ "$DIFFICULTY" != "null" ] || die "manifest grammar — cpu golden without a difficulty"
else
  [ "$DIFFICULTY" = "null" ] || die "manifest grammar — non-cpu golden with a difficulty"
fi

# RUN LOCK (no-reclaim, iter-41 posture): the shared resources are the
# fixed out/record-* paths and the golden home; one recorder at a time.
mkdir -p "$HARNESS/out"
LOCK="$HARNESS/out/record-m4.lock"
if ! mkdir "$LOCK" 2>/dev/null; then
  echo "record-m4.sh REFUSED: run lock $LOCK already exists." >&2
  echo "  Another record-m4.sh run may be writing the shared out/record-*" >&2
  echo "  files right now. NO auto-reclaim. If you are sure no run is live," >&2
  echo "  remove it manually: rm -rf '$LOCK'" >&2
  exit 1
fi
trap 'rm -rf "$LOCK"' EXIT

if [ ! -f "$M4G/$TRACE" ]; then
  echo "record-m4.sh: generating trace ($SEED)"
  node "$HARNESS/gen-trace.js" "$M4G/$TRACE" 3800 "$SEED"
fi
test -s "$M4G/$TRACE" || die "trace $M4G/$TRACE missing or empty"

REPO="$PWD"
RUN=(node run.js --dist "$DIST" --trace "$REPO/$M4G/$TRACE"
     --frames "$FRAMES" --seed "$SEED"
     --p1 "$P1" --p2 "$P2" --stage "$STAGE")
if [ "$CPU" = "true" ]; then
  RUN+=(--cpu --difficulty "$DIFFICULTY")
fi

# FRESHNESS (rm-before-produce): stale A/B run JSONs can never
# masquerade as the two fresh independent browser runs.
rm -f "$HARNESS/out/record-$ID-a.json" "$HARNESS/out/record-$ID-b.json"

cd "$HARNESS"
echo "record-m4.sh: $NAME — fresh run A"
"${RUN[@]}" --out "out/record-$ID-a.json"
test -s "out/record-$ID-a.json" || die "run A left no fresh run JSON (rm-before-produce freshness guard)"
echo "record-m4.sh: $NAME — fresh run B"
"${RUN[@]}" --out "out/record-$ID-b.json"
test -s "out/record-$ID-b.json" || die "run B left no fresh run JSON (rm-before-produce freshness guard)"

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
